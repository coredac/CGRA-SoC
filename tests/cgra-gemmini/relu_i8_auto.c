#include "cgra_dma.h"
#include "cgra_link.h"
#include "cgra_protocol.h"
#include "gemmini.h"
#include "generated/cgra_relu4x4_fast_api.h"

#include <stdint.h>
#include <stdio.h>

enum {
  ELEMENTS = 32,
  OUTPUT_BYTES = ELEMENTS * sizeof(acc_t),
  PUBLICATION_ROWS = ELEMENTS / DIM,
  PUBLICATION_ROW = BANK_NUM * BANK_ROWS - PUBLICATION_ROWS,
  OUTPUT_TAG = 0x91,
};

static elem_t A[DIM][DIM] row_align(1);
static elem_t B[DIM][DIM] row_align(1);
static acc_t output[ELEMENTS] __attribute__((aligned(16)));
static const uint32_t ACC_ADDRESS = (uint32_t)1 << (ADDR_LEN - 1);

static void init_inputs(void) {
  for (unsigned row = 0; row < DIM; ++row) {
    for (unsigned column = 0; column < DIM; ++column) {
      A[row][column] = (elem_t)((int)((row * DIM + column) % ELEMENTS) - ELEMENTS / 2);
      B[row][column] = row == column ? 1 : 0;
    }
  }
  for (unsigned index = 0; index < ELEMENTS; ++index) {
    output[index] = (acc_t)0x5a5a5a5a;
  }
}

static void configure_cgra(void) {
  cgra_link_configure(RELU4X4_FAST_PACKET_COUNT, 1);
  for (unsigned index = 0; index < RELU4X4_FAST_CONFIG_PACKET_COUNT; ++index) {
    cgra_link_queue(RELU4X4_FAST_CONFIG_PACKETS[index]);
  }
  for (unsigned index = 0; index < RELU4X4_FAST_LAUNCH_PACKET_COUNT; ++index) {
    cgra_link_queue(RELU4X4_FAST_LAUNCH_PACKETS[index]);
  }
}

static void run_gemmini(void) {
  gemmini_flush(0);
  gemmini_config_ld(DIM * sizeof(elem_t));
  gemmini_config_ex(WEIGHT_STATIONARY, NO_ACTIVATION, 0);
  gemmini_config_st(DIM * sizeof(elem_t));
  gemmini_mvin(A, 0);
  gemmini_mvin(B, DIM);
  gemmini_fence();
  gemmini_preload(DIM, ACC_ADDRESS);
  gemmini_compute_preloaded(0, GARBAGE_ADDR);
  gemmini_extended_mvout_spad(PUBLICATION_ROW, 1, ACC_ADDRESS, DIM, PUBLICATION_ROWS);
}

static int verify_output(void) {
  int failures = 0;
  for (unsigned index = 0; index < ELEMENTS; ++index) {
    const int32_t input = (int32_t)A[index / DIM][index % DIM];
    const int32_t expected = input > 0 ? input : 0;
    if (output[index] != expected) {
      printf("CGRA ReLU mismatch index=%u actual=%d expected=%d\n", index, (int)output[index], (int)expected);
      ++failures;
    }
  }
  return failures;
}

static int run_pipeline(void) {
  const cgra_link_result_t result = cgra_link_wait();
  if (result.status != AUTO_LINK_STATUS_SUCCESS || result.detail != 0 || result.data != 0) {
    printf("AutoLink result mismatch status=%u detail=%u data=%u\n", (unsigned)result.status, (unsigned)result.detail, (unsigned)result.data);
    return 1;
  }
  cgra_dma_mvout_async(output, CGRA_DMA_DESC_CONST(0, OUTPUT_BYTES, OUTPUT_TAG));
  if (cgra_dma_wait(OUTPUT_TAG) != OUTPUT_TAG) {
    printf("CGRA output DMA tag mismatch\n");
    return 1;
  }
  cgra_dma_memory_fence();
  return verify_output();
}

int main(void) {
  init_inputs();
  configure_cgra();
  run_gemmini();
  const int failures = run_pipeline();
  if (failures != 0) {
    printf("Gemmini + CGRA Auto packed INT8 ReLU: FAIL (%d)\n", failures);
    return 1;
  }
  printf("Gemmini + CGRA Auto packed INT8 ReLU: PASS\n");
  return 0;
}
