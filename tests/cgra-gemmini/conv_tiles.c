#include "cgra_dma.h"
#include "cgra_protocol.h"
#include "gemmini.h"
#include "gemmini_ext_spm.h"
#include "generated/cgra_relu_runtime_fast_api.h"

#include <stdint.h>
#include <stdio.h>

enum {
  INPUT_H = 3,
  INPUT_W = 5,
  INPUT_CHANNELS = 3,
  OUTPUT_CHANNELS = 8,
  KERNEL_DIM = 3,
  PADDING = 1,
  TILE_H = 2,
  TILE_W = 2,
  OUTPUT_H = INPUT_H + 2 * PADDING - KERNEL_DIM + 1,
  OUTPUT_W = INPUT_W + 2 * PADDING - KERNEL_DIM + 1,
  TILE_ELEMENTS = TILE_H * TILE_W * OUTPUT_CHANNELS,
  OUTPUT_WORD = 64,
  INPUT_TAG = 0x31,
  OUTPUT_TAG = 0x32,
};

static elem_t input[INPUT_H][INPUT_W][INPUT_CHANNELS] row_align(1);
static elem_t weights[KERNEL_DIM][KERNEL_DIM][INPUT_CHANNELS][OUTPUT_CHANNELS] row_align(1);
static int32_t tile_output[TILE_ELEMENTS] __attribute__((aligned(16)));
static int32_t output[OUTPUT_H][OUTPUT_W][OUTPUT_CHANNELS];
static const uintptr_t PUBLICATION = GEMMINI_EXT_SPM_BASE + GEMMINI_EXT_SPM_SIZE_BYTES - TILE_ELEMENTS;

static void init_inputs(void) {
  for (unsigned row = 0; row < INPUT_H; ++row) {
    for (unsigned column = 0; column < INPUT_W; ++column) {
      for (unsigned channel = 0; channel < INPUT_CHANNELS; ++channel) {
        input[row][column][channel] = (elem_t)((int)((row * INPUT_W * INPUT_CHANNELS + column * INPUT_CHANNELS + channel) % 5) - 2);
      }
    }
  }
  for (unsigned row = 0; row < KERNEL_DIM; ++row) {
    for (unsigned column = 0; column < KERNEL_DIM; ++column) {
      for (unsigned in = 0; in < INPUT_CHANNELS; ++in) {
        for (unsigned out = 0; out < OUTPUT_CHANNELS; ++out) {
          weights[row][column][in][out] = (elem_t)((int)((row * 4 + column * 2 + in + out) % 5) - 2);
        }
      }
    }
  }
  for (unsigned row = 0; row < OUTPUT_H; ++row) {
    for (unsigned column = 0; column < OUTPUT_W; ++column) {
      for (unsigned channel = 0; channel < OUTPUT_CHANNELS; ++channel) {
        output[row][column][channel] = INT32_C(0x5a5a5a5a);
      }
    }
  }
}

static void run_conv(int row, int column, int height, int width) {
  const int input_row = row - PADDING;
  const int input_column = column - PADDING;
  const int top = input_row < 0 ? -input_row : 0;
  const int left = input_column < 0 ? -input_column : 0;
  const int bottom = input_row + height + KERNEL_DIM - 1 > INPUT_H ? input_row + height + KERNEL_DIM - 1 - INPUT_H : 0;
  const int right = input_column + width + KERNEL_DIM - 1 > INPUT_W ? input_column + width + KERNEL_DIM - 1 - INPUT_W : 0;
  const elem_t *source = &input[input_row + top][input_column + left][0];

  gemmini_extended_config_st(OUTPUT_CHANNELS * sizeof(elem_t), NO_ACTIVATION, ACC_SCALE_IDENTITY);
  gemmini_extended3_config_ex(WEIGHT_STATIONARY, 0, 0, 0, 1, 1, false, false, false);
  sp_tiled_conv(1, INPUT_H, INPUT_W, INPUT_CHANNELS, OUTPUT_CHANNELS, height, width, height, width, 1, PADDING, KERNEL_DIM, 1, INPUT_CHANNELS, OUTPUT_CHANNELS, OUTPUT_CHANNELS, 1, 1, 0, 1, height,
                width, OUTPUT_CHANNELS, KERNEL_DIM, KERNEL_DIM, INPUT_CHANNELS, left, right, top, bottom, 0, 0, 0, 0, source, &weights[0][0][0][0], (elem_t *)PUBLICATION, (const acc_t *)(uintptr_t)1,
                NO_ACTIVATION, ACC_SCALE_IDENTITY, false, false, false, false, false, true, true, false, false, false, 1, 1);
  gemmini_fence();
}

static int run_relu(unsigned elements) {
  const cgra_dma_desc_t bytes = (uint64_t)(elements * sizeof(int32_t)) << CGRA_DMA_DESC_NBYTES_LSB;
  const cgra_dma_desc_t input_desc = bytes | ((uint64_t)INPUT_TAG << CGRA_DMA_DESC_TAG_LSB);
  const cgra_dma_desc_t output_desc = bytes | ((uint64_t)OUTPUT_WORD << CGRA_DMA_DESC_SPM_ADDR_LSB) | ((uint64_t)OUTPUT_TAG << CGRA_DMA_DESC_TAG_LSB);
  uint64_t completed = 0;
  uint64_t status = 0;

  cgra_dma_mvin_i8_async((const void *)PUBLICATION, input_desc);
  if (cgra_dma_wait(INPUT_TAG) != INPUT_TAG) {
    printf("Tile input DMA tag mismatch\n");
    return 1;
  }
  CGRA_SET_EXPECTED_COMPLETES(RELU_RUNTIME_EXPECTED_COMPLETES);
  load_relu_runtime_bound_config_fast(0, OUTPUT_WORD, elements);
  launch_relu_runtime_fast();
  CGRA_WAIT(completed);
  CGRA_STATUS(status);
  if (completed != 1 || (status & UINT64_C(1)) != 1 || ((status >> 1) & UINT64_C(0xffff)) != RELU_RUNTIME_EXPECTED_COMPLETES) {
    printf("Tile ReLU completion mismatch\n");
    return 1;
  }
  cgra_dma_mvout_async(tile_output, output_desc);
  if (cgra_dma_wait(OUTPUT_TAG) != OUTPUT_TAG) {
    printf("Tile output DMA tag mismatch\n");
    return 1;
  }
  cgra_dma_memory_fence();
  return 0;
}

static int run_tiles(void) {
  for (int row = 0; row < OUTPUT_H; row += TILE_H) {
    const int height = OUTPUT_H - row < TILE_H ? OUTPUT_H - row : TILE_H;
    for (int column = 0; column < OUTPUT_W; column += TILE_W) {
      const int width = OUTPUT_W - column < TILE_W ? OUTPUT_W - column : TILE_W;
      run_conv(row, column, height, width);
      if (run_relu(height * width * OUTPUT_CHANNELS) != 0) {
        return 1;
      }
      for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
          for (unsigned channel = 0; channel < OUTPUT_CHANNELS; ++channel) {
            output[row + y][column + x][channel] = tile_output[(y * width + x) * OUTPUT_CHANNELS + channel];
          }
        }
      }
    }
  }
  return 0;
}

static int verify_output(void) {
  int failures = 0;
  for (int row = 0; row < OUTPUT_H; ++row) {
    for (int column = 0; column < OUTPUT_W; ++column) {
      for (unsigned channel = 0; channel < OUTPUT_CHANNELS; ++channel) {
        int32_t sum = 0;
        for (int kr = 0; kr < KERNEL_DIM; ++kr) {
          const int y = row + kr - PADDING;
          if (y < 0 || y >= INPUT_H) {
            continue;
          }
          for (int kc = 0; kc < KERNEL_DIM; ++kc) {
            const int x = column + kc - PADDING;
            if (x < 0 || x >= INPUT_W) {
              continue;
            }
            for (unsigned in = 0; in < INPUT_CHANNELS; ++in) {
              sum += (int32_t)input[y][x][in] * (int32_t)weights[kr][kc][in][channel];
            }
          }
        }
        const int32_t expected = sum > 0 ? sum : 0;
        if (output[row][column][channel] != expected) {
          printf("Tiled Conv + ReLU mismatch row=%d column=%d channel=%u actual=%d expected=%d\n", row, column, channel, (int)output[row][column][channel], (int)expected);
          ++failures;
        }
      }
    }
  }
  return failures;
}

int main(void) {
  init_inputs();
  gemmini_flush(0);
  if (run_tiles() != 0 || verify_output() != 0) {
    printf("Manual tiled native Conv + runtime ReLU: FAIL\n");
    return 1;
  }
  printf("Manual tiled native Conv + runtime ReLU: PASS\n");
  return 0;
}
