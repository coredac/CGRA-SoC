#ifndef GEMMINI_TYPES_H
#define GEMMINI_TYPES_H

#include <stdint.h>

typedef struct {
  uint32_t command;
  uint32_t operand;
  uint32_t lsb;
  uint32_t width;
  uint32_t source;
  uint32_t scale;
  int32_t offset;
  uint32_t source2;
  uint32_t scale2;
  uint32_t relative;
} gemmini_patch_t;

#endif
