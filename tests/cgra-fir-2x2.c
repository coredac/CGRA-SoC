// CGRA RoCC single-CGRA 2x2 FIR smoke test.
//
// The packet sequence is a manual C port of
// VectorCGRA/cgra/test/CgraRTL_fir_2x2_test.py::sim_fir_return.

#include "rocc.h"
#include <stdint.h>
#include <stdio.h>

#define CGRA_STATUS(result) ROCC_INSTRUCTION_D(0, result, 2)
#define CGRA_WAIT(result) ROCC_INSTRUCTION_D(0, result, 4)
#define CGRA_SET_EXPECTED_COMPLETES(count) ROCC_INSTRUCTION_S(0, count, 8)
#define CGRA_RESULT(result) ROCC_INSTRUCTION_D(0, result, 9)
#define CGRA_RAW_PKT_LO(lo) ROCC_INSTRUCTION_S(0, lo, 5)
#define CGRA_RAW_PKT_MID(mid) ROCC_INSTRUCTION_S(0, mid, 6)
#define CGRA_RAW_PKT_HI(hi) ROCC_INSTRUCTION_S(0, hi, 7)
#define CGRA_RAW_PKT_TOP(top) ROCC_INSTRUCTION_S(0, top, 10)

enum {
  CGRA_EXPECTED_COMPLETES = 1,
  CGRA_EXPECTED_RESULT = 2215,

  CGRA_CMD_LAUNCH = 0,
  CGRA_CMD_CONFIG = 3,
  CGRA_CMD_CONFIG_PROLOGUE_FU = 4,
  CGRA_CMD_CONFIG_PROLOGUE_FU_CROSSBAR = 5,
  CGRA_CMD_CONFIG_PROLOGUE_ROUTING_CROSSBAR = 6,
  CGRA_CMD_CONFIG_TOTAL_CTRL_COUNT = 7,
  CGRA_CMD_CONFIG_COUNT_PER_ITER = 8,
  CGRA_CMD_STORE_REQUEST = 12,
  CGRA_CMD_CONST = 13,

  OPT_NAH = 1,
  OPT_ADD = 2,
  OPT_MUL = 7,
  OPT_GRT_PRED = 16,
  OPT_PHI_CONST = 32,
  OPT_RET = 35,
  OPT_ADD_CONST_LD = 81,
  OPT_INC_NE_CONST_NOT_GRT = 82,

  PORT_NORTH = 1,
  PORT_SOUTH = 2,
  PORT_WEST = 3,
  PORT_EAST = 4,
  PORT_NORTHWEST = 5,
  PORT_NORTHEAST = 6,

  DATA_ADDR_NBITS = 7,
  CTRL_HI_NBITS = 11,

  CTRL_READ_REG_IDX_LSB = 0,
  CTRL_READ_REG_FROM_LSB = 16,
  CTRL_WRITE_REG_IDX_LSB = 20,
  CTRL_WRITE_REG_FROM_LSB = 36,
  CTRL_FU_XBAR_OUTPORT_LSB = 48,
  CTRL_ROUTING_XBAR_OUTPORT_LSB = 72,
  CTRL_FU_IN_LSB = 120,
  CTRL_OPERATION_LSB = 132,

  PKT_CTRL_ADDR_LSB = 0,
  PKT_CTRL_LSB = 3,
  PKT_DATA_ADDR_LSB = 142,
  PKT_DATA_LSB = 149,
  PKT_CMD_LSB = 184,
  PKT_VC_ID_LSB = 189,
  PKT_OPAQUE_LSB = 190,
  PKT_DST_CGRA_Y_LSB = 198,
  PKT_DST_CGRA_X_LSB = 199,
  PKT_SRC_CGRA_Y_LSB = 200,
  PKT_SRC_CGRA_X_LSB = 201,
  PKT_DST_CGRA_ID_LSB = 202,
  PKT_SRC_CGRA_ID_LSB = 203,
  PKT_DST_TILE_LSB = 204,
  PKT_SRC_TILE_LSB = 207,
};

typedef struct {
  uint64_t lo;
  uint64_t mid;
  uint64_t hi;
} cgra_ctrl_t;

typedef struct {
  uint64_t lo;
  uint64_t mid;
  uint64_t hi;
  uint64_t top;
} cgra_packet_t;

static uint64_t data_raw(uint32_t payload, uint8_t predicate) {
  return (((uint64_t)payload) << 3) | (((uint64_t)predicate & 0x1ULL) << 2);
}

static void ctrl_set_bit(cgra_ctrl_t *ctrl, int bit_idx, uint64_t bit_val) {
  if (!bit_val) return;
  if (bit_idx < 64) {
    ctrl->lo |= (1ULL << bit_idx);
  } else if (bit_idx < 128) {
    ctrl->mid |= (1ULL << (bit_idx - 64));
  } else {
    ctrl->hi |= (1ULL << (bit_idx - 128));
  }
}

static void ctrl_set_bits(cgra_ctrl_t *ctrl, int lsb, int width, uint64_t value) {
  for (int i = 0; i < width; ++i) {
    ctrl_set_bit(ctrl, lsb + i, (value >> i) & 1ULL);
  }
}

static cgra_ctrl_t build_ctrl(uint8_t operation,
                              const uint8_t *fu_in,
                              const uint8_t *routing_xbar,
                              const uint8_t *fu_xbar,
                              const uint8_t *write_reg_from,
                              const uint8_t *write_reg_idx,
                              const uint8_t *read_reg_from,
                              const uint8_t *read_reg_idx) {
  cgra_ctrl_t ctrl = {0, 0, 0};
  ctrl_set_bits(&ctrl, CTRL_OPERATION_LSB, 7, operation);
  for (int i = 0; i < 4; ++i) {
    ctrl_set_bits(&ctrl, CTRL_FU_IN_LSB + i * 3, 3, fu_in ? fu_in[i] : 0);
    ctrl_set_bits(&ctrl, CTRL_WRITE_REG_FROM_LSB + i * 2, 2, write_reg_from ? write_reg_from[i] : 0);
    ctrl_set_bits(&ctrl, CTRL_WRITE_REG_IDX_LSB + i * 4, 4, write_reg_idx ? write_reg_idx[i] : 0);
    ctrl_set_bits(&ctrl, CTRL_READ_REG_FROM_LSB + i, 1, read_reg_from ? read_reg_from[i] : 0);
    ctrl_set_bits(&ctrl, CTRL_READ_REG_IDX_LSB + i * 4, 4, read_reg_idx ? read_reg_idx[i] : 0);
  }
  for (int i = 0; i < 12; ++i) {
    ctrl_set_bits(&ctrl, CTRL_ROUTING_XBAR_OUTPORT_LSB + i * 4, 4, routing_xbar ? routing_xbar[i] : 0);
    ctrl_set_bits(&ctrl, CTRL_FU_XBAR_OUTPORT_LSB + i * 2, 2, fu_xbar ? fu_xbar[i] : 0);
  }
  return ctrl;
}

static void pkt_set_bit(cgra_packet_t *pkt, int bit_idx, uint64_t bit_val) {
  if (!bit_val) return;
  if (bit_idx < 64) {
    pkt->lo |= (1ULL << bit_idx);
  } else if (bit_idx < 128) {
    pkt->mid |= (1ULL << (bit_idx - 64));
  } else if (bit_idx < 192) {
    pkt->hi |= (1ULL << (bit_idx - 128));
  } else {
    pkt->top |= (1ULL << (bit_idx - 192));
  }
}

static void pkt_set_bits(cgra_packet_t *pkt, int lsb, int width, uint64_t value) {
  for (int i = 0; i < width; ++i) {
    pkt_set_bit(pkt, lsb + i, (value >> i) & 1ULL);
  }
}

static cgra_packet_t build_intra_pkt(uint8_t src_tile,
                                     uint8_t dst_tile,
                                     uint8_t cmd,
                                     uint64_t data,
                                     uint8_t data_addr,
                                     cgra_ctrl_t ctrl,
                                     uint8_t ctrl_addr) {
  cgra_packet_t pkt = {0, 0, 0, 0};
  pkt_set_bits(&pkt, PKT_CTRL_ADDR_LSB, 3, ctrl_addr);
  pkt_set_bits(&pkt, PKT_CTRL_LSB, 64, ctrl.lo);
  pkt_set_bits(&pkt, PKT_CTRL_LSB + 64, 64, ctrl.mid);
  pkt_set_bits(&pkt, PKT_CTRL_LSB + 128, CTRL_HI_NBITS, ctrl.hi);
  pkt_set_bits(&pkt, PKT_DATA_ADDR_LSB, DATA_ADDR_NBITS, data_addr);
  pkt_set_bits(&pkt, PKT_DATA_LSB, 35, data);
  pkt_set_bits(&pkt, PKT_CMD_LSB, 5, cmd);
  pkt_set_bits(&pkt, PKT_VC_ID_LSB, 1, 0);
  pkt_set_bits(&pkt, PKT_OPAQUE_LSB, 8, 0);
  pkt_set_bits(&pkt, PKT_DST_CGRA_Y_LSB, 1, 0);
  pkt_set_bits(&pkt, PKT_DST_CGRA_X_LSB, 1, 0);
  pkt_set_bits(&pkt, PKT_SRC_CGRA_Y_LSB, 1, 0);
  pkt_set_bits(&pkt, PKT_SRC_CGRA_X_LSB, 1, 0);
  pkt_set_bits(&pkt, PKT_DST_CGRA_ID_LSB, 1, 0);
  pkt_set_bits(&pkt, PKT_SRC_CGRA_ID_LSB, 1, 0);
  pkt_set_bits(&pkt, PKT_DST_TILE_LSB, 3, dst_tile);
  pkt_set_bits(&pkt, PKT_SRC_TILE_LSB, 3, src_tile);
  return pkt;
}

static void send_packet(cgra_packet_t pkt) {
  CGRA_RAW_PKT_LO(pkt.lo);
  CGRA_RAW_PKT_MID(pkt.mid);
  CGRA_RAW_PKT_HI(pkt.hi);
  CGRA_RAW_PKT_TOP(pkt.top);
}

static void send_basic(uint8_t tile, uint8_t cmd, uint32_t data, uint8_t predicate, uint8_t data_addr) {
  send_packet(build_intra_pkt(0, tile, cmd, data_raw(data, predicate), data_addr,
                              (cgra_ctrl_t){0, 0, 0}, 0));
}

static void send_config(uint8_t tile, uint8_t ctrl_addr, cgra_ctrl_t ctrl) {
  send_packet(build_intra_pkt(0, tile, CGRA_CMD_CONFIG, 0, 0, ctrl, ctrl_addr));
}

static void send_prologue(uint8_t tile, uint8_t cmd, uint8_t ctrl_addr, uint32_t count, cgra_ctrl_t ctrl) {
  send_packet(build_intra_pkt(0, tile, cmd, data_raw(count, 1), 0, ctrl, ctrl_addr));
}

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
