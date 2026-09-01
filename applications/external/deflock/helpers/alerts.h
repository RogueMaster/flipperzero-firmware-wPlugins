// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt
/**
 * @file alerts.h
 * The beep/vibro alert raised when a Flock/ALPR detection crosses the alert gate.
 *
 * A hit was previously silent -- it appeared as a new row on a screen you had to
 * be looking at, so a camera detected while driving went unnoticed until well
 * past it (GitHub issue #1). WHEN to alert is decided by the pure, host-tested
 * flock_alert_should_fire() in detect_rules.h; this file only owns HOW it sounds.
 *
 * Firmware-only (it names SDK notification messages), so it stays out of the
 * host test build.
 */
#pragma once

#include <notification/notification.h>

#include <stdbool.h>
#include <stdint.h>

/** ReconSettings.alert_mode. Index-aligned with alert_text[] in the settings scene. */
typedef enum {
    ReconAlertOff = 0,
    ReconAlertVibro = 1, /**< default: discreet, like the WATCHSCORE ELEVATED haptic */
    ReconAlertBeep = 2,
    ReconAlertBoth = 3,
    ReconAlertModeCount = 4,
} ReconAlertMode;

/**
 * Fire the detection alert. No-op when `mode` is ReconAlertOff.
 *
 * `sound_enabled` is the app's global Sound setting: when it is off the tone is
 * dropped but the vibro still fires, so the one global mute switch stays honest
 * instead of two settings disagreeing. Every mode also raises the backlight --
 * a hit is worth noticing on a device sitting in a pocket or a cupholder, and
 */
void recon_alert_fire(NotificationApp* notifications, uint8_t mode, bool sound_enabled);
