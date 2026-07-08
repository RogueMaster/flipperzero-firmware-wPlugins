#pragma once

#include <notification/notification.h>

typedef enum {
    PocketLabSoundClick,
    PocketLabSoundCorrect,
    PocketLabSoundWrong,
    PocketLabSoundReward,
} PocketLabSoundId;

void pocketlab_sound_play(NotificationApp* notifications, bool enabled, PocketLabSoundId sound);
