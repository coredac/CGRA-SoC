#ifndef GEMMINI_TILE_H
#define GEMMINI_TILE_H

#include "accel.h"
#include "gemmini.h"
#include "gemmini_job.h"

/* Conservative capture bounds across all preload/publish/store choices. */
#define GEMMINI_TILE_COMMANDS 8
#define GEMMINI_TILE_PATCHES 3

/* Native INT8 x INT8 -> INT32, with K/N and tile rows bounded by DIM.
 * The caller chooses storage, tile strides and whether to preload the input.
 * Scratch addresses and publication strides count native INT8 rows. */
typedef struct {
  unsigned rows, tile_rows, k, n;
  unsigned input_row, weight_row;
  const elem_t *input, *weight;
  acc_t *output;
  uintptr_t input_stride, output_stride;
  unsigned preload_input;
  unsigned publish, publish_row, publish_stride;
  unsigned store;
} gemmini_tile_t;

typedef enum { GEMMINI_TILE_COUNT, GEMMINI_TILE_PATCH, GEMMINI_TILE_ISSUE } gemmini_tile_mode_t;

typedef struct {
  unsigned commands, patches;
  gemmini_tile_mode_t mode;
  uintptr_t control;
} gemmini_tile_stream_t;

static inline void gemmini_tile_patch(gemmini_tile_stream_t *stream, unsigned width, unsigned source, unsigned stride) {
  const gemmini_patch_t patch = {.command = stream->commands, .width = width, .source = source, .scale = stride, .relative = 1};
  if (stream->mode == GEMMINI_TILE_PATCH) {
    gemmini_job_patch_at(stream->control, &patch);
  }
  ++stream->patches;
}

#define GEMMINI_TILE_COMMAND(...)                                                                                                                                                                      \
  do {                                                                                                                                                                                                 \
    if (stream.mode == GEMMINI_TILE_ISSUE) {                                                                                                                                                           \
      __VA_ARGS__;                                                                                                                                                                                     \
    }                                                                                                                                                                                                  \
    ++stream.commands;                                                                                                                                                                                 \
  } while (0)

/* Counting, capture and CPU execution use this same native command sequence. */
static inline gemmini_tile_stream_t gemmini_tile_emit(const gemmini_tile_t *params, gemmini_tile_mode_t mode, uintptr_t control, unsigned tile, unsigned slot) {
  gemmini_tile_stream_t stream = {.mode = mode, .control = control};
  const uint32_t acc_write = UINT32_C(1) << (ADDR_LEN - 1);
  const uint32_t acc_read = acc_write | (UINT32_C(1) << (ADDR_LEN - 3));
  if (!params->preload_input) {
    GEMMINI_TILE_COMMAND(gemmini_config_ld(params->k * sizeof(elem_t)));
  }
  GEMMINI_TILE_COMMAND(gemmini_config_ex(WEIGHT_STATIONARY, NO_ACTIVATION, 0));
  GEMMINI_TILE_COMMAND(gemmini_config_st(params->n * sizeof(acc_t)));
  if (!params->preload_input) {
    gemmini_tile_patch(&stream, 64, GEMMINI_VALUE_TILE_ID, params->input_stride);
    GEMMINI_TILE_COMMAND(gemmini_extended_mvin((const void *)((uintptr_t)params->input + tile * params->input_stride), params->input_row, params->k, params->tile_rows));
  }
  GEMMINI_TILE_COMMAND(gemmini_extended_preload(params->weight_row, acc_write, params->n, params->k, params->n, params->tile_rows));
  const unsigned input_row = params->input_row + (params->preload_input ? tile * params->tile_rows : 0);
  if (params->preload_input) {
    gemmini_tile_patch(&stream, 32, GEMMINI_VALUE_TILE_ID, params->tile_rows);
  }
  GEMMINI_TILE_COMMAND(gemmini_extended_compute_preloaded(input_row, GARBAGE_ADDR, params->k, params->tile_rows, params->n, params->tile_rows));
  if (params->publish) {
    gemmini_tile_patch(&stream, 32, GEMMINI_VALUE_SLOT, params->publish_stride);
    GEMMINI_TILE_COMMAND(gemmini_extended_mvout_spad(params->publish_row + slot * params->publish_stride, params->n * sizeof(acc_t) / DIM, acc_read, params->n, params->tile_rows));
  }
  if (params->store) {
    gemmini_tile_patch(&stream, 64, GEMMINI_VALUE_TILE_ID, params->output_stride);
    GEMMINI_TILE_COMMAND(gemmini_extended_mvout((void *)((uintptr_t)params->output + tile * params->output_stride), acc_read, params->n, params->tile_rows));
  }
  return stream;
}

#undef GEMMINI_TILE_COMMAND

static inline void gemmini_tile_init(accel_t device, const gemmini_tile_t *params) {
  accel_commands(device, {
    gemmini_flush(0);
    gemmini_config_ld(params->n * sizeof(elem_t));
    gemmini_extended_mvin(params->weight, params->weight_row, params->n, params->k);
    if (params->preload_input) {
      gemmini_config_ld(params->k * sizeof(elem_t));
      for (unsigned row = 0; row < params->rows; row += DIM) {
        const unsigned remaining = params->rows - row;
        const unsigned rows = remaining < DIM ? remaining : DIM;
        gemmini_extended_mvin(params->input + row * params->k, params->input_row + row, params->k, rows);
      }
    }
    gemmini_fence();
  });
}

static inline int gemmini_tile_capture(accel_t device, unsigned job, const gemmini_tile_t *params) {
  const gemmini_tile_stream_t counts = gemmini_tile_emit(params, GEMMINI_TILE_COUNT, device.control, 0, 0);
  if (gemmini_job_capture_at(device.control, job, counts.commands, counts.patches)) {
    return 1;
  }
  gemmini_tile_emit(params, GEMMINI_TILE_PATCH, device.control, 0, 0);
  accel_commands(device, { gemmini_tile_emit(params, GEMMINI_TILE_ISSUE, device.control, 0, 0); });
  return gemmini_job_end_at(device.control);
}

/* Submission only; the caller waits before reusing scratch or reading output. */
static inline void gemmini_tile_run(accel_t device, const gemmini_tile_t *params, unsigned tile, unsigned slot) {
  accel_commands(device, { gemmini_tile_emit(params, GEMMINI_TILE_ISSUE, device.control, tile, slot); });
}

#endif
