#ifndef AES_JOB_H
#define AES_JOB_H

#include "cgra_link_control_generated.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

static inline volatile uint64_t *aes_job_reg(uintptr_t offset) { return (volatile uint64_t *)(AES_JOB_BASE + offset); }

static inline void aes_job_write(uintptr_t offset, uint64_t value) { *aes_job_reg(offset) = value; }

static inline uint64_t aes_job_read(uintptr_t offset) { return *aes_job_reg(offset); }

/* key[0] is the least-significant 64-bit limb. */
static inline void aes_job_params(unsigned job, void *destination, volatile uint32_t *completion, const uint64_t key[4], bool encrypt) {
  aes_job_write(AES_JOB_SELECT, job);
  aes_job_write(AES_JOB_DESTINATION, (uintptr_t)destination);
  aes_job_write(AES_JOB_COMPLETION, (uintptr_t)completion);
  aes_job_write(AES_JOB_KEY0, key[0]);
  aes_job_write(AES_JOB_KEY1, key[1]);
  aes_job_write(AES_JOB_KEY2, key[2]);
  aes_job_write(AES_JOB_KEY3, key[3]);
  aes_job_write(AES_JOB_ENCRYPT, encrypt);
}

static inline int aes_job_commit(bool start) {
  aes_job_write(AES_JOB_SUBMIT, start ? 3 : 1);
  __asm__ volatile("" ::: "memory");
  while (aes_job_read(AES_JOB_CONFIG_READY) == 0) {
  }
  return aes_job_read(AES_JOB_CONFIG_STATUS) != AUTO_LINK_STATUS_SUCCESS;
}

/* Downstream source and length come from the AutoLink copy request. */
static inline int aes_job_configure(unsigned job, void *destination, volatile uint32_t *completion, const uint64_t key[4], bool encrypt) {
  aes_job_params(job, destination, completion, key, encrypt);
  return aes_job_commit(false);
}

static inline int aes_job_submit(unsigned job, const void *source, size_t bytes, void *destination, volatile uint32_t *completion, const uint64_t key[4], bool encrypt) {
  aes_job_write(AES_JOB_SOURCE, (uintptr_t)source);
  aes_job_write(AES_JOB_BYTES, bytes);
  aes_job_params(job, destination, completion, key, encrypt);
  return aes_job_commit(true);
}

#endif
