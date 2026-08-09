// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt
//
// helpers/marauder_scan.c -- the generic/Marauder backend.
//
// Until v0.48 this was an entire detection backend with no regression test at
// all, including the precision-critical single-MAC attribution rule. It is also
// the backend that runs on a stock Marauder board, i.e. the one most users will
// actually exercise.
#include "test.h"
#include "marauder_scan.h"

#include <string.h>

void suite_marauder_scan(void);

// 70:c9:4e is a real entry in flock_ouis[]; d4:11:d6 is the SoundThinking OUI.
#define FLOCK_MAC "70:c9:4e:11:22:33"
#define ST_MAC    "d4:11:d6:11:22:33"
#define OTHER_MAC "00:11:22:33:44:55"
#define OTHER2    "aa:bb:cc:dd:ee:ff"

void suite_marauder_scan(void) {
    printf("[marauder_scan]\n");

    MarauderScan s;

    // --- SSID label extraction ----------------------------------------------
    marauder_scan_line("-40 Ch: 6 " OTHER_MAC " ESSID: HomeNetwork", &s);
    CHECK(s.have_ssid);
    CHECK_STR_EQ(s.ssid, "HomeNetwork");
    CHECK_INT_EQ(s.mac_count, 1);

    marauder_scan_line("MAC: " OTHER_MAC " CH: 6 SSID: RawName", &s);
    CHECK(s.have_ssid);
    CHECK_STR_EQ(s.ssid, "RawName");

    marauder_scan_line("Client: " OTHER_MAC " Requesting: ProbedName", &s);
    CHECK(s.have_ssid);
    CHECK_STR_EQ(s.ssid, "ProbedName");

    // Embedded spaces are preserved; trailing CR/LF terminate the name.
    marauder_scan_line("ESSID: My Home Net\r\n", &s);
    CHECK_STR_EQ(s.ssid, "My Home Net");

    // No label at all -> no SSID captured (the line is still scored, see below).
    marauder_scan_line("random chatter " OTHER_MAC, &s);
    CHECK(!s.have_ssid);
    CHECK_STR_EQ(s.ssid, "");

    // --- OUI matching --------------------------------------------------------
    // A BARE OUI IS NOT A DETECTION. These assertions used to demand the
    // opposite -- "a Flock OUI always counts, at possible, even with a benign
    // SSID" -- and that is what put a T-Mobile gateway (SSID "tmobile-5416") on
    // a user's camera list. The table is mostly shared silicon-vendor ranges
    // (Espressif, Liteon), so it describes a huge number of ordinary devices,
    // and Flock's cameras stopped acting as APs around December 2025 anyway.
    // The old fixture encoded the bug as intended behaviour, which is worse than
    // having no test at all: it defended the false positive.
    marauder_scan_line("-40 Ch: 6 " FLOCK_MAC " ESSID: HomeNetwork", &s);
    CHECK_INT_EQ(s.hit_count, 0);
    marauder_scan_line("-40 Ch: 6 " ST_MAC " ESSID: HomeNetwork", &s);
    CHECK_INT_EQ(s.hit_count, 0);

    // The exact shape of the reported false positive.
    marauder_scan_line("-40 Ch: 6 " FLOCK_MAC " ESSID: tmobile-5416", &s);
    CHECK_INT_EQ(s.hit_count, 0);

    // Corroborated by a Flock-shaped SSID on the same line, it still counts --
    // and the device class still comes from the OUI.
    marauder_scan_line("-40 Ch: 6 " FLOCK_MAC " ESSID: Flock-a1b2c3", &s);
    CHECK_INT_EQ(s.hit_count, 1);
    CHECK_INT_EQ(s.hits[0].conf, FlockConfidenceConfirmed);
    CHECK_INT_EQ(s.hits[0].dev_class, FlockClassAlpr);

    marauder_scan_line("-40 Ch: 6 " ST_MAC " ESSID: flock-test", &s);
    CHECK_INT_EQ(s.hit_count, 1);
    CHECK_INT_EQ(s.hits[0].dev_class, FlockClassAcoustic);

    // An unknown OUI on a benign line is not a detection at all.
    marauder_scan_line("-40 Ch: 6 " OTHER_MAC " ESSID: HomeNetwork", &s);
    CHECK_INT_EQ(s.hit_count, 0);

    // A confirmed SSID raises an OUI hit above "possible".
    marauder_scan_line("-40 Ch: 6 " FLOCK_MAC " ESSID: Flock-A1B2C3", &s);
    CHECK_INT_EQ(s.hit_count, 1);
    CHECK_INT_EQ(s.hits[0].conf, FlockConfidenceConfirmed);

    // --- B14 REGRESSION: the single-MAC attribution rule --------------------
    // A line-wide SSID match may be attributed to a MAC ONLY when the line names
    // exactly one. Otherwise one "flock" token would promote every unrelated MAC
    // printed beside it -- a false positive per extra device on the line.
    marauder_scan_line("Client: " OTHER_MAC " Requesting: Flock-A1B2C3", &s);
    CHECK_INT_EQ(s.mac_count, 1);
    CHECK_INT_EQ(s.hit_count, 1); // sole MAC -> attribution allowed
    CHECK_INT_EQ(s.hits[0].conf, FlockConfidenceConfirmed);

    marauder_scan_line("A " OTHER_MAC " B " OTHER2 " ESSID: Flock-A1B2C3", &s);
    CHECK_INT_EQ(s.mac_count, 2);
    CHECK_INT_EQ(s.hit_count, 0); // two MACs -> neither may claim the SSID

    // ...but a real OUI on that same multi-MAC line still counts on its own
    // evidence, and picks up the SSID confidence because it is a genuine match.
    marauder_scan_line("A " FLOCK_MAC " B " OTHER2 " ESSID: Flock-A1B2C3", &s);
    CHECK_INT_EQ(s.mac_count, 2);
    CHECK_INT_EQ(s.hit_count, 1);
    CHECK_INT_EQ(s.hits[0].conf, FlockConfidenceConfirmed);
    CHECK_INT_EQ(s.hits[0].mac[0], 0x70);

    // --- whole-line fallback: DELIBERATE, and deliberately contained ---------
    // With no label, the entire line is scored as if it were the SSID. This is
    // how a bare sniff dump still detects, but it means a stray token anywhere
    // raises confidence -- which is exactly why the single-MAC rule exists.
    marauder_scan_line("junk flock junk " OTHER_MAC, &s);
    CHECK(!s.have_ssid);
    CHECK_INT_EQ(s.hit_count, 1);
    CHECK_INT_EQ(s.hits[0].conf, FlockConfidenceLikely); // "flock" substring

    // The same token with two MACs on the line attributes to neither.
    marauder_scan_line("junk flock junk " OTHER_MAC " " OTHER2, &s);
    CHECK_INT_EQ(s.hit_count, 0);

    // test_flck (CVE-2025-59409) is near self-identifying, so it confirms even
    // from an unlabelled line -- pinned deliberately, not by accident.
    marauder_scan_line("chatter test_flck chatter " OTHER_MAC, &s);
    CHECK_INT_EQ(s.hit_count, 1);
    CHECK_INT_EQ(s.hits[0].conf, FlockConfidenceConfirmed);

    // A benign "Flock-" name must NOT confirm -- the anchored rule applies here
    // too (this is the v0.46 bug, checked on the Marauder path).
    marauder_scan_line("Client: " OTHER_MAC " Requesting: Flock-Guest", &s);
    CHECK_INT_EQ(s.hit_count, 1);
    CHECK_INT_EQ(s.hits[0].conf, FlockConfidenceLikely); // never Confirmed
    marauder_scan_line("Client: " OTHER_MAC " Requesting: Flock-12345", &s);
    CHECK_INT_EQ(s.hits[0].conf, FlockConfidenceLikely);

    // --- malformed / degenerate input ---------------------------------------
    marauder_scan_line("", &s);
    CHECK_INT_EQ(s.hit_count, 0);
    CHECK_INT_EQ(s.mac_count, 0);
    marauder_scan_line(NULL, &s); // must not crash; fully zeroed
    CHECK_INT_EQ(s.hit_count, 0);
    CHECK_INT_EQ(s.mac_count, 0);
    CHECK(!s.have_ssid);

    marauder_scan_line("short", &s); // shorter than one MAC token
    CHECK_INT_EQ(s.hit_count, 0);
    marauder_scan_line("zz:zz:zz:zz:zz:zz ESSID: Flock-A1B2C3", &s); // not hex
    CHECK_INT_EQ(s.mac_count, 0);
    CHECK_INT_EQ(s.hit_count, 0);
    marauder_scan_line("70-c9-4e-11-22-33 ESSID: x", &s); // wrong separator
    CHECK_INT_EQ(s.mac_count, 0);

    // A truncated MAC at the very end of the line must not be read past.
    marauder_scan_line("ESSID: Flock-A1B2C3 70:c9:4e:11:22", &s);
    CHECK_INT_EQ(s.mac_count, 0);

    // --- overflow is counted, not silently dropped ---------------------------
    // Ten Flock-OUI MACs on one line: MARAUDER_MAX_HITS are kept, the rest are
    // reported via `dropped` so a pathological line is visible rather than
    // looking like a clean partial result.
    char many[512];
    size_t w = 0;
    for(int i = 0; i < 10; i++) {
        w += (size_t)snprintf(many + w, sizeof(many) - w, "70:c9:4e:11:22:%02x ", i);
    }
    // Needs a Flock-shaped SSID now that a bare OUI is not a hit; the point of
    // this case is the hit-table cap, not the scoring rule.
    w += (size_t)snprintf(many + w, sizeof(many) - w, "ESSID: Flock-a1b2c3");
    marauder_scan_line(many, &s);
    CHECK_INT_EQ(s.mac_count, 10);
    CHECK_INT_EQ(s.hit_count, MARAUDER_MAX_HITS);
    CHECK_INT_EQ(s.dropped, 10 - MARAUDER_MAX_HITS);

    // --- out is fully overwritten between calls ------------------------------
    // A stale hit from a previous line must never leak into the next result.
    marauder_scan_line("-40 " FLOCK_MAC " ESSID: Flock-A1B2C3", &s);
    CHECK_INT_EQ(s.hit_count, 1);
    marauder_scan_line("nothing here", &s);
    CHECK_INT_EQ(s.hit_count, 0);
    CHECK_INT_EQ(s.dropped, 0);
    CHECK_STR_EQ(s.ssid, "");
}
