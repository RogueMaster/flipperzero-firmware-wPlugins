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

#pragma once

#include "src/engine/entity.h"

#define ENEMY_SIZE 6.0f
#define HALF_ENEMY_SIZE 3.0f

typedef struct Sprite Sprite;

typedef enum
{
    EnemyDirectionUp,
    EnemyDirectionDown,
    EnemyDirectionLeft,
    EnemyDirectionRight,
} EnemyDirection;

typedef struct
{
    Sprite* sprite;
    EnemyDirection direction;
    float speed;
} EnemyContext;

void
enemy_spawn(GameManager* manager);

extern const EntityDescription enemy_description;
