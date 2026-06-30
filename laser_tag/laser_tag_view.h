
#pragma once

#include <gui/view.h>
#include "game_state.h"

typedef struct LaserTagView LaserTagView;

LaserTagView* laser_tag_view_alloc();
void laser_tag_view_free(LaserTagView* laser_tag_view);
void laser_tag_view_draw(View* view, Canvas* canvas);
View* laser_tag_view_get_view(LaserTagView* laser_tag_view);
void laser_tag_view_update(LaserTagView* laser_tag_view, GameState* game_state);
