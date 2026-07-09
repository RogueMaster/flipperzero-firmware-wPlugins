#pragma once

#include <notification/notification.h>

typedef enum {
    PocketLabSoundClick,
    PocketLabSoundCorrect,
    PocketLabSoundWrong,
    PocketLabSoundReward,
    PocketLabSoundCoin,
    PocketLabSoundType,
    PocketLabSoundGeiger,
    PocketLabSoundSip,
    PocketLabSoundComplete,
    PocketLabSoundReset,
    PocketLabSoundThud,
    PocketLabSoundSplat,
    PocketLabSoundFly,
    PocketLabSoundGlitch,
    PocketLabSoundGrow,
} PocketLabSoundId;

void pocketlab_sound_play(NotificationApp* notifications, bool enabled, PocketLabSoundId sound);

/** Plays a short jingle unique to a badge index (for the achievement gallery). */
void pocketlab_sound_play_jingle(NotificationApp* notifications, bool enabled, uint32_t index);
