#include "soc_runtime.h"

#include "accel_generated.h"
#include "auto_link.h"
#include "cgra_dma.h"
#include "cgra_link.h"
#include "gemmini.h"
#include "gemmini_job.h"
#include "generated/cgra_relu_runtime_fast_api.h"

#include <stdio.h>

_Static_assert(DIM == SOC_DIM && sizeof(elem_t) == 1 && sizeof(acc_t) == 4, "SoC v1 requires 16-wide INT8/INT32 Gemmini");
_Static_assert(CGRA_DMA_SPM_WORDS == 32768 && PKT_DATA_LSB + DATA_PAYLOAD_LSB == 135, "SoC v1 requires the attention CGRA packet layout");

enum {
  TILE_BYTES = SOC_WORDS * sizeof(int32_t),
  ROW_BYTES = SOC_DIM * sizeof(int8_t),
  ACC_ROW_BYTES = SOC_DIM * sizeof(int32_t),
  ACC_ROW_STRIDE = sizeof(int32_t) / sizeof(int8_t),
  DMA_TAG = 1,
};

static const uint32_t ACC_WRITE = UINT32_C(1) << (ADDR_LEN - 1);
static const uint32_t ACC_READ = (UINT32_C(1) << (ADDR_LEN - 1)) | (UINT32_C(1) << (ADDR_LEN - 3));

static unsigned long cycles(void) {
  unsigned long value;
  __asm__ volatile("rdcycle %0" : "=r"(value)::"memory");
  return value;
}

static accel_t endpoint(unsigned id) {
  const accel_t devices[] = {GEMMINI0, GEMMINI1, CGRA0};
  return devices[id];
}

static void load_gemmini(accel_t device, const soc_plan_t *plan, const int8_t *weights) {
  accel_commands(device, {
    gemmini_flush(0);
    gemmini_config_ld(ROW_BYTES);
    gemmini_config_ex(WEIGHT_STATIONARY, NO_ACTIVATION, 0);
    gemmini_config_st(ACC_ROW_BYTES);
    gemmini_mvin(weights, plan->weight_row);
  });
}

static void first_gemm(const soc_plan_t *plan, unsigned tile) {
  accel_commands(endpoint(plan->tasks[0].endpoint), {
    gemmini_extended_preload(plan->weight_row, ACC_WRITE, SOC_DIM, SOC_DIM, SOC_DIM, SOC_ROWS);
    gemmini_extended_compute_preloaded(plan->input_row + tile * SOC_ROWS, GARBAGE_ADDR, SOC_DIM, SOC_ROWS, SOC_DIM, SOC_ROWS);
    gemmini_extended_mvout_spad(plan->publication_bytes / ROW_BYTES, ACC_ROW_STRIDE, ACC_READ, SOC_DIM, SOC_ROWS);
  });
}

static void last_gemm(soc_context_t *context, unsigned tile) {
  const soc_plan_t *plan = context->plan;
  const accel_t cgra = endpoint(plan->tasks[1].endpoint);
  const uintptr_t input = cgra.spm + plan->relu_word + tile * plan->slot_words;
  accel_commands(endpoint(plan->tasks[2].endpoint), {
    gemmini_extended_mvin((const void *)input, plan->input_row, SOC_DIM, SOC_ROWS);
    gemmini_extended_preload(plan->weight_row, ACC_WRITE, SOC_DIM, SOC_DIM, SOC_DIM, SOC_ROWS);
    gemmini_extended_compute_preloaded(plan->input_row, GARBAGE_ADDR, SOC_DIM, SOC_ROWS, SOC_DIM, SOC_ROWS);
    gemmini_extended_mvout(context->output + tile * SOC_WORDS, ACC_READ, SOC_DIM, SOC_ROWS);
  });
}

static int capture_first(const soc_plan_t *plan) {
  const soc_task_t *task = &plan->tasks[0];
  const accel_t device = endpoint(task->endpoint);
  if (gemmini_job_capture_at(device.control, task->job, 3, 2) != 0) {
    return 1;
  }
  const gemmini_patch_t input = {
      .command = 1,
      .operand = 0,
      .width = ADDR_LEN,
      .source = GEMMINI_VALUE_TILE_ID,
      .scale = SOC_ROWS,
      .offset = plan->input_row,
  };
  const gemmini_patch_t output = {
      .command = 2,
      .operand = 0,
      .width = ADDR_LEN,
      .source = GEMMINI_VALUE_SLOT,
      .scale = TILE_BYTES / ROW_BYTES,
      .offset = plan->publication_bytes / ROW_BYTES,
  };
  gemmini_job_patch_at(device.control, &input);
  gemmini_job_patch_at(device.control, &output);
  first_gemm(plan, 0);
  return gemmini_job_end_at(device.control);
}

static int capture_last(soc_context_t *context) {
  const soc_plan_t *plan = context->plan;
  const soc_task_t *task = &plan->tasks[2];
  const accel_t device = endpoint(task->endpoint);
  if (gemmini_job_capture_at(device.control, task->job, 4, 2) != 0) {
    return 1;
  }
  const gemmini_patch_t input = {
      .command = 0,
      .operand = 0,
      .width = 64,
      .source = GEMMINI_VALUE_TILE_ID,
      .scale = plan->slot_words,
      .relative = 1,
  };
  const gemmini_patch_t output = {
      .command = 3,
      .operand = 0,
      .width = 64,
      .source = GEMMINI_VALUE_TILE_ID,
      .scale = TILE_BYTES,
      .relative = 1,
  };
  gemmini_job_patch_at(device.control, &input);
  gemmini_job_patch_at(device.control, &output);
  last_gemm(context, 0);
  return gemmini_job_end_at(device.control);
}

static int capture_relu(const soc_plan_t *plan) {
  const soc_task_t *task = &plan->tasks[1];
  const accel_t device = endpoint(task->endpoint);
  const cgra_link_symbol_t symbols[RELU_RUNTIME_SYMBOL_COUNT] = {
      [RELU_RUNTIME_SYMBOL_INPUT0] = {0, plan->slot_words, CGRA_LINK_SLOT},
      [RELU_RUNTIME_SYMBOL_OUTPUT] = {plan->relu_word, plan->slot_words, CGRA_LINK_TILE_ID},
      [RELU_RUNTIME_SYMBOL_ELEMENTS] = {0, 0, CGRA_LINK_ELEMENTS},
  };
  int status = 0;
  accel_commands(device, { status = cgra_job_config_at(device.control, task->job, &RELU_RUNTIME, symbols); });
  if (status != 0) {
    return status;
  }
  while (cgra_link_read_at(device.control, CGRA_LINK_CONTROL_CONFIG_DONE) == 0) {
  }
  return cgra_link_read_at(device.control, CGRA_LINK_CONTROL_CONFIG_STATUS) != AUTO_LINK_STATUS_SUCCESS;
}

static void capture_region(const soc_plan_t *plan, const soc_region_t *region) {
  auto_link_stage_t stages[SOC_TASKS] = {0};
  for (unsigned index = 0; index < region->count; ++index) {
    const soc_task_t *task = &plan->tasks[region->first + index];
    stages[index].endpoint = task->endpoint;
    stages[index].job = task->job;
    stages[index].output.address.binding = -1;
  }
  const auto_link_edge_t edges[] = {
      {.source = 0, .destination = 0, .flags = 3, .address = {.binding = -1}},
      {.source = 0, .destination = 1, .flags = 5, .bytes = TILE_BYTES, .address = {.value = endpoint(plan->tasks[0].endpoint).spm, .binding = -1}},
      {.source = 1, .destination = 2, .flags = 1, .address = {.binding = -1}},
  };
  const auto_link_graph_t graph = {
      .stage_count = region->count,
      .edge_count = region->count,
      .stage_capacity = AUTO_LINK_GRAPH.stage_capacity,
      .edge_capacity = AUTO_LINK_GRAPH.edge_capacity,
      .stages = stages,
      .edges = edges,
  };
  auto_link_load(&graph, NULL);
  auto_link_transfer(1, plan->publication_bytes, 0, TILE_BYTES, plan->slot_words * sizeof(int32_t), 0);
  auto_link_tiles(plan->tiles, 1, 1, 1);
  auto_link_run_config(0);
}

int soc_prepare(soc_context_t *context, const soc_plan_t *plan, const int8_t *input, const int8_t *weight0, const int8_t *weight1, int32_t *output) {
  const unsigned long begin = cycles();
  context->plan = plan;
  context->output = output;
  context->stats = (soc_stats_t){0};
  load_gemmini(endpoint(plan->tasks[0].endpoint), plan, weight0);
  load_gemmini(endpoint(plan->tasks[2].endpoint), plan, weight1);
  accel_commands(endpoint(plan->tasks[0].endpoint), { gemmini_extended_mvin(input, plan->input_row, SOC_DIM, plan->tiles * SOC_ROWS); });
  gemmini_fence();

  if (plan->mode == SOC_MANUAL) {
    accel_commands(endpoint(plan->tasks[1].endpoint), { cgra_send_packets_fast(RELU_RUNTIME.static_packets, RELU_RUNTIME.static_count); });
  }
  for (unsigned index = 0; index < plan->region_count; ++index) {
    const soc_region_t *region = &plan->regions[index];
    if (!region->automatic) {
      continue;
    }
    if (capture_first(plan) != 0 || capture_relu(plan) != 0 || (region->count == SOC_TASKS && capture_last(context) != 0)) {
      printf("SoC runtime: capture failed\n");
      return 1;
    }
    capture_region(plan, region);
  }
  __asm__ volatile("fence iorw, iorw" ::: "memory");
  context->stats.prepare_cycles = cycles() - begin;
  context->stats.total_cycles = context->stats.prepare_cycles;
  return 0;
}

static int manual_relu(soc_context_t *context, unsigned tile) {
  const soc_plan_t *plan = context->plan;
  cgra_kernel_t kernel = RELU_RUNTIME;
  cgra_packet_t packets[RELU_RUNTIME_FAST_CONFIG_PACKET_COUNT];
  const uint32_t symbols[RELU_RUNTIME_SYMBOL_COUNT] = {
      [RELU_RUNTIME_SYMBOL_INPUT0] = 0,
      [RELU_RUNTIME_SYMBOL_OUTPUT] = plan->relu_word + tile * plan->slot_words,
      [RELU_RUNTIME_SYMBOL_ELEMENTS] = SOC_WORDS,
  };
  for (unsigned index = 0; index < kernel.config_count; ++index) {
    packets[index] = kernel.config_packets[index];
  }
  for (unsigned index = 0; index < kernel.patch_count; ++index) {
    const cgra_patch_t *patch = &kernel.patches[index];
    cgra_packet_t *packet = &packets[patch->packet_index];
    const uint32_t value = symbols[patch->symbol_index] * patch->scale + patch->offset;
    packet->hi = (packet->hi & ~RELU_RUNTIME_STORE_DATA_PAYLOAD_HI(UINT32_MAX)) | RELU_RUNTIME_STORE_DATA_PAYLOAD_HI(value);
  }
  kernel.config_packets = packets;
  const accel_t cgra = endpoint(plan->tasks[1].endpoint);
  const uintptr_t input = endpoint(plan->tasks[0].endpoint).spm + plan->publication_bytes;
  const cgra_run_t mode = !context->cgra_started ? CGRA_COLD : tile == 0 ? CGRA_SWITCH : CGRA_REPEAT;
  uint8_t tag = 0;
  accel_commands(cgra, {
    cgra_dma_mvin_async((const void *)input, CGRA_DMA_DESC_CONST(0, TILE_BYTES, DMA_TAG));
    cgra_prepare(&kernel, mode);
    tag = cgra_dma_wait(DMA_TAG);
  });
  if (tag != DMA_TAG) {
    printf("SoC runtime: DMA tag mismatch tile=%u tag=%u\n", tile, tag);
    return 1;
  }
  uint64_t ready = 0;
  uint64_t status = 0;
  uint64_t result = 0;
  accel_commands(cgra, {
    cgra_start(&kernel);
    CGRA_WAIT(ready);
    CGRA_STATUS(status);
    CGRA_RESULT(result);
  });
  context->cgra_started = 1;
  if (ready != 1 || (status & 1) != 1 || ((status >> 1) & UINT64_C(0xffff)) != kernel.expected_completes || result != 0) {
    printf("SoC runtime: CGRA completion mismatch tile=%u ready=%lu status=%lu result=%lu\n", tile, (unsigned long)ready, (unsigned long)status, (unsigned long)result);
    return 1;
  }
  return 0;
}

static int cpu_region(soc_context_t *context, const soc_region_t *region, unsigned tile) {
  for (unsigned index = region->first; index < region->first + region->count; ++index) {
    ++context->stats.cpu_task_submissions;
    if (context->plan->tasks[index].kernel == SOC_RELU_I8) {
      if (manual_relu(context, tile) != 0) {
        return 1;
      }
    } else if (index == 0) {
      first_gemm(context->plan, tile);
    } else {
      last_gemm(context, tile);
    }
    gemmini_fence();
  }
  return 0;
}

static int automatic_region(soc_context_t *context, const soc_region_t *region) {
  const uintptr_t collector = endpoint(context->plan->tasks[1].endpoint).control;
  const unsigned expected = (1u << region->count) - 1;
  unsigned seen = 0;
  int failures = 0;
  ++context->stats.automatic_launches;
  auto_link_run(0, 1);
  for (;;) {
    if (cgra_link_read_at(collector, CGRA_LINK_CONTROL_RESULT_VALID)) {
      const cgra_link_result_t result = cgra_link_wait_at(collector);
      const unsigned bit = result.stage < region->count ? 1u << result.stage : 0;
      if (bit == 0 || (seen & bit) != 0 || result.status != AUTO_LINK_STATUS_SUCCESS || result.detail != 0 || result.data != 0 ||
          result.job != context->plan->tasks[region->first + result.stage].job) {
        printf("SoC runtime: AutoLink result mismatch stage=%u job=%u status=%u detail=%u data=%u\n", result.stage, result.job, result.status, result.detail, result.data);
        ++failures;
      }
      seen |= bit;
      continue;
    }
    if (*(volatile uint32_t *)(AUTO_LINK_BASE + AUTO_LINK_RUNNING) == 0 && cgra_link_read_at(collector, CGRA_LINK_CONTROL_RESULT_VALID) == 0) {
      break;
    }
  }
  context->cgra_started = 1;
  __asm__ volatile("fence iorw, iorw" ::: "memory");
  if (seen != expected) {
    printf("SoC runtime: incomplete results seen=%u expected=%u\n", seen, expected);
    ++failures;
  }
  return failures != 0;
}

static int execute_regions(soc_context_t *context) {
  const soc_plan_t *plan = context->plan;
  if (plan->mode == SOC_MANUAL) {
    for (unsigned tile = 0; tile < plan->tiles; ++tile) {
      for (unsigned index = 0; index < plan->region_count; ++index) {
        if (cpu_region(context, &plan->regions[index], tile) != 0) {
          return 1;
        }
      }
    }
    return 0;
  }
  for (unsigned index = 0; index < plan->region_count; ++index) {
    const soc_region_t *region = &plan->regions[index];
    if (region->automatic) {
      if (automatic_region(context, region) != 0) {
        return 1;
      }
      continue;
    }
    for (unsigned tile = 0; tile < plan->tiles; ++tile) {
      if (cpu_region(context, region, tile) != 0) {
        return 1;
      }
    }
  }
  return 0;
}

int soc_execute(soc_context_t *context) {
  const unsigned long begin = cycles();
  const int status = execute_regions(context);
  __asm__ volatile("fence iorw, iorw" ::: "memory");
  context->stats.execute_cycles = cycles() - begin;
  context->stats.total_cycles = context->stats.prepare_cycles + context->stats.execute_cycles;
  return status;
}
