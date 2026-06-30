#pragma once
typedef enum
{
    ENTITY_IDLE,
    ENTITY_MOVING,
    ENTITY_MOVING_TO_START,
    ENTITY_MOVING_TO_END,
    ENTITY_ATTACKING,
    ENTITY_ATTACKED,
    ENTITY_DEAD
} EntityState;

typedef enum
{
    ENTITY_UP,
    ENTITY_DOWN,
    ENTITY_LEFT,
    ENTITY_RIGHT
} EntityDirection;

#include "engine/engine.h"
#include <engine/level_i.h>
#include "flip_world.h"
#include <game/world.h>
#include <game/level.h>
#include <game/story.h>
#include <game/enemy.h>
#include <game/player.h>
#include <game/npc.h>
