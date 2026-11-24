#include "accel_generated.h"
#include "asic.h"
#include "gemmini.h"
#include "gemmini_conv.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef ASIC_RESNET_FIXTURES
#define ASIC_RESNET_FIXTURES "../../../build/experiments/asic/system/resnet.h"
#endif
#include ASIC_RESNET_FIXTURES

#if DIM != 64
#error "The small ASIC ResNet benchmark requires DIM64 Gemmini"
#endif

enum {
  INPUT_WORDS = RESNET_INPUT_H * RESNET_INPUT_W * RESNET_INPUT_C,
  WORDS = RESNET_H * RESNET_W * RESNET_C,
  FIRST_WORD = 0,
  ACTIVE_WORD = WORDS,
  MAIN_WORD = 2 * WORDS,
  SKIP_WORD = 3 * WORDS,
  SUM_WORD = 4 * WORDS,
  OUTPUT_WORD = 5 * WORDS,
};

static elem_t source[INPUT_WORDS] __attribute__((aligned(64)));
static elem_t first[WORDS] __attribute__((aligned(64)));
static elem_t activated[WORDS] __attribute__((aligned(64)));
static elem_t main_branch[WORDS] __attribute__((aligned(64)));
static elem_t skip_branch[WORDS] __attribute__((aligned(64)));
static int32_t transfer[WORDS] __attribute__((aligned(64)));
static int32_t output[WORDS] __attribute__((aligned(64)));

static unsigned long cycles(void) {
  unsigned long value;
  __asm__ volatile("rdcycle %0" : "=r"(value) : : "memory");
  return value;
}

static int configure(int chained) {
  const asic_job_t relu = {.unit = 1, .count = WORDS, .src0 = FIRST_WORD, .dst = ACTIVE_WORD};
  const asic_job_t add = {.unit = 1, .operation = 1, .count = WORDS, .src0 = MAIN_WORD, .src1 = SKIP_WORD, .dst = SUM_WORD, .next_job = chained ? 3 : 0};
  const asic_job_t final = {.unit = 1, .count = WORDS, .src0 = SUM_WORD, .dst = OUTPUT_WORD};
  accel_commands(GEMMINI0, {
    gemmini_flush(0);
    gemmini_fence();
  });
  return asic_config(ASIC0, 0, &relu) || asic_config(ASIC0, 1, &add) || asic_config(ASIC0, 2, &final);
}

static void convolution(const elem_t *input, const elem_t *weights, elem_t *result, unsigned rows, unsigned columns, unsigned channels, unsigned kernel, unsigned stride, unsigned padding) {
  const gemmini_conv_t params = {.input_rows = rows,
                                 .input_columns = columns,
                                 .input_channels = channels,
                                 .output_rows = RESNET_H,
                                 .output_columns = RESNET_W,
                                 .output_channels = RESNET_C,
                                 .kernel = kernel,
                                 .stride = stride,
                                 .dilation = 1,
                                 .padding_top = padding,
                                 .padding_left = padding,
                                 .input = input,
                                 .weight = weights,
                                 .output = result,
                                 .activation = NO_ACTIVATION,
                                 .scale = ACC_SCALE_IDENTITY};
  asic_fence();
  accel_commands(GEMMINI0, {
    gemmini_conv_emit(&params);
    gemmini_fence();
  });
  asic_fence();
}

/* Only representation changes happen on the CPU; Gemmini/ASIC perform all math. */
static int load(unsigned word, const elem_t *values) {
  for (unsigned index = 0; index < WORDS; ++index) {
    transfer[index] = values[index];
  }
  return asic_load(ASIC0, word, transfer, WORDS);
}

static int execute(unsigned job, unsigned *submissions) {
  int status = asic_start(ASIC0, job);
  if (!status) {
    ++*submissions;
    status = asic_wait(ASIC0);
  }
  return status;
}

static unsigned check(unsigned input) {
  unsigned errors = 0;
  for (unsigned index = 0; index < WORDS; ++index) {
    if (output[index] != resnet_expected[input][index]) {
      if (errors < 4) {
        printf("ASIC ResNet mismatch input=%u index=%u actual=%d expected=%d\n", input, index, output[index], resnet_expected[input][index]);
      }
      ++errors;
    }
  }
  return errors;
}

static int run(unsigned input, int chained) {
  unsigned submissions = 0;
  memset(output, 0xa5, sizeof(output));
  asic_fence();
  const unsigned long begin = cycles();
  memcpy(source, resnet_inputs[input], sizeof(source));
  asic_fence();
  const unsigned long staged = cycles();
  int status = input == 0 ? configure(chained) : 0;
  const unsigned long configured = cycles();
  if (!status) {
    convolution(source, resnet_weights1, first, RESNET_INPUT_H, RESNET_INPUT_W, RESNET_INPUT_C, 3, 2, 1);
  }
  const unsigned long first_done = cycles();
  if (!status) {
    status = load(FIRST_WORD, first) || execute(0, &submissions) || asic_store(ASIC0, transfer, ACTIVE_WORD, WORDS);
  }
  if (!status) {
    for (unsigned index = 0; index < WORDS; ++index) {
      activated[index] = (elem_t)transfer[index];
    }
    asic_fence();
  }
  const unsigned long activation_done = cycles();
  if (!status) {
    convolution(activated, resnet_weights2, main_branch, RESNET_H, RESNET_W, RESNET_C, 3, 1, 1);
    convolution(source, resnet_projection, skip_branch, RESNET_INPUT_H, RESNET_INPUT_W, RESNET_INPUT_C, 1, 2, 0);
  }
  const unsigned long branches_done = cycles();
  if (!status) {
    status = load(MAIN_WORD, main_branch) || load(SKIP_WORD, skip_branch);
  }
  const unsigned long loaded = cycles();
  if (!status) {
    status = execute(1, &submissions);
  }
  if (!status && !chained) {
    status = execute(2, &submissions);
  }
  const unsigned long computed = cycles();
  if (!status) {
    status = asic_store(ASIC0, output, OUTPUT_WORD, WORDS);
  }
  asic_fence();
  const unsigned long finished = cycles();
  const unsigned errors = status ? 0 : check(input);
  printf("ASIC ResNet mode=%s phase=%s input=%u total_cycles=%lu staging_cycles=%lu setup_cycles=%lu first_conv_cycles=%lu activation_transfer_cycles=%lu branch_conv_cycles=%lu "
         "branch_transfer_cycles=%lu add_relu_cycles=%lu output_cycles=%lu gemmini_convs=3 asic_submissions=%u asic_core_starts=3 configured_jobs=%u asic_input_dma_bytes=%u "
         "asic_output_dma_bytes=%u checked=%u mismatches=%u status=%d\n",
         chained ? "chain" : "operator", input ? "warm" : "cold", input, finished - begin, staged - begin, configured - staged, first_done - configured, activation_done - first_done,
         branches_done - activation_done, loaded - branches_done, computed - loaded, finished - computed, submissions, input ? 0 : 3, 3u * WORDS * (unsigned)sizeof(int32_t),
         2u * WORDS * (unsigned)sizeof(int32_t), status ? 0 : WORDS, errors, status);
  return status || errors;
}

int main(int argc, char **argv) {
  const int selected = argc > 1 ? atoi(argv[1]) : -1;
  if (selected < -1 || selected > 1) {
    printf("ASIC ResNet arguments: mode=-1(both)|0(operator)|1(chain)\n");
    return 1;
  }
  printf("ASIC ResNet scope=full-small-projected-block input=8x8x4 output=4x4x8 reference-intermediates=0 intermediate=DRAM cpu-math=0 CPU-format-conversion=timed\n");
  for (int mode = selected < 0 ? 0 : selected; mode <= (selected < 0 ? 1 : selected); ++mode) {
    for (unsigned input = 0; input < 2; ++input) {
      if (run(input, mode)) {
        printf("ASIC ResNet: FAIL\n");
        return 1;
      }
    }
  }
  printf("ASIC ResNet: PASS\n");
  return 0;
}
