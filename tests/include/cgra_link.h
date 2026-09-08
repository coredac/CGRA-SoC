#ifndef CGRA_LINK_H
#define CGRA_LINK_H

#include "cgra_link_control_generated.h"
#include "cgra_runtime.h"

#include <stdint.h>

typedef struct {
  uint32_t status;
  uint32_t detail;
  uint32_t data;
  uint32_t stage;
  uint32_t job;
} cgra_link_result_t;

enum { CGRA_LINK_SLOT = 0, CGRA_LINK_ELEMENTS = 1, CGRA_LINK_TILE_ID = 2 };

typedef struct {
  uint32_t base;
  uint32_t stride;
  uint32_t source;
} cgra_link_symbol_t;

typedef struct {
  uint32_t packet_index;
  uint32_t bit_offset;
  uint32_t bit_width;
  uint32_t symbol_index;
  uint32_t scale;
  uint32_t offset;
} cgra_link_patch_t;

static inline volatile uint32_t *cgra_link_reg(uintptr_t offset) { return (volatile uint32_t *)(CGRA_LINK_CONTROL_BASE + offset); }

static inline uint32_t cgra_link_read(uintptr_t offset) { return *cgra_link_reg(offset); }

static inline void cgra_link_write(uintptr_t offset, uint32_t value) { *cgra_link_reg(offset) = value; }

// Captured by the next begin; other jobs keep writeback disabled.
static inline void cgra_link_output(uintptr_t address, uint32_t word, uint32_t slot_stride, uint32_t channels, uint32_t row_stride) {
  *(volatile uint64_t *)(CGRA_LINK_CONTROL_BASE + CGRA_LINK_CONTROL_OUT_ADDRESS) = address;
  cgra_link_write(CGRA_LINK_CONTROL_OUT_WORD, word);
  cgra_link_write(CGRA_LINK_CONTROL_OUT_SLOT_STRIDE, slot_stride);
  cgra_link_write(CGRA_LINK_CONTROL_OUT_CHANNELS, channels);
  cgra_link_write(CGRA_LINK_CONTROL_OUT_ROW_STRIDE, row_stride);
  cgra_link_write(CGRA_LINK_CONTROL_OUT_ENABLE, 1);
}

static inline int cgra_link_begin(uint32_t job, uint32_t packet_count, uint32_t expected_completes, uint32_t symbol_count, uint32_t patch_count) {
  cgra_link_write(CGRA_LINK_CONTROL_JOB, job);
  cgra_link_write(CGRA_LINK_CONTROL_PACKET_COUNT, packet_count);
  cgra_link_write(CGRA_LINK_CONTROL_EXPECTED_COMPLETES, expected_completes);
  cgra_link_write(CGRA_LINK_CONTROL_SYMBOL_COUNT, symbol_count);
  cgra_link_write(CGRA_LINK_CONTROL_PATCH_COUNT, patch_count);
  cgra_link_write(CGRA_LINK_CONTROL_CONFIG_SUBMIT, 1);
  __asm__ volatile("" ::: "memory");
  while (cgra_link_read(CGRA_LINK_CONTROL_CONFIG_READY) == 0) {
  }
  return cgra_link_read(CGRA_LINK_CONTROL_CONFIG_STATUS) != AUTO_LINK_STATUS_SUCCESS;
}

static inline int cgra_link_configure_job(uint32_t job, uint32_t packet_count, uint32_t expected_completes) { return cgra_link_begin(job, packet_count, expected_completes, 0, 0); }

static inline int cgra_link_configure_template(uint32_t job, uint32_t packet_count, uint32_t expected_completes, const cgra_link_symbol_t *symbols, uint32_t symbol_count,
                                               const cgra_link_patch_t *patches, uint32_t patch_count) {
  if (cgra_link_begin(job, packet_count, expected_completes, symbol_count, patch_count) != 0) {
    return 1;
  }
  for (uint32_t index = 0; index < symbol_count; ++index) {
    cgra_link_write(CGRA_LINK_CONTROL_SYMBOL_BASE, symbols[index].base);
    cgra_link_write(CGRA_LINK_CONTROL_SYMBOL_STRIDE, symbols[index].stride);
    cgra_link_write(CGRA_LINK_CONTROL_SYMBOL_SOURCE, symbols[index].source);
    cgra_link_write(CGRA_LINK_CONTROL_SYMBOL_PUSH, 1);
  }
  for (uint32_t index = 0; index < patch_count; ++index) {
    // The generator emits only native data-payload relocations.
    cgra_link_write(CGRA_LINK_CONTROL_PATCH_PACKET, patches[index].packet_index);
    cgra_link_write(CGRA_LINK_CONTROL_PATCH_SYMBOL, patches[index].symbol_index);
    cgra_link_write(CGRA_LINK_CONTROL_PATCH_SCALE, patches[index].scale);
    cgra_link_write(CGRA_LINK_CONTROL_PATCH_OFFSET, patches[index].offset);
    cgra_link_write(CGRA_LINK_CONTROL_PATCH_PUSH, 1);
  }
  __asm__ volatile("" ::: "memory");
  return 0;
}

static inline int cgra_link_configure(uint32_t packet_count, uint32_t expected_completes) { return cgra_link_configure_job(0, packet_count, expected_completes); }

static inline int cgra_link_config_end(void) {
  // Capture acknowledgement must not wait for Rocket's RoCC busy fence.
  __asm__ volatile("" ::: "memory");
  while (cgra_link_read(CGRA_LINK_CONTROL_CONFIG_DONE) == 0) {
  }
  return cgra_link_read(CGRA_LINK_CONTROL_CONFIG_STATUS) != AUTO_LINK_STATUS_SUCCESS;
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
      .stage = cgra_link_read(CGRA_LINK_CONTROL_RESULT_STAGE),
      .job = cgra_link_read(CGRA_LINK_CONTROL_RESULT_JOB),
  };
}

#endif
