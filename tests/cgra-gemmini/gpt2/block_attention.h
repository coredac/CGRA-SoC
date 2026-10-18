#ifndef BLOCK_ATTENTION_H
#define BLOCK_ATTENTION_H

#include "accel_generated.h"
#include "auto_link.h"
#include "cgra_dma.h"
#include "cgra_link.h"
#include "gemmini.h"
#include "gemmini_job.h"
#include "generated/block_load.h"
#include "generated/cgra_attention_exp_fast_api.h"
#include "generated/cgra_attention_max_fast_api.h"
#include "generated/cgra_attention_norm_fast_api.h"
#include "generated/gpt2_data.h"

#include <stdio.h>
#include <string.h>

enum {
  ATTN_ROWS = 16,
  ATTN_TILES = BLOCK_N / ATTN_ROWS,
  ATTN_N_BLOCKS = BLOCK_N / DIM,
  ATTN_D_BLOCKS = BLOCK_D / DIM,
  ATTN_P_COLS = BLOCK_N < BLOCK_D ? BLOCK_N : BLOCK_D,
  ATTN_P_LOADS = BLOCK_N / ATTN_P_COLS,
  ATTN_Q_ROW = 0,
  ATTN_K_ROW = ATTN_TILES * BLOCK_D,
  ATTN_P_ROW = 0,
  ATTN_V_ROW = BLOCK_N,
  ATTN_SLOT_WORDS = BLOCK_N * ATTN_ROWS,
  ATTN_SLOT_BYTES = ATTN_SLOT_WORDS * sizeof(acc_t),
  ATTN_ROW_BYTES = DIM * sizeof(elem_t),
  ATTN_ACC_READ = (UINT32_C(1) << (ADDR_LEN - 1)) | (UINT32_C(1) << (ADDR_LEN - 3)),
  ATTN_LOOP_SKIPS = (1 << 3) | (1 << 4) | (1 << 5) | (1 << 7),
  ATTN_LOAD_JOB = 1,
};

// Setup includes per-head capture in Hybrid; execution includes loads and waits.
// Total spans the entire call, including result collection and timing overhead.
static struct {
  unsigned long setup, execute, total;
} attn_stats;

static unsigned long attn_clock(void) {
  unsigned long value;
  __asm__ volatile("rdcycle %0" : "=r"(value)::"memory");
  return value;
}

static const cgra_link_symbol_t attn_max_symbols[ATTENTION_MAX_SYMBOL_COUNT] = {
    [ATTENTION_MAX_SYMBOL_INPUT0] = {ATTENTION_MAX_ARG0, ATTN_SLOT_WORDS, CGRA_LINK_SLOT},
    [ATTENTION_MAX_SYMBOL_OUTPUT] = {ATTENTION_MAX_ARG1, ATTN_ROWS, CGRA_LINK_SLOT},
    [ATTENTION_MAX_SYMBOL_COLUMNS] = {BLOCK_N, 0, CGRA_LINK_SLOT},
    [ATTENTION_MAX_SYMBOL_ROW] = {0, 1, CGRA_LINK_ROW},
    [ATTENTION_MAX_SYMBOL_ELEMENTS] = {0, 0, CGRA_LINK_ELEMENTS},
};
static const cgra_link_symbol_t attn_exp_symbols[ATTENTION_EXP_SYMBOL_COUNT] = {
    [ATTENTION_EXP_SYMBOL_INPUT0] = {ATTENTION_MAX_ARG0, ATTN_SLOT_WORDS, CGRA_LINK_SLOT},
    [ATTENTION_EXP_SYMBOL_INPUT1] = {ATTENTION_MAX_ARG1, ATTN_ROWS, CGRA_LINK_SLOT},
    [ATTENTION_EXP_SYMBOL_OUTPUT] = {ATTENTION_EXP_ARG2, ATTN_SLOT_WORDS, CGRA_LINK_SLOT},
    [ATTENTION_EXP_SYMBOL_OUTPUT1] = {ATTENTION_EXP_ARG3, ATTN_ROWS, CGRA_LINK_SLOT},
    [ATTENTION_EXP_SYMBOL_COLUMNS] = {BLOCK_N, 0, CGRA_LINK_SLOT},
    [ATTENTION_EXP_SYMBOL_ROW] = {0, 1, CGRA_LINK_ROW},
    [ATTENTION_EXP_SYMBOL_ELEMENTS] = {0, 0, CGRA_LINK_ELEMENTS},
};
static const cgra_link_symbol_t attn_norm_symbols[ATTENTION_NORM_SYMBOL_COUNT] = {
    [ATTENTION_NORM_SYMBOL_INPUT0] = {ATTENTION_EXP_ARG2, ATTN_SLOT_WORDS, CGRA_LINK_SLOT},
    [ATTENTION_NORM_SYMBOL_INPUT1] = {ATTENTION_EXP_ARG3, ATTN_ROWS, CGRA_LINK_SLOT},
    [ATTENTION_NORM_SYMBOL_OUTPUT] = {ATTENTION_NORM_ARG2, ATTN_SLOT_WORDS, CGRA_LINK_SLOT},
    [ATTENTION_NORM_SYMBOL_COLUMNS] = {BLOCK_N, 0, CGRA_LINK_SLOT},
    [ATTENTION_NORM_SYMBOL_ELEMENTS] = {0, 0, CGRA_LINK_ELEMENTS},
};
static const cgra_link_job_t attn_phases[] = {
    {AUTO_LINK_JOB_SOFTMAX, &ATTENTION_MAX, attn_max_symbols},
    {AUTO_LINK_JOB_SOFTMAX + 1, &ATTENTION_EXP, attn_exp_symbols},
    {AUTO_LINK_JOB_SOFTMAX + 2, &ATTENTION_NORM, attn_norm_symbols},
};

static unsigned attn_score_row(void) { return (GEMMINI0.spm_bytes - AUTO_LINK_GEMMINI0_BUFFER_SLOTS * ATTN_SLOT_BYTES) / ATTN_ROW_BYTES; }

static void attn_load_qk(const elem_t *q, const elem_t *k, unsigned head) {
  const unsigned offset = head * BLOCK_D;
  gemmini_config_ld(BLOCK_HIDDEN * sizeof(elem_t));
  gemmini_config_st(BLOCK_N * sizeof(acc_t));
  for (unsigned tile = 0; tile < ATTN_TILES; ++tile) {
    gemmini_extended_mvin(q + tile * ATTN_ROWS * BLOCK_HIDDEN + offset, ATTN_Q_ROW + tile * BLOCK_D, BLOCK_D, ATTN_ROWS);
  }
  for (unsigned block = 0; block < ATTN_N_BLOCKS; ++block) {
    gemmini_extended_mvin(k + block * DIM * BLOCK_HIDDEN + offset, ATTN_K_ROW + block * BLOCK_D, BLOCK_D, DIM);
  }
}

static void attn_load_v(const elem_t *v, unsigned head) {
  const unsigned offset = head * BLOCK_D;
  gemmini_config_ld(BLOCK_HIDDEN * sizeof(elem_t));
  gemmini_config_st(BLOCK_HIDDEN * sizeof(acc_t));
  for (unsigned block = 0; block < ATTN_N_BLOCKS; ++block) {
    gemmini_extended_mvin(v + block * DIM * BLOCK_HIDDEN + offset, ATTN_V_ROW + block * BLOCK_D, BLOCK_D, DIM);
  }
  gemmini_config_ld(BLOCK_N);
}

static void attn_load(const elem_t *q, const elem_t *k, const elem_t *v, unsigned head) {
  accel_commands(GEMMINI0, { attn_load_qk(q, k, head); });
  accel_commands(GEMMINI1, { attn_load_v(v, head); });
  gemmini_fence();
}

static void attn_scores(unsigned tile, unsigned slot) {
  gemmini_extended_config_ex(WEIGHT_STATIONARY, NO_ACTIVATION, 0, 1, false, true);
  gemmini_loop_ws_spad(1, ATTN_N_BLOCKS, ATTN_D_BLOCKS, 0, 0, 0, ATTN_Q_ROW + tile * BLOCK_D, ATTN_K_ROW + BLOCK_N * ATTN_D_BLOCKS, 0, 0, false, true, true, false, false, NO_ACTIVATION, 0, 0, false,
                       ATTN_LOOP_SKIPS);
  for (unsigned block = 0; block < ATTN_N_BLOCKS; ++block) {
    gemmini_extended_mvout_spad(attn_score_row() + slot * ATTN_SLOT_BYTES / ATTN_ROW_BYTES + block * sizeof(acc_t), BLOCK_N * sizeof(acc_t) / ATTN_ROW_BYTES, ATTN_ACC_READ + block * DIM, DIM,
                                ATTN_ROWS);
  }
}

static void attn_values(acc_t *output, unsigned head, unsigned tile, unsigned slot) {
  gemmini_config_ex(WEIGHT_STATIONARY, NO_ACTIVATION, 0);
  for (unsigned block = 0; block < ATTN_P_LOADS; ++block) {
    gemmini_extended_mvin((const void *)(CGRA0.spm + ATTENTION_NORM_ARG2 + slot * ATTN_SLOT_WORDS + block * ATTN_P_COLS), ATTN_P_ROW + block * ATTN_P_COLS, ATTN_P_COLS, ATTN_ROWS);
  }
  gemmini_loop_ws_spad(1, ATTN_D_BLOCKS, ATTN_N_BLOCKS, 0, 0, 0, ATTN_P_ROW, ATTN_V_ROW + BLOCK_N * ATTN_D_BLOCKS, 0, 0, false, false, true, false, false, NO_ACTIVATION, 0, 0, false, ATTN_LOOP_SKIPS);
  for (unsigned block = 0; block < ATTN_D_BLOCKS; ++block) {
    gemmini_extended_mvout(output + tile * ATTN_ROWS * BLOCK_HIDDEN + head * BLOCK_D + block * DIM, ATTN_ACC_READ + block * DIM, DIM, ATTN_ROWS);
  }
}

static void attn_patch(accel_t device, unsigned command, unsigned operand, unsigned lsb, unsigned width, unsigned source, uint32_t scale, int32_t offset) {
  const gemmini_patch_t patch = {.command = command, .operand = operand, .lsb = lsb, .width = width, .source = source, .scale = scale, .offset = offset};
  gemmini_job_patch_at(device.control, &patch);
}

static int attn_config_scores(void) {
  if (gemmini_job_capture_at(GEMMINI0.control, AUTO_LINK_JOB_SCORES, 4 + ATTN_N_BLOCKS, 2 + 2 * ATTN_N_BLOCKS) != 0) {
    return 1;
  }
  attn_patch(GEMMINI0, 1, 0, 0, 16, GEMMINI_VALUE_ROWS, UINT32_MAX, DIM);
  attn_patch(GEMMINI0, 2, 0, 0, ADDR_LEN, GEMMINI_VALUE_TILE_ID, BLOCK_D, ATTN_Q_ROW);
  for (unsigned block = 0; block < ATTN_N_BLOCKS; ++block) {
    attn_patch(GEMMINI0, 4 + block, 0, 0, ADDR_LEN, GEMMINI_VALUE_SLOT, ATTN_SLOT_BYTES / ATTN_ROW_BYTES, attn_score_row() + block * sizeof(acc_t));
    attn_patch(GEMMINI0, 4 + block, 1, ADDR_LEN + 16, 16, GEMMINI_VALUE_ROWS, 1, 0);
  }
  accel_commands(GEMMINI0, { attn_scores(0, 0); });
  return gemmini_job_end_at(GEMMINI0.control);
}

static int attn_config_values(acc_t *output, unsigned head, int across_heads) {
  const unsigned loads = ATTN_P_LOADS;
  gemmini_job_write_at(GEMMINI1.control, GEMMINI_JOB_WINDOW_PIXEL_BYTES, BLOCK_N);
  if (gemmini_job_capture_at(GEMMINI1.control, AUTO_LINK_JOB_VALUES, 4 + loads + ATTN_D_BLOCKS, 1 + 2 * loads + 2 * ATTN_D_BLOCKS) != 0) {
    return 1;
  }
  for (unsigned block = 0; block < loads; ++block) {
    attn_patch(GEMMINI1, 1 + block, 0, 0, 64, GEMMINI_VALUE_INPUT_ADDRESS, 1, block * ATTN_P_COLS);
    attn_patch(GEMMINI1, 1 + block, 1, ADDR_LEN + 16, 16, GEMMINI_VALUE_ROWS, 1, 0);
  }
  attn_patch(GEMMINI1, 1 + loads, 0, 0, 16, GEMMINI_VALUE_ROWS, UINT32_MAX, DIM);
  for (unsigned block = 0; block < ATTN_D_BLOCKS; ++block) {
    const gemmini_patch_t address = {.command = 4 + loads + block,
                                     .width = 64,
                                     .source = GEMMINI_VALUE_TILE_ID,
                                     .scale = ATTN_ROWS * BLOCK_HIDDEN * sizeof(acc_t),
                                     .source2 = GEMMINI_VALUE_ITERATION,
                                     .scale2 = across_heads ? BLOCK_D * sizeof(acc_t) : 0,
                                     .relative = 1};
    gemmini_job_patch_at(GEMMINI1.control, &address);
    attn_patch(GEMMINI1, 4 + loads + block, 1, ADDR_LEN + 16, 16, GEMMINI_VALUE_ROWS, 1, 0);
  }
  accel_commands(GEMMINI1, { attn_values(output, head, 0, 0); });
  return gemmini_job_end_at(GEMMINI1.control);
}

static int attn_run(unsigned head) {
  auto_link_input_ready();
  unsigned seen = 0;
  int failures = 0;
  for (unsigned index = 0; index < 3; ++index) {
    const cgra_link_result_t result = cgra_link_wait_at(CGRA0.control);
    const unsigned bit = result.stage < 3 ? 1u << result.stage : 0;
    if (bit == 0 || (seen & bit) || result.job != 0 || result.status != AUTO_LINK_STATUS_SUCCESS || result.detail != 0 || result.data != 0) {
      printf("Block attention head=%u stage=%u job=%u status=%u detail=%u data=%u\n", head, result.stage, result.job, result.status, result.detail, result.data);
      ++failures;
    }
    seen |= bit;
  }
  while (*(volatile uint32_t *)(AUTO_LINK_BASE + AUTO_LINK_RUNNING)) {
  }
  gemmini_fence();
  return failures + (seen != 7);
}

// Q/K/V are dense N x hidden INT8 tensors, d=64 and N=16, 64 or 128.
// Returns unscaled INT32 PV in the same dense layout; CGRA requantization follows.
static int attn_config_kernels(void) {
  int failures = attn_config_scores();
  accel_commands(CGRA0, { failures += cgra_job_sequence_at(CGRA0.control, attn_phases, sizeof(attn_phases) / sizeof(attn_phases[0])); });
  return failures;
}

static void attn_config_graph(void) {
  auto_link_load(&AUTO_LINK_GRAPH, NULL);
  auto_link_transfer(AUTO_LINK_COPY_SCORES_SOFTMAX, attn_score_row() * ATTN_ROW_BYTES, ATTENTION_MAX_ARG0 * sizeof(acc_t), ATTN_SLOT_BYTES, ATTN_SLOT_BYTES, BLOCK_N * sizeof(acc_t));
  auto_link_transfer(AUTO_LINK_COPY_SOFTMAX_VALUES, ATTENTION_NORM_ARG2, 0, ATTN_SLOT_WORDS, 0, BLOCK_N);
  auto_link_tiles(BLOCK_N, 1, ATTN_ROWS, 1);
}

static int attn_hybrid(const elem_t *q, const elem_t *k, const elem_t *v, acc_t *output) {
  unsigned long begin = attn_clock();
  if (attn_config_kernels()) {
    return 1;
  }
  attn_config_graph();
  attn_stats.setup += attn_clock() - begin;
  for (unsigned head = 0; head < BLOCK_HEADS; ++head) {
    begin = attn_clock();
    attn_load(q, k, v, head);
    attn_stats.execute += attn_clock() - begin;
    begin = attn_clock();
    const int configured = attn_config_values(output, head, 0);
    attn_stats.setup += attn_clock() - begin;
    if (configured) {
      return 1;
    }
    begin = attn_clock();
    const int failed = attn_run(head);
    attn_stats.execute += attn_clock() - begin;
    if (failed) {
      return 1;
    }
  }
  return 0;
}

static void attn_patch_head(accel_t device, unsigned command) {
  const gemmini_patch_t patch = {.command = command, .width = 64, .source = GEMMINI_VALUE_ITERATION, .scale = BLOCK_D, .relative = 1};
  gemmini_job_patch_at(device.control, &patch);
}

static int attn_config_loads(const elem_t *q, const elem_t *k, const elem_t *v) {
  const unsigned qk_loads = ATTN_TILES + ATTN_N_BLOCKS;
  if (gemmini_job_capture_at(GEMMINI0.control, ATTN_LOAD_JOB, 2 + qk_loads, qk_loads)) {
    return 1;
  }
  for (unsigned index = 0; index < qk_loads; ++index) {
    attn_patch_head(GEMMINI0, 2 + index);
  }
  accel_commands(GEMMINI0, { attn_load_qk(q, k, 0); });
  if (gemmini_job_end_at(GEMMINI0.control)) {
    return 1;
  }
  if (gemmini_job_capture_at(GEMMINI1.control, ATTN_LOAD_JOB, 3 + ATTN_N_BLOCKS, ATTN_N_BLOCKS)) {
    return 1;
  }
  for (unsigned index = 0; index < ATTN_N_BLOCKS; ++index) {
    attn_patch_head(GEMMINI1, 2 + index);
  }
  accel_commands(GEMMINI1, { attn_load_v(v, 0); });
  return gemmini_job_end_at(GEMMINI1.control);
}

static void attn_config_runs(void) {
  auto_link_load(&BLOCK_LOAD_GRAPH, NULL);
  auto_link_job(BLOCK_LOAD_STAGE_QK, ATTN_LOAD_JOB);
  auto_link_job(BLOCK_LOAD_STAGE_V, ATTN_LOAD_JOB);
  auto_link_tiles(1, 1, 1, 1);
  auto_link_run_config(0);
  attn_config_graph();
  auto_link_run_repeat(1, 0, BLOCK_HEADS);
}

static int attn_collect_heads(void) {
  unsigned count = 0, load_seen = 0, attention_seen = 0;
  int failures = 0;
  while (cgra_link_read_at(CGRA0.control, CGRA_LINK_CONTROL_RESULT_VALID)) {
    const cgra_link_result_t result = cgra_link_wait_at(CGRA0.control);
    const unsigned position = count % 5;
    const int loading = position < 2;
    const unsigned stages = loading ? 2 : 3;
    const unsigned job = loading ? ATTN_LOAD_JOB : 0;
    const unsigned bit = result.stage < stages ? 1u << result.stage : 0;
    unsigned *seen = loading ? &load_seen : &attention_seen;
    if (bit == 0 || (*seen & bit) || result.job != job || result.status != AUTO_LINK_STATUS_SUCCESS || result.detail != 0 || result.data != 0) {
      printf("HeadAuto report=%u stage=%u job=%u status=%u detail=%u data=%u\n", count, result.stage, result.job, result.status, result.detail, result.data);
      ++failures;
    }
    *seen |= bit;
    ++count;
    if (position == 4) {
      failures += load_seen != 3 || attention_seen != 7;
      load_seen = attention_seen = 0;
    }
  }
  if (count != 5 * BLOCK_HEADS) {
    printf("HeadAuto reports=%u expected=%u\n", count, 5 * BLOCK_HEADS);
    ++failures;
  }
  return failures;
}

// Only the head loop is autonomous; projections and the rest of the block remain CPU-controlled.
static int attn_head_auto(const elem_t *q, const elem_t *k, const elem_t *v, acc_t *output) {
  unsigned long begin = attn_clock();
  if (attn_config_kernels() || attn_config_loads(q, k, v) || attn_config_values(output, 0, 1)) {
    return 1;
  }
  attn_config_runs();
  attn_stats.setup = attn_clock() - begin;
  begin = attn_clock();
  auto_link_run(0, 2);
  while (*(volatile uint32_t *)(AUTO_LINK_BASE + AUTO_LINK_RUNNING)) {
  }
  gemmini_fence();
  const int failures = attn_collect_heads();
  attn_stats.execute = attn_clock() - begin;
  return failures;
}

static int attn_phase(unsigned phase, unsigned tile, unsigned slot) {
  const cgra_link_job_t *job = &attn_phases[phase];
  cgra_kernel_t kernel = *job->kernel;
  cgra_packet_t config[kernel.config_count];
  memcpy(config, kernel.config_packets, sizeof(config));
  for (unsigned index = 0; index < kernel.patch_count; ++index) {
    const cgra_patch_t *patch = &kernel.patches[index];
    const cgra_link_symbol_t *symbol = &job->bindings[patch->symbol_index];
    const unsigned source = symbol->source == CGRA_LINK_ROW ? tile * ATTN_ROWS : slot;
    const uint32_t value = symbol->source == CGRA_LINK_ELEMENTS ? ATTN_SLOT_WORDS : symbol->base + symbol->stride * source;
    cgra_packet_t *packet = &config[patch->packet_index];
    packet->hi = (packet->hi & ~ATTENTION_MAX_STORE_DATA_PAYLOAD_HI(UINT32_MAX)) | ATTENTION_MAX_STORE_DATA_PAYLOAD_HI(value * patch->scale + patch->offset);
  }
  kernel.config_packets = config;
  uint64_t ready = 0, status = 0, result = 0;
  accel_commands(CGRA0, {
    // LN and projection requantization have already executed on this CGRA.
    cgra_prepare(&kernel, CGRA_SWITCH);
    cgra_start(&kernel);
    CGRA_WAIT(ready);
    CGRA_STATUS(status);
    CGRA_RESULT(result);
  });
  if (ready != 1 || (status & 1) != 1 || ((status >> 1) & 0xffff) != kernel.expected_completes || result != 0) {
    printf("Block attention phase=%u tile=%u ready=%lu status=%lu result=%lu\n", phase, tile, (unsigned long)ready, (unsigned long)status, (unsigned long)result);
    return 1;
  }
  return 0;
}

static int attn_manual(const elem_t *q, const elem_t *k, const elem_t *v, acc_t *output) {
  unsigned long begin = attn_clock();
  // Restore the three resident Softmax programs after the block kernels.
  accel_commands(CGRA0, {
    for (unsigned phase = 0; phase < sizeof(attn_phases) / sizeof(attn_phases[0]); ++phase) {
      const cgra_kernel_t *kernel = attn_phases[phase].kernel;
      cgra_send_packets_fast(kernel->static_packets, kernel->static_count);
    }
  });
  attn_stats.setup = attn_clock() - begin;
  begin = attn_clock();
  for (unsigned head = 0; head < BLOCK_HEADS; ++head) {
    attn_load(q, k, v, head);
    for (unsigned tile = 0; tile < ATTN_TILES; ++tile) {
      const unsigned score_slot = tile % AUTO_LINK_GEMMINI0_BUFFER_SLOTS;
      const unsigned slot = tile % AUTO_LINK_CGRA0_BUFFER_SLOTS;
      accel_commands(GEMMINI0, { attn_scores(tile, score_slot); });
      gemmini_fence();
      const cgra_dma_desc_t descriptor = ((uint64_t)(ATTENTION_MAX_ARG0 + slot * ATTN_SLOT_WORDS) << CGRA_DMA_DESC_SPM_ADDR_LSB) | ((uint64_t)ATTN_SLOT_BYTES << CGRA_DMA_DESC_NBYTES_LSB);
      uint8_t tag;
      accel_commands(CGRA0, {
        cgra_dma_mvin_async((const void *)(GEMMINI0.spm + attn_score_row() * ATTN_ROW_BYTES + score_slot * ATTN_SLOT_BYTES), descriptor);
        tag = cgra_dma_wait(0);
      });
      if (tag != 0) {
        printf("Block attention DMA head=%u tile=%u tag=%u\n", head, tile, tag);
        return 1;
      }
      for (unsigned phase = 0; phase < sizeof(attn_phases) / sizeof(attn_phases[0]); ++phase) {
        if (attn_phase(phase, tile, slot) != 0) {
          return 1;
        }
      }
      accel_commands(GEMMINI1, { attn_values(output, head, tile, slot); });
      gemmini_fence();
    }
  }
  attn_stats.execute = attn_clock() - begin;
  return 0;
}

static int block_attention(const elem_t *q, const elem_t *k, const elem_t *v, acc_t *output, int mode) {
  static int (*const run[])(const elem_t *, const elem_t *, const elem_t *, acc_t *) = {attn_manual, attn_hybrid, attn_head_auto};
  attn_stats.setup = attn_stats.execute = attn_stats.total = 0;
  const unsigned long begin = attn_clock();
  const int failures = run[mode](q, k, v, output);
  attn_stats.total = attn_clock() - begin;
  return failures;
}

#endif
