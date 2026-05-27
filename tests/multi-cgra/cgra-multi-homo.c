// CGRA RoCC multi-CGRA 2x2 homo MeshRTL reference test, handwritten from
// VectorCGRA/multi_cgra/test/MeshMultiCgraRTL_test.py::test_homo.

#include "cgra_protocol.h"
#include "cgra_runtime.h"
#include "cgra_multi_homo_packets.h"
#include <stdint.h>
#include <stdio.h>

enum {
  MULTI_HOMO_EXPECTED_COMPLETES = 2,
  MULTI_HOMO_LOAD_ADDR = 34,
  MULTI_HOMO_STORE_ADDR = 3,
  MULTI_HOMO_INPUT_VALUE = 0xfe,
  MULTI_HOMO_EXPECTED_VALUE = 0xff,
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

  printf("CGRA RoCC multi homo MeshRTL: Starting...\n");
  CGRA_STATUS(status);
  printf("Initial status: 0x%lx\n", status);

  CGRA_SET_EXPECTED_COMPLETES(MULTI_HOMO_EXPECTED_COMPLETES);

  printf("Preloading multi-CGRA data memory...\n");
  cgra_send_packets_fast(CGRA_MULTI_HOMO_PRELOAD_PACKETS,
                         CGRA_MULTI_HOMO_PRELOAD_PACKET_COUNT);

  printf("Configuring and launching CGRA2 tile0/tile2...\n");
  cgra_send_packets_fast(CGRA_MULTI_HOMO_CONFIG_PACKETS,
                         CGRA_MULTI_HOMO_CONFIG_PACKET_COUNT);
  cgra_send_packets_fast(CGRA_MULTI_HOMO_LAUNCH_PACKETS,
                         CGRA_MULTI_HOMO_LAUNCH_PACKET_COUNT);

  CGRA_WAIT(wait_result);
  printf("WAIT result: 0x%lx\n", wait_result);

  CGRA_STATUS(status);
  printf("Final status: 0x%lx\n", status);

  uint64_t v34 = load_result_from_packet(CGRA_MULTI_HOMO_READBACK_PACKETS[0]);
  uint64_t v3 = load_result_from_packet(CGRA_MULTI_HOMO_READBACK_PACKETS[1]);
  printf("load addr %u: 0x%lx\n", MULTI_HOMO_LOAD_ADDR, v34);
  printf("load addr %u: 0x%lx\n", MULTI_HOMO_STORE_ADDR, v3);

  uint64_t complete = status & 0x1ULL;
  uint64_t complete_count = (status >> 1) & 0xFFFFULL;

  if (wait_result != 1 || complete != 1 ||
      complete_count != MULTI_HOMO_EXPECTED_COMPLETES ||
      v34 != MULTI_HOMO_INPUT_VALUE || v3 != MULTI_HOMO_EXPECTED_VALUE) {
    printf("CGRA RoCC multi homo MeshRTL: FAIL\n");
    return 1;
  }

  printf("CGRA RoCC multi homo MeshRTL: PASS\n");
  return 0;
}
