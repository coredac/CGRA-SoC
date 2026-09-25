#include "accel_generated.h"
#include "auto_link.h"
#include "cgra_dma.h"
#include "cgra_link.h"
#include "gemmini.h"
#include "gemmini_conv.h"
#include "generated/cgra_add_relu_runtime_fast_api.h"
#include "generated/cgra_relu_runtime_fast_api.h"
#include "generated/resnet_data.h"
#include "pool.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
  MANUAL,
  HYBRID,
  SKIP_WORD = AUTO_LINK_CGRA1_BUFFER_SLOTS * CORE_WORDS,
  OUTPUT_WORD = 2 * AUTO_LINK_CGRA1_BUFFER_SLOTS * CORE_WORDS,
  STEM_WORD = AUTO_LINK_CGRA0_BUFFER_SLOTS * HALO_WORDS,
  FC_A_ROW = 0,
  FC_B_ROW = FC_A_ROW + FC_CHANNELS,
};

typedef struct {
  int row, column, rows, columns;
} region_t;

typedef struct {
  unsigned long execution, fabric, overlap, peak;
} timing_t;

static elem_t input[INPUT_H][INPUT_W][INPUT_CHANNELS] row_align(1);
static acc_t logits[CLASSES] row_align(1);
static cgra_packet_t relu_config[RELU_RUNTIME_FAST_CONFIG_PACKET_COUNT];
static cgra_packet_t add_config[ADD_RELU_RUNTIME_FAST_CONFIG_PACKET_COUNT];
static unsigned relu_runs, add_runs;
static timing_t timings[BLOCKS];
static const char *const modes[] = {"Manual", "Hybrid"};
static const uint32_t jobs[] = {
    [AUTO_LINK_STAGE_CONV1] = AUTO_LINK_JOB_CONV1, [AUTO_LINK_STAGE_PROJECTION] = AUTO_LINK_JOB_PROJECTION, [AUTO_LINK_STAGE_RELU] = AUTO_LINK_JOB_RELU, [AUTO_LINK_STAGE_CONV2] = AUTO_LINK_JOB_CONV2,
    [AUTO_LINK_STAGE_ADD] = AUTO_LINK_JOB_ADD,
};

enum { STAGES = sizeof(jobs) / sizeof(jobs[0]) };

static unsigned long cycles(void) {
  unsigned long value;
  __asm__ volatile("rdcycle %0" : "=r"(value)::"memory");
  return value;
}

static int minimum(int a, int b) { return a < b ? a : b; }

static int maximum(int a, int b) { return a > b ? a : b; }

static unsigned output_word(unsigned index) {
  unsigned word = OUTPUT_WORD;
  for (unsigned block = 0; block < index; ++block) {
    word += blocks[block].rows * blocks[block].columns * blocks[block].channels;
  }
  return word;
}

static unsigned tile_words(const block_t *block) { return TILE_ROWS * block->columns * block->channels; }

static uintptr_t input_address(void) { return GEMMINI0.spm + GEMMINI0.spm_bytes / 2; }

static uintptr_t pool_address(void) { return input_address() + sizeof(input); }

static uintptr_t block_input(unsigned index) { return index == 0 ? CGRA0.spm + STEM_WORD : CGRA1.spm + output_word(index - 1); }

static uintptr_t publication(accel_t device, unsigned slots, unsigned words) { return device.spm + device.spm_bytes - slots * words * sizeof(elem_t); }

static void init_inputs(void) {
  for (unsigned row = 0; row < INPUT_H; ++row) {
    for (unsigned column = 0; column < INPUT_W; ++column) {
      for (unsigned channel = 0; channel < INPUT_CHANNELS; ++channel) {
        input[row][column][channel] = (elem_t)((int)((row * 7 + column * 3 + channel * 5 + row * column) % 17) - 8);
      }
    }
  }
}

static void load_input(void) {
  accel_commands(GEMMINI0, { gemmini_flush(0); });
  accel_commands(GEMMINI1, { gemmini_flush(0); });
  volatile uint64_t *destination = (volatile uint64_t *)input_address();
  const uint64_t *source = (const uint64_t *)input;
  for (unsigned index = 0; index < sizeof(input) / sizeof(uint64_t); ++index) {
    destination[index] = source[index];
  }
  accel_commands(CGRA0, { cgra_send_packets_fast(RELU_RUNTIME.static_packets, RELU_RUNTIME.static_count); });
  accel_commands(CGRA1, { cgra_send_packets_fast(ADD_RELU_RUNTIME.static_packets, ADD_RELU_RUNTIME.static_count); });
  cgra_dma_memory_fence();
}

static region_t halo(region_t tile, const block_t *block) {
  const int row = maximum(tile.row - PADDING, 0);
  const int end = minimum(tile.row + tile.rows + PADDING, block->rows);
  return (region_t){row, 0, end - row, block->columns};
}

static void run_conv(accel_t device, region_t tile, region_t view, unsigned input_rows, unsigned input_columns, unsigned inputs, unsigned outputs, unsigned kernel, unsigned stride, unsigned padding,
                     uintptr_t source, const elem_t *weights, uintptr_t destination) {
  const int first_row = tile.row * stride - padding;
  const int first_column = tile.column * stride - padding;
  // Native loop_conv reads one extra stride-minus-one extent beyond the mathematical footprint.
  const int end_row = (tile.row + tile.rows) * stride + kernel - 1 - padding;
  const int end_column = (tile.column + tile.columns) * stride + kernel - 1 - padding;
  const int top = maximum(-first_row, 0);
  const int left = maximum(-first_column, 0);
  const int bottom = maximum(end_row - (int)input_rows, 0);
  const int right = maximum(end_column - (int)input_columns, 0);
  const unsigned offset = ((maximum(first_row, 0) - view.row) * view.columns + maximum(first_column, 0) - view.column) * inputs;
  const elem_t *source_tile = (const elem_t *)source + offset;
  accel_commands(device, {
    gemmini_extended_config_st(outputs * sizeof(elem_t), NO_ACTIVATION, ACC_SCALE_IDENTITY);
    gemmini_extended3_config_ex(WEIGHT_STATIONARY, 0, 0, 0, 1, stride, false, false, false);
    sp_tiled_conv(1, view.rows, view.columns, inputs, outputs, tile.rows, tile.columns, tile.rows, tile.columns, stride, padding, kernel, 1, inputs, outputs, outputs, 1, 1, 0, 1, tile.rows,
                  tile.columns, outputs, kernel, kernel, inputs, left, right, top, bottom, 0, 0, 0, 0, source_tile, weights, (elem_t *)destination, (const acc_t *)(uintptr_t)1, NO_ACTIVATION,
                  ACC_SCALE_IDENTITY, false, false, false, false, false, true, true, false, false, false, 1, 1);
  });
  gemmini_fence();
}

static int copy_input(accel_t device, uintptr_t source, unsigned word, unsigned elements, uint8_t tag) {
  const cgra_dma_desc_t descriptor = ((uint64_t)word << CGRA_DMA_DESC_SPM_ADDR_LSB) | ((uint64_t)(elements * sizeof(acc_t)) << CGRA_DMA_DESC_NBYTES_LSB) | ((uint64_t)tag << CGRA_DMA_DESC_TAG_LSB);
  uint8_t observed = 0;
  accel_commands(device, {
    cgra_dma_mvin_i8_async((const void *)source, descriptor);
    observed = cgra_dma_wait(tag);
  });
  if (observed != tag) {
    printf("ResNet DMA device=%u tag=%u expected=%u\n", device.id, observed, tag);
    return 1;
  }
  return 0;
}

static int run_cgra(accel_t device, const cgra_kernel_t *original, cgra_packet_t *config, const uint32_t *symbols, cgra_run_t mode) {
  cgra_kernel_t kernel = *original;
  for (unsigned index = 0; index < kernel.config_count; ++index) {
    config[index] = kernel.config_packets[index];
  }
  for (unsigned index = 0; index < kernel.patch_count; ++index) {
    const cgra_patch_t *patch = &kernel.patches[index];
    const uint32_t value = symbols[patch->symbol_index] * patch->scale + patch->offset;
    cgra_packet_t *packet = &config[patch->packet_index];
    packet->hi = (packet->hi & ~RELU_RUNTIME_STORE_DATA_PAYLOAD_HI(UINT32_MAX)) | RELU_RUNTIME_STORE_DATA_PAYLOAD_HI(value);
  }
  kernel.config_packets = config;
  uint64_t ready = 0, status = 0, result = 0;
  accel_commands(device, {
    cgra_prepare(&kernel, mode);
    cgra_start(&kernel);
    CGRA_WAIT(ready);
    CGRA_STATUS(status);
    CGRA_RESULT(result);
  });
  if (ready != 1 || (status & UINT64_C(1)) != 1 || ((status >> 1) & UINT64_C(0xffff)) != kernel.expected_completes || result != 0) {
    printf("ResNet CGRA device=%u ready=%lu status=%lu result=%lu\n", device.id, (unsigned long)ready, (unsigned long)status, (unsigned long)result);
    return 1;
  }
  return 0;
}

static int run_relu(unsigned output, unsigned elements) {
  const uint32_t symbols[RELU_RUNTIME_SYMBOL_COUNT] = {
      [RELU_RUNTIME_SYMBOL_INPUT0] = 0,
      [RELU_RUNTIME_SYMBOL_OUTPUT] = output,
      [RELU_RUNTIME_SYMBOL_ELEMENTS] = elements,
  };
  return run_cgra(CGRA0, &RELU_RUNTIME, relu_config, symbols, relu_runs++ == 0 ? CGRA_COLD : CGRA_REPEAT);
}

static int run_add(unsigned output, unsigned elements) {
  const uint32_t symbols[ADD_RELU_RUNTIME_SYMBOL_COUNT] = {
      [ADD_RELU_RUNTIME_SYMBOL_INPUT0] = 0,
      [ADD_RELU_RUNTIME_SYMBOL_INPUT1] = SKIP_WORD,
      [ADD_RELU_RUNTIME_SYMBOL_OUTPUT] = output,
      [ADD_RELU_RUNTIME_SYMBOL_ELEMENTS] = elements,
  };
  return run_cgra(CGRA1, &ADD_RELU_RUNTIME, add_config, symbols, add_runs++ == 0 ? CGRA_COLD : CGRA_REPEAT);
}

static int run_stem(void) {
  const region_t source = {0, 0, INPUT_H, INPUT_W};
  const uintptr_t destination = publication(GEMMINI0, AUTO_LINK_GEMMINI0_BUFFER_SLOTS, HALO_WORDS);
  for (unsigned row = 0; row < INPUT_H; row += TILE_ROWS) {
    const region_t tile = {row, 0, TILE_ROWS, INPUT_W};
    const unsigned elements = TILE_ROWS * INPUT_W * STEM_CHANNELS;
    run_conv(GEMMINI0, tile, source, INPUT_H, INPUT_W, INPUT_CHANNELS, STEM_CHANNELS, KERNEL, 1, PADDING, input_address(), stem_weights, destination);
    if (copy_input(CGRA0, destination, 0, elements, 0x20) != 0 || run_relu(STEM_WORD + row * INPUT_W * STEM_CHANNELS, elements) != 0) {
      return 1;
    }
  }
  return 0;
}

static int run_tile(unsigned index, region_t core) {
  const block_t *block = &blocks[index];
  const region_t expanded = halo(core, block);
  const region_t source = {0, 0, block->input_rows, block->input_columns};
  const unsigned elements = core.rows * core.columns * block->channels;
  const unsigned expanded_elements = expanded.rows * expanded.columns * block->channels;
  const unsigned offset = core.row * block->columns * block->channels;
  const uintptr_t first = publication(GEMMINI0, AUTO_LINK_GEMMINI0_BUFFER_SLOTS, HALO_WORDS);
  const uintptr_t second = publication(GEMMINI1, AUTO_LINK_GEMMINI1_BUFFER_SLOTS, CORE_WORDS);
  uintptr_t skip = block_input(index) + offset;
  const unsigned padding = block->stride == 1 ? PADDING : 0;
  run_conv(GEMMINI0, expanded, source, block->input_rows, block->input_columns, block->input_channels, block->channels, KERNEL, block->stride, padding, block_input(index), weights1[index], first);
  if (index != 0) {
    skip = first + HALO_WORDS * sizeof(elem_t);
    run_conv(GEMMINI0, core, source, block->input_rows, block->input_columns, block->input_channels, block->channels, 1, block->stride, 0, block_input(index), projection[index], skip);
  }
  if (copy_input(CGRA0, first, 0, expanded_elements, 0x20) != 0 || run_relu(0, expanded_elements) != 0) {
    return 1;
  }
  run_conv(GEMMINI1, core, expanded, block->rows, block->columns, block->channels, block->channels, KERNEL, 1, PADDING, CGRA0.spm, weights2[index], second);
  if (copy_input(CGRA1, second, 0, elements, 0x21) != 0 || copy_input(CGRA1, skip, SKIP_WORD, elements, 0x22) != 0) {
    return 1;
  }
  return run_add(output_word(index) + offset, elements);
}

static int run_manual(unsigned index) {
  const block_t *block = &blocks[index];
  for (unsigned row = 0; row < block->rows; row += TILE_ROWS) {
    if (run_tile(index, (region_t){row, 0, TILE_ROWS, block->columns}) != 0) {
      return 1;
    }
  }
  return 0;
}

static unsigned job(unsigned stage, unsigned index) { return jobs[stage] + (stage == AUTO_LINK_STAGE_RELU ? 0 : index - 1); }

static int configure_gemmini(unsigned index) {
  const block_t *block = &blocks[index];
  elem_t *first = (elem_t *)publication(GEMMINI0, AUTO_LINK_GEMMINI0_BUFFER_SLOTS, HALO_WORDS);
  elem_t *second = (elem_t *)publication(GEMMINI1, AUTO_LINK_GEMMINI1_BUFFER_SLOTS, CORE_WORDS);
  int status = 0;
  accel_commands(GEMMINI0, {
    status = gemmini_capture_conv_at(GEMMINI0.control, job(AUTO_LINK_STAGE_CONV1, index), block->input_rows, block->input_columns, block->input_channels, block->channels, KERNEL, block->stride, 0, 1,
                                     TILE_ROWS + 2 * PADDING, block->columns, (const elem_t *)block_input(index), weights1[index], NULL, first, NO_ACTIVATION, ACC_SCALE_IDENTITY);
  });
  if (status != 0) {
    return status;
  }
  accel_commands(GEMMINI0, {
    status = gemmini_capture_conv_at(GEMMINI0.control, job(AUTO_LINK_STAGE_PROJECTION, index), block->input_rows, block->input_columns, block->input_channels, block->channels, 1, block->stride, 0, 1,
                                     TILE_ROWS, block->columns, (const elem_t *)block_input(index), projection[index], NULL, first, NO_ACTIVATION, ACC_SCALE_IDENTITY);
  });
  if (status != 0) {
    return status;
  }
  accel_commands(GEMMINI1, {
    status = gemmini_capture_conv_at(GEMMINI1.control, job(AUTO_LINK_STAGE_CONV2, index), block->rows, block->columns, block->channels, block->channels, KERNEL, 1, PADDING, 1, TILE_ROWS,
                                     block->columns, (const elem_t *)CGRA0.spm, weights2[index], NULL, second, NO_ACTIVATION, ACC_SCALE_IDENTITY);
  });
  return status;
}

static int configure_cgra(void) {
  static const cgra_link_symbol_t relu[RELU_RUNTIME_SYMBOL_COUNT] = {
      [RELU_RUNTIME_SYMBOL_INPUT0] = {0, HALO_WORDS, CGRA_LINK_SLOT},
      [RELU_RUNTIME_SYMBOL_OUTPUT] = {0, HALO_WORDS, CGRA_LINK_SLOT},
      [RELU_RUNTIME_SYMBOL_ELEMENTS] = {0, 0, CGRA_LINK_ELEMENTS},
  };
  int status = 0;
  accel_commands(CGRA0, { status = cgra_job_config_at(CGRA0.control, AUTO_LINK_JOB_RELU, &RELU_RUNTIME, relu); });
  if (status != 0) {
    return status;
  }
  for (unsigned index = 1; index < BLOCKS; ++index) {
    const cgra_link_symbol_t add[ADD_RELU_RUNTIME_SYMBOL_COUNT] = {
        [ADD_RELU_RUNTIME_SYMBOL_INPUT0] = {0, CORE_WORDS, CGRA_LINK_SLOT},
        [ADD_RELU_RUNTIME_SYMBOL_INPUT1] = {SKIP_WORD, CORE_WORDS, CGRA_LINK_SLOT},
        [ADD_RELU_RUNTIME_SYMBOL_OUTPUT] = {output_word(index), tile_words(&blocks[index]), CGRA_LINK_TILE_ID},
        [ADD_RELU_RUNTIME_SYMBOL_ELEMENTS] = {0, 0, CGRA_LINK_ELEMENTS},
    };
    accel_commands(CGRA1, { status = cgra_job_config_at(CGRA1.control, job(AUTO_LINK_STAGE_ADD, index), &ADD_RELU_RUNTIME, add); });
    if (status != 0) {
      return status;
    }
  }
  return 0;
}

static void configure_links(unsigned index) {
  const block_t *block = &blocks[index];
  const uintptr_t first = publication(GEMMINI0, AUTO_LINK_GEMMINI0_BUFFER_SLOTS, HALO_WORDS) - GEMMINI0.spm;
  const uintptr_t second = publication(GEMMINI1, AUTO_LINK_GEMMINI1_BUFFER_SLOTS, CORE_WORDS) - GEMMINI1.spm;
  const unsigned pixel_bytes = block->channels * sizeof(elem_t);
  auto_link_transfer(AUTO_LINK_COPY_CONV1_RELU, first, 0, HALO_WORDS * sizeof(elem_t), HALO_WORDS * sizeof(acc_t), pixel_bytes);
  auto_link_transfer(AUTO_LINK_COPY_RELU_CONV2, 0, 0, HALO_WORDS * sizeof(elem_t), 0, pixel_bytes);
  auto_link_transfer(AUTO_LINK_COPY_PROJECTION_ADD, first, SKIP_WORD * sizeof(acc_t), HALO_WORDS * sizeof(elem_t), CORE_WORDS * sizeof(acc_t), pixel_bytes);
  auto_link_transfer(AUTO_LINK_COPY_CONV2_ADD, second, 0, CORE_WORDS * sizeof(elem_t), CORE_WORDS * sizeof(acc_t), pixel_bytes);
  auto_link_region(AUTO_LINK_STAGE_CONV1, block->rows, block->columns, 1, 1, PADDING, PADDING, PADDING, PADDING);
  auto_link_region(AUTO_LINK_STAGE_RELU, block->rows, block->columns, 1, 1, PADDING, PADDING, PADDING, PADDING);
  auto_link_tiles(block->rows, block->columns, TILE_ROWS, block->columns);
  for (unsigned stage = 0; stage < STAGES; ++stage) {
    auto_link_job(stage, job(stage, index));
  }
  auto_link_run_config(index - 1);
}

static int configure_hybrid(void) {
  if (configure_cgra() != 0) {
    return 1;
  }
  for (unsigned index = 1; index < BLOCKS; ++index) {
    if (configure_gemmini(index) != 0) {
      return 1;
    }
    configure_links(index);
  }
  cgra_dma_memory_fence();
  return 0;
}

static int run_hybrid(unsigned index) {
  uint32_t seen = 0;
  int failures = 0;
  auto_link_run(index - 1, 1);
  for (unsigned count = 0; count < STAGES; ++count) {
    const cgra_link_result_t result = cgra_link_wait_at(CGRA0.control);
    const uint32_t bit = result.stage < STAGES ? UINT32_C(1) << result.stage : 0;
    if (bit == 0 || (seen & bit) != 0 || result.job != job(result.stage, index) || result.status != AUTO_LINK_STATUS_SUCCESS || result.detail != 0 || result.data != 0) {
      printf("ResNet result block=%u stage=%u job=%u status=%u detail=%u data=%u\n", index, result.stage, result.job, result.status, result.detail, result.data);
      ++failures;
    }
    seen |= bit;
  }
  while (*(volatile uint32_t *)(AUTO_LINK_BASE + AUTO_LINK_RUNNING)) {
  }
  cgra_dma_memory_fence();
  timing_t *timing = &timings[index];
  timing->fabric = *(volatile uint64_t *)(AUTO_LINK_BASE + AUTO_LINK_CYCLES);
  timing->overlap = *(volatile uint64_t *)(AUTO_LINK_BASE + AUTO_LINK_OVERLAP);
  timing->peak = *(volatile uint64_t *)(AUTO_LINK_BASE + AUTO_LINK_PEAK_ACTIVE);
  return failures + (seen != (UINT32_C(1) << STAGES) - 1);
}

static int run_pool(void) {
  const block_t *block = &blocks[BLOCKS - 1];
  uint32_t status = 0;
  accel_commands(POOL, {
    pool_config_input(CGRA1.spm + output_word(BLOCKS - 1), block->rows, block->columns, block->channels);
    pool_config_output(pool_address());
    pool_config_window(POOL_MODE_AVERAGE, block->rows, block->columns, block->rows, block->columns, 0, 0);
    pool_start();
    status = pool_wait();
  });
  if (status != POOL_STATUS_SUCCESS) {
    printf("ResNet Pool status=%u\n", status);
    return 1;
  }
  return 0;
}

static void run_fc(void) {
  const uint32_t acc_write = UINT32_C(1) << (ADDR_LEN - 1);
  const uint32_t acc_add = UINT32_C(3) << (ADDR_LEN - 2);
  const uint32_t acc_read = acc_write | (UINT32_C(1) << (ADDR_LEN - 3));
  accel_commands(GEMMINI0, {
    gemmini_config_ex(WEIGHT_STATIONARY, NO_ACTIVATION, 0);
    gemmini_extended_config_st(CLASSES * sizeof(acc_t), NO_ACTIVATION, ACC_SCALE_IDENTITY);
    gemmini_extended3_config_ld(FC_CHANNELS * sizeof(elem_t), MVIN_SCALE_IDENTITY, false, 0);
    gemmini_extended3_config_ld(CLASSES * sizeof(elem_t), MVIN_SCALE_IDENTITY, false, 1);
    gemmini_extended3_config_ld(CLASSES * sizeof(acc_t), MVIN_SCALE_IDENTITY, false, 2);
    gemmini_extended_mvin3(fc_bias, acc_write, CLASSES, 1);
    for (unsigned channel = 0; channel < FC_CHANNELS; channel += DIM) {
      gemmini_extended_mvin((const void *)(pool_address() + channel), FC_A_ROW + channel, DIM, 1);
      gemmini_extended_mvin2(fc_weights + channel * CLASSES, FC_B_ROW + channel, CLASSES, DIM);
      gemmini_extended_preload(FC_B_ROW + channel, acc_add, CLASSES, DIM, CLASSES, 1);
      gemmini_extended_compute_preloaded(FC_A_ROW + channel, GARBAGE_ADDR, DIM, 1, CLASSES, 1);
    }
    gemmini_extended_mvout(logits, acc_read, CLASSES, 1);
  });
  gemmini_fence();
}

static int verify_tensor(const char *name, uintptr_t address, const int8_t *expected, unsigned elements) {
  const volatile uint64_t *actual = (const volatile uint64_t *)address;
  int failures = 0;
  for (unsigned index = 0; index < elements; index += sizeof(uint64_t)) {
    const uint64_t values = actual[index / sizeof(uint64_t)];
    uint64_t reference;
    memcpy(&reference, expected + index, sizeof(reference));
    if (values == reference) {
      continue;
    }
    for (unsigned lane = 0; lane < sizeof(uint64_t); ++lane) {
      const elem_t value = (elem_t)(values >> (lane * 8));
      if (value != expected[index + lane]) {
        if (failures < 8) {
          printf("ResNet %s element=%u actual=%d expected=%d\n", name, index + lane, (int)value, (int)expected[index + lane]);
        }
        ++failures;
      }
    }
  }
  printf("ResNet %s checked=%u mismatches=%d\n", name, elements, failures);
  return failures;
}

static int verify_output(void) {
  int failures = verify_tensor("stem", CGRA0.spm + STEM_WORD, expected_stem, INPUT_H * INPUT_W * STEM_CHANNELS);
  static const char *const names[BLOCKS] = {"block0", "block1", "block2"};
  for (unsigned index = 0; index < BLOCKS; ++index) {
    const block_t *block = &blocks[index];
    failures += verify_tensor(names[index], CGRA1.spm + output_word(index), expected[index], block->rows * block->columns * block->channels);
  }
  failures += verify_tensor("pool", pool_address(), expected_pool, FC_CHANNELS);
  for (unsigned index = 0; index < CLASSES; ++index) {
    printf("ResNet logit=%u actual=%d expected=%d\n", index, (int)logits[index], (int)expected_logits[index]);
    failures += logits[index] != expected_logits[index];
  }
  return failures;
}

int main(int argc, char **argv) {
  const unsigned mode = argc > 1 ? (unsigned)atoi(argv[1]) : MANUAL;
  if (mode > HYBRID) {
    printf("ResNet mode must be 0 (Manual) or 1 (Hybrid)\n");
    return 1;
  }
  unsigned long begin = cycles();
  init_inputs();
  load_input();
  if (mode == HYBRID && configure_hybrid() != 0) {
    printf("ResNet-8 Hybrid: FAIL (configuration)\n");
    return 1;
  }
  const unsigned long setup_cycles = cycles() - begin;
  unsigned long execution_cycles = 0;
  printf("ResNet-8 %s ready\n", modes[mode]);
  begin = cycles();
  int failures = run_stem();
  const unsigned long stem_cycles = cycles() - begin;
  execution_cycles += stem_cycles;
  printf("ResNet-8 %s stem complete cycles=%lu\n", modes[mode], stem_cycles);
  for (unsigned index = 0; index < BLOCKS && failures == 0; ++index) {
    begin = cycles();
    failures += mode == HYBRID && index != 0 ? run_hybrid(index) : run_manual(index);
    timings[index].execution = cycles() - begin;
    execution_cycles += timings[index].execution;
    printf("ResNet-8 %s block=%u complete cycles=%lu\n", modes[mode], index, timings[index].execution);
  }
  unsigned long pool_cycles = 0, fc_cycles = 0;
  if (failures == 0) {
    begin = cycles();
    failures += run_pool();
    pool_cycles = cycles() - begin;
    execution_cycles += pool_cycles;
  }
  if (failures == 0) {
    begin = cycles();
    run_fc();
    fc_cycles = cycles() - begin;
    execution_cycles += fc_cycles;
    failures += verify_output();
  }
  for (unsigned index = 0; index < BLOCKS; ++index) {
    const timing_t *timing = &timings[index];
    printf("ResNet-8 %s block=%u execution_cycles=%lu fabric_cycles=%lu overlap=%lu peak=%lu\n", modes[mode], index, timing->execution, timing->fabric, timing->overlap, timing->peak);
  }
  printf("ResNet-8 %s setup_cycles=%lu execution_cycles=%lu pool_cycles=%lu fc_cycles=%lu\n", modes[mode], setup_cycles, execution_cycles, pool_cycles, fc_cycles);
  printf("ResNet-8 %s: %s\n", modes[mode], failures == 0 ? "PASS" : "FAIL");
  return failures != 0;
}
