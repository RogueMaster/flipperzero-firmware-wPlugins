// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt and FlipDeFlock contributors
#include "locator_view.h"
#include "../recon_app_i.h"
#include "ui_widgets.h"

#include <string.h>
#include <math.h>

// RSSI window mapped to the 0..100 meter. -35 dBm ~ right on top of it; -95 dBm
// ~ at the edge of range. Clamped, so the bar saturates rather than clipping.
#define LOC_RSSI_CEIL (-35)
#define LOC_RSSI_FLOOR (-95)
// A reading older than this means the target has gone quiet / out of range.
#define LOC_FRESH_MS 2500
// dB change before we call it warmer/colder (vs the smoothed average).
#define LOC_TREND_DB 2

struct LocatorView {
    View* view;
};

typedef struct {
    void* app; /**< ReconApp* */
    int8_t peak; /**< strongest RSSI seen this hunt */
    float ema; /**< smoothed RSSI for the trend */
    int8_t trend; /**< +1 warmer, -1 colder, 0 steady */
    uint32_t last_tick; /**< last reading folded in (dedupe redraws) */
    bool init;
} LocatorViewModel;

static int loc_pct(int rssi) {
    int p = (rssi - LOC_RSSI_FLOOR) * 100 / (LOC_RSSI_CEIL - LOC_RSSI_FLOOR);
    if(p < 0) p = 0;
    if(p > 100) p = 100;
    return p;
}

static void locator_view_draw_callback(Canvas* canvas, void* _model) {
    LocatorViewModel* model = _model;
    ReconApp* app = model->app;
    if(!app) return;

    // Snapshot everything shared under the lock (same discipline as guardian_view).
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    bool connected = app->esp_connected;
    bool have = app->locate_have;
    int rssi = app->locate_rssi;
    uint32_t tick = app->locate_tick;
    uint8_t kind = app->locate_kind;
    bool gps_valid = app->gps_valid;
    char label[28];
    snprintf(label, sizeof(label), "%s", app->locate_label);
    furi_mutex_release(app->mutex);

    uint32_t now = furi_get_tick();
    bool fresh = have && (now - tick) <= LOC_FRESH_MS;

    // Fold a NEW reading into peak/trend exactly once (dedupe repeat redraws).
    if(fresh && tick != model->last_tick) {
        model->last_tick = tick;
        if(!model->init) {
            model->peak = (int8_t)rssi;
            model->ema = (float)rssi;
            model->trend = 0;
            model->init = true;
        } else {
            if(rssi > model->peak) model->peak = (int8_t)rssi;
            float d = rssi - model->ema;
            model->trend = (d >= LOC_TREND_DB) ? 1 : (d <= -LOC_TREND_DB ? -1 : 0);
            model->ema = model->ema * 0.7f + rssi * 0.3f;
        }
    }

    canvas_clear(canvas);
    ui_title_bar(canvas, "LOCATOR", kind == 'b' ? "BLE" : "WiFi");

    if(!connected) {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 2, 30, "connecting ESP32...");
        canvas_draw_str(canvas, 2, 42, "hold BOOT, tap RESET");
        return;
    }

    // Target label (truncates naturally to the screen width).
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 22, label[0] ? label : "(target)");

    if(!model->init) {
        canvas_draw_str(canvas, 2, 40, "acquiring signal...");
        canvas_draw_str(canvas, 2, 52, "move around to home in");
        return;
    }

    // Big dBm + warmer/colder word.
    char dbm[12];
    snprintf(dbm, sizeof(dbm), "%ddB", fresh ? rssi : model->peak);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 36, dbm);
    canvas_set_font(canvas, FontSecondary);
    const char* word = !fresh ? "quiet" :
                       model->trend > 0 ? "WARMER" :
                       model->trend < 0 ? "colder" :
                                          "steady";
    canvas_draw_str(canvas, 78, 36, word);

    // Hot/cold meter + a peak-hold tick above it.
    int mx = 2, my = 42, mw = 124, mh = 9;
    ui_meter(canvas, mx, my, mw, mh, fresh ? loc_pct(rssi) : 0);
    int peak_x = mx + loc_pct(model->peak) * mw / 100;
    if(peak_x > mx + mw - 1) peak_x = mx + mw - 1;
    canvas_draw_line(canvas, peak_x, my - 3, peak_x, my - 1); // peak marker

    // Footer: peak hold + freshness (or the "out of range" hint), GPS optional.
    char foot[40];
    if(!fresh) {
        snprintf(foot, sizeof(foot), "out of range  best %ddB", model->peak);
    } else if(gps_valid && rssi >= model->peak) {
        snprintf(foot, sizeof(foot), "best %ddB  <- strongest here", model->peak);
    } else {
        snprintf(foot, sizeof(foot), "best %ddB  closer = louder", model->peak);
    }
    canvas_draw_str(canvas, 2, 62, foot);
}

// Locator has no list to navigate: let Back bubble up (the scene stops the hunt).
static bool locator_view_input_callback(InputEvent* event, void* context) {
    UNUSED(event);
    UNUSED(context);
    return false;
}

LocatorView* locator_view_alloc(void) {
    LocatorView* lv = malloc(sizeof(LocatorView));
    lv->view = view_alloc();
    view_set_context(lv->view, lv);
    view_allocate_model(lv->view, ViewModelTypeLocking, sizeof(LocatorViewModel));
    view_set_draw_callback(lv->view, locator_view_draw_callback);
    view_set_input_callback(lv->view, locator_view_input_callback);
    with_view_model(
        lv->view, LocatorViewModel * model, { model->app = NULL; }, false);
    return lv;
}

void locator_view_free(LocatorView* lv) {
    view_free(lv->view);
    free(lv);
}

View* locator_view_get_view(LocatorView* lv) {
    return lv->view;
}

void locator_view_set_app(LocatorView* lv, void* app) {
    with_view_model(
        lv->view, LocatorViewModel * model, { model->app = app; }, false);
}

void locator_view_reset(LocatorView* lv) {
    with_view_model(
        lv->view,
        LocatorViewModel * model,
        {
            model->peak = -128;
            model->ema = 0;
            model->trend = 0;
            model->last_tick = 0;
            model->init = false;
        },
        true);
}

void locator_view_refresh(LocatorView* lv) {
    with_view_model(
        lv->view, LocatorViewModel * model, { UNUSED(model); }, true);
}
