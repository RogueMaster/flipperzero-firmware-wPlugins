#include "ami_tool_i.h"
#include "../helpers/ble_serial.h"

#include <stdio.h>
#include <string.h>

#include <furi_hal_bt.h>

#define AMI_TOOL_BT_SERIAL_BUFFER_SIZE (128U)
#define AMI_TOOL_BT_DISPLAY_SIZE (sizeof(((AmiToolApp*)0)->bt_display_text))
#define AMI_TOOL_BT_MAX_DISPLAY_BYTES (40U)
#define AMI_TOOL_BT_AMIIBO_ID_LEN (8U)
#define AMI_TOOL_BT_GENERATE_COMMAND_SIZE (2U + AMI_TOOL_BT_AMIIBO_ID_LEN)

static const uint8_t ami_tool_scene_bluetooth_generate_ack[] = {0xB0, 0xA2};
static const uint8_t ami_tool_scene_bluetooth_uid_reply_prefix[] = {0xB1, 0xA2};

static void ami_tool_scene_bluetooth_set_display_text(AmiToolApp* app, const char* text) {
    furi_assert(app);
    furi_assert(app->bt_mutex);

    furi_check(furi_mutex_acquire(app->bt_mutex, FuriWaitForever) == FuriStatusOk);
    snprintf(app->bt_display_text, AMI_TOOL_BT_DISPLAY_SIZE, "%s", text ? text : "");
    furi_mutex_release(app->bt_mutex);
}

static void ami_tool_scene_bluetooth_refresh_view(AmiToolApp* app) {
    furi_assert(app);
    furi_assert(app->bt_mutex);

    furi_check(furi_mutex_acquire(app->bt_mutex, FuriWaitForever) == FuriStatusOk);
    furi_string_set(app->text_box_store, app->bt_display_text);
    furi_mutex_release(app->bt_mutex);

    text_box_reset(app->text_box);
    text_box_set_text(app->text_box, furi_string_get_cstr(app->text_box_store));
    view_dispatcher_switch_to_view(app->view_dispatcher, AmiToolViewTextBox);
}

static void ami_tool_scene_bluetooth_notify_update(AmiToolApp* app) {
    view_dispatcher_send_custom_event(app->view_dispatcher, AmiToolEventBluetoothUpdate);
}

static void ami_tool_scene_bluetooth_send_generate_ack(AmiToolApp* app) {
    furi_assert(app);

    if(!app->bt_serial_profile || !app->bt_connected) {
        return;
    }

    if(ble_profile_serial_tx(
           app->bt_serial_profile,
           (uint8_t*)ami_tool_scene_bluetooth_generate_ack,
           sizeof(ami_tool_scene_bluetooth_generate_ack))) {
        ble_profile_serial_notify_buffer_is_empty(app->bt_serial_profile);
    }
}

static void ami_tool_scene_bluetooth_send_uid_response(AmiToolApp* app) {
    furi_assert(app);

    if(!app->bt_serial_profile || !app->bt_connected) {
        return;
    }

    uint8_t response[sizeof(ami_tool_scene_bluetooth_uid_reply_prefix) + sizeof(app->last_uid)] = {0};
    size_t response_size = sizeof(ami_tool_scene_bluetooth_uid_reply_prefix) + 1;

    memcpy(
        response,
        ami_tool_scene_bluetooth_uid_reply_prefix,
        sizeof(ami_tool_scene_bluetooth_uid_reply_prefix));

    if(app->tag_data_valid && app->last_uid_valid && app->last_uid_len > 0) {
        size_t uid_len = app->last_uid_len;
        if(uid_len > sizeof(app->last_uid)) {
            uid_len = sizeof(app->last_uid);
        }
        memcpy(
            response + sizeof(ami_tool_scene_bluetooth_uid_reply_prefix), app->last_uid, uid_len);
        response_size = sizeof(ami_tool_scene_bluetooth_uid_reply_prefix) + uid_len;
    } else {
        response[sizeof(ami_tool_scene_bluetooth_uid_reply_prefix)] = 0x00;
    }

    if(ble_profile_serial_tx(app->bt_serial_profile, response, response_size)) {
        ble_profile_serial_notify_buffer_is_empty(app->bt_serial_profile);
    }
}

static int8_t ami_tool_scene_bluetooth_hex_value(char ch) {
    if(ch >= '0' && ch <= '9') {
        return (int8_t)(ch - '0');
    }

    ch = (char)tolower((unsigned char)ch);
    if(ch >= 'a' && ch <= 'f') {
        return (int8_t)(10 + (ch - 'a'));
    }

    return -1;
}

static bool
    ami_tool_scene_bluetooth_parse_uuid(const char* hex, uint8_t* out, size_t out_len) {
    if(!hex || !out || out_len == 0) {
        return false;
    }

    size_t required = out_len * 2;
    if(strlen(hex) < required) {
        return false;
    }

    for(size_t i = 0; i < out_len; i++) {
        int8_t hi = ami_tool_scene_bluetooth_hex_value(hex[i * 2]);
        int8_t lo = ami_tool_scene_bluetooth_hex_value(hex[i * 2 + 1]);
        if(hi < 0 || lo < 0) {
            return false;
        }
        out[i] = (uint8_t)((hi << 4) | lo);
    }

    return true;
}

static bool ami_tool_scene_bluetooth_prepare_dump(AmiToolApp* app, const char* id_hex) {
    if(!app || !app->tag_data || !id_hex) {
        return false;
    }

    if(!ami_tool_has_retail_key(app) && ami_tool_load_retail_key(app) != AmiToolRetailKeyStatusOk) {
        return false;
    }

    uint8_t uuid[AMI_TOOL_BT_AMIIBO_ID_LEN];
    if(!ami_tool_scene_bluetooth_parse_uuid(id_hex, uuid, sizeof(uuid))) {
        return false;
    }

    ami_tool_clear_cached_tag(app);

    Ntag21xMetadataHeader header;
    RfidxStatus status = amiibo_generate(uuid, app->tag_data, &header);
    if(status != RFIDX_OK) {
        return false;
    }

    const DumpedKeys* keys = (const DumpedKeys*)app->retail_key;
    DerivedKey data_key = {0};
    DerivedKey tag_key = {0};

    status = amiibo_derive_key(&keys->data, app->tag_data, &data_key);
    if(status != RFIDX_OK) {
        return false;
    }

    status = amiibo_derive_key(&keys->tag, app->tag_data, &tag_key);
    if(status != RFIDX_OK) {
        return false;
    }

    status = amiibo_sign_payload(&tag_key, &data_key, app->tag_data);
    if(status != RFIDX_OK) {
        return false;
    }

    status = amiibo_cipher(&data_key, app->tag_data);
    if(status != RFIDX_OK) {
        return false;
    }

    if(app->tag_data->pages_total < 2) {
        return false;
    }

    uint8_t uid[7];
    memcpy(uid, app->tag_data->page[0].data, 3);
    memcpy(uid + 3, app->tag_data->page[1].data, 4);
    ami_tool_store_uid(app, uid, sizeof(uid));

    if(ami_tool_compute_password_from_uid(uid, sizeof(uid), &app->tag_password)) {
        app->tag_password_valid = true;
    } else {
        app->tag_password_valid = false;
        memset(&app->tag_password, 0, sizeof(app->tag_password));
    }

    static const uint8_t default_pack[4] = {0x80, 0x80, 0x00, 0x00};
    memcpy(app->tag_pack, default_pack, sizeof(default_pack));
    app->tag_pack_valid = true;
    app->tag_data_valid = true;

    return true;
}

static bool ami_tool_scene_bluetooth_queue_generate_command(
    AmiToolApp* app,
    const uint8_t* data,
    size_t size) {
    if(!app || !data || size < AMI_TOOL_BT_GENERATE_COMMAND_SIZE) {
        return false;
    }

    if(data[0] != 0xA2 || data[1] != 0xB0) {
        return false;
    }

    furi_check(furi_mutex_acquire(app->bt_mutex, FuriWaitForever) == FuriStatusOk);
    for(size_t i = 0; i < AMI_TOOL_BT_AMIIBO_ID_LEN; i++) {
        snprintf(
            &app->bt_pending_generate_id[i * 2],
            sizeof(app->bt_pending_generate_id) - (i * 2),
            "%02X",
            data[2 + i]);
    }
    app->bt_pending_generate = true;
    furi_mutex_release(app->bt_mutex);

    return true;
}

static bool ami_tool_scene_bluetooth_take_pending_generate_command(
    AmiToolApp* app,
    char* out_id_hex,
    size_t out_size) {
    bool available = false;

    furi_check(furi_mutex_acquire(app->bt_mutex, FuriWaitForever) == FuriStatusOk);
    if(app->bt_pending_generate && out_id_hex && out_size > 0) {
        snprintf(out_id_hex, out_size, "%s", app->bt_pending_generate_id);
        app->bt_pending_generate = false;
        memset(app->bt_pending_generate_id, 0, sizeof(app->bt_pending_generate_id));
        available = true;
    }
    furi_mutex_release(app->bt_mutex);

    return available;
}

static void ami_tool_scene_bluetooth_handle_generate(AmiToolApp* app) {
    char id_hex[sizeof(app->bt_pending_generate_id)] = {0};
    if(!ami_tool_scene_bluetooth_take_pending_generate_command(app, id_hex, sizeof(id_hex))) {
        return;
    }

    if(!ami_tool_scene_bluetooth_prepare_dump(app, id_hex)) {
        return;
    }

    ami_tool_info_stop_emulation(app);
    if(!ami_tool_info_start_emulation(app)) {
        return;
    }

    ami_tool_scene_bluetooth_send_generate_ack(app);

    FuriString* name = furi_string_alloc();
    if(!ami_tool_info_get_name_for_id(app, id_hex, name) || furi_string_empty(name)) {
        furi_string_set(name, id_hex);
    }

    furi_string_printf(app->text_box_store, "Emulating for \"%s\"", furi_string_get_cstr(name));
    text_box_reset(app->text_box);
    text_box_set_text(app->text_box, furi_string_get_cstr(app->text_box_store));
    view_dispatcher_switch_to_view(app->view_dispatcher, AmiToolViewTextBox);
    furi_string_free(name);
}

static void ami_tool_scene_bluetooth_bt_status_changed_callback(BtStatus status, void* context) {
    AmiToolApp* app = context;
    furi_assert(app);

    app->bt_connected = (status == BtStatusConnected);

    switch(status) {
    case BtStatusConnected:
        ami_tool_scene_bluetooth_set_display_text(
            app, "Bluetooth Connection\n\nAwaiting commands");
        break;
    case BtStatusAdvertising:
        ami_tool_scene_bluetooth_set_display_text(
            app, "Bluetooth Connection\n\nAwaiting connections");
        break;
    case BtStatusOff:
        ami_tool_scene_bluetooth_set_display_text(app, "Bluetooth Connection\n\nBluetooth off");
        break;
    case BtStatusUnavailable:
    default:
        ami_tool_scene_bluetooth_set_display_text(
            app, "Bluetooth Connection\n\nBluetooth unavailable");
        break;
    }

    ami_tool_scene_bluetooth_notify_update(app);
}

static uint16_t ami_tool_scene_bluetooth_serial_event_callback(
    SerialServiceEvent event,
    void* context) {
    AmiToolApp* app = context;
    furi_assert(app);

    if(event.event == SerialServiceEventTypeDataReceived) {
        if(ami_tool_scene_bluetooth_queue_generate_command(app, event.data.buffer, event.data.size)) {
            view_dispatcher_send_custom_event(app->view_dispatcher, AmiToolEventBluetoothGenerate);
        } else if(
            event.data.size >= 2 && event.data.buffer[0] == 0xA2 && event.data.buffer[1] == 0xB1) {
            view_dispatcher_send_custom_event(app->view_dispatcher, AmiToolEventBluetoothUidQuery);
        }
    } else if(event.event == SerialServiceEventTypesBleResetRequest) {
        ami_tool_scene_bluetooth_set_display_text(
            app, "Bluetooth Connection\n\nBLE reset requested");
        ami_tool_scene_bluetooth_notify_update(app);
    }

    return AMI_TOOL_BT_SERIAL_BUFFER_SIZE;
}

static void ami_tool_scene_bluetooth_stop(AmiToolApp* app) {
    furi_assert(app);

    if(app->bt_serial_profile) {
        ble_profile_serial_set_event_callback(app->bt_serial_profile, 0, NULL, NULL);
        bt_set_status_changed_callback(app->bt, NULL, NULL);
        furi_hal_bt_stop_advertising();
        bt_disconnect(app->bt);
        bt_profile_restore_default(app->bt);
        app->bt_serial_profile = NULL;
    }

    app->bt_connected = false;
    app->bt_pending_generate = false;
    memset(app->bt_pending_generate_id, 0, sizeof(app->bt_pending_generate_id));
    ami_tool_info_stop_emulation(app);
    ami_tool_scene_bluetooth_set_display_text(app, "Bluetooth Connection\n\nAwaiting connections");
}

void ami_tool_scene_bluetooth_on_enter(void* context) {
    AmiToolApp* app = context;

    app->bt_connected = false;
    ami_tool_scene_bluetooth_set_display_text(app, "Bluetooth Connection\n\nAwaiting connections");
    ami_tool_scene_bluetooth_refresh_view(app);

    app->bt_serial_profile = bt_profile_start(app->bt, ble_profile_serial, NULL);
    if(app->bt_serial_profile) {
        bt_set_status_changed_callback(app->bt, ami_tool_scene_bluetooth_bt_status_changed_callback, app);
        ble_profile_serial_set_event_callback(
            app->bt_serial_profile,
            AMI_TOOL_BT_SERIAL_BUFFER_SIZE,
            ami_tool_scene_bluetooth_serial_event_callback,
            app);
        furi_hal_bt_start_advertising();
    } else {
        ami_tool_scene_bluetooth_set_display_text(
            app, "Bluetooth Connection\n\nFailed to enable BLE");
        ami_tool_scene_bluetooth_refresh_view(app);
    }
}

bool ami_tool_scene_bluetooth_on_event(void* context, SceneManagerEvent event) {
    AmiToolApp* app = context;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == AmiToolEventBluetoothUpdate) {
            ami_tool_scene_bluetooth_refresh_view(app);
            return true;
        } else if(event.event == AmiToolEventBluetoothGenerate) {
            ami_tool_scene_bluetooth_handle_generate(app);
            return true;
        } else if(event.event == AmiToolEventBluetoothUidQuery) {
            ami_tool_scene_bluetooth_send_uid_response(app);
            return true;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        scene_manager_previous_scene(app->scene_manager);
        return true;
    }

    return false;
}

void ami_tool_scene_bluetooth_on_exit(void* context) {
    AmiToolApp* app = context;
    ami_tool_scene_bluetooth_stop(app);
}
