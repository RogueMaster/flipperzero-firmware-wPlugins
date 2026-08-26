#pragma once

#include <gui/view.h>

typedef struct LongTextView LongTextView;

/**
 * Scrollable multi-line text view.
 *
 * The firmware TextBox misbehaves with very long texts (blank screen while
 * scrolling), so responses are shown here instead: line wrapping and
 * scrolling are handled locally with bounded loops.
 */
LongTextView* long_text_view_alloc(void);

void long_text_view_free(LongTextView* long_text_view);

View* long_text_view_get_view(LongTextView* long_text_view);

/** Replace the displayed text and reset the scroll to the top. */
void long_text_view_set_text(LongTextView* long_text_view, const char* text);

/** Clear the displayed text. */
void long_text_view_reset(LongTextView* long_text_view);
