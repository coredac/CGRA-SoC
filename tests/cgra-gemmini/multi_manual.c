#include "accel_generated.h"
#include "cgra_dma.h"
#include "cgra_protocol.h"
#include "cgra_runtime.h"
#include "gemmini.h"
#include "generated/cgra_relu4x4_fast_api.h"
#include "pool.h"

#include <stdint.h>
#include <stdio.h>

enum {
  INSTANCES = 2,
  TILES = 8,
  WORDS = 32,
  BYTES = WORDS * sizeof(acc_t),
  ROW_BYTES = DIM * sizeof(elem_t),
  ACC_ROW_BYTES = DIM * sizeof(acc_t),
  ACC_ROW_STRIDE = sizeof(acc_t) / sizeof(elem_t),
  PUBLICATION_ROWS = BYTES / ACC_ROW_BYTES,
  A_ROW = 0,
  B_ROW = DIM,
  POOL_HEIGHT = 2,
  POOL_WIDTH = 2,
  CHANNELS = WORDS / (POOL_HEIGHT * POOL_WIDTH),
  TAG_BASE = 0x20,
};

static elem_t A[TILES][PUBLICATION_ROWS][DIM] row_align(1);
static elem_t B[INSTANCES][DIM][DIM] row_align(1);
static acc_t gemmini_expected[TILES][INSTANCES][WORDS];
static acc_t relu_expected[TILES][INSTANCES][WORDS];
static acc_t output[TILES][CHANNELS] __attribute__((aligned(32)));

static const uint32_t ACC_WRITE = (uint32_t)1 << (ADDR_LEN - 1);
static const uint32_t ACC_READ = ((uint32_t)1 << (ADDR_LEN - 1)) | ((uint32_t)1 << (ADDR_LEN - 3));
static const cgra_dma_desc_t INPUT[INSTANCES] = {
    CGRA_DMA_DESC_CONST(0, BYTES, TAG_BASE),
    CGRA_DMA_DESC_CONST(0, BYTES, TAG_BASE + 1),
};

static void init_inputs(void) {
  for (unsigned tile = 0; tile < TILES; ++tile) {
    for (unsigned row = 0; row < PUBLICATION_ROWS; ++row) {
      for (unsigned column = 0; column < DIM; ++column) {
        A[tile][row][column] = (elem_t)((int)((tile * 5 + row * DIM + column) % WORDS) - WORDS / 2);
      }
    }
    for (unsigned channel = 0; channel < CHANNELS; ++channel) {
      output[tile][channel] = (acc_t)0x5a5a5a5a;
    }
  }
  for (unsigned row = 0; row < DIM; ++row) {
    for (unsigned column = 0; column < DIM; ++column) {
      B[0][row][column] = row == column ? (elem_t)1 : (elem_t)0;
      B[1][row][column] = row == column ? (elem_t)(column % 2 == 0 ? 2 : -1) : (elem_t)0;
    }
  }
}

static void init_expected(void) {
  for (unsigned tile = 0; tile < TILES; ++tile) {
    for (unsigned instance = 0; instance < INSTANCES; ++instance) {
      for (unsigned row = 0; row < PUBLICATION_ROWS; ++row) {
        for (unsigned column = 0; column < DIM; ++column) {
          acc_t sum = 0;
          for (unsigned input = 0; input < DIM; ++input) {
            const elem_t weight = B[instance][input][column];
            if (weight == 0) {
              continue;
            }
            const acc_t value = instance == 0 ? A[tile][row][input] : relu_expected[tile][instance - 1][row * DIM + input];
            sum += value * weight;
          }
          gemmini_expected[tile][instance][row * DIM + column] = sum;
          relu_expected[tile][instance][row * DIM + column] = sum > 0 ? sum : 0;
        }
      }
    }
  }
}

static void load_gemmini(accel_t device, unsigned instance) {
  accel_commands(device, {
    gemmini_flush(0);
    gemmini_config_ld(ROW_BYTES);
    gemmini_config_ex(WEIGHT_STATIONARY, NO_ACTIVATION, 0);
    gemmini_config_st(ACC_ROW_BYTES);
    gemmini_mvin(B[instance], B_ROW);
  });
}

static void start_gemmini(accel_t device, uint32_t input_row) {
  const uint32_t publication = device.spm_bytes / ROW_BYTES - PUBLICATION_ROWS * ACC_ROW_STRIDE;
  accel_commands(device, {
    gemmini_extended_preload(B_ROW, ACC_WRITE, DIM, DIM, DIM, PUBLICATION_ROWS);
    gemmini_extended_compute_preloaded(input_row, GARBAGE_ADDR, DIM, PUBLICATION_ROWS, DIM, PUBLICATION_ROWS);
    gemmini_extended_mvout_spad(publication, ACC_ROW_STRIDE, ACC_READ, DIM, PUBLICATION_ROWS);
  });
}

static int verify_gemmini(accel_t device, unsigned instance, unsigned tile) {
  const volatile acc_t *actual = (const volatile acc_t *)(device.spm + device.spm_bytes - BYTES);
  int failures = 0;
  for (unsigned index = 0; index < WORDS; ++index) {
    const acc_t expected = gemmini_expected[tile][instance][index];
    const acc_t value = actual[index];
    if (value != expected) {
      printf("Gemmini%u mismatch tile=%u index=%u actual=%d expected=%d\n", instance, tile, index, (int)value, (int)expected);
      ++failures;
    }
  }
  return failures;
}

static void prepare_cgra(accel_t device, accel_t source, unsigned instance, unsigned tile) {
  const uintptr_t input = source.spm + source.spm_bytes - BYTES;
  accel_commands(device, {
    cgra_dma_mvin_async((const void *)input, INPUT[instance]);
    if (tile != 0) {
      cgra_prepare(&RELU4X4, CGRA_REPEAT);
    }
  });
}

static int start_cgra(accel_t device, unsigned instance, unsigned tile) {
  uint8_t tag = 0;
  accel_commands(device, {
    tag = cgra_dma_wait(TAG_BASE + instance);
    cgra_start(&RELU4X4);
  });
  if (tag != TAG_BASE + instance) {
    printf("CGRA%u DMA tag mismatch tile=%u actual=%u expected=%u\n", instance, tile, tag, TAG_BASE + instance);
    return 1;
  }
  return 0;
}

static int wait_cgra(accel_t device, unsigned instance, unsigned tile) {
  uint64_t ready = 0;
  uint64_t status = 0;
  uint64_t result = 0;
  accel_commands(device, {
    CGRA_WAIT(ready);
    CGRA_STATUS(status);
    CGRA_RESULT(result);
  });
  if (ready != 1 || (status & UINT64_C(1)) != 1 || ((status >> 1) & UINT64_C(0xffff)) != RELU4X4_EXPECTED_COMPLETES || result != 0) {
    printf("CGRA%u completion mismatch tile=%u ready=%lu status=%lu result=%lu\n", instance, tile, (unsigned long)ready, (unsigned long)status, (unsigned long)result);
    return 1;
  }
  return 0;
}

static int verify_cgra(accel_t device, unsigned instance, unsigned tile) {
  int failures = 0;
  for (unsigned index = 0; index < WORDS; ++index) {
    const acc_t expected = relu_expected[tile][instance][index];
    const acc_t value = instance == 0 ? ((const volatile int8_t *)device.spm)[index] : ((const volatile acc_t *)device.spm)[index];
    if (value != expected) {
      printf("CGRA%u mismatch tile=%u index=%u actual=%d expected=%d\n", instance, tile, index, (int)value, (int)expected);
      ++failures;
    }
  }
  return failures;
}

static int run_pool(unsigned tile) {
  uint32_t status = 0;
  accel_commands(POOL, {
    pool_config_output((uintptr_t)output[tile]);
    pool_start();
    status = pool_wait();
  });
  if (status != POOL_STATUS_SUCCESS) {
    printf("Pool completion mismatch tile=%u status=%u\n", tile, status);
    return 1;
  }
  return 0;
}

static int verify_pool(unsigned tile) {
  int failures = 0;
  for (unsigned channel = 0; channel < CHANNELS; ++channel) {
    acc_t expected = relu_expected[tile][INSTANCES - 1][channel];
    for (unsigned pixel = 1; pixel < POOL_HEIGHT * POOL_WIDTH; ++pixel) {
      const acc_t value = relu_expected[tile][INSTANCES - 1][pixel * CHANNELS + channel];
      if (value > expected) {
        expected = value;
      }
    }
    if (output[tile][channel] != expected) {
      printf("Pool mismatch tile=%u channel=%u actual=%d expected=%d\n", tile, channel, (int)output[tile][channel], (int)expected);
      ++failures;
    }
  }
  return failures;
}

int main(void) {
  const accel_t gemmini[INSTANCES] = {GEMMINI0, GEMMINI1};
  const accel_t cgra[INSTANCES] = {CGRA0, CGRA1};
  int failures = 0;

  init_inputs();
  init_expected();
  for (unsigned instance = 0; instance < INSTANCES; ++instance) {
    load_gemmini(gemmini[instance], instance);
    accel_commands(cgra[instance], { cgra_config(&RELU4X4, CGRA_COLD); });
  }
  accel_commands(GEMMINI0, { gemmini_extended_mvin(A, A_ROW, DIM, TILES * PUBLICATION_ROWS); });
  accel_commands(POOL, {
    pool_config_input(CGRA1.spm, POOL_HEIGHT, POOL_WIDTH, CHANNELS);
    pool_config_window(POOL_MODE_MAX, POOL_HEIGHT, POOL_WIDTH, POOL_HEIGHT, POOL_WIDTH, 0, 0);
  });
  for (unsigned tile = 0; tile < TILES; ++tile) {
    for (unsigned instance = 0; instance < INSTANCES; ++instance) {
      if (instance != 0) {
        accel_commands(gemmini[instance], { gemmini_extended_mvin((const void *)cgra[instance - 1].spm, A_ROW, DIM, PUBLICATION_ROWS); });
      }
      gemmini_fence();
      const uint32_t input_row = A_ROW + (instance == 0 ? tile * PUBLICATION_ROWS : 0);
      start_gemmini(gemmini[instance], input_row);
      gemmini_fence();
      prepare_cgra(cgra[instance], gemmini[instance], instance, tile);
      failures += start_cgra(cgra[instance], instance, tile);
      failures += wait_cgra(cgra[instance], instance, tile);
      cgra_dma_memory_fence();
    }
    failures += run_pool(tile);
    for (unsigned instance = 0; instance < INSTANCES; ++instance) {
      failures += verify_gemmini(gemmini[instance], instance, tile);
      failures += verify_cgra(cgra[instance], instance, tile);
    }
    failures += verify_pool(tile);
  }

  if (failures != 0) {
    printf("Multi-IP Manual: FAIL (%d)\n", failures);
    return 1;
  }
  printf("Multi-IP Manual: PASS\n");
  return 0;
}
