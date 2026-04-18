#pragma once

#include <gui/view.h>

typedef struct CardView CardView;

CardView *card_view_alloc(void);
void card_view_free(CardView *cv);
View *card_view_get_view(CardView *cv);
void card_view_set_text(CardView *cv, const char *multiline);
void card_view_set_callback(CardView *cv, void *ctx, void (*on_ok)(void *ctx));
