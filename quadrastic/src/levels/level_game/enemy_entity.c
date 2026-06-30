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

#include "enemy_entity.h"

#include "src/engine/game_manager.h"
#include "src/game.h"
#include "src/game_notifications.h"

#include "level_game.h"
#include "player_entity.h"

static Vector
random_pos(Entity* player_entity)
{
    const int full_size = ceilf(ENEMY_SIZE);
    const int half_size = ceilf(HALF_ENEMY_SIZE);

    Vector player_pos = entity_pos_get(player_entity);

    Vector pos;

    do {
        pos.x = half_size + rand() % (SCREEN_WIDTH - full_size);
    } while (fabsf(pos.x - player_pos.x) < 3 * ENEMY_SIZE);
    do {
        pos.y = half_size + rand() % (SCREEN_HEIGHT - full_size);
    } while (fabsf(pos.y - player_pos.y) < 3 * ENEMY_SIZE);

    return pos;
}

static EnemyDirection
random_direction(Vector enemy_pos, Entity* player_entity)
{
    Vector player_pos = entity_pos_get(player_entity);

    EnemyDirection direction = rand() % 4;
    if (direction == EnemyDirectionUp && enemy_pos.y > player_pos.y)
        direction = EnemyDirectionDown;
    else if (direction == EnemyDirectionDown && enemy_pos.y < player_pos.y)
        direction = EnemyDirectionUp;
    else if (direction == EnemyDirectionLeft && enemy_pos.x > player_pos.x)
        direction = EnemyDirectionRight;
    else if (direction == EnemyDirectionRight && enemy_pos.x < player_pos.x)
        direction = EnemyDirectionLeft;

    return direction;
}

void
enemy_spawn(GameManager* manager)
{
    GameContext* game_context = game_manager_game_context_get(manager);
    Level* level = game_manager_current_level_get(manager);

    // Add enemyh to level
    Entity* enemy = level_add_entity(level, &enemy_description);
    EnemyContext* enemy_context = entity_context_get(enemy);

    // Add to enemies list
    GameLevelContext* level_context = level_context_get(level);
    EntityList_push_back(level_context->enemies, enemy);

    // Set enemy position
    Vector enemy_pos = random_pos(level_context->player);
    entity_pos_set(enemy, enemy_pos);
    enemy_context->direction =
      random_direction(enemy_pos, level_context->player);

    // Add collision rect to enemy entity
    entity_collider_add_rect(enemy, ENEMY_SIZE, ENEMY_SIZE);

    // Load target sprite
    enemy_context->sprite = game_manager_sprite_load(manager, "enemy.fxbm");

    float speed;
    switch (game_context->difficulty) {
        case DifficultyEasy:
            speed = 0.25f;
            break;
        case DifficultyHard:
            speed = 1.0f;
            break;
        case DifficultyInsane:
            speed = 0.25f * (4 + rand() % 4);
            break;
        default:
            speed = 0.5f;
            break;
    }
    enemy_context->speed = speed;
}

static void
enemy_update(Entity* self, GameManager* manager, void* context)
{
    // Check pause
    Level* level = game_manager_current_level_get(manager);
    GameLevelContext* level_context = level_context_get(level);
    if (level_context->is_paused) {
        return;
    }

    EnemyContext* enemy_context = context;
    Vector pos = entity_pos_get(self);

    // Control player movement
    if (enemy_context->direction == EnemyDirectionUp)
        pos.y -= enemy_context->speed;
    if (enemy_context->direction == EnemyDirectionDown)
        pos.y += enemy_context->speed;
    if (enemy_context->direction == EnemyDirectionLeft)
        pos.x -= enemy_context->speed;
    if (enemy_context->direction == EnemyDirectionRight)
        pos.x += enemy_context->speed;

    // Clamp enemy position to screen bounds, and set it
    pos.x = CLAMP(pos.x, SCREEN_WIDTH - HALF_ENEMY_SIZE, HALF_ENEMY_SIZE);
    pos.y = CLAMP(pos.y, SCREEN_HEIGHT - HALF_ENEMY_SIZE, HALF_ENEMY_SIZE);
    entity_pos_set(self, pos);

    // Switching direction
    if (enemy_context->direction == EnemyDirectionUp &&
        pos.y <= HALF_ENEMY_SIZE)
        enemy_context->direction = EnemyDirectionDown;
    else if (enemy_context->direction == EnemyDirectionDown &&
             pos.y >= SCREEN_HEIGHT - HALF_ENEMY_SIZE)
        enemy_context->direction = EnemyDirectionUp;
    else if (enemy_context->direction == EnemyDirectionLeft &&
             pos.x <= HALF_ENEMY_SIZE)
        enemy_context->direction = EnemyDirectionRight;
    else if (enemy_context->direction == EnemyDirectionRight &&
             pos.x >= SCREEN_WIDTH - HALF_ENEMY_SIZE)
        enemy_context->direction = EnemyDirectionLeft;
}

static void
enemy_render(Entity* self, GameManager* manager, Canvas* canvas, void* context)
{
    UNUSED(context);
    UNUSED(manager);

    // Get enemy position
    Vector pos = entity_pos_get(self);

    // Draw enemy
    EnemyContext* enemy_context = entity_context_get(self);
    canvas_draw_sprite(canvas,
                       enemy_context->sprite,
                       pos.x - HALF_ENEMY_SIZE,
                       pos.y - HALF_ENEMY_SIZE);
}

static void
enemy_collision(Entity* self,
                Entity* other,
                GameManager* manager,
                void* context)
{
    UNUSED(self);
    UNUSED(context);

    if (entity_description_get(other) != &player_description) {
        return;
    }

    // Game over
    GameContext* game_context = game_manager_game_context_get(manager);
    game_notify(game_context, &sequence_game_over);
    game_manager_next_level_set(manager, game_context->levels.game_over);
}

const EntityDescription enemy_description = {
    .start = NULL,
    .stop = NULL,
    .update = enemy_update,
    .render = enemy_render,
    .collision = enemy_collision,
    .event = NULL,
    .context_size = sizeof(EnemyContext),
};
