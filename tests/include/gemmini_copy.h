#ifndef GEMMINI_COPY_H
#define GEMMINI_COPY_H

#include "../../runtime/planning/workspace.h"
#include "accel.h"
#include "gemmini.h"
#include "gemmini_job.h"

#define GEMMINI_COPY_COMMANDS 4
#define GEMMINI_COPY_PATCHES 0

typedef struct {
  unsigned rows, columns, element_bytes;
  uintptr_t source_stride, destination_stride;
  const void *source;
  void *destination;
} gemmini_copy_t;

/* Column blocks use DIM-spaced rows in the native working half. Strides are bytes. */
static inline int gemmini_copy_check(const gemmini_copy_t *params) {
  return !gemmini_copy_valid(params->rows, params->columns, params->element_bytes, params->source_stride, params->destination_stride);
}

static inline void gemmini_copy_emit(const gemmini_copy_t *params) {
  const int full = params->element_bytes == sizeof(acc_t);
  const uint32_t write = full ? UINT32_C(1) << (ADDR_LEN - 1) : 0;
  const uint32_t read = full ? write | (UINT32_C(1) << (ADDR_LEN - 3)) : 0;
  gemmini_extended3_config_ld(params->source_stride, MVIN_SCALE_IDENTITY, false, full ? 2 : 0);
  gemmini_config_st(params->destination_stride);
  if (full) {
    gemmini_extended_mvin3(params->source, write, params->columns, params->rows);
  } else {
    gemmini_extended_mvin(params->source, write, params->columns, params->rows);
  }
  gemmini_extended_mvout(params->destination, read, params->columns, params->rows);
}

static inline int gemmini_copy_capture(accel_t device, unsigned job, const gemmini_copy_t *params) {
  if (gemmini_copy_check(params) || gemmini_job_capture_at(device.control, job, GEMMINI_COPY_COMMANDS, GEMMINI_COPY_PATCHES)) {
    return 1;
  }
  accel_commands(device, { gemmini_copy_emit(params); });
  return gemmini_job_end_at(device.control);
}

static inline int gemmini_copy_run(accel_t device, const gemmini_copy_t *params) {
  if (gemmini_copy_check(params)) {
    return 1;
  }
  accel_commands(device, { gemmini_copy_emit(params); });
  return 0;
}

#endif
