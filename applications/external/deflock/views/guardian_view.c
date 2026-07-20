// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt and FlipDeFlock contributors
#include "guardian_view.h"
#include "../recon_app_i.h"
#include "ui_widgets.h"

#include <gui/elements.h>
#include <string.h>

struct GuardianView {
    View* view;
    void (*ok_cb)(void*); /**< short-OK -> open the Suspicious list */
    void* ok_ctx;
};

typedef struct {
    void* app; /**< ReconApp* */
} GuardianViewModel;

// Pwnagotchi mood per fused state: face + headline + idle tagline.
static const char* guardian_face(WatchState st) {
    switch(st) {
    case WatchStateElevated:
        return "(>_<)";
    case WatchStateWatchful:
        return "(o_o)";
    case WatchStateClear:
    default:
        return "(-_-)";
    }
}

static const char* guardian_word(WatchState st) {
    switch(st) {
    case WatchStateElevated:
        return "ELEVATED";
    case WatchStateWatchful:
        return "WATCHFUL";
    case WatchStateClear:
    default:
        return "CLEAR";
    }
}

static const char* guardian_mode(uint8_t phase) {
    switch(phase) {
    case 1:
        return "BLE";
    case 2:
        return "WiFi";
    case 0:
    default:
        return "WiFi+BLE";
    }
}

// Draw `text` on up to two FontSecondary lines (~21 chars each), splitting on
// the last space that fits so a long breakdown wraps cleanly instead of clipping.
static void draw_wrapped(Canvas* canvas, const char* text, int y) {
    const int kMax = 21;
    int len = (int)strlen(text);
    if(len <= kMax) {
        canvas_draw_str(canvas, 2, y, text);
        return;
    }
    int split = kMax;
    for(int i = kMax; i > 0; i--) {
        if(text[i] == ' ') {
            split = i;
            break;
        }
    }
    char a[24];
    int an = split < (int)sizeof(a) ? split : (int)sizeof(a) - 1;
    memcpy(a, text, an);
    a[an] = '\0';
    canvas_draw_str(canvas, 2, y, a);

    char b[24];
    const char* rest = text + split + (text[split] == ' ' ? 1 : 0);
    snprintf(b, sizeof(b), "%s", rest);
    canvas_draw_str(canvas, 2, y + 9, b);
}

static void guardian_view_draw_callback(Canvas* canvas, void* _model) {
    GuardianViewModel* model = _model;
    ReconApp* app = model->app;
    if(!app) return;

    // The draw runs on the GUI thread while the scene tick writes these on the
    // app thread, so snapshot everything shared under the lock (same discipline
    // as flock_view). `breakdown` is copied out so a mid-update string can't be
    // read torn.
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    bool connected = app->esp_connected;
    uint32_t hits = app->esp_hits;
    uint8_t channel = app->esp_channel;
    WatchState st = (WatchState)app->watch.state;
    int score = (int)app->watch.score; // threat meter pct (0..100)
    char bd[WATCHSCORE_BREAKDOWN_LEN];
    snprintf(bd, sizeof(bd), "%s", app->watch.breakdown);
    // Guardian HUD tallies were computed in the last watchscore tick under this
    // lock -- read them here instead of re-acquiring the mutex twice per frame.
    unsigned attacks = app->guardian_atk_n;
    unsigned flippers = app->guardian_flip_n;
    furi_mutex_release(app->mutex);

    uint8_t phase = app->guardian_phase;

    uint32_t secs = (furi_get_tick() - app->guardian_since) / 1000;
    char up[16];
    snprintf(
        up,
        sizeof(up),
        "%lu:%02lu:%02lu",
        (unsigned long)(secs / 3600),
        (unsigned long)((secs / 60) % 60),
        (unsigned long)(secs % 60));

    canvas_clear(canvas);

    // --- HUD title bar (inverted): "NET GUARDIAN" + uptime ------------------
    // Leaves color=black, font=Secondary.
    ui_title_bar(canvas, "NET GUARDIAN", up);

    if(!connected) {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 2, 30, "connecting ESP32...");
        canvas_draw_str(canvas, 2, 42, "hold BOOT, tap RESET");
        return;
    }

    // --- mascot + state word (the centerpiece) ------------------------------
    // Face on its own bigger row; state word in FontPrimary beside it. When
    // ELEVATED, the word gets a filled box (loud) but the mascot stays plain so
    // it always reads.
    canvas_set_font(canvas, FontPrimary);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_str(canvas, 2, 29, guardian_face(st));

    if(st == WatchStateElevated) {
        canvas_draw_box(canvas, 40, 18, 88, 13);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_str(canvas, 44, 28, guardian_word(st));
        canvas_set_color(canvas, ColorBlack);
    } else {
        canvas_draw_str(canvas, 44, 28, guardian_word(st));
    }

    // --- THREAT meter -------------------------------------------------------
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 42, "thr");
    ui_meter(canvas, 36, 35, 90, 8, score);

    // --- counters: Flock cameras / active attacks / Flippers ----------------
    // The three "what's near me" numbers. Flock = Flock/ALPR cameras (was "hits").
    // Atk = active attacks (deauth-flood APs, flood-gated so a benign disassoc
    // never shows). Flip = Flipper Zeros advertising nearby. Compact fixed columns
    // so all three read cleanly on the 128 px row.
    char lbl[16];
    snprintf(lbl, sizeof(lbl), "Flock %lu", (unsigned long)hits);
    canvas_draw_str(canvas, 2, 52, lbl);
    snprintf(lbl, sizeof(lbl), "Atk %u", attacks);
    canvas_draw_str(canvas, 52, 52, lbl);
    snprintf(lbl, sizeof(lbl), "Flip %u", flippers);
    canvas_draw_str(canvas, 90, 52, lbl);

    // --- breakdown (alert) / live scan status (clear) -----------------------
    // On an alert the per-signal breakdown explains WHY; when CLEAR the same row
    // shows the rotating sweep (mode + channel) so you can see it's still live.
    if(bd[0]) {
        draw_wrapped(canvas, bd, 62);
    } else {
        char status[32];
        snprintf(status, sizeof(status), "%s ch%u  OK=sus", guardian_mode(phase), channel);
        canvas_draw_str(canvas, 2, 62, status);
    }
}

// Short OK opens the Suspicious-devices list; every other key falls through so
// Back still bubbles up to the scene manager (which tears the radio down).
static bool guardian_view_input_callback(InputEvent* event, void* context) {
    GuardianView* gv = context;
    if(event->type == InputTypeShort && event->key == InputKeyOk && gv->ok_cb) {
        gv->ok_cb(gv->ok_ctx);
        return true;
    }
    return false;
}

GuardianView* guardian_view_alloc(void) {
    GuardianView* gv = malloc(sizeof(GuardianView));
    gv->view = view_alloc();
    gv->ok_cb = NULL;
    gv->ok_ctx = NULL;
    view_set_context(gv->view, gv);
    view_allocate_model(gv->view, ViewModelTypeLocking, sizeof(GuardianViewModel));
    view_set_draw_callback(gv->view, guardian_view_draw_callback);
    view_set_input_callback(gv->view, guardian_view_input_callback);
    with_view_model(gv->view, GuardianViewModel * model, { model->app = NULL; }, false);
    return gv;
}

void guardian_view_free(GuardianView* gv) {
    view_free(gv->view);
    free(gv);
}

View* guardian_view_get_view(GuardianView* gv) {
    return gv->view;
}

void guardian_view_set_app(GuardianView* gv, void* app) {
    with_view_model(gv->view, GuardianViewModel * model, { model->app = app; }, false);
}

void guardian_view_set_ok_callback(GuardianView* gv, void (*cb)(void*), void* ctx) {
    gv->ok_cb = cb;
    gv->ok_ctx = ctx;
}

void guardian_view_refresh(GuardianView* gv) {
    with_view_model(gv->view, GuardianViewModel * model, { UNUSED(model); }, true);
}
