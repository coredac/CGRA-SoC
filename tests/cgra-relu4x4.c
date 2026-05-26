// CGRA RoCC single-CGRA 4x4 ReLU test using generated fast API packets.

#include "cgra_protocol.h"
#include "cgra_runtime.h"
#include "generated/cgra_relu4x4_fast_api.h"
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
    relu4x4_store_fast(addr, relu4x4_input(addr));
  }
}

static int verify_relu4x4_data(int verbose) {
  int failures = 0;

  for (uint8_t addr = 0; addr < RELU4X4_INPUT_COUNT; ++addr) {
    uint32_t actual = relu4x4_read_mem_fast(addr);
    uint32_t expected = relu4x4_expected(addr);
    if (actual != expected) {
      if (verbose) {
        printf("Mismatch addr=%u actual=0x%08x expected=0x%08x\n",
               addr, actual, expected);
      }
      ++failures;
    }
  }

  return failures;
}

int main(void) {
  int template_failures = relu4x4_basic_fast_templates_match_runtime();
  if (template_failures != 0) {
    printf("CGRA RoCC ReLU4x4 fast API: generated packet mismatch (%d)\n",
           template_failures);
    printf("CGRA RoCC ReLU4x4 fast API: FAIL\n");
    return 1;
  }

  uint64_t status = 0;
  uint64_t wait_result = 0;
  uint64_t result = 0;

  CGRA_SET_EXPECTED_COMPLETES(RELU4X4_EXPECTED_COMPLETES);
  preload_relu4x4_data();
  configure_relu4x4_fast();
  CGRA_WAIT(wait_result);

  CGRA_STATUS(status);
  CGRA_RESULT(result);
  uint64_t complete = status & 0x1ULL;
  uint64_t complete_count = (status >> 1) & 0xFFFFULL;
  int data_failures = verify_relu4x4_data(0);

  if (wait_result != 1 || complete != 1 ||
      complete_count != RELU4X4_EXPECTED_COMPLETES ||
      result != RELU4X4_EXPECTED_RESULT ||
      data_failures != 0) {
    if (data_failures != 0) {
      verify_relu4x4_data(1);
    }
    printf("CGRA RoCC ReLU4x4 fast API: FAIL\n");
    return 1;
  }

  printf("CGRA RoCC ReLU4x4 fast API: PASS\n");
  return 0;
}
