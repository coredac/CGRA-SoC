#ifndef ASIC_SYSTEM_FIXTURE_H
#define ASIC_SYSTEM_FIXTURE_H

#include "asic.h"

typedef struct {
  unsigned offset;
  unsigned words;
  const int32_t *input[2];
} fixture_segment_t;

typedef struct {
  const char *name;
  const char *workload;
  unsigned smoke;
  unsigned loads;
  unsigned checks;
  unsigned jobs;
  const fixture_segment_t *load;
  const fixture_segment_t *check;
  const asic_job_t *job;
} fixture_t;

#endif
