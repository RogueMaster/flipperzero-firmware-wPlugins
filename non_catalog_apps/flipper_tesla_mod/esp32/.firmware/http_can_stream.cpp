#include "http_can_stream.h"
#include "can_capture_filter.h"
#include "web_dashboard.h"   // web_dashboard_stream_access(): whose requests are served
#include <freertos/FreeRTOS.h>   // portMUX for the ring (producer core 1 / consumer core 0)
#include <WiFi.h>
#include <esp_heap_caps.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#define HTTP_CAN_STREAM_PORT 82
#define HTTP_CAN_STREAM_RING_SIZE 3072u
#define HTTP_CAN_STREAM_FLUSH_BUDGET 160u
#define HTTP_CAN_STREAM_MAX_FILTER_IDS 32u
// Longest Origin / Host value kept from a request. Ours are far shorter
// (http:// and a 31-character hostname under a local domain); a longer one is
// someone else's.
#define HTTP_CAN_STREAM_HEADER_MAX 80u

struct StreamFrame {
    uint32_t elapsed_ms;
    CanBusId bus;
    CanFrame frame;
};

static WiFiServer g_server(HTTP_CAN_STREAM_PORT);
static WiFiClient g_client;
static bool       g_started = false;
static bool       g_active = false;
static bool       g_enabled = true;
static uint32_t   g_start_ms = 0;
static uint32_t   g_sent = 0;
static uint32_t   g_dropped = 0;
static uint32_t   g_filtered = 0;
static uint16_t   g_head = 0;
static uint16_t   g_tail = 0;
// The producer (http_can_stream_record, from process_frame on the CAN core) and
// the consumer (flush_stream, from web_dashboard_update) both mutate g_tail —
// the producer advances it on drop-oldest, the consumer on send. In the default
// build both run in loop() on core 1, so there is no contention; with the web
// server moved to its own core-0 task they run on different cores, which makes
// g_tail a two-writer and lets write_frame read a slot the producer is
// overwriting. This spinlock serialises the index mutations and makes the
// consumer copy the slot out before its blocking send. It is only ever taken
// while a capture is streaming (record() returns early otherwise), so the normal
// CAN path pays nothing.
static portMUX_TYPE g_ring_mux = portMUX_INITIALIZER_UNLOCKED;
// On the heap, not in .bss: as a static array these 72 KB left the classic
// ESP32 builds a few dozen bytes short of the static DRAM limit. Reserved once
// at boot (http_can_stream_reserve) and never freed.
static StreamFrame *g_ring = nullptr;
static uint32_t   g_filter_ids[HTTP_CAN_STREAM_MAX_FILTER_IDS];
static uint8_t    g_filter_count = 0;
static bool       g_bus_filter_enabled = false;
static CanBusId   g_bus_filter = CAN_BUS_PRIMARY;
static bool       g_meta_enabled = false;
static uint32_t   g_rx_missed_now = 0;   // latest fed-in controller total
static uint32_t   g_rx_missed_base = 0;  // snapshot at capture start
static bool       g_filter_meta_sent = false;
static char       g_filter_modes[CAN_BUS_COUNT][12] = {};

static uint16_t ring_next(uint16_t index) {
    return (uint16_t)((index + 1u) % HTTP_CAN_STREAM_RING_SIZE);
}

static bool ring_empty() {
    return g_head == g_tail;
}

static bool ring_full() {
    return ring_next(g_head) == g_tail;
}

static uint16_t ring_count() {
    if (g_head >= g_tail) return (uint16_t)(g_head - g_tail);
    return (uint16_t)(HTTP_CAN_STREAM_RING_SIZE - g_tail + g_head);
}

static void ring_reset() {
    g_head = 0;
    g_tail = 0;
}

static void id_filter_reset() {
    g_filter_count = 0;
}

static void stream_filter_reset() {
    g_filter_count = 0;
    g_bus_filter_enabled = false;
    g_bus_filter = CAN_BUS_PRIMARY;
    g_meta_enabled = false;
}

static bool filter_matches(uint32_t id) {
    if (g_filter_count == 0) return true;
    for (uint8_t i = 0; i < g_filter_count; i++) {
        if (g_filter_ids[i] == id) return true;
    }
    return false;
}

static int from_hex_digit(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static bool url_decode(char *s) {
    char *src = s;
    char *dst = s;
    while (*src) {
        if (*src == '+') {
            *dst++ = ' ';
            src++;
        } else if (*src == '%') {
            int hi = from_hex_digit(src[1]);
            int lo = from_hex_digit(src[2]);
            if (hi < 0 || lo < 0) return false;
            *dst++ = (char)((hi << 4) | lo);
            src += 3;
        } else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';
    return true;
}

static bool parse_filter(char *filter) {
    id_filter_reset();
    if (filter == nullptr || filter[0] == '\0') return true;
    if (!url_decode(filter)) return false;

    char *p = filter;
    for (;;) {
        while (*p == ' ' || *p == '\t' || *p == ',') p++;
        if (*p == '\0') return true;

        char *start = p;
        while (*p != '\0' && *p != ',') p++;
        char saved = *p;
        *p = '\0';

        char *end = start + strlen(start);
        while (end > start && (end[-1] == ' ' || end[-1] == '\t')) {
            *--end = '\0';
        }
        if (*start == '\0') return false;
        if (g_filter_count >= HTTP_CAN_STREAM_MAX_FILTER_IDS) return false;

        char *parse_start = start;
        if (parse_start[0] == '0' && (parse_start[1] == 'x' || parse_start[1] == 'X')) {
            parse_start += 2;
        }
        char *parse_end = nullptr;
        uint32_t id = (uint32_t)strtoul(parse_start, &parse_end, 16);
        if (parse_end == parse_start || *parse_end != '\0') return false;
        if (id > 0x1FFFFFFFu) return false;

        bool duplicate = false;
        for (uint8_t i = 0; i < g_filter_count; i++) {
            if (g_filter_ids[i] == id) {
                duplicate = true;
                break;
            }
        }
        if (!duplicate) g_filter_ids[g_filter_count++] = id;

        if (saved == '\0') return true;
        *p++ = saved;
    }
}

static bool parse_bus_filter(char *filter) {
    if (filter == nullptr || filter[0] == '\0') return false;
    if (!url_decode(filter)) return false;

    for (char *p = filter; *p; p++) {
        if (*p >= 'A' && *p <= 'Z') *p = (char)(*p - 'A' + 'a');
    }

    if (strcmp(filter, "can0") == 0 || strcmp(filter, "0") == 0 ||
        strcmp(filter, "primary") == 0) {
        g_bus_filter_enabled = true;
        g_bus_filter = CAN_BUS_PRIMARY;
        return true;
    }
    if (strcmp(filter, "can1") == 0 || strcmp(filter, "1") == 0 ||
        strcmp(filter, "secondary") == 0) {
        g_bus_filter_enabled = true;
        g_bus_filter = CAN_BUS_SECONDARY;
        return true;
    }
    return false;
}

static bool configure_filter_from_path(char *path) {
    char *query = strchr(path, '?');
    if (query == nullptr) {
        stream_filter_reset();
        return true;
    }

    *query++ = '\0';
    stream_filter_reset();
    char *param = query;
    while (param != nullptr && *param != '\0') {
        char *next = strchr(param, '&');
        if (next) *next++ = '\0';
        if (strncmp(param, "ids=", 4) == 0) {
            if (!parse_filter(param + 4)) return false;
        } else if (strncmp(param, "bus=", 4) == 0) {
            if (!parse_bus_filter(param + 4)) return false;
        } else if (strncmp(param, "meta=", 5) == 0) {
            const char *v = param + 5;
            g_meta_enabled = (strcmp(v, "1") == 0 || strcmp(v, "true") == 0);
        }
        param = next;
    }
    return true;
}

static void close_stream() {
    // Opt-in (?meta=1) self-labeling footer — best-effort, before the socket is
    // torn down and only while the client is still attached. #-prefixed so
    // importers skip it. Ignore any write failure.
    if (g_active && g_meta_enabled && g_client && g_client.connected()) {
        g_client.printf("# end sent=%lu dropped=%lu filtered=%lu rx_missed_delta=%lu\r\n",
                        (unsigned long)g_sent,
                        (unsigned long)g_dropped,
                        (unsigned long)g_filtered,
                        (unsigned long)http_can_stream_rx_missed());
    }
    if (g_client) {
        g_client.flush();
        g_client.stop();
    }
    if (g_active) {
        Serial.printf("[HTTP-CAN] Stream stopped  sent=%lu dropped=%lu\n",
                      (unsigned long)g_sent, (unsigned long)g_dropped);
    }
    g_active = false;
    ring_reset();
}

// The dashboard page reads the stream from :80, so a reply to it names its
// Origin; any other reply grants nobody (web_stream_access()).
static void print_cors(WiFiClient &client, const char *allow_origin) {
    if (!allow_origin || !allow_origin[0]) return;
    client.printf("Access-Control-Allow-Origin: %s\r\n", allow_origin);
    client.print("Vary: Origin\r\n");
}

static void send_response(WiFiClient &client, int code, const char *status, const char *body,
                          const char *allow_origin = nullptr) {
    client.printf("HTTP/1.1 %d %s\r\n", code, status);
    client.print("Content-Type: text/plain; charset=utf-8\r\n");
    client.print("Cache-Control: no-store\r\n");
    print_cors(client, allow_origin);
    client.print("X-Content-Type-Options: nosniff\r\n");
    client.print("Connection: close\r\n\r\n");
    if (body != nullptr) client.print(body);
    client.flush();
    client.stop();
}

static void begin_stream(WiFiClient &client, const char *allow_origin) {
    close_stream();
    g_client = client;
    g_client.setNoDelay(true);
    g_start_ms = millis();
    g_sent = 0;
    g_dropped = 0;
    g_filtered = 0;
    ring_reset();

    g_client.print("HTTP/1.1 200 OK\r\n");
    g_client.print("Content-Type: text/plain; charset=utf-8\r\n");
    g_client.print("Cache-Control: no-store\r\n");
    print_cors(g_client, allow_origin);
    g_client.print("X-Content-Type-Options: nosniff\r\n");
    g_client.print("Connection: close\r\n\r\n");
    g_active = true;
    g_rx_missed_base = g_rx_missed_now;
    g_filter_meta_sent = false;

    // Opt-in (?meta=1) self-labeling header. Emitted as a #-prefixed comment so
    // candump/SavvyCAN importers skip it; the default path stays byte-identical.
    if (g_meta_enabled) {
        g_client.print("# capture ids=");
        if (g_filter_count == 0) {
            g_client.print("all");
        } else {
            for (uint8_t i = 0; i < g_filter_count; i++) {
                if (i) g_client.print(",");
                g_client.printf("%lX", (unsigned long)g_filter_ids[i]);
            }
        }
        g_client.printf(" bus=%s mode=%s rx_missed_at_start=%lu\r\n",
                        g_bus_filter_enabled ? can_bus_name(g_bus_filter) : "all",
                        "pending",
                        (unsigned long)g_rx_missed_now);
    }

    Serial.printf("[HTTP-CAN] Stream started on :%u/stream filter_ids=%u bus=%s meta=%d\n",
                  HTTP_CAN_STREAM_PORT,
                  g_filter_count,
                  g_bus_filter_enabled ? can_bus_name(g_bus_filter) : "all",
                  g_meta_enabled ? 1 : 0);
}

// The value of header line `line` when it is `name`: copied to out, trimmed,
// "?" when it does not fit (a value that is not ours). False for another header.
static bool header_value(const String &line, const char *name, char *out, size_t out_len) {
    size_t n = strlen(name);
    if (line.length() <= n || line.charAt(n) != ':' ||
        strncasecmp(line.c_str(), name, n) != 0) return false;
    String v = line.substring(n + 1);
    v.trim();
    if (v.length() >= out_len) v = "?";
    memcpy(out, v.c_str(), v.length() + 1);
    return true;
}

// Request line and headers. Origin and Host are kept for
// web_dashboard_stream_access(), empty when the request has none. False when
// the request is not a GET, or its headers did not all arrive in time: then
// whether it may have the stream cannot be told.
static bool parse_request_path(WiFiClient &client, char *path, size_t path_len,
                               char *origin, char *host, size_t header_len) {
    if (path_len == 0) return false;
    path[0] = '\0';
    origin[0] = host[0] = '\0';

    client.setTimeout(20);
    String request_line = client.readStringUntil('\n');
    request_line.trim();
    if (!request_line.startsWith("GET ")) return false;

    int path_start = 4;
    int path_end = request_line.indexOf(' ', path_start);
    if (path_end <= path_start) return false;

    size_t len = (size_t)(path_end - path_start);
    if (len >= path_len) return false;
    memcpy(path, request_line.c_str() + path_start, len);
    path[len] = '\0';

    uint32_t deadline = millis() + 20u;
    while ((int32_t)(millis() - deadline) < 0) {
        if (!client.available()) {
            if (!client.connected()) break;
            delay(1);
            continue;
        }
        String header = client.readStringUntil('\n');
        if (header == "\r" || header.length() == 0) return true;
        if (!header_value(header, "Origin", origin, header_len))
            header_value(header, "Host", host, header_len);
    }
    return false;
}

static void accept_client() {
    WiFiClient incoming = g_server.available();
    if (!incoming) return;

    char path[256];
    char origin[HTTP_CAN_STREAM_HEADER_MAX], host[HTTP_CAN_STREAM_HEADER_MAX];
    if (!parse_request_path(incoming, path, sizeof(path), origin, host, sizeof(origin))) {
        send_response(incoming, 400, "Bad Request", "Bad Request\n");
        return;
    }
    // Before anything else: a refused request must not end the stream that is
    // running or change its filters.
    WebStreamAccess access = web_dashboard_stream_access(origin, host);
    if (access == WEB_STREAM_REFUSE) {
        Serial.printf("[HTTP-CAN] refused: Origin %.64s Host %.64s\n", origin, host);
        send_response(incoming, 403, "Forbidden", "Forbidden\n");
        return;
    }
    const char *grant = access == WEB_STREAM_SERVE_GRANT ? origin : nullptr;

    if (!g_enabled) {
        send_response(incoming, 409, "Conflict", "HTTP CAN log disabled in Active mode\n", grant);
        return;
    }
    if (!g_ring) {
        send_response(incoming, 503, "Service Unavailable", "No memory for the capture buffer\n", grant);
        return;
    }

    uint8_t old_filter_count = g_filter_count;
    uint32_t old_filter_ids[HTTP_CAN_STREAM_MAX_FILTER_IDS];
    memcpy(old_filter_ids, g_filter_ids, sizeof(old_filter_ids));
    bool old_bus_filter_enabled = g_bus_filter_enabled;
    CanBusId old_bus_filter = g_bus_filter;
    bool old_meta_enabled = g_meta_enabled;

    if (!configure_filter_from_path(path)) {
        g_filter_count = old_filter_count;
        memcpy(g_filter_ids, old_filter_ids, sizeof(g_filter_ids));
        g_bus_filter_enabled = old_bus_filter_enabled;
        g_bus_filter = old_bus_filter;
        g_meta_enabled = old_meta_enabled;
        send_response(incoming, 400, "Bad Request", "Invalid stream filter\n", grant);
        return;
    }

    if (strcmp(path, "/stream") != 0 && strcmp(path, "/canlog/stream") != 0) {
        g_filter_count = old_filter_count;
        memcpy(g_filter_ids, old_filter_ids, sizeof(g_filter_ids));
        g_bus_filter_enabled = old_bus_filter_enabled;
        g_bus_filter = old_bus_filter;
        g_meta_enabled = old_meta_enabled;
        send_response(incoming, 404, "Not Found", "Not Found\n", grant);
        return;
    }

    begin_stream(incoming, grant);
}

static bool write_all(const char *line, size_t len) {
    if (!g_active || !g_client.connected()) return false;
    size_t written = g_client.write((const uint8_t *)line, len);
    return written == len;
}

static bool write_frame(const StreamFrame &item) {
    uint32_t sec = item.elapsed_ms / 1000u;
    uint32_t usec = (item.elapsed_ms % 1000u) * 1000u;
    uint8_t dlc = item.frame.dlc;
    if (dlc > 8) dlc = 8;

    char line[72];
    int pos;
    if (item.frame.id <= 0x7FFu) {
        pos = snprintf(line, sizeof(line), "(%lu.%06lu) %s %03lX#",
                       (unsigned long)sec,
                       (unsigned long)usec,
                       can_bus_name(item.bus),
                       (unsigned long)item.frame.id);
    } else {
        pos = snprintf(line, sizeof(line), "(%lu.%06lu) %s %08lX#",
                       (unsigned long)sec,
                       (unsigned long)usec,
                       can_bus_name(item.bus),
                       (unsigned long)item.frame.id);
    }
    if (pos < 0 || pos >= (int)sizeof(line)) return false;

    for (uint8_t i = 0; i < dlc; i++) {
        int wrote = snprintf(line + pos, sizeof(line) - (size_t)pos,
                             "%02X", item.frame.data[i]);
        if (wrote != 2) return false;
        pos += wrote;
    }
    if (pos >= (int)sizeof(line) - 2) return false;
    line[pos++] = '\r';
    line[pos++] = '\n';

    return write_all(line, (size_t)pos);
}

static void flush_stream() {
    if (!g_active) return;
    if (!g_client.connected()) {
        close_stream();
        return;
    }

    uint16_t budget = HTTP_CAN_STREAM_FLUSH_BUDGET;
    while (budget > 0) {
        // Copy the oldest frame out under the lock so the producer can't
        // overwrite the slot mid-send; advance g_tail (also under the lock)
        // only after a successful write, so a busy client still back-pressures
        // instead of losing the frame.
        StreamFrame item;
        portENTER_CRITICAL(&g_ring_mux);
        bool have = !ring_empty();
        if (have) item = g_ring[g_tail];
        portEXIT_CRITICAL(&g_ring_mux);
        if (!have) return;
        if (!write_frame(item)) return;
        portENTER_CRITICAL(&g_ring_mux);
        g_tail = ring_next(g_tail);
        portEXIT_CRITICAL(&g_ring_mux);
        g_sent++;
        budget--;
    }
}

bool http_can_stream_reserve() {
    if (g_ring) return true;
    g_ring = (StreamFrame *)heap_caps_malloc(
        sizeof(StreamFrame) * HTTP_CAN_STREAM_RING_SIZE,
        MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!g_ring) Serial.println("[HTTP-CAN] No memory for the stream buffer, capture unavailable");
    return g_ring != nullptr;
}

void http_can_stream_init() {
    if (g_started) return;
    http_can_stream_reserve();
    g_server.begin();
    g_server.setNoDelay(true);
    g_started = true;
    Serial.printf("[HTTP-CAN] Stream endpoint: http://192.168.4.1:%u/stream\n",
                  HTTP_CAN_STREAM_PORT);
}

void http_can_stream_update() {
    if (!g_started) return;
    if (!g_enabled) {
        close_stream();
        accept_client();
        return;
    }
    accept_client();
    flush_stream();
}

void http_can_stream_record(CanBusId bus, const CanFrame &frame) {
    if (!g_enabled || !g_active || !g_client.connected()) return;
    if (g_bus_filter_enabled && bus != g_bus_filter) {
        g_filtered++;
        return;
    }
    if (!filter_matches(frame.id)) {
        g_filtered++;
        return;
    }

    // Fill the slot, then publish it by advancing g_head — all under the lock
    // so the consumer (possibly on another core) never sees a half-written slot
    // and the drop-oldest g_tail bump can't race the consumer's advance.
    portENTER_CRITICAL(&g_ring_mux);
    if (ring_full()) {
        g_dropped++;
        g_tail = ring_next(g_tail);
    }
    StreamFrame &slot = g_ring[g_head];
    slot.elapsed_ms = millis() - g_start_ms;
    slot.bus = bus;
    slot.frame.id = frame.id;
    slot.frame.dlc = frame.dlc > 8 ? 8 : frame.dlc;
    memcpy(slot.frame.data, frame.data, slot.frame.dlc);
    g_head = ring_next(g_head);
    portEXIT_CRITICAL(&g_ring_mux);
}

void http_can_stream_set_enabled(bool enabled) {
    if (g_enabled == enabled) return;
    g_enabled = enabled;
    if (!g_enabled) {
        close_stream();
        Serial.println("[HTTP-CAN] Stream disabled");
    } else {
        Serial.println("[HTTP-CAN] Stream enabled");
    }
}

bool http_can_stream_active() {
    return g_enabled && g_active && g_client.connected();
}

bool http_can_stream_single_filter(uint32_t *id_out) {
    if (!g_enabled || !g_active || !g_client.connected()) return false;
    if (g_filter_count != 1) return false;
    if (id_out) *id_out = g_filter_ids[0];
    return true;
}

bool http_can_stream_filter_snapshot(uint32_t *ids_out,
                                     uint8_t ids_max,
                                     uint8_t *count_out) {
    if (!g_enabled || !g_active || !g_client.connected()) return false;
    if (count_out) *count_out = g_filter_count;
    if (ids_out && ids_max > 0) {
        uint8_t n = g_filter_count < ids_max ? g_filter_count : ids_max;
        for (uint8_t i = 0; i < n; i++) ids_out[i] = g_filter_ids[i];
    }
    return true;
}

void http_can_stream_note_filters(uint8_t buses, const char *const *modes) {
    if (!g_active || !g_meta_enabled || !g_client.connected()) return;
    bool changed = !g_filter_meta_sent;
    for (uint8_t i = 0; i < CAN_BUS_COUNT; i++) {
        changed |= strcmp(g_filter_modes[i], modes[i]) != 0;
    }
    if (!changed) return;
    size_t written = g_client.printf(
        "# filters elapsed_ms=%lu can0=%s can1=%s mode=%s\r\n",
        (unsigned long)(millis() - g_start_ms), modes[0], modes[1],
        can_capture_mode_label(g_filter_count, buses, g_bus_filter_enabled, g_bus_filter, modes));
    if (written == 0) return;
    for (uint8_t i = 0; i < CAN_BUS_COUNT; i++) {
        snprintf(g_filter_modes[i], sizeof(g_filter_modes[i]), "%s", modes[i]);
    }
    g_filter_meta_sent = true;
}

bool http_can_stream_bus_filter(CanBusId *bus_out) {
    if (!g_enabled || !g_active || !g_client.connected()) return false;
    if (!g_bus_filter_enabled) return false;
    if (bus_out) *bus_out = g_bus_filter;
    return true;
}

uint32_t http_can_stream_frames_sent() {
    return g_sent;
}

uint32_t http_can_stream_frames_dropped() {
    return g_dropped;
}

uint32_t http_can_stream_frames_filtered() {
    return g_filtered;
}

uint16_t http_can_stream_buffered_frames() {
    return ring_count();
}

void http_can_stream_note_rx_missed(uint32_t total_rx_missed) {
    g_rx_missed_now = total_rx_missed;
}

uint32_t http_can_stream_rx_missed(void) {
    if (g_rx_missed_now < g_rx_missed_base) {
        // Counter reset (e.g. driver reinstall after bus-off) — rebase and
        // report 0 rather than underflowing the unsigned subtraction.
        g_rx_missed_base = g_rx_missed_now;
        return 0;
    }
    return g_rx_missed_now - g_rx_missed_base;
}
