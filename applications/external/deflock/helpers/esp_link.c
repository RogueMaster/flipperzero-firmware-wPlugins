// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt and FlipDeFlock contributors
#include "esp_link.h"
#include "../recon_app_i.h"
#include "flock_db.h"
#include "esp_parser.h"

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
    bool skip_line; /**< dropping the remainder of an overlong (overflowed) line */
    uint32_t lines; /**< total completed RX lines (heartbeat) */
    uint32_t dropped; /**< overlong lines dropped whole (wire-protocol health metric) */
};

// ---- parsing helpers -----------------------------------------------------

/** Try to read a "hh:hh:hh:hh:hh:hh" MAC starting at p. (Marauder/generic path;
 *  the companion path's compact-MAC + field parsing live in esp_parser.c.) */
static bool parse_mac_colon(const char* p, uint8_t mac[6]) {
    for(int i = 0; i < 6; i++) {
        int hi = esp_hexval(p[i * 3]);
        int lo = esp_hexval(p[i * 3 + 1]);
        if(hi < 0 || lo < 0) return false;
        if(i < 5 && p[i * 3 + 2] != ':') return false;
        mac[i] = (uint8_t)((hi << 4) | lo);
    }
    return true;
}

// ---- companion protocol --------------------------------------------------

// Apply one already-parsed companion record to the app. This is the ONLY place
// the companion path mutates ReconApp; esp_parser.c does the pure line -> record
// decoding (parse != mutate), which is what makes the wire protocol host-testable.
static void esp_apply_companion(EspLink* esp, const EspMsg* m) {
    ReconApp* app = esp->app;
    switch(m->type) {
    case EspMsgBanner:
        recon_app_set_esp_status(app, 0, 0, 0, true);
        // Version gate: flag (don't refuse) if the companion speaks a different
        // wire-protocol version than we do, so a mismatch surfaces instead of
        // silently mis-parsing. Version 0 = old FW with no version field = OK.
        recon_app_set_esp_proto(
            app,
            m->u.banner.version,
            m->u.banner.version != 0 && m->u.banner.version != ESP_PROTO_VERSION);
        break;
    case EspMsgWifiBegin:
        recon_app_wifi_begin(app);
        break;
    case EspMsgWifiEnd:
        recon_app_wifi_end(app);
        break;
    case EspMsgWifiAp:
        recon_app_wifi_add(
            app,
            m->u.wifi.bssid,
            m->u.wifi.ssid,
            m->u.wifi.rssi,
            m->u.wifi.channel,
            m->u.wifi.auth,
            m->u.wifi.pairwise,
            m->u.wifi.wps);
        break;
    case EspMsgBleBegin:
        recon_app_ble_begin(app);
        break;
    case EspMsgBleEnd:
        recon_app_ble_end(app);
        break;
    case EspMsgBleDev:
        recon_app_ble_add(
            app,
            m->u.ble.addr,
            m->u.ble.name,
            m->u.ble.rssi,
            m->u.ble.cat,
            m->u.ble.company,
            m->u.ble.mfg_len ? m->u.ble.mfg : NULL,
            m->u.ble.mfg_len,
            m->u.ble.raven_gatt);
        break;
    case EspMsgFlock:
        recon_app_report_flock(
            app,
            m->u.flock.mac,
            m->u.flock.ssid,
            m->u.flock.rssi,
            m->u.flock.channel,
            m->u.flock.ftype,
            m->u.flock.conf,
            m->u.flock.fp);
        break;
    case EspMsgDeauthTarget:
        recon_app_add_deauth_target(app, m->u.deauth.bssid, m->u.deauth.channel);
        break;
    case EspMsgAttack:
        recon_app_set_attack(app, m->u.attack.kind, m->u.attack.value);
        break;
    case EspMsgLocate:
        recon_app_set_locate_rssi(app, m->u.locate.rssi);
        break;
    case EspMsgStatus:
        recon_app_set_esp_status(
            app, m->u.status.frames, m->u.status.hits, m->u.status.channel, true);
        if(m->u.status.have_deauths) recon_app_set_deauths(app, m->u.status.deauths);
        break;
    case EspMsgIgnore:
    default:
        break;
    }
}

static void esp_parse_companion(EspLink* esp, char* line) {
    EspMsg msg;
    if(esp_parse_companion_line(line, &msg) != EspMsgIgnore) {
        esp_apply_companion(esp, &msg);
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
                    if(esp->skip_line) {
                        // End of an overlong line -- drop it whole (don't parse the
                        // tail as a spurious record); resume on the next line.
                        esp->skip_line = false;
                        esp->line_len = 0;
                    } else if(esp->line_len > 0) {
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
                } else if(esp->skip_line) {
                    // still discarding the remainder of the overlong line
                } else if(esp->line_len < ESP_LINE_MAX - 1) {
                    esp->line[esp->line_len++] = (char)byte;
                } else {
                    // Overflow: drop this whole line instead of re-parsing its tail
                    // as a new (injectable) record. Count it as a health metric so a
                    // chronic line-length mismatch between firmware and app is visible.
                    esp->skip_line = true;
                    esp->line_len = 0;
                    esp->dropped++;
                    recon_app_set_esp_dropped(esp->app, esp->dropped);
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
        // UART acquire failed -- another owner holds it (commonly the GPS port).
        // Surface it so scenes render "UART busy" instead of a dead "connecting...".
        recon_app_set_esp_link_state(app, EspLinkPortBusy);
        return;
    }
    furi_hal_serial_init(esp->serial, app->settings.esp_baud);
    furi_hal_serial_async_rx_start(esp->serial, esp_rx_isr, esp, false);
    esp->running = true;
    recon_app_set_esp_link_state(app, EspLinkRunning);

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
    recon_app_set_esp_link_state(esp->app, EspLinkStopped);

    Expansion* expansion = furi_record_open(RECORD_EXPANSION);
    expansion_enable(expansion);
    furi_record_close(RECORD_EXPANSION);
}
