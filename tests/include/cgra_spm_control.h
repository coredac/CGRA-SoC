#ifndef CGRA_SPM_CONTROL_H
#define CGRA_SPM_CONTROL_H

#include "cgra_runtime.h"
#include "cgra_spm_control_generated.h"

#include <stdint.h>

typedef struct {
  uint32_t spm_word_address;
  uint32_t dma_tag;
  uint32_t packet_count;
} cgra_spm_config_t;

typedef struct {
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

static inline void cgra_spm_configure(cgra_spm_config_t config) {
  cgra_spm_write(CGRA_SPM_CONTROL_SPM_WORD_ADDRESS, config.spm_word_address);
  cgra_spm_write(CGRA_SPM_CONTROL_DMA_TAG, config.dma_tag);
  cgra_spm_write(CGRA_SPM_CONTROL_PACKET_COUNT, config.packet_count);
  cgra_spm_write(CGRA_SPM_CONTROL_HEADER_SUBMIT, 1);
}

static inline void cgra_spm_add_packet(cgra_packet_t packet) {
  cgra_spm_write64(CGRA_SPM_CONTROL_PACKET_LO, packet.lo);
  cgra_spm_write64(CGRA_SPM_CONTROL_PACKET_MID, packet.mid);
  cgra_spm_write64(CGRA_SPM_CONTROL_PACKET_HI, packet.hi);
  cgra_spm_write64(CGRA_SPM_CONTROL_PACKET_TOP, packet.top);
  cgra_spm_write(CGRA_SPM_CONTROL_PACKET_SUBMIT, 1);
}

static inline cgra_spm_result_t cgra_spm_wait(void) {
  cgra_spm_write(CGRA_SPM_CONTROL_RESULT_POP, 1);
  __asm__ volatile("fence iorw, iorw" ::: "memory");
  return (cgra_spm_result_t){
      .status = cgra_spm_read(CGRA_SPM_CONTROL_RESULT_STATUS),
      .detail = cgra_spm_read(CGRA_SPM_CONTROL_RESULT_DETAIL),
      .data = cgra_spm_read(CGRA_SPM_CONTROL_RESULT_DATA),
  };
}

#endif
