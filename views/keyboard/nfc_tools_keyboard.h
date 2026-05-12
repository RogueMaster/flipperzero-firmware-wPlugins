#pragma once

#include <gui/view.h>
#include <stdbool.h>
#include <stddef.h>

// ── Unified keyboard engine ────────────────────────────────────────────────
//
// Three layouts cover all input contexts in NFC Tools.
// Every layout exposes the complete digit set 0–9 via the right column
// (rows 0-1 for digits 0–6, row 2 for digits 7–9).
//
//  KeyboardLayoutAlpha  — full alphabet + ABC / SYM page toggle
//    ABC page, right col  : 0 1 2 3 / 4 5 6 / 7 8 9
//    SYM page, right col  : 0 1 2 3 / 4 5 6 / , . ?
//    ABC page, row 2 left : z … m  [sym]  [ok]
//    SYM page, row 2 left : { } [ ] \ | ~  [abc]  [ok]
//
//  KeyboardLayoutEmail  — alpha optimised for e-mail addresses
//    Right col, row 2     : 7 8 9
//    Row 2 left           : z … m  @  .  [ok]
//    (No SYM page — @ and . are directly accessible.)
//
//  KeyboardLayoutMime   — alpha optimised for MIME types (e.g. text/plain)
//    Right col, row 2     : 7 8 9
//    Row 2 left           : z … m  /  .  [ok]
//    (No SYM page — / and . are directly accessible.)
//
// Navigation: d-pad moves the cursor, OK confirms, long-OK = shift (uppercase),
// long-Back = backspace, short-Back = exit (passed to SceneManager).

typedef enum {
    KeyboardLayoutAlpha, // replaces SpecialInput
    KeyboardLayoutEmail, // replaces EmailInput
    KeyboardLayoutMime,  // replaces MimeInput
} KeyboardLayout;

typedef struct Keyboard Keyboard;
typedef void (*KeyboardCallback)(void* context);

// ── Lifecycle ──────────────────────────────────────────────────────────────

Keyboard* keyboard_alloc(KeyboardLayout layout);
void      keyboard_free(Keyboard* kb);

// Reset all state (selected cell, page, buffers) — call from scene on_exit.
void keyboard_reset(Keyboard* kb);

// Return the underlying View for registration with ViewDispatcher.
View* keyboard_get_view(Keyboard* kb);

// ── Configuration ──────────────────────────────────────────────────────────

// Short title displayed above the text field.
void keyboard_set_header_text(Keyboard* kb, const char* text);

// Wire up the result buffer and the callback fired when the user presses OK.
// clear_default_text: if true the buffer content is shown highlighted and is
// replaced by the first keystroke (useful for pre-filled defaults).
void keyboard_set_result_callback(
    Keyboard*        kb,
    KeyboardCallback callback,
    void*            callback_context,
    char*            text_buffer,
    size_t           text_buffer_size,
    bool             clear_default_text);

// Minimum number of characters required before OK fires the callback (default 1).
void keyboard_set_minimum_length(Keyboard* kb, size_t minimum_length);
