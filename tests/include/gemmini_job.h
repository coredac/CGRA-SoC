#ifndef GEMMINI_JOB_H
#define GEMMINI_JOB_H

#include "cgra_link_control_generated.h"

#include <stdint.h>

static inline volatile uint32_t *gemmini_job_reg(uintptr_t offset) { return (volatile uint32_t *)(GEMMINI_JOB_BASE + offset); }

static inline uint32_t gemmini_job_read(uintptr_t offset) { return *gemmini_job_reg(offset); }

static inline void gemmini_job_write(uintptr_t offset, uint32_t value) { *gemmini_job_reg(offset) = value; }

static inline int gemmini_job_capture(uint32_t job, uint32_t command_count, uint32_t conv_template) {
  gemmini_job_write(GEMMINI_JOB_SELECT, job);
  gemmini_job_write(GEMMINI_JOB_COMMAND_COUNT, command_count);
  gemmini_job_write(GEMMINI_JOB_CONV_TEMPLATE, conv_template);
  gemmini_job_write(GEMMINI_JOB_SUBMIT, 1);
  __asm__ volatile("" ::: "memory");
  while (gemmini_job_read(GEMMINI_JOB_CONFIG_READY) == 0) {
  }
  return gemmini_job_read(GEMMINI_JOB_CONFIG_STATUS) != AUTO_LINK_STATUS_SUCCESS;
}

static inline int gemmini_job_begin_id(uint32_t job, uint32_t command_count) { return gemmini_job_capture(job, command_count, 0); }

static inline int gemmini_conv_begin(uint32_t job, uint32_t command_count) { return gemmini_job_capture(job, command_count, 1); }

static inline int gemmini_job_begin(uint32_t command_count) { return gemmini_job_begin_id(0, command_count); }

static inline int gemmini_job_end(void) {
  // Capture acknowledgement must not wait for Rocket's RoCC busy fence.
  __asm__ volatile("" ::: "memory");
  while (gemmini_job_read(GEMMINI_JOB_CONFIG_DONE) == 0) {
  }
  return gemmini_job_read(GEMMINI_JOB_CONFIG_STATUS) != AUTO_LINK_STATUS_SUCCESS;
}

#endif
