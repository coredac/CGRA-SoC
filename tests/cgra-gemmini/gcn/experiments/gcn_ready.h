#ifndef GCN_READY_H
#define GCN_READY_H

static int ready_project(const gcn_graph_t *graph, unsigned job) {
  enum { B_ROW = 768, ACC_WRITE = UINT32_C(1) << (ADDR_LEN - 1), ACC_READ = ACC_WRITE | (UINT32_C(1) << (ADDR_LEN - 3)), SKIPS = (1 << 3) | (1 << 4) | (1 << 5) | (1 << 7) };
  if (gemmini_job_capture_at(GEMMINI0.control, job, 33, 0)) {
    return 1;
  }
  accel_commands(GEMMINI0, {
    gemmini_config_ex(WEIGHT_STATIONARY, NO_ACTIVATION, 0);
    gemmini_extended3_config_ld(GCN_INPUTS, MVIN_SCALE_IDENTITY, false, 0);
    gemmini_extended3_config_ld(GCN_OUTPUTS, MVIN_SCALE_IDENTITY, false, 1);
    gemmini_extended3_config_ld(0, MVIN_SCALE_IDENTITY, false, 2);
    gemmini_config_st(GCN_OUTPUTS * sizeof(acc_t));
    for (unsigned column = 0; column < GCN_OUTPUTS; column += DIM) {
      gemmini_extended_mvin3(bias + column, ACC_WRITE + column, DIM, DIM);
    }
    gemmini_extended_mvin(graph->x + job * DIM * GCN_INPUTS, 0, GCN_INPUTS, DIM);
    for (unsigned channel = 0; channel < GCN_INPUTS; channel += DIM) {
      for (unsigned column = 0; column < GCN_OUTPUTS; column += 64) {
        gemmini_extended_mvin2(gcn_weight + channel * GCN_OUTPUTS + column, B_ROW + channel * 8 + column, 64, DIM);
      }
    }
    gemmini_loop_ws_spad(1, 8, 4, 0, 0, 0, 0, B_ROW + GCN_INPUTS * 8, 0, 0, false, false, true, false, true, NO_ACTIVATION, 0, 0, false, SKIPS);
    for (unsigned column = 0; column < GCN_OUTPUTS; column += DIM) {
      gemmini_extended_mvout(projected + job * DIM * GCN_OUTPUTS + column, ACC_READ + column, DIM, DIM);
    }
  });
  return gemmini_job_end_at(GEMMINI0.control);
}

static int ready_task(const gcn_graph_t *graph, const gcn_task_t *task) {
  const unsigned start = task->args[1] - NEIGHBORS;
  for (unsigned edge = start; edge < start + task->args[4]; ++edge) {
    if (graph->col[edge] >= DIM) {
      return 0;
    }
  }
  return 1;
}

static int ready_chain(const gcn_task_t *tasks, unsigned first, unsigned count) {
  cgra_kernel_t kernel = GCN;
  kernel.static_count = 0;
  cgra_packet_t packets[GCN_FAST_CONFIG_PACKET_COUNT];
  kernel.config_packets = packets;
  for (unsigned index = 0; index < count; ++index) {
    const unsigned job = first + index;
    task_packets(tasks[job].args, packets);
    int failed;
    accel_commands(CGRA0, { failed = cgra_link_capture_at(CGRA0.control, job, &kernel, NULL, index + 1 == count ? 0 : job + 2); });
    if (failed) {
      return 1;
    }
  }
  return 0;
}

static void ready_graph(unsigned early, int serial) {
  const auto_link_stage_t stages[] = {
      {.endpoint = GEMMINI0.id, .job = 0, .output = {.address = {.binding = -1}}},
      {.endpoint = GEMMINI0.id, .job = 1, .output = {.address = {.binding = -1}}},
      {.endpoint = CGRA0.id, .job = 0, .output = {.address = {.binding = -1}}},
  };
  const auto_link_edge_t edges[] = {
      {.destination = 0, .flags = 3, .address = {.binding = -1}},
      {.source = serial ? 2 : 0, .destination = 1, .flags = 1, .address = {.binding = -1}},
      {.source = 0, .destination = 2, .flags = 5, .bytes = DIM * GCN_OUTPUTS * sizeof(int32_t), .address = {.value = (uintptr_t)projected, .binding = -1}},
  };
  const auto_link_graph_t plan = {.stage_count = 3, .edge_count = 3, .stage_capacity = 3, .edge_capacity = 3, .stages = stages, .edges = edges};
  auto_link_load(&plan, NULL);
  auto_link_tiles(1, 1, 1, 1);
  auto_link_run_config(0);
  // Profile drain joins the second projection and early aggregation; retain the first half of Z.
  const auto_link_edge_t tail = {.destination = 0,
                                 .flags = 7,
                                 .bytes = DIM * GCN_OUTPUTS * sizeof(int32_t),
                                 .address = {.value = (uintptr_t)(projected + DIM * GCN_OUTPUTS), .binding = -1},
                                 .destination_offset = (PROJECTED + DIM * GCN_OUTPUTS) * sizeof(int32_t)};
  gcn_profile(1, CGRA0, early, &tail, 1);
}

static int ready_execute(unsigned early) {
  cgra_dma_memory_fence();
  auto_link_run(0, 2);
  unsigned seen = 0, failures = 0;
  for (;;) {
    const unsigned running = *(volatile uint32_t *)(AUTO_LINK_BASE + AUTO_LINK_RUNNING);
    if (!cgra_link_read_at(CGRA0.control, CGRA_LINK_CONTROL_RESULT_VALID)) {
      if (!running) {
        break;
      }
      continue;
    }
    const cgra_link_result_t result = cgra_link_wait_at(CGRA0.control);
    unsigned bit = 0;
    if (result.stage == 0 && result.job == 0) {
      bit = 1;
    } else if (result.stage == 1 && result.job == 1) {
      bit = 2;
    } else if (result.stage == 2 && result.job == 0) {
      bit = 4;
    } else if (result.stage == 0 && result.job == early) {
      bit = 8;
    }
    if (!bit || (seen & bit) || result.status != AUTO_LINK_STATUS_SUCCESS || result.detail || result.data) {
      printf("GCN ready report stage=%u job=%u status=%u detail=%u data=%u\n", result.stage, result.job, result.status, result.detail, result.data);
      ++failures;
    }
    seen |= bit;
  }
  cgra_dma_memory_fence();
  if (seen != 15) {
    printf("GCN ready incomplete reports seen=%u expected=15\n", seen);
    ++failures;
  }
  return failures != 0;
}

static int run_ready(const gcn_graph_t *graph, int serial) {
  const unsigned long begin = clock_cycles();
  const unsigned count = task_count(graph);
  gcn_task_t tasks[count], ordered[count];
  tasks_init(graph, tasks);
  unsigned early = 0;
  for (unsigned index = 0; index < count; ++index) {
    if (ready_task(graph, &tasks[index])) {
      ordered[early++] = tasks[index];
    }
  }
  unsigned next = early;
  for (unsigned index = 0; index < count; ++index) {
    if (!ready_task(graph, &tasks[index])) {
      ordered[next++] = tasks[index];
    }
  }
  if (load(graph->col, NEIGHBORS, (graph->edges + 3) & ~3) || load(graph->edge_weight, WEIGHTS, (graph->edges + 3) & ~3) || ready_project(graph, 0) || ready_project(graph, 1) ||
      ready_chain(ordered, 0, early) || ready_chain(ordered, early, count - early)) {
    return 1;
  }
  ready_graph(early, serial);
  stats.setup += clock_cycles() - begin;
  const unsigned long execute = clock_cycles();
  const int failed = ready_execute(early);
  stats.execute = clock_cycles() - execute;
  stats.batches = 1;
  stats.gemmini_jobs = 2;
  stats.cgra_jobs = count;
  stats.profiles = 2;
  launches = count;
  started = 1;
  return failed || store(graph->nodes * GCN_OUTPUTS);
}

#endif
