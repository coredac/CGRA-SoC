#include "internal/driver.h"

#include "gemmini_matrix.h"

_Static_assert(sizeof(elem_t) == 1 && sizeof(acc_t) == 4, "Matrix plans require the INT8/INT32 Gemmini ABI");

static gemmini_matrix_t parameters(const soc_context_t *context, const soc_task_t *task) {
  const soc_matrix_t *params = task->parameters;
  return (gemmini_matrix_t){.m = params->m,
                            .k = params->k,
                            .n = params->n,
                            .transpose_a = params->transpose_a,
                            .transpose_b = params->transpose_b,
                            .accumulate = params->accumulate,
                            .store = params->store,
                            .input_stride = params->input_stride,
                            .weight_stride = params->weight_stride,
                            .output_stride = params->output_stride,
                            .input_elements = params->input_elements,
                            .weight_elements = params->weight_elements,
                            .bias_elements = params->bias_elements,
                            .output_elements = params->output_elements,
                            .input = (const elem_t *)soc_operand(context, params->input),
                            .weight = (const elem_t *)soc_operand(context, params->weight),
                            .bias = (const acc_t *)soc_operand(context, params->bias),
                            .output = (acc_t *)soc_operand(context, params->output)};
}

static int initialize(const soc_context_t *context, const soc_task_t *task) {
  const gemmini_matrix_t params = parameters(context, task);
  return gemmini_matrix_check(&params);
}

static int capture(const soc_context_t *context, const soc_task_t *task) {
  const gemmini_matrix_t params = parameters(context, task);
  return gemmini_matrix_capture(*context->plan->devices[task->endpoint], task->job, &params);
}

static int execute(soc_context_t *context, const soc_task_t *task, unsigned tile) {
  (void)tile;
  const gemmini_matrix_t params = parameters(context, task);
  return gemmini_matrix_run(*context->plan->devices[task->endpoint], &params);
}

const soc_driver_t soc_matrix_driver = {.init = initialize, .capture = capture, .execute = execute};
