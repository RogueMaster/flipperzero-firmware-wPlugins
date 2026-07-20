/*
 * Flock Companion - universal ESP32 Wi-Fi sniffer for the Flipper Zero
 * "Recon Site Survey" app.
 *
 * Runs on ANY ESP32 board exposed to the Flipper UART (Marauder hardware,
 * ReksLab Tri-Board, bare WROVER/WROOM, Xiao ESP32-S3, DevKitC, ...).
 * Puts the radio in promiscuous monitor mode, hops channels 1-11, and reports
 * frames that look like Flock Safety / ALPR surveillance gear (by OUI, by
 * phone-home probe behaviour, and by SSID naming) over the serial link in a
 * simple line protocol the Flipper parses.
 *
 * Detection method and OUI list are from the open-source counter-surveillance
 * projects (colonelpanichacks/flock-you, 0xXyc/flock-you-wifi-recon) and the
 * DeFlock community. Passive recon only -- no deauth, no injection.
 *
 * Build: Arduino IDE or arduino-cli with the esp32 core. Select your board,
 * set Serial baud to 115200. No extra libraries required.
 *
 * Line protocol (newline-terminated, ASCII), TX to Flipper:
 *   FLOCKCO,1                              banner / version on boot and on "ver"
 *   S,<frames>,<hits>,<ch>,<deauth_rate>   status, ~1 Hz (deauth/disassoc per interval)
 *   D,<mac>,<rssi>,<ch>,<type>,<conf>,<ssid>[,fp=<hex32>]   detection
 *       mac : aabbccddeeff (lower hex, no separators)
 *       rssi: signed dBm
 *       ch  : 1-11
 *       type: P=probe-req  B=beacon  R=probe-resp  O=other
 *       conf: 1=possible 2=likely 3=confirmed (ESP-side score)
 *       ssid: raw SSID with ',' and control chars stripped (may be empty)
 *       fp  : FNV-1a uint32 (8 lower-hex) of the probe's IE skeleton (B1) --
 *             a MAC-independent device-CLASS fingerprint; trailing field,
 *             older parsers ignore it. Only emitted for probe requests.
 *   BLE,<addr>,<rssi>,<cat>,<company>,<name>[,<mfghex>][,rv=1]   BLE device
 *       cat   : 0 unknown 1 Flock/Raven 2 AirTag 3 Tile 4 SmartTag 5 FMDN
 *       mfghex: raw mfg-data hex (Flock 0x09C8 only) for serial decode; pure
 *               hex, no '='. Trailing, older parsers ignore.
 *       rv=1  : the device exposed a Raven-specific GATT service (0x3100-
 *               0x3500) -> positive acoustic-sensor (Raven) identification.
 *               Trailing, contains '=' so it's distinguishable from mfghex;
 *               only emitted when matched, older parsers ignore.
 *   DA,<bssid>,<ch>                        deauth/disassoc attack target (attributed)
 *   ATK,<kind>,<value>                     active attack-tool signature
 *       kind : probeflood  (abnormal probe-request rate)
 *              beaconflood (many DISTINCT beaconing BSSIDs/s: Marauder/Pineapple)
 *              blespam     (Apple/Samsung/Google pairing-advert flood)
 *       value: the count/rate measured. New line; older app builds ignore it.
 *   LOC,<rssi>                             Locator: live RSSI of the active target
 *                                          (signed dBm), streamed while homing.
 *
 * RX from Flipper (commands, newline-terminated):
 *   scan   start reporting        stop   pause reporting
 *   ver    re-send banner         ch <n> lock to channel n (0 = hop)
 *   locate <w|b> <mac> [ch]  stream LOC for a target (w=Wi-Fi, b=BLE; mac is
 *                            aabbccddeeff). "locate off" (or any other command)
 *                            ends Locator mode.
 */

#include <Arduino.h>
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"

#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

// ---- Flock-associated OUI prefixes (32) ----------------------------------
static const uint8_t FLOCK_OUIS[][3] = {
    {0x70, 0xc9, 0x4e}, {0x3c, 0x91, 0x80}, {0xd8, 0xf3, 0xbc}, {0x80, 0x30, 0x49},
    {0xb8, 0x35, 0x32}, {0x14, 0x5a, 0xfc}, {0x74, 0x4c, 0xa1}, {0x08, 0x3a, 0x88},
    {0x9c, 0x2f, 0x9d}, {0xc0, 0x35, 0x32}, {0x94, 0x08, 0x53}, {0xe4, 0xaa, 0xea},
    {0xf4, 0x6a, 0xdd}, {0xf8, 0xa2, 0xd6}, {0x24, 0xb2, 0xb9}, {0x00, 0xf4, 0x8d},
    {0xd0, 0x39, 0x57}, {0xe8, 0xd0, 0xfc}, {0xe0, 0x4f, 0x43}, {0xb8, 0x1e, 0xa4},
    {0x70, 0x08, 0x94}, {0x58, 0x8e, 0x81}, {0xec, 0x1b, 0xbd}, {0x3c, 0x71, 0xbf},
    {0x58, 0x00, 0xe3}, {0x90, 0x35, 0xea}, {0x5c, 0x93, 0xa2}, {0x64, 0x6e, 0x69},
    {0x48, 0x27, 0xea}, {0xa4, 0xcf, 0x12}, {0x82, 0x6b, 0xf2},
    {0xb4, 0x1e, 0x52}, // Flock Safety's own registered OUI (GainSec)
};
static const size_t FLOCK_OUI_COUNT = sizeof(FLOCK_OUIS) / sizeof(FLOCK_OUIS[0]);

// ---- State ---------------------------------------------------------------
static volatile bool g_scanning = true;
static volatile uint32_t g_frames = 0;
static volatile uint32_t g_hits = 0;
static volatile uint32_t g_deauths = 0; // deauth + disassoc frames seen (attack indicator)
static uint8_t g_channel = 1;
static uint8_t g_lock_channel = 0; // 0 = hop
static uint32_t g_last_status = 0;
static uint32_t g_last_hop = 0;
static uint32_t g_deauths_last = 0; // for per-interval deauth rate
static uint32_t g_last_da = 0; // rate-limit DA attribution lines

// ---- Active attack-tool detection (emitted as ATK lines, ~1 Hz) -----------
// These are RATE signals reset every status interval, so the alert clears when
// the attack stops. Thresholds are deliberately conservative to avoid false
// positives in dense-but-benign RF; tune for your environment.
//   probeflood : abnormal probe-request rate (KARMA / mass-probe tools)
//   beaconflood: many DISTINCT beaconing BSSIDs/s (Marauder/Pineapple SSID spam)
//   blespam    : a flood of impersonation BLE adverts (Flipper/ESP BLE spam)
#define PROBE_FLOOD_MIN  80 // probe requests in one ~1 s interval
#define BEACON_FLOOD_MIN 40 // distinct beaconing BSSIDs in one ~1 s interval
#define BLE_SPAM_MIN     12 // impersonation-class adverts in one BLE scan
static volatile uint32_t g_probe_reqs = 0; // probe requests this interval (reset ~1 Hz)
#define BEACON_RING 64
static uint32_t g_beacon_ring[BEACON_RING]; // recent beacon-BSSID hashes this interval
static uint8_t g_beacon_ring_n = 0;
static uint32_t g_beacon_distinct = 0; // distinct beaconing BSSIDs this interval

// Note a beacon's source BSSID; counts it once per interval. Approximate by
// design (a small ring + best-effort across the WiFi-callback / loop tasks) --
// it only needs to tell "a handful of real APs" from "a spam flood."
static void note_beacon_bssid(const uint8_t* bssid) {
    uint32_t h = 2166136261u; // FNV-1a over the 6 BSSID bytes (inlined: no fwd dep)
    for(int i = 0; i < 6; i++) {
        h ^= bssid[i];
        h *= 16777619u;
    }
    for(uint8_t i = 0; i < g_beacon_ring_n; i++) {
        if(g_beacon_ring[i] == h) return; // already counted this interval
    }
    if(g_beacon_ring_n < BEACON_RING) {
        g_beacon_ring[g_beacon_ring_n++] = h;
        g_beacon_distinct++;
    }
}

// ---- Locator: stream live RSSI for one target so the app can home in on it ---
// 'w' Wi-Fi (match the MAC in promiscuous frames, channel-locked) or 'b' BLE
// (match the addr in a repeating scan). MAC kept BOTH as bytes (Wi-Fi, compared
// to raw frame bytes) and as the lowercase hex string (BLE, compared to the
// same toString() form the BLE line is built from -- avoids byte-order traps).
static char g_locate_kind = 0; // 0 none / 'w' / 'b'
static uint8_t g_locate_mac[6];
static char g_locate_macs[13]; // lowercase hex, no separators
static uint8_t g_locate_ch = 0;
static int g_locate_best = -127; // strongest RSSI since the last LOC emit (Wi-Fi)
static uint32_t g_last_loc = 0; // LOC emit throttle

static int hexv(char c) {
    if(c >= '0' && c <= '9') return c - '0';
    if(c >= 'a' && c <= 'f') return c - 'a' + 10;
    if(c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}
static bool parse_hexmac(const char* s, uint8_t out[6]) {
    for(int i = 0; i < 6; i++) {
        int hi = hexv(s[i * 2]), lo = hexv(s[i * 2 + 1]);
        if(hi < 0 || lo < 0) return false;
        out[i] = (uint8_t)((hi << 4) | lo);
    }
    return true;
}

// Dual-band (WiFi + BLE) Flock detection. BLE is initialised once and kept
// resident (avoids the Bluedroid init/deinit heap leak); the radio is shared by
// toggling promiscuous off during a BLE scan, then back on. flockcombo
// interleaves a WiFi-promiscuous phase with a periodic BLE scan phase.
static bool g_ble_inited = false;
static BLEScan* g_ble = nullptr;
static bool g_combo = false;
static uint32_t g_phase_start = 0;
#define COMBO_WIFI_MS 9000 // ~3 channel sweeps before a BLE scan (WiFi-biased)
#define COMBO_BLE_SEC 3 // BLE scan seconds (BLE adverts repeat fast)

static bool oui_match(const uint8_t* mac) {
    for(size_t i = 0; i < FLOCK_OUI_COUNT; i++) {
        if(mac[0] == FLOCK_OUIS[i][0] && mac[1] == FLOCK_OUIS[i][1] &&
           mac[2] == FLOCK_OUIS[i][2])
            return true;
    }
    return false;
}

static char lc(char c) {
    return (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c;
}

// Returns: 0 none, 2 likely (flock/flck substring), 3 confirmed (flock-/test_flck)
static int ssid_score(const char* s, int len) {
    if(len <= 0) return 0;
    char buf[64];
    int n = len < 63 ? len : 63;
    for(int i = 0; i < n; i++) buf[i] = lc(s[i]);
    buf[n] = 0;
    if(strstr(buf, "flock-") || strstr(buf, "test_flck")) return 3;
    if(strstr(buf, "flock") || strstr(buf, "flck")) return 2;
    return 0;
}

// Strip ',' and control chars so the SSID can't break the line protocol.
static void emit_ssid(const char* s, int len) {
    for(int i = 0; i < len && i < 48; i++) {
        char c = s[i];
        if(c == ',' || c == '\r' || c == '\n' || (uint8_t)c < 0x20) c = '.';
        Serial.write(c);
    }
}

// ---- B1: probe IE-fingerprint + sequence-number coalescer ----------------
//
// The whole OUI/SSID ladder collapses the day Flock randomizes the probe MAC.
// The probe *body* is MAC-independent: the ordered set of tagged Information
// Elements (supported rates, HT/VHT/HE caps, vendor-specific 0xDD OUI+type) is
// baked into the WiFi SoC driver and can't be scrambled without breaking
// 802.11. We hash that skeleton into a uint32 the Flipper compares against a
// curated table. This is a device-CLASS / firmware-stack match, NOT a unique
// device ID -- the Flipper reports it as a candidate class match only.
//
// We hash only the *skeleton* (tag id + length, plus the first OUI+type bytes
// of vendor-specific IEs), never per-frame variable contents, so the same probe
// template hashes identically regardless of the (possibly randomized) MAC.

#define FNV1A_OFFSET 0x811c9dc5u
#define FNV1A_PRIME 0x01000193u

static inline uint32_t fnv1a_u8(uint32_t h, uint8_t b) {
    return (h ^ b) * FNV1A_PRIME;
}

// FNV-1a over the IE skeleton of a probe request. `p` points at the frame body,
// `len` is the body length (FCS already removed). Tagged params start at byte
// 24 (probe request has no fixed params). For each IE we fold in (tag_id,
// length); for vendor-specific (0xDD) we also fold in up to the first 5 bytes
// (3-byte OUI + 1-2 type/subtype) -- enough to distinguish vendor IEs without
// shipping their variable payloads. Returns 0 if there are no parseable IEs.
static uint32_t ie_skeleton_hash(const uint8_t* p, int len) {
    uint32_t h = FNV1A_OFFSET;
    int off = 24; // tagged parameters begin here for a probe request
    bool any = false;
    while(off + 2 <= len) {
        uint8_t tag = p[off];
        uint8_t tlen = p[off + 1];
        if(off + 2 + tlen > len) break; // truncated IE -> stop
        h = fnv1a_u8(h, tag);
        h = fnv1a_u8(h, tlen);
        if(tag == 0xDD) { // vendor-specific: fold OUI + type (first 5 bytes)
            int n = tlen < 5 ? tlen : 5;
            for(int i = 0; i < n; i++) h = fnv1a_u8(h, p[off + 2 + i]);
        }
        any = true;
        off += 2 + tlen;
    }
    return any ? h : 0;
}

// Sequence-number-run coalescer. A MAC-cycling Flock burst sprays many probes
// from different (randomized) MACs but with a *contiguous* 802.11 sequence
// number run -- the SoC's seq counter increments across the burst regardless of
// the source address. We treat such a run as ONE logical sighting and suppress
// the duplicates on the ESP side so they never flood the Flipper's 96-entry
// table. Keyed on the IE-skeleton hash so unrelated traffic with nearby seq
// numbers isn't merged.
#define SEQ_RUN_GAP 4 // max seq-num step to still count as the same burst
#define SEQ_RUN_MS 1500 // a run older than this is stale; start fresh
static uint32_t g_seq_fp = 0; // IE hash of the current run (0 = none)
static uint16_t g_seq_last = 0; // last 802.11 sequence number in the run
static uint32_t g_seq_t = 0; // millis() of the last frame in the run

// Returns true if this frame should be SUPPRESSED as a duplicate within an
// in-progress MAC-cycling burst (same IE fingerprint, monotonic seq-num run).
static bool seq_run_duplicate(uint32_t fp, uint16_t seq) {
    if(fp == 0) return false; // no fingerprint -> can't coalesce
    uint32_t now = millis();
    bool fresh = (now - g_seq_t) <= SEQ_RUN_MS;
    if(fresh && fp == g_seq_fp) {
        uint16_t step = (uint16_t)(seq - g_seq_last); // wraps mod 4096 naturally
        if(step != 0 && step <= SEQ_RUN_GAP) {
            g_seq_last = seq; // extend the run, suppress this frame
            g_seq_t = now;
            return true;
        }
    }
    // New run (or a gap too large / stale): this frame is the run's first
    // sighting -> report it and start tracking from here.
    g_seq_fp = fp;
    g_seq_last = seq;
    g_seq_t = now;
    return false;
}

static void promisc_cb(void* buf, wifi_promiscuous_pkt_type_t type) {
    if(type != WIFI_PKT_MGMT) return;
    const wifi_promiscuous_pkt_t* pkt = (const wifi_promiscuous_pkt_t*)buf;
    const uint8_t* p = pkt->payload;
    // sig_len includes the 4-byte FCS; drop it so SSID bounds checks stay inside
    // the actual frame body.
    int len = pkt->rx_ctrl.sig_len;
    if(len < 28) return;
    len -= 4;

    // Snapshot the channel now: the hopper may advance before we finish.
    uint8_t frame_channel = g_channel;

    g_frames++;

    uint8_t subtype = (p[0] >> 4) & 0x0F;

    // Active-attack rate sampling (counted on EVERY frame, before any Flock
    // candidacy filtering): probe-request floods and beacon-spam (many distinct
    // beaconing BSSIDs). Flushed/evaluated in the ~1 Hz status block.
    if(subtype == 0x04) {
        g_probe_reqs++; // probe request
    } else if(subtype == 0x08) {
        note_beacon_bssid(p + 16); // beacon: addr3 = BSSID
    }

    // Locator (Wi-Fi): track the strongest RSSI of any frame to/from the target
    // MAC (addr1/2/3). loop() emits it as a throttled LOC line.
    if(g_locate_kind == 'w') {
        int r = pkt->rx_ctrl.rssi;
        if(memcmp(p + 4, g_locate_mac, 6) == 0 || memcmp(p + 10, g_locate_mac, 6) == 0 ||
           memcmp(p + 16, g_locate_mac, 6) == 0) {
            if(r > g_locate_best) g_locate_best = r;
        }
    }

    // Deauthentication (0x0C) / disassociation (0x0A) frames: a flood of these
    // is the signature of a deauth attack or an evil-twin kicking clients off.
    if(subtype == 0x0C || subtype == 0x0A) {
        g_deauths++;
        // Attribution: report the targeted BSSID (addr3) + channel, rate-limited
        // so a heavy flood can't saturate the UART.
        uint32_t now_da = millis();
        if(now_da - g_last_da >= 250) {
            g_last_da = now_da;
            const uint8_t* b = p + 16; // addr3 = BSSID
            Serial.printf(
                "DA,%02x%02x%02x%02x%02x%02x,%u\n",
                b[0], b[1], b[2], b[3], b[4], b[5], frame_channel);
        }
    }

    char ftype = 'O';
    const char* ssid = NULL;
    int ssid_len = 0;

    // Locate SSID element (tag 0) within tagged parameters.
    int tag_off = -1;
    if(subtype == 0x04) { // probe request
        ftype = 'P';
        tag_off = 24;
    } else if(subtype == 0x08) { // beacon
        ftype = 'B';
        tag_off = 36;
    } else if(subtype == 0x05) { // probe response
        ftype = 'R';
        tag_off = 36;
    }
    if(tag_off >= 0 && tag_off + 2 <= len && p[tag_off] == 0x00) {
        ssid_len = p[tag_off + 1];
        if(tag_off + 2 + ssid_len <= len) {
            ssid = (const char*)(p + tag_off + 2);
        } else {
            ssid_len = 0;
        }
    }

    int s_score = ssid ? ssid_score(ssid, ssid_len) : 0;
    bool oui_tx = oui_match(p + 10); // addr2 = transmitter
    bool oui_rx = oui_match(p + 4); // addr1 = receiver (silent station)
    bool is_probe = (ftype == 'P');
    bool wildcard = is_probe && (ssid_len == 0); // broadcast/wildcard probe

    int conf = 0;
    if(s_score == 3)
        conf = 3; // confirmed Flock SSID name
    else if(oui_tx && wildcard)
        conf = 3; // a Flock OUI spraying a wildcard probe is high-confidence
    else if((oui_tx || oui_rx) && is_probe)
        conf = 2; // OUI (sender or silent receiver) + probe behaviour
    else if(s_score == 2)
        conf = 2;
    else if(oui_tx || oui_rx)
        conf = 1; // OUI prefix only

    if(conf == 0) return; // not a candidate; drop to keep UART quiet

    // B1: fingerprint the probe body (MAC-independent device-class signature)
    // and coalesce MAC-cycling bursts via the 802.11 sequence-number run, so a
    // randomized-MAC spray collapses to one logical sighting before it can flood
    // the Flipper's 96-entry table. Only probe requests carry a meaningful IE
    // skeleton. The coalescer runs only on candidate frames so unrelated noise
    // can't capture the run slot and suppress a real detection.
    uint32_t ie_fp = 0;
    if(is_probe) {
        ie_fp = ie_skeleton_hash(p, len);
        // 802.11 sequence control: bytes 22-23, seq number is the top 12 bits.
        uint16_t seq = ((uint16_t)p[23] << 8 | p[22]) >> 4;
        if(seq_run_duplicate(ie_fp, seq)) return; // duplicate in an active burst
    }

    g_hits++;
    if(!g_scanning) return;

    // Report the Flock device's MAC: the transmitter if it matched, else the
    // silent receiver (addr1).
    const uint8_t* mac = oui_tx ? (p + 10) : (oui_rx ? (p + 4) : (p + 10));

    char macstr[13];
    snprintf(
        macstr,
        sizeof(macstr),
        "%02x%02x%02x%02x%02x%02x",
        mac[0],
        mac[1],
        mac[2],
        mac[3],
        mac[4],
        mac[5]);

    Serial.printf("D,%s,%d,%u,%c,%d,", macstr, pkt->rx_ctrl.rssi, frame_channel, ftype, conf);
    if(ssid && ssid_len > 0) emit_ssid(ssid, ssid_len);
    // B1: trailing IE-fingerprint field (probe requests only). Older parsers
    // ignore it; the Flipper matches it against a curated Flock IE-fp table.
    if(ie_fp != 0) Serial.printf(",fp=%08x", ie_fp);
    Serial.print('\n');
}

static void set_channel(uint8_t ch) {
    g_channel = ch;
    esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
}

static void start_promisc() {
    wifi_promiscuous_filter_t filter = {.filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT};
    esp_wifi_set_promiscuous_filter(&filter);
    esp_wifi_set_promiscuous_rx_cb(&promisc_cb);
    esp_wifi_set_promiscuous(true);
    set_channel(g_channel);
}

static void banner() {
    Serial.print("FLOCKCO,1\n");
}

// One-shot WiFi security scan for the FlipDeFlock audit. Switches out of
// promiscuous Flock mode, runs an active esp_wifi_scan (which yields the auth
// mode + ciphers + WPS that Marauder never emits over serial), streams one
// "W," line per AP, then restores Flock promiscuous mode.
//   W,<bssid>,<rssi>,<ch>,<authmode>,<pairwise>,<group>,<wps>,<ssid>
static void wifi_security_scan() {
    esp_wifi_set_promiscuous(false);
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_start();

    wifi_scan_config_t sc = {};
    sc.show_hidden = true;
    sc.scan_type = WIFI_SCAN_TYPE_ACTIVE;

    Serial.print("WBEGIN\n");
    uint16_t num = 0;
    if(esp_wifi_scan_start(&sc, true) == ESP_OK) {
        esp_wifi_scan_get_ap_num(&num);
        if(num > 64) num = 64;
        wifi_ap_record_t* recs =
            (wifi_ap_record_t*)malloc(sizeof(wifi_ap_record_t) * (num ? num : 1));
        if(recs) {
            uint16_t got = num;
            if(esp_wifi_scan_get_ap_records(&got, recs) == ESP_OK) {
                for(uint16_t i = 0; i < got; i++) {
                    wifi_ap_record_t* r = &recs[i];
                    char bss[13];
                    snprintf(
                        bss,
                        sizeof(bss),
                        "%02x%02x%02x%02x%02x%02x",
                        r->bssid[0], r->bssid[1], r->bssid[2],
                        r->bssid[3], r->bssid[4], r->bssid[5]);
                    Serial.printf(
                        "W,%s,%d,%u,%d,%d,%d,%d,",
                        bss, r->rssi, r->primary, (int)r->authmode,
                        (int)r->pairwise_cipher, (int)r->group_cipher, r->wps ? 1 : 0);
                    const char* s = (const char*)r->ssid;
                    for(int k = 0; k < 32 && s[k]; k++) {
                        char c = s[k];
                        if(c == ',' || c == '\r' || c == '\n' || (uint8_t)c < 0x20) c = '.';
                        Serial.write(c);
                    }
                    Serial.print('\n');
                }
            }
            free(recs);
        }
    }
    Serial.printf("WEND,%u\n", num);

    // Back to Flock detection.
    esp_wifi_set_mode(WIFI_MODE_NULL);
    start_promisc();
}

// One-shot BLE scan for the anti-tracker / BLE-Flock feature. Stops WiFi to free
// the radio, active-scans a few seconds, classifies each device, then restores
// WiFi/Flock mode.
//   BLE,<addr>,<rssi>,<cat>,<company>,<name>[,<mfghex>][,rv=1]
//   cat: 0 unknown  1 Flock/Raven  2 AirTag/FindMy  3 Tile  4 SmartTag
//   mfghex: raw manufacturer-specific data as hex (Flock 0x09C8 only), so the
//   Flipper can decode the device serial; trailing field, older parsers ignore.
//   rv=1: device exposed a Raven-specific GATT service (0x3100-0x3500) -> a
//   positive Raven (acoustic sensor) ID. Emitted AFTER mfghex when both apply;
//   contains '=' so the Flipper tells it apart from mfghex. Older parsers ignore.
static void ble_ensure_init() {
    if(g_ble_inited) return;
    BLEDevice::init("");
    g_ble = BLEDevice::getScan();
    g_ble->setActiveScan(true);
    g_ble->setInterval(80); // interval > window so BLE doesn't hog the radio
    g_ble->setWindow(60);
    g_ble_inited = true;
}

// Serialised BLE scan: toggles WiFi promiscuous OFF for the scan, then back ON
// (BLE stays resident). Classifies Flock/Raven by mfg id 0x09C8, device name
// (Penguin* / FS Ext Battery), Raven custom service UUIDs (0x3100-0x3500), or a
// Flock OUI on the BLE address; plus AirTag/Tile/SmartTag. Emits BBEGIN/BLE/BEND.
static void ble_do_scan(int seconds) {
    ble_ensure_init();
    esp_wifi_set_promiscuous(false);

    Serial.print("BBEGIN\n");
    BLEScanResults found = g_ble->start(seconds, false);
    int count = found.getCount();
    if(count > 80) count = 80;
    int spam = 0; // impersonation/pairing adverts -> BLE-spam flood indicator
    for(int i = 0; i < count; i++) {
        BLEAdvertisedDevice d = found.getDevice(i);

        int company = -1;
        int cat = 0;
        // raven: set when this device exposes a Raven-specific GATT service
        // (0x3100-0x3500). Tracked separately from cat because cat=1 also covers
        // the shared battery / Penguin / OUI cases -- only the GATT match is a
        // positive Raven (acoustic) ID, so we surface it as its own rv=1 field.
        bool raven = false;
        if(d.haveManufacturerData()) {
            std::string md = d.getManufacturerData();
            if(md.length() >= 2) company = (uint8_t)md[0] | ((uint8_t)md[1] << 8);
            if(company == 0x09C8)
                cat = 1; // Flock Safety / Raven
            else if(company == 0x004C && md.length() >= 3 && (uint8_t)md[2] == 0x12)
                cat = 2; // Apple Find My / AirTag
        }
        if(cat != 1 && d.haveName()) {
            std::string nm = d.getName();
            if(nm.rfind("Penguin", 0) == 0 || nm.find("FS Ext") != std::string::npos)
                cat = 1; // Flock Penguin battery / FS external battery
        }
        if(d.haveServiceUUID()) {
            std::string u = d.getServiceUUID().toString();
            if(u.find("00003100") != std::string::npos || u.find("00003200") != std::string::npos ||
               u.find("00003300") != std::string::npos || u.find("00003400") != std::string::npos ||
               u.find("00003500") != std::string::npos) {
                cat = 1; // Raven custom GATT services
                raven = true; // Raven-specific GATT -> positive acoustic-sensor ID
            }
            else if(cat == 0 && (u.find("feed") != std::string::npos || u.find("feec") != std::string::npos))
                cat = 3; // Tile
            else if(cat == 0 && u.find("fd5a") != std::string::npos)
                cat = 4; // Samsung SmartTag
            else if(cat == 0 && u.find("feaa") != std::string::npos)
                cat = 5; // Google Find My Device network (Pebblebee/Chipolo/Moto/Eufy)
        }
        if(cat == 0) {
            BLEAddress ba = d.getAddress();
            uint8_t* nat = *ba.getNative(); // getNative() is uint8_t(*)[6]
            if(nat && oui_match(nat)) cat = 1; // Flock OUI on the BLE address
        }

        // Apple/Tile/Samsung/Google pairing adverts are what BLE-spam tools
        // (Flipper "BLE spam", ESP32 sour-apple, etc.) impersonate in bulk. A few
        // are normal; a flood of them in one scan is the spam signature.
        if(cat == 2 || cat == 3 || cat == 4 || cat == 5) spam++;

        std::string a = d.getAddress().toString();
        char addr[13];
        int k = 0;
        for(size_t j = 0; j < a.size() && k < 12; j++) {
            if(a[j] != ':') addr[k++] = a[j];
        }
        addr[k] = 0;

        Serial.printf("BLE,%s,%d,%d,%d,", addr, d.getRSSI(), cat, company);
        if(d.haveName()) {
            std::string nm = d.getName();
            for(size_t j = 0; j < nm.size() && j < 32; j++) {
                char c = nm[j];
                if(c == ',' || c == '\r' || c == '\n' || (uint8_t)c < 0x20) c = '.';
                Serial.write(c);
            }
        }
        // Trailing field: raw mfg-data hex for Flock (0x09C8) only, so the
        // Flipper can decode the device serial. Capped so the line stays well
        // under the Flipper's RX line limit; only Flock units carry it.
        if(cat == 1 && company == 0x09C8 && d.haveManufacturerData()) {
            std::string md = d.getManufacturerData();
            Serial.write(',');
            for(size_t j = 0; j < md.length() && j < 31; j++) {
                Serial.printf("%02x", (uint8_t)md[j]);
            }
        }
        // Raven GATT flag, emitted LAST so it follows the optional mfghex field.
        // The '=' lets the Flipper distinguish it from the pure-hex mfghex token.
        if(raven) Serial.print(",rv=1");
        Serial.print('\n');
    }
    Serial.printf("BEND,%d\n", count);

    // A bulk of impersonation/pairing adverts in a single scan = a BLE-spam
    // flood (independent BLE radio class for the app's fused score).
    if(spam >= BLE_SPAM_MIN) Serial.printf("ATK,blespam,%d\n", spam);

    g_ble->clearResults();
    esp_wifi_set_promiscuous(true);
    set_channel(g_channel);
}

// Locator (BLE): one short scan; emit the target's RSSI if seen. Builds the same
// lowercased, colon-stripped toString() form the BLE line uses, so the comparison
// is byte-order-safe against the addr the app originally parsed.
static void ble_locate_scan() {
    ble_ensure_init();
    esp_wifi_set_promiscuous(false);
    BLEScanResults res = g_ble->start(1, false);
    int best = -127;
    int n = res.getCount();
    for(int i = 0; i < n; i++) {
        BLEAdvertisedDevice d = res.getDevice(i);
        std::string a = d.getAddress().toString();
        char addr[13];
        int k = 0;
        for(size_t j = 0; j < a.size() && k < 12; j++) {
            char c = a[j];
            if(c == ':') continue;
            addr[k++] = (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c;
        }
        addr[k] = 0;
        if(strcmp(addr, g_locate_macs) == 0) {
            int r = d.getRSSI();
            if(r > best) best = r;
        }
    }
    g_ble->clearResults();
    if(best > -127) Serial.printf("LOC,%d\n", best);
}

void setup() {
    Serial.begin(115200);
    delay(200);

    nvs_flash_init();
    esp_event_loop_create_default();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_wifi_set_storage(WIFI_STORAGE_RAM);
    esp_wifi_set_mode(WIFI_MODE_NULL);
    esp_wifi_start();
    start_promisc();

    banner();
}

static void handle_command(String cmd) {
    cmd.trim();
    // Any non-locate command ends Locator mode: unlock the Wi-Fi channel it
    // pinned, and restore promiscuous if BLE-locate had turned it off.
    if(!cmd.startsWith("locate")) {
        if(g_locate_kind == 'w') g_lock_channel = 0;
        if(g_locate_kind == 'b') {
            esp_wifi_set_promiscuous(true);
            set_channel(g_channel);
        }
        g_locate_kind = 0;
    }
    if(cmd == "scan") {
        g_scanning = true;
        g_combo = false; // pure WiFi Flock
    } else if(cmd == "stop") {
        g_scanning = false;
        g_combo = false; // also leave dual-band mode so the board goes idle
    } else if(cmd == "ver") {
        banner();
    } else if(cmd == "wifiscan") {
        wifi_security_scan();
    } else if(cmd == "blescan") {
        ble_do_scan(6);
    } else if(cmd == "flockcombo") {
        g_scanning = true;
        g_combo = true; // interleaved WiFi + BLE Flock detection
        g_phase_start = millis();
    } else if(cmd == "flockwifi") {
        g_combo = false;
    } else if(cmd.startsWith("ch ")) {
        int n = cmd.substring(3).toInt();
        if(n >= 1 && n <= 14) {
            g_lock_channel = n;
            set_channel(n);
        } else {
            g_lock_channel = 0;
        }
    } else if(cmd.startsWith("locate")) {
        // locate <w|b> <hexmac> [ch]   -> stream LOC,<rssi> for that target
        // locate off                   -> stop
        if(cmd.indexOf("off") > 0) {
            if(g_locate_kind == 'b') {
                esp_wifi_set_promiscuous(true);
                set_channel(g_channel);
            }
            g_locate_kind = 0;
            g_lock_channel = 0;
        } else {
            int s1 = cmd.indexOf(' ');
            int s2 = (s1 > 0) ? cmd.indexOf(' ', s1 + 1) : -1;
            int s3 = (s2 > 0) ? cmd.indexOf(' ', s2 + 1) : -1;
            if(s1 > 0 && s2 > s1) {
                char kind = cmd.charAt(s1 + 1);
                String macs = (s3 > s2) ? cmd.substring(s2 + 1, s3) : cmd.substring(s2 + 1);
                macs.toLowerCase();
                uint8_t mac[6];
                if(macs.length() >= 12 && parse_hexmac(macs.c_str(), mac)) {
                    memcpy(g_locate_mac, mac, 6);
                    strncpy(g_locate_macs, macs.c_str(), 12);
                    g_locate_macs[12] = 0;
                    g_locate_ch = (s3 > s2) ? (uint8_t)cmd.substring(s3 + 1).toInt() : 0;
                    g_locate_best = -127;
                    g_scanning = false; // Locator dedicates the radio to one target
                    g_combo = false;
                    g_locate_kind = (kind == 'b') ? 'b' : 'w';
                    if(g_locate_kind == 'w') {
                        esp_wifi_set_promiscuous(true);
                        if(g_locate_ch >= 1 && g_locate_ch <= 14) {
                            g_lock_channel = g_locate_ch;
                            set_channel(g_locate_ch);
                        } else {
                            g_lock_channel = 0;
                        }
                    }
                }
            }
        }
    }
}

void loop() {
    if(Serial.available()) {
        handle_command(Serial.readStringUntil('\n'));
    }

    uint32_t now = millis();

    // Locator mode owns the radio: stream the target's live RSSI as LOC lines.
    if(g_locate_kind == 'w') {
        if(now - g_last_loc >= 120) {
            g_last_loc = now;
            if(g_locate_best > -127) {
                Serial.printf("LOC,%d\n", g_locate_best);
                g_locate_best = -127; // reset window; quiet interval = no LOC (out of range)
            }
        }
        return;
    }
    if(g_locate_kind == 'b') {
        ble_locate_scan(); // ~1 s blocking scan; emits LOC if the target is seen
        return;
    }

    // Dual-band: after a WiFi-promiscuous phase, run a BLE scan phase, then
    // resume. The BLE scan blocks for a few seconds and restores promiscuous.
    if(g_combo && now - g_phase_start >= COMBO_WIFI_MS) {
        ble_do_scan(COMBO_BLE_SEC);
        g_phase_start = millis();
        return;
    }

    // When not scanning, stay idle (no channel hopping, no status TX) so the
    // board isn't "in use" after the app stops/exits.
    if(!g_scanning) return;

    // Channel hop every 300 ms unless locked.
    if(g_lock_channel == 0 && now - g_last_hop >= 300) {
        g_last_hop = now;
        uint8_t ch = g_channel + 1;
        if(ch > 11) ch = 1;
        set_channel(ch);
    }

    // Status heartbeat ~1 Hz. 4th field = deauth/disassoc frames in the LAST
    // interval (a rate, not a lifetime total) so the alert clears when a flood
    // stops. Older parsers ignore the extra field.
    if(now - g_last_status >= 1000) {
        g_last_status = now;
        uint32_t deauth_rate = g_deauths - g_deauths_last;
        g_deauths_last = g_deauths;
        Serial.printf("S,%u,%u,%u,%u\n", g_frames, g_hits, g_channel, deauth_rate);

        // Active attack-tool signatures for this interval, then reset the windows.
        if(g_probe_reqs >= PROBE_FLOOD_MIN) Serial.printf("ATK,probeflood,%u\n", g_probe_reqs);
        if(g_beacon_distinct >= BEACON_FLOOD_MIN)
            Serial.printf("ATK,beaconflood,%u\n", g_beacon_distinct);
        g_probe_reqs = 0;
        g_beacon_distinct = 0;
        g_beacon_ring_n = 0;
    }
}
