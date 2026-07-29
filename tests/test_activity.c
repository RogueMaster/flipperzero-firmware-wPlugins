#include <assert.h>
#include <stdio.h>

#include "morse_flipper_activity.h"

int main(void) {
    MorseFlipperActivityDaily daily;

    assert(!morse_flipper_activity_listening_success(94U));
    assert(morse_flipper_activity_listening_success(95U));
    assert(morse_flipper_activity_listening_success(100U));

    morse_flipper_activity_daily_reset(&daily);
    assert(
        !morse_flipper_activity_daily_note(&daily, UINT16_MAX, MorseFlipperActivityCorrectAnswer));
    assert(!morse_flipper_activity_daily_note(&daily, 10U, MorseFlipperActivityCorrectAnswer));
    assert(!morse_flipper_activity_daily_note(&daily, 10U, MorseFlipperActivityCorrectAnswer));
    assert(morse_flipper_activity_daily_note(&daily, 10U, MorseFlipperActivityCorrectAnswer));
    assert(daily.deed_awards == 1U && daily.correct_answers == 0U);
    assert(morse_flipper_activity_daily_note(&daily, 10U, MorseFlipperActivityListeningSession));
    assert(daily.deed_awards == 2U);
    assert(!morse_flipper_activity_daily_note(&daily, 10U, MorseFlipperActivityCorrectAnswer));
    assert(!morse_flipper_activity_daily_note(&daily, 10U, MorseFlipperActivityCorrectAnswer));
    assert(morse_flipper_activity_daily_note(&daily, 10U, MorseFlipperActivityCorrectAnswer));
    assert(daily.deed_awards == 3U);
    for(uint8_t i = 0U; i < 9U; i++)
        assert(!morse_flipper_activity_daily_note(&daily, 10U, MorseFlipperActivityCorrectAnswer));
    assert(!morse_flipper_activity_daily_note(&daily, 10U, MorseFlipperActivityListeningSession));
    assert(daily.deed_awards == 3U);

    assert(!morse_flipper_activity_daily_note(&daily, 11U, MorseFlipperActivityCorrectAnswer));
    assert(daily.day == 11U && daily.deed_awards == 0U && daily.correct_answers == 1U);
    puts("test_activity: passed");
    return 0;
}
