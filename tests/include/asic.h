#ifndef ASIC_H
#define ASIC_H

#include "accel.h"

typedef struct {
  uint32_t unit, operation, fused, phase;
  uint32_t count, rows, edges, divisor;
  uint32_t src0, src1, src2, src3, dst, next_job;
} asic_job_t;

static inline void asic_fence(void) { __asm__ volatile("fence rw, rw" ::: "memory"); }

enum { ASIC_ACTIVE_CYCLES, ASIC_CORE_STARTS, ASIC_READ_BYTES, ASIC_WRITE_BYTES, ASIC_CPU_STARTS, ASIC_CONFIG_COMMANDS, ASIC_COUNTERS };

static inline uint64_t asic_counter(accel_t device, unsigned selector) {
  uint64_t value;
  accel_commands(device, {
    register uint64_t result asm("x10");
    register uint64_t counter asm("x11") = selector;
    __asm__ volatile(".word " STR(CUSTOMX(0, 1, 1, 0, 10, 11, 0, 11)) : "=r"(result) : "r"(counter) : "memory");
    value = result;
  });
  return value;
}

static inline int asic_wait(accel_t device) {
  uint64_t status;
  accel_commands(device, { ROCC_INSTRUCTION_D(0, status, 7); });
  return (int)status;
}

static inline int asic_config(accel_t device, unsigned job, const asic_job_t *config) {
  const uint64_t shape = config->unit | (config->operation << 3) | (config->fused << 5) | (config->phase << 6) | ((uint64_t)config->count << 16);
  const uint64_t geometry = config->rows | ((uint64_t)config->edges << 16);
  accel_commands(device, {
    ROCC_INSTRUCTION_S(0, job, 0);
    ROCC_INSTRUCTION_SS(0, shape, geometry, 1);
    ROCC_INSTRUCTION_SS(0, config->src0, config->src1, 2);
    ROCC_INSTRUCTION_SS(0, config->src2, config->src3, 3);
    ROCC_INSTRUCTION_SS(0, config->dst, config->divisor, 4);
    ROCC_INSTRUCTION_S(0, config->next_job, 5);
  });
  return asic_wait(device);
}

static inline int asic_start(accel_t device, unsigned job) {
  accel_commands(device, { ROCC_INSTRUCTION_S(0, job, 6); });
  return 0;
}

static inline int asic_load(accel_t device, unsigned local_word, const void *source, unsigned words) {
  asic_fence();
  accel_commands(device, {
    ROCC_INSTRUCTION_S(0, (uintptr_t)source, 8);
    ROCC_INSTRUCTION_SS(0, local_word, (uint64_t)words * sizeof(uint32_t), 9);
  });
  return asic_wait(device);
}

static inline int asic_store(accel_t device, void *destination, unsigned local_word, unsigned words) {
  accel_commands(device, {
    ROCC_INSTRUCTION_S(0, (uintptr_t)destination, 8);
    ROCC_INSTRUCTION_SS(0, local_word, (uint64_t)words * sizeof(uint32_t), 10);
  });
  const int status = asic_wait(device);
  asic_fence();
  return status;
}

#endif
