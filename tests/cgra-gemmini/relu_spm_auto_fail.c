#include "aes_auto_job.h"
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
  WRONG_PUBLICATION_ROW = PUBLICATION_ROW - PUBLICATION_ROWS * GEMMINI_FULL_WIDTH_ROW_STRIDE,
  GEMMINI_LINK_BAD_ADDRESS = 1,
};

static elem_t A[DIM][DIM] row_align(1);
static elem_t B[DIM][DIM] row_align(1);

static const uint32_t ACCUMULATOR_WRITE_ADDRESS = (uint32_t)1 << (ADDR_LEN - 1);
static const uint32_t ACCUMULATOR_FULL_WIDTH_ADDRESS = ((uint32_t)1 << (ADDR_LEN - 1)) | ((uint32_t)1 << (ADDR_LEN - 3));

static void init_inputs(void) {
  volatile uint8_t *ciphertext = (volatile uint8_t *)(uintptr_t)AES_AUTO_CIPHERTEXT_ADDRESS;
  volatile uint32_t *completion = (volatile uint32_t *)(uintptr_t)AES_AUTO_COMPLETION_ADDRESS;
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
  *completion = 0;
  __asm__ volatile("fence rw, rw" ::: "memory");
}

static void run_bad_publication(void) {
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
  gemmini_extended_mvout_spad(WRONG_PUBLICATION_ROW, GEMMINI_FULL_WIDTH_ROW_STRIDE, ACCUMULATOR_FULL_WIDTH_ADDRESS, DIM, PUBLICATION_ROWS);
}

static void configure_cgra(void) {
  load_relu4x4_config_fast();
  cgra_link_configure(RELU4X4_FAST_LAUNCH_PACKET_COUNT);
  for (unsigned i = 0; i < RELU4X4_FAST_LAUNCH_PACKET_COUNT; ++i) {
    cgra_link_queue(RELU4X4_FAST_LAUNCH_PACKETS[i]);
  }
}

static int result_mismatch(cgra_link_result_t result, uint32_t detail) { return result.status != AUTO_LINK_STATUS_SOURCE_FAILURE || result.detail != detail || result.data != 0; }

static int output_changed(void) {
  const volatile uint8_t *ciphertext = (const volatile uint8_t *)(uintptr_t)AES_AUTO_CIPHERTEXT_ADDRESS;
  const volatile uint32_t *completion = (const volatile uint32_t *)(uintptr_t)AES_AUTO_COMPLETION_ADDRESS;
  __asm__ volatile("fence rw, rw" ::: "memory");
  if (*completion != 0) {
    return 1;
  }
  for (unsigned i = 0; i < TRANSFER_BYTES; ++i) {
    if (ciphertext[i] != 0) {
      return 1;
    }
  }
  return 0;
}

int main(void) {
  init_inputs();
  configure_cgra();
  run_bad_publication();

  const cgra_link_result_t cgra = cgra_link_wait();
  const cgra_link_result_t aes = cgra_link_wait();
  if (result_mismatch(cgra, GEMMINI_LINK_BAD_ADDRESS) != 0) {
    printf("Three-IP Auto: CGRA did not report Gemmini BadAddress\n");
    return 1;
  }
  if (result_mismatch(aes, 0) != 0) {
    printf("Three-IP Auto: AES cancellation result mismatch\n");
    return 1;
  }
  if (cgra_link_read(CGRA_LINK_CONTROL_RESULT_VALID) != 0) {
    printf("Three-IP Auto: unexpected extra result\n");
    return 1;
  }
  if (output_changed() != 0) {
    printf("Three-IP Auto: AES output changed after Gemmini failure\n");
    return 1;
  }
  printf("Three-IP Auto Gemmini BadAddress: PASS\n");
  return 0;
}
