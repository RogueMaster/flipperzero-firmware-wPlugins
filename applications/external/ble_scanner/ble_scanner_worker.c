/**
 * ble_scanner_worker.c — Marauder BLE output parser and device list manager.
 *
 * Parsing contract
 * ~~~~~~~~~~~~~~~~
 * Marauder BLE output arrives as:
 *   "-70 AA:BB:CC:DD:EE:FF"   — RSSI then MAC on one line
 *   "Name: SomeDevice"         — optional, may follow immediately
 *   "  Name: SomeDevice"       — also valid (leading whitespace)
 *
 * We detect a device line by scanning for the 17-char MAC pattern
 * (XX:XX:XX:XX:XX:XX).  RSSI is the integer that precedes the MAC.
 * The next line is tentatively stored and applied as the name if it
 * starts with optional whitespace then "Name:".
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

    /* Pending-name buffer: stores the last device line's MAC so the
   * following "Name:" line can be attached to it. */
    char pending_mac[BLE_SCANNER_MAC_LEN];
    bool have_pending;

    bool scanning;
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

/* --------------------------------------------------------------------------
 * RX callback — fires on the UART worker thread for each received line.
 *
 * Two-state parser:
 *   State A (have_pending == false): look for a device line containing a MAC.
 *   State B (have_pending == true):  check if this line is a Name continuation.
 *
 * Must not block or call furi_delay_ms.
 * -------------------------------------------------------------------------- */
static void worker_rx_line(const char* line, void* ctx) {
    BleScanWorker* w = (BleScanWorker*)ctx;
    if(!w->scanning) return;

    char mac[BLE_SCANNER_MAC_LEN];
    const char* mac_pos = find_mac(line, mac);

    if(mac_pos != NULL) {
        /* ---- Device line ---- */

        /* Extract RSSI: scan backwards from mac_pos for an integer. */
        int8_t rssi = -100;
        if(mac_pos > line) {
            /* Walk back over whitespace then digits */
            const char* p = mac_pos - 1;
            while(p > line && (*p == ' ' || *p == '\t'))
                p--;
            /* p now points at the last digit of the RSSI value (if present) */
            if(isdigit((unsigned char)*p) || *p == '-') {
                /* find start of the number */
                const char* num_end = p + 1;
                while(p > line && (isdigit((unsigned char)*(p - 1)) || *(p - 1) == '-'))
                    p--;
                char rssi_buf[8];
                size_t rssi_len = (size_t)(num_end - p);
                if(rssi_len < sizeof(rssi_buf)) {
                    memcpy(rssi_buf, p, rssi_len);
                    rssi_buf[rssi_len] = '\0';
                    int parsed = atoi(rssi_buf);
                    if(parsed <= 0 && parsed >= -120) {
                        rssi = (int8_t)parsed;
                    }
                }
            }
        }

        /* Update or create device entry */
        furi_mutex_acquire(w->results->mutex, FuriWaitForever);
        uint32_t idx = find_or_insert(w->results, mac);
        if(idx < BLE_SCANNER_MAX_DEVICES) {
            BleScanDevice* dev = &w->results->devices[idx];
            dev->rssi = rssi;
            dev->last_seen_tick = furi_get_tick();

            /* AirTag heuristic: Apple OUI */
            if(oui_is_apple(mac)) {
                dev->is_airtag = true;
            }
        }
        furi_mutex_release(w->results->mutex);

        /* Enter state B: next line might be a Name */
        strncpy(w->pending_mac, mac, BLE_SCANNER_MAC_LEN - 1);
        w->pending_mac[BLE_SCANNER_MAC_LEN - 1] = '\0';
        w->have_pending = true;
        return;
    }

    if(w->have_pending) {
        /* ---- Possible Name/Serial continuation line ---- */
        const char* p = line;
        /* Skip leading whitespace */
        while(*p == ' ' || *p == '\t')
            p++;

        if(strncmp(p, "Name:", 5) == 0) {
            p += 5;
            while(*p == ' ')
                p++;

            furi_mutex_acquire(w->results->mutex, FuriWaitForever);
            uint32_t idx = find_or_insert(w->results, w->pending_mac);
            if(idx < BLE_SCANNER_MAX_DEVICES) {
                BleScanDevice* dev = &w->results->devices[idx];
                strncpy(dev->name, p, BLE_SCANNER_NAME_LEN - 1);
                dev->name[BLE_SCANNER_NAME_LEN - 1] = '\0';

                /* AirTag heuristic: name contains "AirTag" */
                /* Case-insensitive check: Marauder may capitalise differently */
                char lower_name[BLE_SCANNER_NAME_LEN] = {0};
                for(int i = 0; i < BLE_SCANNER_NAME_LEN - 1 && dev->name[i]; i++) {
                    lower_name[i] = (char)tolower((unsigned char)dev->name[i]);
                }
                if(strstr(lower_name, "airtag") != NULL) {
                    dev->is_airtag = true;
                }

                /* Log to SD if enabled — do this under the same lock to
         * avoid reading a partially-written struct. */
                worker_log(w, dev);
            }
            furi_mutex_release(w->results->mutex);

            /* Name line consumed — reset pending state. */
            w->have_pending = false;
        } else if(*p == '\0') {
            /* Blank line — keep pending for one more line (some firmware
       * inserts a blank between device and name). */
        } else {
            /* Unrecognised continuation — log the device as-is and reset. */
            furi_mutex_acquire(w->results->mutex, FuriWaitForever);
            uint32_t idx = find_or_insert(w->results, w->pending_mac);
            if(idx < BLE_SCANNER_MAX_DEVICES) {
                worker_log(w, &w->results->devices[idx]);
            }
            furi_mutex_release(w->results->mutex);
            w->have_pending = false;
        }
    }
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
    worker->have_pending = false;

    worker_open_log(worker);

    ble_uart_send(worker->uart, "scanbt");
    FURI_LOG_I(TAG, "BLE scan started");
}

void ble_scan_worker_stop(BleScanWorker* worker) {
    furi_assert(worker);
    if(!worker->scanning) return;

    worker->scanning = false;
    worker->have_pending = false;

    ble_uart_send(worker->uart, "stopscan");
    FURI_LOG_I(TAG, "BLE scan stopped");

    worker_close_log(worker);
}
