#include "generated/runtime_plan.h"
#include "soc_runtime.h"

#include <stdint.h>
#include <stdio.h>

enum { MAX_TILES = 8, MAX_ROWS = MAX_TILES * SOC_ROWS };

static int8_t input[MAX_ROWS][SOC_DIM] __attribute__((aligned(64)));
static int8_t weights[2][SOC_DIM][SOC_DIM] __attribute__((aligned(64)));
static int32_t output[MAX_ROWS][SOC_DIM] __attribute__((aligned(64)));
static int32_t expected[MAX_ROWS][SOC_DIM];

static void init_data(unsigned rows, unsigned sample) {
  for (unsigned row = 0; row < SOC_DIM; ++row) {
    for (unsigned column = 0; column < SOC_DIM; ++column) {
      weights[0][row][column] = (int8_t)((int)((row * 3 + column + sample) % 5) - 2);
      weights[1][row][column] = (int8_t)((int)((row + column * 3 + sample) % 3) - 1);
    }
  }
  for (unsigned row = 0; row < rows; ++row) {
    for (unsigned column = 0; column < SOC_DIM; ++column) {
      input[row][column] = (int8_t)((int)((row * 17 + column * 7 + sample * 29) % 101) - 50);
      output[row][column] = INT32_C(0x5a5a5a5a);
    }
  }
}

static void reference(unsigned rows) {
  for (unsigned row = 0; row < rows; ++row) {
    int8_t activated[SOC_DIM];
    for (unsigned column = 0; column < SOC_DIM; ++column) {
      int32_t sum = 0;
      for (unsigned k = 0; k < SOC_DIM; ++k) {
        sum += input[row][k] * weights[0][k][column];
      }
      activated[column] = sum < 0 ? 0 : sum > 127 ? 127 : (int8_t)sum;
    }
    for (unsigned column = 0; column < SOC_DIM; ++column) {
      int32_t sum = 0;
      for (unsigned k = 0; k < SOC_DIM; ++k) {
        sum += activated[k] * weights[1][k][column];
      }
      expected[row][column] = sum;
    }
  }
}

static int verify(unsigned rows) {
  for (unsigned row = 0; row < rows; ++row) {
    for (unsigned column = 0; column < SOC_DIM; ++column) {
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
  soc_context_t ctx = {0};
  /* End in Manual to exercise return from automatic to native commands. */
  static const unsigned modes[] = {SOC_MANUAL, SOC_HYBRID, SOC_AUTO, SOC_MANUAL};
  for (unsigned trial = 0; trial < sizeof(modes) / sizeof(modes[0]); ++trial) {
    const soc_plan_t *plan = &soc_plans[modes[trial]];
    const unsigned rows = plan->tiles * SOC_ROWS;
    for (unsigned sample = 0; sample < 2; ++sample) {
      init_data(rows, sample);
      reference(rows);
      if (soc_prepare(&ctx, plan, &input[0][0], &weights[0][0][0], &weights[1][0][0], &output[0][0]) || soc_execute(&ctx) || verify(rows) || check_schedule(plan, &ctx)) {
        printf("Runtime %s input=%u: FAIL\n", names[plan->mode], sample);
        return 1;
      }
      printf("Runtime %s input=%u: PASS prepare=%lu execute=%lu total=%lu auto_launches=%u CPU_tasks=%u\n", names[plan->mode], sample, ctx.stats.prepare_cycles, ctx.stats.execute_cycles,
             ctx.stats.total_cycles, ctx.stats.automatic_launches, ctx.stats.cpu_task_submissions);
    }
  }
  puts("Runtime pipeline: PASS");
  return 0;
}
