#include "include/platform/random_port.h"

#include <furi_hal_random.h>

enum {
    RandomPortFallbackSeed = 1, // substituted when the HAL RNG yields exactly 0
};

uint32_t platform_random_seed(void) {
    uint32_t seed = furi_hal_random_get();
    if (seed == 0) {
        seed = RandomPortFallbackSeed;
    }
    return seed;
}
