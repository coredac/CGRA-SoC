#ifndef GCN_GROUP_H
#define GCN_GROUP_H

typedef struct {
  unsigned first, count;
} gcn_group_t;

static unsigned gcn_groups(const gcn_task_t *tasks, unsigned count, gcn_group_t *groups) {
  unsigned used = 0;
  for (unsigned first = 0; first < count;) {
    unsigned end = first + 1;
    // The first task owns the one-time input copies, so its profile has one tile.
    while (first != 0 && end < count) {
      const uint32_t *previous = tasks[end - 1].args;
      const uint32_t *next = tasks[end].args;
      if (next[0] != previous[0] || next[4] != previous[4] || next[5] != previous[5] || next[6] != previous[6] || next[7] != previous[7] || next[1] != previous[1] + previous[4] ||
          next[2] != previous[2] + previous[4] || next[3] != previous[3] + GCN_OUTPUTS) {
        break;
      }
      ++end;
    }
    groups[used++] = (gcn_group_t){first, end - first};
    first = end;
  }
  return used;
}

static int gcn_group_job(unsigned job, const uint32_t *args, unsigned count) {
  cgra_kernel_t kernel = GCN;
  cgra_packet_t packets[GCN_FAST_CONFIG_PACKET_COUNT];
  cgra_patch_t patches[GCN_SYMBOL_COUNT];
  task_packets(args, packets);
  kernel.static_count = 0;
  kernel.config_packets = packets;
  kernel.patches = patches;
  kernel.patch_count = 0;
  for (unsigned index = 0; count > 1 && index < GCN.patch_count; ++index) {
    const cgra_patch_t patch = GCN.patches[index];
    if (patch.symbol_index == GCN_SYMBOL_INPUT1 || patch.symbol_index == GCN_SYMBOL_INPUT2 || patch.symbol_index == GCN_SYMBOL_OUTPUT) {
      patches[kernel.patch_count++] = patch;
    }
  }
  const cgra_link_symbol_t symbols[GCN_SYMBOL_COUNT] = {
      [GCN_SYMBOL_INPUT1] = {args[1], args[4], CGRA_LINK_TILE_ID},
      [GCN_SYMBOL_INPUT2] = {args[2], args[4], CGRA_LINK_TILE_ID},
      [GCN_SYMBOL_OUTPUT] = {args[3], GCN_OUTPUTS, CGRA_LINK_TILE_ID},
  };
  int failed;
  accel_commands(CGRA0, { failed = cgra_link_capture_at(CGRA0.control, job, &kernel, symbols, 0); });
  ++stats.captures;
  stats.packets += kernel.config_count + kernel.launch_count;
  return failed;
}

static int run_grouped(const gcn_graph_t *graph, unsigned capacity) {
  unsigned long begin = clock_cycles();
  const unsigned count = task_count(graph);
  gcn_task_t tasks[count];
  gcn_group_t groups[count];
  tasks_init(graph, tasks);
  const unsigned total = gcn_groups(tasks, count, groups);
  if (gcn_project(graph)) {
    return 1;
  }
  for (unsigned first = 0; first < total;) {
    const unsigned projections = first == 0 ? stats.gemmini_jobs : 0;
    // gpt2.yaml has 32 run profiles, shared by projection and aggregation.
    const unsigned limit = capacity < 32 - projections ? capacity : 32 - projections;
    const unsigned batch = total - first < limit ? total - first : limit;
    for (unsigned job = 0; job < batch; ++job) {
      const gcn_group_t *group = &groups[first + job];
      const unsigned long capture = clock_cycles();
      if (gcn_group_job(job, tasks[group->first].args, group->count)) {
        return 1;
      }
      stats.capture += clock_cycles() - capture;
      gcn_inputs(graph, projections + job, job, group->count, first + job == 0);
      launches += group->count;
    }
    if (batch > stats.cgra_jobs) {
      stats.cgra_jobs = batch;
    }
    stats.setup += clock_cycles() - begin;
    begin = clock_cycles();
    if (gcn_execute(projections, batch)) {
      return 1;
    }
    stats.execute += clock_cycles() - begin;
    begin = clock_cycles();
    first += batch;
  }
  started = 1;
  return store(graph->nodes * GCN_OUTPUTS);
}

#endif
