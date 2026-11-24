#ifndef P1_NATIVE_H
#define P1_NATIVE_H

#include "accel_generated.h"
#include "auto_link.h"
#include "cgra_dma.h"
#include "cgra_link.h"
#include "gemmini.h"
#include "gemmini_job.h"
#include "generated/cgra_attention_exp_fast_api.h"
#include "generated/cgra_attention_max_fast_api.h"
#include "generated/cgra_attention_norm_fast_api.h"
#include "generated/cgra_block_activation_fast_api.h"
#include "generated/cgra_block_ln_mean_fast_api.h"
#include "generated/cgra_block_ln_norm_fast_api.h"
#include "generated/cgra_block_ln_sqrt_fast_api.h"
#include "generated/cgra_block_ln_squares_fast_api.h"
#include "generated/cgra_block_ln_sum_fast_api.h"
#include "generated/cgra_block_ln_variance_fast_api.h"
#include "generated/cgra_block_requant_fast_api.h"
#include "generated/cgra_block_residual_fast_api.h"
#include "generated/gpt2_data.h"

#include <stdio.h>
#include <string.h>

#if DIM != 64
#error "P1 GPT-2 requires Gemmini DIM64"
#endif

enum {
  P1_ROWS = 16,
  P1_CHUNK = 192,
  P1_PUBLICATION = 24576,
  P1_RETAINED = P1_PUBLICATION + 4096,
  P1_WORDS = BLOCK_N * BLOCK_HIDDEN,
  P1_EXPANDED = BLOCK_N * BLOCK_FF,
  P1_LN_ROWS = BLOCK_HIDDEN == 768 ? 2 : 16,
  P1_LN_WORDS = P1_LN_ROWS * BLOCK_HIDDEN,
  P1_AFFINE = 16384,
  P1_LN_OUTPUT = 4096,
  P1_STATS = 6144,
  P1_Q_ROW = 0,
  P1_K_ROW = 64,
  P1_V_ROW = 128,
  P1_P_ROW = 192,
  P1_SCORE_WORDS = P1_ROWS * BLOCK_N,
  P1_ACC_WRITE = UINT32_C(1) << (ADDR_LEN - 1),
  P1_ACC_READ = P1_ACC_WRITE | (UINT32_C(1) << (ADDR_LEN - 3)),
  P1_SKIPS = (1 << 3) | (1 << 4) | (1 << 5) | (1 << 7),
};

enum { P1_COPY, P1_GATHER, P1_COLLECT, P1_MATRIX, P1_LOAD, P1_SCORES, P1_VALUES };

typedef struct {
  unsigned kind, count, packed, source, stride, step, inner, columns, column;
  const void *input, *right;
  const int32_t *bias;
  void *output;
} p1_native_t;

static unsigned long p1_clock(void) {
  unsigned long value;
  __asm__ volatile("rdcycle %0" : "=r"(value) : : "memory");
  return value;
}

static void p1_copy_emit(const p1_native_t *task, unsigned iteration, unsigned tile) {
  const unsigned rows = task->count / DIM;
  const int8_t *input = task->input;
  int8_t *output = task->output;
  gemmini_config_ld(DIM);
  gemmini_config_st(DIM);
  gemmini_extended_mvin(input + iteration * task->count, 0, DIM, rows);
  gemmini_extended_mvout(output + iteration * task->count, 0, DIM, rows);
}

static void p1_gather_emit(const p1_native_t *task, unsigned iteration, unsigned tile) {
  const unsigned bytes = task->packed ? 1 : sizeof(acc_t);
  const unsigned rows = task->count / DIM;
  gemmini_extended3_config_ld(DIM * bytes, MVIN_SCALE_IDENTITY, false, task->packed ? 0 : 2);
  for (unsigned vector = 0; vector < (task->right ? 2 : 1); ++vector) {
    const uint8_t *input = vector ? task->right : task->input;
    const unsigned row = vector * rows;
    const unsigned local = task->packed ? row : P1_ACC_WRITE + row;
    const unsigned source = task->packed ? row : P1_ACC_READ + row;
    if (task->packed) {
      gemmini_extended_mvin(input + iteration * task->count * bytes, local, DIM, rows);
    } else {
      gemmini_extended_mvin3(input + iteration * task->count * bytes, local, DIM, rows);
    }
    gemmini_extended_mvout_spad((P1_PUBLICATION + vector * task->count * bytes) / DIM, bytes, source, DIM, rows);
  }
}

static void p1_collect_emit(const p1_native_t *task, unsigned iteration, unsigned tile) {
  int8_t *output = task->output;
  gemmini_config_ld(DIM);
  gemmini_config_st(task->stride);
  gemmini_extended_mvin((const void *)(CGRA0.spm + task->source), 0, DIM, task->count / DIM);
  gemmini_extended_mvout(output + iteration * task->step, 0, DIM, task->count / DIM);
}

// Native K blocks retain DIM spacing even when the logical output has 16 rows.
static void p1_matrix_emit(const p1_native_t *task, unsigned iteration, unsigned tile) {
  const int8_t *input = task->input;
  const int8_t *weight = task->right;
  const unsigned column = task->column + (BLOCK_N == 16 ? iteration * DIM : 0);
  const unsigned row = BLOCK_N == 16 ? 0 : iteration * P1_ROWS;
  gemmini_extended3_config_ld(task->inner, MVIN_SCALE_IDENTITY, false, 0);
  gemmini_extended3_config_ld(task->columns, MVIN_SCALE_IDENTITY, false, 1);
  gemmini_extended3_config_ld(0, MVIN_SCALE_IDENTITY, false, 2);
  gemmini_config_st(DIM * sizeof(acc_t));
  for (unsigned start = 0; start < task->inner; start += P1_CHUNK) {
    const unsigned width = task->inner - start < P1_CHUNK ? task->inner - start : P1_CHUNK;
    gemmini_config_ex(WEIGHT_STATIONARY, NO_ACTIVATION, 0);
    ROCC_INSTRUCTION_RS1_RS2(XCUSTOM_ACC, 0, P1_CHUNK + width, k_LOOP_WS_CONFIG_SPAD_AB);
    gemmini_loop_ws(1, 1, width / DIM, DIM - P1_ROWS, 0, 0, input + row * task->inner + start, weight + start * task->columns + column, start == 0 ? task->bias + column : NULL, NULL, task->inner,
                    task->columns, 0, DIM, false, false, true, false, true, NO_ACTIVATION, 0, 0, false);
  }
  gemmini_extended_mvout_spad(P1_PUBLICATION / DIM, sizeof(acc_t), P1_ACC_READ, DIM, P1_ROWS);
}

static void p1_load_emit(const p1_native_t *task, unsigned iteration, unsigned tile) {
  const int8_t *q = task->input, *k = task->right, *v = task->output;
  gemmini_config_ld(BLOCK_HIDDEN);
  gemmini_extended_mvin(q + iteration * BLOCK_D, P1_Q_ROW, BLOCK_D, BLOCK_N);
  gemmini_extended_mvin(k + iteration * BLOCK_D, P1_K_ROW, BLOCK_D, BLOCK_N);
  gemmini_extended_mvin(v + iteration * BLOCK_D, P1_V_ROW, BLOCK_D, BLOCK_N);
}

static void p1_scores_emit(const p1_native_t *task, unsigned iteration, unsigned tile) {
  gemmini_config_st(BLOCK_N * sizeof(acc_t));
  gemmini_extended_config_ex(WEIGHT_STATIONARY, NO_ACTIVATION, 0, 1, false, true);
  gemmini_loop_ws_spad(1, 1, 1, DIM - P1_ROWS, DIM - BLOCK_N, 0, P1_Q_ROW + task->source * P1_ROWS, P1_K_ROW + DIM, 0, 0, false, true, true, false, false, NO_ACTIVATION, 0, 0, false, P1_SKIPS);
  gemmini_extended_mvout_spad(P1_PUBLICATION / DIM, BLOCK_N * sizeof(acc_t) / DIM, P1_ACC_READ, BLOCK_N, P1_ROWS);
}

static void p1_values_emit(const p1_native_t *task, unsigned iteration, unsigned tile) {
  acc_t *output = task->output;
  gemmini_config_ld(BLOCK_N);
  gemmini_config_st(BLOCK_HIDDEN * sizeof(acc_t));
  gemmini_config_ex(WEIGHT_STATIONARY, NO_ACTIVATION, 0);
  gemmini_extended_mvin((const void *)(CGRA0.spm + ATTENTION_NORM_ARG2), P1_P_ROW, BLOCK_N, P1_ROWS);
  gemmini_loop_ws_spad(1, 1, 1, DIM - P1_ROWS, 0, DIM - BLOCK_N, P1_P_ROW, P1_V_ROW + DIM, 0, 0, false, false, true, false, false, NO_ACTIVATION, 0, 0, false, P1_SKIPS);
  gemmini_extended_mvout(output + task->source * P1_ROWS * BLOCK_HIDDEN + iteration * BLOCK_D, P1_ACC_READ, BLOCK_D, P1_ROWS);
}

static void p1_emit(const p1_native_t *task, unsigned iteration, unsigned tile) {
  static void (*const emit[])(const p1_native_t *, unsigned, unsigned) = {p1_copy_emit, p1_gather_emit, p1_collect_emit, p1_matrix_emit, p1_load_emit, p1_scores_emit, p1_values_emit};
  emit[task->kind](task, iteration, tile);
}

static void p1_offset(unsigned command, unsigned operand, unsigned source, unsigned scale) {
  const gemmini_patch_t patch = {.command = command, .operand = operand, .width = 64, .source = source, .scale = scale, .relative = 1};
  gemmini_job_patch_at(GEMMINI0.control, &patch);
}

static int p1_capture(unsigned job, const p1_native_t *task) {
  const unsigned vectors = task->right ? 2 : 1;
  const unsigned chunks = (task->inner + P1_CHUNK - 1) / P1_CHUNK;
  const unsigned counts[] = {4, 1 + 2 * vectors, 4, 5 + 8 * chunks, 4, 6, 8};
  const unsigned patches[] = {2, vectors, 1, chunks + (BLOCK_N == 16), 3, 0, 1};
  if (gemmini_job_capture_at(GEMMINI0.control, job, counts[task->kind], patches[task->kind])) {
    return 1;
  }
  switch (task->kind) {
  case P1_COPY:
    p1_offset(2, 0, GEMMINI_VALUE_ITERATION, task->count);
    p1_offset(3, 0, GEMMINI_VALUE_ITERATION, task->count);
    break;
  case P1_GATHER:
    for (unsigned vector = 0; vector < vectors; ++vector) {
      p1_offset(1 + 2 * vector, 0, GEMMINI_VALUE_ITERATION, task->count * (task->packed ? 1 : sizeof(acc_t)));
    }
    break;
  case P1_COLLECT:
    p1_offset(3, 0, GEMMINI_VALUE_ITERATION, task->step);
    break;
  case P1_MATRIX:
    for (unsigned chunk = 0; chunk < chunks; ++chunk) {
      p1_offset(7 + 8 * chunk, BLOCK_N == 16 ? 1 : 0, GEMMINI_VALUE_ITERATION, BLOCK_N == 16 ? DIM : P1_ROWS * task->inner);
    }
    if (BLOCK_N == 16) {
      p1_offset(8, 0, GEMMINI_VALUE_ITERATION, DIM * sizeof(acc_t));
    }
    break;
  case P1_LOAD:
    for (unsigned vector = 0; vector < 3; ++vector) {
      p1_offset(1 + vector, 0, GEMMINI_VALUE_ITERATION, BLOCK_D);
    }
    break;
  case P1_SCORES:
    break;
  case P1_VALUES: {
    const gemmini_patch_t patch = {.command = 7, .width = 64, .source = GEMMINI_VALUE_ITERATION, .scale = BLOCK_D * sizeof(acc_t), .relative = 1};
    gemmini_job_patch_at(GEMMINI0.control, &patch);
    break;
  }
  }
  accel_commands(GEMMINI0, { p1_emit(task, 0, 0); });
  return gemmini_job_end_at(GEMMINI0.control);
}

#endif
