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
    // The bare word, or the word run into something else, is not Flock's shape.
    // Unit names are "Penguin-<digits>"; the dash is the anchor.
    CHECK_INT_EQ(flock_ble_confidence(0, "Penguin", false), FlockConfidencePossible);
    CHECK_INT_EQ(flock_ble_confidence(0, "PenguinSpeaker", false), FlockConfidencePossible);
    CHECK_INT_EQ(flock_ble_confidence(0, "Penguin Audio", false), FlockConfidencePossible);

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

    // --- flock_ble_tell: WHICH signal fired -------------------------------
    // Ordered to mirror flock_ble_confidence()'s precedence, so the tell always
    // explains the rung that function returned.
    const uint8_t addr_flock[6] = {0xb4, 0x1e, 0x52, 0x00, 0x00, 0x02}; // Flock's own OUI
    const uint8_t addr_other[6] = {0xde, 0xad, 0xbe, 0xef, 0x00, 0x01}; // in no table

    CHECK_INT_EQ(flock_ble_tell(FLOCK_BLE_COMPANY_ID, NULL, false, NULL), FlockBleTellMfgId);
    CHECK_INT_EQ(flock_ble_tell(AXON_BLE_COMPANY_ID, NULL, false, NULL), FlockBleTellMfgId);
    CHECK_INT_EQ(flock_ble_tell(BLE_COMPANY_NONE, NULL, true, NULL), FlockBleTellRavenGatt);
    CHECK_INT_EQ(flock_ble_tell(BLE_COMPANY_NONE, "Penguin-42", false, NULL), FlockBleTellNaming);
    CHECK_INT_EQ(
        flock_ble_tell(BLE_COMPANY_NONE, "FS Ext Battery", false, NULL), FlockBleTellNaming);

    // OUI-only is identified POSITIVELY from the address, not inferred from the
    // absence of everything else -- that is the whole reason the address is
    // passed in. Without it we cannot tell "shared silicon prefix" apart from
    // "a newer companion matched on something this build predates".
    CHECK_INT_EQ(
        flock_ble_tell(BLE_COMPANY_NONE, "ESP32", false, addr_flock), FlockBleTellOuiOnly);
    CHECK_INT_EQ(flock_ble_tell(BLE_COMPANY_NONE, "ESP32", false, addr_other), FlockBleTellNone);
    CHECK_INT_EQ(flock_ble_tell(BLE_COMPANY_NONE, "ESP32", false, NULL), FlockBleTellNone);

    // Precedence: a stronger tell wins even when a weaker one is also present.
    CHECK_INT_EQ(
        flock_ble_tell(FLOCK_BLE_COMPANY_ID, "Penguin-1", true, addr_flock), FlockBleTellMfgId);
    CHECK_INT_EQ(
        flock_ble_tell(BLE_COMPANY_NONE, "Penguin-1", true, addr_flock), FlockBleTellRavenGatt);

    // THE SAFETY PROPERTY. The tell is evidence reporting, NOT scoring: for every
    // input, the rung flock_ble_confidence() returns must be exactly what it was
    // before the tell existed. On this path there is no middle rung -- only
    // Confirmed(4) and Possible(1) -- and the default alert gate sits at
    // Likely(2), so any demotion here would silently cost the beep, the vibro and
    // the alert card, permanently. If this block ever fails, detection regressed.
    CHECK_INT_EQ(
        flock_ble_confidence(FLOCK_BLE_COMPANY_ID, NULL, false), FlockConfidenceConfirmed);
    CHECK_INT_EQ(
        flock_ble_confidence(FLOCK_BLE_COMPANY_ID, "ESP32", false), FlockConfidenceConfirmed);
    CHECK_INT_EQ(flock_ble_confidence(AXON_BLE_COMPANY_ID, NULL, false), FlockConfidenceConfirmed);
    CHECK_INT_EQ(flock_ble_confidence(BLE_COMPANY_NONE, NULL, true), FlockConfidenceConfirmed);
    CHECK_INT_EQ(
        flock_ble_confidence(BLE_COMPANY_NONE, "Penguin-42", false), FlockConfidenceConfirmed);
    CHECK_INT_EQ(flock_ble_confidence(BLE_COMPANY_NONE, "ESP32", false), FlockConfidencePossible);

    // EXHAUSTIVE EQUIVALENCE SWEEP. The spot checks above are examples; this is
    // the proof. Walk the whole input space and assert that adding the tell moved
    // no rung anywhere: for every combination, the tell's implied rung (Confirmed
    // for a Flock-specific tell, Possible otherwise) must equal what
    // flock_ble_confidence() independently returns. Two implementations, checked
    // against each other -- if they ever disagree, detection changed.
    {
        const uint16_t companies[] = {
            0, BLE_COMPANY_NONE, 0x004C, 0x0059, FLOCK_BLE_COMPANY_ID, AXON_BLE_COMPANY_ID};
        const char* names[] = {
            NULL,
            "",
            "ESP32",
            "Penguin-1234567890",
            "penguin-42",
            "FS Ext Battery",
            "Unit 7 FS Ext Battery",
            "Flock of Seagulls",
            "MyPenguinSpeaker"};
        const uint8_t a_flock[6] = {0x3c, 0x91, 0x80, 0x00, 0x00, 0x01};
        const uint8_t a_off[6] = {0xde, 0xad, 0xbe, 0xef, 0x00, 0x02};
        const uint8_t* addrs[] = {NULL, a_flock, a_off};

        int mismatches = 0;
        for(size_t ci = 0; ci < sizeof(companies) / sizeof(companies[0]); ci++) {
            for(size_t ni = 0; ni < sizeof(names) / sizeof(names[0]); ni++) {
                for(int rv = 0; rv < 2; rv++) {
                    for(size_t ai = 0; ai < sizeof(addrs) / sizeof(addrs[0]); ai++) {
                        FlockBleTell t =
                            flock_ble_tell(companies[ci], names[ni], rv != 0, addrs[ai]);
                        // Tells at or above OuiOnly are the non-specific ones.
                        FlockConfidence implied =
                            (t != FlockBleTellNone && t != FlockBleTellOuiOnly) ?
                                FlockConfidenceConfirmed :
                                FlockConfidencePossible;
                        FlockConfidence actual =
                            flock_ble_confidence(companies[ci], names[ni], rv != 0);
                        if(implied != actual) mismatches++;
                    }
                }
            }
        }
        CHECK_INT_EQ(mismatches, 0);
    }

    // --- flock_ble_name_is_flock: the specificity test used for name upgrades -
    // A device advertising several identities from ONE address (the bench emitter
    // does exactly this) must not be named permanently by whichever advert landed
    // first. This predicate decides when a later name is more informative.
    CHECK(flock_ble_name_is_flock("Penguin-1234567890"));
    CHECK(flock_ble_name_is_flock("penguin-42"));
    CHECK(flock_ble_name_is_flock("FS Ext Battery"));
    CHECK(flock_ble_name_is_flock("Unit 7 fs ext battery"));
    CHECK(!flock_ble_name_is_flock("ESP32")); // the name that caused all this
    CHECK(!flock_ble_name_is_flock("MyPenguinSpeaker")); // prefix test, not substring
    CHECK(!flock_ble_name_is_flock("Penguin")); // the dash is the anchor
    CHECK(!flock_ble_name_is_flock("PenguinSpeaker"));
    CHECK(!flock_ble_name_is_flock(""));
    CHECK(!flock_ble_name_is_flock(NULL));

    // --- B15: field-observed Flock BLE names added 2026-09-07 ---------------
    //
    // These reach CONFIRMED, so each has to be a string an ordinary device would
    // not choose. The negatives below are the load-bearing half: they are what
    // stops this from becoming the v0.46 `Flock-Guest` over-claim on the BLE path.
    CHECK(flock_ble_name_is_flock("Pigvision"));
    CHECK(flock_ble_name_is_flock("PIGVISION-3"));
    CHECK(flock_ble_name_is_flock("FlockCam"));
    CHECK(flock_ble_name_is_flock("RWLS-38:5B:44:B3:0F:5A")); // as observed in the field
    CHECK(flock_ble_name_is_flock("FS-1A2B3C")); // "FS-" + exactly six hex
    CHECK(flock_ble_name_is_flock("fs-abcdef"));

    // A BARE "Flock" prefix is deliberately NOT a tell here. The BLE path has no
    // Likely rung -- flock_ble_confidence() returns only Confirmed or Possible --
    // so a loose match would promote anything flock-ish straight to Confirmed.
    // That is exactly the v0.46 bug, which the Wi-Fi side can absorb (it has a
    // Likely rung to land on) and this side cannot.
    CHECK(!flock_ble_name_is_flock("Flock-Guest"));
    CHECK(!flock_ble_name_is_flock("Flock"));
    CHECK(!flock_ble_name_is_flock("flocking-awesome"));

    // "FS-" must be SHAPED, not a two-letter prefix: too short, too many
    // ordinary devices start that way.
    CHECK(!flock_ble_name_is_flock("FS-")); // nothing after the dash
    CHECK(!flock_ble_name_is_flock("FS-12")); // too short
    CHECK(!flock_ble_name_is_flock("FS-1A2B3C4")); // trailing junk -> not the form
    CHECK(!flock_ble_name_is_flock("FS-XYZQRS")); // right length, not hex
    CHECK(!flock_ble_name_is_flock("FS")); // no dash; must not read past the NUL
    CHECK(!flock_ble_name_is_flock("F")); // one byte then NUL
    CHECK(!flock_ble_name_is_flock("RWLS")); // the dash is part of the tell
    CHECK(!flock_ble_name_is_flock("Pig")); // prefix of a tell is not a tell

    // The new tells must reach Confirmed through the real scoring entry point,
    // not merely through the helper -- a green test over a function nothing calls
    // proves nothing (see CLAUDE.md).
    CHECK_INT_EQ(
        flock_ble_confidence(BLE_COMPANY_NONE, "Pigvision", false), FlockConfidenceConfirmed);
    CHECK_INT_EQ(
        flock_ble_confidence(BLE_COMPANY_NONE, "RWLS-38:5B:44:B3:0F:5A", false),
        FlockConfidenceConfirmed);
    CHECK_INT_EQ(
        flock_ble_confidence(BLE_COMPANY_NONE, "FS-1A2B3C", false), FlockConfidenceConfirmed);
    CHECK_INT_EQ(
        flock_ble_confidence(BLE_COMPANY_NONE, "Flock-Guest", false), FlockConfidencePossible);
    CHECK_INT_EQ(flock_ble_tell(BLE_COMPANY_NONE, "Pigvision", false, NULL), FlockBleTellNaming);

    // --- the name specificity ladder ---------------------------------------
    CHECK_INT_EQ(flock_ble_name_specificity(NULL), 0);
    CHECK_INT_EQ(flock_ble_name_specificity(""), 0);
    // 1: stock defaults that identify nothing.
    CHECK_INT_EQ(flock_ble_name_specificity("ESP32"), 1);
    CHECK_INT_EQ(flock_ble_name_specificity("ESP32-WROOM"), 1);
    CHECK_INT_EQ(flock_ble_name_specificity("ESP_CFDD91"), 1);
    CHECK_INT_EQ(flock_ble_name_specificity("Arduino"), 1);
    CHECK_INT_EQ(flock_ble_name_specificity("BT"), 1);
    CHECK_INT_EQ(flock_ble_name_specificity("BLE"), 1);
    // ...but a real name that merely STARTS like one is not a default.
    CHECK_INT_EQ(flock_ble_name_specificity("BTLE-Cam-3"), 2);
    // 2: an ordinary name the device actually chose.
    CHECK_INT_EQ(flock_ble_name_specificity("bench-raven"), 2);
    CHECK_INT_EQ(flock_ble_name_specificity("Flock of Seagulls"), 2);
    // 3: self-identifying -- Flock naming, or a bare serial (newer firmware
    // drops "Penguin-" and advertises the serial as the whole name).
    CHECK_INT_EQ(flock_ble_name_specificity("Penguin-1234567890"), 3);
    CHECK_INT_EQ(flock_ble_name_specificity("FS Ext Battery"), 3);
    CHECK_INT_EQ(flock_ble_name_specificity("1234567890"), 3);

    // DENYLIST SAFETY: no stock-default pattern may ever match a Flock name.
    // If one did, a real detection's name could be displaced as if it were junk.
    const char* flock_names[] = {
        "Penguin-1234567890", "penguin-42", "FS Ext Battery", "1234567890"};
    for(size_t i = 0; i < sizeof(flock_names) / sizeof(flock_names[0]); i++) {
        CHECK_INT_EQ(flock_ble_name_specificity(flock_names[i]), 3);
    }

    // Replacement happens only on a STRICT increase.
    CHECK(flock_ble_name_should_replace("ESP32", "bench-raven"));
    CHECK(flock_ble_name_should_replace("ESP32", "Penguin-1"));
    CHECK(flock_ble_name_should_replace("bench-raven", "FS Ext Battery"));
    CHECK(!flock_ble_name_should_replace("Penguin-1", "ESP32")); // never downgrade
    CHECK(!flock_ble_name_should_replace("bench-raven", "ESP32"));
    CHECK(!flock_ble_name_should_replace("Penguin-1", "FS Ext Battery")); // equal
    CHECK(!flock_ble_name_should_replace("bench-raven", "other-name")); // equal
    CHECK(!flock_ble_name_should_replace("ESP32", ""));

    // THE INCIDENT, ENCODED. A device advertised the stack default first and
    // identified itself afterwards; the app latched "ESP32" forever and the row
    // was read as an unrelated gadget, which cost a shipped detection regression.
    // ANTI-FLAP: feed the alternating sequence many times and assert the stored
    // name only ever climbs, changes at most 3 times, and settles.
    {
        const char* seq[] = {"ESP32", "bench-raven", "ESP32", "FS Ext Battery", "ESP32"};
        char stored[33] = "";
        int changes = 0;
        for(int pass = 0; pass < 40; pass++) {
            for(size_t i = 0; i < sizeof(seq) / sizeof(seq[0]); i++) {
                if(stored[0] == '\0' || flock_ble_name_should_replace(stored, seq[i])) {
                    snprintf(stored, sizeof(stored), "%s", seq[i]);
                    changes++;
                }
            }
        }
        CHECK_STR_EQ(stored, "FS Ext Battery"); // settled on the most specific
        CHECK(changes <= 3); // bounded: 200 sightings, at most 3 renames
    }

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
