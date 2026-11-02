"""Check native strided copies and identical manual/captured commands."""

import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
GEMMINI = ROOT / "chipyard/generators/gemmini/software/gemmini-rocc-tests"

SOURCE = r"""
#include "accel_types.h"
#include "gemmini.h"
#include <string.h>
#define ACCEL_H
#define GEMMINI_JOB_H
typedef struct { unsigned funct; uint64_t rs1, rs2; } command_t;
static command_t sent[4];
static unsigned count, expected, captures;
static int fail_capture;
static void record(unsigned funct, uint64_t rs1, uint64_t rs2) {
  if (count < 4) sent[count] = (command_t){.funct = funct, .rs1 = rs1, .rs2 = rs2};
  ++count;
}
#undef ROCC_INSTRUCTION_RS1_RS2
#define ROCC_INSTRUCTION_RS1_RS2(x, rs1, rs2, funct) record(funct, (uintptr_t)(rs1), (uint64_t)(rs2));
#define accel_commands(device, ...) do { (void)(device); __VA_ARGS__ } while (0)
static int gemmini_job_capture_at(uintptr_t control, unsigned job, unsigned commands, unsigned patches) {
  (void)control; (void)job;
  expected = commands;
  ++captures;
  return patches || fail_capture;
}
static int gemmini_job_end_at(uintptr_t control) { (void)control; return count != expected; }
#include "gemmini_copy.h"

int main(void) {
  const accel_t device = {.id = 3};
  gemmini_copy_t cases[] = {
    {.rows = DIM, .columns = DIM, .element_bytes = 1,
     .source_stride = DIM, .destination_stride = DIM * 7,
     .source = (void *)0x1000, .destination = (void *)0x2000},
    {.rows = DIM - 1, .columns = DIM - 3, .element_bytes = 4,
     .source_stride = DIM * 4 * 3, .destination_stride = (DIM - 3) * 4,
     .source = (void *)0x3000, .destination = (void *)0x4000},
    {.rows = 2, .columns = MAX_BLOCK_LEN * DIM, .element_bytes = 1,
     .source_stride = MAX_BLOCK_LEN * DIM, .destination_stride = MAX_BLOCK_LEN * DIM * 2,
     .source = (void *)0x5000, .destination = (void *)0x6000},
    {.rows = 3, .columns = DIM + 1, .element_bytes = 1,
     .source_stride = DIM * 3, .destination_stride = DIM * 4,
     .source = (void *)0x7000, .destination = (void *)0x8000}
  };
  for (unsigned index = 0; index < sizeof(cases) / sizeof(*cases); ++index) {
    const gemmini_copy_t *params = &cases[index];
    count = 0;
    if (gemmini_copy_capture(device, 0, params) || count != GEMMINI_COPY_COMMANDS) return 1;
    command_t captured[4];
    memcpy(captured, sent, sizeof(sent));
    count = 0;
    if (gemmini_copy_run(device, params) || count != GEMMINI_COPY_COMMANDS || memcmp(captured, sent, sizeof(sent))) return 2;
    const int full = params->element_bytes == sizeof(acc_t);
    const uint64_t shape = ((uint64_t)params->rows << (ADDR_LEN + 16)) | ((uint64_t)params->columns << ADDR_LEN);
    const uint32_t write = full ? UINT32_C(1) << (ADDR_LEN - 1) : 0;
    const uint32_t read = full ? write | (UINT32_C(1) << (ADDR_LEN - 3)) : 0;
    if (sent[0].funct != k_CONFIG || sent[0].rs2 != params->source_stride ||
        ((sent[0].rs1 >> 16) & UINT16_MAX) != DIM ||
        ((sent[0].rs1 >> 3) & 3) != (full ? 2 : 0) || ((sent[0].rs1 >> 2) & 1) ||
        sent[1].funct != k_CONFIG || (uint32_t)sent[1].rs2 != params->destination_stride ||
        sent[2].funct != (full ? k_MVIN3 : k_MVIN) || sent[2].rs1 != (uintptr_t)params->source || sent[2].rs2 != (shape | write) ||
        sent[3].funct != k_MVOUT || sent[3].rs1 != (uintptr_t)params->destination || sent[3].rs2 != (shape | read)) return 3;
  }
  gemmini_workspace_t w = gemmini_copy_workspace(2, MAX_BLOCK_LEN * DIM, 1);
  if (w.scratch_rows != (MAX_BLOCK_LEN - 1) * DIM + 2 || w.accumulator_rows) return 5;
  w = gemmini_copy_workspace(3, DIM + 1, 1);
  if (w.scratch_rows != DIM + 3 || w.accumulator_rows) return 6;
  gemmini_copy_t invalid[9];
  for (unsigned index = 0; index < 9; ++index) invalid[index] = cases[0];
  invalid[0].rows = 0;
  invalid[1].columns = 0;
  invalid[2].rows = DIM + 1;
  invalid[3].columns = MAX_BLOCK_LEN * DIM + 1;
  invalid[3].source_stride = invalid[3].destination_stride = invalid[3].columns;
  invalid[4].element_bytes = 2;
  invalid[5].source_stride = DIM - 1;
  invalid[6].destination_stride = DIM - 1;
  invalid[7].destination_stride = UINT64_C(1) << 32;
  invalid[8] = cases[1];
  invalid[8].columns = DIM + 1;
  invalid[8].source_stride = invalid[8].destination_stride = (DIM + 1) * 4;
  for (unsigned index = 0; index < 9; ++index) {
    count = captures = 0;
    if (!gemmini_copy_capture(device, 0, &invalid[index]) || !gemmini_copy_run(device, &invalid[index]) || count || captures) return 4;
  }
  fail_capture = 1;
  count = 0;
  return !gemmini_copy_capture(device, 0, &cases[0]) || count;
}
"""


class RuntimeCopyTest(unittest.TestCase):
    def test_native_commands(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)
            source = path / "test.c"
            source.write_text(SOURCE)
            binary = path / "test"
            subprocess.run(
                [
                    "cc",
                    "-std=gnu11",
                    "-O2",
                    "-Wall",
                    "-Wextra",
                    "-Werror",
                    "-fsanitize=undefined",
                    "-fno-sanitize-recover=all",
                    "-I",
                    str(ROOT / "tests/include"),
                    "-isystem",
                    str(GEMMINI / "include"),
                    "-isystem",
                    str(GEMMINI),
                    str(source),
                    "-o",
                    str(binary),
                ],
                check=True,
            )
            subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    unittest.main()
