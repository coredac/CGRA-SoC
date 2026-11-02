#ifndef ACCEL_TYPES_H
#define ACCEL_TYPES_H

#include <stdint.h>

typedef struct {
  uint32_t id;
  uintptr_t control;
  uintptr_t spm;
  uint32_t spm_bytes;
} accel_t;

#endif
