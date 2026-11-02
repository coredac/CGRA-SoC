#include "Backend/SoC/Query.h"
#include "workspace.h"

#include <limits.h>
#include <string.h>

static void initialize(amoeba_query_result_t *result) {
  *result = (amoeba_query_result_t){.abi = AMOEBA_QUERY_ABI,
                                    .spm_bytes = (uint64_t)BANK_NUM * BANK_ROWS * DIM,
                                    .scratch_capacity = (uint64_t)BANK_NUM * BANK_ROWS * DIM / 2,
                                    .accumulator_capacity = (uint64_t)ACC_ROWS * DIM * sizeof(acc_t) / 2};
}

static int reject(amoeba_query_result_t *result, const char *error) {
  result->error = error;
  return 1;
}

static int parameter(const amoeba_query_call_t *call, const char *name, unsigned *value) {
  unsigned found = 0;
  for (unsigned index = 0; index < call->parameter_count; ++index) {
    const amoeba_query_parameter_t *item = &call->parameters[index];
    if (strcmp(item->name, name) == 0) {
      if (item->value > UINT32_MAX || found++)
        return 0;
      *value = item->value;
    }
  }
  return found == 1;
}

static int view_matches(amoeba_query_view_t view, uint64_t rows, uint64_t columns, unsigned bytes) {
  if (!rows || !columns || columns > UINT64_MAX / bytes || view.rows != rows || view.columns != columns || view.element_bytes != bytes || view.row_stride < columns * bytes ||
      view.row_stride > UINT32_MAX || view.row_stride % bytes || view.offset % bytes || view.offset > UINT64_MAX - columns * bytes)
    return 0;
  return rows - 1 <= (UINT64_MAX - view.offset - columns * bytes) / view.row_stride;
}

static amoeba_query_view_t slice(amoeba_query_view_t view, uint64_t row, uint64_t column, uint64_t rows, uint64_t columns) {
  view.offset += row * view.row_stride + column * view.element_bytes;
  view.rows = rows;
  view.columns = columns;
  return view;
}

static int tile_valid(const amoeba_query_tile_t *tile, amoeba_query_result_t *result) {
  if (tile->rank != result->rank) {
    result->error = "tile rank differs from the native domain";
    return 0;
  }
  for (unsigned index = 0; index < tile->rank; ++index) {
    const amoeba_query_axis_t axis = result->axes[index];
    if (!tile->extent[index] || tile->origin[index] >= axis.extent || tile->extent[index] > axis.extent - tile->origin[index] || tile->origin[index] % axis.alignment ||
        (tile->origin[index] + tile->extent[index] < axis.extent && tile->extent[index] % axis.alignment)) {
      result->error = "tile lies outside the native domain or violates its alignment";
      return 0;
    }
  }
  return 1;
}

static void workspace(gemmini_workspace_t value, amoeba_query_result_t *result) {
  result->scratch_bytes = value.scratch_rows * DIM;
  result->accumulator_bytes = value.accumulator_rows * DIM * sizeof(acc_t);
  result->legal = gemmini_workspace_fits(value);
  if (!result->legal)
    result->error = "tile exceeds native scratch or accumulator workspace";
}

int amoeba_query_matrix(const amoeba_query_call_t *call, const amoeba_query_tile_t *tile, amoeba_query_result_t *result) {
  static _Thread_local amoeba_query_axis_t axes[3];
  static _Thread_local amoeba_query_access_t inputs[3], outputs[1];
  static _Thread_local amoeba_query_parameter_t parameters[10];
  initialize(result);
  if (call->abi != AMOEBA_QUERY_ABI || call->parameter_count != 5 || call->input_count != 3 || call->output_count != 1)
    return reject(result, "matrix query ABI or operand count mismatch");
  unsigned m, n, k, transpose_a, transpose_b;
  if (!parameter(call, "m", &m) || !parameter(call, "n", &n) || !parameter(call, "k", &k) || !parameter(call, "transpose_a", &transpose_a) || !parameter(call, "transpose_b", &transpose_b) || !m ||
      !n || !k || m > UINT16_MAX || n > UINT16_MAX || k > UINT16_MAX || transpose_a > 1 || transpose_b > 1 || (transpose_a && transpose_b))
    return reject(result, "invalid native matrix parameters");
  if (!view_matches(call->inputs[0], transpose_a ? k : m, transpose_a ? m : k, 1) || !view_matches(call->inputs[1], transpose_b ? n : k, transpose_b ? k : n, 1) ||
      !view_matches(call->inputs[2], 1, n, 4) || !view_matches(call->outputs[0], m, n, 4))
    return reject(result, "matrix shapes, formats, or row strides disagree with the native call");
  axes[0] = (amoeba_query_axis_t){"m", m, DIM, AMOEBA_QUERY_PARALLEL};
  axes[1] = (amoeba_query_axis_t){"n", n, DIM, AMOEBA_QUERY_PARALLEL};
  axes[2] = (amoeba_query_axis_t){"k", k, DIM, AMOEBA_QUERY_REDUCTION};
  result->rank = 3;
  result->axes = axes;
  if (!tile) {
    result->legal = 1;
    return 0;
  }
  if (!tile_valid(tile, result))
    return 0;
  const uint64_t row = tile->origin[0], column = tile->origin[1], inner = tile->origin[2];
  const unsigned rows = tile->extent[0], columns = tile->extent[1], channels = tile->extent[2];
  const unsigned first = inner == 0, last = inner + channels == k;
  inputs[0] = (amoeba_query_access_t){transpose_a ? slice(call->inputs[0], inner, row, channels, rows) : slice(call->inputs[0], row, inner, rows, channels), 1};
  inputs[1] = (amoeba_query_access_t){transpose_b ? slice(call->inputs[1], column, inner, columns, channels) : slice(call->inputs[1], inner, column, channels, columns), 1};
  inputs[2] = (amoeba_query_access_t){slice(call->inputs[2], 0, column, 1, columns), first};
  outputs[0] = (amoeba_query_access_t){slice(call->outputs[0], row, column, rows, columns), last};
  parameters[0] = (amoeba_query_parameter_t){"m", rows};
  parameters[1] = (amoeba_query_parameter_t){"n", columns};
  parameters[2] = (amoeba_query_parameter_t){"k", channels};
  parameters[3] = (amoeba_query_parameter_t){"transpose_a", transpose_a};
  parameters[4] = (amoeba_query_parameter_t){"transpose_b", transpose_b};
  parameters[5] = (amoeba_query_parameter_t){"input_stride", inputs[0].view.row_stride};
  parameters[6] = (amoeba_query_parameter_t){"weight_stride", inputs[1].view.row_stride};
  parameters[7] = (amoeba_query_parameter_t){"output_stride", outputs[0].view.row_stride};
  parameters[8] = (amoeba_query_parameter_t){"accumulate", !first};
  parameters[9] = (amoeba_query_parameter_t){"store", last};
  result->input_count = 3;
  result->output_count = 1;
  result->parameter_count = 10;
  result->inputs = inputs;
  result->outputs = outputs;
  result->parameters = parameters;
  workspace(gemmini_matrix_workspace(rows, channels, columns), result);
  return 0;
}

int amoeba_query_conv(const amoeba_query_call_t *call, const amoeba_query_tile_t *tile, amoeba_query_result_t *result) {
  static const char *const names[] = {"input_rows", "input_columns", "input_channels", "output_rows", "output_columns", "output_channels",
                                      "kernel",     "stride",        "dilation",       "padding_top", "padding_left"};
  static _Thread_local amoeba_query_axis_t axes[1];
  static _Thread_local amoeba_query_access_t inputs[2], outputs[1];
  static _Thread_local amoeba_query_parameter_t parameters[11];
  initialize(result);
  if (call->abi != AMOEBA_QUERY_ABI || call->parameter_count != 11 || call->input_count != 2 || call->output_count != 1)
    return reject(result, "Conv query ABI or operand count mismatch");
  unsigned values[11];
  for (unsigned index = 0; index < 11; ++index) {
    if (!parameter(call, names[index], &values[index]) || values[index] > UINT16_MAX || (index < 9 && !values[index]))
      return reject(result, "invalid native Conv parameters");
    parameters[index] = (amoeba_query_parameter_t){names[index], values[index]};
  }
  const unsigned height = values[0], width = values[1], channels = values[2];
  const unsigned out_height = values[3], out_width = values[4], out_channels = values[5];
  const unsigned kernel = values[6], stride = values[7], dilation = values[8], top = values[9], left = values[10];
  const uint64_t halo = (uint64_t)(kernel - 1) * dilation;
  if (stride > UINT8_MAX || dilation > 1023 || top > UINT8_MAX || top > halo || left > halo)
    return reject(result, "Conv padding, stride, or dilation exceeds the native encoding");
  if (!view_matches(call->inputs[0], (uint64_t)height * width, channels, 1) || !view_matches(call->inputs[1], (uint64_t)kernel * kernel * channels, out_channels, 1) ||
      !view_matches(call->outputs[0], (uint64_t)out_height * out_width, out_channels, 1) || call->inputs[0].row_stride != channels || call->inputs[1].row_stride != out_channels ||
      call->outputs[0].row_stride != out_channels)
    return reject(result, "Conv requires contiguous NHWC buffers and matching geometry");
  axes[0] = (amoeba_query_axis_t){"output_rows", out_height, 1, AMOEBA_QUERY_PARALLEL};
  result->rank = 1;
  result->axes = axes;
  if (!tile) {
    result->legal = 1;
    return 0;
  }
  if (!tile_valid(tile, result))
    return 0;
  const uint64_t origin = tile->origin[0], rows = tile->extent[0];
  const int64_t first_row = (int64_t)(origin * stride) - top;
  const int64_t last_row = (int64_t)((origin + rows - 1) * stride + halo + 1) - top;
  const uint64_t start = first_row > 0 ? (uint64_t)first_row : 0;
  const uint64_t end = last_row < height ? (uint64_t)last_row : height;
  if (last_row <= 0 || start >= end) {
    result->error = "Conv tile does not intersect the input image";
    return 0;
  }
  if ((end - start) * width * channels > UINT32_MAX || (uint64_t)kernel * kernel * channels * out_channels > UINT32_MAX || rows * out_width * out_channels > UINT32_MAX) {
    result->error = "Conv tile element counts exceed the native interface";
    return 0;
  }
  const uint64_t tile_top = first_row < 0 ? (uint64_t)-first_row : 0;
  const uint64_t native_rows = rows * stride + halo, native_columns = (uint64_t)out_width * stride + halo;
  if (native_rows > BANK_NUM * BANK_ROWS || native_columns > BANK_NUM * BANK_ROWS || native_rows > tile_top + end - start + UINT8_MAX || native_columns > (uint64_t)left + width + UINT16_MAX) {
    result->error = "Conv tile exceeds native window or padding limits";
    return 0;
  }
  inputs[0] = (amoeba_query_access_t){slice(call->inputs[0], start * width, 0, (end - start) * width, channels), 1};
  inputs[1] = (amoeba_query_access_t){call->inputs[1], 1};
  outputs[0] = (amoeba_query_access_t){slice(call->outputs[0], origin * out_width, 0, rows * out_width, out_channels), 1};
  parameters[0].value = end - start;
  parameters[3].value = rows;
  parameters[9].value = tile_top;
  result->input_count = 2;
  result->output_count = 1;
  result->parameter_count = 11;
  result->inputs = inputs;
  result->outputs = outputs;
  result->parameters = parameters;
  workspace(gemmini_conv_workspace(rows, out_width, channels, out_channels, kernel, stride, dilation), result);
  return 0;
}

int amoeba_query_copy(const amoeba_query_call_t *call, const amoeba_query_tile_t *tile, amoeba_query_result_t *result) {
  static _Thread_local amoeba_query_axis_t axes[2];
  static _Thread_local amoeba_query_access_t inputs[1], outputs[1];
  static _Thread_local amoeba_query_parameter_t parameters[5];
  initialize(result);
  if (call->abi != AMOEBA_QUERY_ABI || call->parameter_count != 0 || call->input_count != 1 || call->output_count != 1)
    return reject(result, "copy query ABI or operand count mismatch");
  const amoeba_query_view_t source = call->inputs[0], destination = call->outputs[0];
  const unsigned bytes = source.element_bytes;
  if ((bytes != sizeof(elem_t) && bytes != sizeof(acc_t)) || !view_matches(source, source.rows, source.columns, bytes) || !view_matches(destination, source.rows, source.columns, bytes))
    return reject(result, "copy requires matching INT8 or INT32 views and positive row strides");
  const int dense = source.row_stride == source.columns * bytes && destination.row_stride == destination.columns * bytes;
  if (dense) {
    axes[0] = (amoeba_query_axis_t){"elements", source.rows * source.columns, DIM, AMOEBA_QUERY_PARALLEL};
    result->rank = 1;
  } else {
    axes[0] = (amoeba_query_axis_t){"rows", source.rows, 1, AMOEBA_QUERY_PARALLEL};
    axes[1] = (amoeba_query_axis_t){"columns", source.columns, 1, AMOEBA_QUERY_PARALLEL};
    result->rank = 2;
  }
  result->axes = axes;
  if (!tile) {
    result->legal = 1;
    return 0;
  }
  if (!tile_valid(tile, result))
    return 0;
  uint64_t rows, columns;
  if (dense) {
    const uint64_t count = tile->extent[0];
    if (count > (uint64_t)DIM * DIM || (count > DIM && count % DIM)) {
      result->error = "dense copy tile must fit a native rectangle";
      return 0;
    }
    rows = count > DIM ? count / DIM : 1;
    columns = count > DIM ? DIM : count;
    const uint64_t offset = tile->origin[0] * bytes, stride = columns * bytes;
    inputs[0] = (amoeba_query_access_t){{source.offset + offset, rows, columns, stride, bytes}, 1};
    outputs[0] = (amoeba_query_access_t){{destination.offset + offset, rows, columns, stride, bytes}, 1};
  } else {
    rows = tile->extent[0];
    columns = tile->extent[1];
    inputs[0] = (amoeba_query_access_t){slice(source, tile->origin[0], tile->origin[1], rows, columns), 1};
    outputs[0] = (amoeba_query_access_t){slice(destination, tile->origin[0], tile->origin[1], rows, columns), 1};
  }
  if (rows > UINT32_MAX || columns > UINT32_MAX || !gemmini_copy_valid(rows, columns, bytes, inputs[0].view.row_stride, outputs[0].view.row_stride)) {
    result->error = "tile exceeds native copy dimensions or strides";
    return 0;
  }
  parameters[0] = (amoeba_query_parameter_t){"rows", rows};
  parameters[1] = (amoeba_query_parameter_t){"columns", columns};
  parameters[2] = (amoeba_query_parameter_t){"element_bytes", bytes};
  parameters[3] = (amoeba_query_parameter_t){"source_stride", inputs[0].view.row_stride};
  parameters[4] = (amoeba_query_parameter_t){"destination_stride", outputs[0].view.row_stride};
  result->input_count = result->output_count = 1;
  result->parameter_count = 5;
  result->inputs = inputs;
  result->outputs = outputs;
  result->parameters = parameters;
  workspace(gemmini_copy_workspace(rows, columns, bytes), result);
  return 0;
}
