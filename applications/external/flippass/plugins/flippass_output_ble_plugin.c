/**
 * @file flippass_output_ble_plugin.c
 * @brief Late-loaded Bluetooth HID transport provider for FlipPass.
 */

#include "flippass_output_plugin.h"

#include <bt/bt_service/bt.h>
#include <extra_profiles/hid_profile.h>
#include <furi.h>
#include <furi_hal.h>
#include <gap.h>
#include <storage/storage.h>
#include <stdio.h>
#include <string.h>

#define FLIPPASS_BT_KEYS_STORAGE_DIR       EXT_PATH("apps_data/bad_usb")
#define FLIPPASS_BT_KEYS_STORAGE_PATH      EXT_PATH("apps_data/bad_usb/.bt_hid.keys")
#define FLIPPASS_BLE_SETUP_DELAY_MS        200U
#define FLIPPASS_BLE_WAIT_STEP_MS          100U
#define FLIPPASS_BLE_CONNECT_TIMEOUT_MS    15000U
#define FLIPPASS_BLE_CONNECT_SETTLE_MS     300U
#define FLIPPASS_BLE_DISCONNECT_STEP_MS    50U
#define FLIPPASS_BLE_DISCONNECT_TIMEOUT_MS 1000U
#define FLIPPASS_BLE_NAME_PREFIX           "BadUSB"
#define FLIPPASS_BLE_NAME_BUFFER_SIZE      21U
#define FLIPPASS_BLE_MAC_XOR               0x0002U

typedef struct {
    char name[FLIPPASS_BLE_NAME_BUFFER_SIZE];
    uint8_t mac[GAP_MAC_ADDR_SIZE];
    bool bonding;
    GapPairing pairing;
} FlipPassBleProfileParams;

typedef struct {
    Bt* bt;
    FuriHalBleProfileBase* profile;
    bool connected;
    BtStatus status;
} FlipPassBlePluginSession;

static FlipPassBlePluginSession* flippass_output_ble_session = NULL;

static void flippass_output_ble_progress(
    const FlipPassOutputPluginHostApiV1* host_api,
    const char* stage,
    const char* detail,
    uint8_t percent) {
    if(host_api != NULL && host_api->progress != NULL) {
        host_api->progress(host_api->host_context, stage, detail, percent);
    }
}

static void
    flippass_output_ble_log(const FlipPassOutputPluginHostApiV1* host_api, const char* message) {
    if(host_api != NULL && host_api->log != NULL && message != NULL) {
        host_api->log(host_api->host_context, "flippass_output_ble", message);
    }
}

static bool flippass_output_ble_should_cancel(const FlipPassOutputPluginHostApiV1* host_api) {
    return host_api != NULL && host_api->should_cancel != NULL &&
           host_api->should_cancel(host_api->host_context);
}

static bool flippass_output_ble_delay_or_cancel(
    const FlipPassOutputPluginHostApiV1* host_api,
    uint32_t delay_ms) {
    while(delay_ms > 0U) {
        const uint32_t step_ms = (delay_ms > 25U) ? 25U : delay_ms;

        if(flippass_output_ble_should_cancel(host_api)) {
            return false;
        }

        furi_delay_ms(step_ms);
        delay_ms -= step_ms;
    }

    return !flippass_output_ble_should_cancel(host_api);
}

static void flippass_output_ble_get_name_impl(char* buffer, size_t size) {
    if(buffer == NULL || size == 0U) {
        return;
    }

    snprintf(buffer, size, "%s %s", FLIPPASS_BLE_NAME_PREFIX, furi_hal_version_get_name_ptr());
}

static FuriHalBleProfileBase* flippass_ble_profile_start(FuriHalBleProfileParams profile_params) {
    UNUSED(profile_params);
    return ble_profile_hid->start(NULL);
}

static void flippass_ble_profile_stop(FuriHalBleProfileBase* profile) {
    ble_profile_hid->stop(profile);
}

static void
    flippass_ble_profile_get_config(GapConfig* config, FuriHalBleProfileParams profile_params) {
    furi_check(config);
    FlipPassBleProfileParams* params = profile_params;

    ble_profile_hid->get_gap_config(config, NULL);

    if(params != NULL) {
        memcpy(config->mac_address, params->mac, sizeof(config->mac_address));
        strlcpy(config->adv_name + 1, params->name, sizeof(config->adv_name) - 1);
        config->bonding_mode = params->bonding;
        config->pairing_method = params->pairing;
    }
}

static const FuriHalBleProfileTemplate flippass_ble_profile_template = {
    .start = flippass_ble_profile_start,
    .stop = flippass_ble_profile_stop,
    .get_gap_config = flippass_ble_profile_get_config,
};

static void flippass_ble_status_changed(BtStatus status, void* context) {
    FlipPassBlePluginSession* session = context;
    furi_assert(session);
    session->status = status;
    session->connected = (status == BtStatusConnected);
}

static bool flippass_ble_prepare_storage(void) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    const bool ok = storage_simply_mkdir(storage, FLIPPASS_BT_KEYS_STORAGE_DIR);
    furi_record_close(RECORD_STORAGE);
    return ok;
}

static void flippass_ble_prepare_profile_params(FlipPassBleProfileParams* profile_params) {
    furi_assert(profile_params);

    memset(profile_params, 0, sizeof(*profile_params));
    memcpy(profile_params->mac, furi_hal_version_get_ble_mac(), sizeof(profile_params->mac));
    profile_params->mac[2]++;
    profile_params->mac[0] ^= FLIPPASS_BLE_MAC_XOR;
    profile_params->mac[1] ^= FLIPPASS_BLE_MAC_XOR >> 8;
    profile_params->bonding = true;
    profile_params->pairing = GapPairingPinCodeVerifyYesNo;
    flippass_output_ble_get_name_impl(profile_params->name, sizeof(profile_params->name));
}

static bool flippass_ble_session_start(const FlipPassOutputPluginHostApiV1* host_api) {
    if(flippass_output_ble_session != NULL) {
        return true;
    }

    if(!flippass_ble_prepare_storage()) {
        return false;
    }

    FlipPassBlePluginSession* session = malloc(sizeof(FlipPassBlePluginSession));
    if(session == NULL) {
        return false;
    }
    memset(session, 0, sizeof(*session));

    session->bt = furi_record_open(RECORD_BT);
    bt_disconnect(session->bt);
    if(!flippass_output_ble_delay_or_cancel(host_api, FLIPPASS_BLE_SETUP_DELAY_MS)) {
        bt_keys_storage_set_default_path(session->bt);
        furi_record_close(RECORD_BT);
        free(session);
        return false;
    }
    bt_keys_storage_set_storage_path(session->bt, FLIPPASS_BT_KEYS_STORAGE_PATH);

    FlipPassBleProfileParams profile_params;
    flippass_ble_prepare_profile_params(&profile_params);

    session->profile =
        bt_profile_start(session->bt, &flippass_ble_profile_template, (void*)&profile_params);
    if(session->profile == NULL) {
        bt_keys_storage_set_default_path(session->bt);
        furi_record_close(RECORD_BT);
        free(session);
        return false;
    }

    session->connected = false;
    session->status = BtStatusOff;
    bt_set_status_changed_callback(session->bt, flippass_ble_status_changed, session);
    furi_hal_bt_start_advertising();
    session->status = BtStatusAdvertising;

    flippass_output_ble_session = session;
    return true;
}

static void flippass_output_ble_release_all(const FlipPassOutputPluginHostApiV1* host_api) {
    UNUSED(host_api);
    if(flippass_output_ble_session == NULL || flippass_output_ble_session->profile == NULL) {
        return;
    }

    ble_profile_hid_kb_release_all(flippass_output_ble_session->profile);
}

static bool flippass_ble_ensure_advertising(const FlipPassOutputPluginHostApiV1* host_api) {
    if(!flippass_ble_session_start(host_api)) {
        return false;
    }

    if(!flippass_output_ble_session->connected) {
        flippass_output_ble_release_all(NULL);
        bt_disconnect(flippass_output_ble_session->bt);
        if(!flippass_output_ble_delay_or_cancel(host_api, FLIPPASS_BLE_SETUP_DELAY_MS)) {
            return false;
        }
        furi_hal_bt_start_advertising();
        flippass_output_ble_session->status = BtStatusAdvertising;
    }

    return true;
}

static bool flippass_ble_wait_connected(const FlipPassOutputPluginHostApiV1* host_api) {
    if(!flippass_ble_ensure_advertising(host_api)) {
        return false;
    }

    uint32_t waited_ms = 0U;
    while(flippass_output_ble_session != NULL && !flippass_output_ble_session->connected &&
          waited_ms < FLIPPASS_BLE_CONNECT_TIMEOUT_MS &&
          !flippass_output_ble_should_cancel(host_api)) {
        if(!flippass_output_ble_delay_or_cancel(host_api, FLIPPASS_BLE_WAIT_STEP_MS)) {
            return false;
        }
        waited_ms += FLIPPASS_BLE_WAIT_STEP_MS;
        flippass_output_ble_progress(
            host_api,
            "Connecting",
            "Waiting for Bluetooth HID host.",
            (uint8_t)(5U + ((waited_ms * 35U) / FLIPPASS_BLE_CONNECT_TIMEOUT_MS)));
    }

    if(flippass_output_ble_session == NULL || !flippass_output_ble_session->connected ||
       flippass_output_ble_should_cancel(host_api)) {
        return false;
    }

    flippass_output_ble_progress(host_api, "Typing", "Bluetooth HID connected.", 40U);
    return flippass_output_ble_delay_or_cancel(host_api, FLIPPASS_BLE_CONNECT_SETTLE_MS);
}

static bool flippass_output_ble_begin(const FlipPassOutputPluginHostApiV1* host_api) {
    if(!flippass_ble_wait_connected(host_api)) {
        return false;
    }

    flippass_output_ble_release_all(host_api);
    return true;
}

static bool
    flippass_output_ble_press_key(const FlipPassOutputPluginHostApiV1* host_api, uint16_t hid_key) {
    UNUSED(host_api);

    if(hid_key == HID_KEYBOARD_NONE || flippass_output_ble_session == NULL ||
       flippass_output_ble_session->profile == NULL) {
        return false;
    }

    ble_profile_hid_kb_press(flippass_output_ble_session->profile, hid_key);
    return true;
}

static bool flippass_output_ble_release_key(
    const FlipPassOutputPluginHostApiV1* host_api,
    uint16_t hid_key) {
    UNUSED(host_api);

    if(hid_key == HID_KEYBOARD_NONE || flippass_output_ble_session == NULL ||
       flippass_output_ble_session->profile == NULL) {
        return false;
    }

    ble_profile_hid_kb_release(flippass_output_ble_session->profile, hid_key);
    return true;
}

static void flippass_output_ble_end(const FlipPassOutputPluginHostApiV1* host_api) {
    flippass_output_ble_release_all(host_api);
}

static bool flippass_output_ble_is_connected(const FlipPassOutputPluginHostApiV1* host_api) {
    UNUSED(host_api);
    return flippass_output_ble_session != NULL && flippass_output_ble_session->connected;
}

static bool flippass_output_ble_is_advertising(const FlipPassOutputPluginHostApiV1* host_api) {
    UNUSED(host_api);
    return flippass_output_ble_session != NULL && !flippass_output_ble_session->connected &&
           flippass_output_ble_session->status == BtStatusAdvertising;
}

static bool flippass_output_ble_advertise(const FlipPassOutputPluginHostApiV1* host_api) {
    return flippass_ble_ensure_advertising(host_api);
}

static void flippass_ble_wait_disconnected(const FlipPassOutputPluginHostApiV1* host_api) {
    uint32_t waited_ms = 0U;

    while(flippass_output_ble_session != NULL && flippass_output_ble_session->connected &&
          waited_ms < FLIPPASS_BLE_DISCONNECT_TIMEOUT_MS) {
        furi_delay_ms(FLIPPASS_BLE_DISCONNECT_STEP_MS);
        waited_ms += FLIPPASS_BLE_DISCONNECT_STEP_MS;
    }

    if(flippass_output_ble_session != NULL && flippass_output_ble_session->connected) {
        flippass_output_ble_log(host_api, "cleanup disconnect wait timed out");
    }
}

static void flippass_output_ble_cleanup(const FlipPassOutputPluginHostApiV1* host_api) {
    if(flippass_output_ble_session == NULL) {
        return;
    }

    flippass_output_ble_release_all(host_api);
    bt_disconnect(flippass_output_ble_session->bt);
    flippass_ble_wait_disconnected(host_api);
    furi_hal_bt_stop_advertising();
    furi_delay_ms(FLIPPASS_BLE_SETUP_DELAY_MS);
    flippass_output_ble_session->connected = false;
    flippass_output_ble_session->status = BtStatusOff;
    bt_set_status_changed_callback(flippass_output_ble_session->bt, NULL, NULL);
    bt_keys_storage_set_default_path(flippass_output_ble_session->bt);
    if(!bt_profile_restore_default(flippass_output_ble_session->bt)) {
        flippass_output_ble_log(host_api, "cleanup restore default profile failed");
    }
    flippass_output_ble_session->profile = NULL;
    furi_record_close(RECORD_BT);
    free(flippass_output_ble_session);
    flippass_output_ble_session = NULL;
}

static const FlipPassOutputPluginV1 flippass_output_ble_plugin = {
    .api_version = FLIPPASS_OUTPUT_PLUGIN_API_VERSION,
    .module_name = "flippass_output_ble",
    .transport = FlipPassOutputPluginTransportBluetooth,
    .begin = flippass_output_ble_begin,
    .press_key = flippass_output_ble_press_key,
    .release_key = flippass_output_ble_release_key,
    .release_all = flippass_output_ble_release_all,
    .end = flippass_output_ble_end,
    .is_connected = flippass_output_ble_is_connected,
    .is_advertising = flippass_output_ble_is_advertising,
    .advertise = flippass_output_ble_advertise,
    .get_name = flippass_output_ble_get_name_impl,
    .cleanup = flippass_output_ble_cleanup,
};

static const FlipperAppPluginDescriptor flippass_output_ble_descriptor = {
    .appid = FLIPPASS_OUTPUT_BLE_PLUGIN_APP_ID,
    .ep_api_version = FLIPPASS_OUTPUT_PLUGIN_API_VERSION,
    .entry_point = &flippass_output_ble_plugin,
};

const FlipPassOutputPluginV1* flippass_output_ble_plugin_table(void) {
    return &flippass_output_ble_plugin;
}

const FlipperAppPluginDescriptor* flippass_output_ble_plugin_ep(void) {
    return &flippass_output_ble_descriptor;
}
