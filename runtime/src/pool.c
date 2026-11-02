#include "internal/driver.h"

#include "pool.h"

#include <stdio.h>

static int init(const soc_context_t *context, const soc_task_t *task) {
  const soc_pool_t *p = task->parameters;
  const uint64_t elements = (uint64_t)p->rows * p->columns * p->channels;
  if (context->plan->tiles != 1 || p->rows == 0 || p->columns == 0 || p->channels == 0 || p->rows > UINT16_MAX || p->columns > UINT16_MAX || p->channels > UINT16_MAX ||
      elements != p->input_elements || p->channels != p->output_elements) {
    printf("SoC runtime: Pool dimensions exceed the native complete-image interface\n");
    return 1;
  }
  const uint64_t groups = ((uint64_t)p->channels + p->lanes - 1) / p->lanes;
  if ((uint64_t)p->rows * p->columns * groups > p->line_entries) {
    printf("SoC runtime: Pool image exceeds line-buffer capacity\n");
    return 1;
  }
  return 0;
}

static int capture(const soc_context_t *context, const soc_task_t *task) {
  const soc_pool_t *p = task->parameters;
  accel_commands(*context->plan->devices[task->endpoint], {
    pool_config_tiled(0);
    pool_config_input(soc_operand(context, p->input), p->rows, p->columns, p->channels);
    pool_config_output(soc_operand(context, p->output));
    pool_config_window(POOL_MODE_AVERAGE, p->rows, p->columns, p->rows, p->columns, 0, 0);
  });
  return 0;
}

static int execute(soc_context_t *context, const soc_task_t *task, unsigned tile) {
  (void)tile;
  capture(context, task);
  uint32_t status;
  accel_commands(*context->plan->devices[task->endpoint], {
    pool_start();
    status = pool_wait();
  });
  if (status != POOL_STATUS_SUCCESS) {
    printf("SoC runtime: Pool status=%u\n", status);
    return 1;
  }
  return 0;
}

const soc_driver_t soc_pool_driver = {.init = init, .capture = capture, .execute = execute};
