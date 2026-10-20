#ifndef SOC_RUNTIME_H
#define SOC_RUNTIME_H

#include "soc_plan.h"

#include <stdint.h>

typedef struct {
  unsigned long prepare_cycles;
  unsigned long execute_cycles;
  unsigned long total_cycles;
  unsigned automatic_launches;
  unsigned cpu_task_submissions;
} soc_stats_t;

typedef struct {
  const soc_plan_t *plan;
  int32_t *output;
  soc_stats_t stats;
  unsigned cgra_started;
} soc_context_t;

/* Zero-initialize once and retain the context across modes and inputs. */
int soc_prepare(soc_context_t *context, const soc_plan_t *plan, const int8_t *input, const int8_t *weight0, const int8_t *weight1, int32_t *output);
int soc_execute(soc_context_t *context);

#endif
