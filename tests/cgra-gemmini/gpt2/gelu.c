#include "block_cgra.h"
#include "generated/gelu_data.h"

#include <stdint.h>
#include <stdio.h>

enum { ELEMENTS = 2053, SPM_ELEMENTS = 257 };
static int8_t input[ELEMENTS] __attribute__((aligned(64)));
static int8_t output[ELEMENTS] __attribute__((aligned(64)));
static int8_t zeros[SPM_ELEMENTS] __attribute__((aligned(64)));

static int run(unsigned index, const int8_t *source, int8_t *destination, unsigned count) {
  block_perf_reset();
  const unsigned long begin = block_clock();
  const int status = block_activation(source, destination, count);
  const unsigned long elapsed = block_clock() - begin;
  printf("GELU run=%u elements=%u cycles=%lu config=%lu execute_wait=%lu dma_in=%lu dma_out=%lu launches=%u\n", index, count, elapsed, block_perf.config, block_perf.execute, block_perf.load,
         block_perf.store, block_perf.launches);
  printf("GELU DMA bytes dram_read=%lu dram_write=%lu spm_read=%lu spm_write=%lu\n", block_perf.dram_read, block_perf.dram_write, block_perf.spm_read, block_perf.spm_write);
  unsigned errors = 0;
  for (unsigned offset = 0; offset < count; ++offset) {
    const int8_t expected = gelu_expected[(int)input[offset] + 128];
    const int8_t actual = ((volatile int8_t *)destination)[offset];
    if (actual != expected) {
      if (errors < 8) {
        printf("GELU mismatch run=%u index=%u expected=%d actual=%d\n", index, offset, expected, actual);
      }
      ++errors;
    }
  }
  printf("GELU run=%u mismatches=%u\n", index, errors);
  return status || errors;
}

int main(void) {
  for (unsigned index = 0; index < ELEMENTS; ++index) {
    input[index] = (int)(index % 256) - 128;
    output[index] = INT8_MIN;
  }
  if (run(0, input, output, ELEMENTS)) {
    printf("GELU: FAIL\n");
    return 1;
  }
  for (unsigned index = 0; index < SPM_ELEMENTS; ++index) {
    input[index] = 127 - (int)(index % 256);
  }
  int8_t *spm_input = (int8_t *)(GEMMINI0.spm + 64 * 1024);
  int8_t *spm_output = (int8_t *)(GEMMINI1.spm + 64 * 1024);
  // Switch programs and publish a changed input without a CPU payload copy.
  if (block_add(input, zeros, spm_input, SPM_ELEMENTS) || run(1, spm_input, spm_output, SPM_ELEMENTS) || run(2, spm_input, spm_input, SPM_ELEMENTS)) {
    printf("GELU: FAIL\n");
    return 1;
  }
  printf("GELU: PASS\n");
  return 0;
}
