// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt and FlipDeFlock contributors
#pragma once

#include <stdint.h>

/**
 * Tiny curated OUI -> vendor lookup (not the full IEEE registry). Returns a
 * vendor name for the first 3 bytes of `mac`, or NULL if unknown. Deliberately
 * conservative: only entries we're confident about, so a survey never shows a
 * wrong manufacturer. Weighted toward Wi-Fi AP, IoT and surveillance-camera
 * vendors useful in a counter-surveillance survey.
 */
const char* oui_vendor(const uint8_t* mac);
