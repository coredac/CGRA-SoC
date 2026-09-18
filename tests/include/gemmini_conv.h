#ifndef GEMMINI_CONV_H
#define GEMMINI_CONV_H

#include "gemmini.h"
#include "gemmini_job.h"

// Batch-one NHWC with trusted native storage and tile dimensions.
static inline int gemmini_capture_conv(uint32_t job, int input_rows, int input_columns, int input_channels, int output_channels, int kernel, int stride, int padding, int dilation, int tile_rows,
                                       int tile_columns, const elem_t *input, const elem_t *weights, const acc_t *bias, elem_t *output, int activation, acc_scale_t scale) {
  // Indices refer to the two setup commands followed by six native loop configuration commands.
  static const gemmini_patch_t patches[] = {
      {2, 0, 16, 16, GEMMINI_VALUE_INPUT_ROWS, 1, 0},    {2, 1, 32, 16, GEMMINI_VALUE_COLUMNS, 1, 0},      {2, 1, 16, 16, GEMMINI_VALUE_ROWS, 1, 0},
      {2, 1, 0, 16, GEMMINI_VALUE_ROWS, 1, 0},           {3, 0, 32, 16, GEMMINI_VALUE_COLUMNS, 1, 0},      {3, 1, 32, 16, GEMMINI_VALUE_ROWS, 1, 0},
      {3, 1, 16, 16, GEMMINI_VALUE_COLUMNS, 1, 0},       {4, 0, 0, 16, GEMMINI_VALUE_LEFT, 1, 0},          {4, 1, 48, 16, GEMMINI_VALUE_RIGHT, 1, 0},
      {4, 1, 32, 16, GEMMINI_VALUE_TOP, 1, 0},           {4, 1, 24, 8, GEMMINI_VALUE_BOTTOM, 1, 0},        {4, 1, 0, 16, GEMMINI_VALUE_INPUT_COLUMNS, 1, 0},
      {5, 0, 48, 16, GEMMINI_VALUE_ROWS, 1, 0},          {5, 1, 48, 16, GEMMINI_VALUE_INPUT_STRIDE, 1, 0}, {5, 1, 0, 16, GEMMINI_VALUE_COLUMNS, 1, 0},
      {6, 1, 0, 64, GEMMINI_VALUE_OUTPUT_ADDRESS, 1, 0}, {7, 1, 0, 64, GEMMINI_VALUE_INPUT_ADDRESS, 1, 0},
  };
  const unsigned count = sizeof(patches) / sizeof(patches[0]);
  const int extent = (kernel - 1) * dilation;
  gemmini_job_write(GEMMINI_JOB_WINDOW_ROWS, input_rows);
  gemmini_job_write(GEMMINI_JOB_WINDOW_COLUMNS, input_columns);
  gemmini_job_write(GEMMINI_JOB_WINDOW_ROW_STEP, stride);
  gemmini_job_write(GEMMINI_JOB_WINDOW_COLUMN_STEP, stride);
  gemmini_job_write(GEMMINI_JOB_WINDOW_TOP, padding);
  gemmini_job_write(GEMMINI_JOB_WINDOW_BOTTOM, extent - padding);
  gemmini_job_write(GEMMINI_JOB_WINDOW_LEFT, padding);
  gemmini_job_write(GEMMINI_JOB_WINDOW_RIGHT, extent - padding);
  *(volatile uint64_t *)(GEMMINI_JOB_BASE + GEMMINI_JOB_WINDOW_ADDRESS) = (uintptr_t)input;
  gemmini_job_write(GEMMINI_JOB_WINDOW_PIXEL_BYTES, input_channels * sizeof(elem_t));
  gemmini_job_write(GEMMINI_JOB_WINDOW_ROW_BYTES, input_columns * input_channels * sizeof(elem_t));
  if (gemmini_job_capture(job, 9, count) != 0) {
    return 1;
  }
  for (unsigned index = 0; index < count; ++index) {
    gemmini_job_patch(&patches[index]);
  }
  const int bottom = tile_rows * stride + extent - padding - input_rows;
  const int right = tile_columns * stride + extent - padding - input_columns;
  gemmini_extended_config_st(output_channels * sizeof(elem_t), activation, scale);
  gemmini_extended3_config_ex(WEIGHT_STATIONARY, 0, 0, 0, 1, 1, false, false, false);
  // A non-null ignored bias pointer requests a fresh zero accumulator in native loop_conv.
  const acc_t *accumulator = bias == NULL ? (const acc_t *)(uintptr_t)1 : bias;
  sp_tiled_conv(1, input_rows, input_columns, input_channels, output_channels, tile_rows, tile_columns, tile_rows, tile_columns, stride, padding, kernel, dilation, input_channels, output_channels,
                output_channels, 1, 1, 0, 1, tile_rows, tile_columns, output_channels, kernel, kernel, input_channels, padding, right > 0 ? right : 0, padding, bottom > 0 ? bottom : 0, 0, 0, 0, 0,
                input, weights, output, accumulator, activation, scale, false, false, false, false, false, bias == NULL, true, false, false, false, 1, 1);
  return gemmini_job_end();
}

#endif
