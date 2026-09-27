#ifndef AUTO_LINK_H
#define AUTO_LINK_H

#include "auto_link_generated.h"
#include "cgra_link_control_generated.h"

#include <stdint.h>

static inline void auto_link_transfer(unsigned task, uintptr_t source_offset, uintptr_t destination_offset, uintptr_t source_stride, uintptr_t destination_stride, uint32_t bytes_per_pixel) {
  volatile uint64_t *fields = (volatile uint64_t *)(AUTO_LINK_BASE + AUTO_LINK_TRANSFER_BASE + task * AUTO_LINK_TRANSFER_STRIDE);
  fields[0] = source_offset;
  fields[1] = destination_offset;
  fields[2] = source_stride;
  fields[3] = destination_stride;
  fields[4] = bytes_per_pixel;
}

static inline void auto_link_region(unsigned stage, uint32_t rows, uint32_t columns, uint32_t row_step, uint32_t column_step, uint32_t top, int32_t bottom, uint32_t left, int32_t right) {
  volatile uint64_t *fields = (volatile uint64_t *)(AUTO_LINK_BASE + AUTO_LINK_REGION_BASE + stage * AUTO_LINK_REGION_STRIDE);
  fields[0] = rows;
  fields[1] = columns;
  fields[2] = row_step;
  fields[3] = column_step;
  fields[4] = top;
  fields[5] = (uint32_t)bottom;
  fields[6] = left;
  fields[7] = (uint32_t)right;
}

static inline void auto_link_tiles(uint32_t rows, uint32_t columns, uint32_t tile_rows, uint32_t tile_columns) {
  *(volatile uint32_t *)(AUTO_LINK_BASE + AUTO_LINK_ROWS) = rows;
  *(volatile uint32_t *)(AUTO_LINK_BASE + AUTO_LINK_COLUMNS) = columns;
  *(volatile uint32_t *)(AUTO_LINK_BASE + AUTO_LINK_TILE_ROWS) = tile_rows;
  *(volatile uint32_t *)(AUTO_LINK_BASE + AUTO_LINK_TILE_COLUMNS) = tile_columns;
}

static inline void auto_link_input_ready(void) {
  *(volatile uint32_t *)(AUTO_LINK_BASE + AUTO_LINK_INPUT_READY) = 1;
  __asm__ volatile("fence iorw, iorw" ::: "memory");
}

static inline void auto_link_job(uint32_t stage, uint32_t job) {
  *(volatile uint32_t *)(AUTO_LINK_BASE + AUTO_LINK_STAGE) = stage;
  *(volatile uint32_t *)(AUTO_LINK_BASE + AUTO_LINK_JOB) = job;
}

static inline uintptr_t auto_link_address(auto_link_address_t address, const uintptr_t *bindings) { return address.binding < 0 ? address.value : bindings[address.binding]; }

static inline void auto_link_load(const auto_link_graph_t *graph, const uintptr_t *bindings) {
  for (unsigned index = 0; index < graph->stage_capacity; ++index) {
    *(volatile uint32_t *)(AUTO_LINK_BASE + AUTO_LINK_STAGE) = index;
    *(volatile uint32_t *)(AUTO_LINK_BASE + AUTO_LINK_ENABLE) = 0;
  }
  for (unsigned index = 0; index < graph->edge_capacity; ++index) {
    *(volatile uint32_t *)(AUTO_LINK_BASE + AUTO_LINK_EDGE) = index;
    *(volatile uint32_t *)(AUTO_LINK_BASE + AUTO_LINK_EDGE_FLAGS) = 0;
  }
  for (unsigned index = 0; index < graph->stage_count; ++index) {
    const auto_link_stage_t *stage = &graph->stages[index];
    const auto_link_output_t *output = &stage->output;
    auto_link_job(index, stage->job);
    auto_link_region(index, 0, 0, 0, 0, 0, 0, 0, 0);
    *(volatile uint32_t *)(AUTO_LINK_BASE + AUTO_LINK_ENDPOINT) = stage->endpoint;
    *(volatile uint32_t *)(AUTO_LINK_BASE + AUTO_LINK_OUTPUT_FLAGS) = output->flags;
    *(volatile uint32_t *)(AUTO_LINK_BASE + AUTO_LINK_OUTPUT_PIXEL_BYTES) = output->pixel_bytes;
    *(volatile uint64_t *)(AUTO_LINK_BASE + AUTO_LINK_OUTPUT_ADDRESS) = auto_link_address(output->address, bindings);
    *(volatile uint64_t *)(AUTO_LINK_BASE + AUTO_LINK_OUTPUT_STRIDE) = output->stride;
    *(volatile uint32_t *)(AUTO_LINK_BASE + AUTO_LINK_OUTPUT_BYTES) = output->bytes;
    *(volatile uint32_t *)(AUTO_LINK_BASE + AUTO_LINK_ENABLE) = 1;
  }
  for (unsigned index = 0; index < graph->edge_count; ++index) {
    const auto_link_edge_t *edge = &graph->edges[index];
    *(volatile uint32_t *)(AUTO_LINK_BASE + AUTO_LINK_EDGE) = index;
    *(volatile uint32_t *)(AUTO_LINK_BASE + AUTO_LINK_EDGE_SOURCE) = edge->source;
    *(volatile uint32_t *)(AUTO_LINK_BASE + AUTO_LINK_EDGE_DESTINATION) = edge->destination;
    *(volatile uint32_t *)(AUTO_LINK_BASE + AUTO_LINK_EDGE_BYTES) = edge->bytes;
    *(volatile uint32_t *)(AUTO_LINK_BASE + AUTO_LINK_EDGE_EXPANSION) = edge->expansion;
    *(volatile uint64_t *)(AUTO_LINK_BASE + AUTO_LINK_EDGE_SOURCE_BASE) = auto_link_address(edge->address, bindings);
    auto_link_transfer(index, edge->source_offset, edge->destination_offset, edge->source_stride, 0, edge->pixel_bytes);
    *(volatile uint32_t *)(AUTO_LINK_BASE + AUTO_LINK_EDGE_FLAGS) = edge->flags;
  }
  __asm__ volatile("fence iorw, iorw" ::: "memory");
}

static inline void auto_link_run_config(uint32_t id) { *(volatile uint32_t *)(AUTO_LINK_BASE + AUTO_LINK_RUN_CAPTURE) = id; }

static inline void auto_link_run(uint32_t first, uint32_t count) {
  *(volatile uint32_t *)(AUTO_LINK_BASE + AUTO_LINK_RUN_FIRST) = first;
  *(volatile uint32_t *)(AUTO_LINK_BASE + AUTO_LINK_RUN_COUNT) = count;
  *(volatile uint32_t *)(AUTO_LINK_BASE + AUTO_LINK_RUN_START) = 1;
  __asm__ volatile("fence iorw, iorw" ::: "memory");
}

#endif
