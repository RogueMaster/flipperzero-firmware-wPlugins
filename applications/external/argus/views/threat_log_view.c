#include "threat_log_view.h"
#include <furi.h>
#include <stdio.h>
#include <string.h>

#define LOG_MAX 40
#define ROW_H   13
#define VISIBLE 4

struct ThreatLogView {
    View* view;
};

typedef struct {
    LogRow rows[LOG_MAX];
    size_t count;
    size_t selected;
    bool esp_connected;
} ThreatLogModel;

static void fmt_age(char* buf, size_t n, uint32_t age_s) {
    if(age_s < 60) {
        snprintf(buf, n, "%lus", (unsigned long)age_s);
    } else if(age_s < 3600) {
        snprintf(buf, n, "%lum", (unsigned long)(age_s / 60));
    } else {
        snprintf(buf, n, "%luh", (unsigned long)(age_s / 3600));
    }
}

static void threat_log_view_draw(Canvas* canvas, void* model) {
    ThreatLogModel* m = model;
    char buf[40];
    char age[12];

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, "Threat Log");
    canvas_set_font(canvas, FontSecondary);
    snprintf(buf, sizeof(buf), "%u", (unsigned)m->count);
    canvas_draw_str_aligned(canvas, 125, 10, AlignRight, AlignBottom, buf);
    canvas_draw_line(canvas, 0, 12, 127, 12);

    if(m->count == 0) {
        const char* msg = m->esp_connected ? "All quiet. No threats." : "Connect ESP32 board...";
        canvas_draw_str_aligned(canvas, 64, 38, AlignCenter, AlignCenter, msg);
        return;
    }

    size_t first = 0;
    if(m->selected >= VISIBLE) first = m->selected - (VISIBLE - 1);

    for(size_t i = 0; i < VISIBLE; i++) {
        size_t idx = first + i;
        if(idx >= m->count) break;
        const LogRow* r = &m->rows[idx];
        int y = 13 + (int)i * ROW_H;
        int base = y + 10;
        bool sel = (idx == m->selected);

        if(sel) {
            canvas_draw_box(canvas, 0, y, 124, ROW_H);
            canvas_set_color(canvas, ColorWhite);
        }

        if(r->kind == 2) {
            char ssid[16];
            if(r->ssid[0]) {
                strncpy(ssid, r->ssid, sizeof(ssid) - 1);
                ssid[sizeof(ssid) - 1] = '\0';
            } else {
                strcpy(ssid, "<hidden>");
            }
            snprintf(buf, sizeof(buf), "TWIN %s", ssid);
        } else {
            const char* k = (r->kind == 1) ? "DISASSOC" : "DEAUTH";
            snprintf(buf, sizeof(buf), "%s c%u  %ddBm", k, (unsigned)r->channel, r->rssi);
        }
        canvas_draw_str(canvas, 3, base, buf);

        fmt_age(age, sizeof(age), r->age_s);
        canvas_draw_str_aligned(canvas, 121, base, AlignRight, AlignBottom, age);

        if(sel) canvas_set_color(canvas, ColorBlack);
    }

    if(m->count > VISIBLE) {
        int track = ROW_H * VISIBLE;
        int knob = track * VISIBLE / (int)m->count;
        if(knob < 4) knob = 4;
        int pos = (int)((track - knob) * m->selected / (m->count - 1));
        canvas_draw_box(canvas, 125, 13 + pos, 3, knob);
    }
}

static bool threat_log_view_input(InputEvent* event, void* context) {
    ThreatLogView* v = context;
    bool consumed = false;
    if(event->type == InputTypeShort || event->type == InputTypeRepeat) {
        if(event->key == InputKeyUp) {
            with_view_model(
                v->view,
                ThreatLogModel * m,
                {
                    if(m->selected > 0) m->selected--;
                },
                true);
            consumed = true;
        } else if(event->key == InputKeyDown) {
            with_view_model(
                v->view,
                ThreatLogModel * m,
                {
                    if(m->count && m->selected + 1 < m->count) m->selected++;
                },
                true);
            consumed = true;
        }
    }
    return consumed;
}

ThreatLogView* threat_log_view_alloc(void) {
    ThreatLogView* v = malloc(sizeof(ThreatLogView));
    v->view = view_alloc();
    view_set_context(v->view, v);
    view_set_draw_callback(v->view, threat_log_view_draw);
    view_set_input_callback(v->view, threat_log_view_input);
    view_allocate_model(v->view, ViewModelTypeLocking, sizeof(ThreatLogModel));
    return v;
}

void threat_log_view_free(ThreatLogView* v) {
    furi_assert(v);
    view_free(v->view);
    free(v);
}

View* threat_log_view_get_view(ThreatLogView* v) {
    furi_assert(v);
    return v->view;
}

void threat_log_view_update(ThreatLogView* v, const LogRow* rows, size_t count, bool esp_connected) {
    furi_assert(v);
    with_view_model(
        v->view,
        ThreatLogModel * m,
        {
            size_t n = count < LOG_MAX ? count : LOG_MAX;
            for(size_t i = 0; i < n; i++)
                m->rows[i] = rows[i];
            m->count = n;
            m->esp_connected = esp_connected;
            if(m->selected >= n) m->selected = n ? n - 1 : 0;
        },
        true);
}
