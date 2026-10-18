#ifndef GCN_BATCH_H
#define GCN_BATCH_H

enum {
  BATCH_WORDS = GCN_MAX_ROWS * GCN_OUTPUTS,
  BATCH_BYTES = BATCH_WORDS * sizeof(int32_t),
  BATCH_INPUT = GCN_MAX_ROWS * GCN_INPUTS,
  BATCH_NEIGHBORS = 2 * BATCH_WORDS,
  BATCH_WEIGHTS = BATCH_NEIGHBORS + ((GCN_MAX_EDGES + 3) & ~3),
  BATCH_OUTPUT = BATCH_WEIGHTS + ((GCN_MAX_EDGES + 3) & ~3),
};
static int8_t batch_input[2][BATCH_INPUT] __attribute__((aligned(64))) = {{1}, {1}};
static int32_t batch_buffer[2][BATCH_WORDS] __attribute__((aligned(64))) = {{1}, {1}};
static int32_t batch_output[2][BATCH_WORDS] __attribute__((aligned(64))) = {{1}, {1}};

typedef struct {
  unsigned long setup, execute, writeback, total;
} batch_times_t;

static void batch_patch(unsigned command, unsigned source, unsigned scale) {
  const gemmini_patch_t patch = {.command = command, .width = 64, .source = source, .scale = scale, .relative = 1};
  gemmini_job_patch_at(GEMMINI0.control, &patch);
}

static void batch_commands(unsigned blocks) {
  enum { B_ROW = 768, ACC_WRITE = UINT32_C(1) << (ADDR_LEN - 1), ACC_READ = ACC_WRITE | (UINT32_C(1) << (ADDR_LEN - 3)), SKIPS = (1 << 3) | (1 << 4) | (1 << 5) | (1 << 7) };
  gemmini_config_ex(WEIGHT_STATIONARY, NO_ACTIVATION, 0);
  gemmini_extended3_config_ld(GCN_INPUTS, MVIN_SCALE_IDENTITY, false, 0);
  gemmini_extended3_config_ld(GCN_OUTPUTS, MVIN_SCALE_IDENTITY, false, 1);
  gemmini_extended3_config_ld(0, MVIN_SCALE_IDENTITY, false, 2);
  gemmini_config_st(GCN_OUTPUTS * sizeof(acc_t));
  for (unsigned row = 0; row < blocks; ++row) {
    for (unsigned column = 0; column < GCN_OUTPUTS; column += DIM) {
      gemmini_extended_mvin3(bias + column, ACC_WRITE + row * GCN_OUTPUTS + column, DIM, DIM);
    }
  }
  for (unsigned row = 0; row < blocks; ++row) {
    gemmini_extended_mvin(batch_input[0] + row * DIM * GCN_INPUTS, row * GCN_INPUTS, GCN_INPUTS, DIM);
  }
  for (unsigned channel = 0; channel < GCN_INPUTS; channel += DIM) {
    for (unsigned column = 0; column < GCN_OUTPUTS; column += 64) {
      gemmini_extended_mvin2(gcn_weight + channel * GCN_OUTPUTS + column, B_ROW + channel * 8 + column, 64, DIM);
    }
  }
  gemmini_loop_ws_spad(blocks, 8, 4, 0, 0, 0, 0, B_ROW + GCN_INPUTS * 8, 0, 0, false, false, true, false, true, NO_ACTIVATION, 0, 0, false, SKIPS);
  for (unsigned row = 0; row < blocks; ++row) {
    for (unsigned column = 0; column < GCN_OUTPUTS; column += DIM) {
      gemmini_extended_mvout(batch_buffer[0] + row * DIM * GCN_OUTPUTS + column, ACC_READ + row * GCN_OUTPUTS + column, DIM, DIM);
    }
  }
}

static int batch_project(const gcn_graph_t *graph) {
  const unsigned blocks = graph->rows / DIM;
  const unsigned inputs = 5 + blocks * 8;
  const unsigned stores = inputs + blocks + 8 + 3;
  if (gemmini_job_capture_at(GEMMINI0.control, 0, stores + blocks * 8, blocks * 9)) {
    return 1;
  }
  for (unsigned row = 0; row < blocks; ++row) {
    batch_patch(inputs + row, GEMMINI_VALUE_TILE_ID, BATCH_INPUT);
    for (unsigned column = 0; column < 8; ++column) {
      batch_patch(stores + row * 8 + column, GEMMINI_VALUE_SLOT, BATCH_BYTES);
    }
  }
  accel_commands(GEMMINI0, { batch_commands(blocks); });
  return gemmini_job_end_at(GEMMINI0.control);
}

static int batch_aggregate(const gcn_graph_t *graph) {
  const unsigned count = task_count(graph);
  gcn_task_t tasks[count];
  tasks_init(graph, tasks);
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
  for (unsigned job = 0; job < count; ++job) {
    uint32_t *args = tasks[job].args;
    args[1] += BATCH_NEIGHBORS - NEIGHBORS;
    args[2] += BATCH_WEIGHTS - WEIGHTS;
    args[3] += BATCH_OUTPUT - OUTPUT;
    task_packets(args, packets);
    const cgra_link_symbol_t symbols[GCN_SYMBOL_COUNT] = {
        [GCN_SYMBOL_INPUT0] = {PROJECTED, BATCH_WORDS, CGRA_LINK_SLOT},
        [GCN_SYMBOL_OUTPUT] = {args[3], BATCH_WORDS, CGRA_LINK_TILE_ID},
    };
    int failed;
    accel_commands(CGRA0, { failed = cgra_link_capture_at(CGRA0.control, job, &kernel, symbols, job + 1 == count ? 0 : job + 2); });
    if (failed) {
      return 1;
    }
  }
  return 0;
}

static void batch_graph(const gcn_graph_t *graph) {
  const auto_link_stage_t stages[] = {
      {.endpoint = GEMMINI0.id, .job = 0, .output = {.address = {.binding = -1}}},
      {.endpoint = CGRA0.id, .job = 0, .output = {.address = {.binding = -1}}},
  };
  const auto_link_edge_t edges[] = {
      {.destination = 0, .flags = 3, .address = {.binding = -1}},
      {.source = 0, .destination = 1, .flags = 5, .bytes = graph->rows * GCN_OUTPUTS * sizeof(int32_t), .address = {.value = (uintptr_t)batch_buffer, .binding = -1}},
  };
  const auto_link_graph_t plan = {.stage_count = 2, .edge_count = 2, .stage_capacity = 3, .edge_capacity = 3, .stages = stages, .edges = edges};
  auto_link_load(&plan, NULL);
  auto_link_transfer(1, 0, PROJECTED * sizeof(int32_t), BATCH_BYTES, BATCH_BYTES, 0);
  auto_link_tiles(2, 1, 1, 1);
  auto_link_run_config(0);
}

static int batch_execute(void) {
  cgra_dma_memory_fence();
  auto_link_run(0, 1);
  unsigned seen = 0, reports = 0, failures = 0;
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
      printf("GCN batch report stage=%u job=%u status=%u detail=%u data=%u\n", result.stage, result.job, result.status, result.detail, result.data);
      ++failures;
    } else {
      seen |= 1u << result.stage;
    }
    ++reports;
  }
  if (reports != 2 || seen != 3) {
    printf("GCN batch reports=%u seen=%u expected=2/3\n", reports, seen);
    ++failures;
  }
  cgra_dma_memory_fence();
  started = 1;
  return failures != 0;
}

static int batch_store(const gcn_graph_t *graph) {
  for (unsigned input = 0; input < 2; ++input) {
    const cgra_dma_desc_t descriptor =
        ((uint64_t)(BATCH_OUTPUT + input * BATCH_WORDS) << CGRA_DMA_DESC_SPM_ADDR_LSB) | ((uint64_t)(graph->nodes * GCN_OUTPUTS * sizeof(int32_t)) << CGRA_DMA_DESC_NBYTES_LSB);
    uint8_t tag;
    accel_commands(CGRA0, {
      cgra_dma_mvout_async(batch_output[input], descriptor);
      tag = cgra_dma_wait(0);
    });
    if (tag) {
      return 1;
    }
  }
  cgra_dma_memory_fence();
  return 0;
}

static int run_batch(const gcn_graph_t *graph, const gcn_graph_t *alternate, unsigned manual) {
  const gcn_graph_t *inputs[] = {graph, alternate};
  batch_times_t serial = {0};
  for (unsigned input = 0; input < 2; ++input) {
    for (unsigned word = 0; word < BATCH_WORDS; ++word) {
      projected[word] = INT32_MIN;
      output[word] = INT32_MIN;
    }
    if (load(output, OUTPUT, graph->nodes * GCN_OUTPUTS)) {
      return 1;
    }
    memset(&stats, 0, sizeof(stats));
    launches = 0;
    const unsigned long begin = clock_cycles();
    accel_commands(CGRA0, { cgra_send_packets_fast(GCN.static_packets, GCN.static_count); });
    stats.setup = clock_cycles() - begin;
    const int status = manual ? run_manual(inputs[input]) : run_auto(inputs[input], 32);
    if (status) {
      return 1;
    }
    const unsigned long elapsed = clock_cycles() - begin;
    serial.total += elapsed;
    serial.setup += stats.setup;
    serial.execute += stats.execute;
    serial.writeback += stats.writeback;
    if (check(projected, inputs[input]->expected_z, graph->rows * GCN_OUTPUTS, "serial projection") || check(output, inputs[input]->expected_y, graph->nodes * GCN_OUTPUTS, "serial output")) {
      return 1;
    }
    printf("GCN serial mode=%s input=%u cycles=%lu mismatches=0\n", manual ? "manual" : "auto", input, elapsed);
  }
  printf("GCN batch serial mode=%s inputs=2 cycles=%lu setup_cycles=%lu execute_cycles=%lu writeback_cycles=%lu\n", manual ? "manual" : "auto", serial.total, serial.setup, serial.execute,
         serial.writeback);
  for (unsigned input = 0; input < 2; ++input) {
    for (unsigned word = 0; word < BATCH_WORDS; ++word) {
      batch_buffer[input][word] = INT32_MIN;
      batch_output[input][word] = INT32_MIN;
    }
    if (load(batch_output[input], BATCH_OUTPUT + input * BATCH_WORDS, graph->nodes * GCN_OUTPUTS)) {
      return 1;
    }
  }
  const unsigned long begin = clock_cycles();
  for (unsigned input = 0; input < 2; ++input) {
    memcpy(batch_input[input], inputs[input]->x, graph->rows * GCN_INPUTS);
  }
  accel_commands(CGRA0, { cgra_send_packets_fast(GCN.static_packets, GCN.static_count); });
  if (load(graph->col, BATCH_NEIGHBORS, (graph->edges + 3) & ~3) || load(graph->edge_weight, BATCH_WEIGHTS, (graph->edges + 3) & ~3) || batch_project(graph) || batch_aggregate(graph)) {
    return 1;
  }
  batch_graph(graph);
  const unsigned long execute = clock_cycles();
  if (batch_execute()) {
    return 1;
  }
  const unsigned long writeback = clock_cycles();
  if (batch_store(graph)) {
    return 1;
  }
  const unsigned long end = clock_cycles();
  const unsigned long fabric = *(volatile uint64_t *)(AUTO_LINK_BASE + AUTO_LINK_CYCLES);
  const unsigned long overlap = *(volatile uint64_t *)(AUTO_LINK_BASE + AUTO_LINK_OVERLAP);
  const unsigned long peak = *(volatile uint64_t *)(AUTO_LINK_BASE + AUTO_LINK_PEAK_ACTIVE);
  unsigned errors = 0;
  for (unsigned input = 0; input < 2; ++input) {
    errors += check(batch_output[input], inputs[input]->expected_y, graph->nodes * GCN_OUTPUTS, "batch output");
  }
  printf("GCN batch pipeline inputs=2 cycles=%lu setup_cycles=%lu execute_cycles=%lu writeback_cycles=%lu launches=%u mismatches=%u\n", end - begin, execute - begin, writeback - execute,
         end - writeback, 2 * task_count(graph), errors);
  printf("GCN batch fabric_cycles=%lu overlap=%lu peak=%lu; comparison includes capture amortization and full-width projection\n", fabric, overlap, peak);
  if (!overlap) {
    printf("GCN batch did not overlap across IPs\n");
    return 1;
  }
  return errors != 0;
}

#endif
