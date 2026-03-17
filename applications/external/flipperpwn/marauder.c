/**
 * marauder.c — Marauder command abstraction over WiFi UART.
 *
 * Marauder text output format (relevant lines)
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *   scanap results:
 *     <idx> <SSID> <RSSI> <Channel> <BSSID> <Encryption>
 *     e.g.  0 MyNetwork -45 6 AA:BB:CC:DD:EE:FF WPA2
 *
 *   pingscan results:
 *     <IP> alive
 *     <IP> dead
 *
 *   portscan results:
 *     <port> open <service>
 *     <port> closed
 *
 *   Prompts start with ">"; binary PCAP framing with "[BUF/" — both are
 *   filtered upstream in wifi_uart.c before reaching this callback.
 *
 * Parsing strategy
 * ~~~~~~~~~~~~~~~~
 *   strtok is unavailable in the Flipper SDK libc.  All tokenising uses
 *   strchr to locate space delimiters and manual pointer arithmetic.
 */

#include "marauder.h"

#include <string.h>
#include <stdlib.h>

#define TAG "FPwn"

/* --------------------------------------------------------------------------
 * Internal struct
 * -------------------------------------------------------------------------- */
struct FPwnMarauder {
    FPwnWifiUart* uart;
    FPwnMarauderState state;

    FPwnWifiAP aps[FPWN_MAX_APS];
    uint32_t ap_count;

    FPwnNetHost hosts[FPWN_MAX_HOSTS];
    uint32_t host_count;

    FPwnPortResult ports[FPWN_MAX_PORTS];
    uint32_t port_count;

    FPwnStation stations[FPWN_MAX_STATIONS];
    uint32_t station_count;

    FPwnCapturedCred creds[FPWN_MAX_CREDS];
    uint32_t cred_count;

    FuriMutex* mutex;

    uint32_t scan_start_tick; /* furi_get_tick() when the current AP scan started */
    uint32_t stop_tick; /* tick when stopscan was sent */
    bool list_pending; /* true = waiting to send 'list -a' after delay */

    /* Secondary log callback — fires for every received line after parsing. */
    FPwnWifiRxCallback log_callback;
    void* log_callback_ctx;
};

/* --------------------------------------------------------------------------
 * Parser helpers
 * -------------------------------------------------------------------------- */

/* Copy at most `n-1` bytes of the current token (up to the next space or
 * end-of-string) into `dst`, then null-terminate.
 * Returns a pointer to the start of the NEXT token (after any trailing
 * spaces), or NULL if there is no further token. */
static const char* copy_token(const char* src, char* dst, size_t n) {
    size_t i = 0;
    while(*src && *src != ' ' && i < n - 1) {
        dst[i++] = *src++;
    }
    dst[i] = '\0';
    /* Advance past delimiter spaces to the next token. */
    while(*src == ' ')
        src++;
    return (*src) ? src : NULL;
}

static const char* find_prev_token_start(const char* start, const char* end) {
    while(end > start && end[-1] == ' ')
        end--;
    while(end > start && end[-1] != ' ')
        end--;
    return end;
}

static bool line_has_done_marker(const char* line) {
    return (strcmp(line, "Done") == 0) || (strcmp(line, "done") == 0) ||
           strstr(line, "Scan complete") || strstr(line, "scan complete") ||
           strstr(line, "Ping scan complete") || strstr(line, "Port scan complete") ||
           strstr(line, "Station scan complete") || strstr(line, "Finished") ||
           strstr(line, "finished");
}

/* --------------------------------------------------------------------------
 * Marauder output parsers
 * -------------------------------------------------------------------------- */

/*
 * Try to parse a scanap result line into `ap`.
 *
 * Expected format (space-separated):
 *   <idx> <SSID> <RSSI> <Channel> <BSSID> <Encryption>
 *
 * Returns true on success.
 */
static bool parse_ap_line(const char* line, FPwnWifiAP* ap) {
    const char* p = line;

    if(!p || *p < '0' || *p > '9') return false;

    char idx_buf[8];
    p = copy_token(p, idx_buf, sizeof(idx_buf));
    if(!p || !*p) return false;

    const char* end = line + strlen(line);
    const char* enc_start = find_prev_token_start(p, end);
    if(enc_start <= p) return false;

    char enc_buf[16];
    size_t enc_len = (size_t)(end - enc_start);
    if(enc_len > sizeof(enc_buf) - 1) enc_len = sizeof(enc_buf) - 1;
    memcpy(enc_buf, enc_start, enc_len);
    enc_buf[enc_len] = '\0';

    const char* before_enc = enc_start;
    const char* bssid_start = find_prev_token_start(p, before_enc);
    if(bssid_start <= p) return false;
    size_t bssid_len = (size_t)(before_enc - bssid_start);
    while(bssid_len > 0 && bssid_start[bssid_len - 1] == ' ')
        bssid_len--;
    if(bssid_len == 0) return false;
    if(bssid_len > sizeof(ap->bssid) - 1) bssid_len = sizeof(ap->bssid) - 1;
    memcpy(ap->bssid, bssid_start, bssid_len);
    ap->bssid[bssid_len] = '\0';

    const char* before_bssid = bssid_start;
    const char* ch_start = find_prev_token_start(p, before_bssid);
    if(ch_start <= p) return false;
    char ch_buf[8];
    size_t ch_len = (size_t)(before_bssid - ch_start);
    while(ch_len > 0 && ch_start[ch_len - 1] == ' ')
        ch_len--;
    if(ch_len == 0) return false;
    if(ch_len > sizeof(ch_buf) - 1) ch_len = sizeof(ch_buf) - 1;
    memcpy(ch_buf, ch_start, ch_len);
    ch_buf[ch_len] = '\0';
    ap->channel = (uint8_t)atoi(ch_buf);

    const char* before_ch = ch_start;
    const char* rssi_start = find_prev_token_start(p, before_ch);
    if(rssi_start <= p) return false;
    char rssi_buf[8];
    size_t rssi_len = (size_t)(before_ch - rssi_start);
    while(rssi_len > 0 && rssi_start[rssi_len - 1] == ' ')
        rssi_len--;
    if(rssi_len == 0) return false;
    if(rssi_len > sizeof(rssi_buf) - 1) rssi_len = sizeof(rssi_buf) - 1;
    memcpy(rssi_buf, rssi_start, rssi_len);
    rssi_buf[rssi_len] = '\0';
    ap->rssi = (int8_t)atoi(rssi_buf);

    size_t ssid_len = (size_t)(rssi_start - p);
    while(ssid_len > 0 && p[ssid_len - 1] == ' ')
        ssid_len--;
    if(ssid_len == 0) return false;
    if(ssid_len > sizeof(ap->ssid) - 1) ssid_len = sizeof(ap->ssid) - 1;
    memcpy(ap->ssid, p, ssid_len);
    ap->ssid[ssid_len] = '\0';

    if(strcmp(enc_buf, "Open") == 0 || strcmp(enc_buf, "OPEN") == 0) {
        ap->encryption = 0;
    } else if(strcmp(enc_buf, "WEP") == 0) {
        ap->encryption = 1;
    } else if(strcmp(enc_buf, "WPA") == 0) {
        ap->encryption = 2;
    } else {
        /* WPA2, WPA2-EAP, unknown — default to WPA2 */
        ap->encryption = 3;
    }

    return true;
}

/*
 * Try to parse a Marauder 'list -a' line into `ap`.
 *
 * Common format: [idx] SSID (rssi) ch:X [ENC] BSSID
 * Example:       [0] MyNetwork (-45) ch:6 [WPA2] AA:BB:CC:DD:EE:FF
 *
 * Returns true on success.
 */
static bool parse_list_ap_line(const char* line, FPwnWifiAP* ap) {
    /* Must start with '[' (bracketed index) */
    if(line[0] != '[') return false;

    const char* idx_end = strchr(line, ']');
    if(!idx_end) return false;
    const char* p = idx_end + 1;

    while(*p == ' ')
        p++;
    if(!*p) return false;

    /* SSID: everything up to the opening '(' that precedes the RSSI */
    const char* paren = strchr(p, '(');
    if(!paren) return false;

    size_t ssid_len = (size_t)(paren - p);
    while(ssid_len > 0 && p[ssid_len - 1] == ' ')
        ssid_len--;
    if(ssid_len > 32) ssid_len = 32;
    memcpy(ap->ssid, p, ssid_len);
    ap->ssid[ssid_len] = '\0';

    /* RSSI inside the parentheses */
    p = paren + 1;
    ap->rssi = (int8_t)atoi(p);

    const char* close_paren = strchr(p, ')');
    if(!close_paren) return false;
    p = close_paren + 1;
    while(*p == ' ')
        p++;

    /* Channel: "ch:X" or "Ch:X" */
    if(strncmp(p, "ch:", 3) == 0 || strncmp(p, "Ch:", 3) == 0) {
        ap->channel = (uint8_t)atoi(p + 3);
        p += 3;
        while(*p >= '0' && *p <= '9')
            p++;
        while(*p == ' ')
            p++;
    }

    /* Encryption: [WPA2], [WPA], [WEP], [Open], etc. */
    if(*p == '[') {
        p++;
        const char* enc_end = strchr(p, ']');
        if(enc_end) {
            size_t enc_len = (size_t)(enc_end - p);
            char enc_buf[8];
            if(enc_len > sizeof(enc_buf) - 1) enc_len = sizeof(enc_buf) - 1;
            memcpy(enc_buf, p, enc_len);
            enc_buf[enc_len] = '\0';

            if(strcmp(enc_buf, "Open") == 0 || strcmp(enc_buf, "OPEN") == 0)
                ap->encryption = 0;
            else if(strcmp(enc_buf, "WEP") == 0)
                ap->encryption = 1;
            else if(strcmp(enc_buf, "WPA") == 0)
                ap->encryption = 2;
            else
                ap->encryption = 3; /* WPA2 or anything else */

            p = enc_end + 1;
            while(*p == ' ')
                p++;
        }
    }

    /* BSSID: remaining content */
    if(*p) {
        strncpy(ap->bssid, p, sizeof(ap->bssid) - 1);
        ap->bssid[sizeof(ap->bssid) - 1] = '\0';
        /* Trim trailing whitespace */
        size_t blen = strlen(ap->bssid);
        while(blen > 0 && (ap->bssid[blen - 1] == ' ' || ap->bssid[blen - 1] == '\n' ||
                           ap->bssid[blen - 1] == '\r')) {
            ap->bssid[--blen] = '\0';
        }
    }

    return ap->ssid[0] != '\0';
}

/*
 * Try to parse a pingscan result line into `host`.
 *
 * Expected format:  <IP> alive   or   <IP> dead
 */
static bool parse_host_line(const char* line, FPwnNetHost* host) {
    const char* p = line;

    /* Field 0: IP address — must start with a digit. */
    if(*p < '0' || *p > '9') return false;

    /* copy_token advances p to the "alive"/"dead" token. */
    p = copy_token(p, host->ip, sizeof(host->ip));
    if(!p) return false;

    host->alive = (strncmp(p, "alive", 5) == 0);
    return true;
}

/*
 * Try to parse a portscan result line into `port`.
 *
 * Expected formats:
 *   <port> open <service>
 *   <port> closed
 */
static bool parse_port_line(const char* line, FPwnPortResult* result) {
    const char* p = line;

    /* Field 0: port number — must start with a digit. */
    if(*p < '0' || *p > '9') return false;

    char port_buf[8];
    /* copy_token advances p to the "open"/"closed" token. */
    p = copy_token(p, port_buf, sizeof(port_buf));
    result->port = (uint16_t)atoi(port_buf);
    if(!p) return false;

    if(strncmp(p, "open", 4) == 0) {
        result->open = true;
        /* Advance p past the "open" token; copy_token returns a pointer
         * to the next token (the service name) or NULL. */
        char discard[8];
        const char* svc_start = copy_token(p, discard, sizeof(discard));
        if(svc_start && *svc_start) {
            copy_token(svc_start, result->service, sizeof(result->service));
        } else {
            result->service[0] = '\0';
        }
    } else if(strncmp(p, "closed", 6) == 0) {
        result->open = false;
        result->service[0] = '\0';
    } else {
        return false;
    }

    return true;
}

/*
 * Try to parse a scansta result line into `sta`.
 *
 * Expected format (space-separated):
 *   <MAC> <RSSI> <AP_SSID>
 *   e.g.  AA:BB:CC:DD:EE:FF -65 MyNetwork
 *
 * Detection heuristic: line starts with two hex digits followed by ':'.
 * Returns true on success.
 */
static bool parse_station_line(const char* line, FPwnStation* sta) {
    /* Quick rejection: must start with two hex chars then ':'. */
    if(!((line[0] >= '0' && line[0] <= '9') || (line[0] >= 'A' && line[0] <= 'F') ||
         (line[0] >= 'a' && line[0] <= 'f')))
        return false;
    if(!((line[1] >= '0' && line[1] <= '9') || (line[1] >= 'A' && line[1] <= 'F') ||
         (line[1] >= 'a' && line[1] <= 'f')))
        return false;
    if(line[2] != ':') return false;

    const char* p = line;

    /* Field 0: MAC address */
    p = copy_token(p, sta->mac, sizeof(sta->mac));
    if(!p) return false;

    /* Field 1: RSSI */
    char rssi_buf[8];
    p = copy_token(p, rssi_buf, sizeof(rssi_buf));
    sta->rssi = (int8_t)atoi(rssi_buf);

    /* Field 2: AP SSID — rest of line (may be absent if station is unassociated) */
    if(p && *p) {
        strncpy(sta->ap_ssid, p, sizeof(sta->ap_ssid) - 1);
        sta->ap_ssid[sizeof(sta->ap_ssid) - 1] = '\0';
        /* Trim trailing whitespace */
        size_t len = strlen(sta->ap_ssid);
        while(len > 0 && (sta->ap_ssid[len - 1] == ' ' || sta->ap_ssid[len - 1] == '\r' ||
                          sta->ap_ssid[len - 1] == '\n')) {
            sta->ap_ssid[--len] = '\0';
        }
    } else {
        sta->ap_ssid[0] = '\0';
    }

    return true;
}

/* --------------------------------------------------------------------------
 * UART RX callback — dispatches parsed results into arrays
 * -------------------------------------------------------------------------- */
static void fpwn_marauder_rx_cb(const char* line, void* ctx) {
    FPwnMarauder* m = (FPwnMarauder*)ctx;

    /* Marauder prompt lines start with ">"; skip parsing but still forward
     * to the log callback so the wifi connected notification fires. */
    if(line[0] == '>') {
        if(m->log_callback) {
            m->log_callback(line, m->log_callback_ctx);
        }
        return;
    }

    furi_mutex_acquire(m->mutex, FuriWaitForever);

    switch(m->state) {
    case FPwnMarauderStateScanning:
    case FPwnMarauderStateScanStopping: {
        /* Skip status / header lines. Detect end-of-results markers while
         * draining so we can transition to Idle without waiting for the
         * safety timeout in the timer callback. */
        if(strstr(line, "Scan complete") || strncmp(line, "Scanning", 8) == 0 ||
           strncmp(line, "Stopping", 8) == 0 || strstr(line, "[APs]") || strstr(line, "Started") ||
           strstr(line, "Done")) {
            if(m->state == FPwnMarauderStateScanStopping &&
               (strstr(line, "Scan complete") || strstr(line, "Done"))) {
                m->state = FPwnMarauderStateIdle;
                FURI_LOG_I(TAG, "scan complete, %lu APs", (unsigned long)m->ap_count);
            }
            break;
        }

        FPwnWifiAP ap;
        memset(&ap, 0, sizeof(ap));
        if(parse_ap_line(line, &ap)) {
            if(m->ap_count < FPWN_MAX_APS) {
                m->aps[m->ap_count++] = ap;
                FURI_LOG_I(
                    TAG, "AP[%lu]: %s %s", (unsigned long)m->ap_count - 1, ap.ssid, ap.bssid);
            }
        } else if(parse_list_ap_line(line, &ap)) {
            if(m->ap_count < FPWN_MAX_APS) {
                m->aps[m->ap_count++] = ap;
                FURI_LOG_I(
                    TAG, "AP[%lu]: %s %s", (unsigned long)m->ap_count - 1, ap.ssid, ap.bssid);
            }
        } else {
            /* Log unparsed lines at Info level to help diagnose Marauder
             * firmware version mismatches or unexpected output formats. */
            FURI_LOG_I(TAG, "scan RX (not parsed): %s", line);
        }
        break;
    }

    case FPwnMarauderStatePingScan: {
        if(line_has_done_marker(line)) {
            m->state = FPwnMarauderStateIdle;
            FURI_LOG_I(TAG, "ping scan complete, %lu hosts", (unsigned long)m->host_count);
            break;
        }
        FPwnNetHost host;
        memset(&host, 0, sizeof(host));
        if(parse_host_line(line, &host)) {
            if(m->host_count < FPWN_MAX_HOSTS) {
                m->hosts[m->host_count++] = host;
                FURI_LOG_D(
                    TAG,
                    "Host[%lu]: %s %s",
                    (unsigned long)m->host_count - 1,
                    host.ip,
                    host.alive ? "alive" : "dead");
            }
        }
        break;
    }

    case FPwnMarauderStatePortScan: {
        if(line_has_done_marker(line)) {
            m->state = FPwnMarauderStateIdle;
            FURI_LOG_I(TAG, "port scan complete, %lu results", (unsigned long)m->port_count);
            break;
        }
        FPwnPortResult pr;
        memset(&pr, 0, sizeof(pr));
        if(parse_port_line(line, &pr)) {
            if(m->port_count < FPWN_MAX_PORTS) {
                m->ports[m->port_count++] = pr;
                FURI_LOG_D(
                    TAG,
                    "Port[%lu]: %u %s",
                    (unsigned long)m->port_count - 1,
                    pr.port,
                    pr.open ? "open" : "closed");
            }
        }
        break;
    }

    case FPwnMarauderStateStationScan: {
        if(line_has_done_marker(line)) {
            m->state = FPwnMarauderStateIdle;
            FURI_LOG_I(
                TAG, "station scan complete, %lu stations", (unsigned long)m->station_count);
            break;
        }
        FPwnStation sta;
        memset(&sta, 0, sizeof(sta));
        if(parse_station_line(line, &sta)) {
            if(m->station_count < FPWN_MAX_STATIONS) {
                m->stations[m->station_count++] = sta;
                FURI_LOG_D(
                    TAG,
                    "STA[%lu]: %s rssi=%d ap=%s",
                    (unsigned long)m->station_count - 1,
                    sta.mac,
                    (int)sta.rssi,
                    sta.ap_ssid);
            }
        }
        break;
    }

    case FPwnMarauderStateEvilPortal:
        /* Capture POST data and credential-bearing lines from the portal.
         * Marauder outputs lines like "POST data: user=x&pass=y" when a
         * victim submits the captive portal form.
         * NOTE: mutex is already held by the caller — do NOT re-acquire. */
        if(strstr(line, "POST") || strstr(line, "post") || strstr(line, "password") ||
           strstr(line, "Password") || strstr(line, "username") || strstr(line, "Username") ||
           strstr(line, "credential") || strstr(line, "login")) {
            if(m->cred_count < FPWN_MAX_CREDS) {
                strncpy(m->creds[m->cred_count].data, line, sizeof(m->creds[0].data) - 1);
                m->creds[m->cred_count].data[sizeof(m->creds[0].data) - 1] = '\0';
                m->cred_count++;
                FURI_LOG_I(
                    TAG,
                    "Evil portal cred captured [%lu]: %s",
                    (unsigned long)m->cred_count - 1,
                    line);
            }
        }
        break;

    case FPwnMarauderStateSniffDeauth:
        /* Raw handshake capture — no structured parsing, output goes to log
         * TextBox via the log_callback forwarding below. */
        break;

    case FPwnMarauderStateSniffProbe:
        /* Raw probe request capture — no structured parsing, output goes to
         * log TextBox via the log_callback forwarding below. */
        break;

    default:
        /* Other states: log but don't parse structured output. */
        FURI_LOG_D(TAG, "RX[idle]: %s", line);
        break;
    }

    furi_mutex_release(m->mutex);

    /* Forward every line to the optional log callback (status TextBox). */
    if(m->log_callback) {
        m->log_callback(line, m->log_callback_ctx);
    }
}

/* --------------------------------------------------------------------------
 * Lifecycle
 * -------------------------------------------------------------------------- */

FPwnMarauder* fpwn_marauder_alloc(FPwnWifiUart* uart) {
    furi_assert(uart);

    FPwnMarauder* m = malloc(sizeof(FPwnMarauder));
    furi_assert(m);
    memset(m, 0, sizeof(FPwnMarauder));

    m->uart = uart;
    m->state = FPwnMarauderStateIdle;

    m->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    furi_assert(m->mutex);

    fpwn_wifi_uart_set_rx_callback(uart, fpwn_marauder_rx_cb, m);

    FURI_LOG_I(TAG, "Marauder layer initialised");
    return m;
}

void fpwn_marauder_free(FPwnMarauder* marauder) {
    furi_assert(marauder);
    /* Deregister callback so no stale calls fire after free. */
    fpwn_wifi_uart_set_rx_callback(marauder->uart, NULL, NULL);
    furi_mutex_free(marauder->mutex);
    free(marauder);
}

/* --------------------------------------------------------------------------
 * Commands
 * -------------------------------------------------------------------------- */

void fpwn_marauder_scan_ap(FPwnMarauder* m) {
    furi_assert(m);
    furi_mutex_acquire(m->mutex, FuriWaitForever);
    memset(m->aps, 0, sizeof(m->aps));
    m->ap_count = 0;
    m->state = FPwnMarauderStateScanning;
    m->scan_start_tick = furi_get_tick();
    furi_mutex_release(m->mutex);

    fpwn_wifi_uart_send(m->uart, "scanap");
    FURI_LOG_I(TAG, "scanap started");
}

void fpwn_marauder_stop_scan(FPwnMarauder* m) {
    furi_assert(m);
    fpwn_wifi_uart_send(m->uart, "stopscan");

    furi_mutex_acquire(m->mutex, FuriWaitForever);
    m->state = FPwnMarauderStateScanStopping;
    m->stop_tick = furi_get_tick();
    m->list_pending = true; /* 'list -a' will be sent after a delay */
    furi_mutex_release(m->mutex);

    FURI_LOG_I(TAG, "scan stopping, will send 'list -a' after delay");
}

void fpwn_marauder_join(FPwnMarauder* m, uint8_t ap_idx, const char* password) {
    furi_assert(m);
    furi_assert(password);

    char cmd[128];

    if(password[0] != '\0') {
        snprintf(cmd, sizeof(cmd), "join -a %u -p %s", (unsigned)ap_idx, password);
    } else {
        snprintf(cmd, sizeof(cmd), "join -a %u", (unsigned)ap_idx);
    }

    fpwn_wifi_uart_send(m->uart, cmd);

    furi_mutex_acquire(m->mutex, FuriWaitForever);
    m->state = FPwnMarauderStateJoined;
    furi_mutex_release(m->mutex);

    FURI_LOG_I(TAG, "join AP %u", (unsigned)ap_idx);
}

void fpwn_marauder_ping_scan(FPwnMarauder* m) {
    furi_assert(m);
    furi_mutex_acquire(m->mutex, FuriWaitForever);
    memset(m->hosts, 0, sizeof(m->hosts));
    m->host_count = 0;
    m->state = FPwnMarauderStatePingScan;
    furi_mutex_release(m->mutex);

    fpwn_wifi_uart_send(m->uart, "pingscan");
    FURI_LOG_I(TAG, "pingscan started");
}

void fpwn_marauder_port_scan(FPwnMarauder* m, uint8_t host_idx, bool all_ports) {
    furi_assert(m);
    furi_mutex_acquire(m->mutex, FuriWaitForever);
    memset(m->ports, 0, sizeof(m->ports));
    m->port_count = 0;
    m->state = FPwnMarauderStatePortScan;
    furi_mutex_release(m->mutex);

    char cmd[64];
    if(all_ports) {
        snprintf(cmd, sizeof(cmd), "portscan -t %u -a", (unsigned)host_idx);
    } else {
        snprintf(cmd, sizeof(cmd), "portscan -t %u", (unsigned)host_idx);
    }

    fpwn_wifi_uart_send(m->uart, cmd);
    FURI_LOG_I(TAG, "portscan host %u (all=%d)", (unsigned)host_idx, (int)all_ports);
}

void fpwn_marauder_deauth(FPwnMarauder* m) {
    furi_assert(m);
    fpwn_wifi_uart_send(m->uart, "attack -t deauth");

    furi_mutex_acquire(m->mutex, FuriWaitForever);
    m->state = FPwnMarauderStateDeauth;
    furi_mutex_release(m->mutex);

    FURI_LOG_I(TAG, "deauth attack started");
}

void fpwn_marauder_sniff_pmkid(FPwnMarauder* m) {
    furi_assert(m);
    fpwn_wifi_uart_send(m->uart, "sniffpmkid");

    furi_mutex_acquire(m->mutex, FuriWaitForever);
    m->state = FPwnMarauderStateSniffPmkid;
    furi_mutex_release(m->mutex);

    FURI_LOG_I(TAG, "pmkid sniff started");
}

void fpwn_marauder_stop(FPwnMarauder* m) {
    furi_assert(m);
    fpwn_wifi_uart_send(m->uart, "stopscan");

    furi_mutex_acquire(m->mutex, FuriWaitForever);
    m->state = FPwnMarauderStateIdle;
    furi_mutex_release(m->mutex);

    FURI_LOG_I(TAG, "stopped");
}

void fpwn_marauder_evil_portal(FPwnMarauder* m, const char* ssid) {
    furi_assert(m);
    /* Start evil portal — Marauder hosts a captive portal AP */
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "evilportal -s %s", ssid);
    fpwn_wifi_uart_send(m->uart, cmd);

    furi_mutex_acquire(m->mutex, FuriWaitForever);
    memset(m->creds, 0, sizeof(m->creds));
    m->cred_count = 0;
    m->state = FPwnMarauderStateEvilPortal;
    furi_mutex_release(m->mutex);

    FURI_LOG_I(TAG, "evil portal started: %s", ssid);
}

void fpwn_marauder_beacon_spam(FPwnMarauder* m) {
    furi_assert(m);
    fpwn_wifi_uart_send(m->uart, "attack -t beacon -l");

    furi_mutex_acquire(m->mutex, FuriWaitForever);
    m->state = FPwnMarauderStateBeaconSpam;
    furi_mutex_release(m->mutex);

    FURI_LOG_I(TAG, "beacon spam started");
}

void fpwn_marauder_scan_sta(FPwnMarauder* m) {
    furi_assert(m);
    furi_mutex_acquire(m->mutex, FuriWaitForever);
    memset(m->stations, 0, sizeof(m->stations));
    m->station_count = 0;
    m->state = FPwnMarauderStateStationScan;
    furi_mutex_release(m->mutex);
    fpwn_wifi_uart_send(m->uart, "scansta");
    FURI_LOG_I(TAG, "station scan started");
}

void fpwn_marauder_sniff_deauth(FPwnMarauder* m) {
    furi_assert(m);
    fpwn_wifi_uart_send(m->uart, "sniffdeauth");
    furi_mutex_acquire(m->mutex, FuriWaitForever);
    m->state = FPwnMarauderStateSniffDeauth;
    furi_mutex_release(m->mutex);
    FURI_LOG_I(TAG, "sniff deauth (handshake capture) started");
}

void fpwn_marauder_select_ap(FPwnMarauder* m, uint8_t ap_idx) {
    furi_assert(m);
    char cmd[32];
    snprintf(cmd, sizeof(cmd), "select -a %u", (unsigned)ap_idx);
    fpwn_wifi_uart_send(m->uart, cmd);
    FURI_LOG_I(TAG, "selected AP %u", (unsigned)ap_idx);
}

void fpwn_marauder_sniff_probe(FPwnMarauder* m) {
    furi_assert(m);
    fpwn_wifi_uart_send(m->uart, "sniffprobes");

    furi_mutex_acquire(m->mutex, FuriWaitForever);
    m->state = FPwnMarauderStateSniffProbe;
    furi_mutex_release(m->mutex);

    FURI_LOG_I(TAG, "probe sniff started");
}

void fpwn_marauder_deauth_targeted(FPwnMarauder* m, uint8_t ap_idx) {
    furi_assert(m);
    /* First select the AP, then deauth */
    fpwn_marauder_select_ap(m, ap_idx);
    furi_delay_ms(200); /* Small delay for Marauder to process select */
    fpwn_wifi_uart_send(m->uart, "attack -t deauth");

    furi_mutex_acquire(m->mutex, FuriWaitForever);
    m->state = FPwnMarauderStateDeauth;
    furi_mutex_release(m->mutex);

    FURI_LOG_I(TAG, "targeted deauth AP %u", (unsigned)ap_idx);
}

/* --------------------------------------------------------------------------
 * Log callback
 * -------------------------------------------------------------------------- */

void fpwn_marauder_set_log_callback(FPwnMarauder* m, FPwnWifiRxCallback cb, void* ctx) {
    furi_assert(m);
    m->log_callback_ctx = ctx;
    __DMB();
    m->log_callback = cb;
}

/* --------------------------------------------------------------------------
 * Accessors
 * -------------------------------------------------------------------------- */

FPwnMarauderState fpwn_marauder_get_state(FPwnMarauder* m) {
    furi_assert(m);
    furi_mutex_acquire(m->mutex, FuriWaitForever);
    FPwnMarauderState s = m->state;
    furi_mutex_release(m->mutex);
    return s;
}

uint32_t fpwn_marauder_copy_aps(FPwnMarauder* m, FPwnWifiAP* dst, uint32_t max_count) {
    furi_assert(m);
    furi_assert(dst);
    furi_mutex_acquire(m->mutex, FuriWaitForever);
    uint32_t count = (m->ap_count < max_count) ? m->ap_count : max_count;
    if(count > 0) memcpy(dst, m->aps, count * sizeof(FPwnWifiAP));
    furi_mutex_release(m->mutex);
    return count;
}

uint32_t fpwn_marauder_copy_hosts(FPwnMarauder* m, FPwnNetHost* dst, uint32_t max_count) {
    furi_assert(m);
    furi_assert(dst);
    furi_mutex_acquire(m->mutex, FuriWaitForever);
    uint32_t count = (m->host_count < max_count) ? m->host_count : max_count;
    if(count > 0) memcpy(dst, m->hosts, count * sizeof(FPwnNetHost));
    furi_mutex_release(m->mutex);
    return count;
}

uint32_t fpwn_marauder_copy_ports(FPwnMarauder* m, FPwnPortResult* dst, uint32_t max_count) {
    furi_assert(m);
    furi_assert(dst);
    furi_mutex_acquire(m->mutex, FuriWaitForever);
    uint32_t count = (m->port_count < max_count) ? m->port_count : max_count;
    if(count > 0) memcpy(dst, m->ports, count * sizeof(FPwnPortResult));
    furi_mutex_release(m->mutex);
    return count;
}

uint32_t fpwn_marauder_copy_stations(FPwnMarauder* m, FPwnStation* dst, uint32_t max_count) {
    furi_assert(m);
    furi_assert(dst);
    furi_mutex_acquire(m->mutex, FuriWaitForever);
    uint32_t count = (m->station_count < max_count) ? m->station_count : max_count;
    if(count > 0) memcpy(dst, m->stations, count * sizeof(FPwnStation));
    furi_mutex_release(m->mutex);
    return count;
}

uint32_t fpwn_marauder_copy_creds(FPwnMarauder* m, FPwnCapturedCred* dst, uint32_t max_count) {
    furi_assert(m);
    furi_assert(dst);
    furi_mutex_acquire(m->mutex, FuriWaitForever);
    uint32_t count = (m->cred_count < max_count) ? m->cred_count : max_count;
    if(count > 0) memcpy(dst, m->creds, count * sizeof(FPwnCapturedCred));
    furi_mutex_release(m->mutex);
    return count;
}

FPwnWifiAP* fpwn_marauder_get_aps(FPwnMarauder* m, uint32_t* count) {
    furi_assert(m);
    furi_assert(count);
    furi_mutex_acquire(m->mutex, FuriWaitForever);
    *count = m->ap_count;
    furi_mutex_release(m->mutex);
    return m->aps;
}

FPwnNetHost* fpwn_marauder_get_hosts(FPwnMarauder* m, uint32_t* count) {
    furi_assert(m);
    furi_assert(count);
    furi_mutex_acquire(m->mutex, FuriWaitForever);
    *count = m->host_count;
    furi_mutex_release(m->mutex);
    return m->hosts;
}

FPwnPortResult* fpwn_marauder_get_ports(FPwnMarauder* m, uint32_t* count) {
    furi_assert(m);
    furi_assert(count);
    furi_mutex_acquire(m->mutex, FuriWaitForever);
    *count = m->port_count;
    furi_mutex_release(m->mutex);
    return m->ports;
}

FPwnStation* fpwn_marauder_get_stations(FPwnMarauder* m, uint32_t* count) {
    furi_assert(m);
    furi_assert(count);
    furi_mutex_acquire(m->mutex, FuriWaitForever);
    *count = m->station_count;
    furi_mutex_release(m->mutex);
    return m->stations;
}

FPwnCapturedCred* fpwn_marauder_get_creds(FPwnMarauder* m, uint32_t* count) {
    furi_assert(m);
    furi_assert(count);
    furi_mutex_acquire(m->mutex, FuriWaitForever);
    *count = m->cred_count;
    furi_mutex_release(m->mutex);
    return m->creds;
}

/* Poll for deferred 'list -a' after stopscan.  Called from the scan timer.
 * Sends 'list -a' once 1.5 s have elapsed since the stopscan command. */
void fpwn_marauder_poll_list(FPwnMarauder* m) {
    furi_assert(m);
    furi_mutex_acquire(m->mutex, FuriWaitForever);
    if(m->list_pending && (furi_get_tick() - m->stop_tick) > furi_ms_to_ticks(1500)) {
        m->list_pending = false;
        furi_mutex_release(m->mutex);
        fpwn_wifi_uart_send(m->uart, "list -a");
        FURI_LOG_I(TAG, "sent deferred 'list -a'");
        return;
    }
    furi_mutex_release(m->mutex);
}

/* Returns the furi_get_tick() value recorded when the last AP scan started.
 * Used by the timer callback to auto-stop after a fixed interval. */
uint32_t fpwn_marauder_get_scan_start(FPwnMarauder* m) {
    furi_assert(m);
    furi_mutex_acquire(m->mutex, FuriWaitForever);
    uint32_t tick = m->scan_start_tick;
    furi_mutex_release(m->mutex);
    return tick;
}
