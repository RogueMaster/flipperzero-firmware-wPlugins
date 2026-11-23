/*
 * Moon Lander for Flipper Zero - main / dispatch
 *
 * Event-driven main loop: input + tick events are pushed into a single queue.
 * 60 Hz tick timer drives game physics; menu is event-driven only.
 * All model state lives under a mutex; the draw callback waits at most 25 ms
 * for it so it never stalls the GUI thread for long.
 */

#include <furi.h>
#include <furi_hal_cortex.h>
#include <furi_hal_random.h>
#include <gui/gui.h>
#include <gui/elements.h>
#include <input/input.h>
#include <notification/notification_messages.h>
#include <storage/storage.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* In the app's own data folder, /ext/apps_data/<appid>, which the storage
 * service creates when a file in it is opened. */
#define HIGH_SCORE_PATH APP_DATA_PATH("score.bin")
#define SETTINGS_PATH   APP_DATA_PATH("settings.bin")
#define SETTINGS_VERSION 3

#include "moon_lander.h"
#include "menu.h"
#include "game.h"
#include "vgm_tilt.h"

#define TICK_HZ 60

/* In-game redraw caps, in ms between frames. Each frame goes to the LCD over
 * SPI and, with a Video Game Module on HDMI, to the TV over a 1.84 Mbaud
 * serial link (~6 ms per frame), and the firmware busy-waits on both. That
 * load starves its lowest-priority thread, which delivers our ticks and key
 * releases. Physics still runs every tick; the VGM repeats each frame on
 * the TV. 28 ms = every 2nd 16 ms tick (~31 fps); 66 ms = every 5th tick
 * (~12 fps) still covers the 2.5-3 Hz banner and crash-flash blinking. */
#define FRAME_MS       28
#define STILL_FRAME_MS 66

typedef enum {
    AppEventInput = 0,
    AppEventTick,
} AppEventType;

typedef struct {
    AppEventType type;
    InputEvent input; // valid when type == AppEventInput
} AppEvent;

typedef struct {
    Screen screen;
    bool should_exit;
    MenuState menu;
    GameState game;
    int settings_focus;   // a SettingsRow; reset when Settings is opened from the menu
    int settings_top;     // first SettingsRow shown; the list scrolls to keep the focus shown
    int custom_focus;     // custom difficulty screen row: 0 Vx, 1 Vy, 2 angle
    VgmTilt* vgm;         // non-NULL while a VGM tilt mode is active
    int  tutorial_level;          // 1 or 2 (valid when screen == ScreenTutorial)
    bool tutorial_popup_showing;  // physics paused; waiting for OK to dismiss
    int  high_score;
    uint16_t high_score_seed;     // the seed the high score was set on
    bool game_complete_new_record;

    /* Debug overlay — toggled from Settings screen. */
    bool     debug_hud;
    uint32_t debug_tick_count;
    uint32_t debug_free_heap;
    uint8_t  debug_queue_depth;
    uint8_t  debug_peak_queue;
    /* Longest in-game draw callback and longest gap between processed ticks:
     * the _max fields collect the current 1 s window, the others show the
     * last one. */
    uint32_t debug_draw_us;
    uint32_t debug_draw_us_max;
    uint32_t debug_gap_ms;
    uint32_t debug_gap_ms_max;
} AppModel;

typedef struct {
    FuriMessageQueue* queue;
    FuriMutex* mutex;
    FuriTimer* tick_timer;
    Gui* gui;
    ViewPort* view_port;
    uint32_t last_tick_ms;
    uint32_t last_draw_tick;      // kernel tick of the last in-game redraw request
    volatile bool tick_pending;   // a tick is already in the queue; set by the timer, cleared by the loop
    NotificationApp* notification;
    bool backlight_forced;        // backlight held on while flying (see backlight_update)
    AppModel model;
} App;

/* ----- High score persistence -------------------------------------------- */

static void high_score_load(int* hs, uint16_t* seed) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    *hs = 0;
    *seed = SEED_MIN;
    if(storage_file_open(file, HIGH_SCORE_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        /* A short or damaged file reads as no score rather than garbage. */
        if(storage_file_read(file, hs, sizeof(int)) != sizeof(int) || *hs < 0) *hs = 0;
        /* The seed it was set on follows. Files from before seeds stop
         * after the score, which was set on seed 1. */
        uint16_t s;
        if(storage_file_read(file, &s, sizeof(s)) == sizeof(s) && s >= SEED_MIN && s <= SEED_MAX) {
            *seed = s;
        }
        storage_file_close(file);
    }
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

static void high_score_save(int hs, uint16_t seed) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, HIGH_SCORE_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_write(file, &hs, sizeof(int));
        storage_file_write(file, &seed, sizeof(seed));
        storage_file_close(file);
    }
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

/* ----- Settings persistence ---------------------------------------------- */

typedef struct {
    uint8_t version;
    uint8_t thrust_mode;
    uint8_t fuel_mode;
    uint8_t difficulty;
    uint8_t sound_level;
    uint8_t vibration_level;
    uint8_t debug_hud;    // 0/1; uint8_t so a damaged byte can't become an invalid bool
    /* Added in version 2 */
    uint8_t custom_vx;
    uint8_t custom_vy;
    uint8_t custom_angle;
    uint8_t tv_mode;      // 0/1 (v2 builds briefly stored a squish level; any non-zero = on)
    /* Added in version 3 */
    uint8_t thrust_key;
    uint8_t seed_lo;      // seed, low byte first
    uint8_t seed_hi;
} SavedSettings;

/* Version 1 files stop after debug_hud, version 2 files after tv_mode. */
#define SAVED_SETTINGS_V1_SIZE offsetof(SavedSettings, custom_vx)
#define SAVED_SETTINGS_V2_SIZE offsetof(SavedSettings, thrust_key)

static SavedSettings settings_snapshot(const AppModel* m) {
    SavedSettings s = {
        .version         = SETTINGS_VERSION,
        .thrust_mode     = (uint8_t)m->menu.thrust_mode,
        .fuel_mode       = (uint8_t)m->menu.fuel_mode,
        .difficulty      = (uint8_t)m->menu.difficulty,
        .sound_level     = (uint8_t)m->menu.sound_level,
        .vibration_level = (uint8_t)m->menu.vibration_level,
        .debug_hud       = m->debug_hud,
        .custom_vx       = m->menu.custom_vx,
        .custom_vy       = m->menu.custom_vy,
        .custom_angle    = m->menu.custom_angle,
        .tv_mode         = m->menu.tv_mode,
        .thrust_key      = (uint8_t)m->menu.thrust_key,
        .seed_lo         = (uint8_t)(m->menu.seed & 0xFF),
        .seed_hi         = (uint8_t)(m->menu.seed >> 8),
    };
    return s;
}

/* game.c keeps the custom limits, TV mode, thrust key and seed itself; push
 * them over. */
static void settings_apply_to_game(const AppModel* m) {
    game_set_custom_limits(m->menu.custom_vx, m->menu.custom_vy, m->menu.custom_angle);
    game_set_tv_mode(m->menu.tv_mode);
    game_set_thrust_key(m->menu.thrust_key);
    game_set_seed(m->menu.seed);
}

static void settings_load(AppModel* m) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, SETTINGS_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        SavedSettings s = {0};
        size_t n = storage_file_read(file, &s, sizeof(s));
        /* Every value indexes a label/volume table, so a damaged file must
         * not get through: out-of-range values keep the defaults. Version 1
         * files predate the custom limits and TV mode, version 2 files the
         * thrust key and seed. */
        bool base_ok = n >= SAVED_SETTINGS_V1_SIZE &&
                       s.version >= 1 && s.version <= SETTINGS_VERSION &&
                       s.thrust_mode < ThrustModeCount && s.fuel_mode < FuelModeCount &&
                       s.difficulty < DifficultyCount && s.sound_level < SoundCount &&
                       s.vibration_level < VibrationCount;
        if(base_ok) {
            m->menu.thrust_mode  = (ThrustMode)s.thrust_mode;
            m->menu.fuel_mode    = (FuelMode)s.fuel_mode;
            m->menu.difficulty   = (Difficulty)s.difficulty;
            m->menu.sound_level     = (SoundLevel)s.sound_level;
            m->menu.vibration_level = (VibrationLevel)s.vibration_level;
            m->debug_hud         = s.debug_hud != 0;
        }
        if(base_ok && s.version >= 2 && n >= SAVED_SETTINGS_V2_SIZE &&
           s.custom_vx >= 1 && s.custom_vx <= CUSTOM_VX_MAX &&
           s.custom_vy >= 1 && s.custom_vy <= CUSTOM_VY_MAX &&
           s.custom_angle >= 1 && s.custom_angle <= CUSTOM_ANGLE_MAX) {
            m->menu.custom_vx    = s.custom_vx;
            m->menu.custom_vy    = s.custom_vy;
            m->menu.custom_angle = s.custom_angle;
            m->menu.tv_mode      = s.tv_mode != 0;
        }
        if(base_ok && s.version >= 3 && n == sizeof(s)) {
            uint16_t seed = (uint16_t)(s.seed_lo | (s.seed_hi << 8));
            if(s.thrust_key < ThrustKeyCount && seed >= SEED_MIN && seed <= SEED_MAX) {
                m->menu.thrust_key = (ThrustKey)s.thrust_key;
                m->menu.seed       = seed;
            }
        }
        storage_file_close(file);
    }
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

static void settings_save(const AppModel* m) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, SETTINGS_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        SavedSettings s = settings_snapshot(m);
        storage_file_write(file, &s, sizeof(s));
        storage_file_close(file);
    }
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

/* ----- Settings and custom difficulty screens ------------------------------ */

typedef enum {
    SettingsRowSound = 0,
    SettingsRowVibration,
    SettingsRowDifficulty,   // OK on Custom opens the custom limits screen
    SettingsRowThrustKey,
    SettingsRowSeed,         // OK picks a random seed
    SettingsRowTvMode,
    SettingsRowDebugHud,
    SettingsRowCount,
} SettingsRow;

#define SETTINGS_ROWS_VISIBLE 4
#define CUSTOM_ROWS           3

static void settings_row_label(const AppModel* m, int row, char* buf, size_t size) {
    switch(row) {
        case SettingsRowSound:
            snprintf(buf, size, "Sound: %s", sound_level_label[m->menu.sound_level]);
            break;
        case SettingsRowVibration:
            snprintf(buf, size, "Vibration: %s", vibration_level_label[m->menu.vibration_level]);
            break;
        case SettingsRowDifficulty:
            snprintf(buf, size, "Difficulty: %s", difficulty_label[m->menu.difficulty]);
            break;
        case SettingsRowThrustKey:
            snprintf(buf, size, "Thrust button: %s", thrust_key_label[m->menu.thrust_key]);
            break;
        case SettingsRowSeed:
            snprintf(buf, size, "Seed: %u", (unsigned)m->menu.seed);
            break;
        case SettingsRowTvMode:
            snprintf(buf, size, "TV mode: %s", m->menu.tv_mode ? "On" : "Off");
            break;
        default:
            snprintf(buf, size, "Debug HUD: %s", m->debug_hud ? "On" : "Off");
            break;
    }
}

static int cycle(int value, int step, int count) {
    return (value + step + count) % count;
}

static void settings_row_change(AppModel* m, int row, int step) {
    switch(row) {
        case SettingsRowSound:
            m->menu.sound_level = (SoundLevel)cycle(m->menu.sound_level, step, SoundCount);
            break;
        case SettingsRowVibration:
            m->menu.vibration_level =
                (VibrationLevel)cycle(m->menu.vibration_level, step, VibrationCount);
            break;
        case SettingsRowDifficulty:
            m->menu.difficulty = (Difficulty)cycle(m->menu.difficulty, step, DifficultyCount);
            break;
        case SettingsRowThrustKey:
            m->menu.thrust_key = (ThrustKey)cycle(m->menu.thrust_key, step, ThrustKeyCount);
            break;
        case SettingsRowSeed:
            m->menu.seed = (uint16_t)(SEED_MIN + cycle(m->menu.seed - SEED_MIN, step,
                                                       SEED_MAX - SEED_MIN + 1));
            break;
        case SettingsRowTvMode:
            m->menu.tv_mode = !m->menu.tv_mode;
            break;
        default:
            m->debug_hud = !m->debug_hud;
            break;
    }
}

/* A random seed other than the current one, for OK on the Seed row. */
static uint16_t random_seed(uint16_t current) {
    uint16_t s = (uint16_t)(SEED_MIN + furi_hal_random_get() % (SEED_MAX - SEED_MIN + 1));
    if(s == current) s = (s == SEED_MAX) ? SEED_MIN : (uint16_t)(s + 1);
    return s;
}

/* Scrolls the Settings list so the focused row is shown, with one more row
 * past it when there is one, so the next row is always in view. */
static void settings_scroll_to_focus(AppModel* m) {
    int top = m->settings_top;
    if(m->settings_focus < top + 1) top = m->settings_focus - 1;
    if(m->settings_focus > top + SETTINGS_ROWS_VISIBLE - 2) {
        top = m->settings_focus - SETTINGS_ROWS_VISIBLE + 2;
    }
    if(top > SettingsRowCount - SETTINGS_ROWS_VISIBLE) top = SettingsRowCount - SETTINGS_ROWS_VISIBLE;
    if(top < 0) top = 0;
    m->settings_top = top;
}

/* Custom limits step by 1 between 1 and Easy's value; they don't wrap. */
static void custom_row_change(AppModel* m, int row, int step) {
    uint8_t* value;
    int max;
    switch(row) {
        case 0:  value = &m->menu.custom_vx;    max = CUSTOM_VX_MAX;    break;
        case 1:  value = &m->menu.custom_vy;    max = CUSTOM_VY_MAX;    break;
        default: value = &m->menu.custom_angle; max = CUSTOM_ANGLE_MAX; break;
    }
    int v = *value + step;
    if(v < 1) v = 1;
    if(v > max) v = max;
    *value = (uint8_t)v;
}

/* ----- Callbacks --------------------------------------------------------- */

static void draw_debug_overlay(Canvas* canvas, const AppModel* m) {
    const GameState* g = &m->game;

    /* Draw debug stats in the top 24 rows where the HUD normally lives.
     * No background box — terrain and lander show through beneath. */
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontSecondary);

    char buf[32];

    /* Row 0: free heap + tick counter */
    snprintf(buf, sizeof(buf), "HP:%lu TK:%lu",
             (unsigned long)m->debug_free_heap,
             (unsigned long)(m->debug_tick_count % 100000UL));
    canvas_draw_str(canvas, 0, 7, buf);

    /* Row 1: queue depth (current/peak), longest frame draw (ms) and longest
     * gap between game ticks (ms) over the last ~1 s. Ticks are due every
     * 16 ms; a big TG means they ran late, usually because the Flipper's
     * timer thread (which also delivers key releases) got no CPU time. */
    snprintf(buf, sizeof(buf), "Q:%u/%u DR:%lu.%lu TG:%lu",
             m->debug_queue_depth, m->debug_peak_queue,
             (unsigned long)(m->debug_draw_us / 1000),
             (unsigned long)(m->debug_draw_us % 1000 / 100),
             (unsigned long)m->debug_gap_ms);
    canvas_draw_str(canvas, 0, 15, buf);

    /* Row 2: angle, velocities and speed, or a NaN/runaway warning */
    bool bad_a = (g->angle != g->angle) || (g->angle > 1e6f) || (g->angle < -1e6f);
    bool bad_v = (g->vx != g->vx) || (g->vy != g->vy)
              || (g->vx > 1e6f) || (g->vx < -1e6f)
              || (g->vy > 1e6f) || (g->vy < -1e6f);
    if(bad_a || bad_v) {
        snprintf(buf, sizeof(buf), "!BAD:%s%s",
                 bad_a ? " ANG" : "", bad_v ? " VEL" : "");
    } else {
        int deg = (int)(g->angle * (180.0f / 3.14159265f));
        int speed = (int)sqrtf(g->vx * g->vx + g->vy * g->vy);
        snprintf(buf, sizeof(buf), "A:%+d X:%+d Y:%+d S:%d",
                 deg, (int)g->vx, (int)g->vy, speed);
    }
    canvas_draw_str(canvas, 0, 23, buf);
}

static void draw_callback(Canvas* canvas, void* ctx) {
    App* app = ctx;
    if (furi_mutex_acquire(app->mutex, 25) != FuriStatusOk) return;
    /* Times the in-game draw for the debug HUD's DR — i.e. how long the
     * mutex is held. The cortex timer's .start is the CPU cycle counter. */
    uint32_t draw_start = furi_hal_cortex_timer_get(0).start;

    canvas_clear(canvas);
    switch (app->model.screen) {
        case ScreenMenu:
            menu_draw(canvas, &app->model.menu);
            break;
        case ScreenGame: {
            /* The debug overlay replaces the HUD, except while the pause
             * menu or the landed/crashed banner is up: it would be drawn
             * over them. */
            bool overlay = app->model.debug_hud && !app->model.game.pause.open &&
                           !game_banner_visible(&app->model.game);
            app->model.game.hud_hidden = overlay;
            game_draw(canvas, &app->model.game);
            if(overlay) draw_debug_overlay(canvas, &app->model);
            break;
        }
        case ScreenTutorial: {
            bool overlay = app->model.debug_hud && !app->model.tutorial_popup_showing &&
                           !app->model.game.pause.open && !game_banner_visible(&app->model.game);
            app->model.game.hud_hidden = overlay;
            game_draw(canvas, &app->model.game);
            if(app->model.tutorial_popup_showing) {
                game_draw_tutorial_popup(canvas,
                                         app->model.tutorial_level,
                                         app->model.menu.thrust_mode,
                                         &app->model.game);
            }
            if(overlay) draw_debug_overlay(canvas, &app->model);
            break;
        }
        case ScreenInfo: {
            canvas_set_font(canvas, FontPrimary);
            canvas_draw_str_aligned(
                canvas, SCREEN_W / 2, 2, AlignCenter, AlignTop, "ABOUT");
            canvas_draw_line(canvas, 0, 12, SCREEN_W - 1, 12);
            canvas_set_font(canvas, FontSecondary);
            canvas_draw_str_aligned(canvas, SCREEN_W / 2, 20, AlignCenter, AlignTop,
                "Clauded 'from scratch,'");
            canvas_draw_str_aligned(canvas, SCREEN_W / 2, 28, AlignCenter, AlignTop,
                "tweaked by");
            canvas_draw_str_aligned(canvas, SCREEN_W / 2, 36, AlignCenter, AlignTop,
                "FToThe3rdPower.");
            canvas_draw_str_aligned(canvas, SCREEN_W / 2, 46, AlignCenter, AlignTop,
                "GPL-3.0, no warranty");
            canvas_draw_str_aligned(canvas, SCREEN_W / 2, SCREEN_H - 1,
                AlignCenter, AlignBottom, "Back to return");
            break;
        }
        case ScreenScore: {
            canvas_set_font(canvas, FontPrimary);
            canvas_draw_str_aligned(
                canvas, SCREEN_W / 2, 8, AlignCenter, AlignCenter, "HIGH SCORE");
            canvas_draw_line(canvas, 0, 14, SCREEN_W - 1, 14);
            if(app->model.high_score > 0) {
                char hs_buf[16];
                snprintf(hs_buf, sizeof(hs_buf), "%d", app->model.high_score);
                canvas_draw_str_aligned(
                    canvas, SCREEN_W / 2, 32, AlignCenter, AlignCenter, hs_buf);
                canvas_set_font(canvas, FontSecondary);
                snprintf(hs_buf, sizeof(hs_buf), "Seed %u", (unsigned)app->model.high_score_seed);
                canvas_draw_str_aligned(
                    canvas, SCREEN_W / 2, 43, AlignCenter, AlignCenter, hs_buf);
            } else {
                canvas_set_font(canvas, FontSecondary);
                canvas_draw_str_aligned(
                    canvas, SCREEN_W / 2, 36, AlignCenter, AlignCenter, "No score yet");
            }
            canvas_set_font(canvas, FontSecondary);
            canvas_draw_str_aligned(canvas, SCREEN_W / 2, SCREEN_H - 1,
                AlignCenter, AlignBottom, "Back to return");
            break;
        }
        case ScreenGameComplete: {
            canvas_set_font(canvas, FontPrimary);
            canvas_draw_str_aligned(
                canvas, SCREEN_W / 2, 8, AlignCenter, AlignCenter, "YOU WIN!");
            canvas_draw_line(canvas, 0, 14, SCREEN_W - 1, 14);
            canvas_set_font(canvas, FontSecondary);
            char buf[24];
            snprintf(buf, sizeof(buf), "Score: %d", app->model.game.score);
            canvas_draw_str_aligned(canvas, SCREEN_W / 2, 28, AlignCenter, AlignCenter, buf);
            if(app->model.game_complete_new_record) {
                canvas_draw_str_aligned(
                    canvas, SCREEN_W / 2, 40, AlignCenter, AlignCenter, "New high score!");
            } else {
                snprintf(buf, sizeof(buf), "Best: %d", app->model.high_score);
                canvas_draw_str_aligned(
                    canvas, SCREEN_W / 2, 40, AlignCenter, AlignCenter, buf);
            }
            canvas_draw_str_aligned(
                canvas, SCREEN_W / 2, SCREEN_H - 1,
                AlignCenter, AlignBottom, "OK / Back: menu");
            break;
        }
        case ScreenSettings: {
            canvas_set_font(canvas, FontPrimary);
            canvas_draw_str_aligned(
                canvas, SCREEN_W / 2, 2, AlignCenter, AlignTop, "SETTINGS");
            canvas_draw_line(canvas, 0, 12, SCREEN_W - 1, 12);
            /* Four rows fit. The list scrolls to keep the focused row shown,
             * and the scrollbar on the right shows where it is in the list. */
            int focus = app->model.settings_focus;
            int first = app->model.settings_top;
            char buf[24];
            for(int row = first; row < first + SETTINGS_ROWS_VISIBLE && row < SettingsRowCount;
                row++) {
                settings_row_label(&app->model, row, buf, sizeof(buf));
                draw_selector_row_w(canvas, 2, 14 + 12 * (row - first), SCREEN_W - 8, buf,
                                    row == focus);
            }
            elements_scrollbar_pos(
                canvas, SCREEN_W, 14, 12 * SETTINGS_ROWS_VISIBLE, focus, SettingsRowCount);
            break;
        }
        case ScreenCustomDifficulty: {
            const MenuState* ms = &app->model.menu;
            canvas_set_font(canvas, FontPrimary);
            canvas_draw_str_aligned(
                canvas, SCREEN_W / 2, 2, AlignCenter, AlignTop, "CUSTOM LIMITS");
            canvas_draw_line(canvas, 0, 12, SCREEN_W - 1, 12);
            char buf[24];
            snprintf(buf, sizeof(buf), "Vx < %u", ms->custom_vx);
            draw_selector_row(canvas, 14, buf, app->model.custom_focus == 0);
            snprintf(buf, sizeof(buf), "Vy < %u", ms->custom_vy);
            draw_selector_row(canvas, 26, buf, app->model.custom_focus == 1);
            snprintf(buf, sizeof(buf), "Angle < %u deg", ms->custom_angle);
            draw_selector_row(canvas, 38, buf, app->model.custom_focus == 2);
            canvas_set_font(canvas, FontSecondary);
            snprintf(buf, sizeof(buf), "Easy max: %d / %d / %d",
                     CUSTOM_VX_MAX, CUSTOM_VY_MAX, CUSTOM_ANGLE_MAX);
            canvas_draw_str_aligned(
                canvas, SCREEN_W / 2, SCREEN_H - 1, AlignCenter, AlignBottom, buf);
            break;
        }
    }

    if(app->model.screen == ScreenGame || app->model.screen == ScreenTutorial) {
        uint32_t us = (furi_hal_cortex_timer_get(0).start - draw_start) /
                      furi_hal_cortex_instructions_per_microsecond();
        if(us > app->model.debug_draw_us_max) app->model.debug_draw_us_max = us;
    }
    furi_mutex_release(app->mutex);
}

static void input_callback(InputEvent* event, void* ctx) {
    App* app = ctx;
    AppEvent ev = {.type = AppEventInput, .input = *event};
    furi_message_queue_put(app->queue, &ev, 0);
}

static void tick_timer_callback(void* ctx) {
    App* app = ctx;
    /* Keep at most one tick in the queue. If the loop falls behind, extra
     * ticks would fill it and input_callback's zero-timeout put would drop
     * key Press/Release events, leaving thrust/Left/Right stuck held (or never
     * held). Skipped ticks don't slow physics: dt comes from the clock. */
    if (app->tick_pending) return;
    app->tick_pending = true;
    AppEvent ev = {.type = AppEventTick};
    if (furi_message_queue_put(app->queue, &ev, 0) != FuriStatusOk) app->tick_pending = false;
}

/* ----- Dispatch ---------------------------------------------------------- */

/* Single point of truth for screen transitions. Handles audio
 * acquire/release on entering/leaving ScreenGame, plus game state init,
 * plus settings-screen focus reset. */
static void set_screen(App* app, Screen new_screen) {
    AppModel* m = &app->model;
    Screen old = m->screen;
    bool old_live = (old == ScreenGame || old == ScreenTutorial);
    bool new_live = (new_screen == ScreenGame || new_screen == ScreenTutorial);

    if(old_live && !new_live) {
        game_audio_stop();
        furi_timer_stop(app->tick_timer);
        if(m->vgm) { vgm_tilt_free(m->vgm); m->vgm = NULL; }
    }
    m->screen = new_screen;
    /* Tutorial popups belong to the tutorial screen. Back on one leaves with
     * the flag still set, and handle_tick would then never tick the next
     * game. */
    if(new_screen != ScreenTutorial) m->tutorial_popup_showing = false;
    if(new_live && !old_live) {
        game_audio_start();
        if(new_screen == ScreenTutorial) {
            m->tutorial_level = 1;
            game_init_tutorial(&m->game, 1, 0, m->menu.difficulty);
            m->tutorial_popup_showing = true;
        } else {
            FuelMode fm = m->menu.fuel_mode;
            int start_fuel = fuel_mode_starting[fm];
            game_init(&m->game, 1, 0, fm, start_fuel, m->menu.difficulty);
        }
        bool is_vidya = (m->menu.thrust_mode >= ThrustModeVidyaTap);
        if(is_vidya) {
            m->vgm = vgm_tilt_alloc();
            m->game.vgm_missing = !vgm_tilt_present(m->vgm);
        }
        m->debug_tick_count  = 0;
        m->debug_peak_queue  = 0;
        m->debug_queue_depth = 0;
        m->debug_free_heap   = 0;
        m->debug_draw_us     = 0;
        m->debug_draw_us_max = 0;
        m->debug_gap_ms      = 0;
        m->debug_gap_ms_max  = 0;
        app->last_tick_ms = furi_get_tick();
        app->last_draw_tick = app->last_tick_ms - furi_kernel_get_tick_frequency(); // first tick draws
        uint32_t period = furi_kernel_get_tick_frequency() / TICK_HZ;
        if(period < 1) period = 1;
        furi_timer_start(app->tick_timer, period);
    }
    /* Coming back from the custom limits screen keeps the Difficulty row. */
    if(new_screen == ScreenSettings && old == ScreenMenu) {
        m->settings_focus = 0;
        m->settings_top   = 0;
    }
    if(new_screen == ScreenCustomDifficulty) {
        m->custom_focus = 0;
    }
}

static void handle_menu_action(App* app, MenuAction action) {
    AppModel* m = &app->model;
    switch (action) {
        case MenuActionStart:    set_screen(app, ScreenGame);     break;
        case MenuActionTutorial: set_screen(app, ScreenTutorial); break;
        case MenuActionScore:    set_screen(app, ScreenScore);    break;
        case MenuActionInfo:     set_screen(app, ScreenInfo);     break;
        case MenuActionSettings: set_screen(app, ScreenSettings); break;
        case MenuActionExit:     m->should_exit = true; break;
        case MenuActionNone:     break;
    }
}

/* Keeps the game's score if it beats the high score, with the seed it was
 * set on. Returns true for a new high score. */
static bool high_score_record(AppModel* m) {
    if(m->game.score <= m->high_score) return false;
    m->high_score      = m->game.score;
    m->high_score_seed = m->menu.seed;
    high_score_save(m->high_score, m->high_score_seed);
    return true;
}

/* Back to the menu from a game screen, keeping a new high score (the
 * tutorial doesn't count toward it). */
static void leave_game(App* app) {
    AppModel* m = &app->model;
    if(m->screen == ScreenGame) high_score_record(m);
    set_screen(app, ScreenMenu);
}

static void handle_input_event(App* app, const InputEvent* ev) {
    AppModel* m = &app->model;

    /* Off the game screens OK is a click: it acts on Short only. An OK held
     * through a screen change (say, as the thrust key while Back leaves the
     * game) sends Long and Repeat but never Short, so it can't pick START or
     * anything else until it's released and pressed again. The game screens
     * have their own rule (ok_armed). */
    bool live = (m->screen == ScreenGame || m->screen == ScreenTutorial);
    if(!live && ev->key == InputKeyOk &&
       (ev->type == InputTypeLong || ev->type == InputTypeRepeat)) {
        return;
    }

    switch (m->screen) {
        case ScreenMenu: {
            ThrustMode prev_thrust = m->menu.thrust_mode;
            FuelMode   prev_fuel   = m->menu.fuel_mode;
            MenuAction a = menu_input(&m->menu, ev);
            if(m->menu.thrust_mode != prev_thrust || m->menu.fuel_mode != prev_fuel) {
                settings_save(&app->model);
            }
            handle_menu_action(app, a);
            break;
        }
        case ScreenGame: {
            GameAction a = game_input(&m->game, ev, m->menu.thrust_mode);
            if(a == GameActionExitToMenu) {
                leave_game(app);
            } else if(a == GameActionWin) {
                m->game_complete_new_record = high_score_record(m);
                set_screen(app, ScreenGameComplete);
            }
            break;
        }
        case ScreenTutorial: {
            /* While a popup is up the flight hasn't started: Back leaves
             * right away (no pause menu) and OK starts the level. Other
             * presses are dropped, so no burst or thrust (OK can be the
             * thrust key) fires on the way in; releases still reach the game
             * so no key stays held. */
            if(m->tutorial_popup_showing) {
                if(ev->key == InputKeyBack &&
                   (ev->type == InputTypeShort || ev->type == InputTypeLong)) {
                    leave_game(app);
                } else if(ev->key == InputKeyOk && ev->type == InputTypeShort) {
                    m->tutorial_popup_showing = false;
                } else if(ev->type == InputTypeRelease) {
                    game_input(&m->game, ev, m->menu.thrust_mode);
                }
                break;
            }
            /* Level advance / retry, on an OK press that began after
             * touchdown (see ok_armed in game.h). */
            if(ev->type == InputTypeShort && ev->key == InputKeyOk &&
               m->game.status != GameStatusFlying && m->game.ok_armed) {
                if(m->game.status == GameStatusLanded) {
                    if(m->tutorial_level < 2) {
                        m->tutorial_level++;
                        game_init_tutorial(&m->game, m->tutorial_level, m->game.score, m->menu.difficulty);
                        m->tutorial_popup_showing = true;
                    } else {
                        set_screen(app, ScreenMenu);
                    }
                } else {
                    game_init_tutorial(&m->game, m->tutorial_level, m->game.score, m->menu.difficulty);
                }
                break;
            }
            GameAction a = game_input(&m->game, ev, m->menu.thrust_mode);
            if(a == GameActionExitToMenu) leave_game(app);
            break;
        }
        case ScreenSettings: {
            if (ev->type != InputTypeShort && ev->type != InputTypeRepeat) break;
            SavedSettings before = settings_snapshot(m);
            switch (ev->key) {
                case InputKeyUp:
                    if (m->settings_focus > 0) m->settings_focus--;
                    settings_scroll_to_focus(m);
                    break;
                case InputKeyDown:
                    if (m->settings_focus < SettingsRowCount - 1) m->settings_focus++;
                    settings_scroll_to_focus(m);
                    break;
                case InputKeyLeft:
                    settings_row_change(m, m->settings_focus, -1);
                    break;
                case InputKeyRight:
                    settings_row_change(m, m->settings_focus, +1);
                    break;
                case InputKeyOk:
                    if (m->settings_focus == SettingsRowDifficulty) {
                        /* Scroll to Custom with Left/Right, then OK edits its limits. */
                        if (ev->type == InputTypeShort && m->menu.difficulty == DifficultyCustom) {
                            set_screen(app, ScreenCustomDifficulty);
                        }
                    } else if (m->settings_focus == SettingsRowSeed) {
                        if (ev->type == InputTypeShort) m->menu.seed = random_seed(m->menu.seed);
                    } else {
                        settings_row_change(m, m->settings_focus, +1);
                    }
                    break;
                case InputKeyBack:
                    set_screen(app, ScreenMenu);
                    break;
                default:
                    break;
            }
            /* Only touch the SD card when a value actually changed. */
            SavedSettings after = settings_snapshot(m);
            if(memcmp(&before, &after, sizeof(before)) != 0) {
                settings_save(&app->model);
                settings_apply_to_game(&app->model);
            }
            break;
        }
        case ScreenCustomDifficulty: {
            if (ev->type != InputTypeShort && ev->type != InputTypeRepeat) break;
            SavedSettings before = settings_snapshot(m);
            switch (ev->key) {
                case InputKeyUp:
                    if (m->custom_focus > 0) m->custom_focus--;
                    break;
                case InputKeyDown:
                    if (m->custom_focus < CUSTOM_ROWS - 1) m->custom_focus++;
                    break;
                case InputKeyLeft:
                    custom_row_change(m, m->custom_focus, -1);
                    break;
                case InputKeyRight:
                    custom_row_change(m, m->custom_focus, +1);
                    break;
                case InputKeyOk:
                case InputKeyBack:
                    if (ev->type == InputTypeShort) set_screen(app, ScreenSettings);
                    break;
                default:
                    break;
            }
            SavedSettings after = settings_snapshot(m);
            if(memcmp(&before, &after, sizeof(before)) != 0) {
                settings_save(&app->model);
                settings_apply_to_game(&app->model);
            }
            break;
        }
        case ScreenInfo:
        case ScreenScore:
            if(ev->key == InputKeyBack &&
               (ev->type == InputTypeShort || ev->type == InputTypeLong)) {
                set_screen(app, ScreenMenu);
            }
            break;
        case ScreenGameComplete:
            if(ev->type == InputTypeShort &&
               (ev->key == InputKeyOk || ev->key == InputKeyBack)) {
                set_screen(app, ScreenMenu);
            }
            break;
    }
}

static void handle_tick(App* app, float dt) {
    if(app->model.screen == ScreenGame || app->model.screen == ScreenTutorial) {
        AppModel* m = &app->model;
        /* Kept up to date here because game_init's memset clears it on
         * every retry and level change. */
        m->game.vgm_missing = m->vgm && !vgm_tilt_present(m->vgm);
        if(m->tutorial_popup_showing) {
            /* No ticks, so no audio updates: make sure nothing keeps
             * sounding (e.g. a landing chime cut short by OK). */
            game_audio_silence();
            return;
        }
        if(m->vgm && vgm_tilt_present(m->vgm)) {
            m->game.tilt_pitch = vgm_tilt_roll(m->vgm);
            m->game.tilt_roll  = vgm_tilt_pitch(m->vgm);
            if(m->game.needs_tilt_cal) {
                m->game.tilt_pitch_offset = m->game.tilt_pitch;
                m->game.tilt_roll_offset  = m->game.tilt_roll;
                m->game.needs_tilt_cal    = false;
            }
        }
        game_tick(&m->game, m->menu.thrust_mode, dt);
        if(m->game.exit_requested) {
            /* Back held long enough mid-flight. Return now: audio is already
             * stopped, and an update would switch the vibro back on. */
            leave_game(app);
            return;
        }
        game_audio_update(&m->game, m->menu.thrust_mode,
                          m->menu.sound_level, m->menu.vibration_level);

        /* Debug metrics — always updated so the overlay is current even if
         * logging is off. Serial log fires at 6 Hz to stay readable. */
        m->debug_tick_count++;
        uint32_t qd = furi_message_queue_get_count(app->queue);
        if((uint8_t)qd > m->debug_peak_queue) m->debug_peak_queue = (uint8_t)qd;
        m->debug_queue_depth = (uint8_t)(qd < 255 ? qd : 255);
        m->debug_free_heap   = (uint32_t)memmgr_get_free_heap();
        if(m->debug_hud && (m->debug_tick_count % 10 == 0)) {
            int deg = (int)(m->game.angle * (180.0f / 3.14159265f));
            FURI_LOG_I("LanderDbg","HP:%lu TK:%lu Q:%u DR:%luus TG:%lums A:%d X:%d Y:%d",
                       (unsigned long)m->debug_free_heap,
                       (unsigned long)m->debug_tick_count,
                       (unsigned)m->debug_queue_depth,
                       (unsigned long)m->debug_draw_us,
                       (unsigned long)m->debug_gap_ms,
                       deg, (int)m->game.vx, (int)m->game.vy);
        }
    }
}

/* While a level is being flown, keep the backlight on: Full Tilt needs no
 * key presses, so the display would time out mid-descent. When the flight
 * stops (a banner, the pause menu, a popup, or a screen change), hand the
 * backlight back to the system and light it again, so its timeout starts
 * from that moment instead of running out at once after a long level. */
static void backlight_update(App* app) {
    const AppModel* m = &app->model;
    bool flying = (m->screen == ScreenGame || m->screen == ScreenTutorial) &&
                  m->game.status == GameStatusFlying && !m->game.pause.open &&
                  !m->tutorial_popup_showing;
    if(flying == app->backlight_forced) return;
    app->backlight_forced = flying;
    if(flying) {
        notification_message(app->notification, &sequence_display_backlight_enforce_on);
    } else {
        notification_message(app->notification, &sequence_display_backlight_enforce_auto);
        notification_message(app->notification, &sequence_display_backlight_on);
    }
}

/* ----- App entrypoint ---------------------------------------------------- */

int32_t moon_lander_app(void* p) {
    UNUSED(p);

    App* app = malloc(sizeof(App));
    memset(app, 0, sizeof(App));
    app->queue = furi_message_queue_alloc(16, sizeof(AppEvent));
    app->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    app->tick_timer =
        furi_timer_alloc(tick_timer_callback, FuriTimerTypePeriodic, app);

    menu_init(&app->model.menu);
    settings_load(&app->model);
    settings_apply_to_game(&app->model);
    app->model.screen = ScreenMenu;
    app->model.should_exit = false;
    high_score_load(&app->model.high_score, &app->model.high_score_seed);

    app->view_port = view_port_alloc();
    view_port_draw_callback_set(app->view_port, draw_callback, app);
    view_port_input_callback_set(app->view_port, input_callback, app);

    app->gui = furi_record_open(RECORD_GUI);
    app->notification = furi_record_open(RECORD_NOTIFICATION);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);

    uint32_t tick_freq = furi_kernel_get_tick_frequency();
    app->last_tick_ms = furi_get_tick();

    while (!app->model.should_exit) {
        AppEvent ev;
        if (furi_message_queue_get(app->queue, &ev, FuriWaitForever) != FuriStatusOk) continue;
        /* Out of the queue, so let the timer post the next tick, even if we
         * now wait for the mutex while a frame draws. */
        if (ev.type == AppEventTick) app->tick_pending = false;

        furi_mutex_acquire(app->mutex, FuriWaitForever);

        if (ev.type == AppEventInput) {
            handle_input_event(app, &ev.input);
        } else { // AppEventTick
            uint32_t now = furi_get_tick();
            uint32_t gap_ms = (now - app->last_tick_ms) * 1000 / tick_freq;
            AppModel* m = &app->model;
            if (gap_ms > m->debug_gap_ms_max) m->debug_gap_ms_max = gap_ms;
            /* Roll the debug window over each wall-clock second. Done here
             * rather than in handle_tick so it also runs while the tutorial
             * popup pauses the game. */
            if (now / tick_freq != app->last_tick_ms / tick_freq) {
                m->debug_draw_us     = m->debug_draw_us_max;
                m->debug_gap_ms      = m->debug_gap_ms_max;
                m->debug_draw_us_max = 0;
                m->debug_gap_ms_max  = 0;
            }
            float dt = (float)(now - app->last_tick_ms) / (float)tick_freq;
            app->last_tick_ms = now;
            /* Guard against giant dt after a stall. */
            if (dt > 0.1f) dt = 0.1f;
            handle_tick(app, dt);
        }

        backlight_update(app);
        furi_mutex_release(app->mutex);
        /* Off the game screens, redraw after every event. In-game, redraw only
         * on ticks, capped by FRAME_MS (STILL_FRAME_MS while a banner or the
         * tutorial popup is up); a key event shows up on the next frame. */
        bool live = (app->model.screen == ScreenGame || app->model.screen == ScreenTutorial);
        if (!live) {
            view_port_update(app->view_port);
        } else if (ev.type == AppEventTick) {
            bool still = app->model.tutorial_popup_showing || app->model.game.pause.open ||
                         app->model.game.status != GameStatusFlying;
            uint32_t now = furi_get_tick();
            uint32_t min_gap = (still ? STILL_FRAME_MS : FRAME_MS) * tick_freq / 1000;
            if (now - app->last_draw_tick >= min_gap) {
                app->last_draw_tick = now;
                view_port_update(app->view_port);
            }
        }
    }

    furi_timer_stop(app->tick_timer);
    furi_timer_free(app->tick_timer);
    /* Belt-and-suspenders: if somehow we exit while still in-game, release
     * the speaker and turn off vibration so we don't leave it stuck. */
    game_audio_stop();
    if(app->backlight_forced) {
        notification_message(app->notification, &sequence_display_backlight_enforce_auto);
    }
    furi_record_close(RECORD_NOTIFICATION);
    gui_remove_view_port(app->gui, app->view_port);
    view_port_free(app->view_port);
    furi_record_close(RECORD_GUI);
    furi_message_queue_free(app->queue);
    furi_mutex_free(app->mutex);
    free(app);
    return 0;
}
