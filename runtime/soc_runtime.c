#include "driver.h"

#include "auto_link.h"

#include <stdio.h>

static unsigned long cycles(void) {
  unsigned long value;
  __asm__ volatile("rdcycle %0" : "=r"(value)::"memory");
  return value;
}

static int automatic_task(const soc_plan_t *plan, unsigned task) {
  for (unsigned index = 0; index < plan->step_count; ++index) {
    const soc_step_t *step = &plan->steps[index];
    if (!step->automatic) {
      continue;
    }
    for (unsigned entry = 0; entry < step->task_count; ++entry) {
      if (step->tasks[entry] == task) {
        return 1;
      }
    }
  }
  return 0;
}

static void capture_step(const soc_context_t *context, const soc_step_t *step) {
  uintptr_t bindings[context->plan->device_count];
  for (unsigned index = 0; index < context->plan->device_count; ++index) {
    bindings[index] = context->plan->devices[index]->spm;
  }
  auto_link_load(step->graph, bindings);
  for (unsigned index = 0; index < step->graph->edge_count; ++index) {
    const auto_link_edge_t *edge = &step->graph->edges[index];
    auto_link_transfer(index, edge->source_offset, edge->destination_offset, edge->source_stride, step->destination_strides[index], edge->pixel_bytes);
  }
  auto_link_tiles(step->tiles, 1, 1, 1);
  auto_link_run_config(step->profile);
}

int soc_prepare(soc_context_t *context, const soc_plan_t *plan, const uintptr_t *arguments) {
  const unsigned long begin = cycles();
  context->plan = plan;
  context->arguments = arguments;
  context->stats = (soc_stats_t){0};
  for (unsigned index = 0; index < plan->device_count; ++index) {
    context->states[index].last = NULL;
  }
  for (unsigned index = 0; index < plan->task_count; ++index) {
    const soc_task_t *task = &plan->tasks[index];
    if (task->kind == SOC_GEMMINI) {
      soc_gemmini_init(context, task);
    }
  }
  __asm__ volatile("fence iorw, iorw" ::: "memory");
  for (unsigned index = 0; index < plan->task_count; ++index) {
    const soc_task_t *task = &plan->tasks[index];
    if (!automatic_task(plan, index)) {
      if (task->kind == SOC_CGRA) {
        soc_cgra_load(context, task);
      }
      continue;
    }
    const int status = task->kind == SOC_GEMMINI ? soc_gemmini_capture(context, task) : soc_cgra_capture(context, task);
    if (status != 0) {
      printf("SoC runtime: capture failed task=%u\n", index);
      return status;
    }
  }
  for (unsigned index = 0; index < plan->step_count; ++index) {
    if (plan->steps[index].automatic) {
      capture_step(context, &plan->steps[index]);
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
      if (task->kind == SOC_CGRA) {
        if (soc_cgra_execute(context, task, tile) != 0) {
          return 1;
        }
      } else {
        soc_gemmini_execute(context, task, tile);
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
        if (task->kind == SOC_CGRA) {
          context->states[task->endpoint].started = 1;
          context->states[task->endpoint].last = NULL;
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

int soc_execute(soc_context_t *context) {
  const unsigned long begin = cycles();
  int status = 0;
  for (unsigned index = 0; index < context->plan->step_count; ++index) {
    const soc_step_t *step = &context->plan->steps[index];
    status = step->automatic ? automatic_step(context, step) : cpu_step(context, step);
    if (status != 0) {
      break;
    }
  }
  __asm__ volatile("fence iorw, iorw" ::: "memory");
  context->stats.execute_cycles = cycles() - begin;
  context->stats.total_cycles = context->stats.prepare_cycles + context->stats.execute_cycles;
  return status;
}
