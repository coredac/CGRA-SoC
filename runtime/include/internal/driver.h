#ifndef SOC_DRIVER_H
#define SOC_DRIVER_H

#include "accel.h"
#include "soc_runtime.h"

static inline uint64_t soc_operand(const soc_context_t *context, soc_operand_t operand) {
  switch (operand.base) {
  case SOC_LITERAL:
    return operand.value;
  case SOC_ARGUMENT:
    return context->arguments[operand.index] + operand.value;
  case SOC_SPM:
    return context->plan->devices[operand.index]->spm + operand.value;
  }
  __builtin_unreachable();
}

int soc_gemmini_init(const soc_context_t *context, const soc_task_t *task);
int soc_gemmini_capture(const soc_context_t *context, const soc_task_t *task);
int soc_gemmini_execute(soc_context_t *context, const soc_task_t *task, unsigned tile);

int soc_cgra_init(const soc_context_t *context, const soc_task_t *task);
int soc_cgra_capture(const soc_context_t *context, const soc_task_t *task);
int soc_cgra_execute(soc_context_t *context, const soc_task_t *task, unsigned tile);
int soc_cgra_store(const soc_context_t *context, const soc_output_t *output);

int soc_store(const soc_context_t *context, const soc_output_t *output);

#endif
