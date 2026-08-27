#include "cgra_dma.h"
#include "cgra_protocol.h"
#include "cgra_runtime.h"
#include "cgra_spm_window.h"
#include "gemmini.h"
#include "gemmini_ext_spm.h"
#include "generated/cgra_relu4x4_fast_api.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

int Aes256Accel(bool encrypt, const unsigned char *data, size_t data_length, uint64_t key0, uint64_t key1, uint64_t key2, uint64_t key3, unsigned char *result);

enum {
  CGRA_WORD_COUNT = 32,
  TRANSFER_BYTES = CGRA_WORD_COUNT * sizeof(acc_t),
  GEMMINI_FULL_WIDTH_ROW_BYTES = DIM * sizeof(acc_t),
  GEMMINI_FULL_WIDTH_ROW_STRIDE = sizeof(acc_t) / sizeof(elem_t),
  PUBLICATION_ROWS = TRANSFER_BYTES / GEMMINI_FULL_WIDTH_ROW_BYTES,
  PUBLICATION_ROW = BANK_NUM * BANK_ROWS - PUBLICATION_ROWS * GEMMINI_FULL_WIDTH_ROW_STRIDE,
  CGRA_SPM_WORD_ADDR = 0,
  CGRA_EXPECTED_COMPLETES = 1,
  INPUT_DMA_TAG = 0x10,
};

static elem_t A[DIM][DIM] row_align(1);
static elem_t B[DIM][DIM] row_align(1);
static uint8_t ciphertext[TRANSFER_BYTES] __attribute__((aligned(32)));

static const cgra_dma_desc_t INPUT_DESCRIPTOR = CGRA_DMA_DESC_CONST(CGRA_SPM_WORD_ADDR, TRANSFER_BYTES, INPUT_DMA_TAG);
static const uint32_t ACCUMULATOR_WRITE_ADDRESS = (uint32_t)1 << (ADDR_LEN - 1);
static const uint32_t ACCUMULATOR_FULL_WIDTH_ADDRESS = ((uint32_t)1 << (ADDR_LEN - 1)) | ((uint32_t)1 << (ADDR_LEN - 3));
static const uint8_t EXPECTED[TRANSFER_BYTES] = {
    0xdc, 0x95, 0xc0, 0x78, 0xa2, 0x40, 0x89, 0x89, 0xad, 0x48, 0xa2, 0x14, 0x92, 0x84, 0x20, 0x87, 0xdc, 0x95, 0xc0, 0x78, 0xa2, 0x40, 0x89, 0x89, 0xad, 0x48, 0xa2, 0x14, 0x92, 0x84, 0x20, 0x87,
    0xdc, 0x95, 0xc0, 0x78, 0xa2, 0x40, 0x89, 0x89, 0xad, 0x48, 0xa2, 0x14, 0x92, 0x84, 0x20, 0x87, 0xdc, 0x95, 0xc0, 0x78, 0xa2, 0x40, 0x89, 0x89, 0xad, 0x48, 0xa2, 0x14, 0x92, 0x84, 0x20, 0x87,
    0x9f, 0x82, 0xb0, 0xc6, 0xaf, 0x87, 0xed, 0xe6, 0xae, 0x1a, 0xe8, 0x85, 0x76, 0xee, 0x84, 0x22, 0x20, 0x54, 0xb1, 0xc5, 0xb0, 0xb4, 0x54, 0xcc, 0x79, 0x53, 0x9f, 0x89, 0x44, 0x05, 0xab, 0x49,
    0xdf, 0x53, 0x36, 0x8c, 0x09, 0x1c, 0xad, 0x1e, 0x7c, 0xaf, 0xfd, 0xf2, 0x59, 0xf7, 0x0f, 0x0e, 0x30, 0xd5, 0x8e, 0x7a, 0xb8, 0x57, 0x4f, 0xbc, 0xda, 0x61, 0xdb, 0xda, 0x36, 0xbe, 0x29, 0x3b,
};

static void init_inputs(void) {
  for (int i = 0; i < DIM; ++i) {
    for (int j = 0; j < DIM; ++j) {
      const int index = i * DIM + j;
      A[i][j] = (elem_t)(index % CGRA_WORD_COUNT - CGRA_WORD_COUNT / 2);
      B[i][j] = i == j ? (elem_t)1 : (elem_t)0;
    }
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
  gemmini_extended_mvout_spad(PUBLICATION_ROW, GEMMINI_FULL_WIDTH_ROW_STRIDE, ACCUMULATOR_FULL_WIDTH_ADDRESS, DIM, PUBLICATION_ROWS);
  gemmini_fence();
}

static int run_cgra(void) {
  const uintptr_t input_address = GEMMINI_EXT_SPM_BASE + GEMMINI_EXT_SPM_SIZE_BYTES - TRANSFER_BYTES;
  uint64_t wait_result = 0;
  uint64_t status = 0;
  uint64_t result = 0;

  cgra_dma_mvin_async((const void *)input_address, INPUT_DESCRIPTOR);
  CGRA_SET_EXPECTED_COMPLETES(CGRA_EXPECTED_COMPLETES);
  load_relu4x4_config_fast();
  if (cgra_dma_wait(INPUT_DMA_TAG) != INPUT_DMA_TAG) {
    return 1;
  }
  launch_relu4x4_fast();
  CGRA_WAIT(wait_result);
  CGRA_STATUS(status);
  CGRA_RESULT(result);
  return wait_result != 1 || (status & UINT64_C(1)) != 1 || ((status >> 1) & UINT64_C(0xffff)) != CGRA_EXPECTED_COMPLETES || result != 0;
}

static int run_aes(void) {
  const unsigned char *input = (const unsigned char *)(uintptr_t)CGRA_SPM_WINDOW_BASE;
  return Aes256Accel(true, input, TRANSFER_BYTES, 0, 0, 0, 0, ciphertext) != 1;
}

static int verify_output(void) {
  for (unsigned i = 0; i < TRANSFER_BYTES; ++i) {
    if (ciphertext[i] != EXPECTED[i]) {
      return 1;
    }
  }
  return 0;
}

int main(void) {
  init_inputs();
  run_gemmini();
  if (run_cgra() != 0 || run_aes() != 0 || verify_output() != 0) {
    printf("Gemmini + CGRA + AES Manual: FAIL\n");
    return 1;
  }
  printf("Gemmini + CGRA + AES Manual: PASS\n");
  return 0;
}
