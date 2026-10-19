#ifndef BLOCK_HYBRID_H
#define BLOCK_HYBRID_H

#include "block_auto.h"

enum { BH_PROFILES = 8, BH_JOBS = 8, BH_REPORTS = 128, BH_TOTAL_REPORTS = BLOCK_HIDDEN == 768 ? 624 : (BLOCK_N == 16 ? 102 : 378) };

static const int8_t *bh_input;
static int8_t *bh_output;
static unsigned bh_cgra_jobs;

static int bh_load(void) { return ba_copy_plan(GEMMINI0, bh_input, ba_plan.skip, BA_WORDS); }

static int bh_store(void) { return ba_copy_plan(GEMMINI1, ba_plan.branch, bh_output, BA_WORDS); }

static int bh_ln(const int32_t *gamma, const int32_t *beta) {
  bh_cgra_jobs = 7;
  return block_load(gamma, BA_AFFINE, BLOCK_HIDDEN, 0) || block_load(beta, BA_AFFINE + BLOCK_HIDDEN, BLOCK_HIDDEN, 0) || ba_ln_kernels(0, BA_AFFINE) ||
         ba_vector(ba_plan.skip, NULL, ba_plan.normalized, BA_WORDS, BA_LN_WORDS, 0, BA_LN_OUTPUT, 1);
}

static int bh_ln1(void) { return bh_ln(block_gamma1, block_beta1); }

static int bh_ln2(void) { return bh_ln(block_gamma2, block_beta2); }

static int bh_requant(unsigned divisor) {
  const uint32_t args[8] = {0, 1024, divisor, 1024};
  bh_cgra_jobs = 1;
  return ba_kernel(0, &BLOCK_REQUANT, args, 0);
}

static int bh_projection(accel_t device, const int8_t *input, const int8_t *weight, const int32_t *bias, int8_t *output, unsigned inner, unsigned columns, unsigned divisor) {
  if (BLOCK_N != 16 && (columns / 64 > BH_PROFILES || columns / 64 > BH_JOBS)) {
    return 1;
  }
  return bh_requant(divisor) || ba_projection(device, input, weight, bias, output, inner, columns, 0);
}

static int bh_query(void) { return bh_projection(GEMMINI0, ba_plan.normalized, block_wq, block_bq, ba_plan.query, BLOCK_HIDDEN, BLOCK_HIDDEN, BLOCK_Q_DIV); }

static int bh_key(void) { return bh_projection(GEMMINI0, ba_plan.normalized, block_wk, block_bk, ba_plan.key, BLOCK_HIDDEN, BLOCK_HIDDEN, BLOCK_K_DIV); }

static int bh_value(void) { return bh_projection(GEMMINI1, ba_plan.normalized, block_wv, block_bv, ba_plan.value, BLOCK_HIDDEN, BLOCK_HIDDEN, BLOCK_V_DIV); }

static int bh_attention(void) {
  bh_cgra_jobs = 3;
  for (unsigned phase = 0; phase < 3; ++phase) {
    const cgra_link_job_t *job = &attn_phases[phase];
    int failed;
    accel_commands(CGRA0, { failed = cgra_job_full_at(CGRA0.control, job->job, job->kernel, job->bindings, phase == 2 ? 0 : attn_phases[phase + 1].job + 1); });
    if (failed) {
      return 1;
    }
  }
  // Attention captures scores/values as job 0 and head loads as job 1.
  ba_plan.jobs[0] = ba_plan.jobs[1] = 2;
  return ba_attention();
}

static int bh_pv(void) { return bh_requant(BLOCK_PV_DIV) || ba_vector(ba_plan.accum, NULL, ba_plan.query, BA_WORDS, 1024, 0, 1024, 0); }

static int bh_project(void) { return bh_projection(GEMMINI0, ba_plan.query, block_wo, block_bo, ba_plan.branch, BLOCK_HIDDEN, BLOCK_HIDDEN, BLOCK_ATTENTION_DIV); }

static int bh_add(int8_t *output) {
  const uint32_t args[8] = {0, 1024, 2048, 1024};
  bh_cgra_jobs = 1;
  return ba_kernel(0, &BLOCK_RESIDUAL, args, 0) || ba_vector(ba_plan.skip, ba_plan.branch, output, BA_WORDS, 1024, 0, 2048, 1);
}

static int bh_add1(void) { return bh_add(ba_plan.skip); }

static int bh_add2(void) { return bh_add(ba_plan.branch); }

static int bh_fc1(void) {
  if (BLOCK_HIDDEN == 768) {
    // Split FC1 to keep each batch below the 128-report Hybrid limit.
    return bh_requant(BLOCK_FC1_DIV) || ba_projection_columns(GEMMINI0, ba_plan.normalized, block_w1, block_b1, ba_plan.expanded, BLOCK_HIDDEN, BLOCK_FF, 0, 0, BLOCK_FF / 2);
  }
  return bh_projection(GEMMINI0, ba_plan.normalized, block_w1, block_b1, ba_plan.expanded, BLOCK_HIDDEN, BLOCK_FF, BLOCK_FC1_DIV);
}

static int bh_fc1b(void) {
  return bh_requant(BLOCK_FC1_DIV) || ba_projection_columns(GEMMINI0, ba_plan.normalized, block_w1, block_b1, ba_plan.expanded, BLOCK_HIDDEN, BLOCK_FF, 0, BLOCK_FF / 2, BLOCK_FF / 2);
}

static int bh_fc2(void) { return bh_projection(GEMMINI1, ba_plan.expanded, block_w2, block_b2, ba_plan.branch, BLOCK_FF, BLOCK_HIDDEN, BLOCK_FC2_DIV); }

static int bh_gelu(void) {
  const uint32_t args[8] = {0, 2048, 2048};
  bh_cgra_jobs = 1;
  return ba_kernel(0, &BLOCK_ACTIVATION, args, 0) || ba_vector(ba_plan.expanded, NULL, ba_plan.expanded, BA_EXPANDED, 2048, 0, 2048, 1);
}

static const struct {
  const char *name;
  int (*configure)(void);
} bh_stages[] = {{"input", bh_load},
                 {"ln1", bh_ln1},
                 {"q", bh_query},
                 {"k", bh_key},
                 {"v", bh_value},
                 {"attention", bh_attention},
                 {"pv", bh_pv},
                 {"projection", bh_project},
                 {"add1", bh_add1},
                 {"ln2", bh_ln2},
                 {BLOCK_HIDDEN == 768 ? "fc1a" : "fc1", bh_fc1},
                 {"fc1b", BLOCK_HIDDEN == 768 ? bh_fc1b : NULL},
                 {"gelu", bh_gelu},
                 {"fc2", bh_fc2},
                 {"add2", bh_add2},
                 {"output", bh_store}};

enum { BH_STAGES = sizeof(bh_stages) / sizeof(bh_stages[0]) };

typedef struct {
  unsigned profiles, jobs[3], reports;
  unsigned long setup, execute;
} bh_stats_t;

static bh_stats_t bh_stats[BH_STAGES];

static int bh_execute(unsigned run, int8_t *output) {
  bh_input = block_x[run];
  bh_output = output;
  unsigned reports = 0;
  for (unsigned stage = 0; stage < BH_STAGES; ++stage) {
    if (!bh_stages[stage].configure) {
      continue;
    }
    const unsigned long begin = block_clock();
    // The preceding execution has drained before any cache ID is reused.
    ba_reset();
    ba_plan.jobs[0] = ba_plan.jobs[1] = 0;
    bh_cgra_jobs = 0;
    if (bh_stages[stage].configure()) {
      printf("Hybrid block configuration failed at %s\n", bh_stages[stage].name);
      return 1;
    }
    bh_stats_t *stats = &bh_stats[stage];
    stats->profiles = ba_plan.profiles;
    stats->jobs[0] = ba_plan.jobs[0];
    stats->jobs[1] = ba_plan.jobs[1];
    stats->jobs[2] = bh_cgra_jobs;
    stats->reports = ba_plan.reports;
    if (stats->profiles > BH_PROFILES || stats->jobs[0] > BH_JOBS || stats->jobs[1] > BH_JOBS || stats->jobs[2] > BH_JOBS || stats->reports > BH_REPORTS) {
      printf("Hybrid block capacity exceeded at %s\n", bh_stages[stage].name);
      return 1;
    }
    const unsigned long configured = block_clock();
    const int failed = ba_execute();
    const unsigned long end = block_clock();
    stats->setup = configured - begin;
    stats->execute = end - configured;
    if (failed) {
      printf("Hybrid block execution failed at %s\n", bh_stages[stage].name);
      return 1;
    }
    reports += stats->reports;
  }
  if (reports != BH_TOTAL_REPORTS) {
    printf("Hybrid block reports=%u expected=%u\n", reports, BH_TOTAL_REPORTS);
    return 1;
  }
  return 0;
}

static void bh_print(unsigned run, unsigned long total) {
  bh_stats_t peak = {0};
  unsigned reports = 0;
  unsigned long setup = 0, execute = 0;
  for (unsigned stage = 0; stage < BH_STAGES; ++stage) {
    if (!bh_stages[stage].configure) {
      continue;
    }
    const bh_stats_t *stats = &bh_stats[stage];
    printf("Hybrid block run=%u stage=%s profiles=%u jobs=%u/%u/%u reports=%u setup=%lu execute=%lu\n", run, bh_stages[stage].name, stats->profiles, stats->jobs[0], stats->jobs[1], stats->jobs[2],
           stats->reports, stats->setup, stats->execute);
    if (stats->profiles > peak.profiles) {
      peak.profiles = stats->profiles;
    }
    for (unsigned endpoint = 0; endpoint < 3; ++endpoint) {
      if (stats->jobs[endpoint] > peak.jobs[endpoint]) {
        peak.jobs[endpoint] = stats->jobs[endpoint];
      }
    }
    if (stats->reports > peak.reports) {
      peak.reports = stats->reports;
    }
    reports += stats->reports;
    setup += stats->setup;
    execute += stats->execute;
  }
  printf("Hybrid block run=%u peak_profiles=%u peak_jobs=%u/%u/%u peak_pending_reports=%u reports=%u\n", run, peak.profiles, peak.jobs[0], peak.jobs[1], peak.jobs[2], peak.reports, reports);
  printf("Hybrid block run=%u setup=%lu execute=%lu total=%lu\n", run, setup, execute, total);
}

#endif
