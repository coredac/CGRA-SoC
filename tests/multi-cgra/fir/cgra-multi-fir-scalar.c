// CGRA RoCC multi-CGRA FIR scalar test, handwritten from
// VectorCGRA/multi_cgra/test/MeshMultiCgraRTL_test.py::test_fir_scalar.

#include "cgra_multi_fir_common.h"
#include <stdint.h>
#include <stdio.h>

int main(void) {
  uint64_t status = 0;
  uint64_t wait_result = 0;
  uint64_t result = 0;

  printf("CGRA RoCC multi FIR scalar 4x4: Starting...\n");
  CGRA_STATUS(status);
  printf("Initial status: 0x%lx\n", status);

  CGRA_SET_EXPECTED_COMPLETES(CGRA_MULTI_FIR_EXPECTED_COMPLETES);

  printf("Preloading FIR scalar data memory...\n");
  cgra_multi_fir_preload_scalar();

  printf("Configuring and launching FIR scalar tiles...\n");
  cgra_multi_fir_configure_4x4(OPT_ADD, OPT_MUL,
                               CGRA_MULTI_FIR_SCALAR_LOOP_UPPER_BOUND,
                               CGRA_MULTI_FIR_SCALAR_TOTAL_CTRL_STEPS);

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
    printf("CGRA RoCC multi FIR scalar 4x4: FAIL\n");
    return 1;
  }

  printf("CGRA RoCC multi FIR scalar 4x4: PASS\n");
  return 0;
}
