// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt
#include "../recon_app_i.h"
#include "../helpers/esp_link.h"
#include "../helpers/scan_session.h"
#include "../helpers/scene_util.h"

// Locator step 2: the homing HUD. Tells the companion to stream live RSSI for
// the selected target (`locate <w|b> <mac> <ch>`) and shows the hot/cold meter.
// Companion-only: the generic/Marauder backend has no `locate` command.

// Scene ticks are 250 ms (RECON_TICK_MS), so 20 is five seconds. See the
// re-arm comment in on_event for why this exists at all.
#define LOCATOR_REARM_TICKS 20
static uint8_t s_rearm_ticks;

/** Send `locate` for the currently selected target. Safe to repeat. */
static void locator_send_target(ReconApp* app) {
    if(!app->esp) return;
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    uint8_t kind = app->locate_kind;
    uint8_t ch = app->locate_ch;
    uint8_t mac[6];
    memcpy(mac, app->locate_mac, 6);
    furi_mutex_release(app->mutex);
    if(!kind) return;

    char cmd[40];
    snprintf(
        cmd,
        sizeof(cmd),
        "locate %c %02x%02x%02x%02x%02x%02x %u",
        kind == 'b' ? 'b' : 'w',
        mac[0],
        mac[1],
        mac[2],
        mac[3],
        mac[4],
        mac[5],
        ch);
    esp_link_send(app->esp, cmd);
}

void recon_scene_locator_home_on_enter(void* context) {
    ReconApp* app = context;

    if(app->settings.backend != EspBackendCompanion) {
        app->locator_blocked = true;
        scene_show_companion_guard(
            app,
            "Locator needs the\nFlipDeFlock companion FW\n(live signal homing).\n\nYou're in Marauder mode.\nFlash via 'ESP32 Firmware'\nor switch Board Mode in\nSettings.");
        return;
    }
    app->locator_blocked = false;

    // Fresh reading state, and snapshot the target out of the lock.
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    app->locate_have = false;
    app->locate_rssi = 0;
    app->locate_tick = 0;
    app->locate_init = false; // reset the peak-hold fold (now lives in ReconApp)
    app->locate_peak = -128;
    app->esp_connected = false;
    furi_mutex_release(app->mutex);

    s_rearm_ticks = 0;
    locator_view_reset(app->locator_view);

    // ESP first so it claims its UART; GPS only if on a different port (the
    // homing meter works without it -- GPS only adds the "strongest here" note).
    // scan_session_start is idempotent across re-entry (see scan_session.h); the
    // target `locate` command below is (re)sent every enter regardless, so the
    // companion always homes the currently selected device.
    scan_session_start(app);
    locator_send_target(app);

    scan_session_gps_start(app);

    view_dispatcher_switch_to_view(app->view_dispatcher, ReconViewLocator);
}

bool recon_scene_locator_home_on_event(void* context, SceneManagerEvent event) {
    ReconApp* app = context;
    if(app->locator_blocked) return false; // guard screen: let Back exit
    if(event.type == SceneManagerEventTypeTick) {
        // RE-ARM THE HUNT PERIODICALLY. The companion drops Locator mode on any
        // command that re-tasks the radio, which is correct -- but it means one
        // stray command from anywhere in the app silently ends a hunt, leaving a
        // live link, a frozen meter and nothing on either side to say why. Two
        // such commands were found on the bench (the 10 s survey poll and the
        // banner-triggered relay-config resend) and both are now held back while
        // hunting; this is the guard that stops the NEXT one costing a release.
        //
        // Re-sending is idempotent on the board: it re-pins the same channel and
        // re-arms the same target. It does clear the companion's 400 ms window
        // peak, which is why this is every 5 s and not every tick -- the app
        // keeps its own peak-hold, so nothing the operator reads is lost.
        if(++s_rearm_ticks >= LOCATOR_REARM_TICKS) {
            s_rearm_ticks = 0;
            locator_send_target(app);
        }
        locator_view_refresh(app->locator_view);
        return true;
    }
    return false;
}

void recon_scene_locator_home_on_exit(void* context) {
    ReconApp* app = context;
    // End locate mode, but leave the LINK up: this scene can be a child of Flock
    // Detail or the Guardian's Suspicious list, and freeing the link here used
    // to kill the scan the user was going back to. `stop` idles the companion,
    // which is why every scan scene re-sends its kickoff on entry. The Main
    // Menu's on_enter owns the teardown (see helpers/scan_session.h).
    if(app->esp) esp_link_send(app->esp, "stop");
    // The hunt is over, so say so. locate_kind is what recon_survey_tick() and
    // recon_app_gps_cfg_tick() test to keep their hands off the radio while the
    // Locator owns it, and until this line nothing ever cleared it: after one
    // visit here the survey poll never fired again and the relay config was
    // never re-sent after a companion reboot, for the rest of the app run. The
    // target itself (locate_mac / locate_label) is left alone, because the
    // picker re-sets locate_kind on the way back in.
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    app->locate_kind = 0;
    furi_mutex_release(app->mutex);
    widget_reset(app->widget);
}
