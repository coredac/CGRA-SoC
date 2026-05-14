// CGRA RoCC single-CGRA 4x4 FIR test generated from unified kernel YAML.

#include "cgra_protocol.h"
#include "cgra_runtime.h"
#include "generated/cgra_fir4x4_api.h"
#include <stdint.h>
#include <stdio.h>

enum {
  FIR4X4_EXPECTED_COMPLETES = 1,
  FIR4X4_EXPECTED_RESULT = 23536,
  FIR4X4_INPUT_COUNT = 32,
  FIR4X4_INPUT_BASE_VALUE = 10,
};

static void preload_fir4x4_data(void) {
  for (uint8_t addr = 0; addr < FIR4X4_INPUT_COUNT; ++addr) {
    send_basic(0, CGRA_CMD_STORE_REQUEST,
               (uint32_t)(FIR4X4_INPUT_BASE_VALUE + addr), 1, addr);
  }
}

int main(void) {
  uint64_t status = 0;
  uint64_t wait_result = 0;
  uint64_t result = 0;

  printf("CGRA RoCC FIR YAML 4x4: Starting...\n");

  CGRA_STATUS(status);
  printf("Initial status: 0x%lx\n", status);

  CGRA_SET_EXPECTED_COMPLETES(FIR4X4_EXPECTED_COMPLETES);

  printf("Preloading FIR4x4 data memory...\n");
  preload_fir4x4_data();

  printf("Configuring and launching FIR4x4...\n");
  configure_fir4x4();

  CGRA_WAIT(wait_result);
  printf("WAIT result: 0x%lx\n", wait_result);

  CGRA_STATUS(status);
  printf("Final status: 0x%lx\n", status);

  CGRA_RESULT(result);
  printf("Result: 0x%lx\n", result);

  uint64_t complete = status & 0x1ULL;
  uint64_t complete_count = (status >> 1) & 0xFFFFULL;

  if (wait_result != 1 || complete != 1 ||
      complete_count != FIR4X4_EXPECTED_COMPLETES ||
      result != FIR4X4_EXPECTED_RESULT) {
    printf("CGRA RoCC FIR YAML 4x4: FAIL\n");
    return 1;
  }

  printf("CGRA RoCC FIR YAML 4x4: PASS\n");
  return 0;
}
