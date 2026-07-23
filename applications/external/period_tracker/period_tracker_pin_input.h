#pragma once

#include <gui/view.h>
#include <stdint.h>
#include <stdbool.h>

/** Fixed 4-digit PIN entry view (empty start, masked digits). MIT original. */

#define PIN_INPUT_DIGIT_COUNT 4

typedef struct PinInput PinInput;

/** Called when the user confirms a complete 4-digit PIN (0000–9999). */
typedef void (*PinInputDoneCallback)(void* context, uint16_t pin);

PinInput* pin_input_alloc(void);
void pin_input_free(PinInput* pin_input);
View* pin_input_get_view(PinInput* pin_input);

/** Clear entered digits and selection (call on scene enter). */
void pin_input_reset(PinInput* pin_input);

void pin_input_set_header(PinInput* pin_input, const char* header);

void pin_input_set_result_callback(
    PinInput* pin_input,
    PinInputDoneCallback callback,
    void* context);

/**
 * When true (default), completing the 4th digit submits immediately.
 * When false, the user must press OK again to confirm (can still Backspace the 4th digit).
 * Use false for PIN setup / confirmation entry.
 */
void pin_input_set_auto_submit(PinInput* pin_input, bool auto_submit);
