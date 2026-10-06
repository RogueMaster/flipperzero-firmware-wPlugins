#include "alert_view.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <furi.h>
#include <gui/canvas.h>
#include <input/input.h>

typedef struct {
    AlertRuleId rule;
    uint8_t value;
    uint8_t threshold;
    bool critical;
    char lines[ADVICE_MAX_LINES][ADVICE_LINE_LEN];
    size_t line_count;
} AlertModel;

struct PhrAlertView {
    View* view;
    PhrAlertCallback ok_cb;
    void* ok_ctx;
};

static void draw_value(Canvas* c, int x, int base_y, uint8_t v, const char* unit) {
    char s[8];
    snprintf(s, sizeof(s), "%u", v);
    canvas_set_font(c, FontBigNumbers);
    canvas_draw_str(c, x, base_y, s);
    int w = canvas_string_width(c, s);
    canvas_set_font(c, FontPrimary);
    if(unit[0] == 'C') {
        canvas_draw_circle(c, x + w + 4, base_y - 9, 2);
        canvas_draw_str(c, x + w + 8, base_y, "C");
    } else {
        canvas_draw_str(c, x + w + 2, base_y, unit);
    }
}

/* Shortens src with a trailing ".." until it is at most max_w px wide in the CURRENT font. */
static void fit_str(Canvas* c, const char* src, char* dst, size_t cap, int max_w) {
    snprintf(dst, cap, "%s", src);
    if(canvas_string_width(c, dst) <= max_w) return;
    size_t n = strlen(dst);
    int dots = canvas_string_width(c, "..");
    while(n > 0 && canvas_string_width(c, dst) + dots > max_w)
        dst[--n] = '\0';
    if(n + 2 < cap) {
        dst[n] = '.';
        dst[n + 1] = '.';
        dst[n + 2] = '\0';
    }
}

static void alert_draw(Canvas* c, void* model) {
    const AlertModel* m = model;
    const AlertRuleMeta* meta = alerts_meta(m->rule);
    canvas_clear(c);
    canvas_set_color(c, ColorBlack);

    // header: inverted bar with warning sign and rule name
    canvas_draw_box(c, 0, 0, 128, 12);
    canvas_set_color(c, ColorWhite);
    canvas_draw_line(c, 6, 2, 2, 9);
    canvas_draw_line(c, 6, 2, 10, 9);
    canvas_draw_line(c, 2, 9, 10, 9);
    canvas_draw_line(c, 6, 4, 6, 6);
    canvas_draw_dot(c, 6, 8);
    canvas_set_font(c, FontPrimary);
    char title[28];
    fit_str(c, meta->name, title, sizeof(title), 128 - 15 - 3);
    canvas_draw_str(c, 15, 9, title);
    canvas_set_color(c, ColorBlack);

    // value vs threshold
    if(m->rule == AlertLinkLost) {
        canvas_set_font(c, FontPrimary);
        canvas_draw_str(c, 4, 23, "No data from PC");
        canvas_set_font(c, FontSecondary);
        char s[28];
        snprintf(s, sizeof(s), "for %u s (limit %u s)", m->value, m->threshold);
        canvas_draw_str(c, 4, 31, s);
    } else {
        draw_value(c, 4, 29, m->value, meta->unit);
        canvas_set_font(c, FontSecondary);
        char s[24];
        canvas_draw_str_aligned(
            c, 125, 22, AlignRight, AlignBottom, meta->lower_is_worse ? "below" : "limit");
        snprintf(
            s, sizeof(s), "%u%s%s", m->threshold, meta->unit[0] == 'C' ? " " : "", meta->unit);
        canvas_set_font(c, FontPrimary);
        canvas_draw_str_aligned(c, 125, 31, AlignRight, AlignBottom, s);
    }

    // advice: at most 2 tips, 9 px line pitch so descenders never touch the next line
    canvas_set_font(c, FontSecondary);
    const int tip_x = 10;
    size_t tips = m->line_count < 2 ? m->line_count : 2;
    for(size_t i = 0; i < tips; i++) {
        int y = 40 + (int)i * 9;
        char tip[ADVICE_LINE_LEN];
        fit_str(c, m->lines[i], tip, sizeof(tip), 128 - tip_x - 3);
        canvas_draw_str(c, 4, y, "-");
        canvas_draw_str(c, tip_x, y, tip);
    }

    // footer
    canvas_draw_line(c, 0, 53, 127, 53);
    canvas_draw_str(c, 2, 63, "OK snooze 10m");
    canvas_draw_str_aligned(c, 126, 63, AlignRight, AlignBottom, "Back close");
}

static bool alert_input(InputEvent* event, void* context) {
    PhrAlertView* v = context;
    if(event->type == InputTypeShort && event->key == InputKeyOk) {
        if(v->ok_cb) v->ok_cb(v->ok_ctx);
        return true;
    }
    return false;
}

PhrAlertView* phr_alert_view_alloc(void) {
    PhrAlertView* v = malloc(sizeof(PhrAlertView));
    memset(v, 0, sizeof(PhrAlertView));
    v->view = view_alloc();
    view_allocate_model(v->view, ViewModelTypeLocking, sizeof(AlertModel));
    view_set_context(v->view, v);
    view_set_draw_callback(v->view, alert_draw);
    view_set_input_callback(v->view, alert_input);
    return v;
}

void phr_alert_view_free(PhrAlertView* v) {
    view_free(v->view);
    free(v);
}

View* phr_alert_view_get_view(PhrAlertView* v) {
    return v->view;
}

void phr_alert_view_set_ok_callback(PhrAlertView* v, PhrAlertCallback cb, void* ctx) {
    v->ok_cb = cb;
    v->ok_ctx = ctx;
}

void phr_alert_view_set(
    PhrAlertView* v,
    AlertRuleId rule,
    uint8_t value,
    uint8_t threshold,
    bool critical,
    char lines[ADVICE_MAX_LINES][ADVICE_LINE_LEN],
    size_t line_count) {
    with_view_model(
        v->view,
        AlertModel * m,
        {
            m->rule = rule;
            m->value = value;
            m->threshold = threshold;
            m->critical = critical;
            m->line_count = line_count;
            memcpy(m->lines, lines, sizeof(m->lines));
        },
        true);
}
