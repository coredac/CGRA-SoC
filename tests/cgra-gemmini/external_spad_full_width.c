// clang-format off
#include "include/gemmini_params_issue4_t1.h"
#include "gemmini.h"
// clang-format on

#include <stdint.h>
#include <stdio.h>

#if DIM != 16
#error "Issue #4 T1 validation requires DIM=16"
#endif

#if MAX_BYTES != 64
#error "Issue #4 T1 validation requires 64-byte Gemmini DMA transactions"
#endif

#ifndef ACC_READ_FULL_WIDTH
#error "Issue #4 T1 validation requires full-width accumulator reads"
#endif

typedef char issue4_acc_t_must_be_32_bits[(sizeof(acc_t) == 4) ? 1 : -1];
typedef char issue4_elem_t_must_be_8_bits[(sizeof(elem_t) == 1) ? 1 : -1];

enum {
  EXTERNAL_SPAD_BASE = 0x60000000,
  EXTERNAL_SPAD_ROW_BYTES = DIM * sizeof(elem_t),
  FULL_ACC_ROW_BYTES = DIM * sizeof(acc_t),
  FULL_ACC_DST_STRIDE = FULL_ACC_ROW_BYTES / EXTERNAL_SPAD_ROW_BYTES,
  TELEMETRY_BASE = EXTERNAL_SPAD_BASE + 64 * 1024,
  TELEMETRY_WRITE_COMMITS = 0x00,
  TELEMETRY_WRITE_ACKS = 0x08,
  TELEMETRY_FULL_LINE_WRITES = 0x10,
  TELEMETRY_PARTIAL_WRITES = 0x18,
  TELEMETRY_LAST_ADDRESS = 0x20,
  TELEMETRY_LAST_MASK = 0x28,
  TELEMETRY_SAW_OUTSTANDING = 0x30,
};

static const uint32_t ACC_FULL_ADDR = (uint32_t)5 << (ADDR_LEN - 3);
static acc_t source[DIM][DIM] row_align_acc(1);

struct write_telemetry {
  uint32_t commits;
  uint32_t acks;
  uint32_t full_lines;
  uint32_t partials;
  uint64_t last_address;
  uint64_t last_mask;
  uint32_t saw_outstanding;
};

static uint32_t read32(uintptr_t address) {
  return *(volatile uint32_t *)address;
}

static uint64_t read64(uintptr_t address) {
  return *(volatile uint64_t *)address;
}

static struct write_telemetry read_write_telemetry(void) {
  struct write_telemetry result;

  result.commits = read32(TELEMETRY_BASE + TELEMETRY_WRITE_COMMITS);
  result.acks = read32(TELEMETRY_BASE + TELEMETRY_WRITE_ACKS);
  result.full_lines = read32(TELEMETRY_BASE + TELEMETRY_FULL_LINE_WRITES);
  result.partials = read32(TELEMETRY_BASE + TELEMETRY_PARTIAL_WRITES);
  result.last_address = read64(TELEMETRY_BASE + TELEMETRY_LAST_ADDRESS);
  result.last_mask = read64(TELEMETRY_BASE + TELEMETRY_LAST_MASK);
  result.saw_outstanding = read32(TELEMETRY_BASE + TELEMETRY_SAW_OUTSTANDING);
  return result;
}

static acc_t acc_from_bits(uint32_t bits) {
  union {
    uint32_t bits;
    acc_t value;
  } conversion = {.bits = bits};
  return conversion.value;
}

static uint32_t data_bits(unsigned phase, unsigned row, unsigned col) {
  uint32_t value = ((phase + row + col) & 1U) ? 0x89abcdefU : 0x12345678U;
  value ^= phase * 0x01020408U;
  value ^= row * 0x10200103U;
  value ^= col * 0x00010111U;
  return value;
}

static uint32_t guard_bits(unsigned guard_id, unsigned col) {
  const uint32_t base = guard_id == 0 ? 0x5aa55aa5U : 0xc33cc33cU;
  return base ^ (col * 0x01010307U);
}

static void load_source_rows(unsigned phase, unsigned rows) {
  for (unsigned row = 0; row < rows; ++row) {
    for (unsigned col = 0; col < DIM; ++col) {
      source[row][col] = acc_from_bits(data_bits(phase, row, col));
    }
  }
  gemmini_extended_mvin(source, ACC_FULL_ADDR, DIM, rows);
  gemmini_fence();
}

static void publish_guard(uint32_t dst_row, unsigned guard_id) {
  for (unsigned col = 0; col < DIM; ++col) {
    source[0][col] = acc_from_bits(guard_bits(guard_id, col));
  }
  gemmini_extended_mvin(source, ACC_FULL_ADDR, DIM, 1);
  gemmini_fence();
  gemmini_extended_mvout_spad(dst_row, FULL_ACC_DST_STRIDE, ACC_FULL_ADDR, DIM,
                              1);
  gemmini_fence();
}

static int verify_word_bytes(uintptr_t address, uint32_t expected,
                             const char *region, unsigned row, unsigned col) {
  int failures = 0;
  volatile uint8_t *actual = (volatile uint8_t *)address;

  for (unsigned byte = 0; byte < sizeof(expected); ++byte) {
    const uint8_t expected_byte = (uint8_t)(expected >> (byte * 8));
    if (actual[byte] != expected_byte) {
      printf(
          "%s byte mismatch row=%u col=%u byte=%u actual=0x%x expected=0x%x\n",
          region, row, col, byte, actual[byte], expected_byte);
      ++failures;
    }
  }
  return failures;
}

static int verify_guard(uint32_t dst_row, unsigned guard_id,
                        const char *region) {
  int failures = 0;
  const uintptr_t base =
      EXTERNAL_SPAD_BASE + (uintptr_t)dst_row * EXTERNAL_SPAD_ROW_BYTES;

  for (unsigned col = 0; col < DIM; ++col) {
    failures += verify_word_bytes(base + col * sizeof(acc_t),
                                  guard_bits(guard_id, col), region, 0, col);
  }
  return failures;
}

static int verify_result(uint32_t dst_row, unsigned phase, unsigned rows) {
  int failures = 0;
  const uintptr_t base =
      EXTERNAL_SPAD_BASE + (uintptr_t)dst_row * EXTERNAL_SPAD_ROW_BYTES;

  for (unsigned row = 0; row < rows; ++row) {
    for (unsigned col = 0; col < DIM; ++col) {
      const uintptr_t address =
          base + row * FULL_ACC_ROW_BYTES + col * sizeof(acc_t);
      failures += verify_word_bytes(address, data_bits(phase, row, col),
                                    "result", row, col);
    }
  }
  return failures;
}

static int validate_phase(const char *name, unsigned phase, uint32_t dst_row,
                          unsigned rows) {
  const uint32_t guard_before_row = dst_row - FULL_ACC_DST_STRIDE;
  const uint32_t guard_after_row = dst_row + rows * FULL_ACC_DST_STRIDE;
  int failures = 0;

  publish_guard(guard_before_row, 0);
  publish_guard(guard_after_row, 1);
  const struct write_telemetry before = read_write_telemetry();

  load_source_rows(phase, rows);
  gemmini_extended_mvout_spad(dst_row, FULL_ACC_DST_STRIDE, ACC_FULL_ADDR, DIM,
                              rows);
  gemmini_fence();

  const struct write_telemetry after = read_write_telemetry();
  const uint64_t expected_last_address =
      EXTERNAL_SPAD_BASE +
      (uint64_t)(dst_row + (rows - 1) * FULL_ACC_DST_STRIDE) *
          EXTERNAL_SPAD_ROW_BYTES;

  if (after.commits - before.commits != rows ||
      after.acks - before.acks != rows ||
      after.full_lines - before.full_lines != rows ||
      after.partials != before.partials) {
    printf("%s telemetry count mismatch commits=%u acks=%u full=%u partial=%u "
           "rows=%u\n",
           name, after.commits - before.commits, after.acks - before.acks,
           after.full_lines - before.full_lines,
           after.partials - before.partials, rows);
    ++failures;
  }
  if (after.commits != after.acks) {
    printf("%s fence returned before all writes were acknowledged: commits=%u "
           "acks=%u\n",
           name, after.commits, after.acks);
    ++failures;
  }
  if (after.last_address != expected_last_address ||
      after.last_mask != UINT64_MAX || after.saw_outstanding == 0) {
    printf("%s telemetry detail mismatch addr=0x%lx expected=0x%lx mask=0x%lx "
           "outstanding=%u\n",
           name, (unsigned long)after.last_address,
           (unsigned long)expected_last_address, (unsigned long)after.last_mask,
           after.saw_outstanding);
    ++failures;
  }
  failures += verify_result(dst_row, phase, rows);
  failures += verify_guard(guard_before_row, 0, "guard-before");
  failures += verify_guard(guard_after_row, 1, "guard-after");

  printf("Issue #4 T1 %s (%u bytes): %s\n", name, rows * FULL_ACC_ROW_BYTES,
         failures == 0 ? "PASS" : "FAIL");
  return failures;
}

int main(void) {
  int failures = 0;

  gemmini_flush(0);
  gemmini_config_ld(DIM * sizeof(acc_t));
  gemmini_config_st(DIM * sizeof(acc_t));

  failures += validate_phase("one-row layout", 1, 64, 1);
  failures += validate_phase("two-row transfer", 2, 128, 2);
  failures += validate_phase("full 16x16 transfer", 3, 256, DIM);

  if (failures != 0) {
    printf("Issue #4 T1 external-SPAD full-width validation: FAIL (%d "
           "mismatches)\n",
           failures);
    return 1;
  }

  printf("Issue #4 T1 external-SPAD full-width validation: PASS\n");
  return 0;
}
