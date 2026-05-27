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
  uint64_t top;
} cgra_packet_t;

#if CGRA_INTRA_PKT_NBITS > 256
#error "cgra runtime packet send supports up to four 64-bit chunks"
#endif

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

static inline void cgra_send_packet_fast(cgra_packet_t pkt) {
  CGRA_RAW_PKT_LO(pkt.lo);
  CGRA_RAW_PKT_MID(pkt.mid);
  CGRA_RAW_PKT_HI(pkt.hi);
#if CGRA_INTRA_PKT_NBITS > 192
  CGRA_RAW_PKT_TOP(pkt.top);
#endif
}

static inline void cgra_send_packets_fast(const cgra_packet_t *pkts,
                                          size_t count) {
  for (size_t i = 0; i < count; ++i) {
    cgra_send_packet_fast(pkts[i]);
  }
}

#endif
