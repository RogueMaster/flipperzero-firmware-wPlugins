#ifndef SKELETON_H
#define SKELETON_H

#include "game.h"

typedef struct {
    Sprite* sprite;
    bool is_going_right;
    float Yvelocity;
} SkeletonContext;

void skeleton_spawn(Level* level, GameManager* manager);

extern const EntityDescription skel_desc;

#endif
