"""Compare differential AutoLink staging with the full MMIO loader."""

import re
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

PREFIX = r"""
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "auto_link_generated.h"
#include "cgra_link_control_generated.h"
enum { STAGES = 4, EDGES = 5 };
typedef struct {
  unsigned stage, edge;
  uint8_t stages[STAGES][256], edges[EDGES][256];
  uint64_t regions[STAGES][8], transfers[EDGES][5];
} bank_t;
static bank_t bank;
static unsigned writes;
static void write_mmio(uintptr_t address, uint64_t value, unsigned bytes) {
  const uintptr_t offset = address - AUTO_LINK_BASE;
  ++writes;
  if (offset == AUTO_LINK_STAGE) { bank.stage = value; return; }
  if (offset == AUTO_LINK_EDGE) { bank.edge = value; return; }
  if (offset >= AUTO_LINK_REGION_BASE) {
    const unsigned slot = (offset - AUTO_LINK_REGION_BASE) / AUTO_LINK_REGION_STRIDE;
    const unsigned field = (offset - AUTO_LINK_REGION_BASE) % AUTO_LINK_REGION_STRIDE / 8;
    bank.regions[slot][field] = value;
    return;
  }
  if (offset >= AUTO_LINK_TRANSFER_BASE) {
    const unsigned slot = (offset - AUTO_LINK_TRANSFER_BASE) / AUTO_LINK_TRANSFER_STRIDE;
    const unsigned field = (offset - AUTO_LINK_TRANSFER_BASE) % AUTO_LINK_TRANSFER_STRIDE / 8;
    bank.transfers[slot][field] = value;
    return;
  }
  uint8_t *fields = offset >= AUTO_LINK_EDGE_SOURCE && offset <= AUTO_LINK_EDGE_SOURCE_BASE
    ? bank.edges[bank.edge] : bank.stages[bank.stage];
  memcpy(fields + offset, &value, bytes);
}
"""

SUFFIX = r"""
static int compare(const auto_link_graph_t *graph, const bank_t *expected) {
  for (unsigned index = 0; index < STAGES; ++index) {
    if (index < graph->stage_count) {
      if (memcmp(bank.stages[index], expected->stages[index], sizeof(bank.stages[index])) ||
          memcmp(bank.regions[index], expected->regions[index], sizeof(bank.regions[index]))) return 1;
    } else if (memcmp(bank.stages[index] + AUTO_LINK_ENABLE,
                      expected->stages[index] + AUTO_LINK_ENABLE, 4)) return 2;
  }
  for (unsigned index = 0; index < EDGES; ++index) {
    if (index < graph->edge_count) {
      if (memcmp(bank.edges[index], expected->edges[index], sizeof(bank.edges[index])) ||
          memcmp(bank.transfers[index], expected->transfers[index], sizeof(bank.transfers[index]))) return 3;
    } else if (memcmp(bank.edges[index] + AUTO_LINK_EDGE_FLAGS,
                      expected->edges[index] + AUTO_LINK_EDGE_FLAGS, 4)) return 4;
  }
  return 0;
}

static int step(const auto_link_graph_t *graph, const auto_link_graph_t *previous,
                const uintptr_t *bindings, int cheaper) {
  const bank_t before = bank;
  writes = 0;
  auto_link_load(graph, bindings);
  const bank_t expected = bank;
  const unsigned full_writes = writes;
  bank = before;
  writes = 0;
  auto_link_update(graph, previous, bindings);
  const int mismatch = compare(graph, &expected);
  if (mismatch || (cheaper && writes >= full_writes)) {
    fprintf(stderr, "profile mismatch=%d writes=%u/%u\n", mismatch, writes, full_writes);
    return 1;
  }
  return 0;
}

static void change_stage(auto_link_stage_t *stage, unsigned field) {
  switch (field) {
  case 0: ++stage->endpoint; break;
  case 1: ++stage->job; break;
  case 2: ++stage->output.flags; break;
  case 3: ++stage->output.pixel_bytes; break;
  case 4: stage->output.address = (auto_link_address_t){.value = UINT64_C(0x123456789a), .binding = -1}; break;
  case 5: stage->output.stride += UINT64_C(0x100000001); break;
  case 6: ++stage->output.bytes; break;
  }
}

static void change_edge(auto_link_edge_t *edge, unsigned field) {
  switch (field) {
  case 0: ++edge->source; break;
  case 1: ++edge->destination; break;
  case 2: ++edge->flags; break;
  case 3: ++edge->bytes; break;
  case 4: ++edge->expansion; break;
  case 5: edge->address = (auto_link_address_t){.value = UINT64_C(0xabcdef1234), .binding = -1}; break;
  case 6: edge->source_offset += UINT64_C(0x100000001); break;
  case 7: edge->destination_offset += UINT64_C(0x100000003); break;
  case 8: edge->source_stride += UINT64_C(0x100000005); break;
  case 9: edge->destination_stride += UINT64_C(0x100000007); break;
  case 10: ++edge->pixel_bytes; break;
  }
}

int main(void) {
  uintptr_t bindings[] = {UINT64_C(0x1234500000), UINT64_C(0x2345600000)};
  auto_link_stage_t stages[STAGES] = {0}, next_stages[STAGES];
  auto_link_edge_t edges[EDGES] = {0}, next_edges[EDGES];
  for (unsigned index = 0; index < STAGES; ++index) {
    stages[index] = (auto_link_stage_t){.endpoint = index + 1, .job = index + 3,
      .output = {.flags = index + 1, .pixel_bytes = 4, .address = {.binding = index % 2}, .stride = 64, .bytes = 32}};
  }
  for (unsigned index = 0; index < EDGES; ++index) {
    edges[index] = (auto_link_edge_t){.source = index % STAGES, .destination = (index + 1) % STAGES,
      .flags = 3, .bytes = 32, .expansion = 2, .address = {.binding = index % 2},
      .source_offset = index * 16, .destination_offset = index * 32,
      .source_stride = 64, .destination_stride = 128, .pixel_bytes = 4};
  }
  auto_link_graph_t previous = {.stage_count = STAGES, .edge_count = EDGES,
    .stage_capacity = STAGES, .edge_capacity = EDGES, .stages = stages, .edges = edges};
  memset(&bank, 0xa5, sizeof(bank));
  if (step(&previous, NULL, bindings, 0) || step(&previous, &previous, bindings, 1) ||
      writes != STAGES + EDGES) return 1;

  for (unsigned field = 0; field < 18; ++field) {
    memcpy(next_stages, stages, sizeof(stages));
    memcpy(next_edges, edges, sizeof(edges));
    if (field < 7) change_stage(&next_stages[1], field);
    else change_edge(&next_edges[2], field - 7);
    const auto_link_graph_t next = {.stage_count = STAGES, .edge_count = EDGES,
      .stage_capacity = STAGES, .edge_capacity = EDGES, .stages = next_stages, .edges = next_edges};
    if (step(&next, &previous, bindings, 1) || writes != STAGES + EDGES + 1) return 2;
    memcpy(stages, next_stages, sizeof(stages));
    memcpy(edges, next_edges, sizeof(edges));
  }

  const auto_link_graph_t small = {.stage_count = 1, .edge_count = 1,
    .stage_capacity = STAGES, .edge_capacity = EDGES, .stages = stages, .edges = edges};
  if (step(&small, &previous, bindings, 1)) return 3;
  // Growing slots must overwrite all dirty fields, including regions and zeros.
  for (unsigned index = 1; index < STAGES; ++index) {
    memset(bank.stages[index], 0x5a, sizeof(bank.stages[index]));
    memset(bank.regions[index], 0x5a, sizeof(bank.regions[index]));
  }
  for (unsigned index = 1; index < EDGES; ++index) {
    memset(bank.edges[index], 0x5a, sizeof(bank.edges[index]));
    memset(bank.transfers[index], 0x5a, sizeof(bank.transfers[index]));
  }
  memset(stages + 1, 0, (STAGES - 1) * sizeof(*stages));
  memset(edges + 1, 0, (EDGES - 1) * sizeof(*edges));
  for (unsigned index = 1; index < STAGES; ++index) stages[index].output.address.binding = -1;
  for (unsigned index = 1; index < EDGES; ++index) edges[index].address.binding = -1;
  if (step(&previous, &small, bindings, 1)) return 4;
  bindings[0] += UINT64_C(0x100000000);
  bindings[1] += UINT64_C(0x200000000);
  memset(&bank, 0xa5, sizeof(bank));
  if (step(&previous, NULL, bindings, 0)) return 5;
  puts("profile MMIO oracle: PASS (identical, 18 fields, shrink/grow, dirty reset, rebinding)");
  return 0;
}
"""


class RuntimeProfileTest(unittest.TestCase):
    def test_staging_updates(self):
        header = (ROOT / "tests/include/auto_link.h").read_text()
        header = re.sub(
            r"\*\(volatile uint(32|64)_t \*\)\(([^;]+)\) = ([^;]+);",
            lambda match: f"write_mmio({match[2]}, {match[3]}, {int(match[1]) // 8});",
            header,
        )
        header = re.sub(
            r"fields\[(\d+)\] = ([^;]+);",
            r"write_mmio((uintptr_t)&fields[\1], \2, 8);",
            header,
        ).replace('__asm__ volatile("fence iorw, iorw" ::: "memory");', "")
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)
            source = path / "profile.c"
            source.write_text(PREFIX + header + SUFFIX)
            binary = path / "profile"
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
                    f"-I{ROOT / 'tests/include'}",
                    str(source),
                    "-o",
                    str(binary),
                ],
                check=True,
            )
            subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    unittest.main()
