// CGRA RoCC single-CGRA 4x4 SAD test generated from unified kernel YAML.

#include "cgra_protocol.h"
#include "cgra_runtime.h"
#include "generated/cgra_sad_api.h"
#include <stdint.h>
#include <stdio.h>

enum {
  SAD_EXPECTED_COMPLETES = 1,
  SAD_EXPECTED_RESULT = 24,
  SAD_N = 8,
  SAD_BASE_B = 8,
};

static void preload_sad_data(void) {
  for (uint8_t i = 0; i < SAD_N; ++i) {
    send_basic(0, CGRA_CMD_STORE_REQUEST, (uint32_t)(i + 1), 1, i);
    send_basic(0, CGRA_CMD_STORE_REQUEST, (uint32_t)(i + 4), 1,
               SAD_BASE_B + i);
  }
}

int main(void) {
  uint64_t status = 0;
  uint64_t wait_result = 0;
  uint64_t result = 0;

  printf("CGRA RoCC SAD 4x4: Starting...\n");
  CGRA_STATUS(status);
  printf("Initial status: 0x%lx\n", status);

  CGRA_SET_EXPECTED_COMPLETES(SAD_EXPECTED_COMPLETES);

  printf("Preloading SAD data memory...\n");
  preload_sad_data();

  printf("Configuring and launching SAD...\n");
  configure_sad();

  CGRA_WAIT(wait_result);
  printf("WAIT result: 0x%lx\n", wait_result);

  CGRA_STATUS(status);
  printf("Final status: 0x%lx\n", status);

  CGRA_RESULT(result);
  printf("Result: 0x%lx\n", result);

  uint64_t complete = status & 0x1ULL;
  uint64_t complete_count = (status >> 1) & 0xFFFFULL;

  if (wait_result != 1 || complete != 1 ||
      complete_count != SAD_EXPECTED_COMPLETES ||
      result != SAD_EXPECTED_RESULT) {
    printf("CGRA RoCC SAD 4x4: FAIL\n");
    return 1;
  }

  printf("CGRA RoCC SAD 4x4: PASS\n");
  return 0;
}
