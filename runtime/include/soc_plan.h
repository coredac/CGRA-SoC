#ifndef SOC_PLAN_H
#define SOC_PLAN_H

#include "accel_types.h"
#include "auto_link_types.h"
#include "cgra_types.h"
#include "gemmini_types.h"
#include "soc_driver.h"

#define SOC_PLAN_ABI 8

typedef enum { SOC_MANUAL, SOC_HYBRID, SOC_AUTO } soc_mode_t;
typedef enum { SOC_GEMMINI, SOC_CGRA } soc_kind_t;
typedef enum { SOC_LITERAL, SOC_ARGUMENT, SOC_SPM, SOC_WORKSPACE } soc_base_t;

typedef struct {
  uint64_t value;
  soc_base_t base;
  unsigned index;
} soc_operand_t;

typedef struct {
  unsigned rows, tile_rows, k, n;
  unsigned input_row, weight_row;
  soc_operand_t input, weight, output;
  uintptr_t input_stride, output_stride;
  unsigned preload_input;
  unsigned publish, publish_row, publish_stride;
  unsigned store;
} soc_gemmini_t;

typedef struct {
  unsigned input_rows, input_columns, input_channels;
  unsigned output_rows, output_columns, output_channels;
  unsigned kernel, stride, dilation, padding_top, padding_left;
  unsigned input_elements, weight_elements, output_elements;
  soc_operand_t input, weight, output;
} soc_conv_t;

typedef struct {
  unsigned m, k, n;
  unsigned transpose_a, transpose_b;
  unsigned accumulate, store;
  uintptr_t input_stride, weight_stride, output_stride;
  unsigned input_elements, weight_elements, bias_elements, output_elements;
  soc_operand_t input, weight, bias, output;
} soc_matrix_t;

typedef struct {
  unsigned rows, columns, element_bytes;
  uintptr_t source_stride, destination_stride;
  soc_operand_t source, destination;
} soc_copy_t;

typedef struct {
  unsigned rows, columns, channels, input_elements, output_elements;
  unsigned lanes, line_entries;
  soc_operand_t input, output;
} soc_pool_t;

/* DMA bytes count native SPM bytes, including packed INT8 transfers. */
typedef struct {
  soc_operand_t source;
  uintptr_t source_stride;
  unsigned word;
  unsigned bytes;
  unsigned tag;
  unsigned packed;
} soc_dma_t;

typedef struct {
  soc_kind_t kind;
  unsigned endpoint;
  unsigned word;
  unsigned bytes;
  unsigned packed;
  soc_operand_t destination;
} soc_output_t;

/* The plan supplies kernel parameters, placement and relocation. */
struct soc_task {
  const soc_driver_t *driver;
  unsigned endpoint;
  unsigned job;
  const void *parameters;
  const cgra_kernel_t *kernel;
  const cgra_link_symbol_t *symbols;
  unsigned full_config;
  unsigned elements;
  unsigned preload_count;
  const soc_dma_t *preloads;
  unsigned dma_count;
  const soc_dma_t *dmas;
};

typedef enum { SOC_INIT, SOC_CAPTURE, SOC_PROFILE } soc_prep_kind_t;

/* Index selects a task for INIT/CAPTURE or an execution step for PROFILE. */
typedef struct {
  soc_prep_kind_t kind;
  unsigned index;
} soc_prep_t;

/* CPU steps execute their task list for each tile; automatic steps launch a profile. */
typedef struct {
  unsigned automatic;
  unsigned tiles;
  unsigned task_count;
  const unsigned *tasks;
  unsigned profile;
  unsigned collector;
  const auto_link_graph_t *graph;
  /* Nonempty preparation starts a cache epoch after preceding steps drain. */
  unsigned prep_count;
  const soc_prep_t *prep;
} soc_step_t;

typedef struct {
  unsigned abi;
  soc_mode_t mode;
  unsigned tiles;
  unsigned device_count;
  unsigned argument_count;
  const accel_t *const *devices;
  unsigned task_count;
  const soc_task_t *tasks;
  unsigned prep_count;
  const soc_prep_t *prep;
  unsigned step_count;
  const soc_step_t *steps;
  unsigned output_count;
  const soc_output_t *outputs;
  uint64_t workspace_bytes;
  unsigned workspace_alignment;
} soc_plan_t;

#endif
