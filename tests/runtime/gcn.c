#include "generated/runtime_gcn.h"
#include "generated/runtime_gcn_data.h"
#include "soc_runtime.h"

#include <stdint.h>
#include <stdio.h>

static uint8_t workspace[SOC_PLAN_WORKSPACE_BYTES] __attribute__((aligned(SOC_PLAN_ALIGNMENT)));

static int8_t input[GCN_NODES][GCN_INPUTS] __attribute__((aligned(64)));
static int8_t weight[GCN_INPUTS][GCN_OUTPUTS] __attribute__((aligned(64)));
static int32_t output[GCN_NODES][GCN_OUTPUTS] __attribute__((aligned(64)));

static void init_data(unsigned sample) {
  for (unsigned row = 0; row < GCN_NODES; ++row)
    for (unsigned column = 0; column < GCN_INPUTS; ++column)
      input[row][column] = (int)((row * 7 + column * 3 + sample * 5) % 11) - 5;
  for (unsigned row = 0; row < GCN_INPUTS; ++row)
    for (unsigned column = 0; column < GCN_OUTPUTS; ++column)
      weight[row][column] = (int)((row * 3 + column * 5 + sample) % 7) - 3;
  for (unsigned row = 0; row < GCN_NODES; ++row)
    for (unsigned column = 0; column < GCN_OUTPUTS; ++column)
      output[row][column] = -1;
}

static int verify(void) {
  for (unsigned row = 0; row < GCN_NODES; ++row)
    for (unsigned column = 0; column < GCN_OUTPUTS; ++column) {
      uint32_t sum = 0;
      for (unsigned edge = 0; edge < gcn_degrees[row]; ++edge) {
        const unsigned neighbor = gcn_neighbors[row][edge];
        int32_t projected = 0;
        for (unsigned k = 0; k < GCN_INPUTS; ++k)
          projected += input[neighbor][k] * weight[k][column];
        sum += (uint32_t)projected * (uint32_t)gcn_edges[row][edge];
      }
      const int32_t value = (int32_t)sum;
      const int32_t expected = value > 0 ? value : 0;
      if (output[row][column] != expected) {
        printf("GCN mismatch row=%u col=%u got=%d expected=%d\n", row, column, output[row][column], expected);
        return 1;
      }
    }
  return 0;
}

int main(void) {
  static int (*const execute[])(soc_context_t *) = {soc_execute_manual, soc_execute_hybrid, soc_execute_auto};
  soc_state_t states[SOC_PLAN_DEVICES] = {0};
  soc_context_t context = {.states = states, .workspace = (uintptr_t)workspace, .workspace_bytes = sizeof(workspace)};
  uintptr_t arguments[GCN_ARGUMENTS];
  gcn_arguments(arguments, &input[0][0], &weight[0][0], &output[0][0]);
  const unsigned modes[] = {SOC_MANUAL, SOC_HYBRID, SOC_AUTO, SOC_MANUAL};
  for (unsigned trial = 0; trial < sizeof(modes) / sizeof(modes[0]); ++trial) {
    init_data(trial % 2);
    if (soc_prepare(&context, &soc_plans[modes[trial]], arguments) || execute[modes[trial]](&context) || verify()) {
      printf("GCN mode=%u: FAIL\n", modes[trial]);
      return 1;
    }
    if (modes[trial] == SOC_AUTO && (context.stats.automatic_launches != SOC_PLAN_AUTO_LAUNCHES || context.stats.cpu_task_submissions != 0)) {
      puts("Auto execution did not follow the compiled launch plan");
      return 1;
    }
  }
  puts("PASS");
  return 0;
}
