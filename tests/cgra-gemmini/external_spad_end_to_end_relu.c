// clang-format off
#include "include/gemmini_params_issue4_t1.h"
#include "gemmini.h"
// clang-format on
#include "cgra_dma.h"
#include "cgra_protocol.h"
#include "cgra_transfer_control.h"
#include "gemmini_external_spad.h"
#include "generated/cgra_relu4x4_fast_api.h"

#include <stdint.h>
#include <stdio.h>

enum {
  VALIDATION_CONSUMER_ENABLE = 0x200,
  WORD_COUNT = 32,
  TRANSFER_BYTES = WORD_COUNT * sizeof(acc_t),
  PUBLICATION_ROWS =
      TRANSFER_BYTES / GEMMINI_EXTERNAL_SPAD_FULL_WIDTH_ROW_BYTES,
  SPM_WORD_ADDRESS = 0,
  JOB_ID = 0x701,
  DMA_TAG = 0x71,
  OUTPUT_DMA_TAG = 0x91,
  MISMATCH_JOB_ID = 0x702,
  MISMATCH_LAUNCH_JOB_ID = 0x703,
  MISMATCH_DMA_TAG = 0x72,
  MISMATCH_LAUNCH_DMA_TAG = 0x73,
  MISMATCH_BYTES = GEMMINI_EXTERNAL_SPAD_FULL_WIDTH_ROW_BYTES,
  MISMATCH_SPM_WORD_ADDRESS = WORD_COUNT,
};

_Static_assert(DIM == GEMMINI_EXTERNAL_SPAD_MATRIX_DIMENSION,
               "end-to-end validation must match Gemmini DIM");
_Static_assert(sizeof(elem_t) == 1, "Gemmini elements must remain int8");
_Static_assert(sizeof(acc_t) == 4, "Gemmini accumulators must remain int32");
_Static_assert(CGRA_DATA_PAYLOAD_NBITS == 32, "CGRA words must be int32");
_Static_assert(CGRA_DMA_DRAM_DATA_NBITS == 128,
               "CGRA DMA must retain one 16-byte TileLink beat");
_Static_assert(TRANSFER_BYTES == 128,
               "T7 validates exactly one 128-byte chunk");
_Static_assert(PUBLICATION_ROWS == 2,
               "T7 publication must contain two full-width rows");
_Static_assert(RELU4X4_FAST_LAUNCH_PACKET_COUNT > 0,
               "ReLU must contain a real launch sequence");
_Static_assert(RELU4X4_FAST_LAUNCH_PACKET_COUNT <= 16,
               "ReLU launch sequence exceeds the bounded T6 gate");

static elem_t matrix_a[DIM][DIM] row_align(1);
static elem_t matrix_b[DIM][DIM] row_align(1);
static acc_t cgra_output[WORD_COUNT] __attribute__((aligned(16)));

static const cgra_dma_desc_t OUTPUT_DESCRIPTOR =
    CGRA_DMA_DESC_CONST(SPM_WORD_ADDRESS, TRANSFER_BYTES, OUTPUT_DMA_TAG);

static const uint32_t ACCUMULATOR_WRITE_ADDRESS = (uint32_t)1 << (ADDR_LEN - 1);
static const uint32_t ACCUMULATOR_FULL_WIDTH_ADDRESS =
    ((uint32_t)1 << (ADDR_LEN - 1)) | ((uint32_t)1 << (ADDR_LEN - 3));

#ifdef CGRA_EXTERNAL_SPAD_VALIDATION_CONFIG
static void validation_write32(uintptr_t offset, uint32_t value) {
  *(volatile uint32_t *)(GEMMINI_EXTERNAL_SPAD_VALIDATION_TELEMETRY_BASE +
                         offset) = value;
}
#endif

static void initialize_inputs(void) {
  for (int row = 0; row < DIM; ++row) {
    for (int column = 0; column < DIM; ++column) {
      const int index = row * DIM + column;
      matrix_a[row][column] = (elem_t)(index % WORD_COUNT - WORD_COUNT / 2);
      matrix_b[row][column] = row == column ? (elem_t)1 : (elem_t)0;
    }
  }
  for (unsigned index = 0; index < WORD_COUNT; ++index) {
    cgra_output[index] = (acc_t)0x5a5a5a5a;
  }
}

static void compute_gemmini_identity_gemm(void) {
  const uint32_t matrix_a_address = 0;
  const uint32_t matrix_b_address = DIM;

  gemmini_flush(0);
  gemmini_config_ld(DIM * sizeof(elem_t));
  gemmini_config_ex(WEIGHT_STATIONARY, NO_ACTIVATION, 0);
  gemmini_config_st(DIM * sizeof(acc_t));
  gemmini_mvin(matrix_a, matrix_a_address);
  gemmini_mvin(matrix_b, matrix_b_address);
  gemmini_preload(matrix_b_address, ACCUMULATOR_WRITE_ADDRESS);
  gemmini_compute_preloaded(matrix_a_address, GARBAGE_ADDR);
  gemmini_fence();
}

static void submit_launch_sequence(cgra_transfer_launch_header_t header) {
  cgra_transfer_submit_launch_header(header);
  for (unsigned index = 0; index < RELU4X4_FAST_LAUNCH_PACKET_COUNT; ++index) {
    cgra_transfer_submit_launch_packet(RELU4X4_FAST_LAUNCH_PACKETS[index]);
  }
}

static int verify_success_identity(cgra_transfer_launch_result_t launch,
                                   cgra_transfer_compute_result_t compute) {
  if (launch.job_id != JOB_ID || launch.slot != 0 ||
      launch.requested_bytes != TRANSFER_BYTES ||
      launch.actual_bytes != TRANSFER_BYTES ||
      launch.spm_word_address != SPM_WORD_ADDRESS ||
      launch.dma_tag != DMA_TAG ||
      launch.packet_count != RELU4X4_FAST_LAUNCH_PACKET_COUNT ||
      launch.status != CGRA_TRANSFER_LAUNCH_STATUS_LAUNCH_ACCEPTED) {
    printf("T7 LaunchAccepted identity/status mismatch\n");
    return 1;
  }
  if (compute.job_id != JOB_ID || compute.slot != 0 ||
      compute.requested_bytes != TRANSFER_BYTES ||
      compute.actual_bytes != TRANSFER_BYTES ||
      compute.spm_word_address != SPM_WORD_ADDRESS ||
      compute.dma_tag != DMA_TAG ||
      compute.packet_count != RELU4X4_FAST_LAUNCH_PACKET_COUNT ||
      compute.complete_data != 0 ||
      compute.status != CGRA_TRANSFER_COMPUTE_STATUS_SUCCESS) {
    printf("T7 real CMD_COMPLETE identity/status mismatch\n");
    return 1;
  }
  return 0;
}

static int verify_relu_output(void) {
  int failures = 0;
  for (unsigned index = 0; index < WORD_COUNT; ++index) {
    const int input = (int)matrix_a[index / DIM][index % DIM];
    const acc_t expected = input > 0 ? (acc_t)input : (acc_t)0;
    if (cgra_output[index] != expected) {
      printf("T7 ReLU mismatch index=%u actual=%d expected=%d\n", index,
             (int)cgra_output[index], (int)expected);
      ++failures;
    }
  }
  return failures;
}

static int run_success_path(void) {
  const cgra_transfer_pull_descriptor_t pull = {
      .job_id = JOB_ID,
      .slot = 0,
      .bytes = TRANSFER_BYTES,
      .spm_word_address = SPM_WORD_ADDRESS,
      .dma_tag = DMA_TAG,
  };
  const cgra_transfer_launch_header_t launch = {
      .job_id = JOB_ID,
      .slot = 0,
      .bytes = TRANSFER_BYTES,
      .spm_word_address = SPM_WORD_ADDRESS,
      .dma_tag = DMA_TAG,
      .packet_count = RELU4X4_FAST_LAUNCH_PACKET_COUNT,
  };

  cgra_transfer_submit_pull(pull);
  CGRA_SET_EXPECTED_COMPLETES(1);
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
  if (verify_success_identity(launch_result, compute_result) != 0) {
    return 1;
  }
  if (cgra_transfer_read32(CGRA_TRANSFER_CONTROL_LAUNCH_ERROR_VALID) != 0 ||
      cgra_transfer_read32(CGRA_TRANSFER_CONTROL_COMPUTE_ERROR_VALID) != 0) {
    printf("T7 success path reported a protocol error\n");
    return 1;
  }

  cgra_dma_mvout_async(cgra_output, OUTPUT_DESCRIPTOR);
  if (cgra_dma_wait(OUTPUT_DMA_TAG) != OUTPUT_DMA_TAG) {
    printf("T7 output DMA MVOUT tag mismatch\n");
    return 1;
  }
  cgra_dma_memory_fence();
  return verify_relu_output();
}

static int run_mismatch_path(void) {
  const cgra_transfer_pull_descriptor_t pull = {
      .job_id = MISMATCH_JOB_ID,
      .slot = 1,
      .bytes = MISMATCH_BYTES,
      .spm_word_address = MISMATCH_SPM_WORD_ADDRESS,
      .dma_tag = MISMATCH_DMA_TAG,
  };
  const cgra_transfer_launch_header_t launch = {
      .job_id = MISMATCH_LAUNCH_JOB_ID,
      .slot = 1,
      .bytes = MISMATCH_BYTES,
      .spm_word_address = MISMATCH_SPM_WORD_ADDRESS,
      .dma_tag = MISMATCH_LAUNCH_DMA_TAG,
      .packet_count = RELU4X4_FAST_LAUNCH_PACKET_COUNT,
  };

  cgra_transfer_submit_pull(pull);
  submit_launch_sequence(launch);
  gemmini_extended_mvout_spad(GEMMINI_EXTERNAL_SPAD_OUTPUT_SLOT1_ROW,
                              GEMMINI_EXTERNAL_SPAD_FULL_WIDTH_ROW_STRIDE,
                              ACCUMULATOR_FULL_WIDTH_ADDRESS, DIM, 1);

  const cgra_transfer_protocol_error_t error =
      cgra_transfer_wait_launch_error();
  if (error.job_id != MISMATCH_JOB_ID || error.slot != 1 ||
      error.requested_bytes != MISMATCH_BYTES ||
      error.actual_bytes != MISMATCH_BYTES ||
      error.spm_word_address != MISMATCH_SPM_WORD_ADDRESS ||
      error.dma_tag != MISMATCH_DMA_TAG ||
      error.operation != CGRA_TRANSFER_LAUNCH_ERROR_OPERATION_COMPLETION ||
      error.reason != CGRA_TRANSFER_LAUNCH_ERROR_REASON_IDENTITY_MISMATCH) {
    printf("T7 mismatch path returned the wrong typed launch error\n");
    return 1;
  }
  if (cgra_transfer_read32(CGRA_TRANSFER_CONTROL_LAUNCH_RESULT_VALID) != 0 ||
      cgra_transfer_read32(CGRA_TRANSFER_CONTROL_COMPUTE_RESULT_VALID) != 0 ||
      cgra_transfer_read32(CGRA_TRANSFER_CONTROL_COMPUTE_ERROR_VALID) != 0) {
    printf("T7 mismatch path produced a false launch/compute event\n");
    return 1;
  }
  return 0;
}

int main(void) {
  initialize_inputs();
  compute_gemmini_identity_gemm();

#ifdef CGRA_EXTERNAL_SPAD_VALIDATION_CONFIG
  // The validation configuration keeps legacy T5 source selection at this
  // page. Ordering and synchronization below use only production typed events.
  validation_write32(VALIDATION_CONSUMER_ENABLE, 1);
#endif

  int failures = run_success_path();
  if (failures == 0) {
    failures += run_mismatch_path();
  }
  if (failures != 0) {
    printf("Issue #4 T7 Gemmini-to-CGRA ReLU: FAIL (%d)\n", failures);
    return 1;
  }

  printf("Issue #4 T7 Gemmini accumulator -> external SPAD -> automatic "
         "CGRA DMA -> real launch/CMD_COMPLETE -> DMA MVOUT ReLU: PASS\n");
  return 0;
}
