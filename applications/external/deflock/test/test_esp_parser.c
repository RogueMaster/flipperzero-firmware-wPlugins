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
    CHECK_INT_EQ(P("GARBAGE"), EspMsgIgnore);
    CHECK_INT_EQ(P(""), EspMsgIgnore);

#undef P
}
