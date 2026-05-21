#ifndef CGRA_MULTI_SYSTOLIC_COMMON_H
#define CGRA_MULTI_SYSTOLIC_COMMON_H

#include "cgra_protocol.h"
#include "cgra_runtime.h"

enum {
  CGRA_MULTI_SYSTOLIC_COUNT_PER_ITER = 1,
  CGRA_MULTI_SYSTOLIC_TOTAL_CTRL_COUNT = 3,
};

static inline cgra_ctrl_t cgra_multi_systolic_ctrl_ld_const_east(void) {
  const uint8_t fu_in[CTRL_FU_IN_COUNT] = {1, 2, 3, 4};
  const uint8_t routing_xbar[CTRL_ROUTING_XBAR_OUTPORT_COUNT] = {
      0, 0, 0, 0, 0, 0, 0, 0};
  const uint8_t fu_xbar[CTRL_FU_XBAR_OUTPORT_COUNT] = {0, 0, 0, 1,
                                                        0, 0, 0, 0};
  const uint8_t write_reg_from[CTRL_FU_IN_COUNT] = {0, 0, 0, 0};
  const uint8_t write_reg_idx[CTRL_FU_IN_COUNT] = {0, 0, 0, 0};
  const uint8_t read_reg_from[CTRL_FU_IN_COUNT] = {0, 0, 0, 0};
  const uint8_t read_reg_idx[CTRL_FU_IN_COUNT] = {0, 0, 0, 0};
  return build_ctrl(OPT_LD_CONST, fu_in, routing_xbar, fu_xbar,
                    write_reg_from, write_reg_idx, read_reg_from,
                    read_reg_idx);
}

static inline cgra_ctrl_t
cgra_multi_systolic_ctrl_mul_const_forward_west_east_south(void) {
  const uint8_t fu_in[CTRL_FU_IN_COUNT] = {1, 2, 3, 4};
  const uint8_t routing_xbar[CTRL_ROUTING_XBAR_OUTPORT_COUNT] = {
      0, 0, 0, PORT_WEST, PORT_WEST, 0, 0, 0};
  const uint8_t fu_xbar[CTRL_FU_XBAR_OUTPORT_COUNT] = {0, 1, 0, 0,
                                                        0, 0, 0, 0};
  const uint8_t write_reg_from[CTRL_FU_IN_COUNT] = {0, 0, 0, 0};
  const uint8_t write_reg_idx[CTRL_FU_IN_COUNT] = {0, 0, 0, 0};
  const uint8_t read_reg_from[CTRL_FU_IN_COUNT] = {0, 0, 0, 0};
  const uint8_t read_reg_idx[CTRL_FU_IN_COUNT] = {0, 0, 0, 0};
  return build_ctrl(OPT_MUL_CONST, fu_in, routing_xbar, fu_xbar,
                    write_reg_from, write_reg_idx, read_reg_from,
                    read_reg_idx);
}

static inline cgra_ctrl_t
cgra_multi_systolic_ctrl_mul_const_from_west_south(void) {
  const uint8_t fu_in[CTRL_FU_IN_COUNT] = {1, 2, 3, 4};
  const uint8_t routing_xbar[CTRL_ROUTING_XBAR_OUTPORT_COUNT] = {
      0, 0, 0, 0, PORT_WEST, 0, 0, 0};
  const uint8_t fu_xbar[CTRL_FU_XBAR_OUTPORT_COUNT] = {0, 1, 0, 0,
                                                        0, 0, 0, 0};
  const uint8_t write_reg_from[CTRL_FU_IN_COUNT] = {0, 0, 0, 0};
  const uint8_t write_reg_idx[CTRL_FU_IN_COUNT] = {0, 0, 0, 0};
  const uint8_t read_reg_from[CTRL_FU_IN_COUNT] = {0, 0, 0, 0};
  const uint8_t read_reg_idx[CTRL_FU_IN_COUNT] = {0, 0, 0, 0};
  return build_ctrl(OPT_MUL_CONST, fu_in, routing_xbar, fu_xbar,
                    write_reg_from, write_reg_idx, read_reg_from,
                    read_reg_idx);
}

static inline cgra_ctrl_t
cgra_multi_systolic_ctrl_mul_const_add_forward_west_east_south(void) {
  const uint8_t fu_in[CTRL_FU_IN_COUNT] = {1, 2, 3, 4};
  const uint8_t routing_xbar[CTRL_ROUTING_XBAR_OUTPORT_COUNT] = {
      0, 0, 0, PORT_WEST, PORT_WEST, 0, PORT_NORTH, 0};
  const uint8_t fu_xbar[CTRL_FU_XBAR_OUTPORT_COUNT] = {0, 1, 0, 0,
                                                        0, 0, 0, 0};
  const uint8_t write_reg_from[CTRL_FU_IN_COUNT] = {0, 0, 0, 0};
  const uint8_t write_reg_idx[CTRL_FU_IN_COUNT] = {0, 0, 0, 0};
  const uint8_t read_reg_from[CTRL_FU_IN_COUNT] = {0, 0, 0, 0};
  const uint8_t read_reg_idx[CTRL_FU_IN_COUNT] = {0, 0, 0, 0};
  return build_ctrl(OPT_MUL_CONST_ADD, fu_in, routing_xbar, fu_xbar,
                    write_reg_from, write_reg_idx, read_reg_from,
                    read_reg_idx);
}

static inline cgra_ctrl_t
cgra_multi_systolic_ctrl_mul_const_add_from_west_north_south(void) {
  const uint8_t fu_in[CTRL_FU_IN_COUNT] = {1, 2, 3, 4};
  const uint8_t routing_xbar[CTRL_ROUTING_XBAR_OUTPORT_COUNT] = {
      0, 0, 0, 0, PORT_WEST, 0, PORT_NORTH, 0};
  const uint8_t fu_xbar[CTRL_FU_XBAR_OUTPORT_COUNT] = {0, 1, 0, 0,
                                                        0, 0, 0, 0};
  const uint8_t write_reg_from[CTRL_FU_IN_COUNT] = {0, 0, 0, 0};
  const uint8_t write_reg_idx[CTRL_FU_IN_COUNT] = {0, 0, 0, 0};
  const uint8_t read_reg_from[CTRL_FU_IN_COUNT] = {0, 0, 0, 0};
  const uint8_t read_reg_idx[CTRL_FU_IN_COUNT] = {0, 0, 0, 0};
  return build_ctrl(OPT_MUL_CONST_ADD, fu_in, routing_xbar, fu_xbar,
                    write_reg_from, write_reg_idx, read_reg_from,
                    read_reg_idx);
}

static inline cgra_ctrl_t cgra_multi_systolic_ctrl_store_from_north(void) {
  const uint8_t fu_in[CTRL_FU_IN_COUNT] = {1, 2, 3, 4};
  const uint8_t routing_xbar[CTRL_ROUTING_XBAR_OUTPORT_COUNT] = {
      0, 0, 0, 0, PORT_NORTH, 0, 0, 0};
  const uint8_t fu_xbar[CTRL_FU_XBAR_OUTPORT_COUNT] = {0, 0, 0, 0,
                                                        0, 0, 0, 0};
  const uint8_t write_reg_from[CTRL_FU_IN_COUNT] = {0, 0, 0, 0};
  const uint8_t write_reg_idx[CTRL_FU_IN_COUNT] = {0, 0, 0, 0};
  const uint8_t read_reg_from[CTRL_FU_IN_COUNT] = {0, 0, 0, 0};
  const uint8_t read_reg_idx[CTRL_FU_IN_COUNT] = {0, 0, 0, 0};
  return build_ctrl(OPT_STR_CONST, fu_in, routing_xbar, fu_xbar,
                    write_reg_from, write_reg_idx, read_reg_from,
                    read_reg_idx);
}

static inline void cgra_multi_systolic_preload(uint32_t data_addr,
                                               uint32_t value) {
  send_basic(0, CGRA_CMD_STORE_REQUEST, value, 1, data_addr);
}

static inline void cgra_multi_systolic_send_const(cgra_target_t target,
                                                  uint8_t tile,
                                                  uint32_t value) {
  send_basic_to(target, tile, CGRA_CMD_CONST, value, 1, 0);
}

static inline void cgra_multi_systolic_send_count_per_iter(cgra_target_t target,
                                                           uint8_t tile) {
  send_basic_to(target, tile, CGRA_CMD_CONFIG_COUNT_PER_ITER,
                CGRA_MULTI_SYSTOLIC_COUNT_PER_ITER, 1, 0);
}

static inline void cgra_multi_systolic_send_total_ctrl_count(
    cgra_target_t target, uint8_t tile) {
  send_basic_to(target, tile, CGRA_CMD_CONFIG_TOTAL_CTRL_COUNT,
                CGRA_MULTI_SYSTOLIC_TOTAL_CTRL_COUNT, 1, 0);
}

static inline void cgra_multi_systolic_send_config(cgra_target_t target,
                                                   uint8_t tile,
                                                   cgra_ctrl_t ctrl) {
  send_config_to(target, tile, 0, ctrl);
}

static inline void cgra_multi_systolic_send_launch(cgra_target_t target,
                                                   uint8_t tile) {
  send_basic_to(target, tile, CGRA_CMD_LAUNCH, 0, 0, 0);
}

#endif
