#include "cgra_dma.h"
#include "cgra_link.h"
#include "cgra_protocol.h"
#include "gemmini.h"
#include "gemmini_ext_spm.h"
#include "generated/cgra_relu4x4_fast_api.h"

#include <stdint.h>
#include <stdio.h>

enum {
  INPUT_H = 2,
  INPUT_W = 2,
  INPUT_CHANNELS = 3,
  OUTPUT_CHANNELS = 8,
  KERNEL_DIM = 3,
  PADDING = 1,
  STRIDE = 1,
  OUTPUT_H = (INPUT_H + 2 * PADDING - KERNEL_DIM) / STRIDE + 1,
  OUTPUT_W = (INPUT_W + 2 * PADDING - KERNEL_DIM) / STRIDE + 1,
  INPUT_ELEMENTS = INPUT_H * INPUT_W * INPUT_CHANNELS,
  INPUT_ROW = BANK_NUM * BANK_ROWS / 2,
  ELEMENTS = OUTPUT_H * OUTPUT_W * OUTPUT_CHANNELS,
  PACKED_BYTES = ELEMENTS * sizeof(elem_t),
  OUTPUT_BYTES = ELEMENTS * sizeof(acc_t),
  OUTPUT_TAG = 0x92,
};

static elem_t input[INPUT_H][INPUT_W][INPUT_CHANNELS] row_align(1);
static elem_t weights[KERNEL_DIM][KERNEL_DIM][INPUT_CHANNELS][OUTPUT_CHANNELS] row_align(1);
static acc_t output[ELEMENTS] __attribute__((aligned(16)));

static void init_inputs(void) {
  for (unsigned row = 0; row < INPUT_H; ++row) {
    for (unsigned column = 0; column < INPUT_W; ++column) {
      for (unsigned channel = 0; channel < INPUT_CHANNELS; ++channel) {
        input[row][column][channel] = (elem_t)((int)((row * INPUT_W * INPUT_CHANNELS + column * INPUT_CHANNELS + channel) % 5) - 2);
      }
    }
  }
  for (unsigned row = 0; row < KERNEL_DIM; ++row) {
    for (unsigned column = 0; column < KERNEL_DIM; ++column) {
      for (unsigned input_channel = 0; input_channel < INPUT_CHANNELS; ++input_channel) {
        for (unsigned output_channel = 0; output_channel < OUTPUT_CHANNELS; ++output_channel) {
          weights[row][column][input_channel][output_channel] = (elem_t)((int)((row * 4 + column * 2 + input_channel + output_channel) % 5) - 2);
        }
      }
    }
  }
  for (unsigned index = 0; index < ELEMENTS; ++index) {
    output[index] = (acc_t)0x5a5a5a5a;
  }
}

static void preload_input(void) {
  gemmini_flush(0);
  gemmini_config_ld(INPUT_ELEMENTS * sizeof(elem_t));
  gemmini_extended_mvin(input, INPUT_ROW, INPUT_ELEMENTS, 1);
  gemmini_fence();
}

static void configure_cgra(void) {
  cgra_link_configure(RELU4X4_FAST_PACKET_COUNT, 1);
  for (unsigned index = 0; index < RELU4X4_FAST_CONFIG_PACKET_COUNT; ++index) {
    cgra_link_queue(RELU4X4_FAST_CONFIG_PACKETS[index]);
  }
  for (unsigned index = 0; index < RELU4X4_FAST_LAUNCH_PACKET_COUNT; ++index) {
    cgra_link_queue(RELU4X4_FAST_LAUNCH_PACKETS[index]);
  }
}

static void run_gemmini(void) {
  const elem_t *local_input = (const elem_t *)(uintptr_t)(GEMMINI_EXT_SPM_BASE + INPUT_ROW * DIM * sizeof(elem_t));
  elem_t *publication = (elem_t *)(uintptr_t)(GEMMINI_EXT_SPM_BASE + GEMMINI_EXT_SPM_SIZE_BYTES - PACKED_BYTES);
  tiled_conv_auto(1, INPUT_H, INPUT_W, INPUT_CHANNELS, OUTPUT_CHANNELS, OUTPUT_H, OUTPUT_W, STRIDE, 1, 1, PADDING, KERNEL_DIM, false, false, false, false, false, local_input, &weights[0][0][0][0],
                  NULL, publication, NO_ACTIVATION, ACC_SCALE_IDENTITY, 0, 0, 0, WS);
}

static int verify_output(void) {
  int failures = 0;
  for (int row = 0; row < OUTPUT_H; ++row) {
    for (int column = 0; column < OUTPUT_W; ++column) {
      for (unsigned channel = 0; channel < OUTPUT_CHANNELS; ++channel) {
        int32_t sum = 0;
        for (int kernel_row = 0; kernel_row < KERNEL_DIM; ++kernel_row) {
          const int input_row = row * STRIDE + kernel_row - PADDING;
          if (input_row < 0 || input_row >= INPUT_H) {
            continue;
          }
          for (int kernel_column = 0; kernel_column < KERNEL_DIM; ++kernel_column) {
            const int input_column = column * STRIDE + kernel_column - PADDING;
            if (input_column < 0 || input_column >= INPUT_W) {
              continue;
            }
            for (unsigned input_channel = 0; input_channel < INPUT_CHANNELS; ++input_channel) {
              sum += (int32_t)input[input_row][input_column][input_channel] * (int32_t)weights[kernel_row][kernel_column][input_channel][channel];
            }
          }
        }
        const unsigned index = (row * OUTPUT_W + column) * OUTPUT_CHANNELS + channel;
        const int32_t expected = sum > 0 ? sum : 0;
        if (output[index] != expected) {
          printf("Conv + CGRA ReLU mismatch row=%d column=%d channel=%u actual=%d expected=%d\n", row, column, channel, (int)output[index], (int)expected);
          ++failures;
        }
      }
    }
  }
  return failures;
}

static int run_pipeline(void) {
  const cgra_link_result_t result = cgra_link_wait();
  if (result.status != AUTO_LINK_STATUS_SUCCESS || result.detail != 0 || result.data != 0) {
    printf("AutoLink result mismatch status=%u detail=%u data=%u\n", (unsigned)result.status, (unsigned)result.detail, (unsigned)result.data);
    return 1;
  }
  cgra_dma_mvout_async(output, CGRA_DMA_DESC_CONST(0, OUTPUT_BYTES, OUTPUT_TAG));
  if (cgra_dma_wait(OUTPUT_TAG) != OUTPUT_TAG) {
    printf("CGRA output DMA tag mismatch\n");
    return 1;
  }
  cgra_dma_memory_fence();
  return verify_output();
}

int main(void) {
  init_inputs();
  preload_input();
  configure_cgra();
  run_gemmini();
  const int failures = run_pipeline();
  if (failures != 0) {
    printf("Native Gemmini Conv + CGRA Auto packed INT8 ReLU: FAIL (%d)\n", failures);
    return 1;
  }
  printf("Native Gemmini Conv + CGRA Auto packed INT8 ReLU: PASS\n");
  return 0;
}
