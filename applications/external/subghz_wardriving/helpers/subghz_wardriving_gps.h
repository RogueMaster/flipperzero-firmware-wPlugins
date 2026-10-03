#pragma once

#include <furi_hal.h>
#include <flipper_application/flipper_application.h>
#if __has_include(<gps/gps.h>)
#define SUBGHZ_GPS_HAS_RPC 1
#include <gps/gps.h>
#else
#define SUBGHZ_GPS_HAS_RPC 0
#endif
#include "ubox.h"

#define RX_BUF_SIZE 1024

// Order matches the config menu.
typedef enum {
    SubGhzGpsProtocolOff,
    SubGhzGpsProtocolRpc,
    SubGhzGpsProtocolNmea,
    SubGhzGpsProtocolUbox,
} SubGhzGpsProtocol;

// UART pins the GPS module is wired to. Order matches the config menu.
typedef enum {
    SubGhzGpsPinsUsart, // 13/14 (TX/RX)
    SubGhzGpsPinsLpuart, // 15/16 (TX/RX), e.g. boards with an ESP32 on 13/14
    SubGhzGpsPinsCount,
} SubGhzGpsPins;

typedef struct SubGhzGPS SubGhzGPS;

struct SubGhzGPS {
    FlipperApplication* plugin_app; // set for the UART plugin, NULL for inline RPC
    FuriThread* thread;
    FuriStreamBuffer* rx_stream;
    uint8_t rx_buf[RX_BUF_SIZE];
    FuriHalSerialHandle* serial_handle;
    SubGhzGpsProtocol protocol;
    uint32_t baudrate;
    SubGhzGpsPins pins;
    UboxRx ubox;
    FuriTimer* timer;

    // inline RPC transport (RECORD_GPS), unused by the UART plugin
#if SUBGHZ_GPS_HAS_RPC
    Gps* rpc;
    FuriTimer* rpc_timer;
    uint32_t rpc_last_rx;
#endif

    float latitude;
    float longitude;
    int satellites;
    uint8_t fix_second;
    uint8_t fix_minute;
    uint8_t fix_hour;

    void (*deinit)(SubGhzGPS* subghz_gps);
};

// Realtime info string (distance/direction/sats/time), shared by all sources.
void subghz_gps_cat_realtime(
    SubGhzGPS* subghz_gps,
    FuriString* descr,
    float latitude,
    float longitude);

/**
 * Load the UART GPS plugin (.fal) for NMEA or Ubox.
 *
 * @return SubGhzGPS* object, or NULL on load failure
*/
SubGhzGPS* subghz_gps_plugin_init(
    SubGhzGpsProtocol protocol,
    uint32_t baudrate,
    SubGhzGpsPins pins);

/**
 * Unload the UART GPS plugin.
*/
void subghz_gps_plugin_deinit(SubGhzGPS* subghz_gps);

/**
 * Start the inline RPC GPS source (RECORD_GPS, companion over USB/BLE).
 * No plugin is loaded. Returns a SubGhzGPS with the same data contract.
*/
SubGhzGPS* subghz_gps_rpc_start(void);

/**
 * Stop the inline RPC GPS source.
*/
void subghz_gps_rpc_stop(SubGhzGPS* subghz_gps);

/**
 * Stop and free whatever GPS source is active (UART plugin or inline RPC).
 * NULL-safe.
*/
void subghz_gps_stop(SubGhzGPS* subghz_gps);

/**
 * Reconcile the active GPS source with the current settings.
 *
 * Loads, reloads (on protocol, baudrate or pins change) or unloads the source
 * so it matches @p protocol / @p baudrate / @p pins. A no-op when the running
 * source already matches, so it is cheap to call before every Read. Call as:
 *     subghz->gps = subghz_gps_apply(subghz->gps, protocol, baudrate, pins);
 *
 * @param current  currently active source, or NULL if none
 * @return the source that now matches the settings, or NULL if protocol is off
 *         (or a UART plugin failed to load)
*/
SubGhzGPS* subghz_gps_apply(
    SubGhzGPS* current,
    SubGhzGpsProtocol protocol,
    uint32_t baudrate,
    SubGhzGpsPins pins);
