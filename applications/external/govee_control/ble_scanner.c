#include <furi.h>
#include <furi_hal_bt.h>
#include <bt/bt_service/bt.h>
#include <furi_hal.h>
#include "govee_h6006.h"

#define TAG "GoveeBLE"

typedef struct {
    FuriThread* thread;
    FuriMessageQueue* queue;
    bool scanning;
    void (*device_found_callback)(uint8_t* address, const char* name, int8_t rssi);
} BleScanner;

typedef struct {
    uint8_t address[6];
    char name[32];
    int8_t rssi;
} BleDevice;

// TODO: Implement device filtering
// static bool is_govee_device(const char* name) {
//     if(!name) return false;
//     // H6006 devices typically show as "ihoment_H6006_XXXX" or "Govee_H6006_XXXX"
//     return (strstr(name, "H6006") != NULL) ||
//            (strstr(name, "ihoment") != NULL) ||
//            (strstr(name, "Govee") != NULL);
// }

static int32_t ble_scanner_thread(void* context) {
    BleScanner* scanner = context;
    FURI_LOG_I(TAG, "BLE Scanner started");

    // Initialize BLE stack
    furi_hal_bt_start_advertising();
    furi_delay_ms(100);
    furi_hal_bt_stop_advertising();

    while(scanner->scanning) {
        // TODO: Implement actual BLE GAP scanning
        // This is a simplified version - real implementation needs GAP API

        // Simulate finding a device for testing
        static bool found = false;
        if(!found) {
            BleDevice device;
            memset(&device, 0, sizeof(device));
            strncpy(device.name, "ihoment_H6006_ABCD", sizeof(device.name));
            device.address[0] = 0xAA;
            device.address[1] = 0xBB;
            device.address[2] = 0xCC;
            device.address[3] = 0xDD;
            device.address[4] = 0xEE;
            device.address[5] = 0xFF;
            device.rssi = -65;

            if(scanner->device_found_callback) {
                scanner->device_found_callback(device.address, device.name, device.rssi);
            }
            found = true;
        }

        furi_delay_ms(1000);
    }

    FURI_LOG_I(TAG, "BLE Scanner stopped");
    return 0;
}

BleScanner* ble_scanner_alloc() {
    BleScanner* scanner = malloc(sizeof(BleScanner));
    if(!scanner) {
        return NULL;
    }

    scanner->queue = furi_message_queue_alloc(10, sizeof(BleDevice));
    if(!scanner->queue) {
        free(scanner);
        return NULL;
    }

    scanner->thread = furi_thread_alloc();
    if(!scanner->thread) {
        furi_message_queue_free(scanner->queue);
        free(scanner);
        return NULL;
    }

    furi_thread_set_name(scanner->thread, "BleScanner");
    furi_thread_set_stack_size(scanner->thread, 2048);
    furi_thread_set_context(scanner->thread, scanner);
    furi_thread_set_callback(scanner->thread, ble_scanner_thread);
    scanner->scanning = false;
    scanner->device_found_callback = NULL;
    return scanner;
}

void ble_scanner_free(BleScanner* scanner) {
    if(scanner->scanning) {
        scanner->scanning = false;
        furi_thread_join(scanner->thread);
    }
    furi_thread_free(scanner->thread);
    furi_message_queue_free(scanner->queue);
    free(scanner);
}

void ble_scanner_start(BleScanner* scanner, void (*callback)(uint8_t*, const char*, int8_t)) {
    scanner->device_found_callback = callback;
    scanner->scanning = true;
    furi_thread_start(scanner->thread);
}

void ble_scanner_stop(BleScanner* scanner) {
    scanner->scanning = false;
    furi_thread_join(scanner->thread);
}
