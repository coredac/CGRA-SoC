#include "accel_generated.h"
#include "asic.h"
#include "cgra_dma.h"
#include "gemmini.h"
#include "gemmini_conv.h"
#include "onchip.h"
#include "region.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "/home/jjqin/CGRA-SoC/build/experiments/asic/system/cgra_add_relu_runtime_fast_api.h"
#include "/home/jjqin/CGRA-SoC/build/experiments/asic/system/cgra_relu_runtime_fast_api.h"
#ifndef RESNET_DATA
#define RESNET_DATA "/home/jjqin/CGRA-SoC/build/experiments/asic/system/resnet.h"
#endif
#include RESNET_DATA

#if DIM != 64
#error "The ResNet comparison requires DIM64 Gemmini"
#endif

enum { INPUT_WORDS = RESNET_INPUT_H * RESNET_INPUT_W * RESNET_INPUT_C, WORDS = RESNET_H * RESNET_W * RESNET_C, MAX_WORDS = 4096 };
// Reuse the first activation regions after the host has consumed their data.
enum { FIRST = 0, ACTIVE = MAX_WORDS, LEFT = FIRST, RIGHT = ACTIVE, SUM = 2 * MAX_WORDS, OUTPUT = 3 * MAX_WORDS };

typedef struct {
  const char *name;
  int split;
} backend_t;

typedef struct {
  unsigned submissions, host_reads, host_writes, peer_bytes, onchip_bytes, conversion_bytes;
  unsigned long add_cycles, peer_cycles, relu_cycles;
  uint64_t before[2][ASIC_COUNTERS], after[2][ASIC_COUNTERS];
} stats_t;

static const backend_t backends[] = {{"CGRA-fused", 0}, {"ASIC-split", 1}};
static cgra_packet_t relu_packets[RELU_RUNTIME_FAST_CONFIG_PACKET_COUNT];
static cgra_packet_t add_packets[ADD_RELU_RUNTIME_FAST_CONFIG_PACKET_COUNT];
static cgra_kernel_t kernels[2];
static int cached;
static int onchip;
static int last_job = -1;
static elem_t source[INPUT_WORDS] __attribute__((aligned(64)));
static elem_t first[WORDS] __attribute__((aligned(64)));
static elem_t activated[WORDS] __attribute__((aligned(64)));
static elem_t main_branch[WORDS] __attribute__((aligned(64)));
static elem_t skip_branch[WORDS] __attribute__((aligned(64)));
static int32_t transfer[MAX_WORDS] __attribute__((aligned(64)));
static int32_t output[MAX_WORDS] __attribute__((aligned(64)));

static unsigned long cycles(void) {
  unsigned long value;
  __asm__ volatile("rdcycle %0" : "=r"(value) : : "memory");
  return value;
}

static void snapshot(uint64_t counters[2][ASIC_COUNTERS]) {
  for (unsigned index = 0; index < ASIC_COUNTERS; ++index) {
    counters[0][index] = asic_counter(ADD0, index);
    counters[1][index] = asic_counter(RELU0, index);
  }
}

static uint64_t delta(const stats_t *stats, unsigned device, unsigned counter) { return stats->after[device][counter] - stats->before[device][counter]; }

static cgra_kernel_t resolve(const cgra_kernel_t *original, const uint32_t *args, cgra_packet_t *packets) {
  cgra_kernel_t kernel = *original;
  memcpy(packets, kernel.config_packets, kernel.config_count * sizeof(*packets));
  for (unsigned index = 0; index < kernel.patch_count; ++index) {
    const cgra_patch_t *patch = &kernel.patches[index];
    const uint32_t value = args[patch->symbol_index] * patch->scale + patch->offset;
    cgra_packet_t *packet = packets + patch->packet_index;
    packet->hi = (packet->hi & ~RELU_RUNTIME_STORE_DATA_PAYLOAD_HI(UINT32_MAX)) | RELU_RUNTIME_STORE_DATA_PAYLOAD_HI(value);
  }
  kernel.config_packets = packets;
  return kernel;
}

static int configure_jobs(const backend_t *backend, unsigned count, int full) {
  if (backend->split) {
    const asic_job_t add = {.unit = 1, .operation = 1, .count = count, .src0 = LEFT, .src1 = RIGHT, .dst = SUM};
    const asic_job_t final = {.unit = 1, .count = count, .src0 = SUM, .dst = OUTPUT};
    int status = asic_config(ADD0, 0, &add) || asic_config(RELU0, 1, &final);
    if (!status && full) {
      const asic_job_t relu = {.unit = 1, .count = count, .src0 = FIRST, .dst = ACTIVE};
      status = asic_config(RELU0, 0, &relu);
    }
    return status;
  }
  last_job = -1;
  const uint32_t relu_args[] = {FIRST, ACTIVE, count};
  const uint32_t add_args[] = {LEFT, RIGHT, OUTPUT, count};
  kernels[1] = resolve(&ADD_RELU_RUNTIME, add_args, add_packets);
  if (full)
    kernels[0] = resolve(&RELU_RUNTIME, relu_args, relu_packets);
  if (cached)
    return cache_config(kernels, full);
  uint64_t ready;
  accel_commands(CGRA0, {
    if (full)
      cgra_send_packets_fast(kernels[0].static_packets, kernels[0].static_count);
    cgra_send_packets_fast(kernels[1].static_packets, kernels[1].static_count);
    CGRA_WAIT(ready);
  });
  return ready != 1;
}

static int copy(const backend_t *backend, accel_t dedicated, unsigned word, int32_t *values, unsigned count, int store, int local, stats_t *stats) {
  int status;
  if (backend->split) {
    status = store ? asic_store(dedicated, values, word, count) : asic_load(dedicated, word, values, count);
  } else {
    const cgra_dma_desc_t descriptor = ((uint64_t)word << CGRA_DMA_DESC_SPM_ADDR_LSB) | ((uint64_t)count * sizeof(int32_t) << CGRA_DMA_DESC_NBYTES_LSB);
    uint8_t tag;
    cgra_dma_memory_fence();
    accel_commands(CGRA0, {
      if (store)
        cgra_dma_mvout_async(values, descriptor);
      else
        cgra_dma_mvin_async(values, descriptor);
      tag = cgra_dma_wait(0);
    });
    cgra_dma_memory_fence();
    status = tag != 0;
  }
  if (!status) {
    if (local)
      stats->onchip_bytes += count * sizeof(int32_t);
    else if (store)
      stats->host_writes += count * sizeof(int32_t);
    else
      stats->host_reads += count * sizeof(int32_t);
  }
  return status;
}

static int configure(const backend_t *backend, unsigned count, int full) {
  const int status = configure_jobs(backend, count, full);
  if (status)
    return status;
  region_config(backend->split, count, SUM);
  return 0;
}

static int execute(const backend_t *backend, unsigned region, stats_t *stats) {
  ++stats->submissions;
  return region_run(backend->split, region);
}

static int add_relu(const backend_t *backend, unsigned count, stats_t *stats) {
  const unsigned long begin = cycles();
  const int status = execute(backend, 1, stats);
  stats->add_cycles = cycles() - begin;
  if (backend->split && !status)
    stats->peer_bytes = count * sizeof(int32_t);
  return status;
}

static int convolution(const elem_t *input, const elem_t *weights, elem_t *result, unsigned rows, unsigned columns, unsigned channels, unsigned kernel, unsigned stride, unsigned padding) {
  unsigned tile_rows = RESNET_H;
  while (tile_rows && !gemmini_workspace_fits(gemmini_conv_workspace(tile_rows, RESNET_W, channels, RESNET_C, kernel, stride, 1)))
    --tile_rows;
  if (!tile_rows)
    return 1;
  asic_fence();
  for (unsigned row = 0; row < RESNET_H; row += tile_rows) {
    const unsigned take = RESNET_H - row < tile_rows ? RESNET_H - row : tile_rows;
    const int first_row = (int)(row * stride) - (int)padding;
    const unsigned begin = first_row > 0 ? first_row : 0;
    const unsigned limit = (row + take) * stride + kernel - 1 - padding;
    const unsigned end = limit < rows ? limit : rows;
    const gemmini_conv_t params = {.input_rows = end - begin,
                                   .input_columns = columns,
                                   .input_channels = channels,
                                   .output_rows = take,
                                   .output_columns = RESNET_W,
                                   .output_channels = RESNET_C,
                                   .kernel = kernel,
                                   .stride = stride,
                                   .dilation = 1,
                                   .padding_top = first_row < 0 ? -first_row : 0,
                                   .padding_left = padding,
                                   .input = input + begin * columns * channels,
                                   .weight = weights,
                                   .output = result + row * RESNET_W * RESNET_C,
                                   .activation = NO_ACTIVATION,
                                   .scale = ACC_SCALE_IDENTITY};
    if (!gemmini_conv_valid(&params, (end - begin) * columns * channels, kernel * kernel * channels * RESNET_C, take * RESNET_W * RESNET_C))
      return 1;
    accel_commands(GEMMINI0, {
      gemmini_conv_emit(&params);
      gemmini_fence();
    });
  }
  asic_fence();
  return 0;
}

static uintptr_t publication(unsigned slot) { return GEMMINI0.spm + GEMMINI0.spm_bytes / 2 + slot * MAX_WORDS; }

static int load8(const backend_t *backend, accel_t device, unsigned word, const elem_t *values, stats_t *stats) {
  if (onchip) {
    int32_t *wide = (int32_t *)publication(4);
    if (convert(values, wide, WORDS, 1))
      return 1;
    stats->conversion_bytes += WORDS * (sizeof(elem_t) + sizeof(acc_t));
    return copy(backend, device, word, wide, WORDS, 0, 1, stats);
  }
  for (unsigned index = 0; index < WORDS; ++index)
    transfer[index] = values[index];
  return copy(backend, device, word, transfer, WORDS, 0, 0, stats);
}

static int activation_output(const backend_t *backend, elem_t *destination, stats_t *stats) {
  int32_t *wide = onchip ? (int32_t *)publication(4) : transfer;
  const int status = copy(backend, RELU0, ACTIVE, wide, WORDS, 1, onchip, stats);
  if (status)
    return status;
  if (onchip) {
    const int result = convert(wide, destination, WORDS, 0);
    if (!result)
      stats->conversion_bytes += WORDS * (sizeof(elem_t) + sizeof(acc_t));
    return result;
  }
  for (unsigned index = 0; index < WORDS; ++index)
    destination[index] = (elem_t)wide[index];
  asic_fence();
  return 0;
}

static unsigned check(const int32_t *reference, unsigned count) {
  unsigned errors = 0;
  for (unsigned index = 0; index < count; ++index) {
    if (output[index] == reference[index])
      continue;
    if (errors < 4)
      printf("SPLIT mismatch index=%u actual=%d expected=%d\n", index, output[index], reference[index]);
    ++errors;
  }
  return errors;
}

static int counters(const backend_t *backend, unsigned count, int full, unsigned input, const stats_t *stats) {
  if (!backend->split)
    return 0;
  const uint64_t add_starts = delta(stats, 0, ASIC_CORE_STARTS);
  const uint64_t relu_starts = delta(stats, 1, ASIC_CORE_STARTS);
  const uint64_t add_reads = delta(stats, 0, ASIC_READ_BYTES);
  const uint64_t relu_reads = delta(stats, 1, ASIC_READ_BYTES);
  const uint64_t writes = delta(stats, 1, ASIC_WRITE_BYTES);
  printf("SPLIT counters n=%u input=%u add_active=%lu relu_active=%lu add_starts=%lu relu_starts=%lu add_cpu_starts=%lu relu_cpu_starts=%lu add_read=%lu relu_read=%lu relu_write=%lu add_config=%lu "
         "relu_config=%lu\n",
         count, input, (unsigned long)delta(stats, 0, ASIC_ACTIVE_CYCLES), (unsigned long)delta(stats, 1, ASIC_ACTIVE_CYCLES), (unsigned long)add_starts, (unsigned long)relu_starts,
         (unsigned long)delta(stats, 0, ASIC_CPU_STARTS), (unsigned long)delta(stats, 1, ASIC_CPU_STARTS), (unsigned long)add_reads, (unsigned long)relu_reads, (unsigned long)writes,
         (unsigned long)delta(stats, 0, ASIC_CONFIG_COMMANDS), (unsigned long)delta(stats, 1, ASIC_CONFIG_COMMANDS));
  return delta(stats, 0, ASIC_CPU_STARTS) || delta(stats, 1, ASIC_CPU_STARTS) || add_starts != 1 || relu_starts != (unsigned)(full + 1) || add_reads != count * 8u ||
         relu_reads != count * 4u * (full + 1) || writes != count * 4u * (full + 1) || delta(stats, 0, ASIC_WRITE_BYTES) != 0;
}

static int resnet(const backend_t *backend, unsigned sample) {
  const unsigned input = sample % 2;
  stats_t stats = {0};
  elem_t *first_result = onchip ? (elem_t *)publication(0) : first;
  elem_t *active_result = onchip ? (elem_t *)publication(1) : activated;
  elem_t *main_result = onchip ? (elem_t *)publication(2) : main_branch;
  elem_t *skip_result = onchip ? (elem_t *)publication(3) : skip_branch;
  snapshot(stats.before);
  asic_fence();
  const unsigned long begin = cycles();
  memcpy(source, resnet_inputs[input], sizeof(source));
  asic_fence();
  const unsigned long staged = cycles();
  int status = 0;
  if (!sample) {
    accel_commands(GEMMINI0, {
      gemmini_flush(0);
      gemmini_fence();
    });
    status = configure(backend, WORDS, 1);
  }
  const unsigned long configured = cycles();
  if (!status)
    status = convolution(source, resnet_weights1, first_result, RESNET_INPUT_H, RESNET_INPUT_W, RESNET_INPUT_C, 3, 2, 1);
  const unsigned long first_done = cycles();
  if (!status)
    status = load8(backend, RELU0, FIRST, first_result, &stats);
  if (!status)
    status = execute(backend, 0, &stats);
  if (!status)
    status = activation_output(backend, active_result, &stats);
  const unsigned long activation_done = cycles();
  if (!status)
    status = convolution(active_result, resnet_weights2, main_result, RESNET_H, RESNET_W, RESNET_C, 3, 1, 1);
  if (!status)
    status = convolution(source, resnet_projection, skip_result, RESNET_INPUT_H, RESNET_INPUT_W, RESNET_INPUT_C, 1, 2, 0);
  const unsigned long branches_done = cycles();
  if (!status)
    status = load8(backend, ADD0, LEFT, main_result, &stats) || load8(backend, ADD0, RIGHT, skip_result, &stats);
  const unsigned long loaded = cycles();
  if (!status)
    status = add_relu(backend, WORDS, &stats);
  const unsigned long computed = cycles();
  if (!status)
    status = copy(backend, RELU0, OUTPUT, output, WORDS, 1, 0, &stats);
  asic_fence();
  const unsigned long finished = cycles();
  snapshot(stats.after);
  const unsigned errors = status ? 0 : check(resnet_expected[input], WORDS);
  const int counter_errors = status ? 0 : counters(backend, WORDS, 1, input, &stats) || stats.submissions != 2;
  printf("SPLIT resnet backend=%s sample=%u input=%u phase=%s cycles=%lu staging=%lu setup=%lu conv1=%lu activation=%lu branches=%lu branch_load=%lu pair=%lu output=%lu add=%lu peer=%lu relu=%lu "
         "submissions=%u "
         "host_read=%u host_write=%u peer_bytes=%u onchip_bytes=%u conversion_bytes=%u checked=%u errors=%u counter_errors=%d status=%d\n",
         backend->name, sample, input, sample ? "warm" : "cold", finished - begin, staged - begin, configured - staged, first_done - configured, activation_done - first_done,
         branches_done - activation_done, loaded - branches_done, computed - loaded, finished - computed, stats.add_cycles, stats.peer_cycles, stats.relu_cycles, stats.submissions, stats.host_reads,
         stats.host_writes, stats.peer_bytes, stats.onchip_bytes, stats.conversion_bytes, status ? 0 : WORDS, errors, counter_errors, status);
  return status || errors || counter_errors;
}

int main(int argc, char **argv) {
  const int selected = argc > 1 ? atoi(argv[1]) : 0;
  if (selected < 0 || selected > 1 || WORDS > MAX_WORDS)
    return 1;
  cached = 1;
  onchip = 1;
  printf("SPLIT control=regional-Hybrid intermediate=SPM ASIC=independent-Add-ReLU aggregate_spm=131072 CGRA=resident-fused-AddReLU\n");
  printf("SPLIT shape input=%ux%ux%u output=%ux%ux%u words=%u\n", RESNET_INPUT_H, RESNET_INPUT_W, RESNET_INPUT_C, RESNET_H, RESNET_W, RESNET_C, WORDS);
  for (unsigned sample = 0; sample < 4; ++sample) {
    if (resnet(backends + selected, sample)) {
      printf("SPLIT: FAIL\n");
      return 1;
    }
  }
  printf("SPLIT: PASS\n");
  return 0;
}
