#pragma once
#include <furi.h>

typedef enum {
    TitleIndexStart = 0, // switch to lobby options (local or online)
    TitleIndexMenu = 1, // switch to system menu
} TitleIndex;

typedef enum {
    MenuIndexProfile = 0, // profile
    MenuIndexMap = 1, // map
    MenuIndexSettings = 2, // settings
    MenuIndexAbout = 3, // about
} MenuIndex;

typedef enum {
    MenuSettingsMain = 0, // hovering over `Settings` in system menu
    MenuSettingsSound = 1, // sound on/off
    MenuSettingsVibration = 2, // vibration on/off
    MenuSettingsLeave = 3, // leave game
} MenuSettingsIndex;

typedef enum {
    LobbyMenuLocal = 0, // local game
    LobbyMenuOnline = 1, // online game
} LobbyMenuIndex;

typedef enum {
    ToggleOn, // On
    ToggleOff, // Off
} ToggleState;

typedef enum {
    GameStatePlaying = 0, // Game is currently playing
    GameStateMenu = 1, // Game is in menu state
    GameStateSwitchingLevels = 2, // Game is switching levels
    GameStateLeavingGame = 3, // Game is leaving
} GameState;

inline bool toggleToBool(ToggleState state) noexcept {
    return state == ToggleOn;
}
inline const char* toggleToString(ToggleState state) noexcept {
    return state == ToggleOn ? "On" : "Off";
}
