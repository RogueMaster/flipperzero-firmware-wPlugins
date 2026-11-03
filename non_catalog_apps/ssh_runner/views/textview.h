/* Scrollable, word-wrapped text with an optional title, a fixed status footer and button
 * hints: the one text screen of the app (Output, host-key prompts, reports, About, keys).
 *
 * Layout: title centred on top (truncated with "..."), in FontPrimary for plain words and in
 * FontKeyboard for technical text (text_is_technical: addresses, commands, paths, which read
 * better in a fixed-width font); body
 * wrapped by core/wrap.c at word boundaries with 2 px margins and a stock scrollbar; the
 * bottom 12 px hold either button hints (elements_button_*) or a one-line status bar, and
 * the body never runs underneath them. Up/Down scroll; Left/Right/OK go to the callback.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */
#pragma once

#include <gui/view.h>
#include <input/input.h>

typedef struct TextView TextView;

typedef enum {
    TextViewFontBody, /* FontSecondary, proportional */
    TextViewFontMono, /* FontKeyboard, monospace: fingerprints, keys, output */
} TextViewFont;

/* Left/Right/OK short presses (Back is navigation and never reaches it). */
typedef void (*TextViewCallback)(void* context, InputKey key);

TextView* text_view_alloc(void);
void text_view_free(TextView* tv);
View* text_view_get_view(TextView* tv);

void text_view_reset(TextView* tv);
/* Title; icon (10 px high, may be NULL) is drawn at its left. The font follows the text
 * (text_is_technical); _mono forces FontKeyboard for names without symbols (an SSID). */
void text_view_set_header(TextView* tv, const char* header, const Icon* icon);
void text_view_set_header_mono(TextView* tv, const char* header, const Icon* icon);
/* Body blocks 0..TEXT_VIEW_BLOCKS-1, each in its own font, one after the other (e.g. the
 * host in mono, a warning in words, then fingerprints in mono). The text is copied. */
#define TEXT_VIEW_BLOCKS 6
void text_view_set_text(TextView* tv, uint8_t block, const char* text, TextViewFont font);
/* Show the caller's text in place, with no copy (Output: RAM). The caller changes that
 * text only between lock() and unlock(), and points the blocks at it again (set_ref) before
 * unlock(), so a frame never sees text and lengths that disagree. set_ref() only while
 * locked; set_text() and reset() drop the reference. */
void text_view_lock(TextView* tv);
void text_view_set_ref(TextView* tv, uint8_t block, const char* text, size_t len, TextViewFont font);
void text_view_unlock(TextView* tv, bool redraw);
/* Fixed one-line status bar at the bottom (ignored while buttons are set). */
void text_view_set_footer(TextView* tv, const char* footer, const Icon* icon);
/* The footer's right end, e.g. the Back sign and "Stop"; NULL clears it. */
void text_view_set_footer_hint(TextView* tv, const Icon* icon, const char* hint);
void text_view_set_buttons(TextView* tv, const char* left, const char* center, const char* right);
/* Keep the last line in view when the text grows (Output). */
void text_view_set_follow_end(TextView* tv, bool follow);
void text_view_scroll_to_end(TextView* tv);
void text_view_set_callback(TextView* tv, TextViewCallback cb, void* context);
