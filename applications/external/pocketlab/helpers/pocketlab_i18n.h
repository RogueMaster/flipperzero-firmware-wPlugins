#pragma once

typedef enum {
    PocketLabTextAppName,
    PocketLabTextMenuStart,
    PocketLabTextMenuProgress,
    PocketLabTextMenuSettings,
    PocketLabTextMenuAbout,
    PocketLabTextLabsTitle,
    PocketLabTextSettingsSound,
    PocketLabTextSettingsReset,
    PocketLabTextOn,
    PocketLabTextOff,
    PocketLabTextCorrect,
    PocketLabTextTryAgain,
    PocketLabTextLabComplete,
    PocketLabTextCount,
} PocketLabTextId;

const char* pocketlab_text(PocketLabTextId id);
