/*
 * BEEPBACK - the screens, ported from the browser build one draw call at
 * a time. Every coordinate here is the browser's coordinate.
 *
 * The browser's C.str() upper-cases whatever it is handed, so the source
 * strings are written the way they read and bb_str() upper-cases them on
 * the way to the canvas. That keeps the two sets of strings comparable
 * by eye, which is the whole point of copying them across verbatim.
 */
#include "beepback.h"
#include "beepback_tables.h"
#include <stdio.h>

#define FONT_S_H 9 /* the browser's FONT_S.h */
#define FONT_P_H 10 /* and FONT_P.h          */

static bool bb_font_primary;

static void bb_font(Canvas* c, bool primary) {
    bb_font_primary = primary;
    canvas_set_font(c, primary ? FontPrimary : FontSecondary);
}
static int32_t bb_font_h(void) {
    return bb_font_primary ? FONT_P_H : FONT_S_H;
}

/* upper-case into a scratch buffer, as C.str() does */
static const char* bb_up(const char* t) {
    static char buf[48];
    size_t i = 0;
    for(; t && t[i] && i < sizeof(buf) - 1; i++)
        buf[i] = (t[i] >= 'a' && t[i] <= 'z') ? (char)(t[i] - 32) : t[i];
    buf[i] = 0;
    return buf;
}
static void bb_str(Canvas* c, int32_t x, int32_t y, Align h, Align v, const char* t) {
    canvas_draw_str_aligned(c, x, y, h, v, bb_up(t));
}
static int32_t bb_text_w(Canvas* c, const char* t) {
    return canvas_string_width(c, bb_up(t));
}

/* ------------------------------------------------------------------ */
/* Chevrons, bands and dots                                            */
/* ------------------------------------------------------------------ */

/* 2px thick so it reads as an arrow, not a scratch */
static void bb_chev_l(Canvas* c, int32_t x, int32_t y) {
    canvas_draw_line(c, x + 4, y - 4, x, y);
    canvas_draw_line(c, x, y, x + 4, y + 4);
    canvas_draw_line(c, x + 5, y - 4, x + 1, y);
    canvas_draw_line(c, x + 1, y, x + 5, y + 4);
}
static void bb_chev_r(Canvas* c, int32_t x, int32_t y) {
    canvas_draw_line(c, x, y - 4, x + 4, y);
    canvas_draw_line(c, x + 4, y, x, y + 4);
    canvas_draw_line(c, x + 1, y - 4, x + 5, y);
    canvas_draw_line(c, x + 5, y, x + 1, y + 4);
}
static void bb_chev_u(Canvas* c, int32_t x, int32_t y) {
    canvas_draw_line(c, x - 3, y + 2, x, y - 2);
    canvas_draw_line(c, x, y - 2, x + 3, y + 2);
}
static void bb_chev_d(Canvas* c, int32_t x, int32_t y) {
    canvas_draw_line(c, x - 3, y - 2, x, y + 2);
    canvas_draw_line(c, x, y + 2, x + 3, y - 2);
}

static void bb_title(Canvas* c, const char* text) {
    canvas_set_color(c, ColorBlack);
    canvas_draw_box(c, 0, 0, BB_W, 13);
    canvas_set_color(c, ColorWhite);
    bb_font(c, true);
    bb_str(c, 64, 6, AlignCenter, AlignCenter, text);
    canvas_set_color(c, ColorBlack);
}

/* hints live in their own strip so they cannot read as a pressable row */
static void bb_footer(Canvas* c, const char* text) {
    canvas_set_color(c, ColorBlack);
    canvas_draw_box(c, 0, 53, BB_W, 11);
    canvas_set_color(c, ColorWhite);
    bb_font(c, false);
    bb_str(c, 64, 58, AlignCenter, AlignCenter, text);
    canvas_set_color(c, ColorBlack);
}

/* Drawn as pixels, not arcs. An antialiased circle thresholds differently
   depending on ink colour, which is why the footer dots did not match the
   ones in the game. */
static const char* const BB_DOT_FULL[5] = {"..#..", ".###.", "#####", ".###.", "..#.."};
static const char* const BB_DOT_EMPTY[5] = {".###.", "#...#", "#...#", "#...#", ".###."};

static void bb_page_dot(Canvas* c, int32_t cx, int32_t cy, bool full) {
    const char* const* art = full ? BB_DOT_FULL : BB_DOT_EMPTY;
    for(int32_t r = 0; r < 5; r++)
        for(int32_t col = 0; col < 5; col++)
            if(art[r][col] == '#') canvas_draw_dot(c, cx - 2 + col, cy - 2 + r);
}

static void bb_footer_pages(Canvas* c, uint8_t total, uint8_t at, const char* label) {
    canvas_set_color(c, ColorBlack);
    canvas_draw_box(c, 0, 53, BB_W, 11);
    canvas_set_color(c, ColorWhite);
    bb_font(c, false);
    for(uint8_t i = 0; i < total; i++) bb_page_dot(c, BB_ROW_L + i * 7, 58, i == at);
    bb_str(c, 108, 58, AlignRight, AlignCenter, label);
    bb_chev_r(c, 112, 58);
    canvas_set_color(c, ColorBlack);
}

/* flanks a value with chevrons so it is obvious the row is adjustable */
static void bb_adjustable(Canvas* c, int32_t y, const char* text, bool left, bool right) {
    bb_font(c, false);
    bb_str(c, BB_ROW_VAL, y, AlignRight, AlignCenter, text);
    if(left) bb_chev_l(c, BB_ROW_AL, y);
    if(right) bb_chev_r(c, BB_ROW_AR, y);
}

/* ------------------------------------------------------------------ */
/* Hearts and step dots                                                */
/* ------------------------------------------------------------------ */

static const char* const BB_HEART_FULL[BB_HEART_H] =
    {".##.##.", "#######", "#######", ".#####.", "..###..", "...#..."};
static const char* const BB_HEART_EMPTY[BB_HEART_H] =
    {".##.##.", "#..#..#", "#.....#", ".#...#.", "..#.#..", "...#..."};

static void bb_heart(Canvas* c, int32_t x, int32_t y, bool full, int32_t s) {
    const char* const* art = full ? BB_HEART_FULL : BB_HEART_EMPTY;
    for(int32_t r = 0; r < BB_HEART_H; r++)
        for(int32_t col = 0; col < BB_HEART_W; col++) {
            if(art[r][col] != '#') continue;
            if(s <= 1) {
                canvas_draw_dot(c, x + col, y + r);
            } else {
                canvas_draw_box(c, x + col * s, y + r * s, (size_t)s, (size_t)s);
            }
        }
}
static void bb_lives(Canvas* c, int32_t x, int32_t y, uint8_t n, int32_t s) {
    for(int32_t i = 0; i < BB_LIVES; i++) bb_heart(c, x + i * 8 * s, y, i < (int32_t)n, s);
}

static void bb_steps(Canvas* c, int32_t y, uint8_t total, uint8_t done) {
    char buf[8];
    if(!total) return;
    if(total > BB_DOT_MAX) {
        bb_font(c, false);
        snprintf(buf, sizeof(buf), "%u", done);
        bb_str(c, 64, y, AlignCenter, AlignCenter, buf);
        return;
    }
    int32_t sp = 8;
    if(total * sp > 120) sp = 120 / total;
    int32_t start = 64 - ((total - 1) * sp) / 2;
    for(uint8_t i = 0; i < total; i++) bb_page_dot(c, start + i * sp, y, i < done);
}

/* ------------------------------------------------------------------ */
/* Shapes and arrows                                                   */
/* ------------------------------------------------------------------ */

static void bb_poly(
    Canvas* c,
    int32_t cx,
    int32_t cy,
    const float* xs,
    const float* ys,
    uint8_t n,
    int32_t r) {
    int32_t px[10], py[10];
    for(uint8_t i = 0; i < n; i++) {
        px[i] = cx + (int32_t)(xs[i] * (float)r);
        py[i] = cy + (int32_t)(ys[i] * (float)r);
    }
    int32_t lo = py[0], hi = py[0];
    for(uint8_t i = 1; i < n; i++) {
        if(py[i] < lo) lo = py[i];
        if(py[i] > hi) hi = py[i];
    }
    for(int32_t y = lo; y <= hi; y++) {
        int32_t at[10];
        uint8_t hits = 0;
        for(uint8_t i = 0; i < n && hits < 10; i++) {
            uint8_t j = (uint8_t)((i + 1) % n);
            int32_t y0 = py[i], y1 = py[j];
            if((y0 <= y && y1 > y) || (y1 <= y && y0 > y))
                at[hits++] = px[i] + (px[j] - px[i]) * (y - y0) / (y1 - y0);
        }
        for(uint8_t a = 1; a < hits; a++) {
            int32_t v = at[a];
            int8_t b = (int8_t)(a - 1);
            while(b >= 0 && at[b] > v) {
                at[b + 1] = at[b];
                b--;
            }
            at[b + 1] = v;
        }
        for(uint8_t a = 0; a + 1 < hits; a += 2) canvas_draw_line(c, at[a], y, at[a + 1], y);
    }
}

static void bb_shape_at(Canvas* c, int32_t cx, int32_t cy, int32_t r, uint8_t kind) {
    switch(kind) {
    case BbShapeCircle:
        canvas_draw_disc(c, cx, cy, (size_t)r);
        break;
    case BbShapeTriangle:
        bb_poly(c, cx, cy, BB_TRI_X, BB_TRI_Y, 3, r);
        break;
    case BbShapeSquare:
        bb_poly(c, cx, cy, BB_SQR_X, BB_SQR_Y, 4, r);
        break;
    case BbShapePentagon:
        bb_poly(c, cx, cy, BB_PENT_X, BB_PENT_Y, 5, r);
        break;
    default:
        bb_poly(c, cx, cy, BB_STAR_X, BB_STAR_Y, 10, r);
        break;
    }
}

void bb_shape_public(Canvas* c, int32_t cx, int32_t cy, int32_t r, uint8_t kind) {
    bb_shape_at(c, cx, cy, r, kind);
}

static void bb_shape(Canvas* c, int32_t cx, int32_t cy, int32_t r, uint8_t kind, bool invert) {
    if(invert) {
        canvas_set_color(c, ColorBlack);
        canvas_draw_box(c, cx - r - 4, cy - r - 4, (size_t)(r * 2 + 8), (size_t)(r * 2 + 8));
        canvas_set_color(c, ColorWhite);
    } else {
        canvas_set_color(c, ColorBlack);
    }
    bb_shape_at(c, cx, cy, r, kind);
    canvas_set_color(c, ColorBlack);
}

static void bb_glyph(Canvas* c, int32_t cx, int32_t cy, uint8_t btn, int32_t s) {
    int32_t q = s / 2;
    if(btn == BbBtnUp)
        for(int32_t i = 0; i <= s; i++) canvas_draw_line(c, cx - i, cy - s + i + q, cx + i, cy - s + i + q);
    if(btn == BbBtnDown)
        for(int32_t i = 0; i <= s; i++) canvas_draw_line(c, cx - i, cy + s - i - q, cx + i, cy + s - i - q);
    if(btn == BbBtnLeft)
        for(int32_t i = 0; i <= s; i++) canvas_draw_line(c, cx - s + i + q, cy - i, cx - s + i + q, cy + i);
    if(btn == BbBtnRight)
        for(int32_t i = 0; i <= s; i++) canvas_draw_line(c, cx + s - i - q, cy - i, cx + s - i - q, cy + i);
    if(btn == BbBtnOk) canvas_draw_disc(c, cx, cy, (size_t)(s - 2 > 1 ? s - 2 : 1));
}

/* the assist visual for one button */
static void bb_cue(
    Canvas* c,
    const BeepbackApp* app,
    int32_t cx,
    int32_t cy,
    uint8_t btn,
    int32_t r,
    bool invert) {
    if(bb_shapes_on(app)) {
        bb_shape(c, cx, cy, r, bb_button_shape[btn], invert);
    } else if(bb_arrows_on(app)) {
        if(invert) {
            canvas_set_color(c, ColorBlack);
            canvas_draw_box(c, cx - r - 4, cy - r - 4, (size_t)(r * 2 + 8), (size_t)(r * 2 + 8));
            canvas_set_color(c, ColorWhite);
        }
        bb_glyph(c, cx, cy, btn, r);
        canvas_set_color(c, ColorBlack);
    }
}

/* "DOUBLE DOWN" reads as the idiom. Boxing the button name splits it into
   an instruction and a button, which is what it actually is. */
static void bb_rule_line(Canvas* c, const BeepbackApp* app, int32_t cx, int32_t cy, bool primary,
                         bool inv) {
    bb_font(c, primary);
    int32_t h = bb_font_h(), gap = 4, total = 0, w[3];
    uint8_t n = app->rule_seg_n ? app->rule_seg_n : 1;
    for(uint8_t i = 0; i < n; i++) {
        const BbSeg* sg = &app->rule_segs[i];
        const char* t = sg->btn >= 0 ? bb_button_name[sg->btn] : (sg->text ? sg->text : "");
        w[i] = bb_text_w(c, t) + (sg->btn >= 0 ? 7 : 0);
        total += w[i];
    }
    total += gap * (n - 1);
    int32_t x = cx - total / 2;
    for(uint8_t i = 0; i < n; i++) {
        const BbSeg* sg = &app->rule_segs[i];
        if(sg->btn >= 0) {
            canvas_set_color(c, inv ? ColorWhite : ColorBlack);
            canvas_draw_rbox(c, x, cy - h / 2 - 1, (size_t)w[i], (size_t)(h + 2), 2);
            canvas_set_color(c, inv ? ColorBlack : ColorWhite);
            bb_str(c, x + w[i] / 2, cy, AlignCenter, AlignCenter, bb_button_name[sg->btn]);
            canvas_set_color(c, inv ? ColorWhite : ColorBlack);
        } else {
            bb_str(c, x + w[i] / 2, cy, AlignCenter, AlignCenter, sg->text ? sg->text : "");
        }
        x += w[i] + gap;
    }
}

/* ------------------------------------------------------------------ */
/* Menus                                                               */
/* ------------------------------------------------------------------ */

static void bb_sel_rbox(Canvas* c, int32_t x, int32_t top, int32_t w, int32_t h, int32_t r) {
    canvas_set_color(c, ColorBlack);
    canvas_draw_rbox(c, x, top, (size_t)w, (size_t)h, (size_t)r);
    canvas_set_color(c, ColorWhite);
}

static void bb_draw_menu(Canvas* c, const BeepbackApp* app) {
    static const char* const items[3] = {"PLAY", "HOW TO PLAY", "SETTINGS"};
    bb_title(c, "BEEPBACK");
    for(uint8_t i = 0; i < 3; i++) {
        int32_t top = 19 + i * 15;
        if(app->menu_idx == i) bb_sel_rbox(c, 12, top, 104, 14, 3);
        bb_font(c, true);
        bb_str(c, 64, top + 7, AlignCenter, AlignCenter, items[i]);
        canvas_set_color(c, ColorBlack);
    }
    bb_chev_r(c, BB_CHEV_R, 36); /* scores are that way */
}

static void bb_draw_score_pick(Canvas* c, const BeepbackApp* app) {
    bb_title(c, "BEST SCORES");
    for(uint8_t i = 0; i < BB_MODE_COUNT; i++) {
        int32_t top = 14 + i * 10;
        if(app->score_mode == i) bb_sel_rbox(c, 12, top, 104, 10, 3);
        bb_font(c, false);
        bb_str(c, 64, top + 5, AlignCenter, AlignCenter, bb_mode_name[i]);
        canvas_set_color(c, ColorBlack);
    }
    /* LEFT has always gone back to the menu from here; the arrow saying so
       was the one that was missing, which reads as no way out but BACK */
    bb_chev_l(c, BB_CHEV_L, 36);
    bb_chev_r(c, BB_CHEV_R, 36);
}

static void bb_draw_scores(Canvas* c, const BeepbackApp* app) {
    char buf[40];
    uint8_t m = app->score_mode < BB_MODE_COUNT ? app->score_mode : 0;
    snprintf(buf, sizeof(buf), "%s SCORES", bb_mode_name[m]);
    bb_title(c, buf);
    bb_font(c, false);
    for(uint8_t i = 0; i < BB_ASSIST_COUNT; i++) {
        int32_t y = 21 + i * 11;
        bb_str(c, BB_ROW_L, y, AlignLeft, AlignCenter, bb_mode_label[i]);
        snprintf(buf, sizeof(buf), "%lu", (unsigned long)bb_best_for(app, m, i));
        bb_str(c, BB_ROW_R, y, AlignRight, AlignCenter, buf);
    }
    bb_chev_l(c, BB_CHEV_L, 36);
}

static void bb_draw_credits(Canvas* c) {
    bb_title(c, "CREDITS");
    bb_font(c, false);
    bb_str(c, 64, 26, AlignCenter, AlignCenter, "made by");
    bb_font(c, true);
    bb_str(c, 64, 38, AlignCenter, AlignCenter, "TIJNV50");
    bb_font(c, false);
    bb_str(c, 64, 52, AlignCenter, AlignCenter, "with Claude (Opus 5)");
    bb_chev_l(c, BB_CHEV_L, 36);
}

static void bb_draw_mode(Canvas* c, const BeepbackApp* app) {
    bb_title(c, "MODE");
    uint8_t page = (uint8_t)(app->mode_idx >> 1);
    for(uint8_t i = page * 2; i < page * 2 + 2 && i < BB_MODE_COUNT; i++) {
        int32_t top = 20 + (i - page * 2) * 16;
        if(app->mode_idx == i) bb_sel_rbox(c, 12, top, 104, 14, 3);
        bb_font(c, true);
        bb_str(c, 64, top + 7, AlignCenter, AlignCenter, bb_mode_name[i]);
        canvas_set_color(c, ColorBlack);
    }
    if(page > 0) bb_chev_l(c, BB_CHEV_L, 36);
    if(page < 2) bb_chev_r(c, BB_CHEV_R, 36);
    bb_footer(c, bb_mode_blurb[app->mode_idx < BB_MODE_COUNT ? app->mode_idx : 0]);
}

/* seven rules do not fit, so show where you are */
static void bb_scrollbar(Canvas* c, int32_t top, int32_t track, uint8_t vis, uint8_t n,
                         uint8_t scroll) {
    if(n > vis && scroll > n - vis) scroll = (uint8_t)(n - vis);
    canvas_draw_frame(c, 122, top, 4, (size_t)track);
    int32_t thumb = track * vis / n;
    if(thumb < 6) thumb = 6;
    int32_t room = track - thumb;
    int32_t y = top + (n > vis ? room * scroll / (n - vis) : 0);
    canvas_draw_box(c, 123, y + 1, 2, (size_t)(thumb - 2));
}

static const char* bb_ch_name(uint8_t i) {
    return i >= BB_RULE_COUNT ? "RANDOM" : bb_rule_label[i];
}

static void bb_draw_ch_pick(Canvas* c, const BeepbackApp* app) {
    const uint8_t VIS = 4, rowH = 11, n = BB_RULE_COUNT + 1;
    const int32_t top0 = 15;
    bb_title(c, "CHALLENGE");
    for(uint8_t i = 0; i < VIS; i++) {
        uint8_t idx = (uint8_t)(app->ch_scroll + i);
        if(idx >= n) break;
        int32_t top = top0 + i * rowH;
        if(idx == app->ch_idx) bb_sel_rbox(c, 2, top, 116, rowH, 2);
        bb_font(c, false);
        bb_str(c, BB_ROW_L, top + 5, AlignLeft, AlignCenter, bb_ch_name(idx));
        if(idx == app->ch_idx) bb_chev_r(c, 104, top + 5);
        canvas_set_color(c, ColorBlack);
    }
    bb_scrollbar(c, top0, VIS * rowH, VIS, n, app->ch_scroll);
}

static void bb_draw_setup(Canvas* c, const BeepbackApp* app) {
    const char* label[4];
    const char* value[4];
    uint8_t kind[4];
    char a[24], b[24];
    uint8_t rows = bb_setup_rows(app, label, value, kind, a, b, sizeof(a));
    uint8_t last = (uint8_t)(rows - 1);
    const int32_t rowH = 12;
    int32_t top0 = 13 + (51 - rows * rowH) / 2;

    bb_title(c, bb_mode_name[app->mode < BB_MODE_COUNT ? app->mode : 0]);
    for(uint8_t i = 0; i < rows; i++) {
        int32_t top = top0 + i * rowH, mid = top + rowH / 2;
        bool sel = (app->setup_idx == i);
        if(sel) bb_sel_rbox(c, 2, top, 124, rowH, 2);
        bb_font(c, i == last);
        bb_str(c, i == last ? 64 : BB_ROW_L, mid, i == last ? AlignCenter : AlignLeft,
               AlignCenter, label[i]);
        if(value[i]) {
            if(sel) {
                uint8_t v = (kind[i] == BbRowSpeed) ? app->set.speed : app->set.diff;
                uint8_t hi = (kind[i] == BbRowSpeed) ? BB_SPEED_COUNT - 1 : BB_DIFF_COUNT - 1;
                /* the daily's rows are the day's, so they offer no arrows */
                bool live = (kind[i] == BbRowTime || kind[i] == BbRowRamp ||
                             kind[i] == BbRowSpeed) &&
                            app->mode != BbModeDaily;
                bb_adjustable(c, mid, value[i], live && v > 0, live && v < hi);
            } else {
                bb_font(c, false);
                bb_str(c, BB_ROW_R, mid, AlignRight, AlignCenter, value[i]);
            }
        }
        canvas_set_color(c, ColorBlack);
    }
}

static void bb_draw_rule_card(Canvas* c, const BeepbackApp* app) {
    bb_title(c, "RULE");
    bb_rule_line(c, app, 64, 30, true, false);
    bb_font(c, false);
    bb_str(c, 64, 46, AlignCenter, AlignCenter,
           app->rule_idx >= 0 ? bb_rule_tip[app->rule_idx] : "");
}

static void bb_draw_detail(Canvas* c, const BeepbackApp* app) {
    static const char* const tag[BB_ASSIST_COUNT] = {"EAR", "LED", "SHP", "ARR"};
    const char* val[3];
    uint8_t at[3], hi[3];
    char buf[16];
    bb_title(c, "SCORES");
    val[0] = bb_mode_name[app->det_mode % BB_MODE_COUNT];
    at[0] = app->det_mode;
    hi[0] = 2;
    val[1] = bb_diff_name[app->det_time % BB_DIFF_COUNT];
    at[1] = app->det_time;
    hi[1] = 3;
    val[2] = bb_speed_name[app->det_speed % BB_SPEED_COUNT];
    at[2] = app->det_speed;
    hi[2] = 2;
    static const char* const rowname[3] = {"MODE", "TIME", "SPEED"};
    for(uint8_t i = 0; i < 3; i++) {
        int32_t top = 14 + i * 10;
        if(app->det_row == i) bb_sel_rbox(c, 2, top, 124, 10, 2);
        bb_font(c, false);
        bb_str(c, BB_ROW_L, top + 5, AlignLeft, AlignCenter, rowname[i]);
        if(app->det_row == i) {
            bb_adjustable(c, top + 5, val[i], at[i] > 0, at[i] < hi[i]);
        } else {
            bb_str(c, BB_ROW_R, top + 5, AlignRight, AlignCenter, val[i]);
        }
        canvas_set_color(c, ColorBlack);
    }
    const uint32_t* cell = app->rec.best[app->det_mode % BB_LADDER_MODES]
                                        [app->det_time % BB_DIFF_COUNT]
                                        [app->det_speed % BB_SPEED_COUNT];
    bb_font(c, false);
    for(uint8_t i = 0; i < BB_ASSIST_COUNT; i++) {
        bool left = (i % 2) == 0;
        int32_t y = 49 + (i < 2 ? 0 : 9);
        bb_str(c, left ? 6 : 68, y, AlignLeft, AlignCenter, tag[i]);
        snprintf(buf, sizeof(buf), "%lu", (unsigned long)cell[i]);
        bb_str(c, left ? 60 : 122, y, AlignRight, AlignCenter, buf);
    }
}

static void bb_draw_reset(Canvas* c, const BeepbackApp* app) {
    static const char* const rows[2] = {"SCORES", "TUTORIAL"};
    bb_title(c, "RESET");
    for(uint8_t i = 0; i < 2; i++) {
        int32_t top = 25 + i * 13;
        if(app->reset_idx == i) bb_sel_rbox(c, 2, top, 124, 13, 2);
        bb_font(c, false);
        bb_str(c, BB_ROW_L, top + 6, AlignLeft, AlignCenter, rows[i]);
        bb_str(c, BB_ROW_R, top + 6, AlignRight, AlignCenter,
               (i == 0 && app->now < app->set_flash) ? "CLEARED" : "OK");
        canvas_set_color(c, ColorBlack);
    }
}

static void bb_draw_settings(Canvas* c, const BeepbackApp* app) {
    static const char* const name[5] = {"VOLUME", "ASSIST", "SOUNDS", "SCORES", "RESET"};
    bool autoAssist = (app->set.volume == 0 && app->set.assist == 0);
    bb_title(c, "SETTINGS");
    for(uint8_t i = 0; i < 5; i++) {
        int32_t top = 14 + i * 10;
        bool sel = (app->set_idx == i);
        if(sel) bb_sel_rbox(c, 2, top, 124, 10, 2);
        bb_font(c, false);
        bb_str(c, BB_ROW_L, top + 5, AlignLeft, AlignCenter, name[i]);
        if(i <= 1 && sel) {
            uint8_t v = (i == 0) ? app->set.volume : app->set.assist;
            uint8_t hi = (i == 0) ? BB_VOL_COUNT - 1 : BB_ASSIST_COUNT - 1;
            const char* text = (i == 0) ? bb_vol_name[app->set.volume % BB_VOL_COUNT] :
                                          (autoAssist ? "SHAPES!" :
                                                        bb_assist_name[app->set.assist % BB_ASSIST_COUNT]);
            bb_adjustable(c, top + 5, text, v > 0, v < hi);
        } else if(i <= 1) {
            bb_str(c, BB_ROW_R, top + 5, AlignRight, AlignCenter,
                   (i == 0) ? bb_vol_name[app->set.volume % BB_VOL_COUNT] :
                              (autoAssist ? "SHAPES!" :
                                            bb_assist_name[app->set.assist % BB_ASSIST_COUNT]));
        } else {
            bb_chev_r(c, BB_ROW_AR, top + 5);
        }
        canvas_set_color(c, ColorBlack);
    }
}

/* ------------------------------------------------------------------ */
/* The guides                                                          */
/* ------------------------------------------------------------------ */

static const char* const TUT_PAGES[4][3] = {
    {"Every sound belongs", "to one button.", "Listen closely!"},
    {"Wait for GO! then", "press the buttons", "in the same order."},
    {"Wrong button or out", "of time costs one", "of your 3 lives."},
    {"A round grows step", "by step. Clear it for", "a fresh, longer one."},
};
static const char* const TUT_RULES[2][3] = {
    {"Same game as classic,", "but every round hands", "you a rule to obey."},
    {"It holds for the whole", "round and changes", "when the round clears."},
};
static const char* const TUT_REFLEX[2][3] = {
    {"One cue at a time.", "Hit the matching button", "before the bar empties."},
    {"The window shrinks with", "every hit. A miss ends", "the run, and so does BACK."},
};

static void bb_draw_help(Canvas* c, const BeepbackApp* app) {
    static const char* const blurb[3] = {"the basic game", "rules on top of it", "pure reaction"};
    bb_title(c, "HOW TO PLAY");
    for(uint8_t i = 0; i < 3; i++) {
        int32_t top = 14 + i * 13;
        if(app->help_idx == i) bb_sel_rbox(c, 12, top, 104, 12, 3);
        bb_font(c, true);
        bb_str(c, 64, top + 6, AlignCenter, AlignCenter, bb_mode_name[i]);
        canvas_set_color(c, ColorBlack);
    }
    bb_footer(c, blurb[app->help_idx < 3 ? app->help_idx : 0]);
}

static void bb_draw_pages(Canvas* c, const char* const page[3], uint8_t total, uint8_t at,
                          const char* last_label) {
    bb_font(c, false);
    for(uint8_t i = 0; i < 3; i++)
        if(page[i]) bb_str(c, 64, 21 + i * 11, AlignCenter, AlignCenter, page[i]);
    bb_footer_pages(c, total, at, at + 1 < total ? "OK" : last_label);
}

static void bb_draw_rules_guide(Canvas* c, const BeepbackApp* app) {
    uint8_t p = app->tut_page < 2 ? app->tut_page : 1;
    bb_title(c, "RULES");
    bb_draw_pages(c, TUT_RULES[p], 2, p, "THE RULES");
}

static void bb_draw_reflex_guide(Canvas* c, const BeepbackApp* app) {
    uint8_t p = app->tut_page < 2 ? app->tut_page : 1;
    bb_title(c, "REFLEX");
    bb_draw_pages(c, TUT_REFLEX[p], 2, p, "SOUNDS");
}

static void bb_draw_rule_list(Canvas* c, const BeepbackApp* app) {
    const uint8_t VIS = 4, rowH = 11;
    const int32_t top0 = 15;
    bb_title(c, "THE RULES");
    for(uint8_t i = 0; i < VIS; i++) {
        uint8_t idx = (uint8_t)(app->rule_scroll + i);
        if(idx >= BB_RULE_COUNT) break;
        int32_t top = top0 + i * rowH;
        if(idx == app->rule_sel) bb_sel_rbox(c, 2, top, 116, rowH, 2);
        bb_font(c, false);
        bb_str(c, BB_ROW_L, top + 5, AlignLeft, AlignCenter, bb_rule_label[idx]);
        if(idx == app->rule_sel) bb_chev_r(c, 104, top + 5);
        canvas_set_color(c, ColorBlack);
    }
    bb_scrollbar(c, top0, VIS * rowH, VIS, BB_RULE_COUNT, app->rule_scroll);
}

static void bb_draw_rule_info(Canvas* c, const BeepbackApp* app) {
    uint8_t r = app->rule_sel < BB_RULE_COUNT ? app->rule_sel : 0;
    bb_title(c, bb_rule_label[r]);
    bb_font(c, false);
    for(uint8_t i = 0; i < 3; i++)
        bb_str(c, 64, 21 + i * 11, AlignCenter, AlignCenter, bb_rule_help[r][i]);
    if(r > 0) bb_chev_u(c, 121, 20);
    if(r < BB_RULE_COUNT - 1) bb_chev_d(c, 121, 44);
    bb_footer(c, "up / down: next rule");
}

static void bb_draw_tutorial(Canvas* c, const BeepbackApp* app) {
    char buf[24];
    uint8_t extra = bb_visual_on(app) ? 1 : 0;
    uint8_t total = (uint8_t)(4 + extra);
    uint8_t p = app->tut_page < total ? app->tut_page : (uint8_t)(total - 1);
    static const uint8_t order[BbBtnCount] = {BbBtnDown, BbBtnLeft, BbBtnOk, BbBtnRight, BbBtnUp};

    bb_title(c, "HOW TO PLAY");
    if(p < 4) {
        bb_font(c, false);
        for(uint8_t i = 0; i < 3; i++)
            bb_str(c, 64, 21 + i * 11, AlignCenter, AlignCenter, TUT_PAGES[p][i]);
    } else if(bb_led_mode(app)) {
        bb_font(c, false);
        for(uint8_t b = 0; b < BbBtnCount; b++) {
            snprintf(buf, sizeof(buf), "%s = %s", bb_button_short[order[b]], bb_led_name[order[b]]);
            bb_str(c, 64, 16 + b * 8, AlignCenter, AlignCenter, buf);
        }
    } else {
        bb_font(c, false);
        bb_str(c, 64, 18, AlignCenter, AlignCenter,
               bb_shapes_on(app) ? "one shape per sound" : "one arrow per sound");
        for(uint8_t b = 0; b < BbBtnCount; b++) {
            bb_cue(c, app, 13 + b * 25, 32, order[b], 7, false);
            bb_font(c, false);
            bb_str(c, 13 + b * 25, 44, AlignCenter, AlignCenter, bb_button_short[order[b]]);
        }
    }
    bb_footer_pages(c, total, p, p + 1 < total ? "OK" : "SOUNDS");
}

static void bb_draw_sound_test(Canvas* c, const BeepbackApp* app) {
    bb_title(c, "TRY THE SOUNDS");
    if(app->test_btn < 0) {
        bb_font(c, false);
        bb_str(c, 64, 27, AlignCenter, AlignCenter, "Press any of the five");
        bb_str(c, 64, 39, AlignCenter, AlignCenter, "game buttons.");
    } else {
        /* a cursor is only as trustworthy as the code that moved it */
        uint8_t b = (uint8_t)(app->test_btn < BbBtnCount ? app->test_btn : BbBtnCount - 1);
        if(bb_cue_on(app)) {
            bb_cue(c, app, 30, 35, b, 13, false);
        } else {
            bb_glyph(c, 30, 35, b, 11);
        }
        bb_font(c, true);
        bb_str(c, 88, 35, AlignCenter, AlignCenter, bb_button_name[b]);
    }
    bb_footer(c, "back to leave");
}

/* ------------------------------------------------------------------ */
/* In the game                                                         */
/* ------------------------------------------------------------------ */

static void bb_hud(Canvas* c, const BeepbackApp* app) {
    char buf[24];
    bb_font(c, false);
    if(bb_is_challenge_mode(app->mode)) {
        snprintf(buf, sizeof(buf), "LEN %u", app->stage);
        bb_str(c, 2, 1, AlignLeft, AlignTop, buf);
        snprintf(buf, sizeof(buf), "%lu", (unsigned long)app->score);
        bb_str(c, 64, 1, AlignCenter, AlignTop, buf);
    } else {
        snprintf(buf, sizeof(buf), "R%u", app->round);
        bb_str(c, 2, 1, AlignLeft, AlignTop, buf);
        snprintf(buf, sizeof(buf), "%u/%u", app->stage, app->target);
        bb_str(c, 64, 1, AlignCenter, AlignTop, buf);
    }
    bb_lives(c, 103, 1, app->lives, 1);
}

static void bb_bar(Canvas* c, uint32_t left, uint32_t total) {
    canvas_draw_frame(c, 0, 52, BB_W, 11);
    if(!total) return;
    if(left > total) left = total;
    int32_t w = (int32_t)((uint32_t)(BB_W - 4) * left / total);
    if(w > 0) canvas_draw_box(c, 2, 54, (size_t)w, 7);
}

static void bb_draw_listen(Canvas* c, const BeepbackApp* app) {
    bb_hud(c, app);
    bb_font(c, true);
    bb_str(c, 64, 30, AlignCenter, AlignCenter, bb_visual_on(app) ? "WATCH!" : "LISTEN!");
    if(app->mode == BbModeRules) {
        bb_rule_line(c, app, 64, 46, false, false);
    } else {
        bb_font(c, false);
        bb_str(c, 64, 46, AlignCenter, AlignCenter, "here it comes...");
    }
}

static void bb_draw_playback(Canvas* c, const BeepbackApp* app) {
    bb_hud(c, app);
    uint8_t done = (uint8_t)(app->play_idx + 1);
    if(bb_cue_on(app)) {
        if(app->tone_on)
            bb_cue(c, app, 64, 33, app->base.step[app->play_idx], BB_SHAPE_R,
                   app->now < app->step_start + BB_FLASH_MS);
        bb_steps(c, 58, app->stage, done);
    } else {
        bb_font(c, false);
        bb_str(c, 64, 16, AlignCenter, AlignCenter, "LISTEN");
        if(app->tone_on) {
            canvas_draw_disc(c, 64, 34, 10);
        } else {
            canvas_draw_circle(c, 64, 34, 10);
            canvas_draw_circle(c, 64, 34, 6);
        }
        bb_steps(c, 55, app->stage, done);
    }
}

static void bb_draw_go(Canvas* c, const BeepbackApp* app) {
    bb_hud(c, app);
    canvas_draw_rframe(c, 24, 20, 80, 28, 4);
    bb_font(c, true);
    bb_str(c, 64, 34, AlignCenter, AlignCenter, "GO!");
    if(app->mode == BbModeRules) bb_rule_line(c, app, 64, 56, false, false);
}

static void bb_draw_input(Canvas* c, const BeepbackApp* app) {
    bb_hud(c, app);
    bb_steps(c, 18, app->expected.len, app->input_idx);
    if(app->last_press >= 0 && app->now < app->press_flash) {
        if(bb_cue_on(app)) {
            bb_cue(c, app, 64, 36, (uint8_t)app->last_press, 13, false);
        } else {
            bb_glyph(c, 64, 36, (uint8_t)app->last_press, 10);
        }
    }
    uint32_t total = app->input_end - app->input_start;
    uint32_t left = app->input_end > app->now ? app->input_end - app->now : 0;
    bb_bar(c, left, total);
}

static void bb_draw_reflex(Canvas* c, const BeepbackApp* app) {
    char buf[24];
    bb_font(c, false);
    snprintf(buf, sizeof(buf), "HITS %lu", (unsigned long)app->rx_hits);
    bb_str(c, 2, 1, AlignLeft, AlignTop, buf);
    snprintf(buf, sizeof(buf), "%lu", (unsigned long)app->score);
    bb_str(c, 64, 1, AlignCenter, AlignTop, buf);
    snprintf(buf, sizeof(buf), "%uMS", app->rx_window);
    bb_str(c, 126, 1, AlignRight, AlignTop, buf);

    if(app->scene == BbSceneReflexCue) {
        if(bb_cue_on(app)) {
            bb_cue(c, app, 64, 32, app->rx_cue, BB_SHAPE_R, app->now < app->rx_at + BB_FLASH_MS);
        } else if(!bb_led_mode(app)) {
            canvas_draw_disc(c, 64, 32, 10); /* by ear: marks that a cue is live */
        }
        uint32_t left = app->phase > app->now ? app->phase - app->now : 0;
        bb_bar(c, left, app->rx_window);
    } else {
        bb_font(c, true);
        bb_str(c, 64, 32, AlignCenter, AlignCenter, "...");
        /* said once at the start of a run, then it gets out of the way */
        if(app->rx_hits == 0) bb_footer(c, "no pausing: back ends it");
    }
}

static void bb_draw_success(Canvas* c, const BeepbackApp* app) {
    char buf[16];
    bb_hud(c, app);
    bb_font(c, true);
    bb_str(c, 64, 30, AlignCenter, AlignCenter, bb_praise[app->praise % BB_PRAISE_COUNT]);
    bb_font(c, false);
    snprintf(buf, sizeof(buf), "+%u", BB_NOTE_POINTS * app->expected.len);
    bb_str(c, 64, 47, AlignCenter, AlignCenter, buf);
}

static void bb_draw_round_clear(Canvas* c, const BeepbackApp* app) {
    char buf[24];
    bb_hud(c, app);
    bb_font(c, true);
    snprintf(buf, sizeof(buf), "ROUND %u CLEAR", app->round);
    bb_str(c, 64, 28, AlignCenter, AlignCenter, buf);
    bb_font(c, false);
    snprintf(buf, sizeof(buf), "+%u", BB_ROUND_BONUS);
    bb_str(c, 64, 45, AlignCenter, AlignCenter, buf);
}

static void bb_draw_wrong(Canvas* c, const BeepbackApp* app) {
    bb_font(c, true);
    bb_str(c, 64, 16, AlignCenter, AlignCenter, "WRONG!");
    bb_lives(c, 41, 30, app->lives, 2);
}

static void bb_draw_retry(Canvas* c, const BeepbackApp* app) {
    bb_hud(c, app);
    bb_font(c, true);
    bb_str(c, 64, 34, AlignCenter, AlignCenter, "TRY AGAIN");
}

static void bb_draw_pause(Canvas* c, const BeepbackApp* app) {
    char buf[24];
    canvas_set_color(c, ColorBlack);
    canvas_draw_box(c, 0, 0, BB_W, BB_H);
    canvas_set_color(c, ColorWhite);
    bb_font(c, true);
    bb_str(c, 64, 9, AlignCenter, AlignCenter, "PAUSED");
    canvas_draw_box(c, 8, 17, 112, 1);

    bb_font(c, false);
    if(bb_is_challenge_mode(app->mode)) {
        snprintf(buf, sizeof(buf), "LEN %u", app->stage);
        bb_str(c, 8, 26, AlignLeft, AlignCenter, buf);
        snprintf(buf, sizeof(buf), "%lu", (unsigned long)app->score);
        bb_str(c, 64, 26, AlignCenter, AlignCenter, buf);
    } else {
        snprintf(buf, sizeof(buf), "ROUND %u", app->round);
        bb_str(c, 8, 26, AlignLeft, AlignCenter, buf);
        snprintf(buf, sizeof(buf), "%u/%u", app->stage, app->target);
        bb_str(c, 64, 26, AlignCenter, AlignCenter, buf);
    }
    bb_lives(c, 96, 22, app->lives, 1);

    if(bb_is_challenge_mode(app->mode)) {
        bb_str(c, 64, 34, AlignCenter, AlignCenter, bb_mode_name[app->mode % BB_MODE_COUNT]);
        bb_rule_line(c, app, 64, 44, false, true); /* clear of the footer at 51 */
    } else if(app->mode == BbModeRules) {
        bb_rule_line(c, app, 64, 39, false, true);
    } else {
        bb_str(c, 64, 39, AlignCenter, AlignCenter, bb_mode_name[app->mode % BB_MODE_COUNT]);
    }

    bb_font(c, false);
    bb_str(c, 6, 55, AlignLeft, AlignCenter, "OK: resume");
    bb_str(c, 122, 55, AlignRight, AlignCenter, "BACK: quit");
    canvas_set_color(c, ColorBlack);
}

static void bb_draw_game_over(Canvas* c, const BeepbackApp* app) {
    char va[24], vb[24], vc[24], vd[24];
    const char* la[4];
    const char* lb[4];
    uint8_t n = 0;
    bool rx = (app->run_game_mode == BbModeReflex);

    bb_title(c, app->new_best ? "NEW BEST!" : "GAME OVER");
    bb_font(c, false);

    if(app->go_page) {
        /* what you played on: the numbers above belong to this setup */
        la[0] = "MODE";
        lb[0] = bb_mode_name[app->run_game_mode % BB_MODE_COUNT];
        la[1] = "ASSIST";
        lb[1] = bb_assist_name[app->run_mode % BB_ASSIST_COUNT];
        la[2] = rx ? "RAMP" : "TIME";
        lb[2] = bb_diff_name[app->run_diff % BB_DIFF_COUNT];
        la[3] = "SPEED";
        lb[3] = bb_speed_name[app->run_speed % BB_SPEED_COUNT];
        for(uint8_t i = 0; i < 4; i++) {
            int32_t y = 18 + i * 10;
            bb_str(c, BB_ROW_L, y, AlignLeft, AlignCenter, la[i]);
            bb_str(c, BB_ROW_R, y, AlignRight, AlignCenter, lb[i]);
        }
        bb_chev_l(c, BB_CHEV_L, 33);
    } else {
        snprintf(va, sizeof(va), "%lu", (unsigned long)app->score);
        la[n] = "SCORE";
        lb[n++] = va;
        snprintf(vb, sizeof(vb), "%lu", (unsigned long)(rx ? app->rx_hits : app->run_best));
        la[n] = rx ? "HITS" : "LONGEST";
        lb[n++] = vb;
        if(rx) {
            snprintf(vc, sizeof(vc), "%luMS", (unsigned long)app->run_best);
            la[n] = "FASTEST";
            lb[n++] = vc;
            snprintf(vd, sizeof(vd), "%lu", (unsigned long)app->prev_best);
            la[n] = app->new_best ? "PREVIOUS BEST" : "BEST";
            lb[n++] = vd;
        } else {
            snprintf(vc, sizeof(vc), "%lu", (unsigned long)app->prev_best);
            la[n] = app->new_best ? "PREVIOUS BEST" : "BEST";
            lb[n++] = vc;
        }
        int32_t y0 = (n == 4) ? 18 : 21, step = (n == 4) ? 10 : 11;
        for(uint8_t i = 0; i < n; i++) {
            bb_str(c, BB_ROW_L, y0 + i * step, AlignLeft, AlignCenter, la[i]);
            bb_str(c, BB_ROW_R, y0 + i * step, AlignRight, AlignCenter, lb[i]);
        }
        bb_chev_r(c, BB_CHEV_R, 33);
    }
    bb_footer(c, "OK: again    BACK: menu");

    /* Wipes in top to bottom, the move the intro ends on. The prompt is
       the last thing to arrive, which is when input goes live. */
    if(app->now < app->lock_until) {
        uint32_t left = app->lock_until - app->now;
        uint32_t cut = BB_H - (BB_H * left / BB_OVER_LOCK);
        canvas_set_color(c, ColorWhite);
        canvas_draw_box(c, 0, (int32_t)cut, BB_W, (size_t)(BB_H - cut));
        canvas_set_color(c, ColorBlack);
    }
}

/* ------------------------------------------------------------------ */

void bb_draw(Canvas* canvas, BeepbackApp* app) {
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    bb_font(canvas, false);

    switch(app->scene) {
    case BbSceneSplash:
        bb_draw_splash(canvas, app);
        break;
    case BbSceneMenu:
        bb_draw_menu(canvas, app);
        break;
    case BbSceneMode:
        bb_draw_mode(canvas, app);
        break;
    case BbSceneChPick:
        bb_draw_ch_pick(canvas, app);
        break;
    case BbSceneSetup:
        bb_draw_setup(canvas, app);
        break;
    case BbSceneRuleCard:
        bb_draw_rule_card(canvas, app);
        break;
    case BbSceneListen:
        bb_draw_listen(canvas, app);
        break;
    case BbScenePlayback:
        bb_draw_playback(canvas, app);
        break;
    case BbSceneGo:
        bb_draw_go(canvas, app);
        break;
    case BbSceneInput:
    case BbSceneHold:
        bb_draw_input(canvas, app);
        break;
    case BbSceneSuccess:
        bb_draw_success(canvas, app);
        break;
    case BbSceneRoundClear:
        bb_draw_round_clear(canvas, app);
        break;
    case BbSceneWrong:
        bb_draw_wrong(canvas, app);
        break;
    case BbSceneRetry:
        bb_draw_retry(canvas, app);
        break;
    case BbSceneReflexGap:
    case BbSceneReflexCue:
        bb_draw_reflex(canvas, app);
        break;
    case BbSceneGameOver:
        bb_draw_game_over(canvas, app);
        break;
    case BbSceneSettings:
        bb_draw_settings(canvas, app);
        break;
    case BbSceneDetail:
        bb_draw_detail(canvas, app);
        break;
    case BbSceneReset:
        bb_draw_reset(canvas, app);
        break;
    case BbSceneHelp:
        bb_draw_help(canvas, app);
        break;
    case BbSceneTutorial:
        bb_draw_tutorial(canvas, app);
        break;
    case BbSceneRulesGuide:
        bb_draw_rules_guide(canvas, app);
        break;
    case BbSceneRuleList:
        bb_draw_rule_list(canvas, app);
        break;
    case BbSceneRuleInfo:
        bb_draw_rule_info(canvas, app);
        break;
    case BbSceneReflexGuide:
        bb_draw_reflex_guide(canvas, app);
        break;
    case BbSceneSoundTest:
        bb_draw_sound_test(canvas, app);
        break;
    case BbSceneScorePick:
        bb_draw_score_pick(canvas, app);
        break;
    case BbSceneScores:
        bb_draw_scores(canvas, app);
        break;
    case BbSceneCredits:
        bb_draw_credits(canvas);
        break;
    default:
        bb_draw_menu(canvas, app);
        break;
    }

    if(app->paused) bb_draw_pause(canvas, app);
}

void bb_shape_flash(Canvas* c, int32_t cx, int32_t cy, int32_t r, uint8_t kind, bool invert) {
    bb_shape(c, cx, cy, r, kind, invert);
}

/* the splash's last phase wipes over whatever comes next */
void bb_draw_under_wipe(Canvas* c, BeepbackApp* app) {
    if(app->first_run) {
        bb_draw_tutorial(c, app);
    } else {
        bb_draw_menu(c, app);
    }
}
