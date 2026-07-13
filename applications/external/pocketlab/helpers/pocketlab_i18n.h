#pragma once

typedef enum {
    PocketLabTextAppName,
    PocketLabTextMenuStart,
    PocketLabTextMenuQuiz,
    PocketLabTextMenuProgress,
    PocketLabTextMenuSettings,
    PocketLabTextMenuAbout,
    PocketLabTextLabsTitle,
    PocketLabTextSettingsSound,
    PocketLabTextSettingsLed,
    PocketLabTextSettingsVibro,
    PocketLabTextSettingsReset,
    PocketLabTextOn,
    PocketLabTextOff,
    PocketLabTextCorrect,
    PocketLabTextTryAgain,
    PocketLabTextLabComplete,
    PocketLabTextCount,
} PocketLabTextId;

const char* pocketlab_text(PocketLabTextId id);
