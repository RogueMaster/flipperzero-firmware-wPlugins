// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt and FlipDeFlock contributors
/**
 * @file scan_session.h
 * Shared ESP/GPS link lifecycle for the scan scenes.
 *
 * Every scan scene (flock, flock_map, guardian, wifi, ble, locator) opened a
 * detail/child scene via scene_manager_next_scene, which SUSPENDS the parent
 * without calling its on_exit; pressing Back re-runs the parent's on_enter. A
 * naive `app->esp = esp_link_alloc(...)` in on_enter therefore ran a SECOND time
 * on Back, overwriting the still-live link without freeing it -- leaking the heap
 * struct + worker stack and orphaning the UART, so the replacement link could not
 * acquire it and all ESP scanning died for the rest of the session (bug B1).
 *
 * These helpers own app->esp / app->gps so that idempotent re-entry guard lives
 * in exactly ONE place instead of being copy-pasted (and forgotten) per scene.
 * on_enter calls scan_session_start()/scan_session_gps_start(); on_exit calls
 * scan_session_stop().
 */
#pragma once

#include <stdbool.h>

// `app` is a ReconApp* everywhere; typed void* to avoid a header cycle, matching
// esp_link.h / gps_link.h (recon_app_i.h's ReconApp is an anonymous-struct typedef).

/**
 * Allocate + start app->esp, but ONLY if it is not already running. Returns true
 * only when it FRESHLY started, so a caller can send its one-time backend kickoff
 * command (e.g. "flockcombo") exactly once and not again on a Back re-entry.
 */
bool scan_session_start(void* app);

/**
 * Allocate + start app->gps, but ONLY if GPS is enabled AND on a different UART
 * than the ESP (a shared port would steal the ESP's UART and silently kill
 * detection). Idempotent; a no-op when GPS is off or shares the ESP's port.
 */
void scan_session_gps_start(void* app);

/**
 * Stop + free app->esp and app->gps if present, NULLing both. Idempotent and
 * safe to call from any scan scene's on_exit regardless of which links it
 * started (an unused link is simply NULL -> skipped).
 */
void scan_session_stop(void* app);
