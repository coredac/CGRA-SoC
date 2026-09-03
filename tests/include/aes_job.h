#ifndef AES_JOB_H
#define AES_JOB_H

#include "cgra_link_control_generated.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

static inline volatile uint64_t *aes_job_reg(uintptr_t offset) { return (volatile uint64_t *)(AES_JOB_BASE + offset); }

static inline void aes_job_write(uintptr_t offset, uint64_t value) { *aes_job_reg(offset) = value; }

/* key[0] is the least-significant 64-bit limb. */
static inline void aes_job_submit(const void *source, size_t bytes, void *destination, volatile uint32_t *completion, const uint64_t key[4], bool encrypt) {
  aes_job_write(AES_JOB_SOURCE, (uintptr_t)source);
  aes_job_write(AES_JOB_BYTES, bytes);
  aes_job_write(AES_JOB_DESTINATION, (uintptr_t)destination);
  aes_job_write(AES_JOB_COMPLETION, (uintptr_t)completion);
  aes_job_write(AES_JOB_KEY0, key[0]);
  aes_job_write(AES_JOB_KEY1, key[1]);
  aes_job_write(AES_JOB_KEY2, key[2]);
  aes_job_write(AES_JOB_KEY3, key[3]);
  aes_job_write(AES_JOB_ENCRYPT, encrypt);
  aes_job_write(AES_JOB_SUBMIT, 1);
  __asm__ volatile("fence iorw, iorw" ::: "memory");
}

#endif
