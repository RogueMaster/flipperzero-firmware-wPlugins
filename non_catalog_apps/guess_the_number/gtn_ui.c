/*
 * Everything you see on screen. The display is 128x64, 1-bit, and gets
 * redrawn from scratch every frame. Animations don't keep their own state,
 * they are worked out from how long a screen has been open, so they can't
 * get out of sync with the game.
 */

#include "gtn.h"
#include "gtn_qr.h"

#include <gui/elements.h>
#include <stdio.h>
#include <string.h>

#define SCREEN_W 128
#define SCREEN_H 64

/* ---------- tiny math / randomness for animations ---------- */

/* A rough sine wave, good enough for bobbing and wobbling things. */
static const int8_t sin_tab[16] = {0, 3, 6, 7, 8, 7, 6, 3, 0, -3, -6, -7, -8, -7, -6, -3};

/* Hash-style "random": the same input always gives the same number, so
 * the confetti and the number rain look random but don't flicker. */
static uint32_t prand(uint32_t x) {
    x ^= x >> 16;
    x *= 0x7feb352dU;
    x ^= x >> 15;
    x *= 0x846ca68bU;
    x ^= x >> 16;
    return x;
}

static int32_t abs_i32(int32_t v) {
    return v < 0 ? -v : v;
}

/* 0..256 ease-out progress of `t` over `dur` */
/* Fast start, soft landing. Returns 0..256 instead of floats. */
static int32_t ease_out(uint32_t t, uint32_t dur) {
    if(t >= dur) return 256;
    int32_t p = (int32_t)(t * 256 / dur);
    int32_t inv = 256 - p;
    return 256 - (inv * inv) / 256;
}

static const int8_t shake_tab[8] = {3, -3, 2, -2, 1, -1, 1, 0};

/* Short horizontal shake after a wrong guess or an error. */
static int32_t shake(uint32_t since, uint32_t now) {
    if(since == 0) return 0;
    uint32_t t = now - since;
    if(t >= 320) return 0;
    return shake_tab[t / 40];
}

/* ---------- drawing helpers ---------- */

static void title_bar(Canvas* canvas, const char* title, int32_t dx) {
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_box(canvas, 0, 0, SCREEN_W, 13);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, SCREEN_W / 2 + dx, 2, AlignCenter, AlignTop, title);
    canvas_set_color(canvas, ColorBlack);
}

/* Word-wrap `text` into `width` px. '\n' forces a line break.
 * Draws lines [first, first + max_lines) with baselines starting at y.
 * Returns the total number of lines. */
/* The firmware has no word wrap that fits here, so this is a small one.
 * It can also skip lines, which is how the long texts scroll. */
static size_t wrap_text(
    Canvas* canvas,
    const char* text,
    int32_t x,
    int32_t y,
    int32_t width,
    int32_t line_h,
    size_t first,
    size_t max_lines,
    bool center) {
    char line[64] = {0};
    char cand[64];
    size_t n = 0;
    const char* p = text;

#define EMIT()                                                                  \
    do {                                                                        \
        if(n >= first && n < first + max_lines) {                               \
            int32_t ly = y + (int32_t)(n - first) * line_h;                     \
            if(center)                                                          \
                canvas_draw_str_aligned(                                        \
                    canvas, x + width / 2, ly, AlignCenter, AlignBottom, line); \
            else                                                                \
                canvas_draw_str(canvas, x, ly, line);                           \
        }                                                                       \
        n++;                                                                    \
        line[0] = '\0';                                                         \
    } while(0)

    while(*p) {
        if(*p == '\n') {
            if(line[0]) EMIT();
            p++;
            continue;
        }
        if(*p == ' ') {
            p++;
            continue;
        }
        const char* w = p;
        while(*p && *p != ' ' && *p != '\n')
            p++;
        size_t wl = (size_t)(p - w);
        if(wl > sizeof(line) - 1) wl = sizeof(line) - 1;
        size_t ll = strlen(line);
        size_t pos = 0;
        if(ll && ll + 1 + wl < sizeof(cand)) {
            memcpy(cand, line, ll);
            cand[ll] = ' ';
            pos = ll + 1;
        }
        memcpy(cand + pos, w, wl);
        cand[pos + wl] = '\0';
        if(line[0] && (pos == 0 || canvas_string_width(canvas, cand) > width)) {
            EMIT();
            memcpy(line, w, wl);
            line[wl] = '\0';
        } else {
            memcpy(line, cand, sizeof(line));
        }
    }
    if(line[0]) EMIT();
#undef EMIT
    return n;
}

static void tri_up(Canvas* canvas, int32_t x, int32_t y) {
    for(int i = 0; i < 4; i++)
        canvas_draw_line(canvas, x - i, y + i, x + i, y + i);
}

static void tri_down(Canvas* canvas, int32_t x, int32_t y) {
    for(int i = 0; i < 4; i++)
        canvas_draw_line(canvas, x - i, y - i, x + i, y - i);
}

static void star(Canvas* canvas, int32_t x, int32_t y, bool big) {
    int32_t r = big ? 3 : 2;
    canvas_draw_line(canvas, x - r, y, x + r, y);
    canvas_draw_line(canvas, x, y - r, x, y + r);
    if(big) {
        canvas_draw_dot(canvas, x - 1, y - 1);
        canvas_draw_dot(canvas, x + 1, y - 1);
        canvas_draw_dot(canvas, x - 1, y + 1);
        canvas_draw_dot(canvas, x + 1, y + 1);
    }
}

static void header_row(Canvas* canvas, const char* left, const char* right) {
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 8, left);
    canvas_draw_str_aligned(canvas, SCREEN_W - 2, 8, AlignRight, AlignBottom, right);
    canvas_draw_line(canvas, 0, 10, SCREEN_W - 1, 10);
}

/* returns the drawn width */
static uint16_t
    big_number(Canvas* canvas, int32_t cx, int32_t baseline, int32_t value, bool question) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%ld", (long)value);
    canvas_set_font(canvas, FontBigNumbers);
    uint16_t w = canvas_string_width(canvas, buf);
    int32_t x = cx - (int32_t)w / 2;
    canvas_draw_str(canvas, x, baseline, buf);
    if(question) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, x + w + 2, baseline, "?");
    }
    return w;
}

static void dots(char* out, size_t size, const char* text, uint32_t t) {
    snprintf(out, size, "%s%.*s", text, (int)((t / 250) % 4), "...");
}

/* ---------- screens ---------- */

static void draw_splash(Canvas* canvas, uint32_t t) {
    static const char glyphs[] = "?0123456789?";
    /* number rain */
    canvas_set_font(canvas, FontSecondary);
    for(uint32_t i = 0; i < 10; i++) {
        uint32_t speed = 35 + prand(i) % 45;
        uint32_t travel = prand(i + 50) % 80 + t * speed / 1000;
        int32_t y = (int32_t)(travel % 84) - 10;
        char ch[2] = {glyphs[prand(i * 7 + travel / 84) % 12], 0};
        if(y < 7) continue; /* no negative coordinates */
        canvas_draw_str(canvas, 4 + i * 12, y, ch);
    }

    /* title card drops in */
    int32_t box_y = -26 + (44 * ease_out(t, 550)) / 256;
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_box(canvas, 6, box_y - 2, 116, 26);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_rframe(canvas, 8, box_y, 112, 22, 3);
    canvas_draw_rframe(canvas, 9, box_y + 1, 110, 20, 2);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(
        canvas, SCREEN_W / 2, box_y + 15, AlignCenter, AlignBottom, "Guess The Number");

    /* typed byline */
    if(t > 650) {
        static const char byline[] = "by Prevo128";
        size_t n = (t - 650) / 55;
        if(n > sizeof(byline) - 1) n = sizeof(byline) - 1;
        char buf[16];
        snprintf(buf, sizeof(buf), "%.*s%s", (int)n, byline, ((t / 200) % 2) ? "_" : " ");
        canvas_set_font(canvas, FontSecondary);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_box(canvas, 34, 45, 60, 11);
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_str(canvas, 40, 54, buf);
    }

    /* finale: sparkles around the title card */
    if(t > 1350) {
        static const uint8_t sx[6] = {12, 30, 64, 98, 116, 64};
        static const uint8_t sy[6] = {14, 44, 14, 44, 14, 44};
        for(uint32_t i = 0; i < 6; i++) {
            uint32_t phase = (t / 130 + i * 2) % 6;
            if(phase < 3) {
                canvas_set_color(canvas, ColorWhite);
                canvas_draw_box(canvas, sx[i] - 4, sy[i] - 4, 9, 9);
                canvas_set_color(canvas, ColorBlack);
                star(canvas, sx[i], sy[i], phase == 1);
            }
        }
    }
}

static void draw_menu(Canvas* canvas, GtnState* s, uint32_t now) {
    static const char* items[GTN_MENU_ITEMS] = {
        "Be the guesser", "Let the computer guess", "Stats & Hall of Fame", "About", "Exit"};
    title_bar(canvas, "Guess The Number", 0);

    /* bobbing question marks in the title bar */
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontPrimary);
    int32_t bob = sin_tab[(now / 90) % 16] / 4;
    canvas_draw_str(canvas, 3, 11 + bob, "?");
    canvas_draw_str(canvas, SCREEN_W - 8, 11 - bob, "?");
    canvas_set_color(canvas, ColorBlack);

    /* sliding highlight */
    int32_t from = 14 + s->menu_prev * 10;
    int32_t to = 14 + s->menu_index * 10;
    int32_t p = ease_out(now - s->menu_changed_at, 140);
    int32_t hy = from + ((to - from) * p) / 256;
    elements_slightly_rounded_box(canvas, 2, hy, SCREEN_W - 4, 10);

    canvas_set_color(canvas, ColorXOR);
    canvas_set_font(canvas, FontSecondary);
    for(uint8_t i = 0; i < GTN_MENU_ITEMS; i++) {
        canvas_draw_str(canvas, 8, 14 + i * 10 + 8, items[i]);
    }
    int32_t arrow = (p >= 256) ? abs_i32(sin_tab[(now / 70) % 16]) / 4 : 0;
    canvas_draw_str(canvas, SCREEN_W - 11 + arrow, hy + 8, ">");
    canvas_set_color(canvas, ColorBlack);
}

static void draw_range_row(
    Canvas* canvas,
    int32_t top,
    const char* label,
    int32_t value,
    bool sel,
    int32_t push) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%ld", (long)value);
    if(sel) {
        elements_slightly_rounded_box(canvas, 2, top, SCREEN_W - 4, 13);
        canvas_set_color(canvas, ColorWhite);
    }
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 8, top + 10, label);
    canvas_set_font(canvas, FontKeyboard);
    canvas_draw_str_aligned(canvas, 88, top + 10, AlignCenter, AlignBottom, buf);
    if(sel) {
        int32_t l = push < 0 ? -2 : 0;
        int32_t r = push > 0 ? 2 : 0;
        canvas_draw_str_aligned(canvas, 62 + l, top + 10, AlignCenter, AlignBottom, "<");
        canvas_draw_str_aligned(canvas, 114 + r, top + 10, AlignCenter, AlignBottom, ">");
    }
    canvas_set_color(canvas, ColorBlack);
}

static void draw_range(Canvas* canvas, GtnState* s, uint32_t now) {
    title_bar(canvas, s->mode == GtnModePlayer ? "You guess" : "I guess", 0);
    int32_t push = (now - s->nudge_at < 120) ? s->nudge : 0;
    draw_range_row(canvas, 15, "Min", s->floor, s->range_row == 0, push);
    draw_range_row(canvas, 29, "Max", s->ceiling, s->range_row == 1, push);
    canvas_set_font(canvas, FontSecondary);
    if(s->range_error) {
        int32_t dx = shake(s->nudge_at, now);
        canvas_draw_str_aligned(
            canvas, SCREEN_W / 2 + dx, 51, AlignCenter, AlignBottom, "Min must be <= Max!");
    } else {
        canvas_draw_str_aligned(
            canvas, SCREEN_W / 2, 51, AlignCenter, AlignBottom, "Hold < > to go fast");
    }
    elements_button_center(canvas, "Start");
}

/* slot machine style number roller inside a frame */
/* Slot machine style reel used while the secret is drawn and while the
 * computer is thinking. It only pretends to roll, the number is decided elsewhere. */
static void roller(Canvas* canvas, int32_t cy, int32_t lo, int32_t hi, uint32_t bucket) {
    canvas_draw_rframe(canvas, 30, cy - 12, 68, 24, 3);
    /* reel ticks */
    for(int32_t y = cy - 9; y <= cy + 9; y += 3) {
        canvas_draw_dot(canvas, 33, y);
        canvas_draw_dot(canvas, 94, y);
    }
    int32_t span = hi - lo + 1;
    int32_t v = lo + (int32_t)(prand(bucket) % (uint32_t)(span > 0 ? span : 1));
    /* the reel moves: number slides a bit inside the frame */
    int32_t slide = (int32_t)(bucket % 3) - 1;
    big_number(canvas, SCREEN_W / 2, cy + 7 + slide, v, false);
}

static void draw_starting(Canvas* canvas, GtnState* s, uint32_t now) {
    uint32_t t = now - s->screen_since;
    if(!s->started) {
        char buf[24];
        dots(buf, sizeof(buf), "Starting game", t);
        title_bar(canvas, buf, 0);
        /* reel spins fast, then slows down */
        uint32_t bucket = t < 450 ? t / 45 : 10 + (t - 450) / 110;
        roller(canvas, 28, s->floor, s->ceiling, bucket + s->secret * 31);
        canvas_set_font(canvas, FontSecondary);
        wrap_text(canvas, s->msg, 2, 52, SCREEN_W - 4, 9, 0, 2, true);
    } else {
        char body[80];
        snprintf(
            body,
            sizeof(body),
            "Guess the number between %ld and %ld I came up with!",
            (long)s->floor,
            (long)s->ceiling);
        title_bar(canvas, "Guess The Number", 0);
        canvas_set_font(canvas, FontPrimary);
        /* "Game started!" pops in */
        int32_t dy = (t < 600) ? (8 * (256 - ease_out(t, 300))) / 256 : 0;
        canvas_draw_str_aligned(
            canvas, SCREEN_W / 2, 25 + dy, AlignCenter, AlignBottom, "Game started!");
        if((t / 250) % 2 == 0 || t > 1000) {
            star(canvas, 18, 20 + dy, true);
            star(canvas, 110, 20 + dy, true);
        }
        canvas_set_font(canvas, FontSecondary);
        wrap_text(canvas, body, 2, 36, SCREEN_W - 4, 9, 0, 2, true);
        elements_button_center(canvas, "Go");
    }
}

static void draw_player(Canvas* canvas, GtnState* s, uint32_t now) {
    char left[24];
    char right[16];
    snprintf(left, sizeof(left), "Range %ld-%ld", (long)s->floor, (long)s->ceiling);
    snprintf(right, sizeof(right), "Tries: %lu", (unsigned long)s->tries);
    header_row(canvas, left, right);

    int32_t dx = shake(s->wrong_at, now);
    uint16_t w = big_number(canvas, SCREEN_W / 2 + dx, 30, s->guess, false);

    /* spinner; the arrow pointing the right way bounces */
    int32_t sx = SCREEN_W / 2 - (int32_t)w / 2 - 9 + dx;
    int32_t bounce = ((now / 180) % 2) ? 2 : 0;
    tri_up(canvas, sx, 15 - (s->hint_dir > 0 ? bounce : 0));
    tri_down(canvas, sx, 27 + (s->hint_dir < 0 ? bounce : 0));

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 98, 23, "+/-1");
    canvas_draw_str(canvas, 98, 31, "</>10");

    wrap_text(canvas, s->msg, 2, 41, SCREEN_W - 4, 9, 0, 2, true);
    elements_button_center(canvas, "Guess");
}

/* Two states: "Hmm..." with the reel and a progress bar while thinking,
 * then the guess with a small pop and the three answer buttons. */
static void draw_computer(Canvas* canvas, GtnState* s, uint32_t now) {
    uint32_t t = now - s->screen_since;
    char right[16];
    snprintf(
        right,
        sizeof(right),
        "Try %lu/%lu",
        (unsigned long)(s->thinking ? s->tries + 1 : s->tries),
        (unsigned long)s->max_tries);

    if(s->thinking) {
        char left[16];
        dots(left, sizeof(left), "Hmm", t);
        header_row(canvas, left, right);
        int32_t lo = s->lower <= s->upper ? s->lower : s->floor;
        int32_t hi = s->lower <= s->upper ? s->upper : s->ceiling;
        roller(canvas, 23, lo, hi, t / 70 + 977);
        canvas_set_font(canvas, FontSecondary);
        wrap_text(canvas, s->msg, 2, 44, SCREEN_W - 4, 9, 0, 2, true);
        /* "brain loading" bar */
        int32_t p = (int32_t)(t * 100 / (s->think_ms ? s->think_ms : 1));
        if(p > 100) p = 100;
        canvas_draw_rframe(canvas, 14, 57, 100, 6, 2);
        canvas_draw_box(canvas, 16, 59, (size_t)(96 * p / 100), 2);
        return;
    }

    header_row(canvas, "Is it...", right);
    uint16_t w = big_number(canvas, SCREEN_W / 2, 30, s->guess, true);

    /* reveal pop: a frame bursts out of the number */
    uint32_t since_reveal = t - s->think_ms;
    if(since_reveal < 200) {
        int32_t m = 1 + (int32_t)since_reveal / 80;
        int32_t bw = (int32_t)w + 12;
        canvas_draw_rframe(canvas, SCREEN_W / 2 - bw / 2 - m, 15 - m, bw + 2 * m, 15 + 2 * m, 3);
    }

    canvas_set_font(canvas, FontSecondary);
    wrap_text(canvas, s->msg, 2, 41, SCREEN_W - 4, 9, 0, 2, true);

    elements_button_left(canvas, "Lower");
    elements_button_center(canvas, "Yes!");
    elements_button_right(canvas, "Higher");
}

/* The 2 second victory screen (the time.sleep(2) from the Python version). */
static void draw_celebrate(Canvas* canvas, GtnState* s, uint32_t now) {
    uint32_t t = now - s->screen_since;

    /* confetti */
    for(uint32_t i = 0; i < 26; i++) {
        uint32_t speed = 25 + prand(i + 200) % 55;
        int32_t y = (int32_t)((prand(i + 100) % 64 + t * speed / 1000) % 72) - 6;
        int32_t x = 3 + (int32_t)(prand(i) % 120) + sin_tab[(t / 90 + i) % 16] / 3;
        if(y < 3 || y > SCREEN_H - 4) continue; /* keep coordinates on screen */
        switch(i % 3) {
        case 0:
            canvas_draw_box(canvas, x, y, 2, 2);
            break;
        case 1:
            star(canvas, x, y, false);
            break;
        default:
            canvas_draw_line(canvas, x, y, x + 2, y + 2);
            break;
        }
    }

    /* card */
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_box(canvas, 8, 9, 112, 44);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_rframe(canvas, 8, 9, 112, 44, 4);
    canvas_draw_rframe(canvas, 9, 10, 110, 42, 3);

    /* cheer slides open letter by letter */
    size_t n = t / 45;
    size_t len = strlen(s->cheer);
    if(n > len) n = len;
    char buf[24];
    snprintf(buf, sizeof(buf), "%.*s", (int)n, s->cheer);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, SCREEN_W / 2, 22, AlignCenter, AlignBottom, buf);

    /* bouncing number */
    int32_t bob = abs_i32(sin_tab[(t / 50) % 16]) / 3;
    big_number(canvas, SCREEN_W / 2, 45 - bob, s->cheer_value, false);
    bool blink = (t / 200) % 2;
    star(canvas, 28, 36, blink);
    star(canvas, 100, 36, !blink);
}

static void draw_text(Canvas* canvas, GtnState* s, uint32_t now) {
    int32_t dx = s->title_shake ? shake(s->screen_since, now) * 2 : 0;
    title_bar(canvas, s->title, dx);
    canvas_set_font(canvas, FontSecondary);
    const size_t visible = 4;
    size_t total = wrap_text(canvas, s->body, 3, 22, SCREEN_W - 10, 9, s->scroll, visible, false);
    s->text_lines = total;
    if(total > visible) {
        elements_scrollbar_pos(canvas, SCREEN_W - 2, 15, 36, s->scroll, total - visible + 1);
    }
    elements_button_center(canvas, s->button);
}

/* The QR code is drawn at 2x so phones can actually scan it. It builds
 * up row by row, then a scan line passes over it once. */
static void draw_about(Canvas* canvas, GtnState* s, uint32_t now) {
    uint32_t t = now - s->screen_since;
    const int32_t qx = 2, qy = 2, scale = 2;

    /* QR code builds up row by row, then a scan line sweeps once */
    uint32_t rows_shown = t / 16;
    for(uint32_t y = 0; y < GTN_QR_SIZE; y++) {
        if(y >= rows_shown) break;
        for(uint32_t x = 0; x < GTN_QR_SIZE; x++) {
            if(gtn_qr_rows[y] & (1UL << x)) {
                canvas_draw_box(canvas, qx + x * scale, qy + y * scale, scale, scale);
            }
        }
    }
    uint32_t scan_t = t > 400 ? t - 400 : 0;
    if(t > 400 && scan_t < 700) {
        int32_t ly = qy + (int32_t)(scan_t * (GTN_QR_SIZE * scale) / 700);
        canvas_set_color(canvas, ColorXOR);
        canvas_draw_line(canvas, qx - 1, ly, qx + GTN_QR_SIZE * scale, ly);
        canvas_set_color(canvas, ColorBlack);
    }

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 57, 10, "About");
    canvas_draw_line(canvas, 57, 12, 125, 12);
    canvas_set_font(canvas, FontSecondary);
    wrap_text(
        canvas,
        "Guess The Number v" GTN_VERSION "\nby Prevo128\nPython game, now on Flipper.",
        57,
        21,
        70,
        8,
        0,
        5,
        false);

    canvas_draw_str(canvas, 2, 63, "github.com/Prevo128");
    /* little arrow nudging toward the QR */
    int32_t a = abs_i32(sin_tab[(now / 80) % 16]) / 4;
    canvas_draw_str(canvas, 104 - a, 63, "< scan");
}

/* ---------- stats ---------- */

static const char* const stats_titles[GTN_STATS_PAGES] =
    {"Hall of Fame", "You guess", "I guess", "Overall"};

/* count-up animation for numbers when a page opens */
/* Numbers count up from 0 every time a stats page opens. */
static uint32_t count_up(uint32_t v, uint32_t t) {
    return (uint32_t)(((uint64_t)v * (uint32_t)ease_out(t, 700)) / 256);
}

static void fmt_avg(char* out, size_t size, uint32_t sum, uint32_t n) {
    if(n == 0) {
        snprintf(out, size, "-");
        return;
    }
    uint32_t a10 = (sum * 10 + n / 2) / n;
    snprintf(out, size, "%lu.%lu", (unsigned long)(a10 / 10), (unsigned long)(a10 % 10));
}

static void fmt_time(char* out, size_t size, uint32_t sec) {
    if(sec >= 3600) {
        snprintf(
            out, size, "%luh %02lum", (unsigned long)(sec / 3600), (unsigned long)(sec / 60 % 60));
    } else {
        snprintf(out, size, "%lum %02lus", (unsigned long)(sec / 60), (unsigned long)(sec % 60));
    }
}

static void
    tile(Canvas* canvas, int32_t x, int32_t y, int32_t w, const char* label, const char* value) {
    canvas_draw_rframe(canvas, x, y, w, 20, 3);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, x + 3, y + 8, label);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, x + w - 4, y + 18, AlignRight, AlignBottom, value);
}

static void leader_row(Canvas* canvas, int32_t y, const char* label, const char* value) {
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 3, y, label);
    uint16_t lw = canvas_string_width(canvas, label);
    uint16_t vw = canvas_string_width(canvas, value);
    canvas_draw_str_aligned(canvas, SCREEN_W - 3, y, AlignRight, AlignBottom, value);
    for(int32_t x = 3 + lw + 3; x < SCREEN_W - 3 - (int32_t)vw - 2; x += 2) {
        canvas_draw_dot(canvas, x, y - 1);
    }
}

/* 16x16 trophy, one row per uint16, bit 15 = leftmost */
/* 16x16 trophy, drawn by hand. One row per value, leftmost pixel is bit 15. */
static const uint16_t trophy_bits[16] = {
    0x7FFE,
    0xDFFB,
    0x9FF9,
    0x9FF9,
    0x9FF9,
    0x5FFA,
    0x3FFC,
    0x1FF8,
    0x0FF0,
    0x03C0,
    0x0180,
    0x0180,
    0x03C0,
    0x0FF0,
    0x0810,
    0x0FF0};

static void trophy(Canvas* canvas, int32_t x0, int32_t y0, uint32_t t) {
    for(int32_t y = 0; y < 16; y++) {
        for(int32_t x = 0; x < 16; x++) {
            if(trophy_bits[y] & (0x8000 >> x)) canvas_draw_dot(canvas, x0 + x, y0 + y);
        }
    }
    /* shine sweeping over the cup */
    int32_t c = (int32_t)((t / 45) % 40);
    canvas_set_color(canvas, ColorWhite);
    for(int32_t r = 0; r < 6; r++) {
        int32_t x = c - r, y = 1 + r;
        if(x >= 3 && x <= 12 && (trophy_bits[y] & (0x8000 >> x))) {
            canvas_draw_dot(canvas, x0 + x, y0 + y);
            if(x + 1 <= 12) canvas_draw_dot(canvas, x0 + x + 1, y0 + y);
        }
    }
    canvas_set_color(canvas, ColorBlack);
    /* twinkle */
    if((t / 300) % 3 == 0) star(canvas, x0 + 17, y0 + 1, false);
}

static void page_hall(Canvas* canvas, GtnState* s, uint32_t t, uint32_t now) {
    trophy(canvas, 3, 19, now);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 11, 46, AlignCenter, AlignBottom, "TOP");
    canvas_draw_str_aligned(canvas, 11, 54, AlignCenter, AlignBottom, "3");

    if(s->stats.hs[0].score == 0) {
        canvas_set_font(canvas, FontSecondary);
        wrap_text(
            canvas, "No champions yet. Win a game to get in here!", 26, 27, 98, 9, 0, 3, true);
        return;
    }

    for(int i = 0; i < GTN_HS_COUNT; i++) {
        const GtnHighscore* h = &s->stats.hs[i];
        int32_t y = 16 + i * 14; /* row top */
        /* rows slide in one after another */
        uint32_t delay = (uint32_t)i * 110;
        int32_t off = t < delay ? 110 : (110 * (256 - ease_out(t - delay, 320))) / 256;
        int32_t x = 26 + off;
        if(x >= SCREEN_W) continue;

        /* rank badge */
        canvas_draw_disc(canvas, x + 5, y + 6, 5);
        char rk[2] = {(char)('1' + i), 0};
        canvas_set_color(canvas, ColorWhite);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, x + 5, y + 10, AlignCenter, AlignBottom, rk);
        canvas_set_color(canvas, ColorBlack);

        if(h->score == 0) {
            canvas_draw_str(canvas, x + 15, y + 10, "- - -");
            continue;
        }
        char buf[24];
        snprintf(buf, sizeof(buf), "%u", (unsigned)count_up(h->score, t > delay ? t - delay : 0));
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, x + 15, y + 10, buf);
        uint16_t w = canvas_string_width(canvas, buf);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, x + 17 + w, y + 10, "pts");
        snprintf(buf, sizeof(buf), "%d-%d %ux", h->floor, h->ceiling, (unsigned)h->tries);
        canvas_draw_str_aligned(canvas, x + 100, y + 10, AlignRight, AlignBottom, buf);
        if(i < GTN_HS_COUNT - 1) {
            for(int32_t dx = x + 14; dx < x + 100; dx += 3)
                canvas_draw_dot(canvas, dx, y + 13);
        }
        if(i == 0 && t > 500) {
            /* crown over #1 */
            canvas_draw_line(canvas, x + 2, y - 1, x + 8, y - 1);
            canvas_draw_dot(canvas, x + 2, y - 2);
            canvas_draw_dot(canvas, x + 5, y - 3);
            canvas_draw_dot(canvas, x + 8, y - 2);
        }
    }
}

static void page_you(Canvas* canvas, GtnState* s, uint32_t t) {
    const GtnStats* st = &s->stats;
    char a[12], b[12], c[12], d[12];
    snprintf(a, sizeof(a), "%lu", (unsigned long)count_up(st->p_played, t));
    snprintf(b, sizeof(b), "%lu", (unsigned long)count_up(st->p_won, t));
    fmt_avg(c, sizeof(c), count_up(st->p_guesses, t), st->p_won);
    if(st->p_best_tries)
        snprintf(d, sizeof(d), "%lu", (unsigned long)st->p_best_tries);
    else
        snprintf(d, sizeof(d), "-");
    tile(canvas, 2, 15, 61, "Played", a);
    tile(canvas, 65, 15, 61, "Won", b);
    tile(canvas, 2, 37, 61, "Avg tries", c);
    tile(canvas, 65, 37, 61, "Best", d);
    if(st->p_best_tries == 1 && (t / 250) % 2) star(canvas, 100, 46, true);
}

static void page_cpu(Canvas* canvas, GtnState* s, uint32_t t) {
    const GtnStats* st = &s->stats;
    char a[12], b[12], c[12];
    snprintf(a, sizeof(a), "%lu", (unsigned long)count_up(st->c_played, t));
    snprintf(b, sizeof(b), "%lu", (unsigned long)count_up(st->c_found, t));
    snprintf(c, sizeof(c), "%lu", (unsigned long)count_up(st->c_cheats, t));
    tile(canvas, 2, 15, 40, "Games", a);
    tile(canvas, 44, 15, 40, "Found", b);
    tile(canvas, 86, 15, 40, "Cheats", c);

    /* trust meter */
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 3, 45, "My trust in you");
    uint32_t trust = 0;
    char pct[8] = "??";
    if(st->c_played) {
        uint32_t honest = st->c_played > st->c_cheats ? st->c_played - st->c_cheats : 0;
        trust = honest * 100 / st->c_played;
        snprintf(pct, sizeof(pct), "%lu%%", (unsigned long)count_up(trust, t));
    }
    canvas_draw_str_aligned(canvas, SCREEN_W - 3, 45, AlignRight, AlignBottom, pct);
    canvas_draw_rframe(canvas, 2, 48, 124, 8, 2);
    int32_t fill = (int32_t)(120 * count_up(trust, t) / 100);
    /* moving candy stripes */
    for(int32_t x = 0; x < fill; x++) {
        for(int32_t y = 0; y < 4; y++) {
            if(((x + y + (int32_t)(t / 60)) % 6) < 3) canvas_draw_dot(canvas, 4 + x, 50 + y);
        }
    }
}

static void page_overall(Canvas* canvas, GtnState* s, uint32_t t) {
    const GtnStats* st = &s->stats;
    char v[20];
    snprintf(v, sizeof(v), "%lu", (unsigned long)count_up(st->p_played + st->c_played, t));
    leader_row(canvas, 21, "Games played", v);
    fmt_time(v, sizeof(v), count_up(st->play_seconds, t));
    leader_row(canvas, 29, "Time played", v);
    snprintf(v, sizeof(v), "%lu", (unsigned long)count_up(st->p_psychic, t));
    leader_row(canvas, 37, "Your psychic hits", v);
    snprintf(v, sizeof(v), "%lu", (unsigned long)count_up(st->c_psychic, t));
    leader_row(canvas, 45, "My psychic hits", v);
    fmt_avg(v, sizeof(v), count_up(st->c_tries, t), st->c_found);
    leader_row(canvas, 53, "My avg tries", v);
}

/* The stats screen: four pages you flip with left/right. On top of the
 * current page come the wipe animation, the page dots and, while OK is
 * held, the "Reset all stats?" box. */
static void draw_stats(Canvas* canvas, GtnState* s, uint32_t now) {
    uint32_t t = now - s->page_since;

    title_bar(canvas, stats_titles[s->page], 0);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontPrimary);
    int32_t bob = abs_i32(sin_tab[(now / 80) % 16]) / 4;
    canvas_draw_str(canvas, 3 - (bob > 1 ? 1 : 0), 11, "<");
    canvas_draw_str(canvas, SCREEN_W - 9 + (bob > 1 ? 1 : 0), 11, ">");
    canvas_set_color(canvas, ColorBlack);

    switch(s->page) {
    case 0:
        page_hall(canvas, s, t, now);
        break;
    case 1:
        page_you(canvas, s, t);
        break;
    case 2:
        page_cpu(canvas, s, t);
        break;
    default:
        page_overall(canvas, s, t);
        break;
    }

    /* page wipe with a scanner bar */
    if(s->page_dir != 0 && t < 220) {
        int32_t p = ease_out(t, 220);
        int32_t edge = s->page_dir > 0 ? SCREEN_W - (SCREEN_W * p) / 256 : (SCREEN_W * p) / 256;
        canvas_set_color(canvas, ColorWhite);
        if(s->page_dir > 0) {
            if(edge > 0) canvas_draw_box(canvas, 0, 13, (size_t)edge, 45);
        } else if(edge < SCREEN_W) {
            canvas_draw_box(canvas, edge, 13, (size_t)(SCREEN_W - edge), 45);
        }
        canvas_set_color(canvas, ColorBlack);
        if(edge >= 1 && edge < SCREEN_W - 1) canvas_draw_box(canvas, edge - 1, 13, 2, 45);
    }

    /* footer: hint left, page dots right */
    canvas_set_font(canvas, FontSecondary);
    if(s->page == GTN_STATS_PAGES - 1) canvas_draw_str(canvas, 3, 63, "Hold OK: reset");
    for(int i = 0; i < GTN_STATS_PAGES; i++) {
        int32_t x = SCREEN_W - 6 - (GTN_STATS_PAGES - 1 - i) * 7;
        if(i == s->page)
            canvas_draw_box(canvas, x - 2, 59, 5, 4);
        else
            canvas_draw_dot(canvas, x, 61);
    }

    /* hold-to-reset overlay */
    if(s->hold_ok && now - s->hold_since > 200) {
        uint32_t held = now - s->hold_since;
        int32_t p = (int32_t)(held * 100 / GTN_RESET_HOLD_MS);
        if(p > 100) p = 100;
        int32_t jx = shake_tab[(now / 40) % 8] / 2;
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_box(canvas, 12 + jx, 18, 104, 30);
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_rframe(canvas, 12 + jx, 18, 104, 30, 3);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(
            canvas, SCREEN_W / 2 + jx, 31, AlignCenter, AlignBottom, "Reset all stats?");
        canvas_draw_rframe(canvas, 18 + jx, 35, 92, 8, 2);
        canvas_draw_box(canvas, 20 + jx, 37, (size_t)(88 * p / 100), 4);
    }

    /* toast after a reset */
    if(s->toast_at && now - s->toast_at < 1300) {
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_rbox(canvas, 24, 24, 80, 16, 3);
        canvas_set_color(canvas, ColorWhite);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(
            canvas, SCREEN_W / 2, 36, AlignCenter, AlignBottom, "Stats cleared");
        canvas_set_color(canvas, ColorBlack);
    }
}

static void butterfly(Canvas* canvas, int32_t cx, int32_t cy, uint32_t t) {
    int32_t flap = ((t / 120) % 2) ? 2 : 0;
    canvas_draw_disc(canvas, cx - 7 + flap, cy - 4, 6 - flap);
    canvas_draw_disc(canvas, cx + 7 - flap, cy - 4, 6 - flap);
    canvas_draw_disc(canvas, cx - 5 + flap / 2, cy + 6, 4 - flap / 2);
    canvas_draw_disc(canvas, cx + 5 - flap / 2, cy + 6, 4 - flap / 2);
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_disc(canvas, cx - 7 + flap, cy - 4, 2);
    canvas_draw_disc(canvas, cx + 7 - flap, cy - 4, 2);
    canvas_draw_box(canvas, cx - 1, cy - 9, 3, 20);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_line(canvas, cx, cy - 8, cx, cy + 9);
    canvas_draw_line(canvas, cx, cy - 8, cx - 4, cy - 13);
    canvas_draw_line(canvas, cx, cy - 8, cx + 4, cy - 13);
}

/* Bye bye, butterfly! It flies off to the top right while the app closes. */
static void draw_bye(Canvas* canvas, GtnState* s, uint32_t now) {
    uint32_t t = now - s->screen_since;
    /* flutters away up and to the right */
    if(t > GTN_BYE_MS) t = GTN_BYE_MS;
    int32_t cx = SCREEN_W / 2 + (int32_t)(t * 36 / GTN_BYE_MS) + sin_tab[(t / 70) % 16] / 2;
    int32_t cy = 28 - (int32_t)(t * 12 / GTN_BYE_MS) + sin_tab[(t / 50) % 16] / 3;
    butterfly(canvas, cx, cy, t);
    /* sparkle trail */
    for(uint32_t i = 1; i <= 3; i++) {
        uint32_t tt = t > i * 150 ? t - i * 150 : 0;
        int32_t px = SCREEN_W / 2 + (int32_t)(tt * 36 / GTN_BYE_MS) - 12;
        int32_t py = 28 - (int32_t)(tt * 12 / GTN_BYE_MS) + 8;
        if(t > i * 150 && ((t / 100 + i) % 2)) canvas_draw_dot(canvas, px, py);
    }
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(
        canvas, SCREEN_W / 2, 58, AlignCenter, AlignBottom, "Bye bye, butterfly!");
}

void gtn_draw(Canvas* canvas, GtnState* s, uint32_t now) {
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    switch(s->screen) {
    case GtnScreenSplash:
        draw_splash(canvas, now - s->screen_since);
        break;
    case GtnScreenMenu:
        draw_menu(canvas, s, now);
        break;
    case GtnScreenRange:
        draw_range(canvas, s, now);
        break;
    case GtnScreenStarting:
        draw_starting(canvas, s, now);
        break;
    case GtnScreenPlayer:
        draw_player(canvas, s, now);
        break;
    case GtnScreenComputer:
        draw_computer(canvas, s, now);
        break;
    case GtnScreenCelebrate:
        draw_celebrate(canvas, s, now);
        break;
    case GtnScreenText:
        draw_text(canvas, s, now);
        break;
    case GtnScreenAbout:
        draw_about(canvas, s, now);
        break;
    case GtnScreenStats:
        draw_stats(canvas, s, now);
        break;
    case GtnScreenBye:
        draw_bye(canvas, s, now);
        break;
    }
}
