/*
 * BEEPBACK - the intro, ported from the browser build's splash.
 *
 * Brain, dither, ferris wheel, flash, wipe. The lobe and groove tables
 * are the browser's, the ordered dither is the browser's Bayer matrix,
 * and the wheel contracts inward on the same easeOut curve. Every key
 * skips it.
 */
#include "beepback.h"
#include "beepback_tables.h"

#define SP_T1 (BB_SP_HOLD)
#define SP_T2 (SP_T1 + BB_SP_FADE)
#define SP_T3 (SP_T2 + BB_SP_GLIDE)
#define SP_T4 (SP_T3 + BB_SP_FLASH)
#define SP_T5 (SP_T4 + BB_SP_WIPE)

/* BRAIN_LOBES: x, y, r */
static const int8_t BRAIN_LOBES[7][3] = {
    {14, 14, 11},
    {24, 11, 10},
    {33, 15, 10},
    {19, 22, 10},
    {29, 23, 10},
    {11, 20, 8},
    {37, 22, 7}};
/* BRAIN_GROOVES: polylines, terminated by a -1 x */
static const int8_t BRAIN_GROOVES[7][5][2] = {
    {{24, 4}, {24, 30}, {-1, -1}, {-1, -1}, {-1, -1}},
    {{8, 14}, {14, 11}, {18, 15}, {13, 18}, {-1, -1}},
    {{9, 24}, {15, 21}, {19, 25}, {14, 28}, {-1, -1}},
    {{30, 12}, {36, 10}, {39, 15}, {34, 17}, {-1, -1}},
    {{31, 24}, {37, 21}, {41, 25}, {35, 27}, {-1, -1}},
    {{18, 7}, {22, 5}, {-1, -1}, {-1, -1}, {-1, -1}},
    {{27, 6}, {32, 7}, {-1, -1}, {-1, -1}, {-1, -1}}};

static void bb_brain(Canvas* c, int32_t ox, int32_t oy) {
    canvas_set_color(c, ColorBlack);
    for(uint8_t i = 0; i < 7; i++)
        canvas_draw_disc(
            c, ox + BRAIN_LOBES[i][0], oy + BRAIN_LOBES[i][1], (size_t)BRAIN_LOBES[i][2]);
    /* the stem */
    for(int32_t y = 31; y <= 36; y++) {
        int32_t inset = (y - 31) / 3;
        canvas_draw_line(c, ox + 22 + inset, oy + y, ox + 30 - inset, oy + y);
    }
    /* the grooves are knocked back out in white */
    canvas_set_color(c, ColorWhite);
    for(uint8_t g = 0; g < 7; g++)
        for(uint8_t p = 0; p + 1 < 5; p++) {
            if(BRAIN_GROOVES[g][p + 1][0] < 0) break;
            canvas_draw_line(
                c,
                ox + BRAIN_GROOVES[g][p][0],
                oy + BRAIN_GROOVES[g][p][1],
                ox + BRAIN_GROOVES[g][p + 1][0],
                oy + BRAIN_GROOVES[g][p + 1][1]);
        }
    canvas_set_color(c, ColorBlack);
}

/* ordered dither: the only honest way to fade on a 1-bit screen */
static const uint8_t BAYER[4][4] = {{0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}};

static void bb_dither_erase(Canvas* c, int32_t x, int32_t y, int32_t w, int32_t h, uint8_t level) {
    canvas_set_color(c, ColorWhite);
    for(int32_t py = y; py < y + h; py++)
        for(int32_t px = x; px < x + w; px++)
            if(BAYER[py & 3][px & 3] < level) canvas_draw_dot(c, px, py);
    canvas_set_color(c, ColorBlack);
}

/* easeOut: 1 - (1-t)^3, on a 0..1000 scale to stay clear of floats here */
static uint32_t ease_out(uint32_t t1000) {
    uint32_t inv = 1000 - (t1000 > 1000 ? 1000 : t1000);
    return 1000 - (inv * inv / 1000) * inv / 1000;
}

void bb_splash_done(BeepbackApp* app) {
    if(app->first_run) {
        app->first_run = false;
        /* and remember it: this flag is what the save file carries, and
           without it the guide opened itself on every single launch */
        app->set.tutorial_done = true;
        app->tut_page = 0;
        app->help_from = BbSceneMenu;
        bb_enter(app, BbSceneTutorial);
    } else {
        bb_enter(app, BbSceneMenu);
    }
}

void bb_update_splash(BeepbackApp* app) {
    static const uint32_t ph[5] = {BB_SP_HOLD, BB_SP_FADE, BB_SP_GLIDE, BB_SP_FLASH, BB_SP_WIPE};
    static const uint8_t order[BbBtnCount] = {BbBtnDown, BbBtnLeft, BbBtnOk, BbBtnRight, BbBtnUp};
    if(!app->sp_start) app->sp_start = app->now;
    uint32_t t = app->now - app->sp_start;
    uint8_t i = 0;
    while(i < 5 && t >= ph[i]) {
        t -= ph[i];
        i++;
    }
    if(i >= 5) {
        bb_splash_done(app);
        return;
    }
    app->sp_phase = i;

    if(i == 3) { /* one tone per shape as they flash past */
        int8_t idx = (int8_t)(t * 5 / (ph[3] ? ph[3] : 1));
        if(idx > 4) idx = 4;
        if(idx != app->sp_flash_idx) {
            app->sp_flash_idx = idx;
            bb_tone(app, bb_button_hz[order[idx]], 90);
            bb_led_flash(app, bb_button_led[order[idx]], 130);
        }
    }
}

/* 0..1000 through the current phase */
static uint32_t sp_t(const BeepbackApp* app) {
    static const uint32_t ph[5] = {BB_SP_HOLD, BB_SP_FADE, BB_SP_GLIDE, BB_SP_FLASH, BB_SP_WIPE};
    uint32_t t = app->now - app->sp_start;
    for(uint8_t i = 0; i < app->sp_phase && i < 5; i++)
        t -= ph[i];
    uint32_t len = ph[app->sp_phase < 5 ? app->sp_phase : 4];
    return len ? (t * 1000 / len) : 1000;
}

void bb_draw_splash(Canvas* c, BeepbackApp* app) {
    static const uint8_t order[BbBtnCount] = {BbBtnDown, BbBtnLeft, BbBtnOk, BbBtnRight, BbBtnUp};
    uint8_t ph = app->sp_phase;
    uint32_t t = sp_t(app);

    if(ph == 0 || ph == 1) {
        bb_brain(c, 6, 14);
        canvas_set_color(c, ColorBlack);
        canvas_set_font(c, FontPrimary);
        canvas_draw_str_aligned(c, 94, 33, AlignCenter, AlignCenter, "BEEPBACK");
        if(ph == 1) bb_dither_erase(c, 0, 0, BB_W, BB_H, (uint8_t)(t * 17 / 1000));
        return;
    }
    if(ph == 2) {
        /* the ring turns and closes in; the shapes stay upright */
        uint32_t e = ease_out(t);
        int32_t rad = (int32_t)(78 * (1000 - e) / 1000);
        int32_t r = 7 + (int32_t)(5 * e / 1000);
        /* BB_SP_TURNS turns, in 64ths of a circle for the sine table */
        int32_t spin = (int32_t)((uint32_t)(BB_SP_TURNS * 64.0f) * e / 1000);
        for(uint8_t k = 0; k < BbBtnCount; k++) {
            int32_t idx = (spin + k * 64 / BbBtnCount) & 63;
            /* -90 degrees is index 48, which is where the browser starts */
            int32_t cx = 64 + (int32_t)(rad * BB_SIN[(idx + 48) & 63]);
            int32_t cy = 32 + (int32_t)(rad * BB_SIN[idx]);
            if(cx - r < 0 || cx + r >= BB_W || cy - r < 0 || cy + r >= BB_H) continue;
            canvas_set_color(c, ColorBlack);
            bb_shape_public(c, cx, cy, r, bb_button_shape[order[k]]);
        }
        return;
    }
    if(ph == 3) {
        int32_t idx = (int32_t)(t * 5 / 1000);
        if(idx > 4) idx = 4;
        bool inv = ((t * 5) % 1000) < 350;
        bb_shape_flash(c, 64, 32, BB_SHAPE_R, bb_button_shape[order[idx]], inv);
        return;
    }
    /* the wipe hands the screen over, top down, with the menu underneath */
    bb_draw_under_wipe(c, app);
    canvas_set_color(c, ColorWhite);
    canvas_draw_box(c, 0, (int32_t)(t * BB_H / 1000), BB_W, BB_H);
    canvas_set_color(c, ColorBlack);
}
