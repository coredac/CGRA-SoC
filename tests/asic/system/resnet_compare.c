#include "accel_generated.h"
#include "asic.h"
#include "cgra_dma.h"
#include "gemmini.h"
#include "gemmini_conv.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef ASIC_RESNET_FIXTURES
#define ASIC_RESNET_FIXTURES "../../../build/experiments/asic/system/resnet.h"
#endif
#include ASIC_RESNET_FIXTURES
#include "../../../build/experiments/asic/system/cgra_add_relu_runtime_fast_api.h"
#include "../../../build/experiments/asic/system/cgra_relu_runtime_fast_api.h"

#if DIM != 64
#error "The small ResNet comparison requires DIM64 Gemmini"
#endif

enum {
  INPUT_WORDS = RESNET_INPUT_H * RESNET_INPUT_W * RESNET_INPUT_C,
  WORDS = RESNET_H * RESNET_W * RESNET_C,
  FIRST_WORD = 0,
  ACTIVE_WORD = WORDS,
  MAIN_WORD = 2 * WORDS,
  SKIP_WORD = 3 * WORDS,
  OUTPUT_WORD = 5 * WORDS,
};

typedef struct {
  const char *name;
  accel_t device;
  int dedicated;
} backend_t;

static const backend_t backends[] = {{"CGRA", CGRA0, 0}, {"ASIC", ASIC0, 1}};
static cgra_packet_t relu_packets[RELU_RUNTIME_FAST_CONFIG_PACKET_COUNT];
static cgra_packet_t add_packets[ADD_RELU_RUNTIME_FAST_CONFIG_PACKET_COUNT];
static cgra_kernel_t kernels[2];
static int cgra_started;
static unsigned dma_bytes[2];

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

static cgra_kernel_t resolve(const cgra_kernel_t *original, const uint32_t *args, cgra_packet_t *packets) {
  cgra_kernel_t kernel = *original;
  memcpy(packets, kernel.config_packets, kernel.config_count * sizeof(*packets));
  for (unsigned index = 0; index < kernel.patch_count; ++index) {
    const cgra_patch_t *patch = &kernel.patches[index];
    const uint32_t value = args[patch->symbol_index] * patch->scale + patch->offset;
    cgra_packet_t *packet = packets + patch->packet_index;
    packet->hi = (packet->hi & ~RELU_RUNTIME_STORE_DATA_PAYLOAD_HI(UINT32_MAX)) | RELU_RUNTIME_STORE_DATA_PAYLOAD_HI(value);
  }
  kernel.config_packets = packets;
  return kernel;
}

static int configure(const backend_t *backend) {
  const asic_job_t relu = {.unit = 1, .count = WORDS, .src0 = FIRST_WORD, .dst = ACTIVE_WORD};
  const asic_job_t add = {.unit = 1, .operation = 2, .count = WORDS, .src0 = MAIN_WORD, .src1 = SKIP_WORD, .dst = OUTPUT_WORD};
  accel_commands(GEMMINI0, {
    gemmini_flush(0);
    gemmini_fence();
  });
  if (backend->dedicated) {
    return asic_config(backend->device, 0, &relu) || asic_config(backend->device, 1, &add);
  }
  const uint32_t relu_args[] = {FIRST_WORD, ACTIVE_WORD, WORDS};
  const uint32_t add_args[] = {MAIN_WORD, SKIP_WORD, OUTPUT_WORD, WORDS};
  kernels[0] = resolve(&RELU_RUNTIME, relu_args, relu_packets);
  kernels[1] = resolve(&ADD_RELU_RUNTIME, add_args, add_packets);
  uint64_t ready;
  accel_commands(backend->device, {
    cgra_send_packets_fast(kernels[0].static_packets, kernels[0].static_count);
    cgra_send_packets_fast(kernels[1].static_packets, kernels[1].static_count);
    CGRA_WAIT(ready);
  });
  return ready != 1;
}

static int copy(const backend_t *backend, unsigned word, int32_t *values, int store) {
  if (backend->dedicated) {
    const int status = store ? asic_store(backend->device, values, word, WORDS) : asic_load(backend->device, word, values, WORDS);
    if (!status) {
      dma_bytes[store] += WORDS * sizeof(int32_t);
    }
    return status;
  }
  const cgra_dma_desc_t descriptor = ((uint64_t)word << CGRA_DMA_DESC_SPM_ADDR_LSB) | ((uint64_t)WORDS * sizeof(int32_t) << CGRA_DMA_DESC_NBYTES_LSB);
  uint8_t tag;
  cgra_dma_memory_fence();
  accel_commands(backend->device, {
    if (store) {
      cgra_dma_mvout_async(values, descriptor);
    } else {
      cgra_dma_mvin_async(values, descriptor);
    }
    tag = cgra_dma_wait(0);
  });
  cgra_dma_memory_fence();
  if (!tag) {
    dma_bytes[store] += WORDS * sizeof(int32_t);
  }
  return tag != 0;
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
static int load(const backend_t *backend, unsigned word, const elem_t *values) {
  for (unsigned index = 0; index < WORDS; ++index) {
    transfer[index] = values[index];
  }
  return copy(backend, word, transfer, 0);
}

static int execute(const backend_t *backend, unsigned job, unsigned *submissions) {
  if (backend->dedicated) {
    int status = asic_start(backend->device, job);
    if (!status) {
      ++*submissions;
      status = asic_wait(backend->device);
    }
    return status;
  }
  const cgra_kernel_t *kernel = kernels + job;
  uint64_t ready, status, result;
  accel_commands(backend->device, {
    cgra_prepare(kernel, cgra_started ? CGRA_SWITCH : CGRA_COLD);
    cgra_start(kernel);
    ++*submissions;
    CGRA_WAIT(ready);
    CGRA_STATUS(status);
    CGRA_RESULT(result);
  });
  cgra_started = 1;
  if (ready != 1 || !(status & 1) || ((status >> 1) & 0xffff) != kernel->expected_completes || result != 0) {
    printf("ResNet COMPARE CGRA job=%u ready=%lu status=%lu result=%lu\n", job, ready, status, result);
    return 1;
  }
  return 0;
}

static unsigned check(unsigned input) {
  unsigned errors = 0;
  for (unsigned index = 0; index < WORDS; ++index) {
    if (output[index] != resnet_expected[input][index]) {
      if (errors < 4) {
        printf("ResNet COMPARE mismatch input=%u index=%u actual=%d expected=%d\n", input, index, output[index], resnet_expected[input][index]);
      }
      ++errors;
    }
  }
  return errors;
}

static int run(const backend_t *backend, unsigned input) {
  unsigned submissions = 0;
  memset(dma_bytes, 0, sizeof(dma_bytes));
  memset(output, 0xa5, sizeof(output));
  asic_fence();
  const unsigned long begin = cycles();
  memcpy(source, resnet_inputs[input], sizeof(source));
  asic_fence();
  const unsigned long staged = cycles();
  int status = input == 0 ? configure(backend) : 0;
  const unsigned long configured = cycles();
  if (!status) {
    convolution(source, resnet_weights1, first, RESNET_INPUT_H, RESNET_INPUT_W, RESNET_INPUT_C, 3, 2, 1);
  }
  const unsigned long first_done = cycles();
  if (!status) {
    status = load(backend, FIRST_WORD, first) || execute(backend, 0, &submissions) || copy(backend, ACTIVE_WORD, transfer, 1);
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
    status = load(backend, MAIN_WORD, main_branch) || load(backend, SKIP_WORD, skip_branch);
  }
  const unsigned long loaded = cycles();
  if (!status) {
    status = execute(backend, 1, &submissions);
  }
  const unsigned long computed = cycles();
  if (!status) {
    status = copy(backend, OUTPUT_WORD, output, 1);
  }
  asic_fence();
  const unsigned long finished = cycles();
  const unsigned errors = status ? 0 : check(input);
  printf("ResNet COMPARE backend=%s phase=%s input=%u total_cycles=%lu staging_cycles=%lu setup_cycles=%lu first_conv_cycles=%lu activation_transfer_cycles=%lu branch_conv_cycles=%lu "
         "branch_transfer_cycles=%lu add_relu_cycles=%lu output_cycles=%lu gemmini_convs=3 cpu_submissions=%u native_kernel_starts=%u configured_programs=%u input_dma_bytes=%u "
         "output_dma_bytes=%u checked=%u mismatches=%u status=%d\n",
         backend->name, input ? "warm" : "cold", input, finished - begin, staged - begin, configured - staged, first_done - configured, activation_done - first_done, branches_done - activation_done,
         loaded - branches_done, computed - loaded, finished - computed, submissions, submissions, input ? 0 : 2, dma_bytes[0], dma_bytes[1], status ? 0 : WORDS, errors, status);
  return status || errors;
}

int main(int argc, char **argv) {
  const int selected = argc > 1 ? atoi(argv[1]) : -1;
  if (selected < -1 || selected > 1) {
    printf("ResNet COMPARE arguments: backend=-1(both)|0(CGRA)|1(ASIC)\n");
    return 1;
  }
  printf("ResNet COMPARE scope=full-small-projected-block input=8x8x4 output=4x4x8 reference-intermediates=0 intermediate=DRAM cpu-math=0 CPU-format-conversion=timed "
         "common-Gemmini=1 common-layout=1 common-raw32-DMA=1 fused-AddReLU=1 root-submissions=0 cold=configuration\n");
  for (int index = selected < 0 ? 0 : selected; index <= (selected < 0 ? 1 : selected); ++index) {
    for (unsigned input = 0; input < 2; ++input) {
      if (run(backends + index, input)) {
        printf("ResNet COMPARE: FAIL\n");
        return 1;
      }
    }
  }
  printf("ResNet COMPARE: PASS\n");
  return 0;
}
