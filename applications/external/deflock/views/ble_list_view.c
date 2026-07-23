// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt and FlipDeFlock contributors
#include "ble_list_view.h"
#include "../recon_app_i.h"
#include "ui_widgets.h"

#include <gui/elements.h>
#include <string.h>

#define ROW_H        11
#define LIST_TOP     27
#define VISIBLE_ROWS 3
#define MAX_ACTIONS  2

struct BleListView {
    View* view;
    BleListViewOkCallback ok_cb;
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
    BleListView* owner; /**< for action/header strings (GUI-thread only) */
    int selected;
    int top;
} BleListViewModel;

static const char* ble_cat_str(uint8_t cat) {
    switch(cat) {
    case BleCatFlock:
        return "FLOCK";
    case BleCatAirTag:
        return "AirTag";
    case BleCatTile:
        return "Tile";
    case BleCatSmartTag:
        return "Tag";
    case BleCatFindMyDevice:
        return "FindMy";
    case BleCatFlipper:
        return "Flipper";
    default:
        return "BLE";
    }
}

static void ble_list_view_draw_callback(Canvas* canvas, void* _model) {
    BleListViewModel* model = _model;
    ReconApp* app = model->app;
    BleListView* v = model->owner;
    if(!app || !v) return;

    canvas_clear(canvas);

    // Title bar (inverted). Leaves color=black, font=Secondary.
    ui_title_bar(canvas, "BLE / TRACKER", v->has_right ? v->right : NULL);

    // Full status sub-line (nothing from the old header lost).
    if(v->has_header) {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 0, 22, v->header);
    }
    canvas_draw_line(canvas, 0, 24, 128, 24);

    int action_count = v->action_count;

    furi_mutex_acquire(app->mutex, FuriWaitForever);
    int data_count = (int)app->ble_count;

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

        // Device row.
        BleDevice* d = &app->ble[idx - action_count];
        char pfx[4];
        snprintf(pfx, sizeof(pfx), "%s%s", d->marked ? "*" : "", d->following ? "!" : "");
        char line[48];
        if(d->name[0]) {
            snprintf(line, sizeof(line), "%s%s %s", pfx, ble_cat_str(d->cat), d->name);
        } else {
            snprintf(
                line,
                sizeof(line),
                "%s%s %02X%02X%02X",
                pfx,
                ble_cat_str(d->cat),
                d->addr[3],
                d->addr[4],
                d->addr[5]);
        }
        canvas_draw_str(canvas, 2, y + 8, line);

        // Right edge: RSSI as signal bars; selected (inverted) row shows white
        // dB text instead (bars hardcode ColorBlack -> invisible on black).
        if(sel) {
            char meta[10];
            snprintf(meta, sizeof(meta), "%ddB", d->rssi);
            canvas_draw_str_aligned(canvas, 126, y + 8, AlignRight, AlignBottom, meta);
        } else {
            ui_signal_bars(canvas, 104, y - 1, d->rssi);
        }
    }
    canvas_set_color(canvas, ColorBlack);

    // No device rows yet -> show the scene's status (scanning) centered below
    // the action rows.
    if(data_count == 0 && v->has_status) {
        int y = LIST_TOP + action_count * ROW_H + 10;
        if(y > 60) y = 60;
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, y, AlignCenter, AlignCenter, v->status);
    }

    furi_mutex_release(app->mutex);
}

static bool ble_list_view_input_callback(InputEvent* event, void* context) {
    BleListView* v = context;
    bool handled = false;

    if(event->type == InputTypeShort || event->type == InputTypeRepeat) {
        if(event->key == InputKeyUp) {
            with_view_model(
                v->view,
                BleListViewModel * model,
                {
                    if(model->selected > 0) model->selected--;
                },
                true);
            handled = true;
        } else if(event->key == InputKeyDown) {
            with_view_model(
                v->view,
                BleListViewModel * model,
                {
                    ReconApp* app = model->app;
                    int total = v->action_count;
                    if(app) {
                        furi_mutex_acquire(app->mutex, FuriWaitForever);
                        total += (int)app->ble_count;
                        furi_mutex_release(app->mutex);
                    }
                    if(model->selected < total - 1) model->selected++;
                },
                true);
            handled = true;
        } else if(event->key == InputKeyOk && event->type == InputTypeShort) {
            int sel = 0;
            with_view_model(v->view, BleListViewModel * model, { sel = model->selected; }, false);
            if(v->ok_cb) v->ok_cb(v->ok_ctx, sel);
            handled = true;
        }
    }
    return handled;
}

BleListView* ble_list_view_alloc(void) {
    BleListView* v = malloc(sizeof(BleListView));
    memset(v, 0, sizeof(BleListView));
    v->view = view_alloc();
    view_set_context(v->view, v);
    view_allocate_model(v->view, ViewModelTypeLocking, sizeof(BleListViewModel));
    view_set_draw_callback(v->view, ble_list_view_draw_callback);
    view_set_input_callback(v->view, ble_list_view_input_callback);
    with_view_model(
        v->view,
        BleListViewModel * model,
        {
            model->app = NULL;
            model->owner = v;
            model->selected = 0;
            model->top = 0;
        },
        false);
    return v;
}

void ble_list_view_free(BleListView* v) {
    furi_assert(v);
    view_free(v->view);
    free(v);
}

View* ble_list_view_get_view(BleListView* v) {
    furi_assert(v);
    return v->view;
}

void ble_list_view_set_app(BleListView* v, void* app) {
    with_view_model(v->view, BleListViewModel * model, { model->app = app; }, false);
}

void ble_list_view_set_ok_callback(BleListView* v, BleListViewOkCallback cb, void* context) {
    v->ok_cb = cb;
    v->ok_ctx = context;
}

void ble_list_view_set_right(BleListView* v, const char* right) {
    if(right) {
        strncpy(v->right, right, sizeof(v->right) - 1);
        v->right[sizeof(v->right) - 1] = '\0';
        v->has_right = true;
    } else {
        v->has_right = false;
    }
}

void ble_list_view_set_header(BleListView* v, const char* header) {
    if(header) {
        strncpy(v->header, header, sizeof(v->header) - 1);
        v->header[sizeof(v->header) - 1] = '\0';
        v->has_header = true;
    } else {
        v->has_header = false;
    }
}

void ble_list_view_set_status(BleListView* v, const char* status) {
    if(status) {
        strncpy(v->status, status, sizeof(v->status) - 1);
        v->status[sizeof(v->status) - 1] = '\0';
        v->has_status = true;
    } else {
        v->has_status = false;
    }
}

void ble_list_view_set_actions(BleListView* v, const char* const* labels, int count) {
    if(count > MAX_ACTIONS) count = MAX_ACTIONS;
    if(count < 0) count = 0;
    for(int i = 0; i < count; i++)
        v->actions[i] = labels[i];
    v->action_count = count;
}

void ble_list_view_refresh(BleListView* v) {
    with_view_model(v->view, BleListViewModel * model, { UNUSED(model); }, true);
}

void ble_list_view_reset(BleListView* v) {
    with_view_model(
        v->view,
        BleListViewModel * model,
        {
            model->selected = 0;
            model->top = 0;
        },
        true);
}
