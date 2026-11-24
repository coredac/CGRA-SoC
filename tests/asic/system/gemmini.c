#include "gemmini.h"
#include "accel_generated.h"
#include "asic.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef ASIC_GCN_FIXTURES
#define ASIC_GCN_FIXTURES "../../../build/experiments/asic/system/gcn.h"
#endif
#include ASIC_GCN_FIXTURES

#if DIM != 64
#error "The small ASIC GCN benchmark requires DIM64 Gemmini"
#endif

enum {
  WORDS = GCN_NODES * GCN_COLUMNS,
  EDGE_WORDS = (GCN_EDGES + 7) / 8 * 8,
  ROW_WORDS = (GCN_NODES + 8) / 8 * 8,
  LOCAL_ROWS = 0,
  LOCAL_COLUMNS = LOCAL_ROWS + ROW_WORDS,
  LOCAL_WEIGHTS = LOCAL_COLUMNS + EDGE_WORDS,
  LOCAL_FEATURES = LOCAL_WEIGHTS + EDGE_WORDS,
  LOCAL_SUM = LOCAL_FEATURES + WORDS,
  LOCAL_OUTPUT = LOCAL_SUM + WORDS,
  FULL_ROW_STRIDE = GCN_COLUMNS * sizeof(acc_t) / (DIM * sizeof(elem_t)),
};

static elem_t source[DIM * GCN_INNER] __attribute__((aligned(64)));
static const acc_t bias[DIM] __attribute__((aligned(64))) = {0};
static acc_t output[WORDS] __attribute__((aligned(64)));

static unsigned long cycles(void) {
  unsigned long value;
  __asm__ volatile("rdcycle %0" : "=r"(value) : : "memory");
  return value;
}

static int configure(int chained) {
  const asic_job_t aggregate = {
      .unit = 0, .rows = GCN_NODES, .edges = GCN_EDGES, .src0 = LOCAL_ROWS, .src1 = LOCAL_COLUMNS, .src2 = LOCAL_WEIGHTS, .src3 = LOCAL_FEATURES, .dst = LOCAL_SUM, .next_job = chained ? 2 : 0};
  const asic_job_t relu = {.unit = 1, .count = WORDS, .src0 = LOCAL_SUM, .dst = LOCAL_OUTPUT};
  accel_commands(GEMMINI0, {
    gemmini_flush(0);
    gemmini_fence();
  });
  return asic_config(ASIC0, 0, &aggregate) || asic_config(ASIC0, 1, &relu) || asic_load(ASIC0, LOCAL_ROWS, gcn_rowptr, ROW_WORDS) || asic_load(ASIC0, LOCAL_COLUMNS, gcn_col, EDGE_WORDS) ||
         asic_load(ASIC0, LOCAL_WEIGHTS, gcn_edge_weight, EDGE_WORDS);
}

static void project(unsigned publication_row) {
  enum { B_ROW = DIM, ACC_WRITE = UINT32_C(1) << (ADDR_LEN - 1), ACC_READ = ACC_WRITE | (UINT32_C(1) << (ADDR_LEN - 3)), SKIPS = (1 << 3) | (1 << 4) | (1 << 5) | (1 << 7) };
  for (unsigned column = 0; column < GCN_COLUMNS; column += DIM) {
    accel_commands(GEMMINI0, {
      gemmini_config_ex(WEIGHT_STATIONARY, NO_ACTIVATION, 0);
      gemmini_extended3_config_ld(GCN_INNER, MVIN_SCALE_IDENTITY, false, 0);
      gemmini_extended3_config_ld(GCN_COLUMNS, MVIN_SCALE_IDENTITY, false, 1);
      gemmini_extended3_config_ld(0, MVIN_SCALE_IDENTITY, false, 2);
      gemmini_config_st(DIM * sizeof(acc_t));
      gemmini_extended_mvin3(bias, ACC_WRITE, DIM, DIM);
      gemmini_extended_mvin(source, 0, GCN_INNER, DIM);
      gemmini_extended_mvin2(gcn_weight + column, B_ROW, DIM, DIM);
      gemmini_loop_ws_spad(1, 1, 1, 0, 0, 0, 0, B_ROW + DIM, 0, 0, false, false, true, false, true, NO_ACTIVATION, 0, 0, false, SKIPS);
      gemmini_extended_mvout_spad(publication_row + column * sizeof(acc_t) / (DIM * sizeof(elem_t)), FULL_ROW_STRIDE, ACC_READ, DIM, GCN_NODES);
      gemmini_fence();
    });
  }
}

static unsigned check(unsigned input) {
  unsigned errors = 0;
  for (unsigned index = 0; index < WORDS; ++index) {
    const int expected = gcn_expected[input][index];
    if (output[index] != expected) {
      if (errors < 4) {
        printf("ASIC GCN mismatch input=%u index=%u actual=%d expected=%d\n", input, index, output[index], expected);
      }
      ++errors;
    }
  }
  return errors;
}

static int run(unsigned input, int chained) {
  const unsigned publication = GEMMINI0.spm_bytes - WORDS * sizeof(acc_t);
  const unsigned publication_row = publication / (DIM * sizeof(elem_t));
  const uintptr_t projected = GEMMINI0.spm + publication;
  unsigned submissions = 0;
  memset(output, 0xa5, sizeof(output));
  asic_fence();
  const unsigned long begin = cycles();
  memcpy(source, gcn_inputs[input], sizeof(source));
  asic_fence();
  const unsigned long staged = cycles();
  int status = input == 0 ? configure(chained) : 0;
  const unsigned long configured = cycles();
  if (!status) {
    project(publication_row);
  }
  const unsigned long projected_at = cycles();
  if (!status) {
    status = asic_load(ASIC0, LOCAL_FEATURES, (const void *)projected, WORDS);
  }
  const unsigned long copied = cycles();
  for (unsigned job = 0; !status && job < (chained ? 1u : 2u); ++job) {
    status = asic_start(ASIC0, job);
    if (!status) {
      ++submissions;
      status = asic_wait(ASIC0);
    }
  }
  const unsigned long computed = cycles();
  if (!status) {
    status = asic_store(ASIC0, output, LOCAL_OUTPUT, WORDS);
  }
  asic_fence();
  const unsigned long finished = cycles();
  const unsigned errors = status ? 0 : check(input);
  printf("ASIC GCN mode=%s phase=%s input=%u total_cycles=%lu staging_cycles=%lu setup_cycles=%lu projection_cycles=%lu spm_copy_cycles=%lu asic_cycles=%lu output_cycles=%lu gemmini_tiles=2 "
         "asic_submissions=%u projected_spm_bytes=%u output_dma_bytes=%u checked=%u mismatches=%u status=%d\n",
         chained ? "chain" : "operator", input ? "warm" : "cold", input, finished - begin, staged - begin, configured - staged, projected_at - configured, copied - projected_at, computed - copied,
         finished - computed, submissions, (unsigned)(WORDS * sizeof(acc_t)), (unsigned)sizeof(output), status ? 0 : WORDS, errors, status);
  return status || errors;
}

int main(int argc, char **argv) {
  const int selected = argc > 1 ? atoi(argv[1]) : -1;
  if (selected < -1 || selected > 1) {
    printf("ASIC GCN arguments: mode=-1(both)|0(operator)|1(chain)\n");
    return 1;
  }
  printf("ASIC GCN scope=full-small-GCN nodes=%u inner=%u columns=%u reference-intermediates=0 intermediate=Gemmini-SPM-to-ASIC-DMA\n", GCN_NODES, GCN_INNER, GCN_COLUMNS);
  for (int mode = selected < 0 ? 0 : selected; mode <= (selected < 0 ? 1 : selected); ++mode) {
    for (unsigned input = 0; input < 2; ++input) {
      if (run(input, mode)) {
        printf("ASIC GCN: FAIL\n");
        return 1;
      }
    }
  }
  printf("ASIC GCN: PASS\n");
  return 0;
}
