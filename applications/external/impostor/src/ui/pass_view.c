#include "include/ui/pass_view.h"
#include <furi.h>
#include <gui/canvas.h>
#include <gui/elements.h>
#include <gui/view.h>
#include <input/input.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  char name[IMPOSTOR_MAX_NAME_LEN];
  char title[64];
  char bottom[64];
} PassModel;

struct PassView {
  View *view;
  void *ctx;
  void (*on_long_ok)(void *ctx);
};

static void pass_draw_callback(Canvas *canvas, void *model) {
  const PassModel *m = model;
  canvas_clear(canvas);
  canvas_set_font(canvas, FontPrimary);
  elements_multiline_text_aligned(canvas, 64, 4, AlignCenter, AlignTop,
                                  m->title);
  canvas_set_font(canvas, FontSecondary);
  elements_multiline_text_aligned(canvas, 64, 18, AlignCenter, AlignTop,
                                  m->name);
  canvas_set_font(canvas, FontPrimary);
  elements_multiline_text_aligned(canvas, 64, 58, AlignCenter, AlignBottom,
                                  m->bottom);
}

static bool pass_input_callback(InputEvent *event, void *context) {
  PassView *pv = context;
  if (!pv || !pv->on_long_ok) {
    return false;
  }
  if (event->type == InputTypeLong && event->key == InputKeyOk) {
    pv->on_long_ok(pv->ctx);
    return true;
  }
  return false;
}

PassView *pass_view_alloc(void) {
  PassView *pv = malloc(sizeof(PassView));
  if (!pv) {
    return NULL;
  }
  *pv = (PassView){0};
  pv->view = view_alloc();
  if (!pv->view) {
    free(pv);
    return NULL;
  }
  view_allocate_model(pv->view, ViewModelTypeLockFree, sizeof(PassModel));
  view_set_context(pv->view, pv);
  view_set_draw_callback(pv->view, pass_draw_callback);
  view_set_input_callback(pv->view, pass_input_callback);
  return pv;
}

void pass_view_free(PassView *pv) {
  if (!pv) {
    return;
  }
  view_free(pv->view);
  free(pv);
}

View *pass_view_get_view(PassView *pv) {
  furi_assert(pv);
  return pv->view;
}

void pass_view_set_ui(PassView *pv, const char *name, const char *title,
                      const char *bottom) {
  furi_assert(pv);
  PassModel *m = view_get_model(pv->view);
  memset(m, 0, sizeof(*m));
  if (name) {
    strlcpy(m->name, name, sizeof(m->name));
  }
  if (title) {
    strlcpy(m->title, title, sizeof(m->title));
  }
  if (bottom) {
    strlcpy(m->bottom, bottom, sizeof(m->bottom));
  }
  view_commit_model(pv->view, true);
}

void pass_view_set_callback(PassView *pv, void *ctx,
                            void (*on_long_ok)(void *ctx)) {
  furi_assert(pv);
  pv->ctx = ctx;
  pv->on_long_ok = on_long_ok;
}
