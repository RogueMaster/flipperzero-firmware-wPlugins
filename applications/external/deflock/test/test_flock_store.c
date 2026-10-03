// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt
//
// Tests for the persisted-hit record format (issue #2). The round trip is the
// contract that matters: whatever is written on one run must come back
// bit-for-bit on the next, and anything malformed must be REJECTED rather than
// half-parsed into a plausible-looking wrong detection.
#include "flock_store.h"
#include "flock_db.h" // FlockClassDrone -- the ceiling below must track the enum
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
    // NAN, matching the app: a non-aircraft row has no operator position, and
    // memset's 0 would both write "0.000000" and read back as a real fix.
    r.op_lat = NAN;
    r.op_lon = NAN;
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

    // --- a discredited fingerprint is dropped on the way in ------------------
    // The denylist landed after cards were already in the field carrying these,
    // and a stored hit is never rescored. Without this the hash kept displaying
    // as the reason for a detection forever, so the guard only ever protected
    // NEW sightings. The ROW survives -- it is the operator's record of a real
    // sighting -- but the discredited evidence does not.
    {
        FlockStoreRec out;
        // 96fcd1b2 is the stock ESP32 scan skeleton (see flock_ie_fps_generic[]).
        CHECK(flock_store_parse_line(
            "06:FC:CB:3A:F8:9E,,-26,6,F,3,96fcd1b2,,,,10,0,1788980501,0,0,", &out));
        CHECK_INT_EQ((int)out.ie_fp, 0); // zeroed, so nothing claims "IE fp"
        CHECK_INT_EQ((int)out.conf, 3); // the recorded rung is left alone

        // ba9fafa0 was the surviving hash here until 2026-10-02. It is now on the
        // generic denylist too, so a saved row carrying it must be zeroed on load
        // exactly like 96fcd1b2 above -- that is how a retraction reaches hits
        // already written to someone's SD card.
        CHECK(flock_store_parse_line(
            "7A:B2:1B:2C:F2:AD,,-37,8,F,3,ba9fafa0,,,,10,0,1788980501,0,0,", &out));
        CHECK_INT_EQ((int)out.ie_fp, 0);

        // An unclassified fingerprint on the same row shape is untouched.
        CHECK(flock_store_parse_line(
            "7A:B2:1B:2C:F2:AD,,-37,8,F,3,5a17c3e2,,,,10,0,1788980501,0,0,", &out));
        CHECK_INT_EQ((int)out.ie_fp, (int)0x5a17c3e2u);
    }

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

    // EVERY ftype letter MUST SURVIVE A ROUND TRIP.
    //
    // 'S' did not. It was added to the parser and to flock_method_of() while the
    // serializer's whitelist still read "PBROFL", so a community-signature
    // detection was written out with an EMPTY frame type and came back from
    // hits.csv having forgotten what it was -- its detail screen reverted to the
    // generic "ESP probe rule". Nothing broke loudly; the row still loaded.
    // Tested as a set rather than one letter, so the next letter cannot repeat it.
    {
        const char* letters = "PBROFLS";
        for(const char* c = letters; *c; c++) {
            FlockStoreRec r = sample();
            r.ftype = *c;
            char line[FLOCK_STORE_LINE_MAX];
            CHECK(flock_store_fmt_line(line, sizeof(line), &r) > 0);
            FlockStoreRec out;
            CHECK(flock_store_parse_line(line, &out));
            CHECK_INT_EQ(out.ftype, *c);
        }
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
        CHECK_STR_CONTAINS(line, ",1785000000,1,1,,,,0\n"); // epoch,class,hidden,label

        FlockStoreRec alpr = sample();
        check_roundtrip(&alpr); // both default to 0
        CHECK(flock_store_fmt_line(line, sizeof(line), &alpr) > 0);
        CHECK_STR_CONTAINS(line, ",1785000000,0,0,,,,0\n");

        // FlockClassBodycam (Axon). The bound below used to be a literal 1, so
        // adding a third class made the parser reject the whole LINE -- a stored
        // Axon sighting would not have come back mislabelled, it would not have
        // come back at all. Round-trip every class the enum can produce.
        FlockStoreRec bodycam = sample();
        bodycam.dev_class = 2; // FlockClassBodycam
        check_roundtrip(&bodycam);
        CHECK(flock_store_fmt_line(line, sizeof(line), &bodycam) > 0);
        CHECK_STR_CONTAINS(line, ",1785000000,2,0,,,,0\n");

        // Every value the enum can hold must survive; anything above the bound
        // is still rejected, so a corrupt or future-class file cannot be read as
        // a class this build would then mislabel.
        for(unsigned c = 0; c <= FLOCK_STORE_MAX_DEV_CLASS; c++) {
            FlockStoreRec r = sample();
            r.dev_class = (uint8_t)c;
            check_roundtrip(&r);
        }

        // THE CHECK THAT WOULD ACTUALLY HAVE CAUGHT IT.
        //
        // The loop above walks 0..MAX using the constant itself, so it stays
        // green when the constant falls BEHIND the enum -- which is exactly what
        // happened when FlockClassDrone (4) was added and this ceiling was left
        // at FlockClassGear (3). The parser rejects any row above the ceiling
        // outright, so a Remote ID drone was written to hits.csv correctly and
        // then silently dropped on every reload: the detection simply vanished
        // when the app restarted, and never reached an export. A self-referential
        // bound cannot detect that. Tie it to the enum.
        CHECK_INT_EQ((unsigned)FLOCK_STORE_MAX_DEV_CLASS, (unsigned)FlockClassDrone);

        // And the drone specifically, end to end through the CSV.
        {
            FlockStoreRec drone = sample();
            drone.dev_class = (uint8_t)FlockClassDrone;
            check_roundtrip(&drone);
        }
        {
            FlockStoreRec junk;
            char over[FLOCK_STORE_LINE_MAX];
            snprintf(
                over,
                sizeof(over),
                "E0:0A:F6:12:34:AB,x,-67,11,P,4,deadbeef,,,,42,1,1785000000,%u,0",
                (unsigned)(FLOCK_STORE_MAX_DEV_CLASS + 1u));
            CHECK(!flock_store_parse_line(over, &junk));
        }
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
        // 17 columns (v3 plus one). 16 is now a valid v3 line, so the
        // one-too-many case moved up by one.
        CHECK(!flock_store_parse_line(
            "E0:0A:F6:12:34:AB,x,-67,11,P,4,deadbeef,,,,42,1,1785000000,0,0,lbl,9", &junk));
        // class outside the known enum
        CHECK(!flock_store_parse_line(
            "E0:0A:F6:12:34:AB,x,-67,11,P,4,deadbeef,,,,42,1,1785000000,7,0", &junk));
        // hidden outside 0/1
        CHECK(!flock_store_parse_line(
            "E0:0A:F6:12:34:AB,x,-67,11,P,4,deadbeef,,,,42,1,1785000000,0,5", &junk));
    }

    // ---- v3: the marked bit field and the operator label -------------------
    {
        char line[FLOCK_STORE_LINE_MAX];

        // Both flags live in ONE column so an older build, which reads that
        // column with != 0, still sees a confirmed-only row as marked instead of
        // rejecting the line. That back-compatibility is the whole reason
        // `confirmed` did not get a column of its own.
        FlockStoreRec conf_only = sample();
        conf_only.marked = false;
        conf_only.confirmed = true;
        check_roundtrip(&conf_only);
        CHECK(flock_store_fmt_line(line, sizeof(line), &conf_only) > 0);
        CHECK_STR_CONTAINS(line, ",42,2,1785000000,"); // count,marked-bits,epoch

        FlockStoreRec both = sample();
        both.marked = true;
        both.confirmed = true;
        check_roundtrip(&both);
        CHECK(flock_store_fmt_line(line, sizeof(line), &both) > 0);
        CHECK_STR_CONTAINS(line, ",42,3,1785000000,");

        // A label must survive verbatim AND must never touch the SSID: the name
        // observed on the air and the name the operator gave it are separate
        // facts, and overwriting one with the other destroys evidence.
        FlockStoreRec named = sample();
        snprintf(named.label, sizeof(named.label), "pole by the school");
        check_roundtrip(&named);
        CHECK_STR_EQ(named.ssid, "Flock-A1B2C3");

        // Operator-typed, so it can carry a comma or a quote straight into the
        // middle of a record. Same escaping the SSID gets.
        FlockStoreRec nasty = sample();
        snprintf(nasty.label, sizeof(nasty.label), "a,b\"c");
        check_roundtrip(&nasty);

        // A bit outside the two defined ones is a corrupt record, not a future
        // flag to guess at.
        FlockStoreRec junk;
        CHECK(!flock_store_parse_line(
            "E0:0A:F6:12:34:AB,x,-67,11,P,4,deadbeef,,,,42,4,1785000000,0,0,", &junk));

        // A REAL v2 line (15 cols, marked=1) still loads: no label, not
        // confirmed. This is the upgrade path for every existing hits.csv.
        CHECK(flock_store_parse_line(
            "E0:0A:F6:12:34:AB,x,-67,11,P,4,deadbeef,,,,42,1,1785000000,0,0", &junk));
        CHECK(junk.marked);
        CHECK(!junk.confirmed);
        CHECK_STR_EQ(junk.label, "");
    }

    // Schema gate: this build reads v1..v4 and nothing else. A NEWER marker must
    // be refused -- a future format may reorder columns, and guessing at it is
    // how you get a plausible-looking wrong detection.
    //
    // WHEN YOU ADD A VERSION, move the "future" marker up with it. v4 was sitting
    // in this negative assert as the unreadable future while v4 was being made
    // the current format, which would have refused every file the build itself
    // writes.
    {
        CHECK(flock_store_schema_supported(FLOCK_STORE_SCHEMA));
        CHECK(flock_store_schema_supported(FLOCK_STORE_SCHEMA_V3));
        CHECK(flock_store_schema_supported(FLOCK_STORE_SCHEMA_V2));
        CHECK(flock_store_schema_supported(FLOCK_STORE_SCHEMA_V1));
        CHECK(!flock_store_schema_supported("# FlipDeFlock hits v5"));
        CHECK(!flock_store_schema_supported("# FlipDeFlock hits"));
        CHECK(!flock_store_schema_supported(""));
        CHECK(!flock_store_schema_supported(NULL));
    }

    // ---- v4: the Remote ID tail -------------------------------------------
    {
        // A drone round-trips with its operator position and aircraft type.
        FlockStoreRec drone = sample();
        drone.dev_class = (uint8_t)FlockClassDrone;
        drone.lat = 40.712800f; // the aircraft
        drone.lon = -74.006000f;
        drone.op_lat = 40.720000f; // the pilot, ~1 km away
        drone.op_lon = -74.010000f;
        drone.ua_type = 2; // multirotor
        char dl[FLOCK_STORE_LINE_MAX];
        CHECK(flock_store_fmt_line(dl, sizeof(dl), &drone) > 0);
        FlockStoreRec back;
        CHECK(flock_store_parse_line(dl, &back));
        CHECK(fabsf(back.op_lat - 40.72f) < 1e-5f);
        CHECK(fabsf(back.op_lon - (-74.01f)) < 1e-5f);
        CHECK_INT_EQ(back.ua_type, 2);
        CHECK_INT_EQ(back.dev_class, (int)FlockClassDrone);
        // The aircraft position must not be confused with the operator's.
        CHECK(fabsf(back.lat - 40.7128f) < 1e-5f);
        CHECK(back.lat != back.op_lat);

        // A v3 line (no Remote ID tail) must load with the operator position
        // NAN, never 0. 0/0 is a real place, and a zeroed field rendered
        // "Pilot lat: 0.00000" on the detail screen as though it were a fix.
        FlockStoreRec v3;
        CHECK(flock_store_parse_line(
            "E0:0A:F6:12:34:AB,x,-67,11,P,4,deadbeef,,,,42,1,1785000000,4,0,", &v3));
        CHECK(isnan(v3.op_lat));
        CHECK(isnan(v3.op_lon));
        CHECK_INT_EQ(v3.ua_type, 0);
        CHECK_INT_EQ(v3.dev_class, (int)FlockClassDrone);

        // An out-of-range aircraft type is rejected rather than mislabelled:
        // OdidUaType is a nibble.
        FlockStoreRec junk;
        CHECK(!flock_store_parse_line(
            "E0:0A:F6:12:34:AB,x,-67,11,P,4,deadbeef,,,,42,1,1785000000,4,0,,40.72,-74.01,16",
            &junk));
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
