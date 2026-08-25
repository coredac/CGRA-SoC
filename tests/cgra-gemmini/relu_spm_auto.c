#include "cgra_dma.h"
#include "cgra_link.h"
#include "cgra_protocol.h"
#include "gemmini.h"
#include "generated/cgra_relu4x4_fast_api.h"

#include <stdint.h>
#include <stdio.h>

enum {
  CGRA_WORD_COUNT = 32,
  CGRA_TRANSFER_BYTES = CGRA_WORD_COUNT * sizeof(acc_t),
  GEMMINI_FULL_WIDTH_ROW_BYTES = DIM * sizeof(acc_t),
  GEMMINI_FULL_WIDTH_ROW_STRIDE = sizeof(acc_t) / sizeof(elem_t),
  PUBLICATION_ROWS = CGRA_TRANSFER_BYTES / GEMMINI_FULL_WIDTH_ROW_BYTES,
  PUBLICATION_ROW =
      BANK_NUM * BANK_ROWS - PUBLICATION_ROWS * GEMMINI_FULL_WIDTH_ROW_STRIDE,
  CGRA_SPM_WORD_ADDR = 0,
  OUTPUT_DMA_TAG = 0x91,
};

static elem_t A[DIM][DIM] row_align(1);
static elem_t B[DIM][DIM] row_align(1);
static acc_t cgra_output[CGRA_WORD_COUNT] __attribute__((aligned(16)));

static const cgra_dma_desc_t OUTPUT_DESCRIPTOR = CGRA_DMA_DESC_CONST(
    CGRA_SPM_WORD_ADDR, CGRA_TRANSFER_BYTES, OUTPUT_DMA_TAG);

static const uint32_t ACCUMULATOR_WRITE_ADDRESS = (uint32_t)1 << (ADDR_LEN - 1);
static const uint32_t ACCUMULATOR_FULL_WIDTH_ADDRESS =
    ((uint32_t)1 << (ADDR_LEN - 1)) | ((uint32_t)1 << (ADDR_LEN - 3));

static void init_inputs(void) {
  for (int i = 0; i < DIM; ++i) {
    for (int j = 0; j < DIM; ++j) {
      const int index = i * DIM + j;
      A[i][j] = (elem_t)(index % CGRA_WORD_COUNT - CGRA_WORD_COUNT / 2);
      B[i][j] = i == j ? (elem_t)1 : (elem_t)0;
    }
  }
  for (unsigned index = 0; index < CGRA_WORD_COUNT; ++index) {
    cgra_output[index] = (acc_t)0x5a5a5a5a;
  }
}

static void run_gemmini(void) {
  const uint32_t A_addr = 0;
  const uint32_t B_addr = DIM;

  gemmini_flush(0);
  gemmini_config_ld(DIM * sizeof(elem_t));
  gemmini_config_ex(WEIGHT_STATIONARY, NO_ACTIVATION, 0);
  gemmini_config_st(DIM * sizeof(acc_t));
  gemmini_mvin(A, A_addr);
  gemmini_mvin(B, B_addr);
  gemmini_preload(B_addr, ACCUMULATOR_WRITE_ADDRESS);
  gemmini_compute_preloaded(A_addr, GARBAGE_ADDR);
  gemmini_extended_mvout_spad(PUBLICATION_ROW, GEMMINI_FULL_WIDTH_ROW_STRIDE,
                              ACCUMULATOR_FULL_WIDTH_ADDRESS, DIM,
                              PUBLICATION_ROWS);
}

static void configure_cgra(void) {
  load_relu4x4_config_fast();
  cgra_link_configure(RELU4X4_FAST_LAUNCH_PACKET_COUNT);
  for (unsigned index = 0; index < RELU4X4_FAST_LAUNCH_PACKET_COUNT; ++index) {
    cgra_link_queue(RELU4X4_FAST_LAUNCH_PACKETS[index]);
  }
}

static int verify_result(cgra_link_result_t result) {
  if (result.status != AUTO_LINK_STATUS_SUCCESS || result.detail != 0 ||
      result.data != 0) {
    printf("AutoLink result mismatch\n");
    return 1;
  }
  return 0;
}

static int verify_cgra_relu_outputs(void) {
  int failures = 0;
  for (unsigned index = 0; index < CGRA_WORD_COUNT; ++index) {
    const int input = (int)A[index / DIM][index % DIM];
    const acc_t expected = input > 0 ? (acc_t)input : (acc_t)0;
    if (cgra_output[index] != expected) {
      printf("CGRA ReLU mismatch index=%u actual=%d expected=%d\n", index,
             (int)cgra_output[index], (int)expected);
      ++failures;
    }
  }
  return failures;
}

static int run_pipeline(void) {
  const cgra_link_result_t result = cgra_link_wait();
  if (verify_result(result) != 0) {
    return 1;
  }

  cgra_dma_mvout_async(cgra_output, OUTPUT_DESCRIPTOR);
  if (cgra_dma_wait(OUTPUT_DMA_TAG) != OUTPUT_DMA_TAG) {
    printf("CGRA DMA MVOUT tag mismatch\n");
    return 1;
  }
  cgra_dma_memory_fence();
  return verify_cgra_relu_outputs();
}

int main(void) {
  init_inputs();
  configure_cgra();
  run_gemmini();

  const int failures = run_pipeline();
  if (failures != 0) {
    printf("Gemmini + CGRA Auto SPM ReLU: FAIL (%d)\n", failures);
    return 1;
  }

  printf("Gemmini + CGRA Auto SPM ReLU: PASS\n");
  return 0;
}
