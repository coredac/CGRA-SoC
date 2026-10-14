#ifndef BLOCK_GEMM_H
#define BLOCK_GEMM_H

#include "accel.h"
#include "gemmini.h"

// Dense INT8 matrices, INT32 bias/output; rows % 16 = columns % 64 = inner % 64 = 0.
// Supports 0 < inner <= 3072; owns scratch rows 0..3839 and accumulator rows 0..63.
static void block_matmul(accel_t device, const elem_t *input, const elem_t *weight, const acc_t *bias, acc_t *output, unsigned rows, unsigned inner, unsigned columns) {
  enum {
    TILE_COLS = 64,
    CHUNK = 768,
    B_ROW = CHUNK,
    ACC_WRITE = UINT32_C(1) << (ADDR_LEN - 1),
    ACC_READ = ACC_WRITE | (UINT32_C(1) << (ADDR_LEN - 3)),
    LOOP_SKIPS = (1 << 3) | (1 << 4) | (1 << 5) | (1 << 7),
  };
  accel_commands(device, {
    gemmini_config_ex(WEIGHT_STATIONARY, NO_ACTIVATION, 0);
    gemmini_extended3_config_ld(inner * sizeof(elem_t), MVIN_SCALE_IDENTITY, false, 0);
    gemmini_extended3_config_ld(columns * sizeof(elem_t), MVIN_SCALE_IDENTITY, false, 1);
    gemmini_extended3_config_ld(0, MVIN_SCALE_IDENTITY, false, 2);
    gemmini_config_st(columns * sizeof(acc_t));
    for (unsigned row = 0; row < rows; row += DIM) {
      for (unsigned column = 0; column < columns; column += TILE_COLS) {
        for (unsigned block = 0; block < TILE_COLS; block += DIM) {
          gemmini_extended_mvin3(bias + column + block, ACC_WRITE + block, DIM, DIM);
        }
        for (unsigned start = 0; start < inner; start += CHUNK) {
          const unsigned remaining = inner - start;
          const unsigned width = remaining < CHUNK ? remaining : CHUNK;
          for (unsigned channel = 0; channel < width; channel += TILE_COLS) {
            gemmini_extended_mvin(input + row * inner + start + channel, channel, TILE_COLS, DIM);
          }
          for (unsigned channel = 0; channel < width; channel += DIM) {
            gemmini_extended_mvin2(weight + (start + channel) * columns + column, B_ROW + channel * (TILE_COLS / DIM), TILE_COLS, DIM);
          }
          const unsigned b_end = B_ROW + width * (TILE_COLS / DIM);
          // Accumulate all K chunks into the bias at accumulator base zero.
          gemmini_loop_ws_spad(1, TILE_COLS / DIM, width / DIM, 0, 0, 0, 0, b_end, 0, 0, false, false, true, false, true, NO_ACTIVATION, 0, 0, false, LOOP_SKIPS);
          gemmini_fence();
        }
        for (unsigned block = 0; block < TILE_COLS; block += DIM) {
          gemmini_extended_mvout(output + row * columns + column + block, ACC_READ + block, DIM, DIM);
        }
        // Finish output DMA before replacing the accumulator for the next tile.
        gemmini_fence();
      }
    }
  });
}

#endif
