#include "../weebo_i.h"
#include <nfc/protocols/mf_ultralight/mf_ultralight_listener.h>
#include <lib/toolbox/path.h>

#define TAG "SceneEmulate"

static void restart_listener_with_current_data(Weebo* weebo) {
    const MfUltralightData* data = nfc_device_get_data(weebo->nfc_device, NfcProtocolMfUltralight);
    weebo->listener = nfc_listener_alloc(weebo->nfc, NfcProtocolMfUltralight, data);
    nfc_listener_start(weebo->listener, NULL, NULL);
}

void weebo_scene_emulate_widget_callback(GuiButtonType result, InputType type, void* context) {
    furi_assert(context);
    Weebo* weebo = context;

    if(type == InputTypeShort) {
        view_dispatcher_send_custom_event(weebo->view_dispatcher, result);
    }
}

void weebo_scene_emulate_draw_screen(Weebo* weebo) {
    Widget* widget = weebo->widget;
    FuriString* info_str = furi_string_alloc();
    FuriString* uid_str = furi_string_alloc();
    FuriString* file_str = furi_string_alloc();
    const MfUltralightData* data = nfc_device_get_data(weebo->nfc_device, NfcProtocolMfUltralight);

    furi_string_cat_printf(info_str, "Emulating");
    furi_string_cat_printf(
        uid_str,
        "%02X%02X%02X%02X%02X%02X%02X",
        data->iso14443_3a_data->uid[0],
        data->iso14443_3a_data->uid[1],
        data->iso14443_3a_data->uid[2],
        data->iso14443_3a_data->uid[3],
        data->iso14443_3a_data->uid[4],
        data->iso14443_3a_data->uid[5],
        data->iso14443_3a_data->uid[6]);

    // Show file info with counter
    size_t file_count = weebo_get_file_count(weebo);
    if(file_count > 0) {
        furi_string_printf(
            file_str,
            "%s (%zu/%zu)",
            weebo->file_name,
            weebo_get_current_file_index(weebo) + 1,
            file_count);
    } else {
        furi_string_cat_printf(file_str, "%s", weebo->file_name);
    }

    widget_reset(widget);
    widget_add_string_element(
        widget, 64, 5, AlignCenter, AlignCenter, FontSecondary, furi_string_get_cstr(info_str));
    widget_add_string_element(
        widget, 64, 17, AlignCenter, AlignCenter, FontSecondary, furi_string_get_cstr(file_str));
    widget_add_string_element(
        widget, 64, 29, AlignCenter, AlignCenter, FontSecondary, furi_string_get_cstr(uid_str));

    widget_add_button_element(
        widget, GuiButtonTypeCenter, "Remix", weebo_scene_emulate_widget_callback, weebo);

    // Only show prev/next buttons if there are multiple files
    if(file_count > 1) {
        widget_add_button_element(
            widget, GuiButtonTypeLeft, "Prev", weebo_scene_emulate_widget_callback, weebo);
        widget_add_button_element(
            widget, GuiButtonTypeRight, "Next", weebo_scene_emulate_widget_callback, weebo);
    }

    furi_string_free(info_str);
    furi_string_free(uid_str);
    furi_string_free(file_str);

    view_dispatcher_switch_to_view(weebo->view_dispatcher, WeeboViewWidget);
}

void weebo_scene_emulate_on_enter(void* context) {
    Weebo* weebo = context;

    // Get directory from the current loaded file path and scan for .nfc files
    FuriString* directory = furi_string_alloc();
    path_extract_dirname(furi_string_get_cstr(weebo->load_path), directory);
    weebo_scan_nfc_files(weebo, furi_string_get_cstr(directory));

    // Find current file index in the scanned list
    weebo_set_current_file_path(weebo, furi_string_get_cstr(weebo->load_path));

    furi_string_free(directory);

    nfc_device_load(weebo->nfc_device, furi_string_get_cstr(weebo->load_path));
    const MfUltralightData* data = nfc_device_get_data(weebo->nfc_device, NfcProtocolMfUltralight);
    weebo->listener = nfc_listener_alloc(weebo->nfc, NfcProtocolMfUltralight, data);
    nfc_listener_start(weebo->listener, NULL, NULL);

    weebo_scene_emulate_draw_screen(weebo);
    weebo_blink_start(weebo);
}

bool weebo_scene_emulate_on_event(void* context, SceneManagerEvent event) {
    Weebo* weebo = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(weebo->scene_manager, WeeboSceneEmulate, event.event);

        if(event.event == GuiButtonTypeCenter) {
            //stop listener
            FURI_LOG_D(TAG, "Stopping listener");
            nfc_listener_stop(weebo->listener);
            nfc_listener_free(weebo->listener);
            weebo->listener = NULL;

            weebo_remix(weebo);
            //start listener
            FURI_LOG_D(TAG, "Starting listener");
            restart_listener_with_current_data(weebo);

            weebo_scene_emulate_draw_screen(weebo);
            consumed = true;

        } else if(event.event == GuiButtonTypeLeft) {
            // Previous file
            if(weebo_get_file_count(weebo) > 1) {
                //stop listener
                FURI_LOG_D(TAG, "Stopping listener for prev file");
                nfc_listener_stop(weebo->listener);
                nfc_listener_free(weebo->listener);
                weebo->listener = NULL;

                if(weebo_cycle_file(weebo, WeeboCycleDirectionPrev)) {
                    FURI_LOG_D(TAG, "Starting listener with prev file");
                    restart_listener_with_current_data(weebo);
                    weebo_scene_emulate_draw_screen(weebo);
                } else {
                    // If cycling fails (no valid files), stop trying
                    FURI_LOG_W(TAG, "Cannot cycle to prev file - no valid files found");
                }
            }
            consumed = true;

        } else if(event.event == GuiButtonTypeRight) {
            // Next file
            if(weebo_get_file_count(weebo) > 1) {
                //stop listener
                FURI_LOG_D(TAG, "Stopping listener for next file");
                nfc_listener_stop(weebo->listener);
                nfc_listener_free(weebo->listener);
                weebo->listener = NULL;

                if(weebo_cycle_file(weebo, WeeboCycleDirectionNext)) {
                    FURI_LOG_D(TAG, "Starting listener with next file");
                    restart_listener_with_current_data(weebo);
                    weebo_scene_emulate_draw_screen(weebo);
                } else {
                    // If cycling fails (no valid files), stop trying
                    FURI_LOG_W(TAG, "Cannot cycle to next file - no valid files found");
                }
            }
            consumed = true;
        }
    }

    return consumed;
}

void weebo_scene_emulate_on_exit(void* context) {
    Weebo* weebo = context;

    if(weebo->listener) {
        nfc_listener_stop(weebo->listener);
        nfc_listener_free(weebo->listener);
        weebo->listener = NULL;
    }

    widget_reset(weebo->widget);
    weebo_blink_stop(weebo);
}
