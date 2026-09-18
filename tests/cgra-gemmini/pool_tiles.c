#include "auto_link.h"
#include "cgra_link.h"
#include "gemmini.h"
#include "gemmini_conv.h"
#include "gemmini_ext_spm.h"
#include "generated/cgra_relu_runtime_fast_api.h"
#include "pool.h"

#include <stdint.h>
#include <stdio.h>

enum {
  INPUT_H = 4,
  INPUT_W = 6,
  INPUT_CHANNELS = 3,
  CHANNELS = 8,
  KERNEL = 3,
  PADDING = 1,
  POOL_KERNEL = 2,
  POOL_STRIDE = 2,
  OUTPUT_H = (INPUT_H - POOL_KERNEL) / POOL_STRIDE + 1,
  OUTPUT_W = (INPUT_W - POOL_KERNEL) / POOL_STRIDE + 1,
  TILE_PIXELS = 2,
  SLOT_ELEMENTS = TILE_PIXELS * POOL_STRIDE * POOL_STRIDE * CHANNELS,
  SLOT_BYTES = SLOT_ELEMENTS * sizeof(elem_t),
  PUBLICATION = GEMMINI_EXT_SPM_SIZE_BYTES - AUTO_LINK_GEMMINI_BUFFER_SLOTS * SLOT_BYTES,
  OUTPUT_ELEMENTS = OUTPUT_H * OUTPUT_W * CHANNELS,
  SENTINEL = -85,
};

static elem_t input[INPUT_H][INPUT_W][INPUT_CHANNELS] row_align(1);
static elem_t weights[KERNEL][KERNEL][INPUT_CHANNELS][CHANNELS] row_align(1);
static elem_t output[OUTPUT_ELEMENTS + 2] __attribute__((aligned(32)));
static elem_t expected[OUTPUT_ELEMENTS];

static unsigned long cycles(void) {
  unsigned long value;
  __asm__ volatile("rdcycle %0" : "=r"(value));
  return value;
}

static void init_inputs(void) {
  for (unsigned row = 0; row < INPUT_H; ++row) {
    for (unsigned column = 0; column < INPUT_W; ++column) {
      for (unsigned channel = 0; channel < INPUT_CHANNELS; ++channel) {
        input[row][column][channel] = (elem_t)((int)((row * 7 + column * 3 + channel) % 5) - 2);
      }
    }
  }
  for (unsigned row = 0; row < KERNEL; ++row) {
    for (unsigned column = 0; column < KERNEL; ++column) {
      for (unsigned channel = 0; channel < INPUT_CHANNELS; ++channel) {
        for (unsigned out = 0; out < CHANNELS; ++out) {
          weights[row][column][channel][out] = (elem_t)((int)((row * 4 + column * 2 + channel + out) % 5) - 2);
        }
      }
    }
  }
}

static int configure_relu(void) {
  static const cgra_link_symbol_t symbols[RELU_RUNTIME_SYMBOL_COUNT] = {
      [RELU_RUNTIME_SYMBOL_INPUT0] = {0, SLOT_ELEMENTS, CGRA_LINK_SLOT},
      [RELU_RUNTIME_SYMBOL_OUTPUT] = {0, SLOT_ELEMENTS, CGRA_LINK_SLOT},
      [RELU_RUNTIME_SYMBOL_ELEMENTS] = {0, 0, CGRA_LINK_ELEMENTS},
  };
  return cgra_job_config(AUTO_LINK_JOB_CGRA, &RELU_RUNTIME, symbols);
}

static int configure_conv(void) {
  elem_t *destination = (elem_t *)(uintptr_t)(GEMMINI_EXT_SPM_BASE + PUBLICATION);
  return gemmini_capture_conv(AUTO_LINK_JOB_GEMMINI, INPUT_H, INPUT_W, INPUT_CHANNELS, CHANNELS, KERNEL, 1, PADDING, 1, TILE_PIXELS * POOL_STRIDE, TILE_PIXELS * POOL_STRIDE, &input[0][0][0],
                              &weights[0][0][0][0], NULL, destination, NO_ACTIVATION, ACC_SCALE_IDENTITY);
}

static void configure_pool(void) {
  pool_config_input(0, INPUT_H, INPUT_W, CHANNELS);
  pool_config_window(POOL_MODE_MAX, POOL_KERNEL, POOL_KERNEL, POOL_STRIDE, POOL_STRIDE, 0, 0);
  pool_config_output((uintptr_t)&output[1]);
  pool_config_tiled(1);
}

static int32_t conv(unsigned row, unsigned column, unsigned channel) {
  int32_t sum = 0;
  for (int y = 0; y < KERNEL; ++y) {
    const int in_row = (int)row + y - PADDING;
    if (in_row < 0 || in_row >= INPUT_H) {
      continue;
    }
    for (int x = 0; x < KERNEL; ++x) {
      const int in_column = (int)column + x - PADDING;
      if (in_column < 0 || in_column >= INPUT_W) {
        continue;
      }
      for (unsigned in = 0; in < INPUT_CHANNELS; ++in) {
        sum += input[in_row][in_column][in] * weights[y][x][in][channel];
      }
    }
  }
  if (sum < 0) {
    return 0;
  }
  return sum > INT8_MAX ? INT8_MAX : sum;
}

static void init_expected(void) {
  for (unsigned row = 0; row < OUTPUT_H; ++row) {
    for (unsigned column = 0; column < OUTPUT_W; ++column) {
      for (unsigned channel = 0; channel < CHANNELS; ++channel) {
        int32_t value = 0;
        for (unsigned y = 0; y < POOL_KERNEL; ++y) {
          for (unsigned x = 0; x < POOL_KERNEL; ++x) {
            const int32_t element = conv(row * POOL_STRIDE + y, column * POOL_STRIDE + x, channel);
            if (element > value) {
              value = element;
            }
          }
        }
        const unsigned index = (row * OUTPUT_W + column) * CHANNELS + channel;
        expected[index] = (elem_t)value;
      }
    }
  }
}

static int verify_output(void) {
  int failures = 0;
  for (unsigned index = 0; index < OUTPUT_ELEMENTS; ++index) {
    if (output[index + 1] != expected[index]) {
      printf("Pool tile mismatch index=%u actual=%d expected=%d\n", index, (int)output[index + 1], (int)expected[index]);
      ++failures;
    }
  }
  if (output[0] != SENTINEL || output[OUTPUT_ELEMENTS + 1] != SENTINEL) {
    printf("Pool tile output guard changed\n");
    ++failures;
  }
  return failures;
}

static int run_tiles(unsigned rows, unsigned columns) {
  for (unsigned index = 0; index < OUTPUT_ELEMENTS + 2; ++index) {
    output[index] = SENTINEL;
  }
  __asm__ volatile("fence rw, rw" ::: "memory");
  auto_link_tiles(OUTPUT_H, OUTPUT_W, rows, columns);
  const unsigned long begin = cycles();
  auto_link_input_ready();
  int failures = 0;
  for (unsigned stage = 0; stage < 3; ++stage) {
    const cgra_link_result_t result = cgra_link_wait();
    if (result.status != AUTO_LINK_STATUS_SUCCESS || result.detail != 0 || result.data != 0) {
      printf("Pool tile stage=%u status=%u detail=%u\n", result.stage, result.status, result.detail);
      ++failures;
    }
  }
  while (*(volatile uint32_t *)(AUTO_LINK_BASE + AUTO_LINK_RUNNING)) {
  }
  __asm__ volatile("fence rw, rw" ::: "memory");
  const unsigned long overlap = *(volatile uint64_t *)(AUTO_LINK_BASE + AUTO_LINK_OVERLAP);
  const unsigned long peak = *(volatile uint64_t *)(AUTO_LINK_BASE + AUTO_LINK_PEAK_ACTIVE);
  printf("Pool tile shape=%ux%u cycles=%lu overlap=%lu peak=%lu\n", rows, columns, cycles() - begin, overlap, peak);
  if (overlap == 0) {
    printf("Pool tiles did not overlap across IPs\n");
    ++failures;
  }
  if (peak < 3) {
    printf("Pool tiles did not overlap across all three IPs\n");
    ++failures;
  }
  return failures + verify_output();
}

int main(void) {
  init_inputs();
  init_expected();
  gemmini_flush(0);
  if (configure_relu() != 0 || configure_conv() != 0) {
    printf("Pool tile capture: FAIL\n");
    return 1;
  }
  configure_pool();
  auto_link_transfer(AUTO_LINK_COPY_GEMMINI_CGRA, PUBLICATION, 0, SLOT_BYTES, SLOT_ELEMENTS * sizeof(int32_t), CHANNELS * sizeof(elem_t));
  auto_link_transfer(AUTO_LINK_COPY_CGRA_POOL, 0, 0, SLOT_BYTES, 0, CHANNELS * sizeof(elem_t));
  auto_link_region(AUTO_LINK_STAGE_GEMMINI, INPUT_H, INPUT_W, POOL_STRIDE, POOL_STRIDE, 0, POOL_KERNEL - POOL_STRIDE, 0, POOL_KERNEL - POOL_STRIDE);
  auto_link_region(AUTO_LINK_STAGE_CGRA, INPUT_H, INPUT_W, POOL_STRIDE, POOL_STRIDE, 0, POOL_KERNEL - POOL_STRIDE, 0, POOL_KERNEL - POOL_STRIDE);
  int failures = run_tiles(1, TILE_PIXELS);
  failures += run_tiles(TILE_PIXELS, 1);
  printf("Gemmini Conv + CGRA ReLU + Pool tiles: %s\n", failures == 0 ? "PASS" : "FAIL");
  return failures != 0;
}
