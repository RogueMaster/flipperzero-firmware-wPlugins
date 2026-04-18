#include "include/ui/card_view.h"
#include <furi.h>
#include <gui/canvas.h>
#include <gui/elements.h>
#include <gui/view.h>
#include <input/input.h>
#include <stdlib.h>
#include <string.h>

#define CARD_TEXT_MAX 256

typedef struct {
  char text[CARD_TEXT_MAX];
} CardModel;

struct CardView {
  View *view;
  void *ctx;
  void (*on_ok)(void *ctx);
};

static void card_draw_callback(Canvas *canvas, void *model) {
  const CardModel *m = model;
  canvas_clear(canvas);
  canvas_set_font(canvas, FontPrimary);
  elements_multiline_text_aligned(canvas, 64, 4, AlignCenter, AlignTop,
                                  m->text);
}

static bool card_input_callback(InputEvent *event, void *context) {
  CardView *cv = context;
  if (!cv || !cv->on_ok) {
    return false;
  }
  if (event->type == InputTypeShort && event->key == InputKeyOk) {
    cv->on_ok(cv->ctx);
    return true;
  }
  return false;
}

CardView *card_view_alloc(void) {
  CardView *cv = malloc(sizeof(CardView));
  if (!cv) {
    return NULL;
  }
  *cv = (CardView){0};
  cv->view = view_alloc();
  if (!cv->view) {
    free(cv);
    return NULL;
  }
  view_allocate_model(cv->view, ViewModelTypeLockFree, sizeof(CardModel));
  view_set_context(cv->view, cv);
  view_set_draw_callback(cv->view, card_draw_callback);
  view_set_input_callback(cv->view, card_input_callback);
  return cv;
}

void card_view_free(CardView *cv) {
  if (!cv) {
    return;
  }
  view_free(cv->view);
  free(cv);
}

View *card_view_get_view(CardView *cv) {
  furi_assert(cv);
  return cv->view;
}

void card_view_set_text(CardView *cv, const char *multiline) {
  furi_assert(cv);
  CardModel *m = view_get_model(cv->view);
  memset(m, 0, sizeof(*m));
  if (multiline) {
    strlcpy(m->text, multiline, sizeof(m->text));
  }
  view_commit_model(cv->view, true);
}

void card_view_set_callback(CardView *cv, void *ctx, void (*on_ok)(void *ctx)) {
  furi_assert(cv);
  cv->ctx = ctx;
  cv->on_ok = on_ok;
}
