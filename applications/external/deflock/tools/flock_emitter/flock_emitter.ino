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
 *   2 Raven GATT service 0x3100               "Flock Raven (audio)"
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
    const char* expect;
} BleIdentity;

static const BleIdentity BLE_IDS[] = {
    {"Penguin-1234567890", "TN72023022000771", NULL, "FLOCK, serial TN72023022000771"},
    {"FS Ext Battery", NULL, NULL, "FLOCK, no serial (model label, not a serial)"},
    {"bench-raven", NULL, "00003100-0000-1000-8000-00805f9b34fb", "Flock Raven (audio)"},
};
#define BLE_ID_COUNT (sizeof(BLE_IDS) / sizeof(BLE_IDS[0]))

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
    cfg.ssid = NULL; // wildcard probe: no SSID IE, the phone-home shape
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

    BLEAdvertisementData data;
    data.setName(id->name);

    if(!id->service_uuid) {
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
    g_adv->start();

    Serial.printf("[BLE ] #%d name=%-20s -> expect: %s\n", idx, id->name, id->expect);
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

    BLEDevice::init("");
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
