// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt and FlipDeFlock contributors
#pragma once

#include <stddef.h>
#include <stdbool.h>

/** Create the apps_data report folders if missing. @param app ReconApp*. */
void recon_report_ensure_dirs(void* app);

/**
 * Write a Markdown report + a DeFlock-compatible GeoJSON of every *marked*
 * Flock detection. On success returns true and fills `out_path_md` (caller
 * provides a char buffer of at least `out_len`).
 */
bool recon_report_save_flock(void* app, char* out_path_md, size_t out_len);

/**
 * Append one NFC/RFID audit finding (a single preformatted line) to the daily
 * NFC audit log on the SD card.
 */
bool recon_report_append_nfc(void* app, const char* line);

/**
 * Write a Markdown + CSV report of the last WiFi security scan (all APs with
 * grade, auth, WPS and weaknesses). Returns true on success and fills
 * `out_path_md`.
 */
bool recon_report_save_wifi(void* app, char* out_path_md, size_t out_len);

/**
 * Write a CSV + GeoJSON of the last BLE scan (all devices, with tracker
 * category and "following" flag; GeoJSON points for geotagged trackers).
 */
bool recon_report_save_ble(void* app, char* out_path_md, size_t out_len);
