#include "accel_generated.h"
#include "block_gemm.h"
#include "cgra_dma.h"
#include "generated/cgra_gcn_aggregate_fast_api.h"
#include "generated/cgra_gcn_fast_api.h"
#include "generated/cgra_gcn_relu_fast_api.h"
#include "generated/gcn_data.h"

#include <stdio.h>
#include <stdlib.h>
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
static unsigned split_relu;
static const cgra_kernel_t *aggregation = &GCN;
static struct {
  unsigned long setup, execute, writeback, capture;
  unsigned batches, gemmini_jobs, cgra_jobs, profiles, captures, packets;
} stats;

typedef struct {
  uint32_t args[8];
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
      tasks[task++] = (gcn_task_t){{PROJECTED, NEIGHBORS + start, WEIGHTS + start, OUTPUT + row * GCN_OUTPUTS, degree, channel, degree * count, GCN_OUTPUTS}};
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

static void resolve_packets(const cgra_kernel_t *kernel, const uint32_t *args, cgra_packet_t *packets) {
  for (unsigned index = 0; index < kernel->config_count; ++index) {
    packets[index] = kernel->config_packets[index];
  }
  for (unsigned index = 0; index < kernel->patch_count; ++index) {
    const cgra_patch_t *patch = &kernel->patches[index];
    const uint32_t value = args[patch->symbol_index] * patch->scale + patch->offset;
    cgra_packet_t *packet = &packets[patch->packet_index];
    packet->hi = (packet->hi & ~GCN_STORE_DATA_PAYLOAD_HI(UINT32_MAX)) | GCN_STORE_DATA_PAYLOAD_HI(value);
  }
}

static void task_packets(const uint32_t *args, cgra_packet_t *packets) { resolve_packets(&GCN, args, packets); }

#include "gcn_auto.h"
#include "gcn_group.h"

#include "experiments/gcn_pipe.h"

static int run_kernel(const cgra_kernel_t *source, const uint32_t *args, cgra_run_t mode) {
  cgra_kernel_t kernel = *source;
  cgra_packet_t packets[kernel.config_count];
  resolve_packets(&kernel, args, packets);
  kernel.config_packets = packets;
  uint64_t ready = 0, status = 0, result = 0;
  accel_commands(CGRA0, {
    cgra_prepare(&kernel, mode);
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

static int aggregate(const uint32_t *args) { return run_kernel(aggregation, args, started ? CGRA_REPEAT : CGRA_SWITCH); }

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
  if (split_relu) {
    const uint32_t args[] = {OUTPUT, OUTPUT, graph->nodes * GCN_OUTPUTS};
    if (run_kernel(&GCN_RELU, args, CGRA_SWITCH)) {
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

#include "experiments/gcn_batch.h"
#include "experiments/gcn_ready.h"

int main(int argc, char **argv) {
  const unsigned selected_graph = argc > 1 ? atoi(argv[1]) : 0;
  const int selected_mode = argc > 2 ? atoi(argv[2]) : -1;
  const unsigned repeats = argc > 3 ? atoi(argv[3]) : 2;
  const unsigned width = argc > 4 ? atoi(argv[4]) : 64;
  const unsigned selected_layout = argc > 5 ? atoi(argv[5]) : 0;
  if (selected_graph > GCN_GRAPHS || selected_mode < -1 || selected_mode > 10 || selected_mode == 7 || repeats == 0 || (width != 16 && width != 32 && width != 64) || selected_layout > 2) {
    printf("GCN arguments: graph=0..%u mode=-1..6|8..10 repeats>0 width=16|32|64 layout=0(fused)|1(split)|2(compare); mode8/9 grouped Auto/Hybrid, mode10 grouped comparison; mode3 channel pipeline, "
           "mode4/5 two-input Auto/Manual comparison, mode6 star readiness "
           "comparison\n",
           GCN_GRAPHS);
    return 1;
  }
  if (selected_layout && selected_mode >= 3) {
    printf("GCN split comparison supports Manual/Auto/Hybrid only\n");
    return 1;
  }
  if (GCN_MAX_ROWS > 32 && selected_mode >= 3) {
    printf("GCN large fixtures support Manual/Auto/Hybrid only; pipeline experiments require the default fixtures\n");
    return 1;
  }
  if (selected_mode == 6 && selected_graph != 2) {
    printf("GCN readiness comparison requires graph2\n");
    return 1;
  }
  const char *modes[] = {"manual", "auto", "hybrid", "pipeline", "batch-auto", "batch-manual", "ready-serial", "ready-fork", "group-auto", "group-hybrid"};
  printf("GCN dataset=%s graphs=%u input=%u output=%u output_scale=32768 base_spm_words=%u\n", GCN_DATASET, GCN_GRAPHS, GCN_INPUTS, GCN_OUTPUTS, OUTPUT + GCN_MAX_ROWS * GCN_OUTPUTS);
  for (unsigned repeat = 0; repeat < repeats; ++repeat) {
    for (unsigned index = 0; index < GCN_GRAPHS; ++index) {
      const gcn_graph_t *graph = &gcn_graphs[index];
      if (selected_graph != 0 && graph->id != selected_graph) {
        continue;
      }
      const unsigned first_mode = selected_mode < 0 || selected_mode == 10 ? 0 : selected_mode;
      const unsigned end_mode = selected_mode == 10 ? 10 : selected_mode < 0 ? 3 : first_mode + (selected_mode == 6 ? 2 : 1);
      for (unsigned mode = first_mode; mode < end_mode; ++mode) {
        if (selected_mode == 10 && mode >= 3 && mode < 8) {
          continue;
        }
        if (mode == 4 || mode == 5) {
          printf("GCN start mode=batch graph=%u repeat=%u\n", graph->id, repeat);
          if (run_batch(graph, &gcn_alt_graphs[index], mode == 5)) {
            printf("GCN: FAIL\n");
            return 1;
          }
          continue;
        }
        const unsigned first_layout = selected_layout == 2 ? 0 : selected_layout;
        const unsigned end_layout = selected_layout == 2 ? 2 : first_layout + 1;
        for (unsigned layout = first_layout; layout < end_layout; ++layout) {
          split_relu = layout;
          aggregation = split_relu ? &GCN_AGGREGATE : &GCN;
          started = 0;
          for (unsigned word = 0; word < GCN_MAX_ROWS * GCN_OUTPUTS; ++word) {
            projected[word] = INT32_MIN;
            output[word] = INT32_MIN;
          }
          if (mode == 3) {
            for (unsigned slot = 0; slot < 2; ++slot) {
              for (unsigned word = 0; word < PIPE_WORDS; ++word) {
                pipe_buffer[slot][word] = INT32_MIN;
              }
            }
          }
          launches = 0;
          memset(&stats, 0, sizeof(stats));
          // Poison device output as well, so an incomplete cached chain cannot reuse an old result.
          if (load(output, OUTPUT, graph->nodes * GCN_OUTPUTS)) {
            printf("GCN: FAIL output initialization\n");
            return 1;
          }
          printf("GCN start mode=%s layout=%s graph=%u repeat=%u\n", modes[mode], split_relu ? "split" : "fused", graph->id, repeat);
          const unsigned long begin = clock_cycles();
          // Charge the same resident-program load to every mode and inference.
          accel_commands(CGRA0, {
            cgra_send_packets_fast(aggregation->static_packets, aggregation->static_count);
            if (split_relu) {
              cgra_send_packets_fast(GCN_RELU.static_packets, GCN_RELU.static_count);
            }
          });
          stats.setup = clock_cycles() - begin;
          int status;
          switch (mode) {
          case 0:
            status = run_manual(graph);
            break;
          case 3:
            status = run_pipeline(graph, width);
            break;
          case 6:
          case 7:
            status = run_ready(graph, mode == 6);
            break;
          case 8:
          case 9:
            status = run_grouped(graph, mode == 8 ? 32 : 8);
            break;
          default:
            status = run_auto(graph, mode == 1 ? 32 : 8);
            break;
          }
          const unsigned long elapsed = clock_cycles() - begin;
          const unsigned projection_errors = mode == 3 ? 0 : check(projected, graph->expected_z, graph->rows * GCN_OUTPUTS, "projection");
          const unsigned errors = projection_errors + check(output, graph->expected_y, graph->nodes * GCN_OUTPUTS, "output");
          if (mode == 3) {
            printf("GCN pipeline tiles=%u width=%u fabric_cycles=%lu overlap=%lu peak=%lu\n", GCN_OUTPUTS / width, width, pipe_stats.cycles, pipe_stats.overlap, pipe_stats.peak);
          }
          printf("GCN setup_cycles=%lu execute_cycles=%lu writeback_cycles=%lu launches=%u batches=%u peak_gemmini_jobs=%u peak_cgra_jobs=%u peak_profiles=%u\n", stats.setup, stats.execute,
                 stats.writeback, launches, stats.batches, stats.gemmini_jobs, stats.cgra_jobs, stats.profiles);
          printf("GCN mode=%s layout=%s graph=%u repeat=%u nodes=%u edges_with_loops=%u cycles=%lu mismatches=%u\n", modes[mode], split_relu ? "split" : "fused", graph->id, repeat, graph->nodes,
                 graph->edges, elapsed, errors);
          printf("GCN cgra_captures=%u captured_packets=%u capture_cycles=%lu\n", stats.captures, stats.packets, stats.capture);
          if (status || errors) {
            printf("GCN: FAIL\n");
            return 1;
          }
        }
      }
    }
  }
  printf("GCN: PASS\n");
  return 0;
}
