// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt
//
// Confidence-scoring truth tables for flock_db. Locks in B6 (strict
// "Flock-XXXXXX" provisioning-AP anchoring) and the precision-first contracts:
// OUI-only never confirms, user IE-fingerprints stay UNVERIFIED.
#include "flock_db.h"
#include "test.h"

#include <stdio.h>
#include <string.h> // strstr, for the class-label assertion below

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

    // --- Axon Enterprise: a THIRD device class ------------------------------
    // 00:25:df is Axon's only IEEE registration. What matters here is not just
    // that it matches, but that it stays OUT of the other two classes: an Axon
    // body camera moves with a person, so reporting it as an ALPR would claim a
    // camera on a pole that was never seen.
    const uint8_t axon[6] = {0x00, 0x25, 0xdf, 0x00, 0x00, 0x01};
    CHECK(axon_oui_match(axon));
    CHECK(!axon_oui_match(NULL));
    CHECK(!flock_oui_match(axon)); // not an ALPR
    CHECK(!soundthinking_oui_match(axon)); // not an acoustic sensor
    CHECK_INT_EQ(flock_class_from_mac(axon), FlockClassBodycam);
    CHECK_STR_EQ(
        flock_class_str(FlockClassBodycam), "Body cam"); // covers Axon, Utility, Digital Ally
    // The long label must not contain the word "camera" -- see flock_class_long_str.
    CHECK(strstr(flock_class_long_str(FlockClassBodycam), "camera") == NULL);
    // An Axon OUI is still an OUI match for method-labelling purposes.
    CHECK_INT_EQ(flock_method_of(axon, "", 'B', 0), FlockMethodOui);

    // The three tables are DISJOINT in both directions: a Flock or SoundThinking
    // prefix must never be read as Axon.
    CHECK(!axon_oui_match(known)); // b4:1e:52 is Flock's own OUI
    const uint8_t st_disjoint[6] = {0xd4, 0x11, 0xd6, 0x00, 0x00, 0x01};
    CHECK(!axon_oui_match(st_disjoint));
    CHECK_INT_EQ(flock_class_from_mac(st_disjoint), FlockClassAcoustic);
    CHECK_INT_EQ(flock_class_from_mac(known), FlockClassAlpr);

    // NAME-COLLISION GUARD. "Axon Networks Inc" is an unrelated networking
    // company that also holds IEEE registrations, and a vendor-database search
    // for "axon" returns it alongside Axona, Axonne, Interaxon, Maxon, Praxon,
    // Paxonet and Yaxon. None are police equipment. If any of these ever match,
    // somebody imported by substring instead of verifying at the registry.
    static const uint8_t not_axon[][6] = {
        {0x00, 0x58, 0x28, 0, 0, 1}, // Axon Networks Inc
        {0x00, 0xc0, 0xd4, 0, 0, 1}, // Axon Networks, Inc.
        {0x84, 0x70, 0x03, 0, 0, 1}, // Axon Networks Inc
        {0xfc, 0x85, 0x96, 0, 0, 1}, // Axonne Inc.
        {0x00, 0x24, 0x42, 0, 0, 1}, // Axona Limited
        {0x00, 0x04, 0xeb, 0, 0, 1}, // Paxonet Communications
        {0x00, 0x90, 0x6e, 0, 0, 1}, // Praxon, Inc.
        {0xb4, 0xa3, 0x05, 0, 0, 1}, // Xiamen Yaxon Network
        {0xf4, 0x46, 0x2a, 0, 0, 1}, // maxon zub
    };
    for(size_t i = 0; i < sizeof(not_axon) / sizeof(not_axon[0]); i++) {
        CHECK(!axon_oui_match(not_axon[i]));
        CHECK(!flock_oui_match(not_axon[i]));
        CHECK(!soundthinking_oui_match(not_axon[i]));
    }

    // MISATTRIBUTION GUARD. A curated "law enforcement OUI" list circulating
    // upstream was checked prefix-by-prefix against the IEEE registry and 11 of
    // its 15 entries were wrong. These are the real registrants of prefixes that
    // list filed as police equipment; every one must stay unmatched.
    static const uint8_t misattributed[][6] = {
        {0x00, 0x1f, 0x55, 0, 0, 1}, // claimed Axon/TASER -- really Honeywell Security
        {0x00, 0x0f, 0x13, 0, 0, 1}, // claimed Axon/TASER -- really Nisca
        {0x00, 0x11, 0x24, 0, 0, 1}, // claimed Digital Ally -- really Apple
        {0x00, 0x1b, 0x63, 0, 0, 1}, // claimed Digital Ally -- really Apple
        {0x00, 0x1a, 0xe9, 0, 0, 1}, // claimed WatchGuard -- really Nintendo
        {0xe0, 0x13, 0x33, 0, 0, 1}, // claimed Panasonic i-PRO -- really General Motors
        {0x3c, 0xbb, 0xfd, 0, 0, 1}, // claimed Panasonic i-PRO -- really Samsung
        {0x50, 0xec, 0x50, 0, 0, 1}, // claimed Getac -- really Xiaomi
        {0x00, 0x1c, 0x23, 0, 0, 1}, // claimed Getac -- really Dell
        {0x00, 0x40, 0x8c, 0, 0, 1}, // claimed Flock Safety -- really Axis Communications
        {0xac, 0xcc, 0x8e, 0, 0, 1}, // claimed Flock Safety -- really Axis Communications
    };
    for(size_t i = 0; i < sizeof(misattributed) / sizeof(misattributed[0]); i++) {
        CHECK(!axon_oui_match(misattributed[i]));
        CHECK(!flock_oui_match(misattributed[i]));
        CHECK(!soundthinking_oui_match(misattributed[i]));
    }

    // --- DEMOTED prefixes: out of the built-ins, into the seed file ---------
    // 48:27:ea is registered to SAMSUNG ELECTRONICS and a4:cf:12 to Espressif,
    // and upstream rates both "low confidence, WiGLE crowdsource" -- its weakest
    // tier. While they were compiled in, any Samsung- or ESP32-based device that
    // sent a wildcard probe (i.e. scanned for a network, which every client does)
    // scored LIKELY. That is the reported T-Mobile hotspot false positive.
    //
    // They are NOT on the retracted denylist below: nothing says they are wrong,
    // only that they are unverified, so they live in docs/signatures.seed.json
    // and a user can opt back into them. This asserts they are out of the
    // BUILT-INS, which is the claim of field corroboration they failed.
    static const uint8_t demoted[][6] = {
        {0x48, 0x27, 0xea, 0x00, 0x00, 0x01}, // Samsung Electronics
        {0xa4, 0xcf, 0x12, 0x00, 0x00, 0x01}, // Espressif
    };
    for(size_t i = 0; i < sizeof(demoted) / sizeof(demoted[0]); i++) {
        CHECK(!flock_oui_match(demoted[i]));
        CHECK(!soundthinking_oui_match(demoted[i]));
        CHECK(!axon_oui_match(demoted[i]));
    }
    // ...but a user file can still opt back in, which is the point of demoting
    // rather than retracting.
    static const uint8_t optin[][3] = {{0x48, 0x27, 0xea}};
    FlockDbExtras ex_demoted = {.ouis = optin, .oui_count = 1};
    flock_db_set_extras(&ex_demoted);
    CHECK(flock_oui_match(demoted[0]));
    flock_db_set_extras(NULL);
    CHECK(!flock_oui_match(demoted[0]));

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
    CHECK_INT_EQ((int)flock_oui_count(), 32); // +14:b5:cd (Liteon), 2026-09-10

    // The 2026-09-07 community-table sweep, pinned both ways. Every prefix here
    // was resolved against the IEEE MA-L registry before the verdict; the
    // organisation name is the whole argument, not the list it appeared on.
    static const uint8_t added_liteon[6] = {0xe0, 0x0a, 0xf6, 0x01, 0x02, 0x03};
    static const uint8_t added_silabs[6] = {0x38, 0x5b, 0x44, 0x01, 0x02, 0x03};
    CHECK(flock_oui_match(added_liteon)); // Liteon, same vendor as 21 built-ins
    CHECK(flock_oui_match(added_silabs)); // SiLabs, corroborated by "RWLS-38:5B:44:.."

    // REJECTED, and asserted absent so a future "let's widen recall" import
    // cannot land them quietly. tools/check_oui_parity.py::TOO_GENERIC blocks
    // them at CI; this is the same claim at the library boundary, so the rule
    // survives someone editing only one of the two.
    static const uint8_t samsung[6] = {0x48, 0x27, 0xea, 0, 0, 0}; // phones/hotspots
    static const uint8_t espressif[6] = {0xa4, 0xcf, 0x12, 0, 0, 0}; // our OWN board
    static const uint8_t ubiquiti[6] = {0xf0, 0x9f, 0xc2, 0, 0, 0};
    static const uint8_t ieee_ra[6] = {0x8c, 0x1f, 0x64, 0, 0, 0}; // shared MA-M/S block
    CHECK(!flock_oui_match(samsung));
    CHECK(!flock_oui_match(espressif));
    CHECK(!flock_oui_match(ubiquiti));
    CHECK(!flock_oui_match(ieee_ra));
    // f8:a2:d6 is on the same community lists and was tempting for the same
    // reason (it IS Liteon) -- it is covered by the retracted[] block above,
    // which is where a third removal would also belong.

    // --- v0.88 vendor tables: drones and body cams --------------------------
    //
    // Each vendor is asserted to resolve to ITS OWN vendor and class. The whole
    // point of these tables is attribution, so a table that matched but named the
    // wrong company would be worse than no table -- that is the Motorola Mobility
    // failure the misattributed list exists for.
    static const uint8_t dji[6] = {0x60, 0x60, 0x1f, 1, 2, 3};
    static const uint8_t skydio[6] = {0x38, 0x1d, 0x14, 1, 2, 3};
    static const uint8_t utility[6] = {0x00, 0x09, 0xbc, 1, 2, 3};
    static const uint8_t dally[6] = {0x00, 0x23, 0xbd, 1, 2, 3};
    CHECK_INT_EQ(flock_vendor_from_mac(dji), FlockVendorDrone);
    CHECK_INT_EQ(flock_vendor_from_mac(skydio), FlockVendorDrone);
    CHECK_INT_EQ(flock_vendor_from_mac(utility), FlockVendorUtility);
    CHECK_INT_EQ(flock_vendor_from_mac(dally), FlockVendorDigitalAlly);
    CHECK_STR_EQ(flock_class_str(FlockClassDrone), "Drone");
    CHECK_STR_EQ(flock_device_long_str(FlockVendorDrone, FlockClassDrone), "Unmanned aircraft");
    CHECK_STR_EQ(flock_device_long_str(FlockVendorUtility, FlockClassBodycam), "Utility BodyWorn");

    // A drone OUI must NEVER come back as a Flock camera -- the exact over-claim
    // FlockVendor was introduced to stop. Nor may it reach the Flock matcher.
    CHECK(!flock_oui_match(dji));
    CHECK(!flock_oui_match(utility));
    CHECK_INT_EQ(flock_vendor_of(dji, ""), FlockVendorDrone);

    // REJECTED drone-adjacent prefixes. Every one appears in a community drone
    // list; every one would misattribute.
    static const uint8_t ronin[6] = {0xf8, 0x40, 0x68, 0, 0, 0}; // DJI, but a gimbal
    static const uint8_t osmo[6] = {0x20, 0x1f, 0x55, 0, 0, 0}; // DJI, but a handheld
    static const uint8_t autelan[6] = {0x4c, 0x48, 0xda, 0, 0, 0}; // networking, not Autel
    static const uint8_t autel_ma_m[6] = {0xec, 0x5b, 0xcd, 0, 0, 0}; // shared IEEE block
    static const uint8_t vievu[6] = {0xfc, 0x01, 0x9e, 0, 0, 0}; // discontinued line
    static const uint8_t ipro[6] = {0xd4, 0x2d, 0xc5, 0, 0, 0}; // product not knowable
    CHECK_INT_EQ(flock_vendor_from_mac(ronin), FlockVendorUnknown);
    CHECK_INT_EQ(flock_vendor_from_mac(osmo), FlockVendorUnknown);
    CHECK_INT_EQ(flock_vendor_from_mac(autelan), FlockVendorUnknown);
    CHECK_INT_EQ(flock_vendor_from_mac(autel_ma_m), FlockVendorUnknown);
    CHECK_INT_EQ(flock_vendor_from_mac(vievu), FlockVendorUnknown);
    CHECK_INT_EQ(flock_vendor_from_mac(ipro), FlockVendorUnknown);

    // Every device label still fits the detail screen's 20-character row. The
    // longest new one ("Digital Ally FirstVU") is exactly at the limit, so this
    // is the check that catches the next one that is not.
    for(int v = 0; v <= (int)FlockVendorDrone; v++) {
        for(int c = 0; c <= (int)FlockClassDrone; c++) {
            CHECK((int)strlen(flock_device_long_str((FlockVendor)v, (FlockDevClass)c)) <= 20);
            // 13 is the incumbent ceiling, set by "SoundThinking", which has
            // shipped in the narrow report/list column since v0.77. A new vendor
            // wider than the widest existing one is the thing worth catching --
            // not SoundThinking itself, which is a real company name and is not
            // going to be abbreviated to make a test pass.
            CHECK((int)strlen(flock_vendor_str((FlockVendor)v)) <= 13);
        }
    }

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

    // --- known-generic skeleton denylist ------------------------------------
    // These are commodity WiFi-stack scan skeletons, each with in-repo
    // provenance (see flock_ie_fps_generic[]). A match on one says "this device
    // has WiFi", not "this device is a camera".
    CHECK(flock_ie_fp_is_generic(0x96FCD1B2u)); // stock ESP32 scan (our own emitter)
    CHECK(flock_ie_fp_is_generic(0x173D7A70u)); // common phone stack, clean bench
    CHECK(flock_ie_fp_is_generic(0x7C923B53u)); // smeared across unrelated vendors
    // RETRACTED, and it was our own advice. This project told wiilover22 to put
    // 0x89C3DEBF in his signatures.json; his next drive found it on TEN devices
    // spread over 7.9 km at -17 to -96 dBm. Ten of the nineteen rows in the
    // hits.csv he sent back were phones matched on it.
    CHECK(flock_ie_fp_is_generic(0x89C3DEBFu));
    CHECK(flock_ie_fp_is_generic(0xC59C341Fu)); // 24 randomised MACs, one probe each
    // RETRACTED 2026-10-02. Promoted on geography -- four devices kilometres
    // apart "cannot be riding in his car" -- and then the reporter watched it
    // follow his car. Its signature is a lone WMM element with no Wi-Fi Alliance
    // anchor, i.e. a bare client probe, and the production Flock signature
    // matched zero times in the same 45 KB of survey log.
    CHECK(flock_ie_fp_is_generic(0xBA9FAFA0u));
    CHECK(!flock_ie_fp_is_generic(0)); // 0 is "no fingerprint", not "generic"
    CHECK(!flock_ie_fp_is_generic(0x42D75CD1u)); // the shipped candidates are NOT generic
    CHECK(!flock_ie_fp_is_generic(0xD0BBEC4Cu));
    CHECK(!flock_ie_fp_is_generic(0xdeadbeef));

    // A generic hash never matches on its own.
    CHECK_INT_EQ(flock_ie_fp_match(0x96FCD1B2u), FlockIeFpNone);

    // THE REGRESSION THAT MATTERS. "Confirm: I saw it" on the wrong row writes
    // the selected device's fingerprint to learned.txt, which is merged into the
    // user tier -- so an operator who confirmed a phone once would otherwise
    // carry a hash that flags phones on every street, forever. The denylist has
    // to beat the user tier, not just the learn-time write, because the poisoned
    // files already exist on cards in the field.
    static const uint32_t poisoned[] = {0x96FCD1B2u, 0x173D7A70u, 0x7C923B53u};
    FlockDbExtras ex_generic = {.ie_fps = poisoned, .ie_fp_count = 3};
    flock_db_set_extras(&ex_generic);
    CHECK_INT_EQ(flock_ie_fp_match(0x96FCD1B2u), FlockIeFpNone);
    CHECK_INT_EQ(flock_ie_fp_match(0x173D7A70u), FlockIeFpNone);
    CHECK_INT_EQ(flock_ie_fp_match(0x7C923B53u), FlockIeFpNone);
    // ...and the guard is narrow: a legitimate user fp alongside them still works.
    static const uint32_t mixed[] = {0x96FCD1B2u, 0xabcdef01u};
    FlockDbExtras ex_mixed = {.ie_fps = mixed, .ie_fp_count = 2};
    flock_db_set_extras(&ex_mixed);
    CHECK_INT_EQ(flock_ie_fp_match(0x96FCD1B2u), FlockIeFpNone);
    CHECK_INT_EQ(flock_ie_fp_match(0xabcdef01u), FlockIeFpUser);
    flock_db_set_extras(NULL);

    // THE RETRACTION HAS TO BEAT A CARD ALREADY IN THE FIELD. wiilover22's
    // signatures.json still lists 0x89C3DEBF because we told him to add it, and
    // he will not edit it. Denylisting only works if it outranks his file.
    // BOTH hashes on that card are now retracted, and both must go inert on
    // upgrade without him editing anything. 0xBA9FAFA0 joined 0x89C3DEBF on
    // 2026-10-02 after he reported it following his car.
    static const uint32_t wiilover_card[] = {0x89C3DEBFu, 0xBA9FAFA0u};
    FlockDbExtras ex_field = {.ie_fps = wiilover_card, .ie_fp_count = 2};
    flock_db_set_extras(&ex_field);
    CHECK_INT_EQ(flock_ie_fp_match(0x89C3DEBFu), FlockIeFpNone); // goes inert, no edit needed
    CHECK_INT_EQ(flock_ie_fp_match(0xBA9FAFA0u), FlockIeFpNone);
    flock_db_set_extras(NULL);

    // The shipped candidates still match -- the guard did not blunt detection.
    CHECK_INT_EQ(flock_ie_fp_match(0x42D75CD1u), FlockIeFpCandidate);
    CHECK_INT_EQ(flock_ie_fp_match(0xD0BBEC4Cu), FlockIeFpCandidate);
    // A CANDIDATE NEVER AUTO-CONFIRMS, not even riding a Flock OUI. 0xD0BBEC4C
    // was captured on 24:B2:B9 and 70:08:94, both in flock_ouis[], so if it were
    // ever promoted to flock_ie_fps[] it would Confirm on sight -- which is
    // exactly why one reporter on one road is not enough to promote it.
    // The OUI is the load-bearing part; the last three bytes are invented
    // rather than the real unit's, which does not need publishing.
    static const uint8_t flock_oui_mac[6] = {0x24, 0xB2, 0xB9, 0x00, 0x00, 0x01};
    CHECK_INT_EQ(flock_ie_fp_confidence(0xD0BBEC4Cu, flock_oui_mac), FlockConfidenceProbeFp);
    // And a retracted one justifies nothing at all, OUI or no OUI.
    CHECK_INT_EQ(flock_ie_fp_confidence(0x89C3DEBFu, flock_oui_mac), FlockConfidenceNone);
    CHECK_INT_EQ(flock_ie_fp_confidence(0xBA9FAFA0u, flock_oui_mac), FlockConfidenceNone);
    // and a generic hash must not reach the IE-fp method either.
    CHECK_INT_EQ(flock_method_of(NULL, NULL, 'D', 0x96FCD1B2u), FlockMethodUnknown);
    CHECK_INT_EQ(flock_method_of(NULL, NULL, 'D', 0x42D75CD1u), FlockMethodIeFp);

    // --- flock_ie_fp_confidence: what a fingerprint match alone justifies ----
    // Used by the survey promotion path, which is the ONLY place a learned
    // fingerprint can fire: the companion drops anything it does not itself
    // score before it even computes the fingerprint, so a randomised-MAC camera
    // never reaches the parser at all.
    static const uint8_t flock_mac[] = {0x24, 0xb2, 0xb9, 0x11, 0x22, 0x33};
    static const uint8_t rand_mac[] = {0x06, 0xfc, 0xcb, 0x3a, 0xf8, 0x9e};
    CHECK(flock_oui_match(flock_mac)); // fixture guard: this really is a Flock OUI
    CHECK(!flock_oui_match(rand_mac));

    // Nothing matched -> None, on any address. The common case.
    CHECK_INT_EQ(flock_ie_fp_confidence(0, flock_mac), FlockConfidenceNone);
    CHECK_INT_EQ(flock_ie_fp_confidence(0xdeadbeef, flock_mac), FlockConfidenceNone);
    CHECK_INT_EQ(flock_ie_fp_confidence(0xdeadbeef, NULL), FlockConfidenceNone);

    // A generic skeleton stays None even on a Flock OUI. This is the exact
    // combination seen in the field (7c923b53 on 24:B2:B9): if the denylist did
    // not outrank everything, a promoted generic hash on a shared-silicon Flock
    // prefix would auto-Confirm a single weak frame.
    CHECK_INT_EQ(flock_ie_fp_confidence(0x7C923B53u, flock_mac), FlockConfidenceNone);

    // The shipped single-source candidate is capped at Class?, Flock OUI or not.
    CHECK_INT_EQ(flock_ie_fp_confidence(0x42D75CD1u, flock_mac), FlockConfidenceProbeFp);
    CHECK_INT_EQ(flock_ie_fp_confidence(0x42D75CD1u, rand_mac), FlockConfidenceProbeFp);
    CHECK_INT_EQ(flock_ie_fp_confidence(0x42D75CD1u, NULL), FlockConfidenceProbeFp);

    // A user / learned fingerprint is also capped at Class?, never Confirmed.
    // This is the path that makes a taught camera detectable again: it scores
    // above None, so it reaches the hit table instead of being discarded.
    // NOT 0x89C3DEBF, which this used to use: that hash is now on the generic
    // denylist (we retracted it), so it would prove the opposite of the point.
    static const uint32_t learned[] = {0x5eed1234u};
    FlockDbExtras ex_learned = {.ie_fps = learned, .ie_fp_count = 1};
    flock_db_set_extras(&ex_learned);
    CHECK_INT_EQ(flock_ie_fp_confidence(0x5eed1234u, rand_mac), FlockConfidenceProbeFp);
    CHECK_INT_EQ(flock_ie_fp_confidence(0x5eed1234u, flock_mac), FlockConfidenceProbeFp);
    CHECK(flock_ie_fp_confidence(0x5eed1234u, rand_mac) > FlockConfidenceNone);
    flock_db_set_extras(NULL);
    // ...and once forgotten it stops scoring, so the table is genuinely the source.
    CHECK_INT_EQ(flock_ie_fp_confidence(0x5eed1234u, rand_mac), FlockConfidenceNone);

    // --- pinned WHOLE addresses (FlockDbExtras.macs) -------------------------
    // For the camera whose randomised MAC turns out to be STABLE: invented, so
    // no OUI table can match it, but unchanged between visits, so the address
    // identifies the unit. An `ouis` entry cannot express that.
    static const uint8_t pinned[][6] = {
        {0x06, 0xfc, 0xcb, 0x3a, 0xf8, 0x9e},
    };
    const uint8_t same_oui_diff_dev[6] = {0x06, 0xfc, 0xcb, 0x11, 0x22, 0x33};

    CHECK(!flock_user_mac_match(rand_mac)); // nothing registered yet
    CHECK_INT_EQ(flock_mac_pin_confidence(rand_mac), FlockConfidenceNone);

    FlockDbExtras ex_pin = {.macs = pinned, .mac_count = 1};
    flock_db_set_extras(&ex_pin);
    CHECK(flock_user_mac_match(rand_mac));
    CHECK_INT_EQ(flock_mac_pin_confidence(rand_mac), FlockConfidenceProbeFp);
    // WHOLE address, not a prefix. Another device that randomised into the same
    // first three bytes must NOT match -- that is the entire reason `ouis` was
    // the wrong tool for this.
    CHECK(!flock_user_mac_match(same_oui_diff_dev));
    CHECK_INT_EQ(flock_mac_pin_confidence(same_oui_diff_dev), FlockConfidenceNone);
    CHECK(!flock_user_mac_match(NULL));
    // Capped at Class?, never Confirmed, even on a Flock OUI.
    CHECK(flock_mac_pin_confidence(rand_mac) < FlockConfidenceConfirmed);
    // It names the method, so the detail screen does not claim a fingerprint.
    CHECK_INT_EQ(flock_method_of(rand_mac, NULL, 'P', 0), FlockMethodPin);
    CHECK_STR_EQ(flock_method_str(FlockMethodPin), "flagged MAC");
    flock_db_set_extras(NULL);
    CHECK(!flock_user_mac_match(rand_mac)); // unregisters cleanly
    CHECK_INT_EQ(flock_method_of(rand_mac, NULL, 'P', 0), FlockMethodUnknown);

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

    // --- vendor attribution (v0.77) ----------------------------------------
    //
    // THE CONTRACT THIS SUITE EXISTS TO PROTECT: the app must never print the
    // word "Flock" over hardware that is not Flock's. Before the vendor field
    // every ALPR-class detection rendered as "Flock / ALPR camera", so an Axon
    // Outpost, a Ubicquia streetlight node and a MAC in no table at all were all
    // announced as Flock Safety cameras. These rows are the guard.
    {
        const uint8_t ubicquia[6] = {0x94, 0x7b, 0xbe, 0x11, 0x22, 0x33};
        const uint8_t motorola[6] = {0x00, 0x04, 0x7d, 0x11, 0x22, 0x33};
        const uint8_t moto_my[6] = {0x9c, 0x86, 0x2b, 0x11, 0x22, 0x33};
        const uint8_t verkada[6] = {0xe0, 0xa7, 0x00, 0x11, 0x22, 0x33};
        const uint8_t genetec[6] = {0x0c, 0xbf, 0x15, 0x11, 0x22, 0x33};
        const uint8_t genetec2[6] = {0x00, 0xbf, 0x15, 0x11, 0x22, 0x33};
        const uint8_t avigilon[6] = {0x70, 0x1a, 0xd5, 0x11, 0x22, 0x33};
        const uint8_t flock[6] = {0xb4, 0x1e, 0x52, 0x11, 0x22, 0x33};
        const uint8_t shotspot[6] = {0xd4, 0x11, 0xd6, 0x11, 0x22, 0x33};
        const uint8_t axon[6] = {0x00, 0x25, 0xdf, 0x11, 0x22, 0x33};
        const uint8_t nobody[6] = {0x02, 0x00, 0x00, 0x11, 0x22, 0x33};

        // Each vendor-exclusive prefix resolves to its own vendor...
        CHECK_INT_EQ(flock_vendor_from_mac(ubicquia), FlockVendorUbicquia);
        CHECK_INT_EQ(flock_vendor_from_mac(motorola), FlockVendorMotorola);
        CHECK_INT_EQ(flock_vendor_from_mac(moto_my), FlockVendorMotorola);
        CHECK_INT_EQ(flock_vendor_from_mac(verkada), FlockVendorVerkada);
        CHECK_INT_EQ(flock_vendor_from_mac(genetec), FlockVendorGenetec);
        CHECK_INT_EQ(flock_vendor_from_mac(genetec2), FlockVendorGenetec);
        CHECK_INT_EQ(flock_vendor_from_mac(avigilon), FlockVendorAvigilon);
        CHECK_INT_EQ(flock_vendor_from_mac(flock), FlockVendorFlock);
        CHECK_INT_EQ(flock_vendor_from_mac(shotspot), FlockVendorSoundThinking);
        CHECK_INT_EQ(flock_vendor_from_mac(axon), FlockVendorAxon);
        // ...and an unlisted MAC names NOBODY. "Unknown" is the honest answer,
        // not a fallback to the historical default of Flock.
        CHECK_INT_EQ(flock_vendor_from_mac(nobody), FlockVendorUnknown);
        CHECK_INT_EQ(flock_vendor_from_mac(NULL), FlockVendorUnknown);

        // ...and to the "vendor known, kind unknown" class, NOT to ALPR. These
        // OUIs carry plate readers, building cameras and hand-held radios alike;
        // calling any of them ALPR would invent a detection.
        CHECK_INT_EQ(flock_class_from_mac(ubicquia), FlockClassGear);
        CHECK_INT_EQ(flock_class_from_mac(motorola), FlockClassGear);
        CHECK_INT_EQ(flock_class_from_mac(verkada), FlockClassGear);
        CHECK_INT_EQ(flock_class_from_mac(genetec), FlockClassGear);
        CHECK_INT_EQ(flock_class_from_mac(avigilon), FlockClassGear);
        // The three original tables keep the classes they shipped with.
        CHECK_INT_EQ(flock_class_from_mac(flock), FlockClassAlpr);
        CHECK_INT_EQ(flock_class_from_mac(shotspot), FlockClassAcoustic);
        CHECK_INT_EQ(flock_class_from_mac(axon), FlockClassBodycam);
        CHECK_INT_EQ(flock_class_from_mac(nobody), FlockClassAlpr);

        // vendor_exclusive_oui_match() is the ESP's conf=1 gate. It must cover
        // the five competitor tables and NOTHING else -- widening it to the Flock
        // tables would resurrect bare-OUI scoring on shared silicon ranges, the
        // rule that reported a T-Mobile gateway as a possible camera.
        CHECK(vendor_exclusive_oui_match(ubicquia));
        CHECK(vendor_exclusive_oui_match(motorola));
        CHECK(vendor_exclusive_oui_match(verkada));
        CHECK(vendor_exclusive_oui_match(genetec));
        CHECK(vendor_exclusive_oui_match(avigilon));
        CHECK(!vendor_exclusive_oui_match(flock));
        CHECK(!vendor_exclusive_oui_match(shotspot));
        CHECK(!vendor_exclusive_oui_match(axon));
        CHECK(!vendor_exclusive_oui_match(nobody));
        CHECK(!vendor_exclusive_oui_match(NULL));

        // A vendor-exclusive prefix must report OUI as its method. Before the
        // new tables were wired into flock_method_of() these fell through to
        // "ESP probe rule", hiding the one indicator we actually hold.
        CHECK_INT_EQ(flock_method_of(ubicquia, "", 'B', 0), FlockMethodOui);
        CHECK_INT_EQ(flock_method_of(motorola, "", 'B', 0), FlockMethodOui);
        CHECK_INT_EQ(flock_method_of(verkada, "", 'B', 0), FlockMethodOui);

        // SSID evidence outranks the MAC: a Flock provisioning name identifies
        // Flock even from a randomized or unlisted address.
        CHECK_INT_EQ(flock_vendor_of(nobody, "Flock-A1B2C3"), FlockVendorFlock);
        CHECK_INT_EQ(flock_vendor_of(NULL, "test_flck"), FlockVendorFlock);
        // But a benign name that merely contains "flock" is only Likely, and
        // that is still Flock-attributable -- it is the CONFIDENCE that stays
        // low, not the vendor. Guarding the split explicitly so nobody "fixes"
        // one by weakening the other.
        CHECK_INT_EQ(flock_vendor_of(nobody, "Flock-Guest"), FlockVendorFlock);
        CHECK_INT_EQ(flock_ssid_confidence("Flock-Guest"), FlockConfidenceLikely);
        // No Flock tell anywhere -> the MAC decides, and it names nobody.
        CHECK_INT_EQ(flock_vendor_of(nobody, "linksys"), FlockVendorUnknown);
        CHECK_INT_EQ(flock_vendor_of(ubicquia, "linksys"), FlockVendorUbicquia);

        // --- THE REGRESSION ------------------------------------------------
        // Not one label for a non-Flock vendor may contain the string "Flock".
        // Asserted by search rather than by exact match so that rewording a
        // label cannot quietly reintroduce the over-claim.
        CHECK(strstr(flock_device_long_str(FlockVendorUbicquia, FlockClassGear), "Flock") == NULL);
        CHECK(strstr(flock_device_long_str(FlockVendorMotorola, FlockClassGear), "Flock") == NULL);
        CHECK(strstr(flock_device_long_str(FlockVendorVerkada, FlockClassGear), "Flock") == NULL);
        CHECK(strstr(flock_device_long_str(FlockVendorGenetec, FlockClassGear), "Flock") == NULL);
        CHECK(strstr(flock_device_long_str(FlockVendorAvigilon, FlockClassGear), "Flock") == NULL);
        CHECK(strstr(flock_device_long_str(FlockVendorAxon, FlockClassBodycam), "Flock") == NULL);
        CHECK(
            strstr(flock_device_long_str(FlockVendorSoundThinking, FlockClassAcoustic), "Flock") ==
            NULL);
        // The unattributed case is the one that actually shipped wrong: an ALPR
        // class with no vendor evidence used to render "Flock / ALPR camera".
        CHECK(strstr(flock_device_long_str(FlockVendorUnknown, FlockClassAlpr), "Flock") == NULL);
        CHECK(strstr(flock_device_long_str(FlockVendorUnknown, FlockClassGear), "Flock") == NULL);
        CHECK(
            strstr(flock_device_long_str(FlockVendorUnknown, FlockClassAcoustic), "Flock") ==
            NULL);
        CHECK(
            strstr(flock_device_long_str(FlockVendorUnknown, FlockClassBodycam), "Flock") == NULL);

        // Conversely, real Flock evidence MUST still say Flock -- the fix must
        // not have been achieved by deleting the attribution everywhere.
        CHECK(strstr(flock_device_long_str(FlockVendorFlock, FlockClassAlpr), "Flock") != NULL);
        CHECK(
            strstr(flock_device_long_str(FlockVendorFlock, FlockClassAcoustic), "Flock") != NULL);

        // Axon's label must no longer promise the device MOVES. Axon shipped
        // Outpost and Lightpost -- fixed pole ALPR -- on this same OUI, so the
        // old "body/in-car kit" wording is now wrong in the other direction.
        CHECK(strstr(flock_device_long_str(FlockVendorAxon, FlockClassBodycam), "body") != NULL);
        CHECK(strstr(flock_device_long_str(FlockVendorAxon, FlockClassBodycam), "fixed") != NULL);

        // WIDTH BUDGET. The detail screen's 128 px row cuts at ~20 characters,
        // and device identity is the last field that may ever be truncated --
        // "SoundThinking (acoustic sensor)" was shortened for exactly this.
        // Every vendor, every class, so a future vendor cannot land untested.
        for(int v = FlockVendorUnknown; v <= FlockVendorAvigilon; v++) {
            for(int c = FlockClassAlpr; c <= FlockClassGear; c++) {
                CHECK(strlen(flock_device_long_str((FlockVendor)v, (FlockDevClass)c)) <= 20);
            }
            // The short label shares a narrow report column and a list row.
            CHECK(strlen(flock_vendor_str((FlockVendor)v)) <= 13);
        }
        // An unattributed detection reads as a dash, never as the word "Unknown"
        // -- a dash says "not attributed" without implying a failed lookup.
        CHECK_STR_EQ(flock_vendor_str(FlockVendorUnknown), "-");

        // --- MISATTRIBUTION DENYLIST ---------------------------------------
        // Look-alike prefixes belonging to OTHER companies, which a substring
        // search of the IEEE registry hands you right next to the real ones.
        // tools/check_oui_parity.py blocks these at CI; this asserts the shipped
        // behaviour, because a table can be wrong in ways a grep is not.
        {
            // Motorola MOBILITY (a Lenovo company) -- consumer phones.
            const uint8_t moto_phone_a[6] = {0x50, 0x16, 0xf4, 1, 2, 3};
            const uint8_t moto_phone_b[6] = {0xc4, 0xa0, 0x52, 1, 2, 3};
            const uint8_t moto_phone_c[6] = {0xc8, 0x58, 0x95, 1, 2, 3};
            // GENETEC Corporation (Japan) and Netgenetech -- not Genetec Inc.
            const uint8_t genetec_jp[6] = {0x00, 0x0a, 0xb1, 1, 2, 3};
            const uint8_t netgenetech[6] = {0xd8, 0xc0, 0x68, 1, 2, 3};
            // Axon NETWORKS Inc -- an unrelated networking company.
            const uint8_t axon_net[6] = {0x00, 0x58, 0x28, 1, 2, 3};
            const uint8_t* lookalikes[] = {
                moto_phone_a, moto_phone_b, moto_phone_c, genetec_jp, netgenetech, axon_net};
            for(size_t i = 0; i < sizeof(lookalikes) / sizeof(lookalikes[0]); i++) {
                CHECK_INT_EQ(flock_vendor_from_mac(lookalikes[i]), FlockVendorUnknown);
                CHECK(!vendor_exclusive_oui_match(lookalikes[i]));
                CHECK(!flock_oui_match(lookalikes[i]));
            }
        }
    }

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
