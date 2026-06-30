/**
 * evil_ble_scanner.c — BLE scan orchestration and Marauder output parser.
 *
 * Marauder scanbt output format
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Marauder emits BLE scan results in this format:
 *
 *   <RSSI> Device: <name-or-MAC>
 *   e.g.  -65 Device: MyPhone
 *         -70 Device: aa:bb:cc:dd:ee:ff
 *
 * The " Device: " marker is the stable anchor.  Lines lacking it are skipped
 * (prompts, status messages, etc.).
 *
 * Parsing strategy
 * ~~~~~~~~~~~~~~~~
 * strtok is unavailable in the Flipper SDK libc.  All tokenising uses
 * strstr / pointer arithmetic — same approach as marauder.c in flipperpwn.
 *
 * MAC vs name disambiguation
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~
 * When the text after "Device: " is a MAC address (17 chars, colons at
 * positions 2/5/8/11/14), we parse it directly.  Otherwise it is treated
 * as a human-readable name and we derive a deterministic placeholder MAC:
 *
 *   DE:AD:xx:xx:xx:xx  where the last four bytes come from djb2(name)
 *
 * This ensures deduplication across multiple scan lines for the same device
 * and gives the extra_beacon clone engine a usable address.
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

    volatile bool scanning;
};

/* --------------------------------------------------------------------------
 * Parser helpers (no strtok available)
 * -------------------------------------------------------------------------- */

/* Parse a MAC string (upper or lower case) into a 6-byte array and
 * normalise the string representation to uppercase "AA:BB:CC:DD:EE:FF".
 * Returns true on success. */
static bool parse_mac(const char* mac_str, EvilBleDevice* dev) {
    unsigned int b[EXTRA_BEACON_MAC_ADDR_SIZE];
    int matched =
        sscanf(mac_str, "%02x:%02x:%02x:%02x:%02x:%02x", &b[0], &b[1], &b[2], &b[3], &b[4], &b[5]);
    if(matched != EXTRA_BEACON_MAC_ADDR_SIZE) return false;
    for(int i = 0; i < EXTRA_BEACON_MAC_ADDR_SIZE; i++) {
        dev->mac_bytes[i] = (uint8_t)b[i];
    }
    snprintf(
        dev->mac,
        EVIL_BLE_MAC_LEN,
        "%02X:%02X:%02X:%02X:%02X:%02X",
        b[0],
        b[1],
        b[2],
        b[3],
        b[4],
        b[5]);
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
 * Actual Marauder format:
 *   <RSSI> Device: <name-or-MAC>
 *   e.g. "-65 Device: MyPhone"
 *        "-70 Device: aa:bb:cc:dd:ee:ff"
 *
 * Anchor: the literal " Device: " substring.  Everything before it is the
 * RSSI; everything after is either a MAC or a human-readable name.
 *
 * Returns true and populates *dev on success.
 * -------------------------------------------------------------------------- */
static bool parse_scanbt_line(const char* line, EvilBleDevice* dev) {
    /* Find the stable " Device: " anchor. */
    const char* marker = strstr(line, " Device: ");
    if(!marker) return false;

    /* Parse RSSI from the start of the line. */
    int rssi_val = atoi(line);
    if(rssi_val > 0 || rssi_val < -120) return false;
    dev->rssi = (int8_t)rssi_val;

    /* Text after the 9-character " Device: " marker. */
    const char* text = marker + 9;

    /* Trim any trailing CR/LF/spaces from text by working on a local copy. */
    char text_buf[EVIL_BLE_NAME_LEN];
    strncpy(text_buf, text, sizeof(text_buf) - 1);
    text_buf[sizeof(text_buf) - 1] = '\0';
    size_t tlen = strlen(text_buf);
    while(tlen > 0 && (text_buf[tlen - 1] == '\r' || text_buf[tlen - 1] == '\n' ||
                       text_buf[tlen - 1] == ' ')) {
        text_buf[--tlen] = '\0';
    }
    if(tlen == 0) return false;

    /* Disambiguate: MAC address is exactly 17 chars with colons at 2,5,8,11,14. */
    bool is_mac =
        (tlen >= 17 && text_buf[2] == ':' && text_buf[5] == ':' && text_buf[8] == ':' &&
         text_buf[11] == ':' && text_buf[14] == ':');

    if(is_mac) {
        /* Parse and normalise to uppercase. */
        if(!parse_mac(text_buf, dev)) return false;
        dev->name[0] = '\0';
        /* Build a best-effort name from the MAC so the UI isn't blank. */
        snprintf(dev->name, EVIL_BLE_NAME_LEN, "(%s)", dev->mac);
    } else {
        /* Store the human-readable name. */
        strncpy(dev->name, text_buf, EVIL_BLE_NAME_LEN - 1);
        dev->name[EVIL_BLE_NAME_LEN - 1] = '\0';

        /* Derive a deterministic placeholder MAC via djb2 hash of the name.
         * DE:AD prefix makes placeholder MACs easy to spot in logs. */
        uint32_t hash = 5381;
        for(const char* c = text_buf; *c; c++)
            hash = hash * 33 + (uint8_t)*c;
        snprintf(
            dev->mac,
            EVIL_BLE_MAC_LEN,
            "DE:AD:%02X:%02X:%02X:%02X",
            (uint8_t)(hash >> 24),
            (uint8_t)(hash >> 16),
            (uint8_t)(hash >> 8),
            (uint8_t)(hash));
        /* Populate mac_bytes to match the generated string. */
        dev->mac_bytes[0] = 0xDE;
        dev->mac_bytes[1] = 0xAD;
        dev->mac_bytes[2] = (uint8_t)(hash >> 24);
        dev->mac_bytes[3] = (uint8_t)(hash >> 16);
        dev->mac_bytes[4] = (uint8_t)(hash >> 8);
        dev->mac_bytes[5] = (uint8_t)(hash);

        /* adv_data_len = 0 signals the clone engine to use the name fallback. */
        dev->adv_data_len = 0;
    }

    /* Build synthetic advertisement payload from the name when available. */
    if(dev->name[0] != '\0' && dev->name[0] != '(') {
        dev->adv_data_len = build_adv_from_name(dev->name, dev->adv_data);
    }

    return true;
}

/* --------------------------------------------------------------------------
 * UART RX callback — fires on the worker thread per completed line
 * -------------------------------------------------------------------------- */
static void evil_ble_scanner_rx_cb(const char* line, void* ctx) {
    if(!ctx) return; /* Guard against teardown race (set_rx_callback clears ctx before cb) */

    EvilBleScanner* scanner = (EvilBleScanner*)ctx;

    if(!scanner->scanning) return;

    /* parse_scanbt_line requires the " Device: " anchor — every non-data line
     * (prompts, status messages, echo) will simply fail that check and return
     * false.  No pre-filtering needed beyond the scanning gate above. */

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
