#include "auto_link.h"
#include "cgra_link.h"
#include "cgra_spm_window.h"
#include "gemmini.h"
#include "gemmini_ext_spm.h"
#include "gemmini_job.h"
#include "generated/cgra_add_relu_fast_api.h"
#include "generated/cgra_relu_tail_fast_api.h"

#include <stdint.h>
#include <stdio.h>

enum {
  POSITIONS = 4,
  CHANNELS = 8,
  GEMMINI_ROWS = 2,
  GEMMINI_ROW_BYTES = DIM * sizeof(elem_t),
  GEMMINI_ACC_ROW_BYTES = DIM * sizeof(acc_t),
  GEMMINI_ACC_ROW_STRIDE = sizeof(acc_t) / sizeof(elem_t),
  A1_ROW = 0,
  W1_ROW = DIM,
  W2_ROW = 2 * DIM,
  A2_ROW = 3 * DIM,
  TENSOR_BYTES = POSITIONS * CHANNELS * sizeof(acc_t),
  CONV1_SPM_OFFSET = GEMMINI_EXT_SPM_SIZE_BYTES - 2 * TENSOR_BYTES,
  CONV2_SPM_OFFSET = GEMMINI_EXT_SPM_SIZE_BYTES - TENSOR_BYTES,
  CONV1_SPM_ROW = CONV1_SPM_OFFSET / GEMMINI_ROW_BYTES,
  CONV2_SPM_ROW = CONV2_SPM_OFFSET / GEMMINI_ROW_BYTES,
  SKIP_WORD = 0,
  OUTPUT_WORD = 64,
  CONV1_COMMANDS = 5,
  CONV2_COMMANDS = 7,
  STAGE_COUNT = 4,
};

static const int8_t INPUT[POSITIONS][CHANNELS] = {
    {-8, -4, -1, 0, 1, 3, 6, 9},
    {7, -6, 5, -4, 3, -2, 1, 0},
    {-3, 8, -7, 6, -5, 4, -2, 1},
    {2, -1, 4, -3, 6, -5, 8, -7},
};

// Fixed weights include folded BatchNorm; folded bias is zero.
static const int8_t WEIGHT1[CHANNELS][CHANNELS] = {
    {1, -1, 0, 2, 0, 1, -2, 1}, {0, 1, 1, -1, 2, 0, 1, -2}, {-1, 0, 2, 1, -1, 1, 0, 1}, {2, 1, -1, 0, 1, -2, 1, 0},
    {1, 0, 1, -2, 0, 2, -1, 1}, {-2, 1, 0, 1, 1, 0, 2, -1}, {0, -1, 1, 0, 2, 1, -1, 2}, {1, 2, -2, 1, 0, -1, 1, 0},
};

static const int8_t WEIGHT2[CHANNELS][CHANNELS] = {
    {1, 0, 1, -1, 0, 2, -1, 1}, {0, 1, -1, 1, 2, 0, 1, -1}, {1, -1, 1, 0, -1, 1, 0, 2}, {-1, 1, 0, 1, 1, -1, 2, 0},
    {0, 2, -1, 1, 1, 0, -1, 1}, {2, 0, 1, -1, 0, 1, 1, -2}, {-1, 1, 0, 2, -1, 1, 0, 1}, {1, -1, 2, 0, 1, -2, 1, 1},
};

static elem_t A1[GEMMINI_ROWS][DIM] row_align(1);
static elem_t B1[DIM][DIM] row_align(1);
static elem_t B2[DIM][DIM] row_align(1);

static const uint32_t CONV1_ACC_WRITE = (uint32_t)1 << (ADDR_LEN - 1);
static const uint32_t CONV2_ACC_WRITE = ((uint32_t)1 << (ADDR_LEN - 1)) + GEMMINI_ROWS;
static const uint32_t CONV1_ACC_READ = ((uint32_t)1 << (ADDR_LEN - 1)) | ((uint32_t)1 << (ADDR_LEN - 3));
static const uint32_t CONV2_ACC_READ = (((uint32_t)1 << (ADDR_LEN - 1)) | ((uint32_t)1 << (ADDR_LEN - 3))) + GEMMINI_ROWS;

static int8_t requant(int32_t value) {
  if (value > INT8_MAX) {
    return INT8_MAX;
  }
  if (value < INT8_MIN) {
    return INT8_MIN;
  }
  return (int8_t)value;
}

static void init_inputs(void) {
  for (unsigned row = 0; row < GEMMINI_ROWS; ++row) {
    for (unsigned column = 0; column < DIM; ++column) {
      A1[row][column] = 0;
    }
  }
  for (unsigned row = 0; row < DIM; ++row) {
    for (unsigned column = 0; column < DIM; ++column) {
      B1[row][column] = 0;
      B2[row][column] = 0;
    }
  }
  for (unsigned position = 0; position < POSITIONS; ++position) {
    for (unsigned channel = 0; channel < CHANNELS; ++channel) {
      A1[position / 2][(position % 2) * CHANNELS + channel] = (elem_t)INPUT[position][channel];
    }
  }
  for (unsigned block = 0; block < 2; ++block) {
    for (unsigned input = 0; input < CHANNELS; ++input) {
      for (unsigned output = 0; output < CHANNELS; ++output) {
        const unsigned row = block * CHANNELS + input;
        const unsigned column = block * CHANNELS + output;
        B1[row][column] = (elem_t)WEIGHT1[input][output];
        B2[row][column] = (elem_t)WEIGHT2[input][output];
      }
    }
  }
}

static void preload_gemmini(void) {
  gemmini_flush(0);
  gemmini_config_ld(GEMMINI_ROW_BYTES);
  gemmini_extended_mvin(A1, A1_ROW, DIM, GEMMINI_ROWS);
  gemmini_extended_mvin(B1, W1_ROW, DIM, DIM);
  gemmini_extended_mvin(B2, W2_ROW, DIM, DIM);
  gemmini_fence();
}

static int preload_skip(void) {
  uint64_t wait_result = 0;
  for (unsigned position = 0; position < POSITIONS; ++position) {
    for (unsigned channel = 0; channel < CHANNELS; ++channel) {
      const unsigned word = position * CHANNELS + channel;
      add_relu_store_fast(SKIP_WORD + word, (uint32_t)(int32_t)INPUT[position][channel]);
    }
  }
  CGRA_WAIT(wait_result);
  return wait_result != 1;
}

static void configure_cgra(void) {
  cgra_job_config(0, &RELU_TAIL, NULL);
  cgra_job_config(1, &ADD_RELU, NULL);
}

static int configure_gemmini(void) {
  // Five commands: execute config, store config, preload, compute, and publish.
  if (gemmini_job_begin_id(0, CONV1_COMMANDS) != 0) {
    return 1;
  }
  gemmini_config_ex(WEIGHT_STATIONARY, NO_ACTIVATION, 0);
  gemmini_config_st(GEMMINI_ACC_ROW_BYTES);
  gemmini_extended_preload(W1_ROW, CONV1_ACC_WRITE, DIM, DIM, DIM, GEMMINI_ROWS);
  gemmini_extended_compute_preloaded(A1_ROW, GARBAGE_ADDR, DIM, GEMMINI_ROWS, DIM, GEMMINI_ROWS);
  gemmini_extended_mvout_spad(CONV1_SPM_ROW, GEMMINI_ACC_ROW_STRIDE, CONV1_ACC_READ, DIM, GEMMINI_ROWS);

  // Seven commands add load config and packed CGRA mvin before Conv2.
  if (gemmini_job_begin_id(1, CONV2_COMMANDS) != 0) {
    return 1;
  }
  gemmini_config_ld(GEMMINI_ROW_BYTES);
  gemmini_extended_mvin((const void *)(uintptr_t)CGRA_SPM_WINDOW_BASE, A2_ROW, DIM, GEMMINI_ROWS);
  gemmini_config_ex(WEIGHT_STATIONARY, NO_ACTIVATION, 0);
  gemmini_config_st(GEMMINI_ACC_ROW_BYTES);
  gemmini_extended_preload(W2_ROW, CONV2_ACC_WRITE, DIM, DIM, DIM, GEMMINI_ROWS);
  gemmini_extended_compute_preloaded(A2_ROW, GARBAGE_ADDR, DIM, GEMMINI_ROWS, DIM, GEMMINI_ROWS);
  gemmini_extended_mvout_spad(CONV2_SPM_ROW, GEMMINI_ACC_ROW_STRIDE, CONV2_ACC_READ, DIM, GEMMINI_ROWS);
  return 0;
}

static int verify_results(void) {
  const uint32_t expected = (UINT32_C(1) << AUTO_LINK_STAGE_CONV1) | (UINT32_C(1) << AUTO_LINK_STAGE_RELU1) | (UINT32_C(1) << AUTO_LINK_STAGE_CONV2) | (UINT32_C(1) << AUTO_LINK_STAGE_ADD_RELU);
  uint32_t seen = 0;
  int failures = 0;

  for (unsigned index = 0; index < STAGE_COUNT; ++index) {
    const cgra_link_result_t result = cgra_link_wait();
    const uint32_t bit = result.stage < 32 ? UINT32_C(1) << result.stage : 0;
    if (result.status != AUTO_LINK_STATUS_SUCCESS || result.detail != 0 || result.data != 0 || (expected & bit) == 0 || (seen & bit) != 0) {
      printf("AutoLink result mismatch stage=%u job=%u status=%u detail=%u data=%u\n", result.stage, result.job, result.status, result.detail, result.data);
      ++failures;
    }
    seen |= bit;
  }
  return failures + (seen != expected);
}

static int verify_output(void) {
  int8_t relu1[POSITIONS][CHANNELS];
  int8_t conv2[POSITIONS][CHANNELS];
  int failures = 0;

  for (unsigned position = 0; position < POSITIONS; ++position) {
    for (unsigned output = 0; output < CHANNELS; ++output) {
      int32_t sum = 0;
      for (unsigned input = 0; input < CHANNELS; ++input) {
        sum += (int32_t)INPUT[position][input] * (int32_t)WEIGHT1[input][output];
      }
      relu1[position][output] = requant(sum > 0 ? sum : 0);
    }
  }
  for (unsigned position = 0; position < POSITIONS; ++position) {
    for (unsigned output = 0; output < CHANNELS; ++output) {
      int32_t sum = 0;
      for (unsigned input = 0; input < CHANNELS; ++input) {
        sum += (int32_t)relu1[position][input] * (int32_t)WEIGHT2[input][output];
      }
      conv2[position][output] = requant(sum);
    }
  }
  for (unsigned position = 0; position < POSITIONS; ++position) {
    for (unsigned channel = 0; channel < CHANNELS; ++channel) {
      const unsigned word = position * CHANNELS + channel;
      const int32_t sum = (int32_t)conv2[position][channel] + (int32_t)INPUT[position][channel];
      const int32_t expected = sum > 0 ? sum : 0;
      const int32_t actual = (int32_t)(uint32_t)add_relu_read_mem_fast(OUTPUT_WORD + word);
      if (actual != expected) {
        printf("Residual mismatch index=%u actual=%d expected=%d\n", word, (int)actual, (int)expected);
        ++failures;
      }
    }
  }
  return failures;
}

int main(void) {
  init_inputs();
  preload_gemmini();
  if (preload_skip() != 0) {
    printf("Gemmini + CGRA Residual Auto: FAIL\n");
    return 1;
  }
  configure_cgra();
  if (configure_gemmini() != 0) {
    printf("Gemmini + CGRA Residual Auto: FAIL\n");
    return 1;
  }

  // Reuse the captured jobs and resident configurations for a second run.
  for (unsigned run = 0; run < 2; ++run) {
    auto_link_input_ready();
    int failures = verify_results();
    failures += verify_output();
    if (failures != 0) {
      printf("Gemmini + CGRA Residual Auto: FAIL (%d)\n", failures);
      return 1;
    }
  }
  printf("Gemmini + CGRA Residual Auto: PASS\n");
  return 0;
}
