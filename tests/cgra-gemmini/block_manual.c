#include "accel_generated.h"
#include "auto_link.h"
#include "cgra_dma.h"
#include "gemmini.h"
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

typedef struct {
  int row;
  int column;
  int rows;
  int columns;
} region_t;

static cgra_packet_t relu_config[RELU_RUNTIME_FAST_CONFIG_PACKET_COUNT];
static cgra_packet_t add_config[ADD_RELU_RUNTIME_FAST_CONFIG_PACKET_COUNT];

static int minimum(int a, int b) { return a < b ? a : b; }

static int maximum(int a, int b) { return a > b ? a : b; }

static region_t halo(region_t tile) {
  const int row = maximum(tile.row - PADDING, 0);
  const int column = maximum(tile.column - PADDING, 0);
  const int end_row = minimum(tile.row + tile.rows + PADDING, HEIGHT);
  const int end_column = minimum(tile.column + tile.columns + PADDING, WIDTH);
  return (region_t){row, column, end_row - row, end_column - column};
}

static void run_conv(accel_t device, region_t tile, region_t view, int input_rows, int input_columns, int channels, int kernel, int stride, int padding, uintptr_t source, uintptr_t weights,
                     uintptr_t destination) {
  const int first_row = tile.row * stride - padding;
  const int first_column = tile.column * stride - padding;
  // Native loop_conv includes stride-minus-one extra input positions.
  const int end_row = (tile.row + tile.rows) * stride + kernel - 1 - padding;
  const int end_column = (tile.column + tile.columns) * stride + kernel - 1 - padding;
  const int top = maximum(-first_row, 0);
  const int left = maximum(-first_column, 0);
  const int bottom = maximum(end_row - input_rows, 0);
  const int right = maximum(end_column - input_columns, 0);
  const unsigned offset = ((maximum(first_row, 0) - view.row) * view.columns + maximum(first_column, 0) - view.column) * channels;
  const elem_t *input_pointer = (const elem_t *)source + offset;
  accel_commands(device, {
    gemmini_extended_config_st(CHANNELS * sizeof(elem_t), NO_ACTIVATION, ACC_SCALE_IDENTITY);
    gemmini_extended3_config_ex(WEIGHT_STATIONARY, 0, 0, 0, 1, stride, false, false, false);
    sp_tiled_conv(1, view.rows, view.columns, channels, CHANNELS, tile.rows, tile.columns, tile.rows, tile.columns, stride, padding, kernel, 1, channels, CHANNELS, CHANNELS, 1, 1, 0, 1, tile.rows,
                  tile.columns, CHANNELS, kernel, kernel, channels, left, right, top, bottom, 0, 0, 0, 0, input_pointer, (const elem_t *)weights, (elem_t *)destination, (const acc_t *)(uintptr_t)1,
                  NO_ACTIVATION, ACC_SCALE_IDENTITY, false, false, false, false, false, true, true, false, false, false, 1, 1);
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
    printf("Block DMA mismatch device=%u tag=%u expected=%u\n", device.id, observed, tag);
    return 1;
  }
  return 0;
}

static int run_cgra(accel_t device, const cgra_kernel_t *original, cgra_packet_t *config, const uint32_t *symbols, unsigned tile) {
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
  uint64_t ready = 0;
  uint64_t status = 0;
  uint64_t result = 0;
  accel_commands(device, {
    cgra_prepare(&kernel, tile == 0 ? CGRA_COLD : CGRA_REPEAT);
    cgra_start(&kernel);
    CGRA_WAIT(ready);
    CGRA_STATUS(status);
    CGRA_RESULT(result);
  });
  if (ready != 1 || (status & UINT64_C(1)) != 1 || ((status >> 1) & UINT64_C(0xffff)) != kernel.expected_completes || result != 0) {
    printf("Block CGRA completion device=%u tile=%u ready=%lu status=%lu result=%lu\n", device.id, tile, (unsigned long)ready, (unsigned long)status, (unsigned long)result);
    return 1;
  }
  return 0;
}

static int run_tile(region_t core, unsigned tile) {
  const region_t expanded = halo(core);
  const region_t source = {0, 0, INPUT_H, INPUT_W};
  const unsigned elements = core.rows * core.columns * CHANNELS;
  const unsigned expanded_elements = expanded.rows * expanded.columns * CHANNELS;
  const uintptr_t first = publication(GEMMINI0, AUTO_LINK_GEMMINI0_BUFFER_SLOTS, HALO_WORDS);
  const uintptr_t skip = first + HALO_WORDS * sizeof(elem_t);
  const uintptr_t second = publication(GEMMINI1, AUTO_LINK_GEMMINI1_BUFFER_SLOTS, CORE_WORDS);
  const uint32_t relu_symbols[RELU_RUNTIME_SYMBOL_COUNT] = {
      [RELU_RUNTIME_SYMBOL_INPUT0] = 0,
      [RELU_RUNTIME_SYMBOL_OUTPUT] = 0,
      [RELU_RUNTIME_SYMBOL_ELEMENTS] = expanded_elements,
  };
  const uint32_t add_symbols[ADD_RELU_RUNTIME_SYMBOL_COUNT] = {
      [ADD_RELU_RUNTIME_SYMBOL_INPUT0] = 0,
      [ADD_RELU_RUNTIME_SYMBOL_INPUT1] = SKIP_WORD,
      [ADD_RELU_RUNTIME_SYMBOL_OUTPUT] = OUTPUT_WORD + tile * CORE_WORDS,
      [ADD_RELU_RUNTIME_SYMBOL_ELEMENTS] = elements,
  };
  run_conv(GEMMINI0, expanded, source, INPUT_H, INPUT_W, INPUT_CHANNELS, KERNEL, STRIDE, PADDING, input_address(), weights1_address(), first);
  run_conv(GEMMINI0, core, source, INPUT_H, INPUT_W, INPUT_CHANNELS, 1, STRIDE, 0, input_address(), projection_address(), skip);
  int failures = copy_input(CGRA0, first, 0, expanded_elements, 0x20);
  failures += run_cgra(CGRA0, &RELU_RUNTIME, relu_config, relu_symbols, tile);
  run_conv(GEMMINI1, core, expanded, HEIGHT, WIDTH, CHANNELS, KERNEL, 1, PADDING, CGRA0.spm, weights2_address(), second);
  failures += copy_input(CGRA1, second, 0, elements, 0x21);
  failures += copy_input(CGRA1, skip, SKIP_WORD, elements, 0x22);
  failures += run_cgra(CGRA1, &ADD_RELU_RUNTIME, add_config, add_symbols, tile);
  return failures;
}

int main(void) {
  init_inputs();
  init_expected();
  load_inputs();
  accel_commands(CGRA0, { cgra_send_packets_fast(RELU_RUNTIME.static_packets, RELU_RUNTIME.static_count); });
  accel_commands(CGRA1, { cgra_send_packets_fast(ADD_RELU_RUNTIME.static_packets, ADD_RELU_RUNTIME.static_count); });
  cgra_dma_memory_fence();
  unsigned tile = 0;
  unsigned long cpu_cycles = 0;
  int failures = 0;
  for (int row = 0; row < HEIGHT; row += TILE_H) {
    for (int column = 0; column < WIDTH; column += TILE_W) {
      const region_t core = {row, column, minimum(TILE_H, HEIGHT - row), minimum(TILE_W, WIDTH - column)};
      const unsigned long begin = cycles();
      failures += run_tile(core, tile++);
      cpu_cycles += cycles() - begin;
    }
  }
  cgra_dma_memory_fence();
  printf("Block Manual tiles=%u shape=%ux%u cpu_cycles=%lu\n", TILES, TILE_H, TILE_W, cpu_cycles);
  failures += verify_output();
  printf("Projected residual Manual: %s\n", failures == 0 ? "PASS" : "FAIL");
  return failures != 0;
}
