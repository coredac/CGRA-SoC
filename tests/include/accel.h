#ifndef ACCEL_H
#define ACCEL_H

#include "rocc.h"

#include <stdint.h>

typedef struct {
  uint32_t id;
  uintptr_t control;
  uintptr_t spm;
  uint32_t spm_bytes;
} accel_t;

// CPU command blocks are ordered and non-nested. The target persists until the next block; selecting it does not wait for execution.
#define accel_commands(device, ...)                                                                                                                                                                    \
  do {                                                                                                                                                                                                 \
    const uint32_t accel_id_ = (device).id;                                                                                                                                                            \
    __asm__ volatile("" ::: "memory");                                                                                                                                                                 \
    ROCC_INSTRUCTION_S(0, accel_id_, 127);                                                                                                                                                             \
    __VA_ARGS__                                                                                                                                                                                        \
    __asm__ volatile("" ::: "memory");                                                                                                                                                                 \
  } while (0)

#endif
