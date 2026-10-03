#include "accel_generated.h"
#include "auto_link.h"
#include "cgra_dma.h"
#include "cgra_link.h"
#include "gemmini.h"
#include "gemmini_job.h"
#include "generated/cgra_attention_exp_fast_api.h"
#include "generated/cgra_attention_max_fast_api.h"
#include "generated/cgra_attention_norm_fast_api.h"
#include "generated/mha_data.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum {
  TILES = MHA_N / MHA_TILE_ROWS,
  N_BLOCKS = MHA_N / DIM,
  D_BLOCKS = MHA_D / DIM,
  Q_ROW = 0,
  K_ROW = TILES * MHA_D,
  P_ROW = 0,
  V_ROW = MHA_N,
  A_ROW = K_ROW + MHA_N * D_BLOCKS,
  B_ROW = A_ROW + MHA_HIDDEN,
  SLOT_WORDS = MHA_N * MHA_TILE_ROWS,
  SLOT_BYTES = SLOT_WORDS * sizeof(acc_t),
  ROW_BYTES = DIM * sizeof(elem_t),
  INPUT_WORD = ATTENTION_MAX_ARG0,
  MAX_WORD = ATTENTION_MAX_ARG1,
  EXP_WORD = ATTENTION_EXP_ARG2,
  SUM_WORD = ATTENTION_EXP_ARG3,
  PROB_WORD = ATTENTION_NORM_ARG2,
  ACC_WRITE = UINT32_C(1) << (ADDR_LEN - 1),
  ACC_ADD = UINT32_C(3) << (ADDR_LEN - 2),
  ACC_READ = ACC_WRITE | (UINT32_C(1) << (ADDR_LEN - 3)),
  LOOP_SKIPS = (1 << 3) | (1 << 4) | (1 << 5) | (1 << 7),
};

static elem_t concat[MHA_N * MHA_HIDDEN] __attribute__((aligned(64)));
static elem_t output[MHA_N * MHA_HIDDEN] __attribute__((aligned(64)));

static const cgra_link_symbol_t max_symbols[ATTENTION_MAX_SYMBOL_COUNT] = {
    [ATTENTION_MAX_SYMBOL_INPUT0] = {INPUT_WORD, SLOT_WORDS, CGRA_LINK_SLOT},
    [ATTENTION_MAX_SYMBOL_OUTPUT] = {MAX_WORD, MHA_TILE_ROWS, CGRA_LINK_SLOT},
    [ATTENTION_MAX_SYMBOL_COLUMNS] = {MHA_N, 0, CGRA_LINK_SLOT},
    [ATTENTION_MAX_SYMBOL_ROW] = {0, 1, CGRA_LINK_ROW},
    [ATTENTION_MAX_SYMBOL_ELEMENTS] = {0, 0, CGRA_LINK_ELEMENTS},
};
static const cgra_link_symbol_t exp_symbols[ATTENTION_EXP_SYMBOL_COUNT] = {
    [ATTENTION_EXP_SYMBOL_INPUT0] = {INPUT_WORD, SLOT_WORDS, CGRA_LINK_SLOT},
    [ATTENTION_EXP_SYMBOL_INPUT1] = {MAX_WORD, MHA_TILE_ROWS, CGRA_LINK_SLOT},
    [ATTENTION_EXP_SYMBOL_OUTPUT] = {EXP_WORD, SLOT_WORDS, CGRA_LINK_SLOT},
    [ATTENTION_EXP_SYMBOL_OUTPUT1] = {SUM_WORD, MHA_TILE_ROWS, CGRA_LINK_SLOT},
    [ATTENTION_EXP_SYMBOL_COLUMNS] = {MHA_N, 0, CGRA_LINK_SLOT},
    [ATTENTION_EXP_SYMBOL_ROW] = {0, 1, CGRA_LINK_ROW},
    [ATTENTION_EXP_SYMBOL_ELEMENTS] = {0, 0, CGRA_LINK_ELEMENTS},
};
static const cgra_link_symbol_t norm_symbols[ATTENTION_NORM_SYMBOL_COUNT] = {
    [ATTENTION_NORM_SYMBOL_INPUT0] = {EXP_WORD, SLOT_WORDS, CGRA_LINK_SLOT},
    [ATTENTION_NORM_SYMBOL_INPUT1] = {SUM_WORD, MHA_TILE_ROWS, CGRA_LINK_SLOT},
    [ATTENTION_NORM_SYMBOL_OUTPUT] = {PROB_WORD, SLOT_WORDS, CGRA_LINK_SLOT},
    [ATTENTION_NORM_SYMBOL_COLUMNS] = {MHA_N, 0, CGRA_LINK_SLOT},
    [ATTENTION_NORM_SYMBOL_ELEMENTS] = {0, 0, CGRA_LINK_ELEMENTS},
};
static const cgra_link_job_t phases[] = {
    {AUTO_LINK_JOB_SOFTMAX, &ATTENTION_MAX, max_symbols},
    {AUTO_LINK_JOB_SOFTMAX + 1, &ATTENTION_EXP, exp_symbols},
    {AUTO_LINK_JOB_SOFTMAX + 2, &ATTENTION_NORM, norm_symbols},
};

static unsigned long cycles(void) {
  unsigned long value;
  __asm__ volatile("rdcycle %0" : "=r"(value)::"memory");
  return value;
}

static unsigned score_row(void) { return (GEMMINI0.spm_bytes - AUTO_LINK_GEMMINI0_BUFFER_SLOTS * SLOT_BYTES) / ROW_BYTES; }

static void init_inputs(void) {
  memset(concat, 0, sizeof(concat));
  memset(output, 0, sizeof(output));
  accel_commands(GEMMINI0, { gemmini_flush(0); });
  accel_commands(GEMMINI1, { gemmini_flush(0); });
  gemmini_fence();
}

// A is one row tile; B holds the current H x d projection weights.
static void projection_config(unsigned weight_stride, unsigned output_stride) {
  gemmini_config_ex(WEIGHT_STATIONARY, NO_ACTIVATION, 0);
  gemmini_extended3_config_ld(MHA_HIDDEN, MVIN_SCALE_IDENTITY, false, 0);
  gemmini_extended3_config_ld(weight_stride, MVIN_SCALE_IDENTITY, false, 1);
  gemmini_extended3_config_ld(0, MVIN_SCALE_IDENTITY, false, 2);
  gemmini_config_st(output_stride);
}

static void load_weights(const elem_t *weights, unsigned stride) {
  for (unsigned channel = 0; channel < MHA_HIDDEN; channel += DIM) {
    gemmini_extended_mvin2(weights + channel * stride, B_ROW + channel * D_BLOCKS, MHA_D, DIM);
  }
}

static void project_tile(const elem_t *input, const acc_t *bias) {
  for (unsigned channel = 0; channel < MHA_HIDDEN; channel += MHA_D) {
    gemmini_extended_mvin(input + channel, A_ROW + channel, MHA_D, MHA_TILE_ROWS);
  }
  for (unsigned column = 0; column < MHA_D; column += DIM) {
    gemmini_extended_mvin3(bias + column, ACC_WRITE + column, DIM, MHA_TILE_ROWS);
    for (unsigned channel = 0; channel < MHA_HIDDEN; channel += DIM) {
      gemmini_extended_preload(B_ROW + channel * D_BLOCKS + column, ACC_ADD + column, DIM, DIM, DIM, MHA_TILE_ROWS);
      gemmini_extended_compute_preloaded(A_ROW + channel, GARBAGE_ADDR, DIM, MHA_TILE_ROWS, DIM, MHA_TILE_ROWS);
    }
  }
}

static void project_local(const elem_t *weights, const acc_t *bias, unsigned destination) {
  projection_config(MHA_D, MHA_D);
  load_weights(weights, MHA_D);
  for (unsigned tile = 0; tile < TILES; ++tile) {
    project_tile(mha_x + tile * MHA_TILE_ROWS * MHA_HIDDEN, bias);
    for (unsigned column = 0; column < MHA_D; column += DIM) {
      gemmini_extended_mvout_spad(destination + tile * MHA_D + column, 1, ACC_WRITE + column, DIM, MHA_TILE_ROWS);
    }
  }
}

static void project_head(unsigned head) {
  const unsigned weight = head * MHA_HIDDEN * MHA_D;
  const unsigned bias = head * MHA_D;
  accel_commands(GEMMINI0, {
    project_local(mha_wq + weight, mha_bq + bias, Q_ROW);
    project_local(mha_wk + weight, mha_bk + bias, K_ROW);
  });
  accel_commands(GEMMINI1, { project_local(mha_wv + weight, mha_bv + bias, V_ROW); });
  gemmini_fence();
}

static void scores(void) {
  gemmini_extended_config_ex(WEIGHT_STATIONARY, NO_ACTIVATION, 0, 1, false, true);
  gemmini_loop_ws_spad(1, N_BLOCKS, D_BLOCKS, 0, 0, 0, Q_ROW, K_ROW + MHA_N * D_BLOCKS, 0, 0, false, true, true, false, false, NO_ACTIVATION, 0, 0, false, LOOP_SKIPS);
  for (unsigned block = 0; block < N_BLOCKS; ++block) {
    gemmini_extended_mvout_spad(score_row() + block * sizeof(acc_t), MHA_N * sizeof(acc_t) / ROW_BYTES, ACC_READ + block * DIM, DIM, MHA_TILE_ROWS);
  }
}

static void values(unsigned head) {
  gemmini_config_ex(WEIGHT_STATIONARY, NO_ACTIVATION, 0);
  for (unsigned block = 0; block < MHA_N / MHA_D; ++block) {
    gemmini_extended_mvin((const void *)(CGRA0.spm + PROB_WORD + block * MHA_D), P_ROW + block * MHA_D, MHA_D, MHA_TILE_ROWS);
  }
  gemmini_loop_ws_spad(1, D_BLOCKS, N_BLOCKS, 0, 0, 0, P_ROW, V_ROW + MHA_N * D_BLOCKS, 0, 0, false, false, false, false, false, NO_ACTIVATION, 0, 0, false, LOOP_SKIPS);
  for (unsigned block = 0; block < D_BLOCKS; ++block) {
    gemmini_extended_mvout(concat + head * MHA_D + block * DIM, ACC_WRITE + block * DIM, DIM, MHA_TILE_ROWS);
  }
}

static void patch_gemmini(accel_t device, unsigned command, unsigned operand, unsigned lsb, unsigned width, unsigned source, uint32_t scale, int32_t offset) {
  const gemmini_patch_t patch = {.command = command, .operand = operand, .lsb = lsb, .width = width, .source = source, .scale = scale, .offset = offset};
  gemmini_job_patch_at(device.control, &patch);
}

static int configure_scores(void) {
  if (gemmini_job_capture_at(GEMMINI0.control, AUTO_LINK_JOB_SCORES, 4 + N_BLOCKS, 2 + 2 * N_BLOCKS) != 0) {
    return 1;
  }
  patch_gemmini(GEMMINI0, 1, 0, 0, 16, GEMMINI_VALUE_ROWS, UINT32_MAX, DIM);
  patch_gemmini(GEMMINI0, 2, 0, 0, ADDR_LEN, GEMMINI_VALUE_TILE_ID, MHA_D, Q_ROW);
  for (unsigned block = 0; block < N_BLOCKS; ++block) {
    patch_gemmini(GEMMINI0, 4 + block, 0, 0, ADDR_LEN, GEMMINI_VALUE_SLOT, SLOT_BYTES / ROW_BYTES, score_row() + block * sizeof(acc_t));
    patch_gemmini(GEMMINI0, 4 + block, 1, ADDR_LEN + 16, 16, GEMMINI_VALUE_ROWS, 1, 0);
  }
  accel_commands(GEMMINI0, { scores(); });
  return gemmini_job_end_at(GEMMINI0.control);
}

static int configure_values(unsigned head) {
  const unsigned loads = MHA_N / MHA_D;
  gemmini_job_write_at(GEMMINI1.control, GEMMINI_JOB_WINDOW_PIXEL_BYTES, MHA_N);
  if (gemmini_job_capture_at(GEMMINI1.control, AUTO_LINK_JOB_VALUES, 4 + loads + D_BLOCKS, 1 + 2 * loads + 2 * D_BLOCKS) != 0) {
    return 1;
  }
  for (unsigned block = 0; block < loads; ++block) {
    patch_gemmini(GEMMINI1, 1 + block, 0, 0, 64, GEMMINI_VALUE_INPUT_ADDRESS, 1, block * MHA_D);
    patch_gemmini(GEMMINI1, 1 + block, 1, ADDR_LEN + 16, 16, GEMMINI_VALUE_ROWS, 1, 0);
  }
  patch_gemmini(GEMMINI1, 1 + loads, 0, 0, 16, GEMMINI_VALUE_ROWS, UINT32_MAX, DIM);
  for (unsigned block = 0; block < D_BLOCKS; ++block) {
    // Patch the low address word; this test's DRAM allocation stays within one 4 GiB region.
    patch_gemmini(GEMMINI1, 4 + loads + block, 0, 0, 32, GEMMINI_VALUE_TILE_ID, MHA_TILE_ROWS * MHA_HIDDEN, (uintptr_t)(concat + head * MHA_D + block * DIM));
    patch_gemmini(GEMMINI1, 4 + loads + block, 1, ADDR_LEN + 16, 16, GEMMINI_VALUE_ROWS, 1, 0);
  }
  accel_commands(GEMMINI1, { values(head); });
  return gemmini_job_end_at(GEMMINI1.control);
}

static int run_head(unsigned head) {
  // Projections change native DMA strides; restore the auto jobs' load/store state.
  accel_commands(GEMMINI0, { gemmini_config_st(MHA_N * sizeof(acc_t)); });
  accel_commands(GEMMINI1, {
    gemmini_config_ld(MHA_N);
    gemmini_config_st(MHA_HIDDEN);
  });
  gemmini_fence();
  if (configure_values(head) != 0) {
    return 1;
  }
  auto_link_input_ready();
  unsigned seen = 0;
  int failures = 0;
  for (unsigned index = 0; index < 3; ++index) {
    const cgra_link_result_t result = cgra_link_wait_at(CGRA0.control);
    const unsigned bit = result.stage < 3 ? 1u << result.stage : 0;
    if (bit == 0 || (seen & bit) || result.job != 0 || result.status != AUTO_LINK_STATUS_SUCCESS || result.detail != 0 || result.data != 0) {
      printf("MHA head=%u stage=%u job=%u status=%u detail=%u data=%u\n", head, result.stage, result.job, result.status, result.detail, result.data);
      ++failures;
    }
    seen |= bit;
  }
  while (*(volatile uint32_t *)(AUTO_LINK_BASE + AUTO_LINK_RUNNING)) {
  }
  return failures + (seen != 7);
}

static void project_output(void) {
  accel_commands(GEMMINI0, {
    projection_config(MHA_HIDDEN, MHA_HIDDEN);
    for (unsigned column = 0; column < MHA_HIDDEN; column += MHA_D) {
      load_weights(mha_wo + column, MHA_HIDDEN);
      for (unsigned tile = 0; tile < TILES; ++tile) {
        const unsigned offset = tile * MHA_TILE_ROWS * MHA_HIDDEN;
        project_tile(concat + offset, mha_bo + column);
        for (unsigned block = 0; block < D_BLOCKS; ++block) {
          gemmini_extended_mvout(output + offset + column + block * DIM, ACC_WRITE + block * DIM, DIM, MHA_TILE_ROWS);
        }
      }
    }
  });
  gemmini_fence();
}

static int verify_output(const char *name, const elem_t *actual, const elem_t *expected) {
  int failures = 0;
  cgra_dma_memory_fence();
  for (unsigned index = 0; index < MHA_N * MHA_HIDDEN; ++index) {
    if (actual[index] != expected[index]) {
      if (failures < 8) {
        printf("MHA %s element=%u actual=%d expected=%d\n", name, index, (int)actual[index], (int)expected[index]);
      }
      ++failures;
    }
  }
  printf("MHA %s checked=%u mismatches=%d\n", name, MHA_N * MHA_HIDDEN, failures);
  return failures;
}

int main(void) {
  unsigned long begin = cycles();
  init_inputs();
  int failures = configure_scores();
  accel_commands(CGRA0, { failures += cgra_job_sequence_at(CGRA0.control, phases, sizeof(phases) / sizeof(phases[0])); });
  if (failures) {
    printf("MHA Hybrid: FAIL (configuration)\n");
    return 1;
  }
  auto_link_transfer(AUTO_LINK_COPY_SCORES_SOFTMAX, score_row() * ROW_BYTES, INPUT_WORD * sizeof(acc_t), SLOT_BYTES, SLOT_BYTES, MHA_N * sizeof(acc_t));
  auto_link_transfer(AUTO_LINK_COPY_SOFTMAX_VALUES, PROB_WORD, 0, SLOT_WORDS, 0, MHA_N);
  auto_link_tiles(MHA_N, 1, MHA_TILE_ROWS, 1);
  const unsigned long setup_cycles = cycles() - begin;
  printf("MHA Hybrid ready N=%u heads=%u hidden=%u d=%u tile_rows=%u\n", MHA_N, MHA_HEADS, MHA_HIDDEN, MHA_D, MHA_TILE_ROWS);
  begin = cycles();
  for (unsigned head = 0; head < MHA_HEADS; ++head) {
    const unsigned long start = cycles();
    project_head(head);
    const unsigned long projected = cycles();
    if (run_head(head) != 0) {
      printf("MHA Hybrid: FAIL (head=%u)\n", head);
      return 1;
    }
    printf("MHA head=%u projection_cycles=%lu attention_cycles=%lu\n", head, projected - start, cycles() - projected);
  }
  const unsigned long heads_done = cycles();
  project_output();
  const unsigned long finished = cycles();
  failures = verify_output("concat", concat, mha_concat_expected);
  failures += verify_output("output", output, mha_expected);
  printf("MHA Hybrid setup_cycles=%lu heads_cycles=%lu output_cycles=%lu execution_cycles=%lu\n", setup_cycles, heads_done - begin, finished - heads_done, finished - begin);
  printf("MHA Hybrid: %s (%d mismatches)\n", failures == 0 ? "PASS" : "FAIL", failures);
  return failures != 0;
}
