// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt
/**
 * @file report_fmt.h
 * Shared field emitters for the report writers -- the MAC and coordinate
 * formatting that was copy-pasted inline across the flock / wifi / ble writers
 * (R8). Pure (libc only), so they are host-testable.
 */
#pragma once

#include <stdint.h>
#include <stddef.h>

/** Format a 6-byte MAC as "XX:XX:XX:XX:XX:XX" (uppercase) into `out` (>= 18 bytes). */
void fmt_mac(char* out, size_t out_len, const uint8_t mac[6]);

/**
 * Format a coordinate as "%.6f", or copy `fallback` (e.g. "-" for a table cell,
 * "" for an omitted field) when the value is NaN (no fix). Truncation-safe.
 */
void fmt_coord(char* out, size_t out_len, float value, const char* fallback);

/**
 * Map an RSSI (dBm; 0 = unknown) to a strength level: -1 unknown, else 1..4.
 *
 * Lives here, in the pure/host-tested layer, rather than in views/ui_widgets.c
 * so the thresholds are testable. ui_signal_level() delegates to it, and every
 * screen that draws bars goes through that -- so the list rows and the detail
 * screen cannot disagree about what "-74 dBm" looks like.
 */
int fmt_signal_level(int rssi);
