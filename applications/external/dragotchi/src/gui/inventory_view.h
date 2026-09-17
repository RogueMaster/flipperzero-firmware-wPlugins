#ifndef __inventory_view_h__
#define __inventory_view_h__
#include <gui/view.h>
#include "../game_structs.h"
View *inventory_view_alloc(void *context);
void inventory_view_free(View *);
void inventory_view_update(View *, const struct GameState *);
#endif
