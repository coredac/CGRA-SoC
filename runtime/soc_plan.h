#ifndef SOC_PLAN_H
#define SOC_PLAN_H

#define SOC_PLAN_ABI 1
enum { SOC_TASKS = 3, SOC_DIM = 16, SOC_ROWS = 2, SOC_WORDS = SOC_ROWS * SOC_DIM };

typedef enum { SOC_MANUAL, SOC_HYBRID, SOC_AUTO } soc_mode_t;
typedef enum { SOC_GEMM, SOC_RELU_I8 } soc_kernel_t;

typedef struct {
  soc_kernel_t kernel;
  unsigned endpoint;
  unsigned job;
} soc_task_t;

typedef struct {
  unsigned first;
  unsigned count;
  unsigned automatic;
} soc_region_t;

/* The v1 plan describes the fixed GEMM -> ReLU/INT8 -> GEMM template. */
typedef struct {
  unsigned abi;
  soc_mode_t mode;
  unsigned tiles;
  soc_task_t tasks[SOC_TASKS];
  unsigned region_count;
  soc_region_t regions[SOC_TASKS];
  unsigned input_row;
  unsigned weight_row;
  unsigned publication_bytes;
  unsigned slot_words;
  unsigned relu_word;
} soc_plan_t;

#endif
