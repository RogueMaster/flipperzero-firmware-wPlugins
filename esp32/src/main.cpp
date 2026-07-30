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
 *   SCAN
 *     -> +AP <rssi> <ssid>          (one line per network, up to 24)
 *     -> OK
 *
 *   JOIN <ssid> <pass>
 *     -> +WIFI CONNECTING
 *     -> +WIFI OK <ip>
 *     -> OK
 *     or
 *     -> ERR ARGS, or ERR WIFI <reason code>
 *
 *   WOL <ssid> <pass> <mac> <broadcast-ip> <port>
 *     -> +WIFI CONNECTING          (only when an association has to be made)
 *     -> +WIFI OK <ip>
 *     -> +SEND <count>
 *     -> OK
 *     or
 *     -> ERR ARGS, ERR UDP, or ERR WIFI <reason code>
 *
 * The board keeps no credentials of its own: everything arrives with the
 * request, so a flash backup taken from this firmware never contains secrets.
 *
 * LED, because otherwise a working board is indistinguishable from a dead one:
 *
 *   red, green, blue    boot self test, one second each
 *   green blip every 3s idle and running
 *   pulsing blue        a command is being handled
 *   one green blink     command succeeded
 *   three red blinks    command failed
 *
 * Everything runs at a low PWM duty, see LED_LEVEL. Note that the board is
 * powered from the Flipper's 5V rail, which the app switches off when it
 * exits, so leaving the app kills the LED with it.
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>

#define WOL_FW_VERSION   9
#define LINE_MAX         320
#define MAX_FIELDS       8
#define WIFI_TIMEOUT_MS  20000
#define WOL_PACKET_SIZE  102
#define WOL_REPEAT       3
#define UDP_LOCAL_PORT   40000

/*
 * Discrete RGB LED, same pins Marauder uses for its MARAUDER_FLIPPER target.
 * The polarity of the part is not documented anywhere I could check, so every
 * signal below is a blink rather than a steady level: an inverted LED still
 * blinks, and the only question worth answering here is whether the firmware
 * is alive at all.
 */
#define LED_B_PIN 4
#define LED_G_PIN 5
#define LED_R_PIN 6

/*
 * Driven through LEDC rather than digitalWrite: at full duty this LED is
 * blinding in a dark room. Duty is out of 255; raise LED_LEVEL if the signals
 * are hard to see in daylight.
 */
#define LED_CH_R     0
#define LED_CH_G     1
#define LED_CH_B     2
#define LED_PWM_FREQ 2000
#define LED_PWM_BITS 8
#define LED_LEVEL    10
#define LED_MAX      ((1 << LED_PWM_BITS) - 1)

/*
 * Common anode part: pulling the pin low is what lights the die. Getting this
 * backwards is not subtle. Every state ends up within 4 percent of full
 * brightness, so the board glows constantly and the self test looks like
 * nothing is happening at all, which is exactly how this was found.
 */
#define LED_ACTIVE_LOW 1

static inline uint32_t led_duty(bool on) {
#if LED_ACTIVE_LOW
    return on ? (LED_MAX - LED_LEVEL) : LED_MAX;
#else
    return on ? LED_LEVEL : 0;
#endif
}

#define HEARTBEAT_PERIOD_MS 3000
#define HEARTBEAT_BLIP_MS   60
#define BUSY_PULSE_MS       250

static WiFiUDP udp;
static volatile uint8_t last_wifi_reason = 0;
static char line[LINE_MAX];
static size_t line_len = 0;
static bool udp_started = false;
static uint32_t last_heartbeat = 0;

static void led_init(void) {
    ledcSetup(LED_CH_R, LED_PWM_FREQ, LED_PWM_BITS);
    ledcSetup(LED_CH_G, LED_PWM_FREQ, LED_PWM_BITS);
    ledcSetup(LED_CH_B, LED_PWM_FREQ, LED_PWM_BITS);
    ledcAttachPin(LED_R_PIN, LED_CH_R);
    ledcAttachPin(LED_G_PIN, LED_CH_G);
    ledcAttachPin(LED_B_PIN, LED_CH_B);
}

static void led_set(bool red, bool green, bool blue) {
    ledcWrite(LED_CH_R, led_duty(red));
    ledcWrite(LED_CH_G, led_duty(green));
    ledcWrite(LED_CH_B, led_duty(blue));
}

static void led_off(void) {
    led_set(false, false, false);
}

/**
 * One second of each primary at boot, then dark. What actually appears
 * identifies the part:
 *
 *   red, green, blue, then dark   pins and polarity are right
 *   cyan, magenta, yellow, white  common anode, set LED_ACTIVE_LOW to 1
 *   nothing changes               these are not the pins, or that light is
 *                                 the board's own power indicator
 */
static void led_self_test(void) {
    led_set(true, false, false);
    delay(800);
    led_set(false, true, false);
    delay(800);
    led_set(false, false, true);
    delay(800);
    led_off();
}

static void led_blink(bool red, bool green, bool blue, uint8_t times, uint16_t period_ms) {
    for(uint8_t i = 0; i < times; i++) {
        led_set(red, green, blue);
        delay(period_ms / 2);
        led_off();
        delay(period_ms / 2);
    }
}

/**
 * Slow blue pulse for the blocking parts of a command. Joining an AP can take
 * twenty seconds, and a steady light through all of it reads as a hang, with
 * the eventual switch off looking like the board died.
 */
static void led_busy_tick(void) {
    static uint32_t last = 0;
    static bool on = false;

    if(millis() - last < BUSY_PULSE_MS) return;
    last = millis();
    on = !on;
    led_set(false, false, on);
}

/** Short green blip so an idle board still proves it is running. */
static void led_heartbeat(void) {
    if(millis() - last_heartbeat < HEARTBEAT_PERIOD_MS) return;
    last_heartbeat = millis();

    led_set(false, true, false);
    delay(HEARTBEAT_BLIP_MS);
    led_off();
}

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

/** Keep the last disconnect reason so a failure can say what went wrong. */
static void wifi_event(WiFiEvent_t event, WiFiEventInfo_t info) {
    if(event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
        last_wifi_reason = info.wifi_sta_disconnected.reason;
    }
}

static bool wifi_up(const char* ssid, const char* pass) {
    if(WiFi.status() == WL_CONNECTED && WiFi.SSID() == String(ssid)) return true;

    Serial.println("+WIFI CONNECTING");

    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);
    /* The board runs off the Flipper's 5V boost, which does not have much
     * headroom. Full transmit power draws enough on association to brown the
     * rail out; 11 dBm is plenty for reaching an AP in the same flat, and
     * modem sleep stays on to keep the average down. */
    WiFi.setTxPower(WIFI_POWER_11dBm);
    WiFi.disconnect(false, true);
    last_wifi_reason = 0;
    WiFi.begin(ssid, pass);

    uint32_t started = millis();
    while(WiFi.status() != WL_CONNECTED) {
        if(millis() - started > WIFI_TIMEOUT_MS) return false;
        led_busy_tick();
        delay(100);
    }

    if(!udp_started) {
        // WiFiUDP::begin() is what enables SO_BROADCAST on the socket
        udp_started = udp.begin(UDP_LOCAL_PORT);
    }
    return udp_started;
}

static bool send_one(IPAddress dst, uint16_t port, const uint8_t* packet, size_t len) {
    if(udp.beginPacket(dst, port) != 1) return false;
    udp.write(packet, len);
    return udp.endPacket() == 1;
}

static bool send_magic_packet(const uint8_t* mac, IPAddress target, uint16_t port) {
    uint8_t packet[WOL_PACKET_SIZE];
    memset(packet, 0xFF, 6);
    for(size_t i = 0; i < 16; i++) {
        memcpy(packet + 6 + i * 6, mac, 6);
    }

    /* wifi_up() returns early when the association is already there, so the
     * socket may never have been opened. beginPacket() would make one anyway,
     * but then nothing is bound to a local port. */
    if(!udp_started) udp_started = udp.begin(UDP_LOCAL_PORT);

    /*
     * Aim at the configured address and at the subnet directed broadcast.
     * The packet leaves from a Wi-Fi client, and access points differ in which
     * of the two they bridge onto the wired segment: 255.255.255.255 is
     * commonly dropped while 192.168.x.255 goes through. A WoL sender running
     * on a wired PC never runs into this, which is why the same MAC and port
     * can work there and do nothing from here.
     */
    IPAddress destinations[2];
    size_t count = 0;
    destinations[count++] = target;

    IPAddress local = WiFi.localIP();
    IPAddress mask = WiFi.subnetMask();
    if((uint32_t)mask != 0) {
        IPAddress directed(
            local[0] | (uint8_t)~mask[0],
            local[1] | (uint8_t)~mask[1],
            local[2] | (uint8_t)~mask[2],
            local[3] | (uint8_t)~mask[3]);
        if(directed != target) destinations[count++] = directed;
    }

    size_t ok = 0;
    size_t attempts = 0;
    for(size_t i = 0; i < WOL_REPEAT; i++) {
        for(size_t d = 0; d < count; d++) {
            attempts++;
            if(send_one(destinations[d], port, packet, sizeof(packet))) ok++;
            delay(40);
        }
    }

    Serial.printf("+SEND %u/%u %s", (unsigned)ok, (unsigned)attempts, destinations[0].toString().c_str());
    if(count > 1) Serial.printf(" %s", destinations[1].toString().c_str());
    Serial.printf(" port %u\n", (unsigned)port);

    return ok > 0;
}

static void reply_ok(void) {
    Serial.println("OK");
    led_blink(false, true, false, 1, 250);
}

static void reply_err(const char* reason) {
    Serial.print("ERR ");
    Serial.println(reason);
    led_blink(true, false, false, 3, 140);
}

/**
 * Report why the association failed, not just that it did. The reason comes
 * from the disconnect event; esp_wifi_types.h numbers them, the ones that
 * matter here are 201 no AP found, 202 auth failure and 15/204 handshake
 * timeout, which in practice all mean a wrong password.
 */
static void reply_wifi_err(void) {
    char reason[24];
    snprintf(reason, sizeof(reason), "WIFI %u", (unsigned)last_wifi_reason);
    reply_err(reason);
}

/** List what the radio can actually see, so "not found" becomes checkable. */
static void handle_scan(void) {
    int found = WiFi.scanNetworks(false, false, false, 300);

    for(int i = 0; i < found && i < 24; i++) {
        Serial.printf("+AP %d %s\n", WiFi.RSSI(i), WiFi.SSID(i).c_str());
    }
    WiFi.scanDelete();

    if(found < 0) {
        reply_err("SCAN");
        return;
    }
    reply_ok();
}

static void handle_ping(void) {
    Serial.printf("+WOLFW %u\n", (unsigned)WOL_FW_VERSION);
    reply_ok();
}

static void handle_status(void) {
    if(WiFi.status() == WL_CONNECTED) {
        Serial.printf("+WIFI %s %s\n", WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
    } else {
        Serial.println("+WIFI - -");
    }
    reply_ok();
}

static void handle_join(char** fields, size_t count) {
    if(count < 3 || fields[1][0] == '\0') {
        reply_err("ARGS");
        return;
    }

    if(!wifi_up(fields[1], fields[2])) {
        reply_wifi_err();
        return;
    }

    Serial.printf("+WIFI OK %s\n", WiFi.localIP().toString().c_str());
    reply_ok();
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
        reply_err("ARGS");
        return;
    }

    if(!wifi_up(ssid, pass)) {
        reply_wifi_err();
        return;
    }
    Serial.printf("+WIFI OK %s\n", WiFi.localIP().toString().c_str());

    if(!send_magic_packet(mac, target, (uint16_t)port)) {
        reply_err("UDP");
        return;
    }
    reply_ok();
}

static void handle_line(char* buf) {
    char* fields[MAX_FIELDS];
    size_t count = split_fields(buf, fields, MAX_FIELDS);

    if(count == 0 || fields[0][0] == '\0') return;

    led_set(false, false, true); // blue while a command is in flight

    if(!strcmp(fields[0], "PING")) {
        handle_ping();
    } else if(!strcmp(fields[0], "STATUS")) {
        handle_status();
    } else if(!strcmp(fields[0], "SCAN")) {
        handle_scan();
    } else if(!strcmp(fields[0], "JOIN")) {
        handle_join(fields, count);
    } else if(!strcmp(fields[0], "WOL")) {
        handle_wol(fields, count);
    } else {
        reply_err("CMD");
    }

    led_off();
    last_heartbeat = millis();
}

void setup() {
    // banner first: the Flipper starts probing as soon as the board has power,
    // and anything that delays this races the first PING
    Serial.begin(115200);
    Serial.printf("+WOLFW %u ready\n", (unsigned)WOL_FW_VERSION);

    led_init();
    led_self_test();

    WiFi.mode(WIFI_STA);
    WiFi.persistent(false);
    WiFi.onEvent(wifi_event);

    last_heartbeat = millis();
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

    led_heartbeat();
    delay(2);
}
