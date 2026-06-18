#include "generated/openfpga_and2_or2_k4_frame_bitstream.h"
#include "generated/openfpga_and2_or2_k4_frame_pin_map.h"
#include "mmio.h"
#include <stdint.h>
#include <stdio.h>

enum {
  OPENFPGA_CONTROL = 0x00,
  OPENFPGA_STATUS = 0x08,
  OPENFPGA_CFG_WORD = 0x10,
  OPENFPGA_USER_INPUT = 0x20,
  OPENFPGA_USER_OUTPUT = 0x28,
};

static inline uintptr_t openfpga_reg(uintptr_t offset) {
  return (uintptr_t)OPENFPGA_AND2_OR2_K4_FRAME_BASE + offset;
}

static void openfpga_write(uintptr_t offset, uint32_t value) {
  reg_write32(openfpga_reg(offset), value);
}

static uint32_t openfpga_read(uintptr_t offset) {
  return reg_read32(openfpga_reg(offset));
}

int main(void) {
  printf("OpenFPGA AND2_OR2 k4 frame demo: Starting\n");

  openfpga_write(OPENFPGA_CONTROL, 1);

  for (uint32_t i = 0; i < OPENFPGA_AND2_OR2_K4_FRAME_BITSTREAM_LEN; i++) {
    openfpga_write(OPENFPGA_CFG_WORD, openfpga_and2_or2_k4_frame_cfg_word(i));
  }

  uint32_t status = openfpga_read(OPENFPGA_STATUS);
  if ((status & 0x1u) == 0) {
    printf("OpenFPGA AND2_OR2 k4 frame demo: STATUS not programmed, status=0x%08x\n",
           status);
    return 1;
  }

  for (uint32_t a = 0; a < 2; a++) {
    for (uint32_t b = 0; b < 2; b++) {
      uint32_t input = OPENFPGA_AND2_OR2_K4_FRAME_INPUT_FIELD_A_PACK(a) |
                       OPENFPGA_AND2_OR2_K4_FRAME_INPUT_FIELD_B_PACK(b);
      openfpga_write(OPENFPGA_USER_INPUT, input);

      uint32_t output = openfpga_read(OPENFPGA_USER_OUTPUT);
      uint32_t actual_c =
          OPENFPGA_AND2_OR2_K4_FRAME_OUTPUT_FIELD_C_GET(output);
      uint32_t actual_d =
          OPENFPGA_AND2_OR2_K4_FRAME_OUTPUT_FIELD_D_GET(output);
      uint32_t expected_c = a & b;
      uint32_t expected_d = a | b;

      if (actual_c != expected_c || actual_d != expected_d) {
        printf("OpenFPGA AND2_OR2 k4 frame demo: mismatch a=%u b=%u c=%u/%u d=%u/%u status=0x%08x\n",
               a, b, actual_c, expected_c, actual_d, expected_d,
               openfpga_read(OPENFPGA_STATUS));
        return 1;
      }
    }
  }

  printf("OpenFPGA AND2_OR2 k4 frame demo: PASS\n");
  return 0;
}
