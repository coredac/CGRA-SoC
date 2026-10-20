#include "block_spm.h"
#include "generated/gpt2_data.h"

#include <stdint.h>
#include <stdio.h>

// Exercise both publication column blocks and dense row strides on each endpoint.
static int run(accel_t device) {
  enum { ROWS = 16, WIDTH = 128, WORDS = ROWS * WIDTH };
  elem_t *input = (elem_t *)(device.spm + 64 * 1024);
  elem_t *output = (elem_t *)(device.spm + 72 * 1024);
  elem_t *copy = (elem_t *)(device.spm + 80 * 1024);
  accel_commands(device, { gemmini_flush(0); });
  gemmini_fence();
  if (block_load(block_expected_ln1[0], 0, WORDS, 1) || block_store(input, 0, WORDS) || block_project_spm(device, input, block_wq, block_bq, output, ROWS, WIDTH, WIDTH, BLOCK_Q_DIV)) {
    return 1;
  }
  // Reconsume the packed CGRA output through this Gemmini's local DMA route.
  accel_commands(device, {
    gemmini_config_ld(WIDTH);
    gemmini_config_st(WIDTH);
    for (unsigned column = 0; column < WIDTH; column += 64) {
      gemmini_extended_mvin(output + column, 0, 64, ROWS);
      gemmini_fence();
      for (unsigned block = 0; block < 64; block += DIM) {
        gemmini_extended_mvout(copy + column + block, block, DIM, ROWS);
      }
      gemmini_fence();
    }
  });
  unsigned errors = 0;
  for (unsigned index = 0; index < WORDS; ++index) {
    const int8_t actual = ((volatile elem_t *)copy)[index];
    if (actual != block_expected_q[0][index]) {
      if (errors < 8) {
        printf("SPM device=%u index=%u actual=%d expected=%d\n", device.id, index, actual, block_expected_q[0][index]);
      }
      ++errors;
    }
  }
  printf("SPM device=%u outputs=%u mismatches=%u\n", device.id, WORDS, errors);
  return errors != 0;
}

int main(void) {
  if (BLOCK_HIDDEN != 128 || run(GEMMINI0) || run(GEMMINI1)) {
    printf("SPM: FAIL\n");
    return 1;
  }
  printf("SPM: PASS\n");
  return 0;
}
