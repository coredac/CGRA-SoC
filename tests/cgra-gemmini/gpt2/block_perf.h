#ifndef BLOCK_PERF_H
#define BLOCK_PERF_H

#include "accel_generated.h"

#include <stdio.h>
#include <string.h>

static struct {
  unsigned long config, execute, load, store;
  unsigned long ln[7];
  unsigned long dram_read, dram_write, spm_read, spm_write;
  unsigned launches;
} block_perf;

static int block_on_spm(const void *pointer) {
  const uintptr_t address = (uintptr_t)pointer;
  return (address >= GEMMINI0.spm && address < GEMMINI0.spm + GEMMINI0.spm_bytes) || (address >= GEMMINI1.spm && address < GEMMINI1.spm + GEMMINI1.spm_bytes);
}

static unsigned long block_clock(void) {
  unsigned long value;
  __asm__ volatile("rdcycle %0" : "=r"(value)::"memory");
  return value;
}

// An independent CSR read can pass an outstanding RoCC response on Rocket.
// Consume the response register before taking the completion timestamp.
static unsigned long block_done_clock(unsigned long response) {
  unsigned long value;
  __asm__ volatile("add zero, %1, zero\n\trdcycle %0" : "=r"(value) : "r"(response) : "memory");
  return value;
}

static void block_perf_reset(void) { memset(&block_perf, 0, sizeof(block_perf)); }

// These counters cover block_cgra helpers, not Gemmini or Attention scheduling.
// Config includes CPU patching/submission; execute can include queued config drain.
// CPU cycle intervals include command submission and completion waits, not just FU work.
// LayerNorm phase totals overlap config/execute totals and must not be added to them.
static void block_perf_print(void) {
  static const char *names[] = {"sum", "mean", "squares", "variance", "sqrt", "norm", "requant"};
  printf("Block CGRA helper cycles config=%lu execute_wait=%lu dma_in=%lu dma_out=%lu launches=%u\n", block_perf.config, block_perf.execute, block_perf.load, block_perf.store, block_perf.launches);
  printf("Block CGRA DMA bytes dram_read=%lu dram_write=%lu spm_read=%lu spm_write=%lu (includes affine parameters)\n", block_perf.dram_read, block_perf.dram_write, block_perf.spm_read,
         block_perf.spm_write);
  for (unsigned phase = 0; phase < 7; ++phase) {
    printf("Block LN phase=%s cycles=%lu\n", names[phase], block_perf.ln[phase]);
  }
}

#endif
