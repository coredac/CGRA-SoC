// CGRA RoCC multi-CGRA 2x2 systolic test, handwritten from
// VectorCGRA/multi_cgra/test/MeshMultiCgraRTL_test.py::test_systolic.

#include "cgra_protocol.h"
#include "cgra_runtime.h"
#include "cgra_multi_systolic_common.h"
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

static void preload_data(void) {
  cgra_multi_systolic_preload(64, 1);
  cgra_multi_systolic_preload(65, 2);
  cgra_multi_systolic_preload(66, 3);
  cgra_multi_systolic_preload(67, 4);
  cgra_multi_systolic_preload(68, 5);
  cgra_multi_systolic_preload(69, 6);
  cgra_multi_systolic_preload(0, 7);
  cgra_multi_systolic_preload(1, 8);
  cgra_multi_systolic_preload(2, 9);
}

static void configure_and_launch_systolic(void) {
  const cgra_target_t target_cgra2 = {0, 2, 0, 0, 0, 1};
  const cgra_target_t target_cgra0 = {0, 0, 0, 0, 0, 0};
  const cgra_target_t target_cgra3 = {0, 3, 0, 0, 1, 1};
  const cgra_target_t target_cgra1 = {0, 1, 0, 0, 1, 0};

  cgra_multi_systolic_send_const(target_cgra2, 2, 64);
  cgra_multi_systolic_send_const(target_cgra2, 2, 65);
  cgra_multi_systolic_send_const(target_cgra2, 2, 66);
  cgra_multi_systolic_send_count_per_iter(target_cgra2, 2);
  cgra_multi_systolic_send_total_ctrl_count(target_cgra2, 2);
  cgra_multi_systolic_send_config(target_cgra2, 2,
                                  cgra_multi_systolic_ctrl_ld_const_east());
  cgra_multi_systolic_send_launch(target_cgra2, 2);

  cgra_multi_systolic_send_const(target_cgra2, 0, 67);
  cgra_multi_systolic_send_const(target_cgra2, 0, 68);
  cgra_multi_systolic_send_const(target_cgra2, 0, 69);
  cgra_multi_systolic_send_count_per_iter(target_cgra2, 0);
  cgra_multi_systolic_send_total_ctrl_count(target_cgra2, 0);
  cgra_multi_systolic_send_config(target_cgra2, 0,
                                  cgra_multi_systolic_ctrl_ld_const_east());
  cgra_multi_systolic_send_launch(target_cgra2, 0);

  cgra_multi_systolic_send_const(target_cgra0, 2, 0);
  cgra_multi_systolic_send_const(target_cgra0, 2, 1);
  cgra_multi_systolic_send_const(target_cgra0, 2, 2);
  cgra_multi_systolic_send_count_per_iter(target_cgra0, 2);
  cgra_multi_systolic_send_total_ctrl_count(target_cgra0, 2);
  cgra_multi_systolic_send_config(target_cgra0, 2,
                                  cgra_multi_systolic_ctrl_ld_const_east());
  cgra_multi_systolic_send_launch(target_cgra0, 2);

  cgra_multi_systolic_send_const(target_cgra2, 3, 2);
  cgra_multi_systolic_send_count_per_iter(target_cgra2, 3);
  cgra_multi_systolic_send_total_ctrl_count(target_cgra2, 3);
  cgra_multi_systolic_send_config(
      target_cgra2, 3,
      cgra_multi_systolic_ctrl_mul_const_forward_west_east_south());
  cgra_multi_systolic_send_launch(target_cgra2, 3);

  cgra_multi_systolic_send_const(target_cgra2, 1, 4);
  cgra_multi_systolic_send_count_per_iter(target_cgra2, 1);
  cgra_multi_systolic_send_total_ctrl_count(target_cgra2, 1);
  cgra_multi_systolic_send_config(
      target_cgra2, 1,
      cgra_multi_systolic_ctrl_mul_const_add_forward_west_east_south());
  cgra_multi_systolic_send_launch(target_cgra2, 1);

  cgra_multi_systolic_send_const(target_cgra0, 3, 6);
  cgra_multi_systolic_send_count_per_iter(target_cgra0, 3);
  cgra_multi_systolic_send_total_ctrl_count(target_cgra0, 3);
  cgra_multi_systolic_send_config(
      target_cgra0, 3,
      cgra_multi_systolic_ctrl_mul_const_add_forward_west_east_south());
  cgra_multi_systolic_send_launch(target_cgra0, 3);

  cgra_multi_systolic_send_const(target_cgra0, 1, 3);
  cgra_multi_systolic_send_const(target_cgra0, 1, 4);
  cgra_multi_systolic_send_const(target_cgra0, 1, 5);
  cgra_multi_systolic_send_count_per_iter(target_cgra0, 1);
  cgra_multi_systolic_send_total_ctrl_count(target_cgra0, 1);
  cgra_multi_systolic_send_config(target_cgra0, 1,
                                  cgra_multi_systolic_ctrl_store_from_north());
  cgra_multi_systolic_send_launch(target_cgra0, 1);

  cgra_multi_systolic_send_const(target_cgra3, 2, 8);
  cgra_multi_systolic_send_count_per_iter(target_cgra3, 2);
  cgra_multi_systolic_send_total_ctrl_count(target_cgra3, 2);
  cgra_multi_systolic_send_config(
      target_cgra3, 2,
      cgra_multi_systolic_ctrl_mul_const_forward_west_east_south());
  cgra_multi_systolic_send_launch(target_cgra3, 2);

  cgra_multi_systolic_send_const(target_cgra3, 0, 10);
  cgra_multi_systolic_send_count_per_iter(target_cgra3, 0);
  cgra_multi_systolic_send_total_ctrl_count(target_cgra3, 0);
  cgra_multi_systolic_send_config(
      target_cgra3, 0,
      cgra_multi_systolic_ctrl_mul_const_add_forward_west_east_south());
  cgra_multi_systolic_send_launch(target_cgra3, 0);

  cgra_multi_systolic_send_const(target_cgra1, 2, 12);
  cgra_multi_systolic_send_count_per_iter(target_cgra1, 2);
  cgra_multi_systolic_send_total_ctrl_count(target_cgra1, 2);
  cgra_multi_systolic_send_config(
      target_cgra1, 2,
      cgra_multi_systolic_ctrl_mul_const_add_forward_west_east_south());
  cgra_multi_systolic_send_launch(target_cgra1, 2);

  cgra_multi_systolic_send_const(target_cgra1, 0, 32);
  cgra_multi_systolic_send_const(target_cgra1, 0, 33);
  cgra_multi_systolic_send_const(target_cgra1, 0, 34);
  cgra_multi_systolic_send_count_per_iter(target_cgra1, 0);
  cgra_multi_systolic_send_total_ctrl_count(target_cgra1, 0);
  cgra_multi_systolic_send_config(target_cgra1, 0,
                                  cgra_multi_systolic_ctrl_store_from_north());
  cgra_multi_systolic_send_launch(target_cgra1, 0);

  cgra_multi_systolic_send_const(target_cgra3, 3, 14);
  cgra_multi_systolic_send_count_per_iter(target_cgra3, 3);
  cgra_multi_systolic_send_total_ctrl_count(target_cgra3, 3);
  cgra_multi_systolic_send_config(
      target_cgra3, 3, cgra_multi_systolic_ctrl_mul_const_from_west_south());
  cgra_multi_systolic_send_launch(target_cgra3, 3);

  cgra_multi_systolic_send_const(target_cgra3, 1, 16);
  cgra_multi_systolic_send_count_per_iter(target_cgra3, 1);
  cgra_multi_systolic_send_total_ctrl_count(target_cgra3, 1);
  cgra_multi_systolic_send_config(
      target_cgra3, 1,
      cgra_multi_systolic_ctrl_mul_const_add_from_west_north_south());
  cgra_multi_systolic_send_launch(target_cgra3, 1);

  cgra_multi_systolic_send_const(target_cgra1, 3, 18);
  cgra_multi_systolic_send_count_per_iter(target_cgra1, 3);
  cgra_multi_systolic_send_total_ctrl_count(target_cgra1, 3);
  cgra_multi_systolic_send_config(
      target_cgra1, 3,
      cgra_multi_systolic_ctrl_mul_const_add_from_west_north_south());
  cgra_multi_systolic_send_launch(target_cgra1, 3);

  cgra_multi_systolic_send_const(target_cgra1, 1, 35);
  cgra_multi_systolic_send_const(target_cgra1, 1, 36);
  cgra_multi_systolic_send_const(target_cgra1, 1, 37);
  cgra_multi_systolic_send_count_per_iter(target_cgra1, 1);
  cgra_multi_systolic_send_total_ctrl_count(target_cgra1, 1);
  cgra_multi_systolic_send_config(target_cgra1, 1,
                                  cgra_multi_systolic_ctrl_store_from_north());
  cgra_multi_systolic_send_launch(target_cgra1, 1);
}

int main(void) {
  uint64_t status = 0;
  uint64_t wait_result = 0;

  printf("CGRA RoCC multi systolic 2x2 MeshRTL: Starting...\n");
  CGRA_STATUS(status);
  printf("Initial status: 0x%lx\n", status);

  CGRA_SET_EXPECTED_COMPLETES(SYSTOLIC_EXPECTED_COMPLETES);

  printf("Preloading multi-CGRA data memory...\n");
  preload_data();

  printf("Configuring and launching systolic CGRAs...\n");
  configure_and_launch_systolic();

  CGRA_WAIT(wait_result);
  printf("WAIT result: 0x%lx\n", wait_result);

  CGRA_STATUS(status);
  printf("Final status: 0x%lx\n", status);

  uint32_t v3 = read_mem(3);
  uint32_t v4 = read_mem(4);
  uint32_t v5 = read_mem(5);
  uint32_t v32 = read_mem(32);
  uint32_t v33 = read_mem(33);
  uint32_t v34 = read_mem(34);
  uint32_t v35 = read_mem(35);
  uint32_t v36 = read_mem(36);
  uint32_t v37 = read_mem(37);

  printf("read_mem(3): 0x%x\n", v3);
  printf("read_mem(4): 0x%x\n", v4);
  printf("read_mem(5): 0x%x\n", v5);
  printf("read_mem(32): 0x%x\n", v32);
  printf("read_mem(33): 0x%x\n", v33);
  printf("read_mem(34): 0x%x\n", v34);
  printf("read_mem(35): 0x%x\n", v35);
  printf("read_mem(36): 0x%x\n", v36);
  printf("read_mem(37): 0x%x\n", v37);

  uint64_t complete = status & 0x1ULL;
  uint64_t complete_count = (status >> 1) & 0xFFFFULL;

  if (wait_result != 1 || complete != 1 ||
      complete_count != SYSTOLIC_EXPECTED_COMPLETES ||
      v3 != SYSTOLIC_EXPECTED_ADDR_3 || v4 != SYSTOLIC_EXPECTED_ADDR_4 ||
      v5 != SYSTOLIC_EXPECTED_ADDR_5 || v32 != SYSTOLIC_EXPECTED_ADDR_32 ||
      v33 != SYSTOLIC_EXPECTED_ADDR_33 || v34 != SYSTOLIC_EXPECTED_ADDR_34 ||
      v35 != SYSTOLIC_EXPECTED_ADDR_35 || v36 != SYSTOLIC_EXPECTED_ADDR_36 ||
      v37 != SYSTOLIC_EXPECTED_ADDR_37) {
    printf("CGRA RoCC multi systolic 2x2 MeshRTL: FAIL\n");
    return 1;
  }

  printf("CGRA RoCC multi systolic 2x2 MeshRTL: PASS\n");
  return 0;
}
