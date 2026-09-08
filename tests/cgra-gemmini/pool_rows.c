#include "cgra_protocol.h"
#include "cgra_spm_window.h"
#include "generated/cgra_relu4x4_fast_api.h"
#include "pool.h"

#include <stdint.h>
#include <stdio.h>

enum {
  INPUT_H = 3,
  INPUT_W = 6,
  CHANNELS = 3,
  KERNEL = 2,
  OUTPUT_H = INPUT_H - KERNEL + 1,
  OUTPUT_W = INPUT_W - KERNEL + 1,
  TILE_W = 2,
  OUTPUT_ELEMENTS = OUTPUT_H * OUTPUT_W * CHANNELS,
  LAST_TILE_W = (OUTPUT_W - 1) % TILE_W + 1,
  TILE_ELEMENTS = OUTPUT_H * LAST_TILE_W * CHANNELS,
  GUARD = 1,
  SENTINEL = 0x5a5a5a5a,
};

static int32_t input[INPUT_H][INPUT_W][CHANNELS];
static int32_t output[OUTPUT_ELEMENTS + 2 * GUARD] __attribute__((aligned(32)));
static int32_t packed[TILE_ELEMENTS + 2 * GUARD] __attribute__((aligned(32)));

static void init_inputs(void) {
  for (unsigned row = 0; row < INPUT_H; ++row) {
    for (unsigned column = 0; column < INPUT_W; ++column) {
      for (unsigned channel = 0; channel < CHANNELS; ++channel) {
        input[row][column][channel] = (int32_t)((row * 7 + column * 11 + channel * 3) % 23) - 17;
      }
    }
  }
  for (unsigned index = 0; index < OUTPUT_ELEMENTS + 2 * GUARD; ++index) {
    output[index] = SENTINEL;
  }
  for (unsigned index = 0; index < TILE_ELEMENTS + 2 * GUARD; ++index) {
    packed[index] = SENTINEL;
  }
}

static int load_tile(unsigned column, unsigned width) {
  uint64_t completed = 0;
  const unsigned input_width = width + KERNEL - 1;
  for (unsigned row = 0; row < INPUT_H; ++row) {
    for (unsigned x = 0; x < input_width; ++x) {
      for (unsigned channel = 0; channel < CHANNELS; ++channel) {
        const unsigned word = (row * input_width + x) * CHANNELS + channel;
        relu4x4_store_fast(word, (uint32_t)input[row][column + x][channel]);
      }
    }
  }
  CGRA_WAIT(completed);
  return completed != 1;
}

static int32_t expected(unsigned row, unsigned column, unsigned channel) {
  int32_t value = INT32_MIN;
  for (unsigned y = 0; y < KERNEL; ++y) {
    for (unsigned x = 0; x < KERNEL; ++x) {
      if (input[row + y][column + x][channel] > value) {
        value = input[row + y][column + x][channel];
      }
    }
  }
  return value;
}

static int verify_output(unsigned columns) {
  int failures = 0;
  for (unsigned row = 0; row < OUTPUT_H; ++row) {
    for (unsigned column = 0; column < OUTPUT_W; ++column) {
      for (unsigned channel = 0; channel < CHANNELS; ++channel) {
        const unsigned index = GUARD + (row * OUTPUT_W + column) * CHANNELS + channel;
        const int32_t value = column < columns ? expected(row, column, channel) : SENTINEL;
        if (output[index] != value) {
          printf("Pool row mismatch row=%u column=%u channel=%u actual=%d expected=%d\n", row, column, channel, (int)output[index], (int)value);
          ++failures;
        }
      }
    }
  }
  if (output[0] != SENTINEL || output[OUTPUT_ELEMENTS + GUARD] != SENTINEL) {
    printf("Pool output guard changed\n");
    ++failures;
  }
  return failures;
}

static int run_pool(void) {
  pool_start();
  const uint32_t status = pool_wait();
  if (status != POOL_STATUS_SUCCESS) {
    printf("Pool status=%u\n", status);
    return 1;
  }
  return 0;
}

static int run_tiles(void) {
  int failures = 0;
  pool_config_window(POOL_MODE_MAX, KERNEL, KERNEL, 1, 1, 0, 0);
  for (unsigned column = 0; column < OUTPUT_W; column += TILE_W) {
    const unsigned width = OUTPUT_W - column < TILE_W ? OUTPUT_W - column : TILE_W;
    if (load_tile(column, width) != 0) {
      printf("CGRA tile preload failed\n");
      return 1;
    }
    pool_config_input(CGRA_SPM_WINDOW_BASE, INPUT_H, width + KERNEL - 1, CHANNELS);
    pool_config_output_stride((uintptr_t)&output[GUARD + column * CHANNELS], OUTPUT_W * CHANNELS * sizeof(output[0]));
    if (run_pool() != 0) {
      return 1;
    }
    failures += verify_output(column + width);
  }
  return failures;
}

static int check_compact(void) {
  pool_config_output((uintptr_t)&packed[GUARD]);
  if (run_pool() != 0) {
    return 1;
  }
  int failures = 0;
  for (unsigned row = 0; row < OUTPUT_H; ++row) {
    for (unsigned column = 0; column < LAST_TILE_W; ++column) {
      for (unsigned channel = 0; channel < CHANNELS; ++channel) {
        const unsigned index = GUARD + (row * LAST_TILE_W + column) * CHANNELS + channel;
        const int32_t value = expected(row, OUTPUT_W - LAST_TILE_W + column, channel);
        if (packed[index] != value) {
          printf("Pool compact mismatch index=%u actual=%d expected=%d\n", index, (int)packed[index], (int)value);
          ++failures;
        }
      }
    }
  }
  if (packed[0] != SENTINEL || packed[TILE_ELEMENTS + GUARD] != SENTINEL) {
    printf("Pool compact guard changed\n");
    ++failures;
  }
  return failures;
}

int main(void) {
  init_inputs();
  __asm__ volatile("fence rw, rw" ::: "memory");
  int failures = run_tiles();
  failures += check_compact();
  printf("Pool NHWC row writeback: %s\n", failures == 0 ? "PASS" : "FAIL");
  return failures != 0;
}
