#include "block_cgra.h"
#include "generated/norm_data.h"

#include <stdint.h>
#include <stdio.h>

static int8_t output[NORM_ROWS * NORM_WIDTH] __attribute__((aligned(64))) = {1};

static unsigned check(unsigned run) {
  unsigned errors = 0;
  for (unsigned index = 0; index < NORM_ROWS * NORM_WIDTH; ++index) {
    if (output[index] != norm_expected[run][index]) {
      if (errors < 8) {
        printf("Norm mismatch run=%u row=%u col=%u expected=%d actual=%d\n", run, index / NORM_WIDTH, index % NORM_WIDTH, norm_expected[run][index], output[index]);
      }
      ++errors;
    }
  }
  printf("Norm run=%u outputs=%u mismatches=%u\n", run, NORM_ROWS * NORM_WIDTH, errors);
  return errors;
}

int main(void) {
  printf("Norm maximum layout rows=%u width=%u runs=%u packed_dma_native_bytes=%u\n", NORM_ROWS, NORM_WIDTH, NORM_RUNS, NORM_ROWS * NORM_WIDTH * (unsigned)sizeof(int32_t));
  for (unsigned run = 0; run < NORM_RUNS; ++run) {
    for (unsigned index = 0; index < NORM_ROWS * NORM_WIDTH; ++index) {
      output[index] = INT8_MIN;
    }
    printf("Norm run=%u begin\n", run);
    block_perf_reset();
    const unsigned long begin = block_clock();
    const int status = block_layernorm(norm_input[run], norm_gamma, norm_beta, output, NORM_ROWS, NORM_WIDTH);
    const unsigned long elapsed = block_clock() - begin;
    block_perf_print();
    printf("Norm run=%u cycles=%lu\n", run, elapsed);
    if (status || check(run)) {
      printf("Norm: FAIL run=%u\n", run);
      return 1;
    }
  }
  printf("Norm: PASS\n");
  return 0;
}
