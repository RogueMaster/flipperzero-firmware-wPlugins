// scenes/protopirate_scene_saved_info.c
#include "../protopirate_app_i.h"
#include "../helpers/protopirate_storage.h"
#include "proto_pirate_icons.h"

#define TAG "ProtoPirateSceneSavedInfo"

static bool is_emu_off = false;

static void protopirate_scene_saved_info_widget_callback(
    GuiButtonType result,
    InputType type,
    void* context) {
    ProtoPirateApp* app = context;

    if((result == GuiButtonTypeLeft) && (type == InputTypeShort)) {
#ifdef ENABLE_EMULATE_FEATURE
        if(!is_emu_off) {
            view_dispatcher_send_custom_event(
                app->view_dispatcher, ProtoPirateCustomEventSavedInfoEmulate);
        }
#endif
    } else if(result == GuiButtonTypeRight && (type == InputTypeShort)) {
        //Send delete event and get user confirmation to delete.
        view_dispatcher_send_custom_event(
            app->view_dispatcher, ProtoPirateCustomEventSavedInfoDelete);
    }
}

void protopirate_scene_saved_info_on_enter(void* context) {
    furi_check(context);
    ProtoPirateApp* app = context;
    Storage* storage = NULL;
    FlipperFormat* ff = NULL;
    FuriString* info_str = NULL;
    FuriString* temp_str = NULL;
    bool success = false;

    FURI_LOG_I(TAG, "=== ENTER START ===");

    protopirate_release_shared_radio_state(app);

    if(!protopirate_ensure_widget(app)) {
        notification_message(app->notifications, &sequence_error);
        scene_manager_previous_scene(app->scene_manager);
        return;
    }

    // Reset widget first
    widget_reset(app->widget);

    // Validate file path
    if(!app->loaded_file_path || furi_string_empty(app->loaded_file_path)) {
        FURI_LOG_E(TAG, "No file path");
        widget_add_string_element(
            app->widget, 64, 32, AlignCenter, AlignCenter, FontPrimary, "No file selected");
        goto switch_view;
    }

    FURI_LOG_I(TAG, "Path: %s", furi_string_get_cstr(app->loaded_file_path));

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
    if(!flipper_format_file_open_existing(ff, furi_string_get_cstr(app->loaded_file_path))) {
        FURI_LOG_E(TAG, "File open failed");
        widget_add_string_element(
            app->widget, 64, 32, AlignCenter, AlignCenter, FontPrimary, "File open failed");
        goto cleanup;
    }

    FURI_LOG_I(TAG, "File opened, reading...");

    // Read fields
    uint32_t temp_data = 0;
    // reset is_emu_off state before loading
    is_emu_off = false;

    flipper_format_rewind(ff);
    if(flipper_format_read_string(ff, "Protocol", temp_str)) {
        furi_string_cat_printf(info_str, "Protocol: %s\n", furi_string_get_cstr(temp_str));
    }
    if(furi_string_cmp_str(temp_str, "Scher-Khan") == 0) {
        is_emu_off = true;
    } else if(furi_string_cmp_str(temp_str, "Kia V5") == 0) {
        is_emu_off = true;
    }

    flipper_format_rewind(ff);
    if(flipper_format_read_uint32(ff, "Frequency", &temp_data, 1)) {
        furi_string_cat_printf(
            info_str, "Freq: %lu.%02lu MHz\n", temp_data / 1000000, (temp_data % 1000000) / 10000);
    }

    flipper_format_rewind(ff);
    if(flipper_format_read_string(ff, "Preset", temp_str)) {
        // Convert full preset name to short name
        const char* preset_name = preset_name_to_short(furi_string_get_cstr(temp_str));
        furi_string_cat_printf(info_str, "Modulation: %s\n", preset_name);
    }

    flipper_format_rewind(ff);
    if(flipper_format_read_uint32(ff, "Serial", &temp_data, 1)) {
        furi_string_cat_printf(info_str, "Serial: %08lX\n", temp_data);
    }

    flipper_format_rewind(ff);
    if(flipper_format_read_uint32(ff, "Btn", &temp_data, 1)) {
        furi_string_cat_printf(info_str, "Button: %02X\n", (uint8_t)temp_data);
    }

    flipper_format_rewind(ff);
    if(flipper_format_read_uint32(ff, "Cnt", &temp_data, 1)) {
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
    if(flipper_format_read_uint32(ff, "Type", &temp_data, 1)) {
        furi_string_cat_printf(info_str, "Type: %02X\n", (uint8_t)temp_data);
    }

    flipper_format_rewind(ff);
    if(flipper_format_read_uint32(ff, "KeyIdx", &temp_data, 1)) {
        furi_string_cat_printf(info_str, "KeyIdx: %d\n", (uint8_t)temp_data);
    }

    flipper_format_rewind(ff);
    if(flipper_format_read_string(ff, "Key", temp_str)) {
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
    if(flipper_format_read_string(ff, "Manufacture", temp_str)) {
        furi_string_cat_printf(info_str, "Manufacture: %s\n", furi_string_get_cstr(temp_str));
    }

    FURI_LOG_I(TAG, "Read complete, len=%u", furi_string_size(info_str));
    success = true;

cleanup:
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

    // Now do widget operations
    if(success && info_str && furi_string_size(info_str) > 0) {
        FURI_LOG_I(TAG, "Adding scroll element");
        widget_add_text_scroll_element(app->widget, 0, 0, 128, 50, furi_string_get_cstr(info_str));

#ifdef ENABLE_EMULATE_FEATURE
        if(!is_emu_off) {
            widget_add_button_element(
                app->widget,
                GuiButtonTypeLeft,
                "Emulate",
                protopirate_scene_saved_info_widget_callback,
                app);
        }
#endif
        widget_add_button_element(
            app->widget,
            GuiButtonTypeRight,
            "Delete",
            protopirate_scene_saved_info_widget_callback,
            app);
    }

    // Free strings
    if(temp_str) furi_string_free(temp_str);
    if(info_str) furi_string_free(info_str);

    FURI_LOG_I(TAG, "Cleanup done");

switch_view:
    FURI_LOG_I(TAG, "Switching to widget");
    view_dispatcher_switch_to_view(app->view_dispatcher, ProtoPirateViewWidget);
    FURI_LOG_I(TAG, "=== ENTER DONE ===");
}

bool protopirate_scene_saved_info_on_event(void* context, SceneManagerEvent event) {
    ProtoPirateApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == ProtoPirateCustomEventSavedInfoDelete) {
            FURI_LOG_I(TAG, "Delete requested");
            if(app->loaded_file_path && !furi_string_empty(app->loaded_file_path)) {
                //Show the "Are you Sure" dialog.
                app->dialogs = furi_record_open(RECORD_DIALOGS);
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
                furi_record_close(RECORD_DIALOGS);
                app->dialogs = NULL;

                //Delete if the user said yes.
                if(dialog_result == DialogMessageButtonLeft) {
                    notification_message(app->notifications, &sequence_semi_success);
                    protopirate_storage_delete_file(furi_string_get_cstr(app->loaded_file_path));
                    scene_manager_previous_scene(app->scene_manager);
                }
            }
            consumed = true;
        }
#ifdef ENABLE_EMULATE_FEATURE
        if(event.event == ProtoPirateCustomEventSavedInfoEmulate && !is_emu_off) {
            FURI_LOG_I(TAG, "Emulate requested");
            scene_manager_next_scene(app->scene_manager, ProtoPirateSceneEmulate);
            consumed = true;
        }
#endif
    }

    return consumed;
}

void protopirate_scene_saved_info_on_exit(void* context) {
    ProtoPirateApp* app = context;
    FURI_LOG_I(TAG, "Exiting SavedInfo scene");
    widget_reset(app->widget);
}
