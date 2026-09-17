#include "esp_link.h"
#include "tuning.h"
#include <furi.h>
#include <furi_hal_serial.h>
#include <furi_hal_serial_control.h>
#include <expansion/expansion.h>
#include <stdlib.h>

/* ISR-context RX callback: drain the UART into a stream buffer. */
static void esp_rx_cb(FuriHalSerialHandle *handle, FuriHalSerialRxEvent event, void *context) {
    FuriStreamBuffer *rx = context;
    if(event & FuriHalSerialRxEventData) {
        while(furi_hal_serial_async_rx_available(handle)) {
            uint8_t b = furi_hal_serial_async_rx(handle);
            furi_stream_buffer_send(rx, &b, 1, 0);
        }
    }
}

/* Parse "DRAGO wifi=<n> rssi=<-d>" out of a completed line. */
static bool parse_report(const char *line, uint8_t *wifi_out, int8_t *rssi_out) {
    const char *p = strstr(line, "DRAGO");
    if(!p) return false;
    const char *w = strstr(p, "wifi=");
    if(!w) return false;
    int wifi = atoi(w + 5);
    if(wifi < 0) wifi = 0;
    if(wifi > 255) wifi = 255;
    int rssi = 0;
    const char *r = strstr(p, "rssi=");
    if(r) rssi = atoi(r + 5);
    if(rssi < -127) rssi = -127;
    if(rssi > 0) rssi = 0;
    *wifi_out = (uint8_t)wifi;
    *rssi_out = (int8_t)rssi;
    return true;
}

bool esp_probe(uint8_t *wifi_count_out, int8_t *rssi_out) {
    bool ok = false;

    /* Release the expansion service's grip on the UART first (required). */
    Expansion *expansion = furi_record_open(RECORD_EXPANSION);
    expansion_disable(expansion);

    FuriHalSerialHandle *handle = furi_hal_serial_control_acquire(FuriHalSerialIdUsart);
    if(handle) {
        FuriStreamBuffer *rx = furi_stream_buffer_alloc(256, 1);
        furi_hal_serial_init(handle, ESP_LINK_BAUD);
        furi_hal_serial_async_rx_start(handle, esp_rx_cb, rx, false);

        /* Nudge the board in case it reports on request. */
        const uint8_t query[] = "?\n";
        furi_hal_serial_tx(handle, query, sizeof(query) - 1);

        char line[96];
        size_t len = 0;
        uint32_t deadline = furi_get_tick() + ESP_PROBE_MS;
        while(!ok && furi_get_tick() < deadline) {
            uint8_t b;
            size_t got = furi_stream_buffer_receive(rx, &b, 1, 20);
            if(got == 0) continue;
            if(b == '\n' || b == '\r') {
                if(len > 0) {
                    line[len] = '\0';
                    if(parse_report(line, wifi_count_out, rssi_out)) ok = true;
                    len = 0;
                }
            } else if(len < sizeof(line) - 1) {
                line[len++] = (char)b;
            } else {
                len = 0; /* overflow: drop the line */
            }
        }

        furi_hal_serial_async_rx_stop(handle);
        furi_hal_serial_deinit(handle);
        furi_hal_serial_control_release(handle);
        furi_stream_buffer_free(rx);
    }

    expansion_enable(expansion);
    furi_record_close(RECORD_EXPANSION);
    return ok;
}
