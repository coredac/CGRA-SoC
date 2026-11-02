#include "generated/runtime_chain.h"
#include "soc_runtime.h"

#include <stdint.h>
#include <stdio.h>

enum { ROWS = SOC_PLAN_ROWS, WIDTH = 8, DIVISOR = 4 };

static int32_t input[ROWS][WIDTH] __attribute__((aligned(64)));
static int32_t extra[ROWS][WIDTH] __attribute__((aligned(64)));
static int8_t output[ROWS][WIDTH] __attribute__((aligned(64)));

static void init_data(unsigned sample) {
  for (unsigned row = 0; row < ROWS; ++row)
    for (unsigned column = 0; column < WIDTH; ++column) {
      input[row][column] = (int32_t)((row * 233 + column * 107 + sample * 59) % 1201) - 200;
      extra[row][column] = (int32_t)((row * 71 + column * 139 + sample * 83) % 501) - 250;
      output[row][column] = -1;
    }
}

static int verify(void) {
  for (unsigned row = 0; row < ROWS; ++row)
    for (unsigned column = 0; column < WIDTH; ++column) {
      const int32_t positive = input[row][column] > 0 ? input[row][column] : 0;
      const int32_t sum = positive + extra[row][column];
      const int32_t scaled = sum > 0 ? (sum + DIVISOR / 2) / DIVISOR : 0;
      const int8_t expected = scaled > 127 ? 127 : scaled;
      if (output[row][column] != expected) {
        printf("Chain mismatch row=%u col=%u got=%d expected=%d\n", row, column, output[row][column], expected);
        return 1;
      }
    }
  return 0;
}

int main(void) {
  static int (*const execute[])(soc_context_t *) = {soc_execute_manual, soc_execute_hybrid, soc_execute_auto};
  soc_state_t states[SOC_PLAN_DEVICES] = {0};
  soc_context_t context = {.states = states};
  const uintptr_t arguments[] = {(uintptr_t)input, 0, (uintptr_t)extra, 0, (uintptr_t)output};
  const unsigned modes[] = {SOC_MANUAL, SOC_HYBRID, SOC_AUTO, SOC_MANUAL};
  for (unsigned trial = 0; trial < sizeof(modes) / sizeof(modes[0]); ++trial) {
    init_data(trial % 2);
    const soc_plan_t *plan = &soc_plans[modes[trial]];
    if (soc_prepare(&context, plan, arguments) || execute[modes[trial]](&context) || verify()) {
      printf("Chain mode=%u: FAIL\n", modes[trial]);
      return 1;
    }
  }
  puts("PASS");
  return 0;
}
