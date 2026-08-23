#ifndef CGRA_SPM_CONTROL_H
#define CGRA_SPM_CONTROL_H

#include "cgra_runtime.h"
#include "cgra_spm_control_generated.h"

#include <stdint.h>

typedef struct {
  uint32_t job_id;
  uint32_t slot;
  uint32_t bytes;
  uint32_t mode;
  uint32_t spm_word_address;
  uint32_t dma_tag;
  uint32_t packet_count;
} cgra_spm_job_t;

typedef struct {
  uint32_t job_id;
  uint32_t slot;
  uint32_t bytes;
  uint32_t stage;
  uint32_t status;
  uint32_t detail;
  uint32_t data;
} cgra_spm_result_t;

static inline volatile uint32_t *cgra_spm_reg32(uintptr_t offset) {
  return (volatile uint32_t *)(CGRA_SPM_CONTROL_BASE + offset);
}

static inline volatile uint64_t *cgra_spm_reg64(uintptr_t offset) {
  return (volatile uint64_t *)(CGRA_SPM_CONTROL_BASE + offset);
}

static inline uint32_t cgra_spm_read(uintptr_t offset) {
  return *cgra_spm_reg32(offset);
}

static inline void cgra_spm_write(uintptr_t offset, uint32_t value) {
  *cgra_spm_reg32(offset) = value;
}

static inline void cgra_spm_write64(uintptr_t offset, uint64_t value) {
  *cgra_spm_reg64(offset) = value;
}

static inline void cgra_spm_set_job(cgra_spm_job_t job) {
  cgra_spm_write(CGRA_SPM_CONTROL_JOB_ID, job.job_id);
  cgra_spm_write(CGRA_SPM_CONTROL_SLOT, job.slot);
  cgra_spm_write(CGRA_SPM_CONTROL_BYTES, job.bytes);
  cgra_spm_write(CGRA_SPM_CONTROL_MODE, job.mode);
  cgra_spm_write(CGRA_SPM_CONTROL_SPM_WORD_ADDRESS, job.spm_word_address);
  cgra_spm_write(CGRA_SPM_CONTROL_DMA_TAG, job.dma_tag);
  cgra_spm_write(CGRA_SPM_CONTROL_PACKET_COUNT, job.packet_count);
  cgra_spm_write(CGRA_SPM_CONTROL_HEADER_SUBMIT, 1);
}

static inline void cgra_spm_add_packet(cgra_packet_t packet) {
  cgra_spm_write64(CGRA_SPM_CONTROL_PACKET_LO, packet.lo);
  cgra_spm_write64(CGRA_SPM_CONTROL_PACKET_MID, packet.mid);
  cgra_spm_write64(CGRA_SPM_CONTROL_PACKET_HI, packet.hi);
  cgra_spm_write64(CGRA_SPM_CONTROL_PACKET_TOP, packet.top);
  cgra_spm_write(CGRA_SPM_CONTROL_PACKET_SUBMIT, 1);
}

static inline void cgra_spm_start(void) {
  cgra_spm_write(CGRA_SPM_CONTROL_JOB_SUBMIT, 1);
}

static inline void cgra_spm_step(void) {
  cgra_spm_write(CGRA_SPM_CONTROL_STEP_SUBMIT, 1);
}

static inline cgra_spm_result_t cgra_spm_wait(void) {
  cgra_spm_write(CGRA_SPM_CONTROL_RESULT_POP, 1);
  __asm__ volatile("fence iorw, iorw" ::: "memory");
  return (cgra_spm_result_t){
      .job_id = cgra_spm_read(CGRA_SPM_CONTROL_RESULT_JOB_ID),
      .slot = cgra_spm_read(CGRA_SPM_CONTROL_RESULT_SLOT),
      .bytes = cgra_spm_read(CGRA_SPM_CONTROL_RESULT_BYTES),
      .stage = cgra_spm_read(CGRA_SPM_CONTROL_RESULT_STAGE),
      .status = cgra_spm_read(CGRA_SPM_CONTROL_RESULT_STATUS),
      .detail = cgra_spm_read(CGRA_SPM_CONTROL_RESULT_DETAIL),
      .data = cgra_spm_read(CGRA_SPM_CONTROL_RESULT_DATA),
  };
}

#endif
