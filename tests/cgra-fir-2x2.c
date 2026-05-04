// CGRA RoCC single-CGRA 2x2 FIR smoke test.
//
// The packet sequence is a manual C port of
// VectorCGRA/cgra/test/CgraRTL_fir_2x2_test.py::sim_fir_return.

#include "cgra_protocol.h"
#include "cgra_runtime.h"
#include <stdint.h>
#include <stdio.h>

enum {
  CGRA_EXPECTED_COMPLETES = 1,
  CGRA_EXPECTED_RESULT = 2215,
};

static void preload_data(void) {
  for (uint8_t addr = 0; addr < 16; ++addr) {
    send_basic(0, CGRA_CMD_STORE_REQUEST, 10 + addr, 1, addr);
  }
}

static void configure_tile0(void) {
  send_basic(0, CGRA_CMD_CONST, 2, 1, 0);
  send_basic(0, CGRA_CMD_CONST, 3, 1, 0);
  send_basic(0, CGRA_CMD_CONFIG_COUNT_PER_ITER, 3, 1, 0);
  send_basic(0, CGRA_CMD_CONFIG_TOTAL_CTRL_COUNT, 34, 1, 0);

  send_config(0, 0, build_ctrl(
      OPT_PHI_CONST,
      (const uint8_t[4]){1, 2, 3, 4},
      (const uint8_t[12]){0, 0, 0, 0, 0, 0, 0, 0, PORT_EAST, 0, 0, 0},
      (const uint8_t[12]){0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0},
      0, 0, 0, 0));
  send_config(0, 1, build_ctrl(
      OPT_ADD_CONST_LD,
      (const uint8_t[4]){1, 2, 3, 4},
      (const uint8_t[12]){0, 0, 0, 0, 0, 0, 0, 0, PORT_NORTHEAST, 0, 0, 0},
      (const uint8_t[12]){1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
      0, 0, 0, 0));
  send_config(0, 2, build_ctrl(
      OPT_NAH,
      (const uint8_t[4]){1, 2, 3, 4},
      0, 0, 0, 0, 0, 0));

  send_prologue(0, CGRA_CMD_CONFIG_PROLOGUE_FU, 0, 1, (cgra_ctrl_t){0, 0, 0});
  send_prologue(0, CGRA_CMD_CONFIG_PROLOGUE_ROUTING_CROSSBAR, 0, 2, build_ctrl(
      0, 0,
      (const uint8_t[12]){PORT_EAST, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
      0, 0, 0, 0, 0));
  send_prologue(0, CGRA_CMD_CONFIG_PROLOGUE_FU_CROSSBAR, 0, 1, build_ctrl(
      0, 0, 0,
      (const uint8_t[12]){0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
      0, 0, 0, 0));
  send_basic(0, CGRA_CMD_LAUNCH, 0, 0, 0);
}

static void configure_tile1(void) {
  send_basic(1, CGRA_CMD_CONFIG_COUNT_PER_ITER, 3, 1, 0);
  send_basic(1, CGRA_CMD_CONFIG_TOTAL_CTRL_COUNT, 34, 1, 0);

  send_config(1, 0, build_ctrl(
      OPT_GRT_PRED,
      (const uint8_t[4]){1, 2, 3, 4},
      (const uint8_t[12]){0, 0, 0, 0, 0, 0, 0, 0, 0, PORT_NORTH, 0, 0},
      (const uint8_t[12]){0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0},
      (const uint8_t[4]){0, 2, 0, 0},
      0,
      (const uint8_t[4]){1, 0, 0, 0},
      0));
  send_config(1, 1, build_ctrl(
      OPT_ADD,
      (const uint8_t[4]){1, 2, 3, 4},
      (const uint8_t[12]){0, 0, 0, 0, 0, 0, 0, 0, PORT_NORTHWEST, PORT_WEST, 0, 0},
      (const uint8_t[12]){0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0},
      (const uint8_t[4]){2, 0, 0, 0},
      0, 0, 0));
  send_config(1, 2, build_ctrl(
      OPT_RET,
      (const uint8_t[4]){2, 0, 0, 0},
      0, 0, 0, 0,
      (const uint8_t[4]){0, 1, 0, 0},
      0));

  send_prologue(1, CGRA_CMD_CONFIG_PROLOGUE_FU, 0, 2, (cgra_ctrl_t){0, 0, 0});
  send_prologue(1, CGRA_CMD_CONFIG_PROLOGUE_ROUTING_CROSSBAR, 0, 2, build_ctrl(
      0, 0,
      (const uint8_t[12]){PORT_NORTH, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
      0, 0, 0, 0, 0));
  send_prologue(1, CGRA_CMD_CONFIG_PROLOGUE_FU_CROSSBAR, 0, 2, build_ctrl(
      0, 0, 0,
      (const uint8_t[12]){0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
      0, 0, 0, 0));
  send_prologue(1, CGRA_CMD_CONFIG_PROLOGUE_FU, 1, 1, (cgra_ctrl_t){0, 0, 0});
  send_prologue(1, CGRA_CMD_CONFIG_PROLOGUE_ROUTING_CROSSBAR, 1, 1, build_ctrl(
      0, 0,
      (const uint8_t[12]){PORT_WEST, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
      0, 0, 0, 0, 0));
  send_prologue(1, CGRA_CMD_CONFIG_PROLOGUE_ROUTING_CROSSBAR, 1, 1, build_ctrl(
      0, 0,
      (const uint8_t[12]){PORT_NORTHWEST, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
      0, 0, 0, 0, 0));
  send_prologue(1, CGRA_CMD_CONFIG_PROLOGUE_FU_CROSSBAR, 1, 1, build_ctrl(
      0, 0, 0,
      (const uint8_t[12]){0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
      0, 0, 0, 0));
  send_prologue(1, CGRA_CMD_CONFIG_PROLOGUE_FU, 2, 2, (cgra_ctrl_t){0, 0, 0});
  send_basic(1, CGRA_CMD_LAUNCH, 0, 0, 0);
}

static void configure_tile2(void) {
  send_basic(2, CGRA_CMD_CONST, 0, 1, 0);
  send_basic(2, CGRA_CMD_CONFIG_COUNT_PER_ITER, 3, 1, 0);
  send_basic(2, CGRA_CMD_CONFIG_TOTAL_CTRL_COUNT, 34, 1, 0);

  send_config(2, 0, build_ctrl(
      OPT_MUL,
      (const uint8_t[4]){1, 2, 3, 4},
      (const uint8_t[12]){0, 0, 0, 0, 0, 0, 0, 0, 0, PORT_SOUTH, 0, 0},
      (const uint8_t[12]){0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0},
      0, 0,
      (const uint8_t[4]){1, 0, 0, 0},
      0));
  send_config(2, 1, build_ctrl(
      OPT_ADD_CONST_LD,
      (const uint8_t[4]){1, 2, 3, 4},
      (const uint8_t[12]){0, 0, 0, 0, 0, 0, 0, 0, PORT_EAST, 0, 0, 0},
      (const uint8_t[12]){0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0},
      (const uint8_t[4]){2, 0, 0, 0},
      0, 0, 0));
  send_config(2, 2, build_ctrl(
      OPT_NAH,
      (const uint8_t[4]){1, 2, 3, 4},
      0, 0, 0, 0, 0, 0));

  send_prologue(2, CGRA_CMD_CONFIG_PROLOGUE_FU, 0, 1, (cgra_ctrl_t){0, 0, 0});
  send_prologue(2, CGRA_CMD_CONFIG_PROLOGUE_ROUTING_CROSSBAR, 0, 1, build_ctrl(
      0, 0,
      (const uint8_t[12]){PORT_SOUTH, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
      0, 0, 0, 0, 0));
  send_prologue(2, CGRA_CMD_CONFIG_PROLOGUE_FU_CROSSBAR, 0, 1, build_ctrl(
      0, 0, 0,
      (const uint8_t[12]){0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
      0, 0, 0, 0));
  send_basic(2, CGRA_CMD_LAUNCH, 0, 0, 0);
}

static void configure_tile3(void) {
  send_basic(3, CGRA_CMD_CONST, 2, 1, 0);
  send_basic(3, CGRA_CMD_CONST, 10, 1, 0);
  send_basic(3, CGRA_CMD_CONFIG_COUNT_PER_ITER, 3, 1, 0);
  send_basic(3, CGRA_CMD_CONFIG_TOTAL_CTRL_COUNT, 34, 1, 0);

  send_config(3, 0, build_ctrl(
      OPT_PHI_CONST,
      (const uint8_t[4]){2, 0, 0, 0},
      0,
      (const uint8_t[12]){0, 0, 1, 0, 0, 0, 0, 1, 1, 0, 0, 0},
      (const uint8_t[4]){2, 0, 0, 0},
      0,
      (const uint8_t[4]){0, 1, 0, 0},
      0));
  send_config(3, 1, build_ctrl(
      OPT_INC_NE_CONST_NOT_GRT,
      (const uint8_t[4]){1, 2, 3, 4},
      0,
      (const uint8_t[12]){0, 1, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0},
      (const uint8_t[4]){0, 2, 0, 0},
      0,
      (const uint8_t[4]){1, 0, 0, 0},
      0));
  send_config(3, 2, build_ctrl(
      OPT_NAH,
      (const uint8_t[4]){1, 2, 3, 4},
      0, 0, 0, 0, 0, 0));
  send_basic(3, CGRA_CMD_LAUNCH, 0, 0, 0);
}

int main(void) {
  uint64_t status = 0;
  uint64_t wait_result = 0;
  uint64_t result = 0;

  printf("CGRA RoCC FIR 2x2: Starting...\n");

  CGRA_STATUS(status);
  printf("Initial status: 0x%lx\n", status);

  CGRA_SET_EXPECTED_COMPLETES(CGRA_EXPECTED_COMPLETES);

  printf("Preloading data memory...\n");
  preload_data();

  printf("Configuring and launching FIR tiles...\n");
  configure_tile0();
  configure_tile1();
  configure_tile2();
  configure_tile3();

  CGRA_WAIT(wait_result);
  printf("WAIT result: 0x%lx\n", wait_result);

  CGRA_STATUS(status);
  printf("Final status: 0x%lx\n", status);

  CGRA_RESULT(result);
  printf("Result: 0x%lx\n", result);

  uint64_t complete = status & 0x1ULL;
  uint64_t complete_count = (status >> 1) & 0xFFFFULL;

  if (wait_result != 1 || complete != 1 ||
      complete_count != CGRA_EXPECTED_COMPLETES ||
      result != CGRA_EXPECTED_RESULT) {
    printf("CGRA RoCC FIR 2x2: FAIL\n");
    return 1;
  }

  printf("CGRA RoCC FIR 2x2: PASS\n");
  return 0;
}
