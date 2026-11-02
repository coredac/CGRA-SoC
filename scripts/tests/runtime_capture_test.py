"""Check direct full capture against the original merged packet stream."""

import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

PREFIX = r"""
#include <stdio.h>
#include <string.h>
#include "cgra_types.h"
#include "cgra_link_control_generated.h"
#define CGRA_RUNTIME_H
static uint64_t events[256][3];
static unsigned event_count, fail_begin;
static void record(unsigned kind, uintptr_t address, uint64_t value) {
  events[event_count][0] = kind;
  events[event_count][1] = address;
  events[event_count++][2] = value;
}
static uint32_t read_control(uintptr_t base, uintptr_t offset) {
  record(1, base + offset, 0);
  return offset == CGRA_LINK_CONTROL_CONFIG_READY ? 1 : fail_begin;
}
static void write_control(uintptr_t base, uintptr_t offset, uint32_t value) {
  record(2, base + offset, value);
}
#define CGRA_WAIT(value) do { record(3, 0, 0); (value) = 1; } while (0)
#define CGRA_RAW_PKT_LO(value) record(4, 0, value)
#define CGRA_RAW_PKT_MID(value) record(5, 0, value)
#define CGRA_SPM_PKT_HI(value) record(6, 0, value)
#define CGRA_SPM_PKT_TOP(value) record(7, 0, value)
static inline void cgra_send_packets_fast(const cgra_packet_t *packets, size_t count) {
  for (size_t index = 0; index < count; ++index) {
    record(8, 0, packets[index].lo);
  }
}
"""

SUFFIX = r"""
// Keep the former merge and patch-copy algorithm as an independent oracle.
static int merged(uintptr_t base, uint32_t job, const cgra_kernel_t *kernel,
                  const cgra_link_symbol_t *bindings, uint32_t next_job) {
  cgra_kernel_t full = *kernel;
  cgra_packet_t packets[kernel->config_count + kernel->static_count];
  cgra_patch_t patches[kernel->patch_count + 1];
  uint32_t count = 0;
  for (uint32_t index = 0; index < kernel->rearm_count; ++index) {
    packets[count++] = kernel->config_packets[index];
  }
  for (uint32_t index = 0; index < kernel->static_count; ++index) {
    packets[count++] = kernel->static_packets[index];
  }
  for (uint32_t index = kernel->rearm_count; index < kernel->config_count; ++index) {
    packets[count++] = kernel->config_packets[index];
  }
  for (uint32_t index = 0; index < kernel->patch_count; ++index) {
    patches[index] = kernel->patches[index];
    patches[index].packet_index += kernel->static_count;
  }
  full.static_count = 0;
  full.config_packets = packets;
  full.config_count = count;
  full.setup_count += kernel->static_count;
  full.patches = patches;
  return cgra_link_capture_at(base, job, &full, bindings, next_job);
}

int main(void) {
  const cgra_packet_t packets[] = {
    {1, 11, 21, 31}, {2, 12, 22, 32}, {3, 13, 23, 33},
    {4, 14, 24, 34}, {5, 15, 25, 35}, {6, 16, 26, 36},
    {7, 17, 27, 37}, {8, 18, 28, 38}
  };
  const cgra_patch_t patches[] = {
    {0, 0, UINT32_MAX, 9}, {2, 1, 17, UINT32_MAX},
    {4, 2, UINT32_MAX, UINT32_MAX}, {6, 3, 11, 3}, {7, 4, 19, 7}
  };
  const cgra_link_symbol_t symbols[] = {
    {UINT32_MAX, UINT32_MAX, CGRA_LINK_SLOT},
    {UINT32_MAX, UINT32_MAX, CGRA_LINK_ELEMENTS},
    {UINT32_MAX, UINT32_MAX, CGRA_LINK_TILE_ID},
    {37, 53, CGRA_LINK_ROW}, {41, 59, CGRA_LINK_ROWS}
  };
  cgra_kernel_t kernel = {
    .static_packets = packets + 3, .config_packets = packets, .config_count = 6,
    .setup_count = 2, .launch_packets = packets + 6, .launch_count = 2,
    .expected_completes = 3, .patches = patches
  };
  for (unsigned variant = 0; variant < 128; ++variant) {
    kernel.static_count = variant & 1 ? 3 : 0;
    kernel.rearm_count = variant & 2 ? 2 : 0;
    kernel.patch_count = variant & 4 ? 5 : 0;
    const cgra_link_symbol_t *bindings = variant & 8 ? symbols : NULL;
    const uint32_t next_job = variant & 16 ? 7 : 0;
    fail_begin = (variant >> 5) & 1;
    const uintptr_t base = variant & 64 ? 0x28000 : 0x16000;
    const uint32_t job = variant & 64 ? 13 : 0;
    event_count = 0;
    const int expected_status = merged(base, job, &kernel, bindings, next_job);
    uint64_t expected[256][3];
    const unsigned expected_count = event_count;
    memcpy(expected, events, expected_count * sizeof(*events));
    event_count = 0;
    const int status = cgra_job_full_at(base, job, &kernel, bindings, next_job);
    if (status != expected_status || event_count != expected_count ||
        memcmp(events, expected, expected_count * sizeof(*events))) {
      fprintf(stderr, "capture differs variant=%u status=%d/%d events=%u/%u\n",
              variant, status, expected_status, event_count, expected_count);
      return 1;
    }
  }
  return 0;
}
"""


class RuntimeCaptureTest(unittest.TestCase):
    def test_full_stream(self):
        header = (ROOT / "tests/include/cgra_link.h").read_text()
        header = header.replace(
            "*(volatile uint32_t *)(base + offset) = value;",
            "write_control(base, offset, value);",
        ).replace("*(volatile uint32_t *)(base + offset)", "read_control(base, offset)")
        header = header.replace(
            '__asm__ volatile("add zero, %0, zero" : : "r"(ready) : "memory");',
            "(void)ready;",
        )
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)
            source = path / "test.c"
            source.write_text(PREFIX + header + SUFFIX)
            binary = path / "test"
            for width in (192, 256):
                with self.subTest(packet_bits=width):
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
                            f"-DCGRA_INTRA_PKT_NBITS={width}",
                            f"-I{ROOT / 'tests/include'}",
                            str(source),
                            "-o",
                            str(binary),
                        ],
                        check=True,
                    )
                    subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    unittest.main()
