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

#include <m-list.h>

#include "src/engine/level.h"

LIST_DEF(EntityList, Entity*, M_POD_OPLIST);
#define M_OPL_EntityList_t() LIST_OPLIST(EntityList)

typedef struct
{
    Entity* pause_menu;
    Entity* player;
    Entity* target;
    EntityList_t enemies;

    bool is_paused;

} GameLevelContext;

void
pause_game(Level* level);

void
resume_game(Level* level);

extern const LevelBehaviour level_game;
