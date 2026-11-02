#ifndef GEMMINI_CONV_H
#define GEMMINI_CONV_H

#include "../../runtime/planning/workspace.h"
#include "gemmini.h"
#include "gemmini_job.h"

/* Complete native NHWC invocation; a_spad_id=b_spad_id=1 reserves the lower half. */
#define GEMMINI_CONV_COMMANDS 9

typedef struct {
  unsigned input_rows, input_columns, input_channels;
  unsigned output_rows, output_columns, output_channels;
  unsigned kernel, stride, dilation, padding_top, padding_left;
  const elem_t *input, *weight;
  const acc_t *bias;
  elem_t *output;
  int activation;
  acc_scale_t scale;
} gemmini_conv_t;

static inline int gemmini_conv_valid(const gemmini_conv_t *p, unsigned input_elements, unsigned weight_elements, unsigned output_elements) {
  const unsigned dimensions[] = {p->input_rows, p->input_columns, p->input_channels, p->output_rows, p->output_columns, p->output_channels, p->kernel, p->dilation};
  for (unsigned index = 0; index < sizeof(dimensions) / sizeof(dimensions[0]); ++index) {
    if (!dimensions[index] || dimensions[index] > UINT16_MAX) {
      return 0;
    }
  }
  /* Native command fields use eight-bit stride/top and ten-bit dilation. */
  if (!p->stride || p->stride > UINT8_MAX || p->padding_top > UINT8_MAX || p->padding_left > UINT16_MAX || p->dilation > 1023) {
    return 0;
  }
  if ((uint64_t)p->input_rows * p->input_columns * p->input_channels != input_elements || (uint64_t)p->kernel * p->kernel * p->input_channels * p->output_channels != weight_elements ||
      (uint64_t)p->output_rows * p->output_columns * p->output_channels != output_elements) {
    return 0;
  }
  const uint64_t extent = (uint64_t)(p->kernel - 1) * p->dilation;
  const uint64_t rows = (uint64_t)p->output_rows * p->stride + extent;
  const uint64_t columns = (uint64_t)p->output_columns * p->stride + extent;
  if (rows > BANK_NUM * BANK_ROWS || columns > BANK_NUM * BANK_ROWS || p->padding_top > extent || p->padding_left > extent) {
    return 0;
  }
  if (rows > (uint64_t)p->padding_top + p->input_rows + UINT8_MAX || columns > (uint64_t)p->padding_left + p->input_columns + UINT16_MAX) {
    return 0;
  }
  return gemmini_workspace_fits(gemmini_conv_workspace(p->output_rows, p->output_columns, p->input_channels, p->output_channels, p->kernel, p->stride, p->dilation));
}

/* Manual submission and capture share these native commands. */
static inline void gemmini_conv_emit(const gemmini_conv_t *p) {
  const int extent = (p->kernel - 1) * p->dilation;
  const int bottom = (int)(p->output_rows * p->stride) + extent - (int)p->padding_top - (int)p->input_rows;
  const int right = (int)(p->output_columns * p->stride) + extent - (int)p->padding_left - (int)p->input_columns;
  gemmini_extended_config_st(p->output_channels * sizeof(elem_t), p->activation, p->scale);
  gemmini_extended3_config_ex(WEIGHT_STATIONARY, 0, 0, 0, 1, p->stride, false, false, false);
  const acc_t *accumulator = p->bias == NULL ? (const acc_t *)(uintptr_t)1 : p->bias;
  sp_tiled_conv(1, p->input_rows, p->input_columns, p->input_channels, p->output_channels, p->output_rows, p->output_columns, p->output_rows, p->output_columns, p->stride, p->padding_top, p->kernel,
                p->dilation, p->input_channels, p->output_channels, p->output_channels, 1, 1, 0, 1, p->output_rows, p->output_columns, p->output_channels, p->kernel, p->kernel, p->input_channels,
                p->padding_left, right > 0 ? right : 0, p->padding_top, bottom > 0 ? bottom : 0, 0, 0, 0, 0, p->input, p->weight, p->output, accumulator, p->activation, p->scale, false, false, false,
                false, false, p->bias == NULL, true, false, false, false, 1, 1);
}

static inline int gemmini_conv_capture(uintptr_t control, unsigned job, const gemmini_conv_t *params) {
  if (gemmini_job_capture_at(control, job, GEMMINI_CONV_COMMANDS, 0)) {
    return 1;
  }
  gemmini_conv_emit(params);
  return gemmini_job_end_at(control);
}

// Batch-one NHWC with trusted native storage and tile dimensions.
static inline int gemmini_capture_conv_at(uintptr_t control, uint32_t job, int input_rows, int input_columns, int input_channels, int output_channels, int kernel, int stride, int padding,
                                          int dilation, int tile_rows, int tile_columns, const elem_t *input, const elem_t *weights, const acc_t *bias, elem_t *output, int activation,
                                          acc_scale_t scale) {
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
  gemmini_job_write_at(control, GEMMINI_JOB_WINDOW_ROWS, input_rows);
  gemmini_job_write_at(control, GEMMINI_JOB_WINDOW_COLUMNS, input_columns);
  gemmini_job_write_at(control, GEMMINI_JOB_WINDOW_ROW_STEP, stride);
  gemmini_job_write_at(control, GEMMINI_JOB_WINDOW_COLUMN_STEP, stride);
  gemmini_job_write_at(control, GEMMINI_JOB_WINDOW_TOP, padding);
  gemmini_job_write_at(control, GEMMINI_JOB_WINDOW_BOTTOM, extent - padding);
  gemmini_job_write_at(control, GEMMINI_JOB_WINDOW_LEFT, padding);
  gemmini_job_write_at(control, GEMMINI_JOB_WINDOW_RIGHT, extent - padding);
  *(volatile uint64_t *)(control + GEMMINI_JOB_WINDOW_ADDRESS) = (uintptr_t)input;
  gemmini_job_write_at(control, GEMMINI_JOB_WINDOW_PIXEL_BYTES, input_channels * sizeof(elem_t));
  gemmini_job_write_at(control, GEMMINI_JOB_WINDOW_ROW_BYTES, input_columns * input_channels * sizeof(elem_t));
  if (gemmini_job_capture_at(control, job, 9, count) != 0) {
    return 1;
  }
  for (unsigned index = 0; index < count; ++index) {
    gemmini_job_patch_at(control, &patches[index]);
  }
  const gemmini_conv_t params = {.input_rows = input_rows,
                                 .input_columns = input_columns,
                                 .input_channels = input_channels,
                                 .output_rows = tile_rows,
                                 .output_columns = tile_columns,
                                 .output_channels = output_channels,
                                 .kernel = kernel,
                                 .stride = stride,
                                 .dilation = dilation,
                                 .padding_top = padding,
                                 .padding_left = padding,
                                 .input = input,
                                 .weight = weights,
                                 .bias = bias,
                                 .output = output,
                                 .activation = activation,
                                 .scale = scale};
  gemmini_conv_emit(&params);
  return gemmini_job_end_at(control);
}

static inline int gemmini_capture_conv(uint32_t job, int input_rows, int input_columns, int input_channels, int output_channels, int kernel, int stride, int padding, int dilation, int tile_rows,
                                       int tile_columns, const elem_t *input, const elem_t *weights, const acc_t *bias, elem_t *output, int activation, acc_scale_t scale) {
  return gemmini_capture_conv_at(GEMMINI_JOB_BASE, job, input_rows, input_columns, input_channels, output_channels, kernel, stride, padding, dilation, tile_rows, tile_columns, input, weights, bias,
                                 output, activation, scale);
}

#endif
