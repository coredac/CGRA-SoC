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

static inline volatile uint32_t *cgra_link_reg(uintptr_t offset) { return (volatile uint32_t *)(CGRA_LINK_CONTROL_BASE + offset); }

static inline uint32_t cgra_link_read(uintptr_t offset) { return *cgra_link_reg(offset); }

static inline void cgra_link_write(uintptr_t offset, uint32_t value) { *cgra_link_reg(offset) = value; }

static inline int cgra_link_begin(uint32_t job, uint32_t packet_count, uint32_t expected_completes, uint32_t patch_count, uint32_t rearm_count, uint32_t setup_count) {
  uint64_t ready = 0;
  // Drain native static configuration before subsequent RoCC packets enter capture.
  CGRA_WAIT(ready);
  (void)ready;
  cgra_link_write(CGRA_LINK_CONTROL_JOB, job);
  cgra_link_write(CGRA_LINK_CONTROL_PACKET_COUNT, packet_count);
  cgra_link_write(CGRA_LINK_CONTROL_EXPECTED_COMPLETES, expected_completes);
  cgra_link_write(CGRA_LINK_CONTROL_PATCH_COUNT, patch_count);
  cgra_link_write(CGRA_LINK_CONTROL_REARM_COUNT, rearm_count);
  cgra_link_write(CGRA_LINK_CONTROL_SETUP_COUNT, setup_count);
  cgra_link_write(CGRA_LINK_CONTROL_CONFIG_SUBMIT, 1);
  __asm__ volatile("" ::: "memory");
  while (cgra_link_read(CGRA_LINK_CONTROL_CONFIG_READY) == 0) {
  }
  return cgra_link_read(CGRA_LINK_CONTROL_CONFIG_STATUS) != AUTO_LINK_STATUS_SUCCESS;
}

static inline void cgra_link_patches(const cgra_kernel_t *kernel, const cgra_link_symbol_t *symbols, uint32_t count) {
  for (uint32_t index = 0; index < count; ++index) {
    const cgra_patch_t *patch = &kernel->patches[index];
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
}

static inline void cgra_link_queue(cgra_packet_t packet) {
  CGRA_RAW_PKT_LO(packet.lo);
  CGRA_RAW_PKT_MID(packet.mid);
  CGRA_SPM_PKT_HI(packet.hi);
#if CGRA_INTRA_PKT_NBITS > 192
  CGRA_SPM_PKT_TOP(packet.top);
#endif
}

// Configure before releasing dependencies; AutoLink later starts the captured job.
static inline int cgra_job_config(uint32_t job, const cgra_kernel_t *kernel, const cgra_link_symbol_t *bindings) {
  cgra_send_packets_fast(kernel->static_packets, kernel->static_count);
  const uint32_t patch_count = bindings == NULL ? 0 : kernel->patch_count;
  if (cgra_link_begin(job, kernel->config_count + kernel->launch_count, kernel->expected_completes, patch_count, kernel->rearm_count, kernel->setup_count) != 0) {
    return 1;
  }
  cgra_link_patches(kernel, bindings, patch_count);
  for (uint32_t index = 0; index < kernel->config_count; ++index) {
    cgra_link_queue(kernel->config_packets[index]);
  }
  for (uint32_t index = 0; index < kernel->launch_count; ++index) {
    cgra_link_queue(kernel->launch_packets[index]);
  }
  return 0;
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
