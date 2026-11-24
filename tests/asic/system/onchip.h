#ifndef ASIC_SYSTEM_ONCHIP_H
#define ASIC_SYSTEM_ONCHIP_H

#include "accel_generated.h"
#include "gemmini.h"

/* Convert complete DIM-wide rows in the accumulator working half. */
static int convert(const void *source, void *destination, unsigned count, int expand) {
  if (count % DIM || count / DIM > DIM || count / DIM > ACC_ROWS / 2)
    return 1;
  const uint32_t write = UINT32_C(1) << (ADDR_LEN - 1);
  const uint32_t read = write | (expand ? UINT32_C(1) << (ADDR_LEN - 3) : 0);
  const unsigned source_bytes = expand ? sizeof(elem_t) : sizeof(acc_t);
  const unsigned destination_bytes = expand ? sizeof(acc_t) : sizeof(elem_t);
  accel_commands(GEMMINI0, {
    gemmini_extended3_config_ld(DIM * source_bytes, MVIN_SCALE_IDENTITY, expand, 2);
    gemmini_extended_config_st(DIM * destination_bytes, NO_ACTIVATION, ACC_SCALE_IDENTITY);
    gemmini_extended_mvin3(source, write, DIM, count / DIM);
    gemmini_extended_mvout(destination, read, DIM, count / DIM);
    gemmini_fence();
  });
  return 0;
}

#endif
