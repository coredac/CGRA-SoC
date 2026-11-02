#include "generated/runtime_softmax.h"
#include "soc_runtime.h"

#include <stdint.h>
#include <stdio.h>

enum { ROWS = SOC_PLAN_ROWS, COLUMNS = 4 };

static int32_t scores[ROWS][COLUMNS] __attribute__((aligned(64)));
static int32_t maximum[ROWS] __attribute__((aligned(64)));
static int32_t exponent[ROWS][COLUMNS] __attribute__((aligned(64)));
static int32_t total[ROWS] __attribute__((aligned(64)));
static int8_t output[ROWS][COLUMNS] __attribute__((aligned(64)));

static int verify(void) {
  for (unsigned row = 0; row < ROWS; ++row) {
    int32_t peak = scores[row][0];
    for (unsigned column = 1; column <= row; ++column)
      if (scores[row][column] > peak)
        peak = scores[row][column];
    int32_t values[COLUMNS] = {0};
    int32_t sum = 0;
    for (unsigned column = 0; column <= row; ++column) {
      int32_t distance = (peak - scores[row][column]) >> 1;
      if (distance > 4096)
        distance = 4096;
      const unsigned shift = (distance * 370) >> 16;
      const int32_t polynomial = 346 - distance + 177 * shift;
      values[column] = (polynomial * polynomial + 62885) >> (shift + 2);
      sum += values[column];
    }
    if (maximum[row] != peak || total[row] != sum) {
      printf("Softmax summary mismatch row=%u max=%d/%d sum=%d/%d\n", row, maximum[row], peak, total[row], sum);
      return 1;
    }
    for (unsigned column = 0; column < COLUMNS; ++column) {
      const int32_t expected = (values[column] * 127 + sum / 2) / sum;
      if (exponent[row][column] != values[column] || output[row][column] != expected) {
        printf("Softmax mismatch row=%u column=%u exp=%d/%d output=%d/%d\n", row, column, exponent[row][column], values[column], output[row][column], expected);
        return 1;
      }
    }
  }
  return 0;
}

int main(void) {
  static int (*const execute[])(soc_context_t *) = {soc_execute_manual, soc_execute_hybrid, soc_execute_auto};
  soc_state_t states[SOC_PLAN_DEVICES] = {0};
  soc_context_t context = {.states = states};
  const unsigned modes[] = {SOC_MANUAL, SOC_HYBRID, SOC_AUTO, SOC_MANUAL};
  const uintptr_t arguments[] = {(uintptr_t)scores, (uintptr_t)maximum, (uintptr_t)exponent, (uintptr_t)total, (uintptr_t)output};
  for (unsigned trial = 0; trial < sizeof(modes) / sizeof(modes[0]); ++trial) {
    const unsigned sample = trial % 2;
    for (unsigned row = 0; row < ROWS; ++row) {
      maximum[row] = total[row] = -1;
      for (unsigned column = 0; column < COLUMNS; ++column) {
        scores[row][column] = ((int)((row * 13 + column * 7 + sample * 11) % 31) - 15) * 257;
        exponent[row][column] = -1;
        output[row][column] = -1;
      }
    }
    if (soc_prepare(&context, &soc_plans[modes[trial]], arguments) || execute[modes[trial]](&context) || verify()) {
      printf("Softmax mode=%u sample=%u: FAIL\n", modes[trial], sample);
      return 1;
    }
  }
  puts("PASS");
  return 0;
}
