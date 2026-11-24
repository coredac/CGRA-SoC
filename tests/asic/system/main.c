#include "accel_generated.h"
#include "fixture.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef ASIC_FIXTURES
#define ASIC_FIXTURES "../../../build/experiments/asic/system/fixtures.h"
#endif
#include ASIC_FIXTURES

static int32_t transfer[4096] __attribute__((aligned(64)));
static int32_t outputs[4096] __attribute__((aligned(64)));

static unsigned long cycles(void) {
  unsigned long value;
  __asm__ volatile("rdcycle %0" : "=r"(value) : : "memory");
  return value;
}

static unsigned padded(unsigned words) { return (words + 7u) / 8u * 8u; }

static int configure(const fixture_t *fixture, int chained) {
  for (unsigned index = 0; index < fixture->jobs; ++index) {
    asic_job_t job = fixture->job[index];
    job.next_job = chained && index + 1 < fixture->jobs ? index + 2 : 0;
    const int status = asic_config(ASIC0, index, &job);
    if (status) {
      return status;
    }
  }
  return 0;
}

static int load(const fixture_t *fixture, unsigned input, unsigned *bytes) {
  for (unsigned index = 0; index < fixture->loads; ++index) {
    const fixture_segment_t *segment = &fixture->load[index];
    const unsigned words = padded(segment->words);
    memcpy(transfer, segment->input[input], segment->words * sizeof(*transfer));
    memset(transfer + segment->words, 0, (words - segment->words) * sizeof(*transfer));
    asic_fence();
    const int status = asic_load(ASIC0, segment->offset, transfer, words);
    if (status) {
      return status;
    }
    *bytes += words * sizeof(*transfer);
  }
  return 0;
}

static int execute(const fixture_t *fixture, int chained, unsigned *submissions) {
  const unsigned count = chained ? 1 : fixture->jobs;
  for (unsigned index = 0; index < count; ++index) {
    int status = asic_start(ASIC0, index);
    if (!status) {
      ++*submissions;
      status = asic_wait(ASIC0);
    }
    if (status) {
      return status;
    }
  }
  return 0;
}

static int store(const fixture_t *fixture, unsigned *bytes) {
  unsigned offset = 0;
  for (unsigned index = 0; index < fixture->checks; ++index) {
    const fixture_segment_t *segment = &fixture->check[index];
    const unsigned words = padded(segment->words);
    const int status = asic_store(ASIC0, outputs + offset, segment->offset, words);
    if (status) {
      return status;
    }
    offset += words;
    *bytes += words * sizeof(*outputs);
  }
  asic_fence();
  return 0;
}

static unsigned check(const fixture_t *fixture, unsigned input, unsigned *checked) {
  unsigned errors = 0;
  unsigned offset = 0;
  for (unsigned index = 0; index < fixture->checks; ++index) {
    const fixture_segment_t *segment = &fixture->check[index];
    for (unsigned word = 0; word < segment->words; ++word) {
      const int actual = outputs[offset + word];
      const int expected = segment->input[input][word];
      if (actual != expected) {
        if (errors < 4) {
          printf("ASIC mismatch case=%s input=%u check=%u word=%u actual=%d expected=%d\n", fixture->name, input, index, word, actual, expected);
        }
        ++errors;
      }
    }
    *checked += segment->words;
    offset += padded(segment->words);
  }
  return errors;
}

static int run(const fixture_t *fixture, int chained, unsigned input) {
  unsigned input_bytes = 0;
  unsigned output_bytes = 0;
  unsigned submissions = 0;
  unsigned checked = 0;
  memset(outputs, 0xa5, sizeof(outputs));
  asic_fence();
  const unsigned long begin = cycles();
  int status = input == 0 ? configure(fixture, chained) : 0;
  const unsigned long configured = cycles();
  if (!status) {
    status = load(fixture, input, &input_bytes);
  }
  const unsigned long loaded = cycles();
  if (!status) {
    status = execute(fixture, chained, &submissions);
  }
  const unsigned long executed = cycles();
  if (!status) {
    status = store(fixture, &output_bytes);
  }
  const unsigned long finished = cycles();
  const unsigned errors = status ? 0 : check(fixture, input, &checked);
  printf("ASIC SYSTEM case=%s workload=%s mode=%s phase=%s input=%u total_cycles=%lu setup_cycles=%lu input_cycles=%lu execute_cycles=%lu output_cycles=%lu cpu_submissions=%u configured_jobs=%u "
         "input_dma_bytes=%u output_dma_bytes=%u checked=%u mismatches=%u status=%d\n",
         fixture->name, fixture->workload, chained ? "chain" : "operator", input ? "warm" : "cold", input, finished - begin, configured - begin, loaded - configured, executed - loaded,
         finished - executed, submissions, input ? 0 : fixture->jobs, input_bytes, output_bytes, checked, errors, status);
  return status || errors;
}

int main(int argc, char **argv) {
  const int selected = argc > 1 ? atoi(argv[1]) : -1;
  const int all = argc > 2 ? atoi(argv[2]) : 0;
  if (selected < -1 || selected > 1 || all < 0 || all > 1) {
    printf("ASIC SYSTEM arguments: mode=-1(both)|0(operator)|1(chain) coverage=0(smoke)|1(all)\n");
    return 1;
  }
  printf("ASIC SYSTEM scope=non-matrix-kernel-chains reference-intermediates=1 full-workload=0 CPU-cycle-boundary=configuration-input-execution-output\n");
  for (unsigned index = 0; index < FIXTURE_COUNT; ++index) {
    const fixture_t *fixture = &fixtures[index];
    if (!all && !fixture->smoke) {
      continue;
    }
    for (int mode = selected < 0 ? 0 : selected; mode <= (selected < 0 ? 1 : selected); ++mode) {
      for (unsigned input = 0; input < 2; ++input) {
        if (run(fixture, mode, input)) {
          printf("ASIC SYSTEM: FAIL\n");
          return 1;
        }
      }
    }
  }
  printf("ASIC SYSTEM: PASS\n");
  return 0;
}
