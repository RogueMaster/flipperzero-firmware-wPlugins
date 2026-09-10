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
