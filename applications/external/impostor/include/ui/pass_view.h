#pragma once

#include "include/domain/game_limits.h"
#include <gui/view.h>

typedef struct PassView PassView;

PassView *pass_view_alloc(void);
void pass_view_free(PassView *pv);
View *pass_view_get_view(PassView *pv);
void pass_view_set_ui(PassView *pv, const char *name, const char *title,
                      const char *bottom);
void pass_view_set_callback(PassView *pv, void *ctx,
                            void (*on_long_ok)(void *ctx));
