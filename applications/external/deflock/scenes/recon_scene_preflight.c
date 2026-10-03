// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt
//
// One screen that answers the field question the detector previously could not:
// "is a zero-hit session empty, or broken?" It proves the UART, paired builds,
// wire protocol, signature-table revision, production signature matcher, and RF
// receive counter before allowing a companion-backed detection session.
#include "../recon_app_i.h"
#include "../helpers/alerts.h"
#include "../helpers/esp_link.h"
#include "../helpers/esp_parser.h"
#include "../helpers/preflight.h"
#include "../helpers/scan_session.h"

#include <gui/modules/widget.h>
#include <storage/storage.h>
#include <string.h>

typedef enum {
    PreflightEventRetry = 100,
    PreflightEventPage,
    PreflightEventDetect,
    PreflightEventTest,
} PreflightEvent;

typedef struct {
    uint8_t backend;
    uint8_t link_state;
    uint8_t proto;
    uint8_t band;
    bool connected;
    bool proto_mismatch;
    bool sig_mismatch;
    bool sigtest_seen;
    bool sigtest_pass;
    bool has_5ghz;
    bool has_ble; /**< the chip has a BLE radio at all (the S2 does not) */
    bool sd_ok; /**< the card is mounted, so hits and survey can be written */
    bool gps_enabled;
    bool gps_valid;
    uint8_t gps_source;
    uint8_t gps_relay;
    int gps_sats;
    uint16_t channels;
    uint32_t frames;
    uint32_t lines;
    uint32_t dropped;
    uint32_t reboots;
    uint32_t sigtest_hash;
    int32_t frame_rate;
    char build[12];
    char sig_revision[16];
    char chip[12];
} PreflightSnapshot;

static void preflight_button_cb(GuiButtonType type, InputType input, void* context) {
    if(input != InputTypeShort) return;
    ReconApp* app = context;
    if(type == GuiButtonTypeLeft) {
        view_dispatcher_send_custom_event(
            app->view_dispatcher, app->preflight_page ? PreflightEventTest : PreflightEventRetry);
    } else if(type == GuiButtonTypeCenter) {
        view_dispatcher_send_custom_event(app->view_dispatcher, PreflightEventPage);
    } else if(type == GuiButtonTypeRight) {
        view_dispatcher_send_custom_event(app->view_dispatcher, PreflightEventDetect);
    }
}

static void preflight_snapshot(ReconApp* app, PreflightSnapshot* out) {
    memset(out, 0, sizeof(*out));
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    out->backend = app->settings.backend;
    out->link_state = app->esp_link_state;
    out->proto = app->esp_proto_version;
    out->band = app->esp_band_actual;
    out->connected = app->esp_connected;
    out->proto_mismatch = app->esp_proto_mismatch;
    out->sig_mismatch = app->esp_sig_mismatch;
    out->sigtest_seen = app->esp_sigtest_seen;
    out->sigtest_pass = app->esp_sigtest_pass;
    out->has_5ghz = app->esp_has_5ghz;
    out->has_ble = !recon_esp_chip_has_no_ble(app->esp_chip);
    out->gps_enabled = app->settings.gps_enabled;
    out->gps_valid = app->gps_valid;
    out->gps_source = app->settings.gps_source;
    out->gps_relay = app->gps_relay;
    out->gps_sats = app->gps_sats;
    out->channels = app->esp_band_channels;
    out->frames = app->esp_frames;
    out->lines = app->esp_lines;
    out->dropped = app->esp_dropped_lines;
    out->reboots = app->esp_reboots;
    out->sigtest_hash = app->esp_sigtest_hash;
    out->frame_rate = app->esp_frame_rate;
    snprintf(out->build, sizeof(out->build), "%s", app->esp_build);
    snprintf(out->sig_revision, sizeof(out->sig_revision), "%s", app->esp_sig_revision);
    snprintf(out->chip, sizeof(out->chip), "%s", app->esp_chip);
    furi_mutex_release(app->mutex);

    // Outside the lock on purpose: this reaches the storage service, and the
    // worker thread takes app->mutex on every companion line. A card that is
    // missing or unmounted means every hit and every survey row this session
    // produces is thrown away at save time, which is a silent way to lose a
    // whole drive -- so it belongs on the readiness screen next to the radio.
    out->sd_ok = storage_sd_status(app->storage) == FSE_OK;
}

static ReconPreflightState preflight_state(const PreflightSnapshot* s, uint32_t elapsed_ms) {
    ReconPreflightInput input = {
        .companion_backend = s->backend == EspBackendCompanion,
        .link_running = s->link_state == EspLinkRunning,
        .link_busy = s->link_state == EspLinkPortBusy,
        .connected = s->connected,
        .protocol_seen = s->proto != 0,
        .protocol_match = !s->proto_mismatch,
        .signature_seen = s->sig_revision[0] != '\0',
        .signature_match = !s->sig_mismatch,
        .sigtest_seen = s->sigtest_seen,
        .sigtest_pass = s->sigtest_pass,
        .frames = s->frames,
        .elapsed_ms = elapsed_ms,
    };
    return recon_preflight_evaluate(&input);
}

// The summary has five fixed rows above the soft keys. Keep the first row well
// under 128 px instead of feeding a prose reason to a wrapping text box: one
// wrapped title used to push the actions entirely off the physical display.
static const char* preflight_reason_short(
    const PreflightSnapshot* s,
    ReconPreflightState state,
    uint32_t elapsed_ms) {
    if(state == ReconPreflightReady) return "capture OK";
    if(state == ReconPreflightLimited) return "generic UART";
    if(s->link_state == EspLinkPortBusy) return "UART busy";
    if(s->proto && s->proto_mismatch) return "proto mismatch";
    if(s->sig_revision[0] && s->sig_mismatch) return "sig mismatch";
    if(s->sigtest_seen && !s->sigtest_pass) return "sig test failed";
    if(state == ReconPreflightWaiting) return "collecting";
    if(!s->connected) return "no companion";
    if(!s->proto) return "no protocol";
    if(!s->sig_revision[0]) return "no sig rev";
    if(!s->sigtest_seen) return "no sig test";
    if(s->frames == 0 && elapsed_ms >= RECON_PREFLIGHT_GRACE_MS) return "zero RF";
    return "incomplete";
}

static const char* preflight_state_short(ReconPreflightState state) {
    if(state == ReconPreflightReady) return "READY";
    if(state == ReconPreflightLimited) return "LIMIT";
    if(state == ReconPreflightFailed) return "FAIL";
    return "CHECK";
}

static const char* preflight_band(const PreflightSnapshot* s) {
    if(s->channels == 0) return "?";
    if(s->band == ReconEspBand5) return "5g";
    if(s->band == ReconEspBandAll) return "all";
    return "2g";
}

static void preflight_gps(const PreflightSnapshot* s, char out[12]) {
    if(!s->gps_enabled) {
        snprintf(out, 12, "off");
    } else if(s->gps_valid) {
        snprintf(out, 12, "fix%d", s->gps_sats);
    } else if(s->gps_source == ReconGpsSourceCompanion && s->gps_relay == ReconGpsRelayOff) {
        snprintf(out, 12, "relay!");
    } else {
        snprintf(out, 12, "search");
    }
}

static void preflight_draw(ReconApp* app) {
    PreflightSnapshot s;
    preflight_snapshot(app, &s);
    uint32_t elapsed = furi_get_tick() - app->preflight_started_tick;
    ReconPreflightState state = preflight_state(&s, elapsed);
    const char* actual_build = s.build[0] ? s.build : "?";
    const char* actual_sig = s.sig_revision[0] ? s.sig_revision : "?";
    const char* chip = s.chip[0] ? s.chip : "board?";
    const char* test = !s.sigtest_seen ? "?" : (s.sigtest_pass ? "PASS" : "FAIL");
    char gps[12];
    preflight_gps(&s, gps);

    char lines[5][40];
    memset(lines, 0, sizeof(lines));
    if(app->preflight_page == 0) {
        snprintf(
            lines[0],
            sizeof(lines[0]),
            "%s: %s",
            preflight_state_short(state),
            preflight_reason_short(&s, state, elapsed));
        snprintf(lines[1], sizeof(lines[1]), "App %s ESP %s", RECON_VERSION, actual_build);
        snprintf(
            lines[2],
            sizeof(lines[2]),
            "P%u/%u S%.4s/%.4s T%s",
            (unsigned)s.proto,
            (unsigned)ESP_PROTO_VERSION,
            actual_sig,
            FDF_SIGNATURE_REVISION,
            test);
        if(s.frame_rate >= 0) {
            snprintf(
                lines[3],
                sizeof(lines[3]),
                "RX%ld/s F%lu D%lu R%lu",
                (long)s.frame_rate,
                (unsigned long)s.frames,
                (unsigned long)s.dropped,
                (unsigned long)s.reboots);
        } else {
            snprintf(
                lines[3],
                sizeof(lines[3]),
                "RX? F%lu D%lu R%lu",
                (unsigned long)s.frames,
                (unsigned long)s.dropped,
                (unsigned long)s.reboots);
        }
        // The SD marker is appended only when the card is BAD. Worst-case this
        // row is already near the 128 px limit ("esp32s2 all/13 Gsearch"), and a
        // healthy card is the uninteresting case; page 1 always spells it out.
        snprintf(
            lines[4],
            sizeof(lines[4]),
            "%s %s/%u G%s%s",
            chip,
            preflight_band(&s),
            (unsigned)s.channels,
            gps,
            s.sd_ok ? "" : " SD!");
    } else {
        snprintf(lines[0], sizeof(lines[0]), "DETAILS: %s", recon_preflight_state_label(state));
        // Two rows to print one agreeing pair is a waste of a five-row screen.
        // Collapse to one while they match, and spend the row freed on the
        // capability line; split back out the moment they disagree, which is the
        // only time the two values are separately interesting.
        size_t i = 1;
        if(s.sig_revision[0] && !s.sig_mismatch) {
            snprintf(lines[i++], sizeof(lines[0]), "Sig OK %s", actual_sig);
        } else {
            snprintf(lines[i++], sizeof(lines[0]), "Sig want %s", FDF_SIGNATURE_REVISION);
            snprintf(lines[i++], sizeof(lines[0]), "Sig got  %s", actual_sig);
        }
        snprintf(
            lines[i++], sizeof(lines[0]), "Test %08lx %s", (unsigned long)s.sigtest_hash, test);
        // What this board can hear AT ALL, which no counter above can express: an
        // S2 has no BLE radio, so "no BLE hits" from one is not evidence of quiet
        // air. has_5ghz was plumbed into this screen and then never printed.
        snprintf(
            lines[i++],
            sizeof(lines[0]),
            "Cap BLE%s 5G%s SD%s",
            s.has_ble ? "+" : "-",
            s.has_5ghz ? "+" : "-",
            s.sd_ok ? "+" : "!");
        if(i < 5) {
            snprintf(
                lines[i],
                sizeof(lines[0]),
                "L%lu D%lu R%lu G%s",
                (unsigned long)s.lines,
                (unsigned long)s.dropped,
                (unsigned long)s.reboots,
                gps);
        }
    }

    Widget* widget = app->widget;
    widget_reset(widget);
    // Re-add actions first. Screen streaming can observe widget construction
    // between element additions; text-first ordering briefly produced a full
    // status page with no visible controls on every periodic refresh.
    // Page 1 trades Retry for the alert check. Retry restarts the link, which is
    // a page-0 concern, and the operator needs to know the buzzer and vibro they
    // are about to rely on actually fire -- using the CONFIGURED mode, so this
    // proves the alert they will really get rather than a generic beep.
    widget_add_button_element(
        widget,
        GuiButtonTypeLeft,
        app->preflight_page ? "Test" : "Retry",
        preflight_button_cb,
        app);
    widget_add_button_element(
        widget,
        GuiButtonTypeCenter,
        app->preflight_page ? "Back" : "More",
        preflight_button_cb,
        app);
    // Fail closed for companion mode. A generic backend may proceed only after
    // it has at least proved live serial input, which evaluates as LIMITED.
    if(state == ReconPreflightReady || state == ReconPreflightLimited) {
        widget_add_button_element(widget, GuiButtonTypeRight, "Detect", preflight_button_cb, app);
    }
    for(size_t i = 0; i < 5; i++) {
        widget_add_string_element(
            widget, 2, (int32_t)(i * 10), AlignLeft, AlignTop, FontSecondary, lines[i]);
    }
}

static void preflight_reset_health(ReconApp* app) {
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    app->esp_connected = false;
    app->esp_frames = 0;
    app->esp_hits = 0;
    app->esp_frames_prev = 0;
    app->esp_rate_tick = 0;
    app->esp_frame_rate = -1;
    app->esp_lines = 0;
    app->esp_dropped_lines = 0;
    app->esp_reboots = 0;
    app->esp_rebase = true;
    app->esp_proto_version = 0;
    app->esp_proto_mismatch = false;
    app->esp_build[0] = '\0';
    app->esp_sig_revision[0] = '\0';
    app->esp_sig_mismatch = false;
    app->esp_sigtest_hash = 0;
    app->esp_sigtest_seen = false;
    app->esp_sigtest_pass = false;
    app->esp_chip[0] = '\0';
    app->esp_band_channels = 0;
    furi_mutex_release(app->mutex);
}

static void preflight_begin(ReconApp* app, bool restart) {
    if(restart) scan_session_stop(app);
    bool fresh = app->esp == NULL;
    if(fresh) preflight_reset_health(app);

    app->preflight_started_tick = furi_get_tick();
    app->preflight_last_query_tick = app->preflight_started_tick;
    app->preflight_last_draw_tick = app->preflight_started_tick;
    app->preflight_page = 0;

    scan_session_start(app);
    if(app->settings.backend == EspBackendCompanion) {
        esp_link_send(app->esp, "flockcombo");
        esp_link_send(app->esp, "sigtest");
    }
    scan_session_gps_start(app);
    preflight_draw(app);
}

void recon_scene_preflight_on_enter(void* context) {
    ReconApp* app = context;
    preflight_begin(app, false);
    view_dispatcher_switch_to_view(app->view_dispatcher, ReconViewWidget);
}

bool recon_scene_preflight_on_event(void* context, SceneManagerEvent event) {
    ReconApp* app = context;
    if(event.type == SceneManagerEventTypeTick) {
        uint32_t now = furi_get_tick();
        PreflightSnapshot s;
        preflight_snapshot(app, &s);
        if(s.backend == EspBackendCompanion &&
           (!s.proto || !s.sig_revision[0] || !s.sigtest_seen) &&
           (uint32_t)(now - app->preflight_last_query_tick) >= 2000u) {
            // A board still booting can miss the first request. Retry boundedly
            // while evidence is absent; stop once all three replies arrive.
            esp_link_send(app->esp, "ver");
            esp_link_send(app->esp, "sigtest");
            app->preflight_last_query_tick = now;
        }
        if((uint32_t)(now - app->preflight_last_draw_tick) >= 500u) {
            preflight_draw(app);
            app->preflight_last_draw_tick = now;
        }
        return true;
    }
    if(event.type != SceneManagerEventTypeCustom) return false;

    if(event.event == PreflightEventRetry) {
        preflight_begin(app, true);
        return true;
    }
    if(event.event == PreflightEventTest) {
        furi_mutex_acquire(app->mutex, FuriWaitForever);
        uint8_t mode = app->settings.alert_mode;
        bool sound = app->settings.sound;
        furi_mutex_release(app->mutex);
        // Fired outside the lock, same reason as recon_app_alert_tick(): the
        // notification service must not stall behind the ESP worker.
        recon_alert_fire(app->notifications, mode, sound);
        return true;
    }
    if(event.event == PreflightEventPage) {
        app->preflight_page = app->preflight_page ? 0 : 1;
        preflight_draw(app);
        return true;
    }
    if(event.event == PreflightEventDetect) {
        PreflightSnapshot s;
        preflight_snapshot(app, &s);
        ReconPreflightState state =
            preflight_state(&s, furi_get_tick() - app->preflight_started_tick);
        if(state == ReconPreflightReady || state == ReconPreflightLimited) {
            // The preflight frames are proof, not part of the field session.
            // Restart so the detector's counters and diagnostics begin at zero.
            scan_session_stop(app);
            scene_manager_next_scene(app->scene_manager, ReconSceneFlock);
        }
        return true;
    }
    return false;
}

void recon_scene_preflight_on_exit(void* context) {
    ReconApp* app = context;
    widget_reset(app->widget);
    // The Main Menu owns teardown on Back, matching every other scan scene. The
    // Detect button explicitly stops first so its field counters start clean.
}
