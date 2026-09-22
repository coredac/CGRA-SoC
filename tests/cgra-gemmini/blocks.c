#include "accel_generated.h"
#include "auto_link.h"
#include "cgra_link.h"
#include "gemmini.h"
#include "gemmini_conv.h"
#include "generated/blocks_data.h"
#include "generated/cgra_add_relu_runtime_fast_api.h"
#include "generated/cgra_relu_runtime_fast_api.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

enum {
  CPU_MODE,
  CACHED_MODE,
  AUTO_MODE,
  SKIP_WORD = AUTO_LINK_CGRA1_BUFFER_SLOTS * CORE_WORDS,
  OUTPUT_WORD = 2 * AUTO_LINK_CGRA1_BUFFER_SLOTS * CORE_WORDS,
};

static elem_t input[INPUT_H][INPUT_W][INPUT_CHANNELS] row_align(1);

static const uint32_t jobs[] = {
    [AUTO_LINK_STAGE_CONV1] = AUTO_LINK_JOB_CONV1, [AUTO_LINK_STAGE_PROJECTION] = AUTO_LINK_JOB_PROJECTION, [AUTO_LINK_STAGE_RELU] = AUTO_LINK_JOB_RELU, [AUTO_LINK_STAGE_CONV2] = AUTO_LINK_JOB_CONV2,
    [AUTO_LINK_STAGE_ADD] = AUTO_LINK_JOB_ADD,
};

enum { STAGES = sizeof(jobs) / sizeof(jobs[0]) };

static cgra_link_result_t results[BLOCKS][STAGES];
static unsigned mode;
static unsigned completed;
static const char *const modes[] = {"CPU", "Cached", "Auto"};

typedef struct {
  unsigned long setup, execution, boundary, fabric, overlap, peak;
} timing_t;

static timing_t timings[BLOCKS];

static unsigned job(unsigned stage, unsigned block) { return jobs[stage] + (mode != CPU_MODE && stage != AUTO_LINK_STAGE_RELU ? block : 0); }

static unsigned long cycles(void) {
  unsigned long value;
  __asm__ volatile("rdcycle %0" : "=r"(value)::"memory");
  return value;
}

static unsigned tile_rows(const block_t *block, unsigned row) {
  const unsigned remaining = block->rows - row;
  return remaining < TILE_ROWS ? remaining : TILE_ROWS;
}

static unsigned tile_words(const block_t *block) { return tile_rows(block, 0) * block->columns * block->channels; }

static unsigned output_word(unsigned index) {
  unsigned word = OUTPUT_WORD;
  for (unsigned block = 0; block < index; ++block) {
    word += ((blocks[block].rows + TILE_ROWS - 1) / TILE_ROWS) * tile_words(&blocks[block]);
  }
  return word;
}

static void init_inputs(void) {
  for (unsigned row = 0; row < INPUT_H; ++row) {
    for (unsigned column = 0; column < INPUT_W; ++column) {
      for (unsigned channel = 0; channel < INPUT_CHANNELS; ++channel) {
        input[row][column][channel] = (elem_t)(3 * ((int)((row * 7 + column * 3 + channel * 2) % 7) - 3));
      }
    }
  }
}

static uintptr_t input_address(void) { return GEMMINI0.spm + GEMMINI0.spm_bytes / 2; }

static uintptr_t publication(accel_t device, unsigned slots, unsigned words) { return device.spm + device.spm_bytes - slots * words * sizeof(elem_t); }

static void load_input(void) {
  accel_commands(GEMMINI0, { gemmini_flush(0); });
  accel_commands(GEMMINI1, { gemmini_flush(0); });
  volatile elem_t *destination = (volatile elem_t *)input_address();
  const elem_t *source = &input[0][0][0];
  for (unsigned index = 0; index < sizeof(input); ++index) {
    destination[index] = source[index];
  }
  __asm__ volatile("fence rw, rw" ::: "memory");
}

static int configure_gemmini(unsigned index) {
  const block_t *block = &blocks[index];
  const unsigned halo_rows = block->rows < TILE_ROWS + 2 * PADDING ? block->rows : TILE_ROWS + 2 * PADDING;
  const elem_t *source = (const elem_t *)(index == 0 ? input_address() : CGRA1.spm + output_word(index - 1));
  elem_t *first = (elem_t *)publication(GEMMINI0, AUTO_LINK_GEMMINI0_BUFFER_SLOTS, HALO_WORDS);
  elem_t *second = (elem_t *)publication(GEMMINI1, AUTO_LINK_GEMMINI1_BUFFER_SLOTS, CORE_WORDS);
  int status = 0;
  accel_commands(GEMMINI0, {
    status = gemmini_capture_conv_at(GEMMINI0.control, job(AUTO_LINK_STAGE_CONV1, index), block->input_rows, block->input_columns, block->input_channels, block->channels, KERNEL, STRIDE, PADDING, 1,
                                     halo_rows, block->columns, source, weights1[index], NULL, first, NO_ACTIVATION, ACC_SCALE_IDENTITY);
  });
  if (status != 0) {
    return status;
  }
  accel_commands(GEMMINI0, {
    status = gemmini_capture_conv_at(GEMMINI0.control, job(AUTO_LINK_STAGE_PROJECTION, index), block->input_rows, block->input_columns, block->input_channels, block->channels, 1, STRIDE, 0, 1,
                                     tile_rows(block, 0), block->columns, source, projection[index], NULL, first, NO_ACTIVATION, ACC_SCALE_IDENTITY);
  });
  if (status != 0) {
    return status;
  }
  accel_commands(GEMMINI1, {
    status = gemmini_capture_conv_at(GEMMINI1.control, job(AUTO_LINK_STAGE_CONV2, index), block->rows, block->columns, block->channels, block->channels, KERNEL, 1, PADDING, 1, tile_rows(block, 0),
                                     block->columns, (const elem_t *)CGRA0.spm, weights2[index], NULL, second, NO_ACTIVATION, ACC_SCALE_IDENTITY);
  });
  return status;
}

static int configure_relu(void) {
  static const cgra_link_symbol_t relu[RELU_RUNTIME_SYMBOL_COUNT] = {
      [RELU_RUNTIME_SYMBOL_INPUT0] = {0, HALO_WORDS, CGRA_LINK_SLOT},
      [RELU_RUNTIME_SYMBOL_OUTPUT] = {0, HALO_WORDS, CGRA_LINK_SLOT},
      [RELU_RUNTIME_SYMBOL_ELEMENTS] = {0, 0, CGRA_LINK_ELEMENTS},
  };
  int status = 0;
  accel_commands(CGRA0, { status = cgra_job_config_at(CGRA0.control, AUTO_LINK_JOB_RELU, &RELU_RUNTIME, relu); });
  return status;
}

static int configure_add(unsigned index) {
  const cgra_link_symbol_t add[ADD_RELU_RUNTIME_SYMBOL_COUNT] = {
      [ADD_RELU_RUNTIME_SYMBOL_INPUT0] = {0, CORE_WORDS, CGRA_LINK_SLOT},
      [ADD_RELU_RUNTIME_SYMBOL_INPUT1] = {SKIP_WORD, CORE_WORDS, CGRA_LINK_SLOT},
      [ADD_RELU_RUNTIME_SYMBOL_OUTPUT] = {output_word(index), tile_words(&blocks[index]), CGRA_LINK_TILE_ID},
      [ADD_RELU_RUNTIME_SYMBOL_ELEMENTS] = {0, 0, CGRA_LINK_ELEMENTS},
  };
  int status = 0;
  accel_commands(CGRA1, { status = cgra_job_config_at(CGRA1.control, job(AUTO_LINK_STAGE_ADD, index), &ADD_RELU_RUNTIME, add); });
  return status;
}

static void configure_links(const block_t *block) {
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
}

static int configure_block(unsigned index) {
  const unsigned long begin = cycles();
  if (configure_gemmini(index) != 0 || configure_add(index) != 0) {
    return 1;
  }
  configure_links(&blocks[index]);
  for (unsigned stage = 0; stage < STAGES; ++stage) {
    auto_link_job(stage, job(stage, index));
  }
  if (mode != CPU_MODE) {
    auto_link_run_config(index);
  }
  __asm__ volatile("fence rw, rw" ::: "memory");
  timings[index].setup = cycles() - begin;
  return 0;
}

static int collect_results(unsigned block) {
  int failures = 0;
  for (unsigned index = 0; index < STAGES; ++index) {
    results[block][index] = cgra_link_wait_at(CGRA0.control);
    failures += results[block][index].status != AUTO_LINK_STATUS_SUCCESS;
  }
  return failures;
}

static int verify_results(unsigned block) {
  uint32_t seen = 0;
  int failures = 0;
  for (unsigned index = 0; index < STAGES; ++index) {
    const cgra_link_result_t result = results[block][index];
    const uint32_t bit = result.stage < STAGES ? UINT32_C(1) << result.stage : 0;
    if (bit == 0 || (seen & bit) != 0 || result.job != job(result.stage, block) || result.status != AUTO_LINK_STATUS_SUCCESS || result.detail != 0 || result.data != 0) {
      printf("Blocks result block=%u stage=%u job=%u status=%u detail=%u data=%u\n", block, result.stage, result.job, result.status, result.detail, result.data);
      ++failures;
    }
    seen |= bit;
  }
  return failures + (seen != (UINT32_C(1) << STAGES) - 1);
}

static int verify_output(unsigned index) {
  const block_t *block = &blocks[index];
  const volatile elem_t *output = (const volatile elem_t *)(CGRA1.spm + output_word(index));
  unsigned tile = 0;
  int failures = 0;
  for (unsigned row = 0; row < block->rows; row += TILE_ROWS, ++tile) {
    const unsigned count = tile_rows(block, row) * block->columns * block->channels;
    const unsigned start = row * block->columns * block->channels;
    for (unsigned element = 0; element < count; ++element) {
      const elem_t value = output[tile * tile_words(block) + element];
      if (value != expected[index][start + element]) {
        printf("Blocks mismatch block=%u tile=%u element=%u actual=%d expected=%d\n", index, tile, element, (int)value, (int)expected[index][start + element]);
        ++failures;
      }
    }
  }
  return failures;
}

static void wait_idle(void) {
  while (*(volatile uint32_t *)(AUTO_LINK_BASE + AUTO_LINK_RUNNING)) {
  }
  __asm__ volatile("fence rw, rw" ::: "memory");
}

static void read_counters(timing_t *timing) {
  timing->fabric = *(volatile uint64_t *)(AUTO_LINK_BASE + AUTO_LINK_CYCLES);
  timing->overlap = *(volatile uint64_t *)(AUTO_LINK_BASE + AUTO_LINK_OVERLAP);
  timing->peak = *(volatile uint64_t *)(AUTO_LINK_BASE + AUTO_LINK_PEAK_ACTIVE);
}

static int run_triggered(void) {
  unsigned long previous = 0;
  for (unsigned index = 0; index < BLOCKS; ++index) {
    timing_t *timing = &timings[index];
    if (mode == CPU_MODE && index != 0 && configure_block(index) != 0) {
      return 1;
    }
    const unsigned long begin = cycles();
    if (index != 0) {
      timing->boundary = begin - previous;
    }
    if (mode == CPU_MODE) {
      auto_link_input_ready();
    } else {
      auto_link_run(index, 1);
    }
    const int failures = collect_results(index);
    wait_idle();
    previous = cycles();
    timing->execution = previous - begin;
    read_counters(timing);
    ++completed;
    if (failures != 0) {
      return failures;
    }
  }
  return 0;
}

static int run_auto(timing_t *timing) {
  int failures = 0;
  auto_link_run(0, BLOCKS);
  for (unsigned index = 0; index < BLOCKS; ++index) {
    failures += collect_results(index);
    ++completed;
    if (failures != 0) {
      break;
    }
  }
  wait_idle();
  read_counters(timing);
  return failures;
}

static int report_blocks(void) {
  int failures = 0;
  for (unsigned index = 0; index < completed; ++index) {
    const block_t *block = &blocks[index];
    const timing_t *timing = &timings[index];
    printf("Blocks %s block=%u tiles=%u shape=%ux%u output_word=%u configure_cycles=%lu\n", modes[mode], index, (block->rows + TILE_ROWS - 1) / TILE_ROWS, TILE_ROWS, block->columns,
           output_word(index), timing->setup);
    if (mode != AUTO_MODE) {
      printf("Blocks %s block=%u boundary_cycles=%lu execution_cycles=%lu fabric_cycles=%lu overlap=%lu peak=%lu\n", modes[mode], index, timing->boundary, timing->execution, timing->fabric,
             timing->overlap, timing->peak);
      if (timing->overlap == 0) {
        printf("Block %u tiles did not overlap across IPs\n", index);
        ++failures;
      }
    }
    failures += verify_results(index);
    failures += verify_output(index);
  }
  return failures;
}

int main(int argc, char **argv) {
  mode = argc > 1 ? (unsigned)atoi(argv[1]) : CPU_MODE;
  init_inputs();
  load_input();
  unsigned long begin = cycles();
  if (configure_relu() != 0) {
    printf("Projected residual blocks %s: FAIL (ReLU configuration)\n", modes[mode]);
    return 1;
  }
  const unsigned count = mode == CPU_MODE ? 1 : BLOCKS;
  for (unsigned index = 0; index < count; ++index) {
    if (configure_block(index) != 0) {
      printf("Projected residual blocks %s: FAIL (block %u configuration)\n", modes[mode], index);
      return 1;
    }
  }
  const unsigned long setup_cycles = cycles() - begin;
  printf("Blocks %s ready\n", modes[mode]);
  timing_t automatic = {0};
  begin = cycles();
  int failures = mode == AUTO_MODE ? run_auto(&automatic) : run_triggered();
  const unsigned long execution_cycles = cycles() - begin;
  failures += report_blocks();
  if (mode == AUTO_MODE) {
    printf("Blocks Auto last_block fabric_cycles=%lu overlap=%lu peak=%lu\n", automatic.fabric, automatic.overlap, automatic.peak);
    failures += automatic.overlap == 0;
  }
  if (mode != AUTO_MODE) {
    unsigned long boundary_cycles = 0;
    for (unsigned index = 0; index < completed; ++index) {
      boundary_cycles += timings[index].boundary;
    }
    printf("Blocks %s boundary_cycles=%lu\n", modes[mode], boundary_cycles);
  }
  printf("Blocks %s initial_setup_cycles=%lu chain_cycles=%lu\n", modes[mode], setup_cycles, execution_cycles);
  printf("Projected residual blocks %s: %s\n", modes[mode], failures == 0 ? "PASS" : "FAIL");
  return failures != 0;
}
