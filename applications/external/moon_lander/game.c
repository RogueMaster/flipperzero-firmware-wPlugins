#include "game.h"
#include "lander_sprite.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <gui/canvas.h>
#include <furi_hal_rtc.h>
#include <furi_hal_speaker.h>
#include <furi_hal_vibro.h>

/* ----- Tunables (all in pixels [1px = 1m, based on the Apollo Lunar Module] & seconds) -------------------------------- */

/* NOTE: lander geometry (LANDER_HALF_W, LANDER_BODY_TOP, LANDER_BODY_BOT,
 * LANDER_FOOT_DX, LANDER_FOOT_DY) lives in
 * lander_sprite.h — single source of truth shared with the menu. The
 * collision math below uses LANDER_FOOT_DX / LANDER_FOOT_DY from that
 * header. */

#define PAD_W                          16      // 2x lander total width
#define CEILING_Y                      (-10.0f) // play area extends 10px above screen so lander fully disappears
#define TERRAIN_TOP_Y                  26      // highest peak (smallest y) after normalization
#define TERRAIN_BOT_Y                  (SCREEN_H - 1) // deepest valley = bottom row of screen
#define DESPIKE_HEIGHT                 5       // a column must be this many px above BOTH x±2 neighbours to be a spike

/* Scoring */
#define HIGHEST_LEVEL                  30      // bump this when adding levels; scoring depends on it
// Distance thresholds (from spawn center) for multiplier tiers 1x/2x/3x/5x
#define MUL_1X_THRESH                  8       // basically right below spawn
#define MUL_2X_THRESH                  (SCREEN_W / 5) // ~25 px
#define MUL_3X_THRESH                  (SCREEN_W / 3) // ~42 px — beyond this → 5x
#define MULTIPLIER_DISAPPEARING_HEIGHT 11      // rows above a pad where the lander hides that pad's above-pad label

#define GRAVITY                        6.0f    // pixels/sec^2 downward
#define THRUST_MAX                     18.0f   // pixels/sec^2 along lander up-axis at full thrust
#define IMPULSE_DV                     5.0f    // velocity change per tap (TapImpulse mode)
#define IMPULSE_FUEL                   2.0f
#define RAMP_TIME                      0.5f    // seconds from 0 -> full thrust in Ramp mode
#define FUEL_BURN_RATE                 12.0f   // units/sec at full thrust
#define ROT_RATE                       1.8f    // radians/sec while Left/Right held
#define WRAP_X                         1       // wrap horizontally (classic)

/* Safe-landing parameters come from the Difficulty — see apply_difficulty(). */

/* VGM tilt control parameters (steering is a 1:1 roll → angle mapping) */
#define TILT_THRUST_DEAD               3.0f    // pitch dead-zone (degrees) before thrust starts
#define TILT_THRUST_MAX                35.0f   // pitch degrees that yield 100% thrust

/* Back while flying: tap = pause menu, hold = back to the menu */
#define BACK_HOLD_EXIT                 1.0f    // sec of holding Back to leave mid-flight
#define BACK_HOLD_SHOW                 0.3f    // sec before the hold-to-exit box replaces the pause menu (taps never show it)
#define TOAST_TIME                     0.5f    // sec a toast ("Tilt zeroed") stays up

#define START_FUEL                     100.0f

/* ----- Audio / feedback tunables ----------------------------------------- */
#define AUDIO_VOLUME                   0.5f    // 0.0 to 1.0
#define THRUST_FREQ_MIN                220     // Hz at near-zero thrust
#define THRUST_FREQ_MAX                440     // Hz at full thrust
#define SFX_LAND_FREQ                  880     // Hz
#define SFX_LAND_DUR                   0.25f   // sec
#define SFX_CRASH_FREQ                 100     // Hz
#define SFX_CRASH_DUR                  0.50f   // sec
#define SFX_TAP_FREQ                   220     // Hz
#define SFX_TAP_DUR                    0.06f   // sec
#define FLASH_DURATION                 1.6f    // sec - crash inversion flashing
#define LAND_PULSE_ON                  0.08f   // sec each landing vibration pulse is on
#define LAND_PULSE_OFF                 0.08f   // sec gap between pulses
#define LAND_PULSE_COUNT               3       // number of pulses

/* ----- 'RNG' (xorshift32, seeded from level) ------------------------------- */

static uint32_t game_rand(GameState* g) {
    uint32_t x = g->rng_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    g->rng_state = x ? x : 0xDEADBEEFu;
    return g->rng_state;
}

static int rand_range(GameState* g, int lo, int hi) {
    if (hi <= lo) return lo;
    return lo + (int)(game_rand(g) % (uint32_t)(hi - lo + 1));
}

/* ----- Terrain ----------------------------------------------------------- */

static int clamp_int(int v, int lo, int hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static void terrain_generate(GameState* g) {
    /* Random walk with mild step size, then a single smoothing pass, then
     * linearly stretched so the lowest point sits at TERRAIN_BOT_Y (the very
     * bottom row of the screen) and the highest peak at TERRAIN_TOP_Y. The
     * normalization guarantees consistent vertical range regardless of how
     * the walk happened to drift. */

    int y = rand_range(g, 30, 60);
    for (int x = 0; x < SCREEN_W; x++) {
        int step = rand_range(g, -3, 3);
        y = clamp_int(y + step, 0, 200);
        g->terrain[x] = (uint8_t)y;
    }

    /* Light smoothing - just one pass to keep local variation visible. */
    for (int x = 1; x < SCREEN_W - 1; x++) {
        g->terrain[x] = (uint8_t)(
            (g->terrain[x - 1] + g->terrain[x] + g->terrain[x + 1]) / 3);
    }

    /* Normalize to [TERRAIN_TOP_Y, TERRAIN_BOT_Y]. */
    int min_y = 255, max_y = 0;
    for (int x = 0; x < SCREEN_W; x++) {
        if (g->terrain[x] < min_y) min_y = g->terrain[x];
        if (g->terrain[x] > max_y) max_y = g->terrain[x];
    }
    int src_range = max_y - min_y;
    if (src_range < 1) src_range = 1;
    int dst_range = TERRAIN_BOT_Y - TERRAIN_TOP_Y;
    for (int x = 0; x < SCREEN_W; x++) {
        int v = TERRAIN_TOP_Y + ((int)g->terrain[x] - min_y) * dst_range / src_range;
        g->terrain[x] = (uint8_t)v;
    }
}

/* Remove narrow peaks: any column more than DESPIKE_HEIGHT pixels above BOTH
 * x±2 neighbours is snapped to their average. Using x±2 as the reference
 * catches 1- and 2-column-wide spikes (a 2-wide spike has two adjacent columns
 * at the same elevation, so neither would look like a spike to an x±1 check).
 * Two passes so back-to-back spikes are also handled. */
static void terrain_despike(GameState* g) {
    uint8_t out[SCREEN_W];
    for (int pass = 0; pass < 2; pass++) {
        memcpy(out, g->terrain, SCREEN_W);
        for (int x = 2; x < SCREEN_W - 2; x++) {
            int t  = (int)g->terrain[x];
            int dl = (int)g->terrain[x - 2] - t;
            int dr = (int)g->terrain[x + 2] - t;
            if (dl > DESPIKE_HEIGHT && dr > DESPIKE_HEIGHT) {
                out[x] = (uint8_t)(((int)g->terrain[x - 2] + (int)g->terrain[x + 2]) / 2);
            }
        }
        memcpy(g->terrain, out, SCREEN_W);
    }
}

int game_pads_for_level(int level) {
    if (level <= 3)  return 5;
    if (level <= 6)  return 4;
    if (level <= 8)  return 3;
    if (level == 9)  return 2;
    return 1;
}

static void pads_place(GameState* g) {
    g->num_pads = (uint8_t)game_pads_for_level(g->level);

    /* Divide the playable strip into num_pads regions and place one pad per
     * region with random horizontal jitter. Guarantees no overlap — regions
     * are sized for the widest (PAD_W) pad, so narrower ones below only add
     * slack. */
    int margin = 6;
    int usable = SCREEN_W - 2 * margin;
    int region_w = usable / g->num_pads;

    for (int i = 0; i < g->num_pads; i++) {
        int region_start = margin + i * region_w;
        int max_offset = region_w - PAD_W;
        int offset = (max_offset > 0) ? rand_range(g, 0, max_offset) : 0;
        int px = region_start + offset;
        if (px < 0) px = 0;
        if (px + PAD_W > SCREEN_W) px = SCREEN_W - PAD_W;
        g->pad_x[i] = (uint8_t)px;
        g->pad_w[i] = PAD_W;   // narrowed below, before anything is flattened
    }

    /* Assign multipliers based on distance of each pad's center from the
     * lander spawn (screen center), then shrink high-multiplier pads to
     * match: 3×→13px, 5×→10px. Multiplier depends only on the center, which
     * narrowing doesn't move, so this can run in one pass per pad. */
    int spawn_cx = SCREEN_W / 2;
    for (int i = 0; i < g->num_pads; i++) {
        int pad_cx = g->pad_x[i] + PAD_W / 2;
        int dist = abs(pad_cx - spawn_cx);
        uint8_t mul;
        if      (dist <= MUL_1X_THRESH) mul = 1;
        else if (dist <= MUL_2X_THRESH) mul = 2;
        else if (dist <= MUL_3X_THRESH) mul = 3;
        else                            mul = 5;
        g->pad_mul[i] = mul;

        int new_w = PAD_W;
        if      (mul == 5) new_w = 10;
        else if (mul == 3) new_w = 13;
        if (new_w < PAD_W) {
            g->pad_x[i] = (uint8_t)(pad_cx - new_w / 2);
            g->pad_w[i] = (uint8_t)new_w;
        }
    }

    /* Flatten exactly each pad's final width — never wider. A flat spot in
     * the terrain means "this is a pad" and nothing else, so the ground
     * itself marks the pad; no separate on-screen marker is needed. Use the
     * lowest (largest-y) value under the span so the pad sits flush with
     * the surrounding ground. */
    for (int i = 0; i < g->num_pads; i++) {
        int px = g->pad_x[i];
        int pw = g->pad_w[i];
        int flat_y = g->terrain[px];
        for (int dx = 1; dx < pw; dx++) {
            int t = g->terrain[px + dx];
            if (t > flat_y) flat_y = t;
        }
        for (int dx = 0; dx < pw; dx++) {
            g->terrain[px + dx] = (uint8_t)flat_y;
        }
    }
}

/* Flat ground should always mean "pad". Natural terrain right beside a pad
 * sometimes sits at the pad's height, which reads as a wider pad than the
 * one that counts; raise those edge columns 1 px so every pad ends in a
 * visible step. Runs after the last despike, which could level a column. */
static void pads_mark_edges(GameState* g) {
    for (int i = 0; i < g->num_pads; i++) {
        int px = g->pad_x[i];
        int pw = g->pad_w[i];
        int y  = g->terrain[px];
        int edges[2] = {px - 1, px + pw};
        for (int e = 0; e < 2; e++) {
            int x = edges[e];
            if (x < 0 || x >= SCREEN_W || g->terrain[x] != y) continue;
            bool in_pad = false;
            for (int j = 0; j < g->num_pads; j++) {
                if (x >= g->pad_x[j] && x < g->pad_x[j] + g->pad_w[j]) in_pad = true;
            }
            if (!in_pad) g->terrain[x] = (uint8_t)(y - 1);
        }
    }
}

/* ----- Init -------------------------------------------------------------- */

/* DifficultyCustom limits, set from Settings via game_set_custom_limits().
 * Defaults match Easy. */
static float custom_vx    = 8.0f;
static float custom_vy    = 16.0f;
static float custom_angle = 0.44f;   // radians

void game_set_custom_limits(int vx, int vy, int angle_deg) {
    custom_vx    = (float)vx;
    custom_vy    = (float)vy;
    custom_angle = (float)angle_deg * (3.14159265f / 180.0f);
}

/* World seed, set from Settings via game_set_seed(). It is hashed into a
 * salt that is XORed into every level's terrain seed. Seed 1's salt is 0,
 * so it keeps the original 30 levels. */
static uint32_t seed_salt = 0;

void game_set_seed(uint16_t seed) {
    /* lowbias32 hash of seed - 1; it maps 0 to 0. */
    uint32_t h = (uint32_t)seed - 1u;
    h ^= h >> 16;
    h *= 0x7FEB352Du;
    h ^= h >> 15;
    h *= 0x846CA68Bu;
    h ^= h >> 16;
    seed_salt = h;
}

static void apply_difficulty(GameState* g, Difficulty d) {
    switch(d) {
        case DifficultyEasy:       g->safe_vy = 16.0f; g->safe_vx = 8.0f; g->safe_angle = 0.44f;  break;
        case DifficultyHard:       g->safe_vy =  4.0f; g->safe_vx = 2.0f; g->safe_angle = 0.11f;  break;
        case DifficultyRealistic:  g->safe_vy =  1.0f; g->safe_vx = 1.0f; g->safe_angle = 0.052f; break;
        case DifficultyCustom:     g->safe_vy = custom_vy; g->safe_vx = custom_vx; g->safe_angle = custom_angle; break;
        default: /* Medium */      g->safe_vy =  8.0f; g->safe_vx = 4.0f; g->safe_angle = 0.22f;  break;
    }
}

void game_init(GameState* g, int level, int score, FuelMode fuel_mode, int starting_fuel, Difficulty difficulty) {
    memset(g, 0, sizeof(*g));
    g->level = level;
    g->score = score;
    g->fuel_mode = fuel_mode;
    /* Seed depends on level and the world seed. Mixing constants keep
     * adjacent levels visually different rather than near-identical. */
    g->rng_state = (0xA5C3F00Du ^ ((uint32_t)level * 0x9E3779B1u)) ^ seed_salt;

    terrain_generate(g);
    terrain_despike(g);
    pads_place(g);
    terrain_despike(g); // second pass: catches edge spikes introduced at pad boundaries
    pads_mark_edges(g);

    g->x = (float)SCREEN_W / 2.0f;
    g->y = 6.0f;
    /* Random initial Vx in [-5, +5] pixels/sec, fixed per level by the seed. */
    g->vx = (float)rand_range(g, -5, 5);
    g->vy = 0.0f;
    g->angle = 0.0f;
    g->fuel = (float)starting_fuel;
    g->fuel_at_level_start = (float)starting_fuel;
    g->status = GameStatusFlying;
    g->status_time = 0.0f;
    g->elapsed = 0.0f;
    g->thrust_hold_time = 0.0f;
    g->current_thrust = 0.0f;
    g->needs_tilt_cal = true;
    g->tilt_pitch_offset = 0.0f;
    g->tilt_roll_offset  = 0.0f;
    g->gravity_scale = 1.0f;
    g->is_tutorial   = false;
    g->difficulty    = difficulty;
    apply_difficulty(g, difficulty);
}

void game_init_tutorial(GameState* g, int tut_level, int score, Difficulty difficulty) {
    memset(g, 0, sizeof(*g));
    g->level       = tut_level;
    g->score       = score;
    g->is_tutorial = true;
    g->gravity_scale = (tut_level == 1) ? 0.5f : 1.0f;
    g->fuel_mode   = FuelModeFull;

    /* Completely flat terrain */
    uint8_t flat_y = (uint8_t)(TERRAIN_BOT_Y - 14);
    for(int x = 0; x < SCREEN_W; x++) g->terrain[x] = flat_y;

    if(tut_level == 1) {
        /* Entire floor is the landing zone */
        g->num_pads   = 1;
        g->pad_x[0]   = 0;
        g->pad_w[0]   = SCREEN_W;
        g->pad_mul[0] = 1;
    } else {
        /* Two standard-width pads centred at 1/3 and 2/3 of screen width */
        g->num_pads = 2;
        g->pad_x[0]   = (uint8_t)(SCREEN_W / 3 - PAD_W / 2);
        g->pad_w[0]   = PAD_W;
        g->pad_mul[0] = 1;
        g->pad_x[1]   = (uint8_t)(2 * SCREEN_W / 3 - PAD_W / 2);
        g->pad_w[1]   = PAD_W;
        g->pad_mul[1] = 1;
    }

    g->x  = (float)SCREEN_W / 2.0f;
    g->y  = 6.0f;
    g->vx = 0.0f;
    g->vy = 0.0f;
    g->angle = 0.0f;
    g->fuel  = START_FUEL;
    g->fuel_at_level_start = START_FUEL;
    g->status      = GameStatusFlying;
    g->status_time = 0.0f;
    g->elapsed     = 0.0f;
    g->needs_tilt_cal    = true;
    g->tilt_pitch_offset = 0.0f;
    g->tilt_roll_offset  = 0.0f;
    g->difficulty        = difficulty;
    apply_difficulty(g, difficulty);
}

/* ----- Input ------------------------------------------------------------- */

static bool mode_uses_tilt(ThrustMode m) {
    return m == ThrustModeVidyaTap || m == ThrustModeVidyaBinary ||
           m == ThrustModeVidyaRamp || m == ThrustModeVidyaFull;
}

static void show_toast(GameState* g, const char* msg) {
    g->toast = msg;
    g->toast_time = TOAST_TIME;
}

/* The key that fires the engine, set from Settings via game_set_thrust_key(). */
static ThrustKey thrust_key = ThrustKeyUp;

void game_set_thrust_key(ThrustKey key) {
    thrust_key = key;
}

static InputKey thrust_input_key(void) {
    return (thrust_key == ThrustKeyOk) ? InputKeyOk : InputKeyUp;
}

/* Tap modes (Tap Impulse, Vidya Tilt+Tap) fire a fixed impulse on each press
 * of the thrust key, separate from the per-tick thrust level. */
static void apply_tap_impulse(GameState* g) {
    if (g->status != GameStatusFlying || g->pause.open || g->fuel <= 0.0f) return;
    g->vx += sinf(g->angle) * IMPULSE_DV;
    g->vy += -cosf(g->angle) * IMPULSE_DV;
    g->fuel -= IMPULSE_FUEL;
    if (g->fuel < 0.0f) g->fuel = 0.0f;
    g->sfx_remaining = SFX_TAP_DUR;
    g->sfx_freq = SFX_TAP_FREQ;
    g->sfx_vibrate = true;
}

static void release_key(GameState* g, InputKey key) {
    if (key == InputKeyLeft)       g->left_held = false;
    if (key == InputKeyRight)      g->right_held = false;
    if (key == thrust_input_key()) g->thrust_held = false;
    if (key == InputKeyOk)         g->ok_armed = false;
}

/* Keys while the pause menu is open (Back is handled in game_input). */
static GameAction pause_input(GameState* g, const InputEvent* ev) {
    /* Releases still clear held keys, so nothing is stuck on resume, but
     * presses start no new holds: Up and OK work the menu here. */
    if (ev->type == InputTypeRelease) release_key(g, ev->key);
    switch (pause_menu_input(&g->pause, ev)) {
        case PauseActionResume:
            g->pause.open = false;
            break;
        case PauseActionZeroTilt:
            /* The way the Flipper is held now steers straight up. Roll only,
             * as the old Back tap did: Full Tilt's thrust zero stays where
             * the level started. */
            g->tilt_roll_offset = g->tilt_roll;
            show_toast(g, "Tilt zeroed");
            g->pause.open = false;
            break;
        case PauseActionQuit:
            return GameActionExitToMenu;
        default:
            break;
    }
    return GameActionNone;
}

/* One tank: landing with less than 1 fuel left ends the game, that mode's
 * natural final state. Landing on the last level still wins. */
static bool one_tank_game_over(const GameState* g) {
    return g->status == GameStatusLanded && !g->is_tutorial &&
           g->fuel_mode != FuelModeFull && (int)g->fuel <= 0 && g->level < HIGHEST_LEVEL;
}

GameAction game_input(GameState* g, const InputEvent* ev, ThrustMode thrust_mode) {
    if (ev->key == InputKeyBack) {
        if (g->status == GameStatusFlying) {
            /* Mid-flight, Back pauses the moment it goes down. A tap leaves
             * the pause menu open, or closes it if it was already open.
             * Holding it BACK_HOLD_EXIT (timed in game_tick) leaves for the
             * menu, so a tap can't end the run by accident. */
            if (ev->type == InputTypePress) {
                g->back_held = true;
                g->back_hold_time = 0.0f;
                g->back_opened_pause = !g->pause.open;
                if (!g->pause.open) pause_menu_open(&g->pause, mode_uses_tilt(thrust_mode));
            } else if (ev->type == InputTypeShort) {
                if (!g->back_opened_pause) g->pause.open = false;
            } else if (ev->type == InputTypeRelease) {
                g->back_held = false;
                g->back_opened_pause = false;
            }
            return GameActionNone;
        }
        /* From a status banner, Back returns to the menu. */
        if (ev->type == InputTypeShort || ev->type == InputTypeLong) {
            return GameActionExitToMenu;
        }
        return GameActionNone;
    }

    if (g->pause.open) return pause_input(g, ev);

    /* Held state for Left/Right and the thrust key lives between Press and
     * Release. The Flipper sends Press + Release for every keypress, plus
     * Short/Long/Repeat in between. */
    if (ev->type == InputTypePress) {
        if (ev->key == InputKeyLeft)  g->left_held = true;
        if (ev->key == InputKeyRight) g->right_held = true;
        if (ev->key == thrust_input_key()) {
            g->thrust_held = true;
            g->thrust_hold_time = 0.0f;
            if (thrust_mode == ThrustModeTapImpulse || thrust_mode == ThrustModeVidyaTap) {
                apply_tap_impulse(g);
            }
        }
        if (ev->key == InputKeyOk) g->ok_armed = (g->status != GameStatusFlying);
    } else if (ev->type == InputTypeRelease) {
        release_key(g, ev->key);
    }

    /* On status screens, OK retries (crash) or advances (landed).
     * Score carries forward across levels and across retries.
     * Fuel behavior depends on fuel_mode:
     *   - FuelModeFull: every level starts with START_FUEL (classic).
     *   - No-refuel:    advance carries remaining fuel forward;
     *                   retry rewinds to fuel_at_level_start. */
    if (ev->type == InputTypeShort && ev->key == InputKeyOk && g->ok_armed) {
        if (g->status == GameStatusLanded) {
            int next = g->level + 1;
            if (next > HIGHEST_LEVEL) return GameActionWin;
            /* The banner says the game is over; OK goes back to the menu,
             * which keeps a new high score. */
            if (one_tank_game_over(g)) return GameActionExitToMenu;
            /* Capture campaign state before memset wipes it. */
            FuelMode mode = g->fuel_mode;
            Difficulty diff = g->difficulty;
            int score = g->score;
            int next_fuel = (mode == FuelModeFull) ? (int)START_FUEL : (int)g->fuel;
            game_init(g, next, score, mode, next_fuel, diff);
        } else if (g->status == GameStatusCrashed) {
            FuelMode mode = g->fuel_mode;
            Difficulty diff = g->difficulty;
            int score = g->score;
            int level = g->level;
            int retry_fuel =
                (mode == FuelModeFull) ? (int)START_FUEL : (int)g->fuel_at_level_start;
            game_init(g, level, score, mode, retry_fuel, diff);
        }
    }

    return GameActionNone;
}

/* ----- Physics ----------------------------------------------------------- */

/* Returns the index of the pad that fully contains [x_left, x_right], or -1. */
static int on_pad_idx(const GameState* g, int x_left, int x_right) {
    for (int i = 0; i < g->num_pads; i++) {
        int px = g->pad_x[i];
        if (x_left >= px && x_right <= px + (int)g->pad_w[i] - 1) return i;
    }
    return -1;
}

static void check_collision(GameState* g) {
    /* Rotate both foot positions into world space. */
    float s = sinf(g->angle);
    float c = cosf(g->angle);

    /* Local foot coords: (-LANDER_FOOT_DX, LANDER_FOOT_DY) and (+LANDER_FOOT_DX, LANDER_FOOT_DY) */
    float lfx = g->x + (-LANDER_FOOT_DX) * c - LANDER_FOOT_DY * s;
    float lfy = g->y + (-LANDER_FOOT_DX) * s + LANDER_FOOT_DY * c;
    float rfx = g->x +  LANDER_FOOT_DX  * c - LANDER_FOOT_DY * s;
    float rfy = g->y +  LANDER_FOOT_DX  * s + LANDER_FOOT_DY * c;

    int lxi = clamp_int((int)lfx, 0, SCREEN_W - 1);
    int rxi = clamp_int((int)rfx, 0, SCREEN_W - 1);

    bool left_contact  = lfy >= (float)g->terrain[lxi];
    bool right_contact = rfy >= (float)g->terrain[rxi];

    if (!left_contact && !right_contact) return;

    /* Something touched. Decide outcome.
     * We don't require BOTH feet to be in contact in the same frame —
     * with sub-pixel motion and any tilt, one foot always touches first.
     * The `upright` check already enforces that the lander is level enough. */
    int xmin = lxi < rxi ? lxi : rxi;
    int xmax = lxi > rxi ? lxi : rxi;
    int pad_idx = on_pad_idx(g, xmin, xmax);
    bool on_flat = (pad_idx >= 0);
    bool slow_enough = (fabsf(g->vy) < g->safe_vy) && (fabsf(g->vx) < g->safe_vx);
    bool upright = fabsf(g->angle) < g->safe_angle;

    if (on_flat && slow_enough && upright) {
        g->status = GameStatusLanded;
        if(g->is_tutorial) {
            g->score += 1;
        } else {
            /* score = fuel_left * multiplier * (HIGHEST_LEVEL - level + 1) */
            int mult = (int)g->pad_mul[pad_idx];
            int level_bonus = HIGHEST_LEVEL - g->level + 1;
            if(level_bonus < 1) level_bonus = 1;
            g->score += (int)g->fuel * mult * level_bonus;
        }
        g->sfx_remaining = SFX_LAND_DUR;
        g->sfx_freq = SFX_LAND_FREQ;
        g->sfx_vibrate = false;
    } else {
        g->status = GameStatusCrashed;
        g->sfx_remaining = SFX_CRASH_DUR;
        g->sfx_freq = SFX_CRASH_FREQ;
        g->sfx_vibrate = true;
    }
    g->status_time = 0.0f;
    g->land_vx    = g->vx;
    g->land_vy    = g->vy;
    g->land_angle = g->angle;
    g->land_off_pad = !on_flat;
    g->vx = g->vy = 0.0f;
}

void game_tick(GameState* g, ThrustMode mode, float dt) {
    g->status_time += dt;

    /* SFX timer runs regardless of game status so crash/land SFX can play out. */
    if (g->sfx_remaining > 0.0f) {
        g->sfx_remaining -= dt;
        if (g->sfx_remaining < 0.0f) g->sfx_remaining = 0.0f;
    }

    if (g->status != GameStatusFlying) {
        return;
    }

    if (g->back_held) {
        g->back_hold_time += dt;
        if (g->back_hold_time >= BACK_HOLD_EXIT) g->exit_requested = true;
    }
    /* Paused: only the Back hold above is timed; the flight is frozen. */
    if (g->pause.open) {
        g->current_thrust = 0.0f;
        return;
    }

    g->elapsed += dt;

    if (g->toast_time > 0.0f) g->toast_time -= dt;

    /* Rotation — buttons for non-VGM modes; direct angle mapping for VGM modes. */
    if(mode_uses_tilt(mode)) {
        float cal_roll = g->tilt_roll - g->tilt_roll_offset;
        g->angle = -cal_roll * (3.14159265f / 180.0f);
    } else {
        if(g->left_held)  g->angle -= ROT_RATE * dt;
        if(g->right_held) g->angle += ROT_RATE * dt;
    }
    /* Keep the angle within -180..180 degrees, so a full turn counts as
     * upright again for the landing check, not just on the HUD. */
    if(g->angle > 3.14159265f) g->angle -= 2.0f * 3.14159265f;
    else if(g->angle < -3.14159265f) g->angle += 2.0f * 3.14159265f;

    /* Thrust level (0..1) per mode. */
    g->current_thrust = 0.0f;
    if(mode == ThrustModeBinary || mode == ThrustModeVidyaBinary) {
        g->current_thrust = g->thrust_held ? 1.0f : 0.0f;
    } else if(mode == ThrustModeRamp || mode == ThrustModeVidyaRamp) {
        if(g->thrust_held) {
            g->thrust_hold_time += dt;
            float t = g->thrust_hold_time / RAMP_TIME;
            if(t > 1.0f) t = 1.0f;
            g->current_thrust = t;
        } else {
            g->thrust_hold_time = 0.0f;
            g->current_thrust = 0.0f;
        }
    } else if(mode == ThrustModeVidyaFull) {
        /* Calibrated pitch drives thrust: dead-zone → 0, TILT_THRUST_MAX → 100%. */
        float cal_pitch = g->tilt_pitch - g->tilt_pitch_offset;
        float t = (cal_pitch - TILT_THRUST_DEAD) / (TILT_THRUST_MAX - TILT_THRUST_DEAD);
        if(t < 0.0f) t = 0.0f;
        if(t > 1.0f) t = 1.0f;
        g->current_thrust = t;
    }
    /* TapImpulse and VidyaTap fire in game_input, on the key press. */

    /* Apply thrust acceleration along lander up-axis (-y when angle=0). */
    if (g->current_thrust > 0.0f && g->fuel > 0.0f) {
        float a = THRUST_MAX * g->current_thrust;
        g->vx += sinf(g->angle) * a * dt;
        g->vy += -cosf(g->angle) * a * dt;
        g->fuel -= FUEL_BURN_RATE * g->current_thrust * dt;
        if (g->fuel < 0.0f) g->fuel = 0.0f;
    }

    /* Gravity */
    g->vy += GRAVITY * g->gravity_scale * dt;

    /* Integrate */
    g->x += g->vx * dt;
    g->y += g->vy * dt;

    /* Horizontal wrap */
    if (WRAP_X) {
        if (g->x + LANDER_FOOT_DX < 0.0f)             g->x += (float)SCREEN_W;
        if (g->x - LANDER_FOOT_DX >= (float)SCREEN_W) g->x -= (float)SCREEN_W;
    } else {
        if (g->x < 0.0f) { g->x = 0.0f; g->vx = 0.0f; }
        if (g->x >= SCREEN_W) { g->x = SCREEN_W - 1; g->vx = 0.0f; }
    }

    /* Out-of-fuel doesn't end the game by itself: you glide until the
     * collision check decides the outcome. */

    /* Collision with terrain or ceiling */
    if (g->y < CEILING_Y) { g->y = CEILING_Y; if (g->vy < 0.0f) g->vy = 0.0f; }

    check_collision(g);
}

/* ----- Drawing ----------------------------------------------------------- */

/* 5×7 pixel-art theta (Θ). x/y = top-left corner. Matches FontSecondary cap height. */
static void draw_theta(Canvas* canvas, int x, int y) {
    canvas_draw_line(canvas, x+1, y,   x+3, y);           // top arc
    canvas_draw_dot(canvas,  x,   y+1);
    canvas_draw_dot(canvas,  x+4, y+1);
    canvas_draw_dot(canvas,  x,   y+2);
    canvas_draw_dot(canvas,  x+4, y+2);
    canvas_draw_line(canvas, x,   y+3, x+4, y+3);         // crossbar (midpoint)
    canvas_draw_dot(canvas,  x,   y+4);
    canvas_draw_dot(canvas,  x+4, y+4);
    canvas_draw_dot(canvas,  x,   y+5);
    canvas_draw_dot(canvas,  x+4, y+5);
    canvas_draw_line(canvas, x+1, y+6, x+3, y+6);         // bottom arc
}

/* 3×3 pixel-art degree symbol (°). x/y = top-left corner. */
static void draw_degree_sym(Canvas* canvas, int x, int y) {
    canvas_draw_dot(canvas, x+1, y);
    canvas_draw_dot(canvas, x,   y+1);
    canvas_draw_dot(canvas, x+2, y+1);
    canvas_draw_dot(canvas, x+1, y+2);
}

/* TV mode: the Video Game Module always outputs 4:3 and shows each Flipper
 * pixel 2 wide by 3 tall, so drawing the playfield at 2/3 height makes it
 * look right on the TV. Set from Settings via game_set_tv_mode(). */
#define TV_MODE_SQUISH (2.0f / 3.0f)
static float y_squish = 1.0f;

void game_set_tv_mode(bool on) {
    y_squish = on ? TV_MODE_SQUISH : 1.0f;
}

/* World y (1 px = 1 m) to screen y. TV mode squishes the world toward the
 * bottom row; physics and collision stay in world units. With TV mode off
 * this is exactly y. */
static float screen_y(float y) {
    return (float)(SCREEN_H - 1) - ((float)(SCREEN_H - 1) - y) * y_squish;
}

/* True while any pixel of the (upright) lander is inside the box on top of
 * pad i that is (pad width + 1) columns wide and
 * MULTIPLIER_DISAPPEARING_HEIGHT rows tall — where a label drawn above the
 * pad would sit right over the lander as it touches down. */
static bool lander_over_pad(const GameState* g, int i) {
    int pad_y    = g->terrain[g->pad_x[i]];
    int zone_x0  = g->pad_x[i];
    int zone_x1  = g->pad_x[i] + g->pad_w[i];
    int zone_y0  = pad_y - MULTIPLIER_DISAPPEARING_HEIGHT;
    int left     = (int)floorf(g->x - LANDER_FOOT_DX);
    int right    = (int)floorf(g->x + LANDER_FOOT_DX);
    int top      = (int)floorf(g->y + LANDER_BODY_TOP);
    int bottom   = (int)floorf(g->y + LANDER_FOOT_DY);
    return right >= zone_x0 && left <= zone_x1 && bottom >= zone_y0 && top <= pad_y;
}

static void draw_terrain(Canvas* canvas, const GameState* g) {
    /* Solid fill: each column is a bar from the terrain surface down to the
     * screen bottom.  With TV mode off the top pixel of each bar is exactly
     * terrain[x], so what despike wrote is exactly what's drawn — no
     * Bresenham smear from neighbours.
     *
     * Painted as horizontal runs, one row at a time, rather than one
     * vertical canvas_draw_line per column: u8g2 plots a line pixel by
     * pixel through four calls (~110 instructions each), and this fill is
     * ~2,300 pixels every frame. A 1-px-tall box is one tight loop
     * (~8 instructions per pixel). Same pixels either way. */
    uint8_t top[SCREEN_W];   // screen row of each column's surface
    int min_top = SCREEN_H;
    for (int x = 0; x < SCREEN_W; x++) {
        int t = (int)(screen_y((float)g->terrain[x]) + 0.5f);
        top[x] = (uint8_t)t;
        if (t < min_top) min_top = t;
    }
    for (int y = min_top; y < SCREEN_H; y++) {
        int x = 0;
        while (x < SCREEN_W) {
            if (top[x] > y) {
                x++;
                continue;
            }
            int x0 = x;
            while (x < SCREEN_W && top[x] <= y) x++;
            canvas_draw_box(canvas, x0, y, x - x0, 1);
        }
    }
    /* Pads: multiplier label drawn UNDER the pad (inside the solid fill)
     * using ColorWhite so it shows as bright text on dark ground.
     * Falls back to ABOVE the pad (in bright sky, ColorBlack) when there
     * is no room below — and then hides while the lander is over the pad,
     * where it would be in the way. */
    canvas_set_font(canvas, FontSecondary);
    const int label_h = 7;
    for (int i = 0; i < g->num_pads; i++) {
        int px = g->pad_x[i];
        int py = top[px];
        int pw = (int)g->pad_w[i];

        char buf[8];
        snprintf(buf, sizeof(buf), "%dx", (int)g->pad_mul[i]);

        int label_top_below = py + 3;
        int label_top_above = py - 2 - label_h;
        if (label_top_below + label_h <= SCREEN_H) {
            canvas_set_color(canvas, ColorWhite);
            canvas_draw_str_aligned(
                canvas, px + pw / 2, label_top_below, AlignCenter, AlignTop, buf);
            canvas_set_color(canvas, ColorBlack);
        } else if (label_top_above >= 0 && !lander_over_pad(g, i)) {
            canvas_draw_str_aligned(
                canvas, px + pw / 2, py - 2, AlignCenter, AlignBottom, buf);
        }
    }
}

/* Thin wrapper around the shared sprite. Flame draws when current_thrust > 0
 * AND fuel remains AND we're still flying — gated here, not inside the
 * sprite, so the sprite stays a pure renderer. */
static void draw_lander(Canvas* canvas, const GameState* g) {
    float thrust_for_draw = 0.0f;
    if (g->fuel > 0.0f && g->status == GameStatusFlying) {
        thrust_for_draw = g->current_thrust;
    }
    float sy = screen_y(g->y);
    lander_draw_rotated(canvas, g->x, sy, g->angle, thrust_for_draw, y_squish);

    if(sy < 0.0f) {
        int ix = clamp_int((int)g->x, 2, SCREEN_W - 3);
        canvas_draw_line(canvas, ix - 2, 0, ix + 2, 0);
        canvas_draw_line(canvas, ix - 1, 1, ix + 1, 1);
        canvas_draw_dot (canvas, ix,     2);
    }
}

static void draw_hud(Canvas* canvas, const GameState* g) {
    canvas_set_font(canvas, FontSecondary);

    char buf[16];

    /* Top center: current level. Hidden while the lander is still high enough
     * to visually overlap with the top text row — appears once the lander
     * has fallen below the bottom of the top line of HUD text (~y=7).
     * Body top is at g->y + LANDER_BODY_TOP (=g->y - 3); the +1 margin keeps
     * it from popping in the moment the body grazes the text line. */
    if(screen_y(g->y) > 11.0f) {
        snprintf(buf, sizeof(buf), g->is_tutorial ? "T%d" : "L%d", g->level);
        canvas_draw_str_aligned(canvas, SCREEN_W / 2, 0, AlignCenter, AlignTop, buf);
    }

    /* Left column - AlignTop so successive lines stack predictably. */
    snprintf(buf, sizeof(buf), "S:%04d", g->score);
    canvas_draw_str_aligned(canvas, 0, 0, AlignLeft, AlignTop, buf);

    snprintf(buf, sizeof(buf), "T:%04d", (int)g->elapsed);
    canvas_draw_str_aligned(canvas, 0, 8, AlignLeft, AlignTop, buf);

    snprintf(buf, sizeof(buf), "F:%04d", (int)g->fuel);
    canvas_draw_str_aligned(canvas, 0, 16, AlignLeft, AlignTop, buf);

    /* Right column - %+4d keeps width stable as values change sign / magnitude. */

    /* Row 0: angle — normalize to -180..180 then draw θ:±NNN° */
    float a = g->angle;
    while(a >  3.14159265f) a -= 6.28318530f;
    while(a < -3.14159265f) a += 6.28318530f;
    snprintf(buf, sizeof(buf), ":%+04d", (int)(a * (180.0f / 3.14159265f)));
    int text_w = (int)canvas_string_width(canvas, buf);
    /* theta(5) + gap(1) + text + gap(1) + degree(3) */
    int ax = SCREEN_W - (6 + text_w + 1 + 3);
    draw_theta(canvas, ax, 0);
    canvas_draw_str(canvas, ax + 6, 7, buf);
    draw_degree_sym(canvas, ax + 6 + text_w + 1, 1);

    snprintf(buf, sizeof(buf), "Vx:%+4d", (int)g->vx);
    canvas_draw_str_aligned(canvas, SCREEN_W, 8, AlignRight, AlignTop, buf);

    snprintf(buf, sizeof(buf), "Vy:%+4d", -(int)g->vy);
    canvas_draw_str_aligned(canvas, SCREEN_W, 16, AlignRight, AlignTop, buf);
}

bool game_banner_visible(const GameState* g) {
    if (g->status == GameStatusFlying) return false;
    /* Hold off the banner while the crash flash is still going. */
    return !(g->status == GameStatusCrashed && g->status_time < FLASH_DURATION);
}

static void draw_status_banner(Canvas* canvas, const GameState* g) {
    if (!game_banner_visible(g)) return;

    bool game_over = one_tank_game_over(g);
    const char* line1 = "";
    const char* line2 = "";
    switch (g->status) {
        case GameStatusLanded:
            line1 = "LANDED!";
            if (game_over) {
                line2 = "OK / Back: menu";
            } else {
                /* "done" when OK ends the run: the tutorial's last level or
                 * level 30. */
                line2 = (g->is_tutorial ? g->level >= 2 : g->level >= HIGHEST_LEVEL)
                      ? "OK: done  Back: menu"
                      : "OK: next  Back: menu";
            }
            break;
        case GameStatusCrashed:
            line1 = "CRASHED";
            line2 = "OK: retry  Back: menu";
            break;
        default: break;
    }

    bool is_crash   = (g->status == GameStatusCrashed);
    bool vx_bad     = fabsf(g->land_vx)    >= g->safe_vx;
    bool vy_bad     = fabsf(g->land_vy)    >= g->safe_vy;
    bool angle_bad  = is_crash && fabsf(g->land_angle) >= g->safe_angle;
    bool pad_bad    = is_crash && g->land_off_pad;
    bool blink_hide = is_crash && (((int)(g->status_time * 3.0f) % 2) == 1);

    /* Banner grows a line when the angle, or missing the pad, caused the
     * crash (both at once: the angle gets the line), and for the one-tank
     * game over, which shows "Game over" and the final score. */
    bool reason_line = angle_bad || pad_bad || game_over;
    int bx = 14, bw = SCREEN_W - 28;
    int by = reason_line ? 10 : 14;
    int bh = reason_line ? 44 : 36;

    canvas_set_color(canvas, ColorWhite);
    canvas_draw_box(canvas, bx, by, bw, bh);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_rframe(canvas, bx, by, bw, bh, 2);

    /* Line 1 — status title */
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, SCREEN_W / 2, by + 8, AlignCenter, AlignCenter, line1);

    /* Line 2 — velocities; offending values blink on crash. The game over
     * says so here instead. */
    char buf[24];
    canvas_set_font(canvas, FontSecondary);
    int vel_y = by + (reason_line ? 19 : 20);
    if(game_over) {
        canvas_draw_str_aligned(canvas, SCREEN_W / 2, vel_y, AlignCenter, AlignCenter, "Game over");
    } else {
        if(!(vx_bad && blink_hide)) {
            snprintf(buf, sizeof(buf), "Vx:%+d", (int)g->land_vx);
            canvas_draw_str_aligned(canvas, SCREEN_W / 2 - 3, vel_y, AlignRight, AlignCenter, buf);
        }
        if(!(vy_bad && blink_hide)) {
            snprintf(buf, sizeof(buf), "Vy:%+d", -(int)g->land_vy);
            canvas_draw_str_aligned(canvas, SCREEN_W / 2 + 3, vel_y, AlignLeft, AlignCenter, buf);
        }
    }

    /* Line 3 — the final score on the game over; else the angle when it was
     * a cause, else "Missed the pad"; those two blink.
     * Angle layout: [θ][:NN][°][R/L] — θ and ° drawn as pixel-art, ASCII
     * measured with canvas_string_width so the whole thing centers correctly. */
    if(game_over) {
        snprintf(buf, sizeof(buf), "Score: %d", g->score);
        canvas_draw_str_aligned(canvas, SCREEN_W / 2, by + 29, AlignCenter, AlignCenter, buf);
    } else if(angle_bad && !blink_hide) {
        int deg = (int)(fabsf(g->land_angle) * (180.0f / 3.14159265f) + 0.5f);
        char dir_str[2] = {(g->land_angle >= 0.0f) ? 'R' : 'L', '\0'};
        snprintf(buf, sizeof(buf), ":%d", deg);

        int ascii_w = (int)canvas_string_width(canvas, buf);
        int dir_w   = (int)canvas_string_width(canvas, dir_str);
        /* theta=5 +1gap, ascii_w, +1gap, degree=3 +1gap, dir */
        int total_w = 6 + ascii_w + 1 + 3 + 1 + dir_w;
        int cx = SCREEN_W / 2;
        int cy = by + 29;   // vertical center of this line
        int x  = cx - total_w / 2;

        draw_theta(canvas, x, cy - 4);         // 7px tall, aligned with text top
        x += 6;
        canvas_draw_str(canvas, x, cy + 3, buf); // FontSecondary baseline = cy+3
        x += ascii_w + 1;
        draw_degree_sym(canvas, x, cy - 3);    // superscript: top aligns with text top
        x += 4;
        canvas_draw_str(canvas, x, cy + 3, dir_str);
    } else if(pad_bad && !blink_hide) {
        /* Slow and level but off the pad — e.g. on the flat shelf around a
         * narrowed 3x/5x pad. */
        canvas_draw_str_aligned(canvas, SCREEN_W / 2, by + 29, AlignCenter, AlignCenter, "Missed the pad");
    }

    /* Last line — action prompt */
    int prompt_y = by + (reason_line ? 39 : 30);
    canvas_draw_str_aligned(canvas, SCREEN_W / 2, prompt_y, AlignCenter, AlignCenter, line2);
}

/* Draw a ~20% dim overlay by setting 1 in every 5 pixels black using a
 * diagonal stripe pattern. Called just before the status banner so the
 * banner itself stays crisp white on top. */
static void draw_dim_overlay(Canvas* canvas) {
    canvas_set_color(canvas, ColorBlack);
    for(int y = 0; y < SCREEN_H; y++) {
        int x = (5 - (y % 5)) % 5;
        for(; x < SCREEN_W; x += 5) {
            canvas_draw_dot(canvas, x, y);
        }
    }
}

void game_draw_tutorial_popup(Canvas* canvas, int tut_level, ThrustMode thrust_mode, const GameState* g) {
    int bx = 2, by = 1, bw = 124, bh = 62;
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_box(canvas, bx, by, bw, bh);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_rframe(canvas, bx, by, bw, bh, 3);

    int cx = SCREEN_W / 2;

    if(tut_level == 1) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, cx, by + 7, AlignCenter, AlignCenter, "Controls");
        canvas_draw_line(canvas, bx + 2, by + 13, bx + bw - 3, by + 13);
        canvas_set_font(canvas, FontSecondary);

        /* Names the thrust key from Settings. Vidya Ramp ramps on the key
         * like Ramp; only Full Tilt thrusts by tilting forward. */
        const char* key = thrust_key_label[thrust_key];
        char thrust_str[28];
        switch(thrust_mode) {
            case ThrustModeVidyaFull:
                snprintf(thrust_str, sizeof(thrust_str), "Tilt fwd = ramp thruster"); break;
            case ThrustModeRamp:
            case ThrustModeVidyaRamp:
                snprintf(thrust_str, sizeof(thrust_str), "%s = ramp thruster", key);   break;
            case ThrustModeTapImpulse:
            case ThrustModeVidyaTap:
                snprintf(thrust_str, sizeof(thrust_str), "%s = burst thruster", key);  break;
            default:
                snprintf(thrust_str, sizeof(thrust_str), "%s = engage thruster", key); break;
        }
        bool is_vidya = (thrust_mode >= ThrustModeVidyaTap);
        const char* rotate_str = is_vidya ? "L/R tilt = rotate" : "L/R = rotate";

        canvas_draw_str_aligned(canvas, cx, by + 19, AlignCenter, AlignCenter, thrust_str);
        canvas_draw_str_aligned(canvas, cx, by + 27, AlignCenter, AlignCenter, rotate_str);
        canvas_draw_str_aligned(canvas, cx, by + 36, AlignCenter, AlignCenter, "Land slow & upright:");

        /* Criteria line: dynamic values from difficulty */
        char prefix[20];
        snprintf(prefix, sizeof(prefix), "Vy<%d  Vx<%d  ", (int)g->safe_vy, (int)g->safe_vx);
        char angle_str[16];
        int angle_tenths = (int)(g->safe_angle * (1800.0f / 3.14159265f) + 0.5f);
        snprintf(angle_str, sizeof(angle_str), "<%d.%d", angle_tenths / 10, angle_tenths % 10);
        int prefix_w    = (int)canvas_string_width(canvas, prefix);
        int angle_str_w = (int)canvas_string_width(canvas, angle_str);
        int total_w = prefix_w + 6 + angle_str_w + 4;
        int line_cy = by + 46;
        int x = cx - total_w / 2;
        canvas_draw_str(canvas, x, line_cy + 3, prefix);
        x += prefix_w;
        draw_theta(canvas, x, line_cy - 4);
        x += 6;
        canvas_draw_str(canvas, x, line_cy + 3, angle_str);
        x += angle_str_w + 1;
        draw_degree_sym(canvas, x, line_cy - 3);

        canvas_draw_str_aligned(canvas, cx, by + 56, AlignCenter, AlignCenter, "OK to begin");
    } else {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, cx, by + 18, AlignCenter, AlignCenter, "Full gravity on.");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, cx, by + 33, AlignCenter, AlignCenter, "Land on a pad.");
        canvas_draw_str_aligned(canvas, cx, by + 50, AlignCenter, AlignCenter, "OK to begin");
    }
}

/* Mid-flight messages, in a small white box so they read over terrain: the
 * hold-to-exit progress while Back is held, else the pause menu while
 * paused, else a short toast. The progress box and toast sit below the HUD
 * rows so the debug overlay doesn't cover them. */
static void draw_flight_messages(Canvas* canvas, const GameState* g) {
    canvas_set_font(canvas, FontSecondary);
    if (g->back_held && g->back_hold_time > BACK_HOLD_SHOW) {
        const int bw = 72, bh = 18, bx = (SCREEN_W - bw) / 2, by = 26;
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_box(canvas, bx, by, bw, bh);
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_rframe(canvas, bx, by, bw, bh, 2);
        canvas_draw_str_aligned(canvas, SCREEN_W / 2, by + 6, AlignCenter, AlignCenter, "Hold to exit");
        float p = g->back_hold_time / BACK_HOLD_EXIT;
        if (p > 1.0f) p = 1.0f;
        canvas_draw_frame(canvas, bx + 6, by + 11, bw - 12, 4);
        canvas_draw_box(canvas, bx + 6, by + 11, (size_t)((bw - 12) * p), 4);
    } else if (g->pause.open) {
        pause_menu_draw(canvas, &g->pause);
    } else if (g->toast && g->toast_time > 0.0f) {
        const int bh = 11, by = 28;
        int bw = (int)canvas_string_width(canvas, g->toast) + 10;
        int bx = (SCREEN_W - bw) / 2;
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_box(canvas, bx, by, bw, bh);
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_rframe(canvas, bx, by, bw, bh, 2);
        canvas_draw_str_aligned(canvas, SCREEN_W / 2, by + 6, AlignCenter, AlignCenter, g->toast);
    }
}

void game_draw(Canvas* canvas, const GameState* g) {
    draw_terrain(canvas, g);
    draw_lander(canvas, g);

    /* Crash flash: XOR-invert the scene (terrain + lander) on alternating
     * phases for FLASH_DURATION. FLASH_PHASE_HZ controls how often it flips —
     * lower number = slower flashing. 2.5 Hz = 400 ms per phase. HUD draws
     * on top unaffected so the player can still read what's going on. */
    if (g->status == GameStatusCrashed && g->status_time < FLASH_DURATION) {
        const float FLASH_PHASE_HZ = 2.5f;
        int phase = (int)(g->status_time * FLASH_PHASE_HZ) % 2;
        if (phase == 1) {
            canvas_set_color(canvas, ColorXOR);
            canvas_draw_box(canvas, 0, 0, SCREEN_W, SCREEN_H);
            canvas_set_color(canvas, ColorBlack);
        }
    }

    if(!g->hud_hidden) draw_hud(canvas, g);

    /* Dim the whole frame under the pause menu or the status banner. */
    if(g->pause.open || game_banner_visible(g)) draw_dim_overlay(canvas);

    if(g->vgm_missing) {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(
            canvas, SCREEN_W / 2, SCREEN_H / 2 - 4, AlignCenter, AlignCenter,
            "VGM not found");
    }

    if(g->status == GameStatusFlying) draw_flight_messages(canvas, g);
    draw_status_banner(canvas, g);
}

/* ----- Audio + vibration -------------------------------------------------
 * The speaker is acquired when entering the game screen and released on
 * leaving. While acquired, we set its frequency once per tick based on game
 * state; furi_hal_speaker_start() with a new frequency while already
 * playing changes the pitch smoothly.
 *
 * Vibration is on during continuous thrust (Binary/Ramp/Full Tilt), pulses
 * for tap impulses and crashes, and gives three short pulses on landing.
 * Its strength comes from per-tick software PWM (see VibrationLevel).
 */

static bool      audio_acquired = false;
static uint16_t  audio_current_freq = 0;   // 0 = silent
static bool      audio_vibrating = false;
static float     audio_volume = AUDIO_VOLUME;

void game_audio_start(void) {
    if (audio_acquired) return;
    if (furi_hal_speaker_acquire(100)) {  // 100ms timeout; if taken, run silent
        audio_acquired = true;
    }
    audio_current_freq = 0;
    audio_vibrating = false;
}

void game_audio_stop(void) {
    if (audio_acquired) {
        furi_hal_speaker_stop();
        furi_hal_speaker_release();
        audio_acquired = false;
    }
    audio_current_freq = 0;
    if (audio_vibrating) {
        furi_hal_vibro_on(false);
        audio_vibrating = false;
    }
}

static void audio_set_freq(uint16_t freq) {
    if (!audio_acquired) return;
    if (freq == audio_current_freq) return;
    if (freq == 0) {
        furi_hal_speaker_stop();
    } else {
        furi_hal_speaker_start((float)freq, audio_volume);
    }
    audio_current_freq = freq;
}

static void audio_set_vibro(bool on) {
    if (audio_vibrating == on) return;
    furi_hal_vibro_on(on);
    audio_vibrating = on;
}

void game_audio_update(const GameState* g, ThrustMode mode, SoundLevel sound_level, VibrationLevel vibration_level) {
    static const float sound_volumes[SoundCount] = {0.0f, 0.2f, 0.45f, 0.7f};
    static uint32_t vibro_pwm_tick = 0;
    audio_volume = sound_volumes[sound_level];

    bool continuous_thrust = (mode == ThrustModeBinary    ||
                              mode == ThrustModeRamp       ||
                              mode == ThrustModeVidyaBinary||
                              mode == ThrustModeVidyaRamp  ||
                              mode == ThrustModeVidyaFull);
    uint16_t target_freq = 0;
    bool vibro_any = false;

    if(g->sfx_remaining > 0.0f) {
        target_freq = g->sfx_freq;
        if(g->sfx_vibrate) vibro_any = true;
    } else if(continuous_thrust &&
              g->current_thrust > 0.05f &&
              g->fuel > 0.0f &&
              g->status == GameStatusFlying) {
        target_freq = (uint16_t)(THRUST_FREQ_MIN +
                                 g->current_thrust * (THRUST_FREQ_MAX - THRUST_FREQ_MIN));
        vibro_any = true;
    }

    /* Paused: silence, even if a tap blip was still playing. */
    if(g->pause.open) {
        target_freq = 0;
        vibro_any = false;
    }

    /* Landing celebration: 3 short vibration pulses. */
    if(g->status == GameStatusLanded) {
        float period = LAND_PULSE_ON + LAND_PULSE_OFF;
        float total  = LAND_PULSE_COUNT * period;
        if(g->status_time < total) {
            float phase = g->status_time - ((int)(g->status_time / period)) * period;
            if(phase < LAND_PULSE_ON) vibro_any = true;
        }
    }

    if(sound_level == SoundOff) target_freq = 0;

    /* The Flipper's Stealth mode silences the speaker and vibration, as it
     * does for the system's own notifications. */
    if(furi_hal_rtc_is_flag_set(FuriHalRtcFlagStealthMode)) {
        target_freq = 0;
        vibro_any = false;
    }

    /* Software PWM for intensity. Counter resets on silence so every new
     * burst (crash, tap, thrust) always starts on the "on" tick. */
    bool target_vibro = false;
    if(vibro_any && vibration_level != VibrationOff) {
        vibro_pwm_tick++;
        switch(vibration_level) {
            case VibrationLow:  target_vibro = (vibro_pwm_tick % 4) == 1; break; // 25%
            case VibrationMed:  target_vibro = (vibro_pwm_tick % 2) == 1; break; // 50%
            case VibrationHigh: target_vibro = true;                       break; // 100%
            default: break;
        }
    } else {
        vibro_pwm_tick = 0;
    }

    audio_set_freq(target_freq);
    audio_set_vibro(target_vibro);
}

void game_audio_silence(void) {
    audio_set_freq(0);
    audio_set_vibro(false);
}
