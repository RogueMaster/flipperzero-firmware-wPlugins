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

#include "level_game.h"

#include <stddef.h>

#include <m-list.h>

#include <dolphin/dolphin.h>

#include "src/game.h"

#include "context_menu.h"

#include "player_entity.h"
#include "target_entity.h"

static void
on_resume_clicked(void* context, uint32_t index)
{
    UNUSED(index);
    resume_game(context);
}

static void
on_menu_clicked(void* context, uint32_t index)
{
    UNUSED(index);
    GameManager* manager = context;
    GameContext* game_context = game_manager_game_context_get(manager);
    game_manager_next_level_set(manager, game_context->levels.menu);
}

static void
on_quit_clicked(void* context, uint32_t index)
{
    UNUSED(index);
    GameManager* manager = context;
    game_manager_game_stop(manager);
}

static void
level_game_alloc(Level* level, GameManager* manager, void* _level_context)
{
    GameLevelContext* level_context = _level_context;

    // Add entities to the level
    level_context->player = player_spawn(level, manager);
    level_context->target = target_create(level, manager);
    EntityList_init(level_context->enemies);

    // Pause menu initialization
    level_context->is_paused = false;
    level_context->pause_menu =
      level_add_entity(level, &context_menu_description);

    context_menu_add_item(
      level_context->pause_menu, "Resume", 0, on_resume_clicked, level);
    context_menu_add_item(
      level_context->pause_menu, "Menu", 1, on_menu_clicked, manager);
    context_menu_add_item(
      level_context->pause_menu, "Quit", 2, on_quit_clicked, manager);

    context_menu_back_callback_set(
      level_context->pause_menu, (ContextMenuBackCallback)resume_game, level);
}

static void
level_game_start(Level* level, GameManager* manager, void* context)
{
    UNUSED(level);

    dolphin_deed(DolphinDeedPluginGameStart);

    GameContext* game_context = game_manager_game_context_get(manager);
    game_context->score = 0;

    GameLevelContext* level_context = context;
    player_respawn(level_context->player);
    target_reset(level_context->target, manager);

    FURI_LOG_D(GAME_NAME, "Game level started");
}

static void
level_game_stop(Level* level, GameManager* manager, void* _level_context)
{
    UNUSED(manager);

    GameLevelContext* level_context = _level_context;

    // Clear enemies
    for
        M_EACH(item, level_context->enemies, EntityList_t)
        {
            level_remove_entity(level, *item);
        }
    EntityList_clear(level_context->enemies);

    resume_game(level);
}

void
pause_game(Level* level)
{
    GameLevelContext* level_context = level_context_get(level);
    if (level_context->is_paused) {
        return;
    }

    level_context->is_paused = true;
    context_menu_reset_state(level_context->pause_menu);
}

void
resume_game(Level* level)
{
    GameLevelContext* level_context = level_context_get(level);
    level_context->is_paused = false;
}

const LevelBehaviour level_game = {
    .alloc = level_game_alloc,
    .free = NULL,
    .start = level_game_start,
    .stop = level_game_stop,
    .context_size = sizeof(GameLevelContext),
};
