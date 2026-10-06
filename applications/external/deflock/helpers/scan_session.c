// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt
#include "scan_session.h"
#include "../recon_app_i.h"
#include "esp_link.h"
#include "gps_link.h"
#include "gps_rpc.h"

bool scan_session_start(void* _app) {
    ReconApp* app = _app;
    if(app->esp) return false; // already live (a Back re-entry) -> keep it, don't leak
    app->esp = esp_link_alloc(app);
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    // A fresh link has not gone silent, and has no scan mode to restore yet;
    // the scene that opened it sends the kickoff right after this returns.
    app->esp_lost = false;
    app->esp_rx_tick = 0;
    app->esp_kickoff[0] = '\0';
    furi_mutex_release(app->mutex);
    esp_link_start(app->esp);

    // Configure the companion's GPS relay once, centrally, for every screen that
    // opens a session -- rather than in each scene, which is how the alert
    // delivery bug happened. The command is built in esp_link so that this call
    // and the on-banner re-send cannot drift apart; it is a no-op on Marauder.
    esp_link_send_band(app->esp);
    esp_link_send_gps_cfg(app->esp);
    recon_diag_begin(app);
    // A survey is a snapshot of ONE OUTING, not a running history.
    //
    // Clearing the app's copy is not enough -- the counts live on the COMPANION,
    // which keeps counting until it is power-cycled. Without this the numbers are
    // "since the board booted", so a phone seen around the house for a few hours
    // climbs into the hundreds and reads exactly like the persistent emitter a
    // camera produces. That inverts the one thing the survey is for: the whole
    // method is "the row with a big count next to a camera you can see IS the
    // camera", and a stale total makes an ordinary device look like one.
    esp_link_send(app->esp, "surveyclear");
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    app->survey_count = 0;
    // Stamped now, not at save time: this labels the session in survey_log.csv,
    // and the useful label is when the operator was standing there.
    app->survey_session_epoch = furi_hal_rtc_get_timestamp();
    // Start the clock now rather than at 0. A poll fired the instant a scan opens
    // asks the companion for a table it has not filled yet, wasting the one
    // request a short session would ever make.
    app->survey_last_poll = furi_get_tick();
    furi_mutex_release(app->mutex);
    return true;
}

void scan_session_gps_start(void* _app) {
    ReconApp* app = _app;
    if(!app->settings.gps_enabled) return;

    // Phone source: the fix arrives over the firmware's RPC location service from
    // whatever companion app is paired, so like the companion relay there is no
    // second UART to open -- but unlike it, there is a subscription to set up, so
    // this cannot simply return. Handled first because the UART checks below are
    // meaningless for it.
    if(app->settings.gps_source == ReconGpsSourcePhone) {
        if(app->gps_rpc) return; // already running
        app->gps_rpc = gps_rpc_alloc(app);
        gps_rpc_start(app->gps_rpc);
        return;
    }

    if(app->gps) return; // already running
    // Companion source: the fix arrives as `G,<nmea>` on the ESP link, so there
    // is no second UART to open here at all. Opening one would take a port for
    // nothing (and on a single-UART wiring, take the ESP's).
    if(app->settings.gps_source == ReconGpsSourceCompanion) return;
    if(app->settings.gps_uart == app->settings.esp_uart) return; // would steal the ESP's UART
    app->gps = gps_link_alloc(app);
    gps_link_start(app->gps);
}

void scan_session_stop(void* _app) {
    ReconApp* app = _app;
    // Nothing running -> nothing to tear down, and crucially nothing to persist.
    // This is called from recon_scene_start_on_enter(), which also runs once at
    // launch before any scan has happened; without this guard that first call
    // would write an EMPTY table straight over the hits.csv recon_hits_load()
    // had just restored -- issue #5's exact failure, from the other direction.
    if(!app->esp && !app->gps && !app->gps_rpc) return;

    if(app->esp) {
        // FINAL SURVEY DUMP, before the link goes away.
        //
        // The survey lives in the companion's RAM and only crosses the wire when
        // asked, so without this a short session captures nothing at all: the
        // periodic poll fires once at session start, when the board's table is
        // still empty, and a session that ends before the next one leaves no file
        // behind. Reported on issue #25 with sessions of 7, 14 and 26 seconds --
        // every one of them under the poll interval, every one producing nothing,
        // and no way for the operator to tell the feature from a broken one.
        //
        // Asking here and draining briefly is the only point where the whole
        // session's survey is available AND the link still exists. ~350 ms covers
        // a full 32-row dump at 115200 (about 1.3 KB, ~110 ms on the wire) with
        // room for the worker thread to parse it.
        esp_link_send(app->esp, "survey");
        furi_delay_ms(350);

        esp_link_stop(app->esp);
        esp_link_free(app->esp);
        app->esp = NULL;
    }
    if(app->gps) {
        gps_link_stop(app->gps);
        gps_link_free(app->gps);
        app->gps = NULL;
    }
    if(app->gps_rpc) {
        // Unsubscribes from the RPC callback before freeing -- see gps_rpc_stop().
        gps_rpc_stop(app->gps_rpc);
        gps_rpc_free(app->gps_rpc);
        app->gps_rpc = NULL;
    }
    // Persist the detections this scan collected (opt-in; a no-op when the
    // setting is off). Both call sites -- the Main Menu's on_enter and the app
    // teardown -- funnel through here, so one call site still covers Flock,
    // Flock Map, Net Guardian, WiFi, BLE and Locator, including the case that
    // prompted the request: backing straight out of the app after a scan. Done
    // AFTER the links are torn down, so the write can't race a still-running
    // ESP worker.
    recon_hits_save(app);
    // Then the session's own vital signs. Written unconditionally: a drive that
    // found nothing is exactly the session whose diagnostics matter most, and
    // that is precisely the case where hits.csv is empty and tells you nothing.
    recon_diag_save(app);
    // What was actually in the air, matched or not -- the only thing that can
    // explain a session with frames but no candidates.
    recon_survey_save(app);
}
