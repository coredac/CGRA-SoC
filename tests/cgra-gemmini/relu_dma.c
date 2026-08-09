#include "cgra_dma.h"
#include "cgra_protocol.h"
#include "cgra_runtime.h"
#include "generated/cgra_relu4x4_fast_api.h"
#include "gemmini.h"

#include <stdint.h>
#include <stdio.h>

#ifndef ACC_READ_FULL_WIDTH
#error "This demo requires Gemmini full-width accumulator reads"
#endif

_Static_assert(DIM == 16, "the demo requires a 16x16 Gemmini array");
_Static_assert(sizeof(elem_t) == 1, "Gemmini elements must be int8");
_Static_assert(sizeof(acc_t) == 4, "Gemmini accumulators must be int32");
_Static_assert(CGRA_DATA_PAYLOAD_NBITS == 32, "CGRA words must be 32 bits");
_Static_assert(CGRA_DMA_DRAM_DATA_NBITS == 128,
               "CGRA DMA beats must be 128 bits");

enum {
  MATRIX_ELEMENT_COUNT = DIM * DIM,
  CGRA_CHUNK_ELEMENTS = 32,
  CGRA_CHUNK_BYTES = CGRA_CHUNK_ELEMENTS * sizeof(acc_t),
  CGRA_SPM_WORD_ADDR = 0,
  CGRA_EXPECTED_COMPLETES = 1,
  CGRA_EXPECTED_RESULT = 0,
};

_Static_assert(CGRA_CHUNK_ELEMENTS <= MATRIX_ELEMENT_COUNT,
               "the chunk 0 smoke test must fit in the matrix");
_Static_assert(CGRA_CHUNK_ELEMENTS % 2 == 0,
               "the smoke-test input must split evenly around zero");
_Static_assert(CGRA_CHUNK_BYTES == 128,
               "each CGRA chunk must contain exactly 128 bytes");
_Static_assert(CGRA_CHUNK_BYTES % CGRA_DMA_BEAT_BYTES == 0,
               "each CGRA chunk must contain complete DMA beats");

static elem_t A[DIM][DIM] row_align(1);
static elem_t B[DIM][DIM] row_align(1);
static acc_t gemmini_output[DIM][DIM] row_align_acc(1);
static acc_t cgra_output[DIM][DIM] row_align_acc(1);

enum {
  MVIN_TAG_0 = 0x10,
  MVOUT_TAG_0 = 0x80,
};

static const cgra_dma_desc_t MVIN_DESCRIPTOR =
    CGRA_DMA_DESC_CONST(CGRA_SPM_WORD_ADDR, CGRA_CHUNK_BYTES, MVIN_TAG_0);

static const cgra_dma_desc_t MVOUT_DESCRIPTOR =
    CGRA_DMA_DESC_CONST(CGRA_SPM_WORD_ADDR, CGRA_CHUNK_BYTES, MVOUT_TAG_0);

static void init_inputs(void) {
  for (int i = 0; i < DIM; ++i) {
    for (int j = 0; j < DIM; ++j) {
      const int index = i * DIM + j;
      A[i][j] = (elem_t)(index % CGRA_CHUNK_ELEMENTS -
                         CGRA_CHUNK_ELEMENTS / 2);
      B[i][j] = i == j ? (elem_t)1 : (elem_t)0;
      cgra_output[i][j] = (acc_t)0x5a5a5a5a;
    }
  }
}

int main(void) {
  static const uint32_t GEMMINI_ACCUMULATOR_ADDR_BIT =
      UINT32_C(1) << (ADDR_LEN - 1);
  static const uint32_t GEMMINI_READ_FULL_ACC_ROW_BIT =
      UINT32_C(1) << (ADDR_LEN - 3);
  const uint32_t A_addr = 0;
  const uint32_t B_addr = DIM;
  const uint32_t accumulator_write_addr = GEMMINI_ACCUMULATOR_ADDR_BIT;
  const uint32_t full_width_mvout_addr =
      GEMMINI_ACCUMULATOR_ADDR_BIT | GEMMINI_READ_FULL_ACC_ROW_BIT;

  init_inputs();

  gemmini_flush(0);
  gemmini_config_ld(DIM * sizeof(elem_t));
  gemmini_config_ex(WEIGHT_STATIONARY, NO_ACTIVATION, 0);
  gemmini_config_st(DIM * sizeof(acc_t));

  gemmini_mvin(A, A_addr);
  gemmini_mvin(B, B_addr);
  gemmini_preload(B_addr, accumulator_write_addr);
  gemmini_compute_preloaded(A_addr, GARBAGE_ADDR);
  gemmini_mvout(gemmini_output, full_width_mvout_addr);
  gemmini_fence();

  /* Chunk 0 is the first 32 int32 elements (128 bytes) of Gemmini's output. */
  acc_t *gemmini_words = &gemmini_output[0][0];
  acc_t *cgra_words = &cgra_output[0][0];
  uint64_t wait_result = 0;
  uint64_t status = 0;
  uint64_t result = 0;

  cgra_dma_mvin_async(gemmini_words, MVIN_DESCRIPTOR);
  CGRA_SET_EXPECTED_COMPLETES(CGRA_EXPECTED_COMPLETES);
  load_relu4x4_config_fast();

  uint8_t observed_mvin_tag = cgra_dma_wait(MVIN_TAG_0);
  if (observed_mvin_tag != MVIN_TAG_0) {
    printf("chunk 0 CGRA DMA MVIN tag mismatch: expected=%u observed=%u\n",
           MVIN_TAG_0, observed_mvin_tag);
    return 1;
  }

  launch_relu4x4_fast();
  CGRA_WAIT(wait_result);
  CGRA_STATUS(status);
  CGRA_RESULT(result);

  const uint64_t complete = status & UINT64_C(1);
  const uint64_t complete_count = (status >> 1) & UINT64_C(0xffff);
  if (wait_result != 1 || complete != 1 ||
      complete_count != CGRA_EXPECTED_COMPLETES ||
      result != CGRA_EXPECTED_RESULT) {
    printf("chunk 0 CGRA completion failure: wait=%lu status=0x%lx "
           "result=%lu\n", wait_result, status, result);
    return 1;
  }

  cgra_dma_mvout_async(cgra_words, MVOUT_DESCRIPTOR);
  uint8_t observed_mvout_tag = cgra_dma_wait(MVOUT_TAG_0);
  if (observed_mvout_tag != MVOUT_TAG_0) {
    printf("chunk 0 CGRA DMA MVOUT tag mismatch: expected=%u observed=%u\n",
           MVOUT_TAG_0, observed_mvout_tag);
    return 1;
  }

  cgra_dma_memory_fence();

  int gemmini_failures = 0;
  int cgra_failures = 0;

  for (int index = 0; index < CGRA_CHUNK_ELEMENTS; ++index) {
    const int row = index / DIM;
    const int column = index % DIM;
    const acc_t expected_gemmini = (acc_t)A[row][column];
    const acc_t expected_cgra =
        expected_gemmini > 0 ? expected_gemmini : 0;

    if (gemmini_words[index] != expected_gemmini) {
      printf("Gemmini mismatch [%d][%d]: actual=%d expected=%d\n",
             row, column, (int)gemmini_words[index],
             (int)expected_gemmini);
      ++gemmini_failures;
    }
    if (cgra_words[index] != expected_cgra) {
      printf("CGRA mismatch [%d][%d]: actual=%d expected=%d\n",
             row, column, (int)cgra_words[index],
             (int)expected_cgra);
      ++cgra_failures;
    }
  }

  if (gemmini_failures != 0 || cgra_failures != 0) {
    printf("Gemmini + CGRA DMA ReLU chunk 0: FAIL gemmini=%d cgra=%d\n",
           gemmini_failures, cgra_failures);
    return 1;
  }

  printf("Gemmini + CGRA DMA ReLU chunk 0 smoke test: PASS\n");
  return 0;
}
