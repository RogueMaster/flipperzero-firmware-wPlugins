#include <assert.h>
#include <stdio.h>

#include "morse_flipper_training_timing.h"

int main(void) {
    unsigned checks = 0U;
#define CHECK(value)   \
    do {               \
        assert(value); \
        checks++;      \
    } while(0)
    CHECK(morse_flipper_training_char_gap_ms(100U, 12U, 0U) == 300U);
    CHECK(morse_flipper_training_char_gap_ms(100U, 12U, 12U) == 300U);
    CHECK(morse_flipper_training_char_gap_ms(100U, 12U, 20U) == 300U);
    CHECK(morse_flipper_training_char_gap_ms(100U, 12U, 6U) == 1089U);
    CHECK(morse_flipper_training_char_gap_ms(1000U, 30U, 29U) == 3000U);
    CHECK(morse_flipper_training_char_gap_ms(65535U, 30U, 1U) == 65535U);
    printf("test_training_timing: %u checks passed\n", checks);
    return 0;
}
