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

#endif
