#include "cgra_dma.h"
#include "cgra_protocol.h"
#include "cgra_runtime.h"
#include "gemmini.h"
#include "gemmini_ext_spm.h"
#include "generated/cgra_relu4x4_fast_api.h"

#include <stdint.h>
#include <stdio.h>

enum {
  ELEMENTS = 32,
  OUTPUT_BYTES = ELEMENTS * sizeof(acc_t),
  PUBLICATION_ROWS = ELEMENTS / DIM,
  PUBLICATION_ROW = BANK_NUM * BANK_ROWS - PUBLICATION_ROWS,
  TAIL_WORD = 64,
  INPUT_TAG = 0x10,
  TAIL_TAG = 0x11,
  SHORT_TAG = 0x12,
  OUTPUT_TAG = 0x80,
};

static elem_t A[DIM][DIM] row_align(1);
static elem_t B[DIM][DIM] row_align(1);
static acc_t output[ELEMENTS] __attribute__((aligned(16)));
static const uintptr_t INPUT_ADDRESS = GEMMINI_EXT_SPM_BASE + GEMMINI_EXT_SPM_SIZE_BYTES - ELEMENTS;
static const uint32_t ACC_ADDRESS = (uint32_t)1 << (ADDR_LEN - 1);
static const uint32_t SENTINEL = UINT32_C(0x5a5a5a5a);

static void init_inputs(void) {
  for (unsigned row = 0; row < DIM; ++row) {
    for (unsigned column = 0; column < DIM; ++column) {
      A[row][column] = (elem_t)((int)((row * DIM + column) % ELEMENTS) - ELEMENTS / 2);
      B[row][column] = row == column ? 1 : 0;
    }
  }
  for (unsigned index = 0; index < ELEMENTS; ++index) {
    output[index] = (acc_t)SENTINEL;
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
  gemmini_fence();
}

static int check_tail(unsigned offset, unsigned elements, cgra_dma_desc_t descriptor, uint8_t tag) {
  uint64_t wait_result = 0;
  relu4x4_store_fast(TAIL_WORD + elements, SENTINEL);
  CGRA_WAIT(wait_result);
  if (wait_result != 1) {
    printf("CGRA sentinel store failed\n");
    return 1;
  }
  cgra_dma_mvin_i8_async((const void *)(INPUT_ADDRESS + offset), descriptor);
  if (cgra_dma_wait(tag) != tag) {
    printf("CGRA packed tail DMA tag mismatch\n");
    return 1;
  }
  for (unsigned index = 0; index < elements; ++index) {
    const int32_t expected = (int32_t)A[(offset + index) / DIM][(offset + index) % DIM];
    const int32_t actual = (int32_t)relu4x4_read_mem_fast(TAIL_WORD + index);
    if (actual != expected) {
      printf("CGRA packed tail mismatch count=%u index=%u actual=%d expected=%d\n", elements, index, (int)actual, (int)expected);
      return 1;
    }
  }
  if ((uint32_t)relu4x4_read_mem_fast(TAIL_WORD + elements) != SENTINEL) {
    printf("CGRA packed tail overwrote sentinel count=%u\n", elements);
    return 1;
  }
  return 0;
}

static int run_cgra(void) {
  uint64_t wait_result = 0;
  uint64_t status = 0;
  uint64_t result = 0;

  if (check_tail(1, 19, CGRA_DMA_I8_DESC_CONST(TAIL_WORD, 19, TAIL_TAG), TAIL_TAG) != 0 || check_tail(15, 3, CGRA_DMA_I8_DESC_CONST(TAIL_WORD, 3, SHORT_TAG), SHORT_TAG) != 0) {
    return 1;
  }
  cgra_dma_mvin_i8_async((const void *)INPUT_ADDRESS, CGRA_DMA_I8_DESC_CONST(0, ELEMENTS, INPUT_TAG));
  CGRA_SET_EXPECTED_COMPLETES(1);
  load_relu4x4_config_fast();
  if (cgra_dma_wait(INPUT_TAG) != INPUT_TAG) {
    printf("CGRA packed input DMA tag mismatch\n");
    return 1;
  }
  launch_relu4x4_fast();
  CGRA_WAIT(wait_result);
  CGRA_STATUS(status);
  CGRA_RESULT(result);
  if (wait_result != 1 || (status & UINT64_C(1)) != 1 || ((status >> 1) & UINT64_C(0xffff)) != 1 || result != 0) {
    printf("CGRA completion failure\n");
    return 1;
  }
  cgra_dma_mvout_async(output, CGRA_DMA_DESC_CONST(0, OUTPUT_BYTES, OUTPUT_TAG));
  if (cgra_dma_wait(OUTPUT_TAG) != OUTPUT_TAG) {
    printf("CGRA output DMA tag mismatch\n");
    return 1;
  }
  cgra_dma_memory_fence();
  return 0;
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

int main(void) {
  init_inputs();
  run_gemmini();
  if (run_cgra() != 0 || verify_output() != 0) {
    printf("Gemmini + CGRA packed INT8 ReLU: FAIL\n");
    return 1;
  }
  printf("Gemmini + CGRA packed INT8 ReLU: PASS\n");
  return 0;
}
