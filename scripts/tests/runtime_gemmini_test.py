"""Compare native tile submission with patched captured commands."""

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
#include "gemmini_types.h"
#include <string.h>
#define ACCEL_H
#define GEMMINI_JOB_H
#define GEMMINI_VALUE_SLOT 0
#define GEMMINI_VALUE_TILE_ID 1
typedef struct {
  unsigned funct;
  uint64_t operands[2];
} command_t;
static command_t sent[64];
static gemmini_patch_t patches[8];
static unsigned sent_count, patch_count, expected_commands, expected_patches, fail_capture;
static void record(unsigned funct, uint64_t rs1, uint64_t rs2) { sent[sent_count++] = (command_t){.funct = funct, .operands = {rs1, rs2}}; }
#undef ROCC_INSTRUCTION_RS1_RS2
#define ROCC_INSTRUCTION_RS1_RS2(x, rs1, rs2, funct) record(funct, (uintptr_t)(rs1), (uint64_t)(rs2))
#undef gemmini_fence
#define gemmini_fence() ((void)0)
#define accel_commands(device, ...)                                                                                                                                                                    \
  do {                                                                                                                                                                                                 \
    (void)(device);                                                                                                                                                                                    \
    __VA_ARGS__                                                                                                                                                                                        \
  } while (0)
static void gemmini_job_patch_at(uintptr_t control, const gemmini_patch_t *patch) {
  (void)control;
  patches[patch_count++] = *patch;
}
static int gemmini_job_capture_at(uintptr_t control, unsigned job, unsigned commands, unsigned updates) {
  (void)control;
  (void)job;
  expected_commands = commands;
  expected_patches = updates;
  return fail_capture;
}
static int gemmini_job_end_at(uintptr_t control) {
  (void)control;
  return sent_count != expected_commands || patch_count != expected_patches;
}
#include "gemmini_tile.h"

int main(void) {
  elem_t input[40 * DIM], weight[DIM * DIM];
  acc_t output[40 * DIM];
  const accel_t device = {.id = 3};
  gemmini_tile_t params = {.rows = 40,
                           .tile_rows = 2,
                           .k = 7,
                           .n = 12,
                           .input_row = 16,
                           .weight_row = 96,
                           .input = input,
                           .weight = weight,
                           .output = output,
                           .input_stride = 48,
                           .output_stride = 160,
                           .publish = 1,
                           .publish_row = 400,
                           .publish_stride = 8,
                           .store = 1};
  for (unsigned preload = 0; preload < 2; ++preload) {
    params.preload_input = preload;
    sent_count = 0;
    gemmini_tile_init(device, &params);
    if (sent_count != (preload ? 7u : 3u))
      return 1;
    if (preload && (sent[sent_count - 1].operands[1] >> 48) != 8)
      return 2;
    for (unsigned publish = 0; publish < 2; ++publish) {
      for (unsigned store = 0; store < 2; ++store) {
        params.publish = publish;
        params.store = store;
        sent_count = patch_count = 0;
        const gemmini_tile_stream_t counts = gemmini_tile_emit(&params, GEMMINI_TILE_COUNT, device.control, 0, 0);
        if (counts.commands > GEMMINI_TILE_COMMANDS || counts.patches > GEMMINI_TILE_PATCHES || sent_count || patch_count)
          return 9;
        if (gemmini_tile_capture(device, 2, &params))
          return 3;
        if (sent_count != counts.commands || patch_count != counts.patches)
          return 10;
        command_t captured[64];
        const unsigned count = sent_count;
        memcpy(captured, sent, count * sizeof(*sent));
        for (unsigned tile = 0; tile < 3; ++tile) {
          for (unsigned slot = 0; slot < 2; ++slot) {
            command_t replay[64];
            memcpy(replay, captured, count * sizeof(*replay));
            for (unsigned index = 0; index < patch_count; ++index) {
              const gemmini_patch_t *patch = &patches[index];
              const uint64_t field = UINT64_MAX >> (64 - patch->width);
              uint64_t *operand = &replay[patch->command].operands[patch->operand];
              const unsigned source = patch->source == GEMMINI_VALUE_TILE_ID ? tile : slot;
              const uint64_t value = ((*operand & field) + source * patch->scale) & field;
              *operand = (*operand & ~field) | value;
            }
            sent_count = 0;
            gemmini_tile_run(device, &params, tile, slot);
            if (sent_count != count || memcmp(sent, replay, count * sizeof(*sent)))
              return 4;
            unsigned publications = 0, stores = 0;
            for (unsigned index = 0; index < count; ++index) {
              const command_t *command = &sent[index];
              if (command->funct == k_MVOUT_SPAD) {
                ++publications;
                if ((uint32_t)command->operands[0] != params.publish_row + slot * params.publish_stride)
                  return 5;
              }
              if (command->funct == k_MVOUT) {
                ++stores;
                if (command->operands[0] != (uintptr_t)output + tile * params.output_stride)
                  return 6;
              }
            }
            if (publications != publish || stores != store)
              return 7;
          }
        }
      }
    }
  }
  sent_count = patch_count = 0;
  fail_capture = 1;
  if (!gemmini_tile_capture(device, 0, &params) || sent_count || patch_count)
    return 8;
  return 0;
}
"""


class RuntimeGemminiTest(unittest.TestCase):
    def test_native_capture_matches_manual(self):
        metadata = yaml.safe_load(
            (ROOT / "configs/kernels/gemmini_tile.yaml").read_text()
        )["runtime"]
        header = (ROOT / "tests/include/gemmini_tile.h").read_text()
        for field in ("commands", "patches"):
            macro = re.search(
                rf"^#define GEMMINI_TILE_{field.upper()} (\d+)$", header, re.MULTILINE
            )
            if not macro or metadata[field] != int(macro.group(1)):
                self.fail(f"native {field} bound differs from compiler capabilities")
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
