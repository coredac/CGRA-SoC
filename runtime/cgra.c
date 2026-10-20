#include "driver.h"

#include "cgra_dma.h"

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

static void relocate(const soc_task_t *task, cgra_packet_t *packets, unsigned tile, unsigned elements) {
  const cgra_kernel_t *kernel = task->kernel;
  for (unsigned index = 0; index < kernel->config_count; ++index) {
    packets[index] = kernel->config_packets[index];
  }
  for (unsigned index = 0; index < kernel->launch_count; ++index) {
    packets[kernel->config_count + index] = kernel->launch_packets[index];
  }
  for (unsigned index = 0; index < kernel->patch_count; ++index) {
    const cgra_patch_t *patch = &kernel->patches[index];
    const uint32_t value = symbol(task->symbols[patch->symbol_index], tile, elements) * patch->scale + patch->offset;
    payload(&packets[patch->packet_index], value);
  }
}

void soc_cgra_load(const soc_context_t *context, const soc_task_t *task) {
  const accel_t device = *context->plan->devices[task->endpoint];
  accel_commands(device, { cgra_send_packets_fast(task->kernel->static_packets, task->kernel->static_count); });
}

int soc_cgra_capture(const soc_context_t *context, const soc_task_t *task) {
  const accel_t device = *context->plan->devices[task->endpoint];
  int status = 0;
  accel_commands(device, { status = cgra_job_config_at(device.control, task->job, task->kernel, task->symbols); });
  if (status != 0) {
    return status;
  }
  while (cgra_link_read_at(device.control, CGRA_LINK_CONTROL_CONFIG_DONE) == 0) {
  }
  return cgra_link_read_at(device.control, CGRA_LINK_CONTROL_CONFIG_STATUS) != AUTO_LINK_STATUS_SUCCESS;
}

static int load(const soc_context_t *context, const soc_task_t *task, const cgra_kernel_t *kernel, cgra_run_t mode) {
  if (task->dma_count == 0) {
    cgra_prepare(kernel, mode);
  }
  for (unsigned index = 0; index < task->dma_count; ++index) {
    const soc_dma_t *dma = &task->dmas[index];
    const cgra_dma_desc_t descriptor = ((uint64_t)dma->word << CGRA_DMA_DESC_SPM_ADDR_LSB) | ((uint64_t)dma->bytes << CGRA_DMA_DESC_NBYTES_LSB) | ((uint64_t)dma->tag << CGRA_DMA_DESC_TAG_LSB);
    cgra_dma_mvin_async((const void *)soc_operand(context, dma->source), descriptor);
    if (index + 1 == task->dma_count) {
      cgra_prepare(kernel, mode);
    }
    const uint8_t observed = cgra_dma_wait(dma->tag);
    if (observed != dma->tag) {
      printf("SoC runtime: DMA tag mismatch expected=%u observed=%u\n", dma->tag, observed);
      return 1;
    }
  }
  return 0;
}

int soc_cgra_execute(soc_context_t *context, const soc_task_t *task, unsigned tile) {
  const accel_t device = *context->plan->devices[task->endpoint];
  soc_state_t *state = &context->states[task->endpoint];
  cgra_kernel_t kernel = *task->kernel;
  cgra_packet_t packets[kernel.config_count + kernel.launch_count];
  const unsigned elements = task->dma_count == 0 ? 0 : task->dmas[task->dma_count - 1].bytes / CGRA_DMA_WORD_BYTES;
  relocate(task, packets, tile, elements);
  kernel.config_packets = packets;
  kernel.launch_packets = packets + kernel.config_count;
  const cgra_run_t mode = !state->started ? CGRA_COLD : state->last == task ? CGRA_REPEAT : CGRA_SWITCH;
  int status = 0;
  accel_commands(device, { status = load(context, task, &kernel, mode); });
  if (status != 0) {
    return status;
  }
  uint64_t ready = 0;
  uint64_t completion = 0;
  uint64_t result = 0;
  accel_commands(device, {
    cgra_start(&kernel);
    CGRA_WAIT(ready);
    CGRA_STATUS(completion);
    CGRA_RESULT(result);
  });
  state->started = 1;
  state->last = task;
  if (ready != 1 || (completion & 1) != 1 || ((completion >> 1) & UINT64_C(0xffff)) != kernel.expected_completes || result != 0) {
    printf("SoC runtime: CGRA completion mismatch endpoint=%u tile=%u ready=%lu status=%lu result=%lu\n", task->endpoint, tile, (unsigned long)ready, (unsigned long)completion, (unsigned long)result);
    return 1;
  }
  return 0;
}
