// CGRA RoCC multi-CGRA FIR vector test, handwritten from
// VectorCGRA/multi_cgra/test/MeshMultiCgraRTL_test.py::test_fir_vector.

#include "cgra_protocol.h"
#include "cgra_runtime.h"
#include "cgra_multi_fir_vector_packets.h"
#include <stdint.h>
#include <stdio.h>

enum {
  CGRA_MULTI_FIR_EXPECTED_COMPLETES = 1,
  CGRA_MULTI_FIR_EXPECTED_RESULT = 2215,
};

int main(void) {
  uint64_t status = 0;
  uint64_t wait_result = 0;
  uint64_t result = 0;

  printf("CGRA RoCC multi FIR vector 4x4: Starting...\n");
  CGRA_STATUS(status);
  printf("Initial status: 0x%lx\n", status);

  CGRA_SET_EXPECTED_COMPLETES(CGRA_MULTI_FIR_EXPECTED_COMPLETES);

  printf("Preloading FIR vector data memory...\n");
  cgra_send_packets_fast(CGRA_MULTI_FIR_VECTOR_PRELOAD_PACKETS,
                         CGRA_MULTI_FIR_VECTOR_PRELOAD_PACKET_COUNT);

  printf("Configuring and launching FIR vector tiles...\n");
  cgra_send_packets_fast(CGRA_MULTI_FIR_VECTOR_CONFIG_PACKETS,
                         CGRA_MULTI_FIR_VECTOR_CONFIG_PACKET_COUNT);
  cgra_send_packets_fast(CGRA_MULTI_FIR_VECTOR_PROLOGUE_PACKETS,
                         CGRA_MULTI_FIR_VECTOR_PROLOGUE_PACKET_COUNT);
  cgra_send_packets_fast(CGRA_MULTI_FIR_VECTOR_LAUNCH_PACKETS,
                         CGRA_MULTI_FIR_VECTOR_LAUNCH_PACKET_COUNT);

  CGRA_WAIT(wait_result);
  printf("WAIT result: 0x%lx\n", wait_result);

  CGRA_STATUS(status);
  printf("Final status: 0x%lx\n", status);

  CGRA_RESULT(result);
  printf("Result: 0x%lx\n", result);

  uint64_t complete = status & 0x1ULL;
  uint64_t complete_count = (status >> 1) & 0xFFFFULL;

  if (wait_result != 1 || complete != 1 ||
      complete_count != CGRA_MULTI_FIR_EXPECTED_COMPLETES ||
      result != CGRA_MULTI_FIR_EXPECTED_RESULT) {
    printf("CGRA RoCC multi FIR vector 4x4: FAIL\n");
    return 1;
  }

  printf("CGRA RoCC multi FIR vector 4x4: PASS\n");
  return 0;
}
