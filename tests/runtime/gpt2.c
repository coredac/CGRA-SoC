#include "generated/runtime_gpt2.h"
#include "generated/runtime_gpt2_data.h"
#include "soc_runtime.h"

#include <stdint.h>
#include <stdio.h>

static uint8_t workspace[SOC_PLAN_WORKSPACE_BYTES] __attribute__((aligned(SOC_PLAN_ALIGNMENT)));

static int8_t output[GPT2_OUTPUT_BYTES] __attribute__((aligned(GPT2_DMA_BYTES)));

static int verify(unsigned sample) {
  for (unsigned index = 0; index < GPT2_OUTPUTS; ++index) {
    if (output[index] != gpt2_expected[sample][index]) {
      printf("GPT2 mismatch sample=%u index=%u actual=%d expected=%d\n", sample, index, output[index], gpt2_expected[sample][index]);
      return 1;
    }
  }
  return 0;
}

int main(void) {
  static int (*const execute[])(soc_context_t *) = {soc_execute_manual, soc_execute_hybrid, soc_execute_auto};
  soc_state_t states[SOC_PLAN_DEVICES] = {0};
  soc_context_t context = {.states = states, .workspace = (uintptr_t)workspace, .workspace_bytes = sizeof(workspace)};
  uintptr_t arguments[GPT2_ARGUMENTS];
  const unsigned modes[] = {SOC_MANUAL, SOC_HYBRID, SOC_AUTO, SOC_MANUAL};
  for (unsigned trial = 0; trial < sizeof(modes) / sizeof(modes[0]); ++trial) {
    for (unsigned sample = 0; sample < GPT2_SAMPLES; ++sample) {
      for (unsigned index = 0; index < GPT2_OUTPUT_BYTES; ++index)
        output[index] = 101;
      gpt2_arguments(arguments, sample, output);
      if (soc_prepare(&context, &soc_plans[modes[trial]], arguments) || execute[modes[trial]](&context) || verify(sample)) {
        printf("GPT2 mode=%u sample=%u: FAIL\n", modes[trial], sample);
        return 1;
      }
      if (modes[trial] == SOC_AUTO && (context.stats.automatic_launches != SOC_PLAN_AUTO_LAUNCHES || context.stats.cpu_task_submissions != 0)) {
        puts("Auto execution did not follow the compiled launch plan");
        return 1;
      }
      printf("GPT2 mode=%u sample=%u: PASS cycles=%lu launches=%u cpu_tasks=%u\n", modes[trial], sample, context.stats.total_cycles, context.stats.automatic_launches,
             context.stats.cpu_task_submissions);
    }
  }
  puts("PASS");
  return 0;
}
