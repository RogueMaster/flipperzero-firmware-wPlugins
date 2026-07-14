/* Copyright (c) 2025 Eero Prittinen */

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "game_settings.h"

#define PI2 ((double) 6.28318530718)

typedef enum {
  TURNING_NONE,
  TURNING_LEFT,
  TURNING_RIGHT
} turning_t;

typedef turning_t (*autopilot_algorithm_t)(double dist, double dir, double enemyDir, double ownDir);

typedef struct {
  double dir;
  double dist;
  double ownDir;
  int health;
  autopilot_algorithm_t autopilot;
} fighter_jet_t;

typedef struct {
  double dir;
  turning_t turning;
  bool shooting;
  bool braking;
  int health;
  fighter_jet_t enemies[MAX_ENEMY_COUNT];
  size_t enemyCount;
  int round;
  uint32_t roundStartTs;
} game_state_t;
