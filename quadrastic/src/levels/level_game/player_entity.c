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

#include "player_entity.h"

#include "src/engine/game_manager.h"
#include "src/game.h"

#include "level_game.h"

#define PLAYER_SIZE 6
#define PLAYER_SPEED 1
#define PLAYER_INITIAL_X 64
#define PLAYER_INITIAL_Y 32
#define PLAYER_ANIMATION_DURATION 15.0f

Entity*
player_spawn(Level* level, GameManager* manager)
{
    Entity* player = level_add_entity(level, &player_description);

    // Set position and collider rect
    entity_pos_set(player, (Vector){ PLAYER_INITIAL_X, PLAYER_INITIAL_Y });
    entity_collider_add_rect(player, PLAYER_SIZE, PLAYER_SIZE);

    // Load player sprite
    PlayerContext* player_context = entity_context_get(player);
    player_context->sprite = game_manager_sprite_load(manager, "player.fxbm");

    return player;
}

void
player_respawn(Entity* player)
{
    // Set player position.
    entity_pos_set(player, (Vector){ PLAYER_INITIAL_X, PLAYER_INITIAL_Y });

    // Reset animation
    PlayerContext* player_context = entity_context_get(player);
    player_context->time = 0.0f;
}

static void
player_update(Entity* self, GameManager* manager, void* _entity_context)
{
    // Check pause
    Level* level = game_manager_current_level_get(manager);
    GameLevelContext* level_context = level_context_get(level);
    if (level_context->is_paused) {
        return;
    }

    // Start level animation
    PlayerContext* player = _entity_context;
    if (player->time < PLAYER_ANIMATION_DURATION) {
        player->time += 1.0f;
        return;
    }

    InputState input = game_manager_input_get(manager);
    Vector pos = entity_pos_get(self);

    // Control player movement
    if (input.held & GameKeyUp)
        pos.y -= PLAYER_SPEED;
    if (input.held & GameKeyDown)
        pos.y += PLAYER_SPEED;
    if (input.held & GameKeyLeft)
        pos.x -= PLAYER_SPEED;
    if (input.held & GameKeyRight)
        pos.x += PLAYER_SPEED;

    // Clamp player position to screen bounds, and set it
    pos.x = CLAMP(pos.x, 125, 3);
    pos.y = CLAMP(pos.y, 61, 3);
    entity_pos_set(self, pos);

    // Control game pause
    if (input.pressed & GameKeyBack) {
        pause_game(level);
    }
}

static void
player_render(Entity* self,
              GameManager* manager,
              Canvas* canvas,
              void* _entity_context)
{
    PlayerContext* player = _entity_context;

    // Draw initial animation
    if (player->time < PLAYER_ANIMATION_DURATION) {
        float step = 61 / PLAYER_ANIMATION_DURATION;
        float left_x = player->time * step;
        float right_x = 122 - left_x;
        left_x = CLAMP(left_x, 61, 0);
        right_x = CLAMP(right_x, 122, 61);

        canvas_draw_sprite(canvas, player->sprite, left_x, 29);
        canvas_draw_sprite(canvas, player->sprite, right_x, 29);

        return;
    }

    // Draw player sprite
    Vector pos = entity_pos_get(self);
    canvas_draw_sprite(canvas, player->sprite, pos.x - 3, pos.y - 3);

    // Draw score
    GameContext* game_context = game_manager_game_context_get(manager);
    canvas_printf_aligned(
      canvas, 64, 0, AlignCenter, AlignTop, "%lu", game_context->score);
}

const EntityDescription player_description = {
    .start = NULL,
    .stop = NULL,
    .update = player_update,
    .render = player_render,
    .collision = NULL,
    .event = NULL,
    .context_size = sizeof(PlayerContext),
};
