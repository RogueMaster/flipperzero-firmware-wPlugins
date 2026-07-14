#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_bt.h>
#include <bt/bt_service/bt.h>
#include "govee_h6006.h"

#define TAG "GoveeBLE"

typedef struct {
    uint8_t address[6];
    bool connected;
    FuriThread* keepalive_thread;
    FuriMutex* mutex;
    void (*status_callback)(bool connected, uint8_t* data);
} BleConnection;

static int32_t keepalive_thread(void* context) {
    BleConnection* conn = context;
    uint8_t packet[GOVEE_PACKET_SIZE];

    while(conn->connected) {
        govee_h6006_create_keepalive_packet(packet);
        // TODO: Send packet via BLE GATT write
        FURI_LOG_D(TAG, "Sending keepalive packet");
        furi_delay_ms(2000); // Send keepalive every 2 seconds
    }

    return 0;
}

BleConnection* ble_connection_alloc(uint8_t* address) {
    BleConnection* conn = malloc(sizeof(BleConnection));
    if(!conn) {
        FURI_LOG_E(TAG, "Failed to allocate BleConnection");
        return NULL;
    }

    memcpy(conn->address, address, 6);
    conn->connected = false;
    conn->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    if(!conn->mutex) {
        free(conn);
        return NULL;
    }

    conn->keepalive_thread = furi_thread_alloc();
    if(!conn->keepalive_thread) {
        furi_mutex_free(conn->mutex);
        free(conn);
        return NULL;
    }

    furi_thread_set_name(conn->keepalive_thread, "GoveeKeepalive");
    furi_thread_set_stack_size(conn->keepalive_thread, 1024);
    furi_thread_set_context(conn->keepalive_thread, conn);
    furi_thread_set_callback(conn->keepalive_thread, keepalive_thread);
    conn->status_callback = NULL;
    return conn;
}

void ble_connection_free(BleConnection* conn) {
    if(conn->connected) {
        conn->connected = false;
        furi_thread_join(conn->keepalive_thread);
    }
    furi_thread_free(conn->keepalive_thread);
    furi_mutex_free(conn->mutex);
    free(conn);
}

bool ble_connection_connect(BleConnection* conn) {
    FURI_LOG_I(
        TAG,
        "Connecting to device %02X:%02X:%02X:%02X:%02X:%02X",
        conn->address[0],
        conn->address[1],
        conn->address[2],
        conn->address[3],
        conn->address[4],
        conn->address[5]);

    // TODO: Implement actual BLE GATT connection
    // 1. Connect to device using address
    // 2. Discover services
    // 3. Find Govee control service (UUID: 000102030405060708090a0b0c0d1910)
    // 4. Get write characteristic (UUID: 000102030405060708090a0b0c0d2b10)
    // 5. Enable notifications if available

    conn->connected = true;
    furi_thread_start(conn->keepalive_thread);

    if(conn->status_callback) {
        conn->status_callback(true, NULL);
    }

    return true;
}

void ble_connection_disconnect(BleConnection* conn) {
    FURI_LOG_I(TAG, "Disconnecting from device");

    conn->connected = false;
    furi_thread_join(conn->keepalive_thread);

    // TODO: Implement actual BLE disconnection

    if(conn->status_callback) {
        conn->status_callback(false, NULL);
    }
}

bool ble_connection_send_packet(BleConnection* conn, uint8_t* packet) {
    if(!conn->connected) {
        FURI_LOG_W(TAG, "Not connected, cannot send packet");
        return false;
    }

    furi_mutex_acquire(conn->mutex, FuriWaitForever);

    FURI_LOG_D(
        TAG,
        "Sending packet: %02X %02X %02X ... %02X",
        packet[0],
        packet[1],
        packet[2],
        packet[19]);

    // TODO: Implement actual BLE GATT write
    // Write to characteristic UUID: 000102030405060708090a0b0c0d2b10

    furi_mutex_release(conn->mutex);

    return true;
}

bool ble_connection_set_power(BleConnection* conn, bool on) {
    uint8_t packet[GOVEE_PACKET_SIZE];
    govee_h6006_create_power_packet(packet, on);
    return ble_connection_send_packet(conn, packet);
}

bool ble_connection_set_brightness(BleConnection* conn, uint8_t brightness) {
    uint8_t packet[GOVEE_PACKET_SIZE];
    govee_h6006_create_brightness_packet(packet, brightness);
    return ble_connection_send_packet(conn, packet);
}

bool ble_connection_set_color(BleConnection* conn, uint8_t r, uint8_t g, uint8_t b) {
    uint8_t packet[GOVEE_PACKET_SIZE];
    govee_h6006_create_color_packet(packet, r, g, b);
    return ble_connection_send_packet(conn, packet);
}

bool ble_connection_set_white(BleConnection* conn, uint16_t temperature) {
    uint8_t packet[GOVEE_PACKET_SIZE];
    govee_h6006_create_white_packet(packet, temperature);
    return ble_connection_send_packet(conn, packet);
}
