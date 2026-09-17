#include "accel_generated.h"
#include "auto_link.h"
#include "cgra_link.h"
#include "gemmini.h"
#include "gemmini_job.h"
#include "generated/cgra_relu4x4_fast_api.h"
#include "pool.h"

#include <stdint.h>
#include <stdio.h>

enum {
  INSTANCES = 2,
  STAGES = 5,
  WORDS = 32,
  BYTES = WORDS * sizeof(acc_t),
  ROW_BYTES = DIM * sizeof(elem_t),
  ACC_ROW_BYTES = DIM * sizeof(acc_t),
  ACC_ROW_STRIDE = sizeof(acc_t) / sizeof(elem_t),
  PUBLICATION_ROWS = BYTES / ACC_ROW_BYTES,
  A_ROW = 0,
  B_ROW = DIM,
  COMMANDS = 3,
  POOL_HEIGHT = 2,
  POOL_WIDTH = 2,
  CHANNELS = WORDS / (POOL_HEIGHT * POOL_WIDTH),
};

static elem_t A[INSTANCES][DIM][DIM] row_align(1);
static elem_t B[DIM][DIM] row_align(1);
static acc_t output[CHANNELS] __attribute__((aligned(32)));

static const uint32_t ACC_WRITE = (uint32_t)1 << (ADDR_LEN - 1);
static const uint32_t ACC_READ = ((uint32_t)1 << (ADDR_LEN - 1)) | ((uint32_t)1 << (ADDR_LEN - 3));

static void init_inputs(void) {
  for (unsigned row = 0; row < DIM; ++row) {
    for (unsigned column = 0; column < DIM; ++column) {
      const int value = (int)((row * DIM + column) % WORDS) - WORDS / 2;
      A[0][row][column] = (elem_t)value;
      A[1][row][column] = (elem_t)(-value - 1);
      B[row][column] = row == column ? (elem_t)1 : (elem_t)0;
    }
  }
  for (unsigned channel = 0; channel < CHANNELS; ++channel) {
    output[channel] = (acc_t)0x5a5a5a5a;
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

static int configure_gemmini(accel_t device, uint32_t job) {
  const uint32_t publication = device.spm_bytes / ROW_BYTES - PUBLICATION_ROWS * ACC_ROW_STRIDE;
  if (gemmini_job_capture_at(device.control, job, COMMANDS, 0) != 0) {
    return 1;
  }
  accel_commands(device, {
    gemmini_preload(B_ROW, ACC_WRITE);
    gemmini_compute_preloaded(A_ROW, GARBAGE_ADDR);
    gemmini_extended_mvout_spad(publication, ACC_ROW_STRIDE, ACC_READ, DIM, PUBLICATION_ROWS);
  });
  return 0;
}

static int configure_cgra(accel_t device, uint32_t job) {
  int status = 0;
  accel_commands(device, { status = cgra_job_config_at(device.control, job, &RELU4X4, NULL); });
  return status;
}

static void configure_pool(void) {
  accel_commands(POOL, {
    pool_config_input(CGRA0.spm, POOL_HEIGHT, POOL_WIDTH, CHANNELS);
    pool_config_output((uintptr_t)output);
    pool_config_window(POOL_MODE_MAX, POOL_HEIGHT, POOL_WIDTH, POOL_HEIGHT, POOL_WIDTH, 0, 0);
  });
}

static int verify_results(void) {
  const uint32_t expected = (UINT32_C(1) << AUTO_LINK_STAGE_GEMMINI0) | (UINT32_C(1) << AUTO_LINK_STAGE_GEMMINI1) | (UINT32_C(1) << AUTO_LINK_STAGE_CGRA0) | (UINT32_C(1) << AUTO_LINK_STAGE_CGRA1) |
                            (UINT32_C(1) << AUTO_LINK_STAGE_POOL);
  uint32_t seen = 0;
  int failures = 0;
  for (unsigned index = 0; index < STAGES; ++index) {
    const cgra_link_result_t result = cgra_link_wait_at(CGRA0.control);
    const uint32_t bit = result.stage < 32 ? UINT32_C(1) << result.stage : 0;
    if (result.status != AUTO_LINK_STATUS_SUCCESS || result.detail != 0 || result.data != 0 || result.job != 0 || (expected & bit) == 0 || (seen & bit) != 0) {
      printf("AutoLink result mismatch stage=%u job=%u status=%u detail=%u data=%u\n", result.stage, result.job, result.status, result.detail, result.data);
      ++failures;
    }
    seen |= bit;
  }
  return failures + (seen != expected);
}

static acc_t expected_relu(unsigned instance, unsigned index) {
  const acc_t value = A[instance][index / DIM][index % DIM];
  return value > 0 ? value : 0;
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

static int verify_pool(void) {
  int failures = 0;
  for (unsigned channel = 0; channel < CHANNELS; ++channel) {
    acc_t expected = expected_relu(0, channel);
    for (unsigned pixel = 1; pixel < POOL_HEIGHT * POOL_WIDTH; ++pixel) {
      const acc_t value = expected_relu(0, pixel * CHANNELS + channel);
      if (value > expected) {
        expected = value;
      }
    }
    if (output[channel] != expected) {
      printf("Pool mismatch channel=%u actual=%d expected=%d\n", channel, (int)output[channel], (int)expected);
      ++failures;
    }
  }
  return failures;
}

int main(void) {
  const accel_t gemmini[INSTANCES] = {GEMMINI0, GEMMINI1};
  const accel_t cgra[INSTANCES] = {CGRA0, CGRA1};
  const uint32_t gemmini_jobs[INSTANCES] = {AUTO_LINK_JOB_GEMMINI0, AUTO_LINK_JOB_GEMMINI1};
  const uint32_t cgra_jobs[INSTANCES] = {AUTO_LINK_JOB_CGRA0, AUTO_LINK_JOB_CGRA1};

  init_inputs();
  for (unsigned instance = 0; instance < INSTANCES; ++instance) {
    load_gemmini(gemmini[instance], instance);
  }
  gemmini_fence();
  for (unsigned instance = 0; instance < INSTANCES; ++instance) {
    if (configure_gemmini(gemmini[instance], gemmini_jobs[instance]) != 0 || configure_cgra(cgra[instance], cgra_jobs[instance]) != 0) {
      printf("Multi-IP Auto: FAIL (configuration instance=%u)\n", instance);
      return 1;
    }
  }
  configure_pool();

  auto_link_input_ready();
  int failures = verify_results();
  for (unsigned instance = 0; instance < INSTANCES; ++instance) {
    failures += verify_gemmini(gemmini[instance], instance);
    failures += verify_cgra(cgra[instance], instance);
  }
  failures += verify_pool();
  if (failures != 0) {
    printf("Multi-IP Auto: FAIL (%d)\n", failures);
    return 1;
  }
  printf("Multi-IP Auto: PASS\n");
  return 0;
}
