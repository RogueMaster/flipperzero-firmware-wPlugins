// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt
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
 * Write a REDACTED Markdown report of every stored detection, for sending in
 * when the app flags something it should not have.
 *
 * Not the same file as recon_report_save_flock(): that one is evidence about
 * cameras and is meant to carry coordinates. This one is evidence about the
 * DETECTOR, so location is exactly what it must not contain.
 *
 * Stripped: GPS coordinates and heading, the low three octets of every MAC, the
 * sighting timestamp, and any SSID that did not itself match a Flock naming rule
 * (reduced to a shape -- see fmt_ssid_shape). Kept: the confidence rung, the
 * indicator that actually fired, the OUI, frame type, channel, RSSI, sighting
 * count and IE fingerprint, which are what a false positive has to be diagnosed
 * from.
 *
 * Exports EVERY stored detection rather than only the marked ones: the question
 * being asked is "what did this list get wrong", and marking is already spoken
 * for by the DeFlock report.
 */
bool recon_report_save_fp(void* app, char* out_path_md, size_t out_len);
