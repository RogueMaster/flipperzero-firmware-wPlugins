// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt and FlipDeFlock contributors
#include "esp_link.h"
#include "../recon_app_i.h"
#include "flock_db.h"
#include "esp_parser.h"
#include "gps_link.h" // gps_apply_nmea: the shared decode+publish path
#include "marauder_scan.h"

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
    uint32_t dropped; /**< lines dropped whole: overlong, or corrupted by an RX drop */
    /**
     * Bytes the ISR could not hand to the worker because the stream buffer was
     * full. Written ONLY by the ISR, read only by the worker -- single writer, so
     * no lock is needed. At 921600 baud a full buffer drops bytes MID-LINE, which
     * silently corrupts a record rather than failing it; the worker compares this
     * against @ref line_rx_dropped to discard any line that lost bytes.
     */
    volatile uint32_t rx_dropped;
    uint32_t line_rx_dropped; /**< rx_dropped as of the start of the current line */
};

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
        // Ask the GUI thread to re-send the GPS relay config now that the board
        // has demonstrably booted and is listening. The session sends it once at
        // start-up, which a board still coming up simply misses -- and a silently
        // dropped config is indistinguishable from a dead GPS module, which is
        // most of why issue #5's GPS problem took four rounds. A banner also
        // arrives on every ESP reboot, so a mid-session power blip re-arms the
        // relay by itself.
        //
        // A FLAG, not a send. This runs on the ESP worker thread, and calling
        // furi_hal_serial_tx here would race the commands the GUI thread sends on
        // the same handle when entering a scan scene. Identical discipline to
        // alert_pending: the worker only ever raises, the GUI tick acts.
        recon_app_request_gps_cfg(app);
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
            m->u.flock.fp,
            m->u.flock.dev_class,
            m->u.flock.hidden);
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
    case EspMsgGpsNmea:
        // Only when the operator actually selected the companion as the GPS
        // source. A board that relays NMEA must not be able to override a GPS
        // the user wired to the Flipper itself, and must not quietly start
        // geotagging when GPS is switched off altogether.
        //
        // m is const, but u.gps.nmea is a char* INTO the mutable line buffer --
        // nmea_parse_line() tokenizes in place, exactly as on the direct path.
        if(app->settings.gps_enabled && app->settings.gps_source == ReconGpsSourceCompanion) {
            gps_apply_nmea(app, m->u.gps.nmea);
        }
        break;
    case EspMsgChip:
        recon_app_set_chip(
            app,
            m->u.chip.target,
            m->u.chip.gpio_count,
            m->u.chip.gps_pin_mask,
            m->u.chip.has_5ghz);
        break;
    case EspMsgBand:
        recon_app_set_band(app, m->u.band.sel, m->u.band.channels);
        break;
    case EspMsgGpsCfg:
        // Recorded regardless of the current source setting: it is the answer to
        // a question we asked, and the badge decides what to make of it. Storing
        // it only when the companion source is selected would lose the reply to
        // the explicit `gps off` the session sends in the other direction.
        recon_app_set_gps_relay(app, m->u.gpscfg.on, m->u.gpscfg.pin, m->u.gpscfg.baud);
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
 * Apply a scraped generic/Marauder line to the app.
 *
 * The DECISION (which MACs are hits, at what rung) lives in the pure, host-tested
 * marauder_scan.c; this function only applies the result. Keeping the two apart
 * is what makes the precision rules -- especially the single-MAC attribution
 * rule -- regression-testable, and it mirrors the split the companion path
 * already uses.
 */
static void esp_parse_generic(EspLink* esp, char* line) {
    // Liveness/connected is handled by the worker via recon_app_set_esp_lines().
    MarauderScan scan;
    marauder_scan_line(line, &scan);

    for(int i = 0; i < scan.hit_count; i++) {
        const MarauderHit* h = &scan.hits[i];
        recon_app_report_flock(
            esp->app,
            h->mac,
            scan.have_ssid ? scan.ssid : "",
            0,
            0,
            'O',
            h->conf,
            0,
            h->dev_class,
            false); // Marauder's scraped text carries no hidden-SSID signal
    }

    // A line naming more MACs than one scan can carry is pathological; surface it
    // as wire-protocol health rather than dropping it silently.
    if(scan.dropped > 0) {
        esp->dropped += (uint32_t)scan.dropped;
        recon_app_set_esp_dropped(esp->app, esp->dropped);
    }
}

// ---- worker --------------------------------------------------------------

static void esp_rx_isr(FuriHalSerialHandle* handle, FuriHalSerialRxEvent event, void* context) {
    EspLink* esp = context;
    if(event == FuriHalSerialRxEventData) {
        uint8_t data = furi_hal_serial_async_rx(handle);
        // 0 timeout: an ISR must never block. A full buffer therefore DROPS the
        // byte, and a byte lost mid-line corrupts that record rather than failing
        // it -- the parser would happily read the damaged remainder as a valid
        // one. Count it so the worker can discard the affected line instead.
        if(furi_stream_buffer_send(esp->rx_stream, &data, 1, 0) != 1) {
            esp->rx_dropped++;
        }
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
                    } else if(esp->rx_dropped != esp->line_rx_dropped) {
                        // The ISR dropped at least one byte while this line was
                        // being assembled, so what we hold is a record with a hole
                        // in it -- not a shorter valid one. Discard it whole, for
                        // the same reason an overlong line is discarded whole.
                        esp->line_len = 0;
                        esp->dropped++;
                        recon_app_set_esp_dropped(esp->app, esp->dropped);
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
                    // A line boundary is the only safe place to re-sync the drop
                    // watermark: whatever was lost belonged to the line just ended.
                    esp->line_rx_dropped = esp->rx_dropped;
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

// Index-aligned with ReconEspBand.
static const char* const esp_band_cmd[] = {"band 2g", "band 5g", "band all"};

void esp_link_send_band(EspLink* esp) {
    ReconApp* app = esp->app;
    if(app->settings.backend != EspBackendCompanion) return;
    uint8_t i = app->settings.esp_band;
    if(i >= ReconEspBandCount) i = ReconEspBand24;
    // Sent every session, not only when non-default: the board keeps its own
    // selection across app restarts, so a companion left on "all" by an earlier
    // run would silently keep the slow sweep even after the setting was changed.
    esp_link_send(esp, esp_band_cmd[i]);
}

void esp_link_send_gps_cfg(EspLink* esp) {
    ReconApp* app = esp->app;
    if(app->settings.backend != EspBackendCompanion) return;

    // Sent in BOTH directions so the board's state always matches the setting:
    // without the explicit "off", a board left relaying from an earlier session
    // keeps overriding a GPS the user has since moved to the Flipper's own UART.
    char cmd[32];
    if(app->settings.gps_enabled && app->settings.gps_source == ReconGpsSourceCompanion) {
        snprintf(
            cmd,
            sizeof(cmd),
            "gps %u %lu",
            (unsigned)app->settings.esp_gps_pin,
            (unsigned long)app->settings.gps_baud);
    } else {
        snprintf(cmd, sizeof(cmd), "gps off");
    }
    recon_app_gps_relay_pending(app);
    esp_link_send(esp, cmd);
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
