// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt and FlipDeFlock contributors
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
