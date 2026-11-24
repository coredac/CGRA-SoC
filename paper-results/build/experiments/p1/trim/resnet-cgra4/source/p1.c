#include "accel_generated.h"
#include "auto_link.h"
#include "cgra_dma.h"
#include "cgra_link.h"
#include "gemmini_conv.h"
#include "generated/cgra_add_relu_runtime_fast_api.h"
#include "generated/cgra_relu_runtime_fast_api.h"
#include "generated/p1_resnet_data.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef P1_CGRAS
#define P1_CGRAS 1
#endif

#if DIM != 64
#error "P1 requires the DIM64 Gemmini configuration"
#endif
#if P1_CGRAS != 1 && P1_CGRAS != 2 && P1_CGRAS != 4
#error "P1 requires one, two or four homogeneous CGRAs"
#endif

static const accel_t devices[] = {
    CGRA0,
#if P1_CGRAS >= 2
    CGRA1,
#endif
#if P1_CGRAS == 4
    CGRA2,
    CGRA3,
#endif
};

enum {
  STRIP_ROWS = 2,
  STRIPS = (HEIGHT + STRIP_ROWS - 1) / STRIP_ROWS,
  LANES = STRIPS < P1_CGRAS ? STRIPS : P1_CGRAS,
  GROUPS = (STRIPS + P1_CGRAS - 1) / P1_CGRAS,
  HALO_WORDS = (STRIP_ROWS + 2) * WIDTH * CHANNELS,
  CORE_WORDS = STRIP_ROWS * WIDTH * CHANNELS,
  SKIP_WORD = HALO_WORDS,
  MAIN_WORD = SKIP_WORD + CORE_WORDS,
  OUTPUT_WORD = MAIN_WORD + CORE_WORDS,
  LIVE_WORDS = OUTPUT_WORD + GROUPS * CORE_WORDS,
  PUBLICATION_BYTES = HALO_WORDS + 2 * CORE_WORDS,
};

enum { FIRST = 0, PROJECTION, ACTIVATE, SECOND, ADD, STAGES };
enum { ROOT = 3, DATA = 5, INT8_EXPANSION = 2, EDGES = 6, AUTO_EDGES = 7 };

typedef struct {
  gemmini_conv_t first, projection, second;
  unsigned elements, halo_elements;
  uint32_t relu[RELU_RUNTIME_SYMBOL_COUNT];
  uint32_t add[ADD_RELU_RUNTIME_SYMBOL_COUNT];
} strip_t;

static strip_t strips[STRIPS];
static int32_t output[HEIGHT * WIDTH * CHANNELS] __attribute__((aligned(64)));

static unsigned long cycles(void) {
  unsigned long value;
  __asm__ volatile("rdcycle %0" : "=r"(value) : : "memory");
  return value;
}

static int minimum(int a, int b) { return a < b ? a : b; }

static int maximum(int a, int b) { return a > b ? a : b; }

/* Each lane keeps three publications in the upper half; operands stay in DRAM. */
static uintptr_t first_address(unsigned lane) { return GEMMINI0.spm + GEMMINI0.spm_bytes / 2 + lane * PUBLICATION_BYTES; }

static uintptr_t skip_address(unsigned lane) { return first_address(lane) + HALO_WORDS; }

static uintptr_t second_address(unsigned lane) { return skip_address(lane) + CORE_WORDS; }

static gemmini_conv_t convolution(int row, int rows, int source_row, int source_rows, int source_columns, int inputs, int kernel, int stride, int padding, const elem_t *source, const elem_t *weights,
                                  uintptr_t destination) {
  const int first = row * stride - padding;
  const int begin = maximum(first, 0);
  const int end = minimum((row + rows) * stride + kernel - 1 - padding, source_rows);
  return (gemmini_conv_t){.input_rows = end - begin,
                          .input_columns = source_columns,
                          .input_channels = inputs,
                          .output_rows = rows,
                          .output_columns = WIDTH,
                          .output_channels = CHANNELS,
                          .kernel = kernel,
                          .stride = stride,
                          .dilation = 1,
                          .padding_top = maximum(-first, 0),
                          .padding_left = padding,
                          .input = source + (begin - source_row) * source_columns * inputs,
                          .weight = weights,
                          .output = (elem_t *)destination,
                          .activation = NO_ACTIVATION,
                          .scale = ACC_SCALE_IDENTITY};
}

static void plan(void) {
  for (unsigned index = 0; index < STRIPS; ++index) {
    const int row = index * STRIP_ROWS;
    const int rows = minimum(STRIP_ROWS, HEIGHT - row);
    const int halo_row = maximum(row - PADDING, 0);
    const int halo_end = minimum(row + rows + PADDING, HEIGHT);
    const unsigned lane = index % P1_CGRAS;
    strip_t *strip = &strips[index];
    strip->elements = rows * WIDTH * CHANNELS;
    strip->halo_elements = (halo_end - halo_row) * WIDTH * CHANNELS;
    strip->first = convolution(halo_row, halo_end - halo_row, 0, INPUT_H, INPUT_W, INPUT_CHANNELS, KERNEL, STRIDE, PADDING, p1_input, p1_weights1, first_address(lane));
    strip->projection = convolution(row, rows, 0, INPUT_H, INPUT_W, INPUT_CHANNELS, 1, STRIDE, 0, p1_input, p1_projection, skip_address(lane));
    strip->second = convolution(row, rows, halo_row, HEIGHT, WIDTH, CHANNELS, KERNEL, 1, PADDING, (const elem_t *)devices[lane].spm, p1_weights2, second_address(lane));
    strip->relu[RELU_RUNTIME_SYMBOL_INPUT0] = 0;
    strip->relu[RELU_RUNTIME_SYMBOL_OUTPUT] = 0;
    strip->relu[RELU_RUNTIME_SYMBOL_ELEMENTS] = strip->halo_elements;
    strip->add[ADD_RELU_RUNTIME_SYMBOL_INPUT0] = MAIN_WORD;
    strip->add[ADD_RELU_RUNTIME_SYMBOL_INPUT1] = SKIP_WORD;
    strip->add[ADD_RELU_RUNTIME_SYMBOL_OUTPUT] = OUTPUT_WORD + index / P1_CGRAS * CORE_WORDS;
    strip->add[ADD_RELU_RUNTIME_SYMBOL_ELEMENTS] = strip->elements;
  }
}

static cgra_kernel_t resolve(const cgra_kernel_t *original, const uint32_t *symbols, cgra_packet_t *packets) {
  cgra_kernel_t kernel = *original;
  memcpy(packets, kernel.config_packets, kernel.config_count * sizeof(*packets));
  for (unsigned index = 0; index < kernel.patch_count; ++index) {
    const cgra_patch_t *patch = &kernel.patches[index];
    const uint32_t value = symbols[patch->symbol_index] * patch->scale + patch->offset;
    cgra_packet_t *packet = &packets[patch->packet_index];
    packet->hi = (packet->hi & ~RELU_RUNTIME_STORE_DATA_PAYLOAD_HI(UINT32_MAX)) | RELU_RUNTIME_STORE_DATA_PAYLOAD_HI(value);
  }
  kernel.static_count = 0;
  kernel.config_packets = packets;
  return kernel;
}

static int dma(accel_t device, uintptr_t source, unsigned word, unsigned elements) {
  const cgra_dma_desc_t descriptor = ((uint64_t)word << CGRA_DMA_DESC_SPM_ADDR_LSB) | ((uint64_t)elements * sizeof(acc_t) << CGRA_DMA_DESC_NBYTES_LSB);
  uint8_t tag;
  accel_commands(device, {
    cgra_dma_mvin_i8_async((const void *)source, descriptor);
    tag = cgra_dma_wait(0);
  });
  return tag != 0;
}

static void native_conv(const gemmini_conv_t *params) {
  accel_commands(GEMMINI0, {
    gemmini_conv_emit(params);
    gemmini_fence();
  });
}

static void native_cgra(accel_t device, const cgra_kernel_t *original, const uint32_t *symbols) {
  cgra_packet_t packets[ADD_RELU_RUNTIME_FAST_CONFIG_PACKET_COUNT];
  const cgra_kernel_t kernel = resolve(original, symbols, packets);
  accel_commands(device, {
    cgra_prepare(&kernel, CGRA_SWITCH);
    cgra_start(&kernel);
  });
}

static int completion(accel_t device, const cgra_kernel_t *kernel) {
  uint64_t ready, status, result;
  accel_commands(device, {
    CGRA_WAIT(ready);
    CGRA_STATUS(status);
    CGRA_RESULT(result);
  });
  if (ready == 1 && (status & 1) && ((status >> 1) & 0xffff) == kernel->expected_completes && result == 0) {
    return 0;
  }
  printf("P1 ResNet completion ready=%lu status=%lu result=%lu\n", ready, status, result);
  return 1;
}

static int manual(void) {
  for (unsigned first = 0; first < STRIPS; first += P1_CGRAS) {
    const unsigned count = minimum(P1_CGRAS, STRIPS - first);
    for (unsigned lane = 0; lane < count; ++lane) {
      const strip_t *strip = &strips[first + lane];
      native_conv(&strip->first);
      native_conv(&strip->projection);
      if (dma(devices[lane], first_address(lane), 0, strip->halo_elements)) {
        return 1;
      }
      native_cgra(devices[lane], &RELU_RUNTIME, strip->relu);
    }
    for (unsigned lane = 0; lane < count; ++lane) {
      const strip_t *strip = &strips[first + lane];
      if (completion(devices[lane], &RELU_RUNTIME)) {
        return 1;
      }
      native_conv(&strip->second);
      if (dma(devices[lane], skip_address(lane), SKIP_WORD, strip->elements) || dma(devices[lane], second_address(lane), MAIN_WORD, strip->elements)) {
        return 1;
      }
      native_cgra(devices[lane], &ADD_RELU_RUNTIME, strip->add);
    }
    for (unsigned lane = 0; lane < count; ++lane) {
      if (completion(devices[lane], &ADD_RELU_RUNTIME)) {
        return 1;
      }
    }
  }
  return 0;
}

static int capture_jobs(void) {
  for (unsigned index = 0; index < STRIPS; ++index) {
    const strip_t *strip = &strips[index];
    const accel_t device = devices[index % P1_CGRAS];
    const gemmini_conv_t *convs[] = {&strip->first, &strip->projection, &strip->second};
    for (unsigned job = 0; job < 3; ++job) {
      int failed;
      accel_commands(GEMMINI0, { failed = gemmini_conv_capture(GEMMINI0.control, 3 * index + job, convs[job]); });
      if (failed) {
        return 1;
      }
    }
    const cgra_kernel_t *kernels[] = {&RELU_RUNTIME, &ADD_RELU_RUNTIME};
    const uint32_t *symbols[] = {strip->relu, strip->add};
    for (unsigned job = 0; job < 2; ++job) {
      cgra_packet_t packets[ADD_RELU_RUNTIME_FAST_CONFIG_PACKET_COUNT];
      const cgra_kernel_t kernel = resolve(kernels[job], symbols[job], packets);
      int failed;
      accel_commands(device, { failed = cgra_job_config_at(device.control, 2 * (index / P1_CGRAS) + job, &kernel, NULL); });
      if (failed) {
        return 1;
      }
    }
  }
  return 0;
}

static auto_link_output_t publication(uintptr_t address, unsigned elements) { return (auto_link_output_t){.flags = 1, .address = {.value = address, .binding = -1}, .bytes = elements}; }

static auto_link_edge_t edge(unsigned source, unsigned destination, uintptr_t address, unsigned word, unsigned elements, unsigned expansion) {
  return (auto_link_edge_t){
      .source = source, .destination = destination, .flags = DATA, .bytes = elements, .expansion = expansion, .address = {.value = address, .binding = -1}, .destination_offset = word * sizeof(acc_t)};
}

static void capture_profiles(void) {
  for (unsigned group = 0; group < GROUPS; ++group) {
    const unsigned count = minimum(P1_CGRAS, STRIPS - group * P1_CGRAS);
    auto_link_stage_t stages[STAGES * LANES];
    auto_link_edge_t edges[EDGES * LANES];
    for (unsigned lane = 0; lane < count; ++lane) {
      const unsigned index = group * P1_CGRAS + lane;
      const unsigned base = lane * STAGES;
      const strip_t *strip = &strips[index];
      stages[base + FIRST] = (auto_link_stage_t){.endpoint = GEMMINI0.id, .job = 3 * index, .output = publication(first_address(lane), strip->halo_elements)};
      stages[base + PROJECTION] = (auto_link_stage_t){.endpoint = GEMMINI0.id, .job = 3 * index + 1, .output = publication(skip_address(lane), strip->elements)};
      stages[base + ACTIVATE] = (auto_link_stage_t){.endpoint = devices[lane].id, .job = 2 * group, .output = {.address = {.binding = -1}}};
      stages[base + SECOND] = (auto_link_stage_t){.endpoint = GEMMINI0.id, .job = 3 * index + 2, .output = publication(second_address(lane), strip->elements)};
      stages[base + ADD] = (auto_link_stage_t){.endpoint = devices[lane].id, .job = 2 * group + 1, .output = {.address = {.binding = -1}}};
      auto_link_edge_t *deps = &edges[lane * EDGES];
      deps[0] = (auto_link_edge_t){.destination = base + FIRST, .flags = ROOT, .address = {.binding = -1}};
      deps[1] = (auto_link_edge_t){.destination = base + PROJECTION, .flags = ROOT, .address = {.binding = -1}};
      deps[2] = edge(base + FIRST, base + ACTIVATE, first_address(lane), 0, strip->halo_elements, INT8_EXPANSION);
      deps[3] = edge(base + ACTIVATE, base + SECOND, devices[lane].spm, 0, strip->halo_elements, 0);
      deps[4] = edge(base + PROJECTION, base + ADD, skip_address(lane), SKIP_WORD, strip->elements, INT8_EXPANSION);
      deps[5] = edge(base + SECOND, base + ADD, second_address(lane), MAIN_WORD, strip->elements, INT8_EXPANSION);
    }
    const auto_link_graph_t graph = {
        .stage_count = STAGES * count, .edge_count = EDGES * count, .stage_capacity = AUTO_LINK_GRAPH.stage_capacity, .edge_capacity = AUTO_LINK_GRAPH.edge_capacity, .stages = stages, .edges = edges};
    auto_link_load(&graph, NULL);
    auto_link_tiles(1, 1, 1, 1);
    auto_link_run_config(group);
  }
}

static void capture_auto_profiles(void) {
  for (unsigned group = 0; group <= GROUPS; ++group) {
    const unsigned count = group < GROUPS ? minimum(P1_CGRAS, STRIPS - group * P1_CGRAS) : 0;
    const unsigned previous = group ? minimum(P1_CGRAS, STRIPS - (group - 1) * P1_CGRAS) : 0;
    const unsigned cores = ADD * count;
    auto_link_stage_t stages[STAGES * LANES];
    auto_link_edge_t edges[AUTO_EDGES * LANES];
    for (unsigned lane = 0; lane < count; ++lane) {
      const unsigned index = group * P1_CGRAS + lane;
      const unsigned base = lane * ADD;
      const strip_t *strip = &strips[index];
      stages[base + FIRST] = (auto_link_stage_t){.endpoint = GEMMINI0.id, .job = 3 * index, .output = publication(first_address(lane), strip->halo_elements)};
      stages[base + PROJECTION] = (auto_link_stage_t){.endpoint = GEMMINI0.id, .job = 3 * index + 1, .output = publication(skip_address(lane), strip->elements)};
      stages[base + ACTIVATE] = (auto_link_stage_t){.endpoint = devices[lane].id, .job = 2 * group, .output = {.address = {.binding = -1}}};
      stages[base + SECOND] = (auto_link_stage_t){.endpoint = GEMMINI0.id, .job = 3 * index + 2, .output = publication(second_address(lane), strip->elements)};
      auto_link_edge_t *deps = &edges[lane * ADD];
      deps[0] = (auto_link_edge_t){.destination = base + FIRST, .flags = ROOT, .address = {.binding = -1}};
      // The previous Add owns the skip publication until its input and compute drain.
      deps[1] = (auto_link_edge_t){.source = cores + lane, .destination = base + PROJECTION, .flags = group ? 1 : ROOT, .address = {.binding = -1}};
      deps[2] = edge(base + FIRST, base + ACTIVATE, first_address(lane), 0, strip->halo_elements, INT8_EXPANSION);
      deps[3] = edge(base + ACTIVATE, base + SECOND, devices[lane].spm, 0, strip->halo_elements, 0);
    }
    for (unsigned lane = 0; lane < previous; ++lane) {
      const unsigned index = (group - 1) * P1_CGRAS + lane;
      const strip_t *strip = &strips[index];
      const unsigned stage = cores + lane;
      stages[stage] = (auto_link_stage_t){.endpoint = devices[lane].id, .job = 2 * (group - 1) + 1, .output = {.address = {.binding = -1}}};
      auto_link_edge_t *deps = &edges[cores + 2 * lane];
      deps[0] = edge(0, stage, skip_address(lane), SKIP_WORD, strip->elements, INT8_EXPANSION);
      deps[1] = edge(0, stage, second_address(lane), MAIN_WORD, strip->elements, INT8_EXPANSION);
      deps[0].flags |= 2;
      deps[1].flags |= 2;
    }
    if (group && count) {
      for (unsigned lane = 0; lane < count; ++lane) {
        // Current Second must not overwrite the previous main publication before Add consumes it.
        edges[cores + 2 * previous + lane] = (auto_link_edge_t){.source = cores + lane, .destination = lane * ADD + SECOND, .flags = 1, .address = {.binding = -1}};
      }
    }
    const auto_link_graph_t graph = {.stage_count = cores + previous,
                                     .edge_count = cores + 2 * previous + (group ? count : 0),
                                     .stage_capacity = AUTO_LINK_GRAPH.stage_capacity,
                                     .edge_capacity = AUTO_LINK_GRAPH.edge_capacity,
                                     .stages = stages,
                                     .edges = edges};
    auto_link_load(&graph, NULL);
    auto_link_tiles(1, 1, 1, 1);
    auto_link_run_config(group);
  }
}

static int execute(unsigned first, unsigned count) {
  unsigned reports = 0, failed = 0;
  unsigned seen[STRIPS] = {0};
  static const unsigned jobs[STAGES] = {0, 1, 0, 2, 1};
  cgra_dma_memory_fence();
  auto_link_run(first, count);
  for (;;) {
    const unsigned running = *(volatile uint32_t *)(AUTO_LINK_BASE + AUTO_LINK_RUNNING);
    if (!cgra_link_read_at(CGRA0.control, CGRA_LINK_CONTROL_RESULT_VALID)) {
      if (!running) {
        break;
      }
      continue;
    }
    const cgra_link_result_t result = cgra_link_wait_at(CGRA0.control);
    const unsigned stage = result.stage % STAGES;
    const unsigned lane = result.stage / STAGES;
    const int cgra = stage == ACTIVATE || stage == ADD;
    const unsigned group = cgra ? result.job / 2 : result.job / (3 * P1_CGRAS);
    const unsigned index = group * P1_CGRAS + lane;
    const unsigned job = cgra ? 2 * group + jobs[stage] : 3 * index + jobs[stage];
    const int valid = group >= first && group < first + count && lane < P1_CGRAS && index < STRIPS;
    if (!valid || result.status != AUTO_LINK_STATUS_SUCCESS || result.detail || result.data || result.job != job || (seen[index] & (1u << stage))) {
      printf("P1 ResNet report stage=%u job=%u status=%u detail=%u data=%u\n", result.stage, result.job, result.status, result.detail, result.data);
      failed = 1;
    }
    if (valid) {
      seen[index] |= 1u << stage;
    }
    ++reports;
  }
  cgra_dma_memory_fence();
  const unsigned strip_count = minimum(STRIPS, (first + count) * P1_CGRAS) - first * P1_CGRAS;
  return failed || reports != STAGES * strip_count;
}

static int automatic(void) {
  unsigned reports = 0, failed = 0, profile = 0, collected = 0;
  unsigned seen[STRIPS] = {0};
  static const unsigned jobs[STAGES] = {0, 1, 0, 2, 1};
  cgra_dma_memory_fence();
  auto_link_run(0, GROUPS + 1);
  for (;;) {
    const unsigned running = *(volatile uint32_t *)(AUTO_LINK_BASE + AUTO_LINK_RUNNING);
    if (!cgra_link_read_at(CGRA0.control, CGRA_LINK_CONTROL_RESULT_VALID)) {
      if (!running) {
        break;
      }
      continue;
    }
    const cgra_link_result_t result = cgra_link_wait_at(CGRA0.control);
    const unsigned count = profile < GROUPS ? minimum(P1_CGRAS, STRIPS - profile * P1_CGRAS) : 0;
    const unsigned previous = profile ? minimum(P1_CGRAS, STRIPS - (profile - 1) * P1_CGRAS) : 0;
    const unsigned cores = ADD * count;
    const int core = result.stage < cores;
    const unsigned stage = core ? result.stage % ADD : ADD;
    const unsigned lane = core ? result.stage / ADD : result.stage - cores;
    const unsigned group = core ? profile : profile - 1;
    const unsigned index = group * P1_CGRAS + lane;
    const int cgra = stage == ACTIVATE || stage == ADD;
    const unsigned job = cgra ? 2 * group + jobs[stage] : 3 * index + jobs[stage];
    const int valid = profile <= GROUPS && result.stage < cores + previous && lane < P1_CGRAS && index < STRIPS;
    if (!valid || result.status != AUTO_LINK_STATUS_SUCCESS || result.detail || result.data || result.job != job || (seen[index] & (1u << stage))) {
      printf("P1 ResNet Auto report profile=%u stage=%u job=%u status=%u detail=%u data=%u\n", profile, result.stage, result.job, result.status, result.detail, result.data);
      failed = 1;
    }
    if (valid) {
      seen[index] |= 1u << stage;
    }
    ++reports;
    // Profiles drain before the sequencer advances, including their stage reports.
    if (++collected == cores + previous) {
      ++profile;
      collected = 0;
    }
  }
  cgra_dma_memory_fence();
  return failed || reports != STAGES * STRIPS || profile != GROUPS + 1;
}

static int output_dma(int read) {
  for (unsigned index = 0; index < STRIPS; ++index) {
    const accel_t device = devices[index % P1_CGRAS];
    const unsigned word = OUTPUT_WORD + index / P1_CGRAS * CORE_WORDS;
    const cgra_dma_desc_t descriptor = ((uint64_t)word << CGRA_DMA_DESC_SPM_ADDR_LSB) | ((uint64_t)strips[index].elements * sizeof(acc_t) << CGRA_DMA_DESC_NBYTES_LSB);
    uint8_t tag;
    accel_commands(device, {
      if (read) {
        cgra_dma_mvout_async(output + index * CORE_WORDS, descriptor);
      } else {
        cgra_dma_mvin_async(output + index * CORE_WORDS, descriptor);
      }
      tag = cgra_dma_wait(0);
    });
    if (tag) {
      return 1;
    }
  }
  cgra_dma_memory_fence();
  return 0;
}

static unsigned check(void) {
  unsigned errors = 0;
  for (unsigned index = 0; index < HEIGHT * WIDTH * CHANNELS; ++index) {
    if (output[index] != p1_expected[index]) {
      if (errors < 4) {
        printf("P1 ResNet mismatch index=%u actual=%d expected=%d\n", index, output[index], p1_expected[index]);
      }
      ++errors;
    }
  }
  return errors;
}

int main(int argc, char **argv) {
  const int selected = argc > 1 ? atoi(argv[1]) : -1;
  const unsigned repeats = argc > 2 ? atoi(argv[2]) : 2;
  const unsigned timing_only = argc > 3 ? atoi(argv[3]) : 0;
  if (selected < -1 || selected > 2 || repeats == 0 || timing_only > 1) {
    printf("P1 ResNet arguments: mode=-1(all)|0(Manual)|1(Hybrid)|2(Auto) warm_repeats>0 timing_only=0|1\n");
    return 1;
  }
  plan();
  printf("P1 ResNet shape=%ux%ux%u->%ux%ux%u strips=%u cgras=%u groups=%u gemmini_jobs=%u cgra_jobs_per_device=%u stages=%u edges=%u live_words_per_device=%u publication_bytes=%u warm_repeats=%u\n",
         INPUT_H, INPUT_W, INPUT_CHANNELS, HEIGHT, WIDTH, CHANNELS, STRIPS, P1_CGRAS, GROUPS, 3 * STRIPS, 2 * GROUPS, STAGES * LANES, EDGES * LANES, LIVE_WORDS, LANES * PUBLICATION_BYTES, repeats);
  printf("P1 ResNet Auto schedule=rotated_add profiles=%u stage_bound=%u edge_bound=%u\n", GROUPS + 1, STAGES * LANES, AUTO_EDGES * LANES);
  const char *modes[] = {"Manual", "Hybrid", "Auto"};
  const unsigned first = selected < 0 ? 0 : selected;
  const unsigned end = selected < 0 ? 3 : first + 1;
  for (unsigned mode = first; mode < end; ++mode) {
    for (unsigned repeat = 0; repeat <= repeats; ++repeat) {
      const int cold = repeat == 0;
      if (!timing_only) {
        for (unsigned index = 0; index < HEIGHT * WIDTH * CHANNELS; ++index) {
          output[index] = INT32_MIN;
        }
        cgra_dma_memory_fence();
        if (output_dma(0)) {
          printf("P1 ResNet: FAIL output initialization\n");
          return 1;
        }
      }
      const unsigned long begin = cycles();
      if (cold) {
        for (unsigned lane = 0; lane < LANES; ++lane) {
          accel_commands(devices[lane], {
            cgra_send_packets_fast(RELU_RUNTIME.static_packets, RELU_RUNTIME.static_count);
            cgra_send_packets_fast(ADD_RELU_RUNTIME.static_packets, ADD_RELU_RUNTIME.static_count);
          });
        }
      }
      const unsigned long capture_begin = cycles();
      int failed = 0;
      if (cold && mode) {
        failed = capture_jobs();
        if (!failed) {
          if (mode == 1) {
            capture_profiles();
          } else {
            capture_auto_profiles();
          }
        }
      }
      const unsigned long execute_begin = cycles();
      if (!failed && mode == 0) {
        failed = manual();
      }
      if (!failed && mode == 1) {
        for (unsigned index = 0; !failed && index < GROUPS; ++index) {
          failed = execute(index, 1);
        }
      }
      if (!failed && mode == 2) {
        failed = automatic();
      }
      const unsigned long writeback_begin = cycles();
      if (!failed) {
        failed = output_dma(1);
      }
      const unsigned long elapsed = cycles() - begin;
      const unsigned errors = failed || timing_only ? 0 : check();
      printf("P1 ResNet mode=%s phase=%s cgras=%u repeat=%u cycles=%lu checked=%u mismatches=%u status=%u captures=%u\n", modes[mode], cold ? "cold" : "warm", P1_CGRAS, repeat, elapsed, !timing_only,
             errors, failed, cold && mode != 0);
      printf("P1 ResNet setup_cycles=%lu capture_cycles=%lu execute_cycles=%lu writeback_cycles=%lu total_cycles=%lu\n", execute_begin - begin, cold && mode ? execute_begin - capture_begin : 0,
             writeback_begin - execute_begin, elapsed - (writeback_begin - begin), elapsed);
      if (failed || errors) {
        printf("P1 ResNet: FAIL\n");
        return 1;
      }
    }
  }
  printf("P1 ResNet: PASS\n");
  return 0;
}
