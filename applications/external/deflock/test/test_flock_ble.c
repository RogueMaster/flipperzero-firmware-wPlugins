// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt
//
// helpers/flock_ble.c -- the BLE side of the detection path: serial extraction
// from the 0x09C8 advert, conservative model identification, and (new in v0.48)
// the confidence floor that stops an OUI-only match being announced as a
// confirmed camera.
//
// This module shipped untested until v0.48 and was not even in the Makefile,
// while flock_ble_extract_serial() parsed raw, attacker-controlled advertisement
// bytes on every Flock-classified BLE device.
#include "test.h"
#include "flock_ble.h"

#include <string.h>

void suite_flock_ble(void);

void suite_flock_ble(void) {
    printf("[flock_ble]\n");

    char s[24];

    // --- B13 REGRESSION: OUI-only must never reach CONFIRMED ----------------
    // The companion sets cat=1 ("Flock") from several signals, and one of them is
    // a bare OUI-prefix match on the BLE address. Those prefixes are SHARED
    // silicon-vendor ranges (Espressif, Liteon...), so before v0.48 every
    // ordinary ESP32-based BLE device with a static address was reported as a
    // CONFIRMED Flock camera -- the v0.46 "Flock-Guest" over-claim, on the BLE
    // path. If this block ever goes green at Confirmed, that bug is back.
    CHECK_INT_EQ(flock_ble_confidence(0, NULL, false), FlockConfidencePossible);
    CHECK_INT_EQ(flock_ble_confidence(0, "", false), FlockConfidencePossible);
    CHECK_INT_EQ(flock_ble_confidence(0x004C, "iPhone", false), FlockConfidencePossible);
    CHECK_INT_EQ(flock_ble_confidence(0x0059, "SomeSensor", false), FlockConfidencePossible);
    // Names that merely CONTAIN a Flock-ish word are not a Flock-specific tell.
    CHECK_INT_EQ(flock_ble_confidence(0, "Flock of Seagulls", false), FlockConfidencePossible);
    CHECK_INT_EQ(flock_ble_confidence(0, "MyPenguinSpeaker", false), FlockConfidencePossible);

    // --- Flock-specific tells DO reach CONFIRMED ----------------------------
    // 0x09C8 is Flock's own manufacturer id in the advert.
    CHECK_INT_EQ(
        flock_ble_confidence(FLOCK_BLE_COMPANY_ID, NULL, false), FlockConfidenceConfirmed);
    CHECK_INT_EQ(flock_ble_confidence(0x09C8, "anything", false), FlockConfidenceConfirmed);
    // Raven-specific GATT services are Raven-SPECIFIC, so they stand alone.
    CHECK_INT_EQ(flock_ble_confidence(0, NULL, true), FlockConfidenceConfirmed);
    CHECK_INT_EQ(flock_ble_confidence(0x004C, "iPhone", true), FlockConfidenceConfirmed);
    // Flock's own product naming, case-insensitive, prefix for Penguin.
    CHECK_INT_EQ(flock_ble_confidence(0, "Penguin-1234567890", false), FlockConfidenceConfirmed);
    CHECK_INT_EQ(flock_ble_confidence(0, "penguin-42", false), FlockConfidenceConfirmed);
    CHECK_INT_EQ(flock_ble_confidence(0, "FS Ext Battery", false), FlockConfidenceConfirmed);
    CHECK_INT_EQ(flock_ble_confidence(0, "fs ext battery", false), FlockConfidenceConfirmed);
    // "FS Ext" is matched as a substring (mirrors the companion's own test), so a
    // decorated name still lands.
    CHECK_INT_EQ(
        flock_ble_confidence(0, "Unit 7 FS Ext Battery", false), FlockConfidenceConfirmed);

    // Never None: the caller only asks about devices already classified as Flock,
    // so the floor is Possible. A None here would silently drop the detection.
    CHECK(flock_ble_confidence(0, NULL, false) != FlockConfidenceNone);

    // --- flock_ble_extract_serial: the 0x09C8 manufacturer payload ----------
    // Layout: 2-byte LE company id, then a plain-ASCII serial. We take the
    // longest alphanumeric run of >= 6 chars.
    const uint8_t mfg_ok[] = {0xC8, 0x09, 'T', 'N', '7', '2', '0', '2', '3', '0', '2', '2'};
    CHECK(flock_ble_extract_serial(mfg_ok, sizeof(mfg_ok), NULL, s, sizeof(s)));
    CHECK_STR_EQ(s, "TN72023022");

    // A run shorter than 6 chars is not a serial.
    const uint8_t mfg_short[] = {0xC8, 0x09, 'A', 'B', '1'};
    CHECK(!flock_ble_extract_serial(mfg_short, sizeof(mfg_short), NULL, s, sizeof(s)));
    CHECK_STR_EQ(s, ""); // cleared on failure, never left stale

    // Punctuation splits runs; the LONGEST one wins.
    const uint8_t mfg_split[] = {
        0xC8, 0x09, 'A', 'B', '1', '2', '-', 'L', 'O', 'N', 'G', 'E', 'S', 'T', '9'};
    CHECK(flock_ble_extract_serial(mfg_split, sizeof(mfg_split), NULL, s, sizeof(s)));
    CHECK_STR_EQ(s, "LONGEST9");

    // Degenerate inputs must not read past the buffer or assert.
    CHECK(!flock_ble_extract_serial(NULL, 0, NULL, s, sizeof(s)));
    CHECK(!flock_ble_extract_serial(mfg_ok, 0, NULL, s, sizeof(s)));
    CHECK(!flock_ble_extract_serial(mfg_ok, 2, NULL, s, sizeof(s))); // company id only
    CHECK(!flock_ble_extract_serial(NULL, 0, NULL, NULL, sizeof(s))); // NULL out
    CHECK(!flock_ble_extract_serial(mfg_ok, sizeof(mfg_ok), NULL, s, 0)); // zero cap

    // Truncation: a serial longer than the output buffer is cut, never overflowed,
    // and stays NUL-terminated.
    char tiny[5];
    memset(tiny, 'X', sizeof(tiny));
    CHECK(flock_ble_extract_serial(mfg_ok, sizeof(mfg_ok), NULL, tiny, sizeof(tiny)));
    CHECK_INT_EQ((int)strlen(tiny), 4);
    CHECK_STR_EQ(tiny, "TN72");

    // Non-printable / high bytes terminate a run rather than being copied through.
    const uint8_t mfg_bin[] = {0xC8, 0x09, 'A', 'B', 'C', 0x00, 0xFF, 'D', 'E', 'F'};
    CHECK(!flock_ble_extract_serial(mfg_bin, sizeof(mfg_bin), NULL, s, sizeof(s)));

    // --- serial fallback: the GAP name IS the serial on newer firmware -------
    CHECK(flock_ble_extract_serial(NULL, 0, "1234567890", s, sizeof(s)));
    CHECK_STR_EQ(s, "1234567890");
    // Legacy "Penguin-" prefix is stripped.
    CHECK(flock_ble_extract_serial(NULL, 0, "Penguin-1234567890", s, sizeof(s)));
    CHECK_STR_EQ(s, "1234567890");
    CHECK(flock_ble_extract_serial(NULL, 0, "penguin-1234567890", s, sizeof(s)));
    CHECK_STR_EQ(s, "1234567890");
    // A model LABEL is not a unit id: spaces disqualify it, and so does having no
    // digit at all. "FS Ext Battery" must not be stored as a serial.
    CHECK(!flock_ble_extract_serial(NULL, 0, "FS Ext Battery", s, sizeof(s)));
    CHECK(!flock_ble_extract_serial(NULL, 0, "AbcdefGh", s, sizeof(s))); // no digit
    CHECK(!flock_ble_extract_serial(NULL, 0, "12345", s, sizeof(s))); // too short
    CHECK(!flock_ble_extract_serial(NULL, 0, "", s, sizeof(s)));

    // --- flock_ble_model_ex: conservative BY DESIGN --------------------------
    // Raven is the ONLY positively derivable model, via its own GATT services.
    CHECK_INT_EQ(flock_ble_model_ex(NULL, NULL, true), FlockBleModelRaven);
    CHECK_INT_EQ(flock_ble_model_ex("TN72023022", "Penguin-1", true), FlockBleModelRaven);

    // Falcon is NEVER asserted: absence of the Raven GATT is not proof of Falcon,
    // and there is no Falcon-specific tell. A wrong confident label is worse than
    // a generic one.
    CHECK_INT_EQ(flock_ble_model_ex("TN72023022", NULL, false), FlockBleModelGeneric);
    CHECK_INT_EQ(flock_ble_model_ex(NULL, "Penguin-123", false), FlockBleModelGeneric);
    CHECK_INT_EQ(flock_ble_model_ex(NULL, "FS Ext Battery", false), FlockBleModelGeneric);
    CHECK_INT_EQ(flock_ble_model_ex(NULL, NULL, false), FlockBleModelUnknown);
    CHECK_INT_EQ(flock_ble_model_ex("", "", false), FlockBleModelUnknown);
    CHECK_INT_EQ(flock_ble_model_ex(NULL, "Tile", false), FlockBleModelUnknown);

    // --- labels --------------------------------------------------------------
    // Raven is GATT-backed and confident -> no "?". Falcon keeps its "?" because
    // it is never asserted.
    CHECK_STR_CONTAINS(flock_ble_model_str(FlockBleModelRaven), "Raven");
    CHECK(strchr(flock_ble_model_str(FlockBleModelRaven), '?') == NULL);
    CHECK_STR_CONTAINS(flock_ble_model_str(FlockBleModelFalcon), "?");
    CHECK_STR_CONTAINS(flock_ble_model_str(FlockBleModelGeneric), "battery");
    CHECK_STR_EQ(flock_ble_model_str(FlockBleModelUnknown), "-");
    CHECK_STR_EQ(flock_ble_model_str((FlockBleModel)99), "-"); // out-of-range -> default
}
