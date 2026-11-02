#include "internal/driver.h"

#include "gemmini_copy.h"

static gemmini_copy_t parameters(const soc_context_t *context, const soc_task_t *task) {
  const soc_copy_t *params = task->parameters;
  return (gemmini_copy_t){.rows = params->rows,
                          .columns = params->columns,
                          .element_bytes = params->element_bytes,
                          .source_stride = params->source_stride,
                          .destination_stride = params->destination_stride,
                          .source = (const void *)soc_operand(context, params->source),
                          .destination = (void *)soc_operand(context, params->destination)};
}

static int initialize(const soc_context_t *context, const soc_task_t *task) {
  const gemmini_copy_t params = parameters(context, task);
  return gemmini_copy_check(&params);
}

static int capture(const soc_context_t *context, const soc_task_t *task) {
  const gemmini_copy_t params = parameters(context, task);
  return gemmini_copy_capture(*context->plan->devices[task->endpoint], task->job, &params);
}

static int execute(soc_context_t *context, const soc_task_t *task, unsigned tile) {
  (void)tile;
  const gemmini_copy_t params = parameters(context, task);
  return gemmini_copy_run(*context->plan->devices[task->endpoint], &params);
}

const soc_driver_t soc_copy_driver = {.init = initialize, .capture = capture, .execute = execute};
