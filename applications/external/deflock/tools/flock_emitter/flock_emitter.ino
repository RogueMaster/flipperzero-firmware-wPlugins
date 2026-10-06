// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt
/*
 * Flock Emitter - a BENCH TARGET for FlipDeFlock. NOT a detector.
 *
 * ############################################################################
 * # THIS SKETCH TRANSMITS. FlipDeFlock itself never does; this is a separate  #
 * # tool that exists only so the detector has something to detect. Run it on  #
 * # a bench, on a board you own, and turn it off afterwards. Do not run it    #
 * # where it could pollute someone else's capture or be mistaken for real     #
 * # surveillance hardware. See README.md in this folder.                      #
 * ############################################################################
 *
 * WHY THIS EXISTS. Everything in FlipDeFlock since v0.20 is compile-verified in
 * CI and has never been validated against a radio. The host tests prove the
 * scoring maths; the CI build proves it links. Neither proves that a frame on
 * the air becomes the right row on the Flipper's screen. That gap needs a
 * transmitter, and driving to a real ALPR pole to get one is a poor test loop:
 * you cannot ask a real camera to emit a Possible, then a Likely, then a
 * CONFIRMED, then a known false positive, on a 3-second rotation.
 *
 * So this board impersonates each rung in turn. Every identity below is chosen
 * to land on exactly one branch of the ladder in helpers/flock_db.c and
 * helpers/esp_parser.c, INCLUDING the branches that must NOT fire. If the
 * Flipper shows something other than the "expect" column, that is a real bug.
 *
 * ---------------------------------------------------------------------------
 * WiFi identities (beacons rotate every WIFI_ROTATE_MS; probe identities
 * hold for PROBE_HOLD_MS so they outlive a detector channel sweep)
 *
 *   # Identity                          Expect on the Flipper
 *   0 Flock OUI, beacon, named SSID      p Possible   (OUI only)
 *   1 Flock OUI, wildcard probe-req      L Likely     (OUI + probe behaviour)
 *   2 SSID "Flock-A1B2C3"                ! CONFIRMED  (anchored provisioning name)
 *   3 SSID "test_flck"                   ! CONFIRMED  (CVE-2025-59409 dev SSID)
 *   4 SSID "Flock-Guest"                 L Likely, NEVER Confirmed  <-- B6 guard
 *   5 SoundThinking OUI d4:11:d6         "ST" tag, Possible/Likely
 *   6 Flock OUI, beacon, zero-length IE  "[hid]" tag, rung unchanged
 *   7 Flock OUI, beacon, all-NUL IE      "[hid]" tag, rung unchanged
 *   8 Ubicquia OUI 94:7b:be, beacon      p Possible, vendor "Ubicquia"
 *   9 Motorola Sol. 00:04:7d, beacon     p Possible, vendor "Motorola"
 *  10 Motorola MOBILITY 50:16:f4         NOTHING -- must never appear
 *  11 Motorola Solutions, wildcard probe L Likely, class Gear (v0.97 rung)
 *  12 no-table MAC, probe-req SSID       L Likely, NEVER Confirmed (the name
 *     "Flock-A1B2C3"                       is what the sender SEEKS, not is)
 *
 * Identities 8-10 cover the v0.77 vendor work, and 8 is the one that proves the
 * companion was reflashed: a bare named beacon from a vendor-exclusive OUI
 * scored conf=0 (i.e. was never reported) on every build before v0.77, so if it
 * shows up at all, the new rung is live. Check the detail screen reads
 * "Ubicquia streetlight" and NOT "Flock / ALPR camera" -- that label is the
 * whole point of the change.
 *
 * Identity 10 is the attribution guard. Motorola Mobility is Lenovo's consumer
 * phone business, a different company from Motorola Solutions, and a substring
 * search of the IEEE registry hands you both. If it is ever listed, a phone
 * prefix got into a table.
 *
 * Identity 4 is the important one, and it is not hypothetical: through v0.46 the
 * companion substring-matched "flock-", the Flipper took its score verbatim, and
 * Flock-Guest really did display as CONFIRMED. Both sides now anchor on
 * ^flock-[0-9a-f]{6}$ and the Flipper re-derives any claimed CONFIRMED from the
 * SSID it was sent. If the bench ever shows Flock-Guest as CONFIRMED again,
 * one of those two guards regressed.
 *
 * Identities 6 and 7 are the two legal hidden-SSID encodings, and the companion
 * detects them on DIFFERENT code paths (a zero length versus a byte scan for
 * all-NUL). The host tests cover the `hid=1` token, not the detection, so this
 * sketch is the only thing that ever exercises either branch.
 *
 * ---------------------------------------------------------------------------
 * Net Guardian test (serial-toggled, OFF by default):
 *   send "atk on" over serial  -> sustained beacon flood, Net Guardian ACTIVE
 *   send "atk off"             -> stop. Never collides with the Flock tables.
 *
 * BLE identities (rotates every BLE_ROTATE_MS)
 *
 *   0 mfg 0x09C8 + name "Penguin-1234567890"  FLOCK, serial decoded
 *   1 mfg 0x09C8 + name "FS Ext Battery"      FLOCK, model label not a serial
 *   2 Raven GATT service 0x3100            "Flock Raven (audio)"
 *   3 mfg 0x09C8, undecodable payload      CONFIRMED on the mfg id ALONE
 *   4 name "RWLS-38:5B:44:B3:0F:5A"      CONFIRMED by NAME alone
 *   5 name "FS-1A2B3C"                   CONFIRMED by NAME alone
 *   6 Remote ID (ASTM F3411)             DRONE, own class, + operator location
 *
 * Identity 6 is not a Flock device at all and must NOT come out as one: expect
 * class "Drone" / "Unmanned aircraft", serial BENCH-DRONE-01, and an OPERATOR
 * position about a kilometre from the aircraft position. It rotates Basic ID ->
 * Location -> System from a single address, exactly as a real aircraft does, so
 * it exercises the accumulate-across-adverts path rather than handing the
 * detector everything in one packet.
 *
 * 4 and 5 set name_only, so they carry NO manufacturer data and no service
 * UUID. That flag is the whole test: without it apply_ble_identity() attaches
 * the 0x09C8 company id to them and they are Confirmed before the name is
 * consulted -- which is what happened on their first run, against a companion
 * that had never heard of either name.
 *
 * ---------------------------------------------------------------------------
 * HARDWARE / LIMITS
 *
 *   - Any classic ESP32 (WROOM/WROVER). 2.4 GHz only, which is all FlipDeFlock's
 *     companion scans, so there is nothing this rig cannot reach.
 *   - Beacons are injected with esp_wifi_80211_tx() in AP mode. The ESP32 IDF
 *     refuses to inject frames whose source MAC is not one of the interface
 *     MACs UNLESS en_sys_seq is true and the frame is a valid mgmt frame; we
 *     hand-build the whole 802.11 header, so the spoofed OUIs go out as-is.
 *   - Probe requests are produced by esp_wifi_scan_start() in STA mode after
 *     setting the interface MAC, since the IDF will not inject a probe-req with
 *     a foreign address.
 *
 * BUILD
 *   arduino-cli compile --fqbn esp32:esp32:esp32 tools/flock_emitter
 *   arduino-cli upload  --fqbn esp32:esp32:esp32 -p <port> tools/flock_emitter
 */

#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>

#include <BLEDevice.h>
#include <BLEAdvertising.h>


// Rotation periods. WiFi is slower because switching identity restarts the
// driver (~600 ms); BLE only swaps an advert payload.
#define WIFI_ROTATE_MS 3000

// A PROBE IDENTITY MUST OUTLIVE A CHANNEL SWEEP, or it cannot test the thing it
// exists to test. The detector watches any one channel for 300 ms out of every
// 3900 ms, so a 3-second identity that fires a single scan is usually not on the
// air when the radio is listening -- and when it is, one scan puts 1-2 probes in
// the window, below the sustained-probe threshold that the OUI+probe rung needs.
//
// A fielded camera does not behave like that. It sits in station mode probing
// roughly every 125 ms, continuously, so it is still probing on the detector's
// next visit and the one after. That persistence IS the signal. Modelling it
// takes two things: hold the identity across several sweeps, and keep probing
// for the whole hold rather than once at the start.
//
// Without this the rig reported a clean run while never exercising the branch --
// the same shape of failure as the beacon injection flag above.
#define PROBE_HOLD_MS  12000 // ~3 detector sweeps
#define PROBE_REARM_MS 150 // re-arm the scan this often, approximating ~125 ms
#define BLE_ROTATE_MS  1500
#define BEACON_MS      120 // beacon interval while an identity is active

// Fixed channel. The companion hops 1-13 at 300 ms, so it revisits this often
// enough to catch several beacons per identity. Pick a quiet one for the bench.
#define EMIT_CHANNEL 6

// ---- WiFi identities -------------------------------------------------------

typedef enum {
    EmitBeacon = 0, /**< inject a beacon with this SSID (NULL = no SSID IE) */
    EmitProbe, /**< scan, producing probe requests from this MAC */
} EmitKind;

/**
 * How to encode the SSID information element.
 *
 * Two DIFFERENT encodings both mean "hidden network", both appear in the wild,
 * and the companion has a separate code path for each (`ssid_len == 0` versus
 * the all-NUL scan in promisc_cb). Emitting only one leaves the other branch
 * untested, so both get an identity.
 */
typedef enum {
    SsidNamed = 0, /**< tag 0, length N, the name in `ssid` */
    SsidZeroLen, /**< tag 0, length 0 */
    SsidAllNul, /**< tag 0, length N, every byte NUL */
} SsidEnc;

typedef struct {
    uint8_t mac[6];
    const char* ssid; /**< the name, for SsidNamed. Ignored otherwise. */
    SsidEnc enc;
    EmitKind kind;
    const char* expect; /**< what the Flipper should show; printed to serial */
} WifiIdentity;

// b4:1e:52 is Flock Safety's own registered OUI; d4:11:d6 is SoundThinking.
// Both are in the compiled-in tables, so these exercise the real matchers
// rather than a test-only backdoor (there isn't one, by design).
static const WifiIdentity WIFI_IDS[] = {
    {{0xb4, 0x1e, 0x52, 0x00, 0x00, 0x01},
     "bench-net-1",
     SsidNamed,
     EmitBeacon,
     "p Possible (OUI only)"},
    {{0xb4, 0x1e, 0x52, 0x00, 0x00, 0x02},
     NULL,
     SsidZeroLen,
     EmitProbe,
     "L Likely (OUI + probe)"},
    {{0x70, 0xc9, 0x4e, 0x00, 0x00, 0x03},
     "Flock-A1B2C3",
     SsidNamed,
     EmitBeacon,
     "! CONFIRMED (anchored)"},
    {{0x3c, 0x91, 0x80, 0x00, 0x00, 0x04},
     "test_flck",
     SsidNamed,
     EmitBeacon,
     "! CONFIRMED (CVE dev SSID)"},
    {{0x80, 0x30, 0x49, 0x00, 0x00, 0x05},
     "Flock-Guest",
     SsidNamed,
     EmitBeacon,
     "L Likely -- MUST NOT be CONFIRMED (B6)"},
    {{0xd4, 0x11, 0xd6, 0x00, 0x00, 0x06},
     "bench-net-2",
     SsidNamed,
     EmitBeacon,
     "ST tag, acoustic class"},
    {{0x14, 0x5a, 0xfc, 0x00, 0x00, 0x07},
     NULL,
     SsidZeroLen,
     EmitBeacon,
     "[hid] tag, rung unchanged (zero-len IE)"},
    {{0x08, 0x3a, 0x88, 0x00, 0x00, 0x08},
     NULL,
     SsidAllNul,
     EmitBeacon,
     "[hid] tag, rung unchanged (all-NUL IE)"},
    // 9-11: vendor-exclusive competitor OUIs (v0.77). A NAMED BEACON with no
    // Flock tell anywhere -- no "flock" in the SSID, no probe behaviour, an OUI
    // in none of the Flock tables. On the pre-v0.77 build this frame produced
    // NOTHING AT ALL (bare-OUI beacons score conf=0), so if the Flipper lists
    // these the new rung is live; if it does not, the companion was not
    // reflashed. That is the single most useful thing this rig can tell you
    // about this change.
    {{0x94, 0x7b, 0xbe, 0x00, 0x00, 0x09},
     "bench-street-1",
     SsidNamed,
     EmitBeacon,
     "p Possible, vendor Ubicquia -- MUST NOT say Flock"},
    {{0x00, 0x04, 0x7d, 0x00, 0x00, 0x0a},
     "bench-moto-1",
     SsidNamed,
     EmitBeacon,
     "p Possible, vendor Motorola -- MUST NOT say ALPR"},
    // 11 is the ATTRIBUTION GUARD, and the reason it is worth a whole identity:
    // Motorola MOBILITY (a Lenovo company) makes consumer phones and is a
    // different company from Motorola Solutions, but a substring search of the
    // IEEE registry returns both. If the Flipper ever lists this one, a phone
    // prefix has been let into a table and every Moto handset in range is about
    // to be reported as surveillance hardware.
    {{0x50, 0x16, 0xf4, 0x00, 0x00, 0x0b},
     "bench-phone-1",
     SsidNamed,
     EmitBeacon,
     "NOTHING -- Motorola MOBILITY, must never be listed"},
    // MOTOROLA SOLUTIONS, PROBING LIKE A POLE. The one identity that exercises
    // the vendor + sustained-wildcard-probe rung added in v0.97.
    //
    // Motorola Solutions sells ALPR poles and hand-portable radios on this one
    // prefix, so the vendor alone cannot say which is in front of you, and until
    // v0.97 a beacon and continuous wildcard probing both scored "possible".
    // This identity is the probing case: it must come out LIKELY, class Gear,
    // vendor Motorola. Identity 9 is the same OUI BEACONING and must stay
    // "possible" -- the pair is the test, neither half alone proves anything.
    {{0x00, 0x04, 0x7d, 0x00, 0x00, 0x0c},
     NULL,
     SsidZeroLen,
     EmitProbe,
     "L Likely, class Gear (vendor + sustained probe)"},
    // A PHONE LOOKING FOR A CAMERA. The SSID in a probe REQUEST names the
    // network the sender wants, not the sender: a phone that once joined a
    // camera's provisioning AP probes for "Flock-A1B2C3" from its own address.
    // Before the cap this came out CONFIRMED under the phone's MAC. The MAC is
    // in no table on purpose, so the name is the only thing that can score;
    // identity 2 is the same name in a BEACON and must still Confirm -- the
    // pair is the test.
    {{0x00, 0x11, 0x22, 0x00, 0x00, 0x0d},
     "Flock-A1B2C3",
     SsidNamed,
     EmitProbe,
     "L Likely -- name in a probe REQUEST must NOT Confirm"},
};
#define WIFI_ID_COUNT (sizeof(WIFI_IDS) / sizeof(WIFI_IDS[0]))

/** Length of the all-NUL SSID IE emitted by SsidAllNul identities. */
#define ALLNUL_SSID_LEN 6

/* Arduino-ESP32 core 2.x / 3.x compatibility -- see the fuller note in
 * esp32_companion/flock_companion/flock_companion.ino. Core 3.x takes an Arduino
 * String where 2.x took std::string. The advert payload here is BINARY (a 2-byte
 * little-endian company id, then an optional ASCII serial), so the 3.x
 * conversion MUST be length-preserving: a C-string copy would stop at the
 * company id's high NUL byte and emit a truncated, unrecognisable advert --
 * which would make this bench target silently stop testing what it claims to.
 */
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
static inline String fmfg(const std::string& s) {
    // Built byte-by-byte rather than from a C string: String::concat(char)
    // appends via memcpy and tracks length separately, so a 0x00 byte inside the
    // payload survives. String(s.c_str()) would not.
    String out;
    out.reserve(s.size());
    for(size_t i = 0; i < s.size(); i++) {
        out.concat((char)s[i]);
    }
    return out;
}
#else
static inline const std::string& fmfg(const std::string& s) {
    return s;
}
#endif

// ---- BLE identities --------------------------------------------------------

typedef struct {
    const char* name;
    const char* serial; /**< appended after the 0x09C8 company id; NULL = none */
    const char* service_uuid; /**< advertised service UUID, or NULL */
    /**
     * Advertise the GAP name and NOTHING ELSE -- no manufacturer data, no
     * service UUID. For identities whose whole purpose is to test the NAMING
     * tell.
     *
     * Without this flag the rig cannot test naming at all. apply_ble_identity()
     * attaches the 0x09C8 company id to every identity that has no service UUID,
     * so a "name-only" row still arrives carrying the strongest tell there is and
     * is Confirmed on the manufacturer id before the name is ever consulted. Two
     * such rows were added on 2026-09-07 and read as CONFIRMED on the FIRST bench
     * run, with the companion still running firmware that had never heard of
     * those names -- a pass that proved nothing. Caught only by asking why a
     * detection that should have been impossible succeeded.
     */
    bool name_only;
    /**
     * Advertise an ASTM F3411 Remote ID broadcast instead of a Flock advert.
     *
     * The three messages below are sent in ROTATION from this one address, which
     * is exactly how a real aircraft behaves: a BLE legacy advert carries ONE
     * 25-byte message, so the serial, the aircraft position and the operator
     * position arrive seconds apart and the detector has to accumulate them.
     * A rig that sent all three at once would never exercise that, and merging
     * across adverts is the part most likely to be wrong.
     */
    bool remote_id;
    const char* expect;
    /**
     * The BLE address this identity advertises from. DISTINCT PER IDENTITY, and
     * that is the entire point.
     *
     * Every BLE identity used to advertise from the board's own single address,
     * so the detector -- which keys its table on the address -- collapsed all of
     * them into ONE row that changed name and evidence as the rotation advanced.
     * A row could not be attributed to the identity that produced it, which makes
     * the rig unable to validate the thing it exists to validate. It also cost a
     * day: a row correctly Confirmed by the Raven identity, wearing a name from a
     * different advert, was read as a false positive.
     *
     * Must be a RANDOM STATIC address, so the top two bits of the first byte are
     * set (0xC0 mask) -- esp_ble_gap_set_rand_addr() rejects anything else. That
     * means these deliberately do NOT match a Flock OUI, which is correct here:
     * each identity then exercises exactly ONE tell (mfg id, naming, or GATT)
     * with no OUI match confusing the result. The BLE bare-OUI path is covered by
     * the host tests instead (flock_ble_tell's OuiOnly case).
     */
    uint8_t addr[6];
} BleIdentity;

// ALL OF THESE ADVERTISE FROM ONE BLE ADDRESS -- the board's own. Unlike the
// WiFi identities above, which carry a spoofed source MAC in a hand-built frame,
// apply_ble_identity() only swaps the advertised PAYLOAD. The detector keys its
// table on the address, so all of these collapse into a SINGLE row that changes
// name and evidence as the rotation advances.
//
// That is worth knowing before reading bench output: there is no separate
// "Penguin" row and no separate "bench-raven" row, and the one row you get is
// named by whichever advert was seen first. On 2026-09-06 that was the stack's
// default GAP name, and the resulting "ESP32" row -- correctly Confirmed via the
// Raven GATT identity below -- was mistaken for a false positive and cost a
// shipped detection regression. Both halves of that are now fixed (an explicit
// init name here, a specificity upgrade in the app), but the shared address is
// inherent to how this rig works.
static const BleIdentity BLE_IDS[] = {
    {"Penguin-1234567890",
     "TN72023022000771",
     NULL,
     false,
     false,
     "FLOCK, serial TN72023022000771",
     {0xC0, 0xFD, 0x00, 0x00, 0x00, 0x01}},
    {"FS Ext Battery",
     NULL,
     NULL,
     false,
     false,
     "FLOCK, no serial (model label, not a serial)",
     {0xC0, 0xFD, 0x00, 0x00, 0x00, 0x02}},
    {"bench-raven",
     NULL,
     "00003100-0000-1000-8000-00805f9b34fb",
     false,
     false,
     "Flock Raven (audio)",
     {0xC0, 0xFD, 0x00, 0x00, 0x00, 0x03}},
    // THE REGRESSION CANARY. 0x09C8 with a payload that yields NO decodable
    // serial (too short for the >=6 alphanumeric run) and no Flock naming, so the
    // ONLY thing identifying it is the manufacturer id. It must still read
    // CONFIRMED. v0.87 briefly gated that id behind "a serial decoded", which
    // would have shown this identity as Possible -- below the default alert
    // threshold, i.e. silent. Had this identity existed, that change could not
    // have been written. If this ever shows anything but Confirmed, the gate is
    // back.
    {"bench-mfgonly",
     "A1",
     NULL,
     false,
     false,
     "FLOCK Confirmed on mfg id ALONE (no serial)",
     {0xC0, 0xFD, 0x00, 0x00, 0x00, 0x04}},
    // NAME-ONLY tells, added 2026-09-07 with the field-observed patterns.
    //
    // No manufacturer data and no GATT service, so the GAP name is the ONLY
    // thing that can classify these -- which is exactly what makes them a test.
    // The patterns live in three places (the companion's ble_do_scan, the app's
    // flock_ble_name_is_flock, and the emitter here), and when they were added
    // the companion's copy was the one that could silently not learn them: a
    // device it files as cat=0 never reaches the app's scorer at all, so the app
    // would look correct in isolation while the pair found nothing. These two
    // rows fail on the bench in that case instead of in someone's car.
    //
    // "RWLS-..." is verbatim from the field report the prefix came from -- the
    // unit appends its own MAC, whose OUI (38:5b:44, Silicon Labs) is the
    // corroboration that got that prefix into the OUI table in the same commit.
    {"RWLS-38:5B:44:B3:0F:5A",
     NULL,
     NULL,
     true,
     false,
     "FLOCK by NAME alone (RWLS- prefix)",
     {0xC0, 0xFD, 0x00, 0x00, 0x00, 0x05}},
    // The shaped form. "FS-" plus exactly six hex; the negative cases ("FS-12",
    // "FS-XYZQRS", trailing junk) are covered by the host tests, because an
    // emitter can only ever demonstrate the positive.
    {"FS-1A2B3C",
     NULL,
     NULL,
     true,
     false,
     "FLOCK by NAME alone (FS-XXXXXX unit id)",
     {0xC0, 0xFD, 0x00, 0x00, 0x00, 0x06}},
    // REMOTE ID. Not a Flock device at all -- it must come out as its own class
    // ("Drone" / "Unmanned aircraft"), never as a camera. Rotates Basic ID ->
    // Location -> System from this one address so the detector has to accumulate
    // across adverts to end up holding the serial, the aircraft position and the
    // operator position at the same time.
    {"bench-drone",
     NULL,
     NULL,
     false,
     true,
     "DRONE: serial BENCH-DRONE-01 + operator location",
     {0xC0, 0xFD, 0x00, 0x00, 0x00, 0x07}},
};
#define BLE_ID_COUNT (sizeof(BLE_IDS) / sizeof(BLE_IDS[0]))

// ---- Remote ID (ASTM F3411) bench messages --------------------------------
//
// Three encoded 25-byte messages, sent in rotation from one address.
//
// COORDINATES ARE A DELIBERATE, PUBLIC, OBVIOUSLY-FAKE PLACE (lower Manhattan),
// not wherever this board happens to be. A bench rig that transmitted its own
// real position would write the operator's location into every capture, log and
// screenshot taken while testing -- and those get attached to issues.
//
// Aircraft 40.7128 / -74.0060, operator 40.7200 / -74.0100, i.e. about a
// kilometre apart, so a bearing to the operator is visibly different from a
// bearing to the drone and a test cannot pass by confusing the two.
//
// THESE EXACT BYTES ARE PINNED BY test_open_drone_id.c. Hand-encoding a
// little-endian int32 is precisely the thing to get wrong, and it is invisible
// on hardware -- the first version of this block decoded to -74.0264 and
// 40.7564, which are perfectly plausible coordinates a few kilometres away and
// would have "passed" any test that only checked a drone appeared. If you edit a
// coordinate here, update that test in the same commit; it decodes these arrays
// with the real decoder and asserts the degrees.
static const uint8_t ODID_BASIC_ID[25] = {
    0x02, // Basic ID, protocol version 2
    0x12, // id type 1 (serial), UA type 2 (multirotor)
    'B', 'E', 'N', 'C', 'H', '-', 'D', 'R', 'O', 'N', 'E', '-', '0', '1',
    0, 0, 0, 0, 0, 0, // id padding to 20 bytes
    0, 0, 0 // reserved
};
static const uint8_t ODID_LOCATION[25] = {
    0x12, // Location, protocol version 2
    0x00, // status/flags: SpeedMult 0, EW 0
    90, // direction 90 deg
    40, // speed 40 * 0.25 = 10 m/s
    0xC0, 0x47, 0x44, 0x18, // latitude  407128000 -> 40.7128
    0xA0, 0x94, 0xE3, 0xD3, // longitude -740060000 -> -74.0060
    0x00, 0x00, // baro altitude: unknown
    0x60, 0x09, // geo altitude 2400 -> 200 m
    0x98, 0x08, // height 2200 -> 100 m
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};
static const uint8_t ODID_SYSTEM[25] = {
    0x42, // System, protocol version 2
    0x01, // operator location type: live GPS
    0x00, 0x61, 0x45, 0x18, // operator latitude  407200000 -> 40.7200
    0x60, 0xF8, 0xE2, 0xD3, // operator longitude -740100000 -> -74.0100
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};
static const uint8_t* const ODID_ROTATION[3] = {ODID_BASIC_ID, ODID_LOCATION, ODID_SYSTEM};
static uint8_t g_odid_step = 0;

// XUNTONG company id 0x09C8, little-endian on the wire.
static const uint8_t XUNTONG_LE[2] = {0xC8, 0x09};

// ---- state -----------------------------------------------------------------

static int g_wifi_idx = -1;
static int g_ble_idx = -1;
static uint32_t g_last_wifi_rotate = 0;
static uint32_t g_last_ble_rotate = 0;
static uint32_t g_last_beacon = 0;
static uint32_t g_last_probe = 0; /**< re-arm clock for probe identities */

static BLEAdvertising* g_adv = nullptr;

// ---- WiFi ------------------------------------------------------------------

/**
 * Build and inject one beacon frame.
 *
 * Hand-rolled rather than using softAP so the source MAC can be an arbitrary
 * Flock OUI and so the SSID IE can be omitted entirely (which softAP cannot do
 * -- its "hidden" mode still emits a zero-length IE, and we want to cover the
 * absent-IE case too).
 */
static void send_beacon(const WifiIdentity* id) {
    uint8_t frame[128];
    size_t n = 0;

    frame[n++] = 0x80; // type/subtype: mgmt, beacon
    frame[n++] = 0x00; // flags
    frame[n++] = 0x00; // duration
    frame[n++] = 0x00;
    for(int i = 0; i < 6; i++)
        frame[n++] = 0xFF; // addr1: broadcast
    memcpy(frame + n, id->mac, 6); // addr2: source (the spoofed OUI)
    n += 6;
    memcpy(frame + n, id->mac, 6); // addr3: BSSID
    n += 6;
    frame[n++] = 0x00; // sequence control (the driver rewrites this)
    frame[n++] = 0x00;

    memset(frame + n, 0, 8); // timestamp
    n += 8;
    frame[n++] = 0x64; // beacon interval (100 TU)
    frame[n++] = 0x00;
    frame[n++] = 0x01; // capability: ESS
    frame[n++] = 0x00;

    // SSID IE (tag 0). Both hidden encodings are emitted, by separate
    // identities, because the companion detects them on separate code paths:
    // SsidZeroLen exercises its `ssid_len == 0` case and SsidAllNul exercises
    // the byte scan. Neither is reachable from the host tests -- those cover
    // the `hid=1` TOKEN, not the ESP's detection of the frame -- so this sketch
    // is the only thing that ever runs either branch.
    //
    // The IE is always present. The companion only claims hid=1 when it FINDS
    // the IE and finds it empty; omitting the IE entirely is a parse miss, not
    // evidence of hiding, and must not be reported as such.
    if(id->enc == SsidNamed) {
        size_t len = strlen(id->ssid);
        frame[n++] = 0x00;
        frame[n++] = (uint8_t)len;
        memcpy(frame + n, id->ssid, len);
        n += len;
    } else if(id->enc == SsidAllNul) {
        frame[n++] = 0x00; // tag 0
        frame[n++] = ALLNUL_SSID_LEN; // length N...
        memset(frame + n, 0, ALLNUL_SSID_LEN); // ...of nothing but NULs
        n += ALLNUL_SSID_LEN;
    } else {
        frame[n++] = 0x00; // tag 0
        frame[n++] = 0x00; // length 0 -> hidden
    }

    // Supported rates (tag 1): 1, 2, 5.5, 11 Mbps. Present so the frame parses
    // as a plausible beacon in Wireshark during debugging.
    frame[n++] = 0x01;
    frame[n++] = 0x04;
    frame[n++] = 0x82;
    frame[n++] = 0x84;
    frame[n++] = 0x8b;
    frame[n++] = 0x96;

    // DS parameter set (tag 3): current channel.
    frame[n++] = 0x03;
    frame[n++] = 0x01;
    frame[n++] = EMIT_CHANNEL;

    // en_sys_seq MUST be true. The IDF will not send a frame whose source MAC is
    // not one of the interface MACs unless it owns the sequence number, and every
    // identity here is a spoofed OUI -- that is the whole point of the rig. Passed
    // as false, the driver logs
    //     E wifi:en_sys_seq should be true to avoid side-effect to WiFi connection
    // for every single beacon and nothing reaches the air.
    //
    // That is how it shipped, and it is why "the emitter has never been run" was
    // load-bearing: it compiles either way, and the failure is only visible on a
    // detector that stays empty while the serial log cheerfully narrates
    // identities it never actually transmitted. Verified on hardware -- with false
    // the Flipper saw 0 Wi-Fi detections across several minutes at 36 frames/s
    // while happily reporting the BLE identities from the same board.
    //
    // The driver overwriting the sequence number costs this rig nothing: the
    // identities are distinct MACs, not a MAC-cycling burst, so nothing here
    // depends on controlling seq. (The companion's sequence-run coalescer is
    // exercised by real MAC-cycling hardware, not by this.)
    esp_wifi_80211_tx(WIFI_IF_AP, frame, n, true);
}

/** Switch the STA interface MAC, then scan, which sprays probe requests. */
/**
 * Point the STA interface at `id`s MAC. Returns the drivers verdict.
 *
 * THE INTERFACE MUST BE STOPPED FIRST. esp_wifi_set_mac() refuses while Wi-Fi is
 * started, and it refuses QUIETLY unless the return code is read -- which is how
 * this rig spent a bench session emitting probe requests from the ESP32 factory
 * Espressif MAC while its serial log announced a spoofed Flock OUI. The detector
 * was right to ignore them: they genuinely were not Flock frames.
 *
 * Symptom to recognise: beacon identities detect normally (they are hand-built
 * frames, so the source MAC is whatever we write into the buffer) while every
 * probe identity is invisible. If that returns, check this return code before
 * suspecting the detector.
 */
static esp_err_t set_sta_mac(const WifiIdentity* id) {
    esp_wifi_stop();
    esp_wifi_set_mode(WIFI_MODE_APSTA);
    esp_err_t rc = esp_wifi_set_mac(WIFI_IF_STA, (uint8_t*)id->mac);
    esp_wifi_start();
    return rc;
}

static void start_probe_burst(const WifiIdentity* id) {
    uint8_t actual[6] = {0};
    esp_wifi_get_mac(WIFI_IF_STA, actual);
    // Only restart the interface when the MAC is actually wrong -- doing it on
    // every re-arm would tear down the scan we are trying to sustain.
    if(memcmp(actual, id->mac, 6) != 0) {
        esp_err_t rc = set_sta_mac(id);
        esp_wifi_get_mac(WIFI_IF_STA, actual);
        if(memcmp(actual, id->mac, 6) != 0) {
            Serial.printf(
                "[WARN] STA MAC spoof FAILED rc=%d, probes go out as %02x:%02x:%02x\n",
                (int)rc, actual[0], actual[1], actual[2]);
        }
    }

    wifi_scan_config_t cfg = {};
    // Wildcard probe (no SSID IE, the phone-home shape) unless the identity
    // names a network, in which case it is a DIRECTED probe for that name --
    // the shape a phone makes when it looks for a network it once joined.
    cfg.ssid = (id->enc == SsidNamed && id->ssid) ? (uint8_t*)id->ssid : NULL;
    cfg.bssid = NULL;
    cfg.channel = EMIT_CHANNEL;
    cfg.show_hidden = true;
    cfg.scan_type = WIFI_SCAN_TYPE_ACTIVE;
    esp_wifi_scan_start(&cfg, false);
}


static void apply_wifi_identity(int idx) {
    const WifiIdentity* id = &WIFI_IDS[idx];

    esp_wifi_scan_stop();
    // THE AP INTERFACE MAC IS DELIBERATELY LEFT ALONE.
    //
    // It used to be set to the identity for beacon rows, and that was both
    // unnecessary and actively harmful. Unnecessary because an injected beacon
    // carries its source in addr2/addr3 of a frame we build by hand (see
    // send_beacon) -- the interface MAC is not consulted. Harmful because this
    // sketch runs in APSTA mode, so the ESP32's OWN SoftAP is up and beaconing
    // its default `ESP_xxxxxx` SSID: pointing that AP at the identity's address
    // made the board emit a SECOND, unintended beacon from the spoofed MAC.
    //
    // The detector stores the FIRST SSID it sees for a MAC, so whichever beacon
    // won the race named the row. Observed 2026-08-29: identity #9 arrived as
    // `ESP_CFDD91` rather than `bench-moto-1`, on MAC 00:04:7d:00:00:0a. The
    // rung was still right (the OUI is what fires it) but the name was the
    // board's own, which makes this rig lie about the exact field an SSID
    // identity exists to test -- and sends anyone reading the bench output
    // hunting a false positive that is really the test rig.
    //
    // Left in place for probe identities, which genuinely do need it: a probe's
    // source IS the interface MAC. That is set on WIFI_IF_STA by set_sta_mac(),
    // and the ESP32 rejects a STA MAC equal to the AP MAC with
    // ESP_ERR_WIFI_MAC (0x3009) -- which is why these two must stay different,
    // and another reason not to write an identity into the AP interface.
    esp_wifi_set_channel(EMIT_CHANNEL, WIFI_SECOND_CHAN_NONE);

    if(id->kind == EmitProbe) start_probe_burst(id);

    Serial.printf(
        "[WIFI] #%d %02x:%02x:%02x:%02x:%02x:%02x ssid=%-14s -> expect: %s\n",
        idx,
        id->mac[0],
        id->mac[1],
        id->mac[2],
        id->mac[3],
        id->mac[4],
        id->mac[5],
        id->enc == SsidNamed  ? id->ssid :
        id->enc == SsidAllNul ? "<all-NUL>" :
                                "<zero-len>",
        id->expect);
}

// ---- BLE -------------------------------------------------------------------

static void apply_ble_identity(int idx) {
    const BleIdentity* id = &BLE_IDS[idx];

    g_adv->stop();

    // Advertise this identity from ITS OWN address, so the detector files it as
    // its own device instead of folding every identity into one row. Done while
    // advertising is stopped: esp_ble_gap_set_rand_addr() (which this wraps)
    // will not take effect underneath a running advertiser.
    esp_bd_addr_t bd;
    memcpy(bd, id->addr, sizeof(bd));
    g_adv->setDeviceAddress(bd, BLE_ADDR_TYPE_RANDOM);

    BLEAdvertisementData data;
    // THE NAME IS OMITTED FROM A REMOTE ID ADVERT, and it has to be.
    //
    // BLE legacy advertising carries 31 bytes total. One ODID service-data
    // element is 2 (len+type) + 2 (UUID 0xFFFA) + 1 (app code) + 1 (counter) +
    // 25 (message) = EXACTLY 31. Adding "bench-drone" as a name AD costs another
    // 13, and the flags AD another 3, for 47 into a 31-byte packet -- the stack
    // rejects the whole thing and the board advertises NO Remote ID at all.
    //
    // That is precisely what happened on the first bench run: RWLS- and FS-1A2B3C
    // (which fit easily) both came back CONFIRMED while the drone never appeared
    // once, and it read like a companion-side failure. It was this. The name
    // still goes out in the SCAN RESPONSE below, which is a separate 31 bytes, so
    // the identity is still attributable in the log.
    if(!id->remote_id) data.setName(id->name);

    // REMOTE ID: service data under UUID 0xFFFA, first byte the 0x0D application
    // code, then a message counter, then one 25-byte message. Cycling the
    // message each call is what makes the detector accumulate rather than
    // receive everything at once.
    if(id->remote_id) {
        std::string sd;
        sd.push_back((char)0x0D);
        sd.push_back((char)g_odid_step);
        const uint8_t* msg = ODID_ROTATION[g_odid_step % 3];
        g_odid_step++;
        for(size_t i = 0; i < 25; i++) sd.push_back((char)msg[i]);
        data.setServiceData(BLEUUID((uint16_t)0xFFFA), fmfg(sd));
        // PRINT WHAT WAS ACTUALLY BUILT, not what we intended. The library's
        // addData() silently RETURNS when a payload would exceed the 31-byte
        // legacy limit -- no error, no log, the element just is not there. That
        // is how the first version of this identity advertised nothing at all
        // while every line of code looked right, and it cost a bench run to find.
        // An advert that is not exactly 31 bytes here is a bug.
        // `auto` + length() + [] on purpose: getPayload() returns std::string on
        // core 2.x and Arduino String on 3.x, and this sketch only has the
        // one-way fmfg() shim. Naming the type compiled here on 2.0.17 and broke
        // the 3.x CI job. length() and operator[] exist on BOTH, so this needs no
        // shim at all.
        auto pl = data.getPayload();
        Serial.printf("[RID ] advert payload %u bytes: ", (unsigned)pl.length());
        for(size_t i = 0; i < pl.length(); i++) Serial.printf("%02x", (uint8_t)pl[i]);
        Serial.printf("\n");
    }

    if(!id->service_uuid && !id->name_only && !id->remote_id) {
        // Manufacturer-specific data: the 2-byte company id, then the serial if
        // this identity has one. flock_ble_extract_serial() digs the serial back
        // out of exactly this layout. The no-serial case ("FS Ext Battery") still
        // carries the company id, so it must classify as Flock with no serial
        // rather than falling back to reading the model label as one.
        std::string mfg((const char*)XUNTONG_LE, sizeof(XUNTONG_LE));
        if(id->serial) mfg.append(id->serial);
        data.setManufacturerData(fmfg(mfg));
    }

    if(id->service_uuid) {
        data.setCompleteServices(BLEUUID(id->service_uuid));
    }

    g_adv->setAdvertisementData(data);

    // Put the identity's name in the SCAN RESPONSE too, so both payloads agree.
    //
    // A scanner may read either one, and if the scan response is left empty the
    // stack answers with its own default GAP name. That is precisely how "ESP32"
    // ended up naming a Flock-Confirmed row (see BLEDevice::init in setup()).
    // Identity #3 needs this most: "bench-raven" plus a complete 128-bit UUID is
    // 31 bytes, right at the AD limit, so the name is the field most likely to be
    // dropped from the advert and looked for in the scan response instead.
    BLEAdvertisementData scan_rsp;
    scan_rsp.setName(id->name);
    g_adv->setScanResponseData(scan_rsp);
    g_adv->setScanResponse(true);

    g_adv->start();

    // Address included so the bench log can be matched against the detector's
    // rows one-to-one -- the whole reason the addresses are distinct.
    Serial.printf(
        "[BLE ] #%d %02x:%02x:%02x:%02x:%02x:%02x name=%-20s -> expect: %s\n",
        idx,
        id->addr[0],
        id->addr[1],
        id->addr[2],
        id->addr[3],
        id->addr[4],
        id->addr[5],
        id->name,
        id->expect);
}

// ---- setup / loop ----------------------------------------------------------

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println();
    Serial.println("=====================================================");
    Serial.println("  FlipDeFlock bench emitter -- THIS BOARD TRANSMITS");
    Serial.println("  Bench use only. Turn it off when you are done.");
    Serial.println("=====================================================");
    Serial.printf("  WiFi identities: %u (rotate %u ms)\n", (unsigned)WIFI_ID_COUNT, WIFI_ROTATE_MS);
    Serial.printf("  BLE  identities: %u (rotate %u ms)\n", (unsigned)BLE_ID_COUNT, BLE_ROTATE_MS);
    Serial.printf("  Channel: %d\n\n", EMIT_CHANNEL);

    WiFi.mode(WIFI_AP_STA);
    esp_wifi_set_channel(EMIT_CHANNEL, WIFI_SECOND_CHAN_NONE);
    // Injection needs promiscuous mode enabled on some IDF builds, even though
    // we never install an RX callback.
    esp_wifi_set_promiscuous(true);

    // NEVER init with "". The Bluedroid stack then keeps its own default GAP
    // device name -- "ESP32" -- and a scanner that reads the scan response before
    // an identity advert gets THAT as the device's name. The detector stores the
    // first name it sees for a MAC and never replaces it, so the row wears
    // "ESP32" permanently while the same address goes on to advertise
    // "Penguin-...", "FS Ext Battery" and the Raven GATT UUID.
    //
    // THIS RIG HAS NOW CAUSED TWO FALSE DIAGNOSES BY THE SAME MECHANISM. See the
    // block in apply_wifi_identity() recording 2026-08-29, when the board's own
    // SoftAP beacon won the name race against identity #9. On 2026-09-06 the BLE
    // half did it again: a correctly-Confirmed row reading "ESP32" was taken for
    // a false positive and cost a shipped detection regression before the cause
    // was found. A default vendor name from this board is a RIG DEFECT, never a
    // detector finding.
    //
    // Named so it can never be mistaken for a real device, nor for a generic
    // module: if this string ever appears on the detector, the rig is talking.
    BLEDevice::init("FDF-BENCH");
    g_adv = BLEDevice::getAdvertising();
    g_adv->setMinInterval(160); // 100 ms: several adverts per rotation
    g_adv->setMaxInterval(160);

    g_wifi_idx = 0;
    apply_wifi_identity(0);
    g_ble_idx = 0;
    apply_ble_identity(0);

    g_last_wifi_rotate = millis();
    g_last_ble_rotate = millis();
}

// ---- Net Guardian test: sustained beacon flood ---------------------------
//
// Net Guardian on the Flipper triages ATTACKS the companion reports, and one of
// those is a beacon flood -- many DISTINCT beaconing BSSIDs in one ~1 s interval
// (Marauder/Pineapple SSID spam). Its host tests cover the triage LOGIC, but the
// populated attack screen -- the "ACTIVE" verdict that only appears once frames
// keep arriving across several intervals -- can only be exercised by a real
// flood on the air. This is that flood, so the feature can be checked end to end
// on the bench the same way the Flock identities above are.
//
// OFF by default, toggled over serial ("atk on" / "atk off"), so ordinary Flock
// testing is never polluted by attack traffic. When on it sprays distinct-BSSID
// beacons every loop, sustained, which is what pushes Net Guardian past "brief"
// to "ACTIVE".
//
// The BSSIDs use the locally-administered prefix 02:00:00 with an incrementing
// tail. That is deliberately NOT a real OUI, so a flood can never collide with a
// Flock or vendor table and show up as a bogus camera in the detection list --
// it is seen ONLY by the attack counters, which key on distinct BSSID.
static bool g_attack = false;
static uint32_t g_flood_ctr = 0;

#define FLOOD_PER_LOOP 8 // distinct beacons per loop pass; >=40/interval -> flood

// The companion sweeps channels, and a beacon flood is only counted within the
// ONE interval it is seen -- so a single-channel flood is caught roughly once
// per full sweep, which keeps the count below the "more than churn" floor and
// leaves Net Guardian stuck on "brief" instead of ACTIVE. Spraying the three
// non-overlapping 2.4 GHz channels means whichever one the companion dwells on,
// it sees a full flood, so the count and the span both grow every sweep.
static const uint8_t FLOOD_CHANS[] = {EMIT_CHANNEL}; // all flood density on the
// channel the companion already dwells on for Flock, so every dwell sees a full
// flood -- hopping 1/6/11 split the density three ways and undercut the count.

static void send_flood_burst() {
    for(size_t ch = 0; ch < sizeof(FLOOD_CHANS); ch++) {
        esp_wifi_set_channel(FLOOD_CHANS[ch], WIFI_SECOND_CHAN_NONE);
        for(int i = 0; i < FLOOD_PER_LOOP; i++) {
            uint32_t c = g_flood_ctr++;
            WifiIdentity f;
            f.mac[0] = 0x02; // locally administered, cannot match a real OUI table
            f.mac[1] = 0x00;
            f.mac[2] = 0x00;
            f.mac[3] = (uint8_t)(c >> 16);
            f.mac[4] = (uint8_t)(c >> 8);
            f.mac[5] = (uint8_t)c;
            f.ssid = "atk-flood";
            f.enc = SsidNamed;
            f.kind = EmitBeacon;
            f.expect = "";
            send_beacon(&f);
        }
    }
    // Hand the radio back to the Flock identities, which all live on EMIT_CHANNEL.
    esp_wifi_set_channel(EMIT_CHANNEL, WIFI_SECOND_CHAN_NONE);
}

// Non-blocking one-line serial command reader. The sketch is almost all TX; this
// is the only RX path, and it exists purely for the attack toggle above.
static void poll_serial_cmd() {
    static char buf[24];
    static uint8_t len = 0;
    while(Serial.available()) {
        char ch = (char)Serial.read();
        if(ch == '\n' || ch == '\r') {
            buf[len] = '\0';
            if(strcmp(buf, "atk on") == 0) {
                g_attack = true;
                Serial.println("[ATK] beacon flood ON -- Net Guardian should go ACTIVE");
            } else if(strcmp(buf, "atk off") == 0) {
                g_attack = false;
                Serial.println("[ATK] beacon flood OFF");
            } else if(len > 0) {
                Serial.printf("[ATK] unknown cmd '%s' (use: atk on | atk off)\n", buf);
            }
            len = 0;
        } else if(len < sizeof(buf) - 1) {
            buf[len++] = ch;
        }
    }
}

void loop() {
    poll_serial_cmd();

    // ATTACK MODE OWNS THE RADIO. The Flock identity rotation below fights the
    // flood for the antenna -- probe identities in particular call
    // esp_wifi_scan_start, which hops channels for the scan and pulls the radio
    // off whatever channel the flood just set. The result was a flood the
    // companion saw only in bursts, so its per-second distinct count kept
    // dropping under the threshold and Net Guardian never sustained past
    // "brief". The Flock identities are not needed during a Net Guardian test,
    // so give the flood the radio outright and resume on "atk off".
    if(g_attack) {
        send_flood_burst();
        delay(2);
        return;
    }


    uint32_t now = millis();

    // Probe identities hold longer than beacon ones -- see PROBE_HOLD_MS.
    uint32_t hold =
        (WIFI_IDS[g_wifi_idx].kind == EmitProbe) ? PROBE_HOLD_MS : WIFI_ROTATE_MS;
    if(now - g_last_wifi_rotate >= hold) {
        g_last_wifi_rotate = now;
        g_wifi_idx = (g_wifi_idx + 1) % WIFI_ID_COUNT;
        apply_wifi_identity(g_wifi_idx);
    }

    if(now - g_last_ble_rotate >= BLE_ROTATE_MS) {
        g_last_ble_rotate = now;
        g_ble_idx = (g_ble_idx + 1) % BLE_ID_COUNT;
        apply_ble_identity(g_ble_idx);
    }

    // Beacon identities need a steady stream.
    if(WIFI_IDS[g_wifi_idx].kind == EmitBeacon && now - g_last_beacon >= BEACON_MS) {
        g_last_beacon = now;
        send_beacon(&WIFI_IDS[g_wifi_idx]);
    }

    // Probe identities need one too, for the same reason: a camera phoning home
    // does it continuously, not once. Re-arming is cheap and a scan already in
    // flight simply refuses the call, so this needs no completion tracking.
    if(WIFI_IDS[g_wifi_idx].kind == EmitProbe && now - g_last_probe >= PROBE_REARM_MS) {
        g_last_probe = now;
        start_probe_burst(&WIFI_IDS[g_wifi_idx]);
    }

    delay(10);
}
