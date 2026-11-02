"""Check CGRA packet ordering and preload target selection."""

import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

MOCK = r"""
#include "soc_runtime.h"
#define CGRA_LAYOUT_H
#define CGRA_PROTOCOL_H
#define CGRA_INTRA_PKT_NBITS 256
#define DATA_PAYLOAD_NBITS 32
#define DATA_PAYLOAD_LSB 0
#define PKT_DATA_LSB PAYLOAD_SHIFT
#define CGRA_DMA_DESC_SPM_ADDR_LSB 0
#define CGRA_DMA_DESC_NBYTES_LSB 16
#define CGRA_DMA_DESC_TAG_LSB 48
#define CGRA_LINK_CONTROL_CONFIG_DONE 0
#define CGRA_LINK_CONTROL_CONFIG_STATUS 1
#define AUTO_LINK_STATUS_SUCCESS 0
typedef uint64_t cgra_dma_desc_t;
static cgra_packet_t sent[32], pending;
static unsigned sent_count, selections, expected_count, dma_waits, dma_loads, fail_dma;
static unsigned static_packets, full_captures, fail_capture, fail_config;
#undef accel_commands
#define accel_commands(device, ...) do { (void)(device); ++selections; __VA_ARGS__ } while (0)
#define CGRA_RAW_PKT_LO(value) (pending.lo = (value))
#define CGRA_RAW_PKT_MID(value) (pending.mid = (value))
#define CGRA_RAW_PKT_HI(value) (pending.hi = (value))
#define CGRA_RAW_PKT_TOP(value) do { pending.top = (value); sent[sent_count++] = pending; } while (0)
#define CGRA_SET_EXPECTED_COMPLETES(value) (expected_count = (value))
#define CGRA_WAIT(value) ((value) = 1)
#define CGRA_STATUS(value) ((value) = 1 | (expected_count << 1))
#define CGRA_RESULT(value) ((value) = 0)
#include "cgra_runtime.h"
static void cgra_dma_mvin_async(const void *source, cgra_dma_desc_t desc) { (void)source; (void)desc; ++dma_loads; }
static void cgra_dma_mvin_i8_async(const void *source, cgra_dma_desc_t desc) { cgra_dma_mvin_async(source, desc); }
static uint8_t cgra_dma_wait(uint8_t tag) { ++dma_waits; return dma_waits == fail_dma ? tag ^ 1 : tag; }
void cgra_dma_mvout_async(void *, cgra_dma_desc_t);
void cgra_dma_mvout_i8_async(void *, cgra_dma_desc_t);
int cgra_job_full_at(uintptr_t, uint32_t, const cgra_kernel_t *, const cgra_link_symbol_t *, uint32_t);
int cgra_job_config_at(uintptr_t, uint32_t, const cgra_kernel_t *, const cgra_link_symbol_t *);
uint32_t cgra_link_read_at(uintptr_t, uintptr_t);
"""

SOURCE = r"""
#include <stdio.h>
#include <string.h>
#include "runtime/src/cgra.c"

int cgra_job_full_at(uintptr_t base, uint32_t job, const cgra_kernel_t *kernel,
                     const cgra_link_symbol_t *symbols, uint32_t next) {
  (void)base; (void)job; (void)symbols; (void)next;
  ++full_captures;
  return kernel->static_count != 2 || fail_capture;
}

int cgra_job_config_at(uintptr_t base, uint32_t job, const cgra_kernel_t *kernel,
                       const cgra_link_symbol_t *symbols) {
  (void)base; (void)job; (void)symbols;
  static_packets += kernel->static_count;
  return fail_capture;
}

uint32_t cgra_link_read_at(uintptr_t base, uintptr_t offset) {
  (void)base;
  return offset == CGRA_LINK_CONTROL_CONFIG_DONE ? 1 : fail_config;
}

static int reference(const soc_task_t *task, unsigned tile, cgra_run_t mode) {
  cgra_kernel_t kernel = *task->kernel;
  cgra_packet_t packets[16];
  memcpy(packets, kernel.config_packets, kernel.config_count * sizeof(*packets));
  memcpy(packets + kernel.config_count, kernel.launch_packets, kernel.launch_count * sizeof(*packets));
  for (unsigned index = 0; index < kernel.patch_count; ++index) {
    const cgra_patch_t patch = kernel.patches[index];
    const cgra_link_symbol_t binding = task->symbols[patch.symbol_index];
    uint32_t value = binding.base;
    if (binding.source == CGRA_LINK_TILE_ID) value += tile * binding.stride;
    if (binding.source == CGRA_LINK_ELEMENTS) value = task->elements;
    value = value * patch.scale + patch.offset;
    cgra_packet_t *packet = &packets[patch.packet_index];
    uint64_t words[] = {packet->lo, packet->mid, packet->hi, packet->top};
    for (unsigned bit = 0; bit < 32; ++bit) {
      unsigned position = PAYLOAD_SHIFT + bit;
      uint64_t mask = UINT64_C(1) << (position % 64);
      words[position / 64] = (words[position / 64] & ~mask) | ((uint64_t)((value >> bit) & 1) << (position % 64));
    }
    *packet = (cgra_packet_t){words[0], words[1], words[2], words[3]};
  }
  kernel.config_packets = packets;
  kernel.launch_packets = packets + kernel.config_count;
  const unsigned begin = sent_count;
  // Keep the original three-section sequence independent of the shared helper.
  if (mode != CGRA_COLD) cgra_send_packets_fast(packets, kernel.rearm_count);
  if (mode != CGRA_REPEAT) cgra_send_packets_fast(packets + kernel.rearm_count, kernel.setup_count);
  const unsigned first = kernel.rearm_count + kernel.setup_count;
  cgra_send_packets_fast(packets + first, kernel.config_count - first);
  cgra_start(&kernel);
  cgra_packet_t expected[32];
  const unsigned count = sent_count;
  memcpy(expected, sent, count * sizeof(*sent));
  sent_count = begin;
  cgra_prepare(&kernel, mode);
  cgra_start(&kernel);
  return sent_count != count || memcmp(sent, expected, count * sizeof(*sent)) != 0;
}

int main(void) {
  const cgra_packet_t original[] = {
    {1, 11, 21, 31}, {2, 12, 22, 32}, {3, 13, 23, 33},
    {4, 14, 24, 34}, {5, 15, 25, 35}, {6, 16, 26, 36},
    {7, 17, 27, 37}, {8, 18, 28, 38}
  };
  const cgra_patch_t patches[] = {{0, 0, 1, 0}, {2, 0, 2, 3}, {4, 1, 5, 10}, {6, 2, 7, 9}, {7, 0, 3, 1}};
  const cgra_link_symbol_t symbols[] = {{17, 32, CGRA_LINK_TILE_ID}, {0, 0, CGRA_LINK_ELEMENTS}, {128, 64, CGRA_LINK_SLOT}};
  cgra_kernel_t kernel = {
    .static_packets = original, .static_count = 1,
    .config_packets = original, .config_count = 6,
    .rearm_count = 2, .setup_count = 2,
    .launch_packets = original + 6, .launch_count = 2, .expected_completes = 3,
    .patches = patches, .patch_count = 5
  };
  const accel_t device = {.id = 2};
  const accel_t *devices[] = {&device};
  const soc_plan_t plan = {.device_count = 1, .devices = devices};
  const soc_dma_t dma = {.bytes = 4, .tag = 7};
  soc_task_t task = {.kernel = &kernel, .symbols = symbols, .elements = 37, .dmas = &dma};
  soc_task_t other = task;
  soc_state_t state;
  soc_context_t context = {.plan = &plan, .states = &state};
  for (unsigned patched = 0; patched < 2; ++patched) {
    kernel.patch_count = patched ? 5 : 0;
    for (unsigned mode = CGRA_COLD; mode <= CGRA_SWITCH; ++mode) {
      for (unsigned tile = 0; tile < 3; ++tile) {
        for (unsigned dma_count = 0; dma_count < 2; ++dma_count) {
          task.dma_count = dma_count;
          state = (soc_state_t){.started = mode != CGRA_COLD, .last = mode == CGRA_REPEAT ? &task : &other, .loaded = mode == CGRA_REPEAT ? &kernel : NULL};
          sent_count = 0;
          if (state.loaded != &kernel) cgra_send_packets_fast(kernel.static_packets, kernel.static_count);
          if (reference(&task, tile, mode)) {
            fprintf(stderr, "native preparation mismatch mode=%u\n", mode);
            return 1;
          }
          cgra_packet_t expected[32];
          unsigned count = sent_count;
          memcpy(expected, sent, count * sizeof(*sent));
          sent_count = selections = dma_waits = expected_count = 0;
          if (soc_cgra_execute(&context, &task, tile) != 0 || sent_count != count || memcmp(sent, expected, count * sizeof(*sent)) != 0 || selections != 1 || expected_count != 3 || dma_waits != dma_count || state.last != &task || state.loaded != &kernel || !state.started) {
            fprintf(stderr, "stream mismatch mode=%u tile=%u dma=%u patched=%u\n", mode, tile, dma_count, patched);
            return 1;
          }
        }
      }
    }
  }
  const soc_dma_t preloads[] = {{.bytes = 4, .tag = 3}, {.bytes = 8, .tag = 4, .packed = 1}, {.bytes = 4, .tag = 5}};
  task.preloads = preloads;
  for (unsigned count = 0; count <= 3; ++count) {
    task.preload_count = count;
    state = (soc_state_t){.started = 1, .last = &task, .loaded = &kernel};
    selections = dma_waits = dma_loads = 0;
    if (soc_cgra_init(&context, &task) != 0 || selections != (count != 0) || dma_loads != count || dma_waits != count || !state.started || state.last || state.loaded) {
      fprintf(stderr, "preload mismatch count=%u\n", count);
      return 1;
    }
  }
  fail_dma = 2;
  selections = dma_waits = dma_loads = 0;
  if (soc_cgra_init(&context, &task) == 0 || selections != 1 || dma_loads != 2 || dma_waits != 2) {
    fprintf(stderr, "preload did not stop after DMA failure\n");
    return 1;
  }
  fail_dma = 0;
  task.preload_count = 0;
  cgra_kernel_t alternate = kernel;
  alternate.static_count = 2;
  other.kernel = &alternate;
  soc_task_t full = other;
  full.full_config = 1;
  soc_task_t remote = task;
  remote.endpoint = 1;
  const accel_t second = {.id = 3};
  const accel_t *pair[] = {&device, &second};
  const soc_plan_t paired = {.device_count = 2, .devices = pair};
  soc_state_t states[2] = {0};
  context.plan = &paired;
  context.states = states;
  const struct {
    const soc_task_t *task;
    unsigned packets, full, invalidate;
  } captures[] = {
    {&task, 1, 0, 0}, {&task, 1, 0, 0},
    {&other, 3, 0, 0}, {&task, 4, 0, 0},
    {&full, 4, 1, 0}, {&task, 4, 1, 0},
    {&task, 5, 1, 1}, {&task, 6, 1, 2},
    {&remote, 7, 1, 0}, {&task, 7, 1, 0}
  };
  for (unsigned index = 0; index < sizeof(captures) / sizeof(*captures); ++index) {
    if (captures[index].invalidate == 1) complete(&context, &full);
    if (captures[index].invalidate == 2 && soc_cgra_init(&context, &task)) return 1;
    if (soc_cgra_capture(&context, captures[index].task) ||
        static_packets != captures[index].packets || full_captures != captures[index].full) {
      fprintf(stderr, "resident capture mismatch step=%u packets=%u full=%u\n", index, static_packets, full_captures);
      return 1;
    }
  }
  for (unsigned failure = 0; failure < 2; ++failure) {
    fail_capture = failure == 0;
    fail_config = failure == 1;
    if (!soc_cgra_capture(&context, &other) || states[0].loaded) {
      fprintf(stderr, "failed capture retained residency failure=%u\n", failure);
      return 1;
    }
    fail_capture = fail_config = 0;
    const unsigned before = static_packets;
    if (soc_cgra_capture(&context, &other) || static_packets != before + 2 || states[0].loaded != &alternate) return 1;
    if (soc_cgra_capture(&context, &task) || states[0].loaded != &kernel) return 1;
  }
  return 0;
}
"""


class RuntimeCgraTest(unittest.TestCase):
    def test_stream_matches_copy(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)
            (path / "cgra_dma.h").write_text(MOCK)
            (path / "cgra_link.h").write_text("")
            source = path / "test.c"
            source.write_text(SOURCE)
            binary = path / "test"
            for shift in (49, 80):
                with self.subTest(payload_shift=shift):
                    subprocess.run(
                        [
                            "cc",
                            "-std=c11",
                            "-Wall",
                            "-Wextra",
                            "-Werror",
                            "-ffunction-sections",
                            "-Wl,--gc-sections",
                            f"-DPAYLOAD_SHIFT={shift}",
                            f"-I{path}",
                            f"-I{ROOT}",
                            f"-I{ROOT / 'runtime/include'}",
                            f"-I{ROOT / 'tests/include'}",
                            str(source),
                            "-o",
                            str(binary),
                        ],
                        check=True,
                        capture_output=True,
                        text=True,
                    )
                    subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    unittest.main()
