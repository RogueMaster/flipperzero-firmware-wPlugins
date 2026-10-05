#include "../../protopirate_app_i.h"
#include "../../helpers/protopirate_storage.h"
#include "../../protocols/protocols_common.h"
#include "../../protocols/protocol_items.h"
#include "pp_saved_icons.h"

static const ProtoPirateSharedPluginHostApi* g_saved_info_scene_host_api = NULL;

#define TAG "PPSavedInfoPlugin"

#define STATE_EMULATE 0
#define STATE_BF      1

static void plugin_protopirate_scene_saved_info_widget_callback(
    GuiButtonType result,
    InputType type,
    void* context) {
    ProtoPirateApp* app = context;

    if((result == GuiButtonTypeLeft) && (type == InputTypeShort)) {
        const uint32_t left_event =
            (scene_manager_get_scene_state(app->scene_manager, ProtoPirateSceneSavedInfo) ==
             STATE_BF) ?
                ProtoPirateCustomEventBruteforceStart :
                ProtoPirateCustomEventSavedInfoEmulate;
        view_dispatcher_send_custom_event(app->view_dispatcher, left_event);
    } else if(result == GuiButtonTypeRight && (type == InputTypeShort)) {
        //Send delete event and get user confirmation to delete.
        view_dispatcher_send_custom_event(
            app->view_dispatcher, ProtoPirateCustomEventSavedInfoDelete);
    }
}

void plugin_protopirate_scene_saved_info_on_enter(ProtoPirateApp* app) {
    Storage* storage = NULL;
    FlipperFormat* ff = NULL;
    FuriString* info_str = NULL;
    FuriString* temp_str = NULL;
    bool success = false;

    FURI_LOG_I(TAG, "=== ENTER START ===");

    //protopirate_release_shared_radio_state(app);

    if(!g_saved_info_scene_host_api->ensure_widget(app)) {
        notification_message(app->notifications, &sequence_error);
        if(!scene_manager_has_previous_scene(app->scene_manager, ProtoPirateSceneStart)) {
            view_dispatcher_send_custom_event(
                app->view_dispatcher, ProtoPirateCustomEventPluginNavigateStopApp);
        } else
            view_dispatcher_send_custom_event(
                app->view_dispatcher, ProtoPirateCustomEventPluginNavigateBack);
        return;
    }

    // Reset widget first
    widget_reset(app->widget);

    // Validate file path
    if(!app->loaded_file_path || (strlen(app->loaded_file_path) == 0)) {
        FURI_LOG_E(TAG, "No file path");
        widget_add_string_element(
            app->widget, 64, 32, AlignCenter, AlignCenter, FontPrimary, "No file selected");
        goto switch_view;
    }

    FURI_LOG_I(TAG, "Path: %s", app->loaded_file_path);

    // Allocate strings first (no I/O)
    info_str = furi_string_alloc();
    temp_str = furi_string_alloc();
    if(!info_str || !temp_str) {
        FURI_LOG_E(TAG, "String alloc failed");
        widget_add_string_element(
            app->widget, 64, 32, AlignCenter, AlignCenter, FontPrimary, "Memory error");
        goto cleanup;
    }

    FURI_LOG_I(TAG, "Strings allocated");

    // Open storage
    FURI_LOG_I(TAG, "Opening storage...");
    storage = furi_record_open(RECORD_STORAGE);
    if(!storage) {
        FURI_LOG_E(TAG, "Storage open failed");
        widget_add_string_element(
            app->widget, 64, 32, AlignCenter, AlignCenter, FontPrimary, "Storage error");
        goto cleanup;
    }

    FURI_LOG_I(TAG, "Storage opened");

    // Allocate flipper format
    FURI_LOG_I(TAG, "Allocating FF...");
    ff = flipper_format_file_alloc(storage);
    if(!ff) {
        FURI_LOG_E(TAG, "FF alloc failed");
        widget_add_string_element(
            app->widget, 64, 32, AlignCenter, AlignCenter, FontPrimary, "Memory error");
        goto cleanup;
    }

    FURI_LOG_I(TAG, "FF allocated");

    // Open file
    FURI_LOG_I(TAG, "Opening file...");
    if(!flipper_format_file_open_existing(ff, app->loaded_file_path)) {
        FURI_LOG_E(TAG, "File open failed");
        widget_add_string_element(
            app->widget, 64, 32, AlignCenter, AlignCenter, FontPrimary, "File open failed");
        goto cleanup;
    }

    FURI_LOG_I(TAG, "File opened, reading...");
    bool offers_bf = false;

    // Read fields
    uint32_t temp_data = 0;
    app->emulate_disabled_for_loaded = true;

    flipper_format_rewind(ff);
    if(flipper_format_read_string(ff, FF_PROTOCOL, temp_str)) {
        const char* protocol_name = furi_string_get_cstr(temp_str);
        furi_string_cat_printf(info_str, "Protocol: %s\n", protocol_name);

        //Can we emulate this type?
        app->emulate_disabled_for_loaded =
            !g_saved_info_scene_host_api->protocol_catalog_can_tx(protocol_name);

        //Do we need to offer a Brute Force Option?
        offers_bf = g_saved_info_scene_host_api->protocol_catalog_offers_bruteforce(protocol_name);
    }

    flipper_format_rewind(ff);
    if(flipper_format_read_uint32(ff, FF_FREQUENCY, &temp_data, 1)) {
        furi_string_cat_printf(
            info_str, "Freq: %lu.%02lu MHz\n", temp_data / 1000000, (temp_data % 1000000) / 10000);
    }

    flipper_format_rewind(ff);
    if(flipper_format_read_string(ff, FF_PRESET, temp_str)) {
        // Convert full preset name to short name
        const char* preset_name = pp_get_short_preset_name(furi_string_get_cstr(temp_str));
        furi_string_cat_printf(info_str, "Modulation: %s\n", preset_name);
    }

    flipper_format_rewind(ff);
    if(flipper_format_read_uint32(ff, FF_SERIAL, &temp_data, 1)) {
        furi_string_cat_printf(info_str, "Serial: %08lX\n", temp_data);
    }

    flipper_format_rewind(ff);
    if(flipper_format_read_uint32(ff, FF_BTN, &temp_data, 1)) {
        furi_string_cat_printf(info_str, "Button: %02X\n", (uint8_t)temp_data);
    }

    flipper_format_rewind(ff);
    if(flipper_format_read_uint32(ff, FF_CNT, &temp_data, 1)) {
        furi_string_cat_printf(info_str, "Counter: %04lX\n", temp_data);
    }

    flipper_format_rewind(ff);
    if(flipper_format_read_uint32(ff, "Checksum", &temp_data, 1)) {
        furi_string_cat_printf(info_str, "Checksum: %02X\n", (uint8_t)temp_data);
    }

    flipper_format_rewind(ff);
    if(flipper_format_read_uint32(ff, "CRC", &temp_data, 1)) {
        furi_string_cat_printf(info_str, "CRC: %02X\n", (uint8_t)temp_data);
    }

    flipper_format_rewind(ff);
    if(flipper_format_read_uint32(ff, FF_TYPE, &temp_data, 1)) {
        furi_string_cat_printf(info_str, "Type: %02X\n", (uint8_t)temp_data);
    }

    flipper_format_rewind(ff);
    if(flipper_format_read_uint32(ff, "KeyIdx", &temp_data, 1)) {
        furi_string_cat_printf(info_str, "KeyIdx: %d\n", (uint8_t)temp_data);
    }

    flipper_format_rewind(ff);
    if(flipper_format_read_string(ff, FF_KEY, temp_str)) {
        furi_string_cat_printf(info_str, "Key1: %s\n", furi_string_get_cstr(temp_str));
    }

    flipper_format_rewind(ff);
    if(flipper_format_read_string(ff, "Key_2", temp_str)) {
        furi_string_cat_printf(info_str, "Key2: %s\n", furi_string_get_cstr(temp_str));
    }

    flipper_format_rewind(ff);
    if(flipper_format_read_uint32(ff, "ValidationField", &temp_data, 1)) {
        furi_string_cat_printf(info_str, "ValField: %04X\n", (uint16_t)temp_data);
    }

    flipper_format_rewind(ff);
    if(flipper_format_read_uint32(ff, "Seed", &temp_data, 1)) {
        furi_string_cat_printf(info_str, "Seed: %02X\n", (uint8_t)temp_data);
    }

    flipper_format_rewind(ff);
    if(flipper_format_read_string(ff, FF_MANUFACTURE, temp_str)) {
        furi_string_cat_printf(info_str, "Manufacture: %s\n", furi_string_get_cstr(temp_str));
    }

    FURI_LOG_I(TAG, "Read complete, len=%u", furi_string_size(info_str));
    success = true;

cleanup:
    // Now do widget operations
    if(success && info_str && furi_string_size(info_str) > 0) {
        FURI_LOG_I(TAG, "Adding scroll element");
        widget_add_text_scroll_element(app->widget, 0, 0, 128, 50, furi_string_get_cstr(info_str));

        bool needs_bf = false;
        bool error = false;
        if(offers_bf && g_saved_info_scene_host_api->bruteforce_plugin_ensure_loaded(app) &&
           app->running_bruteforce_plugin.bruteforce_plugin) {
            needs_bf =
                app->running_bruteforce_plugin.bruteforce_plugin->widget_left_should_bruteforce(
                    app, ff);
        } else if(offers_bf) {
            //Show the user the error in the button.
            widget_add_button_element(app->widget, GuiButtonTypeLeft, "(Error)", NULL, app);
            needs_bf = false;
            error = true;
        }

        g_saved_info_scene_host_api->bruteforce_plugin_unload_if_idle(app);
        if(needs_bf) {
            scene_manager_set_scene_state(app->scene_manager, ProtoPirateSceneSavedInfo, STATE_BF);
            //Add BF button.
            widget_add_button_element(
                app->widget,
                GuiButtonTypeLeft,
                "BF",
                plugin_protopirate_scene_saved_info_widget_callback,
                app);
        } else if(!error) {
            scene_manager_set_scene_state(
                app->scene_manager, ProtoPirateSceneSavedInfo, STATE_EMULATE);
#ifdef ENABLE_EMULATE_FEATURE
            if(app->emulate_feature_enabled && !app->emulate_disabled_for_loaded) {
                //Add Emulate Button.
                widget_add_button_element(
                    app->widget,
                    GuiButtonTypeLeft,
                    "Emulate",
                    plugin_protopirate_scene_saved_info_widget_callback,
                    app);
            }
#endif
        }

        //Add delete button
        widget_add_button_element(
            app->widget,
            GuiButtonTypeRight,
            "Delete",
            plugin_protopirate_scene_saved_info_widget_callback,
            app);
    }

    // Close file and storage BEFORE widget operations
    if(ff) {
        flipper_format_free(ff);
        ff = NULL;
    }
    if(storage) {
        furi_record_close(RECORD_STORAGE);
        storage = NULL;
    }

    FURI_LOG_I(TAG, "Storage closed");

    // Free strings
    if(temp_str) furi_string_free(temp_str);
    if(info_str) furi_string_free(info_str);

    FURI_LOG_I(TAG, "Cleanup done");

switch_view:
    FURI_LOG_I(TAG, "Switching to widget");
    view_dispatcher_switch_to_view(app->view_dispatcher, ProtoPirateViewWidget);
    FURI_LOG_I(TAG, "=== ENTER DONE ===");
}

bool plugin_protopirate_scene_saved_info_on_event(ProtoPirateApp* context, SceneManagerEvent event) {
    ProtoPirateApp* app = context;
    bool consumed = false;

    //load_emu* = false;
    if(event.type == SceneManagerEventTypeTick) {
        if(app->running_bruteforce_plugin.bruteforce_plugin &&
           app->running_bruteforce_plugin.bruteforce_plugin->is_running(app)) {
            app->running_bruteforce_plugin.bruteforce_plugin->on_scene_event(
                app, ProtoPirateBruteForceContextSavedInfo, event);
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        if(app->running_bruteforce_plugin.bruteforce_plugin &&
           app->running_bruteforce_plugin.bruteforce_plugin->is_running &&
           app->running_bruteforce_plugin.bruteforce_plugin->on_scene_event(
               app, ProtoPirateBruteForceContextReceiverInfo, event)) {
            consumed = true;
        } else {
            if(!scene_manager_has_previous_scene(app->scene_manager, ProtoPirateSceneStart)) {
                view_dispatcher_send_custom_event(
                    app->view_dispatcher, ProtoPirateCustomEventPluginNavigateStopApp);
                consumed = true;
            }
        }
    } else if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == ProtoPirateCustomEventSavedInfoDelete) {
            FURI_LOG_I(TAG, "Delete requested");
            if(app->loaded_file_path && (strlen(app->loaded_file_path) > 0)) {
                DialogMessage* message = dialog_message_alloc();
                dialog_message_set_buttons(message, "Delete", NULL, "Keep");
                dialog_message_set_icon(message, &I_WarningDolphin_45x42, 0, 12);
                dialog_message_set_header(
                    message, "Confirm Delete Action", 64, 0, AlignCenter, AlignTop);
                dialog_message_set_text(
                    message,
                    "Are you sure you\nwant to delete\nthis file?",
                    50,
                    14,
                    AlignLeft,
                    AlignTop);
                DialogMessageButton dialog_result = dialog_message_show(app->dialogs, message);
                dialog_message_free(message);

                //Delete if the user said yes.
                if(dialog_result == DialogMessageButtonLeft) {
                    g_saved_info_scene_host_api->storage_delete_file(app->loaded_file_path);
                    notification_message(app->notifications, &sequence_semi_success);

                    if(!scene_manager_has_previous_scene(
                           app->scene_manager, ProtoPirateSceneStart)) {
                        view_dispatcher_send_custom_event(
                            app->view_dispatcher, ProtoPirateCustomEventPluginNavigateStopApp);
                    } else
                        view_dispatcher_send_custom_event(
                            app->view_dispatcher, ProtoPirateCustomEventPluginNavigateBack);
                }
            }
            consumed = true;
        }
        if(event.event == ProtoPirateCustomEventBruteforceStart ||
           event.event == ProtoPirateCustomEventBruteforceComplete) {
            if(g_saved_info_scene_host_api->bruteforce_plugin_ensure_loaded(app) &&
               app->running_bruteforce_plugin.bruteforce_plugin &&
               app->running_bruteforce_plugin.bruteforce_plugin->on_scene_event(
                   app, ProtoPirateBruteForceContextSavedInfo, event)) {
            }
            if(event.event == ProtoPirateCustomEventBruteforceComplete)
                plugin_protopirate_scene_saved_info_on_enter(app);
            consumed = true;
        }

#ifdef ENABLE_EMULATE_FEATURE
        if(event.event == ProtoPirateCustomEventSavedInfoEmulate && app->emulate_feature_enabled &&
           !app->emulate_disabled_for_loaded) {
            FURI_LOG_I(TAG, "Emulate requested");

            //Send custom event back to the scene, so it can start emulate for us and avoid the crashes.
            view_dispatcher_send_custom_event(
                app->view_dispatcher, ProtoPirateCustomEventPluginNavigateEmulate);

            consumed = true;
        }
#endif
    }

    return consumed;
}

void saved_info_plugin_set_host_api(const ProtoPirateSharedPluginHostApi* host_api) {
    g_saved_info_scene_host_api = host_api;
}

static const ProtoPirateSharedPlugin protopirate_saved_info_plugin = {
    .plugin_name = "",
    .on_enter = plugin_protopirate_scene_saved_info_on_enter,
    .on_event = plugin_protopirate_scene_saved_info_on_event,
    .set_host_api = saved_info_plugin_set_host_api,
    .release = NULL,
};

static const FlipperAppPluginDescriptor protopirate_saved_info_plugin_descriptor = {
    .appid = PROTOPIRATE_SAVED_INFO_PLUGIN_APP_ID,
    .ep_api_version = PROTOPIRATE_SAVED_INFO_PLUGIN_API_VERSION,
    .entry_point = &protopirate_saved_info_plugin,
};

const FlipperAppPluginDescriptor* protopirate_saved_info_plugin_ep(void) {
    return &protopirate_saved_info_plugin_descriptor;
}
