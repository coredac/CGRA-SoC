#ifndef CGRA_RUNTIME_H
#define CGRA_RUNTIME_H

#include "cgra_layout.h"
#include "cgra_protocol.h"
#include <stddef.h>
#include <stdint.h>

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

static inline uint64_t cgra_data_raw(uint32_t payload, uint8_t predicate) {
  return (((uint64_t)payload) << DATA_PAYLOAD_LSB) |
         (((uint64_t)predicate & 0x1ULL) << DATA_PREDICATE_LSB);
}

static inline cgra_ctrl_t cgra_ctrl_empty(void) {
  return (cgra_ctrl_t){0, 0, 0};
}

static inline void cgra_ctrl_set_bit(cgra_ctrl_t *ctrl, int bit_idx,
                                     uint64_t bit_val) {
  if (!bit_val) return;
  if (bit_idx < 64) {
    ctrl->lo |= (1ULL << bit_idx);
  } else if (bit_idx < 128) {
    ctrl->mid |= (1ULL << (bit_idx - 64));
  } else {
    ctrl->hi |= (1ULL << (bit_idx - 128));
  }
}

static inline void cgra_ctrl_set_bits(cgra_ctrl_t *ctrl, int lsb, int width,
                                      uint64_t value) {
  for (int i = 0; i < width; ++i) {
    cgra_ctrl_set_bit(ctrl, lsb + i, (value >> i) & 1ULL);
  }
}

static inline cgra_ctrl_t cgra_build_ctrl(
    uint8_t operation, const uint8_t *fu_in, const uint8_t *routing_xbar,
    const uint8_t *fu_xbar, const uint8_t *write_reg_from,
    const uint8_t *write_reg_idx, const uint8_t *read_reg_from,
    const uint8_t *read_reg_idx) {
  cgra_ctrl_t ctrl = cgra_ctrl_empty();
  cgra_ctrl_set_bits(&ctrl, CTRL_OPERATION_LSB, CTRL_OPERATION_NBITS,
                     operation);
  for (int i = 0; i < CTRL_FU_IN_COUNT; ++i) {
    cgra_ctrl_set_bits(&ctrl, CTRL_FU_IN_LSB + i * CTRL_FU_IN_ELEM_NBITS,
                       CTRL_FU_IN_ELEM_NBITS, fu_in ? fu_in[i] : 0);
    cgra_ctrl_set_bits(
        &ctrl, CTRL_WRITE_REG_FROM_LSB + i * CTRL_WRITE_REG_FROM_ELEM_NBITS,
        CTRL_WRITE_REG_FROM_ELEM_NBITS, write_reg_from ? write_reg_from[i] : 0);
    cgra_ctrl_set_bits(
        &ctrl, CTRL_WRITE_REG_IDX_LSB + i * CTRL_WRITE_REG_IDX_ELEM_NBITS,
        CTRL_WRITE_REG_IDX_ELEM_NBITS, write_reg_idx ? write_reg_idx[i] : 0);
    cgra_ctrl_set_bits(
        &ctrl, CTRL_READ_REG_FROM_LSB + i * CTRL_READ_REG_FROM_ELEM_NBITS,
        CTRL_READ_REG_FROM_ELEM_NBITS, read_reg_from ? read_reg_from[i] : 0);
    cgra_ctrl_set_bits(
        &ctrl, CTRL_READ_REG_IDX_LSB + i * CTRL_READ_REG_IDX_ELEM_NBITS,
        CTRL_READ_REG_IDX_ELEM_NBITS, read_reg_idx ? read_reg_idx[i] : 0);
  }
  for (int i = 0; i < CTRL_ROUTING_XBAR_OUTPORT_COUNT; ++i) {
    cgra_ctrl_set_bits(
        &ctrl,
        CTRL_ROUTING_XBAR_OUTPORT_LSB +
            i * CTRL_ROUTING_XBAR_OUTPORT_ELEM_NBITS,
        CTRL_ROUTING_XBAR_OUTPORT_ELEM_NBITS,
        routing_xbar ? routing_xbar[i] : 0);
    cgra_ctrl_set_bits(
        &ctrl,
        CTRL_FU_XBAR_OUTPORT_LSB + i * CTRL_FU_XBAR_OUTPORT_ELEM_NBITS,
        CTRL_FU_XBAR_OUTPORT_ELEM_NBITS, fu_xbar ? fu_xbar[i] : 0);
  }
  return ctrl;
}

static inline void cgra_pkt_set_bit(cgra_packet_t *pkt, int bit_idx,
                                    uint64_t bit_val) {
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

static inline void cgra_pkt_set_bits(cgra_packet_t *pkt, int lsb, int width,
                                     uint64_t value) {
  for (int i = 0; i < width; ++i) {
    cgra_pkt_set_bit(pkt, lsb + i, (value >> i) & 1ULL);
  }
}

static inline cgra_packet_t cgra_build_intra_pkt(
    uint8_t src_tile, uint8_t dst_tile, uint8_t cmd, uint64_t data,
    uint8_t data_addr, cgra_ctrl_t ctrl, uint8_t ctrl_addr) {
  cgra_packet_t pkt = {0, 0, 0, 0};
  cgra_pkt_set_bits(&pkt, PKT_CTRL_ADDR_LSB, PKT_CTRL_ADDR_NBITS, ctrl_addr);
  cgra_pkt_set_bits(&pkt, PKT_CTRL_LSB, CTRL_LO_NBITS, ctrl.lo);
  cgra_pkt_set_bits(&pkt, PKT_CTRL_LSB + 64, CTRL_MID_NBITS, ctrl.mid);
  cgra_pkt_set_bits(&pkt, PKT_CTRL_LSB + 128, CTRL_HI_NBITS, ctrl.hi);
  cgra_pkt_set_bits(&pkt, PKT_DATA_ADDR_LSB, DATA_ADDR_NBITS, data_addr);
  cgra_pkt_set_bits(&pkt, PKT_DATA_LSB, PKT_DATA_NBITS, data);
  cgra_pkt_set_bits(&pkt, PKT_CMD_LSB, PKT_CMD_NBITS, cmd);
  cgra_pkt_set_bits(&pkt, PKT_VC_ID_LSB, PKT_VC_ID_NBITS, 0);
  cgra_pkt_set_bits(&pkt, PKT_OPAQUE_LSB, PKT_OPAQUE_NBITS, 0);
  cgra_pkt_set_bits(&pkt, PKT_DST_CGRA_Y_LSB, PKT_DST_CGRA_Y_NBITS, 0);
  cgra_pkt_set_bits(&pkt, PKT_DST_CGRA_X_LSB, PKT_DST_CGRA_X_NBITS, 0);
  cgra_pkt_set_bits(&pkt, PKT_SRC_CGRA_Y_LSB, PKT_SRC_CGRA_Y_NBITS, 0);
  cgra_pkt_set_bits(&pkt, PKT_SRC_CGRA_X_LSB, PKT_SRC_CGRA_X_NBITS, 0);
  cgra_pkt_set_bits(&pkt, PKT_DST_CGRA_ID_LSB, PKT_DST_CGRA_ID_NBITS, 0);
  cgra_pkt_set_bits(&pkt, PKT_SRC_CGRA_ID_LSB, PKT_SRC_CGRA_ID_NBITS, 0);
  cgra_pkt_set_bits(&pkt, PKT_DST_TILE_LSB, PKT_DST_TILE_NBITS, dst_tile);
  cgra_pkt_set_bits(&pkt, PKT_SRC_TILE_LSB, PKT_SRC_TILE_NBITS, src_tile);
  return pkt;
}

static inline void cgra_send_packet(cgra_packet_t pkt) {
  CGRA_RAW_PKT_LO(pkt.lo);
  CGRA_RAW_PKT_MID(pkt.mid);
  CGRA_RAW_PKT_HI(pkt.hi);
  CGRA_RAW_PKT_TOP(pkt.top);
}

static inline void cgra_send_packets(const cgra_packet_t *pkts, size_t count) {
  for (size_t i = 0; i < count; ++i) {
    cgra_send_packet(pkts[i]);
  }
}

static inline void cgra_send_basic(uint8_t tile, uint8_t cmd, uint32_t data,
                                   uint8_t predicate, uint8_t data_addr) {
  cgra_send_packet(cgra_build_intra_pkt(0, tile, cmd,
                                        cgra_data_raw(data, predicate),
                                        data_addr, cgra_ctrl_empty(), 0));
}

static inline void cgra_send_config(uint8_t tile, uint8_t ctrl_addr,
                                    cgra_ctrl_t ctrl) {
  cgra_send_packet(cgra_build_intra_pkt(0, tile, CGRA_CMD_CONFIG, 0, 0, ctrl,
                                        ctrl_addr));
}

static inline void cgra_send_prologue(uint8_t tile, uint8_t cmd,
                                      uint8_t ctrl_addr, uint32_t count,
                                      cgra_ctrl_t ctrl) {
  cgra_send_packet(cgra_build_intra_pkt(0, tile, cmd, cgra_data_raw(count, 1),
                                        0, ctrl, ctrl_addr));
}

static inline uint32_t cgra_read_mem(uint8_t data_addr) {
  uint64_t result = 0;
  cgra_send_basic(0, CGRA_CMD_LOAD_REQUEST, 0, 0, data_addr);
  CGRA_LOAD_RESULT(result);
  return (uint32_t)result;
}

static inline uint64_t data_raw(uint32_t payload, uint8_t predicate) {
  return cgra_data_raw(payload, predicate);
}

static inline cgra_ctrl_t build_ctrl(
    uint8_t operation, const uint8_t *fu_in, const uint8_t *routing_xbar,
    const uint8_t *fu_xbar, const uint8_t *write_reg_from,
    const uint8_t *write_reg_idx, const uint8_t *read_reg_from,
    const uint8_t *read_reg_idx) {
  return cgra_build_ctrl(operation, fu_in, routing_xbar, fu_xbar,
                         write_reg_from, write_reg_idx, read_reg_from,
                         read_reg_idx);
}

static inline cgra_packet_t build_intra_pkt(
    uint8_t src_tile, uint8_t dst_tile, uint8_t cmd, uint64_t data,
    uint8_t data_addr, cgra_ctrl_t ctrl, uint8_t ctrl_addr) {
  return cgra_build_intra_pkt(src_tile, dst_tile, cmd, data, data_addr, ctrl,
                              ctrl_addr);
}

static inline void send_packet(cgra_packet_t pkt) {
  cgra_send_packet(pkt);
}

static inline void send_basic(uint8_t tile, uint8_t cmd, uint32_t data,
                              uint8_t predicate, uint8_t data_addr) {
  cgra_send_basic(tile, cmd, data, predicate, data_addr);
}

static inline void send_config(uint8_t tile, uint8_t ctrl_addr,
                               cgra_ctrl_t ctrl) {
  cgra_send_config(tile, ctrl_addr, ctrl);
}

static inline void send_prologue(uint8_t tile, uint8_t cmd, uint8_t ctrl_addr,
                                 uint32_t count, cgra_ctrl_t ctrl) {
  cgra_send_prologue(tile, cmd, ctrl_addr, count, ctrl);
}

static inline uint32_t read_mem(uint8_t data_addr) {
  return cgra_read_mem(data_addr);
}

#endif
