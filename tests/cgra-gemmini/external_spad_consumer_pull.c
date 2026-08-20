// clang-format off
#include "include/gemmini_params_issue4_t1.h"
#include "gemmini.h"
// clang-format on
#include "cgra_dma.h"
#include "gemmini_external_spad.h"

#include <stdint.h>
#include <stdio.h>

enum {
  TELEMETRY_BASE =
      GEMMINI_EXTERNAL_SPAD_BASE + GEMMINI_EXTERNAL_SPAD_SIZE_BYTES,
  WRITER_A_FIRE_COUNT = 0x198,
  WRITER_D_FIRE_COUNT = 0x1a0,
  WRITER_D_BLOCKED = 0x1a8,
  PRODUCER_ACTIVE = 0x178,
  CONSUMER_ENABLE = 0x200,
  PULL_JOB_ID = 0x208,
  PULL_SLOT = 0x210,
  PULL_BYTES = 0x218,
  PULL_SPM_WORD_ADDRESS = 0x220,
  PULL_DMA_TAG = 0x228,
  PULL_SUBMIT = 0x230,
  COMPLETION_ACCEPT = 0x238,
  COMPLETION_VALID = 0x240,
  COMPLETION_JOB_ID = 0x248,
  COMPLETION_SLOT = 0x250,
  COMPLETION_ACTUAL_BYTES = 0x258,
  COMPLETION_DMA_TAG = 0x260,
  COMPLETION_CONSUMER_STATUS = 0x268,
  COMPLETION_PRODUCER_STATUS = 0x270,
  COMPLETION_COUNT = 0x278,
  CONSUMER_ACTIVE = 0x2b0,
  CONSUMER_ERROR_COUNT = 0x2d0,
  CONSUMER_REQUEST_COUNT = 0x2e0,
  CONSUMER_DMA_COMMAND_COUNT = 0x2e8,
  CONSUMER_READ_START_COUNT = 0x2f0,
  CONSUMER_DMA_DONE_COUNT = 0x2f8,
  CONSUMER_RELEASE_COUNT = 0x300,
  ENDPOINT_SLOT0_STATE = 0x308,
  ENDPOINT_SLOT1_STATE = 0x310,
  PRODUCER_SUCCESS_READY_COUNT = 0x318,
  CONSUMER_EARLY_DMA_ISSUE_COUNT = 0x320,
  POLL_LIMIT = 32,
  PUBLICATION_STALL_WAIT_ITERATIONS = 8192,
  DMA_COMPLETION_WAIT_ITERATIONS = 4096,
  SLOT0_SPM_WORD = 0,
  SLOT1_SPM_WORD = 32,
  SLOT0_BYTES = 64,
  SLOT1_BYTES = 128,
  SLOT0_JOB = 0x81,
  SLOT1_JOB = 0x82,
  SLOT0_AUTO_TAG = 0x61,
  SLOT1_AUTO_TAG = 0x62,
  SLOT0_OBSERVER_TAG = 0xa1,
  SLOT1_OBSERVER_TAG = 0xa2,
  CPU_ARBITRATION_MVIN_TAG = 0x71,
  CPU_ARBITRATION_MVOUT_TAG = 0x72,
  CPU_ARBITRATION_SPM_WORD = 112,
};

_Static_assert(DIM == GEMMINI_EXTERNAL_SPAD_MATRIX_DIMENSION,
               "consumer validation must match Gemmini DIM");
_Static_assert(sizeof(acc_t) == 4, "consumer payload must remain int32");
_Static_assert(CGRA_DMA_DRAM_DATA_NBITS == 128,
               "consumer DMA requires one 16-byte TileLink Get per beat");
_Static_assert(CGRA_DMA_SPM_WORDS * sizeof(uint32_t) == 512,
               "T5 validates the current 512-byte CGRA SPM");
_Static_assert(SLOT0_BYTES % GEMMINI_EXTERNAL_SPAD_FULL_WIDTH_ROW_BYTES == 0,
               "slot 0 pull must contain full Gemmini publication rows");
_Static_assert(SLOT1_BYTES % GEMMINI_EXTERNAL_SPAD_FULL_WIDTH_ROW_BYTES == 0,
               "slot 1 pull must contain full Gemmini publication rows");

static const uint32_t ACC_FULL_ADDR = (uint32_t)5 << (ADDR_LEN - 3);
static acc_t accumulator_source[2][DIM] row_align_acc(1);
static acc_t slot0_observer[DIM] __attribute__((aligned(16)));
static acc_t slot1_observer[2 * DIM] __attribute__((aligned(16)));
static uint32_t cpu_arbitration_source[DIM] __attribute__((aligned(16)));
static uint32_t cpu_arbitration_observer[DIM] __attribute__((aligned(16)));

static const cgra_dma_desc_t SLOT0_OBSERVER_DESC =
    CGRA_DMA_DESC_CONST(SLOT0_SPM_WORD, SLOT0_BYTES, SLOT0_OBSERVER_TAG);
static const cgra_dma_desc_t SLOT1_OBSERVER_DESC =
    CGRA_DMA_DESC_CONST(SLOT1_SPM_WORD, SLOT1_BYTES, SLOT1_OBSERVER_TAG);
static const cgra_dma_desc_t CPU_ARBITRATION_MVIN_DESC = CGRA_DMA_DESC_CONST(
    CPU_ARBITRATION_SPM_WORD, SLOT0_BYTES, CPU_ARBITRATION_MVIN_TAG);
static const cgra_dma_desc_t CPU_ARBITRATION_MVOUT_DESC = CGRA_DMA_DESC_CONST(
    CPU_ARBITRATION_SPM_WORD, SLOT0_BYTES, CPU_ARBITRATION_MVOUT_TAG);

static uint32_t read32(uintptr_t offset) {
  return *(volatile uint32_t *)(TELEMETRY_BASE + offset);
}

static void write32(uintptr_t offset, uint32_t value) {
  *(volatile uint32_t *)(TELEMETRY_BASE + offset) = value;
}

static int poll_equal(uintptr_t offset, uint32_t expected) {
  for (unsigned attempt = 0; attempt < POLL_LIMIT; ++attempt) {
    if (read32(offset) == expected) {
      return 0;
    }
  }
  return 1;
}

static int poll_final_blocked(uint32_t expected_a_count,
                              uint32_t expected_d_count) {
  for (unsigned attempt = 0; attempt < POLL_LIMIT; ++attempt) {
    if (read32(WRITER_D_BLOCKED) != 0 &&
        read32(WRITER_A_FIRE_COUNT) == expected_a_count &&
        read32(WRITER_D_FIRE_COUNT) == expected_d_count) {
      return 0;
    }
  }
  return 1;
}

static void wait_iterations(unsigned iterations) {
  for (unsigned remaining = iterations; remaining != 0; --remaining) {
    __asm__ volatile("nop");
  }
}

static uint32_t expected_bits(unsigned phase, unsigned index) {
  uint32_t value = UINT32_C(0x81020304) ^ (phase * UINT32_C(0x10204081));
  value ^= index * UINT32_C(0x01010111);
  return value;
}

static acc_t acc_from_bits(uint32_t bits) {
  union {
    uint32_t bits;
    acc_t value;
  } conversion = {.bits = bits};
  return conversion.value;
}

static void load_accumulator(unsigned phase, unsigned rows) {
  for (unsigned row = 0; row < rows; ++row) {
    for (unsigned column = 0; column < DIM; ++column) {
      accumulator_source[row][column] =
          acc_from_bits(expected_bits(phase, row * DIM + column));
    }
  }
  gemmini_extended_mvin(accumulator_source, ACC_FULL_ADDR, DIM, rows);
  gemmini_fence();
}

static void submit_pull(uint32_t job, uint32_t slot, uint32_t bytes,
                        uint32_t spm_word, uint32_t tag) {
  write32(PULL_JOB_ID, job);
  write32(PULL_SLOT, slot);
  write32(PULL_BYTES, bytes);
  write32(PULL_SPM_WORD_ADDRESS, spm_word);
  write32(PULL_DMA_TAG, tag);
  write32(PULL_SUBMIT, 1);
}

static int verify_stalled_completion(uint32_t job, uint32_t slot,
                                     uint32_t bytes, uint32_t tag) {
  for (unsigned observation = 0; observation < 8; ++observation) {
    if (read32(COMPLETION_VALID) != 1 || read32(COMPLETION_JOB_ID) != job ||
        read32(COMPLETION_SLOT) != slot ||
        read32(COMPLETION_ACTUAL_BYTES) != bytes ||
        read32(COMPLETION_DMA_TAG) != tag ||
        read32(COMPLETION_CONSUMER_STATUS) != 0 ||
        read32(COMPLETION_PRODUCER_STATUS) != 0) {
      printf("consumer completion changed under backpressure job=%u\n", job);
      return 1;
    }
  }
  return 0;
}

static int run_pull(unsigned phase, uint32_t job, uint32_t slot, uint32_t bytes,
                    uint32_t spm_word, uint32_t auto_tag, unsigned rows,
                    uint32_t expected_completion_count) {
  const uint32_t requests_before = read32(CONSUMER_REQUEST_COUNT);
  const uint32_t commands_before = read32(CONSUMER_DMA_COMMAND_COUNT);
  const uint32_t starts_before = read32(CONSUMER_READ_START_COUNT);
  const uint32_t done_before = read32(CONSUMER_DMA_DONE_COUNT);
  const uint32_t releases_before = read32(CONSUMER_RELEASE_COUNT);
  const uint32_t writer_a_before = read32(WRITER_A_FIRE_COUNT);
  const uint32_t writer_d_before = read32(WRITER_D_FIRE_COUNT);
  const uint32_t producer_ready_before = read32(PRODUCER_SUCCESS_READY_COUNT);
  const uint32_t early_issue_before = read32(CONSUMER_EARLY_DMA_ISSUE_COUNT);
  const int test_cpu_arbitration = job == SLOT0_JOB;

  load_accumulator(phase, rows);
  submit_pull(job, slot, bytes, spm_word, auto_tag);
  if (poll_equal(CONSUMER_REQUEST_COUNT, requests_before + 1) != 0) {
    printf("consumer REQUEST was not forwarded job=%u\n", job);
    return 1;
  }
  if (poll_equal(PRODUCER_ACTIVE, 1) != 0) {
    printf("producer was not armed for consumer REQUEST job=%u\n", job);
    return 1;
  }
  if (read32(CONSUMER_DMA_COMMAND_COUNT) != commands_before ||
      read32(CONSUMER_READ_START_COUNT) != starts_before ||
      read32(CONSUMER_DMA_DONE_COUNT) != done_before ||
      read32(CONSUMER_RELEASE_COUNT) != releases_before) {
    printf("consumer DMA started before producer READY job=%u\n", job);
    return 1;
  }
  if (test_cpu_arbitration) {
    cgra_dma_mvin_async(cpu_arbitration_source, CPU_ARBITRATION_MVIN_DESC);
  }

  const uint32_t slot_row = slot == 0 ? GEMMINI_EXTERNAL_SPAD_OUTPUT_SLOT0_ROW
                                      : GEMMINI_EXTERNAL_SPAD_OUTPUT_SLOT1_ROW;
  gemmini_extended_mvout_spad(slot_row,
                              GEMMINI_EXTERNAL_SPAD_FULL_WIDTH_ROW_STRIDE,
                              ACC_FULL_ADDR, DIM, rows);

  if (poll_final_blocked(writer_a_before + rows, writer_d_before + rows - 1) !=
      0) {
    printf("publication final D did not stall job=%u A=%u/%u D=%u/%u "
           "blocked=%u producer=%u slot0=%u slot1=%u consumer=%u "
           "command=%u start=%u done=%u\n",
           job, read32(WRITER_A_FIRE_COUNT), writer_a_before + rows,
           read32(WRITER_D_FIRE_COUNT), writer_d_before + rows - 1,
           read32(WRITER_D_BLOCKED), read32(PRODUCER_ACTIVE),
           read32(ENDPOINT_SLOT0_STATE), read32(ENDPOINT_SLOT1_STATE),
           read32(CONSUMER_ACTIVE), read32(CONSUMER_DMA_COMMAND_COUNT),
           read32(CONSUMER_READ_START_COUNT), read32(CONSUMER_DMA_DONE_COUNT));
    return 1;
  }
  if (read32(CONSUMER_EARLY_DMA_ISSUE_COUNT) != early_issue_before) {
    printf("consumer DMA issued without final-D-derived READY job=%u "
           "slot0_state=%u slot1_state=%u producer_active=%u\n",
           job, read32(ENDPOINT_SLOT0_STATE), read32(ENDPOINT_SLOT1_STATE),
           read32(PRODUCER_ACTIVE));
    return 1;
  }

  // The validation configuration holds the final publication response for
  // 4096 target cycles. Advance that deterministic window without repeatedly
  // issuing slow validation-MMIO reads, then sample the acknowledged count.
  wait_iterations(PUBLICATION_STALL_WAIT_ITERATIONS);
  if (read32(WRITER_D_FIRE_COUNT) != writer_d_before + rows) {
    printf("producer final D did not complete job=%u\n", job);
    return 1;
  }
  if (test_cpu_arbitration) {
    const uint32_t commands_while_cpu = read32(CONSUMER_DMA_COMMAND_COUNT);
    if (commands_while_cpu > commands_before + 1 ||
        read32(CONSUMER_READ_START_COUNT) != starts_before ||
        read32(CONSUMER_DMA_DONE_COUNT) != done_before ||
        read32(CONSUMER_RELEASE_COUNT) != releases_before) {
      printf("automatic DMA bypassed CPU DMA ownership job=%u command=%u "
             "start=%u done=%u release=%u\n",
             job, commands_while_cpu, read32(CONSUMER_READ_START_COUNT),
             read32(CONSUMER_DMA_DONE_COUNT), read32(CONSUMER_RELEASE_COUNT));
      return 1;
    }
    if (cgra_dma_wait(CPU_ARBITRATION_MVIN_TAG) != CPU_ARBITRATION_MVIN_TAG) {
      printf("CPU arbitration DMA completion tag mismatch job=%u\n", job);
      return 1;
    }
  }
  // Allow the automatic owner to cross into the CGRA clock domain and finish
  // its bounded 64/128-byte transfer without a long train of MMIO polls.
  wait_iterations(DMA_COMPLETION_WAIT_ITERATIONS);
  if (poll_equal(COMPLETION_VALID, 1) != 0) {
    printf("consumer pull did not complete job=%u command=%u start=%u done=%u "
           "release=%u active=%u slot0=%u slot1=%u errors=%u\n",
           job, read32(CONSUMER_DMA_COMMAND_COUNT),
           read32(CONSUMER_READ_START_COUNT), read32(CONSUMER_DMA_DONE_COUNT),
           read32(CONSUMER_RELEASE_COUNT), read32(CONSUMER_ACTIVE),
           read32(ENDPOINT_SLOT0_STATE), read32(ENDPOINT_SLOT1_STATE),
           read32(CONSUMER_ERROR_COUNT));
    return 1;
  }
  if (read32(CONSUMER_DMA_COMMAND_COUNT) != commands_before + 1 ||
      read32(CONSUMER_READ_START_COUNT) != starts_before + 1 ||
      read32(CONSUMER_DMA_DONE_COUNT) != done_before + 1 ||
      read32(CONSUMER_RELEASE_COUNT) != releases_before + 1 ||
      read32(PRODUCER_SUCCESS_READY_COUNT) != producer_ready_before + 1 ||
      read32(CONSUMER_EARLY_DMA_ISSUE_COUNT) != early_issue_before) {
    printf("consumer event count mismatch job=%u command=%u start=%u done=%u "
           "release=%u\n",
           job, read32(CONSUMER_DMA_COMMAND_COUNT),
           read32(CONSUMER_READ_START_COUNT), read32(CONSUMER_DMA_DONE_COUNT),
           read32(CONSUMER_RELEASE_COUNT));
    return 1;
  }
  if (verify_stalled_completion(job, slot, bytes, auto_tag) != 0) {
    return 1;
  }

  write32(COMPLETION_ACCEPT, 1);
  if (poll_equal(COMPLETION_COUNT, expected_completion_count) != 0) {
    printf("consumer completion was not accepted job=%u\n", job);
    write32(COMPLETION_ACCEPT, 0);
    return 1;
  }
  write32(COMPLETION_ACCEPT, 0);
  if (read32(COMPLETION_VALID) != 0) {
    printf("consumer completion was delivered more than once job=%u\n", job);
    return 1;
  }
  return 0;
}

static int verify_observer(const acc_t *observer, unsigned words,
                           unsigned phase, const char *name) {
  int failures = 0;
  for (unsigned index = 0; index < words; ++index) {
    const uint32_t actual = (uint32_t)observer[index];
    const uint32_t expected = expected_bits(phase, index);
    if (actual != expected) {
      printf("%s mismatch index=%u actual=0x%x expected=0x%x\n", name, index,
             actual, expected);
      ++failures;
    }
  }
  return failures;
}

int main(void) {
  int failures = 0;

  for (unsigned index = 0; index < DIM; ++index) {
    cpu_arbitration_source[index] =
        UINT32_C(0xc1000000) ^ (index * UINT32_C(0x00102041));
  }

  gemmini_flush(0);
  gemmini_config_ld(DIM * sizeof(acc_t));
  gemmini_config_st(DIM * sizeof(acc_t));
  write32(CONSUMER_ENABLE, 1);
  write32(COMPLETION_ACCEPT, 0);

  failures += run_pull(3, SLOT0_JOB, 0, SLOT0_BYTES, SLOT0_SPM_WORD,
                       SLOT0_AUTO_TAG, 1, 1);
  if (failures == 0) {
    cgra_dma_mvout_async(slot0_observer, SLOT0_OBSERVER_DESC);
    if (cgra_dma_wait(SLOT0_OBSERVER_TAG) != SLOT0_OBSERVER_TAG) {
      printf("slot 0 CPU observer DMA tag mismatch\n");
      ++failures;
    }
    cgra_dma_memory_fence();
    failures += verify_observer(slot0_observer, DIM, 3, "slot0");
  }
  if (failures == 0) {
    cgra_dma_mvout_async(cpu_arbitration_observer, CPU_ARBITRATION_MVOUT_DESC);
    if (cgra_dma_wait(CPU_ARBITRATION_MVOUT_TAG) != CPU_ARBITRATION_MVOUT_TAG) {
      printf("CPU arbitration observer DMA tag mismatch\n");
      ++failures;
    }
    cgra_dma_memory_fence();
    for (unsigned index = 0; index < DIM; ++index) {
      if (cpu_arbitration_observer[index] != cpu_arbitration_source[index]) {
        printf("CPU arbitration payload mismatch index=%u actual=0x%x "
               "expected=0x%x\n",
               index, cpu_arbitration_observer[index],
               cpu_arbitration_source[index]);
        ++failures;
      }
    }
  }

  if (failures == 0) {
    failures += run_pull(7, SLOT1_JOB, 1, SLOT1_BYTES, SLOT1_SPM_WORD,
                         SLOT1_AUTO_TAG, 2, 2);
  }
  if (failures == 0) {
    cgra_dma_mvout_async(slot1_observer, SLOT1_OBSERVER_DESC);
    if (cgra_dma_wait(SLOT1_OBSERVER_TAG) != SLOT1_OBSERVER_TAG) {
      printf("slot 1 CPU observer DMA tag mismatch\n");
      ++failures;
    }
    cgra_dma_memory_fence();
    failures += verify_observer(slot1_observer, 2 * DIM, 7, "slot1");
  }

  if (read32(CONSUMER_ERROR_COUNT) != 0) {
    printf("consumer reported protocol errors=%u\n",
           read32(CONSUMER_ERROR_COUNT));
    ++failures;
  }

  if (failures != 0) {
    printf("Issue #4 T5 CGRA consumer pull: FAIL (%d)\n", failures);
    return 1;
  }

  printf("Issue #4 T5 CGRA consumer pull: PASS "
         "(slot0=64 slot1=128 real-TL-read/CMD_DMA_DONE)\n");
  return 0;
}
