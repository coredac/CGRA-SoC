#include "cgra_link.h"
#include "cgra_spm_window.h"
#include "generated/cgra_relu4x4_fast_api.h"
#include "pool.h"
#include "pool_demo.h"

#include <stdint.h>
#include <stdio.h>

static elem_t A[POOL_DEMO_A_ROWS][DIM] row_align(1);
static elem_t B[POOL_DEMO_B_ROWS][DIM] row_align(1);
static acc_t output[POOL_DEMO_OUTPUT_CHANNELS] __attribute__((aligned(32)));

static void configure_cgra(void) {
  load_relu4x4_config_fast();
  cgra_link_configure(RELU4X4_FAST_LAUNCH_PACKET_COUNT);
  for (unsigned index = 0; index < RELU4X4_FAST_LAUNCH_PACKET_COUNT; ++index) {
    cgra_link_queue(RELU4X4_FAST_LAUNCH_PACKETS[index]);
  }
}

static void configure_pool(void) {
  const pool_job_t job = {
      .mode = POOL_MODE_MAX,
      .source = CGRA_SPM_WINDOW_BASE,
      .destination = (uintptr_t)output,
      .input_height = POOL_DEMO_CONV_HEIGHT,
      .input_width = POOL_DEMO_CONV_WIDTH,
      .channels = POOL_DEMO_OUTPUT_CHANNELS,
      .kernel_height = 2,
      .kernel_width = 2,
      .stride_height = 2,
      .stride_width = 2,
      .pad_height = 0,
      .pad_width = 0,
  };
  pool_configure(&job);
}

static int verify_result(const char *name, cgra_link_result_t result) {
  if (result.status != AUTO_LINK_STATUS_SUCCESS || result.detail != 0 || result.data != 0) {
    printf("%s AutoLink result mismatch\n", name);
    return 1;
  }
  return 0;
}

int main(void) {
  pool_demo_pack(A, B);
  configure_cgra();
  configure_pool();
  pool_demo_load_gemmini(A, B);
  pool_demo_launch_gemmini();

  const cgra_link_result_t cgra = cgra_link_wait();
  const cgra_link_result_t pool = cgra_link_wait();
  if (verify_result("CGRA", cgra) != 0 || verify_result("Pool", pool) != 0) {
    printf("Gemmini Conv + CGRA ReLU + Pool Auto: FAIL\n");
    return 1;
  }
  const int failures = pool_demo_verify(output);
  if (failures != 0) {
    printf("Gemmini Conv + CGRA ReLU + Pool Auto: FAIL (%d)\n", failures);
    return 1;
  }
  printf("Gemmini Conv + CGRA ReLU + Pool Auto: PASS\n");
  return 0;
}
