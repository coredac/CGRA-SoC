#include "accel_generated.h"
#include "block_gemm.h"
#include "cgra_dma.h"
#include "generated/cgra_gcn_fast_api.h"
#include "generated/gcn_data.h"

#include <stdio.h>
#include <string.h>

enum {
  PROJECTED = 0,
  NEIGHBORS = GCN_MAX_ROWS * GCN_OUTPUTS,
  WEIGHTS = NEIGHBORS + ((GCN_MAX_EDGES + 3) & ~3),
  OUTPUT = WEIGHTS + ((GCN_MAX_EDGES + 3) & ~3),
};

static const int32_t bias[GCN_OUTPUTS] __attribute__((aligned(64))) = {0};
static int32_t projected[GCN_MAX_ROWS * GCN_OUTPUTS] __attribute__((aligned(64))) = {1};
static int32_t output[GCN_MAX_ROWS * GCN_OUTPUTS] __attribute__((aligned(64))) = {1};
static unsigned started;
static unsigned launches;
static struct {
  unsigned long setup, execute, writeback;
  unsigned batches, gemmini_jobs, cgra_jobs, profiles;
} stats;

typedef struct {
  uint32_t args[7];
} gcn_task_t;

static unsigned task_count(const gcn_graph_t *graph) {
  unsigned count = 0;
  for (unsigned row = 0; row < graph->nodes; ++row) {
    const unsigned degree = graph->rowptr[row + 1] - graph->rowptr[row];
    const unsigned limit = UINT16_MAX / GCN_CTRL_COUNT_PER_ITER / degree;
    count += (GCN_OUTPUTS + limit - 1) / limit;
  }
  return count;
}

static void tasks_init(const gcn_graph_t *graph, gcn_task_t *tasks) {
  unsigned task = 0;
  for (unsigned row = 0; row < graph->nodes; ++row) {
    const unsigned start = graph->rowptr[row];
    const unsigned degree = graph->rowptr[row + 1] - start;
    const unsigned limit = UINT16_MAX / GCN_CTRL_COUNT_PER_ITER / degree;
    for (unsigned channel = 0; channel < GCN_OUTPUTS; channel += limit) {
      const unsigned count = GCN_OUTPUTS - channel < limit ? GCN_OUTPUTS - channel : limit;
      tasks[task++] = (gcn_task_t){{PROJECTED, NEIGHBORS + start, WEIGHTS + start, OUTPUT + row * GCN_OUTPUTS, degree, channel, degree * count}};
    }
  }
}

static unsigned long clock_cycles(void) {
  unsigned long value;
  __asm__ volatile("rdcycle %0" : "=r"(value) : : "memory");
  return value;
}

static int load(const void *source, unsigned address, unsigned words) {
  const cgra_dma_desc_t descriptor = ((uint64_t)address << CGRA_DMA_DESC_SPM_ADDR_LSB) | ((uint64_t)(words * sizeof(int32_t)) << CGRA_DMA_DESC_NBYTES_LSB);
  uint8_t tag;
  cgra_dma_memory_fence();
  accel_commands(CGRA0, {
    cgra_dma_mvin_async(source, descriptor);
    tag = cgra_dma_wait(0);
  });
  return tag != 0;
}

static int store(unsigned words) {
  const unsigned long begin = clock_cycles();
  const cgra_dma_desc_t descriptor = ((uint64_t)OUTPUT << CGRA_DMA_DESC_SPM_ADDR_LSB) | ((uint64_t)(words * sizeof(int32_t)) << CGRA_DMA_DESC_NBYTES_LSB);
  uint8_t tag;
  accel_commands(CGRA0, {
    cgra_dma_mvout_async(output, descriptor);
    tag = cgra_dma_wait(0);
  });
  cgra_dma_memory_fence();
  stats.writeback += clock_cycles() - begin;
  return tag != 0;
}

#include "gcn_auto.h"

static int aggregate(const uint32_t *args) {
  cgra_kernel_t kernel = GCN;
  cgra_packet_t packets[GCN_FAST_CONFIG_PACKET_COUNT];
  memcpy(packets, kernel.config_packets, sizeof(packets));
  for (unsigned index = 0; index < kernel.patch_count; ++index) {
    const cgra_patch_t *patch = &kernel.patches[index];
    const uint32_t value = args[patch->symbol_index] * patch->scale + patch->offset;
    cgra_packet_t *packet = &packets[patch->packet_index];
    packet->hi = (packet->hi & ~GCN_STORE_DATA_PAYLOAD_HI(UINT32_MAX)) | GCN_STORE_DATA_PAYLOAD_HI(value);
  }
  kernel.config_packets = packets;
  uint64_t ready = 0, status = 0, result = 0;
  accel_commands(CGRA0, {
    if (started) {
      cgra_prepare(&kernel, CGRA_REPEAT);
    } else {
      cgra_config(&kernel, CGRA_COLD);
    }
    cgra_start(&kernel);
    CGRA_WAIT(ready);
    CGRA_STATUS(status);
    CGRA_RESULT(result);
  });
  started = 1;
  ++launches;
  if (ready == 1 && (status & 1) == 1 && ((status >> 1) & 0xffff) == kernel.expected_completes && result == 0) {
    return 0;
  }
  printf("GCN CGRA ready=%lu status=%lu result=%lu\n", (unsigned long)ready, (unsigned long)status, (unsigned long)result);
  return 1;
}

static int run_manual(const gcn_graph_t *graph) {
  const unsigned long begin = clock_cycles();
  const unsigned count = task_count(graph);
  gcn_task_t tasks[count];
  tasks_init(graph, tasks);
  block_matmul(GEMMINI0, graph->x, gcn_weight, bias, projected, graph->rows, GCN_INPUTS, GCN_OUTPUTS);
  if (load(projected, PROJECTED, graph->rows * GCN_OUTPUTS) || load(graph->col, NEIGHBORS, (graph->edges + 3) & ~3) || load(graph->edge_weight, WEIGHTS, (graph->edges + 3) & ~3)) {
    return 1;
  }
  for (unsigned task = 0; task < count; ++task) {
    if (aggregate(tasks[task].args)) {
      return 1;
    }
  }
  stats.execute = clock_cycles() - begin;
  return store(graph->nodes * GCN_OUTPUTS);
}

static unsigned check(const int32_t *actual, const int32_t *expected, unsigned count, const char *phase) {
  unsigned errors = 0;
  for (unsigned index = 0; index < count; ++index) {
    if (actual[index] != expected[index]) {
      if (errors < 8) {
        printf("GCN %s mismatch index=%u expected=%d actual=%d\n", phase, index, expected[index], actual[index]);
      }
      ++errors;
    }
  }
  return errors;
}

int main(void) {
  const char *modes[] = {"manual", "auto", "hybrid"};
  printf("GCN dataset=%s graphs=%u input=%u output=%u output_scale=32768 spm_words=%u\n", GCN_DATASET, GCN_GRAPHS, GCN_INPUTS, GCN_OUTPUTS, OUTPUT + GCN_MAX_ROWS * GCN_OUTPUTS);
  for (unsigned repeat = 0; repeat < 2; ++repeat) {
    for (unsigned index = 0; index < GCN_GRAPHS; ++index) {
      const gcn_graph_t *graph = &gcn_graphs[index];
      for (unsigned mode = 0; mode < 3; ++mode) {
        for (unsigned word = 0; word < GCN_MAX_ROWS * GCN_OUTPUTS; ++word) {
          projected[word] = INT32_MIN;
          output[word] = INT32_MIN;
        }
        launches = 0;
        memset(&stats, 0, sizeof(stats));
        // Poison device output as well, so an incomplete cached chain cannot reuse an old result.
        if (load(output, OUTPUT, graph->nodes * GCN_OUTPUTS)) {
          printf("GCN: FAIL output initialization\n");
          return 1;
        }
        printf("GCN start mode=%s graph=%u repeat=%u\n", modes[mode], graph->id, repeat);
        const unsigned long begin = clock_cycles();
        const int status = mode == 0 ? run_manual(graph) : run_auto(graph, mode == 1 ? 32 : 8);
        const unsigned long elapsed = clock_cycles() - begin;
        const unsigned errors = check(projected, graph->expected_z, graph->rows * GCN_OUTPUTS, "projection") + check(output, graph->expected_y, graph->nodes * GCN_OUTPUTS, "output");
        printf("GCN setup_cycles=%lu execute_cycles=%lu writeback_cycles=%lu launches=%u batches=%u peak_gemmini_jobs=%u peak_cgra_jobs=%u peak_profiles=%u\n", stats.setup, stats.execute,
               stats.writeback, launches, stats.batches, stats.gemmini_jobs, stats.cgra_jobs, stats.profiles);
        printf("GCN mode=%s graph=%u repeat=%u nodes=%u edges_with_loops=%u cycles=%lu mismatches=%u\n", modes[mode], graph->id, repeat, graph->nodes, graph->edges, elapsed, errors);
        if (status || errors) {
          printf("GCN: FAIL\n");
          return 1;
        }
      }
    }
  }
  printf("GCN: PASS\n");
  return 0;
}
