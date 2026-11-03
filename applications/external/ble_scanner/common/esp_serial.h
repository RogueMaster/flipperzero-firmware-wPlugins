/**
 * esp_serial.h -- Shared UART driver for ESP32/Marauder dev boards.
 *
 * Consolidates the near-identical ISR -> StreamBuffer -> worker-thread
 * pipeline previously duplicated across ble_scanner, evil_ble, flipperpwn,
 * rayhunter_client, and rogue_ap_detector.
 *
 * Each app customises behaviour through EspSerialConfig (log tag, thread
 * name, optional probe-on-open, optional connected predicate).
 */
#pragma once

#include <furi.h>
#include <furi_hal.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct EspSerial EspSerial;

typedef void (*EspSerialRxCallback)(const char* line, void* ctx);

/**
 * Optional predicate called on each received line to decide when the
 * board is "connected".  Return true if `line` proves the board is alive.
 * When NULL, any non-empty received line marks the board as connected.
 */
typedef bool (*EspSerialConnectedCheck)(const char* line);

typedef struct {
    const char* tag;
    const char* thread_name;
    bool probe_on_open;
    EspSerialConnectedCheck connected_check;
} EspSerialConfig;

EspSerial* esp_serial_alloc(const EspSerialConfig* config);
void esp_serial_free(EspSerial* serial);
void esp_serial_send(EspSerial* serial, const char* cmd);
void esp_serial_set_rx_callback(EspSerial* serial, EspSerialRxCallback cb, void* ctx);
bool esp_serial_is_connected(EspSerial* serial);

#ifdef __cplusplus
}
#endif
