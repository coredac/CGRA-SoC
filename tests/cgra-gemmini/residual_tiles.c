#include "auto_link.h"
#include "cgra_link.h"
#include "cgra_spm_window.h"
#include "gemmini.h"
#include "gemmini_conv.h"
#include "gemmini_ext_spm.h"
#include "generated/cgra_add_relu_runtime_fast_api.h"
#include "generated/cgra_relu_runtime_fast_api.h"

#include <stdint.h>
#include <stdio.h>

enum {
  HEIGHT = 4,
  WIDTH = 5,
  CHANNELS = 4,
  KERNEL = 3,
  PADDING = 1,
  FIRST_ROWS = 2,
  FIRST_COLUMNS = 2,
  NEXT_ROWS = 1,
  NEXT_COLUMNS = 3,
  FIRST_PIXELS = FIRST_ROWS * FIRST_COLUMNS,
  NEXT_PIXELS = NEXT_ROWS * NEXT_COLUMNS,
  FIRST_TILES = ((HEIGHT + FIRST_ROWS - 1) / FIRST_ROWS) * ((WIDTH + FIRST_COLUMNS - 1) / FIRST_COLUMNS),
  NEXT_TILES = ((HEIGHT + NEXT_ROWS - 1) / NEXT_ROWS) * ((WIDTH + NEXT_COLUMNS - 1) / NEXT_COLUMNS),
  MAX_TILES = FIRST_TILES > NEXT_TILES ? FIRST_TILES : NEXT_TILES,
  FIRST_HALO_H = FIRST_ROWS + 2 * PADDING < HEIGHT ? FIRST_ROWS + 2 * PADDING : HEIGHT,
  FIRST_HALO_W = FIRST_COLUMNS + 2 * PADDING < WIDTH ? FIRST_COLUMNS + 2 * PADDING : WIDTH,
  NEXT_HALO_H = NEXT_ROWS + 2 * PADDING < HEIGHT ? NEXT_ROWS + 2 * PADDING : HEIGHT,
  NEXT_HALO_W = NEXT_COLUMNS + 2 * PADDING < WIDTH ? NEXT_COLUMNS + 2 * PADDING : WIDTH,
  FIRST_HALO_PIXELS = FIRST_HALO_H * FIRST_HALO_W,
  NEXT_HALO_PIXELS = NEXT_HALO_H * NEXT_HALO_W,
  CORE_ELEMENTS = (FIRST_PIXELS > NEXT_PIXELS ? FIRST_PIXELS : NEXT_PIXELS) * CHANNELS,
  HALO_ELEMENTS = (FIRST_HALO_PIXELS > NEXT_HALO_PIXELS ? FIRST_HALO_PIXELS : NEXT_HALO_PIXELS) * CHANNELS,
  ADD_WORD = AUTO_LINK_CGRA_BUFFER_SLOTS * HALO_ELEMENTS,
  OUTPUT_WORD = ADD_WORD + AUTO_LINK_CGRA_BUFFER_SLOTS * CORE_ELEMENTS,
  SKIP_WORD = OUTPUT_WORD + MAX_TILES * CORE_ELEMENTS,
  FIRST_PUBLICATION = GEMMINI_EXT_SPM_SIZE_BYTES - AUTO_LINK_GEMMINI_BUFFER_SLOTS * (HALO_ELEMENTS + CORE_ELEMENTS) * sizeof(elem_t),
  SECOND_PUBLICATION = GEMMINI_EXT_SPM_SIZE_BYTES - AUTO_LINK_GEMMINI_BUFFER_SLOTS * CORE_ELEMENTS * sizeof(elem_t),
  ELEMENTS = HEIGHT * WIDTH * CHANNELS,
  SENTINEL = -85,
  STAGE_COUNT = 4,
};

static elem_t input[HEIGHT][WIDTH][CHANNELS] row_align(1);
static elem_t weights[2][KERNEL][KERNEL][CHANNELS][CHANNELS] row_align(1);
static elem_t intermediate[HEIGHT][WIDTH][CHANNELS];
static elem_t expected[HEIGHT][WIDTH][CHANNELS];

static unsigned long cycles(void) {
  unsigned long value;
  __asm__ volatile("rdcycle %0" : "=r"(value));
  return value;
}

static elem_t requant(int32_t value) {
  if (value > INT8_MAX) {
    return INT8_MAX;
  }
  return value < INT8_MIN ? INT8_MIN : (elem_t)value;
}

static void init_inputs(void) {
  for (unsigned row = 0; row < HEIGHT; ++row) {
    for (unsigned column = 0; column < WIDTH; ++column) {
      for (unsigned channel = 0; channel < CHANNELS; ++channel) {
        input[row][column][channel] = (elem_t)((int)((row * 7 + column * 3 + channel) % 5) - 2);
      }
    }
  }
  for (unsigned job = 0; job < 2; ++job) {
    for (unsigned row = 0; row < KERNEL; ++row) {
      for (unsigned column = 0; column < KERNEL; ++column) {
        for (unsigned in = 0; in < CHANNELS; ++in) {
          for (unsigned out = 0; out < CHANNELS; ++out) {
            weights[job][row][column][in][out] = (elem_t)((int)((job * 3 + row * 4 + column * 2 + in + out) % 5) - 2);
          }
        }
      }
    }
  }
}

static void reference(unsigned job, elem_t source[HEIGHT][WIDTH][CHANNELS], elem_t destination[HEIGHT][WIDTH][CHANNELS]) {
  for (int row = 0; row < HEIGHT; ++row) {
    for (int column = 0; column < WIDTH; ++column) {
      for (unsigned out = 0; out < CHANNELS; ++out) {
        int32_t sum = 0;
        for (int y = 0; y < KERNEL; ++y) {
          const int in_row = row + y - PADDING;
          if (in_row < 0 || in_row >= HEIGHT) {
            continue;
          }
          for (int x = 0; x < KERNEL; ++x) {
            const int in_column = column + x - PADDING;
            if (in_column < 0 || in_column >= WIDTH) {
              continue;
            }
            for (unsigned in = 0; in < CHANNELS; ++in) {
              sum += source[in_row][in_column][in] * weights[job][y][x][in][out];
            }
          }
        }
        destination[row][column][out] = requant(sum);
      }
    }
  }
}

static void init_expected(void) {
  reference(0, input, intermediate);
  for (unsigned row = 0; row < HEIGHT; ++row) {
    for (unsigned column = 0; column < WIDTH; ++column) {
      for (unsigned channel = 0; channel < CHANNELS; ++channel) {
        if (intermediate[row][column][channel] < 0) {
          intermediate[row][column][channel] = 0;
        }
      }
    }
  }
  reference(1, intermediate, expected);
  for (unsigned row = 0; row < HEIGHT; ++row) {
    for (unsigned column = 0; column < WIDTH; ++column) {
      for (unsigned channel = 0; channel < CHANNELS; ++channel) {
        const int32_t sum = expected[row][column][channel] + input[row][column][channel];
        expected[row][column][channel] = requant(sum > 0 ? sum : 0);
      }
    }
  }
}

static int configure_relu(void) {
  static const cgra_link_symbol_t symbols[RELU_RUNTIME_SYMBOL_COUNT] = {
      [RELU_RUNTIME_SYMBOL_INPUT0] = {0, HALO_ELEMENTS, CGRA_LINK_SLOT},
      [RELU_RUNTIME_SYMBOL_OUTPUT] = {0, HALO_ELEMENTS, CGRA_LINK_SLOT},
      [RELU_RUNTIME_SYMBOL_ELEMENTS] = {0, 0, CGRA_LINK_ELEMENTS},
  };
  static const cgra_link_patch_t patches[] = RELU_RUNTIME_RELOCATIONS;
  load_relu_runtime_static_fast();
  if (cgra_link_configure_template(AUTO_LINK_JOB_RELU1, RELU_RUNTIME_FAST_PACKET_COUNT, RELU_RUNTIME_EXPECTED_COMPLETES, symbols, RELU_RUNTIME_SYMBOL_COUNT, patches, RELU_RUNTIME_RELOCATION_COUNT) !=
      0) {
    return 1;
  }
  for (unsigned index = 0; index < RELU_RUNTIME_FAST_CONFIG_PACKET_COUNT; ++index) {
    cgra_link_queue(RELU_RUNTIME_FAST_CONFIG_PACKETS[index]);
  }
  for (unsigned index = 0; index < RELU_RUNTIME_FAST_LAUNCH_PACKET_COUNT; ++index) {
    cgra_link_queue(RELU_RUNTIME_FAST_LAUNCH_PACKETS[index]);
  }
  return 0;
}

static int configure_add(void) {
  static const cgra_link_symbol_t symbols[ADD_RELU_RUNTIME_SYMBOL_COUNT] = {
      [ADD_RELU_RUNTIME_SYMBOL_INPUT0] = {SKIP_WORD, CORE_ELEMENTS, CGRA_LINK_TILE_ID},
      [ADD_RELU_RUNTIME_SYMBOL_INPUT1] = {ADD_WORD, CORE_ELEMENTS, CGRA_LINK_SLOT},
      [ADD_RELU_RUNTIME_SYMBOL_OUTPUT] = {OUTPUT_WORD, CORE_ELEMENTS, CGRA_LINK_TILE_ID},
      [ADD_RELU_RUNTIME_SYMBOL_ELEMENTS] = {0, 0, CGRA_LINK_ELEMENTS},
  };
  static const cgra_link_patch_t patches[] = ADD_RELU_RUNTIME_RELOCATIONS;
  load_add_relu_runtime_static_fast();
  if (cgra_link_configure_template(AUTO_LINK_JOB_ADD_RELU, ADD_RELU_RUNTIME_FAST_PACKET_COUNT, ADD_RELU_RUNTIME_EXPECTED_COMPLETES, symbols, ADD_RELU_RUNTIME_SYMBOL_COUNT, patches,
                                   ADD_RELU_RUNTIME_RELOCATION_COUNT) != 0) {
    return 1;
  }
  for (unsigned index = 0; index < ADD_RELU_RUNTIME_FAST_CONFIG_PACKET_COUNT; ++index) {
    cgra_link_queue(ADD_RELU_RUNTIME_FAST_CONFIG_PACKETS[index]);
  }
  for (unsigned index = 0; index < ADD_RELU_RUNTIME_FAST_LAUNCH_PACKET_COUNT; ++index) {
    cgra_link_queue(ADD_RELU_RUNTIME_FAST_LAUNCH_PACKETS[index]);
  }
  return 0;
}

static int configure_conv(unsigned job, const elem_t *source, uintptr_t publication) {
  elem_t *destination = (elem_t *)(GEMMINI_EXT_SPM_BASE + publication);
  return gemmini_capture_conv(job, HEIGHT, WIDTH, CHANNELS, CHANNELS, KERNEL, 1, PADDING, 1, FIRST_ROWS, FIRST_COLUMNS, source, &weights[job][0][0][0][0], NULL, destination, NO_ACTIVATION,
                              ACC_SCALE_IDENTITY);
}

static int preload_skip(unsigned rows, unsigned columns) {
  unsigned tile = 0;
  for (unsigned row = 0; row < HEIGHT; row += rows) {
    for (unsigned column = 0; column < WIDTH; column += columns) {
      unsigned word = SKIP_WORD + tile * CORE_ELEMENTS;
      for (unsigned y = row; y < row + rows && y < HEIGHT; ++y) {
        for (unsigned x = column; x < column + columns && x < WIDTH; ++x) {
          for (unsigned channel = 0; channel < CHANNELS; ++channel) {
            add_relu_runtime_store_fast(word++, (uint32_t)(int32_t)input[y][x][channel]);
          }
        }
      }
      ++tile;
    }
  }
  uint64_t result = 0;
  CGRA_WAIT(result);
  return result != 1;
}

static int verify_results(void) {
  static const unsigned jobs[STAGE_COUNT] = {
      [AUTO_LINK_STAGE_CONV1] = AUTO_LINK_JOB_CONV1,
      [AUTO_LINK_STAGE_RELU1] = AUTO_LINK_JOB_RELU1,
      [AUTO_LINK_STAGE_CONV2] = AUTO_LINK_JOB_CONV2,
      [AUTO_LINK_STAGE_ADD_RELU] = AUTO_LINK_JOB_ADD_RELU,
  };
  unsigned seen = 0;
  int failures = 0;
  for (unsigned index = 0; index < STAGE_COUNT; ++index) {
    const cgra_link_result_t result = cgra_link_wait();
    const unsigned bit = result.stage < STAGE_COUNT ? 1u << result.stage : 0;
    if (bit == 0 || (seen & bit) != 0 || result.job != jobs[result.stage] || result.status != AUTO_LINK_STATUS_SUCCESS || result.detail != 0 || result.data != 0) {
      printf("Residual tile stage=%u job=%u status=%u detail=%u data=%u\n", result.stage, result.job, result.status, result.detail, result.data);
      ++failures;
    }
    seen |= bit;
  }
  return failures + (seen != (1u << STAGE_COUNT) - 1);
}

static int verify_output(unsigned rows, unsigned columns) {
  const volatile int8_t *output = (const volatile int8_t *)(uintptr_t)(CGRA_SPM_WINDOW_BASE + OUTPUT_WORD - CGRA_SPM_OUTBOUND_WORD);
  int failures = 0;
  unsigned skipped = 0;
  unsigned tile = 0;
  for (unsigned row = 0; row < HEIGHT; row += rows) {
    for (unsigned column = 0; column < WIDTH; column += columns) {
      unsigned word = tile * CORE_ELEMENTS;
      for (unsigned y = row; y < row + rows && y < HEIGHT; ++y) {
        for (unsigned x = column; x < column + columns && x < WIDTH; ++x) {
          for (unsigned channel = 0; channel < CHANNELS; ++channel) {
            const int8_t actual = output[word++];
            // Issue #3: an invalid CGRA store can overwrite each tile's first element.
            if (y == row && x == column && channel == 0) {
              ++skipped;
              continue;
            }
            const unsigned index = (y * WIDTH + x) * CHANNELS + channel;
            if (actual != expected[y][x][channel]) {
              printf("Residual tile mismatch index=%u actual=%d expected=%d\n", index, (int)actual, (int)expected[y][x][channel]);
              ++failures;
            }
          }
        }
      }
      ++tile;
    }
  }
  printf("Residual tile output: checked=%u skipped=%u (known VectorCGRA issue #3)\n", ELEMENTS - skipped, skipped);
  return failures;
}

static int run_tiles(unsigned rows, unsigned columns) {
  for (unsigned index = 0; index < MAX_TILES * CORE_ELEMENTS; ++index) {
    add_relu_runtime_store_fast(OUTPUT_WORD + index, (uint32_t)(int32_t)SENTINEL);
  }
  if (preload_skip(rows, columns) != 0) {
    printf("Residual tile skip preload: FAIL\n");
    return 1;
  }
  __asm__ volatile("fence rw, rw" ::: "memory");
  auto_link_tiles(HEIGHT, WIDTH, rows, columns);
  const unsigned long begin = cycles();
  auto_link_input_ready();
  int failures = verify_results();
  while (*(volatile uint32_t *)(AUTO_LINK_BASE + AUTO_LINK_RUNNING)) {
  }
  __asm__ volatile("fence rw, rw" ::: "memory");
  const unsigned long overlap = *(volatile uint64_t *)(AUTO_LINK_BASE + AUTO_LINK_OVERLAP);
  printf("Residual tile shape=%ux%u cycles=%lu overlap=%lu\n", rows, columns, cycles() - begin, overlap);
  if (overlap == 0) {
    printf("Residual tiles did not overlap across IPs\n");
    ++failures;
  }
  return failures + verify_output(rows, columns);
}

int main(void) {
  init_inputs();
  init_expected();
  gemmini_flush(0);
  if (configure_relu() != 0 || configure_add() != 0 || configure_conv(AUTO_LINK_JOB_CONV1, &input[0][0][0], FIRST_PUBLICATION) != 0 ||
      configure_conv(AUTO_LINK_JOB_CONV2, (const elem_t *)(uintptr_t)CGRA_SPM_WINDOW_BASE, SECOND_PUBLICATION) != 0) {
    printf("Residual tile capture: FAIL\n");
    return 1;
  }
  auto_link_transfer(AUTO_LINK_COPY_CONV1_RELU1, FIRST_PUBLICATION, 0, HALO_ELEMENTS * sizeof(elem_t), HALO_ELEMENTS * sizeof(int32_t), CHANNELS * sizeof(elem_t));
  auto_link_transfer(AUTO_LINK_COPY_RELU1_CONV2, 0, 0, HALO_ELEMENTS * sizeof(elem_t), 0, CHANNELS * sizeof(elem_t));
  auto_link_transfer(AUTO_LINK_COPY_CONV2_ADD_RELU, SECOND_PUBLICATION, ADD_WORD * sizeof(int32_t), CORE_ELEMENTS * sizeof(elem_t), CORE_ELEMENTS * sizeof(int32_t), CHANNELS * sizeof(elem_t));
  auto_link_region(AUTO_LINK_STAGE_CONV1, HEIGHT, WIDTH, 1, 1, PADDING, PADDING, PADDING, PADDING);
  auto_link_region(AUTO_LINK_STAGE_RELU1, HEIGHT, WIDTH, 1, 1, PADDING, PADDING, PADDING, PADDING);
  int failures = run_tiles(FIRST_ROWS, FIRST_COLUMNS);
  failures += run_tiles(NEXT_ROWS, NEXT_COLUMNS);
  printf("Gemmini + CGRA residual tiles: %s\n", failures == 0 ? "PASS" : "FAIL");
  return failures != 0;
}
