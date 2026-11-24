#include "p1_plan.h"

#include <stdlib.h>

static unsigned p1_check(unsigned run) {
  unsigned errors = 0;
  for (unsigned index = 0; index < P1_WORDS; ++index) {
    const int actual = ((const volatile int8_t *)p1_output)[index];
    const int expected = block_expected_y[run][index];
    if (actual != expected) {
      if (errors < 8) {
        printf("P1 GPT2 mismatch run=%u index=%u actual=%d expected=%d\n", run, index, actual, expected);
      }
      ++errors;
    }
  }
  return errors;
}

int main(int argc, char **argv) {
  const int selected = argc > 1 ? atoi(argv[1]) : -1;
  const int warm = argc > 2 ? atoi(argv[2]) : 1;
  const int small = BLOCK_N <= P1_ROWS && BLOCK_HIDDEN == 128;
  const int timing = argc > 3 ? atoi(argv[3]) : !small;
  const int supported = ((BLOCK_N == 2 || BLOCK_N == 16 || BLOCK_N == 64) && BLOCK_HIDDEN == 128 && BLOCK_HEADS == 2 && BLOCK_FF == 512) ||
                        (BLOCK_N == 16 && BLOCK_HIDDEN == 768 && BLOCK_HEADS == 12 && BLOCK_FF == 3072);
  if (!supported || BLOCK_D != 64 || BLOCK_RUNS < 2 || selected < -1 || selected > 2 || warm < 1 || (timing != 0 && timing != 1) || (!small && !timing)) {
    printf("P1 GPT2 arguments: mode=-1(all)|0(Manual)|1(Hybrid)|2(Auto) warm_repeats>=1 timing_only=0|1 (rows/wide require 1)\n");
    return 1;
  }
  const unsigned required = P1_RETAINED + 8 * P1_WORDS + (selected < 0 || selected == 2 ? P1_TILE_BYTES : 0);
  if (GEMMINI0.spm_bytes < required) {
    printf("P1 GPT2 SPM bytes=%u required=%u\n", GEMMINI0.spm_bytes, required);
    return 1;
  }
  if (P1_CGRA_WORDS > CGRA_DMA_SPM_WORDS) {
    printf("P1 GPT2 CGRA words=%u required=%u\n", CGRA_DMA_SPM_WORDS, P1_CGRA_WORDS);
    return 1;
  }
  const char *names[] = {"Manual", "Hybrid", "Auto"};
  const unsigned first = selected < 0 ? 0 : selected;
  const unsigned end = selected < 0 ? 3 : selected + 1;
  printf("P1 GPT2 N=%u hidden=%u heads=%u FF=%u DIM=%u chunk=%u retained_base=%u retained_end=%u cgras=%u\n", BLOCK_N, BLOCK_HIDDEN, BLOCK_HEADS, BLOCK_FF, DIM, P1_CHUNK, P1_RETAINED,
         P1_RETAINED + 8 * P1_WORDS, P1_CGRAS);
  printf("P1 GPT2 Hybrid caches the whole schedule; CPU launches one segment per operator. Input staging and native input/output DMA are timed.\n");
  printf("P1 GPT2 Auto projection_tiles=2 publication_bytes=%u/%u source_stride=%u required_native_slots=2 required_cgra_slots=1\n", P1_PUBLICATION, GEMMINI0.spm_bytes - P1_TILE_BYTES,
         p1_slot_bytes());
  for (unsigned mode = first; mode < end; ++mode) {
    for (unsigned repeat = 0; repeat <= (unsigned)warm; ++repeat) {
      const int cold = repeat == 0;
      const unsigned run = repeat % 2;
      const unsigned long begin = p1_clock();
      memcpy(p1_input, block_x[run], P1_WORDS);
      cgra_dma_memory_fence();
      const unsigned long staged = p1_clock();
      int failed = 0;
      if (cold) {
        accel_commands(GEMMINI0, {
          gemmini_flush(0);
          gemmini_fence();
        });
        p1_build();
        if (mode == 2) {
          p1_pairs();
        }
        failed = p1_affine();
      }
      const unsigned long configured = p1_clock();
      if (!failed && cold && mode) {
        failed = p1_capture_all();
      }
      const unsigned long captured = p1_clock();
      if (!failed) {
        switch (mode) {
        case 0:
          failed = p1_manual();
          break;
        case 1:
          failed = p1_hybrid();
          break;
        case 2:
          failed = p1_run(0, p1.runs);
          break;
        }
      }
      cgra_dma_memory_fence();
      const unsigned long finished = p1_clock();
      const unsigned errors = !failed && !timing ? p1_check(run) : 0;
      printf("P1 GPT2 mode=%s phase=%s repeat=%u input=%u cycles=%lu checked=%u mismatches=%u status=%u captures=%u/%u/%u\n", names[mode], cold ? "cold" : "warm", repeat, run, finished - begin,
             !timing && !failed, errors, failed != 0, cold && mode ? p1.natives : 0, cold && mode ? p1_program_count() : 0, cold && mode ? p1.runs : 0);
      printf("P1 GPT2 staging_cycles=%lu setup_cycles=%lu capture_cycles=%lu execute_cycles=%lu total_cycles=%lu cached_jobs=%u/%u cached_profiles=%u reports=%u auto_launches=%u input_dma_bytes=%u "
             "output_dma_bytes=%u affine_dma_bytes=%u\n",
             staged - begin, configured - staged, captured - configured, finished - captured, finished - begin, mode ? p1.natives : 0, mode ? p1_program_count() : 0, mode ? p1.runs : 0,
             mode ? p1.reports : 0, mode == 2 ? 1 : (mode == 1 ? p1.operators : 0), P1_WORDS, P1_WORDS, cold ? p1_lanes(P1_LN_WORDS, BLOCK_HIDDEN) * 4 * BLOCK_HIDDEN * (unsigned)sizeof(acc_t) : 0);
      if (failed || errors) {
        printf("P1 GPT2: FAIL\n");
        return 1;
      }
    }
  }
  printf("P1 GPT2: PASS\n");
  return 0;
}
