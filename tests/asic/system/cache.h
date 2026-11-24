#ifndef ASIC_SYSTEM_CACHE_H
#define ASIC_SYSTEM_CACHE_H

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
  const auto_link_stage_t stage = {.endpoint = CGRA0.id, .job = full ? 0u : 1u, .output = {.address = {.binding = -1}}};
  const auto_link_edge_t edge = {.destination = 0, .flags = 3, .address = {.binding = -1}};
  const auto_link_graph_t graph = {
      .stage_count = 1, .edge_count = 1, .stage_capacity = AUTO_LINK_GRAPH.stage_capacity, .edge_capacity = AUTO_LINK_GRAPH.edge_capacity, .stages = &stage, .edges = &edge};
  auto_link_load(&graph, NULL);
  auto_link_tiles(1, 1, 1, 1);
  return 0;
}

static int cache_run(unsigned job) {
  auto_link_job(0, job);
  auto_link_input_ready();
  const cgra_link_result_t result = cgra_link_wait_at(CGRA0.control);
  while (*(volatile uint32_t *)(AUTO_LINK_BASE + AUTO_LINK_RUNNING)) {
  }
  return result.status != AUTO_LINK_STATUS_SUCCESS || result.detail || result.data || result.stage || result.job != job;
}

#endif
