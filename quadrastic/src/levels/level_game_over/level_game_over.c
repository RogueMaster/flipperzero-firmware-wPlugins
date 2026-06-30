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

#include "level_game_over.h"

#include <stddef.h>

#include <dolphin/dolphin.h>
#include <furi.h>

#include "src/game.h"
#include "src/game_settings.h"

#include "game_over_entity.h"

typedef struct
{
    Entity* entitiy;
} GameOverLevelContext;

static void
level_game_over_alloc(Level* level, GameManager* manager, void* context)
{
    GameOverLevelContext* game_over_context = context;

    game_over_context->entitiy =
      level_add_entity(level, &game_over_description);
    GameOverEntityContext* entity_context =
      entity_context_get(game_over_context->entitiy);
    entity_context->logo_sprite =
      game_manager_sprite_load(manager, "game_over.fxbm");

    entity_context->score = 0;
    entity_context->max_score = 0;
}

static void
level_game_over_start(Level* level, GameManager* manager, void* _level_context)
{
    UNUSED(level);

    GameOverLevelContext* level_context = _level_context;
    GameOverEntityContext* entity_context =
      entity_context_get(level_context->entitiy);

    GameContext* game_context = game_manager_game_context_get(manager);
    if (game_context->score > game_context->best_score) {
        game_context->best_score = game_context->score;
        game_save_settings(game_context);
    }

    entity_context->score = game_context->score;
    entity_context->max_score = game_context->best_score;

    if (entity_context->score >= WIN_SCORE) {
        dolphin_deed(DolphinDeedPluginGameWin);
    }

    FURI_LOG_D(GAME_NAME, "Game over level started");
}

const LevelBehaviour level_game_over = {
    .alloc = level_game_over_alloc,
    .free = NULL,
    .start = level_game_over_start,
    .stop = NULL,
    .context_size = sizeof(GameOverLevelContext),
};
