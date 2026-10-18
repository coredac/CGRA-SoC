#include "accel_generated.h"
#include "auto_link.h"
#include "cgra_dma.h"
#include "cgra_link.h"
#include "gemmini.h"
#include "gemmini_job.h"
#include "generated/attention_data.h"
#include "generated/cgra_attention_exp_fast_api.h"
#include "generated/cgra_attention_max_fast_api.h"
#include "generated/cgra_attention_norm_fast_api.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

// Query tiles contain at most DIM rows; N and d are multiples of DIM.
enum {
  TILES = (ATTENTION_N + ATTENTION_TILE_ROWS - 1) / ATTENTION_TILE_ROWS,
  N_BLOCKS = ATTENTION_N / DIM,
  D_BLOCKS = ATTENTION_D / DIM,
  Q_ROW = 0,
  K_ROW = TILES * ATTENTION_D,
  P_ROW = 0,
  V_ROW = ATTENTION_N,
  SLOT_WORDS = ATTENTION_N * ATTENTION_TILE_ROWS,
  SLOT_BYTES = SLOT_WORDS * sizeof(acc_t),
  INPUT_WORD = ATTENTION_MAX_ARG0,
  MAX_WORD = ATTENTION_MAX_ARG1,
  EXP_WORD = ATTENTION_EXP_ARG2,
  SUM_WORD = ATTENTION_EXP_ARG3,
  PROB_WORD = ATTENTION_NORM_ARG2,
  OUTPUT_BYTES = ATTENTION_N * ATTENTION_D * sizeof(acc_t),
  OUTPUT_TILE_BYTES = ATTENTION_TILE_ROWS * ATTENTION_D * sizeof(acc_t),
  ROW_BYTES = DIM * sizeof(elem_t),
  ACC_READ = (UINT32_C(1) << (ADDR_LEN - 1)) | (UINT32_C(1) << (ADDR_LEN - 3)),
  LOOP_SKIPS = (1 << 3) | (1 << 4) | (1 << 5) | (1 << 7),
};

static const cgra_link_symbol_t max_symbols[ATTENTION_MAX_SYMBOL_COUNT] = {
    [ATTENTION_MAX_SYMBOL_INPUT0] = {INPUT_WORD, SLOT_WORDS, CGRA_LINK_SLOT},
    [ATTENTION_MAX_SYMBOL_OUTPUT] = {MAX_WORD, ATTENTION_TILE_ROWS, CGRA_LINK_SLOT},
    [ATTENTION_MAX_SYMBOL_COLUMNS] = {ATTENTION_N, 0, CGRA_LINK_SLOT},
    [ATTENTION_MAX_SYMBOL_ROW] = {0, 1, CGRA_LINK_ROW},
    [ATTENTION_MAX_SYMBOL_ELEMENTS] = {0, 0, CGRA_LINK_ELEMENTS},
};
static const cgra_link_symbol_t exp_symbols[ATTENTION_EXP_SYMBOL_COUNT] = {
    [ATTENTION_EXP_SYMBOL_INPUT0] = {INPUT_WORD, SLOT_WORDS, CGRA_LINK_SLOT},
    [ATTENTION_EXP_SYMBOL_INPUT1] = {MAX_WORD, ATTENTION_TILE_ROWS, CGRA_LINK_SLOT},
    [ATTENTION_EXP_SYMBOL_OUTPUT] = {EXP_WORD, SLOT_WORDS, CGRA_LINK_SLOT},
    [ATTENTION_EXP_SYMBOL_OUTPUT1] = {SUM_WORD, ATTENTION_TILE_ROWS, CGRA_LINK_SLOT},
    [ATTENTION_EXP_SYMBOL_COLUMNS] = {ATTENTION_N, 0, CGRA_LINK_SLOT},
    [ATTENTION_EXP_SYMBOL_ROW] = {0, 1, CGRA_LINK_ROW},
    [ATTENTION_EXP_SYMBOL_ELEMENTS] = {0, 0, CGRA_LINK_ELEMENTS},
};
static const cgra_link_symbol_t norm_symbols[ATTENTION_NORM_SYMBOL_COUNT] = {
    [ATTENTION_NORM_SYMBOL_INPUT0] = {EXP_WORD, SLOT_WORDS, CGRA_LINK_SLOT},
    [ATTENTION_NORM_SYMBOL_INPUT1] = {SUM_WORD, ATTENTION_TILE_ROWS, CGRA_LINK_SLOT},
    [ATTENTION_NORM_SYMBOL_OUTPUT] = {PROB_WORD, SLOT_WORDS, CGRA_LINK_SLOT},
    [ATTENTION_NORM_SYMBOL_COLUMNS] = {ATTENTION_N, 0, CGRA_LINK_SLOT},
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

static unsigned tile_rows(unsigned tile) {
  const unsigned remaining = ATTENTION_N - tile * ATTENTION_TILE_ROWS;
  return remaining < ATTENTION_TILE_ROWS ? remaining : ATTENTION_TILE_ROWS;
}

static unsigned score_row(void) { return (GEMMINI0.spm_bytes - AUTO_LINK_GEMMINI0_BUFFER_SLOTS * SLOT_BYTES) / ROW_BYTES; }

static unsigned output_row(void) { return (GEMMINI1.spm_bytes - OUTPUT_BYTES) / ROW_BYTES; }

static void init_inputs(void) {
  accel_commands(GEMMINI0, {
    gemmini_flush(0);
    gemmini_config_ld(ATTENTION_D);
    gemmini_config_st(ATTENTION_N * sizeof(acc_t));
    for (unsigned tile = 0; tile < TILES; ++tile) {
      gemmini_extended_mvin(attention_q + tile * ATTENTION_TILE_ROWS * ATTENTION_D, Q_ROW + tile * ATTENTION_D, ATTENTION_D, tile_rows(tile));
    }
    for (unsigned block = 0; block < N_BLOCKS; ++block) {
      gemmini_extended_mvin(attention_k + block * DIM * ATTENTION_D, K_ROW + block * ATTENTION_D, ATTENTION_D, DIM);
    }
  });
  accel_commands(GEMMINI1, {
    gemmini_flush(0);
    gemmini_config_ld(ATTENTION_D);
    gemmini_config_st(ATTENTION_D * sizeof(acc_t));
    for (unsigned block = 0; block < N_BLOCKS; ++block) {
      gemmini_extended_mvin(attention_v + block * DIM * ATTENTION_D, V_ROW + block * ATTENTION_D, ATTENTION_D, DIM);
    }
    gemmini_config_ld(ATTENTION_N);
  });
  gemmini_fence();
}

// These native commands are shared by manual execution and automatic capture.
static void scores(unsigned tile, unsigned slot, unsigned rows) {
  gemmini_extended_config_ex(WEIGHT_STATIONARY, NO_ACTIVATION, 0, 1, false, true);
  gemmini_loop_ws_spad(1, N_BLOCKS, D_BLOCKS, DIM - rows, 0, 0, Q_ROW + tile * ATTENTION_D, K_ROW + ATTENTION_N * D_BLOCKS, 0, 0, false, true, true, false, false, NO_ACTIVATION, 0, 0, false,
                       LOOP_SKIPS);
  for (unsigned block = 0; block < N_BLOCKS; ++block) {
    gemmini_extended_mvout_spad(score_row() + slot * SLOT_BYTES / ROW_BYTES + block * sizeof(acc_t), ATTENTION_N * sizeof(acc_t) / ROW_BYTES, ACC_READ + block * DIM, DIM, rows);
  }
}

static void values(uintptr_t probabilities, unsigned tile, unsigned rows) {
  gemmini_config_ex(WEIGHT_STATIONARY, NO_ACTIVATION, 0);
  for (unsigned block = 0; block < ATTENTION_N / ATTENTION_D; ++block) {
    gemmini_extended_mvin((const void *)(probabilities + block * ATTENTION_D), P_ROW + block * ATTENTION_D, ATTENTION_D, rows);
  }
  gemmini_loop_ws_spad(1, D_BLOCKS, N_BLOCKS, DIM - rows, 0, 0, P_ROW, V_ROW + ATTENTION_N * D_BLOCKS, 0, 0, false, false, true, false, false, NO_ACTIVATION, 0, 0, false, LOOP_SKIPS);
  for (unsigned block = 0; block < D_BLOCKS; ++block) {
    gemmini_extended_mvout_spad(output_row() + tile * OUTPUT_TILE_BYTES / ROW_BYTES + block * sizeof(acc_t), ATTENTION_D * sizeof(acc_t) / ROW_BYTES, ACC_READ + block * DIM, DIM, rows);
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
  patch_gemmini(GEMMINI0, 2, 0, 0, ADDR_LEN, GEMMINI_VALUE_TILE_ID, ATTENTION_D, Q_ROW);
  for (unsigned block = 0; block < N_BLOCKS; ++block) {
    patch_gemmini(GEMMINI0, 4 + block, 0, 0, ADDR_LEN, GEMMINI_VALUE_SLOT, SLOT_BYTES / ROW_BYTES, score_row() + block * sizeof(acc_t));
    patch_gemmini(GEMMINI0, 4 + block, 1, ADDR_LEN + 16, 16, GEMMINI_VALUE_ROWS, 1, 0);
  }
  accel_commands(GEMMINI0, { scores(0, 0, ATTENTION_TILE_ROWS); });
  return gemmini_job_end_at(GEMMINI0.control);
}

static int configure_values(void) {
  const unsigned loads = ATTENTION_N / ATTENTION_D;
  gemmini_job_write_at(GEMMINI1.control, GEMMINI_JOB_WINDOW_PIXEL_BYTES, ATTENTION_N);
  if (gemmini_job_capture_at(GEMMINI1.control, AUTO_LINK_JOB_VALUES, 4 + loads + D_BLOCKS, 1 + 2 * loads + 2 * D_BLOCKS) != 0) {
    return 1;
  }
  for (unsigned block = 0; block < loads; ++block) {
    patch_gemmini(GEMMINI1, 1 + block, 0, 0, 64, GEMMINI_VALUE_INPUT_ADDRESS, 1, block * ATTENTION_D);
    patch_gemmini(GEMMINI1, 1 + block, 1, ADDR_LEN + 16, 16, GEMMINI_VALUE_ROWS, 1, 0);
  }
  patch_gemmini(GEMMINI1, 1 + loads, 0, 0, 16, GEMMINI_VALUE_ROWS, UINT32_MAX, DIM);
  for (unsigned block = 0; block < D_BLOCKS; ++block) {
    patch_gemmini(GEMMINI1, 4 + loads + block, 0, 0, ADDR_LEN, GEMMINI_VALUE_TILE_ID, OUTPUT_TILE_BYTES / ROW_BYTES, output_row() + block * sizeof(acc_t));
    patch_gemmini(GEMMINI1, 4 + loads + block, 1, ADDR_LEN + 16, 16, GEMMINI_VALUE_ROWS, 1, 0);
  }
  accel_commands(GEMMINI1, { values(CGRA0.spm + PROB_WORD, 0, ATTENTION_TILE_ROWS); });
  return gemmini_job_end_at(GEMMINI1.control);
}

static int run_phase(unsigned phase, unsigned tile, unsigned slot) {
  const cgra_link_job_t *job = &phases[phase];
  cgra_kernel_t kernel = *job->kernel;
  cgra_packet_t config[kernel.config_count];
  memcpy(config, kernel.config_packets, sizeof(config));
  for (unsigned index = 0; index < kernel.patch_count; ++index) {
    const cgra_patch_t *patch = &kernel.patches[index];
    const cgra_link_symbol_t *symbol = &job->bindings[patch->symbol_index];
    const unsigned source = symbol->source == CGRA_LINK_ROW ? tile * ATTENTION_TILE_ROWS : slot;
    const uint32_t value = symbol->source == CGRA_LINK_ELEMENTS ? tile_rows(tile) * ATTENTION_N : symbol->base + symbol->stride * source;
    cgra_packet_t *packet = &config[patch->packet_index];
    packet->hi = (packet->hi & ~ATTENTION_MAX_STORE_DATA_PAYLOAD_HI(UINT32_MAX)) | ATTENTION_MAX_STORE_DATA_PAYLOAD_HI(value * patch->scale + patch->offset);
  }
  kernel.config_packets = config;
  uint64_t ready = 0, status = 0, result = 0;
  accel_commands(CGRA0, {
    cgra_prepare(&kernel, tile == 0 && phase == 0 ? CGRA_COLD : CGRA_SWITCH);
    cgra_start(&kernel);
    CGRA_WAIT(ready);
    CGRA_STATUS(status);
    CGRA_RESULT(result);
  });
  if (ready != 1 || (status & 1) != 1 || ((status >> 1) & 0xffff) != kernel.expected_completes || result != 0) {
    printf("Attention phase=%u tile=%u ready=%lu status=%lu result=%lu\n", phase, tile, (unsigned long)ready, (unsigned long)status, (unsigned long)result);
    return 1;
  }
  return 0;
}

static int run_manual(void) {
  for (unsigned tile = 0; tile < TILES; ++tile) {
    const unsigned rows = tile_rows(tile);
    const unsigned score_slot = tile % AUTO_LINK_GEMMINI0_BUFFER_SLOTS;
    const unsigned slot = tile % AUTO_LINK_CGRA0_BUFFER_SLOTS;
    accel_commands(GEMMINI0, { scores(tile, score_slot, rows); });
    gemmini_fence();
    const cgra_dma_desc_t descriptor = ((uint64_t)(INPUT_WORD + slot * SLOT_WORDS) << CGRA_DMA_DESC_SPM_ADDR_LSB) | ((uint64_t)(rows * ATTENTION_N * sizeof(acc_t)) << CGRA_DMA_DESC_NBYTES_LSB);
    uint8_t tag = 0;
    accel_commands(CGRA0, {
      cgra_dma_mvin_async((const void *)(GEMMINI0.spm + score_row() * ROW_BYTES + score_slot * SLOT_BYTES), descriptor);
      tag = cgra_dma_wait(0);
    });
    if (tag != 0) {
      printf("Attention score DMA tile=%u tag=%u\n", tile, tag);
      return 1;
    }
    for (unsigned phase = 0; phase < sizeof(phases) / sizeof(phases[0]); ++phase) {
      if (run_phase(phase, tile, slot) != 0) {
        return 1;
      }
    }
    accel_commands(GEMMINI1, { values(CGRA0.spm + PROB_WORD + slot * SLOT_WORDS, tile, rows); });
    gemmini_fence();
  }
  return 0;
}

static int run_auto(void) {
  auto_link_transfer(AUTO_LINK_COPY_SCORES_SOFTMAX, score_row() * ROW_BYTES, INPUT_WORD * sizeof(acc_t), SLOT_BYTES, SLOT_BYTES, ATTENTION_N * sizeof(acc_t));
  auto_link_transfer(AUTO_LINK_COPY_SOFTMAX_VALUES, PROB_WORD, 0, SLOT_WORDS, 0, ATTENTION_N);
  auto_link_tiles(ATTENTION_N, 1, ATTENTION_TILE_ROWS, 1);
  auto_link_input_ready();
  unsigned seen = 0;
  int failures = 0;
  for (unsigned index = 0; index < 3; ++index) {
    const cgra_link_result_t result = cgra_link_wait_at(CGRA0.control);
    const unsigned bit = result.stage < 3 ? 1u << result.stage : 0;
    if (bit == 0 || (seen & bit) || result.job != 0 || result.status != AUTO_LINK_STATUS_SUCCESS || result.detail != 0 || result.data != 0) {
      printf("Attention stage=%u job=%u status=%u detail=%u data=%u\n", result.stage, result.job, result.status, result.detail, result.data);
      ++failures;
    }
    seen |= bit;
  }
  while (*(volatile uint32_t *)(AUTO_LINK_BASE + AUTO_LINK_RUNNING)) {
  }
  printf("Attention Auto fabric_cycles=%lu overlap=%lu peak=%lu\n", (unsigned long)*(volatile uint64_t *)(AUTO_LINK_BASE + AUTO_LINK_CYCLES),
         (unsigned long)*(volatile uint64_t *)(AUTO_LINK_BASE + AUTO_LINK_OVERLAP), (unsigned long)*(volatile uint64_t *)(AUTO_LINK_BASE + AUTO_LINK_PEAK_ACTIVE));
  return failures + (seen != 7);
}

static int verify_output(void) {
  const volatile acc_t *actual = (const volatile acc_t *)(GEMMINI1.spm + output_row() * ROW_BYTES);
  int failures = 0;
  cgra_dma_memory_fence();
  for (unsigned index = 0; index < ATTENTION_N * ATTENTION_D; ++index) {
    if (actual[index] != attention_expected[index]) {
      if (failures < 8) {
        printf("Attention element=%u actual=%d expected=%d\n", index, (int)actual[index], (int)attention_expected[index]);
      }
      ++failures;
    }
  }
  printf("Attention checked=%u mismatches=%d\n", ATTENTION_N * ATTENTION_D, failures);
  return failures;
}

int main(int argc, char **argv) {
  const int automatic = argc > 1 && (strcmp(argv[1], "auto") == 0 || strcmp(argv[1], "1") == 0);
  const char *mode = automatic ? "Auto" : "Manual";
  unsigned long begin = cycles();
  init_inputs();
  int failures = 0;
  if (automatic) {
    failures += configure_scores() + configure_values();
    accel_commands(CGRA0, { failures += cgra_job_sequence_at(CGRA0.control, phases, sizeof(phases) / sizeof(phases[0])); });
  } else {
    accel_commands(CGRA0, {
      for (unsigned phase = 0; phase < sizeof(phases) / sizeof(phases[0]); ++phase) {
        cgra_send_packets_fast(phases[phase].kernel->static_packets, phases[phase].kernel->static_count);
      }
    });
  }
  if (failures) {
    printf("Attention %s: FAIL (configuration)\n", mode);
    return 1;
  }
  const unsigned long setup_cycles = cycles() - begin;
  cgra_dma_memory_fence();
  printf("Attention %s ready N=%u d=%u tile_rows=%u\n", mode, ATTENTION_N, ATTENTION_D, ATTENTION_TILE_ROWS);
  begin = cycles();
  failures = automatic ? run_auto() : run_manual();
  const unsigned long execution_cycles = cycles() - begin;
  if (failures == 0) {
    failures += verify_output();
  }
  printf("Attention %s setup_cycles=%lu execution_cycles=%lu\n", mode, setup_cycles, execution_cycles);
  printf("Attention %s: %s (%d mismatches)\n", mode, failures == 0 ? "PASS" : "FAIL", failures);
  return failures != 0;
}
