#include <furi.h>
#include <stdio.h>
#include <input/input.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>

#include "storm_view.h"
#include "../flipper_structs.h"

/* Integer sin/cos * 64 over 32 angle steps (no libm needed on-device). */
static const int8_t COS64[32] = {64, 63, 59, 53, 45, 36, 24, 12, 0, -12, -24, -36, -45, -53, -59, -63, -64, -63, -59, -53, -45, -36, -24, -12, 0, 12, 24, 36, 45, 53, 59, 63};
static const int8_t SIN64[32] = {0, 12, 24, 36, 45, 53, 59, 63, 64, 63, 59, 53, 45, 36, 24, 12, 0, -12, -24, -36, -45, -53, -59, -63, -64, -63, -59, -53, -45, -36, -24, -12};

typedef struct {
    uint8_t wifi;
    int8_t rssi;
    uint8_t category; // enum CatchCategory
    uint8_t tier;     // egg tier: 2 == board-exclusive storm egg
    char text[24];
    uint32_t frame;
} StormModel;

/* A twinkling 4-ray sparkle; ray length pulses 0..3 by phase. */
static void draw_sparkle(Canvas *cv, int x, int y, int len) {
    if(len <= 0) return;
    canvas_draw_dot(cv, x, y);
    canvas_draw_line(cv, x - len, y, x + len, y);
    canvas_draw_line(cv, x, y - len, x, y + len);
    if(len >= 3) { /* diagonal glints on a big pulse */
        canvas_draw_dot(cv, x - 1, y - 1);
        canvas_draw_dot(cv, x + 1, y + 1);
    }
}

static inline int rx(int cx, int k, int r) { return cx + (COS64[k & 31] * r) / 64; }
static inline int ry(int cy, int k, int r) { return cy + (SIN64[k & 31] * r) / 64; }

static void storm_draw(Canvas *cv, void *model) {
    StormModel *m = model;
    canvas_clear(cv);

    /* ---- Radar (left), sweeping and pinging the nearby signals ---- */
    const int cx = 30, cy = 32, R = 26;
    canvas_draw_circle(cv, cx, cy, R);
    canvas_draw_circle(cv, cx, cy, (R * 2) / 3);
    canvas_draw_circle(cv, cx, cy, R / 3);
    canvas_draw_line(cv, cx - R, cy, cx + R, cy);
    canvas_draw_line(cv, cx, cy - R, cx, cy + R);

    int sweep = (int)(m->frame % 32);
    /* beam: the leading line plus a couple of fading trails */
    canvas_draw_line(cv, cx, cy, rx(cx, sweep, R), ry(cy, sweep, R));
    canvas_draw_line(cv, cx, cy, rx(cx, sweep - 1, (R * 4) / 5), ry(cy, sweep - 1, (R * 4) / 5));
    canvas_draw_line(cv, cx, cy, rx(cx, sweep - 2, (R * 3) / 5), ry(cy, sweep - 2, (R * 3) / 5));

    /* blips: one per nearby AP, at deterministic positions; they "ping"
     * (grow) as the sweep passes over them. */
    int n = m->wifi;
    if(n > 16) n = 16;
    for(int i = 0; i < n; i++) {
        uint32_t h = (uint32_t)(i + 1) * 2654435761u;
        int ang = (int)((h >> 3) & 31);
        int rad = 5 + (int)((h >> 9) % (uint32_t)(R - 6));
        int bx = rx(cx, ang, rad);
        int by = ry(cy, ang, rad);
        int d = (sweep - ang) & 31;
        if(d <= 2)
            canvas_draw_disc(cv, bx, by, 2); /* freshly pinged */
        else if(d <= 5)
            canvas_draw_box(cv, bx, by, 2, 2);
        else
            canvas_draw_dot(cv, bx, by);
    }
    canvas_draw_disc(cv, cx, cy, 1); /* the dragon at the centre */

    /* ---- Storm-egg celebration: twinkling sparkles ---- */
    if(m->category == CATCH_EGG && m->tier == 2) {
        static const uint8_t sx[] = {50, 112, 57, 122, 46, 100, 88};
        static const uint8_t sy[] = {6, 9, 27, 31, 45, 16, 40};
        for(unsigned i = 0; i < sizeof(sx); i++) {
            int phase = (int)((m->frame / 2 + i * 3) % 8); /* 0..7 */
            int len = phase <= 3 ? phase : (6 - phase);    /* pulse 0..3..0 */
            draw_sparkle(cv, sx[i], sy[i], len);
        }
    }

    /* ---- Title + readings (right) ---- */
    canvas_set_font(cv, FontPrimary);
    canvas_draw_str(cv, 60, 10, "SIGNAL");
    canvas_draw_str(cv, 60, 21, "STORM");

    canvas_set_font(cv, FontSecondary);
    char buf[24];
    snprintf(buf, sizeof(buf), "%u nets", (unsigned)m->wifi);
    canvas_draw_str(cv, 60, 34, buf);
    snprintf(buf, sizeof(buf), "%d dBm", (int)m->rssi);
    canvas_draw_str(cv, 60, 44, buf);

    /* ---- Catch result (inverted bottom bar) ---- */
    canvas_set_color(cv, ColorBlack);
    canvas_draw_box(cv, 0, 53, 128, 11);
    canvas_set_color(cv, ColorWhite);
    canvas_draw_str(cv, 3, 61, m->text);
    const char *hint = "OK";
    canvas_draw_str(cv, 128 - canvas_string_width(cv, hint) - 3, 61, hint);
    canvas_set_color(cv, ColorBlack);
}

static bool storm_input(InputEvent *event, void *context) {
    struct ApplicationContext *app = context;
    if(event->type == InputTypeShort &&
       (event->key == InputKeyOk || event->key == InputKeyBack)) {
        view_dispatcher_send_custom_event(app->view_dispatcher, STORM_EVT_DONE);
        return true;
    }
    return false;
}

View *storm_view_alloc(void *context) {
    View *v = view_alloc();
    view_set_context(v, context);
    view_set_draw_callback(v, storm_draw);
    view_set_input_callback(v, storm_input);
    view_allocate_model(v, ViewModelTypeLocking, sizeof(StormModel));
    return v;
}

void storm_view_free(View *v) {
    view_free(v);
}

void storm_view_set(View *v, const struct GameState *gs) {
    with_view_model(
        v, StormModel * m,
        {
            m->wifi = gs->last_wifi;
            m->rssi = gs->last_rssi;
            m->category = gs->last_catch.category;
            m->tier = gs->last_catch.tier;
            m->frame = 0;
            for(size_t i = 0; i < sizeof(m->text); i++) {
                m->text[i] = gs->reveal_text[i];
                if(!gs->reveal_text[i]) break;
            }
            m->text[sizeof(m->text) - 1] = '\0';
        },
        true);
}

void storm_view_tick(View *v) {
    with_view_model(v, StormModel * m, { m->frame++; }, true);
}
