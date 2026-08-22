#include "cgra_dma.h"
#include "cgra_protocol.h"
#include "cgra_transfer_control.h"
#include "gemmini.h"
#include "gemmini_external_spad.h"
#include "generated/cgra_relu4x4_fast_api.h"

#include <stdint.h>
#include <stdio.h>

enum {
  CGRA_WORD_COUNT = 32,
  CGRA_TRANSFER_BYTES = CGRA_WORD_COUNT * sizeof(acc_t),
  PUBLICATION_ROWS =
      CGRA_TRANSFER_BYTES / GEMMINI_EXTERNAL_SPAD_FULL_WIDTH_ROW_BYTES,
  CGRA_SPM_WORD_ADDR = 0,
  CGRA_EXPECTED_COMPLETES = 1,
  JOB_ID = 0x701,
  DMA_TAG = 0x71,
  OUTPUT_DMA_TAG = 0x91,
};

static elem_t A[DIM][DIM] row_align(1);
static elem_t B[DIM][DIM] row_align(1);
static acc_t cgra_output[CGRA_WORD_COUNT] __attribute__((aligned(16)));

static const cgra_dma_desc_t OUTPUT_DESCRIPTOR = CGRA_DMA_DESC_CONST(
    CGRA_SPM_WORD_ADDR, CGRA_TRANSFER_BYTES, OUTPUT_DMA_TAG);

static const uint32_t ACCUMULATOR_WRITE_ADDRESS = (uint32_t)1 << (ADDR_LEN - 1);
static const uint32_t ACCUMULATOR_FULL_WIDTH_ADDRESS =
    ((uint32_t)1 << (ADDR_LEN - 1)) | ((uint32_t)1 << (ADDR_LEN - 3));

static void init_inputs(void) {
  for (int i = 0; i < DIM; ++i) {
    for (int j = 0; j < DIM; ++j) {
      const int index = i * DIM + j;
      A[i][j] = (elem_t)(index % CGRA_WORD_COUNT - CGRA_WORD_COUNT / 2);
      B[i][j] = i == j ? (elem_t)1 : (elem_t)0;
    }
  }
  for (unsigned index = 0; index < CGRA_WORD_COUNT; ++index) {
    cgra_output[index] = (acc_t)0x5a5a5a5a;
  }
}

static void run_gemmini_gemm(void) {
  const uint32_t A_addr = 0;
  const uint32_t B_addr = DIM;

  gemmini_flush(0);
  gemmini_config_ld(DIM * sizeof(elem_t));
  gemmini_config_ex(WEIGHT_STATIONARY, NO_ACTIVATION, 0);
  gemmini_config_st(DIM * sizeof(acc_t));
  gemmini_mvin(A, A_addr);
  gemmini_mvin(B, B_addr);
  gemmini_preload(B_addr, ACCUMULATOR_WRITE_ADDRESS);
  gemmini_compute_preloaded(A_addr, GARBAGE_ADDR);
  gemmini_fence();
}

static void submit_launch_sequence(cgra_transfer_launch_header_t header) {
  cgra_transfer_submit_launch_header(header);
  for (unsigned index = 0; index < RELU4X4_FAST_LAUNCH_PACKET_COUNT; ++index) {
    cgra_transfer_submit_launch_packet(RELU4X4_FAST_LAUNCH_PACKETS[index]);
  }
}

static int
verify_transfer_results(cgra_transfer_launch_result_t launch_result,
                        cgra_transfer_compute_result_t compute_result) {
  if (launch_result.job_id != JOB_ID || launch_result.slot != 0 ||
      launch_result.requested_bytes != CGRA_TRANSFER_BYTES ||
      launch_result.actual_bytes != CGRA_TRANSFER_BYTES ||
      launch_result.spm_word_address != CGRA_SPM_WORD_ADDR ||
      launch_result.dma_tag != DMA_TAG ||
      launch_result.packet_count != RELU4X4_FAST_LAUNCH_PACKET_COUNT ||
      launch_result.status != CGRA_TRANSFER_LAUNCH_STATUS_LAUNCH_ACCEPTED) {
    printf("CGRA launch result mismatch\n");
    return 1;
  }
  if (compute_result.job_id != JOB_ID || compute_result.slot != 0 ||
      compute_result.requested_bytes != CGRA_TRANSFER_BYTES ||
      compute_result.actual_bytes != CGRA_TRANSFER_BYTES ||
      compute_result.spm_word_address != CGRA_SPM_WORD_ADDR ||
      compute_result.dma_tag != DMA_TAG ||
      compute_result.packet_count != RELU4X4_FAST_LAUNCH_PACKET_COUNT ||
      compute_result.complete_data != 0 ||
      compute_result.status != CGRA_TRANSFER_COMPUTE_STATUS_SUCCESS) {
    printf("CGRA compute result mismatch\n");
    return 1;
  }
  return 0;
}

static int verify_cgra_relu_outputs(void) {
  int failures = 0;
  for (unsigned index = 0; index < CGRA_WORD_COUNT; ++index) {
    const int input = (int)A[index / DIM][index % DIM];
    const acc_t expected = input > 0 ? (acc_t)input : (acc_t)0;
    if (cgra_output[index] != expected) {
      printf("CGRA ReLU mismatch index=%u actual=%d expected=%d\n", index,
             (int)cgra_output[index], (int)expected);
      ++failures;
    }
  }
  return failures;
}

static int run_spm_dma_relu(void) {
  const cgra_transfer_pull_descriptor_t pull = {
      .job_id = JOB_ID,
      .slot = 0,
      .bytes = CGRA_TRANSFER_BYTES,
      .spm_word_address = CGRA_SPM_WORD_ADDR,
      .dma_tag = DMA_TAG,
  };
  const cgra_transfer_launch_header_t launch = {
      .job_id = JOB_ID,
      .slot = 0,
      .bytes = CGRA_TRANSFER_BYTES,
      .spm_word_address = CGRA_SPM_WORD_ADDR,
      .dma_tag = DMA_TAG,
      .packet_count = RELU4X4_FAST_LAUNCH_PACKET_COUNT,
  };

  cgra_transfer_submit_pull(pull);
  CGRA_SET_EXPECTED_COMPLETES(CGRA_EXPECTED_COMPLETES);
  load_relu4x4_config_fast();
  submit_launch_sequence(launch);

  gemmini_extended_mvout_spad(GEMMINI_EXTERNAL_SPAD_OUTPUT_SLOT0_ROW,
                              GEMMINI_EXTERNAL_SPAD_FULL_WIDTH_ROW_STRIDE,
                              ACCUMULATOR_FULL_WIDTH_ADDRESS, DIM,
                              PUBLICATION_ROWS);

  const cgra_transfer_launch_result_t launch_result =
      cgra_transfer_wait_launch_result();
  const cgra_transfer_compute_result_t compute_result =
      cgra_transfer_wait_compute_result();
  if (verify_transfer_results(launch_result, compute_result) != 0) {
    return 1;
  }
  if (cgra_transfer_read32(CGRA_TRANSFER_CONTROL_LAUNCH_ERROR_VALID) != 0 ||
      cgra_transfer_read32(CGRA_TRANSFER_CONTROL_COMPUTE_ERROR_VALID) != 0) {
    printf("CGRA transfer reported a protocol error\n");
    return 1;
  }

  cgra_dma_mvout_async(cgra_output, OUTPUT_DESCRIPTOR);
  if (cgra_dma_wait(OUTPUT_DMA_TAG) != OUTPUT_DMA_TAG) {
    printf("CGRA DMA MVOUT tag mismatch\n");
    return 1;
  }
  cgra_dma_memory_fence();
  return verify_cgra_relu_outputs();
}

int main(void) {
  init_inputs();
  run_gemmini_gemm();

  const int failures = run_spm_dma_relu();
  if (failures != 0) {
    printf("Gemmini + CGRA SPM DMA ReLU: FAIL (%d)\n", failures);
    return 1;
  }

  printf("Gemmini + CGRA SPM DMA ReLU: PASS\n");
  return 0;
}
