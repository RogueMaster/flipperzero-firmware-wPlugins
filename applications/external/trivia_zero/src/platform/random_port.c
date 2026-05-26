#include "include/platform/random_port.h"

#ifdef __has_include
#if __has_include(<furi_hal_random.h>)
#include <furi_hal_random.h>
#define TZ_HAVE_FURI 1
#endif
#endif

uint32_t tz_rng_u32(void *ctx) {
    (void)ctx;
#ifdef TZ_HAVE_FURI
    return furi_hal_random_get();
#else
    /* Host-build fallback: deterministic counter so cppcheck doesn't see
     * an undefined function call. Tests pass an explicit RngFn stub
     * (see test_question_pool) so this code path is never exercised on host. */
    static uint32_t s = 0u;
    return s++;
#endif
}
