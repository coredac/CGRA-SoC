#include "accel_generated.h"
#include "asic.h"
#include "auto_link.h"
#include "cgra_dma.h"
#include "cgra_link.h"
#include "gemmini.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../../build/experiments/asic/system/cgra_gcn_fast_api.h"
#include "../../../build/experiments/asic/system/gcn.h"

#if DIM != 64
#error "The GCN comparison requires DIM64 Gemmini"
#endif

enum {
  WORDS = GCN_NODES * GCN_COLUMNS,
  EDGE_WORDS = (GCN_EDGES + 7) / 8 * 8,
  ROW_WORDS = (GCN_NODES + 8) / 8 * 8,
  ASIC_ROWS = 0,
  ASIC_COLUMNS = ROW_WORDS,
  ASIC_WEIGHTS = ASIC_COLUMNS + EDGE_WORDS,
  ASIC_FEATURES = ASIC_WEIGHTS + EDGE_WORDS,
  ASIC_SUM = ASIC_FEATURES + WORDS,
  ASIC_OUTPUT = ASIC_SUM + WORDS,
  CGRA_FEATURES = 0,
  CGRA_COLUMNS = 4096,
  CGRA_WEIGHTS = 4352,
  CGRA_OUTPUT = 8192,
  FULL_ROW_STRIDE = GCN_COLUMNS * sizeof(acc_t) / (DIM * sizeof(elem_t)),
};

typedef struct {
  const char *name;
  accel_t device;
  unsigned features;
  unsigned output;
  unsigned jobs;
  int dedicated;
} backend_t;

static const backend_t backends[] = {{"CGRA", CGRA0, CGRA_FEATURES, CGRA_OUTPUT, GCN_NODES, 0}, {"ASIC", ASIC0, ASIC_FEATURES, ASIC_OUTPUT, 2, 1}};
static elem_t source[DIM * GCN_INNER] __attribute__((aligned(64)));
static const acc_t bias[DIM] __attribute__((aligned(64))) = {0};
static acc_t output[WORDS] __attribute__((aligned(64)));

static unsigned long cycles(void) {
  unsigned long value;
  __asm__ volatile("rdcycle %0" : "=r"(value) : : "memory");
  return value;
}

static int transfer(const backend_t *backend, unsigned local, void *memory, unsigned words, int write) {
  if (backend->dedicated) {
    return write ? asic_store(backend->device, memory, local, words) : asic_load(backend->device, local, memory, words);
  }
  const cgra_dma_desc_t descriptor = ((uint64_t)local << CGRA_DMA_DESC_SPM_ADDR_LSB) | ((uint64_t)words * sizeof(int32_t) << CGRA_DMA_DESC_NBYTES_LSB);
  uint8_t tag;
  cgra_dma_memory_fence();
  accel_commands(backend->device, {
    if (write) {
      cgra_dma_mvout_async(memory, descriptor);
    } else {
      cgra_dma_mvin_async(memory, descriptor);
    }
    tag = cgra_dma_wait(0);
  });
  cgra_dma_memory_fence();
  return tag != 0;
}

static int configure_asic(const backend_t *backend, int chained) {
  const asic_job_t aggregate = {
      .unit = 0, .rows = GCN_NODES, .edges = GCN_EDGES, .src0 = ASIC_ROWS, .src1 = ASIC_COLUMNS, .src2 = ASIC_WEIGHTS, .src3 = ASIC_FEATURES, .dst = ASIC_SUM, .next_job = chained ? 2 : 0};
  const asic_job_t relu = {.unit = 1, .count = WORDS, .src0 = ASIC_SUM, .dst = ASIC_OUTPUT};
  return asic_config(backend->device, 0, &aggregate) || asic_config(backend->device, 1, &relu) || transfer(backend, ASIC_ROWS, (void *)gcn_rowptr, ROW_WORDS, 0) ||
         transfer(backend, ASIC_COLUMNS, (void *)gcn_col, EDGE_WORDS, 0) || transfer(backend, ASIC_WEIGHTS, (void *)gcn_edge_weight, EDGE_WORDS, 0);
}

static int configure_cgra(const backend_t *backend, int chained) {
  accel_commands(backend->device, { cgra_send_packets_fast(GCN.static_packets, GCN.static_count); });
  for (unsigned row = 0; row < GCN_NODES; ++row) {
    const unsigned first = gcn_rowptr[row];
    const unsigned degree = gcn_rowptr[row + 1] - first;
    const uint32_t args[GCN_SYMBOL_COUNT] = {CGRA_FEATURES, CGRA_COLUMNS + first, CGRA_WEIGHTS + first, CGRA_OUTPUT + row * GCN_COLUMNS, degree, 0, degree * GCN_COLUMNS, GCN_COLUMNS};
    cgra_packet_t packets[GCN_FAST_CONFIG_PACKET_COUNT];
    memcpy(packets, GCN.config_packets, sizeof(packets));
    for (unsigned index = 0; index < GCN.patch_count; ++index) {
      const cgra_patch_t *patch = &GCN.patches[index];
      const uint32_t value = args[patch->symbol_index] * patch->scale + patch->offset;
      cgra_packet_t *packet = &packets[patch->packet_index];
      packet->hi = (packet->hi & ~GCN_STORE_DATA_PAYLOAD_HI(UINT32_MAX)) | GCN_STORE_DATA_PAYLOAD_HI(value);
    }
    cgra_kernel_t kernel = GCN;
    kernel.static_count = 0;
    kernel.config_packets = packets;
    int status;
    accel_commands(backend->device, { status = cgra_link_capture_at(backend->device.control, row, &kernel, NULL, chained && row + 1 < GCN_NODES ? row + 2 : 0); });
    if (status) {
      return status;
    }
  }
  return transfer(backend, CGRA_COLUMNS, (void *)gcn_col, EDGE_WORDS, 0) || transfer(backend, CGRA_WEIGHTS, (void *)gcn_edge_weight, EDGE_WORDS, 0);
}

static int configure(const backend_t *backend, int chained) {
  accel_commands(GEMMINI0, {
    gemmini_flush(0);
    gemmini_fence();
  });
  const int status = backend->dedicated ? configure_asic(backend, chained) : configure_cgra(backend, chained);
  if (status) {
    return status;
  }
  const auto_link_stage_t stage = {.endpoint = backend->device.id, .job = 0, .output = {.address = {.binding = -1}}};
  const auto_link_edge_t edge = {.destination = 0, .flags = 3, .address = {.binding = -1}};
  const auto_link_graph_t graph = {
      .stage_count = 1, .edge_count = 1, .stage_capacity = AUTO_LINK_GRAPH.stage_capacity, .edge_capacity = AUTO_LINK_GRAPH.edge_capacity, .stages = &stage, .edges = &edge};
  auto_link_load(&graph, NULL);
  auto_link_tiles(1, 1, 1, 1);
  return 0;
}

static void project(unsigned publication_row) {
  enum { B_ROW = DIM, ACC_WRITE = UINT32_C(1) << (ADDR_LEN - 1), ACC_READ = ACC_WRITE | (UINT32_C(1) << (ADDR_LEN - 3)), SKIPS = (1 << 3) | (1 << 4) | (1 << 5) | (1 << 7) };
  for (unsigned column = 0; column < GCN_COLUMNS; column += DIM) {
    accel_commands(GEMMINI0, {
      gemmini_config_ex(WEIGHT_STATIONARY, NO_ACTIVATION, 0);
      gemmini_extended3_config_ld(GCN_INNER, MVIN_SCALE_IDENTITY, false, 0);
      gemmini_extended3_config_ld(GCN_COLUMNS, MVIN_SCALE_IDENTITY, false, 1);
      gemmini_extended3_config_ld(0, MVIN_SCALE_IDENTITY, false, 2);
      gemmini_config_st(DIM * sizeof(acc_t));
      gemmini_extended_mvin3(bias, ACC_WRITE, DIM, DIM);
      gemmini_extended_mvin(source, 0, GCN_INNER, DIM);
      gemmini_extended_mvin2(gcn_weight + column, B_ROW, DIM, DIM);
      gemmini_loop_ws_spad(1, 1, 1, 0, 0, 0, 0, B_ROW + DIM, 0, 0, false, false, true, false, true, NO_ACTIVATION, 0, 0, false, SKIPS);
      gemmini_extended_mvout_spad(publication_row + column * sizeof(acc_t) / (DIM * sizeof(elem_t)), FULL_ROW_STRIDE, ACC_READ, DIM, GCN_NODES);
      gemmini_fence();
    });
  }
}

static int drain(unsigned job) {
  unsigned reports = 0;
  int failed = 0;
  for (;;) {
    const unsigned running = *(volatile uint32_t *)(AUTO_LINK_BASE + AUTO_LINK_RUNNING);
    if (!cgra_link_read_at(CGRA0.control, CGRA_LINK_CONTROL_RESULT_VALID)) {
      if (!running) {
        break;
      }
      continue;
    }
    const cgra_link_result_t result = cgra_link_wait_at(CGRA0.control);
    if (result.status || result.detail || result.data || result.stage || result.job != job) {
      printf("GCN COMPARE report stage=%u job=%u status=%u detail=%u data=%u\n", result.stage, result.job, result.status, result.detail, result.data);
      failed = 1;
    }
    ++reports;
  }
  return failed || reports != 1;
}

static int execute(const backend_t *backend, int chained, unsigned *submissions) {
  const unsigned count = chained ? 1 : backend->jobs;
  for (unsigned job = 0; job < count; ++job) {
    auto_link_job(0, job);
    auto_link_input_ready();
    ++*submissions;
    if (drain(job)) {
      return 1;
    }
  }
  return 0;
}

static unsigned check(unsigned input) {
  unsigned errors = 0;
  for (unsigned index = 0; index < WORDS; ++index) {
    const int expected = gcn_expected[input][index];
    if (output[index] != expected) {
      if (errors < 4) {
        printf("GCN COMPARE mismatch input=%u index=%u actual=%d expected=%d\n", input, index, output[index], expected);
      }
      ++errors;
    }
  }
  return errors;
}

static int run(const backend_t *backend, unsigned input, int chained) {
  const unsigned publication = GEMMINI0.spm_bytes - WORDS * sizeof(acc_t);
  const unsigned publication_row = publication / (DIM * sizeof(elem_t));
  const uintptr_t projected = GEMMINI0.spm + publication;
  unsigned submissions = 0;
  uint64_t before[ASIC_COUNTERS], after[ASIC_COUNTERS];
  memset(output, 0xa5, sizeof(output));
  for (unsigned index = 0; index < ASIC_COUNTERS; ++index)
    before[index] = asic_counter(ASIC0, index);
  asic_fence();
  const unsigned long begin = cycles();
  memcpy(source, gcn_inputs[input], sizeof(source));
  asic_fence();
  const unsigned long staged = cycles();
  int status = input == 0 ? configure(backend, chained) : 0;
  const unsigned long configured = cycles();
  if (!status) {
    project(publication_row);
  }
  const unsigned long projected_at = cycles();
  if (!status) {
    status = transfer(backend, backend->features, (void *)projected, WORDS, 0);
  }
  const unsigned long copied = cycles();
  if (!status) {
    status = execute(backend, chained, &submissions);
  }
  const unsigned long computed = cycles();
  if (!status) {
    status = transfer(backend, backend->output, output, WORDS, 1);
  }
  asic_fence();
  const unsigned long finished = cycles();
  for (unsigned index = 0; index < ASIC_COUNTERS; ++index)
    after[index] = asic_counter(ASIC0, index);
  const unsigned errors = status ? 0 : check(input);
  printf("GCN COMPARE backend=%s mode=%s phase=%s input=%u total_cycles=%lu staging_cycles=%lu setup_cycles=%lu projection_cycles=%lu spm_copy_cycles=%lu nonmatrix_cycles=%lu output_cycles=%lu "
         "cpu_submissions=%u cached_jobs=%u projected_spm_bytes=%u output_dma_bytes=%u checked=%u mismatches=%u status=%d\n",
         backend->name, chained ? "chain" : "per-job", input ? "warm" : "cold", input, finished - begin, staged - begin, configured - staged, projected_at - configured, copied - projected_at,
         computed - copied, finished - computed, submissions, input ? 0 : backend->jobs, (unsigned)(WORDS * sizeof(acc_t)), (unsigned)sizeof(output), status ? 0 : WORDS, errors, status);
  if (backend->dedicated)
    printf("GCN COMPARE hardware active_cycles=%lu core_starts=%lu read_bytes=%lu write_bytes=%lu native_cpu_starts=%lu config_commands=%lu\n", (unsigned long)(after[0] - before[0]),
           (unsigned long)(after[1] - before[1]), (unsigned long)(after[2] - before[2]), (unsigned long)(after[3] - before[3]), (unsigned long)(after[4] - before[4]),
           (unsigned long)(after[5] - before[5]));
  return status || errors;
}

int main(int argc, char **argv) {
  const int selected = argc > 1 ? atoi(argv[1]) : -1;
  const int mode = argc > 2 ? atoi(argv[2]) : -1;
  if (selected < -1 || selected > 1 || mode < -1 || mode > 1) {
    printf("GCN COMPARE arguments: backend=-1(both)|0(CGRA)|1(ASIC) mode=-1(both)|0(per-job)|1(chain)\n");
    return 1;
  }
  printf("GCN COMPARE scope=full-small-GCN nodes=%u inner=%u columns=%u reference-intermediates=0 common-Rocket=1 common-Gemmini=1 common-AutoLink=1 CGRA-row-jobs=16 ASIC-operator-jobs=2\n",
         GCN_NODES, GCN_INNER, GCN_COLUMNS);
  for (int index = selected < 0 ? 0 : selected; index <= (selected < 0 ? 1 : selected); ++index) {
    for (int chained = mode < 0 ? 0 : mode; chained <= (mode < 0 ? 1 : mode); ++chained) {
      for (unsigned input = 0; input < 2; ++input) {
        if (run(&backends[index], input, chained)) {
          printf("GCN COMPARE: FAIL\n");
          return 1;
        }
      }
    }
  }
  printf("GCN COMPARE: PASS\n");
  return 0;
}
