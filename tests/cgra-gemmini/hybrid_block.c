#include "block_hybrid.h"

static int8_t output[BA_WORDS] __attribute__((aligned(64))) = {1};

static int check(unsigned run) {
  unsigned errors = 0;
  for (unsigned index = 0; index < sizeof(output); index += sizeof(uint32_t)) {
    const uint32_t actual = ((const volatile uint32_t *)output)[index / sizeof(uint32_t)];
    uint32_t reference;
    memcpy(&reference, block_expected_y[run] + index, sizeof(reference));
    if (actual != reference) {
      if (errors < 8) {
        printf("Hybrid block mismatch index=%u actual=%08x expected=%08x\n", index, actual, reference);
      }
      ++errors;
    }
  }
  printf("Hybrid block run=%u outputs=%u mismatched_words=%u\n", run, (unsigned)sizeof(output), errors);
  return errors != 0;
}

int main(void) {
  const int supported = (BLOCK_N == 64 && BLOCK_HIDDEN == 128 && BLOCK_HEADS == 2 && BLOCK_FF == 512) || (BLOCK_N == 16 && BLOCK_HIDDEN == 768 && BLOCK_HEADS == 12 && BLOCK_FF == 3072);
  if (!supported || BLOCK_D != 64 || BLOCK_RUNS < 2) {
    printf("Hybrid block requires N64 H128 heads2 FF512 or N16 H768 heads12 FF3072, D64 runs>=2\n");
    return 1;
  }
  accel_commands(GEMMINI0, { gemmini_flush(0); });
  accel_commands(GEMMINI1, { gemmini_flush(0); });
  gemmini_fence();
  printf("Hybrid block limits profiles=%u jobs=%u reports=%u\n", BH_PROFILES, BH_JOBS, BH_REPORTS);
  for (unsigned run = 0; run < 2; ++run) {
    const unsigned long begin = block_clock();
    const int failed = bh_execute(run, output);
    const unsigned long end = block_clock();
    if (failed) {
      printf("Hybrid block: FAIL run=%u\n", run);
      return 1;
    }
    bh_print(run, end - begin);
    if (check(run)) {
      printf("Hybrid block: FAIL run=%u\n", run);
      return 1;
    }
  }
  printf("Hybrid block: PASS\n");
  return 0;
}
