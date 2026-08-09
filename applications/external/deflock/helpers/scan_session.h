// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt
/**
 * @file scan_session.h
 * Shared ESP/GPS link lifecycle for the scan scenes.
 *
 * OWNERSHIP MODEL: the link belongs to the SCAN FEATURE, not to one scene.
 * A scan scene calls scan_session_start()/scan_session_gps_start() in on_enter
 * and does NOT stop the link in on_exit. The link is torn down in exactly two
 * places: recon_scene_start_on_enter() (the user genuinely returned to the Main
 * Menu) and the app teardown after view_dispatcher_run() returns.
 *
 * WHY on_exit is the wrong place (bug B7, hardware-confirmed 2026-08-05).
 * scene_manager_next_scene() -- how every List scene opens its Detail child --
 * UNCONDITIONALLY calls the outgoing scene's on_exit before pushing the new
 * scene. That is documented, intentional SDK behaviour, verified against
 * applications/services/gui/scene_manager.c and then on real hardware: three
 * List->Detail->Back cycles produced three on_exit calls, in lockstep, plus one
 * for the final leave.
 *
 * (An earlier revision of this comment claimed next_scene "SUSPENDS the parent
 * without calling its on_exit". That was never true. It was written during the
 * B1 fix and never checked against the SDK, and every later fix that reasoned
 * from it inherited the error -- including v0.69's, which is why that release's
 * changelog claim did not hold.)
 *
 * So while on_exit did the teardown, opening ANY device's Detail screen freed
 * app->esp instantly -- Wi-Fi scanning, BLE scanning, deauth detection and the
 * GPS relay were all offline app-wide for as long as that Detail screen was up,
 * and the Back that followed re-entered the parent with app->esp == NULL. Every
 * "is this a fresh session?" test therefore answered YES on a plain Back, so
 * the scenes wiped their tables: measured live, Net Guardian lost wifi_count
 * 14 -> 0 and its fused score 30 -> 0, and BLE/Tracker wiped 36 devices (and
 * the tag the user had just set) on a single Back press. The ESP replies to the
 * next trigger from its own cache within a fraction of a second, which is what
 * made the counts look healthy again moments later and hid this for two
 * releases.
 *
 * The original B1 leak this file was created for is still fixed: start is
 * idempotent, so a second call never overwrites a live link.
 *
 * COMPANION STATE, the trap in this model: because the link now survives a
 * Detail round-trip, so does whatever mode the companion was left in. The
 * Locator sends `stop`, which idles the board (g_scanning = g_combo = false),
 * and Flock Detail can open the Locator. A scan scene must therefore (re)send
 * its backend kickoff command on EVERY on_enter, not only a fresh one --
 * `flockcombo` and friends are idempotent mode-selects, so re-sending is cheap
 * and re-arms a board an excursion left idle. Only the DATA resets are gated on
 * a fresh start.
 */
#pragma once

#include <stdbool.h>

// `app` is a ReconApp* everywhere; typed void* to avoid a header cycle, matching
// esp_link.h / gps_link.h (recon_app_i.h's ReconApp is an anonymous-struct typedef).

/**
 * Allocate + start app->esp, but ONLY if it is not already running. Returns true
 * only when it FRESHLY started -- i.e. this is a genuine new scan session
 * entered from the Main Menu, not a return from a Detail child. Use it to gate
 * DATA resets (clearing tables, zeroing counters, re-baselining a score), never
 * to gate the backend kickoff command.
 */
bool scan_session_start(void* app);

/**
 * Allocate + start app->gps, but ONLY if GPS is enabled AND on a different UART
 * than the ESP (a shared port would steal the ESP's UART and silently kill
 * detection). Idempotent; a no-op when GPS is off or shares the ESP's port.
 */
void scan_session_gps_start(void* app);

/**
 * Stop + free app->esp and app->gps if present, NULLing both, then persist the
 * session's detections. A no-op when neither link is up, so calling it on a
 * Main Menu the user never left into a scan cannot overwrite hits.csv with an
 * empty table. Call it ONLY on a genuine return to the Main Menu or at app
 * teardown -- never from a scan scene's on_exit (see the file comment).
 */
void scan_session_stop(void* app);
