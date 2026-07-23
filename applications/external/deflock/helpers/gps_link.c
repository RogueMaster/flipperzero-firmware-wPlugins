// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt and FlipDeFlock contributors
#include "gps_link.h"
#include "../recon_app_i.h"
#include "gps_parser.h"

#include <stdlib.h>
#include <string.h>

#define GPS_RX_BUF   256
#define GPS_LINE_MAX 128

typedef enum {
    GpsEvtStop = (1 << 0),
    GpsEvtRx = (1 << 1),
} GpsEvt;

#define GPS_ALL_EVTS (GpsEvtStop | GpsEvtRx)

struct GpsLink {
    ReconApp* app;
    FuriThread* thread;
    FuriStreamBuffer* rx_stream;
    FuriHalSerialHandle* serial;
    volatile bool running;
    char line[GPS_LINE_MAX];
    size_t line_len;
};

// Pure NMEA parsing (nmea_to_decimal / gps_coord_sane / nmea_tokenize /
// nmea_parse_line) lives in gps_parser.{c,h} so it is host-testable; this file is
// the thin adapter that applies a parsed NmeaFix to ReconApp under the lock.

static void gps_publish(GpsLink* gps, float lat, float lon, int sats, bool valid) {
    ReconApp* app = gps->app;
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    if(valid && gps_coord_sane(lat, lon)) {
        app->gps_lat = lat;
        app->gps_lon = lon;
        app->gps_valid = true;
    } else if(!valid) {
        // Explicit lock loss (RMC 'V' / GGA fixq==0 / GLL 'V'): stop reporting the
        // last fix as current -- previously we kept it, so a lost lock still tagged
        // detections with a stale location. A garbled-but-"valid" sentence (one that
        // fails gps_coord_sane) is ignored rather than cleared, keeping the last fix.
        app->gps_valid = false;
    }
    if(sats >= 0) app->gps_sats = sats;
    furi_mutex_release(app->mutex);
}

static void gps_parse_line(GpsLink* gps, char* line) {
    NmeaFix fix;
    if(!nmea_parse_line(line, &fix)) return; // unknown / malformed / bad checksum
    gps_publish(gps, fix.lat, fix.lon, fix.sats, fix.valid);
    if(fix.has_course) {
        furi_mutex_acquire(gps->app->mutex, FuriWaitForever);
        gps->app->gps_course = fix.course;
        furi_mutex_release(gps->app->mutex);
    }
}

static void gps_rx_isr(FuriHalSerialHandle* handle, FuriHalSerialRxEvent event, void* context) {
    GpsLink* gps = context;
    if(event == FuriHalSerialRxEventData) {
        uint8_t data = furi_hal_serial_async_rx(handle);
        furi_stream_buffer_send(gps->rx_stream, &data, 1, 0);
        furi_thread_flags_set(furi_thread_get_id(gps->thread), GpsEvtRx);
    }
}

static int32_t gps_worker(void* context) {
    GpsLink* gps = context;
    uint8_t byte;
    while(true) {
        uint32_t evt = furi_thread_flags_wait(GPS_ALL_EVTS, FuriFlagWaitAny, FuriWaitForever);
        if(evt & FuriFlagError) continue;
        if(evt & GpsEvtStop) break;
        if(evt & GpsEvtRx) {
            while(furi_stream_buffer_receive(gps->rx_stream, &byte, 1, 0) == 1) {
                if(byte == '\n' || byte == '\r') {
                    if(gps->line_len > 0) {
                        gps->line[gps->line_len] = '\0';
                        gps_parse_line(gps, gps->line);
                        gps->line_len = 0;
                    }
                } else if(gps->line_len < GPS_LINE_MAX - 1) {
                    gps->line[gps->line_len++] = (char)byte;
                } else {
                    gps->line_len = 0; // overflow, resync
                }
            }
        }
    }
    return 0;
}

GpsLink* gps_link_alloc(void* app) {
    GpsLink* gps = malloc(sizeof(GpsLink));
    memset(gps, 0, sizeof(GpsLink));
    gps->app = app;
    return gps;
}

void gps_link_free(GpsLink* gps) {
    furi_assert(gps);
    if(gps->running) gps_link_stop(gps);
    free(gps);
}

void gps_link_start(GpsLink* gps) {
    if(gps->running) return;
    ReconApp* app = gps->app;

    gps->line_len = 0;
    gps->rx_stream = furi_stream_buffer_alloc(GPS_RX_BUF, 1);
    gps->thread = furi_thread_alloc_ex("ReconGpsWorker", 1536, gps_worker, gps);
    furi_thread_start(gps->thread);

    gps->serial = furi_hal_serial_control_acquire((FuriHalSerialId)app->settings.gps_uart);
    if(!gps->serial) {
        // Port busy (e.g. same as ESP); abort cleanly.
        furi_thread_flags_set(furi_thread_get_id(gps->thread), GpsEvtStop);
        furi_thread_join(gps->thread);
        furi_thread_free(gps->thread);
        furi_stream_buffer_free(gps->rx_stream);
        gps->thread = NULL;
        gps->rx_stream = NULL;
        return;
    }
    furi_hal_serial_init(gps->serial, app->settings.gps_baud);
    furi_hal_serial_async_rx_start(gps->serial, gps_rx_isr, gps, false);
    gps->running = true;
}

void gps_link_stop(GpsLink* gps) {
    if(!gps->running && !gps->thread) return;

    if(gps->serial) {
        furi_hal_serial_async_rx_stop(gps->serial);
        furi_hal_serial_deinit(gps->serial);
        furi_hal_serial_control_release(gps->serial);
        gps->serial = NULL;
    }
    if(gps->thread) {
        furi_thread_flags_set(furi_thread_get_id(gps->thread), GpsEvtStop);
        furi_thread_join(gps->thread);
        furi_thread_free(gps->thread);
        gps->thread = NULL;
    }
    if(gps->rx_stream) {
        furi_stream_buffer_free(gps->rx_stream);
        gps->rx_stream = NULL;
    }
    gps->running = false;
}
