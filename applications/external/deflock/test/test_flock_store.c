// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt
//
// Tests for the persisted-hit record format (issue #2). The round trip is the
// contract that matters: whatever is written on one run must come back
// bit-for-bit on the next, and anything malformed must be REJECTED rather than
// half-parsed into a plausible-looking wrong detection.
#include "flock_store.h"
#include "test.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static FlockStoreRec sample(void) {
    FlockStoreRec r;
    memset(&r, 0, sizeof(r));
    const uint8_t mac[6] = {0xE0, 0x0A, 0xF6, 0x12, 0x34, 0xAB};
    memcpy(r.mac, mac, 6);
    snprintf(r.ssid, sizeof(r.ssid), "Flock-A1B2C3");
    r.rssi = -67;
    r.channel = 11;
    r.ftype = 'P';
    r.conf = 4;
    r.ie_fp = 0xdeadbeefu;
    r.lat = 37.774900f;
    r.lon = -122.419400f;
    r.heading = 91.5f;
    r.count = 42;
    r.marked = true;
    r.epoch = 1785000000u;
    return r;
}

/** Round-trip a record and assert every field survived. */
static void check_roundtrip(const FlockStoreRec* in) {
    char line[FLOCK_STORE_LINE_MAX];
    size_t n = flock_store_fmt_line(line, sizeof(line), in);
    CHECK(n > 0);

    FlockStoreRec out;
    memset(&out, 0xAA, sizeof(out)); // poison, so "untouched" is visible
    CHECK(flock_store_parse_line(line, &out));

    CHECK(memcmp(out.mac, in->mac, 6) == 0);
    CHECK_STR_EQ(out.ssid, in->ssid);
    CHECK_INT_EQ(out.rssi, in->rssi);
    CHECK_INT_EQ(out.channel, in->channel);
    CHECK_INT_EQ(out.ftype, in->ftype);
    CHECK_INT_EQ(out.conf, in->conf);
    CHECK_INT_EQ(out.ie_fp, in->ie_fp);
    CHECK_INT_EQ(out.count, in->count);
    CHECK_INT_EQ(out.marked, in->marked);
    CHECK_INT_EQ(out.epoch, in->epoch);
    CHECK_INT_EQ(out.dev_class, in->dev_class);
    CHECK_INT_EQ(out.hidden, in->hidden);

    // Coordinates are written at 6 dp, so compare within that.
    if(isnan(in->lat)) {
        CHECK(isnan(out.lat));
    } else {
        CHECK(fabsf(out.lat - in->lat) < 1e-5f);
    }
    if(isnan(in->lon)) {
        CHECK(isnan(out.lon));
    } else {
        CHECK(fabsf(out.lon - in->lon) < 1e-5f);
    }
    if(isnan(in->heading)) {
        CHECK(isnan(out.heading));
    } else {
        CHECK(fabsf(out.heading - in->heading) < 1e-5f);
    }
}

void suite_flock_store(void) {
    printf("[flock_store]\n");

    // --- the ordinary case ---------------------------------------------------
    FlockStoreRec r = sample();
    check_roundtrip(&r);

    // The written line is one record terminated by a newline, MAC uppercase.
    char line[FLOCK_STORE_LINE_MAX];
    size_t n = flock_store_fmt_line(line, sizeof(line), &r);
    CHECK(n > 0 && line[n - 1] == '\n');
    CHECK_STR_CONTAINS(line, "E0:0A:F6:12:34:AB");
    CHECK_STR_CONTAINS(line, "deadbeef"); // ie_fp stays hex, as shown on-screen

    // --- SSIDs that would corrupt a naive CSV --------------------------------
    // A comma in an SSID would shift every later column by one if unquoted; a
    // quote would break the quoting itself. Both must survive verbatim.
    snprintf(r.ssid, sizeof(r.ssid), "Cam,ALPR");
    check_roundtrip(&r);
    snprintf(r.ssid, sizeof(r.ssid), "say \"cheese\"");
    check_roundtrip(&r);
    snprintf(r.ssid, sizeof(r.ssid), "a,b\"c,\"\"d");
    check_roundtrip(&r);
    r.ssid[0] = '\0'; // hidden network
    check_roundtrip(&r);

    // An SSID is arbitrary bytes. A CR/LF inside one would split a record across
    // two physical lines and neither half would parse, so control characters are
    // flattened to spaces first -- lossy BY DESIGN. Assert the record stays on
    // one line and every later column survives.
    {
        FlockStoreRec nl = sample();
        snprintf(nl.ssid, sizeof(nl.ssid), "two\nlines\twide");
        char l2[FLOCK_STORE_LINE_MAX];
        size_t n2 = flock_store_fmt_line(l2, sizeof(l2), &nl);
        CHECK(n2 > 0);
        CHECK(strchr(l2, '\n') == l2 + n2 - 1); // exactly one newline: the terminator
        FlockStoreRec back;
        CHECK(flock_store_parse_line(l2, &back));
        CHECK_STR_EQ(back.ssid, "two lines wide");
        CHECK_INT_EQ(back.epoch, nl.epoch); // last column still intact
    }

    // --- no GPS fix: NAN out, empty field, NAN back --------------------------
    r = sample();
    r.lat = NAN;
    r.lon = NAN;
    r.heading = NAN;
    check_roundtrip(&r);
    n = flock_store_fmt_line(line, sizeof(line), &r);
    CHECK_STR_CONTAINS(line, ",,,"); // three empty coordinate columns in a row

    // A real 0,0 is NOT the same as "no fix" and must not collapse to one.
    r.lat = 0.0f;
    r.lon = 0.0f;
    r.heading = 0.0f;
    check_roundtrip(&r);

    // --- edge values ---------------------------------------------------------
    r = sample();
    r.rssi = -128;
    r.channel = 255;
    r.conf = 0;
    r.ie_fp = 0;
    r.count = 4294967295u;
    r.marked = false;
    r.ftype = 0; // unknown source
    check_roundtrip(&r);

    // --- malformed input is rejected, and *out is left untouched -------------
    {
        FlockStoreRec guard;
        memset(&guard, 0x5A, sizeof(guard));
        FlockStoreRec before = guard;

        CHECK(!flock_store_parse_line("", &guard));
        CHECK(!flock_store_parse_line("\n", &guard));
        CHECK(!flock_store_parse_line("not,enough,columns", &guard));
        // 12 columns (heading dropped) -- a silently shifted record is exactly
        // the failure mode a column count is there to prevent.
        CHECK(!flock_store_parse_line(
            "E0:0A:F6:12:34:AB,x,-67,11,P,4,deadbeef,37.7,-122.4,42,1,1785000000", &guard));
        // 14 columns
        CHECK(!flock_store_parse_line(
            "E0:0A:F6:12:34:AB,x,-67,11,P,4,deadbeef,37.7,-122.4,91.5,42,1,1785000000,extra",
            &guard));
        // bad MACs
        CHECK(!flock_store_parse_line(
            "E0:0A:F6:12:34,x,-67,11,P,4,deadbeef,,,,42,1,1785000000", &guard));
        CHECK(!flock_store_parse_line(
            "ZZ:0A:F6:12:34:AB,x,-67,11,P,4,deadbeef,,,,42,1,1785000000", &guard));
        // unterminated quote
        CHECK(!flock_store_parse_line(
            "E0:0A:F6:12:34:AB,\"oops,-67,11,P,4,deadbeef,,,,42,1,1785000000", &guard));
        // out-of-range confidence rung / marked flag / non-numeric count
        CHECK(!flock_store_parse_line(
            "E0:0A:F6:12:34:AB,x,-67,11,P,9,deadbeef,,,,42,1,1785000000", &guard));
        CHECK(!flock_store_parse_line(
            "E0:0A:F6:12:34:AB,x,-67,11,P,4,deadbeef,,,,42,7,1785000000", &guard));
        CHECK(!flock_store_parse_line(
            "E0:0A:F6:12:34:AB,x,-67,11,P,4,deadbeef,,,,many,1,1785000000", &guard));
        // an ftype outside the known set
        CHECK(!flock_store_parse_line(
            "E0:0A:F6:12:34:AB,x,-67,11,Z,4,deadbeef,,,,42,1,1785000000", &guard));

        CHECK(memcmp(&guard, &before, sizeof(guard)) == 0); // never partially written
    }

    // A schema/comment line must not parse as a record.
    {
        FlockStoreRec junk;
        CHECK(!flock_store_parse_line(FLOCK_STORE_SCHEMA, &junk));
        CHECK(!flock_store_parse_line(FLOCK_STORE_HEADER, &junk));
    }

    // ---- v2 `class` + `hidden` columns, and the v1 file that predates them ---
    // Both must survive the round trip like any other field...
    {
        FlockStoreRec acoustic = sample();
        acoustic.dev_class = 1; // FlockClassAcoustic
        acoustic.hidden = true;
        check_roundtrip(&acoustic);

        char line[FLOCK_STORE_LINE_MAX];
        CHECK(flock_store_fmt_line(line, sizeof(line), &acoustic) > 0);
        CHECK_STR_CONTAINS(line, ",1785000000,1,1\n"); // epoch,class,hidden

        FlockStoreRec alpr = sample();
        check_roundtrip(&alpr); // both default to 0
        CHECK(flock_store_fmt_line(line, sizeof(line), &alpr) > 0);
        CHECK_STR_CONTAINS(line, ",1785000000,0,0\n");
    }

    // ...and a v1 record (13 columns, neither field) must still load with both
    // defaulted rather than being rejected -- otherwise upgrading bins the
    // user's whole saved history.
    {
        FlockStoreRec v1;
        memset(&v1, 0xAA, sizeof(v1));
        CHECK(flock_store_parse_line(
            "E0:0A:F6:12:34:AB,Flock-A1B2C3,-67,11,P,4,deadbeef,,,,42,1,1785000000", &v1));
        CHECK_INT_EQ(v1.dev_class, 0); // FlockClassAlpr
        CHECK_INT_EQ(v1.hidden, false); // "not observed hiding", not "broadcasts"
        CHECK_INT_EQ(v1.conf, 4);
        CHECK_INT_EQ(v1.epoch, 1785000000u);
        CHECK(isnan(v1.lat)); // empty coord fields still mean "no fix", not 0,0
    }

    // Column counts either side of the two legal shapes are malformed, not a
    // version to guess at.
    {
        FlockStoreRec junk;
        // 12 columns (v1 minus epoch)
        CHECK(!flock_store_parse_line("E0:0A:F6:12:34:AB,x,-67,11,P,4,deadbeef,,,,42,1", &junk));
        // 14 columns: a half-v2 line, class but no hidden
        CHECK(!flock_store_parse_line(
            "E0:0A:F6:12:34:AB,x,-67,11,P,4,deadbeef,,,,42,1,1785000000,0", &junk));
        // 16 columns (v2 plus one)
        CHECK(!flock_store_parse_line(
            "E0:0A:F6:12:34:AB,x,-67,11,P,4,deadbeef,,,,42,1,1785000000,0,0,9", &junk));
        // class outside the known enum
        CHECK(!flock_store_parse_line(
            "E0:0A:F6:12:34:AB,x,-67,11,P,4,deadbeef,,,,42,1,1785000000,7,0", &junk));
        // hidden outside 0/1
        CHECK(!flock_store_parse_line(
            "E0:0A:F6:12:34:AB,x,-67,11,P,4,deadbeef,,,,42,1,1785000000,0,5", &junk));
    }

    // Schema gate: this build reads v1 and v2 and nothing else. A NEWER marker
    // must be refused -- a future format may reorder columns, and guessing at it
    // is how you get a plausible-looking wrong detection.
    {
        CHECK(flock_store_schema_supported(FLOCK_STORE_SCHEMA));
        CHECK(flock_store_schema_supported(FLOCK_STORE_SCHEMA_V1));
        CHECK(!flock_store_schema_supported("# FlipDeFlock hits v3"));
        CHECK(!flock_store_schema_supported("# FlipDeFlock hits"));
        CHECK(!flock_store_schema_supported(""));
        CHECK(!flock_store_schema_supported(NULL));
    }

    // Truncation: too small an output buffer yields 0, not a half-written line.
    {
        char tiny[16];
        FlockStoreRec big = sample();
        CHECK_INT_EQ(flock_store_fmt_line(tiny, sizeof(tiny), &big), 0);
        CHECK_STR_EQ(tiny, "");
    }

    // CRLF from an SD card edited on a PC still parses.
    {
        FlockStoreRec src = sample();
        char l[FLOCK_STORE_LINE_MAX];
        size_t ln = flock_store_fmt_line(l, sizeof(l), &src);
        CHECK(ln > 0);
        char crlf[FLOCK_STORE_LINE_MAX + 2];
        memcpy(crlf, l, ln - 1); // drop the LF
        crlf[ln - 1] = '\r';
        crlf[ln] = '\n';
        crlf[ln + 1] = '\0';
        FlockStoreRec back;
        CHECK(flock_store_parse_line(crlf, &back));
        CHECK_INT_EQ(back.epoch, src.epoch);
    }

    // --- eviction ordering ---------------------------------------------------
    // Weaker evidence goes first, regardless of age...
    CHECK(flock_store_evict_better(1, 9000, 4, 1000)); // Possible beats a newer Confirmed
    CHECK(!flock_store_evict_better(4, 1000, 1, 9000));
    // ...and at the same rung, the older sighting goes.
    CHECK(flock_store_evict_better(4, 1000, 4, 2000));
    CHECK(!flock_store_evict_better(4, 2000, 4, 1000));
    CHECK(!flock_store_evict_better(4, 1000, 4, 1000)); // identical -> keep the incumbent
}
