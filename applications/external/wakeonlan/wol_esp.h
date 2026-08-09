#pragma once

#include <furi.h>
#include <stdbool.h>

#include "wol_config.h"

/**
 * Client for the wol-flipper companion firmware running on the ESP32-S2 dev
 * board (see esp32/src/main.cpp for the wire protocol).
 *
 * All calls are blocking and must be made from a worker thread, never from
 * the GUI thread.
 */
typedef struct WolEsp WolEsp;

typedef enum {
    WolEspOk,
    /** Nothing answered at all: no board, wrong wiring, board not powered. */
    WolEspErrNoReply,
    /** Something answered but it is not this firmware. */
    WolEspErrWrongFirmware,
    /** The Flipper's 5V boost tripped and took the board with it. */
    WolEspErrPower,
    /** The board announced a fresh boot in the middle of a command. */
    WolEspErrReboot,
    WolEspErrWifi,
    /** The AP was not on the air at all. */
    WolEspErrWifiNotFound,
    /** Association was refused: wrong key, in practice. */
    WolEspErrWifiAuth,
    WolEspErrUdp,
    WolEspErrArgs,
    WolEspErrScan,
} WolEspResult;

#define WOL_SSID_MAX_SCAN 24

typedef struct {
    char ssid[WOL_SSID_LEN];
    int8_t rssi;
} WolEspAp;

/** Intermediate notices pushed by the board while a command runs. */
typedef enum {
    WolEspProgressWifi,
    WolEspProgressSending,
} WolEspProgress;

typedef void (*WolEspProgressCallback)(void* context, WolEspProgress progress);

/**
 * @param cancel  pointer to a flag polled while waiting for replies; set it
 *                from another thread to abort early
 */
WolEsp* wol_esp_alloc(volatile bool* cancel);
void wol_esp_free(WolEsp* esp);

void wol_esp_set_progress_callback(WolEsp* esp, WolEspProgressCallback callback, void* context);

/** Enable 5V on pin 1 (if needed) and take over USART. */
bool wol_esp_open(WolEsp* esp);
void wol_esp_close(WolEsp* esp);

/** Identify the board. Writes the firmware protocol version if non NULL. */
WolEspResult wol_esp_ping(WolEsp* esp, uint8_t* version);

/**
 * Everything the board said during the last command, newline separated.
 * Valid until the next command. Meant for putting on screen when something
 * fails, since guessing at a black box over a serial line does not work.
 */
const char* wol_esp_last_reply(WolEsp* esp);

/**
 * SSID the board is currently associated with, empty string when it is not.
 * Cheap, so it is worth asking before paying for a scan.
 */
WolEspResult wol_esp_status(WolEsp* esp, char* ssid, size_t ssid_len);

/** List what the board's radio can see. Takes a few seconds. */
WolEspResult wol_esp_scan(WolEsp* esp, WolEspAp* out, uint8_t capacity, uint8_t* count);

/** Associate with an AP, or confirm an existing association. */
WolEspResult wol_esp_join(WolEsp* esp, const char* ssid, const char* pass);

/** Join if needed, then broadcast the magic packet for mac. */
WolEspResult wol_esp_wake(
    WolEsp* esp,
    const char* ssid,
    const char* pass,
    const uint8_t mac[6],
    const char* ip,
    uint16_t port);
