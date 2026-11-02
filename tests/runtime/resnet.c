#include "generated/runtime_resnet.h"
#include "generated/runtime_resnet_data.h"
#include "soc_runtime.h"

#include <stdint.h>
#include <stdio.h>

static uint8_t workspace[SOC_PLAN_WORKSPACE_BYTES] __attribute__((aligned(SOC_PLAN_ALIGNMENT)));

static int32_t output[RESNET_OUTPUT_WORDS] __attribute__((aligned(RESNET_ALIGNMENT)));

static int verify(unsigned sample) {
  for (unsigned index = 0; index < RESNET_OUTPUTS; ++index) {
    if (output[index] != resnet_expected[sample][index]) {
      printf("ResNet mismatch sample=%u class=%u actual=%d expected=%d\n", sample, index, output[index], resnet_expected[sample][index]);
      return 1;
    }
  }
  return 0;
}

int main(void) {
  static int (*const execute[])(soc_context_t *) = {soc_execute_manual, soc_execute_hybrid, soc_execute_auto};
  soc_state_t states[SOC_PLAN_DEVICES] = {0};
  soc_context_t context = {.states = states, .workspace = (uintptr_t)workspace, .workspace_bytes = sizeof(workspace)};
  uintptr_t arguments[RESNET_ARGUMENTS];
  const unsigned modes[] = {SOC_MANUAL, SOC_HYBRID, SOC_AUTO, SOC_MANUAL};
  for (unsigned trial = 0; trial < sizeof(modes) / sizeof(modes[0]); ++trial) {
    for (unsigned sample = 0; sample < RESNET_SAMPLES; ++sample) {
      for (unsigned index = 0; index < RESNET_OUTPUT_WORDS; ++index)
        output[index] = -1;
      resnet_arguments(arguments, sample, output);
      if (soc_prepare(&context, &soc_plans[modes[trial]], arguments) || execute[modes[trial]](&context) || verify(sample)) {
        printf("ResNet mode=%u sample=%u: FAIL\n", modes[trial], sample);
        return 1;
      }
      if (modes[trial] == SOC_AUTO && (context.stats.automatic_launches != SOC_PLAN_AUTO_LAUNCHES || context.stats.cpu_task_submissions != 0)) {
        puts("Auto execution did not follow the compiled launch plan");
        return 1;
      }
    }
  }
  puts("PASS");
  return 0;
}
