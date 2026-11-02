#include "accel_generated.h"
#include "auto_link.h"
#include "cgra_link.h"
#include "gemmini_copy.h"
#include "gemmini_matrix.h"

#include <stdio.h>

enum {
  ROWS = 7,
  INNER = 2 * DIM + 5,
  COLUMNS = DIM + 3,
  INPUT_STRIDE = INNER + 7,
  WEIGHT_STRIDE = COLUMNS + 5,
  OUTPUT_STRIDE = COLUMNS + 9,
  COPY_ROWS = DIM - 3,
  COPY_COLUMNS = DIM - 5,
  BYTE_COLUMNS = MAX_BLOCK_LEN * DIM - 3
};

static elem_t input[2][ROWS][INPUT_STRIDE] __attribute__((aligned(64)));
static elem_t weight[2][INNER][WEIGHT_STRIDE] __attribute__((aligned(64)));
static acc_t bias[2][COLUMNS] __attribute__((aligned(64)));
static acc_t output[2][ROWS][OUTPUT_STRIDE] __attribute__((aligned(64)));
static elem_t source8[DIM][BYTE_COLUMNS + 3] __attribute__((aligned(64)));
static elem_t destination8[DIM][BYTE_COLUMNS + 5] __attribute__((aligned(64)));
static acc_t source32[DIM][DIM + 3] __attribute__((aligned(64)));
static acc_t destination32[DIM][DIM + 5] __attribute__((aligned(64)));

static void configure(void) {
  const auto_link_stage_t stage = {.endpoint = GEMMINI0.id, .output = {.address = {.binding = -1}}};
  const auto_link_edge_t edge = {.flags = 3, .address = {.binding = -1}};
  const auto_link_graph_t graph = {
      .stage_count = 1, .edge_count = 1, .stage_capacity = AUTO_LINK_GRAPH.stage_capacity, .edge_capacity = AUTO_LINK_GRAPH.edge_capacity, .stages = &stage, .edges = &edge};
  auto_link_load(&graph, NULL);
  auto_link_tiles(1, 1, 1, 1);
}

static int run_captured(void) {
  auto_link_input_ready();
  const cgra_link_result_t result = cgra_link_wait_at(CGRA0.control);
  while (*(volatile uint32_t *)(AUTO_LINK_BASE + AUTO_LINK_RUNNING)) {
  }
  __asm__ volatile("fence iorw, iorw" ::: "memory");
  return result.status != AUTO_LINK_STATUS_SUCCESS || result.detail || result.data || result.stage || result.job;
}

static void initialize(unsigned mode) {
  for (unsigned tile = 0; tile < 2; ++tile) {
    for (unsigned row = 0; row < ROWS; ++row) {
      for (unsigned inner = 0; inner < INPUT_STRIDE; ++inner)
        input[tile][row][inner] = (int)((row * 3 + inner * 7 + tile + mode) % 13) - 6;
      for (unsigned column = 0; column < OUTPUT_STRIDE; ++column)
        output[tile][row][column] = 0x123456;
    }
    for (unsigned inner = 0; inner < INNER; ++inner)
      for (unsigned column = 0; column < WEIGHT_STRIDE; ++column)
        weight[tile][inner][column] = (int)((inner * 5 + column * 3 + tile * 2 + mode) % 11) - 5;
    for (unsigned column = 0; column < COLUMNS; ++column)
      bias[tile][column] = column * 31 - 170;
  }
  for (unsigned row = 0; row < DIM; ++row) {
    for (unsigned column = 0; column < BYTE_COLUMNS + 3; ++column)
      source8[row][column] = (int)((row * 17 + column * 23 + mode) % 255) - 127;
    for (unsigned column = 0; column < BYTE_COLUMNS + 5; ++column)
      destination8[row][column] = 61;
    for (unsigned column = 0; column < DIM + 3; ++column) {
      source32[row][column] = (int)(row * 17431 + column * 23711 + mode) - 500000;
    }
    for (unsigned column = 0; column < DIM + 5; ++column) {
      destination32[row][column] = 0x123456;
    }
  }
}

static int matrices(unsigned automatic) {
  for (unsigned tile = 0; tile < 2; ++tile) {
    for (unsigned inner = 0; inner < INNER; inner += DIM) {
      const unsigned width = INNER - inner < DIM ? INNER - inner : DIM;
      const gemmini_matrix_t params = {.m = ROWS,
                                       .k = width,
                                       .n = COLUMNS,
                                       .accumulate = inner != 0,
                                       .store = inner + width == INNER,
                                       .input_stride = INPUT_STRIDE,
                                       .weight_stride = WEIGHT_STRIDE,
                                       .output_stride = OUTPUT_STRIDE * sizeof(acc_t),
                                       .input_elements = ROWS * width,
                                       .weight_elements = width * COLUMNS,
                                       .bias_elements = COLUMNS,
                                       .output_elements = ROWS * COLUMNS,
                                       .input = &input[tile][0][inner],
                                       .weight = &weight[tile][inner][0],
                                       .bias = bias[tile],
                                       .output = &output[tile][0][0]};
      if (automatic ? gemmini_matrix_capture(GEMMINI0, 0, &params) || run_captured() : gemmini_matrix_run(GEMMINI0, &params))
        return 1;
      __asm__ volatile("fence iorw, iorw" ::: "memory");
    }
    for (unsigned row = 0; row < ROWS; ++row)
      for (unsigned column = 0; column < OUTPUT_STRIDE; ++column) {
        acc_t expected = 0x123456;
        if (column < COLUMNS) {
          expected = bias[tile][column];
          for (unsigned inner = 0; inner < INNER; ++inner)
            expected += input[tile][row][inner] * weight[tile][inner][column];
        }
        if (output[tile][row][column] != expected) {
          printf("Matrix mode=%u tile=%u row=%u column=%u got=%d expected=%d\n", automatic, tile, row, column, output[tile][row][column], expected);
          return 1;
        }
      }
  }
  printf("Native Matrix mode=%u pitched K chunks and cursor reuse PASS\n", automatic);
  return 0;
}

static int copies(unsigned automatic) {
  const gemmini_copy_t params[] = {{.rows = COPY_ROWS,
                                    .columns = BYTE_COLUMNS,
                                    .element_bytes = sizeof(elem_t),
                                    .source_stride = sizeof(source8[0]),
                                    .destination_stride = sizeof(destination8[0]),
                                    .source = source8,
                                    .destination = destination8},
                                   {.rows = COPY_ROWS,
                                    .columns = COPY_COLUMNS,
                                    .element_bytes = sizeof(acc_t),
                                    .source_stride = sizeof(source32[0]),
                                    .destination_stride = sizeof(destination32[0]),
                                    .source = source32,
                                    .destination = destination32}};
  for (unsigned index = 0; index < 2; ++index) {
    if (automatic ? gemmini_copy_capture(GEMMINI0, 0, &params[index]) || run_captured() : gemmini_copy_run(GEMMINI0, &params[index]))
      return 1;
    __asm__ volatile("fence iorw, iorw" ::: "memory");
  }
  for (unsigned row = 0; row < DIM; ++row) {
    for (unsigned column = 0; column < BYTE_COLUMNS + 5; ++column) {
      const int copied = row < COPY_ROWS && column < BYTE_COLUMNS;
      if (destination8[row][column] != (copied ? source8[row][column] : 61)) {
        printf("Wide byte copy mode=%u row=%u column=%u mismatch\n", automatic, row, column);
        return 1;
      }
    }
    for (unsigned column = 0; column < DIM + 5; ++column) {
      const int copied = row < COPY_ROWS && column < COPY_COLUMNS;
      if (destination32[row][column] != (copied ? source32[row][column] : 0x123456)) {
        printf("Copy mode=%u row=%u column=%u mismatch\n", automatic, row, column);
        return 1;
      }
    }
  }
  printf("Native copy mode=%u wide INT8/INT32 pitched tails PASS\n", automatic);
  return 0;
}

int main(void) {
  puts("Native tiles starting");
  fflush(stdout);
  accel_commands(GEMMINI0, { gemmini_flush(0); });
  configure();
  for (unsigned mode = 0; mode < 2; ++mode) {
    initialize(mode);
    if (matrices(mode) || copies(mode)) {
      puts("Native tiles FAIL");
      return 1;
    }
    fflush(stdout);
  }
  puts("Native tiles PASS");
  return 0;
}
