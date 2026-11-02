#include "internal/driver.h"

#include "gemmini_tile.h"

_Static_assert(DIM == 16 && ADDR_LEN == 32 && sizeof(elem_t) == 1 && sizeof(acc_t) == 4, "SoC plans require the INT8/INT32 Gemmini command ABI");
_Static_assert(MVIN_SCALE_IDENTITY == 0 && ACC_SCALE_IDENTITY == 0, "SoC plans require unscaled Gemmini transfers");

static gemmini_tile_t parameters(const soc_context_t *context, const soc_task_t *task) {
  const soc_gemmini_t *params = task->parameters;
  return (gemmini_tile_t){.rows = params->rows,
                          .tile_rows = params->tile_rows,
                          .k = params->k,
                          .n = params->n,
                          .input_row = params->input_row,
                          .weight_row = params->weight_row,
                          .input = (const elem_t *)soc_operand(context, params->input),
                          .weight = (const elem_t *)soc_operand(context, params->weight),
                          .output = (acc_t *)soc_operand(context, params->output),
                          .input_stride = params->input_stride,
                          .output_stride = params->output_stride,
                          .preload_input = params->preload_input,
                          .publish = params->publish,
                          .publish_row = params->publish_row,
                          .publish_stride = params->publish_stride,
                          .store = params->store};
}

int soc_gemmini_init(const soc_context_t *context, const soc_task_t *task) {
  const gemmini_tile_t params = parameters(context, task);
  gemmini_tile_init(*context->plan->devices[task->endpoint], &params);
  return 0;
}

int soc_gemmini_capture(const soc_context_t *context, const soc_task_t *task) {
  const gemmini_tile_t params = parameters(context, task);
  return gemmini_tile_capture(*context->plan->devices[task->endpoint], task->job, &params);
}

int soc_gemmini_execute(soc_context_t *context, const soc_task_t *task, unsigned tile) {
  const gemmini_tile_t params = parameters(context, task);
  gemmini_tile_run(*context->plan->devices[task->endpoint], &params, tile, 0);
  return 0;
}

const soc_driver_t soc_gemmini_driver = {.init = soc_gemmini_init, .capture = soc_gemmini_capture, .execute = soc_gemmini_execute};
