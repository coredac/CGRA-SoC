"""Check complete Conv commands and native workspace validation."""

import re
import subprocess
import tempfile
import unittest
from pathlib import Path

import yaml

ROOT = Path(__file__).resolve().parents[2]
GEMMINI = ROOT / "chipyard/generators/gemmini/software/gemmini-rocc-tests"

SOURCE = r"""
#include <stdint.h>
#include <string.h>
#include "rocc-software/src/xcustom.h"
typedef struct { unsigned funct; uint64_t rs1, rs2; } command_t;
static command_t sent[32];
static unsigned count, expected, patches, failed;
static void record(unsigned funct, uint64_t rs1, uint64_t rs2) {
  sent[count++] = (command_t){funct, rs1, rs2};
}
#undef ROCC_INSTRUCTION_0_R_R
#define ROCC_INSTRUCTION_0_R_R(x, rs1, rs2, funct) record(funct, (uintptr_t)(rs1), (uint64_t)(rs2));
#include "gemmini.h"
#include "gemmini_types.h"
#include "cgra_link_control_generated.h"
#define GEMMINI_JOB_H
static void gemmini_job_write_at(uintptr_t control, uintptr_t offset, uint32_t value) {
  (void)control; (void)offset; (void)value;
}
static void gemmini_job_patch_at(uintptr_t control, const gemmini_patch_t *patch) {
  (void)control; (void)patch;
}
static int gemmini_job_capture_at(uintptr_t control, unsigned job, unsigned commands, unsigned updates) {
  (void)control; (void)job;
  expected = commands;
  patches = updates;
  return failed;
}
static int gemmini_job_end_at(uintptr_t control) { (void)control; return count != expected; }
#include "gemmini_conv.h"

int main(void) {
  uint64_t control[512] = {0};
  elem_t input[64], weight[144], output[64];
  gemmini_conv_t p = {.input_rows=4, .input_columns=4, .input_channels=4,
    .output_rows=4, .output_columns=4, .output_channels=4,
    .kernel=3, .stride=1, .dilation=1, .padding_top=1, .padding_left=1,
    .input=input, .weight=weight, .output=output, .activation=NO_ACTIVATION,
    .scale=ACC_SCALE_IDENTITY};
  for (unsigned trial=0; trial<3; ++trial) {
    if (trial) {
      p.output_rows=p.output_columns=2;
      p.stride=2;
      p.padding_top=p.padding_left=0;
      p.kernel=trial == 1 ? 3 : 1;
    }
    unsigned outputs=p.output_rows*p.output_columns*p.output_channels;
    unsigned weights=p.kernel*p.kernel*p.input_channels*p.output_channels;
    if (!gemmini_conv_valid(&p,64,weights,outputs) || gemmini_conv_valid(&p,63,weights,outputs)) return 1;
    count=0;
    gemmini_conv_emit(&p);
    if (count != 9) return 2;
    command_t manual[9];
    memcpy(manual,sent,sizeof(manual));
    count=0;
    if (gemmini_conv_capture((uintptr_t)control,2,&p) || patches || memcmp(manual,sent,sizeof(manual))) return 3;
    if (sent[6].funct != k_LOOP_CONV_WS_CONFIG_5 || sent[6].rs2 != (uintptr_t)output || sent[7].rs2 != (uintptr_t)input) return 4;
    if ((sent[2].rs2 & 65535) != p.output_rows || ((sent[2].rs2 >> 32) & 65535) != p.output_columns) return 5;
    const unsigned bottom=p.output_rows*p.stride+(p.kernel-1)*p.dilation-p.padding_top-p.input_rows;
    if (((sent[4].rs2>>24)&255) != bottom) return 6;
    if (trial == 0) {
      count=0;
      if (gemmini_capture_conv_at((uintptr_t)control,2,4,4,4,4,3,1,1,1,4,4,input,weight,NULL,output,NO_ACTIVATION,ACC_SCALE_IDENTITY)
          || patches != 17 || memcmp(manual,sent,sizeof(manual))) return 7;
    }
  }
  failed=1;
  count=0;
  if (!gemmini_conv_capture((uintptr_t)control,2,&p) || count) return 8;
  p.input_rows=p.input_columns=p.output_rows=p.output_columns=64;
  if (gemmini_conv_valid(&p,64*64*4,16,64*64*4)) return 9;
  p.input_rows=0;
  if (gemmini_conv_valid(&p,0,16,64*64*4)) return 10;
  return 0;
}
"""


class RuntimeConvTest(unittest.TestCase):
    def test_complete_native_call(self):
        metadata = yaml.safe_load(
            (ROOT / "configs/kernels/gemmini_conv.yaml").read_text()
        )["runtime"]
        header = (ROOT / "tests/include/gemmini_conv.h").read_text()
        header += (ROOT / "runtime/planning/workspace.h").read_text()
        for field in ("commands", "workspace_divisor"):
            macro = re.search(
                rf"^#define GEMMINI_CONV_{field.upper()} (\d+)$", header, re.MULTILINE
            )
            self.assertIsNotNone(macro)
            self.assertEqual(metadata[field], int(macro.group(1)))
        self.assertEqual(metadata["patches"], 0)
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)
            source = path / "test.c"
            source.write_text(SOURCE)
            binary = path / "test"
            # The native Gemmini header uses signed 1<<31 for accumulator addresses.
            subprocess.run(
                [
                    "cc",
                    "-std=gnu11",
                    "-O2",
                    "-fsanitize=undefined",
                    "-fno-sanitize=shift-base",
                    "-fno-sanitize-recover=all",
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
