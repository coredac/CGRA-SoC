#ifndef RESNET_REGION_H
#define RESNET_REGION_H

#include "accel_generated.h"
#include "auto_link.h"
#include "cgra_link.h"

static int cache_config(const cgra_kernel_t *kernels, int full) {
  for (unsigned job = full ? 0 : 1; job < 2; ++job) {
    int status;
    accel_commands(CGRA0, { status = cgra_job_config_at(CGRA0.control, job, kernels + job, NULL); });
    if (status)
      return status;
    while (!cgra_link_read_at(CGRA0.control, CGRA_LINK_CONTROL_CONFIG_DONE)) {
    }
    if (cgra_link_read_at(CGRA0.control, CGRA_LINK_CONTROL_CONFIG_STATUS) != AUTO_LINK_STATUS_SUCCESS)
      return 1;
  }
  return 0;
}

static void region_config(int split, unsigned count, unsigned sum) {
  auto_link_stage_t stages[2] = {{.endpoint = split ? RELU0.id : CGRA0.id, .job = 0, .output = {.address = {.binding = -1}}}, {.endpoint = RELU0.id, .job = 1, .output = {.address = {.binding = -1}}}};
  auto_link_edge_t edges[2] = {{.destination = 0, .flags = 3, .address = {.binding = -1}},
                               {.source = 0,
                                .destination = 1,
                                .flags = 5,
                                .bytes = count * sizeof(int32_t),
                                .address = {.value = ADD0.spm, .binding = -1},
                                .source_offset = sum * sizeof(int32_t),
                                .destination_offset = sum * sizeof(int32_t)}};
  auto_link_graph_t graph = {.stage_count = 1, .edge_count = 1, .stage_capacity = AUTO_LINK_GRAPH.stage_capacity, .edge_capacity = AUTO_LINK_GRAPH.edge_capacity, .stages = stages, .edges = edges};
  auto_link_load(&graph, NULL);
  auto_link_tiles(1, 1, 1, 1);
  auto_link_run_config(0);
  stages[0].endpoint = split ? ADD0.id : CGRA0.id;
  stages[0].job = split ? 0 : 1;
  graph.stage_count = split ? 2 : 1;
  graph.edge_count = split ? 2 : 1;
  auto_link_load(&graph, NULL);
  auto_link_run_config(1);
}

static int region_run(int split, unsigned region) {
  const unsigned expected = split && region ? 2 : 1;
  unsigned seen = 0;
  int failed = 0;
  auto_link_run(region, 1);
  for (;;) {
    const unsigned running = *(volatile uint32_t *)(AUTO_LINK_BASE + AUTO_LINK_RUNNING);
    if (!cgra_link_read_at(CGRA0.control, CGRA_LINK_CONTROL_RESULT_VALID)) {
      if (!running)
        break;
      continue;
    }
    const cgra_link_result_t result = cgra_link_wait_at(CGRA0.control);
    const unsigned job = split ? (region ? result.stage : 0) : region;
    if (result.status || result.detail || result.data || result.stage >= expected || result.job != job || (seen & (1u << result.stage)))
      failed = 1;
    seen |= 1u << result.stage;
  }
  return failed || seen != (1u << expected) - 1;
}

#endif
