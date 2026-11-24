#ifndef P1_PLAN_H
#define P1_PLAN_H

#include "p1_native.h"

enum { P1_SOFTMAX = 0, P1_LN1 = 3, P1_LN2 = 10, P1_QKV = 17, P1_OUTPUT = 18, P1_FC = 19, P1_PV = 20, P1_ADD = 21, P1_GELU = 22 };

typedef struct {
  const cgra_kernel_t *kernel;
  cgra_link_symbol_t symbols[8];
  unsigned next;
} p1_cgra_t;

typedef struct {
  unsigned jobs[3], stages, repeats, attention, first, paired;
  auto_link_edge_t edges[3];
} p1_profile_t;

typedef struct {
  const char *name;
  unsigned first, count;
} p1_segment_t;

static struct {
  p1_native_t native[64];
  p1_cgra_t cgra[32];
  p1_profile_t profiles[32];
  p1_segment_t segments[16];
  unsigned natives, programs, runs, operators, reports;
  int8_t *skip, *normalized, *query, *key, *value, *branch, *expanded;
  int32_t *accum;
} p1;

static int8_t p1_input[P1_WORDS] __attribute__((aligned(64)));
static int8_t p1_output[P1_WORDS] __attribute__((aligned(64)));
static const cgra_kernel_t *p1_resident;
static unsigned p1_resident_job;
static int p1_attention_resident;

static auto_link_edge_t p1_root(void) { return (auto_link_edge_t){.destination = 0, .flags = 3, .address = {.binding = -1}}; }

static unsigned p1_native(p1_native_t task) {
  const unsigned job = p1.natives++;
  p1.native[job] = task;
  return job;
}

static void p1_program(unsigned job, const cgra_kernel_t *kernel, const uint32_t args[8], unsigned next) {
  p1_cgra_t *program = &p1.cgra[job];
  program->kernel = kernel;
  program->next = next;
  for (unsigned index = 0; index < 8; ++index) {
    program->symbols[index] = (cgra_link_symbol_t){args[index], 0, CGRA_LINK_SLOT};
  }
  if (job >= p1.programs) {
    p1.programs = job + 1;
  }
}

static void p1_ln(unsigned first, unsigned affine) {
  const struct {
    const cgra_kernel_t *kernel;
    uint32_t args[8];
  } phases[] = {
      {&BLOCK_LN_SUM, {0, P1_STATS, BLOCK_HIDDEN, P1_LN_WORDS}},
      {&BLOCK_LN_MEAN, {P1_STATS, P1_STATS + 16, BLOCK_HIDDEN, P1_LN_ROWS}},
      {&BLOCK_LN_SQUARES, {0, P1_STATS + 16, P1_STATS + 32, BLOCK_HIDDEN, P1_LN_WORDS}},
      {&BLOCK_LN_VARIANCE, {P1_STATS + 32, P1_STATS + 48, BLOCK_HIDDEN, P1_LN_ROWS}},
      {&BLOCK_LN_SQRT, {P1_STATS + 48, P1_STATS + 64, P1_LN_ROWS}},
      {&BLOCK_LN_NORM, {0, affine, affine + BLOCK_HIDDEN, P1_LN_OUTPUT, P1_STATS + 16, P1_STATS + 64, BLOCK_HIDDEN, P1_LN_WORDS}},
      {&BLOCK_REQUANT, {P1_LN_OUTPUT, P1_LN_OUTPUT, 4, P1_LN_WORDS}},
  };
  for (unsigned phase = 0; phase < 7; ++phase) {
    p1_program(first + phase, phases[phase].kernel, phases[phase].args, phase == 6 ? 0 : first + phase + 2);
  }
}

static void p1_softmax(unsigned job, unsigned tile) {
  const uint32_t max[8] = {ATTENTION_MAX_ARG0, ATTENTION_MAX_ARG1, BLOCK_N, tile * P1_ROWS, P1_SCORE_WORDS};
  const uint32_t exp[8] = {ATTENTION_MAX_ARG0, ATTENTION_MAX_ARG1, ATTENTION_EXP_ARG2, ATTENTION_EXP_ARG3, BLOCK_N, tile * P1_ROWS, P1_SCORE_WORDS};
  const uint32_t norm[8] = {ATTENTION_EXP_ARG2, ATTENTION_EXP_ARG3, ATTENTION_NORM_ARG2, BLOCK_N, P1_SCORE_WORDS};
  p1_program(job, &ATTENTION_MAX, max, job + 2);
  p1_program(job + 1, &ATTENTION_EXP, exp, job + 3);
  p1_program(job + 2, &ATTENTION_NORM, norm, 0);
}

static void p1_programs(void) {
  p1_ln(P1_LN1, P1_AFFINE);
  p1_ln(P1_LN2, P1_AFFINE + 2 * BLOCK_HIDDEN);
  const unsigned divisors[] = {BLOCK_Q_DIV, BLOCK_ATTENTION_DIV, BLOCK_FC1_DIV, BLOCK_PV_DIV};
  for (unsigned index = 0; index < 4; ++index) {
    const uint32_t args[8] = {0, 1024, divisors[index], index == 3 ? P1_VECTOR_WORDS : P1_MATRIX_WORDS};
    p1_program(P1_QKV + index, &BLOCK_REQUANT, args, 0);
  }
  const uint32_t add[8] = {0, P1_VECTOR_WORDS, 2048, P1_VECTOR_WORDS}, gelu[8] = {0, 2048, P1_ACT_WORDS};
  p1_program(P1_ADD, &BLOCK_RESIDUAL, add, 0);
  p1_program(P1_GELU, &BLOCK_ACTIVATION, gelu, 0);
  p1_softmax(P1_SOFTMAX, 0);
}

static p1_profile_t *p1_profile(unsigned producer, unsigned kernel, unsigned collector, unsigned input_bytes, int packed, unsigned output_word, unsigned output_bytes, unsigned repeats) {
  p1_profile_t *profile = &p1.profiles[p1.runs++];
  *profile = (p1_profile_t){.jobs = {producer, kernel, collector}, .stages = 3, .repeats = repeats};
  profile->edges[0] = p1_root();
  profile->edges[1] = (auto_link_edge_t){
      .source = 0, .destination = 1, .flags = 5, .bytes = input_bytes, .expansion = packed ? 2 : 0, .address = {.value = GEMMINI0.spm, .binding = -1}, .source_offset = P1_PUBLICATION};
  profile->edges[2] = (auto_link_edge_t){.source = 1, .destination = 2, .flags = 5, .bytes = output_bytes, .address = {.value = CGRA0.spm, .binding = -1}, .source_offset = output_word};
  p1.reports += 3 * repeats;
  return profile;
}

static void p1_copy(const int8_t *input, int8_t *output) {
  const unsigned job = p1_native((p1_native_t){.kind = P1_COPY, .input = input, .output = output, .count = P1_COPY_WORDS});
  p1.profiles[p1.runs++] = (p1_profile_t){.jobs = {job}, .stages = 1, .repeats = P1_WORDS / P1_COPY_WORDS, .edges = {p1_root()}};
  p1.reports += P1_WORDS / P1_COPY_WORDS;
}

static void p1_vector(const void *left, const void *right, int8_t *output, unsigned count, unsigned chunk, unsigned kernel, unsigned word, int packed) {
  const unsigned gather = p1_native((p1_native_t){.kind = P1_GATHER, .input = left, .right = right, .count = chunk, .packed = packed});
  const unsigned collect = p1_native((p1_native_t){.kind = P1_COLLECT, .output = output, .source = word, .count = chunk, .stride = DIM, .step = chunk});
  p1_profile(gather, kernel, collect, chunk * (packed ? 1 : sizeof(acc_t)) * (right ? 2 : 1), packed, word, chunk, count / chunk);
}

static void p1_projection(const int8_t *input, const int8_t *weight, const int32_t *bias, int8_t *output, unsigned inner, unsigned columns, unsigned kernel) {
  for (unsigned column = 0; column < (BLOCK_N == P1_ROWS ? DIM : columns); column += DIM) {
    const unsigned matrix = p1_native((p1_native_t){.kind = P1_MATRIX, .input = input, .right = weight, .bias = bias, .inner = inner, .columns = columns, .column = column});
    const unsigned collect =
        p1_native((p1_native_t){.kind = P1_COLLECT, .output = output + column, .source = 1024, .count = P1_MATRIX_WORDS, .stride = columns, .step = BLOCK_N == P1_ROWS ? DIM : P1_ROWS * columns});
    p1_profile(matrix, kernel, collect, P1_TILE_BYTES, 0, 1024, P1_MATRIX_WORDS, BLOCK_N == P1_ROWS ? columns / DIM : BLOCK_N / P1_ROWS);
  }
}

static void p1_attention(void) {
  const unsigned load = p1_native((p1_native_t){.kind = P1_LOAD, .input = p1.query, .right = p1.key, .output = p1.value});
  const unsigned first = p1.runs;
  p1.profiles[p1.runs++] = (p1_profile_t){.jobs = {load}, .stages = 1, .repeats = 1, .attention = 1, .edges = {p1_root()}};
  for (unsigned tile = 0; tile < BLOCK_N / P1_ROWS; ++tile) {
    const unsigned score = p1_native((p1_native_t){.kind = P1_SCORES, .source = tile});
    const unsigned values = p1_native((p1_native_t){.kind = P1_VALUES, .source = tile, .output = p1.accum});
    const unsigned kernel = tile == 0 ? P1_SOFTMAX : 23 + (tile - 1) * 3;
    p1_softmax(kernel, tile);
    p1_profile_t *profile = p1_profile(score, kernel, values, P1_SCORE_WORDS * sizeof(acc_t), 0, ATTENTION_NORM_ARG2, P1_SCORE_WORDS, 1);
    profile->attention = tile + 1 == BLOCK_N / P1_ROWS ? 3 : 2;
    profile->first = first;
    profile->edges[1].destination_offset = ATTENTION_MAX_ARG0 * sizeof(acc_t);
  }
  // Each single-tile profile drains before native score/PV endpoint reentry.
  p1.reports += BLOCK_HEADS + 3 * (BLOCK_N / P1_ROWS) * (BLOCK_HEADS - 1);
}

static void p1_segment(const char *name, unsigned first) { p1.segments[p1.operators++] = (p1_segment_t){name, first, p1.runs - first}; }

static void p1_build(void) {
  memset(&p1, 0, sizeof(p1));
  p1_resident = NULL;
  p1_attention_resident = 0;
  p1.skip = (int8_t *)(GEMMINI0.spm + P1_RETAINED);
  p1.query = p1.skip + P1_WORDS;
  p1.key = p1.skip + 2 * P1_WORDS;
  p1.value = p1.skip + 3 * P1_WORDS;
  p1.accum = (int32_t *)(p1.skip + 4 * P1_WORDS);
  p1.normalized = p1.skip + 6 * P1_WORDS;
  p1.branch = p1.skip + 5 * P1_WORDS;
  p1.expanded = p1.query;
  p1_programs();
  unsigned first = p1.runs;
  p1_copy(p1_input, p1.skip);
  p1_segment("input", first);
  first = p1.runs;
  p1_vector(p1.skip, NULL, p1.normalized, P1_WORDS, P1_LN_WORDS, P1_LN1, P1_LN_OUTPUT, 1);
  p1_segment("ln1", first);
  first = p1.runs;
  p1_projection(p1.normalized, block_wq, block_bq, p1.query, BLOCK_HIDDEN, BLOCK_HIDDEN, P1_QKV);
  p1_segment("q", first);
  first = p1.runs;
  p1_projection(p1.normalized, block_wk, block_bk, p1.key, BLOCK_HIDDEN, BLOCK_HIDDEN, P1_QKV);
  p1_segment("k", first);
  first = p1.runs;
  p1_projection(p1.normalized, block_wv, block_bv, p1.value, BLOCK_HIDDEN, BLOCK_HIDDEN, P1_QKV);
  p1_segment("v", first);
  first = p1.runs;
  p1_attention();
  p1_segment("attention", first);
  first = p1.runs;
  p1_vector(p1.accum, NULL, p1.query, P1_WORDS, P1_VECTOR_WORDS, P1_PV, 1024, 0);
  p1_segment("pv", first);
  first = p1.runs;
  p1_projection(p1.query, block_wo, block_bo, p1.branch, BLOCK_HIDDEN, BLOCK_HIDDEN, P1_OUTPUT);
  p1_segment("projection", first);
  first = p1.runs;
  p1_vector(p1.skip, p1.branch, p1.skip, P1_WORDS, P1_VECTOR_WORDS, P1_ADD, 2048, 1);
  p1_segment("add1", first);
  first = p1.runs;
  p1_vector(p1.skip, NULL, p1.normalized, P1_WORDS, P1_LN_WORDS, P1_LN2, P1_LN_OUTPUT, 1);
  p1_segment("ln2", first);
  first = p1.runs;
  p1_projection(p1.normalized, block_w1, block_b1, p1.expanded, BLOCK_HIDDEN, BLOCK_FF, P1_FC);
  p1_segment("fc1", first);
  first = p1.runs;
  p1_vector(p1.expanded, NULL, p1.expanded, P1_EXPANDED, P1_ACT_WORDS, P1_GELU, 2048, 1);
  p1_segment("gelu", first);
  first = p1.runs;
  p1_projection(p1.expanded, block_w2, block_b2, p1.branch, BLOCK_FF, BLOCK_HIDDEN, P1_FC);
  p1_segment("fc2", first);
  first = p1.runs;
  p1_vector(p1.skip, p1.branch, p1.branch, P1_WORDS, P1_VECTOR_WORDS, P1_ADD, 2048, 1);
  p1_segment("add2", first);
  first = p1.runs;
  p1_copy(p1.branch, p1_output);
  p1_segment("output", first);
}

static void p1_pairs(void) {
  for (unsigned index = 0; index < p1.runs; ++index) {
    p1_profile_t *profile = &p1.profiles[index];
    p1_native_t *producer = &p1.native[profile->jobs[0]];
    if (producer->kind != P1_MATRIX) {
      continue;
    }
    // Exactly two roots exist before this profile drains both publications.
    profile->paired = 1;
    producer->paired = 1;
    p1.native[profile->jobs[2]].paired = 1;
    profile->edges[1].source_stride = p1_slot_bytes();
    profile->repeats /= 2;
    p1.reports -= 3 * profile->repeats;
  }
}

static int p1_dma(const void *source, unsigned word, unsigned elements, int packed) {
  const cgra_dma_desc_t descriptor = ((uint64_t)word << CGRA_DMA_DESC_SPM_ADDR_LSB) | ((uint64_t)elements * sizeof(acc_t) << CGRA_DMA_DESC_NBYTES_LSB);
  uint8_t tag;
  cgra_dma_memory_fence();
  accel_commands(CGRA0, {
    if (packed) {
      cgra_dma_mvin_i8_async(source, descriptor);
    } else {
      cgra_dma_mvin_async(source, descriptor);
    }
    tag = cgra_dma_wait(0);
  });
  return tag != 0;
}

static int p1_affine(void) {
  return p1_dma(block_gamma1, P1_AFFINE, BLOCK_HIDDEN, 0) || p1_dma(block_beta1, P1_AFFINE + BLOCK_HIDDEN, BLOCK_HIDDEN, 0) || p1_dma(block_gamma2, P1_AFFINE + 2 * BLOCK_HIDDEN, BLOCK_HIDDEN, 0) ||
         p1_dma(block_beta2, P1_AFFINE + 3 * BLOCK_HIDDEN, BLOCK_HIDDEN, 0);
}

static int p1_capture_profiles(void) {
  for (unsigned index = 0; index < p1.runs; ++index) {
    const p1_profile_t *profile = &p1.profiles[index];
    auto_link_stage_t stages[3];
    for (unsigned stage = 0; stage < profile->stages; ++stage) {
      stages[stage] = (auto_link_stage_t){.endpoint = stage == 1 ? CGRA0.id : GEMMINI0.id, .job = profile->jobs[stage], .output = {.address = {.binding = -1}}};
    }
    const auto_link_graph_t graph = {.stage_count = profile->stages, .edge_count = profile->stages, .stage_capacity = 3, .edge_capacity = 3, .stages = stages, .edges = profile->edges};
    auto_link_load(&graph, NULL);
    auto_link_tiles(profile->paired ? 2 : 1, 1, 1, 1);
    if (profile->attention == 1 || profile->attention == 2) {
      auto_link_run_config(index);
    } else {
      auto_link_run_repeat(index, profile->attention == 3 ? profile->first : index, profile->attention == 3 ? BLOCK_HEADS : profile->repeats);
    }
  }
  return 0;
}

static int p1_capture_all(void) {
  for (unsigned job = 0; job < p1.natives; ++job) {
    if (p1_capture(job, &p1.native[job])) {
      printf("P1 GPT2 capture failed native job=%u kind=%u\n", job, p1.native[job].kind);
      return 1;
    }
  }
  for (unsigned job = 0; job < p1.programs; ++job) {
    const p1_cgra_t *program = &p1.cgra[job];
    int failed;
    accel_commands(CGRA0, { failed = cgra_job_full_at(CGRA0.control, job, program->kernel, program->symbols, program->next); });
    if (failed) {
      printf("P1 GPT2 capture failed CGRA job=%u\n", job);
      return 1;
    }
  }
  return p1_capture_profiles();
}

static int p1_cgra_phase(unsigned job, unsigned tile) {
  const p1_cgra_t *program = &p1.cgra[job];
  cgra_kernel_t kernel = *program->kernel;
  cgra_packet_t config[kernel.config_count];
  memcpy(config, kernel.config_packets, sizeof(config));
  for (unsigned index = 0; index < kernel.patch_count; ++index) {
    const cgra_patch_t *patch = &kernel.patches[index];
    const cgra_link_symbol_t *symbol = &program->symbols[patch->symbol_index];
    const uint32_t source = symbol->source == CGRA_LINK_ROW ? tile * P1_ROWS : 0;
    const uint32_t value = (symbol->base + symbol->stride * source) * patch->scale + patch->offset;
    cgra_packet_t *packet = &config[patch->packet_index];
    packet->hi = (packet->hi & ~BLOCK_REQUANT_STORE_DATA_PAYLOAD_HI(UINT32_MAX)) | BLOCK_REQUANT_STORE_DATA_PAYLOAD_HI(value);
  }
  kernel.config_packets = config;
  uint64_t ready, status, result;
  const int attention = job < 3 || job >= 23;
  accel_commands(CGRA0, {
    if (attention && p1_attention_resident) {
      cgra_prepare(&kernel, CGRA_SWITCH);
    } else if (p1_resident == program->kernel) {
      cgra_prepare(&kernel, p1_resident_job == job ? CGRA_REPEAT : CGRA_SWITCH);
    } else {
      cgra_config(&kernel, CGRA_SWITCH);
    }
    cgra_start(&kernel);
    CGRA_WAIT(ready);
    CGRA_STATUS(status);
    CGRA_RESULT(result);
  });
  p1_resident = program->kernel;
  p1_resident_job = job;
  if (!attention) {
    p1_attention_resident = 0;
  }
  if (ready != 1 || !(status & 1) || ((status >> 1) & 0xffff) != kernel.expected_completes || result) {
    printf("P1 GPT2 CGRA job=%u ready=%lu status=%lu result=%lu\n", job, ready, status, result);
    return 1;
  }
  return 0;
}

static int p1_cgra_run(unsigned job, unsigned tile) {
  for (;;) {
    if (p1_cgra_phase(job, tile)) {
      return 1;
    }
    if (!p1.cgra[job].next) {
      return 0;
    }
    job = p1.cgra[job].next - 1;
  }
}

static int p1_manual_profile(const p1_profile_t *profile, unsigned iteration, unsigned tile) {
  accel_commands(GEMMINI0, {
    p1_emit(&p1.native[profile->jobs[0]], iteration, tile);
    gemmini_fence();
  });
  if (profile->stages == 1) {
    return 0;
  }
  const auto_link_edge_t *edge = &profile->edges[1];
  if (p1_dma((const void *)(edge->address.value + edge->source_offset), edge->destination_offset / sizeof(acc_t), edge->bytes / (edge->expansion ? 1 : sizeof(acc_t)), edge->expansion != 0) ||
      p1_cgra_run(profile->jobs[1], tile)) {
    return 1;
  }
  accel_commands(GEMMINI0, {
    p1_emit(&p1.native[profile->jobs[2]], iteration, tile);
    gemmini_fence();
  });
  return 0;
}

static int p1_manual(void) {
  for (unsigned index = 0; index < p1.runs; ++index) {
    const p1_profile_t *profile = &p1.profiles[index];
    if (profile->attention == 1) {
      accel_commands(CGRA0, {
        for (unsigned phase = 0; phase < 3; ++phase) {
          const cgra_kernel_t *kernel = p1.cgra[phase].kernel;
          cgra_send_packets_fast(kernel->static_packets, kernel->static_count);
        }
      });
      p1_attention_resident = 1;
      for (unsigned head = 0; head < BLOCK_HEADS; ++head) {
        if (p1_manual_profile(profile, head, 0)) {
          return 1;
        }
        for (unsigned tile = 0; tile < BLOCK_N / P1_ROWS; ++tile) {
          if (p1_manual_profile(&p1.profiles[index + tile + 1], head, tile)) {
            return 1;
          }
        }
      }
      index += BLOCK_N / P1_ROWS;
      continue;
    }
    for (unsigned iteration = 0; iteration < profile->repeats; ++iteration) {
      if (p1_manual_profile(profile, iteration, 0)) {
        return 1;
      }
    }
  }
  return 0;
}

static int p1_run(unsigned first, unsigned count) {
  unsigned pending[3][64] = {{0}}, expected = 0;
  for (unsigned index = first; index < first + count; ++index) {
    const p1_profile_t *profile = &p1.profiles[index];
    const unsigned repeats = profile->attention ? BLOCK_HEADS : profile->repeats;
    for (unsigned stage = 0; stage < profile->stages; ++stage) {
      pending[stage][profile->jobs[stage]] += repeats;
      expected += repeats;
    }
  }
  auto_link_run(first, count);
  unsigned reports = 0, failures = 0;
  for (;;) {
    const unsigned running = *(volatile uint32_t *)(AUTO_LINK_BASE + AUTO_LINK_RUNNING);
    if (!cgra_link_read_at(CGRA0.control, CGRA_LINK_CONTROL_RESULT_VALID)) {
      if (!running) {
        break;
      }
      continue;
    }
    const cgra_link_result_t result = cgra_link_wait_at(CGRA0.control);
    if (result.status || result.detail || result.data || result.stage >= 3 || result.job >= 64 || !pending[result.stage][result.job]) {
      printf("P1 GPT2 report stage=%u job=%u status=%u detail=%u data=%u\n", result.stage, result.job, result.status, result.detail, result.data);
      ++failures;
    } else {
      --pending[result.stage][result.job];
    }
    ++reports;
  }
  gemmini_fence();
  if (reports != expected) {
    printf("P1 GPT2 reports=%u expected=%u first=%u count=%u\n", reports, expected, first, count);
    ++failures;
  }
  return failures;
}

static int p1_hybrid(void) {
  for (unsigned index = 0; index < p1.operators; ++index) {
    const p1_segment_t *segment = &p1.segments[index];
    if (p1_run(segment->first, segment->count)) {
      printf("P1 GPT2 failed operator=%s\n", segment->name);
      return 1;
    }
  }
  return 0;
}

#endif
