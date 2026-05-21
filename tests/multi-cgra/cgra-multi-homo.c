// CGRA RoCC multi-CGRA 2x2 homo MeshRTL reference test, handwritten from
// VectorCGRA/multi_cgra/test/MeshMultiCgraRTL_test.py::test_homo.

#include "cgra_protocol.h"
#include "cgra_runtime.h"
#include <stdint.h>
#include <stdio.h>

enum {
  MULTI_HOMO_EXPECTED_COMPLETES = 2,
  MULTI_HOMO_LOAD_ADDR = 34,
  MULTI_HOMO_STORE_ADDR = 3,
  MULTI_HOMO_INPUT_VALUE = 0xfe,
  MULTI_HOMO_EXPECTED_VALUE = 0xff,
};

static cgra_ctrl_t make_ctrl_load(void) {
  const int first_fu_in_xbar =
      CTRL_ROUTING_XBAR_OUTPORT_COUNT - CTRL_FU_IN_COUNT;

  uint8_t fu_in[CTRL_FU_IN_COUNT] = {0};
  uint8_t routing_xbar[CTRL_ROUTING_XBAR_OUTPORT_COUNT] = {0};
  uint8_t fu_xbar[CTRL_FU_XBAR_OUTPORT_COUNT] = {0};
  uint8_t write_reg_from[CTRL_FU_IN_COUNT] = {0};
  uint8_t write_reg_idx[CTRL_FU_IN_COUNT] = {0};
  uint8_t read_reg_from[CTRL_FU_IN_COUNT] = {0};
  uint8_t read_reg_idx[CTRL_FU_IN_COUNT] = {0};

  fu_xbar[first_fu_in_xbar] = 1;
  write_reg_from[0] = 2;
  write_reg_idx[0] = 7;

  return build_ctrl(OPT_LD_CONST, fu_in, routing_xbar, fu_xbar,
                    write_reg_from, write_reg_idx, read_reg_from,
                    read_reg_idx);
}

static cgra_ctrl_t make_ctrl_inc(void) {
  uint8_t fu_in[CTRL_FU_IN_COUNT] = {0};
  uint8_t routing_xbar[CTRL_ROUTING_XBAR_OUTPORT_COUNT] = {0};
  uint8_t fu_xbar[CTRL_FU_XBAR_OUTPORT_COUNT] = {0};
  uint8_t write_reg_from[CTRL_FU_IN_COUNT] = {0};
  uint8_t write_reg_idx[CTRL_FU_IN_COUNT] = {0};
  uint8_t read_reg_from[CTRL_FU_IN_COUNT] = {0};
  uint8_t read_reg_idx[CTRL_FU_IN_COUNT] = {0};

  fu_in[0] = 1;
  fu_xbar[0] = 1;
  read_reg_from[0] = 1;
  read_reg_idx[0] = 7;

  return build_ctrl(OPT_INC, fu_in, routing_xbar, fu_xbar, write_reg_from,
                    write_reg_idx, read_reg_from, read_reg_idx);
}

static cgra_ctrl_t make_ctrl_store(void) {
  const int first_fu_in_xbar =
      CTRL_ROUTING_XBAR_OUTPORT_COUNT - CTRL_FU_IN_COUNT;

  uint8_t fu_in[CTRL_FU_IN_COUNT] = {0};
  uint8_t routing_xbar[CTRL_ROUTING_XBAR_OUTPORT_COUNT] = {0};
  uint8_t fu_xbar[CTRL_FU_XBAR_OUTPORT_COUNT] = {0};
  uint8_t write_reg_from[CTRL_FU_IN_COUNT] = {0};
  uint8_t write_reg_idx[CTRL_FU_IN_COUNT] = {0};
  uint8_t read_reg_from[CTRL_FU_IN_COUNT] = {0};
  uint8_t read_reg_idx[CTRL_FU_IN_COUNT] = {0};

  fu_in[0] = 1;
  routing_xbar[first_fu_in_xbar] = PORT_SOUTH;

  return build_ctrl(OPT_STR_CONST, fu_in, routing_xbar, fu_xbar,
                    write_reg_from, write_reg_idx, read_reg_from,
                    read_reg_idx);
}

static void preload_data(void) {
  send_basic(0, CGRA_CMD_STORE_REQUEST, MULTI_HOMO_INPUT_VALUE, 1,
             MULTI_HOMO_LOAD_ADDR);
}

static void configure_and_launch_homo(void) {
  cgra_target_t target_cgra2 = {0, 2, 0, 0, 0, 1};

  send_basic_to(target_cgra2, 0, CGRA_CMD_CONST, MULTI_HOMO_LOAD_ADDR, 1, 0);
  send_config_to(target_cgra2, 0, 0, make_ctrl_load());
  send_config_to(target_cgra2, 0, 1, make_ctrl_inc());

  send_basic_to(target_cgra2, 2, CGRA_CMD_CONST, MULTI_HOMO_STORE_ADDR, 1, 0);
  send_config_to(target_cgra2, 2, 0, make_ctrl_store());
  send_basic_to(target_cgra2, 2, CGRA_CMD_CONFIG_TOTAL_CTRL_COUNT, 1, 1, 0);

  send_basic_to(target_cgra2, 0, CGRA_CMD_LAUNCH, 0, 0, 0);
  send_basic_to(target_cgra2, 2, CGRA_CMD_LAUNCH, 0, 0, 0);
}

int main(void) {
  uint64_t status = 0;
  uint64_t wait_result = 0;

  printf("CGRA RoCC multi homo MeshRTL: Starting...\n");
  CGRA_STATUS(status);
  printf("Initial status: 0x%lx\n", status);

  CGRA_SET_EXPECTED_COMPLETES(MULTI_HOMO_EXPECTED_COMPLETES);

  printf("Preloading multi-CGRA data memory...\n");
  preload_data();

  printf("Configuring and launching CGRA2 tile0/tile2...\n");
  configure_and_launch_homo();

  CGRA_WAIT(wait_result);
  printf("WAIT result: 0x%lx\n", wait_result);

  CGRA_STATUS(status);
  printf("Final status: 0x%lx\n", status);

  uint32_t v34 = read_mem(MULTI_HOMO_LOAD_ADDR);
  uint32_t v3 = read_mem(MULTI_HOMO_STORE_ADDR);
  printf("read_mem(%u): 0x%x\n", MULTI_HOMO_LOAD_ADDR, v34);
  printf("read_mem(%u): 0x%x\n", MULTI_HOMO_STORE_ADDR, v3);

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
