#include "ap_list_view.h"
#include <furi.h>
#include <stdio.h>
#include <string.h>

#define LIST_MAX 32
#define ROW_H    16
#define VISIBLE  3

struct ApListView {
    View* view;
};

typedef struct {
    ApRow rows[LIST_MAX];
    size_t count;
    size_t selected;
    bool esp_connected;
    char title[20];
} ApListModel;

static const char* enc_label(uint8_t enc) {
    static const char* const t[] = {"Open", "WEP", "WPA", "WPA2", "WPA3", "?"};
    return t[enc > 5 ? 5 : enc];
}

static void ap_list_view_draw(Canvas* canvas, void* model) {
    ApListModel* m = model;
    char buf[40];

    /* header */
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, m->title[0] ? m->title : "Networks");
    canvas_set_font(canvas, FontSecondary);
    snprintf(buf, sizeof(buf), "%u", (unsigned)m->count);
    canvas_draw_str_aligned(canvas, 125, 10, AlignRight, AlignBottom, buf);
    canvas_draw_line(canvas, 0, 12, 127, 12);

    if(m->count == 0) {
        canvas_set_font(canvas, FontSecondary);
        const char* msg = m->esp_connected ? "No matching APs yet..." : "Connect ESP32 board...";
        canvas_draw_str_aligned(canvas, 64, 38, AlignCenter, AlignCenter, msg);
        return;
    }

    size_t first = 0;
    if(m->selected >= VISIBLE) first = m->selected - (VISIBLE - 1);

    for(size_t i = 0; i < VISIBLE; i++) {
        size_t idx = first + i;
        if(idx >= m->count) break;
        const ApRow* r = &m->rows[idx];
        int y = 13 + (int)i * ROW_H;
        bool sel = (idx == m->selected);

        if(sel) {
            canvas_draw_box(canvas, 0, y, 124, ROW_H);
            canvas_set_color(canvas, ColorWhite);
        }

        /* marker: throbbing-less filled '!' for a clone, dot for a clean AP */
        if(r->clone) {
            canvas_draw_line(canvas, 3, y + 2, 3, y + 8);
            canvas_draw_dot(canvas, 3, y + 10);
            canvas_draw_line(canvas, 2, y + 2, 4, y + 2);
        } else {
            canvas_draw_disc(canvas, 3, y + 6, 1);
        }

        /* top line: SSID (left) + BSSID tail (right) */
        canvas_set_font(canvas, FontSecondary);
        char ssid[11];
        if(r->ssid[0]) {
            strncpy(ssid, r->ssid, sizeof(ssid) - 1);
            ssid[sizeof(ssid) - 1] = '\0';
        } else {
            strcpy(ssid, "<hidden>");
        }
        canvas_draw_str(canvas, 9, y + 8, ssid);
        snprintf(buf, sizeof(buf), "%02X:%02X:%02X", r->bssid[3], r->bssid[4], r->bssid[5]);
        canvas_draw_str_aligned(canvas, 121, y + 8, AlignRight, AlignBottom, buf);

        /* bottom line: channel/enc/signal (left) + TWIN flag (right) */
        snprintf(
            buf, sizeof(buf), "c%u %s %ddBm", (unsigned)r->channel, enc_label(r->enc), r->rssi);
        canvas_draw_str(canvas, 9, y + 15, buf);
        if(r->clone) {
            canvas_draw_str_aligned(canvas, 121, y + 15, AlignRight, AlignBottom, "TWIN");
        }

        if(sel) canvas_set_color(canvas, ColorBlack);
    }

    /* scrollbar */
    if(m->count > VISIBLE) {
        int track = ROW_H * VISIBLE;
        int knob = track * VISIBLE / (int)m->count;
        if(knob < 4) knob = 4;
        int pos = (int)((track - knob) * m->selected / (m->count - 1));
        canvas_draw_box(canvas, 125, 13 + pos, 3, knob);
    }
}

static bool ap_list_view_input(InputEvent* event, void* context) {
    ApListView* v = context;
    bool consumed = false;
    if(event->type == InputTypeShort || event->type == InputTypeRepeat) {
        if(event->key == InputKeyUp) {
            with_view_model(
                v->view,
                ApListModel * m,
                {
                    if(m->selected > 0) m->selected--;
                },
                true);
            consumed = true;
        } else if(event->key == InputKeyDown) {
            with_view_model(
                v->view,
                ApListModel * m,
                {
                    if(m->count && m->selected + 1 < m->count) m->selected++;
                },
                true);
            consumed = true;
        }
    }
    return consumed;
}

ApListView* ap_list_view_alloc(void) {
    ApListView* v = malloc(sizeof(ApListView));
    v->view = view_alloc();
    view_set_context(v->view, v);
    view_set_draw_callback(v->view, ap_list_view_draw);
    view_set_input_callback(v->view, ap_list_view_input);
    view_allocate_model(v->view, ViewModelTypeLocking, sizeof(ApListModel));
    return v;
}

void ap_list_view_free(ApListView* v) {
    furi_assert(v);
    view_free(v->view);
    free(v);
}

View* ap_list_view_get_view(ApListView* v) {
    furi_assert(v);
    return v->view;
}

void ap_list_view_set_title(ApListView* v, const char* title) {
    furi_assert(v);
    with_view_model(
        v->view,
        ApListModel * m,
        {
            strncpy(m->title, title, sizeof(m->title) - 1);
            m->title[sizeof(m->title) - 1] = '\0';
        },
        true);
}

void ap_list_view_update(ApListView* v, const ApRow* rows, size_t count, bool esp_connected) {
    furi_assert(v);
    with_view_model(
        v->view,
        ApListModel * m,
        {
            size_t n = count < LIST_MAX ? count : LIST_MAX;
            for(size_t i = 0; i < n; i++)
                m->rows[i] = rows[i];
            m->count = n;
            m->esp_connected = esp_connected;
            if(m->selected >= n) m->selected = n ? n - 1 : 0;
        },
        true);
}
