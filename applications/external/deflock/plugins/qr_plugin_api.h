// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt
/**
 * @file qr_plugin_api.h
 * ABI between the app and the QR encoder plugin. Included by BOTH sides.
 *
 * WHY THE ENCODER IS NOT IN THE APP -- the Flipper's loader has to place each
 * ELF section of a .fap in one contiguous allocation, and users on heavier
 * firmware were being refused with "Not enough RAM to run the app" (issue #5).
 * Nayuki's QR generator is ~4.6 KB of that image and is reachable from exactly
 * one screen (Share to DeFlock), so it is loaded on demand and dropped again on
 * the way out. Peak memory while the QR is on screen is unchanged; every other
 * screen is 4.6 KB cheaper, including the moment of loading, which is the
 * moment that was failing.
 *
 * The .fal ships INSIDE the .fap as a file asset (fal_embedded), so this stays
 * a single-file install -- download flipdeflock.fap, copy it, done.
 *
 * VERSIONING: bump QR_PLUGIN_API_VERSION on any change to QrPluginApi. The
 * loader refuses a mismatched plugin rather than calling through a stale
 * layout, and the app treats a refusal as "feature unavailable", never as a
 * crash.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#define QR_PLUGIN_APP_ID      "flipdeflock_qr"
#define QR_PLUGIN_API_VERSION 1

/**
 * Encoder version cap, mirrored on both sides of the ABI.
 *
 * Version 8 is 49x49 modules. A deflock.org URL with 6-decimal coordinates is
 * well inside that mode's byte capacity, and the cap is what keeps the two
 * transient buffers small.
 */
#define QR_PLUGIN_MAX_VERSION 8

/**
 * Byte length of both the output and scratch buffers, i.e. qrcodegen's
 * BUFFER_LEN_FOR_VERSION(8). Spelled out rather than derived so the app does
 * not have to include qrcodegen.h just to size a buffer; the plugin carries a
 * _Static_assert that the two agree, so a future version bump cannot silently
 * desync the ABI.
 */
#define QR_PLUGIN_BUF_LEN 302

typedef struct {
    /**
     * Encode `text` into `out` (>= QR_PLUGIN_BUF_LEN), using `temp` (same size)
     * as scratch. Returns false if the text does not fit the version cap --
     * which the caller must render as "QR n/a", not as a blank screen.
     */
    bool (*encode_text)(const char* text, uint8_t* temp, uint8_t* out);

    /** Module grid edge length of an encoded QR. */
    int (*get_size)(const uint8_t* qr);

    /** True if the module at (x, y) is dark. Out-of-range reads as light. */
    bool (*get_module)(const uint8_t* qr, int x, int y);
} QrPluginApi;
