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
  TELEMETRY_READ_BACKPRESSURE_CYCLES = 0x40,
  TELEMETRY_SAME_LINE_WRITE_WHILE_READ_BLOCKED = 0x48,
  TELEMETRY_CONFIGURED_STALL_CYCLES = 0x50,
  PROBE_SPM_WORD = 0,
  PROBE_BYTES = CGRA_DMA_BEAT_BYTES,
  PROBE_WORDS = PROBE_BYTES / sizeof(uint32_t),
  PROBE_MVIN_TAG = 0x31,
  PROBE_MVOUT_TAG = 0x32,
  POLL_LIMIT = 100000,
};

_Static_assert(DIM == GEMMINI_EXTERNAL_SPAD_MATRIX_DIMENSION,
               "the validation config must match the external-SPAD contract");
_Static_assert(PROBE_BYTES == 16,
               "the directed stability test uses one TileLink beat");
_Static_assert(GEMMINI_EXTERNAL_SPAD_FULL_WIDTH_ROW_STRIDE == 4,
               "full-width rows must use destination stride four");

static const uint32_t ACC_FULL_ADDR = (uint32_t)5 << (ADDR_LEN - 3);
static acc_t source[1][DIM] row_align_acc(1);
static uint32_t observed[PROBE_WORDS] __attribute__((aligned(16)));

static const cgra_dma_desc_t PROBE_MVIN_DESC =
    CGRA_DMA_DESC_CONST(PROBE_SPM_WORD, PROBE_BYTES, PROBE_MVIN_TAG);
static const cgra_dma_desc_t PROBE_MVOUT_DESC =
    CGRA_DMA_DESC_CONST(PROBE_SPM_WORD, PROBE_BYTES, PROBE_MVOUT_TAG);

static uint32_t read32(uintptr_t address) {
  return *(volatile uint32_t *)address;
}

static uint32_t row_bits(unsigned phase, unsigned column) {
  uint32_t value = (column & 1U) ? UINT32_C(0x89abcdef) : UINT32_C(0x12345678);
  value ^= phase * UINT32_C(0x10204081);
  value ^= column * UINT32_C(0x01010307);
  return value;
}

static acc_t acc_from_bits(uint32_t bits) {
  union {
    uint32_t bits;
    acc_t value;
  } conversion = {.bits = bits};
  return conversion.value;
}

static void load_accumulator(unsigned phase) {
  for (unsigned column = 0; column < DIM; ++column) {
    source[0][column] = acc_from_bits(row_bits(phase, column));
  }
  gemmini_extended_mvin(source, ACC_FULL_ADDR, DIM, 1);
  gemmini_fence();
}

static void issue_publication(void) {
  gemmini_extended_mvout_spad(GEMMINI_EXTERNAL_SPAD_OUTPUT_SLOT0_ROW,
                              GEMMINI_EXTERNAL_SPAD_FULL_WIDTH_ROW_STRIDE,
                              ACC_FULL_ADDR, DIM, 1);
}

static int poll_counter_above(uintptr_t address, uint32_t baseline,
                              uint32_t *observed_value) {
  for (unsigned attempt = 0; attempt < POLL_LIMIT; ++attempt) {
    const uint32_t value = read32(address);
    if (value > baseline) {
      *observed_value = value;
      return 0;
    }
  }
  *observed_value = read32(address);
  return 1;
}

int main(void) {
  const unsigned old_phase = 1;
  const unsigned new_phase = 2;
  const uintptr_t slot_base = GEMMINI_EXTERNAL_SPAD_OUTPUT_SLOT0_BASE;
  const uintptr_t blocked_cycles_address =
      TELEMETRY_BASE + TELEMETRY_READ_BACKPRESSURE_CYCLES;
  const uintptr_t same_line_write_address =
      TELEMETRY_BASE + TELEMETRY_SAME_LINE_WRITE_WHILE_READ_BLOCKED;
  int failures = 0;

  gemmini_flush(0);
  gemmini_config_ld(DIM * sizeof(acc_t));
  gemmini_config_st(DIM * sizeof(acc_t));

  load_accumulator(old_phase);
  issue_publication();
  gemmini_fence();
  load_accumulator(new_phase);

  const uint32_t configured_stall_cycles =
      read32(TELEMETRY_BASE + TELEMETRY_CONFIGURED_STALL_CYCLES);
  const uint32_t blocked_cycles_before = read32(blocked_cycles_address);
  const uint32_t same_line_writes_before = read32(same_line_write_address);
  if (configured_stall_cycles < 2) {
    printf("validation D stall is too short: %u cycles\n",
           configured_stall_cycles);
    ++failures;
  }

  for (unsigned index = 0; index < PROBE_WORDS; ++index) {
    observed[index] = UINT32_C(0x5a5aa5a5);
  }
  cgra_dma_mvin_async((const void *)slot_base, PROBE_MVIN_DESC);

  uint32_t blocked_cycles_during_read = blocked_cycles_before;
  if (poll_counter_above(blocked_cycles_address, blocked_cycles_before,
                         &blocked_cycles_during_read) != 0) {
    printf("CGRA DMA read never reached a backpressured D response\n");
    ++failures;
  }

  issue_publication();

  uint32_t same_line_writes_during_read = same_line_writes_before;
  if (poll_counter_above(same_line_write_address, same_line_writes_before,
                         &same_line_writes_during_read) != 0) {
    printf("same-line Gemmini write did not overlap the blocked D response\n");
    ++failures;
  }

  const uint8_t observed_mvin_tag = cgra_dma_wait(PROBE_MVIN_TAG);
  if (observed_mvin_tag != PROBE_MVIN_TAG) {
    printf("CGRA DMA read tag mismatch actual=%u expected=%u\n",
           observed_mvin_tag, PROBE_MVIN_TAG);
    ++failures;
  }
  cgra_dma_mvout_async(observed, PROBE_MVOUT_DESC);
  const uint8_t observed_mvout_tag = cgra_dma_wait(PROBE_MVOUT_TAG);
  if (observed_mvout_tag != PROBE_MVOUT_TAG) {
    printf("CGRA DMA observation tag mismatch actual=%u expected=%u\n",
           observed_mvout_tag, PROBE_MVOUT_TAG);
    ++failures;
  }
  cgra_dma_memory_fence();
  gemmini_fence();

  for (unsigned index = 0; index < PROBE_WORDS; ++index) {
    const uint32_t expected_old = row_bits(old_phase, index);
    const uint32_t actual_new = read32(slot_base + index * sizeof(uint32_t));
    const uint32_t expected_new = row_bits(new_phase, index);
    if (observed[index] != expected_old) {
      printf("held D payload changed word=%u actual=0x%x expected_old=0x%x\n",
             index, observed[index], expected_old);
      ++failures;
    }
    if (actual_new != expected_new) {
      printf("same-line write missing word=%u actual=0x%x expected_new=0x%x\n",
             index, actual_new, expected_new);
      ++failures;
    }
  }

  const uint32_t blocked_cycles_after = read32(blocked_cycles_address);
  const uint32_t same_line_writes_after = read32(same_line_write_address);
  const uint32_t blocked_delta = blocked_cycles_after - blocked_cycles_before;
  const uint32_t same_line_delta =
      same_line_writes_after - same_line_writes_before;
  if (blocked_delta < 2 || same_line_delta == 0) {
    printf("directed overlap evidence missing blocked_cycles=%u "
           "same_line_writes=%u\n",
           blocked_delta, same_line_delta);
    ++failures;
  }

  if (failures != 0) {
    printf("Issue #4 T2 external-SPAD held-D stability: FAIL (%d)\n", failures);
    return 1;
  }

  printf("Issue #4 T2 external-SPAD held-D stability: PASS "
         "(blocked_cycles=%u same_line_writes=%u)\n",
         blocked_delta, same_line_delta);
  return 0;
}
