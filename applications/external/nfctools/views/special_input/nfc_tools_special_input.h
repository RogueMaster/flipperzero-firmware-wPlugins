#pragma once

// SpecialInput — thin wrapper around the unified keyboard engine.
// Provides the full alpha keyboard (a–z, 0–9, ABC/SYM toggle).
// See views/keyboard/nfc_tools_keyboard.h for implementation details.

#include "../keyboard/nfc_tools_keyboard.h"

// ── Type aliases (no changes required in existing scenes) ──────────────────

typedef Keyboard         SpecialInput;
typedef KeyboardCallback SpecialInputCallback;

// ── Inline forwarding functions ────────────────────────────────────────────

static inline SpecialInput* special_input_alloc(void) {
    return keyboard_alloc(KeyboardLayoutAlpha);
}

static inline void special_input_free(SpecialInput* si) {
    keyboard_free(si);
}

static inline void special_input_reset(SpecialInput* si) {
    keyboard_reset(si);
}

static inline View* special_input_get_view(SpecialInput* si) {
    return keyboard_get_view(si);
}

static inline void special_input_set_header_text(SpecialInput* si, const char* text) {
    keyboard_set_header_text(si, text);
}

static inline void special_input_set_result_callback(
    SpecialInput*        si,
    SpecialInputCallback callback,
    void*                callback_context,
    char*                text_buffer,
    size_t               text_buffer_size,
    bool                 clear_default_text) {
    keyboard_set_result_callback(
        si, callback, callback_context, text_buffer, text_buffer_size, clear_default_text);
}

static inline void special_input_set_minimum_length(SpecialInput* si, size_t minimum_length) {
    keyboard_set_minimum_length(si, minimum_length);
}
