#ifndef SOC_NATIVE_WORKSPACE_H
#define SOC_NATIVE_WORKSPACE_H

#include "gemmini_params.h"

#include <stdint.h>

#define GEMMINI_CONV_WORKSPACE_DIVISOR 2

typedef struct {
  uint64_t scratch_rows, accumulator_rows;
} gemmini_workspace_t;

static inline gemmini_workspace_t gemmini_matrix_workspace(unsigned m, unsigned k, unsigned n) {
  const uint64_t i = ((uint64_t)m + DIM - 1) / DIM;
  const uint64_t j = ((uint64_t)n + DIM - 1) / DIM;
  const uint64_t inner = ((uint64_t)k + DIM - 1) / DIM;
  return (gemmini_workspace_t){(i * inner + inner * j) * DIM, i * j * DIM};
}

/* Batch-one NHWC, without input dilation, transposition, or fused pooling. */
static inline gemmini_workspace_t gemmini_conv_workspace(unsigned rows, unsigned columns, unsigned inputs, unsigned outputs, unsigned kernel, unsigned stride, unsigned dilation) {
  const uint64_t extent = (uint64_t)(kernel - 1) * dilation;
  const uint64_t input_rows = (uint64_t)rows * stride + extent;
  const uint64_t input_columns = (uint64_t)columns * stride + extent;
  const uint64_t input_groups = ((uint64_t)inputs + DIM - 1) / DIM;
  const uint64_t output_groups = ((uint64_t)outputs + DIM - 1) / DIM;
  return (gemmini_workspace_t){input_groups * input_rows * input_columns + output_groups * kernel * kernel * inputs, output_groups * rows * columns};
}

static inline int gemmini_workspace_fits(gemmini_workspace_t workspace) { return workspace.scratch_rows <= BANK_NUM * BANK_ROWS / 2 && workspace.accumulator_rows <= ACC_ROWS / 2; }

static inline gemmini_workspace_t gemmini_copy_workspace(unsigned rows, unsigned columns, unsigned element_bytes) {
  const uint64_t footprint = ((uint64_t)(columns - 1) / DIM) * DIM + rows;
  return element_bytes == sizeof(elem_t) ? (gemmini_workspace_t){footprint, 0} : (gemmini_workspace_t){0, footprint};
}

static inline int gemmini_copy_valid(unsigned rows, unsigned columns, unsigned element_bytes, uint64_t source_stride, uint64_t destination_stride) {
  /* Full accumulator mvout permits one block; byte moves use the DMA block limit. */
  const uint64_t max_columns = element_bytes == sizeof(elem_t) ? (uint64_t)MAX_BLOCK_LEN * DIM : DIM;
  if (!rows || !columns || rows > DIM || columns > UINT16_MAX || columns > max_columns || (element_bytes != sizeof(elem_t) && element_bytes != sizeof(acc_t))) {
    return 0;
  }
  const unsigned bytes = columns * element_bytes;
  return source_stride >= bytes && destination_stride >= bytes && source_stride <= UINT32_MAX && destination_stride <= UINT32_MAX &&
         gemmini_workspace_fits(gemmini_copy_workspace(rows, columns, element_bytes));
}

#endif
