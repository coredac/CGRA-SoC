#ifndef GCN_JOBS_H
#define GCN_JOBS_H

#include "accel_generated.h"
#include "gemmini.h"
#include "gemmini_job.h"

// Capture one 16x64 output tile of a 64-input, 128-output projection.
// Input and output already point to the row block; column is 0 or 64.
static int gcn_projection_job(unsigned job, const int8_t *input, const int8_t *weight, const int32_t *bias, int32_t *output, unsigned column) {
  enum {
    INNER = 64,
    COLUMNS = 128,
    WIDTH = 64,
    B_ROW = 768,
    ACC_WRITE = UINT32_C(1) << (ADDR_LEN - 1),
    ACC_READ = ACC_WRITE | (UINT32_C(1) << (ADDR_LEN - 3)),
    SKIPS = (1 << 3) | (1 << 4) | (1 << 5) | (1 << 7),
    COMMANDS = 5 + WIDTH / DIM + 1 + INNER / DIM + 3 + WIDTH / DIM,
  };
  if (gemmini_job_capture_at(GEMMINI0.control, job, COMMANDS, 0)) {
    return 1;
  }
  accel_commands(GEMMINI0, {
    gemmini_config_ex(WEIGHT_STATIONARY, NO_ACTIVATION, 0);
    gemmini_extended3_config_ld(INNER, MVIN_SCALE_IDENTITY, false, 0);
    gemmini_extended3_config_ld(COLUMNS, MVIN_SCALE_IDENTITY, false, 1);
    gemmini_extended3_config_ld(0, MVIN_SCALE_IDENTITY, false, 2);
    gemmini_config_st(COLUMNS * sizeof(acc_t));
    for (unsigned block = 0; block < WIDTH; block += DIM) {
      gemmini_extended_mvin3(bias + column + block, ACC_WRITE + block, DIM, DIM);
    }
    gemmini_extended_mvin(input, 0, WIDTH, DIM);
    for (unsigned channel = 0; channel < INNER; channel += DIM) {
      gemmini_extended_mvin2(weight + channel * COLUMNS + column, B_ROW + channel * (WIDTH / DIM), WIDTH, DIM);
    }
    gemmini_loop_ws_spad(1, WIDTH / DIM, INNER / DIM, 0, 0, 0, 0, B_ROW + INNER * (WIDTH / DIM), 0, 0, false, false, true, false, true, NO_ACTIVATION, 0, 0, false, SKIPS);
    for (unsigned block = 0; block < WIDTH; block += DIM) {
      gemmini_extended_mvout(output + column + block, ACC_READ + block, DIM, DIM);
    }
  });
  return gemmini_job_end_at(GEMMINI0.control);
}

#endif
