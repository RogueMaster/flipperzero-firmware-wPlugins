#ifndef PLAYER_ANIMATIONS_H
#define PLAYER_ANIMATIONS_H

#include "game.h"
#include "player.h"

extern Sprite* jumping[];

extern Sprite* idle[];
extern Sprite* sword_swing[];
extern Sprite* walking[];

extern Sprite* idle_right[];
extern Sprite* sword_swing_right[];
extern Sprite* walking_right[];

void Jumping_animations_load(GameManager* manager);

void Idle_animation_load(GameManager* manager);
void Swinging_sword_animation_load(GameManager* manager);
void Walking_animation_load(GameManager* manager);

void Idle_animation_right_load(GameManager* manager);
void Swinging_sword_right_animation_load(GameManager* manager);
void Walking_right_animation_load(GameManager* manager);

void Idle_animation_play(GameManager* manager, void* context);
void Swinging_sword_animation_play(GameManager* manager, void* context);
void Walking_animation_play(GameManager* manager, void* context);

void Idle_animation_right_play(GameManager* manager, void* context);
void Swinging_sword_animation_right_play(GameManager* manager, void* context);
void Walking_right_animation_play(GameManager* manager, void* context);

#endif
