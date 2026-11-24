#include "accel_generated.h"
#include "auto_link.h"
#include "cgra_dma.h"
#include "cgra_link.h"
#include "gemmini.h"
#include "gemmini_job.h"
#include "generated/cgra_gcn_fast_api.h"
#include "generated/gcn_data.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef P1_CGRAS
#define P1_CGRAS 1
#endif

#if DIM != 64
#error "P1 requires the DIM64 Gemmini configuration"
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

enum { WIDTH = 64, INNER = GCN_INPUTS, COLUMNS = GCN_OUTPUTS };
static const int32_t bias[COLUMNS] __attribute__((aligned(64))) = {0};
static int32_t projected[DIM * WIDTH] __attribute__((aligned(64)));
static int32_t output[GCN_MAX_ROWS * COLUMNS] __attribute__((aligned(64)));

typedef struct {
  uint32_t args[GCN_SYMBOL_COUNT];
} task_t;

typedef struct {
  unsigned first, rows, neighbors, weights, output, words, tasks;
  task_t *task;
} partition_t;

static unsigned long cycles(void) {
  unsigned long value;
  __asm__ volatile("rdcycle %0" : "=r"(value) : : "memory");
  return value;
}

static void partition_init(const gcn_graph_t *graph, unsigned index, partition_t *part, task_t *tasks) {
  const unsigned first = graph->nodes * index / P1_CGRAS;
  const unsigned end = graph->nodes * (index + 1) / P1_CGRAS;
  const unsigned edges = (graph->edges + 3) & ~3;
  *part = (partition_t){.first = first, .rows = end - first, .neighbors = graph->nodes * WIDTH, .task = tasks};
  part->weights = part->neighbors + edges;
  part->output = part->weights + edges;
  part->words = part->output + part->rows * COLUMNS;
  for (unsigned row = first; row < end; ++row) {
    const unsigned start = graph->rowptr[row];
    const unsigned degree = graph->rowptr[row + 1] - start;
    tasks[part->tasks++] = (task_t){{0, part->neighbors + start, part->weights + start, part->output + (row - first) * COLUMNS, degree, 0, degree * WIDTH, WIDTH}};
  }
}

static cgra_kernel_t resolve(const task_t *task, cgra_packet_t *packets) {
  cgra_kernel_t kernel = GCN;
  memcpy(packets, kernel.config_packets, kernel.config_count * sizeof(*packets));
  for (unsigned index = 0; index < kernel.patch_count; ++index) {
    const cgra_patch_t *patch = &kernel.patches[index];
    const uint32_t value = task->args[patch->symbol_index] * patch->scale + patch->offset;
    cgra_packet_t *packet = &packets[patch->packet_index];
    packet->hi = (packet->hi & ~GCN_STORE_DATA_PAYLOAD_HI(UINT32_MAX)) | GCN_STORE_DATA_PAYLOAD_HI(value);
  }
  kernel.static_count = 0;
  kernel.config_packets = packets;
  return kernel;
}

// Both CPU execution and capture issue this same native command sequence.
static void projection(const gcn_graph_t *graph, unsigned column) {
  enum { B_ROW = DIM, ACC_WRITE = UINT32_C(1) << (ADDR_LEN - 1), ACC_READ = ACC_WRITE | (UINT32_C(1) << (ADDR_LEN - 3)), SKIPS = (1 << 3) | (1 << 4) | (1 << 5) | (1 << 7) };
  accel_commands(GEMMINI0, {
    gemmini_config_ex(WEIGHT_STATIONARY, NO_ACTIVATION, 0);
    gemmini_extended3_config_ld(INNER, MVIN_SCALE_IDENTITY, false, 0);
    gemmini_extended3_config_ld(COLUMNS, MVIN_SCALE_IDENTITY, false, 1);
    gemmini_extended3_config_ld(0, MVIN_SCALE_IDENTITY, false, 2);
    gemmini_config_st(WIDTH * sizeof(acc_t));
    gemmini_extended_mvin3(bias, ACC_WRITE, DIM, DIM);
    gemmini_extended_mvin(graph->x, 0, INNER, DIM);
    gemmini_extended_mvin2(gcn_weight + column, B_ROW, WIDTH, DIM);
    gemmini_loop_ws_spad(1, 1, 1, 0, 0, 0, 0, B_ROW + DIM, 0, 0, false, false, true, false, true, NO_ACTIVATION, 0, 0, false, SKIPS);
    gemmini_extended_mvout(projected, ACC_READ, DIM, DIM);
  });
}

static int dma(accel_t device, void *memory, unsigned address, unsigned words, int write) {
  const cgra_dma_desc_t descriptor = ((uint64_t)address << CGRA_DMA_DESC_SPM_ADDR_LSB) | ((uint64_t)words * sizeof(int32_t) << CGRA_DMA_DESC_NBYTES_LSB);
  uint8_t tag;
  cgra_dma_memory_fence();
  accel_commands(device, {
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

static int inputs(const gcn_graph_t *graph, const partition_t *part, accel_t device) {
  const unsigned edges = (graph->edges + 3) & ~3;
  return dma(device, (void *)graph->col, part->neighbors, edges, 0) || dma(device, (void *)graph->edge_weight, part->weights, edges, 0);
}

static int completion(accel_t device) {
  uint64_t ready, status, result;
  accel_commands(device, {
    CGRA_WAIT(ready);
    CGRA_STATUS(status);
    CGRA_RESULT(result);
  });
  if (ready == 1 && (status & 1) && ((status >> 1) & 0xffff) == GCN_EXPECTED_COMPLETES && result == 0) {
    return 0;
  }
  printf("P1 GCN CGRA%u ready=%lu status=%lu result=%lu\n", device.id, ready, status, result);
  return 1;
}

static int manual_band(const gcn_graph_t *graph, partition_t *parts, unsigned band) {
  unsigned maximum = 0;
  for (unsigned index = 0; index < P1_CGRAS; ++index) {
    if (parts[index].tasks > maximum) {
      maximum = parts[index].tasks;
    }
  }
  for (unsigned task = 0; task < maximum; ++task) {
    for (unsigned index = 0; index < P1_CGRAS; ++index) {
      if (task >= parts[index].tasks) {
        continue;
      }
      task_t binding = parts[index].task[task];
      binding.args[GCN_SYMBOL_OUTPUT] += band * WIDTH;
      cgra_packet_t packets[GCN_FAST_CONFIG_PACKET_COUNT];
      cgra_kernel_t kernel = resolve(&binding, packets);
      accel_commands(devices[index], {
        cgra_prepare(&kernel, CGRA_REPEAT);
        cgra_start(&kernel);
      });
    }
    if (!band && !task) {
      // The first tile has reached private CGRA SPM, so its DRAM slot is free.
      projection(graph, WIDTH);
    }
    for (unsigned index = 0; index < P1_CGRAS; ++index) {
      if (task < parts[index].tasks && completion(devices[index])) {
        return 1;
      }
    }
  }
  return 0;
}

static int manual(const gcn_graph_t *graph, partition_t *parts) {
  projection(graph, 0);
  accel_commands(GEMMINI0, { gemmini_fence(); });
  for (unsigned band = 0; band < COLUMNS / WIDTH; ++band) {
    for (unsigned index = 0; index < P1_CGRAS; ++index) {
      if (dma(devices[index], projected, 0, graph->nodes * WIDTH, 0)) {
        return 1;
      }
    }
    if (manual_band(graph, parts, band)) {
      return 1;
    }
    accel_commands(GEMMINI0, { gemmini_fence(); });
  }
  return 0;
}

static void profile(unsigned id, const auto_link_stage_t *stages, unsigned stage_count, const auto_link_edge_t *edges, unsigned edge_count, unsigned tiles) {
  const auto_link_graph_t graph = {
      .stage_count = stage_count, .edge_count = edge_count, .stage_capacity = AUTO_LINK_GRAPH.stage_capacity, .edge_capacity = AUTO_LINK_GRAPH.edge_capacity, .stages = stages, .edges = edges};
  auto_link_load(&graph, NULL);
  auto_link_tiles(tiles, 1, 1, 1);
  auto_link_run_config(id);
}

static int capture(const gcn_graph_t *graph, partition_t *parts, unsigned mode) {
  if (mode == 2) {
    if (gemmini_job_capture_at(GEMMINI0.control, 0, 12, 1)) {
      return 1;
    }
    const gemmini_patch_t patch = {.command = 7, .width = 64, .source = GEMMINI_VALUE_TILE_ID, .scale = WIDTH, .relative = 1};
    gemmini_job_patch_at(GEMMINI0.control, &patch);
    projection(graph, 0);
    if (gemmini_job_end_at(GEMMINI0.control)) {
      return 1;
    }
  }
  for (unsigned index = 0; index < P1_CGRAS; ++index) {
    const partition_t *part = &parts[index];
    for (unsigned band = 0; band < (mode == 1 ? COLUMNS / WIDTH : 1); ++band) {
      for (unsigned row = 0; row < part->tasks; ++row) {
        const unsigned job = band * part->tasks + row;
        task_t binding = part->task[row];
        binding.args[GCN_SYMBOL_OUTPUT] += band * WIDTH;
        cgra_packet_t packets[GCN_FAST_CONFIG_PACKET_COUNT];
        cgra_kernel_t kernel = resolve(&binding, packets);
        cgra_patch_t patches[GCN_SYMBOL_COUNT];
        kernel.patches = patches;
        kernel.patch_count = 0;
        if (mode == 2) {
          for (unsigned patch = 0; patch < GCN.patch_count; ++patch) {
            if (GCN.patches[patch].symbol_index == GCN_SYMBOL_OUTPUT) {
              patches[kernel.patch_count++] = GCN.patches[patch];
            }
          }
        }
        const unsigned setup = kernel.setup_count;
        const unsigned first = kernel.rearm_count + setup;
        memmove(packets + kernel.rearm_count, packets + first, (kernel.config_count - first) * sizeof(*packets));
        kernel.config_count -= setup;
        kernel.setup_count = 0;
        for (unsigned patch = 0; patch < kernel.patch_count; ++patch) {
          patches[patch].packet_index -= setup;
        }
        const cgra_link_symbol_t symbols[GCN_SYMBOL_COUNT] = {[GCN_SYMBOL_OUTPUT] = {binding.args[GCN_SYMBOL_OUTPUT], WIDTH, CGRA_LINK_TILE_ID}};
        const unsigned next = row + 1 == part->tasks ? 0 : job + 2;
        int failed;
        accel_commands(devices[index], { failed = cgra_link_capture_at(devices[index].control, job, &kernel, mode == 2 ? symbols : NULL, next); });
        if (failed) {
          return 1;
        }
      }
    }
  }
  auto_link_stage_t stages[P1_CGRAS + 1];
  auto_link_edge_t edges[P1_CGRAS + 1];
  if (mode == 2) {
    stages[0] = (auto_link_stage_t){.endpoint = GEMMINI0.id, .output = {.address = {.binding = -1}}};
    edges[0] = (auto_link_edge_t){.destination = 0, .flags = 3, .address = {.binding = -1}};
    for (unsigned index = 0; index < P1_CGRAS; ++index) {
      stages[index + 1] = (auto_link_stage_t){.endpoint = devices[index].id, .output = {.address = {.binding = -1}}};
      edges[index + 1] =
          (auto_link_edge_t){.source = 0, .destination = index + 1, .flags = 5, .bytes = graph->nodes * WIDTH * sizeof(int32_t), .address = {.value = (uintptr_t)projected, .binding = -1}};
    }
    profile(0, stages, P1_CGRAS + 1, edges, P1_CGRAS + 1, COLUMNS / WIDTH);
    return 0;
  }
  for (unsigned band = 0; band < COLUMNS / WIDTH; ++band) {
    for (unsigned index = 0; index < P1_CGRAS; ++index) {
      stages[index] = (auto_link_stage_t){.endpoint = devices[index].id, .job = band * parts[index].tasks, .output = {.address = {.binding = -1}}};
      edges[index] = (auto_link_edge_t){.destination = index, .flags = 3, .address = {.binding = -1}};
    }
    profile(band, stages, P1_CGRAS, edges, P1_CGRAS, 1);
  }
  return 0;
}

static void start(unsigned profile) {
  *(volatile uint32_t *)(AUTO_LINK_BASE + AUTO_LINK_RUN_FIRST) = profile;
  *(volatile uint32_t *)(AUTO_LINK_BASE + AUTO_LINK_RUN_COUNT) = 1;
  *(volatile uint32_t *)(AUTO_LINK_BASE + AUTO_LINK_RUN_START) = 1;
  // Payloads were synchronized before launch; allow the CPU to issue the next producer.
  __asm__ volatile("" ::: "memory");
}

static int drain(const partition_t *parts, unsigned mode, unsigned band) {
  const unsigned expected = P1_CGRAS + (mode == 2);
  unsigned reports = 0, failed = 0, seen = 0;
  for (;;) {
    const unsigned running = *(volatile uint32_t *)(AUTO_LINK_BASE + AUTO_LINK_RUNNING);
    if (!cgra_link_read_at(CGRA0.control, CGRA_LINK_CONTROL_RESULT_VALID)) {
      if (!running) {
        break;
      }
      continue;
    }
    const cgra_link_result_t result = cgra_link_wait_at(CGRA0.control);
    const unsigned index = result.stage - (mode == 2);
    const unsigned job = mode == 1 && index < P1_CGRAS ? band * parts[index].tasks : 0;
    const int bad_stage = result.stage >= expected || (seen & (1u << result.stage));
    if (result.status != AUTO_LINK_STATUS_SUCCESS || result.detail || result.data || bad_stage || result.job != job) {
      printf("P1 GCN report stage=%u job=%u status=%u detail=%u data=%u\n", result.stage, result.job, result.status, result.detail, result.data);
      failed = 1;
    }
    if (result.stage < expected) {
      seen |= 1u << result.stage;
    }
    ++reports;
  }
  cgra_dma_memory_fence();
  return failed || reports != expected || seen != (1u << expected) - 1;
}

static int hybrid(const gcn_graph_t *graph, partition_t *parts) {
  projection(graph, 0);
  accel_commands(GEMMINI0, { gemmini_fence(); });
  for (unsigned band = 0; band < COLUMNS / WIDTH; ++band) {
    for (unsigned index = 0; index < P1_CGRAS; ++index) {
      if (dma(devices[index], projected, 0, graph->nodes * WIDTH, 0)) {
        return 1;
      }
    }
    start(band);
    if (!band) {
      projection(graph, WIDTH);
    }
    if (drain(parts, 1, band)) {
      return 1;
    }
    accel_commands(GEMMINI0, { gemmini_fence(); });
  }
  return 0;
}

static unsigned check(const int32_t *actual, const int32_t *expected, unsigned count) {
  unsigned errors = 0;
  for (unsigned index = 0; index < count; ++index) {
    if (actual[index] != expected[index]) {
      if (errors < 4) {
        printf("P1 GCN mismatch index=%u actual=%d expected=%d\n", index, actual[index], expected[index]);
      }
      ++errors;
    }
  }
  return errors;
}

static unsigned check_projection(const gcn_graph_t *graph) {
  unsigned errors = 0;
  for (unsigned row = 0; row < graph->rows; ++row) {
    errors += check(projected + row * WIDTH, graph->expected_z + row * COLUMNS + WIDTH, WIDTH);
  }
  return errors;
}

int main(int argc, char **argv) {
  const unsigned selected = argc > 1 ? atoi(argv[1]) : 1;
  const int selected_mode = argc > 2 ? atoi(argv[2]) : -1;
  const unsigned repeats = argc > 3 ? atoi(argv[3]) : 2;
  const unsigned timing_only = argc > 4 ? atoi(argv[4]) : 0;
  if ((P1_CGRAS != 1 && P1_CGRAS != 2 && P1_CGRAS != 4) || selected == 0 || selected > GCN_GRAPHS || selected_mode < -1 || selected_mode > 2 || repeats == 0 || timing_only > 1) {
    printf("P1 GCN arguments: graph=1..%u mode=-1(all)|0(Manual)|1(Hybrid)|2(Auto) warm_repeats>0 timing_only=0|1\n", GCN_GRAPHS);
    return 1;
  }
  const gcn_graph_t *graph = &gcn_graphs[selected - 1];
  if (graph->rows != DIM || graph->nodes > DIM || INNER != DIM || COLUMNS != 2 * DIM) {
    printf("P1 GCN requires --row-align 64 and at most 64 nodes, 64 input and 128 output channels\n");
    return 1;
  }
  task_t task_storage[graph->nodes];
  partition_t parts[P1_CGRAS];
  unsigned used = 0;
  for (unsigned index = 0; index < P1_CGRAS; ++index) {
    partition_init(graph, index, &parts[index], task_storage + used);
    used += parts[index].tasks;
    if (parts[index].words > CGRA_DMA_SPM_WORDS) {
      printf("P1 GCN CGRA%u needs %u SPM words; hardware has %u\n", index, parts[index].words, CGRA_DMA_SPM_WORDS);
      return 1;
    }
  }
  const char *modes[] = {"Manual", "Hybrid", "Auto"};
  const unsigned first = selected_mode < 0 ? 0 : selected_mode;
  const unsigned end = selected_mode < 0 ? 3 : first + 1;
  printf("P1 GCN schedule=two64 tiles=%u intermediate_bytes=%u warm_repeats=%u\n", COLUMNS / WIDTH, (unsigned)sizeof(projected), repeats);
  for (unsigned mode = first; mode < end; ++mode) {
    for (unsigned repeat = 0; repeat <= repeats; ++repeat) {
      const int cold = repeat == 0;
      if (!timing_only) {
        for (unsigned word = 0; word < graph->rows * COLUMNS; ++word) {
          output[word] = INT32_MIN;
        }
        for (unsigned word = 0; word < DIM * WIDTH; ++word) {
          projected[word] = INT32_MIN;
        }
        for (unsigned index = 0; index < P1_CGRAS; ++index) {
          if (dma(devices[index], output, parts[index].output, parts[index].rows * COLUMNS, 0)) {
            printf("P1 GCN: FAIL output initialization\n");
            return 1;
          }
        }
      }
      const unsigned long begin = cycles();
      int failed = 0;
      if (cold) {
        for (unsigned index = 0; index < P1_CGRAS; ++index) {
          accel_commands(devices[index], {
            cgra_send_packets_fast(GCN.static_packets, GCN.static_count);
            cgra_send_packets_fast(GCN.config_packets + GCN.rearm_count, GCN.setup_count);
          });
        }
      }
      const unsigned long capture_begin = cycles();
      if (cold && mode) {
        failed = capture(graph, parts, mode);
      }
      const unsigned long input_begin = cycles();
      for (unsigned index = 0; !failed && index < P1_CGRAS; ++index) {
        failed = inputs(graph, &parts[index], devices[index]);
      }
      const unsigned long execute_begin = cycles();
      if (!failed && mode == 0) {
        failed = manual(graph, parts);
      }
      if (!failed && mode == 1) {
        failed = hybrid(graph, parts);
      }
      if (!failed && mode == 2) {
        cgra_dma_memory_fence();
        start(0);
        failed = drain(parts, mode, 0);
      }
      const unsigned long writeback_begin = cycles();
      for (unsigned index = 0; !failed && index < P1_CGRAS; ++index) {
        const partition_t *part = &parts[index];
        failed = dma(devices[index], output + part->first * COLUMNS, part->output, part->rows * COLUMNS, 1);
      }
      const unsigned long elapsed = cycles() - begin;
      unsigned errors = 0;
      if (!failed && !timing_only) {
        errors = check_projection(graph) + check(output, graph->expected_y, graph->nodes * COLUMNS);
      }
      printf("P1 GCN mode=%s phase=%s graph=%u nodes=%u cgras=%u repeat=%u cycles=%lu checked=%u mismatches=%u status=%u captures=%u\n", modes[mode], cold ? "cold" : "warm", graph->id, graph->nodes,
             P1_CGRAS, repeat, elapsed, !timing_only, errors, failed, cold && mode != 0);
      printf("P1 GCN setup_cycles=%lu capture_cycles=%lu input_cycles=%lu execute_cycles=%lu writeback_cycles=%lu total_cycles=%lu\n", execute_begin - begin,
             cold && mode ? input_begin - capture_begin : 0, execute_begin - input_begin, writeback_begin - execute_begin, elapsed - (writeback_begin - begin), elapsed);
      if (cold) {
        for (unsigned index = 0; index < P1_CGRAS; ++index) {
          printf("P1 GCN partition=%u rows=%u cached_jobs=%u words=%u packets=%u captures=%u\n", index, parts[index].rows,
                 mode == 1   ? 2 * parts[index].tasks
                 : mode == 2 ? parts[index].tasks
                             : 0,
                 parts[index].words, GCN.config_count + GCN.launch_count - GCN.setup_count, mode != 0);
        }
      }
      if (failed || errors) {
        printf("P1 GCN: FAIL\n");
        return 1;
      }
      if (mode == 2) {
        const unsigned long fabric = *(volatile uint64_t *)(AUTO_LINK_BASE + AUTO_LINK_CYCLES);
        const unsigned long overlap = *(volatile uint64_t *)(AUTO_LINK_BASE + AUTO_LINK_OVERLAP);
        const unsigned long peak = *(volatile uint64_t *)(AUTO_LINK_BASE + AUTO_LINK_PEAK_ACTIVE);
        printf("P1 GCN fabric_cycles=%lu overlap_cycles=%lu peak_tiles=%lu\n", fabric, overlap, peak);
      }
    }
  }
  printf("P1 GCN: PASS\n");
  return 0;
}
