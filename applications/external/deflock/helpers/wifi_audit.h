// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt and FlipDeFlock contributors
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <core/string.h>

/**
 * WiFi network security grading.
 *
 * Maps the ESP32 scan's auth mode + pairwise cipher + WPS flag to a severity
 * and a human-readable list of weaknesses. Pure logic (host-testable).
 *
 * Higher enum value = worse, so results sort worst-first.
 */
typedef enum {
    WifiGradeStrong,
    WifiGradeOk,
    WifiGradeInfo,
    WifiGradeWeak,
    WifiGradeCritical,
} WifiGrade;

/** Short label for the auth mode (esp wifi_auth_mode_t value). */
const char* wifi_auth_str(uint8_t authmode);

/** Short label for a grade ("CRIT"/"WEAK"/"OK"/"STRONG"/"INFO"). */
const char* wifi_grade_str(WifiGrade grade);

/**
 * Grade a network and append its weaknesses (one per line) to `reasons`.
 *
 * @param authmode  esp wifi_auth_mode_t
 * @param pairwise  esp wifi_cipher_type_t (pairwise cipher)
 * @param wps       WPS advertised
 * @param ssid      SSID (NULL/"" = hidden)
 * @param reasons   output, multiline (may be NULL)
 * @returns worst severity found.
 */
WifiGrade wifi_audit_grade(
    uint8_t authmode,
    uint8_t pairwise,
    bool wps,
    const char* ssid,
    FuriString* reasons);
