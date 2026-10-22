#include "internal/driver.h"

#include "cgra_dma.h"
#include "cgra_link.h"

#include <stdio.h>

_Static_assert(DATA_PAYLOAD_NBITS == 32, "CGRA relocations require a 32-bit payload");

static void payload(cgra_packet_t *packet, uint32_t value) {
  uint64_t words[] = {packet->lo, packet->mid, packet->hi, packet->top};
  const unsigned index = (PKT_DATA_LSB + DATA_PAYLOAD_LSB) / 64;
  const unsigned shift = (PKT_DATA_LSB + DATA_PAYLOAD_LSB) % 64;
  const uint64_t mask = UINT64_C(0xffffffff) << shift;
  words[index] = (words[index] & ~mask) | ((uint64_t)value << shift);
  if (shift > 32) {
    const uint64_t upper = UINT64_C(0xffffffff) >> (64 - shift);
    words[index + 1] = (words[index + 1] & ~upper) | ((uint64_t)value >> (64 - shift));
  }
  *packet = (cgra_packet_t){words[0], words[1], words[2], words[3]};
}

static uint32_t symbol(cgra_link_symbol_t binding, unsigned tile, unsigned elements) {
  switch (binding.source) {
  case CGRA_LINK_ELEMENTS:
    return elements;
  case CGRA_LINK_SLOT:
    return binding.base;
  case CGRA_LINK_TILE_ID:
    return binding.base + tile * binding.stride;
  }
  __builtin_unreachable();
}

static void send(const soc_task_t *task, unsigned tile, const cgra_packet_t *packets, unsigned first, unsigned count, unsigned *cursor) {
  const cgra_kernel_t *kernel = task->kernel;
  while (*cursor < kernel->patch_count && kernel->patches[*cursor].packet_index < first) {
    ++*cursor;
  }
  for (unsigned index = 0; index < count; ++index) {
    cgra_packet_t packet = packets[index];
    while (*cursor < kernel->patch_count && kernel->patches[*cursor].packet_index == first + index) {
      const cgra_patch_t *patch = &kernel->patches[(*cursor)++];
      const uint32_t value = symbol(task->symbols[patch->symbol_index], tile, task->elements) * patch->scale + patch->offset;
      payload(&packet, value);
    }
    cgra_send_packet_fast(packet);
  }
}

static void prepare(const soc_task_t *task, cgra_run_t mode, unsigned tile, unsigned *cursor) {
  const cgra_kernel_t *kernel = task->kernel;
  if (mode != CGRA_COLD) {
    send(task, tile, kernel->config_packets, 0, kernel->rearm_count, cursor);
  }
  if (mode != CGRA_REPEAT) {
    send(task, tile, kernel->config_packets + kernel->rearm_count, kernel->rearm_count, kernel->setup_count, cursor);
  }
  const unsigned first = kernel->rearm_count + kernel->setup_count;
  send(task, tile, kernel->config_packets + first, first, kernel->config_count - first, cursor);
}

int soc_cgra_capture(const soc_context_t *context, const soc_task_t *task) {
  const accel_t device = *context->plan->devices[task->endpoint];
  int status = 0;
  accel_commands(device, {
    status = task->full_config ? cgra_job_full_at(device.control, task->job, task->kernel, task->symbols, 0) : cgra_job_config_at(device.control, task->job, task->kernel, task->symbols);
  });
  if (status != 0) {
    return status;
  }
  while (cgra_link_read_at(device.control, CGRA_LINK_CONTROL_CONFIG_DONE) == 0) {
  }
  if (!task->full_config) {
    context->states[task->endpoint].loaded = task->kernel;
  }
  return cgra_link_read_at(device.control, CGRA_LINK_CONTROL_CONFIG_STATUS) != AUTO_LINK_STATUS_SUCCESS;
}

static cgra_dma_desc_t descriptor(unsigned word, unsigned bytes, unsigned tag) {
  return ((uint64_t)word << CGRA_DMA_DESC_SPM_ADDR_LSB) | ((uint64_t)bytes << CGRA_DMA_DESC_NBYTES_LSB) | ((uint64_t)tag << CGRA_DMA_DESC_TAG_LSB);
}

static void input_dma(const soc_context_t *context, const soc_dma_t *dma, unsigned tile) {
  const void *source = (const void *)(soc_operand(context, dma->source) + tile * dma->source_stride);
  const cgra_dma_desc_t desc = descriptor(dma->word, dma->bytes, dma->tag);
  if (dma->packed) {
    cgra_dma_mvin_i8_async(source, desc);
  } else {
    cgra_dma_mvin_async(source, desc);
  }
}

static int wait_dma(unsigned tag) {
  const uint8_t observed = cgra_dma_wait(tag);
  if (observed != tag) {
    printf("SoC runtime: DMA tag mismatch expected=%u observed=%u\n", tag, observed);
    return 1;
  }
  return 0;
}

int soc_cgra_init(const soc_context_t *context, const soc_task_t *task) {
  soc_state_t *state = &context->states[task->endpoint];
  state->last = NULL;
  state->loaded = NULL;
  const accel_t device = *context->plan->devices[task->endpoint];
  for (unsigned index = 0; index < task->preload_count; ++index) {
    const soc_dma_t *dma = &task->preloads[index];
    int status = 0;
    accel_commands(device, {
      input_dma(context, dma, 0);
      status = wait_dma(dma->tag);
    });
    if (status != 0) {
      return status;
    }
  }
  return 0;
}

static int load(const soc_context_t *context, const soc_task_t *task, cgra_run_t mode, unsigned tile, unsigned *cursor) {
  if (task->dma_count == 0) {
    prepare(task, mode, tile, cursor);
  }
  for (unsigned index = 0; index < task->dma_count; ++index) {
    const soc_dma_t *dma = &task->dmas[index];
    input_dma(context, dma, tile);
    if (index + 1 == task->dma_count) {
      prepare(task, mode, tile, cursor);
    }
    if (wait_dma(dma->tag) != 0) {
      return 1;
    }
  }
  return 0;
}

static int execute(soc_context_t *context, const soc_task_t *task, unsigned tile) {
  soc_state_t *state = &context->states[task->endpoint];
  const cgra_kernel_t *kernel = task->kernel;
  const cgra_run_t mode = !state->started ? CGRA_COLD : state->last == task ? CGRA_REPEAT : CGRA_SWITCH;
  unsigned cursor = 0;
  if (state->loaded != kernel) {
    cgra_send_packets_fast(kernel->static_packets, kernel->static_count);
    state->loaded = kernel;
  }
  const int status = load(context, task, mode, tile, &cursor);
  if (status != 0) {
    return status;
  }
  uint64_t ready = 0;
  uint64_t completion = 0;
  uint64_t result = 0;
  CGRA_SET_EXPECTED_COMPLETES(kernel->expected_completes);
  send(task, tile, kernel->launch_packets, kernel->config_count, kernel->launch_count, &cursor);
  CGRA_WAIT(ready);
  CGRA_STATUS(completion);
  CGRA_RESULT(result);
  state->started = 1;
  state->last = task;
  if (ready != 1 || (completion & 1) != 1 || ((completion >> 1) & UINT64_C(0xffff)) != kernel->expected_completes || result != 0) {
    printf("SoC runtime: CGRA completion mismatch endpoint=%u tile=%u ready=%lu status=%lu result=%lu\n", task->endpoint, tile, (unsigned long)ready, (unsigned long)completion, (unsigned long)result);
    return 1;
  }
  return 0;
}

int soc_cgra_execute(soc_context_t *context, const soc_task_t *task, unsigned tile) {
  const accel_t device = *context->plan->devices[task->endpoint];
  int status = 0;
  accel_commands(device, { status = execute(context, task, tile); });
  return status;
}

int soc_cgra_store(const soc_context_t *context, const soc_output_t *output) {
  const accel_t device = *context->plan->devices[output->endpoint];
  void *destination = (void *)soc_operand(context, output->destination);
  const cgra_dma_desc_t desc = descriptor(output->word, output->bytes, 0);
  int status = 0;
  accel_commands(device, {
    if (output->packed) {
      cgra_dma_mvout_i8_async(destination, desc);
    } else {
      cgra_dma_mvout_async(destination, desc);
    }
    status = wait_dma(0);
  });
  return status;
}

static void complete(soc_context_t *context, const soc_task_t *task) {
  soc_state_t *state = &context->states[task->endpoint];
  state->started = 1;
  state->last = NULL;
  state->loaded = NULL;
}

const soc_driver_t soc_cgra_driver = {.init = soc_cgra_init, .capture = soc_cgra_capture, .execute = soc_cgra_execute, .complete = complete};
