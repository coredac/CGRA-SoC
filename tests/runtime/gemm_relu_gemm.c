#include "generated/runtime_plan.h"
#include "soc_runtime.h"

#include <stdint.h>
#include <stdio.h>

enum { ROWS = SOC_PLAN_ROWS, DIM = 16 };

static int8_t input[ROWS][DIM] __attribute__((aligned(64)));
static int8_t weights[2][DIM][DIM] __attribute__((aligned(64)));
static int32_t output[ROWS][DIM] __attribute__((aligned(64)));
static int32_t expected[ROWS][DIM];

static void init_data(unsigned rows, unsigned sample) {
  for (unsigned row = 0; row < DIM; ++row) {
    for (unsigned column = 0; column < DIM; ++column) {
      weights[0][row][column] = (int8_t)((int)((row * 3 + column + sample) % 5) - 2);
      weights[1][row][column] = (int8_t)((int)((row + column * 3 + sample) % 3) - 1);
    }
  }
  for (unsigned row = 0; row < rows; ++row) {
    for (unsigned column = 0; column < DIM; ++column) {
      input[row][column] = (int8_t)((int)((row * 17 + column * 7 + sample * 29) % 101) - 50);
      output[row][column] = INT32_C(0x5a5a5a5a);
    }
  }
}

static void reference(unsigned rows) {
  for (unsigned row = 0; row < rows; ++row) {
    int8_t activated[DIM];
    for (unsigned column = 0; column < DIM; ++column) {
      int32_t sum = 0;
      for (unsigned k = 0; k < DIM; ++k) {
        sum += input[row][k] * weights[0][k][column];
      }
      activated[column] = sum < 0 ? 0 : sum > 127 ? 127 : (int8_t)sum;
    }
    for (unsigned column = 0; column < DIM; ++column) {
      int32_t sum = 0;
      for (unsigned k = 0; k < DIM; ++k) {
        sum += activated[k] * weights[1][k][column];
      }
      expected[row][column] = sum;
    }
  }
}

static int verify(unsigned rows) {
  for (unsigned row = 0; row < rows; ++row) {
    for (unsigned column = 0; column < DIM; ++column) {
      if (output[row][column] != expected[row][column]) {
        printf("Mismatch row=%u col=%u got=%d expected=%d\n", row, column, (int)output[row][column], (int)expected[row][column]);
        return 1;
      }
    }
  }
  return 0;
}

static int check_schedule(const soc_plan_t *plan, const soc_context_t *ctx) {
  const unsigned launches = plan->mode == SOC_MANUAL ? 0 : 1;
  const unsigned submissions = plan->mode == SOC_MANUAL ? 3 * plan->tiles : plan->mode == SOC_HYBRID ? plan->tiles : 0;
  if (ctx->stats.automatic_launches != launches || ctx->stats.cpu_task_submissions != submissions) {
    printf("Schedule mismatch: launches=%u CPU tasks=%u\n", ctx->stats.automatic_launches, ctx->stats.cpu_task_submissions);
    return 1;
  }
  return 0;
}

int main(void) {
  static const char *names[] = {"manual", "hybrid", "auto"};
  static int (*const execute[])(soc_context_t *) = {soc_execute_manual, soc_execute_hybrid, soc_execute_auto};
  soc_state_t states[SOC_PLAN_DEVICES] = {0};
  soc_context_t ctx = {.states = states};
  const uintptr_t arguments[] = {(uintptr_t)input, (uintptr_t)weights[0], 0, 0, (uintptr_t)weights[1], (uintptr_t)output};
  /* End in Manual to exercise return from automatic to native commands. */
  static const unsigned modes[] = {SOC_MANUAL, SOC_HYBRID, SOC_AUTO, SOC_MANUAL};
  for (unsigned trial = 0; trial < sizeof(modes) / sizeof(modes[0]); ++trial) {
    const soc_plan_t *plan = &soc_plans[modes[trial]];
    const unsigned rows = ROWS;
    for (unsigned sample = 0; sample < 2; ++sample) {
      init_data(rows, sample);
      reference(rows);
      if (soc_prepare(&ctx, plan, arguments) || execute[modes[trial]](&ctx) || verify(rows) || check_schedule(plan, &ctx)) {
        printf("Runtime %s input=%u: FAIL\n", names[plan->mode], sample);
        return 1;
      }
      printf("Runtime %s input=%u: PASS prepare=%lu execute=%lu total=%lu auto_launches=%u CPU_tasks=%u\n", names[plan->mode], sample, ctx.stats.prepare_cycles, ctx.stats.execute_cycles,
             ctx.stats.total_cycles, ctx.stats.automatic_launches, ctx.stats.cpu_task_submissions);
    }
  }
  puts("Runtime GEMM-ReLU-GEMM: PASS");
  return 0;
}
