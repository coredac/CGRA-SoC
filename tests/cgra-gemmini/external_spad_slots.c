#include "cgra_dma.h"
// clang-format off
#include "gemmini.h"
// clang-format on
#include "gemmini_external_spad.h"

#include <stdint.h>
#include <stdio.h>

#ifndef ACC_READ_FULL_WIDTH
#error "Issue #4 T2 requires full-width accumulator reads"
#endif

_Static_assert(DIM == GEMMINI_EXTERNAL_SPAD_MATRIX_DIMENSION,
               "the slot contract must match Gemmini DIM");
_Static_assert(sizeof(elem_t) == 1, "Gemmini elements must be int8");
_Static_assert(sizeof(acc_t) == 4, "Gemmini accumulators must be int32");
_Static_assert(GEMMINI_EXTERNAL_SPAD_SIZE_BYTES == 64 * 1024,
               "the production external SPAD must be 64 KiB");
_Static_assert(GEMMINI_EXTERNAL_SPAD_ROW_BYTES == DIM * sizeof(elem_t),
               "external-SPAD row units must match Gemmini SPAD rows");
_Static_assert(GEMMINI_EXTERNAL_SPAD_FULL_WIDTH_ROW_BYTES ==
                   DIM * sizeof(acc_t),
               "full-width rows must retain every int32 accumulator value");
_Static_assert(GEMMINI_EXTERNAL_SPAD_FULL_WIDTH_ROW_STRIDE == 4,
               "full-width publication requires four SPAD rows per acc row");
_Static_assert(GEMMINI_EXTERNAL_SPAD_OUTPUT_SLOT_COUNT == 2,
               "the production contract reserves two output slots");
_Static_assert(GEMMINI_EXTERNAL_SPAD_OUTPUT_SLOT_SIZE_BYTES ==
                   DIM * DIM * sizeof(acc_t),
               "each output slot must hold one complete matrix");
_Static_assert(GEMMINI_EXTERNAL_SPAD_OUTPUT_RESERVED_BYTES == 2 * 1024,
               "the top 2 KiB must be reserved for publication");
_Static_assert(GEMMINI_EXTERNAL_SPAD_OUTPUT_SLOT0_BASE == UINT64_C(0x6000f800),
               "slot 0 physical address changed");
_Static_assert(GEMMINI_EXTERNAL_SPAD_OUTPUT_SLOT1_BASE == UINT64_C(0x6000fc00),
               "slot 1 physical address changed");
_Static_assert(GEMMINI_EXTERNAL_SPAD_OUTPUT_SLOT0_ROW == 0x0f80,
               "slot 0 Gemmini row changed");
_Static_assert(GEMMINI_EXTERNAL_SPAD_OUTPUT_SLOT1_ROW == 0x0fc0,
               "slot 1 Gemmini row changed");
_Static_assert(GEMMINI_EXTERNAL_SPAD_OUTPUT_SLOT0_BASE +
                       GEMMINI_EXTERNAL_SPAD_OUTPUT_SLOT_SIZE_BYTES ==
                   GEMMINI_EXTERNAL_SPAD_OUTPUT_SLOT1_BASE,
               "publication slots must be contiguous and non-overlapping");
_Static_assert(GEMMINI_EXTERNAL_SPAD_OUTPUT_SLOT1_BASE +
                       GEMMINI_EXTERNAL_SPAD_OUTPUT_SLOT_SIZE_BYTES ==
                   GEMMINI_EXTERNAL_SPAD_BASE +
                       GEMMINI_EXTERNAL_SPAD_SIZE_BYTES,
               "publication slots must occupy the top of the external SPAD");

enum {
  PROBE_SPM_WORD = 0,
  PROBE_BYTES = CGRA_DMA_BEAT_BYTES,
  PROBE_WORDS = PROBE_BYTES / sizeof(uint32_t),
  PROBE_MVIN_TAG = 0x24,
  PROBE_MVOUT_TAG = 0x42,
  GUARD_ROW = GEMMINI_EXTERNAL_SPAD_OUTPUT_SLOT0_ROW -
              GEMMINI_EXTERNAL_SPAD_FULL_WIDTH_ROW_STRIDE,
};

_Static_assert(PROBE_BYTES == 16,
               "the focused CGRA reachability probe uses one TL beat");
_Static_assert(GUARD_ROW >= 0,
               "a guard row must fit below the reserved publication region");

static const uint32_t ACC_FULL_ADDR = (uint32_t)5 << (ADDR_LEN - 3);
static acc_t source[DIM][DIM] row_align_acc(1);
static uint32_t cgra_probe[PROBE_WORDS] __attribute__((aligned(16)));

static const cgra_dma_desc_t PROBE_MVIN_DESC =
    CGRA_DMA_DESC_CONST(PROBE_SPM_WORD, PROBE_BYTES, PROBE_MVIN_TAG);
static const cgra_dma_desc_t PROBE_MVOUT_DESC =
    CGRA_DMA_DESC_CONST(PROBE_SPM_WORD, PROBE_BYTES, PROBE_MVOUT_TAG);

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

static void publish(unsigned phase, uint32_t destination_row, unsigned rows) {
  for (unsigned row = 0; row < rows; ++row) {
    for (unsigned column = 0; column < DIM; ++column) {
      source[row][column] = acc_from_bits(data_bits(phase, row, column));
    }
  }

  gemmini_extended_mvin(source, ACC_FULL_ADDR, DIM, rows);
  gemmini_fence();
  gemmini_extended_mvout_spad(destination_row,
                              GEMMINI_EXTERNAL_SPAD_FULL_WIDTH_ROW_STRIDE,
                              ACC_FULL_ADDR, DIM, rows);
  gemmini_fence();
}

static int verify_word(uintptr_t address, uint32_t expected, const char *region,
                       unsigned row, unsigned column) {
  int failures = 0;
  volatile uint8_t *actual = (volatile uint8_t *)address;

  for (unsigned byte = 0; byte < sizeof(expected); ++byte) {
    const uint8_t expected_byte = (uint8_t)(expected >> (byte * 8));
    if (actual[byte] != expected_byte) {
      printf("%s mismatch row=%u column=%u byte=%u actual=0x%x expected=0x%x\n",
             region, row, column, byte, actual[byte], expected_byte);
      ++failures;
    }
  }
  return failures;
}

static int verify_matrix(uintptr_t base, unsigned phase, const char *region) {
  int failures = 0;

  for (unsigned row = 0; row < DIM; ++row) {
    for (unsigned column = 0; column < DIM; ++column) {
      failures +=
          verify_word(base + row * GEMMINI_EXTERNAL_SPAD_FULL_WIDTH_ROW_BYTES +
                          column * sizeof(acc_t),
                      data_bits(phase, row, column), region, row, column);
    }
  }
  return failures;
}

static int verify_guard(unsigned phase) {
  int failures = 0;
  const uintptr_t base = GEMMINI_EXTERNAL_SPAD_BASE +
                         (uintptr_t)GUARD_ROW * GEMMINI_EXTERNAL_SPAD_ROW_BYTES;

  for (unsigned column = 0; column < DIM; ++column) {
    failures += verify_word(base + column * sizeof(acc_t),
                            data_bits(phase, 0, column), "guard", 0, column);
  }
  return failures;
}

static int verify_cgra_dma_read_reachability(unsigned phase) {
  for (unsigned index = 0; index < PROBE_WORDS; ++index) {
    cgra_probe[index] = UINT32_C(0x5a5aa5a5);
  }

  cgra_dma_mvin_async(
      (const void *)(uintptr_t)GEMMINI_EXTERNAL_SPAD_OUTPUT_SLOT0_BASE,
      PROBE_MVIN_DESC);
  const uint8_t observed_mvin_tag = cgra_dma_wait(PROBE_MVIN_TAG);
  if (observed_mvin_tag != PROBE_MVIN_TAG) {
    printf("CGRA DMA external-SPAD read tag mismatch actual=%u expected=%u\n",
           observed_mvin_tag, PROBE_MVIN_TAG);
    return 1;
  }

  cgra_dma_mvout_async(cgra_probe, PROBE_MVOUT_DESC);
  const uint8_t observed_mvout_tag = cgra_dma_wait(PROBE_MVOUT_TAG);
  if (observed_mvout_tag != PROBE_MVOUT_TAG) {
    printf("CGRA DMA probe observation tag mismatch actual=%u expected=%u\n",
           observed_mvout_tag, PROBE_MVOUT_TAG);
    return 1;
  }
  cgra_dma_memory_fence();

  int failures = 0;
  for (unsigned index = 0; index < PROBE_WORDS; ++index) {
    const uint32_t expected = data_bits(phase, 0, index);
    if (cgra_probe[index] != expected) {
      printf("CGRA DMA external-SPAD read mismatch word=%u actual=0x%x "
             "expected=0x%x\n",
             index, cgra_probe[index], expected);
      ++failures;
    }
  }
  return failures;
}

int main(void) {
  const unsigned guard_phase = 9;
  const unsigned slot0_phase = 1;
  const unsigned slot1_sentinel_phase = 7;
  const unsigned slot1_final_phase = 2;
  int failures = 0;

  gemmini_flush(0);
  gemmini_config_ld(DIM * sizeof(acc_t));
  gemmini_config_st(DIM * sizeof(acc_t));

  publish(guard_phase, GUARD_ROW, 1);
  publish(slot1_sentinel_phase, GEMMINI_EXTERNAL_SPAD_OUTPUT_SLOT1_ROW, DIM);
  publish(slot0_phase, GEMMINI_EXTERNAL_SPAD_OUTPUT_SLOT0_ROW, DIM);

  failures += verify_guard(guard_phase);
  failures += verify_matrix(GEMMINI_EXTERNAL_SPAD_OUTPUT_SLOT0_BASE,
                            slot0_phase, "slot0");
  failures += verify_matrix(GEMMINI_EXTERNAL_SPAD_OUTPUT_SLOT1_BASE,
                            slot1_sentinel_phase, "slot1-sentinel");
  failures += verify_cgra_dma_read_reachability(slot0_phase);

  publish(slot1_final_phase, GEMMINI_EXTERNAL_SPAD_OUTPUT_SLOT1_ROW, DIM);
  failures += verify_guard(guard_phase);
  failures += verify_matrix(GEMMINI_EXTERNAL_SPAD_OUTPUT_SLOT0_BASE,
                            slot0_phase, "slot0-after-slot1");
  failures += verify_matrix(GEMMINI_EXTERNAL_SPAD_OUTPUT_SLOT1_BASE,
                            slot1_final_phase, "slot1-final");

  if (failures != 0) {
    printf("Issue #4 T2 production external-SPAD slots: FAIL (%d mismatches)\n",
           failures);
    return 1;
  }

  printf(
      "Issue #4 T2 production external-SPAD slots and CGRA DMA reachability: "
      "PASS\n");
  return 0;
}
