#pragma once

// MimeInput — thin wrapper around the unified keyboard engine.
// Provides the MIME-optimised keyboard: / and . on ABC row 2 left,
// complete digit set 0–9 via the right column (7–9 on row 2 right).
// See views/keyboard/nfc_tools_keyboard.h for implementation details.

#include "../keyboard/nfc_tools_keyboard.h"

// ── Type aliases (no changes required in existing scenes) ──────────────────

typedef Keyboard         MimeInput;
typedef KeyboardCallback MimeInputCallback;

// ── Inline forwarding functions ────────────────────────────────────────────

static inline MimeInput* mime_input_alloc(void) {
    return keyboard_alloc(KeyboardLayoutMime);
}

static inline void mime_input_free(MimeInput* mi) {
    keyboard_free(mi);
}

static inline void mime_input_reset(MimeInput* mi) {
    keyboard_reset(mi);
}

static inline View* mime_input_get_view(MimeInput* mi) {
    return keyboard_get_view(mi);
}

static inline void mime_input_set_header_text(MimeInput* mi, const char* text) {
    keyboard_set_header_text(mi, text);
}

static inline void mime_input_set_result_callback(
    MimeInput*        mi,
    MimeInputCallback callback,
    void*             callback_context,
    char*             text_buffer,
    size_t            text_buffer_size,
    bool              clear_default_text) {
    keyboard_set_result_callback(
        mi, callback, callback_context, text_buffer, text_buffer_size, clear_default_text);
}

static inline void mime_input_set_minimum_length(MimeInput* mi, size_t minimum_length) {
    keyboard_set_minimum_length(mi, minimum_length);
}
