#ifndef TESTS_INCLUDE_POOL_H
#define TESTS_INCLUDE_POOL_H

#include "rocc.h"

#include <stdint.h>

enum {
  POOL_MODE_MAX = 0,
  POOL_MODE_AVERAGE = 1,
  POOL_STATUS_SUCCESS = 0,
};

typedef struct {
  uint32_t mode;
  uintptr_t source;
  uintptr_t destination;
  uint32_t input_height;
  uint32_t input_width;
  uint32_t channels;
  uint32_t kernel_height;
  uint32_t kernel_width;
  uint32_t stride_height;
  uint32_t stride_width;
  uint32_t pad_height;
  uint32_t pad_width;
} pool_job_t;

#define POOL_CMD_SOURCE 0
#define POOL_CMD_DESTINATION 1
#define POOL_CMD_SHAPE 2
#define POOL_CMD_CHANNELS 3
#define POOL_CMD_KERNEL 4
#define POOL_CMD_STRIDE 5
#define POOL_CMD_PADDING 6
#define POOL_CMD_START 7
#define POOL_CMD_WAIT 8
#define POOL_CMD_MODE 9

static inline void pool_configure(const pool_job_t *job) {
  ROCC_INSTRUCTION_S(2, job->source, POOL_CMD_SOURCE);
  ROCC_INSTRUCTION_S(2, job->destination, POOL_CMD_DESTINATION);
  ROCC_INSTRUCTION_SS(2, job->input_height, job->input_width, POOL_CMD_SHAPE);
  ROCC_INSTRUCTION_S(2, job->channels, POOL_CMD_CHANNELS);
  ROCC_INSTRUCTION_SS(2, job->kernel_height, job->kernel_width, POOL_CMD_KERNEL);
  ROCC_INSTRUCTION_SS(2, job->stride_height, job->stride_width, POOL_CMD_STRIDE);
  ROCC_INSTRUCTION_SS(2, job->pad_height, job->pad_width, POOL_CMD_PADDING);
  ROCC_INSTRUCTION_S(2, job->mode, POOL_CMD_MODE);
}

static inline void pool_start(void) { ROCC_INSTRUCTION_S(2, 0, POOL_CMD_START); }

static inline uint32_t pool_wait(void) {
  uint64_t status;
  ROCC_INSTRUCTION_D(2, status, POOL_CMD_WAIT);
  __asm__ volatile("fence rw, rw" ::: "memory");
  return (uint32_t)status;
}

#endif
