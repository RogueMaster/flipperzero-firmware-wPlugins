#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "morse_flipper_time.h"

int main(void) {
    unsigned checks = 0U;
#define CHECK(value) \
    do { \
        assert(value); \
        checks++; \
    } while(0)
    CHECK(morse_flipper_time_pending(99U, 100U));
    CHECK(morse_flipper_time_reached(100U, 100U));
    CHECK(morse_flipper_time_reached(101U, 100U));
    CHECK(morse_flipper_time_pending(UINT32_MAX - 2U, 1U));
    CHECK(morse_flipper_time_reached(0U, 0U));
    CHECK(morse_flipper_time_pending(0U, 1U));
    CHECK(morse_flipper_time_reached(1U, UINT32_MAX - 1U));
    CHECK(morse_flipper_time_pending(1U, 0x7FFFFFFFU));
    printf("test_time: %u checks passed\n", checks);
    return 0;
}
