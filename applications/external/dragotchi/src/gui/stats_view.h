#ifndef __stats_view_h__
#define __stats_view_h__
#include <gui/view.h>
#include "../game_structs.h"
View *stats_view_alloc(void *context);
void stats_view_free(View *);
void stats_view_update(View *, const struct GameState *, uint32_t age_days);
#endif
