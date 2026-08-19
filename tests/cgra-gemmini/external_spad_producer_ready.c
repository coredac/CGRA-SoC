// clang-format off
#include "include/gemmini_params_issue4_t1.h"
#include "gemmini.h"
// clang-format on
#include "gemmini_external_spad.h"

#include <stdint.h>
#include <stdio.h>

enum {
  TELEMETRY_BASE =
      GEMMINI_EXTERNAL_SPAD_BASE + GEMMINI_EXTERNAL_SPAD_SIZE_BYTES,
  REQUEST_JOB_ID = 0x100,
  REQUEST_SLOT = 0x108,
  REQUEST_MAX_BYTES = 0x110,
  REQUEST_SUBMIT = 0x118,
  READY_ACCEPT = 0x120,
  READY_VALID = 0x128,
  READY_JOB_ID = 0x130,
  READY_SLOT = 0x138,
  READY_ACTUAL_BYTES = 0x140,
  READY_STATUS = 0x148,
  READY_DELIVERY_COUNT = 0x150,
  PRODUCER_ACTIVE = 0x178,
  PRODUCER_ISSUED_BYTES = 0x180,
  PRODUCER_ACKNOWLEDGED_BYTES = 0x188,
  WRITER_A_FIRE_COUNT = 0x198,
  WRITER_D_FIRE_COUNT = 0x1a0,
  WRITER_D_BLOCKED = 0x1a8,
  WRITER_D_BLOCKED_CYCLES = 0x1b0,
  WRITER_LAST_A_ADDRESS = 0x1b8,
  PUBLICATION_STALL_CYCLES = 0x1d8,
  PUBLICATION_STALL_FINAL_ACK = 0x1e0,
  POLL_LIMIT = 1000000,
};

_Static_assert(DIM == GEMMINI_EXTERNAL_SPAD_MATRIX_DIMENSION,
               "the producer validation must match Gemmini DIM");
_Static_assert(sizeof(elem_t) == 1, "Gemmini elements must be int8");
_Static_assert(sizeof(acc_t) == 4, "Gemmini accumulators must be int32");
_Static_assert(GEMMINI_EXTERNAL_SPAD_FULL_WIDTH_ROW_BYTES == 64,
               "the producer adapter observes 64-byte full-width rows");
_Static_assert(GEMMINI_EXTERNAL_SPAD_FULL_WIDTH_ROW_STRIDE == 4,
               "the existing MVOUT_SPAD command must retain stride four");
_Static_assert(GEMMINI_EXTERNAL_SPAD_OUTPUT_SLOT_SIZE_BYTES == 1024,
               "the full publication must occupy exactly one slot");

static const uint32_t ACC_FULL_ADDR = (uint32_t)5 << (ADDR_LEN - 3);
static acc_t source[DIM][DIM] row_align_acc(1);
static elem_t ordinary_spad_row[DIM] row_align(1);

static uint32_t read32(uintptr_t offset) {
  return *(volatile uint32_t *)(TELEMETRY_BASE + offset);
}

static uint64_t read64(uintptr_t offset) {
  return *(volatile uint64_t *)(TELEMETRY_BASE + offset);
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

static uint32_t data_bits(unsigned phase, unsigned row, unsigned column) {
  uint32_t value = ((phase + row + column) & 1U) ? UINT32_C(0x89abcdef)
                                                 : UINT32_C(0x12345678);
  value ^= phase * UINT32_C(0x01020408);
  value ^= row * UINT32_C(0x10200103);
  value ^= column * UINT32_C(0x00010111);
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
      source[row][column] = acc_from_bits(data_bits(phase, row, column));
    }
  }
  gemmini_extended_mvin(source, ACC_FULL_ADDR, DIM, rows);
  gemmini_fence();
}

static int verify_matrix(uintptr_t base, unsigned phase, unsigned rows,
                         const char *region) {
  int failures = 0;
  for (unsigned row = 0; row < rows; ++row) {
    for (unsigned column = 0; column < DIM; ++column) {
      const uint32_t actual = *(
          volatile uint32_t *)(base +
                               row *
                                   GEMMINI_EXTERNAL_SPAD_FULL_WIDTH_ROW_BYTES +
                               column * sizeof(uint32_t));
      const uint32_t expected = data_bits(phase, row, column);
      if (actual != expected) {
        printf("%s mismatch row=%u column=%u actual=0x%x expected=0x%x\n",
               region, row, column, actual, expected);
        ++failures;
      }
    }
  }
  return failures;
}

static int arm_request(uint32_t job_id, uint32_t slot, uint32_t bytes) {
  write32(REQUEST_JOB_ID, job_id);
  write32(REQUEST_SLOT, slot);
  write32(REQUEST_MAX_BYTES, bytes);
  write32(REQUEST_SUBMIT, 1);
  if (poll_equal(PRODUCER_ACTIVE, 1) != 0) {
    printf("producer did not arm job=%u slot=%u bytes=%u\n", job_id, slot,
           bytes);
    return 1;
  }
  return 0;
}

static int verify_stalled_ready(uint32_t job_id, uint32_t slot, uint32_t bytes,
                                uint32_t delivery_count) {
  int failures = 0;
  for (unsigned observation = 0; observation < 8; ++observation) {
    if (read32(READY_VALID) != 1 || read32(READY_JOB_ID) != job_id ||
        read32(READY_SLOT) != slot || read32(READY_ACTUAL_BYTES) != bytes ||
        read32(READY_STATUS) != 0 ||
        read32(READY_DELIVERY_COUNT) != delivery_count) {
      printf("READY changed under backpressure job=%u slot=%u observation=%u\n",
             job_id, slot, observation);
      ++failures;
      break;
    }
  }
  return failures;
}

static int accept_ready(uint32_t expected_delivery_count) {
  write32(READY_ACCEPT, 1);
  if (poll_equal(READY_DELIVERY_COUNT, expected_delivery_count) != 0) {
    printf("READY was not delivered exactly once expected_count=%u actual=%u\n",
           expected_delivery_count, read32(READY_DELIVERY_COUNT));
    write32(READY_ACCEPT, 0);
    return 1;
  }
  write32(READY_ACCEPT, 0);
  if (read32(READY_VALID) != 0) {
    printf("READY remained valid after delivery\n");
    return 1;
  }
  return 0;
}

int main(void) {
  const unsigned slot0_phase = 3;
  const unsigned slot1_phase = 6;
  int failures = 0;

  gemmini_flush(0);
  gemmini_config_ld(DIM * sizeof(acc_t));
  gemmini_config_st(DIM * sizeof(acc_t));
  write32(READY_ACCEPT, 0);

  if (read32(PUBLICATION_STALL_FINAL_ACK) != 1 ||
      read32(PUBLICATION_STALL_CYCLES) < 2) {
    printf("publication D-stall validation controls are not enabled\n");
    return 1;
  }

  load_accumulator(slot0_phase, 1);
  failures += arm_request(0x41, 0, 64);
  const uint32_t one_row_a_before = read32(WRITER_A_FIRE_COUNT);
  const uint32_t one_row_d_before = read32(WRITER_D_FIRE_COUNT);
  gemmini_extended_mvout_spad(GEMMINI_EXTERNAL_SPAD_OUTPUT_SLOT0_ROW,
                              GEMMINI_EXTERNAL_SPAD_FULL_WIDTH_ROW_STRIDE,
                              ACC_FULL_ADDR, DIM, 1);

  if (poll_final_blocked(one_row_a_before + 1, one_row_d_before) != 0) {
    printf("one-row final D did not enter validation backpressure\n");
    ++failures;
  } else if (read32(READY_VALID) != 0 || read32(PRODUCER_ISSUED_BYTES) != 64 ||
             read32(PRODUCER_ACKNOWLEDGED_BYTES) != 0) {
    printf("one-row READY was early issued=%u acknowledged=%u ready=%u\n",
           read32(PRODUCER_ISSUED_BYTES), read32(PRODUCER_ACKNOWLEDGED_BYTES),
           read32(READY_VALID));
    ++failures;
  }
  if (poll_equal(WRITER_D_FIRE_COUNT, one_row_d_before + 1) != 0 ||
      poll_equal(READY_VALID, 1) != 0) {
    printf("one-row READY did not follow the real D handshake\n");
    ++failures;
  }
  failures += verify_stalled_ready(0x41, 0, 64, 0);
  failures += verify_matrix(GEMMINI_EXTERNAL_SPAD_OUTPUT_SLOT0_BASE,
                            slot0_phase, 1, "slot0-one-row");
  failures += accept_ready(1);
  gemmini_fence();

  load_accumulator(slot1_phase, DIM);
  failures +=
      arm_request(0x42, 1, GEMMINI_EXTERNAL_SPAD_OUTPUT_SLOT_SIZE_BYTES);
  const uint32_t full_a_before = read32(WRITER_A_FIRE_COUNT);
  const uint32_t full_d_before = read32(WRITER_D_FIRE_COUNT);

  for (unsigned column = 0; column < DIM; ++column) {
    ordinary_spad_row[column] = (elem_t)(column * 3 + 1);
  }
  gemmini_extended_mvin(ordinary_spad_row, 0, DIM, 1);
  gemmini_fence();
  if (read32(WRITER_A_FIRE_COUNT) != full_a_before ||
      read32(WRITER_D_FIRE_COUNT) != full_d_before ||
      read32(PRODUCER_ISSUED_BYTES) != 0 ||
      read32(PRODUCER_ACKNOWLEDGED_BYTES) != 0 || read32(READY_VALID) != 0) {
    printf("ordinary external-SPAD write advanced publication tracking\n");
    ++failures;
  }

  gemmini_extended_mvout_spad(GEMMINI_EXTERNAL_SPAD_OUTPUT_SLOT1_ROW,
                              GEMMINI_EXTERNAL_SPAD_FULL_WIDTH_ROW_STRIDE,
                              ACC_FULL_ADDR, DIM, DIM);
  if (poll_final_blocked(full_a_before + DIM, full_d_before + DIM - 1) != 0) {
    printf("full publication final D did not enter validation backpressure "
           "a=%u d=%u\n",
           read32(WRITER_A_FIRE_COUNT), read32(WRITER_D_FIRE_COUNT));
    ++failures;
  } else if (read32(READY_VALID) != 0 ||
             read32(PRODUCER_ISSUED_BYTES) !=
                 GEMMINI_EXTERNAL_SPAD_OUTPUT_SLOT_SIZE_BYTES ||
             read32(PRODUCER_ACKNOWLEDGED_BYTES) !=
                 GEMMINI_EXTERNAL_SPAD_OUTPUT_SLOT_SIZE_BYTES -
                     GEMMINI_EXTERNAL_SPAD_FULL_WIDTH_ROW_BYTES ||
             read64(WRITER_LAST_A_ADDRESS) !=
                 GEMMINI_EXTERNAL_SPAD_OUTPUT_SLOT1_BASE +
                     GEMMINI_EXTERNAL_SPAD_OUTPUT_SLOT_SIZE_BYTES -
                     GEMMINI_EXTERNAL_SPAD_FULL_WIDTH_ROW_BYTES) {
    printf("full publication completed before final D handshake\n");
    ++failures;
  }
  if (poll_equal(WRITER_D_FIRE_COUNT, full_d_before + DIM) != 0 ||
      poll_equal(READY_VALID, 1) != 0) {
    printf("full publication READY did not follow all sixteen D handshakes\n");
    ++failures;
  }
  failures += verify_stalled_ready(
      0x42, 1, GEMMINI_EXTERNAL_SPAD_OUTPUT_SLOT_SIZE_BYTES, 1);
  failures += verify_matrix(GEMMINI_EXTERNAL_SPAD_OUTPUT_SLOT1_BASE,
                            slot1_phase, DIM, "slot1-full");
  failures += verify_matrix(GEMMINI_EXTERNAL_SPAD_OUTPUT_SLOT0_BASE,
                            slot0_phase, 1, "slot0-after-slot1");
  failures += accept_ready(2);
  gemmini_fence();

  if (read32(WRITER_D_BLOCKED_CYCLES) < 2 ||
      read32(WRITER_A_FIRE_COUNT) != full_a_before + DIM ||
      read32(WRITER_D_FIRE_COUNT) != full_d_before + DIM) {
    printf("publication monitor count mismatch blocked=%u a=%u d=%u\n",
           read32(WRITER_D_BLOCKED_CYCLES), read32(WRITER_A_FIRE_COUNT),
           read32(WRITER_D_FIRE_COUNT));
    ++failures;
  }

  if (failures != 0) {
    printf("Issue #4 T4 producer final-D READY: FAIL (%d)\n", failures);
    return 1;
  }

  printf("Issue #4 T4 producer final-D READY: PASS "
         "(one-row=64 full=1024 deliveries=2)\n");
  return 0;
}
