#include "monitor_view.h"
#include <furi.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define MON_2PI       6.28318530718f
#define MON_GUARD_MAX 33

/* Eye geometry */
#define CX    34
#define CY    33
#define RX    31 // eyelid half-width
#define LIDH  18 // eyelid bow height
#define IRISR 16 // iris radius

struct MonitorView {
    View* view;
    MonitorViewCallback ok_cb;
    void* ok_ctx;
};

typedef struct {
    MonitorBlip blips[MONITOR_MAX_BLIPS];
    size_t blip_count;
    uint32_t deauth_total;
    uint32_t deauth_rate;
    size_t ap_count;
    size_t twin_count;
    bool esp_connected;
    bool under_attack;
    char guard[MON_GUARD_MAX];
    uint8_t sweep;
    uint8_t anim;
} MonitorModel;

static void polar(uint8_t angle, float radius, int* x, int* y) {
    float rad = (float)angle * (MON_2PI / 256.0f);
    *x = CX + (int)(cosf(rad) * radius);
    *y = CY + (int)(sinf(rad) * radius);
}

/* almond eyelid as a parabola-ish polyline; sign +1 lower lid, -1 upper lid */
static void draw_lid(Canvas* c, int sign, int yoff) {
    int px = 0, py = 0;
    bool have = false;
    for(int x = -RX; x <= RX; x += 2) {
        float t = (float)x / (float)RX;
        int off = (int)(LIDH * (1.0f - t * t));
        int X = CX + x;
        int Y = CY + sign * off + sign * yoff;
        if(have) canvas_draw_line(c, px, py, X, Y);
        px = X;
        py = Y;
        have = true;
    }
}

static void monitor_view_draw(Canvas* canvas, void* model) {
    MonitorModel* m = model;
    char buf[32];

    /* ---- header ---- */
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 9, "ARGUS");
    const char* link = m->esp_connected ? "LIVE" : "NO ESP";
    canvas_draw_str_aligned(canvas, 114, 9, AlignRight, AlignBottom, link);
    if(m->esp_connected) {
        canvas_draw_disc(canvas, 122, 5, 2);
    } else {
        canvas_draw_circle(canvas, 122, 5, 2);
    }
    canvas_draw_line(canvas, 0, 12, 127, 12);

    /* ---- the Eye ---- */
    draw_lid(canvas, -1, 0); // upper lid
    draw_lid(canvas, +1, 0); // lower lid
    if(m->under_attack) { // alarmed: a second, bolder lid
        draw_lid(canvas, -1, 2);
        draw_lid(canvas, +1, 2);
    }

    /* iris rings + faint crosshair */
    canvas_draw_circle(canvas, CX, CY, IRISR);
    canvas_draw_circle(canvas, CX, CY, IRISR - 6);
    for(int d = -(IRISR - 2); d <= IRISR - 2; d += 3) {
        canvas_draw_dot(canvas, CX + d, CY);
        canvas_draw_dot(canvas, CX, CY + d);
    }

    /* sweep line + leading dot */
    int ex, ey;
    polar(m->sweep, IRISR - 1, &ex, &ey);
    canvas_draw_line(canvas, CX, CY, ex, ey);
    canvas_draw_disc(canvas, ex, ey, 1);

    /* blips (APs) mapped by signal strength */
    for(size_t i = 0; i < m->blip_count; i++) {
        int rssi = m->blips[i].rssi;
        if(rssi < -100) rssi = -100;
        if(rssi > -40) rssi = -40;
        float near = (float)(rssi + 100) / 60.0f;
        float br = (1.0f - near) * (float)(IRISR - 4) + 2.0f;
        int bx, by;
        polar(m->blips[i].angle, br, &bx, &by);
        if(m->blips[i].clone) {
            canvas_draw_disc(canvas, bx, by, 2);
            uint8_t pulse = 3 + (m->anim % 4);
            canvas_draw_circle(canvas, bx, by, pulse); // throbbing threat ring
        } else {
            canvas_draw_disc(canvas, bx, by, 1);
        }
    }

    /* pupil: calm disc, or a slit when alarmed */
    if(m->under_attack) {
        canvas_draw_box(canvas, CX - 1, CY - (IRISR - 5), 3, 2 * (IRISR - 5));
    } else {
        canvas_draw_disc(canvas, CX, CY, 2);
    }

    /* ---- right stats column ---- */
    canvas_draw_line(canvas, 67, 13, 67, 52);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 71, 21, "DEAUTHS");
    canvas_set_font(canvas, FontBigNumbers);
    snprintf(buf, sizeof(buf), "%lu", (unsigned long)m->deauth_total);
    canvas_draw_str(canvas, 72, 43, buf);
    canvas_set_font(canvas, FontSecondary);
    snprintf(buf, sizeof(buf), "AP %u  TW %u", (unsigned)m->ap_count, (unsigned)m->twin_count);
    canvas_draw_str(canvas, 71, 51, buf);

    /* ---- footer / status strip ---- */
    if(m->under_attack) {
        canvas_draw_box(canvas, 0, 54, 128, 10);
        canvas_set_color(canvas, ColorWhite);
        snprintf(buf, sizeof(buf), "! DEAUTH ATTACK x%lu", (unsigned long)m->deauth_rate);
        canvas_draw_str(canvas, 3, 62, buf);
        canvas_set_color(canvas, ColorBlack);
        /* bold alarm frame */
        canvas_draw_frame(canvas, 0, 0, 128, 64);
        canvas_draw_frame(canvas, 1, 1, 126, 62);
    } else if(!m->esp_connected) {
        canvas_draw_str(canvas, 2, 62, "Connect ESP32 board...");
    } else if(m->guard[0] == '\0') {
        canvas_draw_str(canvas, 2, 62, "Set a Guarded SSID first");
    } else {
        char g[16];
        strncpy(g, m->guard, sizeof(g) - 1);
        g[sizeof(g) - 1] = '\0';
        snprintf(buf, sizeof(buf), "Guarding %s", g);
        canvas_draw_str(canvas, 2, 62, buf);
        canvas_draw_str_aligned(canvas, 125, 62, AlignRight, AlignBottom, "OK:log");
    }
}

static bool monitor_view_input(InputEvent* event, void* context) {
    MonitorView* v = context;
    if(event->type == InputTypeShort && event->key == InputKeyOk) {
        if(v->ok_cb) v->ok_cb(v->ok_ctx);
        return true;
    }
    return false;
}

MonitorView* monitor_view_alloc(void) {
    MonitorView* v = malloc(sizeof(MonitorView));
    v->ok_cb = NULL;
    v->ok_ctx = NULL;
    v->view = view_alloc();
    view_set_context(v->view, v);
    view_set_draw_callback(v->view, monitor_view_draw);
    view_set_input_callback(v->view, monitor_view_input);
    view_allocate_model(v->view, ViewModelTypeLocking, sizeof(MonitorModel));
    return v;
}

void monitor_view_free(MonitorView* v) {
    furi_assert(v);
    view_free(v->view);
    free(v);
}

View* monitor_view_get_view(MonitorView* v) {
    furi_assert(v);
    return v->view;
}

void monitor_view_set_ok_callback(MonitorView* v, MonitorViewCallback cb, void* context) {
    furi_assert(v);
    v->ok_cb = cb;
    v->ok_ctx = context;
}

void monitor_view_update(
    MonitorView* v,
    const MonitorBlip* blips,
    size_t blip_count,
    uint32_t deauth_total,
    uint32_t deauth_rate,
    size_t ap_count,
    size_t twin_count,
    bool esp_connected,
    bool under_attack,
    const char* guard) {
    furi_assert(v);
    with_view_model(
        v->view,
        MonitorModel * m,
        {
            size_t n = blip_count < MONITOR_MAX_BLIPS ? blip_count : MONITOR_MAX_BLIPS;
            for(size_t i = 0; i < n; i++)
                m->blips[i] = blips[i];
            m->blip_count = n;
            m->deauth_total = deauth_total;
            m->deauth_rate = deauth_rate;
            m->ap_count = ap_count;
            m->twin_count = twin_count;
            m->esp_connected = esp_connected;
            m->under_attack = under_attack;
            if(guard) {
                strncpy(m->guard, guard, MON_GUARD_MAX - 1);
                m->guard[MON_GUARD_MAX - 1] = '\0';
            } else {
                m->guard[0] = '\0';
            }
        },
        true);
}

void monitor_view_tick(MonitorView* v) {
    furi_assert(v);
    with_view_model(
        v->view,
        MonitorModel * m,
        {
            m->sweep += 7;
            m->anim++;
        },
        true);
}
