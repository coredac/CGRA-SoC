#include "accel_generated.h"
#include "asic.h"
#include "auto_link.h"
#include "cgra_link.h"

#include <stdio.h>
#include <string.h>

enum { INPUT = 0, SECOND = 128, THIRD = 256, FEATURES = 384, OUTPUT = 512 };
static int32_t first[128] __attribute__((aligned(64)));
static int32_t second[128] __attribute__((aligned(64)));
static int32_t third[128] __attribute__((aligned(64)));
static int32_t expected[128];
static int32_t result[128] __attribute__((aligned(64)));
static const accel_t devices[] = {ADD0, RELU0, GCN0, NORM0, SOFTMAX0, GPT_VECTOR0};
static unsigned checked, automatic;

static int execute(accel_t device, unsigned index, const asic_job_t *job) {
  if (asic_config(device, index, job))
    return 1;
  const uint64_t cpu_starts = asic_counter(device, ASIC_CPU_STARTS);
  const uint64_t core_starts = asic_counter(device, ASIC_CORE_STARTS);
  const auto_link_stage_t stage = {.endpoint = device.id, .job = index, .output = {.address = {.binding = -1}}};
  const auto_link_edge_t edge = {.destination = 0, .flags = 3, .address = {.binding = -1}};
  const auto_link_graph_t graph = {
      .stage_count = 1, .edge_count = 1, .stage_capacity = AUTO_LINK_GRAPH.stage_capacity, .edge_capacity = AUTO_LINK_GRAPH.edge_capacity, .stages = &stage, .edges = &edge};
  auto_link_load(&graph, NULL);
  auto_link_tiles(1, 1, 1, 1);
  auto_link_input_ready();
  unsigned reports = 0;
  for (;;) {
    const unsigned running = *(volatile uint32_t *)(AUTO_LINK_BASE + AUTO_LINK_RUNNING);
    if (!cgra_link_read_at(CGRA0.control, CGRA_LINK_CONTROL_RESULT_VALID)) {
      if (!running)
        break;
      continue;
    }
    const cgra_link_result_t result = cgra_link_wait_at(CGRA0.control);
    if (result.status || result.detail || result.data || result.stage || result.job != index) {
      printf("ASIC INDEPENDENT AutoLink failure endpoint=%u stage=%u job=%u status=%u detail=%u data=%u\n", device.id, result.stage, result.job, result.status, result.detail, result.data);
      return 1;
    }
    ++reports;
  }
  if (reports != 1)
    return 1;
  if (asic_counter(device, ASIC_CPU_STARTS) != cpu_starts || asic_counter(device, ASIC_CORE_STARTS) != core_starts + 1) {
    printf("ASIC INDEPENDENT AutoLink launch counters failed endpoint=%u job=%u\n", device.id, index);
    return 1;
  }
  ++automatic;
  return 0;
}

static int check(const char *name, accel_t device, unsigned offset, unsigned words) {
  memset(result, 0xa5, sizeof(result));
  if (asic_store(device, result, offset, (words + 7u) / 8u * 8u))
    return 1;
  for (unsigned index = 0; index < words; ++index) {
    if (result[index] != expected[index]) {
      printf("ASIC INDEPENDENT mismatch endpoint=%s index=%u actual=%d expected=%d\n", name, index, result[index], expected[index]);
      return 1;
    }
  }
  checked += words;
  printf("ASIC INDEPENDENT endpoint=%s checked=%u PASS\n", name, words);
  return 0;
}

static int vector_pair(void) {
  for (unsigned index = 0; index < 32; ++index) {
    first[index] = (int)index - 20;
    second[index] = (int)index * 2 - 8;
    expected[index] = first[index] + second[index];
  }
  const asic_job_t add = {.unit = 1, .operation = 1, .count = 32, .src0 = INPUT, .src1 = SECOND, .dst = OUTPUT};
  if (asic_load(ADD0, INPUT, first, 32) || asic_load(ADD0, SECOND, second, 32) || execute(ADD0, 3, &add) || check("add", ADD0, OUTPUT, 32))
    return 1;
  // Move the producer's publication window directly into the consumer's private SPM.
  const void *peer = (const void *)(uintptr_t)(ADD0.spm + OUTPUT * sizeof(int32_t));
  if (asic_load(RELU0, INPUT, peer, 32))
    return 1;
  for (unsigned index = 0; index < 32; ++index)
    expected[index] = expected[index] < 0 ? 0 : expected[index];
  const asic_job_t relu = {.unit = 1, .count = 32, .src0 = INPUT, .dst = OUTPUT};
  return execute(RELU0, 3, &relu) || check("relu-peer-dma", RELU0, OUTPUT, 32);
}

static int gcn(void) {
  memset(first, 0, sizeof(first));
  first[1] = 1;
  memset(second, 0, sizeof(second));
  memset(third, 0, sizeof(third));
  third[0] = 3;
  if (asic_load(GCN0, INPUT, first, 8) || asic_load(GCN0, SECOND, second, 8) || asic_load(GCN0, THIRD, third, 8))
    return 1;
  for (unsigned index = 0; index < 128; ++index) {
    first[index] = (int)index - 64;
    expected[index] = first[index] > 0 ? first[index] * 3 : 0;
  }
  const asic_job_t job = {.unit = 0, .fused = 1, .count = 128, .rows = 1, .edges = 1, .src0 = INPUT, .src1 = SECOND, .src2 = THIRD, .src3 = FEATURES, .dst = OUTPUT};
  return asic_load(GCN0, FEATURES, first, 128) || execute(GCN0, 1, &job) || check("gcn", GCN0, OUTPUT, 128);
}

static int norm(void) {
  for (unsigned index = 0; index < 128; ++index) {
    first[index] = 7;
    second[index] = 64;
    third[index] = ((int)index - 64) * 4;
    expected[index] = (int)index - 64;
  }
  const asic_job_t job = {.unit = 2, .fused = 1, .count = 128, .src0 = INPUT, .src1 = SECOND, .src2 = THIRD, .dst = OUTPUT};
  return asic_load(NORM0, INPUT, first, 128) || asic_load(NORM0, SECOND, second, 128) || asic_load(NORM0, THIRD, third, 128) || execute(NORM0, 7, &job) || check("norm", NORM0, OUTPUT, 128);
}

static int softmax(void) {
  memset(first, 0, sizeof(first));
  expected[0] = 64;
  expected[1] = 64;
  const asic_job_t job = {.unit = 3, .fused = 1, .count = 2, .rows = 1, .src0 = INPUT, .dst = OUTPUT};
  // Exercise the last entry of the six-entry descriptor cache.
  return asic_load(SOFTMAX0, INPUT, first, 8) || execute(SOFTMAX0, 5, &job) || check("softmax", SOFTMAX0, OUTPUT, 2);
}

static int gpt_vector(void) {
  for (unsigned index = 0; index < 32; ++index) {
    first[index] = ((int)index - 16) * 16;
    second[index] = -(int)index;
    const int value = first[index] + second[index];
    expected[index] = value > 127 ? 127 : value < -128 ? -128 : value;
  }
  const asic_job_t job = {.unit = 4, .operation = 2, .count = 32, .src0 = INPUT, .src1 = SECOND, .dst = OUTPUT};
  return asic_load(GPT_VECTOR0, INPUT, first, 32) || asic_load(GPT_VECTOR0, SECOND, second, 32) || execute(GPT_VECTOR0, 7, &job) || check("gpt-vector", GPT_VECTOR0, OUTPUT, 32);
}

static int private_memory(int verify) {
  for (unsigned device = 0; device < sizeof(devices) / sizeof(devices[0]); ++device) {
    for (unsigned index = 0; index < 8; ++index)
      first[index] = expected[index] = 1000 * (int)(device + 1) + (int)index;
    const unsigned end = devices[device].spm_bytes / sizeof(int32_t) - 8;
    const int status = verify ? check("private-spm-end", devices[device], end, 8) : asic_load(devices[device], end, first, 8);
    if (status)
      return status;
  }
  return 0;
}

int main(void) {
  if (private_memory(0) || vector_pair() || gcn() || norm() || softmax() || gpt_vector() || private_memory(1)) {
    printf("ASIC INDEPENDENT: FAIL\n");
    return 1;
  }
  printf("ASIC INDEPENDENT: PASS endpoints=6 spm_bytes=131072 checked=%u peer_dma=1 autolink=%u\n", checked, automatic);
  return 0;
}
