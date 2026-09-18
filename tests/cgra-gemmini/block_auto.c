#include "accel_generated.h"
#include "auto_link.h"
#include "cgra_link.h"
#include "gemmini.h"
#include "gemmini_conv.h"
#include "generated/cgra_add_relu_runtime_fast_api.h"
#include "generated/cgra_relu_runtime_fast_api.h"

#include <stdint.h>
#include <stdio.h>

enum {
  INPUT_H = 8,
  INPUT_W = 10,
  INPUT_CHANNELS = 4,
  CHANNELS = 8,
  KERNEL = 3,
  STRIDE = 2,
  PADDING = 1,
  HEIGHT = (INPUT_H + 2 * PADDING - KERNEL) / STRIDE + 1,
  WIDTH = (INPUT_W + 2 * PADDING - KERNEL) / STRIDE + 1,
  TILE_H = 2,
  TILE_W = 2,
  TILES = ((HEIGHT + TILE_H - 1) / TILE_H) * ((WIDTH + TILE_W - 1) / TILE_W),
  HALO_WORDS = (TILE_H + 2 * PADDING) * (TILE_W + 2 * PADDING) * CHANNELS,
  CORE_WORDS = TILE_H * TILE_W * CHANNELS,
  SKIP_WORD = AUTO_LINK_CGRA1_BUFFER_SLOTS * CORE_WORDS,
  OUTPUT_WORD = 2 * AUTO_LINK_CGRA1_BUFFER_SLOTS * CORE_WORDS,
};

static elem_t input[INPUT_H][INPUT_W][INPUT_CHANNELS] row_align(1);
static elem_t weights1[KERNEL][KERNEL][INPUT_CHANNELS][CHANNELS] row_align(1);
static elem_t weights2[KERNEL][KERNEL][CHANNELS][CHANNELS] row_align(1);
static elem_t projection[INPUT_CHANNELS][CHANNELS] row_align(1);
static elem_t intermediate[HEIGHT][WIDTH][CHANNELS];
static elem_t main_output[HEIGHT][WIDTH][CHANNELS];
static elem_t skip_output[HEIGHT][WIDTH][CHANNELS];
static acc_t expected[HEIGHT][WIDTH][CHANNELS];

static unsigned long cycles(void) {
  unsigned long value;
  __asm__ volatile("rdcycle %0" : "=r"(value)::"memory");
  return value;
}

static elem_t requant(int32_t value) {
  if (value > INT8_MAX) {
    return INT8_MAX;
  }
  return value < INT8_MIN ? INT8_MIN : (elem_t)value;
}

static void init_inputs(void) {
  for (unsigned row = 0; row < INPUT_H; ++row) {
    for (unsigned column = 0; column < INPUT_W; ++column) {
      for (unsigned channel = 0; channel < INPUT_CHANNELS; ++channel) {
        input[row][column][channel] = (elem_t)((int)((row * 7 + column * 3 + channel * 2) % 7) - 3);
      }
    }
  }
  for (unsigned y = 0; y < KERNEL; ++y) {
    for (unsigned x = 0; x < KERNEL; ++x) {
      for (unsigned in = 0; in < INPUT_CHANNELS; ++in) {
        for (unsigned out = 0; out < CHANNELS; ++out) {
          weights1[y][x][in][out] = (elem_t)((int)((y * 3 + x * 2 + in + out * 2) % 5) - 2);
        }
      }
      for (unsigned in = 0; in < CHANNELS; ++in) {
        for (unsigned out = 0; out < CHANNELS; ++out) {
          weights2[y][x][in][out] = (elem_t)((int)((y + x * 3 + in * 2 + out) % 3) - 1);
        }
      }
    }
  }
  for (unsigned in = 0; in < INPUT_CHANNELS; ++in) {
    for (unsigned out = 0; out < CHANNELS; ++out) {
      projection[in][out] = (elem_t)((int)((in * 3 + out) % 5) - 2);
    }
  }
}

static void reference(const elem_t *source, const elem_t *weights, elem_t *destination, int rows, int columns, unsigned channels, unsigned kernel, unsigned stride, int padding, int relu) {
  for (unsigned row = 0; row < HEIGHT; ++row) {
    for (unsigned column = 0; column < WIDTH; ++column) {
      for (unsigned out = 0; out < CHANNELS; ++out) {
        int32_t sum = 0;
        for (unsigned y = 0; y < kernel; ++y) {
          const int in_row = (int)(row * stride + y) - padding;
          if (in_row < 0 || in_row >= rows) {
            continue;
          }
          for (unsigned x = 0; x < kernel; ++x) {
            const int in_column = (int)(column * stride + x) - padding;
            if (in_column < 0 || in_column >= columns) {
              continue;
            }
            for (unsigned in = 0; in < channels; ++in) {
              const unsigned source_index = (in_row * columns + in_column) * channels + in;
              const unsigned weight_index = ((y * kernel + x) * channels + in) * CHANNELS + out;
              sum += source[source_index] * weights[weight_index];
            }
          }
        }
        destination[(row * WIDTH + column) * CHANNELS + out] = requant(relu && sum < 0 ? 0 : sum);
      }
    }
  }
}

static void init_expected(void) {
  reference(&input[0][0][0], &weights1[0][0][0][0], &intermediate[0][0][0], INPUT_H, INPUT_W, INPUT_CHANNELS, KERNEL, STRIDE, PADDING, 1);
  reference(&intermediate[0][0][0], &weights2[0][0][0][0], &main_output[0][0][0], HEIGHT, WIDTH, CHANNELS, KERNEL, 1, PADDING, 0);
  reference(&input[0][0][0], &projection[0][0], &skip_output[0][0][0], INPUT_H, INPUT_W, INPUT_CHANNELS, 1, STRIDE, 0, 0);
  for (unsigned row = 0; row < HEIGHT; ++row) {
    for (unsigned column = 0; column < WIDTH; ++column) {
      for (unsigned channel = 0; channel < CHANNELS; ++channel) {
        const int32_t sum = main_output[row][column][channel] + skip_output[row][column][channel];
        expected[row][column][channel] = sum > 0 ? sum : 0;
      }
    }
  }
}

static uintptr_t input_address(void) { return GEMMINI0.spm + GEMMINI0.spm_bytes / 2; }

static uintptr_t weights1_address(void) { return input_address() + sizeof(input); }

static uintptr_t projection_address(void) { return weights1_address() + sizeof(weights1); }

static uintptr_t weights2_address(void) { return GEMMINI1.spm + GEMMINI1.spm_bytes / 2; }

static uintptr_t publication(accel_t device, unsigned slots, unsigned words) { return device.spm + device.spm_bytes - slots * words * sizeof(elem_t); }

static void preload(uintptr_t address, const elem_t *source, unsigned count) {
  volatile elem_t *destination = (volatile elem_t *)address;
  for (unsigned index = 0; index < count; ++index) {
    destination[index] = source[index];
  }
}

static void load_inputs(void) {
  accel_commands(GEMMINI0, { gemmini_flush(0); });
  accel_commands(GEMMINI1, { gemmini_flush(0); });
  // Native loop_conv uses the lower half; retain compact input and weights in the upper half.
  preload(input_address(), &input[0][0][0], sizeof(input));
  preload(weights1_address(), &weights1[0][0][0][0], sizeof(weights1));
  preload(projection_address(), &projection[0][0], sizeof(projection));
  preload(weights2_address(), &weights2[0][0][0][0], sizeof(weights2));
  __asm__ volatile("fence rw, rw" ::: "memory");
}

static int verify_output(void) {
  const volatile acc_t *output = (const volatile acc_t *)CGRA1.spm + OUTPUT_WORD;
  unsigned tile = 0;
  int failures = 0;
  for (unsigned row = 0; row < HEIGHT; row += TILE_H) {
    for (unsigned column = 0; column < WIDTH; column += TILE_W) {
      unsigned index = tile * CORE_WORDS;
      for (unsigned y = row; y < row + TILE_H && y < HEIGHT; ++y) {
        for (unsigned x = column; x < column + TILE_W && x < WIDTH; ++x) {
          for (unsigned channel = 0; channel < CHANNELS; ++channel) {
            const acc_t value = output[index++];
            if (value != expected[y][x][channel]) {
              printf("Block mismatch tile=%u row=%u column=%u channel=%u actual=%d expected=%d\n", tile, y, x, channel, (int)value, (int)expected[y][x][channel]);
              ++failures;
            }
          }
        }
      }
      ++tile;
    }
  }
  return failures;
}

static int configure_gemmini(void) {
  int status = 0;
  const elem_t *source = (const elem_t *)input_address();
  elem_t *first_output = (elem_t *)publication(GEMMINI0, AUTO_LINK_GEMMINI0_BUFFER_SLOTS, HALO_WORDS);
  elem_t *second_output = (elem_t *)publication(GEMMINI1, AUTO_LINK_GEMMINI1_BUFFER_SLOTS, CORE_WORDS);
  accel_commands(GEMMINI0, {
    status = gemmini_capture_conv_at(GEMMINI0.control, AUTO_LINK_JOB_CONV1, INPUT_H, INPUT_W, INPUT_CHANNELS, CHANNELS, KERNEL, STRIDE, PADDING, 1, TILE_H + 2 * PADDING, TILE_W + 2 * PADDING, source,
                                     (const elem_t *)weights1_address(), NULL, first_output, NO_ACTIVATION, ACC_SCALE_IDENTITY);
  });
  if (status != 0) {
    return status;
  }
  accel_commands(GEMMINI0, {
    status = gemmini_capture_conv_at(GEMMINI0.control, AUTO_LINK_JOB_PROJECTION, INPUT_H, INPUT_W, INPUT_CHANNELS, CHANNELS, 1, STRIDE, 0, 1, TILE_H, TILE_W, source,
                                     (const elem_t *)projection_address(), NULL, first_output, NO_ACTIVATION, ACC_SCALE_IDENTITY);
  });
  if (status != 0) {
    return status;
  }
  accel_commands(GEMMINI1, {
    status = gemmini_capture_conv_at(GEMMINI1.control, AUTO_LINK_JOB_CONV2, HEIGHT, WIDTH, CHANNELS, CHANNELS, KERNEL, 1, PADDING, 1, TILE_H, TILE_W, (const elem_t *)CGRA0.spm,
                                     (const elem_t *)weights2_address(), NULL, second_output, NO_ACTIVATION, ACC_SCALE_IDENTITY);
  });
  return status;
}

static int configure_cgra(void) {
  static const cgra_link_symbol_t relu[RELU_RUNTIME_SYMBOL_COUNT] = {
      [RELU_RUNTIME_SYMBOL_INPUT0] = {0, HALO_WORDS, CGRA_LINK_SLOT},
      [RELU_RUNTIME_SYMBOL_OUTPUT] = {0, HALO_WORDS, CGRA_LINK_SLOT},
      [RELU_RUNTIME_SYMBOL_ELEMENTS] = {0, 0, CGRA_LINK_ELEMENTS},
  };
  static const cgra_link_symbol_t add[ADD_RELU_RUNTIME_SYMBOL_COUNT] = {
      [ADD_RELU_RUNTIME_SYMBOL_INPUT0] = {0, CORE_WORDS, CGRA_LINK_SLOT},
      [ADD_RELU_RUNTIME_SYMBOL_INPUT1] = {SKIP_WORD, CORE_WORDS, CGRA_LINK_SLOT},
      [ADD_RELU_RUNTIME_SYMBOL_OUTPUT] = {OUTPUT_WORD, CORE_WORDS, CGRA_LINK_TILE_ID},
      [ADD_RELU_RUNTIME_SYMBOL_ELEMENTS] = {0, 0, CGRA_LINK_ELEMENTS},
  };
  int status = 0;
  accel_commands(CGRA0, { status = cgra_job_config_at(CGRA0.control, AUTO_LINK_JOB_RELU, &RELU_RUNTIME, relu); });
  if (status != 0) {
    return status;
  }
  accel_commands(CGRA1, { status = cgra_job_config_at(CGRA1.control, AUTO_LINK_JOB_ADD, &ADD_RELU_RUNTIME, add); });
  return status;
}

static void configure_links(void) {
  const uintptr_t first = publication(GEMMINI0, AUTO_LINK_GEMMINI0_BUFFER_SLOTS, HALO_WORDS) - GEMMINI0.spm;
  const uintptr_t second = publication(GEMMINI1, AUTO_LINK_GEMMINI1_BUFFER_SLOTS, CORE_WORDS) - GEMMINI1.spm;
  auto_link_transfer(AUTO_LINK_COPY_CONV1_RELU, first, 0, HALO_WORDS * sizeof(elem_t), HALO_WORDS * sizeof(acc_t), CHANNELS * sizeof(elem_t));
  auto_link_transfer(AUTO_LINK_COPY_RELU_CONV2, 0, 0, HALO_WORDS * sizeof(elem_t), 0, CHANNELS * sizeof(elem_t));
  auto_link_transfer(AUTO_LINK_COPY_PROJECTION_ADD, first, SKIP_WORD * sizeof(acc_t), HALO_WORDS * sizeof(elem_t), CORE_WORDS * sizeof(acc_t), CHANNELS * sizeof(elem_t));
  auto_link_transfer(AUTO_LINK_COPY_CONV2_ADD, second, 0, CORE_WORDS * sizeof(elem_t), CORE_WORDS * sizeof(acc_t), CHANNELS * sizeof(elem_t));
  auto_link_region(AUTO_LINK_STAGE_CONV1, HEIGHT, WIDTH, 1, 1, PADDING, PADDING, PADDING, PADDING);
  auto_link_region(AUTO_LINK_STAGE_RELU, HEIGHT, WIDTH, 1, 1, PADDING, PADDING, PADDING, PADDING);
  auto_link_tiles(HEIGHT, WIDTH, TILE_H, TILE_W);
}

static int verify_results(void) {
  static const uint32_t jobs[] = {
      [AUTO_LINK_STAGE_CONV1] = AUTO_LINK_JOB_CONV1, [AUTO_LINK_STAGE_PROJECTION] = AUTO_LINK_JOB_PROJECTION,
      [AUTO_LINK_STAGE_RELU] = AUTO_LINK_JOB_RELU,   [AUTO_LINK_STAGE_CONV2] = AUTO_LINK_JOB_CONV2,
      [AUTO_LINK_STAGE_ADD] = AUTO_LINK_JOB_ADD,
  };
  const unsigned stages = sizeof(jobs) / sizeof(jobs[0]);
  uint32_t seen = 0;
  int failures = 0;
  for (unsigned index = 0; index < stages; ++index) {
    const cgra_link_result_t result = cgra_link_wait_at(CGRA0.control);
    const uint32_t bit = result.stage < stages ? UINT32_C(1) << result.stage : 0;
    if (bit == 0 || (seen & bit) != 0 || result.job != jobs[result.stage] || result.status != AUTO_LINK_STATUS_SUCCESS || result.detail != 0 || result.data != 0) {
      printf("Block result stage=%u job=%u status=%u detail=%u data=%u\n", result.stage, result.job, result.status, result.detail, result.data);
      ++failures;
    }
    seen |= bit;
  }
  return failures + (seen != (UINT32_C(1) << stages) - 1);
}

int main(void) {
  init_inputs();
  init_expected();
  load_inputs();
  if (configure_gemmini() != 0 || configure_cgra() != 0) {
    printf("Projected residual Auto: FAIL (configuration)\n");
    return 1;
  }
  configure_links();
  __asm__ volatile("fence rw, rw" ::: "memory");
  const unsigned long begin = cycles();
  auto_link_input_ready();
  int failures = verify_results();
  while (*(volatile uint32_t *)(AUTO_LINK_BASE + AUTO_LINK_RUNNING)) {
  }
  __asm__ volatile("fence rw, rw" ::: "memory");
  const unsigned long cpu_cycles = cycles() - begin;
  const unsigned long fabric_cycles = *(volatile uint64_t *)(AUTO_LINK_BASE + AUTO_LINK_CYCLES);
  const unsigned long overlap = *(volatile uint64_t *)(AUTO_LINK_BASE + AUTO_LINK_OVERLAP);
  const unsigned long peak = *(volatile uint64_t *)(AUTO_LINK_BASE + AUTO_LINK_PEAK_ACTIVE);
  printf("Block Auto tiles=%u shape=%ux%u cycles=%lu cpu_cycles=%lu overlap=%lu peak=%lu\n", TILES, TILE_H, TILE_W, fabric_cycles, cpu_cycles, overlap, peak);
  if (overlap == 0) {
    printf("Block tiles did not overlap across IPs\n");
    ++failures;
  }
  failures += verify_output();
  printf("Projected residual Auto: %s\n", failures == 0 ? "PASS" : "FAIL");
  return failures != 0;
}
