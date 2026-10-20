#ifndef BLOCK_SPM_H
#define BLOCK_SPM_H

#include "block_cgra.h"
#include "gemmini.h"

// Dense INT8 matrices; rows % 16 = columns % 64 = inner % 64 = 0, inner <= 3072.
// Owns Gemmini bytes [0,65536), accumulator rows [0,64), and CGRA words [0,2048).
// Input and output SPM regions must be disjoint from this workspace and each other.
static int block_project_spm(accel_t device, const elem_t *input, const elem_t *weight, const acc_t *bias, elem_t *output, unsigned rows, unsigned inner, unsigned columns, unsigned divisor) {
  enum {
    TILE_COLS = 64,
    TILE_WORDS = DIM * TILE_COLS,
    CHUNK = 768,
    B_ROW = CHUNK,
    ROW_BYTES = DIM * sizeof(elem_t),
    PUBLICATION = 60 * 1024,
    ACC_WRITE = UINT32_C(1) << (ADDR_LEN - 1),
    ACC_READ = ACC_WRITE | (UINT32_C(1) << (ADDR_LEN - 3)),
    LOOP_SKIPS = (1 << 3) | (1 << 4) | (1 << 5) | (1 << 7),
  };
  const uint32_t args[] = {0, TILE_WORDS, divisor, TILE_WORDS};
  accel_commands(device, {
    gemmini_config_ex(WEIGHT_STATIONARY, NO_ACTIVATION, 0);
    gemmini_extended3_config_ld(inner * sizeof(elem_t), MVIN_SCALE_IDENTITY, false, 0);
    gemmini_extended3_config_ld(columns * sizeof(elem_t), MVIN_SCALE_IDENTITY, false, 1);
    gemmini_extended3_config_ld(0, MVIN_SCALE_IDENTITY, false, 2);
    gemmini_config_st(TILE_COLS * sizeof(acc_t));
  });
  for (unsigned row = 0; row < rows; row += DIM) {
    for (unsigned column = 0; column < columns; column += TILE_COLS) {
      accel_commands(device, {
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
          gemmini_loop_ws_spad(1, TILE_COLS / DIM, width / DIM, 0, 0, 0, 0, b_end, 0, 0, false, false, true, false, true, NO_ACTIVATION, 0, 0, false, LOOP_SKIPS);
          gemmini_fence();
        }
        for (unsigned block = 0; block < TILE_COLS; block += DIM) {
          gemmini_extended_mvout_spad((PUBLICATION + block * sizeof(acc_t)) / ROW_BYTES, TILE_COLS * sizeof(acc_t) / ROW_BYTES, ACC_READ + block, DIM, DIM);
        }
        gemmini_fence();
      });
      if (block_load((const void *)(device.spm + PUBLICATION), 0, TILE_WORDS, 0) || block_kernel(&BLOCK_REQUANT, args, row != 0 || column != 0)) {
        return 1;
      }
      for (unsigned local_row = 0; local_row < DIM; ++local_row) {
        if (block_store(output + (row + local_row) * columns + column, TILE_WORDS + local_row * TILE_COLS, TILE_COLS)) {
          return 1;
        }
      }
    }
  }
  return 0;
}

#endif
