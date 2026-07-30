/*
 * wol-flipper companion firmware
 *
 * Turns the Flipper WiFi dev board into a Wake-on-LAN transmitter driven over
 * UART0 (115200 8N1) by the wol_flipper app.
 *
 * Line protocol, fields separated by tabs, lines terminated by \n:
 *
 *   PING
 *     -> +WOLFW <version>
 *     -> OK
 *
 *   STATUS
 *     -> +WIFI <ssid|-> <ip|->
 *     -> OK
 *
 *   JOIN <ssid> <pass>
 *     -> +WIFI CONNECTING
 *     -> +WIFI OK <ip>
 *     -> OK
 *     or
 *     -> ERR <ARGS|WIFI>
 *
 *   WOL <ssid> <pass> <mac> <broadcast-ip> <port>
 *     -> +WIFI CONNECTING          (only when an association has to be made)
 *     -> +WIFI OK <ip>
 *     -> +SEND <count>
 *     -> OK
 *     or
 *     -> ERR <ARGS|WIFI|UDP>
 *
 * The board keeps no credentials of its own: everything arrives with the
 * request, so a flash backup taken from this firmware never contains secrets.
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>

#define WOL_FW_VERSION   1
#define LINE_MAX         320
#define MAX_FIELDS       8
#define WIFI_TIMEOUT_MS  20000
#define WOL_PACKET_SIZE  102
#define WOL_REPEAT       3
#define UDP_LOCAL_PORT   40000

static WiFiUDP udp;
static char line[LINE_MAX];
static size_t line_len = 0;
static bool udp_started = false;

static bool parse_hex_byte(const char* s, uint8_t* out) {
    uint8_t value = 0;
    for(size_t i = 0; i < 2; i++) {
        char c = s[i];
        uint8_t digit;
        if(c >= '0' && c <= '9') {
            digit = c - '0';
        } else if(c >= 'a' && c <= 'f') {
            digit = c - 'a' + 10;
        } else if(c >= 'A' && c <= 'F') {
            digit = c - 'A' + 10;
        } else {
            return false;
        }
        value = (value << 4) | digit;
    }
    *out = value;
    return true;
}

/** Accepts AA:BB:CC:DD:EE:FF, AA-BB-..., or AABBCCDDEEFF. */
static bool parse_mac(const char* s, uint8_t* mac) {
    for(size_t i = 0; i < 6; i++) {
        if(!parse_hex_byte(s, &mac[i])) return false;
        s += 2;
        if(i < 5 && (*s == ':' || *s == '-')) s++;
    }
    return *s == '\0';
}

/** Split in place on tabs. Returns the field count. */
static size_t split_fields(char* buf, char** fields, size_t max_fields) {
    size_t count = 0;
    char* cursor = buf;

    while(count < max_fields) {
        fields[count++] = cursor;
        char* tab = strchr(cursor, '\t');
        if(!tab) break;
        *tab = '\0';
        cursor = tab + 1;
    }
    return count;
}

static bool wifi_up(const char* ssid, const char* pass) {
    if(WiFi.status() == WL_CONNECTED && WiFi.SSID() == String(ssid)) return true;

    Serial.println("+WIFI CONNECTING");

    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    WiFi.disconnect(false, true);
    WiFi.begin(ssid, pass);

    uint32_t started = millis();
    while(WiFi.status() != WL_CONNECTED) {
        if(millis() - started > WIFI_TIMEOUT_MS) return false;
        delay(100);
    }

    if(!udp_started) {
        // WiFiUDP::begin() is what enables SO_BROADCAST on the socket
        udp_started = udp.begin(UDP_LOCAL_PORT);
    }
    return udp_started;
}

static bool send_magic_packet(const uint8_t* mac, IPAddress target, uint16_t port) {
    uint8_t packet[WOL_PACKET_SIZE];
    memset(packet, 0xFF, 6);
    for(size_t i = 0; i < 16; i++) {
        memcpy(packet + 6 + i * 6, mac, 6);
    }

    size_t sent = 0;
    for(size_t i = 0; i < WOL_REPEAT; i++) {
        if(udp.beginPacket(target, port) != 1) continue;
        udp.write(packet, sizeof(packet));
        if(udp.endPacket() == 1) sent++;
        delay(100);
    }

    Serial.printf("+SEND %u\n", (unsigned)sent);
    return sent > 0;
}

static void handle_ping(void) {
    Serial.printf("+WOLFW %u\n", (unsigned)WOL_FW_VERSION);
    Serial.println("OK");
}

static void handle_status(void) {
    if(WiFi.status() == WL_CONNECTED) {
        Serial.printf("+WIFI %s %s\n", WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
    } else {
        Serial.println("+WIFI - -");
    }
    Serial.println("OK");
}

static void handle_join(char** fields, size_t count) {
    if(count < 3 || fields[1][0] == '\0') {
        Serial.println("ERR ARGS");
        return;
    }

    if(!wifi_up(fields[1], fields[2])) {
        Serial.println("ERR WIFI");
        return;
    }

    Serial.printf("+WIFI OK %s\n", WiFi.localIP().toString().c_str());
    Serial.println("OK");
}

static void handle_wol(char** fields, size_t count) {
    if(count < 6) {
        Serial.println("ERR ARGS");
        return;
    }

    const char* ssid = fields[1];
    const char* pass = fields[2];
    uint8_t mac[6];
    IPAddress target;
    long port = strtol(fields[5], nullptr, 10);

    if(ssid[0] == '\0' || !parse_mac(fields[3], mac) || !target.fromString(fields[4]) ||
       port < 0 || port > 65535) {
        Serial.println("ERR ARGS");
        return;
    }

    if(!wifi_up(ssid, pass)) {
        Serial.println("ERR WIFI");
        return;
    }
    Serial.printf("+WIFI OK %s\n", WiFi.localIP().toString().c_str());

    if(!send_magic_packet(mac, target, (uint16_t)port)) {
        Serial.println("ERR UDP");
        return;
    }
    Serial.println("OK");
}

static void handle_line(char* buf) {
    char* fields[MAX_FIELDS];
    size_t count = split_fields(buf, fields, MAX_FIELDS);

    if(count == 0 || fields[0][0] == '\0') return;

    if(!strcmp(fields[0], "PING")) {
        handle_ping();
    } else if(!strcmp(fields[0], "STATUS")) {
        handle_status();
    } else if(!strcmp(fields[0], "JOIN")) {
        handle_join(fields, count);
    } else if(!strcmp(fields[0], "WOL")) {
        handle_wol(fields, count);
    } else {
        Serial.println("ERR CMD");
    }
}

void setup() {
    Serial.begin(115200);
    WiFi.mode(WIFI_STA);
    WiFi.persistent(false);
    delay(50);
    Serial.printf("+WOLFW %u ready\n", (unsigned)WOL_FW_VERSION);
}

void loop() {
    while(Serial.available()) {
        char c = (char)Serial.read();

        if(c == '\r') continue;
        if(c == '\n') {
            line[line_len] = '\0';
            handle_line(line);
            line_len = 0;
            continue;
        }

        if(line_len < LINE_MAX - 1) {
            line[line_len++] = c;
        } else {
            // overlong line, drop it rather than truncate into a bogus command
            line_len = 0;
        }
    }
    delay(2);
}
