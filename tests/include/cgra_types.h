#ifndef CGRA_TYPES_H
#define CGRA_TYPES_H

#include <stdint.h>

typedef struct {
  uint64_t lo;
  uint64_t mid;
  uint64_t hi;
  uint64_t top;
} cgra_packet_t;

/* Ordered by packet index in configuration packets followed by launch packets. */
typedef struct {
  uint32_t packet_index;
  uint32_t symbol_index;
  uint32_t scale;
  uint32_t offset;
} cgra_patch_t;

typedef enum { CGRA_COLD, CGRA_REPEAT, CGRA_SWITCH } cgra_run_t;

typedef struct {
  const cgra_packet_t *static_packets;
  uint32_t static_count;
  const cgra_packet_t *config_packets;
  uint32_t config_count;
  uint32_t rearm_count;
  uint32_t setup_count;
  const cgra_packet_t *launch_packets;
  uint32_t launch_count;
  uint32_t expected_completes;
  const cgra_patch_t *patches;
  uint32_t patch_count;
} cgra_kernel_t;

enum { CGRA_LINK_SLOT = 0, CGRA_LINK_ELEMENTS = 1, CGRA_LINK_TILE_ID = 2, CGRA_LINK_ROW = 3, CGRA_LINK_ROWS = 4 };

typedef struct {
  uint32_t base;
  uint32_t stride;
  uint32_t source;
} cgra_link_symbol_t;

#endif
