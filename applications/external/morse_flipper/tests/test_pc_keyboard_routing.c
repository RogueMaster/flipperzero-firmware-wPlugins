#include "pc_keys.h"

#include <assert.h>
#include <stdio.h>

int main(void) {
    /* GPIO paddle contacts still generate notes 1/2 in straight-keyer mode. */
    assert(morse_pc_keyboard_key_for_note(0U, 0U, 1U, true) == MorsePcKeySpace);
    assert(morse_pc_keyboard_key_for_note(0U, 0U, 2U, true) == MorsePcKeySpace);
    assert(morse_pc_keyboard_key_for_note(0U, 3U, 1U, true) == MorsePcKeyC);

    /* Paddle modes retain their configured dit/dah HID mapping. */
    assert(morse_pc_keyboard_key_for_note(0U, 0U, 1U, false) == MorsePcKeyX);
    assert(morse_pc_keyboard_key_for_note(0U, 0U, 2U, false) == MorsePcKeyZ);
    assert(morse_pc_keyboard_key_for_note(7U, 0U, 2U, false) == MorsePcKeyW);

    /* Direct straight sources and invalid notes keep their existing behaviour. */
    assert(morse_pc_keyboard_key_for_note(0U, 0U, 0U, false) == MorsePcKeySpace);
    assert(morse_pc_keyboard_key_for_note(0U, 0U, 3U, false) == MorsePcKeyNone);
    assert(morse_pc_keyboard_key_for_note(0U, 0U, 3U, true) == MorsePcKeyNone);
    puts("test_pc_keyboard_routing: passed");
    return 0;
}
