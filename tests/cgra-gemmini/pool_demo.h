#ifndef TESTS_CGRA_GEMMINI_POOL_DEMO_H
#define TESTS_CGRA_GEMMINI_POOL_DEMO_H

#include "gemmini.h"

#include <stdint.h>
#include <stdio.h>

enum {
  POOL_DEMO_INPUT_HEIGHT = 3,
  POOL_DEMO_INPUT_WIDTH = 3,
  POOL_DEMO_CHANNELS = 1,
  POOL_DEMO_KERNEL_HEIGHT = 2,
  POOL_DEMO_KERNEL_WIDTH = 2,
  POOL_DEMO_CONV_HEIGHT = 2,
  POOL_DEMO_CONV_WIDTH = 2,
  POOL_DEMO_OUTPUT_CHANNELS = 8,
  POOL_DEMO_A_ROWS = 2,
  POOL_DEMO_A_COLUMNS = 6,
  POOL_DEMO_B_ROWS = 6,
  POOL_DEMO_B_COLUMNS = 16,
  POOL_DEMO_WORDS = 32,
  POOL_DEMO_BYTES = POOL_DEMO_WORDS * sizeof(acc_t),
  POOL_DEMO_ROW_STRIDE = sizeof(acc_t) / sizeof(elem_t),
  POOL_DEMO_PUBLICATION_ROWS = 2,
  POOL_DEMO_PUBLICATION_ROW = BANK_NUM * BANK_ROWS - POOL_DEMO_PUBLICATION_ROWS * POOL_DEMO_ROW_STRIDE,
  POOL_DEMO_A_ROW = 0,
  POOL_DEMO_B_ROW = DIM,
};

_Static_assert(DIM == 16 && sizeof(elem_t) == 1, "demo requires 16x16 INT8 Gemmini");
_Static_assert(sizeof(acc_t) == 4, "demo requires INT32 Gemmini accumulators");
_Static_assert(POOL_DEMO_BYTES == 128, "demo requires a 128-byte CGRA transfer");

static const uint32_t POOL_DEMO_ACC_WRITE_ADDR = (uint32_t)1 << (ADDR_LEN - 1);
static const uint32_t POOL_DEMO_ACC_FULL_WIDTH_ADDR = ((uint32_t)1 << (ADDR_LEN - 1)) | ((uint32_t)1 << (ADDR_LEN - 3));

static const int8_t POOL_DEMO_INPUT[POOL_DEMO_INPUT_HEIGHT][POOL_DEMO_INPUT_WIDTH][POOL_DEMO_CHANNELS] = {
    {{-3}, {1}, {2}},
    {{4}, {-2}, {5}},
    {{-1}, {3}, {-4}},
};

static const int8_t POOL_DEMO_FILTER[POOL_DEMO_KERNEL_HEIGHT][POOL_DEMO_KERNEL_WIDTH][POOL_DEMO_CHANNELS][POOL_DEMO_OUTPUT_CHANNELS] = {
    {{{1, 0, 1, 1, -1, 2, 0, 1}}, {{0, 1, 1, -1, 0, 0, 2, -2}}},
    {{{0, 1, 1, -1, 0, 0, -1, 2}}, {{1, 0, 1, 1, -1, -1, 0, -1}}},
};

static inline void pool_demo_pack(elem_t A[POOL_DEMO_A_ROWS][DIM], elem_t B[POOL_DEMO_B_ROWS][DIM]) {
  for (unsigned row = 0; row < POOL_DEMO_A_ROWS; ++row) {
    for (unsigned column = 0; column < DIM; ++column) {
      A[row][column] = 0;
    }
    for (unsigned column = 0; column < POOL_DEMO_INPUT_WIDTH; ++column) {
      A[row][column] = (elem_t)POOL_DEMO_INPUT[row][column][0];
      A[row][column + POOL_DEMO_INPUT_WIDTH] = (elem_t)POOL_DEMO_INPUT[row + 1][column][0];
    }
  }

  for (unsigned row = 0; row < POOL_DEMO_B_ROWS; ++row) {
    for (unsigned column = 0; column < DIM; ++column) {
      B[row][column] = 0;
    }
  }
  for (unsigned channel = 0; channel < POOL_DEMO_OUTPUT_CHANNELS; ++channel) {
    B[0][channel] = (elem_t)POOL_DEMO_FILTER[0][0][0][channel];
    B[1][channel] = (elem_t)POOL_DEMO_FILTER[0][1][0][channel];
    B[3][channel] = (elem_t)POOL_DEMO_FILTER[1][0][0][channel];
    B[4][channel] = (elem_t)POOL_DEMO_FILTER[1][1][0][channel];
    B[1][channel + POOL_DEMO_OUTPUT_CHANNELS] = (elem_t)POOL_DEMO_FILTER[0][0][0][channel];
    B[2][channel + POOL_DEMO_OUTPUT_CHANNELS] = (elem_t)POOL_DEMO_FILTER[0][1][0][channel];
    B[4][channel + POOL_DEMO_OUTPUT_CHANNELS] = (elem_t)POOL_DEMO_FILTER[1][0][0][channel];
    B[5][channel + POOL_DEMO_OUTPUT_CHANNELS] = (elem_t)POOL_DEMO_FILTER[1][1][0][channel];
  }
}

static inline void pool_demo_load_gemmini(elem_t A[POOL_DEMO_A_ROWS][DIM], elem_t B[POOL_DEMO_B_ROWS][DIM]) {
  gemmini_flush(0);
  gemmini_config_ld(DIM * sizeof(elem_t));
  gemmini_config_ex(WEIGHT_STATIONARY, NO_ACTIVATION, 0);
  gemmini_config_st(DIM * sizeof(acc_t));
  gemmini_extended_mvin(A, POOL_DEMO_A_ROW, POOL_DEMO_A_COLUMNS, POOL_DEMO_A_ROWS);
  gemmini_extended_mvin(B, POOL_DEMO_B_ROW, POOL_DEMO_B_COLUMNS, POOL_DEMO_B_ROWS);
  gemmini_fence();
}

static inline void pool_demo_launch_gemmini(void) {
  gemmini_extended_preload(POOL_DEMO_B_ROW, POOL_DEMO_ACC_WRITE_ADDR, POOL_DEMO_B_COLUMNS, POOL_DEMO_B_ROWS, POOL_DEMO_B_COLUMNS, POOL_DEMO_A_ROWS);
  gemmini_extended_compute_preloaded(POOL_DEMO_A_ROW, GARBAGE_ADDR, POOL_DEMO_A_COLUMNS, POOL_DEMO_A_ROWS, POOL_DEMO_B_COLUMNS, POOL_DEMO_A_ROWS);
  gemmini_extended_mvout_spad(POOL_DEMO_PUBLICATION_ROW, POOL_DEMO_ROW_STRIDE, POOL_DEMO_ACC_FULL_WIDTH_ADDR, POOL_DEMO_B_COLUMNS, POOL_DEMO_A_ROWS);
}

static inline void pool_demo_reference(acc_t output[POOL_DEMO_OUTPUT_CHANNELS]) {
  int32_t relu[POOL_DEMO_CONV_HEIGHT][POOL_DEMO_CONV_WIDTH][POOL_DEMO_OUTPUT_CHANNELS];
  for (unsigned row = 0; row < POOL_DEMO_CONV_HEIGHT; ++row) {
    for (unsigned column = 0; column < POOL_DEMO_CONV_WIDTH; ++column) {
      for (unsigned channel = 0; channel < POOL_DEMO_OUTPUT_CHANNELS; ++channel) {
        int32_t sum = 0;
        for (unsigned kernel_row = 0; kernel_row < POOL_DEMO_KERNEL_HEIGHT; ++kernel_row) {
          for (unsigned kernel_column = 0; kernel_column < POOL_DEMO_KERNEL_WIDTH; ++kernel_column) {
            sum += (int32_t)POOL_DEMO_INPUT[row + kernel_row][column + kernel_column][0] * (int32_t)POOL_DEMO_FILTER[kernel_row][kernel_column][0][channel];
          }
        }
        relu[row][column][channel] = sum > 0 ? sum : 0;
      }
    }
  }
  for (unsigned channel = 0; channel < POOL_DEMO_OUTPUT_CHANNELS; ++channel) {
    output[channel] = relu[0][0][channel];
    for (unsigned row = 0; row < POOL_DEMO_CONV_HEIGHT; ++row) {
      for (unsigned column = 0; column < POOL_DEMO_CONV_WIDTH; ++column) {
        if (relu[row][column][channel] > output[channel]) {
          output[channel] = relu[row][column][channel];
        }
      }
    }
  }
}

static inline int pool_demo_verify(const acc_t actual[POOL_DEMO_OUTPUT_CHANNELS]) {
  acc_t expected[POOL_DEMO_OUTPUT_CHANNELS];
  int failures = 0;
  pool_demo_reference(expected);
  for (unsigned channel = 0; channel < POOL_DEMO_OUTPUT_CHANNELS; ++channel) {
    if (actual[channel] != expected[channel]) {
      printf("Pool mismatch channel=%u actual=%d expected=%d\n", channel, (int)actual[channel], (int)expected[channel]);
      ++failures;
    }
  }
  return failures;
}

#endif
