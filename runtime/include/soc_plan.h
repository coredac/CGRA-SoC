#ifndef SOC_PLAN_H
#define SOC_PLAN_H

#include "accel_types.h"
#include "auto_link_types.h"
#include "cgra_types.h"
#include "gemmini_types.h"
#include "soc_driver.h"

#define SOC_PLAN_ABI 5

typedef enum { SOC_MANUAL, SOC_HYBRID, SOC_AUTO } soc_mode_t;
typedef enum { SOC_GEMMINI, SOC_CGRA } soc_kind_t;
typedef enum { SOC_LITERAL, SOC_ARGUMENT, SOC_SPM } soc_base_t;

typedef struct {
  uint64_t value;
  soc_base_t base;
  unsigned index;
} soc_operand_t;

typedef struct {
  unsigned funct;
  soc_operand_t rs1;
  soc_operand_t rs2;
  unsigned patch_begin;
  unsigned patch_count;
} soc_command_t;

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

/* All command selection, buffer placement and relocation comes from the plan. */
struct soc_task {
  const soc_driver_t *driver;
  unsigned endpoint;
  unsigned job;
  unsigned init_count;
  const soc_command_t *init;
  unsigned command_count;
  const soc_command_t *commands;
  unsigned patch_count;
  const gemmini_patch_t *patches;
  const cgra_kernel_t *kernel;
  const cgra_link_symbol_t *symbols;
  unsigned full_config;
  unsigned elements;
  unsigned preload_count;
  const soc_dma_t *preloads;
  unsigned dma_count;
  const soc_dma_t *dmas;
};

/* CPU steps execute their task list for each tile; automatic steps launch a profile. */
typedef struct {
  unsigned automatic;
  unsigned tiles;
  unsigned task_count;
  const unsigned *tasks;
  unsigned profile;
  unsigned collector;
  const auto_link_graph_t *graph;
} soc_step_t;

typedef enum { SOC_INIT, SOC_CAPTURE, SOC_PROFILE } soc_prep_kind_t;

/* Index selects a task for INIT/CAPTURE or an execution step for PROFILE. */
typedef struct {
  soc_prep_kind_t kind;
  unsigned index;
} soc_prep_t;

typedef struct {
  unsigned abi;
  soc_mode_t mode;
  unsigned tiles;
  unsigned device_count;
  const accel_t *const *devices;
  unsigned task_count;
  const soc_task_t *tasks;
  unsigned prep_count;
  const soc_prep_t *prep;
  unsigned step_count;
  const soc_step_t *steps;
  unsigned output_count;
  const soc_output_t *outputs;
} soc_plan_t;

#endif
