#pragma once

#include <gui/gui.h>
#include <input/input.h>
#include "moon_lander.h"
#include "pause_menu.h"

#define MAX_PADS 5

typedef enum {
    GameStatusFlying = 0,
    GameStatusLanded,
    GameStatusCrashed,
} GameStatus;

typedef enum {
    GameActionNone = 0,
    GameActionExitToMenu,
    GameActionWin,
} GameAction;

typedef struct {
    /* Progress */
    int level;            // 1..HIGHEST_LEVEL (30); tutorial uses 1..2
    int score;
    float elapsed;        // seconds since level start

    /* Lander state */
    float x, y;           // pixel coords, y+ = down
    float vx, vy;         // pixels/sec
    float angle;          // radians, 0 = up; +ve = clockwise
    float fuel;           // 0..starting fuel (100, or 200-500 in no-refuel modes)

    /* Input state */
    bool left_held;
    bool right_held;
    bool thrust_held;        // the thrust key (UP or OK, from Settings)
    float thrust_hold_time;  // seconds (used by Ramp mode)
    float current_thrust;    // 0..1, derived from mode + input each tick

    /* VGM tilt state — written by the main loop before each game_tick call.
     * pitch: 0=normal hold (up-button up), +90≈flat/screen-up.
     * roll:  0=level, positive=tilt right. */
    float tilt_pitch;
    float tilt_roll;
    float tilt_pitch_offset;  // calibration zero captured at level start
    float tilt_roll_offset;   // calibration zero captured at level start
    bool  needs_tilt_cal;     // true until the first IMU reading is captured
    bool  vgm_missing;        // VGM mode selected but IMU not detected

    /* Level data */
    uint32_t rng_state;
    uint8_t terrain[SCREEN_W];     // y of ground at each column
    uint8_t pad_x[MAX_PADS];       // left edge of each pad
    uint8_t pad_w[MAX_PADS];       // width of each pad in pixels
    uint8_t pad_mul[MAX_PADS];     // score multiplier per pad
    uint8_t num_pads;

    /* Status */
    GameStatus status;
    float status_time;    // seconds since status changed
    float land_vx;        // vx captured at moment of landing/crash
    float land_vy;        // vy captured at moment of landing/crash
    float land_angle;     // angle (radians) captured at moment of landing/crash
    bool  land_off_pad;   // touched down outside every pad's counted width

    /* Fuel campaign state. fuel_mode is captured at game start and used by the
     * advance/retry logic in game_input(). fuel_at_level_start is the value of
     * `fuel` at the moment this level was entered — for no-refuel modes, a
     * retry rewinds to this rather than to a clean tank. */
    FuelMode fuel_mode;
    float    fuel_at_level_start;

    /* Tutorial mode. gravity_scale=0.5 on tutorial level 1, 1.0 otherwise. */
    bool       is_tutorial;
    float      gravity_scale;
    Difficulty difficulty;

    /* Safe landing thresholds — set from Difficulty at game_init time. */
    float safe_vy;
    float safe_vx;
    float safe_angle;

    /* Sound effects (one-shot, drives both speaker and vibro briefly) */
    float sfx_remaining;  // seconds left on the current SFX, 0 = none
    uint16_t sfx_freq;    // Hz
    bool sfx_vibrate;     // whether this SFX also pulses the vibro motor

    /* Back while flying pauses at once and opens the pause menu; holding it
     * BACK_HOLD_EXIT sets exit_requested, which the app layer acts on. */
    bool  back_held;
    float back_hold_time;     // seconds
    bool  exit_requested;
    PauseMenu pause;          // pause.open = physics frozen, menu up
    bool  back_opened_pause;  // this Back press opened the menu: its tap mustn't close it

    /* True while an OK press that began after touchdown is down. Only such a
     * press advances or retries, so an OK thrust tap that ends just after
     * touchdown can't skip the result banner. */
    bool  ok_armed;

    /* Short message in a box mid-screen while flying, e.g. "Tilt zeroed". */
    const char* toast;
    float       toast_time; // seconds left

    bool hud_hidden;      // set by the app layer when the debug overlay is active
} GameState;

/* Initialize game state for the given level.
 *   score:         carry-forward score (pass 0 for a fresh game).
 *   fuel_mode:     campaign rules; stored in the state for retry/advance logic.
 *   starting_fuel: fuel to put in the tank for this level. The caller computes
 *                  this — typically fuel_mode_starting[mode] on a fresh start,
 *                  the previous level's remaining fuel on advance (no-refuel),
 *                  or fuel_at_level_start on retry. */
void game_init(GameState* g, int level, int score, FuelMode fuel_mode, int starting_fuel, Difficulty difficulty);
/* Tutorial variant: flat terrain. Level 1 is all pad at half gravity;
 * level 2 has two pads at full gravity. */
void game_init_tutorial(GameState* g, int tut_level, int score, Difficulty difficulty);
/* Limits used by DifficultyCustom (degrees for the angle). Set from Settings;
 * applies from the next game_init. */
void game_set_custom_limits(int vx, int vy, int angle_deg);
/* TV mode squishes the playfield toward the bottom row so it looks right on
 * the Video Game Module's 4:3 output. */
void game_set_tv_mode(bool on);
/* The key that fires the engine; set from Settings. */
void game_set_thrust_key(ThrustKey key);
/* World seed (SEED_MIN..SEED_MAX); applies from the next game_init. */
void game_set_seed(uint16_t seed);
GameAction game_input(GameState* g, const InputEvent* ev, ThrustMode thrust_mode);
void game_tick(GameState* g, ThrustMode mode, float dt);
void game_draw(Canvas* canvas, const GameState* g);
/* True while the landed/crashed banner is on screen (after the crash flash). */
bool game_banner_visible(const GameState* g);
void game_draw_tutorial_popup(Canvas* canvas, int tut_level, ThrustMode thrust_mode, const GameState* g);

/* Audio control. start/stop on entering/leaving the game screen; update once
 * per tick. start may quietly fail if the speaker is in use by another app —
 * in that case the game continues silently. */
void game_audio_start(void);
void game_audio_stop(void);
void game_audio_update(const GameState* g, ThrustMode mode, SoundLevel sound_level, VibrationLevel vibration_level);
/* Silence the speaker and the vibration motor while the game isn't ticking
 * (e.g. a tutorial popup). */
void game_audio_silence(void);

/* Pad count for a given level number (1-indexed). */
int game_pads_for_level(int level);
