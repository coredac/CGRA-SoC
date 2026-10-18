#include "block_attention.h"
#include "block_cgra.h"
#include "generated/gpt2_data.h"

#include <stdint.h>
#include <stdio.h>

enum { ROWS = BLOCK_TILE_ROWS, NORM_WORDS = ROWS * BLOCK_HIDDEN, WORDS = BLOCK_N * BLOCK_HIDDEN };
static acc_t accum[WORDS] __attribute__((aligned(64))) = {1};
static elem_t output[WORDS] __attribute__((aligned(64))) = {1};

static int check(const char *phase, unsigned run, const int8_t *expected, unsigned count) {
  unsigned errors = 0;
  for (unsigned index = 0; index < count; ++index) {
    if (output[index] != expected[index]) {
      if (errors < 8) {
        printf("Switch %s run=%u index=%u actual=%d expected=%d\n", phase, run, index, output[index], expected[index]);
      }
      ++errors;
    }
  }
  printf("Switch %s run=%u outputs=%u mismatches=%u cycle=%lu\n", phase, run, count, errors, block_clock());
  return errors != 0;
}

int main(void) {
  accel_commands(GEMMINI0, { gemmini_flush(0); });
  accel_commands(GEMMINI1, { gemmini_flush(0); });
  gemmini_fence();
  for (unsigned run = 0; run < 2; ++run) {
    printf("Switch run=%u mode=%s begin\n", run, run ? "Hybrid" : "Manual");
    if (block_layernorm(block_x[run], block_gamma1, block_beta1, output, ROWS, BLOCK_HIDDEN) || check("ln1", run, block_expected_ln1[run], NORM_WORDS)) {
      return 1;
    }
    // Projections are not under test: use their precomputed Q/K/V outputs.
    if (block_attention(block_expected_q[run], block_expected_k[run], block_expected_v[run], accum, run) || block_quantize(accum, output, WORDS, BLOCK_PV_DIV) ||
        check("attention", run, block_expected_concat[run], WORDS)) {
      return 1;
    }
    if (block_layernorm(block_expected_u[run], block_gamma2, block_beta2, output, ROWS, BLOCK_HIDDEN) || check("ln2", run, block_expected_ln2[run], NORM_WORDS)) {
      return 1;
    }
  }
  printf("Switch: PASS\n");
  return 0;
}
