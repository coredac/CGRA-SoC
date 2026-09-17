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

static elem_t A[INSTANCES][DIM][DIM] row_align(1);
static elem_t B[DIM][DIM] row_align(1);
static acc_t output[INSTANCES][CHANNELS] __attribute__((aligned(32)));

static const uint32_t ACC_WRITE = (uint32_t)1 << (ADDR_LEN - 1);
static const uint32_t ACC_READ = ((uint32_t)1 << (ADDR_LEN - 1)) | ((uint32_t)1 << (ADDR_LEN - 3));
static const cgra_dma_desc_t INPUT[INSTANCES] = {
    CGRA_DMA_DESC_CONST(0, BYTES, TAG_BASE),
    CGRA_DMA_DESC_CONST(0, BYTES, TAG_BASE + 1),
};

static void init_inputs(void) {
  for (unsigned row = 0; row < DIM; ++row) {
    for (unsigned column = 0; column < DIM; ++column) {
      const int value = (int)((row * DIM + column) % WORDS) - WORDS / 2;
      A[0][row][column] = (elem_t)value;
      A[1][row][column] = (elem_t)(-value - 1);
      B[row][column] = row == column ? (elem_t)1 : (elem_t)0;
    }
  }
  for (unsigned instance = 0; instance < INSTANCES; ++instance) {
    for (unsigned channel = 0; channel < CHANNELS; ++channel) {
      output[instance][channel] = (acc_t)0x5a5a5a5a;
    }
  }
}

static void load_gemmini(accel_t device, unsigned instance) {
  accel_commands(device, {
    gemmini_flush(0);
    gemmini_config_ld(ROW_BYTES);
    gemmini_config_ex(WEIGHT_STATIONARY, NO_ACTIVATION, 0);
    gemmini_config_st(ACC_ROW_BYTES);
    gemmini_mvin(A[instance], A_ROW);
    gemmini_mvin(B, B_ROW);
  });
}

static void start_gemmini(accel_t device) {
  const uint32_t publication = device.spm_bytes / ROW_BYTES - PUBLICATION_ROWS * ACC_ROW_STRIDE;
  accel_commands(device, {
    gemmini_preload(B_ROW, ACC_WRITE);
    gemmini_compute_preloaded(A_ROW, GARBAGE_ADDR);
    gemmini_extended_mvout_spad(publication, ACC_ROW_STRIDE, ACC_READ, DIM, PUBLICATION_ROWS);
  });
}

static int verify_gemmini(accel_t device, unsigned instance) {
  const volatile acc_t *actual = (const volatile acc_t *)(device.spm + device.spm_bytes - BYTES);
  int failures = 0;
  for (unsigned index = 0; index < WORDS; ++index) {
    const acc_t expected = A[instance][index / DIM][index % DIM];
    const acc_t value = actual[index];
    if (value != expected) {
      printf("Gemmini%u mismatch index=%u actual=%d expected=%d\n", instance, index, (int)value, (int)expected);
      ++failures;
    }
  }
  return failures;
}

static void configure_cgra(accel_t device, accel_t source, unsigned instance) {
  const uintptr_t input = source.spm + source.spm_bytes - BYTES;
  accel_commands(device, {
    cgra_dma_mvin_async((const void *)input, INPUT[instance]);
    cgra_config(&RELU4X4, CGRA_COLD);
  });
}

static int start_cgra(accel_t device, unsigned instance) {
  uint8_t tag = 0;
  accel_commands(device, {
    tag = cgra_dma_wait(TAG_BASE + instance);
    cgra_start(&RELU4X4);
  });
  if (tag != TAG_BASE + instance) {
    printf("CGRA%u DMA tag mismatch actual=%u expected=%u\n", instance, tag, TAG_BASE + instance);
    return 1;
  }
  return 0;
}

static int wait_cgra(accel_t device, unsigned instance) {
  uint64_t ready = 0;
  uint64_t status = 0;
  uint64_t result = 0;
  accel_commands(device, {
    CGRA_WAIT(ready);
    CGRA_STATUS(status);
    CGRA_RESULT(result);
  });
  if (ready != 1 || (status & UINT64_C(1)) != 1 || ((status >> 1) & UINT64_C(0xffff)) != RELU4X4_EXPECTED_COMPLETES || result != 0) {
    printf("CGRA%u completion mismatch ready=%lu status=%lu result=%lu\n", instance, (unsigned long)ready, (unsigned long)status, (unsigned long)result);
    return 1;
  }
  return 0;
}

static acc_t expected_relu(unsigned instance, unsigned index) {
  const acc_t value = A[instance][index / DIM][index % DIM];
  return value > 0 ? value : 0;
}

static int verify_cgra(accel_t device, unsigned instance) {
  const volatile acc_t *actual = (const volatile acc_t *)device.spm;
  int failures = 0;
  for (unsigned index = 0; index < WORDS; ++index) {
    const acc_t expected = expected_relu(instance, index);
    const acc_t value = actual[index];
    if (value != expected) {
      printf("CGRA%u mismatch index=%u actual=%d expected=%d\n", instance, index, (int)value, (int)expected);
      ++failures;
    }
  }
  return failures;
}

static int run_pool(accel_t source, unsigned instance) {
  uint32_t status = 0;
  accel_commands(POOL, {
    pool_config_input(source.spm, POOL_HEIGHT, POOL_WIDTH, CHANNELS);
    pool_config_output((uintptr_t)output[instance]);
    pool_config_window(POOL_MODE_MAX, POOL_HEIGHT, POOL_WIDTH, POOL_HEIGHT, POOL_WIDTH, 0, 0);
    pool_start();
    status = pool_wait();
  });
  if (status != POOL_STATUS_SUCCESS) {
    printf("Pool completion mismatch input=%u status=%u\n", instance, status);
    return 1;
  }
  return 0;
}

static int verify_pool(unsigned instance) {
  int failures = 0;
  for (unsigned channel = 0; channel < CHANNELS; ++channel) {
    acc_t expected = expected_relu(instance, channel);
    for (unsigned pixel = 1; pixel < POOL_HEIGHT * POOL_WIDTH; ++pixel) {
      const acc_t value = expected_relu(instance, pixel * CHANNELS + channel);
      if (value > expected) {
        expected = value;
      }
    }
    if (output[instance][channel] != expected) {
      printf("Pool mismatch input=%u channel=%u actual=%d expected=%d\n", instance, channel, (int)output[instance][channel], (int)expected);
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
  for (unsigned instance = 0; instance < INSTANCES; ++instance) {
    load_gemmini(gemmini[instance], instance);
  }
  gemmini_fence();
  for (unsigned instance = 0; instance < INSTANCES; ++instance) {
    start_gemmini(gemmini[instance]);
  }
  gemmini_fence();
  for (unsigned instance = 0; instance < INSTANCES; ++instance) {
    failures += verify_gemmini(gemmini[instance], instance);
    configure_cgra(cgra[instance], gemmini[instance], instance);
  }
  for (unsigned instance = 0; instance < INSTANCES; ++instance) {
    failures += start_cgra(cgra[instance], instance);
  }
  for (unsigned instance = 0; instance < INSTANCES; ++instance) {
    failures += wait_cgra(cgra[instance], instance);
  }
  cgra_dma_memory_fence();
  for (unsigned instance = 0; instance < INSTANCES; ++instance) {
    failures += verify_cgra(cgra[instance], instance);
    failures += run_pool(cgra[instance], instance);
    failures += verify_pool(instance);
  }

  if (failures != 0) {
    printf("Multi-IP Manual: FAIL (%d)\n", failures);
    return 1;
  }
  printf("Multi-IP Manual: PASS\n");
  return 0;
}
