#include "auto_link.h"
#include "block_cgra.h"
#include "cgra_link.h"
#include "generated/gelu_data.h"

enum { COUNT = 32, MEAN = 0, ACTIVATION = 1, REQUANT = 2 };

static int32_t input[3][COUNT] __attribute__((aligned(64)));
static int32_t output[COUNT] __attribute__((aligned(64)));

static const cgra_link_symbol_t mean_args[] = {{.base = 0}, {.base = 512}, {.base = 128}, {.base = COUNT}};
static const cgra_link_symbol_t activation_args[] = {{.base = 128}, {.base = 768}, {.base = COUNT}};
static const cgra_link_symbol_t requant_args[] = {{.base = 256}, {.base = 1024}, {.base = 4}, {.base = COUNT}};

static int configure(void) {
  int status = 0;
  accel_commands(CGRA0, {
    status = cgra_job_full(MEAN, &BLOCK_LN_MEAN, mean_args, 0) || cgra_job_full(REQUANT, &BLOCK_REQUANT, requant_args, 0) || cgra_job_full(ACTIVATION, &BLOCK_ACTIVATION, activation_args, 0);
  });
  const auto_link_stage_t stage = {.endpoint = CGRA0.id, .output = {.address = {.binding = -1}}};
  const auto_link_edge_t edge = {.flags = 3, .address = {.binding = -1}};
  const auto_link_graph_t graph = {
      .stage_count = 1, .edge_count = 1, .stage_capacity = AUTO_LINK_GRAPH.stage_capacity, .edge_capacity = AUTO_LINK_GRAPH.edge_capacity, .stages = &stage, .edges = &edge};
  auto_link_load(&graph, NULL);
  auto_link_tiles(1, 1, 1, 1);
  return status;
}

static int native_check(int repeat) {
  static const int32_t values[4] __attribute__((aligned(64))) = {-127, 0, 127, 254};
  static int8_t quantized[4] __attribute__((aligned(64)));
  const uint32_t args[] = {0, 1024, 127, 4};
  if (block_load(values, 0, 4, 0) || block_kernel(&BLOCK_REQUANT, args, repeat) || block_store(quantized, 1024, 4)) {
    return 1;
  }
  return quantized[0] != -1 || quantized[1] != 0 || quantized[2] != 1 || quantized[3] != 2;
}

static int expected(unsigned job, unsigned index) {
  const int value = input[job][index];
  if (job == MEAN) {
    return value * 64 / 128 - 8192;
  }
  if (job == ACTIVATION) {
    return gelu_expected[value + 128];
  }
  const int rounded = value < 0 ? -((-value + 2) / 4) : (value + 2) / 4;
  return rounded < -128 ? -128 : rounded > 127 ? 127 : rounded;
}

static int run_job(unsigned job, unsigned run) {
  static const unsigned addresses[] = {512, 768, 1024};
  auto_link_job(0, job);
  auto_link_input_ready();
  while (*(volatile uint32_t *)(AUTO_LINK_BASE + AUTO_LINK_RUNNING)) {
  }
  const cgra_link_result_t result = cgra_link_wait();
  if (result.status != AUTO_LINK_STATUS_SUCCESS || result.detail || result.data || result.stage || result.job != job) {
    printf("Auto switch result run=%u job=%u status=%u detail=%u data=%u stage=%u actual_job=%u\n", run, job, result.status, result.detail, result.data, result.stage, result.job);
    return 1;
  }
  uint8_t tag;
  accel_commands(CGRA0, {
    cgra_dma_mvout_async(output, block_descriptor(addresses[job], COUNT));
    tag = cgra_dma_wait(0);
  });
  cgra_dma_memory_fence();
  if (tag) {
    return 1;
  }
  for (unsigned index = 0; index < COUNT; ++index) {
    const int value = expected(job, index);
    if (output[index] != value) {
      printf("Auto switch mismatch run=%u job=%u index=%u expected=%d actual=%d\n", run, job, index, value, output[index]);
      return 1;
    }
  }
  printf("Auto switch run=%u job=%u values=%u PASS\n", run, job, COUNT);
  return 0;
}

int main(void) {
  // Native execution before capture must not be mistaken for a cold automatic start.
  // Capture ends with a different kernel, but must not overwrite resident native controls.
  if (native_check(0) || configure() || native_check(1)) {
    printf("Auto switch: FAIL setup\n");
    return 1;
  }
  static const unsigned jobs[] = {MEAN, ACTIVATION, MEAN, MEAN, REQUANT};
  for (unsigned run = 0; run < 2; ++run) {
    for (unsigned index = 0; index < COUNT; ++index) {
      input[MEAN][index] = 128 * (index + 1 + run);
      input[ACTIVATION][index] = (int)((index * 7 + run * 41) % 256) - 128;
      input[REQUANT][index] = (int)(index * 43 + run * 19) - 650;
    }
    if (block_load(input[MEAN], 0, COUNT, 0) || block_load(input[ACTIVATION], 128, COUNT, 0) || block_load(input[REQUANT], 256, COUNT, 0)) {
      return 1;
    }
    for (unsigned step = 0; step < sizeof(jobs) / sizeof(jobs[0]); ++step) {
      if (run_job(jobs[step], run)) {
        printf("Auto switch: FAIL\n");
        return 1;
      }
    }
    // Native use must invalidate the same cached job, not only a different next job.
    if (run_job(MEAN, run) || native_check(0) || block_load(input[MEAN], 0, COUNT, 0) || run_job(MEAN, run)) {
      printf("Auto switch: FAIL native invalidation\n");
      return 1;
    }
  }
  printf("Auto switch: PASS values=%u\n", 2u * COUNT * ((unsigned)(sizeof(jobs) / sizeof(jobs[0])) + 2));
  return 0;
}
