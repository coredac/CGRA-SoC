#include "accel_generated.h"
#include "block_attention.h"
#include "block_cgra.h"
#include "block_gemm.h"
#include "block_spm.h"
#include "generated/gpt2_data.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

enum { HIDDEN_WORDS = BLOCK_N * BLOCK_HIDDEN, EXPANDED_WORDS = BLOCK_N * BLOCK_FF };

// LOADMEM initializes .data without a large simulated CPU BSS-clear loop.
// Each producer overwrites its complete output before any consumer reads it.
static elem_t dram[6 * HIDDEN_WORDS + EXPANDED_WORDS] __attribute__((aligned(64))) = {1};
static acc_t dram_accum[EXPANDED_WORDS] __attribute__((aligned(64))) = {1};
static elem_t *normalized, *query, *key, *value, *skip, *branch, *expanded;
static acc_t *accum;
static int local, benchmark;

static void buffers(void) {
  if (local) {
    skip = (elem_t *)(GEMMINI0.spm + 64 * 1024);
    normalized = skip + HIDDEN_WORDS;
    query = normalized + HIDDEN_WORDS;
    key = query + HIDDEN_WORDS;
    branch = key + HIDDEN_WORDS;
    value = (elem_t *)(GEMMINI1.spm + 64 * 1024);
    accum = (acc_t *)(value + HIDDEN_WORDS);
    // V and PV are dead before the expanded MLP activation is produced.
    expanded = value;
    return;
  }
  normalized = dram;
  query = normalized + HIDDEN_WORDS;
  key = query + HIDDEN_WORDS;
  value = key + HIDDEN_WORDS;
  skip = value + HIDDEN_WORDS;
  branch = skip + HIDDEN_WORDS;
  expanded = branch + HIDDEN_WORDS;
  accum = dram_accum;
}

static int copy_activation(const elem_t *input, elem_t *output) { return block_load(input, 0, HIDDEN_WORDS, 1) || block_store(output, 0, HIDDEN_WORDS); }

static unsigned long stage_begin, execution_cycles;

static int check_stage(const char *name, const volatile elem_t *actual, const int8_t *expected, unsigned count) {
  if (benchmark) {
    return 0;
  }
  const unsigned long end = block_clock();
  const unsigned long elapsed = end - stage_begin;
  execution_cycles += elapsed;
  unsigned errors = 0;
  for (unsigned index = 0; index < count; ++index) {
    if (actual[index] != expected[index]) {
      if (errors < 8) {
        printf("GPT2 %s index=%u actual=%d expected=%d\n", name, index, actual[index], expected[index]);
      }
      ++errors;
    }
  }
  printf("GPT2 %s outputs=%u mismatches=%u cycle=%lu elapsed=%lu\n", name, count, errors, end, elapsed);
  stage_begin = block_clock();
  return errors != 0;
}

static int check_output(unsigned run) {
  unsigned errors = 0;
  for (unsigned index = 0; index < HIDDEN_WORDS; index += sizeof(uint32_t)) {
    const uint32_t actual = ((const volatile uint32_t *)dram)[index / sizeof(uint32_t)];
    uint32_t expected;
    memcpy(&expected, block_expected_y[run] + index, sizeof(expected));
    if (actual != expected) {
      if (errors < 8) {
        printf("GPT2 mismatch index=%u actual=%08x expected=%08x\n", index, actual, expected);
      }
      ++errors;
    }
  }
  printf("GPT2 run=%u outputs=%u mismatched_words=%u\n", run, HIDDEN_WORDS, errors);
  return errors != 0;
}

static int project(accel_t device, const elem_t *input, const elem_t *weight, const acc_t *bias, elem_t *output, unsigned inner, unsigned columns, unsigned divisor) {
  if (local) {
    return block_project_spm(device, input, weight, bias, output, BLOCK_N, inner, columns, divisor);
  }
  block_matmul(device, input, weight, bias, accum, BLOCK_N, inner, columns);
  return block_quantize(accum, output, BLOCK_N * columns, divisor);
}

static int run_block(unsigned run, int mode) {
  const int8_t *input = block_x[run];
  if (local) {
    if (copy_activation(input, skip)) {
      return 1;
    }
    input = skip;
  }
  if (block_layernorm(input, block_gamma1, block_beta1, normalized, BLOCK_N, BLOCK_HIDDEN) || check_stage("ln1", normalized, block_expected_ln1[run], HIDDEN_WORDS)) {
    return 1;
  }
  if (project(GEMMINI0, normalized, block_wq, block_bq, query, BLOCK_HIDDEN, BLOCK_HIDDEN, BLOCK_Q_DIV) || check_stage("q", query, block_expected_q[run], HIDDEN_WORDS)) {
    return 1;
  }
  if (project(GEMMINI0, normalized, block_wk, block_bk, key, BLOCK_HIDDEN, BLOCK_HIDDEN, BLOCK_K_DIV) || check_stage("k", key, block_expected_k[run], HIDDEN_WORDS)) {
    return 1;
  }
  if (project(GEMMINI1, normalized, block_wv, block_bv, value, BLOCK_HIDDEN, BLOCK_HIDDEN, BLOCK_V_DIV) || check_stage("v", value, block_expected_v[run], HIDDEN_WORDS)) {
    return 1;
  }
  // Every head has drained before query storage becomes the concatenation buffer.
  if (block_attention(query, key, value, accum, mode) || block_quantize(accum, query, HIDDEN_WORDS, BLOCK_PV_DIV) || check_stage("concat", query, block_expected_concat[run], HIDDEN_WORDS)) {
    return 1;
  }
  if (project(GEMMINI0, query, block_wo, block_bo, branch, BLOCK_HIDDEN, BLOCK_HIDDEN, BLOCK_ATTENTION_DIV) || check_stage("attention", branch, block_expected_attention[run], HIDDEN_WORDS)) {
    return 1;
  }
  if (block_add(input, branch, skip, HIDDEN_WORDS) || check_stage("residual1", skip, block_expected_u[run], HIDDEN_WORDS)) {
    return 1;
  }
  if (block_layernorm(skip, block_gamma2, block_beta2, normalized, BLOCK_N, BLOCK_HIDDEN) || check_stage("ln2", normalized, block_expected_ln2[run], HIDDEN_WORDS)) {
    return 1;
  }
  if (project(GEMMINI0, normalized, block_w1, block_b1, expanded, BLOCK_HIDDEN, BLOCK_FF, BLOCK_FC1_DIV) || check_stage("fc1", expanded, block_expected_fc1[run], EXPANDED_WORDS)) {
    return 1;
  }
  if (block_activation(expanded, expanded, EXPANDED_WORDS) || check_stage("gelu", expanded, block_expected_gelu[run], EXPANDED_WORDS)) {
    return 1;
  }
  if (project(GEMMINI1, expanded, block_w2, block_b2, branch, BLOCK_FF, BLOCK_HIDDEN, BLOCK_FC2_DIV) || check_stage("fc2", branch, block_expected_fc2[run], HIDDEN_WORDS)) {
    return 1;
  }
  if (block_add(skip, branch, branch, HIDDEN_WORDS)) {
    return 1;
  }
  if (local) {
    // Only the completed block activation is returned to DRAM.
    return copy_activation(branch, dram) || check_stage("residual2", dram, block_expected_y[run], HIDDEN_WORDS);
  }
  return check_stage("residual2", branch, block_expected_y[run], HIDDEN_WORDS);
}

int main(int argc, char **argv) {
  const int mode = argc > 1 ? atoi(argv[1]) : 0;
  static const char *modes[] = {"Manual", "Hybrid", "HeadAuto"};
  if (mode < 0 || mode > 2) {
    printf("GPT2 mode must be 0 (Manual), 1 (Hybrid), or 2 (HeadAuto)\n");
    return 1;
  }
  const char *storage = argc > 2 ? argv[2] : "dram";
  local = strcmp(storage, "spm") == 0;
  const int spm_shape = (BLOCK_N == 64 && BLOCK_HIDDEN == 128 && BLOCK_HEADS == 2 && BLOCK_FF == 512) || (BLOCK_N == 16 && BLOCK_HIDDEN == 768 && BLOCK_HEADS == 12 && BLOCK_FF == 3072);
  if ((!local && strcmp(storage, "dram") != 0) || (local && !spm_shape)) {
    printf("GPT2 storage must be dram, or spm with N=64 H=128 heads=2 FF=512 or N=16 H=768 heads=12 FF=3072\n");
    return 1;
  }
  benchmark = argc > 3 && strcmp(argv[3], "bench") == 0;
  if (argc > 3 && (!benchmark || !local)) {
    printf("GPT2 benchmark requires spm bench\n");
    return 1;
  }
  buffers();
  accel_commands(GEMMINI0, { gemmini_flush(0); });
  accel_commands(GEMMINI1, { gemmini_flush(0); });
  gemmini_fence();
  printf("GPT2 synthetic block N=%u H=%u heads=%u FF=%u runs=%u\n", BLOCK_N, BLOCK_HIDDEN, BLOCK_HEADS, BLOCK_FF, BLOCK_RUNS);
  printf("GPT2 mode=%s storage=%s benchmark=%d\n", modes[mode], storage, benchmark);
  for (unsigned run = 0; run < BLOCK_RUNS; ++run) {
    printf("GPT2 run=%u begin cycle=%lu\n", run, block_clock());
    block_perf_reset();
    execution_cycles = 0;
    stage_begin = block_clock();
    const int failed = run_block(run, mode);
    const unsigned long end = block_clock();
    if (failed || (benchmark && check_output(run))) {
      printf("GPT2: FAIL run=%u\n", run);
      return 1;
    }
    if (local && (block_perf.dram_read != HIDDEN_WORDS + 4 * BLOCK_HIDDEN * sizeof(int32_t) || block_perf.dram_write != HIDDEN_WORDS)) {
      printf("GPT2: FAIL unexpected CGRA DRAM payload read=%lu write=%lu\n", block_perf.dram_read, block_perf.dram_write);
      return 1;
    }
    if (benchmark) {
      printf("GPT2 run=%u total=%lu\n", run, end - stage_begin);
    } else {
      printf("GPT2 run=%u execution_cycles=%lu\n", run, execution_cycles);
    }
    block_perf_print();
  }
  printf("GPT2: PASS\n");
  return 0;
}
