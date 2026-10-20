#ifndef SOC_PLAN_H
#define SOC_PLAN_H

#include "accel.h"
#include "auto_link_types.h"
#include "cgra_link.h"
#include "gemmini_job.h"

#define SOC_PLAN_ABI 2

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
} soc_command_t;

typedef struct {
  soc_operand_t source;
  unsigned word;
  unsigned bytes;
  unsigned tag;
} soc_dma_t;

/* All command selection, buffer placement and relocation comes from the plan. */
typedef struct {
  soc_kind_t kind;
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
  unsigned dma_count;
  const soc_dma_t *dmas;
} soc_task_t;

/* CPU steps execute their task list for each tile; automatic steps launch a profile. */
typedef struct {
  unsigned automatic;
  unsigned tiles;
  unsigned task_count;
  const unsigned *tasks;
  unsigned profile;
  unsigned collector;
  const auto_link_graph_t *graph;
  const uintptr_t *destination_strides;
} soc_step_t;

typedef struct {
  unsigned abi;
  soc_mode_t mode;
  unsigned tiles;
  unsigned device_count;
  const accel_t *const *devices;
  unsigned task_count;
  const soc_task_t *tasks;
  unsigned step_count;
  const soc_step_t *steps;
} soc_plan_t;

#endif
