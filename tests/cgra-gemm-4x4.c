// CGRA RoCC single-CGRA 4x4 GEMM test generated from unified kernel YAML.

#include "cgra_protocol.h"
#include "cgra_runtime.h"
#include "generated/cgra_gemm_api.h"
#include <stdint.h>
#include <stdio.h>

enum {
  GEMM_EXPECTED_COMPLETES = 1,
  GEMM_EXPECTED_RESULT = 0,
  GEMM_N = 4,
  GEMM_BASE_A = 0,
  GEMM_BASE_B = 16,
  GEMM_BASE_C = 32,
};

static void preload_gemm_data(void) {
  for (uint8_t i = 0; i < GEMM_N; ++i) {
    for (uint8_t k = 0; k < GEMM_N; ++k) {
      uint8_t addr = GEMM_BASE_A + i * GEMM_N + k;
      uint32_t value = (uint32_t)((i * (k + 1)) % 17);
      send_basic(0, CGRA_CMD_STORE_REQUEST, value, 1, addr);
    }
  }

  for (uint8_t k = 0; k < GEMM_N; ++k) {
    for (uint8_t j = 0; j < GEMM_N; ++j) {
      uint8_t addr = GEMM_BASE_B + k * GEMM_N + j;
      uint32_t value = (uint32_t)((k * (j + 2)) % 19);
      send_basic(0, CGRA_CMD_STORE_REQUEST, value, 1, addr);
    }
  }

  for (uint8_t i = 0; i < GEMM_N; ++i) {
    for (uint8_t j = 0; j < GEMM_N; ++j) {
      uint8_t addr = GEMM_BASE_C + i * GEMM_N + j;
      uint32_t value = (uint32_t)((i * j) % 13);
      send_basic(0, CGRA_CMD_STORE_REQUEST, value, 1, addr);
    }
  }
}

static void configure_gemm_gep_stride(void) {
  send_basic(4, CGRA_CMD_CONFIG_GEP_STRIDE, GEMM_N, 1, 0);
  send_basic(6, CGRA_CMD_CONFIG_GEP_STRIDE, GEMM_N, 1, 0);
}

int main(void) {
  uint64_t status = 0;
  uint64_t wait_result = 0;
  uint64_t result = 0;

  printf("CGRA RoCC GEMM 4x4: Starting...\n");
  CGRA_STATUS(status);
  printf("Initial status: 0x%lx\n", status);

  CGRA_SET_EXPECTED_COMPLETES(GEMM_EXPECTED_COMPLETES);

  printf("Preloading GEMM data memory...\n");
  preload_gemm_data();

  printf("Configuring and launching GEMM...\n");
  configure_gemm_gep_stride();
  configure_gemm();

  CGRA_WAIT(wait_result);
  printf("WAIT result: 0x%lx\n", wait_result);

  CGRA_STATUS(status);
  printf("Final status: 0x%lx\n", status);

  CGRA_RESULT(result);
  printf("Result: 0x%lx\n", result);

  uint64_t complete = status & 0x1ULL;
  uint64_t complete_count = (status >> 1) & 0xFFFFULL;

  if (wait_result != 1 || complete != 1 ||
      complete_count != GEMM_EXPECTED_COMPLETES ||
      result != GEMM_EXPECTED_RESULT) {
    printf("CGRA RoCC GEMM 4x4: FAIL\n");
    return 1;
  }

  printf("CGRA RoCC GEMM 4x4: PASS\n");
  return 0;
}
