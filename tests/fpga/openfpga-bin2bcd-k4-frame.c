#include "generated/openfpga_bin2bcd_k4_frame_bitstream.h"
#include "generated/openfpga_bin2bcd_k4_frame_pin_map.h"
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
  return (uintptr_t)OPENFPGA_BIN2BCD_K4_FRAME_BASE + offset;
}

static void openfpga_write(uintptr_t offset, uint32_t value) {
  reg_write32(openfpga_reg(offset), value);
}

static uint32_t openfpga_read(uintptr_t offset) {
  return reg_read32(openfpga_reg(offset));
}

static void openfpga_delay(void) {
  for (volatile int i = 0; i < 16; i++) {
  }
}

static uint32_t expected_bcd(uint32_t value) {
  value &= 0xffu;
  return ((value / 100u) << 8) | (((value / 10u) % 10u) << 4) |
         (value % 10u);
}

static void log_progress(const char *message, uint32_t value) {
  printf("OpenFPGA bin2bcd k4 frame demo: %s %u\n", message, value);
  fflush(stdout);
}

static int check_case(uint32_t input) {
  openfpga_write(OPENFPGA_USER_INPUT, input & 0xffu);
  openfpga_delay();

  uint32_t actual = openfpga_read(OPENFPGA_USER_OUTPUT) & 0xfffu;
  uint32_t expected = expected_bcd(input);
  if (actual != expected) {
    printf("OpenFPGA bin2bcd k4 frame demo: mismatch input=%u bcd=0x%03x/0x%03x status=0x%08x\n",
           input & 0xffu, actual, expected, openfpga_read(OPENFPGA_STATUS));
    fflush(stdout);
    return 1;
  }
  return 0;
}

int main(void) {
  static const uint32_t vectors[] = {
      0u, 1u, 7u, 9u, 10u, 12u, 42u, 99u, 100u, 123u, 199u, 255u,
  };

  printf("OpenFPGA bin2bcd k4 frame demo: Starting\n");
  fflush(stdout);

  openfpga_write(OPENFPGA_CONTROL, 1);

  for (uint32_t i = 0; i < OPENFPGA_BIN2BCD_K4_FRAME_BITSTREAM_LEN; i++) {
    openfpga_write(OPENFPGA_CFG_WORD, openfpga_bin2bcd_k4_frame_cfg_word(i));
    if ((i & 0x03ffu) == 0x03ffu) {
      log_progress("configured words", i + 1);
    }
  }
  log_progress("configured words", OPENFPGA_BIN2BCD_K4_FRAME_BITSTREAM_LEN);

  uint32_t status = openfpga_read(OPENFPGA_STATUS);
  printf("OpenFPGA bin2bcd k4 frame demo: status=0x%08x\n", status);
  fflush(stdout);
  if ((status & 0x1u) == 0) {
    printf("OpenFPGA bin2bcd k4 frame demo: STATUS not programmed, status=0x%08x\n",
           status);
    fflush(stdout);
    return 1;
  }

  for (uint32_t i = 0; i < sizeof(vectors) / sizeof(vectors[0]); i++) {
    if (check_case(vectors[i])) {
      return 1;
    }
  }

  printf("OpenFPGA bin2bcd k4 frame demo: PASS\n");
  fflush(stdout);
  return 0;
}
