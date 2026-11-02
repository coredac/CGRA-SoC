#include "generated/runtime_layernorm.h"
#include "soc_runtime.h"

#include <stdint.h>
#include <stdio.h>

enum { ROWS = SOC_PLAN_ROWS, WIDTH = 8, SAMPLES = 2 };

static const int32_t gamma[WIDTH] __attribute__((aligned(64))) = {-512, 0, 32, 64, 96, -32, 48, 512};
static const int32_t beta[WIDTH] __attribute__((aligned(64))) = {-24, -16, -8, 0, 8, 16, 24, -8};

static const int8_t input[SAMPLES][ROWS][WIDTH] __attribute__((aligned(64))) = {
    {
        {-128, 127, -128, 127, -128, 127, -128, 127},
        {-37, -37, -37, -37, -37, -37, -37, -37},
        {-3, -2, -3, -2, -3, -2, -3, -1},
        {-91, 23, 55, -4, 127, -128, 1, 18},
    },
    {
        {127, -128, 127, -128, 127, -128, 127, -128},
        {19, 19, 19, 19, 19, 19, 19, 19},
        {-4, -2, 5, 9, -11, 3, 0, 2},
        {12, -108, 73, 8, -57, 124, -1, 41},
    },
};

// Golden Q4 values use scripts/block_math.py:layernorm, including signed rounding and saturation.
static const int8_t expected[SAMPLES][ROWS][WIDTH] __attribute__((aligned(64))) = {
    {
        {122, -4, -10, 16, -22, -4, -6, 126},
        {-6, -4, -2, 0, 2, 4, 6, -2},
        {60, -4, -6, 5, -10, 2, 0, 127},
        {127, -4, 4, -1, 43, 18, 6, 29},
    },
    {
        {-128, -4, 6, -16, 26, 12, 18, -128},
        {-6, -4, -2, 0, 2, 4, 6, -2},
        {88, -4, 5, 24, -45, 0, 6, 37},
        {-7, -4, 5, -1, -22, -9, 4, 54},
    },
};

static int8_t output[ROWS][WIDTH] __attribute__((aligned(64)));

static int verify(unsigned sample) {
  for (unsigned row = 0; row < ROWS; ++row)
    for (unsigned column = 0; column < WIDTH; ++column)
      if (output[row][column] != expected[sample][row][column]) {
        printf("LayerNorm mismatch row=%u col=%u got=%d expected=%d\n", row, column, output[row][column], expected[sample][row][column]);
        return 1;
      }
  return 0;
}

int main(void) {
  static int (*const execute[])(soc_context_t *) = {soc_execute_manual, soc_execute_hybrid, soc_execute_auto};
  soc_state_t states[SOC_PLAN_DEVICES] = {0};
  soc_context_t context = {.states = states};
  const unsigned modes[] = {SOC_MANUAL, SOC_HYBRID, SOC_AUTO, SOC_MANUAL};
  for (unsigned trial = 0; trial < sizeof(modes) / sizeof(modes[0]); ++trial) {
    const unsigned sample = trial % SAMPLES;
    const uintptr_t arguments[] = {(uintptr_t)input[sample], (uintptr_t)gamma, (uintptr_t)beta, 0, 0, 0, 0, 0, 0, (uintptr_t)output};
    for (unsigned row = 0; row < ROWS; ++row)
      for (unsigned column = 0; column < WIDTH; ++column)
        output[row][column] = 101;
    if (soc_prepare(&context, &soc_plans[modes[trial]], arguments) || execute[modes[trial]](&context) || verify(sample)) {
      printf("LayerNorm mode=%u sample=%u: FAIL\n", modes[trial], sample);
      return 1;
    }
  }
  puts("PASS");
  return 0;
}
