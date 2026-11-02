#include "internal/driver.h"

#include "gemmini_conv.h"

#include <stdio.h>

static gemmini_conv_t parameters(const soc_context_t *context, const soc_task_t *task) {
  const soc_conv_t *p = task->parameters;
  return (gemmini_conv_t){.input_rows = p->input_rows,
                          .input_columns = p->input_columns,
                          .input_channels = p->input_channels,
                          .output_rows = p->output_rows,
                          .output_columns = p->output_columns,
                          .output_channels = p->output_channels,
                          .kernel = p->kernel,
                          .stride = p->stride,
                          .dilation = p->dilation,
                          .padding_top = p->padding_top,
                          .padding_left = p->padding_left,
                          .input = (const elem_t *)soc_operand(context, p->input),
                          .weight = (const elem_t *)soc_operand(context, p->weight),
                          .output = (elem_t *)soc_operand(context, p->output),
                          .activation = NO_ACTIVATION,
                          .scale = ACC_SCALE_IDENTITY};
}

static int init(const soc_context_t *context, const soc_task_t *task) {
  const soc_conv_t *p = task->parameters;
  const gemmini_conv_t native = parameters(context, task);
  if (context->plan->tiles != 1 || !gemmini_conv_valid(&native, p->input_elements, p->weight_elements, p->output_elements)) {
    printf("SoC runtime: Conv dimensions or workspace exceed the native complete-image interface\n");
    return 1;
  }
  accel_commands(*context->plan->devices[task->endpoint], {
    gemmini_flush(0);
    gemmini_fence();
  });
  return 0;
}

static int capture(const soc_context_t *context, const soc_task_t *task) {
  const gemmini_conv_t native = parameters(context, task);
  const accel_t device = *context->plan->devices[task->endpoint];
  int status = 0;
  accel_commands(device, { status = gemmini_conv_capture(device.control, task->job, &native); });
  return status;
}

static int execute(soc_context_t *context, const soc_task_t *task, unsigned tile) {
  (void)tile;
  const gemmini_conv_t native = parameters(context, task);
  accel_commands(*context->plan->devices[task->endpoint], { gemmini_conv_emit(&native); });
  return 0;
}

const soc_driver_t soc_conv_driver = {.init = init, .capture = capture, .execute = execute};
