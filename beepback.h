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
#include <storage/storage.h>

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
#define BB_VOL_COUNT    4 /* OFF LOW MID HIGH */

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
/* Score                                                               */
/*                                                                     */
/* A point for every note you play back right, ten more for clearing a  */
/* round. No multiplier: TIME and SPEED change how hard the run is, not */
/* what it pays, so the number on screen is always something you could  */
/* have counted yourself.                                              */
/* ------------------------------------------------------------------ */
#define BB_ROUND_BONUS 10 /* clearing a round, on top of the notes */

extern const uint16_t bb_time_ms[BB_DIFF_COUNT];
extern const uint16_t bb_rx_shrink[BB_DIFF_COUNT]; /* ms tighter per hit */
extern const uint16_t bb_rx_gap[BB_SPEED_COUNT]; /* wait between cues */
extern const uint16_t bb_speed_tone[BB_SPEED_COUNT];
extern const uint16_t bb_speed_gap[BB_SPEED_COUNT];

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
/* a date for bb_today_seed() to hand back instead of the clock's, so a
   test can name the day it is pinning; zero is the clock, and the
   firmware never sets it */
extern uint32_t bb_seed_override;

/* ==================================================================== */
/* Everything below is a port of the browser build, screen for screen   */
/* and scene for scene. Where the two could differ they do not: the     */
/* names, the layout numbers and the order of the switch statements all */
/* follow beepback.html, because that is the game and this is a second  */
/* way of running it.                                                   */
/* ==================================================================== */

/* ------------------------------------------------------------------ */
/* Presentation tables                                                 */
/* ------------------------------------------------------------------ */

extern const uint16_t bb_button_hz[BbBtnCount]; /* FREQ  */
extern const char* const bb_button_name[BbBtnCount]; /* NAME  */
extern const char* const bb_button_short[BbBtnCount]; /* SHORT */
extern const uint8_t bb_button_shape[BbBtnCount]; /* SHAPE */

extern const char* const bb_diff_name[BB_DIFF_COUNT]; /* DIFF_NAME  */
extern const char* const bb_speed_name[BB_SPEED_COUNT]; /* SPEED_NAME */
extern const char* const bb_mode_name[BB_MODE_COUNT]; /* MODE_NAME  */
extern const char* const bb_mode_blurb[BB_MODE_COUNT];
extern const char* const bb_vol_name[BB_VOL_COUNT]; /* VOL_NAME    */
extern const char* const bb_assist_name[BB_ASSIST_COUNT]; /* ASSIST_NAME */
extern const char* const bb_mode_label[BB_ASSIST_COUNT]; /* MODE_LABEL  */
extern const char* const bb_praise[BB_PRAISE_COUNT]; /* PRAISE */
extern const char* const bb_led_name[BbBtnCount]; /* LED_NAME */
extern const char* const bb_rule_label[BB_RULE_COUNT]; /* RULES[].label */
extern const char* const bb_rule_tip[BB_RULE_COUNT]; /* RULES[].tip   */
extern const char* const bb_rule_help[BB_RULE_COUNT][3]; /* RULES[].help */

/* SPEED_SET lives with the other frozen tables above */
extern const uint8_t bb_vol_gain[BB_VOL_COUNT]; /* VOL_GAIN, percent */

typedef enum {
    BbLedOff = 0,
    BbLedRed,
    BbLedYellow,
    BbLedGreen,
    BbLedBlue,
    BbLedViolet,
    BbLedCount,
} BbLedColor;

extern const uint8_t bb_button_led[BbBtnCount]; /* LED_BTN */

/* ------------------------------------------------------------------ */
/* Tunes                                                               */
/*                                                                     */
/* JINGLE in the browser is a list of notes played on a timeline. Here  */
/* it is a list the main loop steps through, which is the same thing    */
/* with the scheduler written out.                                     */
/* ------------------------------------------------------------------ */

typedef struct {
    uint16_t f; /* Hz, 0 for a rest */
    uint16_t ms;
} BbNote;

extern const BbNote bb_jingle_go[3];
extern const BbNote bb_jingle_win[3];
extern const BbNote bb_jingle_round[4];
extern const BbNote bb_jingle_fail[3];
extern const BbNote bb_jingle_over[4];

/* ------------------------------------------------------------------ */
/* Layout, all of it straight off the browser's screens                */
/* ------------------------------------------------------------------ */
#define BB_ROW_L  14 /* list row label   */
#define BB_ROW_R  114 /* list row value   */
#define BB_CHEV_L 2 /* screen-edge chevrons */
#define BB_CHEV_R 121
#define BB_ROW_VAL 112 /* an adjustable row's value */
#define BB_ROW_AL 4 /* and its two arrows        */
#define BB_ROW_AR 118
#define BB_DOT_MAX 14 /* past this the step dots become a count */

/* ------------------------------------------------------------------ */
/* Scenes, named as the browser names them                             */
/* ------------------------------------------------------------------ */

typedef enum {
    /* No launcher scene. The browser needs a "press to play" screen
       because a page cannot make a sound until someone has touched it,
       and because a web page has nothing to quit back to. The Flipper's
       own apps browser has already done both jobs by the time this runs,
       so the intro starts immediately and BACK on the menu leaves. */
    BbSceneSplash = 0,
    BbSceneMenu,
    BbSceneMode,
    BbSceneChPick,
    BbSceneSetup,
    BbSceneRuleCard,
    BbSceneListen,
    BbScenePlayback,
    BbSceneGo,
    BbSceneInput,
    BbSceneHold,
    BbSceneSuccess,
    BbSceneRoundClear,
    BbSceneWrong,
    BbSceneRetry,
    BbSceneReflexGap,
    BbSceneReflexCue,
    BbSceneGameOver,
    BbSceneSettings,
    BbSceneDetail,
    BbSceneReset,
    BbSceneHelp,
    BbSceneTutorial,
    BbSceneRulesGuide,
    BbSceneRuleList,
    BbSceneRuleInfo,
    BbSceneReflexGuide,
    BbSceneSoundTest,
    BbSceneScorePick,
    BbSceneScores,
    BbSceneCredits,
    BbSceneCount,
} BbScene;

/* GAME_SCENES: the scenes BACK pauses rather than leaves */
bool bb_in_game(BbScene scene);

/* ------------------------------------------------------------------ */
/* Settings and records                                                */
/* ------------------------------------------------------------------ */

typedef struct {
    uint8_t volume; /* SET.volume, 0..3 */
    uint8_t assist; /* SET.assist, 0..3 */
    uint8_t speed; /* SET.speed,  0..2 */
    uint8_t diff; /* S.diff,     0..3 */
    bool tutorial_done; /* the browser's firstRun, inverted */
} BbSettings;

#define BB_LADDER_MODES 3

typedef struct {
    uint32_t best[BB_LADDER_MODES][BB_DIFF_COUNT][BB_SPEED_COUNT][BB_ASSIST_COUNT];
    uint32_t ch_best[BB_RULE_COUNT][BB_ASSIST_COUNT];
    uint32_t daily_best[BB_ASSIST_COUNT];
    uint32_t daily_date;
    bool daily_done;
} BbRecords;

/* one segment of a rule line: a word, or a button drawn in a chip */
typedef struct {
    const char* text;
    int8_t btn; /* < 0 when this segment is a word */
} BbSeg;

/* ------------------------------------------------------------------ */
/* The application, which is the browser's S and SET side by side      */
/* ------------------------------------------------------------------ */

typedef struct {
    Gui* gui;
    ViewPort* view_port;
    FuriMessageQueue* queue;
    FuriMutex* mutex;
    NotificationApp* notifications;
    bool running;
    uint32_t seed; /* the hardware draw a non-daily run is built from */

    uint32_t now;
    BbScene scene;
    bool paused;
    uint32_t pause_at;
    uint32_t phase; /* S.phase: when the current scene is up */

    BbSettings set;
    BbRecords rec;

    /* the run */
    BbSeq base;
    BbPresses expected;
    uint8_t stage, target;
    uint8_t play_idx;
    bool tone_on;
    uint32_t step_start;
    uint8_t input_idx;
    uint32_t input_start, input_end;
    uint8_t lives, round;
    uint32_t score, run_best, prev_best;
    uint8_t praise;
    int8_t last_press;
    uint32_t press_flash;
    bool new_best;
    uint8_t run_mode; /* the assist the run was played on */
    uint8_t run_game_mode, run_diff, run_speed;
    /* the daily runs at NORMAL whatever you had set, so what you had set
       is put aside for the length of the run rather than overwritten */
    uint8_t held_diff, held_speed;
    bool holding;
    uint32_t led_warn;
    int8_t sweep_idx;
    uint32_t lock_until;
    uint8_t go_page;

    /* rules */
    int8_t rule_idx;
    uint8_t rule_a, rule_b;
    BbSeg rule_segs[3];
    uint8_t rule_seg_n;
    BbRng rng;
    bool rng_seeded; /* the daily runs on a seeded generator */

    /* reflex */
    uint32_t rx_hits;
    uint16_t rx_window;
    uint8_t rx_cue;
    uint32_t rx_at;
    uint16_t rx_fastest;

    /* modes and menus */
    uint8_t mode, mode_idx;
    uint8_t menu_idx, set_idx, setup_idx, reset_idx, score_mode;
    uint8_t help_idx, tut_page, rule_sel, rule_scroll;
    uint8_t ch_idx, ch_rule, ch_scroll;
    uint8_t det_row, det_mode, det_time, det_speed;
    int8_t test_btn;
    BbScene test_from, help_from;
    uint32_t set_flash;
    bool first_run;

    /* splash */
    uint32_t sp_start;
    uint8_t sp_phase;
    int8_t sp_flash_idx;

    /* One physical press arrives as more than one event. The latch
       remembers which keys were already acted on at InputTypePress, so
       the InputTypeShort that follows the same press is swallowed rather
       than acted on twice. */
    uint8_t press_latch;

    /* what the hardware is being asked to do */
    uint8_t led;
    uint32_t led_until;
    uint8_t led_gen; /* bumped per flash, so a repeat of one colour still shows */
    uint16_t tone_hz;
    const BbNote* tune;
    uint8_t tune_n, tune_i;
    uint32_t tune_next;
} BeepbackApp;

/* ------------------------------------------------------------------ */
/* The game (beepback_game.c) - enter(), update() and their helpers    */
/* ------------------------------------------------------------------ */

void bb_app_init(BeepbackApp* app);
void bb_enter(BeepbackApp* app, BbScene scene);
void bb_update(BeepbackApp* app);
void bb_tick(BeepbackApp* app, uint32_t dt_ms);
void bb_start_game(BeepbackApp* app);
void bb_take_press(BeepbackApp* app, uint8_t btn);
void bb_reflex_hit(BeepbackApp* app, uint8_t btn);
void bb_reflex_miss(BeepbackApp* app);
void bb_pause_shift(BeepbackApp* app, uint32_t ms);
void bb_new_base(BeepbackApp* app, uint8_t len);
void bb_set_stage(BeepbackApp* app, uint8_t n);
void bb_apply_run_rule(const BeepbackApp* app, const BbSeq* seq, BbPresses* out);
bool bb_rule_fits_run(const BeepbackApp* app);
void bb_pick_rule(BeepbackApp* app);
void bb_new_round(BeepbackApp* app);
void bb_challenge_grow(BeepbackApp* app);
void bb_start_challenge(BeepbackApp* app);
uint8_t bb_daily_rule(void);
uint32_t bb_daily_seed(void);
uint8_t bb_rand_button(BeepbackApp* app);

/* the browser's derived getters, kept as functions of the same name */
uint8_t bb_assist(const BeepbackApp* app);
bool bb_audio_on(const BeepbackApp* app);
bool bb_led_mode(const BeepbackApp* app);
bool bb_shapes_on(const BeepbackApp* app);
bool bb_arrows_on(const BeepbackApp* app);
bool bb_cue_on(const BeepbackApp* app);
bool bb_visual_on(const BeepbackApp* app);
uint16_t bb_tone_ms(const BeepbackApp* app);
uint16_t bb_gap_ms(const BeepbackApp* app);
uint32_t bb_best_for(const BeepbackApp* app, uint8_t mode, uint8_t assist);
/* the one record the finished run competes for */
uint32_t* bb_slot_cell(BeepbackApp* app);
bool bb_is_challenge_mode(uint8_t mode);

/* sound and light, requested here and applied by the app loop */
void bb_play(BeepbackApp* app, const BbNote* notes, uint8_t n);
void bb_tone(BeepbackApp* app, uint16_t hz, uint16_t ms);
void bb_led_flash(BeepbackApp* app, uint8_t color, uint32_t ms);
/* drop whatever is sounding or lit, right now */
void bb_hush(BeepbackApp* app);

/* ------------------------------------------------------------------ */
/* Input (beepback_nav.c) - the browser's press()                      */
/* ------------------------------------------------------------------ */
void bb_press(BeepbackApp* app, InputKey key);
/* one event from the device, turned into at most one bb_press() */
void bb_input_event(BeepbackApp* app, InputKey key, InputType type);
/* what a setup row is, so nothing has to compare its label as a string */
typedef enum {
    BbRowToday = 0,
    BbRowTime,
    BbRowRamp,
    BbRowSpeed,
    BbRowStart,
} BbRowKind;

uint8_t bb_setup_rows(const BeepbackApp* app, const char* label[4], const char* value[4],
                      uint8_t kind[4], char* buf_a, char* buf_b, size_t bufn);

/* ------------------------------------------------------------------ */
/* Screens (beepback_draw.c / beepback_intro.c)                        */
/* ------------------------------------------------------------------ */
void bb_draw(Canvas* canvas, BeepbackApp* app);
void bb_draw_splash(Canvas* canvas, BeepbackApp* app);
/* the splash borrows three things from the screens: a filled shape, a
   shape that can be drawn inverted, and whatever the wipe uncovers */
void bb_shape_public(Canvas* c, int32_t cx, int32_t cy, int32_t r, uint8_t kind);
void bb_shape_flash(Canvas* c, int32_t cx, int32_t cy, int32_t r, uint8_t kind, bool invert);
void bb_draw_under_wipe(Canvas* c, BeepbackApp* app);
void bb_update_splash(BeepbackApp* app);
void bb_splash_done(BeepbackApp* app);

/* ------------------------------------------------------------------ */
/* The save file (beepback_save.c)                                     */
/* ------------------------------------------------------------------ */
#define BB_SAVE_DIR  EXT_PATH("apps_data/beepback")
#define BB_SAVE_PATH BB_SAVE_DIR "/beepback.save"
#define BB_SAVE_VERSION 5
#define BB_SAVE_BYTES   723

size_t bb_save_pack(const BeepbackApp* app, uint8_t* buf, size_t n);
bool bb_save_unpack(BeepbackApp* app, const uint8_t* buf, size_t n);
void bb_save_load(BeepbackApp* app);
void bb_save_store(BeepbackApp* app);
void bb_daily_refresh(BeepbackApp* app, uint32_t today);

/* light the LED for a colour (beepback_led.c) */
void bb_led_apply(BeepbackApp* app, BbLedColor color);
