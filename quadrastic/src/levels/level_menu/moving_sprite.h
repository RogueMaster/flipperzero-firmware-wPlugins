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
#include "src/engine/vector.h"

typedef struct Sprite Sprite;

typedef struct
{
    Sprite* sprite;
    Vector pos_start;
    Vector pos_end;
    float duration;
    float time;
} MovingSpriteContext;

Entity*
moving_sprite_add_to_level(Level* level,
                           GameManager* manager,
                           Vector pos_start,
                           Vector pos_end,
                           float duration,
                           const char* sprite_name);

extern const EntityDescription moving_sprite_description;
