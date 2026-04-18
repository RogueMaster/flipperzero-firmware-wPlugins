#include "include/platform/random_port.h"
#include <furi_hal_random.h>

uint32_t impostor_rng_u32(void *ctx) {
  (void)ctx;
  return furi_hal_random_get();
}
