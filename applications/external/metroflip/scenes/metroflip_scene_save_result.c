#include "../metroflip_i.h"
#include "../api/metroflip/metroflip_api.h"
#include <stdio.h>
enum PopupEvent {
    PopupEventExit,
};

static void metroflip_scene_save_result_popup_callback(void* context) {
    Metroflip* app = context;

    view_dispatcher_send_custom_event(app->view_dispatcher, PopupEventExit);
}

void metroflip_scene_save_result_on_enter(void* context) {
    Metroflip* app = context;
    Popup* popup = app->popup;

    char path[280];
    snprintf(path, sizeof(path), "/ext/apps_data/metroflip/%s.nfc", app->save_buf);
    FURI_LOG_I("path", "path: %s", path);
    bool success;
    if(strcmp(app->card_type, "calypso") == 0) {
        Storage* storage = furi_record_open(RECORD_STORAGE);
        FlipperFormat* ff = flipper_format_file_alloc(storage);
        flipper_format_file_open_new(ff, path);
        success = flipper_format_write_string_cstr(
            ff,
            "Filetype: Flipper Metroflip File\n Version",
            furi_string_get_cstr(app->calypso_file_data));
        flipper_format_file_close(ff);
        flipper_format_free(ff);
        furi_record_close(RECORD_STORAGE);
        furi_string_reset(app->calypso_file_data);
    } else {
        success = nfc_device_save(app->nfc_device, path);
        Storage* storage = furi_record_open(RECORD_STORAGE);
        FlipperFormat* ff = flipper_format_file_alloc(storage);
        flipper_format_write_empty_line(ff);
        flipper_format_file_open_existing(ff, path);
        flipper_format_file_close(ff);
        flipper_format_free(ff);
        furi_record_close(RECORD_STORAGE);
    }

    if(success) {
        popup_set_icon(popup, 36, 5, &I_DolphinDone_80x58);
        popup_set_header(popup, "Saved!", 13, 22, AlignLeft, AlignBottom);
        popup_enable_timeout(popup);
    } else {
        popup_set_icon(popup, 69, 15, &I_WarningDolphinFlip_45x42);
        popup_set_header(popup, "Error!", 13, 22, AlignLeft, AlignBottom);
        popup_disable_timeout(popup);
    }
    popup_set_timeout(popup, 1500);
    popup_set_context(popup, app);
    popup_set_callback(popup, metroflip_scene_save_result_popup_callback);

    view_dispatcher_switch_to_view(app->view_dispatcher, MetroflipViewPopup);
}

bool metroflip_scene_save_result_on_event(void* context, SceneManagerEvent event) {
    Metroflip* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        consumed = true;
        switch(event.event) {
        case PopupEventExit:
            scene_manager_search_and_switch_to_previous_scene(
                app->scene_manager, MetroflipSceneStart);
            break;
        default:
            break;
        }
    }

    return consumed;
}

void metroflip_scene_save_result_on_exit(void* context) {
    Metroflip* app = context;
    popup_reset(app->popup);
}
