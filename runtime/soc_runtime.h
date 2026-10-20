#ifndef SOC_RUNTIME_H
#define SOC_RUNTIME_H

#include "soc_plan.h"

typedef struct {
  unsigned long prepare_cycles;
  unsigned long execute_cycles;
  unsigned long total_cycles;
  unsigned automatic_launches;
  unsigned cpu_task_submissions;
} soc_stats_t;

typedef struct {
  unsigned started;
  const soc_task_t *last;
} soc_state_t;

typedef struct {
  const soc_plan_t *plan;
  const uintptr_t *arguments;
  soc_state_t *states;
  soc_stats_t stats;
} soc_context_t;

/* Supply one zero-initialized state per device; retain it across calls and modes. */
int soc_prepare(soc_context_t *context, const soc_plan_t *plan, const uintptr_t *arguments);
int soc_execute(soc_context_t *context);

#endif
