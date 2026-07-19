/*
 * Argus - ESP32 companion firmware
 * --------------------------------
 * Puts the ESP32 Wi-Fi radio into promiscuous (monitor) mode, watches 802.11
 * management frames and streams two things to the Flipper Zero over UART:
 *   - deauthentication / disassociation frames (an active attack)
 *   - beacons / probe-responses (so the Flipper can spot evil-twin APs)
 *
 * Hardware : Flipper Zero WiFi devboard (ESP32-S2) or any ESP32 dev board.
 * Wiring   : ESP32 UART0  ->  Flipper GPIO  (TX=pin13, RX=pin14, GND=GND), 115200.
 *            (On the official devboard this is already bridged - just plug it in.)
 *
 * Wire protocol (see helpers/uart_link.h on the Flipper side):
 *   ESP32 -> Flipper:
 *     AXHELLO,<version>                                          on boot / PING
 *     AXD,<src>,<bssid>,<ch>,<rssi>,<reason>,<kind>             deauth(0)/disassoc(1)
 *     AXAP,<bssid>,<ch>,<rssi>,<enc>,<ssid>                     an AP beacon/probe-resp
 *   Flipper -> ESP32:
 *     START | STOP | CHAN:<0-13> | GUARD:<ssid> | PING
 *
 * enc codes: 0 Open, 1 WEP, 2 WPA, 3 WPA2, 4 WPA3.
 */

#include <Arduino.h>
#include "esp_wifi.h"

#define ARGUS_FW_VERSION "1.0"

#define HOP_INTERVAL_MS 250
#define AP_REPEAT_MS    2000 // don't re-announce the same BSSID faster than this
#define AP_CACHE_SIZE   48

static bool sniffing = false;
static bool hop_mode = true; // true = cycle channels, false = locked
static uint8_t locked_channel = 1;
static uint8_t cur_channel = 1;
static unsigned long last_hop = 0;

// recently-announced APs, so beacons don't flood the UART
struct ApSeen {
  uint8_t bssid[6];
  unsigned long last_ms;
};
static ApSeen ap_cache[AP_CACHE_SIZE];
static int ap_cache_len = 0;

/* ---- 802.11 framing ---- */
typedef struct {
  uint8_t payload[0];
} wifi_ieee80211_packet_t;

static bool mac_eq(const uint8_t* a, const uint8_t* b) {
  return memcmp(a, b, 6) == 0;
}

static void mac_to_hex(const uint8_t* m, char* out) {
  static const char* H = "0123456789ABCDEF";
  for (int i = 0; i < 6; i++) {
    out[i * 2] = H[m[i] >> 4];
    out[i * 2 + 1] = H[m[i] & 0xF];
  }
  out[12] = '\0';
}

// returns true if this BSSID may be announced again now (and records it)
static bool ap_should_emit(const uint8_t* bssid) {
  unsigned long now = millis();
  for (int i = 0; i < ap_cache_len; i++) {
    if (mac_eq(ap_cache[i].bssid, bssid)) {
      if (now - ap_cache[i].last_ms < AP_REPEAT_MS) return false;
      ap_cache[i].last_ms = now;
      return true;
    }
  }
  // new BSSID
  int slot;
  if (ap_cache_len < AP_CACHE_SIZE) {
    slot = ap_cache_len++;
  } else {
    // evict the oldest
    slot = 0;
    for (int i = 1; i < ap_cache_len; i++)
      if (ap_cache[i].last_ms < ap_cache[slot].last_ms) slot = i;
  }
  memcpy(ap_cache[slot].bssid, bssid, 6);
  ap_cache[slot].last_ms = now;
  return true;
}

// classify encryption from the tagged params of a beacon/probe-resp body
static int classify_enc(const uint8_t* body, int body_len, uint16_t caps) {
  bool privacy = caps & 0x0010;
  bool rsn = false, wpa1 = false, wpa3 = false;

  int i = 0;
  while (i + 2 <= body_len) {
    uint8_t id = body[i];
    uint8_t len = body[i + 1];
    const uint8_t* d = body + i + 2;
    if (i + 2 + len > body_len) break;

    if (id == 48) { // RSN -> WPA2/WPA3
      rsn = true;
      // version(2) group(4) pairCnt(2) pair(4*n) akmCnt(2) akm(4*m)
      if (len >= 8) {
        int p = 2 + 4;
        if (p + 2 <= len) {
          uint16_t pair_cnt = d[p] | (d[p + 1] << 8);
          p += 2 + 4 * pair_cnt;
          if (p + 2 <= len) {
            uint16_t akm_cnt = d[p] | (d[p + 1] << 8);
            p += 2;
            for (uint16_t a = 0; a < akm_cnt && p + 4 <= len; a++, p += 4) {
              // 00-0F-AC suite, type at d[p+3]; 8 = SAE (WPA3)
              if (d[p + 3] == 8) wpa3 = true;
            }
          }
        }
      }
    } else if (id == 221 && len >= 4) { // vendor; Microsoft WPA1 = 00:50:F2 type 1
      if (d[0] == 0x00 && d[1] == 0x50 && d[2] == 0xF2 && d[3] == 0x01) wpa1 = true;
    }
    i += 2 + len;
  }

  if (rsn) return wpa3 ? 4 : 3; // WPA3 : WPA2
  if (wpa1) return 2; // WPA
  if (privacy) return 1; // WEP
  return 0; // Open
}

static void sniffer_cb(void* buf, wifi_promiscuous_pkt_type_t type) {
  if (type != WIFI_PKT_MGMT) return;
  const wifi_promiscuous_pkt_t* pkt = (wifi_promiscuous_pkt_t*)buf;
  const uint8_t* p = pkt->payload;
  int len = pkt->rx_ctrl.sig_len;
  if (len < 24) return;

  uint16_t fc = p[0] | (p[1] << 8);
  uint8_t ftype = (fc >> 2) & 0x3;
  uint8_t fsub = (fc >> 4) & 0xF;
  if (ftype != 0) return; // management only

  const uint8_t* dst = p + 4;
  const uint8_t* src = p + 10;
  const uint8_t* bssid = p + 16;
  int8_t rssi = pkt->rx_ctrl.rssi;
  uint8_t ch = pkt->rx_ctrl.channel;
  (void)dst;

  char bssid_hex[13];
  mac_to_hex(bssid, bssid_hex);

  if (fsub == 12 || fsub == 10) { // deauth / disassoc
    uint8_t reason = 0;
    if (len >= 26) reason = p[24]; // reason code (LSB)
    char src_hex[13];
    mac_to_hex(src, src_hex);
    int kind = (fsub == 10) ? 1 : 0;
    Serial.printf("AXD,%s,%s,%u,%d,%u,%d\n", src_hex, bssid_hex, ch, rssi, reason, kind);

  } else if (fsub == 8 || fsub == 5) { // beacon / probe-response
    if (!ap_should_emit(bssid)) return;
    if (len < 36) return;
    uint16_t caps = p[34] | (p[35] << 8);
    const uint8_t* body = p + 36; // tagged params
    int body_len = len - 36;

    // SSID is tag 0
    char ssid[33] = {0};
    if (body_len >= 2 && body[0] == 0) {
      int sl = body[1];
      if (sl > 32) sl = 32;
      for (int k = 0; k < sl; k++) {
        char c = (char)body[2 + k];
        ssid[k] = (c == ',' || c == '\n' || c == '\r' || c < 32) ? '_' : c;
      }
      ssid[sl] = '\0';
    }

    int enc = classify_enc(body, body_len, caps);
    Serial.printf("AXAP,%s,%u,%d,%d,%s\n", bssid_hex, ch, rssi, enc, ssid);
  }
}

static void start_sniffer() {
  wifi_promiscuous_filter_t filter = {.filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT};
  esp_wifi_set_promiscuous_filter(&filter);
  esp_wifi_set_promiscuous_rx_cb(&sniffer_cb);
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(cur_channel, WIFI_SECOND_CHAN_NONE);
  sniffing = true;
}

static void stop_sniffer() {
  esp_wifi_set_promiscuous(false);
  sniffing = false;
}

static void apply_channel() {
  if (sniffing) esp_wifi_set_channel(cur_channel, WIFI_SECOND_CHAN_NONE);
}

void setup() {
  Serial.begin(115200);
  delay(200);

  // bring the Wi-Fi radio up in NULL mode (no AP/STA) for promiscuous capture
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  esp_wifi_init(&cfg);
  esp_wifi_set_storage(WIFI_STORAGE_RAM);
  esp_wifi_set_mode(WIFI_MODE_NULL);
  esp_wifi_start();

  Serial.printf("AXHELLO,%s\n", ARGUS_FW_VERSION);
  start_sniffer(); // auto-arm; the Flipper can STOP/START on demand
}

static void handle_command(String cmd) {
  cmd.trim();
  if (cmd == "START") {
    if (!sniffing) start_sniffer();
  } else if (cmd == "STOP") {
    if (sniffing) stop_sniffer();
  } else if (cmd == "PING") {
    Serial.printf("AXHELLO,%s\n", ARGUS_FW_VERSION);
  } else if (cmd.startsWith("CHAN:")) {
    int c = cmd.substring(5).toInt();
    if (c <= 0) {
      hop_mode = true;
    } else {
      hop_mode = false;
      locked_channel = (c > 13) ? 13 : c;
      cur_channel = locked_channel;
      apply_channel();
    }
  } else if (cmd.startsWith("GUARD:")) {
    // stored for context; analysis happens on the Flipper. Re-announce presence.
    Serial.printf("AXHELLO,%s\n", ARGUS_FW_VERSION);
  }
}

void loop() {
  if (Serial.available()) {
    handle_command(Serial.readStringUntil('\n'));
  }

  if (sniffing && hop_mode && (millis() - last_hop >= HOP_INTERVAL_MS)) {
    last_hop = millis();
    cur_channel++;
    if (cur_channel > 13) cur_channel = 1;
    esp_wifi_set_channel(cur_channel, WIFI_SECOND_CHAN_NONE);
  }

  delay(2);
}
