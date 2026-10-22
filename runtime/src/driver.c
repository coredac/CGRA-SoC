#include "internal/driver.h"

int soc_store(const soc_context_t *context, const soc_output_t *output) {
  switch (output->kind) {
  case SOC_CGRA:
    return soc_cgra_store(context, output);
  case SOC_GEMMINI:
    /* Gemmini writes outputs within its native task command stream. */
    __builtin_unreachable();
  }
  __builtin_unreachable();
}
