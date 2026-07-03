/**
 * ble_scanner_worker.c — Marauder BLE output parser and device list manager.
 *
 * Parsing contract
 * ~~~~~~~~~~~~~~~~
 * Marauder scanbt (BT_SCAN_ALL) outputs one line per device:
 *   "-65 Device: MyPhone"          — RSSI then name
 *   "-70 Device: aa:bb:cc:dd:ee:ff" — RSSI then MAC (no name available)
 *
 * Format: "<RSSI> Device: <text>"
 *   - Named devices: text is the human-readable name; no MAC is present.
 *   - Unnamed devices: text is the lowercase MAC address.
 *
 * For named devices we derive a deterministic placeholder MAC using an
 * FNV-1a hash of the name, encoded as "DE:VI:CE:hh:hh:hh".  The
 * "DE:VI:CE" prefix is locally-administered and will never appear as a
 * real OUI, so it cannot collide with actual hardware addresses.
 *
 * AirTag heuristic
 * ~~~~~~~~~~~~~~~~
 * Apple AirTags and Find My accessories broadcast the "Find My" service
 * UUID (0xFD6F).  Marauder does not decode service UUIDs in its text
 * output, but AirTags typically appear with no name or "name: " and their
 * OUI prefix is Apple (AC:23:3F, F0:B4:79, etc.).  As a best-effort
 * signal we flag any device whose name contains "AirTag" (case-insensitive)
 * or whose MAC OUI matches a known Apple OUI prefix.
 */

#include "ble_scanner_worker.h"

#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_rtc.h>
#include <lib/datetime/datetime.h>
#include <storage/storage.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TAG "BleScanWorker"

/* --------------------------------------------------------------------------
 * Known Apple OUI prefixes (first 3 octets, upper-case) that are commonly
 * associated with AirTag / Find My accessories.  This list is not exhaustive
 * but catches the most prevalent factory OUIs.
 * -------------------------------------------------------------------------- */
static const char* const APPLE_OUIS[] = {
    "AC:23:3F",
    "F0:B4:79",
    "28:6F:7F",
    "7C:04:D0",
    "D0:03:4B",
    "40:98:AD",
    "98:9E:63",
    "8C:85:90",
    "DC:A9:04",
    NULL,
};

/* --------------------------------------------------------------------------
 * Internal struct
 * -------------------------------------------------------------------------- */
struct BleScanWorker {
    BleUart* uart;
    BleScanResults* results;
    bool log_sd;
    File* log_file;
    Storage* storage;
    volatile bool scanning;
};

/* --------------------------------------------------------------------------
 * Helpers
 * -------------------------------------------------------------------------- */

/* Returns true if `str` (upper-cased) starts with any APPLE_OUIS entry. */
static bool oui_is_apple(const char* mac) {
    for(int i = 0; APPLE_OUIS[i] != NULL; i++) {
        if(strncmp(mac, APPLE_OUIS[i], 8) == 0) {
            return true;
        }
    }
    return false;
}

/* Validate and extract a 17-char MAC from anywhere within `line`.
 * Writes result into `mac_out` (must be BLE_SCANNER_MAC_LEN bytes).
 * Returns a pointer to the start of the MAC within `line`, or NULL if
 * no valid MAC pattern was found. */
static const char* find_mac(const char* line, char mac_out[BLE_SCANNER_MAC_LEN]) {
    size_t len = strlen(line);
    if(len < 17) return NULL;

    for(size_t i = 0; i + 17 <= len; i++) {
        const char* p = line + i;
        /* Quick structure check: positions 2,5,8,11,14 must be ':' */
        if(p[2] == ':' && p[5] == ':' && p[8] == ':' && p[11] == ':' && p[14] == ':') {
            /* Verify each hex byte */
            bool valid = true;
            for(int b = 0; b < 6 && valid; b++) {
                char hi = p[b * 3];
                char lo = p[b * 3 + 1];
                valid = isxdigit((unsigned char)hi) && isxdigit((unsigned char)lo);
            }
            if(valid) {
                /* Normalise to upper-case */
                for(int c = 0; c < 17; c++) {
                    mac_out[c] = (char)toupper((unsigned char)p[c]);
                }
                mac_out[17] = '\0';
                return p;
            }
        }
    }
    return NULL;
}

/* Find or insert a device by MAC.  Returns its index within results->devices,
 * or BLE_SCANNER_MAX_DEVICES if the list is full. Caller must hold mutex. */
static uint32_t find_or_insert(BleScanResults* res, const char* mac) {
    for(uint32_t i = 0; i < res->count; i++) {
        if(strcmp(res->devices[i].mac, mac) == 0) {
            return i;
        }
    }
    if(res->count < BLE_SCANNER_MAX_DEVICES) {
        uint32_t idx = res->count++;
        memset(&res->devices[idx], 0, sizeof(BleScanDevice));
        strncpy(res->devices[idx].mac, mac, BLE_SCANNER_MAC_LEN - 1);
        return idx;
    }
    return BLE_SCANNER_MAX_DEVICES; /* full */
}

/* Write a single log line to the open SD file (if any). */
static void worker_log(BleScanWorker* w, const BleScanDevice* dev) {
    if(!w->log_file) return;
    char line[96];
    int n = snprintf(
        line,
        sizeof(line),
        "%d\t%s\t%s\n",
        (int)dev->rssi,
        dev->mac,
        dev->name[0] ? dev->name : "(unknown)");
    if(n > 0) {
        storage_file_write(w->log_file, line, (size_t)n);
    }
}

/* Open the SD log file at /ext/ble_scanner/scan_YYYYMMDD_HHMMSS.log */
static void worker_open_log(BleScanWorker* w) {
    if(!w->log_sd) return;

    w->storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(w->storage, EXT_PATH("ble_scanner"));

    DateTime dt;
    furi_hal_rtc_get_datetime(&dt);

    char path[64];
    snprintf(
        path,
        sizeof(path),
        EXT_PATH("ble_scanner/scan_%04u%02u%02u_%02u%02u%02u.log"),
        (unsigned)dt.year,
        (unsigned)dt.month,
        (unsigned)dt.day,
        (unsigned)dt.hour,
        (unsigned)dt.minute,
        (unsigned)dt.second);

    w->log_file = storage_file_alloc(w->storage);
    if(!storage_file_open(w->log_file, path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        FURI_LOG_W(TAG, "Failed to open log file: %s", path);
        storage_file_free(w->log_file);
        w->log_file = NULL;
    } else {
        FURI_LOG_I(TAG, "Logging to %s", path);
        /* Write CSV header */
        const char* hdr = "RSSI\tMAC\tName\n";
        storage_file_write(w->log_file, hdr, strlen(hdr));
    }
}

static void worker_close_log(BleScanWorker* w) {
    if(w->log_file) {
        storage_file_close(w->log_file);
        storage_file_free(w->log_file);
        w->log_file = NULL;
    }
    if(w->storage) {
        furi_record_close(RECORD_STORAGE);
        w->storage = NULL;
    }
}

/* Generate a deterministic placeholder MAC for devices known only by name.
 * Uses FNV-1a (32-bit) over the name bytes, then packs 3 low bytes into
 * "DE:VI:CE:hh:hh:hh".  The "DE:VI:CE" prefix is locally-administered and
 * will never appear as a real hardware OUI. */
static void name_hash_mac(const char* name, char mac_out[BLE_SCANNER_MAC_LEN]) {
    /* FNV-1a 32-bit */
    uint32_t hash = 0x811c9dc5u;
    for(const char* p = name; *p; p++) {
        hash ^= (uint8_t)*p;
        hash *= 0x01000193u;
    }
    snprintf(
        mac_out,
        BLE_SCANNER_MAC_LEN,
        "DE:VI:CE:%02X:%02X:%02X",
        (unsigned)((hash >> 16) & 0xFF),
        (unsigned)((hash >> 8) & 0xFF),
        (unsigned)(hash & 0xFF));
}

/* Check if `name` contains "airtag" (case-insensitive). */
static bool name_is_airtag(const char* name) {
    char lower[BLE_SCANNER_NAME_LEN] = {0};
    for(int i = 0; i < BLE_SCANNER_NAME_LEN - 1 && name[i]; i++) {
        lower[i] = (char)tolower((unsigned char)name[i]);
    }
    return strstr(lower, "airtag") != NULL;
}

/* --------------------------------------------------------------------------
 * RX callback — fires on the UART worker thread for each received line.
 *
 * Single-state parser for the scanbt (BT_SCAN_ALL) output format:
 *   "<RSSI> Device: <name-or-MAC>"
 *
 * Each line is self-contained.  No multi-line state required.
 * Must not block or call furi_delay_ms.
 * -------------------------------------------------------------------------- */
static void worker_rx_line(const char* line, void* ctx) {
    if(!ctx) return; /* Guard against teardown race (set_rx_callback clears ctx before cb) */
    BleScanWorker* w = (BleScanWorker*)ctx;
    if(!w->scanning) return;

    /* Every device line contains " Device: " — skip anything else. */
    const char* marker = strstr(line, " Device: ");
    if(!marker) return;

    /* RSSI is the signed integer at the start of the line. */
    int rssi_val = atoi(line);
    if(rssi_val > 0 || rssi_val < -120) return; /* sanity: must be negative dBm */
    int8_t rssi = (int8_t)rssi_val;

    /* Device text starts right after " Device: " (9 chars). */
    const char* device_text = marker + 9;
    if(*device_text == '\0') return;

    char mac[BLE_SCANNER_MAC_LEN];
    char name[BLE_SCANNER_NAME_LEN];
    name[0] = '\0';

    if(find_mac(device_text, mac)) {
        /* Device identified by MAC — no name available from this line. */
    } else {
        /* Device identified by human-readable name — derive a stable
         * placeholder MAC so the entry survives repeated scanbt updates. */
        strncpy(name, device_text, BLE_SCANNER_NAME_LEN - 1);
        name[BLE_SCANNER_NAME_LEN - 1] = '\0';
        name_hash_mac(name, mac);
    }

    furi_mutex_acquire(w->results->mutex, FuriWaitForever);
    uint32_t idx = find_or_insert(w->results, mac);
    if(idx < BLE_SCANNER_MAX_DEVICES) {
        BleScanDevice* dev = &w->results->devices[idx];
        dev->rssi = rssi;
        dev->last_seen_tick = furi_get_tick();

        /* Write name only when we have one (don't blank an existing name). */
        if(name[0] && !dev->name[0]) {
            strncpy(dev->name, name, BLE_SCANNER_NAME_LEN - 1);
            dev->name[BLE_SCANNER_NAME_LEN - 1] = '\0';
        }

        /* AirTag heuristic: Apple OUI or name substring. */
        if(!dev->is_airtag) {
            dev->is_airtag = oui_is_apple(mac) || name_is_airtag(dev->name);
        }

        worker_log(w, dev);
    }
    furi_mutex_release(w->results->mutex);
}

/* --------------------------------------------------------------------------
 * Public API
 * -------------------------------------------------------------------------- */

BleScanWorker* ble_scan_worker_alloc(BleUart* uart, BleScanResults* results, bool log_sd) {
    furi_assert(uart);
    furi_assert(results);

    BleScanWorker* w = malloc(sizeof(BleScanWorker));
    furi_assert(w);
    memset(w, 0, sizeof(BleScanWorker));

    w->uart = uart;
    w->results = results;
    w->log_sd = log_sd;

    ble_uart_set_rx_callback(uart, worker_rx_line, w);

    return w;
}

void ble_scan_worker_free(BleScanWorker* worker) {
    furi_assert(worker);

    if(worker->scanning) {
        ble_scan_worker_stop(worker);
    }

    worker_close_log(worker);

    /* Detach callback before freeing */
    ble_uart_set_rx_callback(worker->uart, NULL, NULL);

    free(worker);
}

void ble_scan_worker_start(BleScanWorker* worker) {
    furi_assert(worker);
    if(worker->scanning) return;

    worker->scanning = true;
    worker_open_log(worker);

    ble_uart_send(worker->uart, "scanbt");
    FURI_LOG_I(TAG, "BLE scan started");
}

void ble_scan_worker_stop(BleScanWorker* worker) {
    furi_assert(worker);
    if(!worker->scanning) return;

    worker->scanning = false;
    ble_uart_send(worker->uart, "stopscan");
    FURI_LOG_I(TAG, "BLE scan stopped");

    worker_close_log(worker);
}
