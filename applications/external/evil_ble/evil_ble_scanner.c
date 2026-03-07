/**
 * evil_ble_scanner.c — BLE scan orchestration and Marauder output parser.
 *
 * Marauder scanbt output format
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Marauder emits BLE scan results in one of two formats depending on firmware:
 *
 *   Format A (common):
 *     <idx> <MAC> <RSSI> <Name>
 *     e.g.  0 AA:BB:CC:DD:EE:FF -65 MyDevice
 *
 *   Format B (some builds — name may be absent or "(unknown)"):
 *     <idx> <MAC> <RSSI>
 *
 * We accept both.  Lines that don't begin with a decimal digit are skipped
 * (prompts, status messages, etc.).
 *
 * Parsing strategy
 * ~~~~~~~~~~~~~~~~
 * strtok is unavailable in the Flipper SDK libc.  All tokenising uses
 * strchr / pointer arithmetic — same approach as marauder.c in flipperpwn.
 *
 * Advertisement data reconstruction
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Marauder's scanbt output does not emit raw adv bytes.  We synthesise a
 * minimal valid BLE advertisement from the name so the extra_beacon can
 * broadcast something useful:
 *
 *   [len][0x09][name bytes...]   — AD type 0x09 = Complete Local Name
 *
 * This is sufficient to fool most proximity detectors and smart-lock
 * companion apps that rely on name matching.
 */

#include "evil_ble_scanner.h"
#include "evil_ble_uart.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TAG "EvilBLE"

/* --------------------------------------------------------------------------
 * Internal struct
 * -------------------------------------------------------------------------- */
struct EvilBleScanner {
    EvilBleUart* uart;

    EvilBleDevice devices[EVIL_BLE_MAX_DEVICES];
    uint32_t device_count;
    FuriMutex* mutex;

    /* Fired on the UART worker thread whenever a new device is appended. */
    EvilBleScannerCallback on_device_found;
    void* callback_ctx;

    bool scanning;
};

/* --------------------------------------------------------------------------
 * Parser helpers (no strtok available)
 * -------------------------------------------------------------------------- */

/* Copy at most n-1 bytes of the current space-delimited token into dst,
 * null-terminate, then return a pointer past any trailing spaces to the
 * start of the next token.  Returns NULL if no further tokens exist. */
static const char* copy_token(const char* src, char* dst, size_t n) {
    size_t i = 0;
    while(*src && *src != ' ' && i < n - 1) {
        dst[i++] = *src++;
    }
    dst[i] = '\0';
    while(*src == ' ')
        src++;
    return (*src) ? src : NULL;
}

/* Parse "AA:BB:CC:DD:EE:FF" into a 6-byte array.
 * Returns true on success. */
static bool parse_mac(const char* mac_str, uint8_t out[EXTRA_BEACON_MAC_ADDR_SIZE]) {
    unsigned int b[EXTRA_BEACON_MAC_ADDR_SIZE];
    int matched =
        sscanf(mac_str, "%02X:%02X:%02X:%02X:%02X:%02X", &b[0], &b[1], &b[2], &b[3], &b[4], &b[5]);
    if(matched != EXTRA_BEACON_MAC_ADDR_SIZE) return false;
    for(int i = 0; i < EXTRA_BEACON_MAC_ADDR_SIZE; i++) {
        out[i] = (uint8_t)b[i];
    }
    return true;
}

/* Build a minimal BLE advertisement payload into adv[] from a device name.
 * Returns the byte count written (0 if name is empty).
 *
 * Layout: [length][0x09][name bytes]
 *   - 0x09 = Complete Local Name AD type
 *   - length = name_len + 1 (includes the type byte)
 */
static uint8_t build_adv_from_name(const char* name, uint8_t adv[EXTRA_BEACON_MAX_DATA_SIZE]) {
    uint8_t name_len = (uint8_t)strlen(name);
    /* Maximum name that fits: 31 - 2 header bytes = 29 bytes. */
    if(name_len > EXTRA_BEACON_MAX_DATA_SIZE - 2) {
        name_len = EXTRA_BEACON_MAX_DATA_SIZE - 2;
    }
    if(name_len == 0) return 0;

    adv[0] = name_len + 1; /* length field: type byte + data bytes */
    adv[1] = 0x09; /* AD type: Complete Local Name           */
    memcpy(&adv[2], name, name_len);
    return (uint8_t)(2 + name_len);
}

/* --------------------------------------------------------------------------
 * Marauder scanbt line parser
 *
 * Expected format (space-separated):
 *   <idx> <MAC> <RSSI> [<Name> ...]
 *
 * Returns true and populates *dev on success.
 * -------------------------------------------------------------------------- */
static bool parse_scanbt_line(const char* line, EvilBleDevice* dev) {
    const char* p = line;

    /* Field 0: index — must start with a decimal digit. */
    if(*p < '0' || *p > '9') return false;

    char idx_buf[8];
    p = copy_token(p, idx_buf, sizeof(idx_buf));
    if(!p) return false;

    /* Field 1: MAC address */
    char mac_buf[EVIL_BLE_MAC_LEN];
    p = copy_token(p, mac_buf, sizeof(mac_buf));
    if(!p) return false;

    /* Validate MAC format before accepting this line. */
    if(!parse_mac(mac_buf, dev->mac_bytes)) return false;
    strncpy(dev->mac, mac_buf, EVIL_BLE_MAC_LEN - 1);
    dev->mac[EVIL_BLE_MAC_LEN - 1] = '\0';

    /* Field 2: RSSI (signed integer). */
    char rssi_buf[8];
    p = copy_token(p, rssi_buf, sizeof(rssi_buf));
    dev->rssi = (int8_t)atoi(rssi_buf);
    /* p may be NULL here — name is optional. */

    /* Field 3+: remainder of the line is the device name (may contain spaces). */
    if(p && *p) {
        strncpy(dev->name, p, EVIL_BLE_NAME_LEN - 1);
        dev->name[EVIL_BLE_NAME_LEN - 1] = '\0';
    } else {
        strncpy(dev->name, "(unknown)", EVIL_BLE_NAME_LEN - 1);
        dev->name[EVIL_BLE_NAME_LEN - 1] = '\0';
    }

    /* Build synthetic advertisement payload from the parsed name. */
    dev->adv_data_len = build_adv_from_name(dev->name, dev->adv_data);

    return true;
}

/* --------------------------------------------------------------------------
 * UART RX callback — fires on the worker thread per completed line
 * -------------------------------------------------------------------------- */
static void evil_ble_scanner_rx_cb(const char* line, void* ctx) {
    EvilBleScanner* scanner = (EvilBleScanner*)ctx;

    if(!scanner->scanning) return;

    /* Marauder prompt lines start with ">"; skip status messages. */
    if(line[0] == '>') return;
    if(strstr(line, "Scan") || strstr(line, "scan")) {
        /* "Scanning for BLE devices..." etc. — informational, not data. */
        if(line[0] != '0' && (line[0] < '0' || line[0] > '9')) return;
    }

    EvilBleDevice dev;
    memset(&dev, 0, sizeof(dev));

    if(!parse_scanbt_line(line, &dev)) return;

    furi_mutex_acquire(scanner->mutex, FuriWaitForever);

    /* Deduplicate by MAC: update RSSI if already seen, else append. */
    bool found = false;
    for(uint32_t i = 0; i < scanner->device_count; i++) {
        if(strncmp(scanner->devices[i].mac, dev.mac, EVIL_BLE_MAC_LEN) == 0) {
            scanner->devices[i].rssi = dev.rssi;
            /* Refresh name only if we got a real one this time. */
            if(strncmp(dev.name, "(unknown)", 9) != 0) {
                strncpy(scanner->devices[i].name, dev.name, EVIL_BLE_NAME_LEN - 1);
                scanner->devices[i].name[EVIL_BLE_NAME_LEN - 1] = '\0';
                scanner->devices[i].adv_data_len =
                    build_adv_from_name(scanner->devices[i].name, scanner->devices[i].adv_data);
            }
            found = true;
            break;
        }
    }

    if(!found && scanner->device_count < EVIL_BLE_MAX_DEVICES) {
        scanner->devices[scanner->device_count++] = dev;
        FURI_LOG_D(
            TAG,
            "BLE[%lu]: %s \"%s\" %d dBm",
            (unsigned long)(scanner->device_count - 1),
            dev.mac,
            dev.name,
            (int)dev.rssi);
    }

    furi_mutex_release(scanner->mutex);

    /* Notify the app (fires view_dispatcher_send_custom_event on GUI thread). */
    if(scanner->on_device_found) {
        scanner->on_device_found(scanner->callback_ctx);
    }
}

/* --------------------------------------------------------------------------
 * Public API
 * -------------------------------------------------------------------------- */

EvilBleScanner* evil_ble_scanner_alloc(EvilBleUart* uart, EvilBleScannerCallback cb, void* ctx) {
    furi_assert(uart);

    EvilBleScanner* scanner = malloc(sizeof(EvilBleScanner));
    furi_assert(scanner);
    memset(scanner, 0, sizeof(EvilBleScanner));

    scanner->uart = uart;
    scanner->on_device_found = cb;
    scanner->callback_ctx = ctx;

    scanner->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    furi_assert(scanner->mutex);

    evil_ble_uart_set_rx_callback(uart, evil_ble_scanner_rx_cb, scanner);

    FURI_LOG_I(TAG, "Scanner initialised");
    return scanner;
}

void evil_ble_scanner_free(EvilBleScanner* scanner) {
    furi_assert(scanner);
    /* Deregister so no stale callbacks fire after free. */
    evil_ble_uart_set_rx_callback(scanner->uart, NULL, NULL);
    furi_mutex_free(scanner->mutex);
    free(scanner);
}

void evil_ble_scanner_start(EvilBleScanner* scanner) {
    furi_assert(scanner);

    furi_mutex_acquire(scanner->mutex, FuriWaitForever);
    memset(scanner->devices, 0, sizeof(scanner->devices));
    scanner->device_count = 0;
    scanner->scanning = true;
    furi_mutex_release(scanner->mutex);

    evil_ble_uart_send(scanner->uart, "scanbt");
    FURI_LOG_I(TAG, "scanbt started");
}

void evil_ble_scanner_stop(EvilBleScanner* scanner) {
    furi_assert(scanner);

    evil_ble_uart_send(scanner->uart, "stopscan");

    furi_mutex_acquire(scanner->mutex, FuriWaitForever);
    scanner->scanning = false;
    furi_mutex_release(scanner->mutex);

    FURI_LOG_I(TAG, "scan stopped");
}

uint32_t evil_ble_scanner_get_count(EvilBleScanner* scanner) {
    furi_assert(scanner);
    furi_mutex_acquire(scanner->mutex, FuriWaitForever);
    uint32_t count = scanner->device_count;
    furi_mutex_release(scanner->mutex);
    return count;
}

bool evil_ble_scanner_get_device(EvilBleScanner* scanner, uint32_t index, EvilBleDevice* out) {
    furi_assert(scanner);
    furi_assert(out);

    furi_mutex_acquire(scanner->mutex, FuriWaitForever);
    bool ok = (index < scanner->device_count);
    if(ok) {
        *out = scanner->devices[index];
    }
    furi_mutex_release(scanner->mutex);
    return ok;
}
