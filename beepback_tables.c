/*
 * BEEPBACK - the browser build's tables, carried over as they stand.
 *
 * Every one of these is a direct copy of a constant in beepback.html.
 * Where a name looks odd it is because the browser's name is kept: the
 * two builds are easier to keep together when a thing is called the same
 * on both sides.
 */
#include "beepback.h"

/* FREQ, NAME, SHORT, SHAPE - all in button order UP DOWN LEFT RIGHT OK */
const uint16_t bb_button_hz[BbBtnCount] = {1397, 440, 587, 1047, 784};
const char* const bb_button_name[BbBtnCount] = {"UP", "DOWN", "LEFT", "RIGHT", "OK"};
const char* const bb_button_short[BbBtnCount] = {"UP", "DN", "LF", "RT", "OK"};
const uint8_t bb_button_shape[BbBtnCount] = {
    BbShapeStar, BbShapeCircle, BbShapeTriangle, BbShapePentagon, BbShapeSquare};

/* the led ladder follows the pitch ladder, low tone = long wavelength */
const uint8_t bb_button_led[BbBtnCount] = {
    BbLedViolet, BbLedRed, BbLedYellow, BbLedBlue, BbLedGreen};
const char* const bb_led_name[BbBtnCount] = {"VIOLET", "RED", "YELLOW", "BLUE", "GREEN"};

const char* const bb_diff_name[BB_DIFF_COUNT] = {"EASY", "NORMAL", "HARD", "INSANE"};
const char* const bb_speed_name[BB_SPEED_COUNT] = {"SLOW", "NORMAL", "FAST"};
const char* const bb_mode_name[BB_MODE_COUNT] =
    {"CLASSIC", "RULES", "REFLEX", "CHALLENGE", "DAILY"};
const char* const bb_mode_blurb[BB_MODE_COUNT] = {
    "repeat the sequence",
    "one rule per round",
    "hit it before it goes",
    "one rule, no rounds",
    "same run for everyone",
};
const char* const bb_vol_name[BB_VOL_COUNT] = {"OFF", "LOW", "MID", "HIGH"};
const char* const bb_assist_name[BB_ASSIST_COUNT] = {"OFF", "LED", "SHAPES", "ARROWS"};
const char* const bb_mode_label[BB_ASSIST_COUNT] = {"EARS", "LED", "SHAPES", "ARROWS"};
const char* const bb_praise[BB_PRAISE_COUNT] =
    {"LET'S GO!", "NICE!", "PERFECT!", "GOOD!", "CLEAN!", "YOU GOT IT!", "SICK!"};

const char* const bb_rule_label[BB_RULE_COUNT] =
    {"SKIP", "DOUBLE", "NO DOUBLES", "EVERY OTHER", "LAST TWICE", "X IS Y", "BACKWARDS"};
const char* const bb_rule_tip[BB_RULE_COUNT] = {
    "never press it",
    "press it twice",
    "repeats count once",
    "1st, 3rd, 5th step",
    "final step twice",
    "press the new one",
    "last step first",
};
const char* const bb_rule_help[BB_RULE_COUNT][3] = {
    {"One button is named.", "It still plays, but you", "never press it."},
    {"One button is named.", "Press it twice every", "time it turns up."},
    {"The same button twice", "in a row counts as", "one single press."},
    {"Press the 1st step,", "the 3rd, the 5th.", "Skip the ones between."},
    {"Repeat as normal, then", "press the final step", "one extra time."},
    {"One button becomes", "another. Press the new", "one wherever it plays."},
    {"Repeat the sequence", "from the last step", "back to the first."},
};

/* SPEED_SET lives in beepback_rules.c with the other frozen tables */
/* VOL_GAIN, but calibrated for this speaker rather than copied.
   The browser's 0.05 to 0.18 is a gain into a laptop's amplifier; the
   Flipper drives a small piezo directly off the same 0..1 scale, so the
   same numbers come out barely audible. The four steps are the browser's
   four steps; what each one is worth is hardware. */
const uint8_t bb_vol_gain[BB_VOL_COUNT] = {0, 30, 65, 100};

const BbNote bb_jingle_go[3] = {{196, 60}, {0, 40}, {294, 80}};
const BbNote bb_jingle_win[3] = {{659, 70}, {880, 70}, {1175, 150}};
const BbNote bb_jingle_round[4] = {{523, 90}, {659, 90}, {880, 90}, {1175, 220}};
const BbNote bb_jingle_fail[3] = {{233, 130}, {0, 40}, {175, 260}};
const BbNote bb_jingle_over[4] = {{392, 120}, {349, 120}, {311, 120}, {262, 360}};
