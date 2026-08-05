// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt and FlipDeFlock contributors
#include "flock_view.h"
#include "../recon_app_i.h"
#include "../helpers/gps_link.h"
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
    char ftype; /**< P/B/R/O/F/L -- 'L' is the BLE radio, everything else Wi-Fi */
    bool acoustic; /**< SoundThinking sensor, not an ALPR -> "ST" tag on the row */
    bool hidden; /**< beacons with no SSID -> "[hid]" instead of a blank name */
    char ssid[RECON_SSID_LEN];
    uint8_t mac[6];
    int8_t rssi;
    bool marked;
    bool selected;
    bool archived; /**< restored from hits.csv, not seen yet this session */
    uint32_t seen_epoch; /**< RTC seconds of that stored sighting (archived only) */
} FlockRowSnap;

/**
 * Compact age of a stored sighting: "5m", "3h", "2d", or "old" past 99 days.
 * Shown in place of the signal bars for an archived row -- a saved RSSI is not
 * a live reading, and drawing bars for it would claim the device is in range
 * right now.
 */
static void flock_age_str(char* out, size_t out_len, uint32_t now_epoch, uint32_t seen_epoch) {
    if(!seen_epoch || now_epoch < seen_epoch) {
        snprintf(out, out_len, "--");
        return;
    }
    uint32_t s = now_epoch - seen_epoch;
    if(s < 3600u) {
        snprintf(out, out_len, "%lum", (unsigned long)(s / 60u));
    } else if(s < 86400u) {
        snprintf(out, out_len, "%luh", (unsigned long)(s / 3600u));
    } else if(s < 86400u * 100u) {
        snprintf(out, out_len, "%lud", (unsigned long)(s / 86400u));
    } else {
        snprintf(out, out_len, "old");
    }
}

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
    uint32_t hits = app->esp_hits;
    uint8_t channel = app->esp_channel;
    uint32_t lines = app->esp_lines;
    int32_t frame_rate = app->esp_frame_rate;
    uint32_t ble_seen = app->esp_ble_seen;
    uint32_t ble_scans = app->esp_ble_scans;
    uint32_t alerts = app->alert_fired;
    bool warn_dismissed = app->warn_dismissed;
    uint32_t reboots = app->esp_reboots;
    uint32_t deauths = app->esp_deauths;
    bool proto_mismatch = app->esp_proto_mismatch;
    uint8_t proto_version = app->esp_proto_version;
    uint32_t dropped = app->esp_dropped_lines;
    bool port_busy = (app->esp_link_state == EspLinkPortBusy);
    bool generic = (app->settings.backend == EspBackendGeneric);
    bool gps_enabled = app->settings.gps_enabled;
    bool gps_valid = app->gps_valid;
    int gps_sats = app->gps_sats;
    // GPS is on but can never produce a fix. Two distinct causes, one badge:
    //
    //  1. GPS Port is set to the SAME UART as the ESP. scan_session_gps_start()
    //     refuses that outright (it would steal the ESP's port) and returns
    //     without allocating app->gps -- silently, until now. This is a pure
    //     configuration test, which is why it reads settings and not link state.
    //  2. The port is real but held by something else, so the acquire failed.
    //     gps_link latches that separately.
    //
    // Either way the old UI showed a hollow "searching" badge forever. A user on
    // issue #5 pointed GPS at pins 13/14 (the ESP's own USART), got no feedback
    // at all, and reasonably concluded GPS was broken.
    // Only the Flipper-UART source can hit either of these: the companion source
    // has no second port to clash over or fail to acquire, so with it selected a
    // missing fix genuinely is "searching" (or the companion isn't relaying).
    bool gps_busy =
        app->settings.gps_source == ReconGpsSourceFlipper &&
        ((app->settings.gps_uart == app->settings.esp_uart) || gps_link_port_busy(app->gps));

    // The companion path's equivalent, which until v0.54 did not exist: with the
    // ESP32 selected as the GPS source, EVERY failure rendered as the hollow
    // "searching" badge forever -- relay refused, firmware too old to have a
    // relay, wrong pin, wrong baud, no sky view. Four distinct problems, one
    // indistinguishable symptom, which is most of why issue #5's GPS report took
    // four rounds to resolve.
    //
    // Two verdicts are now separable, and they need different fixes:
    //   gps_relay_bad  -> the companion ANSWERED and said it is not relaying, so
    //                     it refused the pin (it rejects 0/1/3 and >=48). Change
    //                     the pin.
    //   gps_relay_mute -> nothing came back at all. The firmware predates the
    //                     relay, or is not the FlipDeFlock companion. Reflash.
    // Everything else -- right pin, wrong baud, no antenna, indoors -- is a real
    // "searching", and the badge deliberately keeps saying so rather than
    // guessing at a cause it cannot observe.
    bool gps_companion = app->settings.gps_enabled &&
                         app->settings.gps_source == ReconGpsSourceCompanion;
    // Selecting the companion as the GPS source while running Marauder asks for a
    // relay from firmware that has no such command. Nothing will ever arrive.
    bool gps_relay_bad = gps_companion && (generic || app->gps_relay == ReconGpsRelayOff);
    // Only after the ack window, and only once the link is actually up: before
    // that, silence means "still connecting", not "wrong firmware".
    bool gps_relay_mute = gps_companion && !generic && connected &&
                          app->gps_relay == ReconGpsRelayUnknown && app->gps_cfg_tick &&
                          (furi_get_tick() - app->gps_cfg_tick) > furi_ms_to_ticks(4000);

    // One place decides both the badge and the explanation, so the two can never
    // describe different faults.
    const char* fault_title = NULL;
    const char* fault_msg = NULL;
    const char* fault_fix = NULL;
    if(gps_relay_mute) {
        fault_title = "!FW  no GPS relay";
        fault_msg = "Companion never answered.";
        fault_fix = "Reflash: ESP32 Firmware";
    } else if(gps_relay_bad) {
        fault_title = "!PIN  refused";
        fault_msg = "Board rejected that pin.";
        fault_fix = "Settings > ESP GPS Pin";
    } else if(gps_busy) {
        fault_title = "!PORT  UART clash";
        fault_msg = "GPS and ESP share a port.";
        fault_fix = "Settings > GPS Port";
    }

    app->gps_fault_active = (fault_msg != NULL);

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
            r->ftype = e->ftype;
            r->acoustic = (e->dev_class == FlockClassAcoustic);
            r->hidden = e->hidden;
            strncpy(r->ssid, e->ssid, RECON_SSID_LEN - 1);
            r->ssid[RECON_SSID_LEN - 1] = '\0';
            memcpy(r->mac, e->mac, 6);
            r->rssi = e->rssi;
            r->marked = e->marked;
            r->selected = (idx == model->selected);
            r->archived = e->archived;
            r->seen_epoch = e->seen_epoch;
        }
    }

    furi_mutex_release(app->mutex);

    // Wall clock for the archived rows' age column. Read outside the lock (it is
    // an RTC register read, not shared app state).
    uint32_t now_epoch = furi_hal_rtc_get_timestamp();

    // ---- render from the snapshot (no mutex held) --------------------------
    // Header / status bar. A real deauth flood takes over the header. Compact
    // right-aligned status for the inverted title bar.
    //
    // EVERY COUNTER APPEARS EXACTLY ONCE across the two header lines (issue #5):
    // channel and hits live up here, frames/rx on the sub-line. They used to be
    // printed in both places, which cost the sub-line the width it needed and
    // pushed the row text into an overrun. "FLOCK/ALPR" loses its spaces for the
    // same two characters.
    //
    // Channel is space-padded to a fixed 3 (1-14 / 36-165 / 6 GHz up to 233 on a
    // C5) so the right-aligned block stops jittering as the sweep hops.
    char right[16]; // fits "ch165 h999999" + NUL; snprintf truncates safely beyond
    if(deauths >= DEAUTH_FLOOD_MIN) {
        snprintf(right, sizeof(right), "!DEAUTH");
    } else if(generic) {
        snprintf(right, sizeof(right), "rx%lu", (unsigned long)lines);
    } else {
        snprintf(right, sizeof(right), "ch%3u h%lu", channel, (unsigned long)hits);
    }
    ui_title_bar_icon(canvas, UiIconCamera, "FDF", right); // leaves color=black, font=Secondary

    // Status sub-line: only what the title bar does NOT already show.
    // A wire-protocol version mismatch is the highest-priority health warning (the
    // data may be mis-parsed), then a deauth flood. A non-zero dropped-line count
    // (overlong RX lines) is appended as a "!dN" health suffix on the normal lines.
    char drop[16] = "";
    if(dropped) snprintf(drop, sizeof(drop), " !d%lu", (unsigned long)dropped);
    char hdr[64]; // the non-icon variants (proto mismatch / deauth / Marauder)
    // The normal companion line is drawn as SEGMENTS, not one string, because two
    // of its fields are glyphs. Reusing the row icons rather than the letters
    // "rx" and "b" was a user's suggestion and it is strictly better: the same
    // mark already means Wi-Fi and BLE on every row below, so the header stops
    // needing its own vocabulary. Their mock-up put the icons BESIDE the labels,
    // which is wider than what it replaced -- these replace them.
    bool icon_line = false;
    char rate_s[10] = "";
    char ble_s[10] = "";
    char tail_s[40] = ""; // a<n> + optional !r<n> + optional !d<n>
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
        // Companion status counters stay 0 on a Marauder board, so the title bar
        // carries the RX heartbeat there and the detection count belongs here.
        snprintf(hdr, sizeof(hdr), "%s  hits %zu%s", connected ? "ESP" : "...", count, drop);
    } else {
        // Live activity, not a lifetime total. "frames 319" only ever climbed, so
        // it told you the link was up and nothing about whether the radio was
        // hearing anything RIGHT NOW -- the actual question while parked next to a
        // camera that is not showing up (issue #5).
        //
        //   rx<n>/s  Wi-Fi frames per second. "--" until two status lines land.
        //   b<n>     BLE adverts this session. The Flock screen showed NOTHING
        //            about BLE, so a working BLE half and one that never ran were
        //            indistinguishable -- and BLE is the easy detection on these.
        //   !r<n>    the companion RESTARTED n times. A lifetime counter can only
        //            fall if the board rebooted; that used to be absorbed silently
        //            and just looked like the number sliding back to zero.
        // Clamped, not just formatted: the compiler cannot prove a uint32_t fits
        // these buffers, and a header field that can grow without bound is a bug
        // waiting for a long drive.
        // Spaces dropped after each tag on a user's suggestion: the sub-line has
        // to clear the GPS badge, and "b" is unambiguous next to a digit.
        //
        //   rx<n>/s  Wi-Fi frames per second, "--" until two status lines land.
        //   b<n>     BLE adverts. "b-" means NO BLE scan phase has completed yet,
        //            which a bare 0 could not distinguish from "BLE ran and heard
        //            nothing" -- and that ambiguity is exactly what left a user
        //            unable to tell whether his BLE half worked at all.
        //   a<n>     alerts DELIVERED. The app firing and the Flipper's own
        //            notification settings swallowing it are different faults with
        //            different fixes, and "no beep" was reported three times with
        //            no way to see which one it was.
        //   !r<n>    the companion restarted.
        char rst[10] = "";
        if(reboots) snprintf(rst, sizeof(rst), " !r%u", (unsigned)(reboots > 99 ? 99 : reboots));
        if(frame_rate < 0) {
            snprintf(rate_s, sizeof(rate_s), "--/s");
        } else {
            snprintf(
                rate_s, sizeof(rate_s), "%u/s", (unsigned)(frame_rate > 9999 ? 9999 : frame_rate));
        }
        if(!ble_scans) {
            snprintf(ble_s, sizeof(ble_s), "-");
        } else {
            snprintf(ble_s, sizeof(ble_s), "%u", (unsigned)(ble_seen > 9999 ? 9999 : ble_seen));
        }
        snprintf(
            tail_s, sizeof(tail_s), "a%u%s%s", (unsigned)(alerts > 99 ? 99 : alerts), rst, drop);
        icon_line = true;
    }
    canvas_set_font(canvas, FontSecondary);

    // GPS state as a badge, not a code (issue #5). Four states, each visually
    // distinct so it reads at a glance in a moving car:
    //   filled "GPS n"  -- locked, n satellites (a fresh 2D fix vs a solid one)
    //   hollow "GPS"    -- enabled, searching
    //   filled "GPS!"   -- enabled but it can NEVER get a fix; go fix the config
    //   filled "GPS?"   -- companion source, but the board never answered the
    //                      relay config: wrong/old firmware, so reflash it
    // The last two used to render as the hollow "searching" badge forever, which
    // is indistinguishable from a cold start.
    // Built BEFORE the sub-line is drawn, because its width is what the sub-line
    // has to stop short of. A user counted the remaining gap by hand off a photo
    // to work out whether another field would fit; the code should be the one
    // measuring that, not him.
    char gps_str[12] = "";
    int gps_w = 0;
    if(gps_enabled) {
        // A FAULT BADGE MUST NOT START WITH THE WORD "GPS".
        //
        // "GPS!" and "GPS?" were both read as the GPS being on and working -- the
        // reporter of the original GPS bug looked at a filled "GPS!" and said his
        // board was "showing a gps lock". That is the exact opposite of what it
        // meant, and it is not his misreading: a filled badge is how this header
        // says "locked, n satellites", so a filled badge whose first three
        // characters are G-P-S reads as a lock at a glance. The punctuation was
        // carrying the entire meaning and lost.
        //
        // Each fault now NAMES THE THING TO FIX, and none of them says "GPS":
        //   !PORT  the Flipper's GPS and ESP are on the same UART, or the port is
        //          held -- change GPS Port
        //   !PIN   the companion answered and refused that pin -- change ESP GPS Pin
        //   !FW    the companion never answered at all -- reflash it
        // The leading "!" is this app's existing warning mark (!DEAUTH, !r, !d).
        if(gps_relay_mute) {
            snprintf(gps_str, sizeof(gps_str), "!FW");
        } else if(gps_relay_bad) {
            snprintf(gps_str, sizeof(gps_str), "!PIN");
        } else if(gps_busy) {
            snprintf(gps_str, sizeof(gps_str), "!PORT");
        } else if(gps_valid) {
            snprintf(gps_str, sizeof(gps_str), "GPS %d", gps_sats);
        } else {
            snprintf(gps_str, sizeof(gps_str), "GPS");
        }
        gps_w = canvas_string_width(canvas, gps_str) + 4;
    }
    int sub_limit = gps_enabled ? (128 - gps_w - 3) : 126;

    if(icon_line) {
        // Wi-Fi glyph + rate, BLE glyph + count, then the plain-text tail. Each
        // segment is placed from the measured width of the one before it, and
        // nothing is drawn past sub_limit, so the line can never grow into the
        // GPS badge however large the counters get.
        int sx = 0;
        const char* conn = connected ? "ESP" : "...";
        canvas_draw_str(canvas, sx, 22, conn);
        sx += canvas_string_width(canvas, conn) + 3;
        if(sx + UI_RADIO_ICON_W < sub_limit) {
            ui_icon_radio(canvas, sx, 16, false);
            sx += UI_RADIO_ICON_W;
            ui_draw_str_fit(canvas, sx, 22, rate_s, sub_limit);
            sx += canvas_string_width(canvas, rate_s) + 4;
        }
        if(sx + UI_RADIO_ICON_W < sub_limit) {
            ui_icon_radio(canvas, sx, 16, true);
            sx += UI_RADIO_ICON_W;
            ui_draw_str_fit(canvas, sx, 22, ble_s, sub_limit);
            sx += canvas_string_width(canvas, ble_s) + 4;
        }
        if(sx < sub_limit) ui_draw_str_fit(canvas, sx, 22, tail_s, sub_limit);
    } else {
        ui_draw_str_fit(canvas, 0, 22, hdr, sub_limit);
    }

    if(gps_enabled) {
        int w = gps_w;
        int x = 128 - w;
        // Fill for both "locked" and "misconfigured": a filled badge means "this
        // is settled, stop waiting for it" either way, and the glyph says which.
        if(gps_valid || gps_busy || gps_relay_bad || gps_relay_mute) {
            canvas_draw_box(canvas, x, 14, w, 10);
            canvas_set_color(canvas, ColorWhite);
        } else {
            canvas_draw_frame(canvas, x, 14, w, 10);
        }
        canvas_draw_str(canvas, x + 2, 22, gps_str);
        canvas_set_color(canvas, ColorBlack);
    }
    canvas_draw_line(canvas, 0, 24, 128, 24);

    // A fault explains itself HERE, where you meet it, once per session.
    //
    // The badge names what is wrong in five characters, which is all the header
    // has room for and is useless on its own: a user hit !PORT and said "I don't
    // know what it means and have no way of finding out." Naming a fault without
    // saying what to do about it just relocates the confusion. The full reference
    // lives in Help & Warnings; this is the pointer to it, at the moment it
    // matters. Dismissed with OK, and only re-armed on a fresh scan session, so
    // it never becomes something to swat away every frame.
    if(fault_msg && !warn_dismissed) {
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_box(canvas, 0, 26, 128, 38);
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_frame(canvas, 0, 26, 128, 38);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 3, 36, fault_title);
        canvas_set_font(canvas, FontSecondary);
        ui_draw_str_fit(canvas, 3, 45, fault_msg, 125);
        ui_draw_str_fit(canvas, 3, 53, fault_fix, 125);
        canvas_draw_str(canvas, 3, 62, "OK dismiss - see Help");
        canvas_draw_line(canvas, 0, 24, 128, 24);
        return;
    }

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

        canvas_set_font(canvas, FontSecondary);

        // ---- right edge first: it decides how much width the name gets -------
        // RSSI as signal bars, on EVERY live row including the selected one. It
        // used to switch to raw "-82dB" text when selected, because the bars
        // helper forced ColorBlack and vanished on the inverted row; that made one
        // list show two notations for the same column (issue #5). ui_signal_bars
        // now inherits the row color, so the notation is uniform and the exact dBm
        // lives on the detail screen.
        //
        // An ARCHIVED row shows the age of the stored sighting instead. Its RSSI
        // was recorded on some earlier run, so bars (or a live-looking "-67dB")
        // would assert the device is in range right now -- exactly the kind of
        // over-claim the detections-are-indicators rule exists to prevent.
        int text_max_x;
        if(r->archived) {
            char meta[18];
            char age[8];
            flock_age_str(age, sizeof(age), now_epoch, r->seen_epoch);
            snprintf(meta, sizeof(meta), "%s%s", r->marked ? "*" : "", age);
            canvas_draw_str_aligned(canvas, 126, y + 8, AlignRight, AlignBottom, meta);
            text_max_x = 126 - canvas_string_width(canvas, meta) - 3;
        } else {
            if(r->marked) {
                // marked indicator just left of the bars
                canvas_draw_str(canvas, 96, y + 8, "*");
            }
            ui_signal_bars(canvas, 104, y - 1, r->rssi); // cell ~104..114, baseline y+7
            text_max_x = r->marked ? 94 : 102;
        }

        // ---- left: confidence rung, radio glyph, then the name ---------------
        // The rung and the glyph are drawn as fixed cells rather than being
        // sprintf'd into the string, because one of them is not text. Which radio
        // saw a device is otherwise unknowable from the row (issue #5): an OUI and
        // an RSSI look identical either way, and "has an SSID" is not the tell --
        // hidden APs and probe requests have no name and still came in on Wi-Fi.
        char cbuf[2] = {r->conf_ch, '\0'};
        canvas_draw_str(canvas, 2, y + 8, cbuf);
        ui_icon_radio(canvas, 8, y + 1, r->ftype == 'L');

        // "ST " marks a SoundThinking acoustic sensor. Untagged rows are ALPR
        // cameras -- the common case stays as terse as it was, and the list never
        // silently presents a gunshot sensor as a camera.
        const char* cls = r->acoustic ? "ST " : "";

        char line[48];
        if(r->ssid[0] != '\0') {
            snprintf(line, sizeof(line), "%s%s", cls, r->ssid);
        } else if(r->hidden) {
            // We watched this one beacon without a name. Worth surfacing, but it
            // is an observation only -- the conf char is unchanged by it. Drops
            // the MAC to its last 3 bytes to make room for the tag.
            snprintf(
                line, sizeof(line), "%s[hid] %02X:%02X:%02X", cls, r->mac[3], r->mac[4], r->mac[5]);
        } else {
            snprintf(
                line,
                sizeof(line),
                "%s%02X:%02X:%02X:%02X:%02X:%02X",
                cls,
                r->mac[0],
                r->mac[1],
                r->mac[2],
                r->mac[3],
                r->mac[4],
                r->mac[5]);
        }
        // Measured trim, not a hoped-for fit: the glyph cost the name ~7 px, and a
        // full 32-char SSID never fitted in the first place. Both used to be drawn
        // straight through the bars and off the right edge.
        ui_draw_str_fit(canvas, 17, y + 8, line, text_max_x);
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
            // The fault panel owns OK while it is up: the first press is far more
            // likely to be "I have read this" than "open a detail screen I cannot
            // even see right now".
            ReconApp* app = NULL;
            with_view_model(fv->view, FlockViewModel * model, { app = model->app; }, false);
            if(app) {
                furi_mutex_acquire(app->mutex, FuriWaitForever);
                bool showing = !app->warn_dismissed && app->gps_fault_active;
                if(showing) app->warn_dismissed = true;
                furi_mutex_release(app->mutex);
                if(showing) return true;
            }
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
