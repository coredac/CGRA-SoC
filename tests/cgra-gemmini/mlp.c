#include "accel_generated.h"
#include "block_gemm.h"
#include "generated/mlp_data.h"

#include <stdint.h>
#include <stdio.h>

static acc_t output[MLP_ROWS * MLP_COLS] __attribute__((aligned(64))) = {1};

static unsigned check(const mlp_case_t *test) {
  unsigned errors = 0;
  int64_t maximum = 0;
  for (unsigned index = 0; index < MLP_ROWS * MLP_COLS; ++index) {
    const int64_t expected = test->expected[index];
    if (output[index] != expected) {
      if (errors < 8) {
        printf("MLP mismatch row=%u col=%u expected=%ld actual=%ld\n", index / MLP_COLS, index % MLP_COLS, (long)expected, (long)output[index]);
      }
      ++errors;
    }
    const int64_t magnitude = expected < 0 ? -expected : expected;
    if (magnitude > maximum) {
      maximum = magnitude;
    }
  }
  printf("MLP trial=%u outputs=%u mismatches=%u max_abs=%ld\n", test->trial, MLP_ROWS * MLP_COLS, errors, (long)maximum);
  return errors;
}

static unsigned run(accel_t device, const mlp_case_t *test) {
  for (unsigned index = 0; index < MLP_ROWS * MLP_COLS; ++index) {
    output[index] = INT32_MIN;
  }
  printf("MLP gemmini=%u K=%u chunks=%u trial=%u extreme=%u begin\n", device.id, test->width, test->width / MLP_CHUNK, test->trial, test->extreme);
  block_matmul(device, test->input, test->weight, test->bias, output, MLP_ROWS, test->width, MLP_COLS);
  return check(test);
}

int main(void) {
  accel_commands(GEMMINI0, { gemmini_flush(0); });
  accel_commands(GEMMINI1, { gemmini_flush(0); });
  gemmini_fence();

  printf("MLP working data: A=%u B=%u bytes accumulator=%u bytes\n", MLP_ROWS * MLP_CHUNK, MLP_CHUNK * MLP_COLS, MLP_ROWS * MLP_COLS * (unsigned)sizeof(acc_t));
  unsigned errors = 0;
  const accel_t devices[] = {GEMMINI0, GEMMINI1};
  for (unsigned index = 0; index < sizeof(mlp_cases) / sizeof(mlp_cases[0]); ++index) {
    errors += run(devices[index / 3], &mlp_cases[index]);
  }
  printf("MLP: %s\n", errors ? "FAIL" : "PASS");
  return errors != 0;
}
