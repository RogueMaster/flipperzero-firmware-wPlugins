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

#include "level_about.h"

#include <stddef.h>

#include "src/game.h"

#include "about_entity.h"

static void
level_about_alloc(Level* level, GameManager* manager, void* _level_context)
{
    UNUSED(manager);
    UNUSED(_level_context);

    Entity* entity = level_add_entity(level, &about_description);
    AboutContext* entity_context = entity_context_get(entity);
    entity_context->logo_sprite =
      game_manager_sprite_load(manager, "icon.fxbm");

    FURI_LOG_D(GAME_NAME, "About level allocated");
}

static void
level_about_start(Level* level, GameManager* manager, void* _level_context)
{
    UNUSED(level);
    UNUSED(manager);
    UNUSED(_level_context);

    FURI_LOG_D(GAME_NAME, "About level started");
}

const LevelBehaviour level_about = {
    .alloc = level_about_alloc,
    .free = NULL,
    .start = level_about_start,
    .stop = NULL,
    .context_size = 0,
};
