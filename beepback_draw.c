/*
 * BEEPBACK - every screen.
 *
 * The display is 128x64 and one bit deep, and nothing may be drawn
 * outside it. The browser build shipped two bugs where an arrow was
 * placed past the bottom edge and simply disappeared, so the bands are
 * fixed here and everything else is measured against them: the title
 * bar owns y = 0..12, the footer strip y = 53..63, and body content
 * centres on y = 32 when there is a footer and y = 38 when there is not.
 *
 * Arrows are drawn only where bb_can_adjust() or bb_list_move() says the
 * player could actually go, so no list ever offers a direction that
 * turns out to be a wall.
 */
#include "beepback.h"
#include "beepback_tables.h"
#include <stdio.h>

const char* const bb_mode_name[BB_MODE_COUNT] =
    {"CLASSIC", "RULES", "REFLEX", "CHALLENGE", "DAILY"};
const char* const bb_rule_name[BB_RULE_COUNT] =
    {"SKIP", "DOUBLE", "NO DOUBLES", "EVERY OTHER", "LAST TWICE", "X IS Y", "BACKWARDS"};
const char* const bb_assist_name[BB_ASSIST_COUNT] = {"EARS", "LED", "SHAPES", "ARROWS"};
const char* const bb_time_name[BB_DIFF_COUNT] = {"EASY", "NORMAL", "HARD", "INSANE"};
const char* const bb_speed_name[BB_SPEED_COUNT] = {"SLOW", "NORMAL", "FAST"};
const char* const bb_praise[BB_PRAISE_COUNT] =
    {"NICE", "SHARP", "CLEAN", "GOOD EAR", "LOCKED IN", "SMOOTH", "DIALLED"};

/* Mode select is the only place the game says what the modes are, so the
   footer carries a line about whichever one the cursor is on. */
const char* const bb_mode_blurb[BB_MODE_COUNT] = {
    "REPEAT WHAT YOU HEAR",
    "ONE RULE PER ROUND",
    "ONE CUE, NO MEMORY",
    "ONE RULE, NO ROUNDS",
    "SAME RUN FOR EVERYONE",
};

const char* const bb_volume_name[BB_VOL_COUNT] = {"OFF", "LOW", "MID", "HIGH", "MAX"};

/* Screens read cursors, and a cursor is only ever as trustworthy as the
   code that moved it. Clamping here costs nothing and means a screen can
   never walk off the end of a name table. */
static uint8_t bb_clamp(uint8_t v, uint8_t count) {
    return v < count ? v : (uint8_t)(count - 1);
}

/* Scores share a 32px column four ways, so past five digits they go to
   thousands rather than run into the neighbour. */
static const char* bb_score_short(uint32_t v, char* out, size_t n) {
    if(v < 100000u) {
        snprintf(out, n, "%lu", (unsigned long)v);
        return out;
    }
    if(v < 100000000u) {
        snprintf(out, n, "%luk", (unsigned long)(v / 1000u));
        return out;
    }
    return "99999k"; /* nobody has scored this, but the column still fits */
}

/* Four of the seven rules say the same thing whatever buttons they were
   given, so they are handed back as they are rather than copied into a
   buffer. That is not only cheaper: at -Os the compiler turns a
   parameterless snprintf into a strcpy, which a .fap can only call if
   the firmware happens to export it, and this app now asks the firmware
   for nothing it does not obviously need. */
static const char* const bb_rule_flat[BB_RULE_COUNT] = {
    NULL, /* SKIP names a button      */
    NULL, /* DOUBLE names a button    */
    "A REPEAT IS ONE PRESS",
    "1ST, 3RD, 5TH ONLY",
    "PRESS THE LAST TWICE",
    NULL, /* X IS Y names two buttons */
    "LAST STEP FIRST",
};

const char* bb_rule_line(BbRule rule, uint8_t a, uint8_t b, char* out, size_t n) {
    const char* na = bb_button_name[a < BbBtnCount ? a : 0];
    const char* nb = bb_button_name[b < BbBtnCount ? b : 0];
    switch(rule) {
    case BbRuleSkip:
        snprintf(out, n, "NEVER PRESS %s", na);
        return out;
    case BbRuleDouble:
        snprintf(out, n, "PRESS %s TWICE", na);
        return out;
    case BbRuleSwap:
        snprintf(out, n, "%s MEANS %s", na, nb);
        return out;
    default:
        break;
    }
    const char* flat = bb_rule_flat[rule < BB_RULE_COUNT ? rule : BB_RULE_COUNT - 1];
    return flat ? flat : "";
}

void bb_mult_str(uint16_t mult, char* out, size_t n) {
    /* two decimals, not one: at one decimal x1.55 and x1.45 both render
       as "x1.5" and two different settings look identical */
    snprintf(out, n, "X%u.%02u", mult / 100u, mult % 100u);
}

/* ------------------------------------------------------------------ */
/* Bands and rows                                                      */
/* ------------------------------------------------------------------ */

static void bb_title(Canvas* c, const char* text) {
    canvas_set_color(c, ColorBlack);
    canvas_draw_box(c, 0, 0, BB_W, BB_TITLE_H);
    canvas_set_color(c, ColorWhite);
    canvas_set_font(c, FontPrimary);
    canvas_draw_str_aligned(c, BB_W / 2, BB_TITLE_Y, AlignCenter, AlignCenter, text);
    canvas_set_color(c, ColorBlack);
}

static void bb_footer(Canvas* c, const char* text) {
    canvas_set_color(c, ColorBlack);
    canvas_draw_box(c, 0, BB_FOOT_Y0, BB_W, BB_H - BB_FOOT_Y0);
    canvas_set_color(c, ColorWhite);
    canvas_set_font(c, FontSecondary);
    canvas_draw_str_aligned(c, BB_W / 2, BB_FOOT_Y, AlignCenter, AlignCenter, text);
    canvas_set_color(c, ColorBlack);
}

/* Three rows fit between the bands. Keep the cursor in the middle of the
   window where the list is long enough to allow it. */
#define BB_ROWS 3
static const uint8_t bb_row_y[BB_ROWS] = {BB_BODY_Y - 13, BB_BODY_Y, BB_BODY_Y + 13};

static uint8_t bb_window_first(uint8_t cur, uint8_t count, uint8_t rows) {
    if(count <= rows) return 0;
    int16_t first = (int16_t)cur - rows / 2;
    if(first < 0) first = 0;
    if(first > (int16_t)(count - rows)) first = (int16_t)(count - rows);
    return (uint8_t)first;
}

/* The selected row is inverted, which is the Flipper's own menu idiom and
   far easier to find at a glance than a bullet in the margin. */
static void bb_select(Canvas* c, uint8_t y) {
    canvas_set_color(c, ColorBlack);
    canvas_draw_rbox(c, 2, y - 5, 124, 10, 2);
    canvas_set_color(c, ColorWhite);
}

/* A plain label and value row: label at ROW_L, value ending at ROW_R. */
static void bb_row(Canvas* c, uint8_t y, const char* label, const char* value, bool sel) {
    canvas_set_font(c, FontSecondary);
    if(sel) bb_select(c, y);
    canvas_draw_str_aligned(c, BB_ROW_L, y, AlignLeft, AlignCenter, label);
    if(value) canvas_draw_str_aligned(c, BB_ROW_R, y, AlignRight, AlignCenter, value);
    if(sel) canvas_set_color(c, ColorBlack);
}

/* An action rather than a setting: same inverted row, bigger type. */
static void bb_big_row(Canvas* c, uint8_t y, const char* label, bool sel) {
    if(sel) bb_select(c, y);
    canvas_set_font(c, FontPrimary);
    canvas_draw_str_aligned(c, BB_W / 2, y, AlignCenter, AlignCenter, label);
    canvas_set_color(c, ColorBlack);
    canvas_set_font(c, FontSecondary);
}

/* Arrows sit at the row edges and appear only where a press would move
   something, so the arrow itself says whether there is anything left
   that way. */
static void bb_left_arrow(Canvas* c, uint8_t y) {
    canvas_set_font(c, FontSecondary);
    canvas_draw_str_aligned(c, BB_ROW_AL, y, AlignLeft, AlignCenter, "<");
}

static void bb_right_arrow(Canvas* c, uint8_t y) {
    canvas_set_font(c, FontSecondary);
    canvas_draw_str_aligned(c, BB_ROW_AR, y, AlignLeft, AlignCenter, ">");
}

static void bb_adj_row(
    Canvas* c,
    uint8_t y,
    const char* label,
    const char* value,
    bool sel,
    bool left,
    bool right) {
    canvas_set_font(c, FontSecondary);
    if(sel) bb_select(c, y);
    canvas_draw_str_aligned(c, BB_ROW_L, y, AlignLeft, AlignCenter, label);
    canvas_draw_str_aligned(c, BB_ROW_R, y, AlignRight, AlignCenter, value);
    if(sel) canvas_set_color(c, ColorBlack);
    if(sel && left) bb_left_arrow(c, y);
    if(sel && right) bb_right_arrow(c, y);
}

/* ------------------------------------------------------------------ */
/* Pieces                                                              */
/* ------------------------------------------------------------------ */

/* Step dots are 5x5 pixel art. A filled one is a diamond and an empty
   one an outlined square: drawn with arcs in the browser they came out
   as different shapes on light and dark ink. */
static void bb_dot(Canvas* c, int32_t x, int32_t y, bool filled) {
    if(filled) {
        canvas_draw_dot(c, x + 2, y);
        canvas_draw_line(c, x + 1, y + 1, x + 3, y + 1);
        canvas_draw_line(c, x, y + 2, x + 4, y + 2);
        canvas_draw_line(c, x + 1, y + 3, x + 3, y + 3);
        canvas_draw_dot(c, x + 2, y + 4);
    } else {
        canvas_draw_frame(c, x, y, 5, 5);
    }
}

static void bb_dots(Canvas* c, uint8_t done, uint8_t total, uint8_t y) {
    char buf[16];
    canvas_set_font(c, FontSecondary);
    if(total == 0) return;
    if(total > BB_DOT_MAX) {
        /* the total is already in the HUD, so this is just a count */
        snprintf(buf, sizeof(buf), "%u", done);
        canvas_draw_str_aligned(c, BB_W / 2, y + 2, AlignCenter, AlignCenter, buf);
        return;
    }
    int32_t w = total * 7 - 2;
    int32_t x = (BB_W - w) / 2;
    for(uint8_t i = 0; i < total; i++) bb_dot(c, x + i * 7, y, i < done);
}

static void bb_heart(Canvas* c, int32_t x, int32_t y) {
    canvas_draw_line(c, x + 1, y, x + 2, y);
    canvas_draw_line(c, x + 4, y, x + 5, y);
    canvas_draw_line(c, x, y + 1, x + 6, y + 1);
    canvas_draw_line(c, x, y + 2, x + 6, y + 2);
    canvas_draw_line(c, x + 1, y + 3, x + 5, y + 3);
    canvas_draw_line(c, x + 2, y + 4, x + 4, y + 4);
    canvas_draw_dot(c, x + 3, y + BB_HEART_H - 1);
}

/* Filled polygons by scanline, so the star comes out solid without any
   trigonometry at draw time; the outlines are pre-generated. */
static void bb_poly(
    Canvas* c,
    int32_t cx,
    int32_t cy,
    const float* xs,
    const float* ys,
    uint8_t n,
    int32_t r,
    bool fill) {
    int32_t px[10], py[10];
    for(uint8_t i = 0; i < n; i++) {
        px[i] = cx + (int32_t)(xs[i] * (float)r);
        py[i] = cy + (int32_t)(ys[i] * (float)r);
    }
    if(!fill) {
        for(uint8_t i = 0; i < n; i++) {
            uint8_t j = (uint8_t)((i + 1) % n);
            canvas_draw_line(c, px[i], py[i], px[j], py[j]);
        }
        return;
    }
    int32_t lo = py[0], hi = py[0];
    for(uint8_t i = 1; i < n; i++) {
        if(py[i] < lo) lo = py[i];
        if(py[i] > hi) hi = py[i];
    }
    for(int32_t y = lo; y <= hi; y++) {
        int32_t xs_at[10];
        uint8_t hits = 0;
        for(uint8_t i = 0; i < n && hits < 10; i++) {
            uint8_t j = (uint8_t)((i + 1) % n);
            int32_t y0 = py[i], y1 = py[j];
            if((y0 <= y && y1 > y) || (y1 <= y && y0 > y)) {
                int32_t x0 = px[i], x1 = px[j];
                xs_at[hits++] = x0 + (x1 - x0) * (y - y0) / (y1 - y0);
            }
        }
        for(uint8_t a = 1; a < hits; a++) { /* a handful of points, so insertion */
            int32_t v = xs_at[a];
            int8_t b = (int8_t)(a - 1);
            while(b >= 0 && xs_at[b] > v) {
                xs_at[b + 1] = xs_at[b];
                b--;
            }
            xs_at[b + 1] = v;
        }
        for(uint8_t a = 0; a + 1 < hits; a += 2)
            canvas_draw_line(c, xs_at[a], y, xs_at[a + 1], y);
    }
}

static void bb_shape(Canvas* c, int32_t cx, int32_t cy, uint8_t shape, int32_t r, bool fill) {
    switch(shape) {
    case BbShapeCircle:
        if(fill) {
            canvas_draw_disc(c, cx, cy, (size_t)r);
        } else {
            canvas_draw_circle(c, cx, cy, (size_t)r);
        }
        break;
    case BbShapeTriangle:
        bb_poly(c, cx, cy, BB_TRI_X, BB_TRI_Y, 3, r, fill);
        break;
    case BbShapeSquare:
        bb_poly(c, cx, cy, BB_SQR_X, BB_SQR_Y, 4, r, fill);
        break;
    case BbShapePentagon:
        bb_poly(c, cx, cy, BB_PENT_X, BB_PENT_Y, 5, r, fill);
        break;
    default:
        bb_poly(c, cx, cy, BB_STAR_X, BB_STAR_Y, 10, r, fill);
        break;
    }
}

static void bb_arrow(Canvas* c, int32_t cx, int32_t cy, uint8_t btn) {
    const int32_t s = 14, t = 4;
    switch(btn) {
    case BbBtnUp:
        canvas_draw_line(c, cx, cy - s, cx - s, cy);
        canvas_draw_line(c, cx, cy - s, cx + s, cy);
        canvas_draw_line(c, cx - s, cy, cx + s, cy);
        canvas_draw_box(c, cx - t, cy, (size_t)(t * 2), (size_t)s);
        break;
    case BbBtnDown:
        canvas_draw_line(c, cx, cy + s, cx - s, cy);
        canvas_draw_line(c, cx, cy + s, cx + s, cy);
        canvas_draw_line(c, cx - s, cy, cx + s, cy);
        canvas_draw_box(c, cx - t, cy - s, (size_t)(t * 2), (size_t)s);
        break;
    case BbBtnLeft:
        canvas_draw_line(c, cx - s, cy, cx, cy - s);
        canvas_draw_line(c, cx - s, cy, cx, cy + s);
        canvas_draw_line(c, cx, cy - s, cx, cy + s);
        canvas_draw_box(c, cx, cy - t, (size_t)s, (size_t)(t * 2));
        break;
    case BbBtnRight:
        canvas_draw_line(c, cx + s, cy, cx, cy - s);
        canvas_draw_line(c, cx + s, cy, cx, cy + s);
        canvas_draw_line(c, cx, cy - s, cx, cy + s);
        canvas_draw_box(c, cx - s, cy - t, (size_t)s, (size_t)(t * 2));
        break;
    default:
        canvas_draw_disc(c, cx, cy, 11);
        break;
    }
}

/* Is this playback step still in its first BB_FLASH_MS? A cue drawn
   inverted for that long lands rather than fades in, and it is what
   makes the same button twice running read as two hits and not one long
   one. */
static bool bb_cue_popping(const BeepbackApp* app) {
    const BbRun* run = &app->run;
    uint16_t tone = bb_speed_tone[run->speed < BB_SPEED_COUNT ? run->speed : 1];
    if(tone <= BB_FLASH_MS) return false;
    uint32_t left = run->phase_end > app->now ? run->phase_end - app->now : 0;
    return left > (uint32_t)(tone - BB_FLASH_MS);
}

/* What the player is shown for a button, per assist. EARS and LED both
   leave the screen alone: the point of them is that it is not there, so
   the pop-in has nothing to apply to either. */
#define BB_CUE_HALF 21 /* covers the tallest shape at BB_SHAPE_R */

static void bb_cue(Canvas* c, const BbRun* run, uint8_t btn, int32_t cy, bool pop) {
    if(btn >= BbBtnCount) return;
    if(run->assist != BbAssistShapes && run->assist != BbAssistArrows) return;
    if(pop) {
        canvas_set_color(c, ColorBlack);
        canvas_draw_box(
            c, BB_W / 2 - BB_CUE_HALF, cy - BB_CUE_HALF, BB_CUE_HALF * 2, BB_CUE_HALF * 2);
        canvas_set_color(c, ColorWhite);
    }
    if(run->assist == BbAssistShapes) {
        bb_shape(c, BB_W / 2, cy, bb_button_shape[btn], BB_SHAPE_R, true);
    } else {
        bb_arrow(c, BB_W / 2, cy, btn);
    }
    if(pop) canvas_set_color(c, ColorBlack);
}

static void bb_banner(Canvas* c, const char* text, int32_t y) {
    canvas_set_font(c, FontPrimary);
    canvas_draw_str_aligned(c, BB_W / 2, y, AlignCenter, AlignCenter, text);
    canvas_set_font(c, FontSecondary);
}

/* A drain bar for the response window, kept clear of the bottom edge. */
static void bb_bar(Canvas* c, uint32_t left, uint32_t total, int32_t y) {
    canvas_draw_frame(c, 4, y, BB_W - 8, 7);
    if(total == 0) return;
    if(left > total) left = total;
    uint32_t w = (uint32_t)(BB_W - 12) * left / total;
    if(w) canvas_draw_box(c, 6, y + 2, (size_t)w, 3);
}

/* ------------------------------------------------------------------ */
/* Pages                                                               */
/* ------------------------------------------------------------------ */

typedef struct {
    const char* line[3];
} BbPage;

static const BbPage bb_tut_page[BB_TUT_PAGES] = {
    {{"BEEPBACK", "REPEAT WHAT", "YOU JUST HEARD"}},
    {{"FIVE BUTTONS", "EACH ONE HAS", "ITS OWN TONE"}},
    {{"THREE LIVES", "A MISTAKE REPLAYS", "THE SAME ROUND"}},
    {{"NEED A HAND?", "SETTINGS HAS LED,", "SHAPES AND ARROWS"}},
};

static const BbPage bb_howto_classic[4] = {
    {{"CLASSIC", "LISTEN, THEN", "PRESS IT BACK"}},
    {{"ONE STEP LONGER", "EVERY TIME YOU", "GET IT RIGHT"}},
    {{"CLEAR THE ROUND", "FOR A BONUS AND", "A LONGER ONE"}},
    {{"TIME SETS YOUR", "WINDOW, SPEED SETS", "THE PLAYBACK"}},
};

static const BbPage bb_howto_rules[4] = {
    {{"RULES", "SAME LADDER, PLUS", "ONE RULE A ROUND"}},
    {{"THE RULE CHANGES", "WHAT YOU PRESS,", "NOT WHAT PLAYS"}},
    {{"SKIP A BUTTON,", "DOUBLE IT, SWAP IT", "OR GO BACKWARDS"}},
    {{"YOU GET AN EXTRA", "SECOND TO WORK", "THE RULE OUT"}},
};

static const BbPage bb_howto_reflex[3] = {
    {{"REFLEX", "NO MEMORY HERE.", "ONE CUE AT A TIME"}},
    {{"HIT IT BEFORE", "THE BAR EMPTIES.", "IT GETS TIGHTER"}},
    {{"ONE MISS ENDS IT,", "EARLY PRESSES", "INCLUDED"}},
};

static const BbPage* bb_howto_pageset(uint8_t topic) {
    if(topic == 1) return bb_howto_rules;
    if(topic == 2) return bb_howto_reflex;
    return bb_howto_classic;
}

static void bb_draw_page(Canvas* c, const BbPage* page, uint8_t at, uint8_t count) {
    char buf[12];
    static const uint8_t y[3] = {BB_BODY_Y - 12, BB_BODY_Y, BB_BODY_Y + 12};
    canvas_set_font(c, FontSecondary);
    for(uint8_t i = 0; i < 3; i++)
        if(page->line[i])
            canvas_draw_str_aligned(c, BB_W / 2, y[i], AlignCenter, AlignCenter, page->line[i]);
    /* arrows only where there is another page to reach */
    if(at > 0) bb_left_arrow(c, BB_BODY_Y);
    if(at + 1 < count) bb_right_arrow(c, BB_BODY_Y);
    snprintf(buf, sizeof(buf), "%u/%u", at + 1, count);
    bb_footer(c, buf);
}

/* ------------------------------------------------------------------ */
/* Four assist bests, side by side                                     */
/* ------------------------------------------------------------------ */

static const char* const bb_assist_short[BB_ASSIST_COUNT] = {"EAR", "LED", "SHP", "ARR"};

/* The scores detail screen carries three dials above its bests, which
   leaves one row's worth of height for four numbers, so there they stay
   in columns. */
static void bb_assist_columns(Canvas* c, const uint32_t* best, uint8_t label_y, uint8_t value_y) {
    char buf[12];
    canvas_set_font(c, FontSecondary);
    for(uint8_t a = 0; a < BB_ASSIST_COUNT; a++) {
        int32_t cx = 16 + a * 32;
        canvas_draw_str_aligned(c, cx, label_y, AlignCenter, AlignCenter, bb_assist_short[a]);
        canvas_draw_str_aligned(
            c, cx, value_y, AlignCenter, AlignCenter, bb_score_short(best[a], buf, sizeof(buf)));
    }
}

/* The board has the screen to itself, so four labelled rows: label left,
   value right, the same shape as every other list, and the numbers land
   in a column you can read down. */
static void bb_assist_block(Canvas* c, const uint32_t* best, uint8_t first_y, uint8_t step) {
    char buf[12];
    canvas_set_font(c, FontSecondary);
    for(uint8_t a = 0; a < BB_ASSIST_COUNT; a++) {
        uint8_t y = (uint8_t)(first_y + a * step);
        canvas_draw_str_aligned(c, BB_ROW_L, y, AlignLeft, AlignCenter, bb_assist_name[a]);
        canvas_draw_str_aligned(
            c, BB_ROW_R, y, AlignRight, AlignCenter, bb_score_short(best[a], buf, sizeof(buf)));
    }
}

/* ------------------------------------------------------------------ */
/* The screens                                                         */
/* ------------------------------------------------------------------ */

static void bb_draw_launcher(Canvas* c) {
    canvas_set_font(c, FontPrimary);
    /* no footer here, so the pair centres on the taller body line */
    canvas_draw_str_aligned(c, BB_W / 2, BB_BODY_Y2 - 8, AlignCenter, AlignCenter, "BEEPBACK");
    canvas_set_font(c, FontSecondary);
    canvas_draw_str_aligned(
        c, BB_W / 2, BB_BODY_Y2 + 8, AlignCenter, AlignCenter, "PRESS A BUTTON TO PLAY");
}

static void bb_draw_menu(Canvas* c, const BeepbackApp* app) {
    static const char* const item[3] = {"PLAY", "HOW TO PLAY", "SETTINGS"};
    bb_title(c, "BEEPBACK");
    for(uint8_t i = 0; i < 3; i++) bb_row(c, bb_row_y[i], item[i], NULL, app->menu_cur == i);
    /* the boards are one press to the right, and only that way */
    bb_right_arrow(c, BB_BODY_Y);
    bb_footer(c, "BOARDS >");
}

static void bb_draw_modeselect(Canvas* c, const BeepbackApp* app) {
    uint8_t rows = bb_list_count(app, BbSceneModeSelect);
    static const uint8_t slot_y[2] = {27, 43};
    bb_title(c, "MODE");
    for(uint8_t i = 0; i < rows; i++) {
        uint8_t mode = bb_clamp((uint8_t)(app->mode_page * 2 + i), BB_MODE_COUNT);
        uint8_t y = slot_y[i];
        if(app->mode_row == i) {
            canvas_set_color(c, ColorBlack);
            canvas_draw_rbox(c, 12, (uint8_t)(y - 7), 104, 14, 3);
            canvas_set_color(c, ColorWhite);
        }
        canvas_set_font(c, FontPrimary);
        canvas_draw_str_aligned(c, BB_W / 2, y, AlignCenter, AlignCenter, bb_mode_name[mode]);
        canvas_set_color(c, ColorBlack);
    }
    if(app->mode_page > 0) bb_left_arrow(c, BB_BODY_Y);
    if(app->mode_page + 1 < BB_MODE_PAGES) bb_right_arrow(c, BB_BODY_Y);
    /* the only place the game explains what the modes are */
    uint8_t sel = bb_clamp((uint8_t)(app->mode_page * 2 + app->mode_row), BB_MODE_COUNT);
    bb_footer(c, bb_mode_blurb[sel]);
}

static void bb_draw_rulepick(Canvas* c, const BeepbackApp* app) {
    uint8_t count = BB_RULE_COUNT + 1;
    uint8_t first = bb_window_first(app->rule_cur, count, BB_ROWS);
    bb_title(c, "CHALLENGE RULE");
    for(uint8_t i = 0; i < BB_ROWS; i++) {
        uint8_t at = (uint8_t)(first + i);
        const char* name = (at >= BB_RULE_COUNT) ? "RANDOM" : bb_rule_name[at];
        bb_row(c, bb_row_y[i], name, NULL, app->rule_cur == at);
    }
    char buf[16];
    snprintf(buf, sizeof(buf), "%u/%u", app->rule_cur + 1, count);
    bb_footer(c, buf);
}

static void bb_draw_setup(Canvas* c, const BeepbackApp* app) {
    char buf[24];
    bool daily = (app->run.mode == BbModeDaily);
    bb_title(c, bb_mode_name[bb_clamp(app->run.mode, BB_MODE_COUNT)]);

    if(daily) {
        /* plain rows: the day picks these, so there is nothing to move */
        char date[16];
        snprintf(date, sizeof(date), "%lu", (unsigned long)bb_today_seed());
        bb_row(c, 18, "TODAY", date, false);
        bb_row(c, 28, "TIME", bb_time_name[1], false);
        bb_row(c, 38, "SPEED", bb_speed_name[1], false);
        bool ready = bb_can_start(app, BbModeDaily);
        bb_row(c, 48, ready ? "START" : "PLAYED TODAY", NULL, ready);
    } else {
        char time_v[20];
        uint8_t diff = bb_clamp(app->set.diff, BB_DIFF_COUNT);
        /* the window in seconds, because "HARD" on its own says nothing */
        snprintf(time_v, sizeof(time_v), "%s %uS", bb_time_name[diff], bb_time_ms[diff] / 1000u);
        bb_adj_row(c, 23, "TIME", time_v, app->setup_cur == 0, bb_can_adjust(app, -1),
                   bb_can_adjust(app, 1));
        bb_adj_row(c, 33, "SPEED", bb_speed_name[bb_clamp(app->set.speed, BB_SPEED_COUNT)],
                   app->setup_cur == 1, bb_can_adjust(app, -1), bb_can_adjust(app, 1));
        bb_big_row(c, 43, "START", app->setup_cur == 2);
    }

    /* the multiplier moves as the dials do, so you can see what it costs */
    uint8_t d = daily ? 1 : bb_clamp(app->set.diff, BB_DIFF_COUNT);
    uint8_t s = daily ? 1 : bb_clamp(app->set.speed, BB_SPEED_COUNT);
    char mult[12];
    bb_mult_str(bb_multiplier(app->run.mode, d, s), mult, sizeof(mult));
    snprintf(buf, sizeof(buf), "SCORE  %s", mult);
    bb_footer(c, buf);
}

static void bb_draw_hud(Canvas* c, const BeepbackApp* app) {
    const BbRun* run = &app->run;
    char buf[24];
    canvas_set_font(c, FontSecondary);
    for(uint8_t i = 0; i < run->lives && i < BB_LIVES; i++)
        bb_heart(c, 1 + i * (BB_HEART_W + 1), 1);
    if(run->mode == BbModeReflex) {
        snprintf(buf, sizeof(buf), "x%lu", (unsigned long)run->hits);
    } else if(bb_is_challenge(run->mode)) {
        /* challenge has no target, so it must not print one */
        snprintf(buf, sizeof(buf), "LEN %u", run->shown);
    } else {
        snprintf(buf, sizeof(buf), "R%u %u/%u", run->round, run->shown, run->target);
    }
    canvas_draw_str_aligned(c, 30, 5, AlignLeft, AlignCenter, buf);
    snprintf(buf, sizeof(buf), "%lu", (unsigned long)run->score);
    canvas_draw_str_aligned(c, BB_W - 1, 5, AlignRight, AlignCenter, buf);
}

static void bb_draw_game(Canvas* c, const BeepbackApp* app) {
    const BbRun* run = &app->run;
    char buf[28];
    bb_draw_hud(c, app);
    canvas_set_font(c, FontSecondary);

    switch(run->phase) {
    case BbPhaseRuleCard:
        bb_banner(c, bb_rule_name[bb_clamp(run->rule, BB_RULE_COUNT)], 24);
        canvas_draw_str_aligned(
            c, BB_W / 2, 40, AlignCenter, AlignCenter,
            bb_rule_line(run->rule, run->ra, run->rb, buf, sizeof(buf)));
        break;

    case BbPhaseListen:
        bb_banner(c, "LISTEN", 32);
        break;

    case BbPhasePlayback:
        if(run->play_on)
            bb_cue(c, run, run->seq.step[run->play_i], BB_BODY_Y, bb_cue_popping(app));
        bb_dots(c, (uint8_t)(run->play_i + (run->play_on ? 1 : 0)), run->shown, 57);
        break;

    case BbPhaseGo:
        bb_banner(c, "GO", 32);
        break;

    case BbPhaseInput:
    case BbPhaseHold:
        if(run->flash_end && run->assist == BbAssistShapes) {
            bb_shape(c, BB_W / 2, 28, bb_button_shape[run->flash_btn], 12, true);
        } else if(run->flash_end && run->assist == BbAssistArrows) {
            bb_arrow(c, BB_W / 2, 28, run->flash_btn);
        }
        bb_dots(c, run->idx, run->press.len, 46);
        bb_bar(c, run->win_end > app->now ? run->win_end - app->now : 0, run->win_ms, 56);
        break;

    case BbPhaseSuccess:
        bb_banner(c, bb_praise[run->shown % BB_PRAISE_COUNT], 26);
        snprintf(buf, sizeof(buf), "+%lu", (unsigned long)run->award);
        canvas_draw_str_aligned(c, BB_W / 2, 42, AlignCenter, AlignCenter, buf);
        break;

    case BbPhaseRound:
        snprintf(buf, sizeof(buf), "ROUND %u", run->round);
        bb_banner(c, buf, 24);
        snprintf(buf, sizeof(buf), "+%lu AND +%lu", (unsigned long)run->award, (unsigned long)run->bonus);
        canvas_draw_str_aligned(c, BB_W / 2, 40, AlignCenter, AlignCenter, buf);
        break;

    case BbPhaseWrong:
        bb_banner(c, "MISS", 28);
        snprintf(buf, sizeof(buf), run->lives == 1 ? "%u LIFE LEFT" : "%u LIVES LEFT", run->lives);
        canvas_draw_str_aligned(c, BB_W / 2, 42, AlignCenter, AlignCenter, buf);
        break;

    case BbPhaseRetry:
        bb_banner(c, "AGAIN", 32);
        break;

    case BbPhaseRxReady:
        bb_banner(c, "REFLEX", 22);
        canvas_draw_str_aligned(c, BB_W / 2, 36, AlignCenter, AlignCenter, "ONE MISS ENDS IT");
        /* there is no pause here, and the player should know before it starts */
        canvas_draw_str_aligned(c, BB_W / 2, 46, AlignCenter, AlignCenter, "BACK ENDS THE RUN");
        break;

    case BbPhaseRxWait:
        canvas_draw_str_aligned(c, BB_W / 2, 32, AlignCenter, AlignCenter, "WAIT");
        break;

    case BbPhaseRxCue:
        bb_cue(c, run, run->rx_cue, 30, false);
        if(run->assist == BbAssistOff || run->assist == BbAssistLed)
            bb_banner(c, "NOW", 30);
        bb_bar(c, run->win_end > app->now ? run->win_end - app->now : 0, run->win_ms, 56);
        break;

    default:
        break;
    }
}

/* One button in a chip, the way the rule card names it. */
static int32_t bb_chip_w(Canvas* c, const char* name) {
    canvas_set_font(c, FontSecondary);
    return canvas_string_width(c, name) + 8;
}

static void bb_chip(Canvas* c, int32_t x, uint8_t y, const char* name, int32_t w) {
    canvas_set_color(c, ColorWhite);
    canvas_draw_rbox(c, x, (int32_t)y - 6, (size_t)w, 11, 2);
    canvas_set_color(c, ColorBlack);
    canvas_set_font(c, FontSecondary);
    canvas_draw_str_aligned(c, x + w / 2, y, AlignCenter, AlignCenter, name);
    canvas_set_color(c, ColorWhite);
}

/* Pausing is exactly when you have forgotten what you are obeying, so
   the pause screen carries the rule, the round, the stage and the lives.
   It is inverted, which is what makes it read as a stopped game rather
   than another menu. */
static void bb_draw_pause(Canvas* c, const BeepbackApp* app) {
    const BbRun* run = &app->run;
    char left[24], mid[20];

    canvas_set_color(c, ColorBlack);
    canvas_draw_box(c, 0, 0, BB_W, BB_H);
    canvas_set_color(c, ColorWhite);

    canvas_set_font(c, FontPrimary);
    canvas_draw_str_aligned(c, BB_W / 2, 9, AlignCenter, AlignCenter, "PAUSED");
    canvas_draw_box(c, 8, 17, 112, 1);

    canvas_set_font(c, FontSecondary);
    if(bb_is_challenge(run->mode)) {
        snprintf(left, sizeof(left), "%s", bb_mode_name[bb_clamp(run->mode, BB_MODE_COUNT)]);
        snprintf(mid, sizeof(mid), "LEN %u", run->shown);
    } else {
        snprintf(left, sizeof(left), "ROUND %u", run->round);
        snprintf(mid, sizeof(mid), "%u/%u", run->shown, run->target);
    }
    canvas_draw_str_aligned(c, 8, 26, AlignLeft, AlignCenter, left);
    canvas_draw_str_aligned(c, BB_W / 2, 26, AlignCenter, AlignCenter, mid);
    for(uint8_t i = 0; i < run->lives && i < BB_LIVES; i++)
        bb_heart(c, 96 + i * 8, 22);

    /* the rule, with the buttons it names set in chips */
    if(bb_run_has_rule(run->mode)) {
        const char* name = bb_rule_name[bb_clamp(run->rule, BB_RULE_COUNT)];
        const char* one = NULL;
        const char* two = NULL;
        if(run->rule == BbRuleSkip || run->rule == BbRuleDouble) {
            one = bb_button_name[bb_clamp(run->ra, BbBtnCount)];
        } else if(run->rule == BbRuleSwap) {
            one = bb_button_name[bb_clamp(run->ra, BbBtnCount)];
            two = bb_button_name[bb_clamp(run->rb, BbBtnCount)];
        }
        canvas_set_font(c, FontSecondary);
        int32_t nw = canvas_string_width(c, name);
        int32_t w1 = one ? bb_chip_w(c, one) : 0;
        int32_t w2 = two ? bb_chip_w(c, two) : 0;
        int32_t gap = 2;
        int32_t total = nw + (one ? gap + w1 : 0) + (two ? gap + w2 : 0);
        int32_t x = (BB_W - total) / 2;
        if(x < 2) x = 2;
        canvas_draw_str_aligned(c, x + nw / 2, 39, AlignCenter, AlignCenter, name);
        x += nw;
        if(one) {
            x += gap;
            bb_chip(c, x, 39, one, w1);
            x += w1;
        }
        if(two) {
            x += gap;
            bb_chip(c, x, 39, two, w2);
        }
    } else {
        canvas_set_font(c, FontSecondary);
        canvas_draw_str_aligned(
            c, BB_W / 2, 39, AlignCenter, AlignCenter,
            bb_mode_name[bb_clamp(run->mode, BB_MODE_COUNT)]);
    }

    canvas_set_font(c, FontSecondary);
    canvas_draw_str_aligned(c, 6, 55, AlignLeft, AlignCenter, "OK: RESUME");
    canvas_draw_str_aligned(c, 122, 55, AlignRight, AlignCenter, "BACK: QUIT");
    canvas_set_color(c, ColorBlack);
}


/* Three labelled rows, and a second page behind the chevron holding the
   settings the run was played on - because a record only means something
   later if you can see what it was set to. The multiplier is printed
   before the number, which is how it was asked for. */
static void bb_draw_over(Canvas* c, const BeepbackApp* app) {
    const BbRun* run = &app->run;
    char buf[40], mult[12], num[16];

    bb_title(c, run->record ? "NEW BEST!" : "GAME OVER");
    bb_mult_str(run->mult, mult, sizeof(mult));

    if(app->over_page == 0) {
        snprintf(buf, sizeof(buf), "%s  %lu", mult, (unsigned long)run->score);
        bb_row(c, 21, "SCORE", buf, false);

        if(run->mode == BbModeReflex) {
            snprintf(num, sizeof(num), "%lu", (unsigned long)run->hits);
            bb_row(c, 32, "HITS", num, false);
        } else {
            snprintf(num, sizeof(num), "%u", run->longest);
            bb_row(c, 32, "LONGEST", num, false);
        }

        bb_row(c, 43, "PREVIOUS BEST", bb_score_short(run->prev_best, num, sizeof(num)), false);
        bb_right_arrow(c, BB_BODY_Y);
    } else {
        bb_row(c, 21, "MODE", bb_mode_name[bb_clamp(run->mode, BB_MODE_COUNT)], false);
        bb_row(c, 32, "ASSIST", bb_assist_name[bb_clamp(run->assist, BB_ASSIST_COUNT)], false);
        if(run->mode == BbModeChallenge) {
            bb_row(c, 43, "RULE", bb_rule_name[bb_clamp(run->rule, BB_RULE_COUNT)], false);
        } else {
            bb_row(c, 43, "TIME", bb_time_name[bb_clamp(run->diff, BB_DIFF_COUNT)], false);
        }
        bb_row(c, 54, "SPEED", bb_speed_name[bb_clamp(run->speed, BB_SPEED_COUNT)], false);
        bb_left_arrow(c, BB_BODY_Y);
    }

    if(app->over_page == 0)
        bb_footer(
            c, bb_can_start(app, run->mode) ? "OK: AGAIN    BACK: MENU" : "BACK: MENU");

    /* the screen wipes down from the top while the input is locked */
    uint32_t since = app->now - app->scene_at;
    if(since < BB_OVER_LOCK) {
        uint32_t h = BB_H - (BB_H * since / BB_OVER_LOCK);
        if(h > BB_H) h = BB_H;
        canvas_set_color(c, ColorWhite);
        canvas_draw_box(c, 0, 0, BB_W, (size_t)h);
        canvas_set_color(c, ColorBlack);
    }
}


static void bb_draw_howtopick(Canvas* c, const BeepbackApp* app) {
    static const char* const topic[3] = {"CLASSIC", "RULES", "REFLEX"};
    bb_title(c, "HOW TO PLAY");
    for(uint8_t i = 0; i < 3; i++) bb_row(c, bb_row_y[i], topic[i], NULL, app->howto_cur == i);
    bb_footer(c, "OK READS IT");
}

/* All five rows fit between the title bar and the bottom edge, so this
   screen has no footer and never scrolls. */
static void bb_draw_settings(Canvas* c, const BeepbackApp* app) {
    static const char* const tail[3] = {"SOUNDS", "SCORES", "RESET"};
    static const uint8_t row_y[5] = {19, 29, 39, 49, 59};
    bb_title(c, "SETTINGS");
    for(uint8_t at = 0; at < 5; at++) {
        uint8_t y = row_y[at];
        bool sel = app->settings_cur == at;
        if(at == 0) {
            bb_adj_row(c, y, "VOLUME", bb_volume_name[bb_clamp(app->set.volume, BB_VOL_COUNT)],
                       sel, bb_can_adjust(app, -1), bb_can_adjust(app, 1));
        } else if(at == 1) {
            /* silence with ears only leaves nothing to play by, and the
               row says so, since this screen has no footer to say it in */
            bool forced = bb_effective_assist(app) != app->set.assist;
            uint8_t shown = forced ? bb_effective_assist(app) : app->set.assist;
            bb_adj_row(c, y, forced ? "ASSIST (SILENT)" : "ASSIST",
                       bb_assist_name[bb_clamp(shown, BB_ASSIST_COUNT)], sel,
                       bb_can_adjust(app, -1), bb_can_adjust(app, 1));
        } else {
            bb_row(c, y, tail[bb_clamp((uint8_t)(at - 2), 3)], NULL, sel);
        }
    }
}


static void bb_draw_sounds(Canvas* c, const BeepbackApp* app, uint8_t cur, const char* title) {
    char buf[16];
    uint8_t first = bb_window_first(cur, BbBtnCount, BB_ROWS);
    UNUSED(app);
    bb_title(c, title);
    for(uint8_t i = 0; i < BB_ROWS; i++) {
        uint8_t at = (uint8_t)(first + i);
        at = bb_clamp(at, BbBtnCount);
        snprintf(buf, sizeof(buf), "%u Hz", bb_button_hz[at]);
        bb_row(c, bb_row_y[i], bb_button_name[at], buf, cur == at);
    }
    bb_footer(c, "OK PLAYS IT");
}

static void bb_draw_scores(Canvas* c, const BeepbackApp* app) {
    uint8_t first = bb_window_first(app->scores_cur, BB_MODE_COUNT, BB_ROWS);
    bb_title(c, "SCORES");
    for(uint8_t i = 0; i < BB_ROWS; i++) {
        uint8_t at = (uint8_t)(first + i);
        bb_row(c, bb_row_y[i], bb_mode_name[bb_clamp(at, BB_MODE_COUNT)], ">", app->scores_cur == at);
    }
    bb_footer(c, "OK OPENS IT");
}

static void bb_draw_detail(Canvas* c, const BeepbackApp* app) {
    uint8_t mode = bb_clamp(app->det_mode, BB_MODE_COUNT);
    const uint32_t* best;
    uint32_t across[BB_ASSIST_COUNT];
    bb_title(c, "BEST");

    bb_adj_row(c, 18, "MODE", bb_mode_name[mode], app->det_cur == 0, bb_can_adjust(app, -1),
               bb_can_adjust(app, 1));
    if(mode == BbModeChallenge) {
        bb_adj_row(c, 30, "RULE", bb_rule_name[app->rule_cur % BB_RULE_COUNT], app->det_cur == 1,
                   bb_can_adjust(app, -1), bb_can_adjust(app, 1));
        best = app->rec.ch_best[app->rule_cur % BB_RULE_COUNT];
    } else if(mode == BbModeDaily) {
        char date[16];
        snprintf(date, sizeof(date), "%lu", (unsigned long)app->rec.daily_date);
        bb_row(c, 30, "DATE", date, false);
        best = app->rec.daily_best;
    } else {
        bb_adj_row(c, 27, "TIME", bb_time_name[bb_clamp(app->det_diff, BB_DIFF_COUNT)],
                   app->det_cur == 1, bb_can_adjust(app, -1), bb_can_adjust(app, 1));
        bb_adj_row(c, 36, "SPEED", bb_speed_name[bb_clamp(app->det_speed, BB_SPEED_COUNT)],
                   app->det_cur == 2, bb_can_adjust(app, -1), bb_can_adjust(app, 1));
        for(uint8_t a = 0; a < BB_ASSIST_COUNT; a++)
            across[a] = app->rec.best[bb_clamp(mode, BB_LADDER_MODES)]
                                     [bb_clamp(app->det_diff, BB_DIFF_COUNT)]
                                     [bb_clamp(app->det_speed, BB_SPEED_COUNT)][a];
        best = across;
    }
    bb_assist_columns(c, best, 47, 58);
}

static void bb_draw_reset(Canvas* c, const BeepbackApp* app) {
    static const char* const item[2] = {"SCORES", "TUTORIAL"};
    bb_title(c, "RESET");
    for(uint8_t i = 0; i < 2; i++) bb_row(c, bb_row_y[i], item[i], NULL, app->reset_cur == i);
    bb_footer(c, app->flash_at ? "CLEARED" : "OK CLEARS IT");
}

static void bb_draw_boardpick(Canvas* c, const BeepbackApp* app) {
    uint8_t first = bb_window_first(app->board_cur, BB_MODE_COUNT, BB_ROWS);
    bb_title(c, "BOARDS");
    for(uint8_t i = 0; i < BB_ROWS; i++) {
        uint8_t at = (uint8_t)(first + i);
        bb_row(c, bb_row_y[i], bb_mode_name[bb_clamp(at, BB_MODE_COUNT)], NULL, app->board_cur == at);
    }
    bb_left_arrow(c, BB_BODY_Y);
    bb_right_arrow(c, BB_BODY_Y);
    bb_footer(c, "< MENU   CREDITS >");
}

static void bb_draw_board(Canvas* c, const BeepbackApp* app) {
    uint32_t best[BB_ASSIST_COUNT];
    uint8_t mode = bb_clamp(app->board_cur, BB_MODE_COUNT);
    bb_title(c, bb_mode_name[mode]);
    for(uint8_t a = 0; a < BB_ASSIST_COUNT; a++)
        best[a] = bb_best_of_mode(app, (BbMode)mode, a);
    canvas_set_font(c, FontSecondary);
    bb_assist_block(c, best, 19, 9); /* four rows, clear of the footer band */
    bb_left_arrow(c, BB_BODY_Y);
    bb_footer(c, "< BOARDS");
}

static void bb_draw_credits(Canvas* c) {
    bb_title(c, "CREDITS");
    canvas_set_font(c, FontSecondary);
    canvas_draw_str_aligned(c, BB_W / 2, 24, AlignCenter, AlignCenter, "BEEPBACK v4");
    canvas_draw_str_aligned(c, BB_W / 2, 36, AlignCenter, AlignCenter, "BY TIJNV50");
    canvas_draw_str_aligned(c, BB_W / 2, 46, AlignCenter, AlignCenter, "WITH CLAUDE");
    bb_left_arrow(c, BB_BODY_Y);
    bb_footer(c, "< BOARDS");
}

/* ------------------------------------------------------------------ */

void bb_draw(Canvas* canvas, BeepbackApp* app) {
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontSecondary);

    switch(app->scene) {
    case BbSceneLauncher:
        bb_draw_launcher(canvas);
        break;
    case BbSceneSplash:
        bb_draw_splash(canvas, app);
        break;
    case BbSceneTutorial:
        bb_title(canvas, "HOW IT WORKS");
        bb_draw_page(
            canvas, &bb_tut_page[bb_clamp(app->tut_page, BB_TUT_PAGES)],
            bb_clamp(app->tut_page, BB_TUT_PAGES), BB_TUT_PAGES);
        break;
    case BbSceneMenu:
        bb_draw_menu(canvas, app);
        break;
    case BbSceneModeSelect:
        bb_draw_modeselect(canvas, app);
        break;
    case BbSceneRulePick:
        bb_draw_rulepick(canvas, app);
        break;
    case BbSceneSetup:
        bb_draw_setup(canvas, app);
        break;
    case BbSceneGame:
        bb_draw_game(canvas, app);
        break;
    case BbScenePause:
        bb_draw_pause(canvas, app);
        break;
    case BbSceneOver:
        bb_draw_over(canvas, app);
        break;
    case BbSceneHowToPick:
        bb_draw_howtopick(canvas, app);
        break;
    case BbSceneHowTo:
        bb_title(canvas, bb_mode_name[app->howto_cur == 2 ? (uint8_t)BbModeReflex : bb_clamp(app->howto_cur, 2)]);
        {
            uint8_t topic = bb_clamp(app->howto_cur, 3);
            uint8_t pages = bb_howto_pages(topic);
            uint8_t at = bb_clamp(app->howto_page, pages);
            bb_draw_page(canvas, &bb_howto_pageset(topic)[at], at, pages);
        }
        break;
    case BbSceneSoundTest:
        bb_draw_sounds(canvas, app, app->sound_btn, "SOUND TEST");
        break;
    case BbSceneSounds:
        bb_draw_sounds(canvas, app, app->sounds_cur, "SOUNDS");
        break;
    case BbSceneSettings:
        bb_draw_settings(canvas, app);
        break;
    case BbSceneScores:
        bb_draw_scores(canvas, app);
        break;
    case BbSceneScoreDetail:
        bb_draw_detail(canvas, app);
        break;
    case BbSceneReset:
        bb_draw_reset(canvas, app);
        break;
    case BbSceneBoardPick:
        bb_draw_boardpick(canvas, app);
        break;
    case BbSceneBoard:
        bb_draw_board(canvas, app);
        break;
    case BbSceneCredits:
        bb_draw_credits(canvas);
        break;
    default:
        break;
    }
}
