// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt
//
// Adversarial line fixtures for the companion wire-protocol parser (R2 split:
// parse != mutate). Verifies each line type decodes to the right tagged record
// with the right fields, and that malformed/short/injection-flavoured lines are
// rejected (EspMsgIgnore) -- the whole reason the parser was made pure.
#include "esp_parser.h"
#include "flock_db.h"
#include "test.h"

#include <string.h>
#include <stdio.h>

// The parser is DESTRUCTIVE (splits fields in place) and record string fields
// point INTO the buffer, so keep the caller's buffer alive across the asserts.
static EspMsgType parse_into(char* buf, size_t bufsz, const char* s, EspMsg* out) {
    size_t n = strlen(s);
    if(n >= bufsz) n = bufsz - 1;
    memcpy(buf, s, n);
    buf[n] = '\0';
    return esp_parse_companion_line(buf, out);
}

static bool mac_eq(const uint8_t* m, const uint8_t* want) {
    return memcmp(m, want, 6) == 0;
}

void suite_esp_parser(void) {
    printf("[esp_parser]\n");
    char buf[256];
    EspMsg m;
    const uint8_t A1F6[6] = {0xa1, 0xb2, 0xc3, 0xd4, 0xe5, 0xf6};
#define P(s) parse_into(buf, sizeof(buf), (s), &m)

    // --- esp_split_fields ---------------------------------------------------
    {
        char s1[] = "a,b,c";
        char* f[4];
        CHECK_INT_EQ(esp_split_fields(s1, f, 4), 3);
        CHECK_STR_EQ(f[0], "a");
        CHECK_STR_EQ(f[1], "b");
        CHECK_STR_EQ(f[2], "c");
        char s2[] = "a,,c"; // empty middle field preserved
        CHECK_INT_EQ(esp_split_fields(s2, f, 4), 3);
        CHECK_STR_EQ(f[1], "");
        char s3[] = "abc"; // no commas -> single field
        CHECK_INT_EQ(esp_split_fields(s3, f, 4), 1);
        char s4[] = "a,b,c,d,e"; // capped at max: remainder stays in the last field
        CHECK_INT_EQ(esp_split_fields(s4, f, 3), 3);
        CHECK_STR_EQ(f[2], "c,d,e");
    }

    // --- D: Flock detection -------------------------------------------------
    // NOTE: this fixture used to assert Confirmed for "MyFlock" and so pinned the
    // over-confirm bug as expected behaviour. "MyFlock" merely contains "flock",
    // which is a Likely, and the parser now re-derives that itself.
    CHECK_INT_EQ(P("D,a1b2c3d4e5f6,-40,6,P,3,MyFlock"), EspMsgFlock);
    CHECK(mac_eq(m.u.flock.mac, A1F6));
    CHECK_INT_EQ(m.u.flock.rssi, -40);
    CHECK_INT_EQ(m.u.flock.channel, 6);
    CHECK_INT_EQ(m.u.flock.ftype, 'P');
    CHECK_INT_EQ(m.u.flock.conf, FlockConfidenceLikely);
    CHECK_STR_EQ(m.u.flock.ssid, "MyFlock");
    CHECK_INT_EQ(m.u.flock.fp, 0);

    CHECK_INT_EQ(P("D,a1b2c3d4e5f6,-50,1,B,1"), EspMsgFlock); // minimal (n=6), no ssid
    CHECK_STR_EQ(m.u.flock.ssid, "");
    CHECK_INT_EQ(m.u.flock.conf, FlockConfidencePossible);
    CHECK_INT_EQ(m.u.flock.ftype, 'B');

    CHECK_INT_EQ(P("D,a1b2c3d4e5f6,-50,1,,2,x"), EspMsgFlock); // empty type -> 'O'
    CHECK_INT_EQ(m.u.flock.ftype, 'O');
    CHECK_INT_EQ(m.u.flock.conf, FlockConfidenceLikely);

    // --- companion CONFIRMED is capped by our own SSID rule -----------------
    // The bug this pins: through v0.46 the companion substring-matched "flock-",
    // the Flipper took its conf verbatim (flock_score() has no production
    // caller), and benign names shipped to the screen as CONFIRMED. The parser
    // now re-derives any claimed Confirmed from the SSID it was handed.
    //
    // Every one of these arrives as conf=3 and must come back Likely.
    CHECK_INT_EQ(P("D,a1b2c3d4e5f6,-40,6,B,3,Flock-Guest"), EspMsgFlock);
    CHECK_INT_EQ(m.u.flock.conf, FlockConfidenceLikely);
    CHECK_INT_EQ(P("D,a1b2c3d4e5f6,-40,6,B,3,Flock-Safety-Corp"), EspMsgFlock);
    CHECK_INT_EQ(m.u.flock.conf, FlockConfidenceLikely);
    CHECK_INT_EQ(P("D,a1b2c3d4e5f6,-40,6,B,3,Flock-12345"), EspMsgFlock); // 5 hex, too short
    CHECK_INT_EQ(m.u.flock.conf, FlockConfidenceLikely);
    CHECK_INT_EQ(P("D,a1b2c3d4e5f6,-40,6,B,3,Flock-1234567"), EspMsgFlock); // 7 hex, too long
    CHECK_INT_EQ(m.u.flock.conf, FlockConfidenceLikely);
    CHECK_INT_EQ(P("D,a1b2c3d4e5f6,-40,6,B,3,Flock-GHIJKL"), EspMsgFlock); // not hex
    CHECK_INT_EQ(m.u.flock.conf, FlockConfidenceLikely);

    // ...and the cap must NOT over-correct: real Flock names still Confirm.
    CHECK_INT_EQ(P("D,a1b2c3d4e5f6,-40,6,B,3,Flock-A1B2C3"), EspMsgFlock);
    CHECK_INT_EQ(m.u.flock.conf, FlockConfidenceConfirmed);
    CHECK_INT_EQ(P("D,a1b2c3d4e5f6,-40,6,B,3,flock-a1b2c3"), EspMsgFlock); // case-insensitive
    CHECK_INT_EQ(m.u.flock.conf, FlockConfidenceConfirmed);
    CHECK_INT_EQ(P("D,a1b2c3d4e5f6,-40,6,B,3,test_flck"), EspMsgFlock);
    CHECK_INT_EQ(m.u.flock.conf, FlockConfidenceConfirmed);

    // A perfectly anchored name in a PROBE REQUEST names the network the sender
    // wants, not the sender. A phone probing for a camera's provisioning AP must
    // not become a Confirmed camera under the phone's own MAC. Beacon ('B') and
    // probe response ('R') carry the transmitter's own name and still Confirm.
    CHECK_INT_EQ(P("D,a1b2c3d4e5f6,-40,6,P,3,Flock-A1B2C3"), EspMsgFlock);
    CHECK_INT_EQ(m.u.flock.conf, FlockConfidenceLikely);
    CHECK_INT_EQ(P("D,a1b2c3d4e5f6,-40,6,P,3,test_flck"), EspMsgFlock);
    CHECK_INT_EQ(m.u.flock.conf, FlockConfidenceLikely);
    CHECK_INT_EQ(P("D,a1b2c3d4e5f6,-40,6,R,3,Flock-A1B2C3"), EspMsgFlock);
    CHECK_INT_EQ(m.u.flock.conf, FlockConfidenceConfirmed);

    // The cap only ever lowers. A companion reporting a WEAKER rung than the
    // SSID would justify is left alone -- it knows things this line does not.
    CHECK_INT_EQ(P("D,a1b2c3d4e5f6,-40,6,B,1,Flock-A1B2C3"), EspMsgFlock);
    CHECK_INT_EQ(m.u.flock.conf, FlockConfidencePossible);

    // Empty SSID + conf=3 is a corrupted line (our firmware needs len>0 to score
    // 3 at all), so there is no basis to overrule the ESP's OUI/probe reasoning.
    CHECK_INT_EQ(P("D,a1b2c3d4e5f6,-40,6,B,3,"), EspMsgFlock);
    CHECK_INT_EQ(m.u.flock.conf, FlockConfidenceConfirmed);
    CHECK_INT_EQ(P("D,a1b2c3d4e5f6,-40,6,B,3"), EspMsgFlock); // no ssid field at all
    CHECK_INT_EQ(m.u.flock.conf, FlockConfidenceConfirmed);

    CHECK_INT_EQ(P("D,zzzz,-40,6,P,3,x"), EspMsgIgnore); // bad hex mac
    CHECK_INT_EQ(P("D,a1b2,-40,6,P,3,x"), EspMsgIgnore); // mac too short (<12)
    CHECK_INT_EQ(P("D,a1b2c3d4e5f6,-40,6,P"), EspMsgIgnore); // too few fields (n<6)

    // --- combined ladder, at the boundary the product actually uses ----------
    // These assertions moved here from test_flock_db.c in v0.48, where they
    // tested flock_score() -- a function with no production caller. Pinning them
    // to esp_parse_companion_line() is the difference between testing the ladder
    // and testing a lookalike of it.
    //
    // The contract this encodes: the app RE-DERIVES a claimed Confirmed from the
    // SSID (a companion can be older than the app and over-claim), but it TRUSTS
    // the companion for every rung below Confirmed, because those depend on
    // probe behaviour and the silent receiver's OUI -- things the D-line does not
    // carry and this side genuinely cannot recompute.
    {
        // 70:c9:4e is a real entry in flock_ouis[]. Even so, the OUI alone does
        // not move the rung on this side: whatever the companion scored stands.
        CHECK_INT_EQ(P("D,70c94e010203,-40,6,B,1,homewifi"), EspMsgFlock);
        CHECK_INT_EQ(m.u.flock.conf, FlockConfidencePossible); // OUI only
        CHECK_INT_EQ(P("D,70c94e010203,-40,6,P,2,homewifi"), EspMsgFlock);
        CHECK_INT_EQ(m.u.flock.conf, FlockConfidenceLikely); // OUI + probe
        CHECK_INT_EQ(P("D,70c94e010203,-40,6,B,0,homewifi"), EspMsgFlock);
        CHECK_INT_EQ(m.u.flock.conf, FlockConfidenceNone); // nothing matched

        // A genuine provisioning-AP name survives the cap and reaches Confirmed,
        // with or without a Flock OUI behind it.
        CHECK_INT_EQ(P("D,70c94e010203,-40,6,B,3,Flock-A1B2C3"), EspMsgFlock);
        CHECK_INT_EQ(m.u.flock.conf, FlockConfidenceConfirmed);
        CHECK_INT_EQ(P("D,001122334455,-40,6,B,3,Flock-A1B2C3"), EspMsgFlock);
        CHECK_INT_EQ(m.u.flock.conf, FlockConfidenceConfirmed);

        // ...and an unknown OUI cannot manufacture a rung the companion did not
        // claim, which is what keeps an OUI-only hit off the top of the ladder.
        CHECK_INT_EQ(P("D,001122334455,-40,6,B,1,homewifi"), EspMsgFlock);
        CHECK_INT_EQ(m.u.flock.conf, FlockConfidencePossible);
    }

    // SSID that literally begins "fp=" must NOT be read as the IE-fingerprint
    // (the scan starts at f[7], after the ssid at f[6]).
    CHECK_INT_EQ(P("D,a1b2c3d4e5f6,-40,6,P,3,fp=1234"), EspMsgFlock);
    CHECK_STR_EQ(m.u.flock.ssid, "fp=1234");
    CHECK_INT_EQ(m.u.flock.fp, 0);
    // ...and since "fp=1234" carries no flock token at all, a conf=3 claim over
    // it is uncorroborated and collapses to None. Our firmware cannot emit that
    // combination, so the line is corrupt; recon_app_report_flock drops None.
    CHECK_INT_EQ(m.u.flock.conf, FlockConfidenceNone);

    // Real trailing fp=, no table match (built-in table ships empty): fp passes
    // through, confidence/type unchanged.
    CHECK_INT_EQ(P("D,a1b2c3d4e5f6,-40,6,P,2,name,fp=deadbeef"), EspMsgFlock);
    CHECK_INT_EQ((long)m.u.flock.fp, (long)0xdeadbeefu);
    CHECK_INT_EQ(m.u.flock.conf, FlockConfidenceLikely);
    CHECK_INT_EQ(m.u.flock.ftype, 'P');

    // A registered UNVERIFIED user fp upgrades to Class? (ProbeFp) + ftype 'F',
    // never Confirmed. This is the fp confidence logic, now unit-testable.
    static const uint32_t ufps[] = {0xdeadbeef};
    FlockDbExtras ex_fp = {.ie_fps = ufps, .ie_fp_count = 1};
    flock_db_set_extras(&ex_fp);
    CHECK_INT_EQ(P("D,a1b2c3d4e5f6,-40,6,P,0,,fp=deadbeef"), EspMsgFlock);
    CHECK_INT_EQ(m.u.flock.conf, FlockConfidenceProbeFp);
    CHECK_INT_EQ(m.u.flock.ftype, 'F');

    // ORDERING: the SSID cap runs BEFORE the fp upgrade, so a fingerprint can
    // still raise a rung the cap just lowered. Here conf=3 over "Flock-Guest" is
    // capped to Likely, then the user fp lifts it to Class?. If the two ran the
    // other way round the cap would silently undo a legitimate fp match.
    CHECK_INT_EQ(P("D,a1b2c3d4e5f6,-40,6,P,3,Flock-Guest,fp=deadbeef"), EspMsgFlock);
    CHECK_INT_EQ(m.u.flock.conf, FlockConfidenceProbeFp);
    CHECK_INT_EQ(m.u.flock.ftype, 'F');
    flock_db_set_extras(NULL);

    // --- CANDIDATE built-in fp (h00die's 70:C9:4E camera, single source) -----
    // Shipped in flock_ie_fps_candidate[], so it is live for everyone with NO
    // signatures.json. It must corroborate (lift to Class?) but NEVER auto-Confirm
    // on one source, even sitting on a real Flock OUI. 0x42d75cd1 is the seeded
    // value; the companion emits it lower-hex, parsed case-insensitively.
    //
    // Companion scored Likely (OUI + probe): the candidate fp lifts it to Class?.
    CHECK_INT_EQ(P("D,70c94e010203,-81,7,P,2,,fp=42d75cd1"), EspMsgFlock);
    CHECK_INT_EQ(m.u.flock.conf, FlockConfidenceProbeFp); // Class?, NOT Confirmed
    CHECK_INT_EQ(m.u.flock.ftype, 'F'); // labelled probe-fp source
    CHECK_INT_EQ((long)m.u.flock.fp, (long)0x42d75cd1u);
    // THE SAFETY PROPERTY: even a Flock OUI plus this candidate fp cannot reach
    // Confirmed. Only a VERIFIED built-in (empty today) or an SSID name does that.
    CHECK(m.u.flock.conf < FlockConfidenceConfirmed);
    // Upper-case in the field parses to the same hash and matches.
    CHECK_INT_EQ(P("D,70c94e010203,-81,7,P,1,,fp=42D75CD1"), EspMsgFlock);
    CHECK_INT_EQ(m.u.flock.conf, FlockConfidenceProbeFp);
    // A genuine SSID name still overrules to Confirmed -- the fp cap only raises a
    // floor, it never blocks a real name match.
    CHECK_INT_EQ(P("D,70c94e010203,-81,7,B,3,Flock-A1B2C3,fp=42d75cd1"), EspMsgFlock);
    CHECK_INT_EQ(m.u.flock.conf, FlockConfidenceConfirmed);

    // --- cls=: device class -------------------------------------------------
    // Absent means ALPR, which is what every line from an older companion is.
    CHECK_INT_EQ(P("D,a1b2c3d4e5f6,-40,6,P,2,name"), EspMsgFlock);
    CHECK_INT_EQ(m.u.flock.dev_class, FlockClassAlpr);

    CHECK_INT_EQ(P("D,d411d6010203,-40,6,P,2,name,cls=a"), EspMsgFlock);
    CHECK_INT_EQ(m.u.flock.dev_class, FlockClassAcoustic);

    // Order-independent, and fp= is still read when cls= precedes it: the two
    // used to share a loop that stopped at the first match.
    CHECK_INT_EQ(P("D,d411d6010203,-40,6,P,2,name,cls=a,fp=deadbeef"), EspMsgFlock);
    CHECK_INT_EQ(m.u.flock.dev_class, FlockClassAcoustic);
    CHECK_INT_EQ((long)m.u.flock.fp, (long)0xdeadbeefu);
    CHECK_INT_EQ(P("D,d411d6010203,-40,6,P,2,name,fp=deadbeef,cls=a"), EspMsgFlock);
    CHECK_INT_EQ(m.u.flock.dev_class, FlockClassAcoustic);
    CHECK_INT_EQ((long)m.u.flock.fp, (long)0xdeadbeefu);

    // An SSID that literally begins "cls=" must not be read as the class field
    // -- same trap as the fp= fixture above, same f[7] guard.
    CHECK_INT_EQ(P("D,a1b2c3d4e5f6,-40,6,P,3,cls=a"), EspMsgFlock);
    CHECK_STR_EQ(m.u.flock.ssid, "cls=a");
    CHECK_INT_EQ(m.u.flock.dev_class, FlockClassAlpr);

    // Old companion, no cls= field, but a SoundThinking OUI: the class is still
    // derived from the MAC rather than defaulting to "camera".
    CHECK_INT_EQ(P("D,d411d6010203,-40,6,P,2,name"), EspMsgFlock);
    CHECK_INT_EQ(m.u.flock.dev_class, FlockClassAcoustic);

    // pr= -- the companion's observed probe count for this transmitter. An
    // observation carried through to the operator, never a confidence input:
    // the same line with and without it must score identically.
    CHECK_INT_EQ(P("D,70c94e010203,-40,6,P,2,name,pr=7"), EspMsgFlock);
    CHECK_INT_EQ(m.u.flock.probe_rate, 7);
    CHECK_INT_EQ(m.u.flock.conf, FlockConfidenceLikely);
    CHECK_INT_EQ(P("D,70c94e010203,-40,6,P,2,name"), EspMsgFlock);
    CHECK_INT_EQ(m.u.flock.probe_rate, 0); // absent on an older companion
    CHECK_INT_EQ(m.u.flock.conf, FlockConfidenceLikely); // same rung either way
    // Order-independent, and clamped rather than wrapping.
    CHECK_INT_EQ(P("D,70c94e010203,-40,6,P,2,name,pr=999,fp=deadbeef"), EspMsgFlock);
    CHECK_INT_EQ(m.u.flock.probe_rate, 255);
    CHECK_INT_EQ((long)m.u.flock.fp, (long)0xdeadbeefu);
    // An SSID that literally begins "pr=" is an SSID, not the field.
    CHECK_INT_EQ(P("D,70c94e010203,-40,6,P,2,pr=9"), EspMsgFlock);
    CHECK_STR_EQ(m.u.flock.ssid, "pr=9");
    CHECK_INT_EQ(m.u.flock.probe_rate, 0);

    // cls=x -- Axon body-worn / in-car police equipment. A third class, and the
    // one that must never be reported as a camera on a pole.
    CHECK_INT_EQ(P("D,0025df010203,-40,6,P,2,name,cls=x"), EspMsgFlock);
    CHECK_INT_EQ(m.u.flock.dev_class, FlockClassBodycam);

    // Same MAC-derived fallback for an older companion that predates cls=x.
    CHECK_INT_EQ(P("D,0025df010203,-40,6,P,2,name"), EspMsgFlock);
    CHECK_INT_EQ(m.u.flock.dev_class, FlockClassBodycam);

    // Order-independence applies to the new letter too.
    CHECK_INT_EQ(P("D,0025df010203,-40,6,P,2,name,fp=deadbeef,cls=x"), EspMsgFlock);
    CHECK_INT_EQ(m.u.flock.dev_class, FlockClassBodycam);
    CHECK_INT_EQ((long)m.u.flock.fp, (long)0xdeadbeefu);

    // An Axon OUI is NOT a Flock OUI, so a built-in IE-fp hit on one must not be
    // upgraded to Confirmed the way a Flock OUI would be. Nothing seeds the fp
    // table today, so this pins the intent rather than a reachable path.
    CHECK_INT_EQ(P("D,0025df010203,-40,6,P,2,name,cls=x"), EspMsgFlock);
    CHECK(m.u.flock.conf <= FlockConfidenceLikely);

    // A class letter this build does not know must NOT collapse to "camera": it
    // falls back to the MAC-derived class, so a future companion naming a class
    // we predate cannot make the app claim an ALPR it never saw.
    CHECK_INT_EQ(P("D,0025df010203,-40,6,P,2,name,cls=q"), EspMsgFlock);
    CHECK_INT_EQ(m.u.flock.dev_class, FlockClassBodycam);
    CHECK_INT_EQ(P("D,d411d6010203,-40,6,P,2,name,cls=q"), EspMsgFlock);
    CHECK_INT_EQ(m.u.flock.dev_class, FlockClassAcoustic);

    // cls=g -- vendor-exclusive competitor gear (Ubicquia / Motorola Solutions /
    // Verkada / Genetec / Avigilon). Tested HERE, at the wire boundary the
    // product actually uses, rather than only against flock_db.c in isolation:
    // that is the lesson of v0.47, where a fully-tested scorer had no production
    // caller while the shipped path went unguarded.
    //
    // ON A MAC IN NO VENDOR TABLE, so the wire token is the ONLY thing that can
    // produce Gear. Written this way deliberately: the first draft of this block
    // used a Ubicquia MAC, and deleting the cls=g branch from esp_parser.c left
    // the suite fully GREEN, because the MAC-derived fallback returned Gear on
    // its own. It tested the fallback and called it coverage of the parser.
    // Verified by deleting the branch in a throwaway tree: this row goes red.
    CHECK_INT_EQ(P("D,a1b2c3d4e5f6,-40,6,P,2,name,cls=g"), EspMsgFlock);
    CHECK_INT_EQ(m.u.flock.dev_class, FlockClassGear);

    CHECK_INT_EQ(P("D,947bbe010203,-40,6,B,1,name,cls=g"), EspMsgFlock);
    CHECK_INT_EQ(m.u.flock.dev_class, FlockClassGear);
    CHECK_INT_EQ(m.u.flock.conf, FlockConfidencePossible);

    // MAC-derived fallback for a companion that predates cls=g -- the class must
    // come out of the OUI, NOT default to ALPR. Getting this wrong would put a
    // Motorola hand-held radio on the list as a plate reader.
    CHECK_INT_EQ(P("D,947bbe010203,-40,6,B,1,name"), EspMsgFlock);
    CHECK_INT_EQ(m.u.flock.dev_class, FlockClassGear);
    CHECK_INT_EQ(P("D,00047d010203,-40,6,B,1,name"), EspMsgFlock);
    CHECK_INT_EQ(m.u.flock.dev_class, FlockClassGear);
    CHECK_INT_EQ(P("D,e0a700010203,-40,6,B,1,name"), EspMsgFlock);
    CHECK_INT_EQ(m.u.flock.dev_class, FlockClassGear);
    CHECK_INT_EQ(P("D,0cbf15010203,-40,6,B,1,name"), EspMsgFlock);
    CHECK_INT_EQ(m.u.flock.dev_class, FlockClassGear);
    CHECK_INT_EQ(P("D,701ad5010203,-40,6,B,1,name"), EspMsgFlock);
    CHECK_INT_EQ(m.u.flock.dev_class, FlockClassGear);

    // Order-independence for the new letter, same as cls=a and cls=x.
    CHECK_INT_EQ(P("D,947bbe010203,-40,6,P,1,name,fp=deadbeef,cls=g"), EspMsgFlock);
    CHECK_INT_EQ(m.u.flock.dev_class, FlockClassGear);

    // An unknown class letter on a vendor-exclusive MAC falls back to Gear, not
    // to ALPR -- a future companion naming a class we predate must not be able
    // to make the app claim a camera it never saw.
    CHECK_INT_EQ(P("D,947bbe010203,-40,6,B,1,name,cls=q"), EspMsgFlock);
    CHECK_INT_EQ(m.u.flock.dev_class, FlockClassGear);

    // THE ATTRIBUTION CONTRACT, end to end from the wire. A competitor MAC must
    // resolve to its own vendor and must not be labelled as a Flock camera; a
    // MAC in no table at all must name nobody rather than falling back to Flock.
    CHECK_INT_EQ(P("D,947bbe010203,-40,6,B,1,name,cls=g"), EspMsgFlock);
    CHECK_INT_EQ(flock_vendor_of(m.u.flock.mac, m.u.flock.ssid), FlockVendorUbicquia);
    CHECK(
        strstr(
            flock_device_long_str(
                flock_vendor_of(m.u.flock.mac, m.u.flock.ssid),
                (FlockDevClass)m.u.flock.dev_class),
            "Flock") == NULL);

    CHECK_INT_EQ(P("D,a1b2c3d4e5f6,-40,6,P,2,name"), EspMsgFlock);
    CHECK_INT_EQ(flock_vendor_of(m.u.flock.mac, m.u.flock.ssid), FlockVendorUnknown);
    CHECK(
        strstr(
            flock_device_long_str(
                flock_vendor_of(m.u.flock.mac, m.u.flock.ssid),
                (FlockDevClass)m.u.flock.dev_class),
            "Flock") == NULL);

    // ...and a real Flock provisioning SSID still names Flock, so the fix was
    // not achieved by simply deleting the attribution everywhere.
    CHECK_INT_EQ(P("D,b41e52010203,-40,6,B,3,Flock-A1B2C3"), EspMsgFlock);
    CHECK_INT_EQ(flock_vendor_of(m.u.flock.mac, m.u.flock.ssid), FlockVendorFlock);
    CHECK(
        strstr(
            flock_device_long_str(
                flock_vendor_of(m.u.flock.mac, m.u.flock.ssid),
                (FlockDevClass)m.u.flock.dev_class),
            "Flock") != NULL);

    // An unknown trailing token is ignored, not treated as an error.
    CHECK_INT_EQ(P("D,a1b2c3d4e5f6,-40,6,P,2,name,zz=9"), EspMsgFlock);
    CHECK_INT_EQ(m.u.flock.conf, FlockConfidenceLikely);

    // --- hid=: hidden-SSID attribute ---------------------------------------
    // Absent means "not observed hiding", which is every line from an older
    // companion -- not a claim that the AP broadcasts its name.
    CHECK_INT_EQ(P("D,a1b2c3d4e5f6,-40,6,B,1,"), EspMsgFlock);
    CHECK_INT_EQ(m.u.flock.hidden, false);

    CHECK_INT_EQ(P("D,a1b2c3d4e5f6,-40,6,B,1,,hid=1"), EspMsgFlock);
    CHECK_INT_EQ(m.u.flock.hidden, true);
    // ...and it must NOT move the confidence rung. This is the whole point of
    // the attribute-not-score decision: a hidden SSID is common on consumer
    // routers, so scoring it would promote every hidden ESP32 AP in range.
    CHECK_INT_EQ(m.u.flock.conf, FlockConfidencePossible);

    // All three trailers together, in any order, all survive. This is what the
    // 8-slot field array used to break by gluing tokens onto each other.
    CHECK_INT_EQ(P("D,d411d6010203,-40,6,B,1,,fp=deadbeef,cls=a,hid=1"), EspMsgFlock);
    CHECK_INT_EQ((long)m.u.flock.fp, (long)0xdeadbeefu);
    CHECK_INT_EQ(m.u.flock.dev_class, FlockClassAcoustic);
    CHECK_INT_EQ(m.u.flock.hidden, true);
    CHECK_INT_EQ(P("D,d411d6010203,-40,6,B,1,,hid=1,cls=a,fp=deadbeef"), EspMsgFlock);
    CHECK_INT_EQ((long)m.u.flock.fp, (long)0xdeadbeefu);
    CHECK_INT_EQ(m.u.flock.dev_class, FlockClassAcoustic);
    CHECK_INT_EQ(m.u.flock.hidden, true);

    // ALL FIVE trailers the companion can emit on one probe line, in wire order
    // (fp, sg, pr, cls, hid). 7 + 5 = 12 fields; the array used to hold 10, so
    // the last two tokens were glued into "pr=7,cls=a,hid=1" and cls/hid were
    // lost -- silently, because the MAC-derived class fallback usually agreed.
    // Written on a MAC in NO vendor table so only the wire token can produce
    // the class, and with pr= present so the glue lands on cls=, not fp=.
    CHECK_INT_EQ(P("D,a1b2c3d4e5f6,-40,6,P,2,,fp=deadbeef,sg=1,pr=7,cls=a,hid=1"), EspMsgFlock);
    CHECK_INT_EQ((long)m.u.flock.fp, (long)0xdeadbeefu);
    CHECK_INT_EQ(m.u.flock.probe_rate, 7);
    CHECK_INT_EQ(m.u.flock.dev_class, FlockClassAcoustic);
    CHECK_INT_EQ(m.u.flock.hidden, true);

    // An SSID that literally begins "hid=" is an SSID, not the attribute.
    CHECK_INT_EQ(P("D,a1b2c3d4e5f6,-40,6,B,1,hid=1"), EspMsgFlock);
    CHECK_STR_EQ(m.u.flock.ssid, "hid=1");
    CHECK_INT_EQ(m.u.flock.hidden, false);

    // Anything but "1" is not an assertion of hiding.
    CHECK_INT_EQ(P("D,a1b2c3d4e5f6,-40,6,B,1,,hid=0"), EspMsgFlock);
    CHECK_INT_EQ(m.u.flock.hidden, false);

    // --- W: WiFi AP ---------------------------------------------------------
    CHECK_INT_EQ(P("W,a1b2c3d4e5f6,-55,11,3,4,4,0,HomeNet"), EspMsgWifiAp);
    CHECK(mac_eq(m.u.wifi.bssid, A1F6));
    CHECK_INT_EQ(m.u.wifi.rssi, -55);
    CHECK_INT_EQ(m.u.wifi.channel, 11);
    CHECK_INT_EQ(m.u.wifi.auth, 3);
    CHECK_INT_EQ(m.u.wifi.pairwise, 4);
    CHECK_INT_EQ(m.u.wifi.wps, 0);
    CHECK_STR_EQ(m.u.wifi.ssid, "HomeNet");

    CHECK_INT_EQ(P("W,a1b2c3d4e5f6,-55,11,3,4,4,1,Net"), EspMsgWifiAp); // wps=1
    CHECK_INT_EQ(m.u.wifi.wps, 1);
    CHECK_INT_EQ(P("W,a1b2c3d4e5f6,-55,11,3,4,4,0"), EspMsgWifiAp); // n=8, no ssid
    CHECK_STR_EQ(m.u.wifi.ssid, "");
    CHECK_INT_EQ(P("W,a1b2c3d4e5f6,-55,11,3,4,4"), EspMsgIgnore); // n=7 < 8
    CHECK_INT_EQ(P("W,xx,-55,11,3,4,4,0,Net"), EspMsgIgnore); // bad mac

    // --- BLE: device -------------------------------------------------------
    CHECK_INT_EQ(P("BLE,a1b2c3d4e5f6,-60,1,2504,Tag,09c8aabb,rv=1"), EspMsgBleDev);
    CHECK(mac_eq(m.u.ble.addr, A1F6));
    CHECK_INT_EQ(m.u.ble.rssi, -60);
    CHECK_INT_EQ(m.u.ble.cat, 1);
    CHECK_INT_EQ(m.u.ble.company, 2504);
    CHECK_STR_EQ(m.u.ble.name, "Tag");
    CHECK_INT_EQ((long)m.u.ble.mfg_len, 4);
    CHECK(
        m.u.ble.mfg[0] == 0x09 && m.u.ble.mfg[1] == 0xc8 && m.u.ble.mfg[2] == 0xaa &&
        m.u.ble.mfg[3] == 0xbb);
    CHECK_INT_EQ(m.u.ble.raven_gatt, 1);
    CHECK_INT_EQ(m.u.ble.tracker_separated, 0);

    // All three optional fields fit independently; the ninth field must not be
    // glued to rv=1 by a stale parser cap.
    CHECK_INT_EQ(P("BLE,a1b2c3d4e5f6,-60,1,2504,Tag,09c8,rv=1,sep=1"), EspMsgBleDev);
    CHECK_INT_EQ(m.u.ble.raven_gatt, 1);
    CHECK_INT_EQ(m.u.ble.tracker_separated, 1);

    // Trailers in the other order (rv=1 then mfghex) still decode correctly.
    CHECK_INT_EQ(P("BLE,a1b2c3d4e5f6,-60,1,2504,Tag,rv=1,09c8"), EspMsgBleDev);
    CHECK_INT_EQ(m.u.ble.raven_gatt, 1);
    CHECK_INT_EQ((long)m.u.ble.mfg_len, 2);
    CHECK_STR_EQ(m.u.ble.name, "Tag");

    // Apple Find My state is an optional key=value trailer. An empty name is
    // still a real field, so sep=1 must not be glued into it.
    CHECK_INT_EQ(P("BLE,a1b2c3d4e5f6,-60,2,76,,sep=1"), EspMsgBleDev);
    CHECK_STR_EQ(m.u.ble.name, "");
    CHECK_INT_EQ(m.u.ble.tracker_separated, 1);

    CHECK_INT_EQ(P("BLE,a1b2c3d4e5f6,-60,0,0"), EspMsgBleDev); // minimal (n=5), no name
    CHECK_STR_EQ(m.u.ble.name, "");
    CHECK_INT_EQ((long)m.u.ble.mfg_len, 0);
    CHECK_INT_EQ(m.u.ble.raven_gatt, 0);
    CHECK_INT_EQ(m.u.ble.tracker_separated, 0);

    CHECK_INT_EQ(P("BLE,xx,-60,1,2504,Tag"), EspMsgIgnore); // bad mac
    CHECK_INT_EQ(P("BLE,a1b2c3d4e5f6,-60,1"), EspMsgIgnore); // n=4 < 5

    // --- S: status heartbeat ------------------------------------------------
    CHECK_INT_EQ(P("S,1000,50,6,3"), EspMsgStatus);
    CHECK_INT_EQ((long)m.u.status.frames, 1000);
    CHECK_INT_EQ((long)m.u.status.hits, 50);
    CHECK_INT_EQ(m.u.status.channel, 6);
    CHECK_INT_EQ(m.u.status.have_deauths, 1);
    CHECK_INT_EQ((long)m.u.status.deauths, 3);
    CHECK_INT_EQ(P("S,1000,50,6"), EspMsgStatus); // n=4, no deauth count
    CHECK_INT_EQ(m.u.status.have_deauths, 0);
    CHECK_INT_EQ(P("S,1000,50"), EspMsgIgnore); // n=3 < 4

    // --- DA / ATK / LOC -----------------------------------------------------
    CHECK_INT_EQ(P("DA,a1b2c3d4e5f6,6"), EspMsgDeauthTarget);
    CHECK(mac_eq(m.u.deauth.bssid, A1F6));
    CHECK_INT_EQ(m.u.deauth.channel, 6);
    CHECK_INT_EQ(P("DA,xx,6"), EspMsgIgnore); // bad mac

    CHECK_INT_EQ(P("ATK,blespam,12"), EspMsgAttack);
    CHECK_STR_EQ(m.u.attack.kind, "blespam");
    CHECK_INT_EQ((long)m.u.attack.value, 12);
    CHECK_INT_EQ(P("ATK,probeflood"), EspMsgAttack); // n=2, no value
    CHECK_INT_EQ((long)m.u.attack.value, 0);

    CHECK_INT_EQ(P("LOC,-42"), EspMsgLocate);
    CHECK_INT_EQ(m.u.locate.rssi, -42);

    // --- explicit tracker actions -----------------------------------------
    CHECK_INT_EQ(P("ACT,PING,ok,-61"), EspMsgAction);
    CHECK_STR_EQ(m.u.action.op, "PING");
    CHECK_STR_EQ(m.u.action.status, "ok");
    CHECK_INT_EQ(m.u.action.have_rssi, 1);
    CHECK_INT_EQ(m.u.action.rssi, -61);
    CHECK_INT_EQ(P("ACT,RING,sent"), EspMsgAction);
    CHECK_STR_EQ(m.u.action.op, "RING");
    CHECK_STR_EQ(m.u.action.status, "sent");
    CHECK_INT_EQ(m.u.action.have_rssi, 0);
    CHECK_INT_EQ(P("ACT,,ok"), EspMsgIgnore); // empty operation is not actionable
    CHECK_INT_EQ(P("ACT,PING,"), EspMsgIgnore); // empty status is not displayable

    // --- G: NMEA relayed from a GPS on the ESP board (issue #5) -------------
    // The payload is handed through VERBATIM for gps_parser.c to decode, so the
    // only contract here is "everything from the '$' onward, unmodified".
    // Commas inside the sentence must NOT be treated as protocol field
    // separators -- that is the whole reason this is pass-through and not a
    // parsed record.
    CHECK_INT_EQ(
        P("G,$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,,*6A"), EspMsgGpsNmea);
    CHECK_STR_EQ(m.u.gps.nmea, "$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,,*6A");
    CHECK_INT_EQ(
        P("G,$GPGGA,092750.000,5321.6802,N,00630.3372,W,1,8,1.03,61.7,M,55.2,M,,*76"),
        EspMsgGpsNmea);
    CHECK_STR_EQ(
        m.u.gps.nmea, "$GPGGA,092750.000,5321.6802,N,00630.3372,W,1,8,1.03,61.7,M,55.2,M,,*76");
    // Talker prefix is not our business either (GN/GL/GA multi-constellation).
    CHECK_INT_EQ(P("G,$GNRMC,,V,,,,,,,,,,N,V*37"), EspMsgGpsNmea);
    CHECK_STR_EQ(m.u.gps.nmea, "$GNRMC,,V,,,,,,,,,,N,V*37");

    // Rejected: no payload, or a payload that cannot be a sentence. Forwarding
    // these would only push the rejection one layer deeper into the NMEA parser.
    CHECK_INT_EQ(P("G,"), EspMsgIgnore); // empty
    CHECK_INT_EQ(P("G,GPRMC,1,2"), EspMsgIgnore); // no leading '$'
    CHECK_INT_EQ(P("G"), EspMsgIgnore); // bare prefix, no comma
    // Must not shadow a real Flock detection line or any other prefix.
    CHECK_INT_EQ(P("GPS,1,2"), EspMsgIgnore);

    // --- GPSCFG: the companion's relay-state echo (issue #5) ----------------
    // The app discarded this line until v0.54, which left "relay running", "pin
    // refused" and "firmware has no relay" all rendering as the same hollow
    // searching badge. Each state has to survive the wire intact for the badge to
    // tell them apart.
    CHECK_INT_EQ(P("GPSCFG,1,16,9600"), EspMsgGpsCfg);
    CHECK_INT_EQ(m.u.gpscfg.on, 1);
    CHECK_INT_EQ(m.u.gpscfg.pin, 16);
    CHECK_INT_EQ((int)m.u.gpscfg.baud, 9600);
    // Refused pin: the companion still echoes the pin it was ASKED for, with the
    // relay off. That pairing is what tells the operator to change the pin rather
    // than reflash the board.
    CHECK_INT_EQ(P("GPSCFG,0,1,9600"), EspMsgGpsCfg);
    CHECK_INT_EQ(m.u.gpscfg.on, 0);
    CHECK_INT_EQ(m.u.gpscfg.pin, 1);
    // `gps off` reply, and the -1 "no pin" the companion reports with it.
    CHECK_INT_EQ(P("GPSCFG,0,-1,9600"), EspMsgGpsCfg);
    CHECK_INT_EQ(m.u.gpscfg.on, 0);
    CHECK_INT_EQ(m.u.gpscfg.pin, -1);
    // A high baud must not be truncated into the 16-bit pin field's range.
    CHECK_INT_EQ(P("GPSCFG,1,33,115200"), EspMsgGpsCfg);
    CHECK_INT_EQ((int)m.u.gpscfg.baud, 115200);
    CHECK_INT_EQ(m.u.gpscfg.pin, 33);
    // Malformed: too few fields. Ignored rather than guessed at -- claiming the
    // relay is OFF because a line was unreadable would send the operator hunting
    // a configuration fault that does not exist.
    CHECK_INT_EQ(P("GPSCFG,1,16"), EspMsgIgnore);
    CHECK_INT_EQ(P("GPSCFG,"), EspMsgIgnore);
    CHECK_INT_EQ(P("GPSCFG"), EspMsgIgnore);
    // Must not shadow the NMEA relay line, nor be shadowed by it.
    CHECK_INT_EQ(P("G,$GPRMC,1,2,3"), EspMsgGpsNmea);

    // --- CHIP: the board reporting its own pinout (issue #5) ----------------
    // The app used to offer a hardcoded classic-ESP32 GPS pin list on every
    // board. On an ESP32-C5 four of those pins do not exist, two are the
    // flash/PSRAM bus and one is UART0 itself -- so the picker could hand the
    // operator the very pin carrying the link, which needs a recovery flash to
    // undo. The chip is the only thing that knows, so it says.
    CHECK_INT_EQ(P("CHIP,esp32c5,29,00000000,1fff8030,1"), EspMsgChip);
    CHECK_STR_EQ(m.u.chip.target, "esp32c5");
    CHECK_INT_EQ(m.u.chip.gpio_count, 29);
    CHECK_INT_EQ(m.u.chip.has_5ghz, 1);
    // The mask must survive as 64 bits: a classic ESP32 has usable pins above 31,
    // so folding it through a 32-bit value would silently drop them.
    CHECK_INT_EQ(P("CHIP,esp32,40,000000ff,ffffff00,0"), EspMsgChip);
    CHECK_INT_EQ(m.u.chip.gpio_count, 40);
    CHECK_INT_EQ(m.u.chip.has_5ghz, 0);
    CHECK(m.u.chip.gps_pin_mask == 0x000000ffffffff00ULL);
    CHECK_INT_EQ(P("CHIP,esp32,40"), EspMsgIgnore); // truncated
    CHECK_INT_EQ(P("CHIP"), EspMsgIgnore);

    // --- BAND: the sweep actually in force ----------------------------------
    // Reports coverage that EXISTS, not coverage that was asked for: a 2.4-only
    // radio answers 2g however it was commanded.
    CHECK_INT_EQ(P("BAND,2g,13"), EspMsgBand);
    CHECK_INT_EQ(m.u.band.sel, 0);
    CHECK_INT_EQ(m.u.band.channels, 13);
    CHECK_INT_EQ(P("BAND,all,41"), EspMsgBand);
    CHECK_INT_EQ(m.u.band.sel, 2);
    CHECK_INT_EQ(m.u.band.channels, 41);
    CHECK_INT_EQ(P("BAND,5g,28"), EspMsgBand);
    CHECK_INT_EQ(m.u.band.sel, 1);
    // An unrecognised band name is not a band. Defaulting it to 2g would report
    // a 13-channel sweep that may not be what the radio is doing.
    CHECK_INT_EQ(P("BAND,6g,59"), EspMsgIgnore);
    CHECK_INT_EQ(P("BAND,2g"), EspMsgIgnore);

    // --- banners / batch markers / junk ------------------------------------
    CHECK_INT_EQ(P("FLOCKCO,1"), EspMsgBanner);
    CHECK_INT_EQ(m.u.banner.version, 1); // wire-protocol version parsed from the banner
    CHECK_INT_EQ(P("FLOCKCO,2"), EspMsgBanner);
    CHECK_INT_EQ(m.u.banner.version, 2); // a different version still parses (app flags mismatch)
    CHECK_INT_EQ(P("FLOCKCO"), EspMsgBanner);
    CHECK_INT_EQ(m.u.banner.version, 0); // no version field (old FW) -> 0
    CHECK_INT_EQ(P("WBEGIN"), EspMsgWifiBegin);
    CHECK_INT_EQ(P("WEND"), EspMsgWifiEnd); // must NOT be mistaken for "W,"
    CHECK_INT_EQ(P("BBEGIN"), EspMsgBleBegin);
    CHECK_INT_EQ(P("BEND"), EspMsgBleEnd); // must NOT be mistaken for "BLE,"
    // --- RID: Remote ID transport line --------------------------------------
    // RID,<addr12>,<rssi>,<hex>. The hex is the BLE service data starting at the
    // 0x0D application code; this layer only unhexes it, and everything about
    // what it MEANS is helpers/open_drone_id.c's problem.
    CHECK_INT_EQ(P("RID,c0fd00000007,-42,0d000212424e4348"), EspMsgRemoteId);
    {
        static const uint8_t want[6] = {0xc0, 0xfd, 0x00, 0x00, 0x00, 0x07};
        CHECK(mac_eq(m.u.rid.addr, want));
        CHECK_INT_EQ(m.u.rid.rssi, -42);
        CHECK_INT_EQ((int)m.u.rid.payload_len, 8);
        CHECK_INT_EQ(m.u.rid.payload[0], 0x0d);
        CHECK_INT_EQ(m.u.rid.payload[3], 0x12);
        CHECK_INT_EQ(m.u.rid.payload[7], 0x48);
    }

    // Malformed shapes are DROPPED, not half-decoded. A truncated advert that
    // yielded a few real bytes plus invented ones would let the decoder read
    // coordinates out of nothing, and a plotted position is believed.
    CHECK_INT_EQ(P("RID,c0fd00000007,-42,"), EspMsgIgnore); // no payload
    CHECK_INT_EQ(P("RID,c0fd00000007,-42,zz"), EspMsgIgnore); // not hex at all
    CHECK_INT_EQ(P("RID,nothex,-42,0d00"), EspMsgIgnore); // bad address
    CHECK_INT_EQ(P("RID,c0fd00000007,-42"), EspMsgIgnore); // field missing

    // An odd trailing nibble stops the walk rather than inventing the low half
    // of a byte.
    CHECK_INT_EQ(P("RID,c0fd00000007,-42,0d0002f"), EspMsgRemoteId);
    CHECK_INT_EQ((int)m.u.rid.payload_len, 3);

    // A payload longer than the buffer is clamped, never overrun. Legacy BLE
    // advertising cannot produce this; hostile input can.
    {
        char big[600];
        int o = snprintf(big, sizeof(big), "RID,c0fd00000007,-42,");
        for(int i = 0; i < 200 && o + 2 < (int)sizeof(big); i++)
            o += snprintf(big + o, 3, "0d");
        CHECK_INT_EQ(P(big), EspMsgRemoteId);
        CHECK_INT_EQ((int)m.u.rid.payload_len, 64); // sizeof(payload), not 200
    }

    // --- bwc=1: the Axon body-camera tag ------------------------------------
    // A FOURTH optional trailer on the BLE line. The split array had to grow from
    // 9 to 10 slots for it: esp_split_fields() stops at `max` and GLUES the extra
    // token onto the previous field rather than dropping it, so an unnoticed
    // short array turns the flag into part of the mfg hex and it silently never
    // fires. These asserts are what would catch that.
    CHECK_INT_EQ(P("BLE,001122334455,-50,7,845,AX3,c809aa,bwc=1"), EspMsgBleDev);
    CHECK(m.u.ble.bwc_tag);
    CHECK_INT_EQ(m.u.ble.cat, 7);

    // Present alongside every other trailer, in either order, still parses.
    CHECK_INT_EQ(P("BLE,001122334455,-50,7,845,AX3,c809aa,rv=1,sep=1,bwc=1"), EspMsgBleDev);
    CHECK(m.u.ble.bwc_tag);
    CHECK(m.u.ble.raven_gatt);
    CHECK(m.u.ble.tracker_separated);
    CHECK_INT_EQ((int)m.u.ble.mfg_len, 3); // the hex did NOT swallow a trailer

    CHECK_INT_EQ(P("BLE,001122334455,-50,7,845,AX3,bwc=1,rv=1"), EspMsgBleDev);
    CHECK(m.u.ble.bwc_tag);
    CHECK(m.u.ble.raven_gatt);

    // Absent -> false, and a lookalike token must not set it.
    CHECK_INT_EQ(P("BLE,001122334455,-50,1,2504,Penguin-1"), EspMsgBleDev);
    CHECK(!m.u.ble.bwc_tag);
    CHECK_INT_EQ(P("BLE,001122334455,-50,1,2504,Penguin-1,bwc=0"), EspMsgBleDev);
    CHECK(!m.u.ble.bwc_tag);

    // --- FLOCKCO banner: protocol version AND build version -----------------
    // Two different questions. The protocol number answers "can these two talk";
    // the build answers "which firmware is actually on the board", which nothing
    // could answer before v0.88 -- the only label was the .bin filename someone
    // typed, which cannot be verified after flashing.
    CHECK_INT_EQ(P("FLOCKCO,1,0.88"), EspMsgBanner);
    CHECK_INT_EQ(m.u.banner.version, 1);
    CHECK_STR_EQ(m.u.banner.build, "0.88");

    // BACKWARD COMPATIBILITY, both directions.
    // Older firmware sends no build -- must still report the protocol version and
    // leave the build empty rather than inventing one.
    CHECK_INT_EQ(P("FLOCKCO,1"), EspMsgBanner);
    CHECK_INT_EQ(m.u.banner.version, 1);
    CHECK_STR_EQ(m.u.banner.build, "");

    // Firmware older still sends no version field at all.
    CHECK_INT_EQ(P("FLOCKCO"), EspMsgBanner);
    CHECK_INT_EQ(m.u.banner.version, 0);
    CHECK_STR_EQ(m.u.banner.build, "");

    // An empty build field is not a build.
    CHECK_INT_EQ(P("FLOCKCO,1,"), EspMsgBanner);
    CHECK_STR_EQ(m.u.banner.build, "");

    // A future build string longer than the field is TRUNCATED, never overrun --
    // this is radio-adjacent input arriving over a UART.
    CHECK_INT_EQ(P("FLOCKCO,1,0.88-rc1-verylongsuffix"), EspMsgBanner);
    CHECK(strlen(m.u.banner.build) < sizeof(m.u.banner.build));
    CHECK_INT_EQ(m.u.banner.version, 1);

    // A DIFFERENT protocol version still parses, so the mismatch surfaces as a
    // flag rather than as a dropped line.
    CHECK_INT_EQ(P("FLOCKCO,2,0.99"), EspMsgBanner);
    CHECK_INT_EQ(m.u.banner.version, 2);
    CHECK_STR_EQ(m.u.banner.build, "0.99");

    // --- signature-table pairing + production-path self-test ---------------
    CHECK_INT_EQ(P("SIGREV," FDF_SIGNATURE_REVISION), EspMsgSigRevision);
    CHECK_STR_EQ(m.u.sigrev.revision, FDF_SIGNATURE_REVISION);
    CHECK_INT_EQ(P("SIGREV,"), EspMsgIgnore);
    CHECK_INT_EQ(P("SIGREV,96274812,extra"), EspMsgIgnore);

    // The signature itself contains commas. The parser must peel the hash and
    // result from the RIGHT, preserving the whole human-readable signature.
    CHECK_INT_EQ(
        P("SIGTEST,221:506f9a16030103,45,191,221:0050f208000000,89abcdef,1"), EspMsgSigTest);
    CHECK_STR_EQ(m.u.sigtest.signature, "221:506f9a16030103,45,191,221:0050f208000000");
    CHECK_INT_EQ((long)m.u.sigtest.hash, (long)0x89abcdefu);
    CHECK_INT_EQ(m.u.sigtest.pass, 1);
    CHECK_INT_EQ(P("SIGTEST,1,2,3,00000001,0"), EspMsgSigTest);
    CHECK_INT_EQ(m.u.sigtest.pass, 0);
    CHECK_INT_EQ(P("SIGTEST,nohash,zzzzzzzz,1"), EspMsgIgnore);
    CHECK_INT_EQ(P("SIGTEST,sig,112345678,1"), EspMsgIgnore);
    CHECK_INT_EQ(P("SIGTEST,sig,12345678,2"), EspMsgIgnore);
    CHECK_INT_EQ(P("SIGTEST,sig,12345678"), EspMsgIgnore);

    // --- v0.96 wire additions: fp2, sg=1, and the printable SV signature -----

    // OLD FIRMWARE STILL PARSES. Six-field SV is what every companion before
    // v0.96 emits; the new fields must default rather than reject the line.
    CHECK_INT_EQ(P("SV,06fccb3af89e,-26,6,89c3debf,10"), EspMsgSurvey);
    CHECK_INT_EQ((int)m.u.survey.count, 10);
    CHECK_INT_EQ((int)m.u.survey.fp2, 0);
    CHECK_STR_EQ(m.u.survey.sig, "");

    // NEW FIRMWARE: fp2 plus a signature that CONTAINS COMMAS. The signature is
    // last on the line precisely so it can, and the splitter must hand back the
    // whole remainder instead of the first comma-delimited piece of it.
    CHECK_INT_EQ(
        P("SV,06fccb3af89e,-26,6,89c3debf,10,1a2b3c4d,"
          "2,12,127,221:506f9a16030103,45,191,221:0050f208000000"),
        EspMsgSurvey);
    CHECK_INT_EQ((int)m.u.survey.fp, (int)0x89c3debfu);
    CHECK_INT_EQ((int)m.u.survey.fp2, (int)0x1a2b3c4du);
    CHECK_STR_EQ(m.u.survey.sig, "2,12,127,221:506f9a16030103,45,191,221:0050f208000000");
    CHECK_INT_EQ((int)m.u.survey.count, 10);

    // THE RANDOMISED-CAMERA CASE, which is the whole reason any of this exists.
    // conf=0 on the wire -- no OUI, no Flock SSID, nothing the ladder can score
    // -- and a locally administered address. sg=1 alone has to produce a
    // detection, or a camera that randomises stays invisible.
    CHECK_INT_EQ(P("D,06fccb3af89e,-26,6,P,0,,sg=1"), EspMsgFlock);
    CHECK_INT_EQ((int)m.u.flock.conf, (int)FlockConfidenceProbeFp);
    // 'S', so the detail screen can NAME the signature as the reason. On the
    // bench this row rendered as the generic "ESP probe rule", which tells an
    // operator nothing about why the only indicator that survives a randomised
    // address just fired.
    CHECK_INT_EQ(m.u.flock.ftype, 'S');
    CHECK_INT_EQ((int)flock_method_of(NULL, NULL, 'S', 0), (int)FlockMethodSig);
    // A REAL fingerprint still wins the label: same rung, different evidence,
    // and "IE fp" is the one the operator can go look up in signatures.json.
    CHECK_INT_EQ(P("D,06fccb3af89e,-26,6,P,0,,fp=42d75cd1,sg=1"), EspMsgFlock);
    CHECK_INT_EQ(m.u.flock.ftype, 'F');

    // ...and it is CAPPED there. A community signature is single-source, so it
    // must never reach Confirmed -- not even riding a real Flock OUI, where the
    // OUI rungs have already had their say.
    CHECK_INT_EQ(P("D,70c94e112233,-26,6,P,2,,sg=1"), EspMsgFlock);
    CHECK_INT_EQ((int)m.u.flock.conf, (int)FlockConfidenceProbeFp);

    // A stronger score from the ladder is never dragged DOWN by the cap.
    CHECK_INT_EQ(P("D,70c94e112233,-26,6,B,3,Flock-A1B2C3,sg=1"), EspMsgFlock);
    CHECK_INT_EQ((int)m.u.flock.conf, (int)FlockConfidenceConfirmed);

    // sg=0 is not a match, and an absent sg= is not a match. The parser still
    // returns a Flock message -- dropping a scoreless row is recon_app_flock_add's
    // job, not this layer's -- so what matters is that confidence stays None and
    // the row therefore never reaches the table.
    CHECK_INT_EQ(P("D,06fccb3af89e,-26,6,P,0,,sg=0"), EspMsgFlock);
    CHECK_INT_EQ((int)m.u.flock.conf, (int)FlockConfidenceNone);
    CHECK_INT_EQ(P("D,06fccb3af89e,-26,6,P,0,"), EspMsgFlock);
    CHECK_INT_EQ((int)m.u.flock.conf, (int)FlockConfidenceNone);

    // An UNKNOWN trailing key is skipped, not treated as an error -- that is the
    // forwards-compatibility contract, and it is what lets a newer companion add
    // a field without breaking this build.
    CHECK_INT_EQ(P("D,70c94e112233,-26,6,P,2,,fp=deadbeef,zz=9"), EspMsgFlock);
    CHECK_INT_EQ((int)m.u.flock.fp, (int)0xdeadbeefu);
    // Specifically: a key that merely STARTS like "fp=" must not be read as one.
    CHECK_INT_EQ(P("D,70c94e112233,-26,6,P,2,,fp2=1a2b3c4d"), EspMsgFlock);
    CHECK_INT_EQ((int)m.u.flock.fp, 0);

    // --- probe rate survives the parse instead of being dropped --------------
    //
    // pr= rode the wire from v0.88 and was parsed into the message struct and
    // then never read by anything: not stored, not shown, not scored. It is the
    // one measurement that separates a mains-powered pole phoning home from a
    // battery handheld, on a vendor prefix that sells both.
    CHECK_INT_EQ(P("D,00047d112233,-40,6,P,1,,pr=9"), EspMsgFlock);
    CHECK_INT_EQ((int)m.u.flock.probe_rate, 9);
    // Absent pr= is 0, i.e. "not measured", not "measured as zero".
    CHECK_INT_EQ(P("D,00047d112233,-40,6,P,1,"), EspMsgFlock);
    CHECK_INT_EQ((int)m.u.flock.probe_rate, 0);
    // Clamped rather than wrapped: this is radio-adjacent input over a UART.
    CHECK_INT_EQ(P("D,00047d112233,-40,6,P,1,,pr=9999"), EspMsgFlock);
    CHECK_INT_EQ((int)m.u.flock.probe_rate, 255);

    CHECK_INT_EQ(P("GARBAGE"), EspMsgIgnore);
    CHECK_INT_EQ(P(""), EspMsgIgnore);

#undef P
}
