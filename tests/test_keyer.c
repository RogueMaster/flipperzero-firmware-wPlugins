#include <assert.h>
#include <stdio.h>

#include "keyer.h"

static unsigned checks;

#define CHECK(value) \
    do { \
        assert(value); \
        checks++; \
    } while(0)

int main(void) {
    MorseKeyer keyer;
    MorseKeyerEvent event;

    morse_keyer_init(&keyer, MorseKeyerModeIambicB, 60U);
    morse_keyer_paddle_event(&keyer, MorseKeyerPaddleDit, true, 0U);
    morse_keyer_tick(&keyer, 0U);
    CHECK(morse_keyer_pop_event(&keyer, &event));
    CHECK(event.type == MorseKeyerEventPress && event.paddle == MorseKeyerPaddleDit);

    /* Queue the opposite element, then model an RX Answer-to-Result cleanup. */
    morse_keyer_paddle_event(&keyer, MorseKeyerPaddleDah, true, 1U);
    morse_keyer_paddle_event(&keyer, MorseKeyerPaddleDit, false, 2U);
    morse_keyer_paddle_event(&keyer, MorseKeyerPaddleDah, false, 2U);
    morse_keyer_reset(&keyer);

    CHECK(morse_keyer_get_mode(&keyer) == MorseKeyerModeIambicB);
    CHECK(morse_keyer_get_dit_duration(&keyer) == 60U);
    CHECK(morse_keyer_pop_event(&keyer, &event));
    CHECK(event.type == MorseKeyerEventRelease && event.paddle == MorseKeyerPaddleDit);
    morse_keyer_tick(&keyer, 1000U);
    CHECK(!morse_keyer_pop_event(&keyer, &event));

    printf("test_keyer: %u checks passed\n", checks);
    return 0;
}
