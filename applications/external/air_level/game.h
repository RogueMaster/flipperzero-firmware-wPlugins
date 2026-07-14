#pragma once
#include "engine/engine.h"
#include "engine/sensors/imu.h"

typedef struct {
    Imu* imu;
    bool imu_present;
    Level* level;
    GameManager* game_manager;
} GameContext;
