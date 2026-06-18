#include <stdint.h>
#include <stdio.h>

#include "generated/openfpga_gcd6_k4_frame_bitstream.h"
#include "generated/openfpga_gcd6_k4_frame_pin_map.h"
#include "mmio.h"

enum {
  OPENFPGA_CONTROL = 0x00,
  OPENFPGA_STATUS = 0x08,
  OPENFPGA_CFG_WORD = 0x10,
  OPENFPGA_USER_INPUT = 0x20,
  OPENFPGA_USER_OUTPUT = 0x28,
};

#define STATUS_PROGRAMMED 0x1u

static inline uintptr_t openfpga_reg(uintptr_t offset) {
  return (uintptr_t)OPENFPGA_GCD6_K4_FRAME_BASE + offset;
}

static void openfpga_write(uintptr_t offset, uint32_t value) {
  reg_write32(openfpga_reg(offset), value);
}

static uint32_t openfpga_read(uintptr_t offset) {
  return reg_read32(openfpga_reg(offset));
}

static uint8_t gcd_ref(uint8_t a, uint8_t b) {
  while (b != 0u) {
    uint8_t t = (uint8_t)(a % b);
    a = b;
    b = t;
  }
  return a;
}

static uint32_t pack_input(uint8_t reset, uint8_t start, uint8_t a, uint8_t b) {
  return OPENFPGA_GCD6_K4_FRAME_INPUT_FIELD_RESET_PACK(reset) |
         OPENFPGA_GCD6_K4_FRAME_INPUT_FIELD_START_PACK(start) |
         OPENFPGA_GCD6_K4_FRAME_INPUT_FIELD_A_0_PACK((a >> 0) & 1u) |
         OPENFPGA_GCD6_K4_FRAME_INPUT_FIELD_A_1_PACK((a >> 1) & 1u) |
         OPENFPGA_GCD6_K4_FRAME_INPUT_FIELD_A_2_PACK((a >> 2) & 1u) |
         OPENFPGA_GCD6_K4_FRAME_INPUT_FIELD_A_3_PACK((a >> 3) & 1u) |
         OPENFPGA_GCD6_K4_FRAME_INPUT_FIELD_A_4_PACK((a >> 4) & 1u) |
         OPENFPGA_GCD6_K4_FRAME_INPUT_FIELD_A_5_PACK((a >> 5) & 1u) |
         OPENFPGA_GCD6_K4_FRAME_INPUT_FIELD_B_0_PACK((b >> 0) & 1u) |
         OPENFPGA_GCD6_K4_FRAME_INPUT_FIELD_B_1_PACK((b >> 1) & 1u) |
         OPENFPGA_GCD6_K4_FRAME_INPUT_FIELD_B_2_PACK((b >> 2) & 1u) |
         OPENFPGA_GCD6_K4_FRAME_INPUT_FIELD_B_3_PACK((b >> 3) & 1u) |
         OPENFPGA_GCD6_K4_FRAME_INPUT_FIELD_B_4_PACK((b >> 4) & 1u) |
         OPENFPGA_GCD6_K4_FRAME_INPUT_FIELD_B_5_PACK((b >> 5) & 1u);
}

static void set_input(uint32_t base, uint8_t reset, uint8_t start, uint8_t a, uint8_t b) {
  (void)base;
  openfpga_write(OPENFPGA_USER_INPUT, pack_input(reset, start, a, b));
}

static uint32_t unpack_result(uint32_t output) {
  return (OPENFPGA_GCD6_K4_FRAME_OUTPUT_FIELD_RESULT_0_GET(output) << 0) |
         (OPENFPGA_GCD6_K4_FRAME_OUTPUT_FIELD_RESULT_1_GET(output) << 1) |
         (OPENFPGA_GCD6_K4_FRAME_OUTPUT_FIELD_RESULT_2_GET(output) << 2) |
         (OPENFPGA_GCD6_K4_FRAME_OUTPUT_FIELD_RESULT_3_GET(output) << 3) |
         (OPENFPGA_GCD6_K4_FRAME_OUTPUT_FIELD_RESULT_4_GET(output) << 4) |
         (OPENFPGA_GCD6_K4_FRAME_OUTPUT_FIELD_RESULT_5_GET(output) << 5);
}

static int program_bitstream(uint32_t base) {
  (void)base;
  printf("Programming %u OpenFPGA cfg words...\n", (unsigned)OPENFPGA_GCD6_K4_FRAME_BITSTREAM_LEN);
  openfpga_write(OPENFPGA_CONTROL, 1u);
  for (uint32_t i = 0; i < OPENFPGA_GCD6_K4_FRAME_BITSTREAM_LEN; ++i) {
    uint32_t word = openfpga_gcd6_k4_frame_cfg_word(i);
    openfpga_write(OPENFPGA_CFG_WORD, word);
    if ((i & 0x3ffu) == 0u) {
      printf("  cfg[%u] = 0x%08x\n", (unsigned)i, (unsigned)word);
    }
  }
  uint32_t status = openfpga_read(OPENFPGA_STATUS);
  if ((status & STATUS_PROGRAMMED) == 0u) {
    printf("ERROR: programmed status bit not set, status=0x%08x\n", (unsigned)status);
    return 1;
  }
  printf("Programming done, status=0x%08x\n", (unsigned)status);
  return 0;
}

static int run_case(uint32_t base, uint8_t a, uint8_t b) {
  uint8_t expected = gcd_ref(a, b);

  set_input(base, 0u, 0u, a, b);
  set_input(base, 0u, 1u, a, b);
  set_input(base, 0u, 0u, a, b);

  for (uint32_t attempt = 0; attempt < 512u; ++attempt) {
    uint32_t output = openfpga_read(OPENFPGA_USER_OUTPUT);
    uint32_t done = OPENFPGA_GCD6_K4_FRAME_OUTPUT_FIELD_DONE_GET(output);
    uint32_t result = unpack_result(output);
    if (done != 0u) {
      if (result != expected) {
        printf(
            "ERROR: gcd(%u,%u) got %u, expected %u, raw=0x%08x\n",
            (unsigned)a,
            (unsigned)b,
            (unsigned)result,
            (unsigned)expected,
            (unsigned)output);
        return 1;
      }
      printf("gcd(%u,%u) = %u\n", (unsigned)a, (unsigned)b, (unsigned)result);
      return 0;
    }
  }

  printf("ERROR: gcd(%u,%u) did not assert done\n", (unsigned)a, (unsigned)b);
  return 1;
}

int main(void) {
  uint32_t base = OPENFPGA_GCD6_K4_FRAME_BASE;

  if (program_bitstream(base) != 0) {
    return 1;
  }

  set_input(base, 1u, 0u, 0u, 0u);
  for (volatile uint32_t i = 0; i < 16u; ++i) {
  }
  set_input(base, 0u, 0u, 0u, 0u);

  static const uint8_t cases[][2] = {
      {0u, 0u},
      {12u, 8u},
      {15u, 15u},
      {21u, 6u},
      {27u, 18u},
      {35u, 14u},
      {63u, 21u},
      {62u, 45u},
  };

  for (uint32_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
    if (run_case(base, cases[i][0], cases[i][1]) != 0) {
      return 1;
    }
  }

  printf("OpenFPGA gcd6 k4 frame demo PASS\n");
  return 0;
}
