#pragma once
#include <game/game.h>
#include "flip_world.h"
#define STORY_TUTORIAL_STEPS 9
void story_draw(Entity *player, Canvas *canvas, GameManager *manager);
void story_update(Entity *player, GameManager *manager);
const LevelBehaviour *story_world();