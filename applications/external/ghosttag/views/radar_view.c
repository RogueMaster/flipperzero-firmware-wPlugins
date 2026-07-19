#include "radar_view.h"
#include <furi.h>
#include <math.h>
#include <stdio.h>

#define RADAR_MAX_BLIPS 24
#define RADAR_2PI 6.28318530718f

#define CX 32
#define CY 35
#define R 20

struct RadarView {
    View* view;
    RadarViewCallback ok_cb;
    void* ok_ctx;
};

typedef struct {
    RadarBlip blips[RADAR_MAX_BLIPS];
    size_t blip_count;
    size_t seen;
    size_t following;
    bool esp_connected;
    uint8_t sweep; // 0..255 bearing of the sweep line
    uint8_t anim; // free-running frame counter
} RadarModel;

static void radar_point(uint8_t angle, float radius, int* x, int* y) {
    float rad = (float)angle * (RADAR_2PI / 256.0f);
    *x = CX + (int)(cosf(rad) * radius);
    *y = CY + (int)(sinf(rad) * radius);
}

static void radar_view_draw(Canvas* canvas, void* model) {
    RadarModel* m = model;
    char buf[20];

    // ---- header ----
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 9, "GHOSTTAG");
    const char* link = m->esp_connected ? "LINK" : "NO ESP";
    canvas_draw_str_aligned(canvas, 114, 9, AlignRight, AlignBottom, link);
    if(m->esp_connected) {
        canvas_draw_disc(canvas, 122, 5, 2);
    } else {
        canvas_draw_circle(canvas, 122, 5, 2);
    }
    canvas_draw_line(canvas, 0, 12, 127, 12);

    // ---- radar rings + crosshair ----
    canvas_draw_circle(canvas, CX, CY, R);
    canvas_draw_circle(canvas, CX, CY, 14);
    canvas_draw_circle(canvas, CX, CY, 7);
    canvas_draw_dot(canvas, CX, CY);
    // faint crosshair (dotted)
    for(int d = -R; d <= R; d += 3) {
        canvas_draw_dot(canvas, CX + d, CY);
        canvas_draw_dot(canvas, CX, CY + d);
    }

    // ---- sweep line + leading dot ----
    int ex, ey;
    radar_point(m->sweep, R, &ex, &ey);
    canvas_draw_line(canvas, CX, CY, ex, ey);
    canvas_draw_disc(canvas, ex, ey, 1);
    // short trailing spoke for motion
    int tx, ty;
    radar_point((uint8_t)(m->sweep - 12), R - 4, &tx, &ty);
    canvas_draw_line(canvas, CX, CY, tx, ty);

    // ---- blips ----
    for(size_t i = 0; i < m->blip_count; i++) {
        int rssi = m->blips[i].rssi;
        if(rssi < -100) rssi = -100;
        if(rssi > -40) rssi = -40;
        float near = (float)(rssi + 100) / 60.0f; // 0 far .. 1 close
        float br = (1.0f - near) * (float)(R - 3) + 2.0f;
        int bx, by;
        radar_point(m->blips[i].angle, br, &bx, &by);
        if(m->blips[i].following) {
            canvas_draw_disc(canvas, bx, by, 2);
            uint8_t pulse = 3 + (m->anim % 4);
            canvas_draw_circle(canvas, bx, by, pulse);
        } else {
            canvas_draw_disc(canvas, bx, by, 1);
        }
    }

    // ---- right info panel ----
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 60, 24, "TRACKERS");
    canvas_set_font(canvas, FontBigNumbers);
    snprintf(buf, sizeof(buf), "%u", (unsigned)m->seen);
    canvas_draw_str(canvas, 62, 47, buf);

    // ---- footer status bar ----
    canvas_set_font(canvas, FontSecondary);
    if(!m->esp_connected) {
        canvas_draw_str(canvas, 2, 62, "Connect ESP32 board...");
    } else if(m->following > 0) {
        canvas_draw_box(canvas, 0, 54, 128, 10);
        canvas_set_color(canvas, ColorWhite);
        snprintf(buf, sizeof(buf), "! %u FOLLOWING", (unsigned)m->following);
        canvas_draw_str(canvas, 3, 62, buf);
        canvas_draw_str_aligned(canvas, 125, 62, AlignRight, AlignBottom, "OK");
        canvas_set_color(canvas, ColorBlack);
    } else {
        static const char* dots[] = {"", ".", "..", "..."};
        snprintf(buf, sizeof(buf), "Scanning%s", dots[m->anim % 4]);
        canvas_draw_str(canvas, 2, 62, buf);
        canvas_draw_str_aligned(canvas, 125, 62, AlignRight, AlignBottom, "OK:list");
    }
}

static bool radar_view_input(InputEvent* event, void* context) {
    RadarView* radar = context;
    if(event->type == InputTypeShort && event->key == InputKeyOk) {
        if(radar->ok_cb) radar->ok_cb(radar->ok_ctx);
        return true;
    }
    return false;
}

RadarView* radar_view_alloc(void) {
    RadarView* radar = malloc(sizeof(RadarView));
    radar->ok_cb = NULL;
    radar->ok_ctx = NULL;
    radar->view = view_alloc();
    view_set_context(radar->view, radar);
    view_set_draw_callback(radar->view, radar_view_draw);
    view_set_input_callback(radar->view, radar_view_input);
    view_allocate_model(radar->view, ViewModelTypeLocking, sizeof(RadarModel));
    return radar;
}

void radar_view_free(RadarView* radar) {
    furi_assert(radar);
    view_free(radar->view);
    free(radar);
}

View* radar_view_get_view(RadarView* radar) {
    furi_assert(radar);
    return radar->view;
}

void radar_view_set_data(
    RadarView* radar,
    const RadarBlip* blips,
    size_t blip_count,
    size_t seen,
    size_t following,
    bool esp_connected) {
    furi_assert(radar);
    with_view_model(
        radar->view,
        RadarModel * m,
        {
            size_t n = blip_count < RADAR_MAX_BLIPS ? blip_count : RADAR_MAX_BLIPS;
            for(size_t i = 0; i < n; i++) m->blips[i] = blips[i];
            m->blip_count = n;
            m->seen = seen;
            m->following = following;
            m->esp_connected = esp_connected;
        },
        true);
}

void radar_view_tick(RadarView* radar) {
    furi_assert(radar);
    with_view_model(
        radar->view,
        RadarModel * m,
        {
            m->sweep += 7;
            m->anim++;
        },
        true);
}

void radar_view_set_ok_callback(RadarView* radar, RadarViewCallback cb, void* context) {
    furi_assert(radar);
    radar->ok_cb = cb;
    radar->ok_ctx = context;
}
