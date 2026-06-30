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

#include "menu_entity.h"

#include "src/engine/entity.h"
#include "src/engine/game_manager.h"
#include "src/engine/level.h"

#include "src/game.h"

static void
menu_update(Entity* self, GameManager* manager, void* _entity_context)
{
    UNUSED(self);
    UNUSED(_entity_context);

    GameContext* game_context = game_manager_game_context_get(manager);
    Level* level = game_manager_current_level_get(manager);

    InputState input = game_manager_input_get(manager);
    if (input.pressed & GameKeyBack) {
        game_manager_game_stop(manager);
    } else if (input.pressed & GameKeyOk) {
        game_manager_next_level_set(manager, game_context->levels.game);
        level_send_event(
          level, self, NULL, GameEventSkipAnimation, (EntityEventValue){ 0 });
    } else if (input.pressed & GameKeyLeft) {
        game_manager_next_level_set(manager, game_context->levels.settings);
        level_send_event(
          level, self, NULL, GameEventSkipAnimation, (EntityEventValue){ 0 });
    } else if (input.pressed & GameKeyRight) {
        game_manager_next_level_set(manager, game_context->levels.about);
        level_send_event(
          level, self, NULL, GameEventSkipAnimation, (EntityEventValue){ 0 });
    }
}

const EntityDescription menu_description = {
    .start = NULL,
    .stop = NULL,
    .update = menu_update,
    .render = NULL,
    .collision = NULL,
    .event = NULL,
    .context_size = 0,
};
