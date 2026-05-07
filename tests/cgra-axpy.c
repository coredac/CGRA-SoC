// CGRA RoCC AXPY test generated from compiler-format dataflow YAML.

#include "cgra_protocol.h"
#include "cgra_runtime.h"
#include "generated/cgra_axpy_packets.h"
#include <stdint.h>
#include <stdio.h>

enum {
  AXPY_LENGTH = 16,
  ARG0_BASE = 0,
  ARG1_BASE = 64,
};

static const uint32_t x_values[AXPY_LENGTH] = {
    0, 1, 2, 3, 4, 5, 6, 7,
    8, 9, 10, 11, 12, 13, 14, 15,
};

static const uint32_t y_initial[AXPY_LENGTH] = {
    100, 97, 94, 91, 88, 85, 82, 79,
    76, 73, 70, 67, 64, 61, 58, 55,
};

static void preload_axpy_data(void) {
  for (uint8_t i = 0; i < AXPY_LENGTH; ++i) {
    cgra_send_basic(0, CGRA_CMD_STORE_REQUEST, x_values[i], 1, ARG0_BASE + i);
    cgra_send_basic(0, CGRA_CMD_STORE_REQUEST, y_initial[i], 1, ARG1_BASE + i);
  }
}

int main(void) {
  uint64_t status = 0;
  uint64_t wait_result = 0;

  printf("CGRA RoCC AXPY: Starting...\n");

  CGRA_STATUS(status);
  printf("Initial status: 0x%lx\n", status);

  CGRA_SET_EXPECTED_COMPLETES(CGRA_AXPY_EXPECTED_COMPLETES);

  printf("Preloading AXPY data memory...\n");
  preload_axpy_data();

  printf("Sending generated AXPY packets...\n");
  cgra_send_packets(CGRA_AXPY_PACKETS, CGRA_AXPY_PACKET_COUNT);
  printf("Generated AXPY packets sent.\n");

  CGRA_WAIT(wait_result);
  printf("WAIT result: 0x%lx\n", wait_result);

  CGRA_STATUS(status);
  printf("Final status: 0x%lx\n", status);

  uint64_t complete = status & 0x1ULL;
  uint64_t complete_count = (status >> 1) & 0xFFFFULL;
  int failed = 0;

  if (wait_result != 1 || complete != 1 ||
      complete_count != CGRA_AXPY_EXPECTED_COMPLETES) {
    printf("Completion mismatch: wait=%lu complete=%lu complete_count=%lu expected=%d\n",
           wait_result, complete, complete_count, CGRA_AXPY_EXPECTED_COMPLETES);
    failed = 1;
  }

  for (uint8_t i = 0; i < AXPY_LENGTH; ++i) {
    uint32_t actual = cgra_load_mem(ARG1_BASE + i);
    uint32_t expected = 3 * x_values[i] + y_initial[i];
    if (actual != expected) {
      printf("arg1[%u] mismatch: got=%lu expected=%lu\n",
             i, (uint64_t)actual, (uint64_t)expected);
      failed = 1;
    }
  }

  if (failed) {
    printf("CGRA RoCC AXPY: FAIL\n");
    return 1;
  }

  printf("CGRA RoCC AXPY: PASS\n");
  return 0;
}
