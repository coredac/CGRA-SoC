#ifndef GCN_PIPE_H
#define GCN_PIPE_H

// Slots retain capacity for 64 channels; each tile uses 16, 32 or 64 channels.
enum { PIPE_WORDS = GCN_MAX_ROWS * 64, PIPE_BYTES = PIPE_WORDS * sizeof(int32_t) };
static int32_t pipe_buffer[2][PIPE_WORDS] __attribute__((aligned(64))) = {{1}, {1}};
static struct {
  unsigned long cycles, overlap, peak;
} pipe_stats;

static void pipe_patch(unsigned command, unsigned source, unsigned scale) {
  const gemmini_patch_t patch = {.command = command, .width = 64, .source = source, .scale = scale, .relative = 1};
  gemmini_job_patch_at(GEMMINI0.control, &patch);
}

static int pipe_project(const gcn_graph_t *graph, unsigned width) {
  enum { B_ROW = 768, ACC_WRITE = UINT32_C(1) << (ADDR_LEN - 1), ACC_READ = ACC_WRITE | (UINT32_C(1) << (ADDR_LEN - 3)), SKIPS = (1 << 3) | (1 << 4) | (1 << 5) | (1 << 7) };
  const unsigned blocks = graph->rows / DIM;
  const unsigned columns = width / DIM;
  const unsigned weights = 5 + blocks * (columns + 1);
  const unsigned stores = weights + 4 + 3;
  if (gemmini_job_capture_at(GEMMINI0.control, 0, stores + blocks * columns, blocks * columns * 2 + 4)) {
    return 1;
  }
  for (unsigned row = 0; row < blocks; ++row) {
    for (unsigned column = 0; column < columns; ++column) {
      pipe_patch(5 + row * columns + column, GEMMINI_VALUE_TILE_ID, width * sizeof(int32_t));
      pipe_patch(stores + row * columns + column, GEMMINI_VALUE_SLOT, PIPE_BYTES);
    }
  }
  for (unsigned channel = 0; channel < 4; ++channel) {
    pipe_patch(weights + channel, GEMMINI_VALUE_TILE_ID, width);
  }
  accel_commands(GEMMINI0, {
    gemmini_config_ex(WEIGHT_STATIONARY, NO_ACTIVATION, 0);
    gemmini_extended3_config_ld(GCN_INPUTS, MVIN_SCALE_IDENTITY, false, 0);
    gemmini_extended3_config_ld(GCN_OUTPUTS, MVIN_SCALE_IDENTITY, false, 1);
    gemmini_extended3_config_ld(0, MVIN_SCALE_IDENTITY, false, 2);
    gemmini_config_st(width * sizeof(acc_t));
    for (unsigned row = 0; row < blocks; ++row) {
      for (unsigned column = 0; column < width; column += DIM) {
        gemmini_extended_mvin3(bias + column, ACC_WRITE + row * width + column, DIM, DIM);
      }
    }
    for (unsigned row = 0; row < blocks; ++row) {
      gemmini_extended_mvin(graph->x + row * DIM * GCN_INPUTS, row * GCN_INPUTS, GCN_INPUTS, DIM);
    }
    for (unsigned channel = 0; channel < GCN_INPUTS; channel += DIM) {
      gemmini_extended_mvin2(gcn_weight + channel * GCN_OUTPUTS, B_ROW + channel * columns, width, DIM);
    }
    gemmini_loop_ws_spad(blocks, columns, GCN_INPUTS / DIM, 0, 0, 0, 0, B_ROW + GCN_INPUTS * columns, 0, 0, false, false, true, false, true, NO_ACTIVATION, 0, 0, false, SKIPS);
    for (unsigned row = 0; row < blocks; ++row) {
      for (unsigned column = 0; column < width; column += DIM) {
        gemmini_extended_mvout(pipe_buffer[0] + row * DIM * width + column, ACC_READ + row * width + column, DIM, DIM);
      }
    }
  });
  stats.gemmini_jobs = 1;
  return gemmini_job_end_at(GEMMINI0.control);
}

static int pipe_aggregate(const gcn_graph_t *graph, unsigned width) {
  cgra_kernel_t kernel = GCN;
  kernel.static_count = 0;
  cgra_packet_t packets[GCN_FAST_CONFIG_PACKET_COUNT];
  cgra_patch_t patches[GCN_SYMBOL_COUNT];
  kernel.config_packets = packets;
  kernel.patches = patches;
  kernel.patch_count = 0;
  for (unsigned index = 0; index < GCN.patch_count; ++index) {
    const cgra_patch_t patch = GCN.patches[index];
    if (patch.symbol_index == GCN_SYMBOL_INPUT0 || patch.symbol_index == GCN_SYMBOL_OUTPUT) {
      patches[kernel.patch_count++] = patch;
    }
  }
  for (unsigned row = 0; row < graph->nodes; ++row) {
    const unsigned start = graph->rowptr[row];
    const unsigned degree = graph->rowptr[row + 1] - start;
    const uint32_t args[] = {PROJECTED, NEIGHBORS + start, WEIGHTS + start, OUTPUT + row * GCN_OUTPUTS, degree, 0, degree * width, width};
    task_packets(args, packets);
    const cgra_link_symbol_t symbols[GCN_SYMBOL_COUNT] = {
        [GCN_SYMBOL_INPUT0] = {PROJECTED, PIPE_WORDS, CGRA_LINK_SLOT},
        [GCN_SYMBOL_OUTPUT] = {OUTPUT + row * GCN_OUTPUTS, width, CGRA_LINK_TILE_ID},
    };
    int failed;
    accel_commands(CGRA0, { failed = cgra_link_capture_at(CGRA0.control, row, &kernel, symbols, row + 1 == graph->nodes ? 0 : row + 2); });
    if (failed) {
      return 1;
    }
  }
  stats.cgra_jobs = graph->nodes;
  return 0;
}

static void pipe_graph(const gcn_graph_t *graph, unsigned width) {
  const auto_link_stage_t stages[] = {
      {.endpoint = GEMMINI0.id, .job = 0, .output = {.address = {.binding = -1}}},
      {.endpoint = CGRA0.id, .job = 0, .output = {.address = {.binding = -1}}},
  };
  const auto_link_edge_t edges[] = {
      {.destination = 0, .flags = 3, .address = {.binding = -1}},
      {.source = 0, .destination = 1, .flags = 5, .bytes = graph->rows * width * sizeof(int32_t), .address = {.value = (uintptr_t)pipe_buffer, .binding = -1}},
  };
  const auto_link_graph_t plan = {.stage_count = 2, .edge_count = 2, .stage_capacity = 3, .edge_capacity = 3, .stages = stages, .edges = edges};
  auto_link_load(&plan, NULL);
  auto_link_transfer(1, 0, PROJECTED * sizeof(int32_t), PIPE_BYTES, PIPE_BYTES, 0);
  auto_link_tiles(GCN_OUTPUTS / width, 1, 1, 1);
  auto_link_run_config(0);
  stats.profiles = 1;
}

static int run_pipeline(const gcn_graph_t *graph, unsigned width) {
  const unsigned long begin = clock_cycles();
  if (load(graph->col, NEIGHBORS, (graph->edges + 3) & ~3) || load(graph->edge_weight, WEIGHTS, (graph->edges + 3) & ~3) || pipe_project(graph, width) || pipe_aggregate(graph, width)) {
    return 1;
  }
  pipe_graph(graph, width);
  stats.setup += clock_cycles() - begin;
  const unsigned long execute = clock_cycles();
  cgra_dma_memory_fence();
  auto_link_run(0, 1);
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
    if (result.stage >= 2 || (seen & (1u << result.stage)) || result.job != 0 || result.status != AUTO_LINK_STATUS_SUCCESS || result.detail || result.data) {
      printf("GCN pipeline report stage=%u job=%u status=%u detail=%u data=%u\n", result.stage, result.job, result.status, result.detail, result.data);
      ++failures;
      continue;
    }
    seen |= 1u << result.stage;
  }
  if (failures || seen != 3) {
    printf("GCN pipeline incomplete reports seen=%u failures=%u\n", seen, failures);
    return 1;
  }
  cgra_dma_memory_fence();
  stats.execute = clock_cycles() - execute;
  pipe_stats.cycles = *(volatile uint64_t *)(AUTO_LINK_BASE + AUTO_LINK_CYCLES);
  pipe_stats.overlap = *(volatile uint64_t *)(AUTO_LINK_BASE + AUTO_LINK_OVERLAP);
  pipe_stats.peak = *(volatile uint64_t *)(AUTO_LINK_BASE + AUTO_LINK_PEAK_ACTIVE);
  stats.batches = 1;
  launches = GCN_OUTPUTS / width * graph->nodes;
  started = 1;
  if (pipe_stats.overlap == 0) {
    printf("GCN pipeline did not overlap across IPs\n");
    return 1;
  }
  return store(graph->nodes * GCN_OUTPUTS);
}

#endif
