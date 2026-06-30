/*
 * Copyright 2025 Ivan Barsukov
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "level_menu.h"

#include <stddef.h>

#include "src/engine/vector.h"

#include "src/game.h"

#include "blinking_sprite.h"
#include "delayed_sprite.h"
#include "menu_entity.h"
#include "moving_sprite.h"

typedef struct
{
    Entity* quadrastic_logo;
    Entity* press_ok;
    Entity* left_button;
    Entity* right_button;

} LevelMenuContext;

static void
level_menu_alloc(Level* level, GameManager* manager, void* _level_context)
{
    LevelMenuContext* menu_context = _level_context;

    const float initial_amimation_duration = 45.0f;

    // Quadrastic logo
    menu_context->quadrastic_logo =
      moving_sprite_add_to_level(level,
                                 manager,
                                 (Vector){ .x = 9, .y = 64 },
                                 (Vector){ .x = 9, .y = 2 },
                                 initial_amimation_duration,
                                 "quadrastic.fxbm");

    // Press ok logo
    menu_context->press_ok =
      blinking_sprite_add_to_level(level,
                                   manager,
                                   (Vector){ .x = 31, .y = 33 },
                                   initial_amimation_duration,
                                   15.0f,
                                   7.0f,
                                   "press_ok.fxbm");

    // Settings button
    menu_context->left_button =
      delayed_sprite_add_to_level(level,
                                  manager,
                                  (Vector){ .x = 0, .y = 57 },
                                  initial_amimation_duration,
                                  "left_button.fxbm");

    // About button
    menu_context->right_button =
      delayed_sprite_add_to_level(level,
                                  manager,
                                  (Vector){ .x = 115, .y = 57 },
                                  initial_amimation_duration,
                                  "right_button.fxbm");

    // Menu
    level_add_entity(level, &menu_description);
}

static void
level_menu_start(Level* level, GameManager* manager, void* _level_context)
{
    UNUSED(level);
    UNUSED(manager);
    UNUSED(_level_context);

    FURI_LOG_D(GAME_NAME, "Menu level started");
}

const LevelBehaviour level_menu = {
    .alloc = level_menu_alloc,
    .free = NULL,
    .start = level_menu_start,
    .stop = NULL,
    .context_size = sizeof(LevelMenuContext),
};
