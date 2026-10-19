#ifndef BLOCK_AUTO_H
#define BLOCK_AUTO_H

#include "block_auto_jobs.h"
#include "block_cgra.h"

// Retained-SPM plans for N16/64 H128 and N16 H768 on configs/soc/autolink/gpt2.yaml.
// Runtime graphs below vary copy format, endpoint, and job without regenerating RTL.
enum { BA_LN1 = 3, BA_LN2 = 10, BA_QKV = 17, BA_OUTPUT = 18, BA_FC = 19, BA_PV = 20, BA_ADD = 21, BA_GELU = 22, BA_AFFINE = 16384, BA_LN_OUTPUT = 4096, BA_STATS = 6144 };
enum { BA_WORDS = BLOCK_N * BLOCK_HIDDEN, BA_EXPANDED = BLOCK_N * BLOCK_FF, BA_LN_ROWS = BLOCK_HIDDEN == 768 ? 2 : 16, BA_LN_WORDS = BA_LN_ROWS * BLOCK_HIDDEN };

static struct {
  unsigned profiles, reports, jobs[2], expected[3][32];
  int8_t *skip, *normalized, *query, *key, *value, *branch, *expanded;
  int32_t *accum;
} ba_plan;

static void ba_reset(void) {
  memset(&ba_plan, 0, sizeof(ba_plan));
  ba_plan.jobs[0] = ba_plan.jobs[1] = 2;
  ba_plan.skip = (int8_t *)(GEMMINI0.spm + 64 * 1024);
  ba_plan.normalized = ba_plan.skip + BA_WORDS;
  ba_plan.query = ba_plan.normalized + BA_WORDS;
  ba_plan.key = ba_plan.query + BA_WORDS;
  ba_plan.branch = ba_plan.key + BA_WORDS;
  ba_plan.value = (int8_t *)(GEMMINI1.spm + 64 * 1024);
  ba_plan.accum = (int32_t *)(ba_plan.value + BA_WORDS);
  ba_plan.expanded = ba_plan.value;
}

static unsigned ba_job(accel_t device) { return ba_plan.jobs[device.id]++; }

static void ba_expect(unsigned stage, unsigned job, unsigned count) {
  ba_plan.expected[stage][job] += count;
  ba_plan.reports += count;
}

static int ba_kernel(unsigned job, const cgra_kernel_t *kernel, const uint32_t args[8], unsigned next) {
  cgra_link_symbol_t symbols[8];
  for (unsigned index = 0; index < 8; ++index) {
    symbols[index] = (cgra_link_symbol_t){args[index], 0, CGRA_LINK_SLOT};
  }
  int failed;
  accel_commands(CGRA0, { failed = cgra_job_full_at(CGRA0.control, job, kernel, symbols, next); });
  return failed;
}

static int ba_ln_kernels(unsigned first, unsigned affine) {
  const struct {
    const cgra_kernel_t *kernel;
    uint32_t args[8];
  } phases[] = {
      {&BLOCK_LN_SUM, {0, BA_STATS, BLOCK_HIDDEN, BA_LN_WORDS}},
      {&BLOCK_LN_MEAN, {BA_STATS, BA_STATS + 16, BLOCK_HIDDEN, BA_LN_ROWS}},
      {&BLOCK_LN_SQUARES, {0, BA_STATS + 16, BA_STATS + 32, BLOCK_HIDDEN, BA_LN_WORDS}},
      {&BLOCK_LN_VARIANCE, {BA_STATS + 32, BA_STATS + 48, BLOCK_HIDDEN, BA_LN_ROWS}},
      {&BLOCK_LN_SQRT, {BA_STATS + 48, BA_STATS + 64, BA_LN_ROWS}},
      {&BLOCK_LN_NORM, {0, affine, affine + BLOCK_HIDDEN, BA_LN_OUTPUT, BA_STATS + 16, BA_STATS + 64, BLOCK_HIDDEN, BA_LN_WORDS}},
      {&BLOCK_REQUANT, {BA_LN_OUTPUT, BA_LN_OUTPUT, 4, BA_LN_WORDS}},
  };
  for (unsigned index = 0; index < 7; ++index) {
    if (ba_kernel(first + index, phases[index].kernel, phases[index].args, index == 6 ? 0 : first + index + 2)) {
      return 1;
    }
  }
  return 0;
}

static int ba_kernels(void) {
  if (block_load(block_gamma1, BA_AFFINE, BLOCK_HIDDEN, 0) || block_load(block_beta1, BA_AFFINE + BLOCK_HIDDEN, BLOCK_HIDDEN, 0) ||
      block_load(block_gamma2, BA_AFFINE + 2 * BLOCK_HIDDEN, BLOCK_HIDDEN, 0) || block_load(block_beta2, BA_AFFINE + 3 * BLOCK_HIDDEN, BLOCK_HIDDEN, 0) || ba_ln_kernels(BA_LN1, BA_AFFINE) ||
      ba_ln_kernels(BA_LN2, BA_AFFINE + 2 * BLOCK_HIDDEN)) {
    return 1;
  }
  const unsigned divisors[] = {BLOCK_Q_DIV, BLOCK_ATTENTION_DIV, BLOCK_FC1_DIV, BLOCK_PV_DIV};
  for (unsigned index = 0; index < 4; ++index) {
    const uint32_t args[8] = {0, 1024, divisors[index], 1024};
    if (ba_kernel(BA_QKV + index, &BLOCK_REQUANT, args, 0)) {
      return 1;
    }
  }
  const uint32_t add[8] = {0, 1024, 2048, 1024};
  const uint32_t gelu[8] = {0, 2048, 2048};
  if (ba_kernel(BA_ADD, &BLOCK_RESIDUAL, add, 0) || ba_kernel(BA_GELU, &BLOCK_ACTIVATION, gelu, 0)) {
    return 1;
  }
  for (unsigned phase = 0; phase < 3; ++phase) {
    const cgra_link_job_t *job = &attn_phases[phase];
    int failed;
    accel_commands(CGRA0, { failed = cgra_job_full_at(CGRA0.control, job->job, job->kernel, job->bindings, phase == 2 ? 0 : attn_phases[phase + 1].job + 1); });
    if (failed) {
      return 1;
    }
  }
  return 0;
}

// One tile means gather, compute, and publication drain before the next repeat.
static void ba_pipeline(accel_t producer, unsigned gather, unsigned kernel, accel_t consumer, unsigned store, unsigned input_bytes, int packed, unsigned output_word, unsigned output_bytes,
                        unsigned repeats) {
  const auto_link_stage_t stages[] = {
      {.endpoint = producer.id, .job = gather, .output = {.address = {.binding = -1}}},
      {.endpoint = CGRA0.id, .job = kernel, .output = {.address = {.binding = -1}}},
      {.endpoint = consumer.id, .job = store, .output = {.address = {.binding = -1}}},
  };
  const auto_link_edge_t edges[] = {
      {.destination = 0, .flags = 3, .address = {.binding = -1}},
      {.source = 0, .destination = 1, .flags = 5, .bytes = input_bytes, .expansion = packed ? 2 : 0, .address = {.value = producer.spm, .binding = -1}, .source_offset = BA_PUBLICATION},
      {.source = 1, .destination = 2, .flags = 5, .bytes = output_bytes, .address = {.value = CGRA0.spm, .binding = -1}, .source_offset = output_word},
  };
  const auto_link_graph_t graph = {.stage_count = 3, .edge_count = 3, .stage_capacity = 3, .edge_capacity = 3, .stages = stages, .edges = edges};
  auto_link_load(&graph, NULL);
  auto_link_tiles(1, 1, 1, 1);
  auto_link_run_repeat(ba_plan.profiles, ba_plan.profiles, repeats);
  ++ba_plan.profiles;
  ba_expect(0, gather, repeats);
  ba_expect(1, kernel, repeats);
  ba_expect(2, store, repeats);
}

static int ba_copy_plan(accel_t device, const int8_t *input, int8_t *output, unsigned count) {
  const unsigned job = ba_job(device);
  if (ba_copy(device, job, input, output, 2048)) {
    return 1;
  }
  const auto_link_stage_t stage = {.endpoint = device.id, .job = job, .output = {.address = {.binding = -1}}};
  const auto_link_edge_t edge = {.destination = 0, .flags = 3, .address = {.binding = -1}};
  const auto_link_graph_t graph = {.stage_count = 1, .edge_count = 1, .stage_capacity = 3, .edge_capacity = 3, .stages = &stage, .edges = &edge};
  auto_link_load(&graph, NULL);
  auto_link_tiles(1, 1, 1, 1);
  auto_link_run_repeat(ba_plan.profiles, ba_plan.profiles, count / 2048);
  ++ba_plan.profiles;
  ba_expect(0, job, count / 2048);
  return 0;
}

static int ba_vector(const void *left, const void *right, int8_t *output, unsigned count, unsigned chunk, unsigned kernel, unsigned output_word, int packed) {
  const unsigned gather = ba_job(GEMMINI0), store = ba_job(GEMMINI1);
  if (ba_gather(GEMMINI0, gather, left, right, chunk, packed) || ba_collect(GEMMINI1, store, output_word, output, chunk / DIM, chunk / DIM, chunk)) {
    return 1;
  }
  ba_pipeline(GEMMINI0, gather, kernel, GEMMINI1, store, chunk * (packed ? 1 : sizeof(acc_t)) * (right != NULL ? 2 : 1), packed, output_word, chunk, count / chunk);
  return 0;
}

static int ba_projection_columns(accel_t device, const int8_t *input, const int8_t *weight, const int32_t *bias, int8_t *output, unsigned inner, unsigned columns, unsigned kernel, unsigned first,
                                 unsigned count) {
  const accel_t consumer = device.id == GEMMINI0.id ? GEMMINI1 : GEMMINI0;
  const unsigned producer_job = ba_job(device), consumer_job = ba_job(consumer);
  if (ba_columns(device, producer_job, input, weight, bias, inner, columns, first) || ba_collect(consumer, consumer_job, 1024, output + first, 64, columns, 64)) {
    return 1;
  }
  ba_pipeline(device, producer_job, kernel, consumer, consumer_job, 4096, 0, 1024, 1024, count / 64);
  return 0;
}

static int ba_projection(accel_t device, const int8_t *input, const int8_t *weight, const int32_t *bias, int8_t *output, unsigned inner, unsigned columns, unsigned kernel) {
  if (BLOCK_N == 16) {
    return ba_projection_columns(device, input, weight, bias, output, inner, columns, kernel, 0, columns);
  }
  const accel_t consumer = device.id == GEMMINI0.id ? GEMMINI1 : GEMMINI0;
  for (unsigned column = 0; column < columns; column += 64) {
    const unsigned producer_job = ba_job(device), consumer_job = ba_job(consumer);
    if (ba_matmul(device, producer_job, input, weight, bias, inner, columns, column) || ba_collect(consumer, consumer_job, 1024, output + column, 64, columns, DIM * columns)) {
      return 1;
    }
    ba_pipeline(device, producer_job, kernel, consumer, consumer_job, 4096, 0, 1024, 1024, BLOCK_N / DIM);
  }
  return 0;
}

static int ba_attention(void) {
  if (attn_config_scores() || attn_config_loads(ba_plan.query, ba_plan.key, ba_plan.value) || attn_config_values(ba_plan.accum, 0, 1)) {
    return 1;
  }
  const unsigned first = ba_plan.profiles;
  auto_link_load(&BLOCK_LOAD_GRAPH, NULL);
  auto_link_job(BLOCK_LOAD_STAGE_QK, ATTN_LOAD_JOB);
  auto_link_job(BLOCK_LOAD_STAGE_V, ATTN_LOAD_JOB);
  auto_link_tiles(1, 1, 1, 1);
  auto_link_run_config(ba_plan.profiles++);
  attn_config_graph();
  auto_link_run_repeat(ba_plan.profiles++, first, BLOCK_HEADS);
  ba_expect(0, 1, BLOCK_HEADS);
  ba_expect(1, 1, BLOCK_HEADS);
  for (unsigned stage = 0; stage < 3; ++stage) {
    ba_expect(stage, 0, BLOCK_HEADS);
  }
  return 0;
}

static int ba_block(unsigned run, int8_t *output) {
  return ba_copy_plan(GEMMINI0, block_x[run], ba_plan.skip, BA_WORDS) || ba_vector(ba_plan.skip, NULL, ba_plan.normalized, BA_WORDS, BA_LN_WORDS, BA_LN1, BA_LN_OUTPUT, 1) ||
         ba_projection(GEMMINI0, ba_plan.normalized, block_wq, block_bq, ba_plan.query, BLOCK_HIDDEN, BLOCK_HIDDEN, BA_QKV) ||
         ba_projection(GEMMINI0, ba_plan.normalized, block_wk, block_bk, ba_plan.key, BLOCK_HIDDEN, BLOCK_HIDDEN, BA_QKV) ||
         ba_projection(GEMMINI1, ba_plan.normalized, block_wv, block_bv, ba_plan.value, BLOCK_HIDDEN, BLOCK_HIDDEN, BA_QKV) || ba_attention() ||
         ba_vector(ba_plan.accum, NULL, ba_plan.query, BA_WORDS, 1024, BA_PV, 1024, 0) ||
         ba_projection(GEMMINI0, ba_plan.query, block_wo, block_bo, ba_plan.branch, BLOCK_HIDDEN, BLOCK_HIDDEN, BA_OUTPUT) ||
         ba_vector(ba_plan.skip, ba_plan.branch, ba_plan.skip, BA_WORDS, 1024, BA_ADD, 2048, 1) || ba_vector(ba_plan.skip, NULL, ba_plan.normalized, BA_WORDS, BA_LN_WORDS, BA_LN2, BA_LN_OUTPUT, 1) ||
         ba_projection(GEMMINI0, ba_plan.normalized, block_w1, block_b1, ba_plan.expanded, BLOCK_HIDDEN, BLOCK_FF, BA_FC) ||
         ba_vector(ba_plan.expanded, NULL, ba_plan.expanded, BA_EXPANDED, 2048, BA_GELU, 2048, 1) ||
         ba_projection(GEMMINI1, ba_plan.expanded, block_w2, block_b2, ba_plan.branch, BLOCK_FF, BLOCK_HIDDEN, BA_FC) ||
         ba_vector(ba_plan.skip, ba_plan.branch, ba_plan.branch, BA_WORDS, 1024, BA_ADD, 2048, 1) || ba_copy_plan(GEMMINI1, ba_plan.branch, output, BA_WORDS);
}

static int ba_configure(unsigned mode, unsigned run, int8_t *output) {
  ba_reset();
  if (mode != 1 && ba_kernels()) {
    return 1;
  }
  switch (mode) {
  case 0:
    return ba_block(run, output);
  case 1:
    return ba_copy_plan(GEMMINI0, block_x[run], ba_plan.skip, BA_WORDS) || ba_copy_plan(GEMMINI1, ba_plan.skip, output, BA_WORDS);
  case 2:
    return ba_copy_plan(GEMMINI0, block_expected_ln1[run], ba_plan.normalized, BA_WORDS) ||
           ba_projection(GEMMINI0, ba_plan.normalized, block_wq, block_bq, ba_plan.query, BLOCK_HIDDEN, BLOCK_HIDDEN, BA_QKV) || ba_copy_plan(GEMMINI1, ba_plan.query, output, BA_WORDS);
  case 3:
    return ba_copy_plan(GEMMINI0, block_x[run], ba_plan.skip, BA_WORDS) || ba_vector(ba_plan.skip, NULL, ba_plan.normalized, BA_WORDS, BA_LN_WORDS, BA_LN1, BA_LN_OUTPUT, 1) ||
           ba_copy_plan(GEMMINI1, ba_plan.normalized, output, BA_WORDS);
  case 4:
    return ba_copy_plan(GEMMINI0, block_expected_fc1[run], ba_plan.expanded, BA_EXPANDED) || ba_vector(ba_plan.expanded, NULL, ba_plan.expanded, BA_EXPANDED, 2048, BA_GELU, 2048, 1) ||
           ba_copy_plan(GEMMINI1, ba_plan.expanded, output, BA_EXPANDED);
  case 5:
    return ba_copy_plan(GEMMINI0, block_x[run], ba_plan.skip, BA_WORDS) || ba_copy_plan(GEMMINI0, block_expected_attention[run], ba_plan.branch, BA_WORDS) ||
           ba_vector(ba_plan.skip, ba_plan.branch, ba_plan.skip, BA_WORDS, 1024, BA_ADD, 2048, 1) || ba_copy_plan(GEMMINI1, ba_plan.skip, output, BA_WORDS);
  case 6:
    return ba_vector(block_pv_accum[run], NULL, ba_plan.query, BA_WORDS, 1024, BA_PV, 1024, 0) || ba_copy_plan(GEMMINI1, ba_plan.query, output, BA_WORDS);
  case 7:
    return ba_copy_plan(GEMMINI0, block_expected_q[run], ba_plan.query, BA_WORDS) || ba_copy_plan(GEMMINI0, block_expected_k[run], ba_plan.key, BA_WORDS) ||
           ba_copy_plan(GEMMINI1, block_expected_v[run], ba_plan.value, BA_WORDS) || ba_attention() || ba_vector(ba_plan.accum, NULL, ba_plan.query, BA_WORDS, 1024, BA_PV, 1024, 0) ||
           ba_copy_plan(GEMMINI1, ba_plan.query, output, BA_WORDS);
  }
  return 1;
}

static int ba_execute(void) {
  auto_link_run(0, ba_plan.profiles);
  unsigned reports = 0, failures = 0;
  // Long repeated plans can produce more reports than the physical result queue.
  for (;;) {
    const unsigned running = *(volatile uint32_t *)(AUTO_LINK_BASE + AUTO_LINK_RUNNING);
    if (!cgra_link_read_at(CGRA0.control, CGRA_LINK_CONTROL_RESULT_VALID)) {
      if (!running) {
        break;
      }
      continue;
    }
    const cgra_link_result_t result = cgra_link_wait_at(CGRA0.control);
    if (result.status != AUTO_LINK_STATUS_SUCCESS || result.detail || result.data || result.stage >= 3 || result.job >= 32 || !ba_plan.expected[result.stage][result.job]) {
      printf("Auto block report stage=%u job=%u status=%u detail=%u data=%u\n", result.stage, result.job, result.status, result.detail, result.data);
      ++failures;
    } else {
      --ba_plan.expected[result.stage][result.job];
    }
    ++reports;
  }
  gemmini_fence();
  if (reports != ba_plan.reports) {
    printf("Auto block reports=%u expected=%u\n", reports, ba_plan.reports);
    ++failures;
  }
  return failures;
}

#endif
