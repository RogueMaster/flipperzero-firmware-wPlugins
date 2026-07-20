// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt and FlipDeFlock contributors
#include "esp_link.h"
#include "../recon_app_i.h"
#include "flock_db.h"

#include <expansion/expansion.h>
#include <stdlib.h>
#include <string.h>

#define ESP_RX_BUF   512
// Marauder AP-scan / sniffraw lines can be long; an overlong line is dropped
// whole, so keep this generous to avoid losing MACs on a long generic-backend line.
#define ESP_LINE_MAX 384

typedef enum {
    EspEvtStop = (1 << 0),
    EspEvtRx = (1 << 1),
} EspEvt;

#define ESP_ALL_EVTS (EspEvtStop | EspEvtRx)

struct EspLink {
    ReconApp* app;
    FuriThread* thread;
    FuriStreamBuffer* rx_stream;
    FuriHalSerialHandle* serial;
    volatile bool running;
    char line[ESP_LINE_MAX];
    size_t line_len;
    uint32_t lines; /**< total completed RX lines (heartbeat) */
};

// ---- parsing helpers -----------------------------------------------------

static int hexval(char c) {
    if(c >= '0' && c <= '9') return c - '0';
    if(c >= 'a' && c <= 'f') return c - 'a' + 10;
    if(c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/** Parse "aabbccddeeff" (no separators, 12 hex chars) into 6 bytes. */
static bool parse_mac_compact(const char* s, uint8_t mac[6]) {
    for(int i = 0; i < 6; i++) {
        int hi = hexval(s[i * 2]);
        int lo = hexval(s[i * 2 + 1]);
        if(hi < 0 || lo < 0) return false;
        mac[i] = (uint8_t)((hi << 4) | lo);
    }
    return true;
}

/** Try to read a "hh:hh:hh:hh:hh:hh" MAC starting at p. */
static bool parse_mac_colon(const char* p, uint8_t mac[6]) {
    for(int i = 0; i < 6; i++) {
        int hi = hexval(p[i * 3]);
        int lo = hexval(p[i * 3 + 1]);
        if(hi < 0 || lo < 0) return false;
        if(i < 5 && p[i * 3 + 2] != ':') return false;
        mac[i] = (uint8_t)((hi << 4) | lo);
    }
    return true;
}

static FlockConfidence conf_from_int(int c) {
    switch(c) {
    case 3:
        return FlockConfidenceConfirmed;
    case 2:
        return FlockConfidenceLikely;
    case 1:
        return FlockConfidencePossible;
    default:
        return FlockConfidenceNone;
    }
}

// ---- companion protocol --------------------------------------------------

static void esp_parse_companion(EspLink* esp, char* line) {
    if(strncmp(line, "FLOCKCO", 7) == 0) {
        recon_app_set_esp_status(esp->app, 0, 0, 0, true);
        return;
    }
    // ---- WiFi security scan: WBEGIN / W,... / WEND ----
    if(strncmp(line, "WBEGIN", 6) == 0) {
        recon_app_wifi_begin(esp->app);
        return;
    }
    if(strncmp(line, "WEND", 4) == 0) {
        recon_app_wifi_end(esp->app);
        return;
    }
    if(strncmp(line, "DA,", 3) == 0) {
        // DA,<bssid>,<ch>  deauth/disassoc attack target attribution
        char* f[3];
        int n = 0;
        char* p = line;
        f[n++] = p;
        while(*p && n < 3) {
            if(*p == ',') {
                *p = '\0';
                f[n++] = p + 1;
            }
            p++;
        }
        if(n >= 3) {
            uint8_t bssid[6];
            if(strlen(f[1]) >= 12 && parse_mac_compact(f[1], bssid)) {
                recon_app_add_deauth_target(esp->app, bssid, (uint8_t)atoi(f[2]));
            }
        }
        return;
    }
    if(strncmp(line, "ATK,", 4) == 0) {
        // ATK,<kind>,<value>  active attack-tool signature from the companion:
        //   blespam     a flood of impersonation BLE adverts (Flipper/ESP spam)
        //   beaconflood a flood of distinct beaconing APs (Marauder / Pineapple)
        //   probeflood  an abnormal probe-request rate
        // <value> is the count/rate the companion measured. Older app builds
        // simply ignore this unknown line; older firmware never sends it.
        char* f[3];
        int n = 0;
        char* p = line;
        f[n++] = p;
        while(*p && n < 3) {
            if(*p == ',') {
                *p = '\0';
                f[n++] = p + 1;
            }
            p++;
        }
        if(n >= 2) {
            recon_app_set_attack(esp->app, f[1], (n >= 3) ? strtoul(f[2], NULL, 10) : 0);
        }
        return;
    }
    if(strncmp(line, "LOC,", 4) == 0) {
        // LOC,<rssi>[,<mac>]  live signal strength for the active Locator target.
        // The kind/label live app-side, so we only need the rssi here.
        recon_app_set_locate_rssi(esp->app, (int8_t)atoi(line + 4));
        return;
    }
    // ---- BLE scan: BBEGIN / BLE,... / BEND ----
    if(strncmp(line, "BBEGIN", 6) == 0) {
        recon_app_ble_begin(esp->app);
        return;
    }
    if(strncmp(line, "BEND", 4) == 0) {
        recon_app_ble_end(esp->app);
        return;
    }
    if(strncmp(line, "BLE,", 4) == 0) {
        // BLE,<addr>,<rssi>,<cat>,<company>,<name>[,<mfghex>][,rv=1]
        // Up to two trailing fields follow <name>: mfghex (pure hex, NO '=') and
        // rv=1 (Raven GATT flag, contains '='). They can appear in either order
        // and either may be absent (older firmware omits both). 8 slots hold the
        // 6 base fields (BLE..name) plus both optional trailers, so neither
        // trailer gets folded back into <name>.
        char* f[8];
        int n = 0;
        char* p = line;
        f[n++] = p;
        while(*p && n < 8) {
            if(*p == ',') {
                *p = '\0';
                f[n++] = p + 1;
            }
            p++;
        }
        if(n < 5) return;
        uint8_t addr[6];
        if(strlen(f[1]) < 12 || !parse_mac_compact(f[1], addr)) return;
        // Walk the trailing fields after <name> (f[6], f[7], ...). Each is
        // either the raw mfg-data hex (Flock 0x09C8) -- decoded here for the
        // serial extractor -- or the rv=1 Raven-GATT flag. Distinguish them by
        // the presence of '=': rv=1 has one, mfghex (pure hex) never does.
        uint8_t mfg[32];
        size_t mfg_len = 0;
        bool raven_gatt = false;
        for(int fi = 6; fi < n; fi++) {
            const char* t = f[fi];
            if(strchr(t, '=')) {
                // Keyed flag field. Today the only one is rv=1 (Raven GATT).
                if(strcmp(t, "rv=1") == 0) raven_gatt = true;
            } else if(mfg_len == 0) {
                // Pure-hex mfg blob. Decode to bytes (only the first one wins).
                for(size_t i = 0; mfg_len < sizeof(mfg); i += 2) {
                    int hi = hexval(t[i]);
                    if(hi < 0) break;
                    int lo = hexval(t[i + 1]);
                    if(lo < 0) break;
                    mfg[mfg_len++] = (uint8_t)((hi << 4) | lo);
                }
            }
        }
        recon_app_ble_add(
            esp->app,
            addr,
            (n >= 6) ? f[5] : "",
            (int8_t)atoi(f[2]),
            (uint8_t)atoi(f[3]),
            (uint16_t)atoi(f[4]),
            mfg_len ? mfg : NULL,
            mfg_len,
            raven_gatt);
        return;
    }
    if(line[0] == 'W' && line[1] == ',') {
        // W,<bssid>,<rssi>,<ch>,<auth>,<pair>,<grp>,<wps>,<ssid>
        char* f[9];
        int n = 0;
        char* p = line;
        f[n++] = p;
        while(*p && n < 9) {
            if(*p == ',') {
                *p = '\0';
                f[n++] = p + 1;
            }
            p++;
        }
        if(n < 8) return;
        uint8_t bssid[6];
        if(strlen(f[1]) < 12 || !parse_mac_compact(f[1], bssid)) return;
        recon_app_wifi_add(
            esp->app,
            bssid,
            (n >= 9) ? f[8] : "",
            (int8_t)atoi(f[2]),
            (uint8_t)atoi(f[3]),
            (uint8_t)atoi(f[4]),
            (uint8_t)atoi(f[5]),
            atoi(f[7]) != 0);
        return;
    }
    if(line[0] == 'S' && line[1] == ',') {
        // S,<frames>,<hits>,<ch>[,<deauths>]
        char* f[5];
        int n = 0;
        char* p = line;
        f[n++] = p;
        while(*p && n < 5) {
            if(*p == ',') {
                *p = '\0';
                f[n++] = p + 1;
            }
            p++;
        }
        if(n >= 4) {
            recon_app_set_esp_status(
                esp->app,
                strtoul(f[1], NULL, 10),
                strtoul(f[2], NULL, 10),
                (uint8_t)atoi(f[3]),
                true);
        }
        if(n >= 5) {
            recon_app_set_deauths(esp->app, strtoul(f[4], NULL, 10));
        }
        return;
    }
    if(line[0] == 'D' && line[1] == ',') {
        // D,<mac>,<rssi>,<ch>,<type>,<conf>,<ssid>[,fp=<hex32>]
        char* f[8];
        int n = 0;
        char* p = line;
        f[n++] = p;
        while(*p && n < 8) {
            if(*p == ',') {
                *p = '\0';
                f[n++] = p + 1;
            }
            p++;
        }
        if(n < 6) return;
        uint8_t mac[6];
        if(strlen(f[1]) < 12 || !parse_mac_compact(f[1], mac)) return;
        int8_t rssi = (int8_t)atoi(f[2]);
        uint8_t ch = (uint8_t)atoi(f[3]);
        char ftype = f[4][0] ? f[4][0] : 'O';
        FlockConfidence conf = conf_from_int(atoi(f[5]));
        const char* ssid = (n >= 7) ? f[6] : "";

        // B1: trailing IE-fingerprint field "fp=<hex32>" (probe requests only).
        // Older firmware omits it; backward-compatible. The fp is a
        // MAC-independent device-CLASS signature -- if it matches the curated
        // (currently empty) Flock table we can rescue a detection whose MAC was
        // randomized, and label its source "probe-fp" (ftype 'F').
        uint32_t fp = 0;
        // fp= is a trailing field AFTER the ssid (f[6]); start at f[7] so an SSID
        // that literally begins "fp=" can't be misread as the IE-fingerprint.
        for(int i = 7; i < n; i++) {
            if(strncmp(f[i], "fp=", 3) == 0) {
                fp = (uint32_t)strtoul(f[i] + 3, NULL, 16);
                break;
            }
        }
        FlockIeFp fp_src = flock_ie_fp_match(fp);
        if(fp_src == FlockIeFpBuiltin) {
            // Verified compiled-in class fp. + Flock OUI -> CONFIRMED; otherwise
            // (e.g. a wildcard probe from a randomized/unknown MAC) -> a candidate
            // device-CLASS match. Never weaker than the ESP's own score.
            FlockConfidence fp_conf = flock_oui_match(mac) ? FlockConfidenceConfirmed :
                                                             FlockConfidenceProbeFp;
            if(fp_conf > conf) conf = fp_conf;
            ftype = 'F'; // source label "probe-fp" in the detail scene
        } else if(fp_src == FlockIeFpUser) {
            // UNVERIFIED user fp (signatures.json): a candidate device-CLASS match
            // ONLY -- capped at "Class?", never Confirmed even with a Flock OUI.
            if(FlockConfidenceProbeFp > conf) conf = FlockConfidenceProbeFp;
            ftype = 'F';
        }

        // Pass the raw fp through so the detail screen can show it (for seeding),
        // whether or not it matched a known fingerprint yet.
        recon_app_report_flock(esp->app, mac, ssid, rssi, ch, ftype, conf, fp);
    }
}

// ---- generic / Marauder scraper -----------------------------------------

// Marauder sniff commands the generic backend can drive (index = settings.marauder_cmd).
// "sniffprobe" is first/default: Flock ALPRs are caught by the Wi-Fi probe
// requests they spray to phone home (the flock-you method).
static const char* const ESP_MARAUDER_CMDS[] = {
    "sniffprobe", // client probe requests: "... Client: <mac> Requesting: <ssid>"
    "scanap", // access points:        "<rssi> Ch: <n> <bssid> ESSID: <ssid>"
    "sniffbeacon", // beacon frames (same line format as scanap)
    "sniffraw", // raw:                   "MAC: <mac> CH: <n> ... SSID: <ssid>"
};
#define ESP_MARAUDER_CMD_COUNT (sizeof(ESP_MARAUDER_CMDS) / sizeof(ESP_MARAUDER_CMDS[0]))

/**
 * Marauder prints the network name after a known label. Return a pointer to the
 * name (rest of line) or NULL. Covers scanap/sniffbeacon ("ESSID: "),
 * sniffraw ("SSID: ") and sniffprobe ("Requesting: ").
 */
static const char* marauder_extract_ssid(const char* line) {
    static const char* const labels[] = {"ESSID: ", "SSID: ", "Requesting: "};
    static char buf[RECON_SSID_LEN]; // 33: SSIDs are max 32 bytes
    for(size_t k = 0; k < 3; k++) {
        const char* p = strstr(line, labels[k]);
        if(!p) continue;
        p += strlen(labels[k]);
        // Bound to an SSID length (preserves embedded spaces) so trailing
        // fields on the line aren't absorbed and a far-away "flock" token can't
        // spuriously raise confidence. Single-threaded ESP worker -> static ok.
        size_t i = 0;
        for(; i < RECON_SSID_LEN - 1 && p[i] && p[i] != '\r' && p[i] != '\n'; i++)
            buf[i] = p[i];
        buf[i] = '\0';
        return buf;
    }
    return NULL;
}

static void esp_parse_generic(EspLink* esp, char* line) {
    // Liveness/connected is handled by the worker via recon_app_set_esp_lines().

    // Prefer the labelled SSID Marauder prints over a whole-line scan, so the
    // confidence is attributed to the actual network name and we can display it.
    const char* ssid = marauder_extract_ssid(line);
    FlockConfidence ssid_conf = flock_ssid_confidence(ssid ? ssid : line);
    size_t len = strlen(line);

    // Count MAC tokens first. A line-wide SSID match can only be safely
    // attributed to a specific MAC when the line names exactly one MAC;
    // otherwise an unrelated "flock" substring would promote every MAC on a
    // multi-record log line. So: OUI matches always count; a lone MAC may take
    // the SSID confidence; extra non-OUI MACs are ignored.
    int mac_count = 0;
    for(size_t i = 0; i + 17 <= len; i++) {
        uint8_t mac[6];
        if(parse_mac_colon(line + i, mac)) {
            mac_count++;
            i += 16;
        }
    }
    bool single = (mac_count == 1);

    for(size_t i = 0; i + 17 <= len; i++) {
        uint8_t mac[6];
        if(!parse_mac_colon(line + i, mac)) continue;
        i += 16;

        bool oui = flock_oui_match(mac);
        FlockConfidence conf;
        if(oui) {
            // OUI vendor prefix; SSID naming on the same line can raise it.
            conf = (ssid_conf > FlockConfidencePossible) ? ssid_conf : FlockConfidencePossible;
        } else if(single && ssid_conf != FlockConfidenceNone) {
            // Sole MAC on a line that names a Flock SSID -> attribute to it.
            conf = ssid_conf;
        } else {
            continue;
        }

        recon_app_report_flock(esp->app, mac, ssid ? ssid : "", 0, 0, 'O', conf, 0);
    }
}

// ---- worker --------------------------------------------------------------

static void esp_rx_isr(FuriHalSerialHandle* handle, FuriHalSerialRxEvent event, void* context) {
    EspLink* esp = context;
    if(event == FuriHalSerialRxEventData) {
        uint8_t data = furi_hal_serial_async_rx(handle);
        furi_stream_buffer_send(esp->rx_stream, &data, 1, 0);
        furi_thread_flags_set(furi_thread_get_id(esp->thread), EspEvtRx);
    }
}

static int32_t esp_worker(void* context) {
    EspLink* esp = context;
    uint8_t byte;
    while(true) {
        uint32_t evt = furi_thread_flags_wait(ESP_ALL_EVTS, FuriFlagWaitAny, FuriWaitForever);
        if(evt & FuriFlagError) continue;
        if(evt & EspEvtStop) break;
        if(evt & EspEvtRx) {
            while(furi_stream_buffer_receive(esp->rx_stream, &byte, 1, 0) == 1) {
                if(byte == '\n' || byte == '\r') {
                    if(esp->line_len > 0) {
                        esp->line[esp->line_len] = '\0';
                        // Every completed line counts as RX activity.
                        esp->lines++;
                        recon_app_set_esp_lines(esp->app, esp->lines);
                        if(esp->app->settings.backend == EspBackendCompanion) {
                            esp_parse_companion(esp, esp->line);
                        } else {
                            esp_parse_generic(esp, esp->line);
                        }
                        esp->line_len = 0;
                    }
                } else if(esp->line_len < ESP_LINE_MAX - 1) {
                    esp->line[esp->line_len++] = (char)byte;
                } else {
                    esp->line_len = 0;
                }
            }
        }
    }
    return 0;
}

// ---- lifecycle -----------------------------------------------------------

void esp_link_send(EspLink* esp, const char* cmd) {
    if(!esp->running || !esp->serial) return;
    furi_hal_serial_tx(esp->serial, (const uint8_t*)cmd, strlen(cmd));
    furi_hal_serial_tx(esp->serial, (const uint8_t*)"\n", 1);
}

EspLink* esp_link_alloc(void* app) {
    EspLink* esp = malloc(sizeof(EspLink));
    memset(esp, 0, sizeof(EspLink));
    esp->app = app;
    return esp;
}

void esp_link_free(EspLink* esp) {
    furi_assert(esp);
    if(esp->running) esp_link_stop(esp);
    free(esp);
}

void esp_link_start(EspLink* esp) {
    if(esp->running) return;
    ReconApp* app = esp->app;

    // Free the USART from the expansion module manager so we can own it.
    Expansion* expansion = furi_record_open(RECORD_EXPANSION);
    expansion_disable(expansion);
    furi_record_close(RECORD_EXPANSION);

    esp->line_len = 0;
    esp->rx_stream = furi_stream_buffer_alloc(ESP_RX_BUF, 1);
    esp->thread = furi_thread_alloc_ex("ReconEspWorker", 1536, esp_worker, esp);
    furi_thread_start(esp->thread);

    esp->serial = furi_hal_serial_control_acquire((FuriHalSerialId)app->settings.esp_uart);
    if(!esp->serial) {
        furi_thread_flags_set(furi_thread_get_id(esp->thread), EspEvtStop);
        furi_thread_join(esp->thread);
        furi_thread_free(esp->thread);
        furi_stream_buffer_free(esp->rx_stream);
        esp->thread = NULL;
        esp->rx_stream = NULL;
        Expansion* exp = furi_record_open(RECORD_EXPANSION);
        expansion_enable(exp);
        furi_record_close(RECORD_EXPANSION);
        return;
    }
    furi_hal_serial_init(esp->serial, app->settings.esp_baud);
    furi_hal_serial_async_rx_start(esp->serial, esp_rx_isr, esp, false);
    esp->running = true;

    // Kick the board into reporting.
    if(app->settings.backend == EspBackendCompanion) {
        esp_link_send(esp, "ver");
        esp_link_send(esp, "scan");
    } else {
        // Marauder: clear any running mode, then start the chosen sniffer.
        // Marauder runs one global mode at a time and needs a stop first.
        uint8_t idx = app->settings.marauder_cmd;
        if(idx >= ESP_MARAUDER_CMD_COUNT) idx = 0;
        esp_link_send(esp, "stopscan");
        esp_link_send(esp, ESP_MARAUDER_CMDS[idx]);
    }
}

void esp_link_stop(EspLink* esp) {
    if(!esp->running && !esp->thread) return;

    if(esp->running && esp->serial) {
        if(esp->app->settings.backend == EspBackendCompanion) {
            esp_link_send(esp, "stop");
        } else {
            esp_link_send(esp, "stopscan");
        }
        // Let the stop command fully drain before tearing down the UART, or the
        // last bytes get cut off and the board keeps scanning after we exit.
        furi_hal_serial_tx_wait_complete(esp->serial);
    }
    if(esp->serial) {
        furi_hal_serial_async_rx_stop(esp->serial);
        furi_hal_serial_deinit(esp->serial);
        furi_hal_serial_control_release(esp->serial);
        esp->serial = NULL;
    }
    if(esp->thread) {
        furi_thread_flags_set(furi_thread_get_id(esp->thread), EspEvtStop);
        furi_thread_join(esp->thread);
        furi_thread_free(esp->thread);
        esp->thread = NULL;
    }
    if(esp->rx_stream) {
        furi_stream_buffer_free(esp->rx_stream);
        esp->rx_stream = NULL;
    }
    esp->running = false;

    Expansion* expansion = furi_record_open(RECORD_EXPANSION);
    expansion_enable(expansion);
    furi_record_close(RECORD_EXPANSION);
}
