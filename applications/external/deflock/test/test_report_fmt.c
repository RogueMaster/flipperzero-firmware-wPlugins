// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt
//
// Tests for the shared report field emitters (R8): MAC + coordinate formatting,
// plus the RSSI->bar-level scale every screen that draws signal bars goes through.
#include "report_fmt.h"
#include "test.h"

#include <math.h>
#include <stdio.h>

void suite_report_fmt(void) {
    printf("[report_fmt]\n");
    char out[24];

    // --- fmt_mac ------------------------------------------------------------
    const uint8_t mac[6] = {0xa1, 0xb2, 0xc3, 0xd4, 0xe5, 0xf6};
    fmt_mac(out, sizeof(out), mac);
    CHECK_STR_EQ(out, "A1:B2:C3:D4:E5:F6");
    const uint8_t zero[6] = {0, 0, 0, 0, 0, 0};
    fmt_mac(out, sizeof(out), zero);
    CHECK_STR_EQ(out, "00:00:00:00:00:00");

    // --- fmt_coord (exactly-representable floats so %.6f is deterministic) ---
    fmt_coord(out, sizeof(out), NAN, "-");
    CHECK_STR_EQ(out, "-"); // no fix -> table-cell fallback
    fmt_coord(out, sizeof(out), NAN, "");
    CHECK_STR_EQ(out, ""); // no fix -> omitted-field fallback
    fmt_coord(out, sizeof(out), 48.5f, "-");
    CHECK_STR_EQ(out, "48.500000");
    fmt_coord(out, sizeof(out), -11.25f, "");
    CHECK_STR_EQ(out, "-11.250000");

    // --- fmt_signal_level ---------------------------------------------------
    // 0 dBm is the "no reading" sentinel, not a colossally strong signal: a real
    // RSSI is always negative, so 0 means the field was never populated.
    CHECK_INT_EQ(fmt_signal_level(0), -1);

    // Boundaries, each tested on both sides so a shifted threshold fails loudly.
    CHECK_INT_EQ(fmt_signal_level(-50), 4);
    CHECK_INT_EQ(fmt_signal_level(-51), 3);
    CHECK_INT_EQ(fmt_signal_level(-62), 3);
    CHECK_INT_EQ(fmt_signal_level(-63), 2);
    CHECK_INT_EQ(fmt_signal_level(-74), 2);
    CHECK_INT_EQ(fmt_signal_level(-75), 1);
    CHECK_INT_EQ(fmt_signal_level(-86), 1);
    CHECK_INT_EQ(fmt_signal_level(-87), 1);

    // Strong readings saturate at 4 rather than overflowing the bar count.
    CHECK_INT_EQ(fmt_signal_level(-1), 4);
    CHECK_INT_EQ(fmt_signal_level(-30), 4);

    // A very weak but PRESENT signal never collapses to 0 bars -- an empty bar
    // and "nothing detected" must not look the same on screen.
    CHECK_INT_EQ(fmt_signal_level(-100), 1);
    CHECK_INT_EQ(fmt_signal_level(-128), 1);
}
