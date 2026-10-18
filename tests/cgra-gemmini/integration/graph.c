#include "accel_generated.h"
#include "auto_link.h"
#include "cgra_dma.h"
#include "cgra_link.h"
#include "gemmini.h"
#include "gemmini_job.h"
#include "generated/cgra_relu_runtime_fast_api.h"
#include "generated/graph_dram.h"
#include "generated/graph_local.h"
#include "generated/graph_packed.h"
#include "generated/graph_raw.h"
#include "pool.h"

#include <stdint.h>
#include <stdio.h>

enum {
  WORDS = 32,
  BYTES = WORDS * sizeof(acc_t),
  ROWS = WORDS / DIM,
  ROW_BYTES = DIM * sizeof(elem_t),
  ACC_ROW_BYTES = DIM * sizeof(acc_t),
  A_ROW = 0,
  B_ROW = DIM,
  CHANNELS = WORDS / 4,
};

static elem_t input[ROWS][DIM] row_align(1);
static elem_t weights[DIM][DIM] row_align(1);
static acc_t logits[WORDS] __attribute__((aligned(32)));
static elem_t pooled[CHANNELS] __attribute__((aligned(32)));
static acc_t next_logits[WORDS] __attribute__((aligned(32)));
static elem_t next_pooled[CHANNELS] __attribute__((aligned(32)));
static acc_t raw_input[WORDS] __attribute__((aligned(32)));
static acc_t raw_output[WORDS] __attribute__((aligned(32)));

static void init_inputs(void) {
  for (unsigned index = 0; index < WORDS; ++index) {
    input[index / DIM][index % DIM] = (elem_t)((int)index - WORDS / 2);
    raw_input[index] = ((int)index - WORDS / 2) * 31;
    logits[index] = raw_output[index] = -1;
    next_logits[index] = -1;
  }
  for (unsigned row = 0; row < DIM; ++row) {
    for (unsigned column = 0; column < DIM; ++column) {
      weights[row][column] = row == column;
    }
  }
  for (unsigned index = 0; index < CHANNELS; ++index) {
    pooled[index] = -1;
    next_pooled[index] = -1;
  }
}

static int configure_gemmini(accel_t device, int external) {
  const uint32_t acc_write = UINT32_C(1) << (ADDR_LEN - 1);
  const uint32_t acc_read = acc_write | (UINT32_C(1) << (ADDR_LEN - 3));
  const uint32_t publication = (device.spm_bytes - BYTES) / ROW_BYTES;
  accel_commands(device, {
    gemmini_flush(0);
    gemmini_config_ld(ROW_BYTES);
    gemmini_config_st(ACC_ROW_BYTES);
    gemmini_config_ex(WEIGHT_STATIONARY, NO_ACTIVATION, 0);
    gemmini_extended_mvin(input, A_ROW, DIM, ROWS);
    gemmini_mvin(weights, B_ROW);
  });
  gemmini_fence();
  if (gemmini_job_capture_at(device.control, 0, 3, external) != 0) {
    return 1;
  }
  if (external) {
    const gemmini_patch_t output = {.command = 2, .operand = 0, .lsb = 0, .width = 64, .source = GEMMINI_VALUE_OUTPUT_ADDRESS, .scale = 1};
    gemmini_job_patch_at(device.control, &output);
  }
  accel_commands(device, {
    gemmini_extended_preload(B_ROW, acc_write, DIM, DIM, DIM, ROWS);
    gemmini_extended_compute_preloaded(A_ROW, GARBAGE_ADDR, DIM, ROWS, DIM, ROWS);
    if (!external) {
      gemmini_extended_mvout_spad(publication, sizeof(acc_t) / sizeof(elem_t), acc_read, DIM, ROWS);
    } else {
      gemmini_extended_mvout(NULL, acc_read, DIM, ROWS);
    }
  });
  return 0;
}

static int configure_cgra(accel_t device) {
  static const cgra_link_symbol_t symbols[RELU_RUNTIME_SYMBOL_COUNT] = {
      [RELU_RUNTIME_SYMBOL_INPUT0] = {0, WORDS, CGRA_LINK_SLOT},
      [RELU_RUNTIME_SYMBOL_OUTPUT] = {0, WORDS, CGRA_LINK_SLOT},
      [RELU_RUNTIME_SYMBOL_ELEMENTS] = {0, 0, CGRA_LINK_ELEMENTS},
  };
  int status = 0;
  accel_commands(device, { status = cgra_job_config_at(device.control, 0, &RELU_RUNTIME, symbols); });
  return status;
}

static int run_graph(const auto_link_graph_t *graph, const uintptr_t *addresses, unsigned stages) {
  auto_link_load(graph, addresses);
  auto_link_tiles(1, 1, 1, 1);
  cgra_dma_memory_fence();
  auto_link_input_ready();
  unsigned seen = 0;
  int failures = 0;
  for (unsigned index = 0; index < stages; ++index) {
    const cgra_link_result_t result = cgra_link_wait_at(CGRA0.control);
    const unsigned bit = result.stage < stages ? 1u << result.stage : 0;
    if (bit == 0 || (seen & bit) || result.job != 0 || result.status != AUTO_LINK_STATUS_SUCCESS || result.detail != 0 || result.data != 0) {
      printf("Graph result stage=%u job=%u status=%u detail=%u data=%u\n", result.stage, result.job, result.status, result.detail, result.data);
      ++failures;
    }
    seen |= bit;
  }
  while (*(volatile uint32_t *)(AUTO_LINK_BASE + AUTO_LINK_RUNNING)) {
  }
  cgra_dma_memory_fence();
  return failures + (seen != (1u << stages) - 1);
}

static int verify_relu(uintptr_t address) {
  const volatile elem_t *values = (const volatile elem_t *)address;
  int failures = 0;
  for (unsigned index = 0; index < WORDS; ++index) {
    const int value = input[index / DIM][index % DIM];
    const int expected = value > 0 ? value : 0;
    if (values[index] != expected) {
      printf("Graph ReLU index=%u actual=%d expected=%d\n", index, (int)values[index], expected);
      ++failures;
    }
  }
  return failures;
}

static int verify_dram(const uintptr_t *addresses) {
  const volatile acc_t *result = (const volatile acc_t *)addresses[DRAM_ADDRESS_LOGITS];
  const volatile elem_t *pool = (const volatile elem_t *)addresses[DRAM_ADDRESS_POOLED];
  int failures = verify_relu(CGRA1.spm);
  for (unsigned index = 0; index < WORDS; ++index) {
    failures += result[index] != input[index / DIM][index % DIM];
  }
  for (unsigned channel = 0; channel < CHANNELS; ++channel) {
    const int expected = WORDS - CHANNELS + channel - WORDS / 2;
    const int tail = ((volatile elem_t *)CGRA0.spm)[channel];
    if (pool[channel] != expected || tail != expected) {
      printf("Graph Pool channel=%u actual=%d tail=%d expected=%d\n", channel, (int)pool[channel], tail, expected);
      ++failures;
    }
  }
  return failures;
}

int main(void) {
  init_inputs();
  int failures = configure_gemmini(GEMMINI0, 0) + configure_gemmini(GEMMINI1, 1);
  failures += configure_cgra(CGRA0) + configure_cgra(CGRA1);
  accel_commands(POOL, {
    pool_config_input(0, 2, 2, CHANNELS);
    pool_config_output(0);
    pool_config_window(POOL_MODE_MAX, 2, 2, 2, 2, 0, 0);
  });
  if (failures) {
    printf("Runtime graph: FAIL (configuration)\n");
    return 1;
  }

  failures += run_graph(&LOCAL_GRAPH, NULL, 2);
  failures += verify_relu(CGRA0.spm);
  printf("Graph local checked\n");

  const uintptr_t dram_addresses[][DRAM_ADDRESS_COUNT] = {
      {[DRAM_ADDRESS_LOGITS] = (uintptr_t)logits, [DRAM_ADDRESS_POOLED] = (uintptr_t)pooled},
      {[DRAM_ADDRESS_LOGITS] = (uintptr_t)next_logits, [DRAM_ADDRESS_POOLED] = (uintptr_t)next_pooled},
  };
  for (unsigned run = 0; run < 2; ++run) {
    failures += run_graph(&DRAM_GRAPH, dram_addresses[run], 4);
    failures += verify_dram(dram_addresses[run]);
    printf("Graph DRAM binding %u checked\n", run);
  }

  const uintptr_t raw_addresses[RAW_ADDRESS_COUNT] = {
      [RAW_ADDRESS_INPUT] = (uintptr_t)raw_input,
  };
  failures += run_graph(&RAW_GRAPH, raw_addresses, 1);
  accel_commands(CGRA0, {
    cgra_dma_mvout_async(raw_output, CGRA_DMA_DESC_CONST(0, BYTES, 1));
    failures += cgra_dma_wait(1) != 1;
  });
  cgra_dma_memory_fence();
  for (unsigned index = 0; index < WORDS; ++index) {
    const acc_t expected = raw_input[index] > 0 ? raw_input[index] : 0;
    if (raw_output[index] != expected) {
      printf("Graph INT32 index=%u actual=%d expected=%d\n", index, (int)raw_output[index], (int)expected);
      ++failures;
    }
  }
  printf("Graph INT32 input + manual DMA output checked\n");

  const uintptr_t packed_addresses[PACKED_ADDRESS_COUNT] = {
      [PACKED_ADDRESS_INPUT] = (uintptr_t)input,
  };
  failures += run_graph(&PACKED_GRAPH, packed_addresses, 1);
  const uintptr_t packed_output = GEMMINI1.spm + GEMMINI1.spm_bytes / 2;
  accel_commands(CGRA1, {
    cgra_dma_mvout_i8_async((void *)packed_output, CGRA_DMA_I8_DESC_CONST(0, WORDS, 2));
    failures += cgra_dma_wait(2) != 2;
  });
  cgra_dma_memory_fence();
  failures += verify_relu(packed_output);
  printf("Graph INT8 input + manual DMA output checked\n");

  failures += run_graph(&LOCAL_GRAPH, NULL, 2);
  failures += verify_relu(CGRA0.spm);
  printf("Runtime graph: %s (%d mismatches)\n", failures == 0 ? "PASS" : "FAIL", failures);
  return failures != 0;
}
