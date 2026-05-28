#include "droidbeacon_scene.h"

static void beaming_button_callback(GuiButtonType result, InputType type, void* context);

// ── Build and start the beacon ────────────────────────────────────────────────
// BLE advertisement packet structure for Disney park beacons:
//
//   [0]    0x09  — AD length (9 bytes follow)
//   [1]    0xFF  — AD type: manufacturer specific
//   [2]    0x83  — Disney manufacturer ID low byte (0x0183 little-endian)
//   [3]    0x01  — Disney manufacturer ID high byte
//   [4-9]  payload bytes from DROID_BEACONS[].payload
//
// Total: 10 bytes

static bool start_beacon(DroidbeaconApp* app) {
    const DroidBeacon* beacon = &DROID_BEACONS[app->selected_beacon];

    // Build the advertisement data — matches working JS script format:
    // Uint8Array([0x09, 0xFF, 0x83, 0x01, 0x0A, 0x04, XX, 0x02, 0xA6, 0x01])
    uint8_t adv_data[] = {
        0x09,                  // AD length (9 bytes follow)
        0xFF,                  // AD type: manufacturer specific
        DISNEY_MFR_ID_LO,      // 0x83 — Disney mfr ID low byte
        DISNEY_MFR_ID_HI,      // 0x01 — Disney mfr ID high byte
        beacon->payload[0],
        beacon->payload[1],
        beacon->payload[2],
        beacon->payload[3],
        beacon->payload[4],
        beacon->payload[5],
    };

    // Config matches working JS: setConfig(mac, 0x1F, 50, 150)
    GapExtraBeaconConfig config = {
        .min_adv_interval_ms = 50,
        .max_adv_interval_ms = 150,
        .adv_channel_map     = GapAdvChannelMapAll,
        .adv_power_level     = GapAdvPowerLevel_6dBm,
        .address_type        = GapAddressTypePublic,
        .address             = { 0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC },
    };

    if(!furi_hal_bt_extra_beacon_set_config(&config)) return false;
    if(!furi_hal_bt_extra_beacon_set_data(adv_data, sizeof(adv_data))) return false;
    if(!furi_hal_bt_extra_beacon_start()) return false;

    return true;
}

static void update_beaming_widget(DroidbeaconApp* app) {
    widget_reset(app->beaming_widget);

    const DroidBeacon* beacon = &DROID_BEACONS[app->selected_beacon];

    widget_add_string_element(app->beaming_widget, 64, 4,  AlignCenter, AlignTop, FontPrimary,   beacon->name);
    widget_add_string_element(app->beaming_widget, 64, 20, AlignCenter, AlignTop, FontSecondary, beacon->description);
    widget_add_string_element(app->beaming_widget, 64, 34, AlignCenter, AlignTop, FontSecondary, "Broadcasting...");
    widget_add_button_element(app->beaming_widget, GuiButtonTypeLeft,  "Stop",    beaming_button_callback, app);
}

static void beaming_button_callback(GuiButtonType result, InputType type, void* context) {
    if(type != InputTypeShort) return;
    DroidbeaconApp* app = context;
    if(result == GuiButtonTypeLeft) {
        view_dispatcher_send_custom_event(app->view_dispatcher, DroidbeaconEventBeamStop);
    }
}

void droidbeacon_scene_beaming_on_enter(void* context) {
    furi_assert(context);
    DroidbeaconApp* app = context;

    app->is_beaming = start_beacon(app);

    update_beaming_widget(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, DroidbeaconViewBeaming);

    if(app->is_beaming) {
        notification_message(app->notifications, &sequence_blink_start_blue);
    } else {
        notification_message(app->notifications, &sequence_error);
    }
}

bool droidbeacon_scene_beaming_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);
    DroidbeaconApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == DroidbeaconEventBeamStop) {
            scene_manager_next_scene(app->scene_manager, DroidbeaconSceneResult);
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        scene_manager_next_scene(app->scene_manager, DroidbeaconSceneResult);
        consumed = true;
    }

    return consumed;
}

void droidbeacon_scene_beaming_on_exit(void* context) {
    furi_assert(context);
    DroidbeaconApp* app = context;

    if(furi_hal_bt_extra_beacon_is_active()) {
        furi_hal_bt_extra_beacon_stop();
    }
    app->is_beaming = false;

    notification_message(app->notifications, &sequence_blink_stop);
    widget_reset(app->beaming_widget);
}
