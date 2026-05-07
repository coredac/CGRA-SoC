// CGRA RoCC FIR test generated from compiler-format dataflow YAML.

#include "cgra_protocol.h"
#include "cgra_runtime.h"
#include "generated/cgra_fir_packets.h"
#include <stdint.h>
#include <stdio.h>

enum {
  FIR_LENGTH = 32,
  ARG0_BASE = 0,
  ARG2_BASE = 128,
  STATUS_TIMEOUT = 4096,
};

static const uint32_t input_values[FIR_LENGTH] = {
    1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1,
};

static const uint32_t coefficient_values[FIR_LENGTH] = {
    1, 2, 3, 4, 5, 6, 7, 8,
    9, 10, 11, 12, 13, 14, 15, 16,
    17, 18, 19, 20, 21, 22, 23, 24,
    25, 26, 27, 28, 29, 30, 31, 32,
};

static uint32_t expected_fir_result(void) {
  uint32_t sum = 0;
  for (uint8_t i = 0; i < FIR_LENGTH; ++i) {
    sum += input_values[i] * coefficient_values[i];
  }
  return sum;
}

static void preload_fir_data(void) {
  for (uint8_t i = 0; i < FIR_LENGTH; ++i) {
    cgra_send_basic(0, CGRA_CMD_STORE_REQUEST, input_values[i], 1, ARG0_BASE + i);
    cgra_send_basic(0, CGRA_CMD_STORE_REQUEST, coefficient_values[i], 1,
                    ARG2_BASE + i);
  }
}

static int verify_fir_preload(void) {
  for (uint8_t i = 0; i < FIR_LENGTH; ++i) {
    uint32_t actual_x = cgra_load_mem(ARG0_BASE + i);
    uint32_t actual_coeff = cgra_load_mem(ARG2_BASE + i);
    if (actual_x != input_values[i]) {
      printf("arg0[%u] preload mismatch: got=%lu expected=%lu\n",
             i, (uint64_t)actual_x, (uint64_t)input_values[i]);
      return 0;
    }
    if (actual_coeff != coefficient_values[i]) {
      printf("arg2[%u] preload mismatch: got=%lu expected=%lu\n",
             i, (uint64_t)actual_coeff, (uint64_t)coefficient_values[i]);
      return 0;
    }
  }
  return 1;
}

int main(void) {
  uint64_t status = 0;
  uint64_t result = 0;
  const uint32_t expected = expected_fir_result();

  printf("CGRA RoCC FIR: Starting...\n");

  CGRA_STATUS(status);
  printf("Initial status: 0x%lx\n", status);

  CGRA_SET_EXPECTED_COMPLETES(CGRA_FIR_EXPECTED_COMPLETES);

  printf("Preloading FIR data memory...\n");
  preload_fir_data();
  if (!verify_fir_preload()) {
    printf("CGRA RoCC FIR: FAIL\n");
    return 1;
  }
  printf("FIR preload verified.\n");

  printf("Sending generated FIR packets...\n");
  cgra_send_packets(CGRA_FIR_PACKETS, CGRA_FIR_PACKET_COUNT);
  printf("Generated FIR packets sent.\n");

  for (uint32_t attempt = 0; attempt < STATUS_TIMEOUT; ++attempt) {
    CGRA_STATUS(status);
    const uint64_t complete = status & 0x1ULL;
    const uint64_t complete_count = (status >> 1) & 0xFFFFULL;
    if (complete && complete_count == CGRA_FIR_EXPECTED_COMPLETES) {
      break;
    }
  }

  CGRA_STATUS(status);
  printf("Final status: 0x%lx\n", status);

  CGRA_RESULT(result);
  printf("Result: 0x%lx expected: 0x%lx\n", result, (uint64_t)expected);

  uint64_t complete = status & 0x1ULL;
  uint64_t complete_count = (status >> 1) & 0xFFFFULL;

  if (complete != 1 || complete_count != CGRA_FIR_EXPECTED_COMPLETES ||
      result != expected || result != CGRA_FIR_EXPECTED_RESULT) {
    printf("Completion mismatch: complete=%lu complete_count=%lu expected=%d\n",
           complete, complete_count, CGRA_FIR_EXPECTED_COMPLETES);
    printf("CGRA RoCC FIR: FAIL\n");
    return 1;
  }

  printf("CGRA RoCC FIR: PASS\n");
  return 0;
}
