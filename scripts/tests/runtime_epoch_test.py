"""Check compiler-directed cache reuse after automatic runs drain."""

import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

PREFIX = r"""
#include "soc_runtime.h"
#define ACCEL_H
#include "internal/driver.h"
#include <stdio.h>
#include <string.h>

static unsigned long clock_value;
static uint32_t running;
#define AUTO_LINK_BASE ((uintptr_t)&running)
#define AUTO_LINK_RUNNING 0
#define AUTO_LINK_STATUS_SUCCESS 0
#define CGRA_LINK_CONTROL_RESULT_VALID 0

typedef struct { uint32_t status, detail, data, stage, job; } cgra_link_result_t;
static unsigned events[64], event_count, failures, queued, current;
static unsigned profile_jobs[4], results[4], loaded_job;
static int fail_capture = -1;
static const uintptr_t *expected_bindings;
static unsigned expected_binding_count;
static unsigned stage_limits[16], edge_limits[16], load_count, check_graph;
static unsigned stage_live[8], edge_live[8];
static unsigned tile_loads;
static void record(unsigned event) { events[event_count++] = event; }
static void idle(void) { if (running || current != queued) ++failures; }
static unsigned task_id(const soc_task_t *task) { return (uintptr_t)task->parameters; }
static int init(const soc_context_t *context, const soc_task_t *task) {
  (void)context; idle(); record(50 + task_id(task)); return 0;
}
static int capture(const soc_context_t *context, const soc_task_t *task) {
  (void)context; idle(); record(100 + task_id(task)); clock_value += 20;
  return task_id(task) == (unsigned)fail_capture;
}
static int execute(soc_context_t *context, const soc_task_t *task, unsigned tile) {
  (void)context; (void)tile; idle(); record(600 + task_id(task)); return 0;
}
static void auto_link_load(const auto_link_graph_t *graph, const uintptr_t *bindings) {
  idle(); loaded_job = graph->stages[0].job;
  stage_limits[load_count] = graph->stage_capacity;
  edge_limits[load_count++] = graph->edge_capacity;
  if (check_graph) {
    for (unsigned i = 0; i < graph->stage_capacity; ++i) stage_live[i] = 0;
    for (unsigned i = 0; i < graph->edge_capacity; ++i) edge_live[i] = 0;
    for (unsigned i = 0; i < graph->stage_count; ++i) stage_live[i] = 1;
    for (unsigned i = 0; i < graph->edge_count; ++i) edge_live[i] = 1;
    for (unsigned i = 0; i < 5; ++i) if (stage_live[i] != (i < graph->stage_count)) ++failures;
    for (unsigned i = 0; i < 7; ++i) if (edge_live[i] != (i < graph->edge_count)) ++failures;
  }
  if (expected_bindings && memcmp(bindings, expected_bindings, expected_binding_count * sizeof(*bindings))) ++failures;
}
static void auto_link_update(const auto_link_graph_t *graph, const auto_link_graph_t *previous, const uintptr_t *bindings) {
  auto_link_graph_t next = *graph;
  if (previous) {
    next.stage_capacity = previous->stage_count;
    next.edge_capacity = previous->edge_count;
  }
  auto_link_load(&next, bindings);
}
static void auto_link_tiles(unsigned tiles, unsigned rows, unsigned columns, unsigned stride) {
  (void)rows; (void)columns; (void)stride;
  (void)tiles; ++tile_loads;
}
static void auto_link_run_config(unsigned profile) {
  idle(); profile_jobs[profile] = loaded_job; record(200 + profile);
}
static void auto_link_run(unsigned first, unsigned count) {
  idle(); record(300 + count); current = 0; queued = count; running = 1;
  for (unsigned i = 0; i < count; ++i) results[i] = profile_jobs[first + i];
}
static unsigned cgra_link_read_at(uintptr_t base, unsigned offset) {
  (void)base; (void)offset;
  if (current < queued) return 1;
  if (running) { record(500); running = 0; }
  return 0;
}
static cgra_link_result_t cgra_link_wait_at(uintptr_t base) {
  (void)base; unsigned job = results[current++]; record(400 + job);
  return (cgra_link_result_t){.job = job};
}
int soc_store(const soc_context_t *context, const soc_output_t *output) {
  (void)context; (void)output; idle(); record(700); return 0;
}
"""

SUFFIX = r"""
static int equal(const unsigned *expected, unsigned count) {
  return !failures && event_count == count && !memcmp(events, expected, count * sizeof(*events));
}
static void reset(void) {
  memset(events, 0, sizeof(events)); event_count = failures = queued = current = running = 0;
  fail_capture = -1; expected_bindings = NULL; expected_binding_count = 0;
  load_count = check_graph = tile_loads = 0;
}
int main(void) {
  const soc_driver_t driver = {.init = init, .capture = capture, .execute = execute};
  const accel_t device = {0};
  const accel_t *devices[] = {&device};
  const soc_task_t tasks[] = {{.driver = &driver}, {.driver = &driver, .job = 1, .parameters = (void *)1}, {.driver = &driver, .parameters = (void *)2}};
  const unsigned ids[] = {0, 1, 2};
  const auto_link_stage_t stages[] = {{.job = 0}, {.job = 1}, {.job = 0}};
  const auto_link_graph_t graphs[] = {{.stage_count = 1, .stages = &stages[0]}, {.stage_count = 1, .stages = &stages[1]}, {.stage_count = 1, .stages = &stages[2]}};
  const soc_prep_t global[] = {{.kind = SOC_INIT, .index = 0}};
  const soc_prep_t first[] = {{.kind = SOC_CAPTURE, .index = 0}, {.kind = SOC_PROFILE, .index = 0}, {.kind = SOC_CAPTURE, .index = 1}, {.kind = SOC_PROFILE, .index = 1}};
  const soc_prep_t second[] = {{.kind = SOC_CAPTURE, .index = 2}, {.kind = SOC_PROFILE, .index = 2}};
  soc_step_t steps[] = {
    {.automatic = 1, .tiles = 1, .task_count = 1, .tasks = &ids[0], .graph = &graphs[0], .prep_count = 4, .prep = first},
    {.automatic = 1, .tiles = 1, .task_count = 1, .tasks = &ids[1], .profile = 1, .graph = &graphs[1]},
    {.automatic = 1, .tiles = 1, .task_count = 1, .tasks = &ids[2], .graph = &graphs[2], .prep_count = 2, .prep = second}
  };
  soc_plan_t plan = {.device_count = 1, .devices = devices, .task_count = 3, .tasks = tasks, .prep_count = 1, .prep = global, .step_count = 3, .steps = steps};
  soc_context_t context = {0};
  if (soc_prepare(&context, &plan, NULL) || soc_execute_auto(&context)) return 1;
  const unsigned epochs[] = {50,100,200,101,201,302,400,401,500,102,200,301,400,500};
  if (!equal(epochs, sizeof(epochs)/sizeof(*epochs)) || context.stats.automatic_launches != 2 || context.stats.cpu_task_submissions || context.stats.execute_cycles < 60 ||
      context.stats.prepare_cycles != 1 || context.stats.total_cycles != context.stats.prepare_cycles + context.stats.execute_cycles) return 2;

  reset(); fail_capture = 2;
  if (soc_prepare(&context, &plan, NULL) || !soc_execute_auto(&context)) return 3;
  if (event_count != 10 || events[8] != 500 || events[9] != 102 || context.stats.automatic_launches != 1 || context.stats.cpu_task_submissions || failures) return 4;

  reset(); plan.step_count = 2; plan.prep_count = 4; plan.prep = first; steps[0].prep_count = 0;
  if (soc_prepare(&context, &plan, NULL) || soc_execute_auto(&context)) return 5;
  const unsigned resident[] = {100,200,101,201,302,400,401,500};
  if (!equal(resident, sizeof(resident)/sizeof(*resident)) || context.stats.automatic_launches != 1 || context.stats.cpu_task_submissions) return 6;

  reset(); plan.step_count = 3; plan.prep_count = 1; plan.prep = global; steps[0].prep_count = 4;
  const soc_prep_t cpu_prep[] = {{.kind = SOC_INIT, .index = 2}};
  steps[2].automatic = 0; steps[2].prep_count = 1; steps[2].prep = cpu_prep;
  if (soc_prepare(&context, &plan, NULL) || soc_execute_hybrid(&context)) return 7;
  const unsigned hybrid[] = {50,100,200,101,201,301,400,500,301,401,500,52,602};
  if (!equal(hybrid, sizeof(hybrid)/sizeof(*hybrid)) || context.stats.automatic_launches != 2 || context.stats.cpu_task_submissions != 1) return 8;
  reset();
  plan.step_count = 2; plan.argument_count = 2; plan.workspace_bytes = 64; plan.workspace_alignment = 32;
  const uintptr_t arguments[] = {0x1200, 0x2400};
  if (!soc_prepare(&context, &plan, arguments) || event_count) return 9;
  context.workspace = 0x1000; context.workspace_bytes = 63;
  if (!soc_prepare(&context, &plan, arguments) || event_count) return 10;
  context.workspace = 0x1001; context.workspace_bytes = 64;
  if (!soc_prepare(&context, &plan, arguments) || event_count) return 11;
  context.workspace = 0x1000;
  const uintptr_t bindings[] = {0, 0x1200, 0x2400, 0x1000};
  expected_bindings = bindings; expected_binding_count = 4;
  if (soc_prepare(&context, &plan, arguments) || soc_execute_auto(&context) || failures) return 12;
  if (soc_operand(&context, (soc_operand_t){.base = SOC_WORKSPACE, .value = 20}) != 0x1014) return 13;

  reset(); check_graph = 1;
  const auto_link_stage_t wide_stages[4] = {{0}};
  const auto_link_edge_t wide_edges[6] = {{0}};
  const auto_link_graph_t varied[] = {
    {.stage_count = 3, .edge_count = 6, .stage_capacity = 5, .edge_capacity = 7, .stages = wide_stages, .edges = wide_edges},
    {.stage_count = 1, .edge_count = 1, .stage_capacity = 5, .edge_capacity = 7, .stages = wide_stages, .edges = wide_edges},
    {.stage_count = 4, .edge_count = 5, .stage_capacity = 5, .edge_capacity = 7, .stages = wide_stages, .edges = wide_edges}
  };
  for (unsigned i = 0; i < 3; ++i) steps[i].graph = &varied[i];
  const soc_prep_t profiles[] = {{.kind = SOC_PROFILE, .index = 0}, {.kind = SOC_PROFILE, .index = 1}, {.kind = SOC_PROFILE, .index = 2}};
  plan.prep = profiles; plan.prep_count = 3;
  // An external API may dirty staging before each new prepare, including a mode switch.
  for (unsigned round = 0; round < 2; ++round) {
    for (unsigned i = 0; i < 8; ++i) stage_live[i] = edge_live[i] = 1;
    load_count = tile_loads = 0;
    plan.prep_count = round ? 1 : 3;
    if (soc_prepare(&context, &plan, arguments)) return 14;
    if (round && prepare_operations(&context, profiles + 1, 2)) return 14;
    if (failures || load_count != 3 || tile_loads != 1) return 14;
    const unsigned expected_stages[] = {5, 3, 1}, expected_edges[] = {7, 6, 1};
    if (memcmp(stage_limits, expected_stages, sizeof(expected_stages)) || memcmp(edge_limits, expected_edges, sizeof(expected_edges))) return 15;
  }

  return 0;
}
"""


class RuntimeEpochTest(unittest.TestCase):
    def test_deferred_preparation(self):
        runtime = (ROOT / "runtime/src/soc_runtime.c").read_text()
        for header in ["internal/driver.h", "auto_link.h", "cgra_link.h"]:
            runtime = runtime.replace(f'#include "{header}"', "")
        runtime = runtime.replace(
            '__asm__ volatile("rdcycle %0" : "=r"(value)::"memory");',
            "value = ++clock_value;",
        ).replace('__asm__ volatile("fence iorw, iorw" ::: "memory");', "")
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)
            source = path / "test.c"
            source.write_text(PREFIX + runtime + SUFFIX)
            binary = path / "test"
            subprocess.run(
                [
                    "cc",
                    "-std=gnu11",
                    "-O2",
                    "-Wall",
                    "-Wextra",
                    "-Werror",
                    "-fsanitize=undefined",
                    "-fno-sanitize-recover=all",
                    "-I",
                    str(ROOT / "runtime/include"),
                    "-I",
                    str(ROOT / "tests/include"),
                    str(source),
                    "-o",
                    str(binary),
                ],
                check=True,
            )
            subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    unittest.main()
