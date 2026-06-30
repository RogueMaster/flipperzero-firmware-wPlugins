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

#include <stdint.h>

#include "engine/engine.h" // IWYU pragma: keep

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

#define GAME_NAME "Quadrastic"

#define WIN_SCORE 15

typedef enum
{
    DifficultyEasy,
    DifficultyNormal,
    DifficultyHard,
    DifficultyInsane,
    DifficultyCount,
} Difficulty;

typedef enum
{
    StateOff,
    StateOn,
    StateCount,
} State;

typedef enum
{
    GameEventSkipAnimation,
} GameEvent;

typedef struct
{
    Level* menu;
    Level* game;
    Level* game_over;
    Level* settings;
    Level* about;
} Levels;

typedef struct NotificationApp NotificationApp;

typedef struct
{
    NotificationApp* notification;

    Levels levels;
    Difficulty difficulty;

    uint32_t best_score;
    uint32_t score;

    State sound;
    State vibro;
    State led;

} GameContext;
