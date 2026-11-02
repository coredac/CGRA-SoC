"""Check native matrix strides, K accumulation commands, and workspace rejection."""

import re
import subprocess
import tempfile
import unittest
from pathlib import Path

import yaml

ROOT = Path(__file__).resolve().parents[2]
GEMMINI = ROOT / "chipyard/generators/gemmini/software/gemmini-rocc-tests"

SOURCE = r"""
#include "accel_types.h"
#include "gemmini.h"
#include <string.h>
#define ACCEL_H
#define GEMMINI_JOB_H
typedef struct { unsigned funct; uint64_t rs1, rs2; } command_t;
static command_t sent[64];
static unsigned count, expected, captures;
static int fail_capture;
static void record(unsigned funct, uint64_t rs1, uint64_t rs2) {
  sent[count++] = (command_t){.funct = funct, .rs1 = rs1, .rs2 = rs2};
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
#include "gemmini_matrix.h"

static gemmini_matrix_t matrix(unsigned m, unsigned k, unsigned n, unsigned transpose_a, unsigned transpose_b) {
  return (gemmini_matrix_t){.m = m, .k = k, .n = n,
    .transpose_a = transpose_a, .transpose_b = transpose_b,
    .accumulate = 0, .store = 1,
    .input_stride = transpose_a ? m : k,
    .weight_stride = transpose_b ? k : n,
    .output_stride = n * sizeof(acc_t),
    .input_elements = m * k, .weight_elements = k * n,
    .bias_elements = n, .output_elements = m * n,
    .input = (void *)0x1000, .weight = (void *)0x2000,
    .bias = (void *)0x3000, .output = (void *)0x4000};
}

int main(void) {
  const accel_t device = {.id = 3};
  gemmini_matrix_t cases[] = {
    matrix(4, 64, 256, 0, 0), matrix(4, 256, 64, 0, 0),
    matrix(4, 64, 4, 0, 1), matrix(4, 4, 64, 0, 0),
    matrix(4, 64, 64, 1, 0),
    matrix(7, 32, 19, 0, 0), matrix(7, 32, 19, 0, 0), matrix(7, 17, 19, 0, 0)
  };
  /* A pitched output tile keeps its accumulator across first/middle/final K chunks. */
  for (unsigned index = 5; index < 8; ++index) {
    cases[index].input_stride = 81;
    cases[index].weight_stride = 57;
    cases[index].output_stride = 57 * sizeof(acc_t);
    cases[index].input = (void *)(uintptr_t)(0x1000 + (index - 5) * 32);
    cases[index].weight = (void *)(uintptr_t)(0x2000 + (index - 5) * 32 * 57);
    cases[index].accumulate = index != 5;
    cases[index].store = index == 7;
  }
  for (unsigned index = 0; index < sizeof(cases) / sizeof(*cases); ++index) {
    const gemmini_matrix_t *params = &cases[index];
    count = 0;
    if (gemmini_matrix_capture(device, 0, params) || count != GEMMINI_MATRIX_COMMANDS)
      return 1;
    command_t captured[64];
    memcpy(captured, sent, count * sizeof(*sent));
    count = 0;
    if (gemmini_matrix_run(device, params) || count != GEMMINI_MATRIX_COMMANDS || memcmp(captured, sent, count * sizeof(*sent)))
      return 2;
    const unsigned i = (params->m + DIM - 1) / DIM;
    const unsigned j = (params->n + DIM - 1) / DIM;
    const unsigned k = (params->k + DIM - 1) / DIM;
    const uint64_t bounds = ((uint64_t)k << 32) | ((uint64_t)j << 16) | i;
    const uint64_t padding = ((uint64_t)(k * DIM - params->k) << 32) | ((uint64_t)(j * DIM - params->n) << 16) | (i * DIM - params->m);
    if (sent[1].rs2 != params->input_stride || sent[2].rs2 != params->weight_stride ||
        (uint32_t)sent[4].rs2 != params->output_stride ||
        sent[5].funct != k_LOOP_WS_CONFIG_SPAD_AB || sent[5].rs1 != 0 || sent[5].rs2 != BANK_NUM * BANK_ROWS / 2 ||
        sent[6].funct != k_LOOP_WS_CONFIG_BOUNDS || sent[6].rs1 != padding || sent[6].rs2 != bounds ||
        sent[7].funct != k_LOOP_WS_CONFIG_ADDRS_AB || sent[7].rs1 != (uintptr_t)params->input || sent[7].rs2 != (uintptr_t)params->weight ||
        sent[8].funct != k_LOOP_WS_CONFIG_ADDRS_DC || sent[8].rs1 != (params->accumulate ? 0 : (uintptr_t)params->bias) || sent[8].rs2 != (params->store ? (uintptr_t)params->output : 0) ||
        sent[9].rs1 != params->input_stride / sizeof(elem_t) || sent[9].rs2 != params->weight_stride / sizeof(elem_t) ||
        sent[10].rs1 != 0 || sent[10].rs2 != params->output_stride / sizeof(acc_t) || sent[11].funct != k_LOOP_WS || sent[11].rs1 != 3 || sent[11].rs2 != (params->transpose_b << 1 | params->transpose_a))
      return 3;
  }
  gemmini_matrix_t invalid[] = {
    matrix(16, BANK_NUM * BANK_ROWS, 16, 0, 0),
    matrix(DIM, DIM, ACC_ROWS, 0, 0),
    matrix(4, 64, 64, 1, 1), matrix(0, 64, 64, 0, 0),
    matrix(4, 64, 64, 0, 2), cases[0], cases[0], cases[0], cases[0],
    cases[0], cases[0], cases[0], cases[0], cases[0], cases[0]
  };
  --invalid[5].input_elements;
  --invalid[6].weight_elements;
  --invalid[7].bias_elements;
  --invalid[8].output_elements;
  --invalid[9].input_stride;
  --invalid[10].weight_stride;
  --invalid[11].output_stride;
  invalid[12].output_stride = UINT64_C(1) << 32;
  invalid[13].accumulate = 2;
  invalid[14].store = 2;
  for (unsigned index = 0; index < sizeof(invalid) / sizeof(*invalid); ++index) {
    count = captures = 0;
    if (!gemmini_matrix_capture(device, 0, &invalid[index]) || !gemmini_matrix_run(device, &invalid[index]) || count || captures)
      return 4;
  }
  fail_capture = 1;
  count = 0;
  return !gemmini_matrix_capture(device, 0, &cases[0]) || count;
}
"""


class RuntimeMatrixTest(unittest.TestCase):
    def test_complete_native_commands(self):
        metadata = yaml.safe_load(
            (ROOT / "configs/kernels/gemmini_matrix.yaml").read_text()
        )["runtime"]
        header = (ROOT / "tests/include/gemmini_matrix.h").read_text()
        for field in ("commands", "patches"):
            macro = re.search(
                rf"^#define GEMMINI_MATRIX_{field.upper()} (\d+)$", header, re.MULTILINE
            )
            if not macro or metadata[field] != int(macro.group(1)):
                self.fail(f"native {field} count differs from compiler capability")
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
                    "-I",
                    str(ROOT / "tests/include"),
                    "-I",
                    str(GEMMINI / "include"),
                    "-I",
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
