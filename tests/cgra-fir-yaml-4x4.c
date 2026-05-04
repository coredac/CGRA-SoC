// CGRA RoCC single-CGRA 4x4 FIR test generated from VectorCGRA YAML.

#include "cgra_protocol.h"
#include "cgra_runtime.h"
#include "generated/cgra_fir_yaml_4x4_packets.h"
#include <stdint.h>
#include <stdio.h>

int main(void) {
  uint64_t status = 0;
  uint64_t wait_result = 0;
  uint64_t result = 0;

  printf("CGRA RoCC FIR YAML 4x4: Starting...\n");

  CGRA_STATUS(status);
  printf("Initial status: 0x%lx\n", status);

  CGRA_SET_EXPECTED_COMPLETES(CGRA_FIR_YAML_4X4_EXPECTED_COMPLETES);

  printf("Sending generated FIR packets...\n");
  cgra_send_packets(CGRA_FIR_YAML_4X4_PACKETS, CGRA_FIR_YAML_4X4_PACKET_COUNT);
  printf("Generated FIR packets sent.\n");

  CGRA_WAIT(wait_result);
  printf("WAIT result: 0x%lx\n", wait_result);

  CGRA_STATUS(status);
  printf("Final status: 0x%lx\n", status);

  CGRA_RESULT(result);
  printf("Result: 0x%lx\n", result);

  uint64_t complete = status & 0x1ULL;
  uint64_t complete_count = (status >> 1) & 0xFFFFULL;

  if (wait_result != 1 || complete != 1 ||
      complete_count != CGRA_FIR_YAML_4X4_EXPECTED_COMPLETES ||
      result != CGRA_FIR_YAML_4X4_EXPECTED_RESULT) {
    printf("CGRA RoCC FIR YAML 4x4: FAIL\n");
    return 1;
  }

  printf("CGRA RoCC FIR YAML 4x4: PASS\n");
  return 0;
}
