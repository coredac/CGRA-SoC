#include "generated/runtime_terminal.h"
#include "soc_runtime.h"

#include <stdint.h>
#include <stdio.h>

enum { ROWS = SOC_PLAN_ROWS, WIDTH = 8 };
static int8_t input[ROWS][WIDTH] __attribute__((aligned(64)));
static int8_t weights[WIDTH][WIDTH] __attribute__((aligned(64)));
static int8_t output[ROWS][WIDTH] __attribute__((aligned(64)));

static void init_data(unsigned sample) {
  for (unsigned row = 0; row < ROWS; ++row)
    for (unsigned column = 0; column < WIDTH; ++column) {
      input[row][column] = (int8_t)((int)((row * 13 + column * 7 + sample) % 101) - 50);
      output[row][column] = -1;
    }
  for (unsigned row = 0; row < WIDTH; ++row)
    for (unsigned column = 0; column < WIDTH; ++column)
      weights[row][column] = (int8_t)((int)((row * 3 + column + sample) % 5) - 2);
}

static int verify(void) {
  for (unsigned row = 0; row < ROWS; ++row)
    for (unsigned column = 0; column < WIDTH; ++column) {
      int32_t sum = 0;
      for (unsigned k = 0; k < WIDTH; ++k)
        sum += input[row][k] * weights[k][column];
      const int8_t expected = sum < 0 ? 0 : sum > 127 ? 127 : (int8_t)sum;
      if (output[row][column] != expected) {
        printf("Terminal mismatch row=%u col=%u got=%d expected=%d\n", row, column, output[row][column], expected);
        return 1;
      }
    }
  return 0;
}

int main(void) {
  static int (*const execute[])(soc_context_t *) = {soc_execute_manual, soc_execute_hybrid, soc_execute_auto};
  soc_state_t states[SOC_PLAN_DEVICES] = {0};
  soc_context_t context = {.states = states};
  const uintptr_t arguments[] = {(uintptr_t)input, (uintptr_t)weights, 0, (uintptr_t)output};
  const unsigned modes[] = {SOC_MANUAL, SOC_HYBRID, SOC_AUTO, SOC_MANUAL};
  for (unsigned trial = 0; trial < sizeof(modes) / sizeof(modes[0]); ++trial) {
    const unsigned sample = trial % 2;
    init_data(sample);
    const soc_plan_t *plan = &soc_plans[modes[trial]];
    if (soc_prepare(&context, plan, arguments) || execute[modes[trial]](&context) || verify()) {
      printf("Runtime terminal mode=%u input=%u: FAIL\n", modes[trial], sample);
      return 1;
    }
  }
  puts("Runtime terminal: PASS");
  return 0;
}
