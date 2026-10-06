#pragma once

#include <stdint.h>
#include <gui/view.h>

/**
 * Small number pad for entering a whole number.
 *
 * The value shown when the pad opens is highlighted, and the first digit typed replaces it.
 * DEL removes the last digit (or the whole highlighted value), CLR sets 0, OK confirms.
 * BACK is not handled here, so the view dispatcher's back handling leaves the pad.
 */

typedef struct NumPad NumPad;

typedef void (*NumPadCallback)(void* context, uint32_t value);

NumPad* numpad_alloc(void);

void numpad_free(NumPad* numpad);

View* numpad_get_view(NumPad* numpad);

/**
 * Prepare the pad for a new entry.
 *
 * @param header   text shown on top, must stay valid while the pad is shown (use a literal)
 * @param value    value shown first, clamped to max_value
 * @param max_value largest number that can be entered
 * @param callback called with the entered number when OK is pressed
 */
void numpad_setup(
    NumPad* numpad,
    const char* header,
    uint32_t value,
    uint32_t max_value,
    NumPadCallback callback,
    void* context);
