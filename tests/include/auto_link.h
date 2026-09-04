#ifndef AUTO_LINK_H
#define AUTO_LINK_H

#include "auto_link_generated.h"
#include "cgra_link_control_generated.h"

#include <stdint.h>

static inline void auto_link_input_ready(void) {
  *(volatile uint32_t *)(AUTO_LINK_BASE + AUTO_LINK_INPUT_READY) = 1;
  __asm__ volatile("fence iorw, iorw" ::: "memory");
}

#endif
