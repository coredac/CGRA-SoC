#include "cgra_dma.h"
#include "cgra_protocol.h"
#include "cgra_runtime.h"
#include "cgra_spm_window.h"
#include "gemmini_ext_spm.h"
#include "generated/cgra_relu4x4_fast_api.h"
#include "pool.h"
#include "pool_demo.h"

#include <stdint.h>
#include <stdio.h>

enum {
  CGRA_SPM_WORD_ADDR = 0,
  CGRA_EXPECTED_COMPLETES = 1,
  CGRA_INPUT_TAG = 0x20,
};

static elem_t A[POOL_DEMO_A_ROWS][DIM] row_align(1);
static elem_t B[POOL_DEMO_B_ROWS][DIM] row_align(1);
static acc_t output[POOL_DEMO_OUTPUT_CHANNELS] __attribute__((aligned(32)));

static const cgra_dma_desc_t CGRA_INPUT = CGRA_DMA_DESC_CONST(CGRA_SPM_WORD_ADDR, POOL_DEMO_BYTES, CGRA_INPUT_TAG);

static int run_cgra(void) {
  const uintptr_t source = GEMMINI_EXT_SPM_BASE + GEMMINI_EXT_SPM_SIZE_BYTES - POOL_DEMO_BYTES;
  uint64_t wait_result = 0;
  uint64_t status = 0;
  uint64_t result = 0;

  cgra_dma_mvin_async((const void *)source, CGRA_INPUT);
  CGRA_SET_EXPECTED_COMPLETES(CGRA_EXPECTED_COMPLETES);
  load_relu4x4_config_fast();
  if (cgra_dma_wait(CGRA_INPUT_TAG) != CGRA_INPUT_TAG) {
    return 1;
  }
  launch_relu4x4_fast();
  CGRA_WAIT(wait_result);
  CGRA_STATUS(status);
  CGRA_RESULT(result);
  return wait_result != 1 || (status & UINT64_C(1)) != 1 || ((status >> 1) & UINT64_C(0xffff)) != CGRA_EXPECTED_COMPLETES || result != 0;
}

static int run_pool(void) {
  const pool_job_t job = {
      .mode = POOL_MODE_MAX,
      .source = CGRA_SPM_WINDOW_BASE,
      .destination = (uintptr_t)output,
      .input_height = POOL_DEMO_CONV_HEIGHT,
      .input_width = POOL_DEMO_CONV_WIDTH,
      .channels = POOL_DEMO_OUTPUT_CHANNELS,
      .kernel_height = 2,
      .kernel_width = 2,
      .stride_height = 2,
      .stride_width = 2,
      .pad_height = 0,
      .pad_width = 0,
  };
  pool_configure(&job);
  pool_start();
  return pool_wait() != POOL_STATUS_SUCCESS;
}

int main(void) {
  pool_demo_pack(A, B);
  pool_demo_load_gemmini(A, B);
  pool_demo_launch_gemmini();
  gemmini_fence();

  if (run_cgra() != 0 || run_pool() != 0) {
    printf("Gemmini Conv + CGRA ReLU + Pool Manual: FAIL\n");
    return 1;
  }
  const int failures = pool_demo_verify(output);
  if (failures != 0) {
    printf("Gemmini Conv + CGRA ReLU + Pool Manual: FAIL (%d)\n", failures);
    return 1;
  }
  printf("Gemmini Conv + CGRA ReLU + Pool Manual: PASS\n");
  return 0;
}
