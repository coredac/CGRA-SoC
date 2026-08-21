// clang-format off
#include "include/gemmini_params_issue4_t1.h"
#include "gemmini.h"
// clang-format on
#include "cgra_protocol.h"
#include "cgra_runtime.h"
#include "gemmini_external_spad.h"
#include "generated/cgra_relu4x4_fast_api.h"

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
  COMPLETION_COUNT = 0x278,
  COMPLETION_REQUESTED_BYTES = 0x338,
  CONSUMER_REQUEST_COUNT = 0x2e0,
  CONSUMER_DMA_COMMAND_COUNT = 0x2e8,
  CONSUMER_READ_START_COUNT = 0x2f0,
  CONSUMER_DMA_DONE_COUNT = 0x2f8,
  CONSUMER_RELEASE_COUNT = 0x300,
  PRODUCER_SUCCESS_READY_COUNT = 0x318,
  CONSUMER_EARLY_DMA_ISSUE_COUNT = 0x320,
  LAUNCH_GATE_ENABLE = 0x400,
  LAUNCH_JOB_ID = 0x408,
  LAUNCH_SLOT = 0x410,
  LAUNCH_BYTES = 0x418,
  LAUNCH_SPM_WORD_ADDRESS = 0x420,
  LAUNCH_DMA_TAG = 0x428,
  LAUNCH_PACKET_COUNT = 0x430,
  LAUNCH_SUBMIT = 0x438,
  LAUNCH_PACKET_LO = 0x440,
  LAUNCH_PACKET_MID = 0x448,
  LAUNCH_PACKET_HI = 0x450,
  LAUNCH_PACKET_TOP = 0x458,
  LAUNCH_PACKET_SUBMIT = 0x460,
  LAUNCH_RESULT_ACCEPT = 0x468,
  LAUNCH_RESULT_VALID = 0x470,
  LAUNCH_RESULT_COUNT = 0x478,
  LAST_LAUNCH_JOB_ID = 0x480,
  LAST_LAUNCH_SLOT = 0x488,
  LAST_LAUNCH_ACTUAL_BYTES = 0x490,
  LAST_LAUNCH_SPM_WORD_ADDRESS = 0x498,
  LAST_LAUNCH_DMA_TAG = 0x4a0,
  LAST_LAUNCH_PACKET_COUNT = 0x4a8,
  LAST_LAUNCH_STATUS = 0x4b0,
  LAUNCH_ACCEPTED_PACKET_COUNT = 0x4b8,
  COMPLETION_COUNT_AT_LAUNCH_RESULT = 0x4c0,
  LAUNCH_ERROR_COUNT = 0x4d8,
  LAST_LAUNCH_REQUESTED_BYTES = 0x4f0,
  POLL_LIMIT = 128,
  PUBLICATION_STALL_WAIT_ITERATIONS = 8192,
  DMA_COMPLETION_WAIT_ITERATIONS = 4096,
  JOB_ID = 0x601,
  SLOT = 0,
  TRANSFER_BYTES = 128,
  SPM_WORD_ADDRESS = 0,
  DMA_TAG = 0x66,
  PUBLICATION_ROWS = 2,
  LAUNCH_BUFFER_CAPACITY = 16,
};

_Static_assert(DIM == GEMMINI_EXTERNAL_SPAD_MATRIX_DIMENSION,
               "launch validation must match Gemmini DIM");
_Static_assert(sizeof(acc_t) == 4, "launch payload must remain int32");
_Static_assert(CGRA_INTRA_PKT_NBITS <= 256,
               "validation packet injection supports at most 256 bits");
_Static_assert(RELU4X4_FAST_LAUNCH_PACKET_COUNT > 0,
               "ReLU must contain a real launch sequence");
_Static_assert(RELU4X4_FAST_LAUNCH_PACKET_COUNT <= LAUNCH_BUFFER_CAPACITY,
               "ReLU launch sequence exceeds the T6 bounded buffer");
_Static_assert(TRANSFER_BYTES % GEMMINI_EXTERNAL_SPAD_FULL_WIDTH_ROW_BYTES == 0,
               "the transfer must contain complete publication rows");

static const uint32_t ACC_FULL_ADDR = (uint32_t)5 << (ADDR_LEN - 3);
static acc_t accumulator_source[PUBLICATION_ROWS][DIM] row_align_acc(1);

static uint32_t read32(uintptr_t offset) {
  return *(volatile uint32_t *)(TELEMETRY_BASE + offset);
}

static void write32(uintptr_t offset, uint32_t value) {
  *(volatile uint32_t *)(TELEMETRY_BASE + offset) = value;
}

static void write64(uintptr_t offset, uint64_t value) {
  *(volatile uint64_t *)(TELEMETRY_BASE + offset) = value;
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

static void initialize_accumulator(void) {
  for (unsigned row = 0; row < PUBLICATION_ROWS; ++row) {
    for (unsigned column = 0; column < DIM; ++column) {
      const int value = (int)(row * DIM + column) - DIM;
      accumulator_source[row][column] = (acc_t)value;
    }
  }
  gemmini_extended_mvin(accumulator_source, ACC_FULL_ADDR, DIM,
                        PUBLICATION_ROWS);
  gemmini_fence();
}

static void submit_pull(void) {
  write32(PULL_JOB_ID, JOB_ID);
  write32(PULL_SLOT, SLOT);
  write32(PULL_BYTES, TRANSFER_BYTES);
  write32(PULL_SPM_WORD_ADDRESS, SPM_WORD_ADDRESS);
  write32(PULL_DMA_TAG, DMA_TAG);
  write32(PULL_SUBMIT, 1);
}

static void submit_launch_header(void) {
  write32(LAUNCH_JOB_ID, JOB_ID);
  write32(LAUNCH_SLOT, SLOT);
  write32(LAUNCH_BYTES, TRANSFER_BYTES);
  write32(LAUNCH_SPM_WORD_ADDRESS, SPM_WORD_ADDRESS);
  write32(LAUNCH_DMA_TAG, DMA_TAG);
  write32(LAUNCH_PACKET_COUNT, RELU4X4_FAST_LAUNCH_PACKET_COUNT);
  write32(LAUNCH_SUBMIT, 1);
}

static void submit_launch_packet(cgra_packet_t packet) {
  write64(LAUNCH_PACKET_LO, packet.lo);
  write64(LAUNCH_PACKET_MID, packet.mid);
  write64(LAUNCH_PACKET_HI, packet.hi);
  write64(LAUNCH_PACKET_TOP, packet.top);
  write32(LAUNCH_PACKET_SUBMIT, 1);
}

static int verify_launch_result(void) {
  if (read32(LAST_LAUNCH_JOB_ID) != JOB_ID ||
      read32(LAST_LAUNCH_SLOT) != SLOT ||
      read32(LAST_LAUNCH_REQUESTED_BYTES) != TRANSFER_BYTES ||
      read32(LAST_LAUNCH_ACTUAL_BYTES) != TRANSFER_BYTES ||
      read32(LAST_LAUNCH_SPM_WORD_ADDRESS) != SPM_WORD_ADDRESS ||
      read32(LAST_LAUNCH_DMA_TAG) != DMA_TAG ||
      read32(LAST_LAUNCH_PACKET_COUNT) != RELU4X4_FAST_LAUNCH_PACKET_COUNT ||
      read32(LAST_LAUNCH_STATUS) != 0 ||
      read32(LAUNCH_ACCEPTED_PACKET_COUNT) !=
          RELU4X4_FAST_LAUNCH_PACKET_COUNT ||
      read32(COMPLETION_COUNT_AT_LAUNCH_RESULT) != 1 ||
      read32(LAUNCH_ERROR_COUNT) != 0) {
    return 1;
  }
  return 0;
}

int main(void) {
  gemmini_flush(0);
  gemmini_config_ld(DIM * sizeof(acc_t));
  gemmini_config_st(DIM * sizeof(acc_t));
  initialize_accumulator();

  write32(CONSUMER_ENABLE, 1);
  write32(LAUNCH_GATE_ENABLE, 1);
  write32(COMPLETION_ACCEPT, 0);
  write32(LAUNCH_RESULT_ACCEPT, 0);
  submit_pull();
  if (poll_equal(CONSUMER_REQUEST_COUNT, 1) != 0 ||
      poll_equal(PRODUCER_ACTIVE, 1) != 0) {
    printf("T6 consumer request did not arm producer\n");
    return 1;
  }

  submit_launch_header();
  const unsigned split = RELU4X4_FAST_LAUNCH_PACKET_COUNT / 2;
  for (unsigned index = 0; index < split; ++index) {
    submit_launch_packet(RELU4X4_FAST_LAUNCH_PACKETS[index]);
  }
  CGRA_SET_EXPECTED_COMPLETES(1);
  load_relu4x4_config_fast();
  for (unsigned index = split; index < RELU4X4_FAST_LAUNCH_PACKET_COUNT;
       ++index) {
    submit_launch_packet(RELU4X4_FAST_LAUNCH_PACKETS[index]);
  }

  const uint32_t writer_a_before = read32(WRITER_A_FIRE_COUNT);
  const uint32_t writer_d_before = read32(WRITER_D_FIRE_COUNT);
  gemmini_extended_mvout_spad(GEMMINI_EXTERNAL_SPAD_OUTPUT_SLOT0_ROW,
                              GEMMINI_EXTERNAL_SPAD_FULL_WIDTH_ROW_STRIDE,
                              ACC_FULL_ADDR, DIM, PUBLICATION_ROWS);
  if (poll_final_blocked(writer_a_before + PUBLICATION_ROWS,
                         writer_d_before + PUBLICATION_ROWS - 1) != 0) {
    printf("T6 publication final D did not stall\n");
    return 1;
  }
  if (read32(LAUNCH_RESULT_VALID) != 0 || read32(LAUNCH_RESULT_COUNT) != 0 ||
      read32(LAUNCH_ACCEPTED_PACKET_COUNT) != 0 ||
      read32(CONSUMER_DMA_COMMAND_COUNT) != 0 ||
      read32(CONSUMER_READ_START_COUNT) != 0 ||
      read32(CONSUMER_DMA_DONE_COUNT) != 0 ||
      read32(CONSUMER_RELEASE_COUNT) != 0) {
    printf("T6 launch or DMA occurred before final publication D\n");
    return 1;
  }

  wait_iterations(PUBLICATION_STALL_WAIT_ITERATIONS);
  if (read32(WRITER_D_FIRE_COUNT) != writer_d_before + PUBLICATION_ROWS) {
    printf("T6 producer final D did not complete\n");
    return 1;
  }
  wait_iterations(DMA_COMPLETION_WAIT_ITERATIONS);
  if (poll_equal(COMPLETION_VALID, 1) != 0 ||
      read32(COMPLETION_REQUESTED_BYTES) != TRANSFER_BYTES ||
      read32(CONSUMER_DMA_COMMAND_COUNT) != 1 ||
      read32(CONSUMER_READ_START_COUNT) != 1 ||
      read32(CONSUMER_DMA_DONE_COUNT) != 1 ||
      read32(CONSUMER_RELEASE_COUNT) != 1 ||
      read32(PRODUCER_SUCCESS_READY_COUNT) != 1 ||
      read32(CONSUMER_EARLY_DMA_ISSUE_COUNT) != 0) {
    printf("T6 matching consumer completion did not become available\n");
    return 1;
  }
  if (read32(LAUNCH_RESULT_VALID) != 0 || read32(LAUNCH_RESULT_COUNT) != 0 ||
      read32(LAUNCH_ACCEPTED_PACKET_COUNT) != 0) {
    printf("T6 launch escaped while completion was backpressured\n");
    return 1;
  }

  write32(COMPLETION_ACCEPT, 1);
  if (poll_equal(COMPLETION_COUNT, 1) != 0 ||
      poll_equal(LAUNCH_RESULT_VALID, 1) != 0) {
    printf("T6 completion did not release the retained launch sequence\n");
    return 1;
  }
  write32(COMPLETION_ACCEPT, 0);
  write32(LAUNCH_RESULT_ACCEPT, 1);
  if (poll_equal(LAUNCH_RESULT_COUNT, 1) != 0) {
    printf("T6 launch result was not accepted\n");
    return 1;
  }
  write32(LAUNCH_RESULT_ACCEPT, 0);

  if (verify_launch_result() != 0) {
    printf("T6 launch result identity/count/status mismatch\n");
    return 1;
  }

  printf("Issue #4 T6 event-driven CGRA launch gate: PASS "
         "(completion-before-launch packets=%u)\n",
         (unsigned)RELU4X4_FAST_LAUNCH_PACKET_COUNT);
  return 0;
}
