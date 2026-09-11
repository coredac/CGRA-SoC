#ifndef GEMMINI_CONV_H
#define GEMMINI_CONV_H

#include "gemmini.h"
#include "gemmini_ext_spm.h"
#include "gemmini_job.h"

static inline int gemmini_ranges_overlap(uintptr_t first, uint64_t first_bytes, uintptr_t second, uint64_t second_bytes) { return first < second + second_bytes && second < first + first_bytes; }

// Validate the maximum tile against the native A/B half and accumulator allocation.
static inline int gemmini_conv_fits(int input_rows, int input_columns, int input_channels, int output_channels, int kernel, int stride, int padding, int dilation, int tile_rows, int tile_columns,
                                    const elem_t *input, const elem_t *weights, elem_t *output) {
  if (input_rows <= 0 || input_rows > UINT16_MAX || input_columns <= 0 || input_columns > UINT16_MAX || input_channels <= 0 || input_channels >= 32768 || output_channels <= 0 ||
      output_channels > UINT16_MAX || kernel <= 0 || kernel > UINT16_MAX || stride <= 0 || stride > UINT8_MAX || padding < 0 || padding > UINT8_MAX || dilation <= 0 || dilation >= 1024 ||
      tile_rows <= 0 || tile_rows > UINT16_MAX || tile_columns <= 0 || tile_columns > UINT16_MAX || input == NULL || weights == NULL) {
    return 0;
  }
  const uint64_t extent = (uint64_t)(kernel - 1) * dilation;
  const uint64_t rows = (uint64_t)tile_rows * stride + extent;
  const uint64_t columns = (uint64_t)tile_columns * stride + extent;
  const uint64_t a_rows = ((input_channels + DIM - 1) / DIM) * rows * columns;
  const uint64_t b_rows = (uint64_t)((output_channels + DIM - 1) / DIM) * kernel * kernel * input_channels;
  const uint64_t c_rows = (uint64_t)((output_channels + DIM - 1) / DIM) * tile_rows * tile_columns;
  const uintptr_t spm_end = GEMMINI_EXT_SPM_BASE + GEMMINI_EXT_SPM_SIZE_BYTES;
  const uint64_t row_bytes = DIM * sizeof(elem_t);
  const uint64_t half_rows = BANK_NUM * BANK_ROWS / 2;
  const uint64_t input_bytes = (uint64_t)input_rows * input_columns * input_channels * sizeof(elem_t);
  const uintptr_t source = (uintptr_t)input;
  const uintptr_t destination = (uintptr_t)output;
  if (rows >= 32768 || columns >= 32768 || a_rows + b_rows > half_rows || c_rows > ACC_ROWS / 2 || input_bytes > UINT32_MAX || source > UINTPTR_MAX - input_bytes ||
      destination < GEMMINI_EXT_SPM_BASE || destination >= spm_end || (uint64_t)tile_rows * tile_columns * output_channels * sizeof(elem_t) > spm_end - destination) {
    return 0;
  }
  const uintptr_t weights_start = GEMMINI_EXT_SPM_BASE + (half_rows - b_rows) * row_bytes;
  const uint64_t output_bytes = spm_end - destination;
  return !gemmini_ranges_overlap(source, input_bytes, GEMMINI_EXT_SPM_BASE, a_rows * row_bytes) && !gemmini_ranges_overlap(source, input_bytes, weights_start, b_rows * row_bytes) &&
         !gemmini_ranges_overlap(destination, output_bytes, GEMMINI_EXT_SPM_BASE, a_rows * row_bytes) && !gemmini_ranges_overlap(destination, output_bytes, weights_start, b_rows * row_bytes) &&
         !gemmini_ranges_overlap(source, input_bytes, destination, output_bytes);
}

// Batch-one NHWC; tile_rows/columns bound every run sharing this captured sequence.
static inline int gemmini_capture_conv(uint32_t job, int input_rows, int input_columns, int input_channels, int output_channels, int kernel, int stride, int padding, int dilation, int tile_rows,
                                       int tile_columns, const elem_t *input, const elem_t *weights, const acc_t *bias, elem_t *output, int activation, acc_scale_t scale) {
  if (!gemmini_conv_fits(input_rows, input_columns, input_channels, output_channels, kernel, stride, padding, dilation, tile_rows, tile_columns, input, weights, output)) {
    return 1;
  }
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
  gemmini_job_write(GEMMINI_JOB_WINDOW_OUTPUT_BYTES, output_channels * sizeof(elem_t));
  gemmini_job_write(GEMMINI_JOB_WINDOW_MAX_ROWS, tile_rows);
  gemmini_job_write(GEMMINI_JOB_WINDOW_MAX_COLUMNS, tile_columns);
  *(volatile uint64_t *)(GEMMINI_JOB_BASE + GEMMINI_JOB_WINDOW_OUTPUT_BASE) = (uintptr_t)output;
  gemmini_job_write(GEMMINI_JOB_WINDOW_OUTPUT_SIZE, GEMMINI_EXT_SPM_BASE + GEMMINI_EXT_SPM_SIZE_BYTES - (uintptr_t)output);
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
