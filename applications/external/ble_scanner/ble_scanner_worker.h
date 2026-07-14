#pragma once

#include "ble_uart.h"
#include <furi.h>

/* --------------------------------------------------------------------------
 * Device record
 * -------------------------------------------------------------------------- */
#define BLE_SCANNER_MAX_DEVICES 64
#define BLE_SCANNER_NAME_LEN    32
#define BLE_SCANNER_MAC_LEN     18 /* "AA:BB:CC:DD:EE:FF\0" */

typedef struct {
    char mac[BLE_SCANNER_MAC_LEN];
    int8_t rssi;
    char name[BLE_SCANNER_NAME_LEN];
    uint32_t last_seen_tick;
    bool is_airtag; /* Apple AirTag or Find My accessory heuristic */
} BleScanDevice;

/* --------------------------------------------------------------------------
 * Shared result set — accessed from both the worker thread and the GUI timer.
 * Always acquire the mutex before reading or writing devices[].
 * -------------------------------------------------------------------------- */
typedef struct {
    BleScanDevice devices[BLE_SCANNER_MAX_DEVICES];
    uint32_t count;
    FuriMutex* mutex;
} BleScanResults;

/* --------------------------------------------------------------------------
 * Worker
 * -------------------------------------------------------------------------- */
typedef struct BleScanWorker BleScanWorker;

/* Allocate and start the worker.
 * `uart`    — must be live for the worker's lifetime.
 * `results` — shared result set; worker updates it under results->mutex.
 * `log_sd`  — whether to write a log file to SD card. */
BleScanWorker* ble_scan_worker_alloc(BleUart* uart, BleScanResults* results, bool log_sd);

/* Stop the worker cleanly and free all resources. */
void ble_scan_worker_free(BleScanWorker* worker);

/* Send scanbt\n to start or stopscan\n to stop.  Safe to call from any thread.
 */
void ble_scan_worker_start(BleScanWorker* worker);
void ble_scan_worker_stop(BleScanWorker* worker);
