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

static inline int cgra_link_begin(uint32_t job, uint32_t packet_count, uint32_t expected_completes, uint32_t patch_count) {
  cgra_link_write(CGRA_LINK_CONTROL_JOB, job);
  cgra_link_write(CGRA_LINK_CONTROL_PACKET_COUNT, packet_count);
  cgra_link_write(CGRA_LINK_CONTROL_EXPECTED_COMPLETES, expected_completes);
  cgra_link_write(CGRA_LINK_CONTROL_PATCH_COUNT, patch_count);
  cgra_link_write(CGRA_LINK_CONTROL_CONFIG_SUBMIT, 1);
  __asm__ volatile("" ::: "memory");
  while (cgra_link_read(CGRA_LINK_CONTROL_CONFIG_READY) == 0) {
  }
  return cgra_link_read(CGRA_LINK_CONTROL_CONFIG_STATUS) != AUTO_LINK_STATUS_SUCCESS;
}

static inline int cgra_link_configure_job(uint32_t job, uint32_t packet_count, uint32_t expected_completes) { return cgra_link_begin(job, packet_count, expected_completes, 0); }

static inline int cgra_link_configure_template(uint32_t job, uint32_t packet_count, uint32_t expected_completes, const cgra_link_symbol_t *symbols, uint32_t symbol_count,
                                               const cgra_link_patch_t *patches, uint32_t patch_count) {
  (void)symbol_count;
  if (cgra_link_begin(job, packet_count, expected_completes, patch_count) != 0) {
    return 1;
  }
  for (uint32_t index = 0; index < patch_count; ++index) {
    const cgra_link_patch_t *patch = &patches[index];
    const cgra_link_symbol_t *symbol = &symbols[patch->symbol_index];
    const int elements = symbol->source == CGRA_LINK_ELEMENTS;
    // Fold the payload affine expression with native uint32_t wraparound.
    const uint32_t coefficient = elements ? patch->scale : symbol->stride * patch->scale;
    const uint32_t bias = elements ? patch->offset : symbol->base * patch->scale + patch->offset;
    cgra_link_write(CGRA_LINK_CONTROL_PATCH_PACKET, patch->packet_index);
    cgra_link_write(CGRA_LINK_CONTROL_PATCH_SOURCE, symbol->source);
    cgra_link_write(CGRA_LINK_CONTROL_PATCH_COEFFICIENT, coefficient);
    cgra_link_write(CGRA_LINK_CONTROL_PATCH_BIAS, bias);
    cgra_link_write(CGRA_LINK_CONTROL_PATCH_PUSH, 1);
  }
  __asm__ volatile("" ::: "memory");
  return 0;
}

static inline int cgra_link_configure_resident(uint32_t job, uint32_t packet_count, uint32_t expected_completes, const cgra_link_symbol_t *symbols, uint32_t symbol_count,
                                               const cgra_link_patch_t *patches, uint32_t patch_count, const uint32_t *repeats, uint32_t repeat_count) {
  cgra_link_write(CGRA_LINK_CONTROL_REPEAT_COUNT, repeat_count);
  if (cgra_link_configure_template(job, packet_count, expected_completes, symbols, symbol_count, patches, patch_count) != 0) {
    return 1;
  }
  for (uint32_t index = 0; index < repeat_count; ++index) {
    cgra_link_write(CGRA_LINK_CONTROL_REPEAT_PACKET, repeats[index]);
    cgra_link_write(CGRA_LINK_CONTROL_REPEAT_PUSH, 1);
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
