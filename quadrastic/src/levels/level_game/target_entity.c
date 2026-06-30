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

#include "target_entity.h"

#include "src/engine/game_manager.h"
#include "src/engine/level.h"

#include "src/game.h"
#include "src/game_notifications.h"

#include "enemy_entity.h"
#include "level_game.h"
#include "player_entity.h"

#define TARGET_ANIMATION_DURATION 30.0f
#define TARGET_ANIMATION_RADIUS 10.0f
#define TARGET_SIZE 3.0f
#define HALF_TARGET_SIZE 1.5f

static Vector
random_pos(Entity* player_entity)
{
    const int full_size = ceilf(TARGET_SIZE);
    const int half_size = ceilf(HALF_TARGET_SIZE);

    Vector player_pos = entity_pos_get(player_entity);

    Vector pos;
    do {
        pos.x = half_size + rand() % (SCREEN_WIDTH - full_size);
    } while (fabsf(pos.x - player_pos.x) < 2 * TARGET_SIZE);
    do {
        pos.y = half_size + rand() % (SCREEN_HEIGHT - full_size);
    } while (fabsf(pos.y - player_pos.y) < 2 * TARGET_SIZE);

    return pos;
}

Entity*
target_create(Level* level, GameManager* manager)
{
    Entity* target = level_add_entity(level, &target_description);

    // Set target position
    GameLevelContext* level_context = level_context_get(level);
    entity_pos_set(target, random_pos(level_context->player));

    // Add collision rect to target entity
    entity_collider_add_rect(target, TARGET_SIZE, TARGET_SIZE);

    // Load target sprite
    TargetContext* target_context = entity_context_get(target);
    target_context->sprite = game_manager_sprite_load(manager, "target.fxbm");

    return target;
}

void
target_reset(Entity* self, GameManager* manager)
{
    // Set target position.
    Level* level = game_manager_current_level_get(manager);
    GameLevelContext* level_context = level_context_get(level);
    entity_pos_set(self, random_pos(level_context->player));

    // Reset animation
    TargetContext* target_context = entity_context_get(self);
    target_context->time = 0.0f;
}

static void
target_update(Entity* self, GameManager* manager, void* _entity_context)
{
    UNUSED(self);

    // Check pause
    Level* level = game_manager_current_level_get(manager);
    GameLevelContext* level_context = level_context_get(level);
    if (level_context->is_paused) {
        return;
    }

    // Update animation
    TargetContext* target_context = _entity_context;
    if (target_context->time < TARGET_ANIMATION_DURATION) {
        target_context->time += 1.0f;
    }
}

static void
target_render(Entity* self,
              GameManager* manager,
              Canvas* canvas,
              void* _entity_context)
{
    UNUSED(manager);

    Vector pos = entity_pos_get(self);
    TargetContext* target_context = _entity_context;

    // Draw animation
    if (target_context->time < TARGET_ANIMATION_DURATION) {
        float step = TARGET_ANIMATION_RADIUS / TARGET_ANIMATION_DURATION;
        float radius =
          CLAMP(TARGET_ANIMATION_RADIUS - target_context->time * step,
                TARGET_ANIMATION_RADIUS,
                HALF_TARGET_SIZE);
        canvas_draw_circle(canvas, pos.x, pos.y, radius);
        canvas_draw_dot(canvas, pos.x, pos.y);
        return;
    }

    // Draw target
    canvas_draw_sprite(canvas, target_context->sprite, pos.x - 1, pos.y - 1);
}

static void
target_collision(Entity* self,
                 Entity* other,
                 GameManager* manager,
                 void* _entity_context)
{
    UNUSED(_entity_context);

    if (entity_description_get(other) != &player_description) {
        return;
    }

    // Increase score
    GameContext* game_context = game_manager_game_context_get(manager);
    ++game_context->score;

    // Move target to new random position
    target_reset(self, manager);
    enemy_spawn(manager);

    // Notify
    game_notify(game_context, &sequence_earn_point);
}

const EntityDescription target_description = {
    .start = NULL,
    .stop = NULL,
    .update = target_update,
    .render = target_render,
    .collision = target_collision,
    .event = NULL,
    .context_size = sizeof(TargetContext),
};
