#pragma once

#include <gui/view.h>
#include <stdbool.h>
#include <stddef.h>

// ── Unified virtual keyboard ────────────────────────────────────────────────
//
//
// Physical buttons:
//   D-pad          move selection, wrap-around on both axes
//   OK (short)     insert the selected char / activate the special key
//   OK (long)      insert the opposite-case variant of a letter
//   Back (short)   leave the scene (handed to the SceneManager)
//   Back (long)    delete one character (auto-repeats while held)

typedef struct Keyboard Keyboard;
typedef void (*KeyboardCallback)(void* context);

// ── Lifecycle ──────────────────────────────────────────────────────────────

Keyboard* keyboard_alloc(void);
void      keyboard_free(Keyboard* kb);

// Reset all transient state (selection, page, buffers). Call from on_exit.
void keyboard_reset(Keyboard* kb);

// Underlying View for ViewDispatcher registration.
View* keyboard_get_view(Keyboard* kb);

// ── Configuration ──────────────────────────────────────────────────────────

// Short title shown above the text field.
void keyboard_set_header_text(Keyboard* kb, const char* text);

// Result buffer + callback fired on "save". clear_default_text highlights the
// current content and replaces it on the first keystroke.
void keyboard_set_result_callback(
    Keyboard*        kb,
    KeyboardCallback callback,
    void*            callback_context,
    char*            text_buffer,
    size_t           text_buffer_size,
    bool             clear_default_text);

// Minimum length required before "save" fires the callback (default 1).
void keyboard_set_minimum_length(Keyboard* kb, size_t minimum_length);

// Hard cap on the input length (enforced in kb_insert). 0 (default) =
// text_buffer_size-1. Used for protocol-bounded fields (WiFi SSID/pass, FeliCa…).
void keyboard_set_max_length(Keyboard* kb, size_t max_length);
