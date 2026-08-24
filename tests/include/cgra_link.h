#ifndef CGRA_LINK_H
#define CGRA_LINK_H

#include "cgra_link_control_generated.h"
#include "cgra_runtime.h"

#include <stdint.h>

typedef struct {
  uint32_t status;
  uint32_t detail;
  uint32_t data;
} cgra_link_result_t;

static inline volatile uint32_t *cgra_link_reg(uintptr_t offset) {
  return (volatile uint32_t *)(CGRA_LINK_CONTROL_BASE + offset);
}

static inline uint32_t cgra_link_read(uintptr_t offset) {
  return *cgra_link_reg(offset);
}

static inline void cgra_link_write(uintptr_t offset, uint32_t value) {
  *cgra_link_reg(offset) = value;
}

static inline void cgra_link_configure(uint32_t packet_count) {
  cgra_link_write(CGRA_LINK_CONTROL_PACKET_COUNT, packet_count);
  cgra_link_write(CGRA_LINK_CONTROL_CONFIG_SUBMIT, 1);
  __asm__ volatile("fence iorw, iorw" ::: "memory");
}

static inline void cgra_link_queue(cgra_packet_t packet) {
  CGRA_RAW_PKT_LO(packet.lo);
  CGRA_RAW_PKT_MID(packet.mid);
  CGRA_SPM_PKT_HI(packet.hi);
#if CGRA_INTRA_PKT_NBITS > 192
  CGRA_SPM_PKT_TOP(packet.top);
#endif
}

static inline cgra_link_result_t cgra_link_wait(void) {
  cgra_link_write(CGRA_LINK_CONTROL_RESULT_POP, 1);
  __asm__ volatile("fence iorw, iorw" ::: "memory");
  return (cgra_link_result_t){
      .status = cgra_link_read(CGRA_LINK_CONTROL_RESULT_STATUS),
      .detail = cgra_link_read(CGRA_LINK_CONTROL_RESULT_DETAIL),
      .data = cgra_link_read(CGRA_LINK_CONTROL_RESULT_DATA),
  };
}

#endif
