#include "accel_generated.h"
#include "auto_link.h"
#include "cgra_link.h"
#include "gemmini.h"
#include "gemmini_job.h"
#include "generated/cgra_relu_runtime_fast_api.h"
#include "pool.h"

#include <stdint.h>
#include <stdio.h>

enum {
  INSTANCES = 2,
  STAGES = 5,
  TILES = 8,
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

static elem_t A[TILES][PUBLICATION_ROWS][DIM] row_align(1);
static elem_t B[INSTANCES][DIM][DIM] row_align(1);
static acc_t gemmini_expected[INSTANCES][TILES][WORDS];
static acc_t relu_expected[INSTANCES][TILES][WORDS];
static acc_t output[TILES][CHANNELS] __attribute__((aligned(32)));

static const uint32_t ACC_WRITE = (uint32_t)1 << (ADDR_LEN - 1);
static const uint32_t ACC_READ = ((uint32_t)1 << (ADDR_LEN - 1)) | ((uint32_t)1 << (ADDR_LEN - 3));

static unsigned long cycles(void) {
  unsigned long value;
  __asm__ volatile("rdcycle %0" : "=r"(value)::"memory");
  return value;
}

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
  for (unsigned instance = 0; instance < INSTANCES; ++instance) {
    for (unsigned tile = 0; tile < TILES; ++tile) {
      for (unsigned row = 0; row < PUBLICATION_ROWS; ++row) {
        for (unsigned column = 0; column < DIM; ++column) {
          acc_t sum = 0;
          for (unsigned input = 0; input < DIM; ++input) {
            const elem_t weight = B[instance][input][column];
            if (weight == 0) {
              continue;
            }
            const acc_t value = instance == 0 ? A[tile][row][input] : relu_expected[instance - 1][tile][row * DIM + input];
            sum += value * weight;
          }
          gemmini_expected[instance][tile][row * DIM + column] = sum;
          relu_expected[instance][tile][row * DIM + column] = sum > 0 ? sum : 0;
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

static int configure_gemmini(accel_t device, uint32_t job, unsigned instance, unsigned slots) {
  const uint32_t publication = (device.spm_bytes - slots * BYTES) / ROW_BYTES;
  gemmini_job_write_at(device.control, GEMMINI_JOB_WINDOW_PIXEL_BYTES, CHANNELS * sizeof(elem_t));
  if (gemmini_job_capture_at(device.control, job, COMMANDS + (instance != 0), 2) != 0) {
    return 1;
  }
  if (instance == 0) {
    const gemmini_patch_t input = {.command = 1, .operand = 0, .lsb = 0, .width = ADDR_LEN, .source = GEMMINI_VALUE_TILE_ID, .scale = PUBLICATION_ROWS, .offset = A_ROW};
    gemmini_job_patch_at(device.control, &input);
  } else {
    const gemmini_patch_t input = {.command = 0, .operand = 0, .lsb = 0, .width = 64, .source = GEMMINI_VALUE_INPUT_ADDRESS, .scale = 1, .offset = 0};
    gemmini_job_patch_at(device.control, &input);
  }
  const gemmini_patch_t destination = {
      .command = COMMANDS - 1 + (instance != 0), .operand = 0, .lsb = 0, .width = ADDR_LEN, .source = GEMMINI_VALUE_SLOT, .scale = BYTES / ROW_BYTES, .offset = publication};
  gemmini_job_patch_at(device.control, &destination);
  accel_commands(device, {
    if (instance != 0) {
      gemmini_extended_mvin((const void *)CGRA0.spm, A_ROW, DIM, PUBLICATION_ROWS);
    }
    gemmini_extended_preload(B_ROW, ACC_WRITE, DIM, DIM, DIM, PUBLICATION_ROWS);
    gemmini_extended_compute_preloaded(A_ROW, GARBAGE_ADDR, DIM, PUBLICATION_ROWS, DIM, PUBLICATION_ROWS);
    gemmini_extended_mvout_spad(publication, ACC_ROW_STRIDE, ACC_READ, DIM, PUBLICATION_ROWS);
  });
  return 0;
}

static int configure_cgra(accel_t device, uint32_t job) {
  static const cgra_link_symbol_t symbols[RELU_RUNTIME_SYMBOL_COUNT] = {
      [RELU_RUNTIME_SYMBOL_INPUT0] = {0, WORDS, CGRA_LINK_SLOT},
      [RELU_RUNTIME_SYMBOL_OUTPUT] = {0, WORDS, CGRA_LINK_SLOT},
      [RELU_RUNTIME_SYMBOL_ELEMENTS] = {0, 0, CGRA_LINK_ELEMENTS},
  };
  int status = 0;
  accel_commands(device, { status = cgra_job_config_at(device.control, job, &RELU_RUNTIME, symbols); });
  return status;
}

static void configure_pool(void) {
  accel_commands(POOL, {
    pool_config_input(CGRA1.spm, TILES * POOL_HEIGHT, POOL_WIDTH, CHANNELS);
    pool_config_output((uintptr_t)output);
    pool_config_window(POOL_MODE_MAX, POOL_HEIGHT, POOL_WIDTH, POOL_HEIGHT, POOL_WIDTH, 0, 0);
    pool_config_tiled(1);
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

// The allocator chooses each IP's slot independently; locate the final tile after drain.
static int verify_gemmini(accel_t device, unsigned instance, unsigned slots) {
  for (unsigned slot = 0; slot < slots; ++slot) {
    const volatile acc_t *actual = (const volatile acc_t *)(device.spm + device.spm_bytes - slots * BYTES + slot * BYTES);
    unsigned index = 0;
    while (index < WORDS && actual[index] == gemmini_expected[instance][TILES - 1][index]) {
      ++index;
    }
    if (index == WORDS) {
      return 0;
    }
  }
  printf("Gemmini%u final tile missing from publication slots\n", instance);
  return 1;
}

static int verify_cgra(accel_t device, unsigned instance, unsigned slots) {
  for (unsigned slot = 0; slot < slots; ++slot) {
    unsigned index = 0;
    for (; index < WORDS; ++index) {
      const unsigned offset = slot * WORDS + index;
      const acc_t value = instance == 0 ? ((const volatile int8_t *)device.spm)[offset] : ((const volatile acc_t *)device.spm)[offset];
      if (value != relu_expected[instance][TILES - 1][index]) {
        break;
      }
    }
    if (index == WORDS) {
      return 0;
    }
  }
  printf("CGRA%u final tile missing from local slots\n", instance);
  return 1;
}

static int verify_pool(void) {
  int failures = 0;
  for (unsigned tile = 0; tile < TILES; ++tile) {
    for (unsigned channel = 0; channel < CHANNELS; ++channel) {
      acc_t expected = relu_expected[INSTANCES - 1][tile][channel];
      for (unsigned pixel = 1; pixel < POOL_HEIGHT * POOL_WIDTH; ++pixel) {
        const acc_t value = relu_expected[INSTANCES - 1][tile][pixel * CHANNELS + channel];
        if (value > expected) {
          expected = value;
        }
      }
      if (output[tile][channel] != expected) {
        printf("Pool mismatch tile=%u channel=%u actual=%d expected=%d\n", tile, channel, (int)output[tile][channel], (int)expected);
        ++failures;
      }
    }
  }
  return failures;
}

int main(void) {
  const accel_t gemmini[INSTANCES] = {GEMMINI0, GEMMINI1};
  const accel_t cgra[INSTANCES] = {CGRA0, CGRA1};
  const uint32_t gemmini_jobs[INSTANCES] = {AUTO_LINK_JOB_GEMMINI0, AUTO_LINK_JOB_GEMMINI1};
  const uint32_t cgra_jobs[INSTANCES] = {AUTO_LINK_JOB_CGRA0, AUTO_LINK_JOB_CGRA1};
  const unsigned gemmini_slots[INSTANCES] = {AUTO_LINK_GEMMINI0_BUFFER_SLOTS, AUTO_LINK_GEMMINI1_BUFFER_SLOTS};
  const unsigned cgra_slots[INSTANCES] = {AUTO_LINK_CGRA0_BUFFER_SLOTS, AUTO_LINK_CGRA1_BUFFER_SLOTS};
  const unsigned transfers[INSTANCES] = {AUTO_LINK_COPY_GEMMINI0_CGRA0, AUTO_LINK_COPY_GEMMINI1_CGRA1};

  init_inputs();
  init_expected();
  for (unsigned instance = 0; instance < INSTANCES; ++instance) {
    load_gemmini(gemmini[instance], instance);
  }
  accel_commands(GEMMINI0, { gemmini_extended_mvin(A, A_ROW, DIM, TILES * PUBLICATION_ROWS); });
  gemmini_fence();
  for (unsigned instance = 0; instance < INSTANCES; ++instance) {
    if (configure_gemmini(gemmini[instance], gemmini_jobs[instance], instance, gemmini_slots[instance]) != 0 || configure_cgra(cgra[instance], cgra_jobs[instance]) != 0) {
      printf("Multi-IP Auto: FAIL (configuration instance=%u)\n", instance);
      return 1;
    }
    auto_link_transfer(transfers[instance], gemmini[instance].spm_bytes - gemmini_slots[instance] * BYTES, 0, BYTES, BYTES, CHANNELS * sizeof(acc_t));
  }
  auto_link_transfer(AUTO_LINK_COPY_CGRA0_GEMMINI1, 0, 0, WORDS * sizeof(elem_t), 0, CHANNELS * sizeof(elem_t));
  auto_link_transfer(AUTO_LINK_COPY_CGRA1_POOL, 0, 0, BYTES, 0, CHANNELS * sizeof(acc_t));
  configure_pool();
  const uint32_t stages[] = {AUTO_LINK_STAGE_GEMMINI0, AUTO_LINK_STAGE_CGRA0, AUTO_LINK_STAGE_GEMMINI1, AUTO_LINK_STAGE_CGRA1};
  for (unsigned index = 0; index < sizeof(stages) / sizeof(stages[0]); ++index) {
    auto_link_region(stages[index], TILES * POOL_HEIGHT, POOL_WIDTH, POOL_HEIGHT, POOL_WIDTH, 0, 0, 0, 0);
  }
  auto_link_tiles(TILES, 1, 1, 1);

  __asm__ volatile("fence rw, rw" ::: "memory");
  const unsigned long begin = cycles();
  auto_link_input_ready();
  int failures = verify_results();
  while (*(volatile uint32_t *)(AUTO_LINK_BASE + AUTO_LINK_RUNNING)) {
  }
  __asm__ volatile("fence rw, rw" ::: "memory");
  const unsigned long cpu_cycles = cycles() - begin;
  const unsigned long fabric_cycles = *(volatile uint64_t *)(AUTO_LINK_BASE + AUTO_LINK_CYCLES);
  const unsigned long overlap = *(volatile uint64_t *)(AUTO_LINK_BASE + AUTO_LINK_OVERLAP);
  const unsigned long peak = *(volatile uint64_t *)(AUTO_LINK_BASE + AUTO_LINK_PEAK_ACTIVE);
  printf("Multi-IP tiles=%u slots=%u/%u/%u/%u cycles=%lu cpu_cycles=%lu overlap=%lu peak=%lu\n", TILES, gemmini_slots[0], cgra_slots[0], gemmini_slots[1], cgra_slots[1], fabric_cycles, cpu_cycles,
         overlap, peak);
  if (overlap == 0 || peak < 3) {
    printf("Multi-IP pipeline did not overlap at least three tile IDs\n");
    ++failures;
  }
  for (unsigned instance = 0; instance < INSTANCES; ++instance) {
    failures += verify_gemmini(gemmini[instance], instance, gemmini_slots[instance]);
    failures += verify_cgra(cgra[instance], instance, cgra_slots[instance]);
  }
  failures += verify_pool();
  if (failures != 0) {
    printf("Multi-IP Auto: FAIL (%d)\n", failures);
    return 1;
  }
  printf("Multi-IP Auto: PASS\n");
  return 0;
}
