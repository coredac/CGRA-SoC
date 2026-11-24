#include "accel_generated.h"
#include "asic.h"
#include "auto_link.h"
#include "cgra_link.h"

#include <stdio.h>

static int32_t left[128] __attribute__((aligned(64)));
static int32_t right[128] __attribute__((aligned(64)));
static int32_t output[128] __attribute__((aligned(64)));

static unsigned long cycles(void) {
  unsigned long value;
  __asm__ volatile("rdcycle %0" : "=r"(value) : : "memory");
  return value;
}

int main(void) {
  const asic_job_t add = {.unit = 1, .operation = 1, .count = 128, .src0 = 0, .src1 = 128, .dst = 256, .next_job = 2};
  const asic_job_t relu = {.unit = 1, .count = 128, .src0 = 256, .dst = 512};
  const auto_link_stage_t stage = {.endpoint = ASIC0.id, .job = 0, .output = {.address = {.binding = -1}}};
  const auto_link_edge_t edges[] = {
      {.destination = 0, .flags = 7, .bytes = sizeof(left), .address = {.value = (uintptr_t)left, .binding = -1}, .destination_offset = 0},
      {.destination = 0, .flags = 7, .bytes = sizeof(right), .address = {.value = (uintptr_t)right, .binding = -1}, .destination_offset = 128 * sizeof(int32_t)},
  };
  const auto_link_graph_t graph = {.stage_count = 1, .edge_count = 2, .stage_capacity = 16, .edge_capacity = 24, .stages = &stage, .edges = edges};
  for (unsigned run = 0; run < 2; ++run) {
    const unsigned long begin = cycles();
    for (unsigned i = 0; i < 128; ++i) {
      left[i] = (int)i - 80 + (int)run;
      right[i] = 31 - (int)(i % 17);
    }
    asic_fence();
    if (!run) {
      if (asic_config(ASIC0, 0, &add) || asic_config(ASIC0, 1, &relu)) {
        printf("ASIC AUTO: configuration failed\n");
        return 1;
      }
      auto_link_load(&graph, NULL);
      auto_link_tiles(1, 128, 1, 128);
    }
    auto_link_input_ready();
    const cgra_link_result_t result = cgra_link_wait_at(CGRA0.control);
    if (result.status || asic_store(ASIC0, output, 512, 128)) {
      printf("ASIC AUTO: execution failed status=%u\n", result.status);
      return 1;
    }
    const unsigned long elapsed = cycles() - begin;
    unsigned errors = 0;
    const volatile int32_t *published = (const volatile int32_t *)(ASIC0.spm + 512 * sizeof(int32_t));
    for (unsigned i = 0; i < 128; ++i) {
      const int32_t sum = left[i] + right[i];
      const int32_t expected = sum < 0 ? 0 : sum;
      if (output[i] != expected || published[i] != expected) {
        ++errors;
      }
    }
    printf("ASIC AUTO phase=%s cycles=%lu checked=128 mismatches=%u root_copies=2 cpu_submissions=1 internal_starts=2\n", run ? "warm" : "cold", elapsed, errors);
    if (errors) {
      printf("ASIC AUTO: FAIL\n");
      return 1;
    }
  }
  printf("ASIC AUTO: PASS\n");
  return 0;
}
