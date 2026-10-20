#ifndef BLOCK_AUTO_JOBS_H
#define BLOCK_AUTO_JOBS_H

#include "block_attention.h"

enum { BA_PUBLICATION = 60 * 1024, BA_ACC_WRITE = UINT32_C(1) << (ADDR_LEN - 1), BA_ACC_READ = BA_ACC_WRITE | (UINT32_C(1) << (ADDR_LEN - 3)) };

static void ba_offset(accel_t device, unsigned command, unsigned bytes) {
  const gemmini_patch_t patch = {.command = command, .width = 64, .source = GEMMINI_VALUE_ITERATION, .scale = bytes, .relative = 1};
  gemmini_job_patch_at(device.control, &patch);
}

// One dense chunk per invocation; the sequencer advances the source and destination.
static int ba_copy(accel_t device, unsigned job, const int8_t *input, int8_t *output, unsigned count) {
  const unsigned blocks = count / (DIM * DIM);
  if (gemmini_job_capture_at(device.control, job, 2 + 2 * blocks, 2 * blocks)) {
    return 1;
  }
  for (unsigned block = 0; block < blocks; ++block) {
    ba_offset(device, 2 + 2 * block, count);
    ba_offset(device, 3 + 2 * block, count);
  }
  accel_commands(device, {
    gemmini_config_ld(DIM);
    gemmini_config_st(DIM);
    for (unsigned block = 0; block < blocks; ++block) {
      gemmini_extended_mvin(input + block * DIM * DIM, block * DIM, DIM, DIM);
      gemmini_extended_mvout(output + block * DIM * DIM, block * DIM, DIM, DIM);
    }
  });
  return gemmini_job_end_at(device.control);
}

// Publish one or two dense vectors. Raw INT32 uses the accumulator, never INT8 scratch.
static int ba_gather(accel_t device, unsigned job, const void *left, const void *right, unsigned count, int packed) {
  const unsigned vectors = right != NULL ? 2 : 1;
  const unsigned blocks = count / (DIM * DIM);
  const unsigned bytes = packed ? 1 : sizeof(acc_t);
  if (gemmini_job_capture_at(device.control, job, 1 + 2 * blocks * vectors, blocks * vectors)) {
    return 1;
  }
  for (unsigned block = 0; block < blocks * vectors; ++block) {
    ba_offset(device, 1 + 2 * block, count * bytes);
  }
  accel_commands(device, {
    gemmini_extended3_config_ld(DIM * bytes, MVIN_SCALE_IDENTITY, false, packed ? 0 : 2);
    for (unsigned vector = 0; vector < vectors; ++vector) {
      const uint8_t *input = vector == 0 ? left : right;
      for (unsigned block = 0; block < blocks; ++block) {
        // Keep each source tile live until all STORE_SPAD commands finish.
        const unsigned row = (vector * blocks + block) * DIM;
        const unsigned local = packed ? row : BA_ACC_WRITE + row;
        const unsigned source = packed ? row : BA_ACC_READ + row;
        if (packed) {
          gemmini_extended_mvin(input + block * DIM * DIM * bytes, local, DIM, DIM);
        } else {
          gemmini_extended_mvin3(input + block * DIM * DIM * bytes, local, DIM, DIM);
        }
        gemmini_extended_mvout_spad((BA_PUBLICATION + (vector * count + block * DIM * DIM) * bytes) / DIM, bytes, source, DIM, DIM);
      }
    }
  });
  return gemmini_job_end_at(device.control);
}

// The CGRA packed window is dense; native DMA scatters each 16-column block.
static int ba_collect(accel_t device, unsigned job, unsigned source, int8_t *output, unsigned width, unsigned stride, unsigned step) {
  const unsigned blocks = width / DIM;
  if (gemmini_job_capture_at(device.control, job, 2 + 2 * blocks, blocks)) {
    return 1;
  }
  for (unsigned block = 0; block < blocks; ++block) {
    ba_offset(device, 3 + 2 * block, step);
  }
  accel_commands(device, {
    gemmini_config_ld(width);
    gemmini_config_st(stride);
    for (unsigned block = 0; block < blocks; ++block) {
      gemmini_extended_mvin((const void *)(CGRA0.spm + source + block * DIM), block * DIM, DIM, DIM);
      gemmini_extended_mvout(output + block * DIM, block * DIM, DIM, DIM);
    }
  });
  return gemmini_job_end_at(device.control);
}

// Column is fixed in each cached job; Iteration advances a full 16-row block.
static int ba_matmul(accel_t device, unsigned job, const int8_t *input, const int8_t *weight, const int32_t *bias, unsigned inner, unsigned columns, unsigned column) {
  enum { WIDTH = 64, B_ROW = 768, SKIPS = (1 << 3) | (1 << 4) | (1 << 5) | (1 << 7) };
  const unsigned loads = inner / WIDTH;
  const unsigned commands = 5 + 4 + loads + inner / DIM + 3 + 4;
  if (gemmini_job_capture_at(device.control, job, commands, loads)) {
    return 1;
  }
  for (unsigned load = 0; load < loads; ++load) {
    ba_offset(device, 9 + load, DIM * inner);
  }
  accel_commands(device, {
    gemmini_config_ex(WEIGHT_STATIONARY, NO_ACTIVATION, 0);
    gemmini_extended3_config_ld(inner, MVIN_SCALE_IDENTITY, false, 0);
    gemmini_extended3_config_ld(columns, MVIN_SCALE_IDENTITY, false, 1);
    gemmini_extended3_config_ld(0, MVIN_SCALE_IDENTITY, false, 2);
    gemmini_config_st(WIDTH * sizeof(acc_t));
    for (unsigned block = 0; block < WIDTH; block += DIM) {
      gemmini_extended_mvin3(bias + column + block, BA_ACC_WRITE + block, DIM, DIM);
    }
    for (unsigned channel = 0; channel < inner; channel += WIDTH) {
      gemmini_extended_mvin(input + channel, channel, WIDTH, DIM);
    }
    for (unsigned channel = 0; channel < inner; channel += DIM) {
      gemmini_extended_mvin2(weight + channel * columns + column, B_ROW + channel * (WIDTH / DIM), WIDTH, DIM);
    }
    gemmini_loop_ws_spad(1, WIDTH / DIM, inner / DIM, 0, 0, 0, 0, B_ROW + inner * (WIDTH / DIM), 0, 0, false, false, true, false, true, NO_ACTIVATION, 0, 0, false, SKIPS);
    for (unsigned block = 0; block < WIDTH; block += DIM) {
      gemmini_extended_mvout_spad((BA_PUBLICATION + block * sizeof(acc_t)) / DIM, WIDTH * sizeof(acc_t) / DIM, BA_ACC_READ + block, DIM, DIM);
    }
  });
  return gemmini_job_end_at(device.control);
}

// One 16x64 tile; Iteration advances columns. All loop users must retain accumulator base zero.
static int ba_columns(accel_t device, unsigned job, const int8_t *input, const int8_t *weight, const int32_t *bias, unsigned inner, unsigned columns, unsigned column) {
  enum { WIDTH = 64, CHUNK = 768, PREFIX = 4, LOOP_COMMANDS = 8 };
  const unsigned chunks = (inner + CHUNK - 1) / CHUNK;
  if (gemmini_job_capture_at(device.control, job, PREFIX + LOOP_COMMANDS * chunks + WIDTH / DIM, chunks + 1)) {
    return 1;
  }
  for (unsigned chunk = 0; chunk < chunks; ++chunk) {
    const gemmini_patch_t patch = {.command = PREFIX + LOOP_COMMANDS * chunk + 3, .operand = 1, .width = 64, .source = GEMMINI_VALUE_ITERATION, .scale = WIDTH, .relative = 1};
    gemmini_job_patch_at(device.control, &patch);
  }
  ba_offset(device, PREFIX + 4, WIDTH * sizeof(acc_t));
  accel_commands(device, {
    gemmini_extended3_config_ld(inner, MVIN_SCALE_IDENTITY, false, 0);
    gemmini_extended3_config_ld(columns, MVIN_SCALE_IDENTITY, false, 1);
    gemmini_extended3_config_ld(0, MVIN_SCALE_IDENTITY, false, 2);
    gemmini_config_st(WIDTH * sizeof(acc_t));
    for (unsigned start = 0; start < inner; start += CHUNK) {
      const unsigned remaining = inner - start;
      const unsigned width = remaining < CHUNK ? remaining : CHUNK;
      // A native config waits for the preceding loop to unroll before reusing its working rows.
      gemmini_config_ex(WEIGHT_STATIONARY, NO_ACTIVATION, 0);
      ROCC_INSTRUCTION_RS1_RS2(XCUSTOM_ACC, 0, CHUNK + width * (WIDTH / DIM), k_LOOP_WS_CONFIG_SPAD_AB);
      gemmini_loop_ws(1, WIDTH / DIM, width / DIM, 0, 0, 0, input + start, weight + start * columns + column, start == 0 ? bias + column : NULL, NULL, inner, columns, 0, WIDTH, false, false, true,
                      false, true, NO_ACTIVATION, 0, 0, false);
    }
    for (unsigned block = 0; block < WIDTH; block += DIM) {
      gemmini_extended_mvout_spad((BA_PUBLICATION + block * sizeof(acc_t)) / DIM, WIDTH * sizeof(acc_t) / DIM, BA_ACC_READ + block, DIM, DIM);
    }
  });
  return gemmini_job_end_at(device.control);
}

#endif
