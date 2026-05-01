#include "../fire_string.h"
#include "string_generator.h"

static void bt_conn_scene_builder(FireString* app);

#define SPAM_UNLOCK 5

typedef enum {
    WorkerEvtStartStop = (1 << 0),
    // WorkerEvtPauseResume = (1 << 1),
    // WorkerEvtEnd = (1 << 2),
    // WorkerEvtConnect = (1 << 3),
    // WorkerEvtDisconnect = (1 << 4),
} WorkerEvtFlags;

uint8_t bt_right_btn_clk_cnt = 0;
bool bt_state = false;
bool bt_setup_complete = false;

static const BleProfileHidParams ble_hid_params = {
    .device_name_prefix = "FS-HID",
    .mac_xor = 0x0002,
};

bool bt_ducky_string(FireString* app) {
    if(furi_string_size(app->fire_string) == 0) {
        return false;
    }
    const char* param = furi_string_get_cstr(app->fire_string);
    uint16_t keycode = HID_KEYBOARD_NONE;
    uint32_t i = 0;
    while(param[i] != '\0') {
        keycode = ASCII_TO_KEY(app, param[i]);
        if(keycode != HID_KEYBOARD_NONE) {
            ble_profile_hid_kb_press(app->hid->ble_hid_profile, keycode);
            ble_profile_hid_kb_release(app->hid->ble_hid_profile, keycode);
        }
        i++;
    }
    return true;
}

void bt_remove_pairing(FireString* app) {
    Bt* bt = app->hid->bt;
    bt_disconnect(bt);

    // Wait 2nd core to update nvm storage
    furi_delay_ms(200);

    furi_hal_bt_stop_advertising();

    bt_forget_bonded_devices(bt);

    furi_hal_bt_start_advertising();
}

static int32_t spam_worker(void* context) {
    FURI_LOG_I(TAG, "spam_worker");
    furi_check(context);

    FireString* app = context;

    FURI_LOG_I(TAG, "spam_worker %p starting", furi_thread_get_id(app->thread));

    while(furi_thread_flags_get() == 0 && app->hid->bt_connected) {
        uint16_t keycode = HID_KEYBOARD_NONE;
        if(app->settings->str_type == StrType_Words) {
            const char* param = get_rnd_word(app, false);
            uint32_t i = 0;
            while(param[i] != '\0') {
                keycode = ASCII_TO_KEY(app, param[i]);
                if(keycode != HID_KEYBOARD_NONE) {
                    ble_profile_hid_kb_press(app->hid->ble_hid_profile, keycode);
                    ble_profile_hid_kb_release(app->hid->ble_hid_profile, keycode);
                }
                i++;
            }
        } else {
            char rnd_char = get_rnd_char(app, false);
            keycode = ASCII_TO_KEY(app, rnd_char);
            if(keycode != HID_KEYBOARD_NONE) {
                ble_profile_hid_kb_press(app->hid->ble_hid_profile, keycode);
                ble_profile_hid_kb_release(app->hid->ble_hid_profile, keycode);
            }
        }
    }

    FURI_LOG_I(TAG, "spam_worker %p ended", furi_thread_get_id(app->thread));

    return 0;
}

void bt_btn_callback(GuiButtonType result, InputType type, void* context) {
    // FURI_LOG_T(TAG, "bt_btn_callback");
    furi_check(context);

    FireString* app = context;

    if(type == InputTypeShort) {
        switch(result) {
        case GuiButtonTypeCenter:
            if(!bt_setup_complete) {
                bt_remove_pairing(app);
            } else {
                notification_message(app->notifications, &sequence_single_vibro);
                bt_ducky_string(app);
            }
            break;
        case GuiButtonTypeLeft:
            scene_manager_search_and_switch_to_previous_scene(
                app->scene_manager, FireStringScene_GenerateStepTwo);
            break;
        case GuiButtonTypeRight:
            if(bt_setup_complete && bt_right_btn_clk_cnt <= SPAM_UNLOCK) {
                if(bt_right_btn_clk_cnt == SPAM_UNLOCK) {
                    notification_message(app->notifications, &sequence_audiovisual_alert);
                    notification_message(app->notifications, &sequence_display_backlight_on);
                    app->thread = furi_thread_alloc_ex("SpamWorker", 2048, spam_worker, app);
                    bt_conn_scene_builder(app);
                }
                bt_right_btn_clk_cnt++;
            } else if(!bt_setup_complete) {
                bt_setup_complete = true;
                bt_conn_scene_builder(app);
            }
            break;
        }
    }

    if(bt_setup_complete && bt_right_btn_clk_cnt > SPAM_UNLOCK) {
        if(type == InputTypeRepeat && app->hid->bt_connected) {
            if(furi_thread_get_state(app->thread) == FuriThreadStateStopped) {
                furi_thread_start(app->thread);
            }
        }
        if(type == InputTypeRelease) {
            furi_thread_flags_set(furi_thread_get_id(app->thread), WorkerEvtStartStop);
        }
    }
}

static void bt_conn_scene_builder(FireString* app) {
    FURI_LOG_T(TAG, "bt_conn_scene_builder");

    widget_reset(app->widget);

    widget_add_icon_element(app->widget, 80, 20, &I_Bad_BLE_48x22);
    if(app->hid->bt_connected) {
        widget_add_string_element(
            app->widget, 0, 0, AlignLeft, AlignTop, FontPrimary, "Ready to send Fire String");
        widget_add_button_element(app->widget, GuiButtonTypeCenter, "Send", bt_btn_callback, app);
        if(bt_right_btn_clk_cnt < SPAM_UNLOCK) {
            widget_add_icon_element(app->widget, 0, 20, &I_Connected_62x31);
            widget_add_icon_element(app->widget, 62, 22, &I_Smile_18x18);
            widget_add_button_element(app->widget, GuiButtonTypeRight, "", bt_btn_callback, app);
        } else {
            widget_add_icon_element(app->widget, 0, 20, &I_WarningDolphin_45x42);
            widget_add_icon_element(app->widget, 62, 20, &I_EviSmile2_18x21);
            widget_add_button_element(
                app->widget, GuiButtonTypeRight, "Spam", bt_btn_callback, app);
        }
    } else {
        widget_add_string_element(
            app->widget, 0, 0, AlignLeft, AlignTop, FontPrimary, "Waiting for connection...");
        widget_add_icon_element(app->widget, 62, 22, &I_Error_18x18);
        widget_add_icon_element(app->widget, 0, 20, &I_Connect_me_62x31);
    }
    widget_add_button_element(app->widget, GuiButtonTypeLeft, "Back", bt_btn_callback, app);
}

static void bt_setup_scene_builder(FireString* app) {
    FURI_LOG_T(TAG, "bt_setup_scene_builder");

    widget_reset(app->widget);

    const char* title_str = (app->hid->bt_connected) ? "Connected!" : "Pairing...";

    widget_add_string_element(app->widget, 65, 0, AlignLeft, AlignTop, FontPrimary, title_str);
    widget_add_icon_element(app->widget, 80, 20, &I_Bad_BLE_48x22);

    if(app->hid->bt_connected) {
        widget_add_icon_element(app->widget, 63, 24, &I_Ble_connected_15x15);
        widget_add_icon_element(app->widget, 0, 20, &I_Connected_62x31);
        widget_add_button_element(app->widget, GuiButtonTypeRight, "Next", bt_btn_callback, app);
    } else {
        widget_add_icon_element(app->widget, 63, 24, &I_Ble_disconnected_15x15);
        widget_add_icon_element(app->widget, 0, 0, &I_NFC_dolphin_emulation_51x64);
    }

    widget_add_button_element(app->widget, GuiButtonTypeCenter, "Unpair", bt_btn_callback, app);
    widget_add_button_element(app->widget, GuiButtonTypeLeft, "Back", bt_btn_callback, app);
}

static void bt_hid_connection_status_changed_callback(BtStatus status, void* context) {
    FURI_LOG_T(TAG, "bt_hid_connection_status_changed_callback");

    furi_assert(context);
    FireString* app = context;
    bool conn_status_change = app->hid->bt_connected;
    app->hid->bt_connected = (status == BtStatusConnected);

    if(conn_status_change != app->hid->bt_connected) {
        conn_status_change = true;
    } else {
        conn_status_change = false;
    }

    if(conn_status_change) {
        if(app->hid->bt_connected) {
            notification_message(app->notifications, &sequence_set_blue_255);
        } else {
            notification_message(app->notifications, &sequence_reset_blue);
        }
        notification_message(app->notifications, &sequence_single_vibro);
        if(!bt_setup_complete) {
            bt_setup_scene_builder(app);
        } else {
            if(app->hid->bt_connected) {
                bt_conn_scene_builder(app);
            } else {
                bt_setup_complete = false;
                bt_right_btn_clk_cnt = 0;
                bt_setup_scene_builder(app);
            }
        }
    }
}

static void bt_init(FireString* app) {
    FURI_LOG_T(TAG, "bt_init");
    app->hid->bt = furi_record_open(RECORD_BT);
    bt_disconnect(app->hid->bt);

    // Wait 2nd core to update nvm storage
    furi_delay_ms(200);

    bt_keys_storage_set_storage_path(app->hid->bt, APP_DATA_PATH(HID_BT_KEYS_STORAGE_NAME));

    app->hid->ble_hid_profile =
        bt_profile_start(app->hid->bt, ble_profile_hid, (void*)&ble_hid_params);
    furi_check(app->hid->ble_hid_profile);

    furi_hal_bt_start_advertising();
    bt_set_status_changed_callback(app->hid->bt, bt_hid_connection_status_changed_callback, app);
}

void fire_string_scene_on_enter_bluetooth(void* context) {
    FURI_LOG_T(TAG, "fire_string_scene_on_enter_bluetooth");
    furi_check(context);

    FireString* app = context;

    bt_right_btn_clk_cnt = 0;
    app->hid->bt_connected = false;
    bt_setup_complete = false;

    bt_init(app);

    bt_setup_scene_builder(app);

    view_dispatcher_switch_to_view(app->view_dispatcher, FireStringView_Widget);
}

bool fire_string_scene_on_event_bluetooth(void* context, SceneManagerEvent event) {
    // FURI_LOG_T(TAG, "fire_string_scene_on_event_bluetooth");
    furi_check(context);

    FireString* app = context;
    bool consumed = false;

    switch(event.type) {
    case SceneManagerEventTypeCustom:
        switch(event.event) {
        default:
            break;
        }
        break;
    case SceneManagerEventTypeTick:
        break;
    case SceneManagerEventTypeBack:
        consumed = true;
        scene_manager_search_and_switch_to_previous_scene(
            app->scene_manager, FireStringScene_GenerateStepTwo);
        break;
    default:
        break;
    }
    return consumed;
}

void fire_string_scene_on_exit_bluetooth(void* context) {
    FURI_LOG_T(TAG, "fire_string_scene_on_exit_bluetooth");
    furi_check(context);

    FireString* app = context;

    bt_disconnect(app->hid->bt);
    bt_set_status_changed_callback(app->hid->bt, NULL, NULL);

    // Wait 2nd core to update nvm storage
    furi_delay_ms(200);
    notification_message(app->notifications, &sequence_reset_blue);

    bt_keys_storage_set_default_path(app->hid->bt);

    furi_check(bt_profile_restore_default(app->hid->bt));
    furi_record_close(RECORD_BT);
    app->hid->bt = NULL;

    if(app->thread != NULL) {
        furi_thread_flags_set(app->thread, WorkerEvtStartStop);
        furi_thread_free(app->thread);
        app->thread = NULL;
    }

    widget_reset(app->widget);
}
