#pragma once

#include <stdbool.h>
#include <stdint.h>

enum {
    LaneCount = 3,
    ReefCount = 2,
    ReefSpacing = 84,
    DolphinLeft = 12,
    DolphinRight = 26,
    RockRadius = 5,
    BubbleRadius = 3,
    BubbleLead = 36,
};

typedef enum {
    DashTitle,
    DashSwimming,
    DashOver
} DashPhase;

typedef struct {
    int x;
    unsigned lane;
    bool has_bubble;
} Reef;

typedef struct {
    DashPhase phase;
    unsigned lane;
    unsigned score;
    unsigned best;
    unsigned rocks_passed;
    unsigned frame;
    Reef reefs[ReefCount];
} DashGame;

void dash_start(DashGame* game, uint32_t random);
void dash_move(DashGame* game, int direction);
void dash_tick(DashGame* game, unsigned (*next_lane)(void));
