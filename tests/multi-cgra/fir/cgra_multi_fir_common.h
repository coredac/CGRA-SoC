#ifndef CGRA_MULTI_FIR_COMMON_H
#define CGRA_MULTI_FIR_COMMON_H

#include "cgra_protocol.h"
#include "cgra_runtime.h"
#include <stdint.h>

#if CTRL_FU_IN_COUNT != 4
#error "multi-CGRA FIR tests require 4 FU input ports"
#endif
#if CTRL_ROUTING_XBAR_OUTPORT_COUNT != 8
#error "multi-CGRA FIR tests require 4 tile ports plus 4 FU input routes"
#endif
#if CTRL_FU_XBAR_OUTPORT_COUNT != 8
#error "multi-CGRA FIR tests require 8 FU xbar outports"
#endif

enum {
  CGRA_MULTI_FIR_CTRL_COUNT_PER_ITER = 4,
  CGRA_MULTI_FIR_SCALAR_TOTAL_CTRL_STEPS = 132,
  CGRA_MULTI_FIR_VECTOR_TOTAL_CTRL_STEPS = 38,
  CGRA_MULTI_FIR_EXPECTED_COMPLETES = 1,
  CGRA_MULTI_FIR_EXPECTED_RESULT = 2215,
  CGRA_MULTI_FIR_SUM_INIT_VALUE = 3,
  CGRA_MULTI_FIR_INPUT_BASE_ADDRESS = 0,
  CGRA_MULTI_FIR_COEFFICIENT_BASE_ADDRESS = 2,
  CGRA_MULTI_FIR_LOOP_LOWER_BOUND = 2,
  CGRA_MULTI_FIR_LOOP_INCREMENT = 1,
  CGRA_MULTI_FIR_SCALAR_LOOP_UPPER_BOUND = 10,
  CGRA_MULTI_FIR_VECTOR_LOOP_UPPER_BOUND = 4,
};

static const uint8_t kCgraMultiFirFuIn[4] = {1, 2, 3, 4};
static const uint8_t kCgraMultiFirFuInSecondOnly[4] = {2, 0, 0, 0};
static const uint8_t kCgraMultiFirFuInSecondFirst[4] = {2, 1, 0, 0};
static const uint8_t kCgraMultiFirWriteFu0[4] = {2, 0, 0, 0};
static const uint8_t kCgraMultiFirWriteFu1[4] = {0, 2, 0, 0};
static const uint8_t kCgraMultiFirReadReg0[4] = {1, 0, 0, 0};
static const uint8_t kCgraMultiFirReadReg1[4] = {0, 1, 0, 0};

static inline cgra_ctrl_t cgra_multi_fir_ctrl_nah(void) {
  return build_ctrl(OPT_NAH, kCgraMultiFirFuIn, 0, 0, 0, 0, 0, 0);
}

static inline cgra_ctrl_t cgra_multi_fir_ctrl_prologue_routing(uint8_t port) {
  const uint8_t routing[8] = {port, 0, 0, 0, 0, 0, 0, 0};
  return build_ctrl(0, 0, routing, 0, 0, 0, 0, 0);
}

static inline cgra_ctrl_t cgra_multi_fir_ctrl_reduce(uint8_t operation) {
  const uint8_t routing[8] = {0, 0, 0, 0, PORT_NORTH, 0, 0, 0};
  const uint8_t fu_xbar[8] = {0, 0, 0, 1, 1, 0, 0, 0};
  return build_ctrl(operation, kCgraMultiFirFuIn, routing, fu_xbar,
                    kCgraMultiFirWriteFu0, 0, kCgraMultiFirReadReg1, 0);
}

static inline cgra_ctrl_t cgra_multi_fir_ctrl_sum_phi(void) {
  const uint8_t fu_xbar[8] = {0, 0, 0, 0, 0, 1, 0, 0};
  return build_ctrl(OPT_PHI_CONST, kCgraMultiFirFuIn, 0, fu_xbar,
                    kCgraMultiFirWriteFu1, 0, kCgraMultiFirReadReg0, 0);
}

static inline cgra_ctrl_t cgra_multi_fir_ctrl_ret_gate(void) {
  const uint8_t routing[8] = {0, 0, 0, 0, PORT_WEST, PORT_NORTH, 0, 0};
  const uint8_t fu_xbar[8] = {0, 0, 0, 0, 1, 0, 0, 0};
  return build_ctrl(OPT_GRT_PRED, kCgraMultiFirFuIn, routing, fu_xbar,
                    kCgraMultiFirWriteFu0, 0, 0, 0);
}

static inline cgra_ctrl_t cgra_multi_fir_ctrl_ret(void) {
  return build_ctrl(OPT_RET, kCgraMultiFirFuIn, 0, 0, 0, 0,
                    kCgraMultiFirReadReg0, 0);
}

static inline cgra_ctrl_t cgra_multi_fir_ctrl_addr_add(void) {
  const uint8_t routing[8] = {0, 0, 0, 0, PORT_NORTH, 0, 0, 0};
  const uint8_t fu_xbar[8] = {0, 0, 0, 0, 1, 0, 0, 0};
  return build_ctrl(OPT_ADD_CONST, kCgraMultiFirFuIn, routing, fu_xbar,
                    kCgraMultiFirWriteFu0, 0, 0, 0);
}

static inline cgra_ctrl_t cgra_multi_fir_ctrl_load_addr_to_reg1(void) {
  const uint8_t fu_xbar[8] = {0, 0, 0, 0, 0, 1, 0, 0};
  return build_ctrl(OPT_LD, kCgraMultiFirFuIn, 0, fu_xbar,
                    kCgraMultiFirWriteFu1, 0, kCgraMultiFirReadReg0, 0);
}

static inline cgra_ctrl_t cgra_multi_fir_ctrl_mul(uint8_t operation) {
  const uint8_t routing[8] = {0, 0, 0, 0, PORT_NORTH, 0, 0, 0};
  const uint8_t fu_xbar[8] = {0, 1, 0, 0, 0, 0, 0, 0};
  return build_ctrl(operation, kCgraMultiFirFuIn, routing, fu_xbar, 0, 0,
                    kCgraMultiFirReadReg1, 0);
}

static inline cgra_ctrl_t cgra_multi_fir_ctrl_cmp(void) {
  const uint8_t routing[8] = {0, 0, 0, 0, PORT_NORTH, 0, 0, 0};
  const uint8_t fu_xbar[8] = {1, 0, 0, 0, 1, 0, 0, 0};
  return build_ctrl(OPT_NE_CONST, kCgraMultiFirFuIn, routing, fu_xbar,
                    kCgraMultiFirWriteFu0, 0, 0, 0);
}

static inline cgra_ctrl_t cgra_multi_fir_ctrl_not(void) {
  const uint8_t fu_xbar[8] = {0, 1, 0, 0, 0, 0, 0, 0};
  return build_ctrl(OPT_NOT, kCgraMultiFirFuIn, 0, fu_xbar, 0, 0,
                    kCgraMultiFirReadReg0, 0);
}

static inline cgra_ctrl_t cgra_multi_fir_ctrl_loop_phi(void) {
  const uint8_t routing[8] = {0, 0, 0, 0, PORT_EAST, 0, 0, 0};
  const uint8_t fu_xbar[8] = {0, 1, 0, 1, 1, 0, 0, 0};
  return build_ctrl(OPT_PHI_CONST, kCgraMultiFirFuIn, routing, fu_xbar,
                    kCgraMultiFirWriteFu0, 0, 0, 0);
}

static inline cgra_ctrl_t cgra_multi_fir_ctrl_loop_add(void) {
  const uint8_t fu_xbar[8] = {0, 0, 0, 0, 0, 1, 0, 0};
  return build_ctrl(OPT_ADD_CONST, kCgraMultiFirFuIn, 0, fu_xbar,
                    kCgraMultiFirWriteFu1, 0, kCgraMultiFirReadReg0, 0);
}

static inline cgra_ctrl_t cgra_multi_fir_ctrl_loop_load(void) {
  const uint8_t fu_xbar[8] = {0, 1, 0, 0, 0, 0, 0, 0};
  return build_ctrl(OPT_LD, kCgraMultiFirFuInSecondOnly, 0, fu_xbar, 0, 0,
                    kCgraMultiFirReadReg1, 0);
}

static inline cgra_ctrl_t cgra_multi_fir_ctrl_loop_inc(void) {
  const uint8_t routing[8] = {0, 0, 0, 0, PORT_WEST, 0, 0, 0};
  const uint8_t fu_xbar[8] = {0, 1, 0, 0, 0, 1, 0, 0};
  return build_ctrl(OPT_ADD_CONST, kCgraMultiFirFuIn, routing, fu_xbar,
                    kCgraMultiFirWriteFu1, 0, 0, 0);
}

static inline cgra_ctrl_t cgra_multi_fir_ctrl_loop_grant(void) {
  const uint8_t routing[8] = {0, 0, 0, 0, PORT_SOUTH, 0, 0, 0};
  const uint8_t fu_xbar[8] = {0, 0, 1, 0, 0, 0, 0, 0};
  return build_ctrl(OPT_GRT_PRED, kCgraMultiFirFuInSecondFirst, routing,
                    fu_xbar, 0, 0, kCgraMultiFirReadReg1, 0);
}

static inline void cgra_multi_fir_send_counts(cgra_target_t target,
                                              uint8_t tile,
                                              uint32_t total_ctrl_steps) {
  send_basic_to(target, tile, CGRA_CMD_CONFIG_COUNT_PER_ITER,
                CGRA_MULTI_FIR_CTRL_COUNT_PER_ITER, 1, 0);
  send_basic_to(target, tile, CGRA_CMD_CONFIG_TOTAL_CTRL_COUNT,
                total_ctrl_steps, 1, 0);
}

static inline void cgra_multi_fir_send_launch(cgra_target_t target,
                                              uint8_t tile) {
  send_basic_to(target, tile, CGRA_CMD_LAUNCH, 0, 0, 0);
}

static inline void cgra_multi_fir_preload_scalar(void) {
  for (uint8_t addr = 0; addr < 16; ++addr) {
    send_basic(0, CGRA_CMD_STORE_REQUEST, (uint64_t)(10 + addr), 1, addr);
  }
}

static inline void cgra_multi_fir_preload_vector(void) {
  const uint64_t data[] = {
      UINT64_C(0x0001000100010001),
      UINT64_C(0x0001000100010001),
      UINT64_C(0x000f000e000d000c),
      UINT64_C(0x0013001200110010),
      UINT64_C(0x00110010000f000e),
      UINT64_C(0x0015001400130012),
      UINT64_C(0x0001000100010001),
  };

  for (uint8_t addr = 0; addr < (uint8_t)(sizeof(data) / sizeof(data[0]));
       ++addr) {
    send_basic(0, CGRA_CMD_STORE_REQUEST, data[addr], 1, addr);
  }
}

static inline void cgra_multi_fir_config_accum_tile(cgra_target_t target,
                                                    uint8_t tile,
                                                    uint8_t reduce_op,
                                                    uint32_t total_steps) {
  send_basic_to(target, tile, CGRA_CMD_CONST, CGRA_MULTI_FIR_SUM_INIT_VALUE, 1,
                0);
  cgra_multi_fir_send_counts(target, tile, total_steps);
  send_config_to(target, tile, 0, cgra_multi_fir_ctrl_reduce(reduce_op));
  send_config_to(target, tile, 1, cgra_multi_fir_ctrl_sum_phi());
  send_config_to(target, tile, 2, cgra_multi_fir_ctrl_nah());
  send_config_to(target, tile, 3, cgra_multi_fir_ctrl_nah());
  send_prologue_to(target, tile, CGRA_CMD_CONFIG_PROLOGUE_FU, 0, 1,
                   cgra_ctrl_empty());
  send_prologue_to(target, tile, CGRA_CMD_CONFIG_PROLOGUE_ROUTING_CROSSBAR, 0,
                   1, cgra_multi_fir_ctrl_prologue_routing(PORT_NORTH));
  send_prologue_to(target, tile, CGRA_CMD_CONFIG_PROLOGUE_FU_CROSSBAR, 0, 1,
                   cgra_ctrl_empty());
  cgra_multi_fir_send_launch(target, tile);
}

static inline void cgra_multi_fir_config_return_tile(cgra_target_t target,
                                                    uint8_t tile,
                                                    uint32_t total_steps) {
  cgra_multi_fir_send_counts(target, tile, total_steps);
  send_config_to(target, tile, 0, cgra_multi_fir_ctrl_nah());
  send_config_to(target, tile, 1, cgra_multi_fir_ctrl_ret_gate());
  send_config_to(target, tile, 2, cgra_multi_fir_ctrl_ret());
  send_config_to(target, tile, 3, cgra_multi_fir_ctrl_nah());
  send_prologue_to(target, tile, CGRA_CMD_CONFIG_PROLOGUE_FU, 1, 1,
                   cgra_ctrl_empty());
  send_prologue_to(target, tile, CGRA_CMD_CONFIG_PROLOGUE_FU, 2, 1,
                   cgra_ctrl_empty());
  send_prologue_to(target, tile, CGRA_CMD_CONFIG_PROLOGUE_ROUTING_CROSSBAR, 1,
                   1, cgra_multi_fir_ctrl_prologue_routing(PORT_NORTH));
  send_prologue_to(target, tile, CGRA_CMD_CONFIG_PROLOGUE_ROUTING_CROSSBAR, 1,
                   1, cgra_multi_fir_ctrl_prologue_routing(PORT_WEST));
  send_prologue_to(target, tile, CGRA_CMD_CONFIG_PROLOGUE_FU_CROSSBAR, 1, 1,
                   cgra_ctrl_empty());
  cgra_multi_fir_send_launch(target, tile);
}

static inline void cgra_multi_fir_config_mul_tile(cgra_target_t target,
                                                 uint8_t tile, uint8_t mul_op,
                                                 uint32_t total_steps) {
  send_basic_to(target, tile, CGRA_CMD_CONST,
                CGRA_MULTI_FIR_COEFFICIENT_BASE_ADDRESS, 1, 0);
  cgra_multi_fir_send_counts(target, tile, total_steps);
  send_config_to(target, tile, 0, cgra_multi_fir_ctrl_nah());
  send_config_to(target, tile, 1, cgra_multi_fir_ctrl_addr_add());
  send_config_to(target, tile, 2, cgra_multi_fir_ctrl_load_addr_to_reg1());
  send_config_to(target, tile, 3, cgra_multi_fir_ctrl_mul(mul_op));
  cgra_multi_fir_send_launch(target, tile);
}

static inline void cgra_multi_fir_config_cmp_tile(cgra_target_t target,
                                                 uint8_t tile,
                                                 uint32_t loop_upper_bound,
                                                 uint32_t total_steps) {
  send_basic_to(target, tile, CGRA_CMD_CONST, loop_upper_bound, 1, 0);
  cgra_multi_fir_send_counts(target, tile, total_steps);
  send_config_to(target, tile, 0, cgra_multi_fir_ctrl_nah());
  send_config_to(target, tile, 1, cgra_multi_fir_ctrl_nah());
  send_config_to(target, tile, 2, cgra_multi_fir_ctrl_cmp());
  send_config_to(target, tile, 3, cgra_multi_fir_ctrl_not());
  cgra_multi_fir_send_launch(target, tile);
}

static inline void cgra_multi_fir_config_loop_load_tile(cgra_target_t target,
                                                       uint8_t tile,
                                                       uint32_t total_steps) {
  send_basic_to(target, tile, CGRA_CMD_CONST, CGRA_MULTI_FIR_LOOP_LOWER_BOUND,
                1, 0);
  send_basic_to(target, tile, CGRA_CMD_CONST,
                CGRA_MULTI_FIR_INPUT_BASE_ADDRESS, 1, 0);
  cgra_multi_fir_send_counts(target, tile, total_steps);
  send_config_to(target, tile, 0, cgra_multi_fir_ctrl_loop_phi());
  send_config_to(target, tile, 1, cgra_multi_fir_ctrl_loop_add());
  send_config_to(target, tile, 2, cgra_multi_fir_ctrl_loop_load());
  send_config_to(target, tile, 3, cgra_multi_fir_ctrl_nah());
  send_prologue_to(target, tile, CGRA_CMD_CONFIG_PROLOGUE_ROUTING_CROSSBAR, 0,
                   1, cgra_multi_fir_ctrl_prologue_routing(PORT_EAST));
  cgra_multi_fir_send_launch(target, tile);
}

static inline void cgra_multi_fir_config_loop_update_tile(
    cgra_target_t target, uint8_t tile, uint32_t total_steps) {
  send_basic_to(target, tile, CGRA_CMD_CONST, CGRA_MULTI_FIR_LOOP_INCREMENT, 1,
                0);
  cgra_multi_fir_send_counts(target, tile, total_steps);
  send_config_to(target, tile, 0, cgra_multi_fir_ctrl_nah());
  send_config_to(target, tile, 1, cgra_multi_fir_ctrl_loop_inc());
  send_config_to(target, tile, 2, cgra_multi_fir_ctrl_nah());
  send_config_to(target, tile, 3, cgra_multi_fir_ctrl_loop_grant());
  cgra_multi_fir_send_launch(target, tile);
}

static inline void cgra_multi_fir_configure_4x4(uint8_t reduce_op,
                                                uint8_t mul_op,
                                                uint32_t loop_upper_bound,
                                                uint32_t total_steps) {
  const cgra_target_t target = cgra_target_local();

  cgra_multi_fir_config_accum_tile(target, 0, reduce_op, total_steps);
  cgra_multi_fir_config_return_tile(target, 1, total_steps);
  cgra_multi_fir_config_mul_tile(target, 4, mul_op, total_steps);
  cgra_multi_fir_config_cmp_tile(target, 5, loop_upper_bound, total_steps);
  cgra_multi_fir_config_loop_load_tile(target, 8, total_steps);
  cgra_multi_fir_config_loop_update_tile(target, 9, total_steps);
}

static inline void cgra_multi_fir_configure_scalar_2x2_2x2(
    cgra_target_t target_cgra0, cgra_target_t target_cgra2,
    uint32_t total_steps) {
  cgra_multi_fir_config_accum_tile(target_cgra0, 0, OPT_ADD, total_steps);
  cgra_multi_fir_config_return_tile(target_cgra0, 1, total_steps);
  cgra_multi_fir_config_mul_tile(target_cgra0, 2, OPT_MUL, total_steps);
  cgra_multi_fir_config_cmp_tile(target_cgra0, 3,
                                 CGRA_MULTI_FIR_SCALAR_LOOP_UPPER_BOUND,
                                 total_steps);
  cgra_multi_fir_config_loop_load_tile(target_cgra2, 0, total_steps);
  cgra_multi_fir_config_loop_update_tile(target_cgra2, 1, total_steps);
}

#endif
