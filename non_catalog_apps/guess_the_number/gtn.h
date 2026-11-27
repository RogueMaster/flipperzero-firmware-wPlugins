#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <gui/gui.h>
#include <input/input.h>

#define GTN_VERSION "1.2"

#define GTN_DEFAULT_FLOOR   1
#define GTN_DEFAULT_CEILING 100
#define GTN_LIMIT_MIN       (-999)
#define GTN_LIMIT_MAX       9999

#define GTN_SPLASH_MS       2700
#define GTN_STARTING_MS     850 /* time.sleep(0.85) */
#define GTN_CELEBRATE_MS    2000 /* time.sleep(2) */
#define GTN_THINK_MIN_MS    700 /* computer "thinking" pause, */
#define GTN_THINK_JITTER_MS 300 /* 0.7 - 1.0 s, around the 0.85 s */
#define GTN_BYE_MS          1500

#define GTN_MENU_ITEMS 5

#define GTN_STATS_PAGES   4
#define GTN_RESET_HOLD_MS 1500
#define GTN_HS_COUNT      3
#define GTN_HS_MIN_SPAN   10 /* ranges smaller than this don't enter the Hall of Fame */
#define GTN_STATS_MAGIC   0x534E5447UL /* "GTNS" */
#define GTN_STATS_VERSION 1

typedef struct {
    uint16_t score;
    int16_t floor;
    int16_t ceiling;
    uint16_t tries;
} GtnHighscore;

typedef struct {
    uint32_t magic;
    uint32_t version;
    /* you guess */
    uint32_t p_played;
    uint32_t p_won;
    uint32_t p_guesses;
    uint32_t p_best_tries; /* 0 = none yet */
    uint32_t p_psychic;
    /* computer guesses */
    uint32_t c_played;
    uint32_t c_found;
    uint32_t c_tries;
    uint32_t c_cheats;
    uint32_t c_psychic;
    /* overall */
    uint32_t play_seconds;
    GtnHighscore hs[GTN_HS_COUNT];
} GtnStats;

typedef enum {
    GtnScreenSplash,
    GtnScreenMenu,
    GtnScreenRange,
    GtnScreenStarting,
    GtnScreenPlayer,
    GtnScreenComputer,
    GtnScreenCelebrate,
    GtnScreenText,
    GtnScreenAbout,
    GtnScreenStats,
    GtnScreenBye,
} GtnScreen;

typedef enum {
    GtnModePlayer,
    GtnModeComputer,
} GtnMode;

typedef enum {
    GtnNextMenu,
    GtnNextComputerGuess,
} GtnNext;

typedef enum {
    GtnFxNone,
    GtnFxBlink,
    GtnFxSuccess,
    GtnFxError,
} GtnFx;

typedef struct {
    GtnScreen screen;
    uint32_t screen_since;
    GtnMode mode;

    /* menu (with sliding highlight) */
    uint8_t menu_index;
    uint8_t menu_prev;
    uint32_t menu_changed_at;

    /* range setup */
    uint8_t range_row; /* 0 = min, 1 = max */
    int32_t floor;
    int32_t ceiling;
    bool range_error;
    int8_t nudge; /* -1 / +1: which arrow got pushed */
    uint32_t nudge_at;

    /* "Starting game..." */
    bool started;

    /* player guesses */
    int32_t secret;
    int32_t guess;
    uint32_t tries;
    int8_t hint_dir; /* +1 go bigger, -1 go smaller, 0 none */
    uint32_t wrong_at; /* for the shake */

    /* computer guesses */
    int32_t lower;
    int32_t upper;
    uint32_t max_tries;
    bool thinking;
    uint32_t think_ms;

    /* celebration */
    char cheer[20];
    int32_t cheer_value;

    /* status line on the game screens */
    char msg[96];

    /* generic text page */
    char title[24];
    char body[256];
    char button[12];
    GtnNext text_next;
    bool title_shake;
    size_t scroll;
    size_t text_lines; /* filled in by the renderer */

    /* stats */
    GtnStats stats;
    bool stats_dirty;
    uint8_t page;
    int8_t page_dir;
    uint32_t page_since;
    bool hold_ok;
    uint32_t hold_since;
    uint32_t toast_at;
    uint32_t last_tick;
    uint32_t ms_accum;

    uint16_t repeat_count;
    GtnFx fx;
    bool exit;
} GtnState;

/* provided by the platform (Flipper app or preview tool) */
uint32_t gtn_random(void);

void gtn_init(GtnState* s, uint32_t now_ms);
void gtn_stats_reset(GtnStats* st);
void gtn_input(GtnState* s, InputKey key, InputType type, uint32_t now_ms);
void gtn_tick(GtnState* s, uint32_t now_ms);
void gtn_draw(Canvas* canvas, GtnState* s, uint32_t now_ms);
