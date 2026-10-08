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
#include "generated/mha_g0.h"
#include "generated/mha_g1.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
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
  ACC_READ = ACC_WRITE | (UINT32_C(1) << (ADDR_LEN - 3)),
  LOOP_SKIPS = (1 << 3) | (1 << 4) | (1 << 5) | (1 << 7),
  Q_WEIGHTS = 1,
  Q_ROWS = 2,
  K_WEIGHTS = 3,
  K_ROWS = 4,
  WO_WEIGHTS = 5,
  OUTPUT_ROWS = 6,
  V_WEIGHTS = 1,
  V_ROWS = 2,
  RUNS = 9,
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
  }
  gemmini_loop_ws_spad(1, D_BLOCKS, MHA_HIDDEN / DIM, 0, 0, 0, A_ROW, B_ROW + MHA_HIDDEN * D_BLOCKS, 0, 0, false, false, false, false, true, NO_ACTIVATION, 0, 0, false, LOOP_SKIPS);
}

static void store_tile(uintptr_t destination, int local) {
  for (unsigned column = 0; column < MHA_D; column += DIM) {
    if (local) {
      gemmini_extended_mvout_spad(destination + column, 1, ACC_WRITE + column, DIM, MHA_TILE_ROWS);
    } else {
      gemmini_extended_mvout((void *)(destination + column), ACC_WRITE + column, DIM, MHA_TILE_ROWS);
    }
  }
}

static void project_local(const elem_t *weights, const acc_t *bias, unsigned destination) {
  projection_config(MHA_D, MHA_D);
  load_weights(weights, MHA_D);
  for (unsigned tile = 0; tile < TILES; ++tile) {
    project_tile(mha_x + tile * MHA_TILE_ROWS * MHA_HIDDEN, bias);
    store_tile(destination + tile * MHA_D, 1);
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
  gemmini_config_st(MHA_N * sizeof(acc_t));
  gemmini_extended_config_ex(WEIGHT_STATIONARY, NO_ACTIVATION, 0, 1, false, true);
  gemmini_loop_ws_spad(1, N_BLOCKS, D_BLOCKS, 0, 0, 0, Q_ROW, K_ROW + MHA_N * D_BLOCKS, 0, 0, false, true, true, false, false, NO_ACTIVATION, 0, 0, false, LOOP_SKIPS);
  for (unsigned block = 0; block < N_BLOCKS; ++block) {
    gemmini_extended_mvout_spad(score_row() + block * sizeof(acc_t), MHA_N * sizeof(acc_t) / ROW_BYTES, ACC_READ + block * DIM, DIM, MHA_TILE_ROWS);
  }
}

static void values(unsigned head) {
  gemmini_config_ld(MHA_N);
  gemmini_config_st(MHA_HIDDEN);
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

static void patch_address(accel_t device, unsigned command, unsigned width, uint32_t tile_stride, uint32_t iteration_stride) {
  const gemmini_patch_t patch = {.command = command,
                                 .width = width,
                                 .source = tile_stride ? GEMMINI_VALUE_TILE_ID : GEMMINI_VALUE_ITERATION,
                                 .scale = tile_stride ? tile_stride : iteration_stride,
                                 .source2 = GEMMINI_VALUE_ITERATION,
                                 .scale2 = tile_stride ? iteration_stride : 0,
                                 .relative = 1};
  gemmini_job_patch_at(device.control, &patch);
}

static int configure_scores(void) {
  if (gemmini_job_capture_at(GEMMINI0.control, AUTO_LINK_JOB_SCORES, 5 + N_BLOCKS, 2 + 2 * N_BLOCKS) != 0) {
    return 1;
  }
  patch_gemmini(GEMMINI0, 2, 0, 0, 16, GEMMINI_VALUE_ROWS, UINT32_MAX, DIM);
  patch_gemmini(GEMMINI0, 3, 0, 0, ADDR_LEN, GEMMINI_VALUE_TILE_ID, MHA_D, Q_ROW);
  for (unsigned block = 0; block < N_BLOCKS; ++block) {
    patch_gemmini(GEMMINI0, 5 + block, 0, 0, ADDR_LEN, GEMMINI_VALUE_SLOT, SLOT_BYTES / ROW_BYTES, score_row() + block * sizeof(acc_t));
    patch_gemmini(GEMMINI0, 5 + block, 1, ADDR_LEN + 16, 16, GEMMINI_VALUE_ROWS, 1, 0);
  }
  accel_commands(GEMMINI0, { scores(); });
  return gemmini_job_end_at(GEMMINI0.control);
}

static int configure_values(unsigned head) {
  const unsigned loads = MHA_N / MHA_D;
  gemmini_job_write_at(GEMMINI1.control, GEMMINI_JOB_WINDOW_PIXEL_BYTES, MHA_N);
  if (gemmini_job_capture_at(GEMMINI1.control, AUTO_LINK_JOB_VALUES, 6 + loads + D_BLOCKS, 1 + 2 * loads + 2 * D_BLOCKS) != 0) {
    return 1;
  }
  for (unsigned block = 0; block < loads; ++block) {
    patch_gemmini(GEMMINI1, 3 + block, 0, 0, 64, GEMMINI_VALUE_INPUT_ADDRESS, 1, block * MHA_D);
    patch_gemmini(GEMMINI1, 3 + block, 1, ADDR_LEN + 16, 16, GEMMINI_VALUE_ROWS, 1, 0);
  }
  patch_gemmini(GEMMINI1, 3 + loads, 0, 0, 16, GEMMINI_VALUE_ROWS, UINT32_MAX, DIM);
  for (unsigned block = 0; block < D_BLOCKS; ++block) {
    patch_address(GEMMINI1, 6 + loads + block, 64, MHA_TILE_ROWS * MHA_HIDDEN, MHA_D);
    patch_gemmini(GEMMINI1, 6 + loads + block, 1, ADDR_LEN + 16, 16, GEMMINI_VALUE_ROWS, 1, 0);
  }
  accel_commands(GEMMINI1, { values(head); });
  return gemmini_job_end_at(GEMMINI1.control);
}

static int run_head(unsigned head) {
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
        store_tile((uintptr_t)(output + offset + column), 0);
      }
    }
  });
  gemmini_fence();
}

static int configure_weights(accel_t device, unsigned job, const elem_t *weights, unsigned stride, unsigned iteration_stride, unsigned output_stride) {
  const unsigned loads = MHA_HIDDEN / DIM;
  if (gemmini_job_capture_at(device.control, job, 5 + loads, loads) != 0) {
    return 1;
  }
  for (unsigned index = 0; index < loads; ++index) {
    patch_address(device, 5 + index, 64, 0, iteration_stride);
  }
  accel_commands(device, {
    projection_config(stride, output_stride);
    load_weights(weights, stride);
  });
  return gemmini_job_end_at(device.control);
}

static int configure_rows(accel_t device, unsigned job, const elem_t *input, const acc_t *bias, uintptr_t destination, int local) {
  const unsigned loads = MHA_HIDDEN / MHA_D;
  const unsigned stores = loads + D_BLOCKS + 3;
  if (gemmini_job_capture_at(device.control, job, stores + D_BLOCKS, loads + 2 * D_BLOCKS) != 0) {
    return 1;
  }
  for (unsigned index = 0; index < loads; ++index) {
    patch_address(device, index, 64, MHA_TILE_ROWS * MHA_HIDDEN, 0);
  }
  for (unsigned index = 0; index < D_BLOCKS; ++index) {
    patch_address(device, loads + index, 64, 0, MHA_D * sizeof(acc_t));
    patch_address(device, stores + index, local ? ADDR_LEN : 64, local ? MHA_D : MHA_TILE_ROWS * MHA_HIDDEN, local ? 0 : MHA_D);
  }
  accel_commands(device, {
    project_tile(input, bias);
    store_tile(destination, local);
  });
  return gemmini_job_end_at(device.control);
}

static int configure_projections(void) {
  int failures = configure_weights(GEMMINI0, Q_WEIGHTS, mha_wq, MHA_D, MHA_HIDDEN * MHA_D, MHA_D);
  failures += configure_rows(GEMMINI0, Q_ROWS, mha_x, mha_bq, Q_ROW, 1);
  failures += configure_weights(GEMMINI0, K_WEIGHTS, mha_wk, MHA_D, MHA_HIDDEN * MHA_D, MHA_D);
  failures += configure_rows(GEMMINI0, K_ROWS, mha_x, mha_bk, K_ROW, 1);
  failures += configure_weights(GEMMINI1, V_WEIGHTS, mha_wv, MHA_D, MHA_HIDDEN * MHA_D, MHA_D);
  failures += configure_rows(GEMMINI1, V_ROWS, mha_x, mha_bv, V_ROW, 1);
  failures += configure_weights(GEMMINI0, WO_WEIGHTS, mha_wo, MHA_HIDDEN, MHA_D, MHA_HIDDEN);
  failures += configure_rows(GEMMINI0, OUTPUT_ROWS, concat, mha_bo, (uintptr_t)output, 0);
  return failures;
}

static void configure_attention(void) {
  auto_link_load(&AUTO_LINK_GRAPH, NULL);
  auto_link_transfer(AUTO_LINK_COPY_SCORES_SOFTMAX, score_row() * ROW_BYTES, INPUT_WORD * sizeof(acc_t), SLOT_BYTES, SLOT_BYTES, MHA_N * sizeof(acc_t));
  auto_link_transfer(AUTO_LINK_COPY_SOFTMAX_VALUES, PROB_WORD, 0, SLOT_WORDS, 0, MHA_N);
  auto_link_tiles(MHA_N, 1, MHA_TILE_ROWS, 1);
}

static void configure_profile(unsigned id, unsigned job, unsigned rows, unsigned tile_rows) {
  auto_link_job(0, job);
  auto_link_tiles(rows, 1, tile_rows, 1);
  auto_link_run_config(id);
}

static void configure_runs(void) {
  auto_link_load(&MHA_G0_GRAPH, NULL);
  configure_profile(0, Q_WEIGHTS, 1, 1);
  configure_profile(1, Q_ROWS, MHA_N, MHA_TILE_ROWS);
  configure_profile(2, K_WEIGHTS, 1, 1);
  configure_profile(3, K_ROWS, MHA_N, MHA_TILE_ROWS);
  auto_link_load(&MHA_G1_GRAPH, NULL);
  configure_profile(4, V_WEIGHTS, 1, 1);
  configure_profile(5, V_ROWS, MHA_N, MHA_TILE_ROWS);
  configure_attention();
  auto_link_run_repeat(6, 0, MHA_HEADS);
  auto_link_load(&MHA_G0_GRAPH, NULL);
  configure_profile(7, WO_WEIGHTS, 1, 1);
  auto_link_job(0, OUTPUT_ROWS);
  auto_link_tiles(MHA_N, 1, MHA_TILE_ROWS, 1);
  auto_link_run_repeat(8, 7, MHA_HIDDEN / MHA_D);
}

static int collect_results(void) {
  const unsigned expected = 9 * MHA_HEADS + 2 * (MHA_HIDDEN / MHA_D);
  unsigned count = 0;
  int failures = 0;
  // A failed sequence stops early, so consume only reports that actually arrived.
  while (cgra_link_read_at(CGRA0.control, CGRA_LINK_CONTROL_RESULT_VALID)) {
    const cgra_link_result_t result = cgra_link_wait_at(CGRA0.control);
    if (result.status != AUTO_LINK_STATUS_SUCCESS || result.detail != 0 || result.data != 0) {
      printf("MHA result=%u stage=%u job=%u status=%u detail=%u data=%u\n", count, result.stage, result.job, result.status, result.detail, result.data);
      ++failures;
    }
    ++count;
  }
  printf("MHA Auto reports=%u expected=%u\n", count, expected);
  return failures + (count != expected);
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

int main(int argc, char **argv) {
  const int automatic = argc > 1 && atoi(argv[1]) != 0;
  const char *mode = automatic ? "Auto" : "Hybrid";
  unsigned long begin = cycles();
  init_inputs();
  int failures = configure_scores();
  accel_commands(CGRA0, { failures += cgra_job_sequence_at(CGRA0.control, phases, sizeof(phases) / sizeof(phases[0])); });
  if (automatic) {
    failures += configure_projections();
    failures += configure_values(0);
    configure_runs();
  } else {
    configure_attention();
  }
  if (failures) {
    printf("MHA %s: FAIL (configuration)\n", mode);
    return 1;
  }
  const unsigned long setup_cycles = cycles() - begin;
  printf("MHA %s ready N=%u heads=%u hidden=%u d=%u tile_rows=%u\n", mode, MHA_N, MHA_HEADS, MHA_HIDDEN, MHA_D, MHA_TILE_ROWS);
  begin = cycles();
  if (automatic) {
    auto_link_run(0, RUNS);
    while (*(volatile uint32_t *)(AUTO_LINK_BASE + AUTO_LINK_RUNNING)) {
    }
  } else {
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
    project_output();
  }
  const unsigned long finished = cycles();
  if (automatic) {
    failures += collect_results();
  }
  failures += verify_output("concat", concat, mha_concat_expected);
  failures += verify_output("output", output, mha_expected);
  printf("MHA %s setup_cycles=%lu execution_cycles=%lu\n", mode, setup_cycles, finished - begin);
  printf("MHA %s: %s (%d failures)\n", mode, failures == 0 ? "PASS" : "FAIL", failures);
  return failures != 0;
}
