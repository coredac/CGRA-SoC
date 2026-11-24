#include "/home/jjqin/CGRA-SoC/build/experiments/asic/preflight/gpt-lanes/include/cgra_lanes_requant127_fast_api.h"
#include "/home/jjqin/CGRA-SoC/build/experiments/asic/preflight/gpt-lanes/include/cgra_lanes_requant128_fast_api.h"
#include "/home/jjqin/CGRA-SoC/build/experiments/asic/preflight/gpt-lanes/include/cgra_lanes_requant256_fast_api.h"
#include "/home/jjqin/CGRA-SoC/build/experiments/asic/preflight/gpt-lanes/include/cgra_lanes_requant4_fast_api.h"
#include "/home/jjqin/CGRA-SoC/build/experiments/asic/preflight/gpt-lanes/include/cgra_lanes_requant64_fast_api.h"
#include "/home/jjqin/CGRA-SoC/build/experiments/asic/preflight/gpt-lanes/include/cgra_lanes_residual_fast_api.h"
#include "/home/jjqin/CGRA-SoC/build/experiments/asic/preflight/gpt-schedule/include/cgra_block_requant4_fast_api.h"
#include "/home/jjqin/CGRA-SoC/build/experiments/asic/preflight/gpt-schedule/include/cgra_block_requant64_fast_api.h"
#include "/home/jjqin/CGRA-SoC/build/experiments/asic/preflight/gpt-schedule/include/cgra_block_requant128_fast_api.h"
#include "/home/jjqin/CGRA-SoC/build/experiments/asic/preflight/gpt-schedule/include/cgra_block_requant256_fast_api.h"
#include "accel_generated.h"
#include "asic.h"
#include "auto_link.h"
#include "cgra_dma.h"
#include "cgra_link.h"
#include "gemmini.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef ASIC_GPT_DATA
#define ASIC_GPT_DATA "/home/jjqin/CGRA-SoC/build/experiments/asic/system/gpt.h"
#endif
#include ASIC_GPT_DATA
#include "/home/jjqin/CGRA-SoC/build/experiments/asic/system/cgra_attention_exp_fast_api.h"
#include "/home/jjqin/CGRA-SoC/build/experiments/asic/system/cgra_attention_max_fast_api.h"
#include "/home/jjqin/CGRA-SoC/build/experiments/asic/system/cgra_attention_norm_fast_api.h"
#include "/home/jjqin/CGRA-SoC/build/experiments/asic/preflight/gpt-schedule/include/cgra_block_activation_fast_api.h"
#include "/home/jjqin/CGRA-SoC/build/experiments/asic/preflight/gpt-schedule/include/cgra_block_ln_mean_fast_api.h"
#include "/home/jjqin/CGRA-SoC/build/experiments/asic/preflight/gpt-schedule/include/cgra_block_ln_norm_fast_api.h"
#include "/home/jjqin/CGRA-SoC/build/experiments/asic/preflight/gpt-schedule/include/cgra_block_ln_sqrt_fast_api.h"
#include "/home/jjqin/CGRA-SoC/build/experiments/asic/preflight/gpt-schedule/include/cgra_block_ln_squares_fast_api.h"
#include "/home/jjqin/CGRA-SoC/build/experiments/asic/preflight/gpt-schedule/include/cgra_block_ln_sum_fast_api.h"
#include "/home/jjqin/CGRA-SoC/build/experiments/asic/preflight/gpt-schedule/include/cgra_block_ln_variance_fast_api.h"
#include "/home/jjqin/CGRA-SoC/build/experiments/asic/system/cgra_block_requant_fast_api.h"
#include "/home/jjqin/CGRA-SoC/build/experiments/asic/system/cgra_block_residual_fast_api.h"

#if DIM != 64
#error "This GPT system fixture requires Gemmini DIM=64"
#endif

enum { JOB_LN = 0, JOB_SOFTMAX = 7, JOB_QKV = 13, JOB_PV = 14, JOB_ATTENTION = 15, JOB_FC1 = 16, JOB_FC2 = 17, JOB_GELU = 18, JOB_RESIDUAL = 19, JOBS = 20 };
enum { INPUT = 0, SECOND = 1024, THIRD = 2048, OUTPUT = 3072 };
static int32_t transfer[1024] __attribute__((aligned(64)));
static int32_t result[1024] __attribute__((aligned(64)));
static acc_t accum[1024] __attribute__((aligned(64)));
static elem_t normalized[256] __attribute__((aligned(64)));
static elem_t query[256] __attribute__((aligned(64)));
static elem_t key[256] __attribute__((aligned(64)));
static elem_t value[256] __attribute__((aligned(64)));
static elem_t concat[256] __attribute__((aligned(64)));
static elem_t branch[256] __attribute__((aligned(64)));
static elem_t residual[256] __attribute__((aligned(64)));
static elem_t expanded[1024] __attribute__((aligned(64)));
static elem_t final[256] __attribute__((aligned(64)));
static elem_t head_q[128] __attribute__((aligned(64)));
static elem_t head_k[128] __attribute__((aligned(64)));
static elem_t head_v[128] __attribute__((aligned(64)));
static elem_t head_out[128] __attribute__((aligned(64)));
static elem_t probability[4] __attribute__((aligned(64)));
static acc_t scores[8] __attribute__((aligned(64)));

static int dedicated;

static struct {
  unsigned submissions, configured, input_bytes, output_bytes, gemms;
  int chained;
} stats;

static unsigned long cycles(void) {
  unsigned long value;
  __asm__ volatile("rdcycle %0" : "=r"(value) : : "memory");
  return value;
}

static unsigned padded(unsigned count) { return (count + 7u) / 8u * 8u; }

static void snapshot(uint64_t *counters) {
  for (unsigned index = 0; index < ASIC_COUNTERS; ++index)
    counters[index] = asic_counter(ASIC0, index);
}

static int configure_asic(int chained) {
  asic_job_t jobs[JOBS] = {0};
  for (unsigned phase = 0; phase < 7; ++phase) {
    jobs[JOB_LN + phase] = (asic_job_t){.unit = 2, .phase = phase, .count = 128, .src0 = INPUT, .src1 = SECOND, .src2 = THIRD, .dst = OUTPUT};
    jobs[JOB_LN + phase].next_job = chained && phase < 6 ? JOB_LN + phase + 2 : 0;
  }
  for (unsigned row = 0; row < 2; ++row) {
    for (unsigned phase = 0; phase < 3; ++phase) {
      const unsigned index = JOB_SOFTMAX + row * 3 + phase;
      jobs[index] = (asic_job_t){.unit = 3, .phase = phase, .count = 2, .rows = row, .src0 = INPUT, .dst = OUTPUT};
      jobs[index].next_job = chained && phase < 2 ? index + 2 : 0;
    }
  }
  const unsigned counts[] = {256, 128, 256, 1024, 256};
  const unsigned divisors[] = {BLOCK_Q_DIV, BLOCK_PV_DIV, BLOCK_ATTENTION_DIV, BLOCK_FC1_DIV, BLOCK_FC2_DIV};
  for (unsigned index = JOB_QKV; index <= JOB_FC2; ++index) {
    jobs[index] = (asic_job_t){.unit = 4, .operation = 1, .count = counts[index - JOB_QKV], .divisor = divisors[index - JOB_QKV], .src0 = INPUT, .dst = OUTPUT};
  }
  jobs[JOB_FC1].next_job = chained ? JOB_GELU + 1 : 0;
  jobs[JOB_GELU] = (asic_job_t){.unit = 4, .operation = 0, .count = 1024, .src0 = OUTPUT, .dst = OUTPUT};
  jobs[JOB_RESIDUAL] = (asic_job_t){.unit = 4, .operation = 2, .count = 256, .src0 = INPUT, .src1 = SECOND, .dst = OUTPUT};
  for (unsigned index = 0; index < JOBS; ++index) {
    const int status = asic_config(ASIC0, index, jobs + index);
    if (status)
      return status;
    ++stats.configured;
  }
  return 0;
}

static int configure_cgra(int chained) {
  enum { TOTAL = 4096, MEAN = 4112, SQUARES = 4128, VARIANCE = 4144, STD = 4160, EXP = 5120, SUM = 6144 };
  struct {
    const cgra_kernel_t *kernel;
    uint32_t args[8];
    unsigned next;
  } programs[JOBS] = {
      [JOB_LN] = {&BLOCK_LN_SUM, {INPUT, TOTAL, 128, 128}},
      [JOB_LN + 1] = {&BLOCK_LN_MEAN, {TOTAL, MEAN, 1}},
      [JOB_LN + 2] = {&BLOCK_LN_SQUARES, {INPUT, MEAN, SQUARES, 128, 128}},
      [JOB_LN + 3] = {&BLOCK_LN_VARIANCE, {SQUARES, VARIANCE, 1}},
      [JOB_LN + 4] = {&BLOCK_LN_SQRT, {VARIANCE, STD, 1}},
      [JOB_LN + 5] = {&BLOCK_LN_NORM, {INPUT, SECOND, THIRD, OUTPUT, MEAN, STD, 128, 128}},
      [JOB_LN + 6] = {&LANES_REQUANT4, {OUTPUT, OUTPUT, 64, OUTPUT + 64, OUTPUT + 64}},
      [JOB_SOFTMAX] = {&ATTENTION_MAX, {INPUT, TOTAL, 2, 0, 2}},
      [JOB_SOFTMAX + 1] = {&ATTENTION_EXP, {INPUT, TOTAL, EXP, SUM, 2, 0, 2}},
      [JOB_SOFTMAX + 2] = {&ATTENTION_NORM, {EXP, SUM, OUTPUT, 2, 2}},
      [JOB_SOFTMAX + 3] = {&ATTENTION_MAX, {INPUT, TOTAL, 2, 1, 2}},
      [JOB_SOFTMAX + 4] = {&ATTENTION_EXP, {INPUT, TOTAL, EXP, SUM, 2, 1, 2}},
      [JOB_SOFTMAX + 5] = {&ATTENTION_NORM, {EXP, SUM, OUTPUT, 2, 2}},
      [JOB_QKV] = {&LANES_REQUANT256, {INPUT, OUTPUT, 128, INPUT + 128, OUTPUT + 128}},
      [JOB_PV] = {&LANES_REQUANT127, {INPUT, OUTPUT, BLOCK_PV_DIV, 64, INPUT + 64, OUTPUT + 64}},
      [JOB_ATTENTION] = {&LANES_REQUANT64, {INPUT, OUTPUT, 128, INPUT + 128, OUTPUT + 128}},
      [JOB_FC1] = {&LANES_REQUANT128, {INPUT, OUTPUT, 512, INPUT + 512, OUTPUT + 512}},
      [JOB_FC2] = {&LANES_REQUANT128, {INPUT, OUTPUT, 128, INPUT + 128, OUTPUT + 128}},
      [JOB_GELU] = {&BLOCK_ACTIVATION, {OUTPUT, OUTPUT, 1024}},
      [JOB_RESIDUAL] = {&LANES_RESIDUAL, {INPUT, SECOND, OUTPUT, 128, INPUT + 128, SECOND + 128, OUTPUT + 128}},
  };
  if (chained) {
    for (unsigned phase = 0; phase < 6; ++phase)
      programs[JOB_LN + phase].next = JOB_LN + phase + 2;
    for (unsigned row = 0; row < 2; ++row)
      for (unsigned phase = 0; phase < 2; ++phase)
        programs[JOB_SOFTMAX + row * 3 + phase].next = JOB_SOFTMAX + row * 3 + phase + 2;
    programs[JOB_FC1].next = JOB_GELU + 1;
  }
  for (unsigned job = 0; job < JOBS; ++job) {
    cgra_kernel_t kernel = *programs[job].kernel;
    cgra_packet_t config[kernel.config_count];
    memcpy(config, kernel.config_packets, sizeof(config));
    for (unsigned index = 0; index < kernel.patch_count; ++index) {
      const cgra_patch_t *patch = &kernel.patches[index];
      const uint32_t value = programs[job].args[patch->symbol_index] * patch->scale + patch->offset;
      cgra_packet_t *packet = &config[patch->packet_index];
      packet->hi = (packet->hi & ~BLOCK_REQUANT_STORE_DATA_PAYLOAD_HI(UINT32_MAX)) | BLOCK_REQUANT_STORE_DATA_PAYLOAD_HI(value);
    }
    kernel.config_packets = config;
    kernel.patch_count = 0;
    int status;
    accel_commands(CGRA0, { status = cgra_job_full_at(CGRA0.control, job, &kernel, NULL, programs[job].next); });
    if (status)
      return status;
    ++stats.configured;
  }
  return 0;
}

static int configure(int chained) {
  const int status = dedicated ? configure_asic(chained) : configure_cgra(chained);
  if (status)
    return status;
  const auto_link_stage_t stage = {.endpoint = dedicated ? ASIC0.id : CGRA0.id, .job = 0, .output = {.address = {.binding = -1}}};
  const auto_link_edge_t edge = {.destination = 0, .flags = 3, .address = {.binding = -1}};
  const auto_link_graph_t graph = {
      .stage_count = 1, .edge_count = 1, .stage_capacity = AUTO_LINK_GRAPH.stage_capacity, .edge_capacity = AUTO_LINK_GRAPH.edge_capacity, .stages = &stage, .edges = &edge};
  auto_link_load(&graph, NULL);
  auto_link_tiles(1, 1, 1, 1);
  return 0;
}

static int transfer_words(unsigned address, void *memory, unsigned words, int write) {
  if (dedicated)
    return write ? asic_store(ASIC0, memory, address, words) : asic_load(ASIC0, address, memory, words);
  const cgra_dma_desc_t descriptor = ((uint64_t)address << CGRA_DMA_DESC_SPM_ADDR_LSB) | ((uint64_t)words * sizeof(int32_t) << CGRA_DMA_DESC_NBYTES_LSB);
  uint8_t tag;
  cgra_dma_memory_fence();
  accel_commands(CGRA0, {
    if (write)
      cgra_dma_mvout_async(memory, descriptor);
    else
      cgra_dma_mvin_async(memory, descriptor);
    tag = cgra_dma_wait(0);
  });
  cgra_dma_memory_fence();
  return tag != 0;
}

static int load32(unsigned address, const int32_t *source, unsigned count) {
  const unsigned words = padded(count);
  memcpy(transfer, source, count * sizeof(*transfer));
  memset(transfer + count, 0, (words - count) * sizeof(*transfer));
  stats.input_bytes += words * 4;
  return transfer_words(address, transfer, words, 0);
}

static int load8(unsigned address, const elem_t *source, unsigned count) {
  const unsigned words = padded(count);
  for (unsigned index = 0; index < count; ++index)
    transfer[index] = source[index];
  memset(transfer + count, 0, (words - count) * sizeof(*transfer));
  stats.input_bytes += words * 4;
  return transfer_words(address, transfer, words, 0);
}

static int store8(elem_t *destination, unsigned count) {
  const unsigned words = padded(count);
  const int status = transfer_words(OUTPUT, result, words, 1);
  if (status)
    return status;
  stats.output_bytes += words * 4;
  for (unsigned index = 0; index < count; ++index)
    destination[index] = (elem_t)result[index];
  return 0;
}

static int drain(unsigned job) {
  unsigned reports = 0;
  int failed = 0;
  for (;;) {
    const unsigned running = *(volatile uint32_t *)(AUTO_LINK_BASE + AUTO_LINK_RUNNING);
    if (!cgra_link_read_at(CGRA0.control, CGRA_LINK_CONTROL_RESULT_VALID)) {
      if (!running)
        break;
      continue;
    }
    const cgra_link_result_t result = cgra_link_wait_at(CGRA0.control);
    if (result.status || result.detail || result.data || result.stage || result.job != job) {
      printf("GPT COMPARE report job=%u status=%u detail=%u data=%u\n", result.job, result.status, result.detail, result.data);
      failed = 1;
    }
    ++reports;
  }
  return failed || reports != 1;
}

static int execute(unsigned first, unsigned count) {
  for (unsigned index = 0; index < (stats.chained ? 1u : count); ++index) {
    auto_link_job(0, first + index);
    auto_link_input_ready();
    ++stats.submissions;
    if (drain(first + index))
      return 1;
  }
  return 0;
}

static void matmul(const elem_t *a, const elem_t *b, const acc_t *bias, acc_t *out, unsigned rows, unsigned columns, unsigned inner, int transpose_b) {
  asic_fence();
  accel_commands(GEMMINI0, {
    tiled_matmul_auto(rows, columns, inner, a, b, bias, out, inner, transpose_b ? inner : columns, columns, columns, MVIN_SCALE_IDENTITY, MVIN_SCALE_IDENTITY, MVIN_SCALE_IDENTITY, NO_ACTIVATION,
                      ACC_SCALE_IDENTITY, 0, true, false, transpose_b, true, false, 0, WS);
    gemmini_fence();
  });
  asic_fence();
  ++stats.gemms;
}

static int quantize(unsigned job, unsigned count, elem_t *out) {
  int status = load32(INPUT, accum, count);
  if (!status)
    status = execute(job, 1);
  if (!status)
    status = store8(out, count);
  return status;
}

static int norm(const elem_t *in, const int32_t *gamma, const int32_t *beta, elem_t *out) {
  int status = load32(SECOND, gamma, 128);
  if (!status)
    status = load32(THIRD, beta, 128);
  for (unsigned row = 0; row < 2 && !status; ++row) {
    status = load8(INPUT, in + row * 128, 128);
    if (!status)
      status = execute(JOB_LN, 7);
    if (!status)
      status = store8(out + row * 128, 128);
  }
  return status;
}

static int add(const elem_t *a, const elem_t *b, elem_t *out) {
  int status = load8(INPUT, a, 256);
  if (!status)
    status = load8(SECOND, b, 256);
  if (!status)
    status = execute(JOB_RESIDUAL, 1);
  if (!status)
    status = store8(out, 256);
  return status;
}

static int attention(void) {
  for (unsigned head = 0; head < 2; ++head) {
    for (unsigned row = 0; row < 2; ++row) {
      memcpy(head_q + row * 64, query + row * 128 + head * 64, 64);
      memcpy(head_k + row * 64, key + row * 128 + head * 64, 64);
      memcpy(head_v + row * 64, value + row * 128 + head * 64, 64);
    }
    matmul(head_q, head_k, NULL, scores, 2, 2, 64, 1);
    for (unsigned row = 0; row < 2; ++row) {
      int status = load32(INPUT, scores + row * 2, 2);
      if (!status)
        status = execute(JOB_SOFTMAX + row * 3, 3);
      if (!status)
        status = store8(probability + row * 2, 2);
      if (status)
        return status;
    }
    matmul(probability, head_v, NULL, accum, 2, 64, 2, 0);
    const int status = quantize(JOB_PV, 128, head_out);
    if (status)
      return status;
    for (unsigned row = 0; row < 2; ++row)
      memcpy(concat + row * 128 + head * 64, head_out + row * 64, 64);
  }
  return 0;
}

static int pipeline(unsigned input) {
  int status = norm(block_x[input], block_gamma1, block_beta1, normalized);
  if (status)
    return status;
  const elem_t *weights[] = {block_wq, block_wk, block_wv};
  const acc_t *biases[] = {block_bq, block_bk, block_bv};
  elem_t *outputs[] = {query, key, value};
  for (unsigned index = 0; index < 3; ++index) {
    matmul(normalized, weights[index], biases[index], accum, 2, 128, 128, 0);
    status = quantize(JOB_QKV, 256, outputs[index]);
    if (status)
      return status;
  }
  status = attention();
  if (status)
    return status;
  matmul(concat, block_wo, block_bo, accum, 2, 128, 128, 0);
  status = quantize(JOB_ATTENTION, 256, branch);
  if (!status)
    status = add(block_x[input], branch, residual);
  if (!status)
    status = norm(residual, block_gamma2, block_beta2, normalized);
  if (status)
    return status;
  matmul(normalized, block_w1, block_b1, accum, 2, 512, 128, 0);
  status = quantize(JOB_FC1, 1024, expanded);
  if (!status && !stats.chained) {
    status = load8(OUTPUT, expanded, 1024);
    if (!status)
      status = execute(JOB_GELU, 1);
    if (!status)
      status = store8(expanded, 1024);
  }
  if (status)
    return status;
  matmul(expanded, block_w2, block_b2, accum, 2, 128, 512, 0);
  status = quantize(JOB_FC2, 256, branch);
  if (!status)
    status = add(residual, branch, final);
  return status;
}

static int run(int chained, unsigned input) {
  memset(&stats, 0, sizeof(stats));
  stats.chained = chained;
  memset(final, 0xa5, sizeof(final));
  asic_fence();
  uint64_t before[ASIC_COUNTERS], after[ASIC_COUNTERS];
  if (dedicated)
    snapshot(before);
  const unsigned long begin = cycles();
  if (!input) {
    accel_commands(GEMMINI0, { gemmini_flush(0); });
  }
  int status = input ? 0 : configure(chained);
  const unsigned long configured = cycles();
  if (!status)
    status = pipeline(input);
  asic_fence();
  const unsigned long finished = cycles();
  if (dedicated)
    snapshot(after);
  unsigned errors = 0;
  if (!status) {
    for (unsigned index = 0; index < 256; ++index) {
      if (final[index] != block_expected_y[input][index]) {
        if (errors < 8)
          printf("GPT COMPARE mismatch backend=%s input=%u word=%u actual=%d expected=%d\n", dedicated ? "ASIC" : "CGRA", input, index, final[index], block_expected_y[input][index]);
        ++errors;
      }
    }
  }
  printf(
      "GPT COMPARE backend=%s mode=%s phase=%s input=%u total_cycles=%lu setup_cycles=%lu pipeline_cycles=%lu configured_jobs=%u autolink_submissions=%u gemmini_gemms=%u backend_input_dma_bytes=%u "
      "backend_output_dma_bytes=%u checked=%u mismatches=%u status=%d\n",
      dedicated ? "ASIC" : "CGRA", chained ? "chain" : "operator", input ? "warm" : "cold", input, finished - begin, configured - begin, finished - configured, stats.configured, stats.submissions,
      stats.gemms, stats.input_bytes, stats.output_bytes, status ? 0 : 256, errors, status);
  if (dedicated)
    printf("GPT COMPARE ASIC hardware mode=%s phase=%s input=%u active_cycles=%lu core_starts=%lu read_bytes=%lu write_bytes=%lu native_cpu_starts=%lu config_commands=%lu\n",
           chained ? "chain" : "operator", input ? "warm" : "cold", input, (unsigned long)(after[ASIC_ACTIVE_CYCLES] - before[ASIC_ACTIVE_CYCLES]),
           (unsigned long)(after[ASIC_CORE_STARTS] - before[ASIC_CORE_STARTS]), (unsigned long)(after[ASIC_READ_BYTES] - before[ASIC_READ_BYTES]),
           (unsigned long)(after[ASIC_WRITE_BYTES] - before[ASIC_WRITE_BYTES]), (unsigned long)(after[ASIC_CPU_STARTS] - before[ASIC_CPU_STARTS]),
           (unsigned long)(after[ASIC_CONFIG_COMMANDS] - before[ASIC_CONFIG_COMMANDS]));
  return status || errors;
}

int main(int argc, char **argv) {
  const int mode = argc > 1 ? atoi(argv[1]) : -1;
  const int backend = argc > 2 ? atoi(argv[2]) : -1;
  if (mode < -1 || mode > 1 || backend < -1 || backend > 1)
    return 1;
  printf("GPT COMPARE scope=full-block N=2 hidden=128 heads=2 ff=512 reference-intermediates=0 transport=DRAM-materialized not-optimized-SPM=1 "
         "submission=AutoLink-both-backends CGRA-cache=full-static-per-phase cycle-boundary=configuration-packing-DMA-all-kernels-final-output\n");
  for (dedicated = backend < 0 ? 0 : backend; dedicated <= (backend < 0 ? 1 : backend); ++dedicated) {
    for (int selected = mode < 0 ? 0 : mode; selected <= (mode < 0 ? 1 : mode); ++selected) {
      for (unsigned input = 0; input < 2; ++input) {
        if (run(selected, input)) {
          printf("GPT COMPARE: FAIL\n");
          return 1;
        }
      }
    }
  }
  printf("GPT COMPARE: PASS\n");
  return 0;
}
