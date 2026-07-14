#pragma once

#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_bt.h>
#include "helpers/ble_serial.h"
#include <bt/bt_service/bt.h>
#include <gui/gui.h>
#include <gui/elements.h>
#include <notification/notification_messages.h>
#include <input/input.h>
#include <storage/storage.h>
#include "lora_state_manager.h"

#define TAG                   "BTTransmitter"
#define BT_SERIAL_BUFFER_SIZE 128
#define MAX_DATA_SIZE         (1 << 8) // 256 bytes

#define SCREEN_HEIGHT 64
#define LINE_HEIGHT   11

#define BAR_X     30
#define BAR_WIDTH 97

typedef enum {
    BtStateChecking,
    BtStateInactive,
    BtStateWaiting,
    BtStateReceiving,
    BtStateSending,
    BtStateNoData,
    BtStateLost
} BtState;

#pragma pack(push, 1)
typedef struct {
    char str_data[MAX_DATA_SIZE];
} DataStruct;
#pragma pack(pop)

typedef struct {
    Bt* bt;
    FuriMutex* app_mutex;
    FuriMessageQueue* event_queue;
    NotificationApp* notification;
    FuriHalBleProfileBase* ble_serial_profile;

    BtState bt_state;
    DataStruct data;
    uint8_t lines_count;
    uint32_t last_packet;
    LoraStateManager* state_manager;

} BtTransmitter;

/**
 * @brief Allocate and initialize a BtTransmitter instance.
 */
BtTransmitter* bt_transmitter_alloc(void);

/**
 * @brief Free the BtTransmitter instance.
 */
void bt_transmitter_free(BtTransmitter* bt_transmitter);

/**
 * @brief Set the state manager for the BtTransmitter instance.
 */
void bt_transmitter_set_state_manager(
    BtTransmitter* bt_transmitter,
    LoraStateManager* state_manager);

/**
 * @brief Start the Bluetooth transmitter.
 */
void bt_transmitter_start(BtTransmitter* bt_transmitter);

/**
 * @brief Send by Bluetooth all data in its field data if ready
 */
bool bt_transmitter_send(BtTransmitter* bt_transmitter);

// -- PREPARE DATA TO BE SENT FUNCTIONS -----------------------------------------------

/**
 * @brief Set the str_data field of the DataStruct in the BtTransmitter instance
 * @param bt_transmitter Pointer to the BtTransmitter instance.
 * @param data String to be sent.
 */
void prepare_bt_data_str(BtTransmitter* bt_transmitter, const char* data);
