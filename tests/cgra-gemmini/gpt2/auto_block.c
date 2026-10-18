#include "block_auto.h"

#include <stdlib.h>

static int8_t output[BA_EXPANDED] __attribute__((aligned(64))) = {1};

static int check(unsigned mode, unsigned run) {
  const int8_t *expected[] = {block_expected_y[run],    block_x[run],          block_expected_q[run],      block_expected_ln1[run],
                              block_expected_gelu[run], block_expected_u[run], block_expected_concat[run], block_expected_concat[run]};
  const unsigned count = mode == 4 ? BA_EXPANDED : BA_WORDS;
  unsigned errors = 0;
  for (unsigned index = 0; index < count; index += sizeof(uint32_t)) {
    const uint32_t actual = ((const volatile uint32_t *)output)[index / sizeof(uint32_t)];
    unsigned reference_index = index;
    if (mode == 6) {
      // The raw PV fixture is head-major; concat references interleave heads per row.
      const unsigned head = index / (BLOCK_N * BLOCK_D);
      const unsigned element = index % (BLOCK_N * BLOCK_D);
      reference_index = element / BLOCK_D * BLOCK_HIDDEN + head * BLOCK_D + element % BLOCK_D;
    }
    uint32_t reference;
    memcpy(&reference, expected[mode] + reference_index, sizeof(reference));
    if (actual != reference) {
      if (errors < 8) {
        printf("Auto block mismatch index=%u actual=%08x expected=%08x\n", index, actual, reference);
      }
      ++errors;
    }
  }
  printf("Auto block run=%u outputs=%u mismatched_words=%u\n", run, count, errors);
  return errors != 0;
}

int main(int argc, char **argv) {
  const int mode = argc > 1 ? atoi(argv[1]) : 0;
  const int supported = (BLOCK_N == 64 && BLOCK_HIDDEN == 128 && BLOCK_HEADS == 2 && BLOCK_FF == 512) || (BLOCK_N == 16 && BLOCK_HIDDEN == 768 && BLOCK_HEADS == 12 && BLOCK_FF == 3072);
  if (mode < 0 || mode > 7 || !supported || BLOCK_RUNS < 2) {
    printf("Auto block requires mode0..7, N64/H128 or N16/H768, runs>=2\n");
    return 1;
  }
  accel_commands(GEMMINI0, { gemmini_flush(0); });
  accel_commands(GEMMINI1, { gemmini_flush(0); });
  gemmini_fence();
  printf("Auto block mode=%d N=%u hidden=%u heads=%u FF=%u\n", mode, BLOCK_N, BLOCK_HIDDEN, BLOCK_HEADS, BLOCK_FF);
  for (unsigned run = 0; run < 2; ++run) {
    const unsigned long begin = block_clock();
    if (ba_configure(mode, run, output)) {
      printf("Auto block: FAIL configuration run=%u\n", run);
      return 1;
    }
    const unsigned long configured = block_clock();
    const int failed = ba_execute();
    const unsigned long end = block_clock();
    printf("Auto block run=%u profiles=%u reports=%u jobs=%u/%u setup=%lu\n", run, ba_plan.profiles, ba_plan.reports, ba_plan.jobs[0], ba_plan.jobs[1], configured - begin);
    printf("Auto block run=%u execute=%lu total=%lu\n", run, end - configured, end - begin);
    if (failed || check(mode, run)) {
      printf("Auto block: FAIL run=%u\n", run);
      return 1;
    }
  }
  printf("Auto block: PASS\n");
  return 0;
}
