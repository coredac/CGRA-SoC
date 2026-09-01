#ifndef GEMMINI_JOB_H
#define GEMMINI_JOB_H

#include "cgra_link_control_generated.h"

#include <stdint.h>

static inline volatile uint32_t *gemmini_job_reg(uintptr_t offset) { return (volatile uint32_t *)(GEMMINI_JOB_BASE + offset); }

static inline void gemmini_job_write(uintptr_t offset, uint32_t value) { *gemmini_job_reg(offset) = value; }

static inline void gemmini_job_submit(uint32_t a_row, uint32_t b_row, uint32_t acc_address, uint32_t output_row, uint32_t output_rows) {
  gemmini_job_write(GEMMINI_JOB_A_ROW, a_row);
  gemmini_job_write(GEMMINI_JOB_B_ROW, b_row);
  gemmini_job_write(GEMMINI_JOB_ACC_ADDRESS, acc_address);
  gemmini_job_write(GEMMINI_JOB_OUTPUT_ROW, output_row);
  gemmini_job_write(GEMMINI_JOB_OUTPUT_ROWS, output_rows);
  gemmini_job_write(GEMMINI_JOB_SUBMIT, 1);
  __asm__ volatile("fence iorw, iorw" ::: "memory");
}

#endif
