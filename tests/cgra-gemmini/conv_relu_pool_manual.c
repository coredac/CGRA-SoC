#include "cgra_dma.h"
#include "cgra_protocol.h"
#include "cgra_runtime.h"
#include "cgra_spm_window.h"
#include "gemmini.h"
#include "gemmini_ext_spm.h"
#include "generated/cgra_relu4x4_fast_api.h"
#include "pool.h"

#include <stdint.h>
#include <stdio.h>

enum {
  INPUT_H = 3,
  INPUT_W = 3,
  INPUT_CHANNELS = 1,
  KERNEL_H = 2,
  KERNEL_W = 2,
  CONV_H = 2,
  CONV_W = 2,
  OUTPUT_CHANNELS = 8,
  A_ROWS = 2,
  A_COLUMNS = 6,
  B_ROWS = 6,
  B_COLUMNS = 16,
  CGRA_WORDS = 32,
  CGRA_BYTES = CGRA_WORDS * sizeof(acc_t),
  ROW_STRIDE = sizeof(acc_t) / sizeof(elem_t),
  PUBLICATION_ROWS = 2,
  PUBLICATION_ROW = BANK_NUM * BANK_ROWS - PUBLICATION_ROWS * ROW_STRIDE,
  A_ROW = 0,
  B_ROW = DIM,
  CGRA_SPM_WORD_ADDR = 0,
  CGRA_EXPECTED_COMPLETES = 1,
  CGRA_INPUT_TAG = 0x20,
};

static const int8_t INPUT[INPUT_H][INPUT_W][INPUT_CHANNELS] = {
    {{-3}, {1}, {2}},
    {{4}, {-2}, {5}},
    {{-1}, {3}, {-4}},
};

static const int8_t FILTER[KERNEL_H][KERNEL_W][INPUT_CHANNELS][OUTPUT_CHANNELS] = {
    {{{1, 0, 1, 1, -1, 2, 0, 1}}, {{0, 1, 1, -1, 0, 0, 2, -2}}},
    {{{0, 1, 1, -1, 0, 0, -1, 2}}, {{1, 0, 1, 1, -1, -1, 0, -1}}},
};

static elem_t A[A_ROWS][DIM] row_align(1);
static elem_t B[B_ROWS][DIM] row_align(1);
static acc_t output[OUTPUT_CHANNELS] __attribute__((aligned(32)));

static const uint32_t ACC_WRITE_ADDR = (uint32_t)1 << (ADDR_LEN - 1);
static const uint32_t ACC_FULL_WIDTH_ADDR = ((uint32_t)1 << (ADDR_LEN - 1)) | ((uint32_t)1 << (ADDR_LEN - 3));
static const cgra_dma_desc_t CGRA_INPUT = CGRA_DMA_DESC_CONST(CGRA_SPM_WORD_ADDR, CGRA_BYTES, CGRA_INPUT_TAG);

static void init_inputs(void) {
  for (unsigned row = 0; row < A_ROWS; ++row) {
    for (unsigned column = 0; column < DIM; ++column) {
      A[row][column] = 0;
    }
    for (unsigned column = 0; column < INPUT_W; ++column) {
      A[row][column] = (elem_t)INPUT[row][column][0];
      A[row][column + INPUT_W] = (elem_t)INPUT[row + 1][column][0];
    }
  }

  for (unsigned row = 0; row < B_ROWS; ++row) {
    for (unsigned column = 0; column < DIM; ++column) {
      B[row][column] = 0;
    }
  }
  for (unsigned channel = 0; channel < OUTPUT_CHANNELS; ++channel) {
    B[0][channel] = (elem_t)FILTER[0][0][0][channel];
    B[1][channel] = (elem_t)FILTER[0][1][0][channel];
    B[3][channel] = (elem_t)FILTER[1][0][0][channel];
    B[4][channel] = (elem_t)FILTER[1][1][0][channel];
    B[1][channel + OUTPUT_CHANNELS] = (elem_t)FILTER[0][0][0][channel];
    B[2][channel + OUTPUT_CHANNELS] = (elem_t)FILTER[0][1][0][channel];
    B[4][channel + OUTPUT_CHANNELS] = (elem_t)FILTER[1][0][0][channel];
    B[5][channel + OUTPUT_CHANNELS] = (elem_t)FILTER[1][1][0][channel];
  }
}

static void load_gemmini(void) {
  gemmini_flush(0);
  gemmini_config_ld(DIM * sizeof(elem_t));
  gemmini_config_ex(WEIGHT_STATIONARY, NO_ACTIVATION, 0);
  gemmini_config_st(DIM * sizeof(acc_t));
  gemmini_extended_mvin(A, A_ROW, A_COLUMNS, A_ROWS);
  gemmini_extended_mvin(B, B_ROW, B_COLUMNS, B_ROWS);
  gemmini_fence();
}

static void launch_gemmini(void) {
  gemmini_extended_preload(B_ROW, ACC_WRITE_ADDR, B_COLUMNS, B_ROWS, B_COLUMNS, A_ROWS);
  gemmini_extended_compute_preloaded(A_ROW, GARBAGE_ADDR, A_COLUMNS, A_ROWS, B_COLUMNS, A_ROWS);
  gemmini_extended_mvout_spad(PUBLICATION_ROW, ROW_STRIDE, ACC_FULL_WIDTH_ADDR, B_COLUMNS, A_ROWS);
}

static int verify_output(void) {
  int32_t relu[CONV_H][CONV_W][OUTPUT_CHANNELS];
  acc_t expected[OUTPUT_CHANNELS];
  int failures = 0;

  for (unsigned row = 0; row < CONV_H; ++row) {
    for (unsigned column = 0; column < CONV_W; ++column) {
      for (unsigned channel = 0; channel < OUTPUT_CHANNELS; ++channel) {
        int32_t sum = 0;
        for (unsigned kernel_row = 0; kernel_row < KERNEL_H; ++kernel_row) {
          for (unsigned kernel_column = 0; kernel_column < KERNEL_W; ++kernel_column) {
            sum += (int32_t)INPUT[row + kernel_row][column + kernel_column][0] * (int32_t)FILTER[kernel_row][kernel_column][0][channel];
          }
        }
        relu[row][column][channel] = sum > 0 ? sum : 0;
      }
    }
  }
  for (unsigned channel = 0; channel < OUTPUT_CHANNELS; ++channel) {
    expected[channel] = relu[0][0][channel];
    for (unsigned row = 0; row < CONV_H; ++row) {
      for (unsigned column = 0; column < CONV_W; ++column) {
        if (relu[row][column][channel] > expected[channel]) {
          expected[channel] = relu[row][column][channel];
        }
      }
    }
    if (output[channel] != expected[channel]) {
      printf("Pool mismatch channel=%u actual=%d expected=%d\n", channel, (int)output[channel], (int)expected[channel]);
      ++failures;
    }
  }
  return failures;
}

static int run_cgra(void) {
  const uintptr_t source = GEMMINI_EXT_SPM_BASE + GEMMINI_EXT_SPM_SIZE_BYTES - CGRA_BYTES;
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
  pool_config_input(CGRA_SPM_WINDOW_BASE, CONV_H, CONV_W, OUTPUT_CHANNELS);
  pool_config_output((uintptr_t)output);
  pool_config_window(POOL_MODE_MAX, 2, 2, 2, 2, 0, 0);
  pool_start();
  return pool_wait() != POOL_STATUS_SUCCESS;
}

int main(void) {
  init_inputs();
  load_gemmini();
  launch_gemmini();
  gemmini_fence();

  if (run_cgra() != 0 || run_pool() != 0) {
    printf("Gemmini Conv + CGRA ReLU + Pool Manual: FAIL\n");
    return 1;
  }
  const int failures = verify_output();
  if (failures != 0) {
    printf("Gemmini Conv + CGRA ReLU + Pool Manual: FAIL (%d)\n", failures);
    return 1;
  }
  printf("Gemmini Conv + CGRA ReLU + Pool Manual: PASS\n");
  return 0;
}
