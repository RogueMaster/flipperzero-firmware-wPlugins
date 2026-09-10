/*
 * BEEPBACK - the intro.
 *
 * A brain, held and then dithered away; five buttons riding a ferris
 * wheel through the screen; a flash; a wipe. It is the most code in the
 * project for the least behaviour, which is why it was ported last and
 * why every key skips it.
 *
 * The wheel is bigger than the display on purpose: at BB_SP_RING the
 * cars ride in from one edge and out of the other, and only the ones
 * actually on the glass are drawn. Nothing here may reach outside
 * 128x64 any more than a menu may.
 */
#include "beepback.h"
#include "beepback_tables.h"

#define BB_SP_T1 (BB_SP_HOLD)
#define BB_SP_T2 (BB_SP_T1 + BB_SP_FADE)
#define BB_SP_T3 (BB_SP_T2 + BB_SP_GLIDE)
#define BB_SP_T4 (BB_SP_T3 + BB_SP_FLASH)
#define BB_SP_T5 (BB_SP_T4 + BB_SP_WIPE)

static bool bb_onscreen(int32_t x, int32_t y, int32_t margin) {
    return x - margin >= 0 && x + margin < BB_W && y - margin >= 0 && y + margin < BB_H;
}

/* ------------------------------------------------------------------ */
/* The brain                                                           */
/* ------------------------------------------------------------------ */

static void bb_brain(Canvas* c, int32_t cx, int32_t cy) {
    /* an outline with a stem down the middle and four folds a side,
       which is as much brain as 44x30 pixels will carry */
    canvas_draw_rframe(c, cx - 22, cy - 15, 44, 26, 11);
    canvas_draw_line(c, cx, cy - 15, cx, cy + 11);
    canvas_draw_line(c, cx - 2, cy + 11, cx - 2, cy + 15);
    canvas_draw_line(c, cx + 2, cy + 11, cx + 2, cy + 15);
    canvas_draw_line(c, cx - 2, cy + 15, cx + 2, cy + 15);
    for(int32_t i = 0; i < 3; i++) {
        int32_t y = cy - 9 + i * 8;
        canvas_draw_line(c, cx - 17, y, cx - 8, y);
        canvas_draw_line(c, cx - 8, y, cx - 11, y + 4);
        canvas_draw_line(c, cx + 17, y, cx + 8, y);
        canvas_draw_line(c, cx + 8, y, cx + 11, y + 4);
    }
}

/* An ordered dither over 2x2 blocks: level runs 0 to 16 and says how
   much of the box has been punched back out to white. */
static void bb_dither(Canvas* c, int32_t x0, int32_t y0, int32_t w, int32_t h, uint8_t level) {
    static const uint8_t bayer[16] = {0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5};
    canvas_set_color(c, ColorWhite);
    for(int32_t y = 0; y < h; y += 2) {
        for(int32_t x = 0; x < w; x += 2) {
            uint8_t cell = bayer[(((y / 2) & 3) * 4) + (((x / 2) & 3))];
            if(cell >= level) continue;
            int32_t px = x0 + x, py = y0 + y;
            if(px < 0 || py < 0 || px + 2 > BB_W || py + 2 > BB_H) continue;
            canvas_draw_box(c, px, py, 2, 2);
        }
    }
    canvas_set_color(c, ColorBlack);
}

/* ------------------------------------------------------------------ */
/* The wheel                                                           */
/* ------------------------------------------------------------------ */

/* The five buttons ride the rim in their tone order, so the intro is
   the ladder the whole game is built on, going past once. */
static void bb_wheel(Canvas* c, float turns) {
    const int32_t cx = BB_W / 2, cy = 100; /* the hub sits below the glass */
    const float r = BB_SP_RING;
    float base = turns * 64.0f;

    /* the rim, one dot at a time, and only where there is screen */
    for(int32_t i = 0; i < 64; i++) {
        int32_t x = cx + (int32_t)(r * BB_SIN[i]);
        int32_t y = cy - (int32_t)(r * BB_SIN[(i + 16) & 63]);
        if(bb_onscreen(x, y, 0)) canvas_draw_dot(c, x, y);
    }

    static const uint8_t order[BbBtnCount] = {BbBtnDown, BbBtnLeft, BbBtnOk, BbBtnRight, BbBtnUp};
    for(uint8_t car = 0; car < BbBtnCount; car++) {
        int32_t idx = (int32_t)(base + (float)car * 64.0f / (float)BbBtnCount) & 63;
        int32_t x = cx + (int32_t)(r * BB_SIN[idx]);
        int32_t y = cy - (int32_t)(r * BB_SIN[(idx + 16) & 63]);
        if(!bb_onscreen(x, y, 9)) continue;
        /* a stub of spoke, kept short so it cannot leave with the car */
        canvas_draw_line(c, x, y, x + (cx - x) / 8, y + (cy - y) / 8);
        canvas_set_color(c, ColorWhite);
        canvas_draw_disc(c, x, y, 8);
        canvas_set_color(c, ColorBlack);
        canvas_draw_circle(c, x, y, 8);
        uint8_t btn = order[car];
        switch(bb_button_shape[btn]) {
        case BbShapeCircle:
            canvas_draw_disc(c, x, y, 4);
            break;
        case BbShapeTriangle:
            canvas_draw_line(c, x, y - 4, x - 4, y + 3);
            canvas_draw_line(c, x, y - 4, x + 4, y + 3);
            canvas_draw_line(c, x - 4, y + 3, x + 4, y + 3);
            break;
        case BbShapeSquare:
            canvas_draw_box(c, x - 3, y - 3, 7, 7);
            break;
        case BbShapePentagon:
            canvas_draw_line(c, x, y - 4, x + 4, y - 1);
            canvas_draw_line(c, x + 4, y - 1, x + 2, y + 4);
            canvas_draw_line(c, x + 2, y + 4, x - 2, y + 4);
            canvas_draw_line(c, x - 2, y + 4, x - 4, y - 1);
            canvas_draw_line(c, x - 4, y - 1, x, y - 4);
            break;
        default:
            canvas_draw_line(c, x - 4, y, x + 4, y);
            canvas_draw_line(c, x, y - 4, x, y + 4);
            canvas_draw_line(c, x - 3, y - 3, x + 3, y + 3);
            canvas_draw_line(c, x - 3, y + 3, x + 3, y - 3);
            break;
        }
    }
}

/* ------------------------------------------------------------------ */

void bb_draw_splash(Canvas* c, const BeepbackApp* app) {
    uint32_t t = app->now - app->scene_at;

    if(t < BB_SP_T1) {
        bb_brain(c, BB_W / 2, 30);
        canvas_set_font(c, FontSecondary);
        canvas_draw_str_aligned(c, BB_W / 2, BB_H - 6, AlignCenter, AlignCenter, "BEEPBACK");
        return;
    }

    if(t < BB_SP_T2) {
        uint32_t into = t - BB_SP_T1;
        bb_brain(c, BB_W / 2, 30);
        canvas_set_font(c, FontSecondary);
        canvas_draw_str_aligned(c, BB_W / 2, BB_H - 6, AlignCenter, AlignCenter, "BEEPBACK");
        bb_dither(c, BB_W / 2 - 24, 12, 48, 40, (uint8_t)(1 + into * 16 / BB_SP_FADE));
        return;
    }

    if(t < BB_SP_T3) {
        uint32_t into = t - BB_SP_T2;
        bb_wheel(c, BB_SP_TURNS * (float)into / (float)BB_SP_GLIDE);
        return;
    }

    if(t < BB_SP_T4) {
        uint32_t into = t - BB_SP_T3;
        /* white out, then the wordmark arrives out of the glare */
        if(into * 4 < BB_SP_FLASH) return;
        canvas_set_color(c, ColorBlack);
        canvas_draw_box(c, 0, 0, BB_W, BB_H);
        canvas_set_color(c, ColorWhite);
        canvas_set_font(c, FontPrimary);
        canvas_draw_str_aligned(c, BB_W / 2, 32, AlignCenter, AlignCenter, "BEEPBACK");
        canvas_set_color(c, ColorBlack);
        return;
    }

    /* the wipe hands the screen over, top down */
    uint32_t into = t < BB_SP_T5 ? t - BB_SP_T4 : BB_SP_WIPE;
    uint32_t cut = BB_H * into / BB_SP_WIPE;
    if(cut > BB_H) cut = BB_H;
    if(cut < BB_H) {
        canvas_set_color(c, ColorBlack);
        canvas_draw_box(c, 0, (int32_t)cut, BB_W, (size_t)(BB_H - cut));
        canvas_set_color(c, ColorWhite);
        canvas_set_font(c, FontPrimary);
        canvas_draw_str_aligned(c, BB_W / 2, 32, AlignCenter, AlignCenter, "BEEPBACK");
        canvas_set_color(c, ColorBlack);
    }
}
