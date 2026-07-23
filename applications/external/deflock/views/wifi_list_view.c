// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt and FlipDeFlock contributors
#include "wifi_list_view.h"
#include "../recon_app_i.h"
#include "../helpers/wifi_audit.h"
#include "ui_widgets.h"

#include <gui/elements.h>
#include <string.h>

#define ROW_H        11
#define LIST_TOP     27
#define VISIBLE_ROWS 3
#define MAX_ACTIONS  2

struct WifiListView {
    View* view;
    WifiListViewOkCallback ok_cb;
    void* ok_ctx;
    char right[14];
    char header[40];
    char status[40];
    bool has_right;
    bool has_header;
    bool has_status;
    const char* actions[MAX_ACTIONS];
    int action_count;
};

typedef struct {
    void* app; /**< ReconApp* */
    WifiListView* owner;
    int selected;
    int top;
} WifiListViewModel;

static void wifi_list_view_draw_callback(Canvas* canvas, void* _model) {
    WifiListViewModel* model = _model;
    ReconApp* app = model->app;
    WifiListView* v = model->owner;
    if(!app || !v) return;

    canvas_clear(canvas);

    // Title bar (inverted). Leaves color=black, font=Secondary.
    ui_title_bar(canvas, "WIFI AUDIT", v->has_right ? v->right : NULL);

    // Full status sub-line (nothing from the old header lost).
    if(v->has_header) {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 0, 22, v->header);
    }
    canvas_draw_line(canvas, 0, 24, 128, 24);

    int action_count = v->action_count;

    furi_mutex_acquire(app->mutex, FuriWaitForever);
    int data_count = (int)app->wifi_count;

    int total = action_count + data_count;

    // Clamp selection/scroll.
    if(model->selected >= total) model->selected = total - 1;
    if(model->selected < 0) model->selected = 0;
    if(model->selected < model->top) model->top = model->selected;
    if(model->selected >= model->top + VISIBLE_ROWS)
        model->top = model->selected - VISIBLE_ROWS + 1;
    if(model->top < 0) model->top = 0;

    for(int row = 0; row < VISIBLE_ROWS; row++) {
        int idx = model->top + row;
        if(idx >= total) break;

        int y = LIST_TOP + row * ROW_H;
        bool sel = (idx == model->selected);
        if(sel) {
            canvas_set_color(canvas, ColorBlack);
            canvas_draw_box(canvas, 0, y - 1, 128, ROW_H);
            canvas_set_color(canvas, ColorWhite);
        } else {
            canvas_set_color(canvas, ColorBlack);
        }

        canvas_set_font(canvas, FontSecondary);

        if(idx < action_count) {
            // Action row: ">" glyph + label, no bars.
            char line[40];
            snprintf(line, sizeof(line), "> %s", v->actions[idx]);
            canvas_draw_str(canvas, 2, y + 8, line);
            continue;
        }

        // AP row.
        WifiAp* a = &app->wifi[idx - action_count];
        WifiGrade g = wifi_audit_grade(a->authmode, a->pairwise, a->wps, a->ssid, NULL);
        char tw[4];
        snprintf(
            tw, sizeof(tw), "%s%s", a->marked ? "*" : "", a->rogue ? "!" : (a->dup ? "~" : ""));
        char line[48];
        if(a->ssid[0]) {
            snprintf(line, sizeof(line), "%s%s %s", wifi_grade_str(g), tw, a->ssid);
        } else {
            snprintf(
                line,
                sizeof(line),
                "%s%s [%02X%02X%02X]",
                wifi_grade_str(g),
                tw,
                a->bssid[3],
                a->bssid[4],
                a->bssid[5]);
        }
        canvas_draw_str(canvas, 2, y + 8, line);

        // Right edge: RSSI as signal bars; selected (inverted) row shows white
        // dB text instead (bars hardcode ColorBlack -> invisible on black).
        if(sel) {
            char meta[10];
            snprintf(meta, sizeof(meta), "%ddB", a->rssi);
            canvas_draw_str_aligned(canvas, 126, y + 8, AlignRight, AlignBottom, meta);
        } else {
            ui_signal_bars(canvas, 104, y - 1, a->rssi);
        }
    }
    canvas_set_color(canvas, ColorBlack);

    // No AP rows yet -> show the scene's status (scanning/timeout) centered.
    if(data_count == 0 && v->has_status) {
        int y = LIST_TOP + action_count * ROW_H + 10;
        if(y > 60) y = 60;
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, y, AlignCenter, AlignCenter, v->status);
    }

    furi_mutex_release(app->mutex);
}

static bool wifi_list_view_input_callback(InputEvent* event, void* context) {
    WifiListView* v = context;
    bool handled = false;

    if(event->type == InputTypeShort || event->type == InputTypeRepeat) {
        if(event->key == InputKeyUp) {
            with_view_model(
                v->view,
                WifiListViewModel * model,
                {
                    if(model->selected > 0) model->selected--;
                },
                true);
            handled = true;
        } else if(event->key == InputKeyDown) {
            with_view_model(
                v->view,
                WifiListViewModel * model,
                {
                    ReconApp* app = model->app;
                    int total = v->action_count;
                    if(app) {
                        furi_mutex_acquire(app->mutex, FuriWaitForever);
                        total += (int)app->wifi_count;
                        furi_mutex_release(app->mutex);
                    }
                    if(model->selected < total - 1) model->selected++;
                },
                true);
            handled = true;
        } else if(event->key == InputKeyOk && event->type == InputTypeShort) {
            int sel = 0;
            with_view_model(v->view, WifiListViewModel * model, { sel = model->selected; }, false);
            if(v->ok_cb) v->ok_cb(v->ok_ctx, sel);
            handled = true;
        }
    }
    return handled;
}

WifiListView* wifi_list_view_alloc(void) {
    WifiListView* v = malloc(sizeof(WifiListView));
    memset(v, 0, sizeof(WifiListView));
    v->view = view_alloc();
    view_set_context(v->view, v);
    view_allocate_model(v->view, ViewModelTypeLocking, sizeof(WifiListViewModel));
    view_set_draw_callback(v->view, wifi_list_view_draw_callback);
    view_set_input_callback(v->view, wifi_list_view_input_callback);
    with_view_model(
        v->view,
        WifiListViewModel * model,
        {
            model->app = NULL;
            model->owner = v;
            model->selected = 0;
            model->top = 0;
        },
        false);
    return v;
}

void wifi_list_view_free(WifiListView* v) {
    furi_assert(v);
    view_free(v->view);
    free(v);
}

View* wifi_list_view_get_view(WifiListView* v) {
    furi_assert(v);
    return v->view;
}

void wifi_list_view_set_app(WifiListView* v, void* app) {
    with_view_model(v->view, WifiListViewModel * model, { model->app = app; }, false);
}

void wifi_list_view_set_ok_callback(WifiListView* v, WifiListViewOkCallback cb, void* context) {
    v->ok_cb = cb;
    v->ok_ctx = context;
}

void wifi_list_view_set_right(WifiListView* v, const char* right) {
    if(right) {
        strncpy(v->right, right, sizeof(v->right) - 1);
        v->right[sizeof(v->right) - 1] = '\0';
        v->has_right = true;
    } else {
        v->has_right = false;
    }
}

void wifi_list_view_set_header(WifiListView* v, const char* header) {
    if(header) {
        strncpy(v->header, header, sizeof(v->header) - 1);
        v->header[sizeof(v->header) - 1] = '\0';
        v->has_header = true;
    } else {
        v->has_header = false;
    }
}

void wifi_list_view_set_status(WifiListView* v, const char* status) {
    if(status) {
        strncpy(v->status, status, sizeof(v->status) - 1);
        v->status[sizeof(v->status) - 1] = '\0';
        v->has_status = true;
    } else {
        v->has_status = false;
    }
}

void wifi_list_view_set_actions(WifiListView* v, const char* const* labels, int count) {
    if(count > MAX_ACTIONS) count = MAX_ACTIONS;
    if(count < 0) count = 0;
    for(int i = 0; i < count; i++)
        v->actions[i] = labels[i];
    v->action_count = count;
}

void wifi_list_view_refresh(WifiListView* v) {
    with_view_model(v->view, WifiListViewModel * model, { UNUSED(model); }, true);
}

void wifi_list_view_reset(WifiListView* v) {
    with_view_model(
        v->view,
        WifiListViewModel * model,
        {
            model->selected = 0;
            model->top = 0;
        },
        true);
}
