#ifndef CGRA_TRANSFER_CONTROL_H
#define CGRA_TRANSFER_CONTROL_H

#include "cgra_runtime.h"
#include "cgra_transfer_control_generated.h"

#include <stdint.h>

typedef struct {
  uint32_t job_id;
  uint32_t slot;
  uint32_t bytes;
  uint32_t spm_word_address;
  uint32_t dma_tag;
} cgra_transfer_pull_descriptor_t;

typedef struct {
  uint32_t job_id;
  uint32_t slot;
  uint32_t bytes;
  uint32_t spm_word_address;
  uint32_t dma_tag;
  uint32_t packet_count;
} cgra_transfer_launch_header_t;

typedef struct {
  uint32_t job_id;
  uint32_t slot;
  uint32_t requested_bytes;
  uint32_t actual_bytes;
  uint32_t spm_word_address;
  uint32_t dma_tag;
  uint32_t packet_count;
  uint32_t status;
} cgra_transfer_launch_result_t;

typedef struct {
  uint32_t job_id;
  uint32_t slot;
  uint32_t requested_bytes;
  uint32_t actual_bytes;
  uint32_t spm_word_address;
  uint32_t dma_tag;
  uint32_t operation;
  uint32_t reason;
} cgra_transfer_protocol_error_t;

typedef struct {
  uint32_t job_id;
  uint32_t slot;
  uint32_t requested_bytes;
  uint32_t actual_bytes;
  uint32_t spm_word_address;
  uint32_t dma_tag;
  uint32_t packet_count;
  uint32_t complete_data;
  uint32_t status;
} cgra_transfer_compute_result_t;

static inline volatile uint32_t *cgra_transfer_reg32(uintptr_t offset) {
  return (volatile uint32_t *)(CGRA_TRANSFER_CONTROL_BASE + offset);
}

static inline volatile uint64_t *cgra_transfer_reg64(uintptr_t offset) {
  return (volatile uint64_t *)(CGRA_TRANSFER_CONTROL_BASE + offset);
}

static inline uint32_t cgra_transfer_read32(uintptr_t offset) {
  return *cgra_transfer_reg32(offset);
}

static inline void cgra_transfer_write32(uintptr_t offset, uint32_t value) {
  *cgra_transfer_reg32(offset) = value;
}

static inline void cgra_transfer_write64(uintptr_t offset, uint64_t value) {
  *cgra_transfer_reg64(offset) = value;
}

static inline void
cgra_transfer_submit_pull(cgra_transfer_pull_descriptor_t descriptor) {
  cgra_transfer_write32(CGRA_TRANSFER_CONTROL_PULL_JOB_ID, descriptor.job_id);
  cgra_transfer_write32(CGRA_TRANSFER_CONTROL_PULL_SLOT, descriptor.slot);
  cgra_transfer_write32(CGRA_TRANSFER_CONTROL_PULL_BYTES, descriptor.bytes);
  cgra_transfer_write32(CGRA_TRANSFER_CONTROL_PULL_SPM_WORD_ADDRESS,
                        descriptor.spm_word_address);
  cgra_transfer_write32(CGRA_TRANSFER_CONTROL_PULL_DMA_TAG, descriptor.dma_tag);
  cgra_transfer_write32(CGRA_TRANSFER_CONTROL_PULL_SUBMIT, 1);
}

static inline void
cgra_transfer_submit_launch_header(cgra_transfer_launch_header_t header) {
  cgra_transfer_write32(CGRA_TRANSFER_CONTROL_LAUNCH_JOB_ID, header.job_id);
  cgra_transfer_write32(CGRA_TRANSFER_CONTROL_LAUNCH_SLOT, header.slot);
  cgra_transfer_write32(CGRA_TRANSFER_CONTROL_LAUNCH_BYTES, header.bytes);
  cgra_transfer_write32(CGRA_TRANSFER_CONTROL_LAUNCH_SPM_WORD_ADDRESS,
                        header.spm_word_address);
  cgra_transfer_write32(CGRA_TRANSFER_CONTROL_LAUNCH_DMA_TAG, header.dma_tag);
  cgra_transfer_write32(CGRA_TRANSFER_CONTROL_LAUNCH_PACKET_COUNT,
                        header.packet_count);
  cgra_transfer_write32(CGRA_TRANSFER_CONTROL_LAUNCH_SUBMIT, 1);
}

static inline void cgra_transfer_submit_launch_packet(cgra_packet_t packet) {
  cgra_transfer_write64(CGRA_TRANSFER_CONTROL_PACKET_LO, packet.lo);
  cgra_transfer_write64(CGRA_TRANSFER_CONTROL_PACKET_MID, packet.mid);
  cgra_transfer_write64(CGRA_TRANSFER_CONTROL_PACKET_HI, packet.hi);
  cgra_transfer_write64(CGRA_TRANSFER_CONTROL_PACKET_TOP, packet.top);
  cgra_transfer_write32(CGRA_TRANSFER_CONTROL_PACKET_SUBMIT, 1);
}

static inline cgra_transfer_launch_result_t
cgra_transfer_wait_launch_result(void) {
  cgra_transfer_write32(CGRA_TRANSFER_CONTROL_LAUNCH_RESULT_POP, 1);
  __asm__ volatile("fence iorw, iorw" ::: "memory");
  return (cgra_transfer_launch_result_t){
      .job_id =
          cgra_transfer_read32(CGRA_TRANSFER_CONTROL_LAUNCH_RESULT_JOB_ID),
      .slot = cgra_transfer_read32(CGRA_TRANSFER_CONTROL_LAUNCH_RESULT_SLOT),
      .requested_bytes = cgra_transfer_read32(
          CGRA_TRANSFER_CONTROL_LAUNCH_RESULT_REQUESTED_BYTES),
      .actual_bytes = cgra_transfer_read32(
          CGRA_TRANSFER_CONTROL_LAUNCH_RESULT_ACTUAL_BYTES),
      .spm_word_address = cgra_transfer_read32(
          CGRA_TRANSFER_CONTROL_LAUNCH_RESULT_SPM_WORD_ADDRESS),
      .dma_tag =
          cgra_transfer_read32(CGRA_TRANSFER_CONTROL_LAUNCH_RESULT_DMA_TAG),
      .packet_count = cgra_transfer_read32(
          CGRA_TRANSFER_CONTROL_LAUNCH_RESULT_PACKET_COUNT),
      .status =
          cgra_transfer_read32(CGRA_TRANSFER_CONTROL_LAUNCH_RESULT_STATUS),
  };
}

static inline cgra_transfer_protocol_error_t
cgra_transfer_wait_launch_error(void) {
  cgra_transfer_write32(CGRA_TRANSFER_CONTROL_LAUNCH_ERROR_POP, 1);
  __asm__ volatile("fence iorw, iorw" ::: "memory");
  return (cgra_transfer_protocol_error_t){
      .job_id = cgra_transfer_read32(CGRA_TRANSFER_CONTROL_LAUNCH_ERROR_JOB_ID),
      .slot = cgra_transfer_read32(CGRA_TRANSFER_CONTROL_LAUNCH_ERROR_SLOT),
      .requested_bytes = cgra_transfer_read32(
          CGRA_TRANSFER_CONTROL_LAUNCH_ERROR_REQUESTED_BYTES),
      .actual_bytes =
          cgra_transfer_read32(CGRA_TRANSFER_CONTROL_LAUNCH_ERROR_ACTUAL_BYTES),
      .spm_word_address = cgra_transfer_read32(
          CGRA_TRANSFER_CONTROL_LAUNCH_ERROR_SPM_WORD_ADDRESS),
      .dma_tag =
          cgra_transfer_read32(CGRA_TRANSFER_CONTROL_LAUNCH_ERROR_DMA_TAG),
      .operation =
          cgra_transfer_read32(CGRA_TRANSFER_CONTROL_LAUNCH_ERROR_OPERATION),
      .reason = cgra_transfer_read32(CGRA_TRANSFER_CONTROL_LAUNCH_ERROR_REASON),
  };
}

static inline cgra_transfer_compute_result_t
cgra_transfer_wait_compute_result(void) {
  cgra_transfer_write32(CGRA_TRANSFER_CONTROL_COMPUTE_RESULT_POP, 1);
  __asm__ volatile("fence iorw, iorw" ::: "memory");
  return (cgra_transfer_compute_result_t){
      .job_id =
          cgra_transfer_read32(CGRA_TRANSFER_CONTROL_COMPUTE_RESULT_JOB_ID),
      .slot = cgra_transfer_read32(CGRA_TRANSFER_CONTROL_COMPUTE_RESULT_SLOT),
      .requested_bytes = cgra_transfer_read32(
          CGRA_TRANSFER_CONTROL_COMPUTE_RESULT_REQUESTED_BYTES),
      .actual_bytes = cgra_transfer_read32(
          CGRA_TRANSFER_CONTROL_COMPUTE_RESULT_ACTUAL_BYTES),
      .spm_word_address = cgra_transfer_read32(
          CGRA_TRANSFER_CONTROL_COMPUTE_RESULT_SPM_WORD_ADDRESS),
      .dma_tag =
          cgra_transfer_read32(CGRA_TRANSFER_CONTROL_COMPUTE_RESULT_DMA_TAG),
      .packet_count = cgra_transfer_read32(
          CGRA_TRANSFER_CONTROL_COMPUTE_RESULT_PACKET_COUNT),
      .complete_data =
          cgra_transfer_read32(CGRA_TRANSFER_CONTROL_COMPUTE_RESULT_DATA),
      .status =
          cgra_transfer_read32(CGRA_TRANSFER_CONTROL_COMPUTE_RESULT_STATUS),
  };
}

static inline cgra_transfer_protocol_error_t
cgra_transfer_wait_compute_error(void) {
  cgra_transfer_write32(CGRA_TRANSFER_CONTROL_COMPUTE_ERROR_POP, 1);
  __asm__ volatile("fence iorw, iorw" ::: "memory");
  return (cgra_transfer_protocol_error_t){
      .job_id =
          cgra_transfer_read32(CGRA_TRANSFER_CONTROL_COMPUTE_ERROR_JOB_ID),
      .slot = cgra_transfer_read32(CGRA_TRANSFER_CONTROL_COMPUTE_ERROR_SLOT),
      .requested_bytes = cgra_transfer_read32(
          CGRA_TRANSFER_CONTROL_COMPUTE_ERROR_REQUESTED_BYTES),
      .actual_bytes = cgra_transfer_read32(
          CGRA_TRANSFER_CONTROL_COMPUTE_ERROR_ACTUAL_BYTES),
      .spm_word_address = cgra_transfer_read32(
          CGRA_TRANSFER_CONTROL_COMPUTE_ERROR_SPM_WORD_ADDRESS),
      .dma_tag =
          cgra_transfer_read32(CGRA_TRANSFER_CONTROL_COMPUTE_ERROR_DMA_TAG),
      .operation =
          cgra_transfer_read32(CGRA_TRANSFER_CONTROL_COMPUTE_ERROR_OPERATION),
      .reason =
          cgra_transfer_read32(CGRA_TRANSFER_CONTROL_COMPUTE_ERROR_REASON),
  };
}

#endif
