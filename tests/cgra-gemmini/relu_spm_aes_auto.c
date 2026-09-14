#include "aes_job.h"
#include "auto_link_generated.h"
#include "cgra_link.h"
#include "cgra_protocol.h"
#include "gemmini.h"
#include "generated/cgra_relu4x4_fast_api.h"

#include <stdint.h>
#include <stdio.h>

enum {
  CGRA_WORD_COUNT = 32,
  TRANSFER_BYTES = CGRA_WORD_COUNT * sizeof(acc_t),
  GEMMINI_FULL_WIDTH_ROW_BYTES = DIM * sizeof(acc_t),
  GEMMINI_FULL_WIDTH_ROW_STRIDE = sizeof(acc_t) / sizeof(elem_t),
  PUBLICATION_ROWS = TRANSFER_BYTES / GEMMINI_FULL_WIDTH_ROW_BYTES,
  PUBLICATION_ROW = BANK_NUM * BANK_ROWS - PUBLICATION_ROWS * GEMMINI_FULL_WIDTH_ROW_STRIDE,
};

static elem_t A[DIM][DIM] row_align(1);
static elem_t B[DIM][DIM] row_align(1);
static uint8_t ciphertext[TRANSFER_BYTES] __attribute__((aligned(32)));
static volatile uint32_t completion __attribute__((aligned(8)));

static const uint64_t KEY[4] = {UINT64_C(0x2d9810a30914dff4), UINT64_C(0x1f352c073b6108d7), UINT64_C(0x2b73aef0857d7781), UINT64_C(0x603deb1015ca71be)};
static const uint32_t ACCUMULATOR_WRITE_ADDRESS = (uint32_t)1 << (ADDR_LEN - 1);
static const uint32_t ACCUMULATOR_FULL_WIDTH_ADDRESS = ((uint32_t)1 << (ADDR_LEN - 1)) | ((uint32_t)1 << (ADDR_LEN - 3));
static const uint8_t EXPECTED[TRANSFER_BYTES] = {
    0xe5, 0x68, 0xf6, 0x81, 0x94, 0xcf, 0x76, 0xd6, 0x17, 0x4d, 0x4c, 0xc0, 0x43, 0x10, 0xa8, 0x54, 0xe5, 0x68, 0xf6, 0x81, 0x94, 0xcf, 0x76, 0xd6, 0x17, 0x4d, 0x4c, 0xc0, 0x43, 0x10, 0xa8, 0x54,
    0xe5, 0x68, 0xf6, 0x81, 0x94, 0xcf, 0x76, 0xd6, 0x17, 0x4d, 0x4c, 0xc0, 0x43, 0x10, 0xa8, 0x54, 0xe5, 0x68, 0xf6, 0x81, 0x94, 0xcf, 0x76, 0xd6, 0x17, 0x4d, 0x4c, 0xc0, 0x43, 0x10, 0xa8, 0x54,
    0x2f, 0x97, 0x58, 0xf4, 0xef, 0x76, 0xa7, 0xf7, 0x1b, 0xfa, 0x1f, 0xa5, 0x07, 0x24, 0x1c, 0xdc, 0xbe, 0x68, 0x04, 0x72, 0xb0, 0x93, 0x50, 0x18, 0xf0, 0x9e, 0xb7, 0xdb, 0x2a, 0x61, 0xe0, 0xe2,
    0x40, 0x51, 0xcf, 0xc1, 0xac, 0x31, 0x08, 0x9f, 0x59, 0xc7, 0x1c, 0x4f, 0x91, 0xc9, 0xca, 0x26, 0xd7, 0xc7, 0x22, 0xb0, 0xc0, 0x32, 0x27, 0xcb, 0xc6, 0x05, 0x32, 0xd3, 0xd4, 0x44, 0x55, 0x4b,
};

static void init_inputs(void) {
  for (int i = 0; i < DIM; ++i) {
    for (int j = 0; j < DIM; ++j) {
      const int index = i * DIM + j;
      A[i][j] = (elem_t)(index % CGRA_WORD_COUNT - CGRA_WORD_COUNT / 2);
      B[i][j] = i == j ? (elem_t)1 : (elem_t)0;
    }
  }
  for (unsigned i = 0; i < TRANSFER_BYTES; ++i) {
    ciphertext[i] = 0;
  }
  completion = 0;
  __asm__ volatile("fence rw, rw" ::: "memory");
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
  gemmini_fence();
  gemmini_preload(B_addr, ACCUMULATOR_WRITE_ADDRESS);
  gemmini_compute_preloaded(A_addr, GARBAGE_ADDR);
  gemmini_extended_mvout_spad(PUBLICATION_ROW, GEMMINI_FULL_WIDTH_ROW_STRIDE, ACCUMULATOR_FULL_WIDTH_ADDRESS, DIM, PUBLICATION_ROWS);
}

static int verify_result(cgra_link_result_t result) { return result.status != AUTO_LINK_STATUS_SUCCESS || result.detail != 0 || result.data != 0; }

static int verify_output(void) {
  __asm__ volatile("fence rw, rw" ::: "memory");
  if (completion != 1) {
    return 1;
  }
  for (unsigned i = 0; i < TRANSFER_BYTES; ++i) {
    if (ciphertext[i] != EXPECTED[i]) {
      return 1;
    }
  }
  return 0;
}

int main(void) {
  init_inputs();
  aes_job_configure(AUTO_LINK_JOB_AES, ciphertext, &completion, KEY, true);
  cgra_job_config(0, &RELU4X4, NULL);
  run_gemmini();

  const cgra_link_result_t cgra = cgra_link_wait();
  const cgra_link_result_t aes = cgra_link_wait();
  if (verify_result(cgra) != 0 || verify_result(aes) != 0 || verify_output() != 0) {
    printf("Gemmini + CGRA + AES Auto: FAIL\n");
    return 1;
  }
  printf("Gemmini + CGRA + AES Auto: PASS\n");
  return 0;
}
