// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt and FlipDeFlock contributors
/**
 * @file scene_util.h
 * Small shared helpers for the scan scenes, to stop copy-pasting the same
 * boilerplate (a "needs the companion FW" guard screen, and the save-report
 * success/error chirp) across flock/wifi/ble/guardian/locator.
 */
#pragma once

#include <stdbool.h>

// `app` is a ReconApp* everywhere; typed void* to avoid a header cycle, matching
// esp_link.h / scan_session.h (recon_app_i.h's ReconApp is an anonymous typedef).

/**
 * Render the "this feature needs the FlipDeFlock companion FW" guard screen
 * (shown when a companion-only scene is opened in Marauder mode) and switch to
 * the widget view. `message` is the scene-specific explanation text.
 */
void scene_show_companion_guard(void* app, const char* message);

/**
 * Play the report-save feedback sound -- success or error -- but only when the
 * user has sound enabled. Centralises the `if(settings.sound) ...` chirp shared
 * by the WiFi/BLE "Save Report" actions.
 */
void scene_report_notify(void* app, bool ok);
