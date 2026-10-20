#ifndef GCN_AUTO_H
#define GCN_AUTO_H

#include "auto_link.h"
#include "cgra_link.h"
#include "gcn_jobs.h"

static void gcn_profile_tiles(unsigned profile, accel_t device, unsigned job, const auto_link_edge_t *edges, unsigned count, unsigned tiles) {
  const auto_link_stage_t stage = {.endpoint = device.id, .job = job, .output = {.address = {.binding = -1}}};
  const auto_link_graph_t graph = {.stage_count = 1, .edge_count = count, .stage_capacity = 3, .edge_capacity = 3, .stages = &stage, .edges = edges};
  auto_link_load(&graph, NULL);
  auto_link_tiles(tiles, 1, 1, 1);
  auto_link_run_config(profile);
}

static void gcn_profile(unsigned profile, accel_t device, unsigned job, const auto_link_edge_t *edges, unsigned count) { gcn_profile_tiles(profile, device, job, edges, count, 1); }

static int gcn_project(const gcn_graph_t *graph) {
  const auto_link_edge_t root = {.destination = 0, .flags = 3, .address = {.binding = -1}};
  unsigned job = 0;
  for (unsigned row = 0; row < graph->rows; row += DIM) {
    for (unsigned column = 0; column < GCN_OUTPUTS; column += 64) {
      if (gcn_projection_job(job, graph->x + row * GCN_INPUTS, gcn_weight, bias, projected + row * GCN_OUTPUTS, column)) {
        return 1;
      }
      if (job == 0) {
        gcn_profile(job, GEMMINI0, job, &root, 1);
      } else {
        auto_link_job(0, job);
        auto_link_run_config(job);
      }
      ++job;
    }
  }
  stats.gemmini_jobs = job;
  return 0;
}

static void gcn_inputs(const gcn_graph_t *graph, unsigned profile, unsigned job, unsigned tiles, int copy) {
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
  gcn_profile_tiles(profile, CGRA0, job, copy ? inputs : &root, copy ? 3 : 1, tiles);
}

static int gcn_batch(const gcn_graph_t *graph, const gcn_task_t *tasks, unsigned count, unsigned aggregates, unsigned profile, unsigned batch) {
  const unsigned long begin = clock_cycles();
  const uint32_t relu_args[] = {OUTPUT, OUTPUT, graph->nodes * GCN_OUTPUTS};
  for (unsigned job = 0; job < count; ++job) {
    cgra_kernel_t kernel = job < aggregates ? *aggregation : GCN_RELU;
    // Programs are resident; the final split-layout job runs ReLU over all nodes.
    kernel.static_count = 0;
    cgra_packet_t packets[kernel.config_count];
    // Each job has fixed arguments; capture resolved payloads without relocations.
    resolve_packets(&kernel, job < aggregates ? tasks[job].args : relu_args, packets);
    kernel.config_packets = packets;
    int failed;
    accel_commands(CGRA0, { failed = cgra_link_capture_at(CGRA0.control, job, &kernel, NULL, job + 1 == count ? 0 : job + 2); });
    if (failed) {
      return 1;
    }
    ++stats.captures;
    stats.packets += kernel.config_count + kernel.launch_count;
  }
  stats.capture += clock_cycles() - begin;
  if (count > stats.cgra_jobs) {
    stats.cgra_jobs = count;
  }
  // Later batches reuse the control-only profile captured by batch 1.
  if (batch > 1) {
    return 0;
  }
  gcn_inputs(graph, profile, 0, 1, batch == 0);
  return 0;
}

static int gcn_execute(unsigned projection_jobs, unsigned groups) {
  const unsigned profiles = projection_jobs + groups;
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
    const unsigned job = reports < projection_jobs ? reports : reports - projection_jobs;
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
  const unsigned jobs = count + split_relu;
  gcn_task_t tasks[count];
  tasks_init(graph, tasks);
  if (gcn_project(graph)) {
    return 1;
  }
  for (unsigned first = 0; first < jobs; first += capacity) {
    const unsigned batch = jobs - first < capacity ? jobs - first : capacity;
    const unsigned aggregates = count - first < batch ? count - first : batch;
    const unsigned projections = first == 0 ? stats.gemmini_jobs : 0;
    if (gcn_batch(graph, tasks + first, batch, aggregates, projections, first / capacity)) {
      return 1;
    }
    stats.setup += clock_cycles() - begin;
    begin = clock_cycles();
    if (gcn_execute(projections, 1)) {
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
