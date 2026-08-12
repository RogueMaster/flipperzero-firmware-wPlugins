// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt
//
// Confidence-scoring truth tables for flock_db. Locks in B6 (strict
// "Flock-XXXXXX" provisioning-AP anchoring) and the precision-first contracts:
// OUI-only never confirms, user IE-fingerprints stay UNVERIFIED.
#include "flock_db.h"
#include "test.h"

#include <stdio.h>

void suite_flock_db(void) {
    printf("[flock_db]\n");

    // --- flock_ssid_confidence ---------------------------------------------
    CHECK_INT_EQ(flock_ssid_confidence(NULL), FlockConfidenceNone);
    CHECK_INT_EQ(flock_ssid_confidence(""), FlockConfidenceNone);

    // Exactly "Flock-" + 6 hex -> Confirmed (the provisioning AP), any case.
    CHECK_INT_EQ(flock_ssid_confidence("Flock-A1B2C3"), FlockConfidenceConfirmed);
    CHECK_INT_EQ(flock_ssid_confidence("flock-a1b2c3"), FlockConfidenceConfirmed);
    CHECK_INT_EQ(flock_ssid_confidence("Flock-000000"), FlockConfidenceConfirmed);

    // B6 regression: benign names that merely CONTAIN "flock-" must NOT confirm.
    // They fall through to "Likely" (still contain the "flock" substring).
    //
    // These checks were DECORATIVE until v0.47. flock_score() has no production
    // caller and flock_ssid_confidence() was reached only from the Marauder
    // scraper, so on the default (companion) backend nothing consulted this rule
    // -- the app printed the ESP's looser verdict and "Flock-Guest" really did
    // show as CONFIRMED. esp_parser.c now re-derives any claimed Confirmed
    // through this function, so these rows finally back a live guard. The
    // companion-path versions live in test_esp_parser.c; keep both.
    CHECK_INT_EQ(flock_ssid_confidence("Flock-Guest"), FlockConfidenceLikely);
    CHECK_INT_EQ(flock_ssid_confidence("Flock Freight WiFi"), FlockConfidenceLikely);
    CHECK_INT_EQ(flock_ssid_confidence("Flock-12345"), FlockConfidenceLikely); // 5 hex: too short
    CHECK_INT_EQ(flock_ssid_confidence("Flock-1234567"), FlockConfidenceLikely); // 7: too long
    CHECK_INT_EQ(flock_ssid_confidence("Flock-GHIJKL"), FlockConfidenceLikely); // non-hex

    // Built-in service SSID + weaker substrings.
    CHECK_INT_EQ(flock_ssid_confidence("test_flck"), FlockConfidenceConfirmed);
    CHECK_INT_EQ(flock_ssid_confidence("MyFlockNet"), FlockConfidenceLikely);
    CHECK_INT_EQ(flock_ssid_confidence("somethingflck"), FlockConfidenceLikely);
    CHECK_INT_EQ(flock_ssid_confidence("Starbucks"), FlockConfidenceNone);

    // --- flock_oui_match ----------------------------------------------------
    const uint8_t known[6] = {0xb4, 0x1e, 0x52, 0x00, 0x00, 0x01}; // Flock's own OUI
    const uint8_t unknown[6] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
    CHECK(flock_oui_match(known));
    CHECK(!flock_oui_match(unknown));
    CHECK(!flock_oui_match(NULL));

    // --- RETRACTED prefixes must never match --------------------------------
    // Upstream tracked each of these and then withdrew it. Re-adding one is
    // always a bug, never a rediscovery: the older flat OUI list still carries
    // some of them and has no status column to record the doubt.
    //
    // THIS BLOCK EXISTS BECAUSE THE COMMENT WAS NOT ENOUGH. f8:a2:d6 was dropped
    // for v0.44, then silently re-added by 93beede (2026-08-05) while the tables
    // were reflowed, and shipped in v0.67-v0.71. Every test stayed green and the
    // parity gate stayed green (it compared the two tables to each other, and
    // both had drifted the same way). Nothing asserted absence, so nothing
    // noticed. tools/check_oui_parity.py now enforces the same denylist from the
    // other side; keep BOTH -- the CI gate catches a source edit, this catches a
    // matcher that starts accepting one for some other reason.
    //
    // The pairing with pos[]/neg[] below matters: assert the NEGATIVE case, and
    // keep a positive control beside it so a matcher that has stopped matching
    // anything at all cannot pass this block by accident.
    static const uint8_t retracted[][6] = {
        {0xf8, 0xa2, 0xd6, 0x00, 0x00, 0x01}, // "low confidence; hit on a Sony Media Player"
        {0x6c, 0xcd, 0xd6, 0x00, 0x00, 0x01}, // "Nope - Netgear" (misattributed)
        {0x94, 0x2a, 0x6f, 0x00, 0x00, 0x01}, // "Nope - Ubiquiti" (misattributed)
        {0xf4, 0xe2, 0xc6, 0x00, 0x00, 0x01}, // "Nope - Ubiquiti" (misattributed)
        {0xcc, 0xcc, 0xcc, 0x00, 0x00, 0x01}, // "No clue; no hits"
        {0x00, 0x0c, 0xe7, 0x00, 0x00, 0x01}, // MediaTek, "possible false positive"
    };
    for(size_t i = 0; i < sizeof(retracted) / sizeof(retracted[0]); i++) {
        CHECK(!flock_oui_match(retracted[i]));
        CHECK(!soundthinking_oui_match(retracted[i]));
        // A retracted prefix must also not be reachable as a device class or a
        // method label -- those are what the UI actually prints.
        CHECK_INT_EQ(flock_class_from_mac(retracted[i]), FlockClassAlpr);
        CHECK_INT_EQ(flock_method_of(retracted[i], "", 'B', 0), FlockMethodUnknown);
    }

    // Positive control: a prefix that IS in the table still matches, so the loop
    // above cannot be satisfied by a matcher that rejects everything.
    CHECK(flock_oui_match(known));
    CHECK_INT_EQ(flock_method_of(known, "", 'B', 0), FlockMethodOui);

    // The built-in table's size is pinned so an accidental add/remove shows up
    // here as well as in the CI parity gate. If you intentionally change the
    // table, update this number AND both files' count comments in the same
    // commit -- that is the drift 93beede left behind for five releases.
    CHECK_INT_EQ((int)flock_oui_count(), 31);

    // --- extra OUIs (user signatures merged OVER the built-ins) --------------
    static const uint8_t extra[][3] = {{0x11, 0x22, 0x33}};
    FlockDbExtras ex_oui = {.ouis = extra, .oui_count = 1};
    flock_db_set_extras(&ex_oui);
    CHECK(flock_oui_match(unknown)); // now matches via the extra
    flock_db_set_extras(NULL);
    CHECK(!flock_oui_match(unknown)); // cleared -> built-ins only (fail-safe)

    // --- extra SSID patterns (needles are lower-case per the contract) -------
    static const char* const conf[] = {"acme-cam"};
    static const char* const like[] = {"widgetcorp"};
    FlockDbExtras ex_ssid = {
        .ssid_confirmed = conf,
        .ssid_confirmed_count = 1,
        .ssid_likely = like,
        .ssid_likely_count = 1};
    flock_db_set_extras(&ex_ssid);
    CHECK_INT_EQ(flock_ssid_confidence("ACME-CAM-07"), FlockConfidenceConfirmed);
    CHECK_INT_EQ(flock_ssid_confidence("WidgetCorp Guest"), FlockConfidenceLikely);
    flock_db_set_extras(NULL);
    CHECK_INT_EQ(flock_ssid_confidence("ACME-CAM-07"), FlockConfidenceNone);

    // --- IE-fingerprint match + UNVERIFIED user cap contract ----------------
    CHECK_INT_EQ(flock_ie_fp_match(0), FlockIeFpNone); // 0 = "no fingerprint"
    CHECK_INT_EQ(flock_ie_fp_match(0xdeadbeef), FlockIeFpNone); // built-in table ships empty
    static const uint32_t ufps[] = {0xdeadbeef};
    FlockDbExtras ex_fp = {.ie_fps = ufps, .ie_fp_count = 1};
    flock_db_set_extras(&ex_fp);
    CHECK_INT_EQ(flock_ie_fp_match(0xdeadbeef), FlockIeFpUser); // user match, never "builtin"
    CHECK_INT_EQ(flock_ie_fp_match(0x12345678), FlockIeFpNone);
    flock_db_set_extras(NULL);
    CHECK_INT_EQ(flock_ie_fp_match(0xdeadbeef), FlockIeFpNone);

    // NOTE: the combined-ladder assertions that used to sit here tested
    // flock_score(), which had no production caller. They now live in
    // test_esp_parser.c against esp_parse_companion_line(), the boundary the
    // product actually uses. Do not re-add a scorer here to test.
    const uint8_t nomac[6] = {0, 0, 0, 0, 0, 0};

    // --- SoundThinking: a SEPARATE device class -----------------------------
    // The two tables must stay disjoint. If a prefix ever appeared in both, a
    // gunshot sensor would be reported as a camera (or vice versa) depending on
    // which matcher ran first, which is exactly the over-claim we forbid.
    const uint8_t st[6] = {0xd4, 0x11, 0xd6, 0x01, 0x02, 0x03};
    CHECK(soundthinking_oui_match(st));
    CHECK(!flock_oui_match(st));
    CHECK(!soundthinking_oui_match(known));
    CHECK(!soundthinking_oui_match(nomac));
    CHECK(!soundthinking_oui_match(NULL));
    for(size_t i = 0; i < flock_oui_count(); i++) {
        const uint8_t* p = flock_oui_get(i);
        uint8_t probe[6] = {p[0], p[1], p[2], 0, 0, 0};
        CHECK(!soundthinking_oui_match(probe));
    }

    // Class is derived from the OUI and defaults to ALPR for anything unknown.
    CHECK_INT_EQ(flock_class_from_mac(st), FlockClassAcoustic);
    CHECK_INT_EQ(flock_class_from_mac(known), FlockClassAlpr);
    CHECK_INT_EQ(flock_class_from_mac(nomac), FlockClassAlpr);

    // An acoustic sensor has no known SSID tell, so it can never reach Confirmed
    // on its own behaviour. Asserted here at the level this file owns -- the
    // rung it actually reaches is pinned in test_esp_parser.c.
    CHECK_INT_EQ(flock_ssid_confidence("linksys"), FlockConfidenceNone);

    // A user signature file cannot smuggle in an acoustic prefix: its schema has
    // no class field, so an extra OUI is always read as ALPR.
    static const uint8_t uext[][3] = {{0xaa, 0xbb, 0xcc}};
    FlockDbExtras ex_cls = {.ouis = uext, .oui_count = 1};
    flock_db_set_extras(&ex_cls);
    const uint8_t umac[6] = {0xaa, 0xbb, 0xcc, 0, 0, 1};
    CHECK(flock_oui_match(umac));
    CHECK(!soundthinking_oui_match(umac));
    CHECK_INT_EQ(flock_class_from_mac(umac), FlockClassAlpr);
    flock_db_set_extras(NULL);

    // --- class label strings ------------------------------------------------
    CHECK_STR_EQ(flock_class_str(FlockClassAlpr), "ALPR");
    CHECK_STR_EQ(flock_class_str(FlockClassAcoustic), "Acoustic");
    CHECK_STR_CONTAINS(flock_class_long_str(FlockClassAcoustic), "SoundThinking");
    CHECK_STR_CONTAINS(flock_class_long_str(FlockClassAlpr), "ALPR");

    // --- confidence label strings ------------------------------------------
    CHECK_STR_EQ(flock_confidence_str(FlockConfidenceConfirmed), "CONFIRMED");
    CHECK_STR_EQ(flock_confidence_str(FlockConfidenceProbeFp), "Class?");
    CHECK_STR_EQ(flock_confidence_str(FlockConfidenceLikely), "Likely");
    CHECK_STR_EQ(flock_confidence_str(FlockConfidencePossible), "Possible");
    CHECK_STR_EQ(flock_confidence_str(FlockConfidenceNone), "-");

    // --- flock_method_of: WHY a detection is on the list (issue #5) ---------
    // 3c:91:80 is a real table prefix; aa:bb:cc is in no table.
    const uint8_t oui_mac[6] = {0x3c, 0x91, 0x80, 0x11, 0x22, 0x33};
    const uint8_t off_mac[6] = {0xaa, 0xbb, 0xcc, 0x11, 0x22, 0x33};
    const uint8_t st_mac[6] = {0xd4, 0x11, 0xd6, 0x11, 0x22, 0x33};

    // Strongest re-derivable indicator wins. An SSID pattern outranks the OUI
    // even when both match.
    CHECK_INT_EQ(flock_method_of(oui_mac, "flock-a1b2c3", 'B', 0), FlockMethodSsid);
    CHECK_INT_EQ(flock_method_of(off_mac, "flock-a1b2c3", 'B', 0), FlockMethodSsid);
    CHECK_INT_EQ(flock_method_of(off_mac, "myflock", 'B', 0), FlockMethodSsid); // "Likely" needle

    // This is h00die's actual screenshot (issue #5): a 3c:91:80 beacon whose SSID
    // is just its own MAC. Nothing about that name is a Flock pattern, so the OUI
    // is the ONLY thing that put it on the list -- which is exactly what the user
    // could not tell from a bare "Possible".
    CHECK_INT_EQ(flock_method_of(oui_mac, "3C9180112233", 'B', 0), FlockMethodOui);
    CHECK_INT_EQ(flock_method_of(oui_mac, "", 'B', 0), FlockMethodOui);
    CHECK_INT_EQ(flock_method_of(oui_mac, NULL, 'P', 0), FlockMethodOui);

    // An acoustic prefix is an OUI match too -- it is simply the other device
    // class. Reporting it as unclassified would hide the one indicator we have.
    CHECK_INT_EQ(flock_method_of(st_mac, "", 'B', 0), FlockMethodOui);

    // A BLE sighting is classified on the companion (mfg 0x09C8 / Raven GATT)
    // from advert bytes that never reach this side; name the source, don't guess.
    CHECK_INT_EQ(flock_method_of(off_mac, "", 'L', 0), FlockMethodBle);
    // ...but a re-derivable indicator still outranks it.
    CHECK_INT_EQ(flock_method_of(oui_mac, "", 'L', 0), FlockMethodOui);
    CHECK_INT_EQ(flock_method_of(off_mac, "flock-a1b2c3", 'L', 0), FlockMethodSsid);

    // Nothing we can re-derive: the companion scored probe behaviour we never
    // see. "Unknown" here is the honest answer, not a failure.
    CHECK_INT_EQ(flock_method_of(off_mac, "linksys", 'P', 0), FlockMethodUnknown);
    CHECK_INT_EQ(flock_method_of(NULL, NULL, 'O', 0), FlockMethodUnknown);

    // The built-in IE-fp table ships EMPTY, so fp matching is inert by design and
    // a non-zero fp alone must NOT be reported as a fingerprint match.
    CHECK_INT_EQ(flock_ie_fp_match(0xdeadbeefu), FlockIeFpNone);
    CHECK_INT_EQ(flock_method_of(off_mac, "linksys", 'F', 0xdeadbeefu), FlockMethodUnknown);

    // Registering that fp as a USER signature makes it matchable, and it then
    // outranks the OUI (but not an SSID pattern -- see the ladder above).
    static const uint32_t method_fps[] = {0xdeadbeefu};
    FlockDbExtras ex_method_fp = {.ie_fps = method_fps, .ie_fp_count = 1};
    flock_db_set_extras(&ex_method_fp);
    CHECK_INT_EQ(flock_method_of(off_mac, "linksys", 'F', 0xdeadbeefu), FlockMethodIeFp);
    CHECK_INT_EQ(flock_method_of(oui_mac, "", 'F', 0xdeadbeefu), FlockMethodIeFp);
    CHECK_INT_EQ(flock_method_of(oui_mac, "flock-a1b2c3", 'F', 0xdeadbeefu), FlockMethodSsid);
    // fp == 0 means "no fingerprint" and must never match, even with extras live.
    CHECK_INT_EQ(flock_method_of(oui_mac, "", 'F', 0), FlockMethodOui);
    flock_db_set_extras(NULL);

    // --- method label strings ----------------------------------------------
    CHECK_STR_EQ(flock_method_str(FlockMethodSsid), "SSID");
    CHECK_STR_EQ(flock_method_str(FlockMethodIeFp), "IE fp");
    CHECK_STR_EQ(flock_method_str(FlockMethodOui), "OUI");
    CHECK_STR_EQ(flock_method_str(FlockMethodBle), "BLE mfg ID");
    // Not "none": the companion DID score it, on evidence we cannot re-derive.
    CHECK_STR_EQ(flock_method_str(FlockMethodUnknown), "ESP probe rule");

    // WIDTH BUDGET. The detail screen composes "Method: <label> + <frame>" onto
    // one 128 px row that also carries a scrollbar -- about 26 characters. The
    // longest frame phrase is "probe resp" (10), and "Method: " + " + " costs 11,
    // so a label over 5 chars can only be used ALONE (BLE mfg ID / ESP probe rule
    // are, in flock_detail_view.c). This is not cosmetic: the first draft used
    // "OUI prefix"/"IE fingerprint" and the composed line ran off the screen edge
    // on real hardware, which no host test could see.
    CHECK(strlen(flock_method_str(FlockMethodSsid)) <= 5);
    CHECK(strlen(flock_method_str(FlockMethodIeFp)) <= 5);
    CHECK(strlen(flock_method_str(FlockMethodOui)) <= 5);
    // 8 + 5 + 3 + 10 = 26, the widest line that fits.
    CHECK(strlen("Method: ") + 5 + strlen(" + ") + strlen("probe resp") <= 26);
}
