#include "hunt_view.h"

#include <furi.h>
#include <gui/gui.h>
#include <stdio.h>
#include <string.h>

/* Warmer/colder bands, in dB above the tracked noise floor. */
#define HUNT_BLAZING 30
#define HUNT_HOT     18
#define HUNT_WARM    10
#define HUNT_COOL    4

struct HuntView {
    View* view;
    HuntViewOkCallback ok_cb;
    void* ok_ctx;
};

typedef struct {
    HuntData d;
    uint8_t history[FDY_HISTORY_LEN];
    uint8_t anim;
} HuntModel;

int16_t hunt_view_margin(const HuntData* data) {
    if(!data) return 0;
    int16_t m = (int16_t)(data->rssi - data->floor);
    return m < 0 ? 0 : m;
}

static const char* hunt_word(int16_t margin) {
    if(margin >= HUNT_BLAZING) return "BLAZING";
    if(margin >= HUNT_HOT) return "HOT";
    if(margin >= HUNT_WARM) return "WARM";
    if(margin >= HUNT_COOL) return "COOL";
    return "COLD";
}

static void hunt_view_draw(Canvas* canvas, void* model) {
    HuntModel* m = model;
    const HuntData* d = &m->d;
    char buf[24];

    /* header */
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 9, "LEAK HUNT");
    if(d->band) canvas_draw_str_aligned(canvas, 126, 9, AlignRight, AlignBottom, d->band);
    canvas_draw_line(canvas, 0, 11, 127, 11);

    /* warmer/colder word - the primary feedback while sweeping */
    int16_t margin = hunt_view_margin(d);
    const char* word = hunt_word(margin);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 20, AlignCenter, AlignCenter, word);
    if(margin >= HUNT_BLAZING) {
        /* found it: box the word so it reads at a glance mid-sweep */
        int w = canvas_string_width(canvas, word);
        canvas_draw_frame(canvas, 64 - w / 2 - 4, 13, w + 8, 14);
    }

    /* live bar + peak-hold tick */
    canvas_draw_frame(canvas, 2, 29, 124, 11);
    int fw = (120 * (d->level > 100 ? 100 : d->level)) / 100;
    if(fw > 0) canvas_draw_box(canvas, 4, 31, fw, 7);
    int px = 4 + (120 * (d->peak_norm > 100 ? 100 : d->peak_norm)) / 100;
    canvas_draw_line(canvas, px, 28, px, 40);

    /* rolling trace: the shape of the sweep you just made */
    if(d->history) {
        for(int k = 0; k < 62; k++) {
            int idx = (d->history_head - k + 2 * FDY_HISTORY_LEN) % FDY_HISTORY_LEN;
            int v = d->history[idx];
            int x = 126 - k * 2;
            int y = 51 - (v * 11) / 100;
            if(y < 51)
                canvas_draw_line(canvas, x, 51, x, y);
            else
                canvas_draw_dot(canvas, x, 51);
        }
    }

    /* bottom strip: live value + reset */
    canvas_draw_box(canvas, 0, 53, 128, 11);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontSecondary);
    snprintf(buf, sizeof(buf), "%d dBm  +%d", (int)d->rssi, (int)margin);
    canvas_draw_str(canvas, 3, 62, buf);
    canvas_draw_str_aligned(canvas, 125, 62, AlignRight, AlignBottom, "OK reset");
    canvas_set_color(canvas, ColorBlack);
}

static bool hunt_view_input(InputEvent* event, void* context) {
    HuntView* v = context;
    if(event->type == InputTypeShort && event->key == InputKeyOk) {
        if(v->ok_cb) v->ok_cb(v->ok_ctx);
        return true;
    }
    return false;
}

HuntView* hunt_view_alloc(void) {
    HuntView* v = malloc(sizeof(HuntView));
    v->ok_cb = NULL;
    v->ok_ctx = NULL;
    v->view = view_alloc();
    view_set_context(v->view, v);
    view_set_draw_callback(v->view, hunt_view_draw);
    view_set_input_callback(v->view, hunt_view_input);
    view_allocate_model(v->view, ViewModelTypeLocking, sizeof(HuntModel));
    return v;
}

void hunt_view_free(HuntView* v) {
    furi_assert(v);
    view_free(v->view);
    free(v);
}

View* hunt_view_get_view(HuntView* v) {
    furi_assert(v);
    return v->view;
}

void hunt_view_set_ok_callback(HuntView* v, HuntViewOkCallback cb, void* context) {
    furi_assert(v);
    v->ok_cb = cb;
    v->ok_ctx = context;
}

void hunt_view_update(HuntView* v, const HuntData* data) {
    furi_assert(v);
    with_view_model(
        v->view,
        HuntModel * m,
        {
            m->d = *data;
            if(data->history) {
                memcpy(m->history, data->history, sizeof(m->history));
                m->d.history = m->history; // point at our own copy
            } else {
                m->d.history = NULL;
            }
        },
        true);
}

void hunt_view_tick(HuntView* v) {
    furi_assert(v);
    with_view_model(v->view, HuntModel * m, { m->anim++; }, true);
}
