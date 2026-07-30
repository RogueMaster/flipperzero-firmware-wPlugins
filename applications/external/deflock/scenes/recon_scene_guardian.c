// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt and FlipDeFlock contributors
#include "../recon_app_i.h"
#include "../helpers/esp_link.h"
#include "../helpers/scan_session.h"
#include "../helpers/scene_util.h"

// "Net Guardian": a leave-it-on-the-desk monitor. It keeps the ESP worker alive
// and rotates the companion through its detection modes so EVERY WATCHSCORE
// input stays live -- dual-band Flock + deauth (flockcombo), BLE trackers
// (blescan) and evil-twin APs (wifiscan) -- then ticks the fused scorer every
// frame and surfaces it pwnagotchi-style. The Flock/ALPR scan, by contrast,
// only runs flockcombo and never ticks the scorer.

// Rotating sweep: command + how long to dwell before moving on. flockcombo is
// the primary (continuous, two independent radios + deauth) so it gets the
// longest slice; blescan/wifiscan are one-shot sweeps (~6 s) that refresh their
// arrays. Detections persist in the app arrays and the scorer fuses whatever is
// still within its freshness window, so a 38 s cycle stays inside the 60 s
// Flock / 30 s deauth windows.
static const struct {
    const char* cmd;
    uint32_t dwell_ms;
} GUARD_PHASES[] = {
    {"flockcombo", 20000},
    {"blescan", 9000},
    {"wifiscan", 9000},
};
#define GUARD_PHASE_COUNT (sizeof(GUARD_PHASES) / sizeof(GUARD_PHASES[0]))

#define GUARDIAN_EV_SUS 100 // short-OK -> open the Suspicious list

static void recon_scene_guardian_ok_cb(void* ctx) {
    ReconApp* app = ctx;
    view_dispatcher_send_custom_event(app->view_dispatcher, GUARDIAN_EV_SUS);
}

void recon_scene_guardian_on_enter(void* context) {
    ReconApp* app = context;

    // The rotating sweep (blescan/wifiscan) needs the companion protocol; in
    // Marauder mode explain and bail (Flock/ALPR Detect still works there).
    if(app->settings.backend != EspBackendCompanion) {
        app->guardian_blocked = true;
        scene_show_companion_guard(
            app,
            "Net Guardian needs the\nFlipDeFlock companion FW\n(it rotates WiFi + BLE).\n\nYou're in Marauder mode\n(Flock detect only).\nFlash via 'ESP32 Firmware'\nor switch Board Mode in\nSettings.");
        return;
    }
    app->guardian_blocked = false;

    // Fresh baseline so the guardian starts honestly CLEAR rather than off the
    // tail of a previous scan.
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    app->flock_count = 0;
    app->ble_count = 0;
    app->wifi_count = 0;
    app->deauth_count = 0;
    app->esp_deauths = 0;
    app->esp_attack_tick = 0; // no attack-tool signature carried in from a prior run
    app->esp_frames = 0;
    app->esp_hits = 0;
    app->esp_rebase = true; // per-session rebase off the companion's lifetime total
    app->esp_connected = false;
    furi_mutex_release(app->mutex);

    watchscore_init(&app->watch);
    app->guardian_since = furi_get_tick();
    app->guardian_phase = 0;
    app->guardian_phase_mark = app->guardian_since;

    // ESP first so it claims its UART; GPS only if it's on a different port.
    // scan_session_start keeps the live link across the Sus detail round-trip
    // (idempotent; see scan_session.h / bug B1) and returns true only on a FRESH
    // start, so the first sweep phase is kicked off exactly once.
    if(scan_session_start(app)) {
        esp_link_send(app->esp, GUARD_PHASES[0].cmd);
    }
    scan_session_gps_start(app);

    guardian_view_set_ok_callback(app->guardian_view, recon_scene_guardian_ok_cb, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, ReconViewGuardian);
}

bool recon_scene_guardian_on_event(void* context, SceneManagerEvent event) {
    ReconApp* app = context;
    if(app->guardian_blocked) return false; // Marauder guard screen: let Back exit

    if(event.type == SceneManagerEventTypeCustom && event.event == GUARDIAN_EV_SUS) {
        scene_manager_next_scene(app->scene_manager, ReconSceneGuardianSus);
        return true;
    }

    if(event.type == SceneManagerEventTypeTick) {
        // Advance the rotating sweep when the current phase's dwell elapses.
        uint32_t now = furi_get_tick();
        if(now - app->guardian_phase_mark >= GUARD_PHASES[app->guardian_phase].dwell_ms) {
            app->guardian_phase = (uint8_t)((app->guardian_phase + 1) % GUARD_PHASE_COUNT);
            if(app->esp) esp_link_send(app->esp, GUARD_PHASES[app->guardian_phase].cmd);
            app->guardian_phase_mark = now;
        }

        // Tick the fused scorer live (this also fires the one-shot haptic/sound
        // alert on the transition INTO ELEVATED). On that rising edge also wake
        // the backlight so a guardian across the room is noticeable.
        recon_app_watchscore_tick(app);
        if(app->watch.just_elevated) {
            notification_message(app->notifications, &sequence_display_backlight_on);
        }

        guardian_view_refresh(app->guardian_view);
        return true;
    }
    return false;
}

void recon_scene_guardian_on_exit(void* context) {
    ReconApp* app = context;
    scan_session_stop(app);
    widget_reset(app->widget);
}
