#pragma once

// EmailInput — thin wrapper around the unified keyboard engine.
// Provides the email-optimised keyboard: @ and . on ABC row 2 left,
// complete digit set 0–9 via the right column (7–9 on row 2 right).
// See views/keyboard/nfc_tools_keyboard.h for implementation details.

#include "../keyboard/nfc_tools_keyboard.h"

// ── Type aliases (no changes required in existing scenes) ──────────────────

typedef Keyboard         EmailInput;
typedef KeyboardCallback EmailInputCallback;

// ── Inline forwarding functions ────────────────────────────────────────────

static inline EmailInput* email_input_alloc(void) {
    return keyboard_alloc(KeyboardLayoutEmail);
}

static inline void email_input_free(EmailInput* ei) {
    keyboard_free(ei);
}

static inline void email_input_reset(EmailInput* ei) {
    keyboard_reset(ei);
}

static inline View* email_input_get_view(EmailInput* ei) {
    return keyboard_get_view(ei);
}

static inline void email_input_set_header_text(EmailInput* ei, const char* text) {
    keyboard_set_header_text(ei, text);
}

static inline void email_input_set_result_callback(
    EmailInput*        ei,
    EmailInputCallback callback,
    void*              callback_context,
    char*              text_buffer,
    size_t             text_buffer_size,
    bool               clear_default_text) {
    keyboard_set_result_callback(
        ei, callback, callback_context, text_buffer, text_buffer_size, clear_default_text);
}

static inline void email_input_set_minimum_length(EmailInput* ei, size_t minimum_length) {
    keyboard_set_minimum_length(ei, minimum_length);
}
