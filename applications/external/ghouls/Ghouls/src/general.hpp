#pragma once
#include "config.hpp"

#define FIELD_OF_VIEW 30 // see up to 30 around us
#define FIELD_OF_VIEW_SQUARED 900

#define TICKS_PER_DAY 3600 // 60 seconds at 60fps

#define MAP_WIDTH 96
#define MAP_HEIGHT 48

#define MAP_WALL_HEIGHT 1.0f
#define MAP_WALL_DEPTH 0.2f

#define MAP_OUTER_WALLS 4

#define WALL_SEGMENT_SIZE 8.0f
#define WALL_H_SEGMENT_COUNT ((MAP_WIDTH / 8) * 2)  // top + bottom walls
#define WALL_V_SEGMENT_COUNT ((MAP_HEIGHT / 8) * 2) // left + right walls
#define WALL_SEGMENT_COUNT (WALL_H_SEGMENT_COUNT + WALL_V_SEGMENT_COUNT)

#define ENEMY_SPAWN_MAX 5 // about 10kb if max_triangles is set to 48
#define ENEMY_HEALTH_BASE 100
#define ENEMY_HEALTH_INCREMENT 5
#define ENEMY_STRENGTH_INCREMENT 1

#define WEAPON_SPAWN_COUNT 4 // about 8kb if max_triangles is set to 48

#define TREE_SPAWN_COUNT 54

#define HOUSE_SPAWN_COUNT 6

typedef enum
{
    TIME_DAY = 0,
    TIME_NIGHT = 1,
} TimeOfDay;

typedef enum
{
    TitleIndexStart = 0, // switch to lobby options (local or online)
    TitleIndexMenu = 1,  // switch to system menu
} TitleIndex;

typedef enum
{
    MenuIndexProfile = 0,  // profile
    MenuIndexMap = 1,      // map
    MenuIndexSettings = 2, // settings
    MenuIndexAbout = 3,    // about
} MenuIndex;

typedef enum
{
    MenuSettingsMain = 0,       // hovering over `Settings` in system menu
    MenuSettingsSound = 1,      // sound on/off
    MenuSettingsVibration = 2,  // vibration on/off
    MenuSettingsShowPlayer = 3, // show/hide local player
    MenuSettingsLeave = 4,      // leave game
} MenuSettingsIndex;

typedef enum
{
    LobbyMenuLocal = 0,  // local game
    LobbyMenuOnline = 1, // online game
} LobbyMenuIndex;

typedef enum
{
    ToggleOn,  // On
    ToggleOff, // Off
} ToggleState;

typedef enum
{
    GameStatePlaying = 0,         // Game is currently playing
    GameStateMenu = 1,            // Game is in menu state
    GameStateSwitchingLevels = 2, // Game is switching levels
    GameStateLeavingGame = 3,     // Game is leaving
} GameState;

typedef enum
{
    OnlineStateIdle = 0,        // Not started — ready to create/join a session
    OnlineStateFetchingSession, // HTTP request to create a game session in progress
    OnlineStateConnecting,      // WebSocket connecting to the game server
    OnlineStatePlaying,         // Active online game
    OnlineStateJoiningExisting, // Joining an existing lobby (skip create)
    OnlineStateError,           // Connection or request error
} OnlineGameState;

inline bool toggleToBool(ToggleState state) noexcept { return state == ToggleOn; }
inline const char *toggleToString(ToggleState state) noexcept { return state == ToggleOn ? "On" : "Off"; }
inline uint16_t rgb565(uint32_t c)
{
    return (uint16_t)((((c) >> 19) & 0x1Fu) << 11 | (((c) >> 10) & 0x3Fu) << 5 | (((c) >> 3) & 0x1Fu));
}