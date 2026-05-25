#include "cgra_protocol.h"
#include "cgra_runtime.h"
#include "generated/cgra_relu4x4_api.h"
#include "gemmini.h"

#include <stdint.h>
#include <stdio.h>

enum {
  CGRA_RELU_INPUT_COUNT = 32,
  CGRA_RELU_EXPECTED_COMPLETES = 1,
};

static elem_t A[DIM][DIM] row_align(1);
static elem_t B[DIM][DIM] row_align(1);
static elem_t C[DIM][DIM] row_align(1);

static void init_matrices(void) {
  for (int i = 0; i < DIM; ++i) {
    for (int j = 0; j < DIM; ++j) {
      A[i][j] = (elem_t)(((i * DIM + j) % 32) - 16);
      B[i][j] = (i == j) ? (elem_t)1 : (elem_t)0;
      C[i][j] = 0;
    }
  }
}

static int run_gemmini_gemm(void) {
  const uint32_t A_addr = 0;
  const uint32_t B_addr = DIM;
  const uint32_t C_addr_acc = (uint32_t)1 << (ADDR_LEN - 1);

  gemmini_flush(0);

  gemmini_config_ld(DIM * sizeof(elem_t));
  gemmini_config_ex(WEIGHT_STATIONARY, NO_ACTIVATION, 0);
  gemmini_config_st(DIM * sizeof(elem_t));

  gemmini_mvin(A, A_addr);
  gemmini_mvin(B, B_addr);

  gemmini_preload(B_addr, C_addr_acc);
  gemmini_compute_preloaded(A_addr, GARBAGE_ADDR);

  gemmini_mvout(C, C_addr_acc);

  gemmini_fence();

  int failures = 0;
  for (int i = 0; i < DIM; ++i) {
    for (int j = 0; j < DIM; ++j) {
      if (C[i][j] != A[i][j]) {
        printf("Gemmini mismatch [%d][%d]: actual=%d expected=%d\n",
               i, j, (int)C[i][j], (int)A[i][j]);
        ++failures;
      }
    }
  }

  if (failures != 0) {
    return 1;
  }

  printf("Gemmini GEMM: PASS\n");
  return 0;
}

static int32_t cgra_input_value(int index) {
  const int row = index / DIM;
  const int col = index % DIM;
  return (int32_t)C[row][col];
}

static void preload_cgra_relu_inputs(void) {
  for (uint8_t addr = 0; addr < CGRA_RELU_INPUT_COUNT; ++addr) {
    int32_t v32 = cgra_input_value(addr);
    send_basic(0, CGRA_CMD_STORE_REQUEST, (uint32_t)v32, 1, addr);
  }
}

static int verify_cgra_relu_outputs(void) {
  int failures = 0;

  for (uint8_t addr = 0; addr < CGRA_RELU_INPUT_COUNT; ++addr) {
    int32_t value = cgra_input_value(addr);
    uint32_t expected = value > 0 ? (uint32_t)value : 0;
    uint32_t actual = (uint32_t)read_mem(addr);
    if (actual != expected) {
      printf("CGRA ReLU mismatch addr=%u actual=0x%08x expected=0x%08x input=%d\n",
             addr, actual, expected, (int)value);
      ++failures;
    }
  }

  return failures;
}

static int run_cgra_relu(void) {
  uint64_t wait_result = 0;

  preload_cgra_relu_inputs();

  CGRA_SET_EXPECTED_COMPLETES(CGRA_RELU_EXPECTED_COMPLETES);

  configure_relu4x4();

  CGRA_WAIT(wait_result);

  int failures = verify_cgra_relu_outputs();

  if (wait_result != 1 || failures != 0) {
    printf("CGRA ReLU: FAIL\n");
    return 1;
  }

  printf("CGRA ReLU: PASS\n");
  return 0;
}

int main(void) {
  init_matrices();

  if (run_gemmini_gemm() != 0) {
    return 1;
  }

  if (run_cgra_relu() != 0) {
    return 1;
  }

  printf("Gemmini + CGRA demo: PASS\n");
  return 0;
}
