// Hotspot Arcade firmware for the ESP32-S2 WiFi dev board.
//
// Hosts an open WiFi AP + captive portal that serves a multiplayer game web app
// (streamed from the Flipper into RAM), and acts as the real-time referee over a
// WebSocket while the Flipper drives the session over UART v2. See docs/PROTOCOL.md.
//
// For education/fun on your own hardware. It runs an OPEN access point and a
// catch-all captive page; only operate it where that is allowed.

#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <DNSServer.h>
#include <esp_wifi.h>
#include <lwip/etharp.h>

#include "ha_proto.h"
#include "ha_json.h"
#include "ha_assets.h"
#include "ha_games.h"

#define WS_MSG_MAX 512
#define AP_MAX_CONN 8

static DNSServer dnsServer;
static AsyncWebServer server(80);
static AsyncWebSocket ws("/ws");
static IPAddress apIP(192, 168, 4, 1);
static char apName[33] = "Hotspot Arcade";
static bool portalRunning = false;
static uint8_t apMaxConn = AP_MAX_CONN;

static AssetStore assets;
static Engine engine;

// Serial (to the Flipper) is written from the loop task and the async web/WS
// task; serialize whole frames so bytes can't interleave. Engine state is also
// touched from both tasks, so guard it too.
static SemaphoreHandle_t serialMutex = nullptr;
static SemaphoreHandle_t engineMutex = nullptr;
#define ENGINE_LOCK() xSemaphoreTakeRecursive(engineMutex, portMAX_DELAY)
#define ENGINE_UNLOCK() xSemaphoreGiveRecursive(engineMutex)

// ---------------- device identity (one phone = one player) ----------------
//
// The engine keys a player on the phone rather than on the socket (see Player in
// ha_games.h), so that a second browser context -- iOS opens the portal in a captive
// mini-browser alongside Safari -- joins the player that phone already has instead of
// becoming a new one. The WebSocket layer only knows a peer IP, and that IP is
// something our own DHCP server made up, so resolve it to the station's MAC (the
// actual device identity) and hand the engine an opaque key built from that.
//
// Phones default to a randomized "private" MAC, but it is stable per SSID: it survives
// reconnects and only changes if the AP is renamed. Exactly the lifetime we need.
//
// The IP -> MAC mapping comes from the AP's own DHCP server: ip_event_ap_staipassigned_t
// carries the assigned address *and* the client MAC, so one event handler can keep a
// small table (the AP caps stations well below HA_STA_MAX). A station that got its lease
// before the handler was installed is missing from it; for those, read the MAC out of
// lwIP's ARP cache instead, and if even that misses, key on the IP.

#define HA_STA_MAX 10 // >= ESP_WIFI_MAX_CONN_NUM: the AP cannot hold more leases
#define HA_KEY_MAC 0x01 // key tag: low 48 bits are a station MAC
#define HA_KEY_IP 0x02 // key tag: low 32 bits are an IPv4 address

struct StaLease {
    uint32_t ip; // 0 = free slot
    uint8_t mac[6];
};
static StaLease staLeases[HA_STA_MAX];
// Written from the WiFi event task, read from the async WS task; the critical section
// is a handful of instructions over a 10-entry array.
static portMUX_TYPE leaseMux = portMUX_INITIALIZER_UNLOCKED;

static uint64_t macDeviceKey(const uint8_t* mac) {
    uint64_t v = 0;
    for(int i = 0; i < 6; i++) v = (v << 8) | mac[i];
    return ((uint64_t)HA_KEY_MAC << 56) | v;
}

static void leaseNote(uint32_t ip, const uint8_t* mac) {
    portENTER_CRITICAL(&leaseMux);
    int slot = -1, freeSlot = -1;
    for(int i = 0; i < HA_STA_MAX; i++) {
        if(!staLeases[i].ip) {
            if(freeSlot < 0) freeSlot = i;
        } else if(memcmp(staLeases[i].mac, mac, 6) == 0) {
            slot = i; // same phone, (re)leased
            break;
        } else if(staLeases[i].ip == ip) {
            slot = i; // this address now belongs to a different station
        }
    }
    if(slot < 0) slot = freeSlot >= 0 ? freeSlot : 0;
    staLeases[slot].ip = ip;
    memcpy(staLeases[slot].mac, mac, 6);
    portEXIT_CRITICAL(&leaseMux);
}

static void leaseForget(const uint8_t* mac) {
    portENTER_CRITICAL(&leaseMux);
    for(int i = 0; i < HA_STA_MAX; i++)
        if(staLeases[i].ip && memcmp(staLeases[i].mac, mac, 6) == 0) staLeases[i].ip = 0;
    portEXIT_CRITICAL(&leaseMux);
}

static void leasesClear() {
    portENTER_CRITICAL(&leaseMux);
    memset(staLeases, 0, sizeof(staLeases));
    portEXIT_CRITICAL(&leaseMux);
}

static bool leaseMac(uint32_t ip, uint8_t* out) {
    bool found = false;
    portENTER_CRITICAL(&leaseMux);
    for(int i = 0; i < HA_STA_MAX && !found; i++)
        if(staLeases[i].ip == ip) {
            memcpy(out, staLeases[i].mac, 6);
            found = true;
        }
    portEXIT_CRITICAL(&leaseMux);
    return found;
}

// The address a MAC currently holds, for the log line only (0 if unknown).
static uint32_t leaseIp(const uint8_t* mac) {
    uint32_t ip = 0;
    portENTER_CRITICAL(&leaseMux);
    for(int i = 0; i < HA_STA_MAX && !ip; i++)
        if(staLeases[i].ip && memcmp(staLeases[i].mac, mac, 6) == 0) ip = staLeases[i].ip;
    portEXIT_CRITICAL(&leaseMux);
    return ip;
}

// Fallback for a station whose lease we never saw. Read without the lwIP core lock:
// the ARP table is a fixed static array, so the worst case is reading a half-updated
// entry (a wrong MAC, i.e. one extra player) rather than a bad pointer.
static bool arpMac(uint32_t ip, uint8_t* out) {
    for(size_t i = 0; i < ARP_TABLE_SIZE; i++) {
        ip4_addr_t* eip = nullptr;
        struct netif* nif = nullptr;
        struct eth_addr* eth = nullptr;
        if(etharp_get_entry(i, &eip, &nif, &eth) && eip && eth && eip->addr == ip) {
            memcpy(out, eth->addr, 6);
            return true;
        }
    }
    return false;
}

// Which phone a socket is on, as the opaque key the engine stores. 0 = unknown, which
// makes the engine fall back to one player per connection rather than merging clients.
static uint64_t peerDeviceKey(AsyncWebSocketClient* client) {
    uint32_t ip = (uint32_t)client->remoteIP();
    if(!ip || ip == (uint32_t)apIP) return 0; // not a joined station
    uint8_t mac[6];
    if(leaseMac(ip, mac) || arpMac(ip, mac)) return macDeviceKey(mac);
    return ((uint64_t)HA_KEY_IP << 56) | ip; // last resort: the address itself
}

// Render a key for the serial log: "ip=.. mac=.." when we know both.
static String deviceKeyText(uint64_t key) {
    char b[64];
    if((uint8_t)(key >> 56) == HA_KEY_MAC) {
        uint8_t mac[6];
        for(int i = 0; i < 6; i++) mac[i] = (uint8_t)(key >> (40 - 8 * i));
        uint32_t ip = leaseIp(mac);
        String where = ip ? String("ip=") + IPAddress(ip).toString() + " " : String("");
        snprintf(b, sizeof(b), "mac=%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2],
                 mac[3], mac[4], mac[5]);
        return where + b;
    }
    if((uint8_t)(key >> 56) == HA_KEY_IP)
        return String("ip=") + IPAddress((uint32_t)(key & 0xFFFFFFFFu)).toString() + " mac=?";
    return String("device=unknown");
}

// ---------------- UART TX ----------------

static void uartSend(uint8_t type, const uint8_t* payload, size_t len) {
    if(len > HA_MAX_PAYLOAD) len = HA_MAX_PAYLOAD;
    uint8_t hdr[4] = {HA_SYNC, type, (uint8_t)(len & 0xFF), (uint8_t)(len >> 8)};
    uint8_t crc = ha_crc8_upd(0, type);
    crc = ha_crc8_upd(crc, hdr[2]);
    crc = ha_crc8_upd(crc, hdr[3]);
    for(size_t i = 0; i < len; i++) crc = ha_crc8_upd(crc, payload[i]);

    if(serialMutex) xSemaphoreTake(serialMutex, portMAX_DELAY);
    Serial.write(hdr, 4);
    if(len) Serial.write(payload, len);
    Serial.write(crc);
    if(serialMutex) xSemaphoreGive(serialMutex);
}

static void uartStatus(const char* token) {
    uartSend(HA_MSG_STATUS, (const uint8_t*)token, strlen(token));
}

// ---------------- sinks used by the engine ----------------

void haWsSendWs(uint32_t wsId, const String& msg) {
    if(!wsId) return;
    ws.text(wsId, msg);
}
void haWsBroadcast(const String& msg) {
    ws.textAll(msg);
}
void haUartJoin(uint8_t pid, const char* nick) {
    uint8_t buf[1 + HA_NICK_LEN + 1];
    buf[0] = pid;
    size_t n = strlen(nick);
    if(n > HA_NICK_LEN) n = HA_NICK_LEN;
    memcpy(buf + 1, nick, n);
    uartSend(HA_MSG_JOIN, buf, 1 + n);
}
void haUartLeave(uint8_t pid) {
    uartSend(HA_MSG_LEAVE, &pid, 1);
}
void haUartScore(uint8_t pid, int delta, const char* reason) {
    uint8_t buf[1 + 2 + 24];
    buf[0] = pid;
    int16_t d = (int16_t)delta;
    buf[1] = (uint8_t)(d & 0xFF);
    buf[2] = (uint8_t)((d >> 8) & 0xFF);
    size_t n = strlen(reason);
    if(n > 24) n = 24;
    memcpy(buf + 3, reason, n);
    uartSend(HA_MSG_SCORE, buf, 3 + n);
}
void haUartEvent(const String& json) {
    uartSend(HA_MSG_EVENT, (const uint8_t*)json.c_str(), json.length());
}
void haUartRoundResult(const String& json) {
    uartSend(HA_MSG_ROUND_RESULT, (const uint8_t*)json.c_str(), json.length());
}

// Debug trace of the engine's identity decisions. Serial is the Flipper link, so a
// log line shares the wire with the framed protocol: safe because the receiver only
// starts a frame on the SYNC byte (0xA5) and resyncs on a bad CRC, and because
// haLog() emits printable ASCII only -- a UTF-8 nickname could otherwise carry a
// literal 0xA5 and be mistaken for the start of a frame. Written under the same
// mutex as a frame, so it can never land inside one.
static void haLog(const String& line) {
    if(serialMutex) xSemaphoreTake(serialMutex, portMAX_DELAY);
    Serial.print("\n[ha] ");
    for(const char* p = line.c_str(); *p; p++)
        Serial.write((*p >= 0x20 && (uint8_t)*p < 0x7F) ? *p : '?');
    Serial.print("\n");
    if(serialMutex) xSemaphoreGive(serialMutex);
}
void haLogJoin(uint8_t pid, uint64_t deviceKey, const char* nick, bool consolidated) {
    String where = deviceKeyText(deviceKey);
    if(recognised)
        haLog(String("SAME DEVICE ") + where + " -> pid=" + pid + " nick=\"" + nick +
              "\" (recognised)");
    else
        haLog(String("JOIN pid=") + pid + " " + where + " nick=\"" + nick + "\"");
}

// ---------------- HTTP (captive) ----------------

// Serve the streamed web bundle for every host/path so the captive portal always
// resolves. GET "/" (and every OS captive-probe URL) gets the app; other stored
// asset paths are served by exact match.
class ArcadeHandler : public AsyncWebHandler {
public:
    bool canHandle(AsyncWebServerRequest* request) const override {
        (void)request;
        return true;
    }
    void handleRequest(AsyncWebServerRequest* request) override {
        String url = request->url();
        const Asset* a = assets.find(url.c_str());
        if(!a) a = assets.root(); // captive-detection URLs -> the app
        if(!a || !a->buf || a->len == 0) {
            request->send(200, "text/html", "<h1>Hotspot Arcade</h1><p>No bundle loaded.</p>");
            return;
        }
        AsyncWebServerResponse* res =
            request->beginResponse(200, a->mime, (const uint8_t*)a->buf, a->len);
        if(a->gzip) res->addHeader("Content-Encoding", "gzip");
        res->addHeader("Cache-Control", "no-store");
        request->send(res);
    }
};

// ---------------- WebSocket ----------------

static void onWsEvent(
    AsyncWebSocket* srv,
    AsyncWebSocketClient* client,
    AwsEventType type,
    void* arg,
    uint8_t* data,
    size_t len) {
    (void)srv;
    if(type == WS_EVT_DISCONNECT) {
        ENGINE_LOCK();
        engine.onWsDisconnect(client->id());
        ENGINE_UNLOCK();
    } else if(type == WS_EVT_DATA) {
        AwsFrameInfo* info = (AwsFrameInfo*)arg;
        if(info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT &&
           len < WS_MSG_MAX) {
            char buf[WS_MSG_MAX];
            memcpy(buf, data, len);
            buf[len] = '\0';
            ENGINE_LOCK();
            engine.onInput(client->id(), peerDeviceKey(client), buf);
            ENGINE_UNLOCK();
        }
    }
}

// ---------------- AP lifecycle ----------------

// The AP's DHCP server is where a station's IP and MAC are seen together. Registered
// once in setup(), before any AP comes up, so no lease is missed (see peerDeviceKey).
static void onWiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
    if(event == ARDUINO_EVENT_WIFI_AP_STAIPASSIGNED)
        leaseNote(info.wifi_ap_staipassigned.ip.addr, info.wifi_ap_staipassigned.mac);
    else if(event == ARDUINO_EVENT_WIFI_AP_STADISCONNECTED)
        leaseForget(info.wifi_ap_stadisconnected.mac);
}

static void startPortal() {
    leasesClear(); // a fresh session leases fresh addresses
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
    WiFi.softAP(apName, nullptr, 1, 0, apMaxConn); // open AP, up to apMaxConn stations
    delay(100);
    uartStatus("ap_ok");

    dnsServer.start(53, "*", apIP);
    ws.onEvent(onWsEvent);
    server.addHandler(&ws);
    server.addHandler(new ArcadeHandler()).setFilter(ON_AP_FILTER);
    server.begin();
    portalRunning = true;

    String up = String("up ip=") + WiFi.softAPIP().toString();
    uartStatus(up.c_str());
}

static void stopPortal() {
    if(portalRunning) {
        ws.closeAll();
        server.end();
        dnsServer.stop();
        WiFi.softAPdisconnect(true);
        portalRunning = false;
    }
    leasesClear();
    ENGINE_LOCK();
    engine.reset();
    ENGINE_UNLOCK();
    uartStatus("stopped");
}

// ---------------- UART RX (framed) ----------------

enum RxState { RX_SYNC, RX_TYPE, RX_LEN0, RX_LEN1, RX_PAYLOAD, RX_CRC, RX_RAW };
static RxState rxState = RX_SYNC;
static uint8_t rxType = 0;
static uint16_t rxLen = 0, rxIdx = 0;
static uint8_t rxCrc = 0;
static uint8_t rxBuf[HA_MAX_PAYLOAD + 1];

// FILE_BEGIN payload: flags(1) pathlen(1) path mimelen(1) mime total(4 LE)
static void handleFileBegin(const uint8_t* p, size_t len) {
    if(len < 3) return;
    size_t i = 0;
    uint8_t flags = p[i++];
    uint8_t pathlen = p[i++];
    if(i + pathlen + 1 > len) return;
    char path[HA_ASSET_PATH];
    size_t pl = pathlen < HA_ASSET_PATH - 1 ? pathlen : HA_ASSET_PATH - 1;
    memcpy(path, p + i, pl);
    path[pl] = '\0';
    i += pathlen;
    uint8_t mimelen = p[i++];
    if(i + mimelen + 4 > len) return;
    char mime[HA_ASSET_MIME];
    size_t ml = mimelen < HA_ASSET_MIME - 1 ? mimelen : HA_ASSET_MIME - 1;
    memcpy(mime, p + i, ml);
    mime[ml] = '\0';
    i += mimelen;
    uint32_t total = (uint32_t)p[i] | ((uint32_t)p[i + 1] << 8) | ((uint32_t)p[i + 2] << 16) |
                     ((uint32_t)p[i + 3] << 24);
    assets.begin(path, mime, flags & 1, total);
}

static void dispatchFrame() {
    rxBuf[rxLen] = '\0'; // JSON payloads are text; safe (buf has +1)
    switch(rxType) {
    case HA_MSG_CLEAR_FILES:
        assets.clear();
        uartStatus("cleared");
        break;
    case HA_MSG_FILE_BEGIN:
        handleFileBegin(rxBuf, rxLen);
        if(assets.receiving()) {
            rxState = RX_RAW; // the file bytes follow, unframed
            return;
        }
        uartStatus("fok"); // zero-length file committed immediately
        break;
    case HA_MSG_SET_AP:
        if(rxLen > 0) {
            size_t n = rxLen < sizeof(apName) - 1 ? rxLen : sizeof(apName) - 1;
            memcpy(apName, rxBuf, n);
            apName[n] = '\0';
        }
        uartStatus("ap_set");
        break;
    case HA_MSG_START:
        startPortal();
        break;
    case HA_MSG_STOP:
        stopPortal();
        break;
    case HA_MSG_RESET:
        uartStatus("resetting");
        delay(50);
        ESP.restart();
        break;
    case HA_MSG_SELECT_GAME:
        if(rxLen >= 1) {
            ENGINE_LOCK();
            engine.selectGame(rxBuf[0]);
            ENGINE_UNLOCK();
        }
        break;
    case HA_MSG_CONTENT_CLEAR:
        ENGINE_LOCK();
        engine.contentClear();
        ENGINE_UNLOCK();
        break;
    case HA_MSG_CONTENT_PACK:
        // payload: game byte, then the pack name (not NUL-terminated on the wire).
        if(rxLen >= 2) {
            char name[64];
            size_t n = rxLen - 1 < sizeof(name) - 1 ? (size_t)(rxLen - 1) : sizeof(name) - 1;
            memcpy(name, rxBuf + 1, n);
            name[n] = '\0';
            ENGINE_LOCK();
            engine.contentPack(rxBuf[0], name);
            ENGINE_UNLOCK();
        }
        break;
    case HA_MSG_CONTENT_ITEM:
        ENGINE_LOCK();
        engine.contentItem((const char*)rxBuf);
        ENGINE_UNLOCK();
        break;
    case HA_MSG_ROUND_END:
        ENGINE_LOCK();
        engine.roundEnd();
        ENGINE_UNLOCK();
        break;
    case HA_MSG_CONFIG: {
        int v;
        if(ha_json_int((const char*)rxBuf, "max", &v) && v >= 1 && v <= 15) apMaxConn = (uint8_t)v;
        char lang[8];
        if(ha_json_str((const char*)rxBuf, "lang", lang, sizeof(lang))) {
            ENGINE_LOCK();
            engine.setLang(lang);
            ENGINE_UNLOCK();
        }
        break;
    }
    case HA_MSG_RESET_SCORES:
        ENGINE_LOCK();
        engine.resetScores();
        ENGINE_UNLOCK();
        break;
    default:
        break;
    }
    rxState = RX_SYNC;
}

static void rxByte(uint8_t c) {
    switch(rxState) {
    case RX_SYNC:
        if(c == HA_SYNC) rxState = RX_TYPE;
        break;
    case RX_TYPE:
        rxType = c;
        rxCrc = ha_crc8_upd(0, c);
        rxState = RX_LEN0;
        break;
    case RX_LEN0:
        rxLen = c;
        rxCrc = ha_crc8_upd(rxCrc, c);
        rxState = RX_LEN1;
        break;
    case RX_LEN1:
        rxLen |= ((uint16_t)c << 8);
        rxCrc = ha_crc8_upd(rxCrc, c);
        if(rxLen > HA_MAX_PAYLOAD) {
            rxState = RX_SYNC; // bogus length, resync
            break;
        }
        rxIdx = 0;
        rxState = rxLen ? RX_PAYLOAD : RX_CRC;
        break;
    case RX_PAYLOAD:
        rxBuf[rxIdx++] = c;
        rxCrc = ha_crc8_upd(rxCrc, c);
        if(rxIdx >= rxLen) rxState = RX_CRC;
        break;
    case RX_CRC:
        if(c == rxCrc) {
            dispatchFrame(); // sets next state (RX_SYNC or RX_RAW)
        } else {
            rxState = RX_SYNC; // corrupt frame, drop and resync
        }
        break;
    case RX_RAW:
        break; // handled in bulk below
    }
}

static void pumpSerial() {
    while(Serial.available()) {
        if(rxState == RX_RAW) {
            // Bulk asset bytes: drain as many as we still need in one go.
            size_t need = assets.remaining();
            if(need == 0) {
                rxState = RX_SYNC;
                uartStatus("fok");
                continue;
            }
            uint8_t tmp[256];
            size_t want = need < sizeof(tmp) ? need : sizeof(tmp);
            int got = Serial.read(tmp, want);
            if(got > 0) assets.feed(tmp, (size_t)got);
            if(assets.remaining() == 0) {
                rxState = RX_SYNC;
                uartStatus("fok");
            }
        } else {
            rxByte((uint8_t)Serial.read());
        }
    }
}

// ---------------- Arduino entry ----------------

void setup() {
    serialMutex = xSemaphoreCreateMutex();
    engineMutex = xSemaphoreCreateRecursiveMutex();
    WiFi.onEvent(onWiFiEvent, ARDUINO_EVENT_WIFI_AP_STAIPASSIGNED);
    WiFi.onEvent(onWiFiEvent, ARDUINO_EVENT_WIFI_AP_STADISCONNECTED);
    Serial.setRxBufferSize(4096);
    Serial.begin(HA_UART_BAUD);
    delay(100);
    engine.reset();
    uartStatus("boot");
}

void loop() {
    if(portalRunning) {
        dnsServer.processNextRequest();
        ws.cleanupClients();
        ENGINE_LOCK();
        engine.tick(millis());
        ENGINE_UNLOCK();
    }
    pumpSerial();

    static uint32_t lastPing = 0;
    uint32_t now = millis();
    if(now - lastPing >= 2000) {
        lastPing = now;
        // Identity beacon: magic + version, so the Flipper knows it's our firmware
        // (and which version), not a different project on the same board.
        uint8_t beacon[6] = {
            HA_FW_MAGIC_0,
            HA_FW_MAGIC_1,
            HA_FW_MAGIC_2,
            HA_FW_MAGIC_3,
            (uint8_t)(HA_FW_VERSION & 0xFF),
            (uint8_t)(HA_FW_VERSION >> 8)};
        uartSend(HA_MSG_PING, beacon, sizeof(beacon));
    }
}
