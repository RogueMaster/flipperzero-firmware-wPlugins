// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt and FlipDeFlock contributors
/**
 * @file marauder_scan.h
 * Generic/Marauder backend: scrape Flock detections out of arbitrary firmware
 * text output.
 *
 * This is the UNIVERSAL fallback backend -- it needs no specific firmware, so it
 * is what runs on a stock Marauder board. Unlike the companion path there is no
 * wire protocol to lean on: we scrape MAC and SSID tokens out of whatever the
 * board prints and apply the Flock filter locally.
 *
 * PURE LOGIC, no firmware dependencies, so it is host-testable. The decision
 * (which MACs are hits, and at what rung) lives here; applying those hits to the
 * app lives in esp_link.c. That is the same parse-does-not-mutate split the
 * companion path already uses (esp_parse_companion_line / esp_apply_companion),
 * and it exists so the precision-critical rules below can be regression-tested.
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "flock_db.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Max SSID bytes we keep (802.11 caps an SSID at 32) + NUL. */
#define MARAUDER_SSID_MAX 33

/**
 * Max detections reported from a single scraped line. A real Marauder line names
 * one or two MACs; anything past this is a pathological or hostile line, and the
 * overflow is counted in MarauderScan::dropped rather than silently ignored.
 */
#define MARAUDER_MAX_HITS 8

/** One scraped detection: a MAC on the line that passed the Flock filter. */
typedef struct {
    uint8_t mac[6];
    FlockConfidence conf;
    FlockDevClass dev_class;
} MarauderHit;

/** Everything decided about one scraped line. */
typedef struct {
    /** Labelled SSID if the line had one ("" otherwise). */
    char ssid[MARAUDER_SSID_MAX];
    /** True when the SSID above came from a real "ESSID:"-style label. */
    bool have_ssid;
    /** Total MAC tokens seen on the line (before filtering). */
    int mac_count;
    MarauderHit hits[MARAUDER_MAX_HITS];
    int hit_count;
    /**
     * MAC tokens past MARAUDER_MAX_HITS that were never scored at all. Counted
     * rather than ignored so a pathological line is visible instead of looking
     * like a clean partial result. Note this counts CANDIDATES skipped, not
     * confirmed hits -- some of them might not have scored anyway.
     */
    int dropped;
} MarauderScan;

/**
 * Scrape one line of generic firmware output into detections.
 *
 * PRECISION RULES pinned by test/test_marauder_scan.c -- change them only
 * deliberately:
 *
 *  - An OUI match always counts, at "possible" or better.
 *  - A line-wide SSID match may only be attributed to a MAC when the line names
 *    EXACTLY ONE MAC. Otherwise an unrelated "flock" token on a multi-record log
 *    line would promote every MAC printed beside it.
 *  - When no labelled SSID is present the WHOLE LINE is scored as if it were the
 *    SSID. That is deliberate (it is how a bare sniff dump still detects), but it
 *    means a stray token anywhere on the line can raise confidence -- which is
 *    exactly why the single-MAC rule above exists to contain it.
 *
 * @param line  NUL-terminated line of firmware output (not modified).
 * @param out   Receives the decision; fully overwritten, never partially filled.
 */
void marauder_scan_line(const char* line, MarauderScan* out);

#ifdef __cplusplus
}
#endif
