#ifndef AUTO_LINK_TYPES_H
#define AUTO_LINK_TYPES_H

#include <stdint.h>

typedef struct {
  uintptr_t value;
  int binding;
} auto_link_address_t;

typedef struct {
  unsigned flags;
  unsigned pixel_bytes;
  auto_link_address_t address;
  uintptr_t stride;
  unsigned bytes;
} auto_link_output_t;

typedef struct {
  unsigned endpoint;
  unsigned job;
  auto_link_output_t output;
} auto_link_stage_t;

typedef struct {
  unsigned source;
  unsigned destination;
  unsigned flags;
  unsigned bytes;
  unsigned expansion;
  auto_link_address_t address;
  uintptr_t source_offset;
  uintptr_t destination_offset;
  uintptr_t source_stride;
  unsigned pixel_bytes;
} auto_link_edge_t;

typedef struct {
  unsigned stage_count;
  unsigned edge_count;
  unsigned stage_capacity;
  unsigned edge_capacity;
  const auto_link_stage_t *stages;
  const auto_link_edge_t *edges;
} auto_link_graph_t;

#endif
