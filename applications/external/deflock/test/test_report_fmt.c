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

// ---------------------------------------------------------------------------
// Redaction helpers for the false-positive export (fmt_mac_oui, fmt_ssid_shape).
//
// These exist to make a detection log SAFE TO HAND OVER, so the tests that
// matter are the ones asserting what must NOT survive. A redactor is only worth
// having if it is tested against the leak it is supposed to prevent.
void suite_report_fmt_redact(void) {
    printf("[report_fmt/redact]\n");
    char out[80];

    // --- fmt_mac_oui --------------------------------------------------------
    const uint8_t mac[6] = {0x3C, 0x91, 0x80, 0x11, 0x22, 0x33};
    fmt_mac_oui(out, sizeof(out), mac);
    CHECK_STR_EQ(out, "3C:91:80:xx:xx:xx");

    // The device half is GONE, not merely reformatted. Asserting the absence of
    // each low octet is the point: a change that "still looks redacted" while
    // leaking one byte would pass a shape-only check.
    CHECK(strstr(out, "11") == NULL);
    CHECK(strstr(out, "22") == NULL);
    CHECK(strstr(out, "33") == NULL);

    // Low octets that repeat the OUI bytes must not fool the check above either:
    // the output is fixed-width and positional, so verify it exactly.
    const uint8_t twin[6] = {0xAA, 0xBB, 0xCC, 0xAA, 0xBB, 0xCC};
    fmt_mac_oui(out, sizeof(out), twin);
    CHECK_STR_EQ(out, "AA:BB:CC:xx:xx:xx");

    // --- fmt_ssid_shape -----------------------------------------------------
    // A household name is the worst case: a surname is identifying on its own,
    // and the pair (SSID, location) is independently lookup-able.
    fmt_ssid_shape(out, sizeof(out), "MyHomeNetwork");
    CHECK_STR_EQ(out, "AaAaaaAaaaaaa");

    // No letter of the original name survives in any form.
    fmt_ssid_shape(out, sizeof(out), "SmithFamily");
    CHECK(strstr(out, "Smith") == NULL);
    CHECK(strstr(out, "smith") == NULL);
    CHECK_STR_EQ(out, "AaaaaAaaaaa");

    // A MAC-shaped name still reads as one after masking, which is the whole
    // diagnostic value: "was this a device name or a person's network?"
    fmt_ssid_shape(out, sizeof(out), "3C9180112233");
    CHECK_STR_EQ(out, "dAdddddddddd");

    // Structural separators are kept -- they carry the shape and are not
    // identifying on their own.
    fmt_ssid_shape(out, sizeof(out), "Flock-Guest");
    CHECK_STR_EQ(out, "Aaaaa-Aaaaa");
    fmt_ssid_shape(out, sizeof(out), "net_5.2");
    CHECK_STR_EQ(out, "aaa_d.d");

    // Anything else collapses to '?', including bytes over 127. An SSID is 32
    // bytes of arbitrary data, so this must not depend on locale or on char
    // signedness.
    //
    // Masking is per BYTE, not per glyph: the trailing "\xC3\xA9" is one 'e'
    // acute in UTF-8 and becomes TWO '?'. That is the honest result for a field
    // the app treats as bytes, and it leaks nothing -- but it does mean the
    // shape length is a byte count, so do not read it as a character count.
    fmt_ssid_shape(out, sizeof(out), "a b!\xC3\xA9");
    CHECK_STR_EQ(out, "a?a???");

    // Empty and NULL are named, not blank: a blank cell in a report reads as a
    // missing field rather than as "this AP broadcast no name".
    fmt_ssid_shape(out, sizeof(out), "");
    CHECK_STR_EQ(out, "(none)");
    fmt_ssid_shape(out, sizeof(out), NULL);
    CHECK_STR_EQ(out, "(none)");

    // Truncation must not run past the caller's buffer, and must still
    // terminate. A 32-byte SSID into a short buffer is the realistic case.
    char small[6];
    fmt_ssid_shape(small, sizeof(small), "ABCDEFGHIJ");
    CHECK_STR_EQ(small, "AAAAA");
    CHECK_INT_EQ((int)strlen(small), 5);

    // A one-byte buffer can hold only the terminator.
    char tiny[1];
    fmt_ssid_shape(tiny, sizeof(tiny), "ABC");
    CHECK_INT_EQ((int)strlen(tiny), 0);
}
