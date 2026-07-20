// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt and FlipDeFlock contributors
#include "gps_link.h"
#include "../recon_app_i.h"

#include <math.h>
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

/** Convert NMEA ddmm.mmmm + hemisphere to signed decimal degrees. */
static float nmea_to_decimal(const char* field, const char* dir) {
    if(!field || field[0] == '\0') return NAN;
    float raw = strtof(field, NULL);
    int deg = (int)(raw / 100.0f);
    float minutes = raw - (deg * 100.0f);
    float dec = deg + minutes / 60.0f;
    if(dir && (dir[0] == 'S' || dir[0] == 'W')) dec = -dec;
    return dec;
}

/** Split `line` in place into up to `max` comma-separated fields. */
static int nmea_tokenize(char* line, char** fields, int max) {
    int n = 0;
    char* p = line;
    fields[n++] = p;
    while(*p && n < max) {
        if(*p == ',') {
            *p = '\0';
            fields[n++] = p + 1;
        }
        p++;
    }
    return n;
}

/** Reject NaN, out-of-range, and the "null island" (0,0) noise artifact. */
static bool gps_coord_sane(float lat, float lon) {
    if(isnan(lat) || isnan(lon)) return false;
    if(lat < -90.0f || lat > 90.0f || lon < -180.0f || lon > 180.0f) return false;
    // 0,0 is almost always a partial/garbled sentence, not a real fix.
    if(lat > -0.0001f && lat < 0.0001f && lon > -0.0001f && lon < 0.0001f) return false;
    return true;
}

static void gps_publish(GpsLink* gps, float lat, float lon, int sats, bool valid) {
    ReconApp* app = gps->app;
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    if(valid && gps_coord_sane(lat, lon)) {
        app->gps_lat = lat;
        app->gps_lon = lon;
        app->gps_valid = true;
    }
    if(sats >= 0) app->gps_sats = sats;
    // When !valid we keep the last good fix and only refresh sat count above.
    furi_mutex_release(app->mutex);
}

static void gps_parse_line(GpsLink* gps, char* line) {
    if(line[0] != '$') return;
    size_t len = strlen(line);
    if(len < 7) return;

    // Drop checksum suffix "*hh" if present.
    char* star = strchr(line, '*');
    if(star) *star = '\0';

    const char* type = line + 3; // skip "$GP"/"$GN"/...
    char* fields[20];
    int nf = nmea_tokenize(line, fields, 20);

    if(strncmp(type, "RMC", 3) == 0 && nf >= 7) {
        bool valid = (fields[2][0] == 'A');
        float lat = nmea_to_decimal(fields[3], fields[4]);
        float lon = nmea_to_decimal(fields[5], fields[6]);
        gps_publish(gps, lat, lon, -1, valid);
        // Course over ground (deg) = RMC field 8.
        if(valid && nf >= 9 && fields[8][0]) {
            float course = strtof(fields[8], NULL);
            furi_mutex_acquire(gps->app->mutex, FuriWaitForever);
            gps->app->gps_course = course;
            furi_mutex_release(gps->app->mutex);
        }
    } else if(strncmp(type, "GGA", 3) == 0 && nf >= 8) {
        int fixq = atoi(fields[6]);
        int sats = atoi(fields[7]);
        float lat = nmea_to_decimal(fields[2], fields[3]);
        float lon = nmea_to_decimal(fields[4], fields[5]);
        gps_publish(gps, lat, lon, sats, fixq > 0);
    } else if(strncmp(type, "GLL", 3) == 0 && nf >= 7) {
        bool valid = (fields[6][0] == 'A');
        float lat = nmea_to_decimal(fields[1], fields[2]);
        float lon = nmea_to_decimal(fields[3], fields[4]);
        gps_publish(gps, lat, lon, -1, valid);
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
