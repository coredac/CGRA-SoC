#ifndef GEMMINI_MATRIX_H
#define GEMMINI_MATRIX_H

#include "../../runtime/planning/workspace.h"
#include "accel.h"
#include "gemmini.h"
#include "gemmini_job.h"

#define GEMMINI_MATRIX_COMMANDS 12
#define GEMMINI_MATRIX_PATCHES 0

/* One native WS loop owns the lower scratch half and one accumulator half. */
typedef struct {
  unsigned m, k, n;
  unsigned transpose_a, transpose_b;
  /* Accumulate retains a prior K chunk instead of loading bias; store ends the reduction. */
  unsigned accumulate, store;
  /* Byte strides describe the original pitched views, not the compact tile shapes. */
  uintptr_t input_stride, weight_stride, output_stride;
  unsigned input_elements, weight_elements, bias_elements, output_elements;
  const elem_t *input, *weight;
  const acc_t *bias;
  acc_t *output;
} gemmini_matrix_t;

static inline int gemmini_matrix_check(const gemmini_matrix_t *params) {
  if (!params->m || !params->k || !params->n || params->transpose_a > 1 || params->transpose_b > 1 || (params->transpose_a && params->transpose_b)) {
    return 1;
  }
  if ((uint64_t)params->m * params->k != params->input_elements || (uint64_t)params->k * params->n != params->weight_elements || params->n != params->bias_elements ||
      (uint64_t)params->m * params->n != params->output_elements) {
    return 1;
  }
  if (params->accumulate > 1 || params->store > 1 || params->input_stride < (params->transpose_a ? params->m : params->k) * sizeof(elem_t) ||
      params->weight_stride < (params->transpose_b ? params->k : params->n) * sizeof(elem_t) || params->output_stride < (uint64_t)params->n * sizeof(acc_t) || params->input_stride > UINT32_MAX ||
      params->weight_stride > UINT32_MAX || params->output_stride > UINT32_MAX || params->output_stride % sizeof(acc_t)) {
    return 1;
  }
  return !gemmini_workspace_fits(gemmini_matrix_workspace(params->m, params->k, params->n));
}

/* Loads happen here, after dependencies complete; preparation does not read inputs. */
static inline void gemmini_matrix_emit(const gemmini_matrix_t *params) {
  const unsigned i = (params->m + DIM - 1) / DIM;
  const unsigned j = (params->n + DIM - 1) / DIM;
  const unsigned k = (params->k + DIM - 1) / DIM;
  gemmini_extended_config_ex(WEIGHT_STATIONARY, NO_ACTIVATION, 0, 1, params->transpose_a, params->transpose_b);
  gemmini_extended3_config_ld(params->input_stride, MVIN_SCALE_IDENTITY, false, 0);
  gemmini_extended3_config_ld(params->weight_stride, MVIN_SCALE_IDENTITY, false, 1);
  gemmini_extended3_config_ld(0, MVIN_SCALE_IDENTITY, false, 2);
  gemmini_config_st(params->output_stride);
  ROCC_INSTRUCTION_RS1_RS2(XCUSTOM_ACC, 0, BANK_NUM * BANK_ROWS / 2, k_LOOP_WS_CONFIG_SPAD_AB);
  /* Keep K chunks consecutive: a non-NULL C advances the native accumulator cursors. */
  gemmini_loop_ws(i, j, k, i * DIM - params->m, j * DIM - params->n, k * DIM - params->k, params->input, params->weight, params->accumulate ? NULL : params->bias,
                  params->store ? params->output : NULL, params->input_stride / sizeof(elem_t), params->weight_stride / sizeof(elem_t), 0, params->output_stride / sizeof(acc_t), params->transpose_a,
                  params->transpose_b, true, false, true, NO_ACTIVATION, 0, 0, false);
}

static inline int gemmini_matrix_capture(accel_t device, unsigned job, const gemmini_matrix_t *params) {
  if (gemmini_matrix_check(params) || gemmini_job_capture_at(device.control, job, GEMMINI_MATRIX_COMMANDS, GEMMINI_MATRIX_PATCHES)) {
    return 1;
  }
  accel_commands(device, { gemmini_matrix_emit(params); });
  return gemmini_job_end_at(device.control);
}

/* Submission only; the runtime owns completion waits. */
static inline int gemmini_matrix_run(accel_t device, const gemmini_matrix_t *params) {
  if (gemmini_matrix_check(params)) {
    return 1;
  }
  accel_commands(device, { gemmini_matrix_emit(params); });
  return 0;
}

#endif
