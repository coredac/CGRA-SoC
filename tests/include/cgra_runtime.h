#ifndef CGRA_RUNTIME_H
#define CGRA_RUNTIME_H

#include "cgra_layout.h"
#include "cgra_protocol.h"
#include "cgra_types.h"
#include <stddef.h>
#include <stdint.h>

#if CGRA_INTRA_PKT_NBITS > 256
#error "cgra runtime packet send supports up to four 64-bit chunks"
#endif

static inline void cgra_send_packet_fast(cgra_packet_t pkt) {
  CGRA_RAW_PKT_LO(pkt.lo);
  CGRA_RAW_PKT_MID(pkt.mid);
  CGRA_RAW_PKT_HI(pkt.hi);
#if CGRA_INTRA_PKT_NBITS > 192
  CGRA_RAW_PKT_TOP(pkt.top);
#endif
}

static inline void cgra_send_packets_fast(const cgra_packet_t *pkts, size_t count) {
  for (size_t i = 0; i < count; ++i) {
    cgra_send_packet_fast(pkts[i]);
  }
}

// COLD is the first physical CGRA run after reset; REPEAT reuses fixed setup.
static inline void cgra_prepare(const cgra_kernel_t *kernel, cgra_run_t mode) {
  const cgra_packet_t *packets = kernel->config_packets;
  if (mode != CGRA_COLD) {
    cgra_send_packets_fast(packets, kernel->rearm_count);
  }
  packets += kernel->rearm_count;
  if (mode != CGRA_REPEAT) {
    cgra_send_packets_fast(packets, kernel->setup_count);
  }
  packets += kernel->setup_count;
  cgra_send_packets_fast(packets, kernel->config_count - kernel->rearm_count - kernel->setup_count);
}

static inline void cgra_config(const cgra_kernel_t *kernel, cgra_run_t mode) {
  cgra_send_packets_fast(kernel->static_packets, kernel->static_count);
  cgra_prepare(kernel, mode);
}

// The caller waits for input DMA completion before launching manual execution.
static inline void cgra_start(const cgra_kernel_t *kernel) {
  CGRA_SET_EXPECTED_COMPLETES(kernel->expected_completes);
  cgra_send_packets_fast(kernel->launch_packets, kernel->launch_count);
}

#endif
