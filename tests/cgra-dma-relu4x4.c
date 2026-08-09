// CGRA RoCC 4x4 ReLU test with DMA-only SPM data movement.

#include "cgra_dma.h"
#include "cgra_protocol.h"
#include "cgra_runtime.h"
#include "generated/cgra_relu4x4_fast_api.h"
#include <stdint.h>
#include <stdio.h>

enum {
  RELU4X4_EXPECTED_COMPLETES = 1,
  RELU4X4_EXPECTED_RESULT = 0,
  RELU4X4_ELEMENT_COUNT = 32,
  RELU4X4_SPM_WORD_ADDR = 0,
  RELU4X4_MVIN_TAG = 0x31,
  RELU4X4_MVOUT_TAG = 0x52,
  RELU4X4_SENTINEL = 0x5a5a5a5a,
};

static int32_t input[RELU4X4_ELEMENT_COUNT] __attribute__((aligned(16)));
static int32_t output[RELU4X4_ELEMENT_COUNT] __attribute__((aligned(16)));

static const cgra_dma_desc_t MVIN_DESC = CGRA_DMA_DESC_CONST(
    RELU4X4_SPM_WORD_ADDR, sizeof(input), RELU4X4_MVIN_TAG);
static const cgra_dma_desc_t MVOUT_DESC = CGRA_DMA_DESC_CONST(
    RELU4X4_SPM_WORD_ADDR, sizeof(output), RELU4X4_MVOUT_TAG);

int main(void) {
  uint64_t wait_result = 0;
  uint64_t status = 0;
  uint64_t result = 0;

  for (int i = 0; i < RELU4X4_ELEMENT_COUNT; ++i) {
    input[i] = i - 16;
    output[i] = (int32_t)RELU4X4_SENTINEL;
  }
  cgra_dma_memory_fence();

  cgra_dma_mvin_async(input, MVIN_DESC);
  CGRA_SET_EXPECTED_COMPLETES(RELU4X4_EXPECTED_COMPLETES);
  load_relu4x4_config_fast();
  uint8_t observed_mvin_tag = cgra_dma_wait(RELU4X4_MVIN_TAG);
  if (observed_mvin_tag != RELU4X4_MVIN_TAG) {
    printf("CGRA DMA ReLU4x4: MVIN tag mismatch expected=%u observed=%u\n",
           RELU4X4_MVIN_TAG, observed_mvin_tag);
    return 1;
  }

  launch_relu4x4_fast();
  CGRA_WAIT(wait_result);
  CGRA_STATUS(status);
  CGRA_RESULT(result);

  uint64_t complete = status & UINT64_C(1);
  uint64_t complete_count = (status >> 1) & UINT64_C(0xffff);
  if (wait_result != 1 || complete != 1 ||
      complete_count != RELU4X4_EXPECTED_COMPLETES ||
      result != RELU4X4_EXPECTED_RESULT) {
    printf("CGRA DMA ReLU4x4: CGRA completion failure "
           "wait=%lu status=0x%lx result=%lu\n",
           wait_result, status, result);
    return 1;
  }

  cgra_dma_mvout_async(output, MVOUT_DESC);
  uint8_t observed_mvout_tag = cgra_dma_wait(RELU4X4_MVOUT_TAG);
  if (observed_mvout_tag != RELU4X4_MVOUT_TAG) {
    printf("CGRA DMA ReLU4x4: MVOUT tag mismatch expected=%u observed=%u\n",
           RELU4X4_MVOUT_TAG, observed_mvout_tag);
    return 1;
  }

  cgra_dma_memory_fence();
  int output_failures = 0;
  for (int i = 0; i < RELU4X4_ELEMENT_COUNT; ++i) {
    int32_t expected = input[i] > 0 ? input[i] : 0;
    if (output[i] != expected) {
      printf("CGRA DMA ReLU4x4: output[%d]=%d expected=%d\n",
             i, output[i], expected);
      ++output_failures;
    }
  }
  if (output_failures != 0) {
    printf("CGRA DMA ReLU4x4: %d output mismatches\n", output_failures);
    return 1;
  }

  printf("CGRA DMA ReLU4x4: PASS\n");
  return 0;
}
