
#include "bt_transmitter.h"

// -- PREPARE DATA TO BE SENT FUNCTIONS -----------------------------------------------
void prepare_bt_data_str(BtTransmitter* bt_transmitter, const char* data) {
    if(sizeof(data) > sizeof(bt_transmitter->data.str_data)) {
        FURI_LOG_W("lora_receiver_decode_msg_response", "Data too long");
    }
    strncpy(bt_transmitter->data.str_data, data, sizeof(bt_transmitter->data.str_data));
}

static uint16_t bt_serial_callback(SerialServiceEvent event, void* context) {
    furi_assert(context);
    BtTransmitter* bt_transmitter = context;

    if(event.event == SerialServiceEventTypeDataReceived) {
        FURI_LOG_D(
            TAG,
            "SerialServiceEventTypeDataReceived. Size: %u/%u. Data: %s",
            event.data.size,
            sizeof(DataStruct),
            (char*)event.data.buffer);

        if(event.data.size == sizeof(DataStruct)) {
            memcpy(&bt_transmitter->data, event.data.buffer, sizeof(DataStruct));
            bt_transmitter->bt_state = BtStateReceiving;
            bt_transmitter->last_packet = furi_hal_rtc_get_timestamp();

            // Elegant solution, the backlight is only on when there is continuous communication
            notification_message(bt_transmitter->notification, &sequence_display_backlight_on);

            notification_message(bt_transmitter->notification, &sequence_blink_blue_10);
        }
    }

    return 0;
}

void bt_transmitter_start(BtTransmitter* bt_transmitter) {
    if(bt_transmitter->bt_state == BtStateWaiting) {
        FURI_LOG_D(TAG, "Bluetooth is already started");
        return;
    }

    bt_disconnect(bt_transmitter->bt);
    // Wait 2nd core to update nvm storage
    furi_delay_ms(200);
    bt_keys_storage_set_storage_path(bt_transmitter->bt, APP_DATA_PATH(".bt_serial.keys"));

    BleProfileSerialParams params = {
        .device_name_prefix = "Lora",
        .mac_xor = 0x0002,
    };
    bt_transmitter->ble_serial_profile =
        bt_profile_start(bt_transmitter->bt, ble_profile_serial, &params);

    furi_check(bt_transmitter->ble_serial_profile);

    ble_profile_serial_set_event_callback(
        bt_transmitter->ble_serial_profile,
        BT_SERIAL_BUFFER_SIZE,
        bt_serial_callback,
        bt_transmitter);
    furi_hal_bt_start_advertising();

    bt_transmitter->bt_state = BtStateWaiting;
    FURI_LOG_D(TAG, "Bluetooth is active!");
}

/**
 * @brief Send string by Bluetooth
 */
static bool
    bt_transmitter_send_str(BtTransmitter* bt_transmitter, const char* msg, uint16_t size) {
    furi_assert(bt_transmitter);
    furi_assert(msg);
    furi_assert(size);

    FURI_LOG_D("bt_transmitter_send_str", "Send data: %s", msg);
    bool res = ble_profile_serial_tx(bt_transmitter->ble_serial_profile, (uint8_t*)msg, size);
    FURI_LOG_D("bt_transmitter_send_str", "%s", res ? "OK" : "Fail");

    return res;
}

static bool is_data_ready(BtTransmitter* bt_transmitter) {
    furi_assert(bt_transmitter);
    if(strlen(bt_transmitter->data.str_data) == 0) {
        FURI_LOG_D(TAG, "No str_data to send");
        return false;
    }

    return true;
}

static void reset_data(BtTransmitter* bt_transmitter) {
    memset(bt_transmitter->data.str_data, 0, sizeof(bt_transmitter->data.str_data));
}

bool bt_transmitter_send(BtTransmitter* bt_transmitter) {
    if(bt_transmitter->bt_state != BtStateWaiting) {
        FURI_LOG_D(TAG, "Bluetooth is not ready");
        return false;
    }

    if(!is_data_ready(bt_transmitter)) {
        return false;
    }

    bool res = false;
    bt_transmitter->bt_state = BtStateSending;
    res |= bt_transmitter_send_str(
        bt_transmitter, bt_transmitter->data.str_data, strlen(bt_transmitter->data.str_data));

    reset_data(bt_transmitter);
    bt_transmitter->bt_state = BtStateWaiting;

    return res;
}

void bt_transmitter_stop(BtTransmitter* bt_transmitter) {
    furi_hal_bt_stop_advertising();
    ble_profile_serial_set_event_callback(bt_transmitter->ble_serial_profile, 0, NULL, NULL);

    bt_disconnect(bt_transmitter->bt);

    // Wait 2nd core to update nvm storage
    furi_delay_ms(200);

    bt_keys_storage_set_default_path(bt_transmitter->bt);

    furi_check(bt_profile_restore_default(bt_transmitter->bt));
}

BtTransmitter* bt_transmitter_alloc(void) {
    BtTransmitter* bt_transmitter = malloc(sizeof(BtTransmitter));
    bt_transmitter->event_queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    bt_transmitter->notification = furi_record_open(RECORD_NOTIFICATION);
    bt_transmitter->bt = furi_record_open(RECORD_BT);

    return bt_transmitter;
}

void bt_transmitter_free(BtTransmitter* bt_transmitter) {
    if(bt_transmitter->bt_state == BtStateWaiting) {
        FURI_LOG_D("bt_transmitter_free", "Stopping Bluetooth transmitter");
        bt_transmitter_stop(bt_transmitter);
    }

    furi_message_queue_free(bt_transmitter->event_queue);
    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_BT);
    free(bt_transmitter);
}

void bt_transmitter_set_state_manager(
    BtTransmitter* bt_transmitter,
    LoraStateManager* state_manager) {
    furi_assert(bt_transmitter);
    furi_assert(state_manager);
    bt_transmitter->state_manager = state_manager;
}
