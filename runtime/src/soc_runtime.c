#include "internal/driver.h"

#include "auto_link.h"
#include "cgra_link.h"

#include <stdio.h>

static unsigned long cycles(void) {
  unsigned long value;
  __asm__ volatile("rdcycle %0" : "=r"(value)::"memory");
  return value;
}

static void capture_step(const soc_context_t *context, const soc_step_t *step) {
  uintptr_t bindings[context->plan->device_count];
  for (unsigned index = 0; index < context->plan->device_count; ++index) {
    bindings[index] = context->plan->devices[index]->spm;
  }
  auto_link_load(step->graph, bindings);
  auto_link_tiles(step->tiles, 1, 1, 1);
  auto_link_run_config(step->profile);
}

static int prepare(const soc_context_t *context, const soc_prep_t *operation) {
  const soc_plan_t *plan = context->plan;
  switch (operation->kind) {
  case SOC_INIT: {
    const soc_task_t *task = &plan->tasks[operation->index];
    return task->driver->init(context, task);
  }
  case SOC_CAPTURE: {
    const soc_task_t *task = &plan->tasks[operation->index];
    return task->driver->capture(context, task);
  }
  case SOC_PROFILE:
    capture_step(context, &plan->steps[operation->index]);
    return 0;
  }
  __builtin_unreachable();
}

int soc_prepare(soc_context_t *context, const soc_plan_t *plan, const uintptr_t *arguments) {
  const unsigned long begin = cycles();
  context->plan = plan;
  context->arguments = arguments;
  context->stats = (soc_stats_t){0};
  for (unsigned index = 0; index < plan->prep_count; ++index) {
    const int status = prepare(context, &plan->prep[index]);
    if (status != 0) {
      printf("SoC runtime: preparation failed operation=%u\n", index);
      return status;
    }
  }
  __asm__ volatile("fence iorw, iorw" ::: "memory");
  context->stats.prepare_cycles = cycles() - begin;
  context->stats.total_cycles = context->stats.prepare_cycles;
  return 0;
}

static int cpu_step(soc_context_t *context, const soc_step_t *step) {
  for (unsigned tile = 0; tile < step->tiles; ++tile) {
    for (unsigned index = 0; index < step->task_count; ++index) {
      const soc_task_t *task = &context->plan->tasks[step->tasks[index]];
      ++context->stats.cpu_task_submissions;
      const int status = task->driver->execute(context, task, tile);
      if (status != 0) {
        return status;
      }
      __asm__ volatile("fence iorw, iorw" ::: "memory");
    }
  }
  return 0;
}

static int automatic_step(soc_context_t *context, const soc_step_t *step) {
  const uintptr_t collector = context->plan->devices[step->collector]->control;
  const uint32_t expected = UINT32_MAX >> (32 - step->graph->stage_count);
  uint32_t seen = 0;
  int failures = 0;
  ++context->stats.automatic_launches;
  auto_link_run(step->profile, 1);
  for (;;) {
    if (cgra_link_read_at(collector, CGRA_LINK_CONTROL_RESULT_VALID)) {
      const cgra_link_result_t result = cgra_link_wait_at(collector);
      const uint32_t bit = result.stage < step->graph->stage_count ? UINT32_C(1) << result.stage : 0;
      if (bit == 0 || (seen & bit) != 0 || result.status != AUTO_LINK_STATUS_SUCCESS || result.detail != 0 || result.data != 0 || result.job != step->graph->stages[result.stage].job) {
        printf("SoC runtime: AutoLink result mismatch stage=%u job=%u status=%u detail=%u data=%u\n", result.stage, result.job, result.status, result.detail, result.data);
        ++failures;
      } else {
        const soc_task_t *task = &context->plan->tasks[step->tasks[result.stage]];
        if (task->driver->complete) {
          task->driver->complete(context, task);
        }
      }
      seen |= bit;
      continue;
    }
    if (*(volatile uint32_t *)(AUTO_LINK_BASE + AUTO_LINK_RUNNING) == 0 && cgra_link_read_at(collector, CGRA_LINK_CONTROL_RESULT_VALID) == 0) {
      break;
    }
  }
  __asm__ volatile("fence iorw, iorw" ::: "memory");
  if (seen != expected) {
    printf("SoC runtime: incomplete results seen=%u expected=%u\n", seen, expected);
    ++failures;
  }
  return failures != 0;
}

static int finish(soc_context_t *context, unsigned long begin, int status) {
  __asm__ volatile("fence iorw, iorw" ::: "memory");
  for (unsigned index = 0; status == 0 && index < context->plan->output_count; ++index) {
    status = soc_store(context, &context->plan->outputs[index]);
  }
  __asm__ volatile("fence iorw, iorw" ::: "memory");
  context->stats.execute_cycles = cycles() - begin;
  context->stats.total_cycles = context->stats.prepare_cycles + context->stats.execute_cycles;
  return status;
}

int soc_execute_manual(soc_context_t *context) {
  const unsigned long begin = cycles();
  int status = 0;
  for (unsigned index = 0; status == 0 && index < context->plan->step_count; ++index) {
    status = cpu_step(context, &context->plan->steps[index]);
  }
  return finish(context, begin, status);
}

int soc_execute_auto(soc_context_t *context) {
  const unsigned long begin = cycles();
  int status = 0;
  for (unsigned index = 0; status == 0 && index < context->plan->step_count; ++index) {
    status = automatic_step(context, &context->plan->steps[index]);
  }
  return finish(context, begin, status);
}

int soc_execute_hybrid(soc_context_t *context) {
  const unsigned long begin = cycles();
  int status = 0;
  for (unsigned index = 0; status == 0 && index < context->plan->step_count; ++index) {
    const soc_step_t *step = &context->plan->steps[index];
    status = step->automatic ? automatic_step(context, step) : cpu_step(context, step);
  }
  return finish(context, begin, status);
}
