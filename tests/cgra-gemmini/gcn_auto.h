#ifndef GCN_AUTO_H
#define GCN_AUTO_H

#include "auto_link.h"
#include "cgra_link.h"
#include "gcn_jobs.h"

static void gcn_profile(unsigned profile, accel_t device, unsigned job, const auto_link_edge_t *edges, unsigned count) {
  const auto_link_stage_t stage = {.endpoint = device.id, .job = job, .output = {.address = {.binding = -1}}};
  const auto_link_graph_t graph = {.stage_count = 1, .edge_count = count, .stage_capacity = 3, .edge_capacity = 3, .stages = &stage, .edges = edges};
  auto_link_load(&graph, NULL);
  auto_link_tiles(1, 1, 1, 1);
  auto_link_run_config(profile);
}

static int gcn_project(const gcn_graph_t *graph) {
  const auto_link_edge_t root = {.destination = 0, .flags = 3, .address = {.binding = -1}};
  unsigned job = 0;
  for (unsigned row = 0; row < graph->rows; row += DIM) {
    for (unsigned column = 0; column < GCN_OUTPUTS; column += 64) {
      if (gcn_projection_job(job, graph->x + row * GCN_INPUTS, gcn_weight, bias, projected + row * GCN_OUTPUTS, column)) {
        return 1;
      }
      gcn_profile(job, GEMMINI0, job, &root, 1);
      ++job;
    }
  }
  stats.gemmini_jobs = job;
  return 0;
}

static int gcn_batch(const gcn_graph_t *graph, const gcn_task_t *tasks, unsigned count, unsigned profile, int first) {
  cgra_kernel_t kernel = GCN;
  // All tasks use the same resident program, loaded once before the first batch.
  kernel.static_count = 0;
  for (unsigned job = 0; job < count; ++job) {
    cgra_link_symbol_t symbols[7];
    for (unsigned index = 0; index < 7; ++index) {
      symbols[index] = (cgra_link_symbol_t){.base = tasks[job].args[index], .source = CGRA_LINK_SLOT};
    }
    int failed;
    accel_commands(CGRA0, { failed = cgra_link_capture_at(CGRA0.control, job, &kernel, symbols, job + 1 == count ? 0 : job + 2); });
    if (failed) {
      return 1;
    }
  }
  const auto_link_edge_t root = {.destination = 0, .flags = 3, .address = {.binding = -1}};
  const unsigned edge_bytes = ((graph->edges + 3) & ~3) * sizeof(int32_t);
  const auto_link_edge_t inputs[] = {
      {.destination = 0,
       .flags = 7,
       .bytes = graph->rows * GCN_OUTPUTS * sizeof(int32_t),
       .address = {.value = (uintptr_t)projected, .binding = -1},
       .destination_offset = PROJECTED * sizeof(int32_t)},
      {.destination = 0, .flags = 7, .bytes = edge_bytes, .address = {.value = (uintptr_t)graph->col, .binding = -1}, .destination_offset = NEIGHBORS * sizeof(int32_t)},
      {.destination = 0, .flags = 7, .bytes = edge_bytes, .address = {.value = (uintptr_t)graph->edge_weight, .binding = -1}, .destination_offset = WEIGHTS * sizeof(int32_t)},
  };
  gcn_profile(profile, CGRA0, 0, first ? inputs : &root, first ? 3 : 1);
  if (count > stats.cgra_jobs) {
    stats.cgra_jobs = count;
  }
  return 0;
}

static int gcn_execute(unsigned projection_jobs) {
  const unsigned profiles = projection_jobs + 1;
  cgra_dma_memory_fence();
  auto_link_run(0, profiles);
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
    const unsigned job = reports < projection_jobs ? reports : 0;
    if (result.status != AUTO_LINK_STATUS_SUCCESS || result.detail || result.data || result.stage != 0 || result.job != job) {
      printf("GCN report stage=%u job=%u status=%u detail=%u data=%u\n", result.stage, result.job, result.status, result.detail, result.data);
      ++failures;
    }
    ++reports;
  }
  cgra_dma_memory_fence();
  if (reports != profiles) {
    printf("GCN reports=%u expected=%u\n", reports, profiles);
    ++failures;
  }
  ++stats.batches;
  if (profiles > stats.profiles) {
    stats.profiles = profiles;
  }
  return failures;
}

static int run_auto(const gcn_graph_t *graph, unsigned capacity) {
  unsigned long begin = clock_cycles();
  const unsigned count = task_count(graph);
  gcn_task_t tasks[count];
  tasks_init(graph, tasks);
  if (gcn_project(graph)) {
    return 1;
  }
  accel_commands(CGRA0, { cgra_send_packets_fast(GCN.static_packets, GCN.static_count); });
  for (unsigned first = 0; first < count; first += capacity) {
    const unsigned batch = count - first < capacity ? count - first : capacity;
    const unsigned projections = first == 0 ? stats.gemmini_jobs : 0;
    if (gcn_batch(graph, tasks + first, batch, projections, first == 0)) {
      return 1;
    }
    stats.setup += clock_cycles() - begin;
    begin = clock_cycles();
    if (gcn_execute(projections)) {
      return 1;
    }
    stats.execute += clock_cycles() - begin;
    launches += batch;
    begin = clock_cycles();
  }
  started = 1;
  return store(graph->nodes * GCN_OUTPUTS);
}

#endif
