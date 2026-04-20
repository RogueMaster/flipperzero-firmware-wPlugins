#pragma once
#include "pico-game-engine/engine/draw.hpp"

/*
Let's stay away from storage from now, but I may switch to it
For now I'll just hardcode them..
*/

#define ANIMATION_MAX_CLIPS 10

typedef struct
{
    const char *name;
    void (*update)(Draw *draw, uint16_t tick); // function pointer for the update callback
    uint16_t ticks;                            // duration of the clip
} AnimationClip;

typedef struct
{
    const char *name;
    AnimationClip *clips; // pointer to an array of clips
    uint16_t clipCount;   // number of clips in the animation
} AnimationSequence;

class Animation
{
public:
    Animation();
    ~Animation();
    bool addClip(const AnimationClip &clip); // adds a clip to the current sequence
    void render(Draw *draw);                 // renders the current frame of the animation using the provided Draw object
    void tick();                             // advances the animation by one tick, updating the current clip and sequence as necessary

private:
    uint16_t clipIndex;     // index of the current tick within the current clip
    uint16_t sequenceIndex; // index of the current clip in the sequence
    AnimationSequence currentSequence;
    uint32_t time;
};