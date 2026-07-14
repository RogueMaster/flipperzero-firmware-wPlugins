#pragma once
#include <engine/engine.h>
#include <engine/entity.h>
#include "game.h"
#include "game_menu.h"

#ifdef __cplusplus
extern "C" {
#endif

extern bool firstLevelCompleted;
extern bool startedGame;
extern bool hasSpawnedFirstMob;
void tutorial_level_player_update(Entity* self, GameManager* manager, void* context, Vector* pos);
void tutorial_level_player_render(GameManager* manager, Canvas* canvas, void* context);
void tutorial_level_enemy_render(GameManager* manager, Canvas* canvas, void* context);

#ifdef __cplusplus
}
#endif
