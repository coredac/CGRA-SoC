#ifndef CGRA_DMA_H
#define CGRA_DMA_H

#include <stddef.h>
#include <stdint.h>

#include "cgra_layout.h"
#include "generated/cgra_protocol_generated.h"

#if !CGRA_HAS_DMA
#error "The generated single-CGRA SoC has no DMA interface"
#endif

typedef uint64_t cgra_dma_desc_t;

#define CGRA_DMA_WORD_BYTES (CGRA_DATA_PAYLOAD_NBITS / 8)
#define CGRA_DMA_BEAT_BYTES (CGRA_DMA_DRAM_DATA_NBITS / 8)
#define CGRA_DMA_CONST_ASSERT(condition) \
  (UINT64_C(0) * sizeof(char[(condition) ? 1 : -1]))

#define CGRA_DMA_DESC_CONST(spm_addr, nbytes, tag)                         \
  ((cgra_dma_desc_t)(                                                       \
    CGRA_DMA_CONST_ASSERT(__builtin_constant_p(spm_addr)) +                 \
    CGRA_DMA_CONST_ASSERT(__builtin_constant_p(nbytes)) +                   \
    CGRA_DMA_CONST_ASSERT(__builtin_constant_p(tag)) +                      \
    CGRA_DMA_CONST_ASSERT((uint64_t)(spm_addr) <                            \
      (UINT64_C(1) << CGRA_DMA_DESC_SPM_ADDR_NBITS)) +                      \
    CGRA_DMA_CONST_ASSERT((uint64_t)(nbytes) > 0) +                         \
    CGRA_DMA_CONST_ASSERT((uint64_t)(nbytes) <                              \
      (UINT64_C(1) << CGRA_DMA_DESC_NBYTES_NBITS)) +                        \
    CGRA_DMA_CONST_ASSERT(((uint64_t)(nbytes) % CGRA_DMA_BEAT_BYTES) == 0) +\
    CGRA_DMA_CONST_ASSERT((uint64_t)(tag) <                                 \
      (UINT64_C(1) << CGRA_DMA_DESC_TAG_NBITS)) +                           \
    CGRA_DMA_CONST_ASSERT((uint64_t)(spm_addr) +                            \
      (uint64_t)(nbytes) / CGRA_DMA_WORD_BYTES <= CGRA_DMA_SPM_WORDS) +     \
    ((uint64_t)(spm_addr) << CGRA_DMA_DESC_SPM_ADDR_LSB) +                  \
    ((uint64_t)(nbytes) << CGRA_DMA_DESC_NBYTES_LSB) +                      \
    ((uint64_t)(tag) << CGRA_DMA_DESC_TAG_LSB)))

#define CGRA_DMA_STRINGIFY_IMPL(value) #value
#define CGRA_DMA_STRINGIFY(value) CGRA_DMA_STRINGIFY_IMPL(value)

#if !defined(__riscv)
#error "cgra_dma.h requires the RISC-V RoCC execution path"
#endif

_Static_assert(sizeof(uintptr_t) * 8 == CGRA_DMA_DRAM_ADDR_NBITS,
               "DMA DRAM address width must equal the RISC-V pointer width");
_Static_assert(CGRA_DMA_DESC_NBITS <= sizeof(cgra_dma_desc_t) * 8,
               "DMA descriptor does not fit cgra_dma_desc_t");
_Static_assert(CGRA_DMA_TAG_NBITS == sizeof(uint8_t) * 8,
               "cgra_dma_wait requires an exactly 8-bit generated DMA tag");
_Static_assert(CGRA_DMA_DRAM_DATA_NBITS == 128,
               "Phase-1 TileLink DMA requires a 128-bit data beat");
_Static_assert(CGRA_DMA_DRAM_MASK_NBITS == CGRA_DMA_BEAT_BYTES,
               "DMA mask must contain one bit per byte");

#define CGRA_DMA_ISSUE(funct, dram, desc) do {                              \
    uintptr_t cgra_dma_dram_ = (uintptr_t)(dram);                           \
    cgra_dma_desc_t cgra_dma_desc_ = (desc);                               \
    asm volatile (                                                           \
        ".insn r 0x0b, 3, " CGRA_DMA_STRINGIFY(funct)                      \
        ", x0, %0, %1\n\t"                                                  \
        :: "r" (cgra_dma_dram_), "r" (cgra_dma_desc_)                    \
        : "memory");                                                       \
  } while (0)

static inline void cgra_dma_mvin_async(const void *dram,
                                       cgra_dma_desc_t desc) {
  /* In Phase 1, the bare-metal pointer value is used as a physical address. */
  CGRA_DMA_ISSUE(CGRA_FUNCT_DMA_MVIN_ASYNC, dram, desc);
}

static inline void cgra_dma_mvout_async(void *dram,
                                        cgra_dma_desc_t desc) {
  /* In Phase 1, the bare-metal pointer value is used as a physical address. */
  CGRA_DMA_ISSUE(CGRA_FUNCT_DMA_MVOUT_ASYNC, dram, desc);
}

static inline uint8_t cgra_dma_wait(uint8_t expected_tag) {
  uintptr_t expected = expected_tag;
  uintptr_t observed;
  asm volatile (
      ".insn r 0x0b, 6, " CGRA_DMA_STRINGIFY(CGRA_FUNCT_DMA_WAIT)
      ", %0, %1, x0\n\t"
      : "=r" (observed)
      : "r" (expected)
      : "memory");
  return (uint8_t)observed;
}

static inline void cgra_dma_memory_fence(void) {
  asm volatile ("fence rw, rw" ::: "memory");
}

#endif
