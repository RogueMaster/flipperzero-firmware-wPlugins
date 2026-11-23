#pragma once

#include <stdint.h>
#include <stdbool.h>

#define SCREEN_W 128
#define SCREEN_H 64

/* Chosen in the menu, used by the game. */
typedef enum {
    ThrustModeTapImpulse = 0,   // each tap of the thrust key = one fixed impulse
    ThrustModeBinary,           // hold the thrust key = full thrust, release = off
    ThrustModeRamp,             // hold the thrust key, thrust ramps up over time
    ThrustModeVidyaTap,         // VGM tilt steering + tap impulse
    ThrustModeVidyaBinary,      // VGM tilt steering + binary (held key) thrust
    ThrustModeVidyaRamp,        // VGM tilt steering + ramped (held key) thrust
    ThrustModeVidyaFull,        // VGM full tilt: tilt steers AND fires thrusters
    ThrustModeCount,
} ThrustMode;

extern const char* const thrust_mode_label[ThrustModeCount];

/* The key that fires the engine in every mode except Full Tilt. Set in
 * Settings. */
typedef enum {
    ThrustKeyUp = 0,
    ThrustKeyOk,
    ThrustKeyCount,
} ThrustKey;

extern const char* const thrust_key_label[ThrustKeyCount];

/* World seed, set in Settings: each seed is a different set of 30 levels.
 * Seed 1 is the original set. */
#define SEED_MIN 1
#define SEED_MAX 9999

/* Fuel modes — affects starting fuel and whether fuel refills between levels.
 * "Full" is the classic arcade behavior. The no-refuel modes give you a fixed
 * total tank for the entire game; crashing rewinds to that level's starting
 * fuel rather than draining what's left. */
typedef enum {
    FuelModeFull = 0,    // 100 fuel, refilled every level
    FuelModeEasy,        // 500 fuel total, no top-up
    FuelModeMed,         // 350 fuel total, no top-up
    FuelModeHard,        // 200 fuel total, no top-up
    FuelModeCount,
} FuelMode;

extern const char* const fuel_mode_label[FuelModeCount];
extern const char* const fuel_mode_desc[FuelModeCount];
extern const int        fuel_mode_starting[FuelModeCount];

typedef enum {
    ScreenMenu = 0,
    ScreenGame,
    ScreenTutorial,
    ScreenInfo,
    ScreenSettings,
    ScreenGameComplete,
    ScreenScore,
    ScreenCustomDifficulty,
} Screen;

typedef enum {
    DifficultyEasy = 0,  // safe thresholds doubled
    DifficultyMedium,    // baseline (original values)
    DifficultyHard,      // safe thresholds halved
    DifficultyRealistic, // Vy<1, Vx<1, angle<3°
    DifficultyCustom,    // player-set limits, never looser than Easy
    DifficultyCount,
} Difficulty;

extern const char* const difficulty_label[DifficultyCount];

/* Custom difficulty ranges: 1 up to Easy's limits. */
#define CUSTOM_VX_MAX     8
#define CUSTOM_VY_MAX     16
#define CUSTOM_ANGLE_MAX  25   // degrees

typedef enum {
    SoundOff = 0,
    SoundLow,
    SoundMed,
    SoundHigh,
    SoundCount,
} SoundLevel;

typedef enum {
    /* Every buzz (thrust, taps, crash, landing) is software-PWM'd per tick. */
    VibrationOff = 0,
    VibrationLow,   // motor on 1 tick in 4
    VibrationMed,   // motor on 1 tick in 2
    VibrationHigh,  // motor on continuously
    VibrationCount,
} VibrationLevel;

extern const char* const sound_level_label[SoundCount];
extern const char* const vibration_level_label[VibrationCount];
