#ifndef BLOCK_CGRA_H
#define BLOCK_CGRA_H

#include "accel_generated.h"
#include "block_perf.h"
#include "cgra_dma.h"
#include "generated/cgra_block_activation_fast_api.h"
#include "generated/cgra_block_ln_mean_fast_api.h"
#include "generated/cgra_block_ln_norm_fast_api.h"
#include "generated/cgra_block_ln_sqrt_fast_api.h"
#include "generated/cgra_block_ln_squares_fast_api.h"
#include "generated/cgra_block_ln_sum_fast_api.h"
#include "generated/cgra_block_ln_variance_fast_api.h"
#include "generated/cgra_block_requant_fast_api.h"
#include "generated/cgra_block_residual_fast_api.h"

#include <stdio.h>
#include <string.h>

static int block_cgra_started;

// Every phase owns control memory from zero; only SPM survives a phase switch.
static int block_kernel(const cgra_kernel_t *program, const uint32_t *args, int repeat) {
  const unsigned long begin = block_clock();
  cgra_kernel_t kernel = *program;
  cgra_packet_t config[kernel.config_count];
  memcpy(config, kernel.config_packets, sizeof(config));
  for (unsigned index = 0; index < kernel.patch_count; ++index) {
    const cgra_patch_t *patch = &kernel.patches[index];
    cgra_packet_t *packet = &config[patch->packet_index];
    const uint32_t value = args[patch->symbol_index] * patch->scale + patch->offset;
    packet->hi = (packet->hi & ~BLOCK_REQUANT_STORE_DATA_PAYLOAD_HI(UINT32_MAX)) | BLOCK_REQUANT_STORE_DATA_PAYLOAD_HI(value);
  }
  kernel.config_packets = config;
  uint64_t ready = 0, status = 0, result = 0;
  unsigned long configured = 0, completed = 0;
  accel_commands(CGRA0, {
    if (repeat) {
      cgra_prepare(&kernel, CGRA_REPEAT);
    } else {
      cgra_config(&kernel, block_cgra_started ? CGRA_SWITCH : CGRA_COLD);
    }
    configured = block_clock();
    cgra_start(&kernel);
    CGRA_WAIT(ready);
    CGRA_STATUS(status);
    CGRA_RESULT(result);
    completed = block_done_clock(ready | status | result);
  });
  block_perf.config += configured - begin;
  block_perf.execute += completed - configured;
  ++block_perf.launches;
  block_cgra_started = 1;
  if (ready != 1 || (status & 1) != 1 || ((status >> 1) & 0xffff) != kernel.expected_completes || result != 0) {
    printf("Block CGRA ready=%lu status=%lu result=%lu\n", (unsigned long)ready, (unsigned long)status, (unsigned long)result);
    return 1;
  }
  return 0;
}

static cgra_dma_desc_t block_descriptor(unsigned address, unsigned count) {
  return ((uint64_t)address << CGRA_DMA_DESC_SPM_ADDR_LSB) | ((uint64_t)(count * sizeof(int32_t)) << CGRA_DMA_DESC_NBYTES_LSB);
}

static int block_load(const void *input, unsigned address, unsigned count, int packed) {
  const unsigned long begin = block_clock();
  uint8_t tag;
  cgra_dma_memory_fence();
  accel_commands(CGRA0, {
    if (packed) {
      cgra_dma_mvin_i8_async(input, block_descriptor(address, count));
    } else {
      cgra_dma_mvin_async(input, block_descriptor(address, count));
    }
    tag = cgra_dma_wait(0);
  });
  block_perf.load += block_done_clock(tag) - begin;
  const unsigned bytes = count * (packed ? sizeof(int8_t) : sizeof(int32_t));
  if (block_on_spm(input)) {
    block_perf.spm_read += bytes;
  } else {
    block_perf.dram_read += bytes;
  }
  return tag != 0;
}

static int block_store(int8_t *output, unsigned address, unsigned count) {
  const unsigned long begin = block_clock();
  uint8_t tag;
  accel_commands(CGRA0, {
    cgra_dma_mvout_i8_async(output, block_descriptor(address, count));
    tag = cgra_dma_wait(0);
  });
  cgra_dma_memory_fence();
  block_perf.store += block_clock() - begin;
  if (block_on_spm(output)) {
    block_perf.spm_write += count;
  } else {
    block_perf.dram_write += count;
  }
  return tag != 0;
}

static int block_quantize(const int32_t *input, int8_t *output, unsigned count, unsigned divisor) {
  for (unsigned start = 0; start < count; start += 1024) {
    const unsigned size = count - start < 1024 ? count - start : 1024;
    const uint32_t args[] = {0, 1024, divisor, size};
    if (block_load(input + start, 0, size, 0) || block_kernel(&BLOCK_REQUANT, args, start != 0) || block_store(output + start, 1024, size)) {
      return 1;
    }
  }
  return 0;
}

static int block_layernorm(const int8_t *input, const int32_t *gamma, const int32_t *beta, int8_t *output, unsigned rows, unsigned width) {
  enum { BATCH = 16 };
  const unsigned gamma_base = BATCH * width;
  const unsigned beta_base = gamma_base + width;
  const unsigned output_base = beta_base + width;
  const unsigned stats_base = output_base + BATCH * width;
  // At width 768, row buffers, affine parameters and statistics use 26,192 words.
  if (block_load(gamma, gamma_base, width, 0) || block_load(beta, beta_base, width, 0)) {
    return 1;
  }
  const unsigned mean = stats_base + BATCH, squares = mean + BATCH, variance = squares + BATCH, std = variance + BATCH;
  // Statistics are contiguous per field; each vector kernel restarts at a row boundary.
  const struct {
    const cgra_kernel_t *kernel;
    unsigned ii, count_arg, elements;
    uint32_t args[8], stride[8];
  } phases[] = {
      {&BLOCK_LN_SUM, BLOCK_LN_SUM_CTRL_COUNT_PER_ITER, 3, width, {0, stats_base, width}, {width, 1}},
      {&BLOCK_LN_MEAN, BLOCK_LN_MEAN_CTRL_COUNT_PER_ITER, 3, 1, {stats_base, mean, width}, {1, 1}},
      {&BLOCK_LN_SQUARES, BLOCK_LN_SQUARES_CTRL_COUNT_PER_ITER, 4, width, {0, mean, squares, width}, {width, 1, 1}},
      {&BLOCK_LN_VARIANCE, BLOCK_LN_VARIANCE_CTRL_COUNT_PER_ITER, 3, 1, {squares, variance, width}, {1, 1}},
      {&BLOCK_LN_SQRT, BLOCK_LN_SQRT_CTRL_COUNT_PER_ITER, 2, 1, {variance, std}, {1, 1}},
      {&BLOCK_LN_NORM, BLOCK_LN_NORM_CTRL_COUNT_PER_ITER, 7, width, {0, gamma_base, beta_base, output_base, mean, std, width}, {width, 0, 0, width, 1, 1}},
      {&BLOCK_REQUANT, BLOCK_REQUANT_CTRL_COUNT_PER_ITER, 3, width, {output_base, output_base, 4}, {width, width}},
  };
  for (unsigned start = 0; start < rows; start += BATCH) {
    const unsigned count = rows - start < BATCH ? rows - start : BATCH;
    if (block_load(input + start * width, 0, count * width, 1)) {
      return 1;
    }
    for (unsigned phase = 0; phase < sizeof(phases) / sizeof(phases[0]); ++phase) {
      const unsigned long begin = block_clock();
      // The Attention hardware has a 16-bit total-step counter, including idle slots.
      const unsigned limit = UINT16_MAX / phases[phase].ii / phases[phase].elements;
      for (unsigned row = 0; row < count; row += limit) {
        const unsigned size = count - row < limit ? count - row : limit;
        uint32_t args[8];
        for (unsigned index = 0; index < phases[phase].count_arg; ++index) {
          args[index] = phases[phase].args[index] + row * phases[phase].stride[index];
        }
        args[phases[phase].count_arg] = size * phases[phase].elements;
        if (block_kernel(phases[phase].kernel, args, row != 0)) {
          return 1;
        }
      }
      block_perf.ln[phase] += block_clock() - begin;
    }
    if (block_store(output + start * width, output_base, count * width)) {
      return 1;
    }
  }
  return 0;
}

static int block_activation(const int8_t *input, int8_t *output, unsigned count) {
  // Two 2048-word buffers fit the layout; 27 * 2048 fits the step counter.
  for (unsigned start = 0; start < count; start += 2048) {
    const unsigned size = count - start < 2048 ? count - start : 2048;
    const uint32_t args[] = {0, 2048, size};
    if (block_load(input + start, 0, size, 1) || block_kernel(&BLOCK_ACTIVATION, args, start != 0) || block_store(output + start, 2048, size)) {
      return 1;
    }
  }
  return 0;
}

static int block_add(const int8_t *left, const int8_t *right, int8_t *output, unsigned count) {
  for (unsigned start = 0; start < count; start += 1024) {
    const unsigned size = count - start < 1024 ? count - start : 1024;
    const uint32_t args[] = {0, 1024, 2048, size};
    if (block_load(left + start, 0, size, 1) || block_load(right + start, 1024, size, 1) || block_kernel(&BLOCK_RESIDUAL, args, start != 0) || block_store(output + start, 2048, size)) {
      return 1;
    }
  }
  return 0;
}

#endif
