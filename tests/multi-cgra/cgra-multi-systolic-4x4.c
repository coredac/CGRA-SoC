// CGRA RoCC multi-CGRA 4x4 systolic test, handwritten from
// VectorCGRA/multi_cgra/test/MeshMultiCgraRTL_test.py::test_systolic_4x4_2x2.

#include "cgra_protocol.h"
#include "cgra_runtime.h"
#include "cgra_multi_systolic_4x4_packets.h"
#include <stdint.h>
#include <stdio.h>

enum {
  SYSTOLIC_EXPECTED_COMPLETES = 15,
  SYSTOLIC_EXPECTED_ADDR_3 = 0x3c,
  SYSTOLIC_EXPECTED_ADDR_4 = 0x48,
  SYSTOLIC_EXPECTED_ADDR_5 = 0x54,
  SYSTOLIC_EXPECTED_ADDR_32 = 0x84,
  SYSTOLIC_EXPECTED_ADDR_33 = 0xa2,
  SYSTOLIC_EXPECTED_ADDR_34 = 0xc0,
  SYSTOLIC_EXPECTED_ADDR_35 = 0xcc,
  SYSTOLIC_EXPECTED_ADDR_36 = 0xfc,
  SYSTOLIC_EXPECTED_ADDR_37 = 0x12c,
};

static uint64_t load_result_from_packet(cgra_packet_t pkt) {
  uint64_t result = 0;
  cgra_send_packet_fast(pkt);
  CGRA_LOAD_RESULT(result);
  return result;
}

int main(void) {
  uint64_t status = 0;
  uint64_t wait_result = 0;

  printf("CGRA RoCC multi systolic 4x4 MeshRTL: Starting...\n");
  CGRA_STATUS(status);
  printf("Initial status: 0x%lx\n", status);

  CGRA_SET_EXPECTED_COMPLETES(SYSTOLIC_EXPECTED_COMPLETES);

  printf("Preloading multi-CGRA data memory...\n");
  cgra_send_packets_fast(CGRA_MULTI_SYSTOLIC_4X4_PRELOAD_PACKETS,
                         CGRA_MULTI_SYSTOLIC_4X4_PRELOAD_PACKET_COUNT);

  printf("Configuring and launching systolic CGRAs...\n");
  cgra_send_packets_fast(CGRA_MULTI_SYSTOLIC_4X4_CONFIG_PACKETS,
                         CGRA_MULTI_SYSTOLIC_4X4_CONFIG_PACKET_COUNT);
  cgra_send_packets_fast(CGRA_MULTI_SYSTOLIC_4X4_LAUNCH_PACKETS,
                         CGRA_MULTI_SYSTOLIC_4X4_LAUNCH_PACKET_COUNT);

  CGRA_WAIT(wait_result);
  printf("WAIT result: 0x%lx\n", wait_result);

  CGRA_STATUS(status);
  printf("Final status: 0x%lx\n", status);

  uint64_t v3 =
      load_result_from_packet(CGRA_MULTI_SYSTOLIC_4X4_READBACK_PACKETS[0]);
  uint64_t v4 =
      load_result_from_packet(CGRA_MULTI_SYSTOLIC_4X4_READBACK_PACKETS[1]);
  uint64_t v5 =
      load_result_from_packet(CGRA_MULTI_SYSTOLIC_4X4_READBACK_PACKETS[2]);
  uint64_t v32 =
      load_result_from_packet(CGRA_MULTI_SYSTOLIC_4X4_READBACK_PACKETS[3]);
  uint64_t v33 =
      load_result_from_packet(CGRA_MULTI_SYSTOLIC_4X4_READBACK_PACKETS[4]);
  uint64_t v34 =
      load_result_from_packet(CGRA_MULTI_SYSTOLIC_4X4_READBACK_PACKETS[5]);
  uint64_t v35 =
      load_result_from_packet(CGRA_MULTI_SYSTOLIC_4X4_READBACK_PACKETS[6]);
  uint64_t v36 =
      load_result_from_packet(CGRA_MULTI_SYSTOLIC_4X4_READBACK_PACKETS[7]);
  uint64_t v37 =
      load_result_from_packet(CGRA_MULTI_SYSTOLIC_4X4_READBACK_PACKETS[8]);

  printf("load addr 3: 0x%lx\n", v3);
  printf("load addr 4: 0x%lx\n", v4);
  printf("load addr 5: 0x%lx\n", v5);
  printf("load addr 32: 0x%lx\n", v32);
  printf("load addr 33: 0x%lx\n", v33);
  printf("load addr 34: 0x%lx\n", v34);
  printf("load addr 35: 0x%lx\n", v35);
  printf("load addr 36: 0x%lx\n", v36);
  printf("load addr 37: 0x%lx\n", v37);

  uint64_t complete = status & 0x1ULL;
  uint64_t complete_count = (status >> 1) & 0xFFFFULL;

  if (wait_result != 1 || complete != 1 ||
      complete_count != SYSTOLIC_EXPECTED_COMPLETES ||
      v3 != SYSTOLIC_EXPECTED_ADDR_3 || v4 != SYSTOLIC_EXPECTED_ADDR_4 ||
      v5 != SYSTOLIC_EXPECTED_ADDR_5 || v32 != SYSTOLIC_EXPECTED_ADDR_32 ||
      v33 != SYSTOLIC_EXPECTED_ADDR_33 || v34 != SYSTOLIC_EXPECTED_ADDR_34 ||
      v35 != SYSTOLIC_EXPECTED_ADDR_35 || v36 != SYSTOLIC_EXPECTED_ADDR_36 ||
      v37 != SYSTOLIC_EXPECTED_ADDR_37) {
    printf("CGRA RoCC multi systolic 4x4 MeshRTL: FAIL\n");
    return 1;
  }

  printf("CGRA RoCC multi systolic 4x4 MeshRTL: PASS\n");
  return 0;
}
