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
 * WiFi identities (rotates every WIFI_ROTATE_MS)
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

    esp_wifi_80211_tx(WIFI_IF_AP, frame, n, false);
}

/** Switch the STA interface MAC, then scan, which sprays probe requests. */
static void start_probe_burst(const WifiIdentity* id) {
    esp_wifi_set_mode(WIFI_MODE_APSTA);
    esp_wifi_set_mac(WIFI_IF_STA, (uint8_t*)id->mac);

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
    esp_wifi_set_mac(WIFI_IF_AP, (uint8_t*)id->mac);
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

void loop() {
    uint32_t now = millis();

    if(now - g_last_wifi_rotate >= WIFI_ROTATE_MS) {
        g_last_wifi_rotate = now;
        g_wifi_idx = (g_wifi_idx + 1) % WIFI_ID_COUNT;
        apply_wifi_identity(g_wifi_idx);
    }

    if(now - g_last_ble_rotate >= BLE_ROTATE_MS) {
        g_last_ble_rotate = now;
        g_ble_idx = (g_ble_idx + 1) % BLE_ID_COUNT;
        apply_ble_identity(g_ble_idx);
    }

    // Beacon identities need a steady stream; probe identities are driven by the
    // scan started at rotation time.
    if(WIFI_IDS[g_wifi_idx].kind == EmitBeacon && now - g_last_beacon >= BEACON_MS) {
        g_last_beacon = now;
        send_beacon(&WIFI_IDS[g_wifi_idx]);
    }

    delay(10);
}
