#pragma once

#include <extra_beacon.h>
#include <furi.h>

#define EVIL_BLE_MAX_DEVICES 32
#define EVIL_BLE_NAME_LEN    32
#define EVIL_BLE_MAC_LEN     18

/* Single BLE device record populated from Marauder scanbt output. */
typedef struct {
    char mac[EVIL_BLE_MAC_LEN]; /* "AA:BB:CC:DD:EE:FF" */
    uint8_t mac_bytes[EXTRA_BEACON_MAC_ADDR_SIZE]; /* parsed binary       */
    int8_t rssi;
    char name[EVIL_BLE_NAME_LEN]; /* Complete Local Name or "(unknown)"    */
    uint8_t adv_data[EXTRA_BEACON_MAX_DATA_SIZE]; /* raw adv payload bytes */
    uint8_t adv_data_len;
} EvilBleDevice;

/* Opaque scanner state. */
typedef struct EvilBleScanner EvilBleScanner;

typedef struct EvilBleUart EvilBleUart;

/* Callback fired (on the UART worker thread) whenever a new device is parsed.
 * `ctx` is whatever was passed to evil_ble_scanner_alloc(). */
typedef void (*EvilBleScannerCallback)(void* ctx);

/* Allocate scanner.  Attaches itself to `uart` as the RX callback. */
EvilBleScanner* evil_ble_scanner_alloc(EvilBleUart* uart, EvilBleScannerCallback cb, void* ctx);

/* Release scanner resources. */
void evil_ble_scanner_free(EvilBleScanner* scanner);

/* Send "scanbt" to the ESP32 and reset the device list. */
void evil_ble_scanner_start(EvilBleScanner* scanner);

/* Send "stopscan" to the ESP32. */
void evil_ble_scanner_stop(EvilBleScanner* scanner);

/* Thread-safe snapshot of the current device count. */
uint32_t evil_ble_scanner_get_count(EvilBleScanner* scanner);

/* Thread-safe copy of device at `index` into `out`.
 * Returns false if index is out of range. */
bool evil_ble_scanner_get_device(EvilBleScanner* scanner, uint32_t index, EvilBleDevice* out);
