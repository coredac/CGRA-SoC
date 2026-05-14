// CGRA RoCC single-CGRA 4x4 ReLU test generated from unified kernel YAML.

#include "cgra_protocol.h"
#include "cgra_runtime.h"
#include "generated/cgra_relu4x4_api.h"
#include <stdint.h>
#include <stdio.h>

enum {
  RELU4X4_EXPECTED_COMPLETES = 1,
  RELU4X4_EXPECTED_RESULT = 0,
  RELU4X4_INPUT_COUNT = 32,
};

static uint32_t relu4x4_input(uint8_t addr) {
  return (uint32_t)((int32_t)addr - 16);
}

static uint32_t relu4x4_expected(uint8_t addr) {
  int32_t value = (int32_t)addr - 16;
  return value > 0 ? (uint32_t)value : 0;
}

static void preload_relu4x4_data(void) {
  for (uint8_t addr = 0; addr < RELU4X4_INPUT_COUNT; ++addr) {
    send_basic(0, CGRA_CMD_STORE_REQUEST, relu4x4_input(addr), 1, addr);
  }
}

static int verify_relu4x4_data(void) {
  int failures = 0;

  for (uint8_t addr = 0; addr < RELU4X4_INPUT_COUNT; ++addr) {
    uint32_t actual = read_mem(addr);
    uint32_t expected = relu4x4_expected(addr);
    if (actual != expected) {
      printf("Mismatch addr=%u actual=0x%08x expected=0x%08x\n",
             addr, actual, expected);
      ++failures;
    }
  }

  return failures;
}

int main(void) {
  uint64_t status = 0;
  uint64_t wait_result = 0;
  uint64_t result = 0;

  printf("CGRA RoCC ReLU4x4: Starting...\n");

  CGRA_STATUS(status);
  printf("Initial status: 0x%lx\n", status);

  CGRA_SET_EXPECTED_COMPLETES(RELU4X4_EXPECTED_COMPLETES);

  printf("Preloading ReLU4x4 data memory...\n");
  preload_relu4x4_data();

  printf("Configuring and launching ReLU4x4...\n");
  configure_relu4x4();

  CGRA_WAIT(wait_result);
  printf("WAIT result: 0x%lx\n", wait_result);

  CGRA_STATUS(status);
  printf("Final status: 0x%lx\n", status);

  CGRA_RESULT(result);
  printf("Result: 0x%lx\n", result);

  uint64_t complete = status & 0x1ULL;
  uint64_t complete_count = (status >> 1) & 0xFFFFULL;
  int data_failures = verify_relu4x4_data();

  if (wait_result != 1 || complete != 1 ||
      complete_count != RELU4X4_EXPECTED_COMPLETES ||
      result != RELU4X4_EXPECTED_RESULT ||
      data_failures != 0) {
    printf("CGRA RoCC ReLU4x4: FAIL\n");
    return 1;
  }

  printf("CGRA RoCC ReLU4x4: PASS\n");
  return 0;
}
