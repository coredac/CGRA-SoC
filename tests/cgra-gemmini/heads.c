#include "block_attention.h"
#include "block_cgra.h"

#include <stdio.h>
#include <stdlib.h>

enum { WORDS = BLOCK_N * BLOCK_HIDDEN };

static int upload(const int8_t *input, int8_t *output) {
  for (unsigned start = 0; start < WORDS; start += 2048) {
    if (block_load(input + start, 0, 2048, 1) || block_store(output + start, 0, 2048)) {
      return 1;
    }
  }
  return 0;
}

static unsigned check(const int8_t *output, unsigned run) {
  unsigned errors = 0;
  for (unsigned index = 0; index < WORDS; index += sizeof(uint32_t)) {
    const uint32_t actual = ((const volatile uint32_t *)output)[index / sizeof(uint32_t)];
    uint32_t expected;
    memcpy(&expected, block_expected_concat[run] + index, sizeof(expected));
    if (actual != expected) {
      if (errors < 8) {
        printf("Heads mismatch run=%u index=%u actual=%08x expected=%08x\n", run, index, actual, expected);
      }
      ++errors;
    }
  }
  printf("Heads run=%u values=%u mismatched_words=%u\n", run, WORDS, errors);
  return errors;
}

int main(int argc, char **argv) {
  const int mode = argc > 1 ? atoi(argv[1]) : 2;
  if ((mode != 1 && mode != 2) || BLOCK_N != 64 || BLOCK_HIDDEN != 128 || BLOCK_HEADS != 2 || BLOCK_RUNS < 2) {
    printf("Heads requires mode 1/2 and N=64 H=128 heads=2 runs>=2\n");
    return 1;
  }
  // Match the full block's retained Q/K/V and INT32 PV layout exactly.
  elem_t *q = (elem_t *)(GEMMINI0.spm + 80 * 1024);
  elem_t *k = (elem_t *)(GEMMINI0.spm + 88 * 1024);
  elem_t *v = (elem_t *)(GEMMINI1.spm + 64 * 1024);
  acc_t *pv = (acc_t *)(GEMMINI1.spm + 72 * 1024);
  accel_commands(GEMMINI0, { gemmini_flush(0); });
  accel_commands(GEMMINI1, { gemmini_flush(0); });
  gemmini_fence();
  // The full block executes native CGRA kernels before entering Attention.
  static const int32_t initial[] __attribute__((aligned(64))) = {-127, 0, 127, 254};
  static int8_t quantized[4] __attribute__((aligned(64))) = {1};
  if (block_quantize(initial, quantized, 4, 127) || quantized[0] != -1 || quantized[1] != 0 || quantized[2] != 1 || quantized[3] != 2) {
    printf("Heads: FAIL native setup\n");
    return 1;
  }
  printf("Heads mode=%s N=%u H=%u heads=%u\n", mode == 1 ? "Hybrid" : "HeadAuto", BLOCK_N, BLOCK_HIDDEN, BLOCK_HEADS);
  for (unsigned run = 0; run < 2; ++run) {
    if (upload(block_expected_q[run], q) || upload(block_expected_k[run], k) || upload(block_expected_v[run], v)) {
      return 1;
    }
    const unsigned long begin = block_clock();
    const int status = block_attention(q, k, v, pv, mode);
    const unsigned long total = block_clock() - begin;
    printf("Heads run=%u setup=%lu execute=%lu total=%lu\n", run, attn_stats.setup, attn_stats.execute, total);
    // Exercise native CGRA after AutoLink, and restoration before the next input.
    if (status || block_quantize(pv, q, WORDS, BLOCK_PV_DIV) || check(q, run)) {
      printf("Heads: FAIL\n");
      return 1;
    }
  }
  printf("Heads: PASS\n");
  return 0;
}
