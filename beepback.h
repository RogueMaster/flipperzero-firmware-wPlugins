/*
 * BEEPBACK v4 - a memory and reaction game for the Flipper Zero.
 *
 * Five modes: repeat a sequence, obey a rule while you repeat it, react to
 * single cues, hold one rule for an endless run, or play the daily that every
 * device generates identically from the date.
 *
 * by Tijnv50, with Claude (Opus 5)
 *
 * Every number in this file was settled in the browser build. If you change
 * one here, change it there too, or the two stop being the same game.
 */
#pragma once

#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <input/input.h>
#include <notification/notification_messages.h>

#define BB_W 128
#define BB_H 64

/* ------------------------------------------------------------------ */
/* Gameplay tunables                                                    */
/* ------------------------------------------------------------------ */
#define BB_MAX_SEQ  64 /* a challenge grows until you fail; this is the cap */
#define BB_MAX_PRESS 96 /* DOUBLE and LAST TWICE make more presses than steps */
#define BB_LIVES     3
#define BB_START_LEN 4 /* round 1 builds to this, +1 each round */

#define BB_TONE_MS      220
#define BB_GAP_MS       110
#define BB_ASSIST_GAP   60
#define BB_FLASH_MS     70
#define BB_SHAPE_R      17
#define BB_LISTEN_MS    700
#define BB_GO_MS        450
#define BB_SUCCESS_MS   750
#define BB_ROUND_MS     1100
#define BB_WRONG_MS     900
#define BB_RETRY_MS     650
#define BB_RULECARD_MS  2200
#define BB_RULE_BONUS   1000 /* rules and challenge get an extra second */
#define BB_OVER_LOCK    800 /* game over ignores input this long */
#define BB_PRESS_LED_MS 130
#define BB_TICK_MS      20

/* Response window for the whole sequence, by TIME setting */
#define BB_TIME_EASY   5000
#define BB_TIME_NORMAL 4000
#define BB_TIME_HARD   3000
#define BB_TIME_INSANE 2000

/* Reflex: one ramp for everyone, the setting only changes its steepness */
#define BB_RX_START 1000
#define BB_RX_FLOOR 40 /* no real floor: every run has to end */

/* Splash */
#define BB_SP_HOLD  1100
#define BB_SP_FADE  450
#define BB_SP_GLIDE 1200
#define BB_SP_FLASH 620
#define BB_SP_WIPE  340
#define BB_SP_TURNS 1.5f
#define BB_SP_RING  78.0f

#define BB_DIFF_COUNT   4
#define BB_SPEED_COUNT  3
#define BB_ASSIST_COUNT 4
#define BB_RULE_COUNT   7
#define BB_MODE_COUNT   5
#define BB_PRAISE_COUNT 7

#define BB_HEART_W 7
#define BB_HEART_H 6

/* ------------------------------------------------------------------ */
/* Types                                                               */
/* ------------------------------------------------------------------ */

typedef enum {
    BbBtnUp = 0,
    BbBtnDown,
    BbBtnLeft,
    BbBtnRight,
    BbBtnOk,
    BbBtnCount,
} BbButton;

typedef enum {
    BbAssistOff = 0, /* ears only */
    BbAssistLed,
    BbAssistShapes,
    BbAssistArrows,
} BbAssist;

typedef enum {
    BbModeClassic = 0,
    BbModeRules,
    BbModeReflex,
    BbModeChallenge,
    BbModeDaily,
} BbMode;

/* Challenge and daily share one engine: one rule, one endless sequence. */
#define bb_is_challenge(mode) ((mode) == BbModeChallenge || (mode) == BbModeDaily)

typedef enum {
    BbRuleSkip = 0, /* that button plays but you never press it */
    BbRuleDouble, /* press it twice every time it appears */
    BbRuleNoDoubles, /* the same button twice running is one press */
    BbRuleEveryOther, /* steps 1, 3, 5 ... */
    BbRuleLastTwice, /* press the final step twice */
    BbRuleSwap, /* one button means another */
    BbRuleBackwards, /* last step first */
} BbRule;

typedef enum {
    BbShapeCircle = 0,
    BbShapeTriangle,
    BbShapeSquare,
    BbShapePentagon,
    BbShapeStar,
} BbShape;

typedef struct {
    uint8_t step[BB_MAX_SEQ]; /* the sequence as played */
    uint8_t len;
} BbSeq;

typedef struct {
    uint8_t press[BB_MAX_PRESS]; /* what the rule says to actually press */
    uint8_t len;
} BbPresses;

/* ------------------------------------------------------------------ */
/* Score maths                                                         */
/*                                                                     */
/* Difficulty pays instead of splitting the leaderboard. Normal is the  */
/* baseline; the numbers came out of simulating four skill levels       */
/* against every setting, not out of taste. Multipliers are stored as   */
/* hundredths so the firmware never needs a float.                      */
/* ------------------------------------------------------------------ */
extern const uint16_t bb_mult_time[BB_DIFF_COUNT]; /* 70, 100, 155, 600 */
extern const uint16_t bb_mult_time_rx[BB_DIFF_COUNT]; /* 70, 100, 150, 230 */
extern const uint16_t bb_mult_speed[BB_SPEED_COUNT]; /* 75, 100, 145 */
extern const uint16_t bb_time_ms[BB_DIFF_COUNT];
extern const uint16_t bb_rx_shrink[BB_DIFF_COUNT]; /* ms tighter per hit */
extern const uint16_t bb_rx_gap[BB_SPEED_COUNT]; /* wait between cues */
extern const uint16_t bb_speed_tone[BB_SPEED_COUNT];
extern const uint16_t bb_speed_gap[BB_SPEED_COUNT];

/* multiplier in hundredths, so 155 means x1.55 */
uint16_t bb_multiplier(BbMode mode, uint8_t diff, uint8_t speed);
/* value * mult / 100, rounded the same way the browser build rounds */
uint32_t bb_apply_mult(uint32_t base, uint16_t mult);
/* points for one reflex hit at the given window */
uint32_t bb_rx_hit_value(uint16_t window_ms);
/* response window including the rule allowance */
uint16_t bb_window_ms(BbMode mode, uint8_t diff);

/* ------------------------------------------------------------------ */
/* Rules                                                               */
/* ------------------------------------------------------------------ */

/* Apply the active rule to a sequence, producing what must be pressed. */
void bb_apply_rule(const BbSeq* seq, BbRule rule, uint8_t a, uint8_t b, BbPresses* out);
/* Would this rule leave a sequence worth playing? Rejects anything that
   ends up empty or a single button mashed, which is not a memory test. */
bool bb_rule_fits(const BbSeq* seq, BbRule rule, uint8_t a, uint8_t b);

/* ------------------------------------------------------------------ */
/* Deterministic generator, so every device builds the same daily      */
/* ------------------------------------------------------------------ */
typedef struct {
    uint32_t state;
} BbRng;

void bb_rng_seed(BbRng* r, uint32_t seed);
uint32_t bb_rng_next(BbRng* r);
uint8_t bb_rng_below(BbRng* r, uint8_t n);
/* today as YYYYMMDD, from the Flipper's clock */
uint32_t bb_today_seed(void);

/* ------------------------------------------------------------------ */
/* Presentation tables                                                 */
/* ------------------------------------------------------------------ */

/* tone in Hz per button, low to high: DOWN LEFT OK RIGHT UP */
extern const uint16_t bb_button_hz[BbBtnCount];
/* BbShape per button, in the same low-to-high order */
extern const uint8_t bb_button_shape[BbBtnCount];
/* one-word name per button, for the tutorial and the rule cards */
extern const char* const bb_button_name[BbBtnCount];

typedef enum {
    BbLedOff = 0,
    BbLedRed,
    BbLedYellow,
    BbLedGreen,
    BbLedBlue,
    BbLedViolet,
    BbLedCount,
} BbLedColor;

/* BbLedColor per button, following the tone ladder */
extern const uint8_t bb_button_led[BbBtnCount];

/* ------------------------------------------------------------------ */
/* Layout                                                              */
/*                                                                     */
/* Every screen is 128x64 and nothing may be drawn outside it. These    */
/* are the numbers the browser build settled on; two of its bugs were   */
/* arrows drawn past the bottom edge, so the footer band is the last    */
/* thing on the screen and nothing is centred below it.                 */
/* ------------------------------------------------------------------ */
#define BB_ROW_L  14 /* list row label, left edge                     */
#define BB_ROW_R  114 /* list row value, right edge for plain rows     */
#define BB_ROW_AL 4 /* left arrow of an adjustable row                */
#define BB_ROW_AR 118 /* right arrow of an adjustable row              */
#define BB_ROW_VR 112 /* adjustable value, right-aligned here          */
#define BB_TITLE_H 13 /* title bar fills y = 0..12                     */
#define BB_TITLE_Y 6 /* title text centred here                        */
#define BB_FOOT_Y0 53 /* footer strip fills y = 53..63                 */
#define BB_FOOT_Y  58 /* footer text centred here                      */
#define BB_BODY_Y  32 /* body centre with a footer                     */
#define BB_BODY_Y2 38 /* body centre without one                       */
#define BB_DOT_MAX 14 /* past this many steps the dots become a count  */

/* ------------------------------------------------------------------ */
/* Scenes                                                              */
/* ------------------------------------------------------------------ */

typedef enum {
    BbSceneLauncher = 0,
    BbSceneSplash,
    BbSceneTutorial,
    BbSceneMenu,
    BbSceneModeSelect,
    BbSceneRulePick,
    BbSceneSetup,
    BbSceneGame,
    BbScenePause,
    BbSceneOver,
    BbSceneHowToPick,
    BbSceneHowTo,
    BbSceneSoundTest,
    BbSceneSettings,
    BbSceneSounds,
    BbSceneScores,
    BbSceneScoreDetail,
    BbSceneReset,
    BbSceneBoardPick,
    BbSceneBoard,
    BbSceneCredits,
    BbSceneCount,
} BbScene;

/* ------------------------------------------------------------------ */
/* Stored state                                                        */
/* ------------------------------------------------------------------ */

#define BB_VOL_COUNT 5 /* 0..4, and 0 means silent */

typedef struct {
    uint8_t volume; /* 0..BB_VOL_COUNT-1 */
    uint8_t assist; /* BbAssist */
    uint8_t diff; /* TIME, index into bb_time_ms */
    uint8_t speed; /* index into bb_speed_tone */
    bool tutorial_done;
} BbSettings;

/* classic, rules and reflex keep a best per setting; challenge keeps one
   per rule; the daily keeps one for the day it belongs to. */
#define BB_LADDER_MODES 3

typedef struct {
    uint32_t best[BB_LADDER_MODES][BB_DIFF_COUNT][BB_SPEED_COUNT][BB_ASSIST_COUNT];
    uint32_t ch_best[BB_RULE_COUNT][BB_ASSIST_COUNT];
    uint32_t daily_best[BB_ASSIST_COUNT];
    uint32_t daily_date; /* YYYYMMDD the daily records belong to */
    bool daily_done; /* the one attempt for that date is spent */
} BbRecords;

/* ------------------------------------------------------------------ */
/* A run in progress                                                   */
/* ------------------------------------------------------------------ */

typedef enum {
    BbPhaseRuleCard = 0, /* rules and challenge announce the rule first */
    BbPhaseListen,
    BbPhasePlayback,
    BbPhaseGo,
    BbPhaseInput,
    BbPhaseHold, /* the last press rings before the stage resolves */
    BbPhaseSuccess,
    BbPhaseRound,
    BbPhaseWrong,
    BbPhaseRetry,
    BbPhaseRxReady, /* reflex says its piece before the first cue */
    BbPhaseRxWait, /* the gap between cues                        */
    BbPhaseRxCue, /* a cue is up and the bar is draining          */
    BbPhaseOver,
    BbPhaseCount,
} BbPhase;

typedef struct {
    BbMode mode;
    uint8_t diff;
    uint8_t speed;
    uint8_t assist; /* captured, so settings cannot move a live run */
    uint16_t mult; /* captured once, in hundredths */
    BbRule rule;
    uint8_t ra; /* the button the rule talks about */
    uint8_t rb; /* and the one it becomes, for X IS Y */
    bool rule_random; /* challenge picked RANDOM, so it rolled one */
    BbRng rng;

    BbSeq seq;
    BbPresses press;
    uint8_t shown; /* steps of seq currently in play */
    uint8_t start_len; /* the shortest stage the rule can survive */
    uint8_t target; /* the length that clears the round */
    uint8_t round; /* 1-based; challenge has no rounds */
    uint8_t lives;
    uint8_t idx; /* how many presses are in so far */

    uint32_t score;
    uint32_t award; /* the last award, already multiplied */
    uint32_t hits; /* reflex cues taken */

    BbPhase phase;
    uint32_t phase_end; /* app->now when the phase is up */
    uint8_t play_i; /* playback cursor */
    bool play_on; /* a tone is sounding this playback slot */
    uint32_t win_end; /* the response deadline */
    uint16_t win_ms; /* and how long it was, for the bar */
    uint32_t flash_end; /* a pressed button is lit until here */
    uint8_t flash_btn;

    uint16_t rx_win; /* reflex: the current window */
    uint8_t rx_cue;
    bool quit; /* the run was ended from the pause screen */
    bool record; /* it beat the stored best */
} BbRun;

/* ------------------------------------------------------------------ */
/* The application                                                     */
/* ------------------------------------------------------------------ */

typedef struct {
    Gui* gui;
    ViewPort* view_port;
    FuriMessageQueue* queue;
    FuriMutex* mutex;
    NotificationApp* notifications;
    bool running;
    uint32_t seed; /* the hardware draw a non-daily run is built from */

    uint32_t now; /* ms since launch, advanced only by bb_tick */
    BbScene scene;
    uint32_t scene_at; /* app->now when this scene opened */

    BbSettings set;
    BbRecords rec;
    BbRun run;

    /* what the logic wants the hardware to do; the app loop applies it */
    uint8_t led; /* BbLedColor */
    uint16_t tone_hz; /* 0 is silence */

    /* cursors, one per list, so backing out and returning keeps your place */
    uint8_t menu_cur;
    uint8_t mode_page;
    uint8_t mode_row;
    uint8_t rule_cur; /* BB_RULE_COUNT means RANDOM */
    uint8_t setup_cur;
    uint8_t settings_cur;
    uint8_t sounds_cur;
    uint8_t scores_cur;
    uint8_t reset_cur;
    uint8_t board_cur;
    uint8_t howto_cur;
    uint8_t howto_page;
    uint8_t tut_page;
    uint8_t det_mode; /* the scores detail dials */
    uint8_t det_diff;
    uint8_t det_speed;
    uint8_t det_cur;
    uint8_t sound_btn; /* the sound test cursor */
    uint32_t flash_at; /* a transient confirmation was shown at this time */
} BeepbackApp;

/* ------------------------------------------------------------------ */
/* State machine (beepback_nav.c)                                      */
/* ------------------------------------------------------------------ */

/* clamp a cursor without wrapping; true when it actually moved */
bool bb_list_move(uint8_t* cur, uint8_t count, int8_t delta);
/* where BACK goes from here; BbSceneCount means "leave the app" */
BbScene bb_back_target(const BeepbackApp* app);
/* enter a scene, resetting whatever that scene owns */
void bb_go(BeepbackApp* app, BbScene scene);
/* one short press */
void bb_input(BeepbackApp* app, InputKey key);
/* advance the clock; every timed transition happens in here */
void bb_tick(BeepbackApp* app, uint32_t dt_ms);
void bb_app_init(BeepbackApp* app);
/* EARS with the sound off leaves nothing to go on, so force SHAPES */
uint8_t bb_effective_assist(const BeepbackApp* app);

/* how many entries each list has, so no cursor can run off its end */
uint8_t bb_list_count(const BeepbackApp* app, BbScene scene);
/* would a left or right press on this screen change anything? the screens
   draw an arrow only where this says yes */
bool bb_can_adjust(const BeepbackApp* app, int8_t delta);
/* how many pages the how-to has for a topic */
uint8_t bb_howto_pages(uint8_t topic);

/* ------------------------------------------------------------------ */
/* Runs (beepback_game.c)                                              */
/* ------------------------------------------------------------------ */

void bb_run_start(BeepbackApp* app, BbMode mode);
void bb_run_tick(BeepbackApp* app);
void bb_run_press(BeepbackApp* app, BbButton btn);
void bb_run_end(BeepbackApp* app, bool quit);
/* build the presses for the stage now on screen */
void bb_run_build_presses(BbRun* run);
/* the sequence for a fresh round, re-rolled until the rule can live with it */
void bb_run_new_round(BbRun* run, uint8_t target);
/* one more step on the end of a challenge sequence, guarded the same way */
void bb_run_grow(BbRun* run);
/* the rule, its buttons and the run's settings, all from one seed */
void bb_daily_setup(BbRun* run, uint32_t seed);
/* the stored best for a finished run, and where it is kept */
uint32_t* bb_record_slot(BeepbackApp* app, const BbRun* run);
/* the board's headline: the best across time and speed for a mode */
uint32_t bb_best_of_mode(const BeepbackApp* app, BbMode mode, uint8_t assist);
/* the daily records belong to one date; a new day clears them */
void bb_daily_refresh(BeepbackApp* app, uint32_t today);
/* rules, challenge and the daily transform what you press; classic does not */
bool bb_run_has_rule(BbMode mode);
