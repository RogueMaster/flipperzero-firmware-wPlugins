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

/**
 * Format a MAC with only its OUI intact: "3C:91:80:xx:xx:xx" (>= 18 bytes).
 *
 * For the false-positive export. The OUI is the part that can MATCH a detection
 * rule, so it is the part a maintainer needs; the low three octets identify one
 * physical radio and are what makes a MAC lookup-able in public wardriving sets.
 * Keeping the vendor prefix and dropping the device is the whole trade.
 */
void fmt_mac_oui(char* out, size_t out_len, const uint8_t mac[6]);

/**
 * Reduce an SSID to its SHAPE: upper -> 'A', lower -> 'a', digit -> 'd',
 * `-` `_` `.` kept (structural, and near-universal), anything else -> '?'.
 * An empty name becomes "(none)".
 *
 * So "MyHomeNetwork" -> "AaAaaaAaaaaaa" and "3C9180112233" -> "dAdddddddddd".
 *
 * WHY A SHAPE AND NOT THE NAME: an SSID is frequently a household surname or a
 * street address, and is independently geolocatable through public wardriving
 * databases -- so it is the single most identifying field a user could hand over
 * when reporting a false positive. The shape still answers the question that
 * matters ("was this a MAC-shaped name, or a person's network?") without being
 * the name. Callers keep the literal SSID only when it matched a Flock naming
 * rule, because that is a camera's own name rather than a user's.
 */
void fmt_ssid_shape(char* out, size_t out_len, const char* ssid);

/**
 * Frame-type character, made safe to print with %c. Returns `ftype` when it is
 * one of the known set "PBROFL", otherwise '?'.
 *
 * NOT COSMETIC. A FlockEntry restored from hits.csv can carry ftype == 0:
 * flock_store writes an empty column for any type outside that set, reads it
 * back as f[4][0] == '\0', and memsets the record to zero before parsing. Passed
 * straight to %c that emits a NUL BYTE into the middle of a report, which is a
 * text file people attach to issues.
 */
char fmt_frame_char(char ftype);
