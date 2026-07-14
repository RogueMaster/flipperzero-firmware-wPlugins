#include "../nfc_playlist.h"

static void nfc_playlist_confirm_delete_scene_menu_callback(
    GuiButtonType result,
    InputType type,
    void* context) {
    furi_assert(context);
    NfcPlaylist* nfc_playlist = context;
    if(type == InputTypeShort) {
        view_dispatcher_send_custom_event(nfc_playlist->view_dispatcher, result);
    }
}

void nfc_playlist_confirm_delete_scene_on_enter(void* context) {
    furi_assert(context);
    NfcPlaylist* nfc_playlist = context;

    FuriString* file_name = furi_string_alloc();
    path_extract_filename_no_ext(
        furi_string_get_cstr(nfc_playlist->worker_info.settings->playlist_path), file_name);
    FuriString* temp_str =
        furi_string_alloc_printf("\e#Delete %s?\e#", furi_string_get_cstr(file_name));
    furi_string_free(file_name);

    widget_add_text_box_element(
        nfc_playlist->views.widget,
        0,
        0,
        128,
        23,
        AlignCenter,
        AlignCenter,
        furi_string_get_cstr(temp_str),
        false);
    widget_add_button_element(
        nfc_playlist->views.widget,
        GuiButtonTypeLeft,
        "Cancel",
        nfc_playlist_confirm_delete_scene_menu_callback,
        nfc_playlist);
    widget_add_button_element(
        nfc_playlist->views.widget,
        GuiButtonTypeRight,
        "Delete",
        nfc_playlist_confirm_delete_scene_menu_callback,
        nfc_playlist);

    furi_string_free(temp_str);

    view_dispatcher_switch_to_view(nfc_playlist->view_dispatcher, NfcPlaylistView_Widget);
}

bool nfc_playlist_confirm_delete_scene_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);
    NfcPlaylist* nfc_playlist = context;
    bool consumed = false;
    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case GuiButtonTypeRight: {
            Storage* storage = furi_record_open(RECORD_STORAGE);
            if(storage_simply_remove(
                   storage,
                   furi_string_get_cstr(nfc_playlist->worker_info.settings->playlist_path))) {
                furi_string_reset(nfc_playlist->worker_info.settings->playlist_path);
            }
            furi_record_close(RECORD_STORAGE);
            scene_manager_search_and_switch_to_previous_scene(
                nfc_playlist->scene_manager, NfcPlaylistScene_MainMenu);
            consumed = true;
            break;
        }
        case GuiButtonTypeLeft:
            scene_manager_previous_scene(nfc_playlist->scene_manager);
            consumed = true;
            break;
        default:
            break;
        }
    }
    return consumed;
}

void nfc_playlist_confirm_delete_scene_on_exit(void* context) {
    furi_assert(context);
    NfcPlaylist* nfc_playlist = context;
    widget_reset(nfc_playlist->views.widget);
}
