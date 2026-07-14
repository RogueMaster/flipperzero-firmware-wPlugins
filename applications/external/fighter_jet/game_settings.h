
/* Copyright (c) 2025 Eero Prittinen */

#pragma once

#define FRAMERATE (20)
#define FOV (PI2 / 8)

/* MAP SETTINGS */
#define MAX_ENEMY_DISTANCE (120)
#define ENEMY_COUNT_START (2)
#define ADD_ENEMIES_PER_ROUND (2)
#define MAX_ENEMY_COUNT (12)

/* PLAYER SETTINGS */
#define SPEED ((double) 0.5)
#define TURNING_SPEED ((PI2 / FRAMERATE) / 8)
#define BRAKE_SPEED ((double) 0.1)
#define MAX_HEALTH (100)

/* ENEMY SETTINGS */
#define ENEMY_SPEED ((double) 0.3)
#define ENEMY_TURNING_SPEED ((PI2 / FRAMERATE) / 12)
#define ENEMY_HEALTH (50)
#define ENEMY_SHOOTING_DISTANCE (50)

/* GRAPHIGS SERTTINGS */
#define COMPASS_WIDTH 44
#define ROUND_TITLE_TIME (3 * FRAMERATE)
#define MINIMAP_X 0
#define MINIMAP_Y 0
#define MINIMAP_R 10
