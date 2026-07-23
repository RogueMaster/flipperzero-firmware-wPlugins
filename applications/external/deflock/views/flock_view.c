// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt and FlipDeFlock contributors
#include "flock_view.h"
#include "../recon_app_i.h"
#include "ui_widgets.h"

#include <gui/elements.h>

#define ROW_H            11
#define LIST_TOP         27
#define VISIBLE_ROWS     3
// Deauth/disassoc frames per ~1s interval needed to call it a flood. Normal
// roaming/idle churn is 1-2/s; a real flood is many. Below this we don't alert
// (avoids false positives on benign disassoc churn).
#define DEAUTH_FLOOD_MIN 5

struct FlockView {
    View* view;
    FlockViewOkCallback ok_cb;
    void* ok_ctx;
};

typedef struct {
    void* app; /**< ReconApp* */
    int selected;
    int top;
} FlockViewModel;

static char confidence_char(FlockConfidence c) {
    switch(c) {
    case FlockConfidenceConfirmed:
        return '!';
    case FlockConfidenceProbeFp:
        return 'F'; // B1 IE-fingerprint class match
    case FlockConfidenceLikely:
        return 'L';
    case FlockConfidencePossible:
        return 'p';
    default:
        return '.';
    }
}

// One visible list row, copied out of the shared table under the mutex so the
// render pass can run entirely unlocked.
typedef struct {
    char conf_ch;
    char ssid[RECON_SSID_LEN];
    uint8_t mac[6];
    int8_t rssi;
    bool marked;
    bool selected;
} FlockRowSnap;

static void flock_view_draw_callback(Canvas* canvas, void* _model) {
    FlockViewModel* model = _model;
    ReconApp* app = model->app;
    if(!app) return;

    canvas_clear(canvas);

    // ---- snapshot live data under the mutex; do ALL snprintf/canvas AFTER ----
    // Holding app->mutex across the whole canvas render stalls the ESP worker
    // every frame; copy the scalars, the deauth attribution and the <=3 visible
    // rows into locals (cheap, no canvas/snprintf), release, then draw. Same
    // pattern as flock_map_view.c.
    FlockRowSnap rows[VISIBLE_ROWS];
    int nrows = 0;

    furi_mutex_acquire(app->mutex, FuriWaitForever);

    size_t count = app->flock_count;
    bool connected = app->esp_connected;
    uint32_t frames = app->esp_frames;
    uint32_t hits = app->esp_hits;
    uint8_t channel = app->esp_channel;
    uint32_t lines = app->esp_lines;
    uint32_t deauths = app->esp_deauths;
    bool proto_mismatch = app->esp_proto_mismatch;
    uint8_t proto_version = app->esp_proto_version;
    uint32_t dropped = app->esp_dropped_lines;
    bool port_busy = (app->esp_link_state == EspLinkPortBusy);
    bool generic = (app->settings.backend == EspBackendGeneric);
    bool gps_enabled = app->settings.gps_enabled;
    bool gps_valid = app->gps_valid;
    int gps_sats = app->gps_sats;

    // Most-attacked BSSID + channel for the deauth header attribution.
    bool have_attr = false;
    uint8_t attr_ch = 0, attr_b3 = 0, attr_b4 = 0, attr_b5 = 0;
    {
        int top = -1;
        uint32_t topc = 0;
        for(size_t i = 0; i < app->deauth_count; i++) {
            if(app->deauth[i].count > topc) {
                topc = app->deauth[i].count;
                top = (int)i;
            }
        }
        if(top >= 0) {
            DeauthTarget* t = &app->deauth[top];
            have_attr = true;
            attr_ch = t->channel;
            attr_b3 = t->bssid[3];
            attr_b4 = t->bssid[4];
            attr_b5 = t->bssid[5];
        }
    }

    // Clamp selection/scroll (touches only the view model) then copy the visible
    // rows, so the render loop below needs no lock.
    if(count > 0) {
        if(model->selected >= (int)count) model->selected = count - 1;
        if(model->selected < 0) model->selected = 0;
        if(model->selected < model->top) model->top = model->selected;
        if(model->selected >= model->top + VISIBLE_ROWS)
            model->top = model->selected - VISIBLE_ROWS + 1;
        if(model->top < 0) model->top = 0;

        for(int row = 0; row < VISIBLE_ROWS; row++) {
            int idx = model->top + row;
            if(idx >= (int)count) break;
            FlockEntry* e = &app->flock[idx];
            FlockRowSnap* r = &rows[nrows++];
            r->conf_ch = confidence_char(e->confidence);
            strncpy(r->ssid, e->ssid, RECON_SSID_LEN - 1);
            r->ssid[RECON_SSID_LEN - 1] = '\0';
            memcpy(r->mac, e->mac, 6);
            r->rssi = e->rssi;
            r->marked = e->marked;
            r->selected = (idx == model->selected);
        }
    }

    furi_mutex_release(app->mutex);

    // ---- render from the snapshot (no mutex held) --------------------------
    // Header / status bar. A real deauth flood takes over the header. Compact
    // right-aligned status for the inverted title bar.
    char right[16]; // fits "ch165 h999999" + NUL; snprintf truncates safely beyond
    if(deauths >= DEAUTH_FLOOD_MIN) {
        snprintf(right, sizeof(right), "!DEAUTH");
    } else if(generic) {
        snprintf(right, sizeof(right), "rx %lu", (unsigned long)lines);
    } else {
        snprintf(right, sizeof(right), "ch%u h%lu", channel, (unsigned long)hits);
    }
    ui_title_bar(canvas, "FLOCK / ALPR", right); // leaves color=black, font=Secondary

    // Fuller status sub-line under the bar (nothing from the old header lost).
    // A wire-protocol version mismatch is the highest-priority health warning (the
    // data may be mis-parsed), then a deauth flood. A non-zero dropped-line count
    // (overlong RX lines) is appended as a "!dN" health suffix on the normal lines.
    char drop[16] = "";
    if(dropped) snprintf(drop, sizeof(drop), " !d%lu", (unsigned long)dropped);
    char hdr[48];
    if(proto_mismatch) {
        snprintf(hdr, sizeof(hdr), "! Companion FW proto v%u mismatch", proto_version);
    } else if(deauths >= DEAUTH_FLOOD_MIN) {
        if(have_attr) {
            snprintf(
                hdr, sizeof(hdr), "!DEAUTH ch%u %02X%02X%02X", attr_ch, attr_b3, attr_b4, attr_b5);
        } else {
            snprintf(
                hdr,
                sizeof(hdr),
                "%s DEAUTH! x%lu",
                connected ? "ESP" : "...",
                (unsigned long)deauths);
        }
    } else if(generic) {
        // Companion status counters stay 0 on a Marauder board; show the RX
        // line heartbeat and the detection count instead.
        snprintf(
            hdr,
            sizeof(hdr),
            "%s  rx %lu  hits %zu%s",
            connected ? "ESP" : "...",
            (unsigned long)lines,
            count,
            drop);
    } else {
        snprintf(
            hdr,
            sizeof(hdr),
            "%s ch%u  frames %lu  hits %lu%s",
            connected ? "ESP" : "...",
            channel,
            (unsigned long)frames,
            (unsigned long)hits,
            drop);
    }
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 0, 22, hdr);

    char gps_str[12];
    if(gps_enabled) {
        if(gps_valid) {
            snprintf(gps_str, sizeof(gps_str), "G:%d", gps_sats);
        } else {
            snprintf(gps_str, sizeof(gps_str), "G:-");
        }
        canvas_draw_str_aligned(canvas, 128, 22, AlignRight, AlignBottom, gps_str);
    }
    canvas_draw_line(canvas, 0, 24, 128, 24);

    if(count == 0) {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(
            canvas,
            64,
            44,
            AlignCenter,
            AlignCenter,
            connected ? "Scanning for ALPR..." :
            port_busy ? "UART busy - check port" :
                        "Connect ESP32...");
        return;
    }

    for(int row = 0; row < nrows; row++) {
        FlockRowSnap* r = &rows[row];
        int y = LIST_TOP + row * ROW_H;
        if(r->selected) {
            canvas_set_color(canvas, ColorBlack);
            canvas_draw_box(canvas, 0, y - 1, 128, ROW_H);
            canvas_set_color(canvas, ColorWhite);
        } else {
            canvas_set_color(canvas, ColorBlack);
        }

        char line[48];
        if(r->ssid[0] != '\0') {
            snprintf(line, sizeof(line), "%c %s", r->conf_ch, r->ssid);
        } else {
            snprintf(
                line,
                sizeof(line),
                "%c %02X:%02X:%02X:%02X:%02X:%02X",
                r->conf_ch,
                r->mac[0],
                r->mac[1],
                r->mac[2],
                r->mac[3],
                r->mac[4],
                r->mac[5]);
        }
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 2, y + 8, line);

        // Right edge: RSSI as signal bars (replaces the raw "-33dB"). The bars
        // helper hardcodes ColorBlack, so on the inverted (selected) row it
        // would be invisible -> show the exact dB as white text there instead.
        if(r->selected) {
            char meta[18];
            if(r->marked) {
                snprintf(meta, sizeof(meta), "*%ddB", r->rssi);
            } else {
                snprintf(meta, sizeof(meta), "%ddB", r->rssi);
            }
            canvas_draw_str_aligned(canvas, 126, y + 8, AlignRight, AlignBottom, meta);
        } else {
            if(r->marked) {
                // marked indicator just left of the bars
                canvas_draw_str(canvas, 96, y + 8, "*");
            }
            ui_signal_bars(canvas, 104, y - 1, r->rssi); // cell ~104..114, baseline y+7
        }
    }
    canvas_set_color(canvas, ColorBlack);
}

static bool flock_view_input_callback(InputEvent* event, void* context) {
    FlockView* fv = context;
    bool handled = false;

    if(event->type == InputTypeShort || event->type == InputTypeRepeat) {
        if(event->key == InputKeyUp) {
            with_view_model(
                fv->view,
                FlockViewModel * model,
                {
                    if(model->selected > 0) model->selected--;
                },
                true);
            handled = true;
        } else if(event->key == InputKeyDown) {
            with_view_model(
                fv->view,
                FlockViewModel * model,
                {
                    ReconApp* app = model->app;
                    int count = 0;
                    if(app) {
                        furi_mutex_acquire(app->mutex, FuriWaitForever);
                        count = (int)app->flock_count;
                        furi_mutex_release(app->mutex);
                    }
                    if(model->selected < count - 1) model->selected++;
                },
                true);
            handled = true;
        } else if(event->key == InputKeyOk && event->type == InputTypeShort) {
            int sel = 0;
            with_view_model(fv->view, FlockViewModel * model, { sel = model->selected; }, false);
            if(fv->ok_cb) fv->ok_cb(fv->ok_ctx, sel);
            handled = true;
        }
    }
    return handled;
}

FlockView* flock_view_alloc(void) {
    FlockView* fv = malloc(sizeof(FlockView));
    fv->ok_cb = NULL;
    fv->ok_ctx = NULL;
    fv->view = view_alloc();
    view_set_context(fv->view, fv);
    view_allocate_model(fv->view, ViewModelTypeLocking, sizeof(FlockViewModel));
    view_set_draw_callback(fv->view, flock_view_draw_callback);
    view_set_input_callback(fv->view, flock_view_input_callback);
    with_view_model(
        fv->view,
        FlockViewModel * model,
        {
            model->app = NULL;
            model->selected = 0;
            model->top = 0;
        },
        false);
    return fv;
}

void flock_view_free(FlockView* fv) {
    furi_assert(fv);
    view_free(fv->view);
    free(fv);
}

View* flock_view_get_view(FlockView* fv) {
    furi_assert(fv);
    return fv->view;
}

void flock_view_set_app(FlockView* fv, void* app) {
    with_view_model(fv->view, FlockViewModel * model, { model->app = app; }, false);
}

void flock_view_set_ok_callback(FlockView* fv, FlockViewOkCallback cb, void* context) {
    fv->ok_cb = cb;
    fv->ok_ctx = context;
}

void flock_view_refresh(FlockView* fv) {
    with_view_model(fv->view, FlockViewModel * model, { UNUSED(model); }, true);
}

void flock_view_reset(FlockView* fv) {
    with_view_model(
        fv->view,
        FlockViewModel * model,
        {
            model->selected = 0;
            model->top = 0;
        },
        true);
}
