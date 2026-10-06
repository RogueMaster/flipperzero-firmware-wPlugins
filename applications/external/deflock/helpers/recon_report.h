// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt
#pragma once

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

/** Create the apps_data report folders if missing. @param app ReconApp*. */
void recon_report_ensure_dirs(void* app);

/**
 * What an export includes, and how much of it is stripped.
 *
 * Two independent axes rather than one "mode" enum, because the questions are
 * unrelated: SCOPE is "which detections", REDACTION is "how much of each". The
 * menu offers three of the four combinations; raw-marked-only is the one nobody
 * asked for.
 */
typedef enum {
    /**
     * Strip everything that describes the OPERATOR rather than the camera.
     *
     * MACs drop to their OUI, the sighting epoch and the observer heading are
     * omitted, operator labels are left out, and any SSID that did not itself
     * match a Flock naming rule is reduced to a shape (fmt_ssid_shape).
     * Coordinates are KEPT -- they are the entire point of a camera report.
     *
     * The SSID rule is the one that matters most in practice. A scan sweeps up
     * every household network in range, and an SSID is frequently a surname or
     * a street address and is independently geolocatable through public
     * wardriving databases. A user driving their own neighbourhood ends up with
     * their own home network in the table; publishing that file publishes where
     * they live.
     */
    ReconExportRedact = 1 << 0,
    /** Every stored detection, not just the ones the operator marked. */
    ReconExportAll = 1 << 1,
} ReconExportFlags;

/**
 * Write a Markdown report + a DeFlock-compatible GeoJSON + a KML of the Flock
 * detections selected by `flags` (a ReconExportFlags bitmask). On success
 * returns true and fills `out_path_md` (caller provides a char buffer of at
 * least `out_len`).
 *
 * Without ReconExportRedact the files are written verbatim and their names gain
 * a `_RAW` suffix, so the unshareable copy is distinguishable from the
 * shareable one after it has left the device.
 */
bool recon_report_save_flock(void* app, char* out_path_md, size_t out_len, uint8_t flags);

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

/**
 * Write a Markdown report for a camera the operator can SEE and the detector did
 * not flag.
 *
 * The third kind of report, and the one that was missing. save_flock() is
 * evidence about cameras the app found; save_fp() is evidence about rows it got
 * wrong. Neither can express "there is a camera right here and you said
 * nothing", which is the most common thing a field tester actually experiences
 * and the only data that can ever measure RECALL. Every report this project has
 * received arrived unlabelled, and the question that could not be answered from
 * any of them was which row was the camera the reporter could physically see.
 *
 * Carries the operator's claim, the GPS position (KEPT - this is a claim about a
 * camera, so it follows the DeFlock export rather than the false positive
 * report), the per-stage drop counters that separate a deaf radio from a
 * companion that heard the street and recognised none of it, and the full air
 * survey, where a randomising camera is the only thing it ever appears in.
 *
 * Succeeds on an empty table and an empty survey: both are results, and the
 * empty survey is the strongest single line in the file.
 */
bool recon_report_save_missed(void* app, char* out_path_md, size_t out_len);
