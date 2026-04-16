#include "animation.hpp"

Animation::Animation()
{
    clipIndex = 0;
    time = 0;
    sequenceIndex = 0;
    currentSequence.clipCount = 0;
}

Animation::~Animation()
{
    // nothing to do here
}

bool Animation::addClip(const AnimationClip &clip)
{
    if (sequenceIndex >= ANIMATION_MAX_CLIPS)
    {
        return false; // Can't add more clips
    }
    currentSequence.clips[sequenceIndex++] = clip;
    currentSequence.clipCount++;
    return true;
}

void Animation::render(Draw *draw)
{
    if (currentSequence.clipCount == 0)
    {
        return; // No clips to render
    }
    AnimationClip &currentClip = currentSequence.clips[sequenceIndex];
    if (currentClip.update)
    {
        currentClip.update(draw, clipIndex);
    }
}

void Animation::tick()
{
    time++;
    if (currentSequence.clipCount == 0)
    {
        return; // No clips to play
    }
    AnimationClip &currentClip = currentSequence.clips[sequenceIndex];
    if (clipIndex >= currentClip.ticks)
    {
        clipIndex = 0;
        sequenceIndex = (sequenceIndex + 1) % currentSequence.clipCount; // Loop back to the first clip
    }
    else
    {
        clipIndex++;
    }
}