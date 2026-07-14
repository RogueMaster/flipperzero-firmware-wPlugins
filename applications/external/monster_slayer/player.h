#ifndef PLAYER_H
#define PLAYER_H

#include "game.h"

typedef struct {
    Sprite* sprite;
    float Yvelocity;
    bool is_swinging_sword;
    bool is_hitting;
    int weapon_damage;
} PlayerContext;

extern float player_sword_reach;

extern Entity* player;

extern float ground_hight;

extern bool is_player_facing_right;

extern const EntityDescription player_desc;

void player_spawn(Level* level, GameManager* manager);

extern void Animations(GameManager* manager, void* context);

#endif
