#include "../nfc_playlist.h"

static void nfc_playlist_nfc_duplicate_scene_menu_callback(
    GuiButtonType result,
    InputType type,
    void* context) {
    furi_assert(context);
    NfcPlaylist* nfc_playlist = context;
    if(type == InputTypeShort) {
        view_dispatcher_send_custom_event(nfc_playlist->view_dispatcher, result);
    }
}

void nfc_playlist_nfc_duplicate_scene_on_enter(void* context) {
    furi_assert(context);
    NfcPlaylist* nfc_playlist = context;

    widget_add_text_box_element(
        nfc_playlist->views.widget,
        0,
        0,
        128,
        23,
        AlignCenter,
        AlignCenter,
        "\e#This item is already\nin the playlist\e#",
        false);
    widget_add_button_element(
        nfc_playlist->views.widget,
        GuiButtonTypeLeft,
        "Try Again",
        nfc_playlist_nfc_duplicate_scene_menu_callback,
        nfc_playlist);
    widget_add_button_element(
        nfc_playlist->views.widget,
        GuiButtonTypeRight,
        "Continue",
        nfc_playlist_nfc_duplicate_scene_menu_callback,
        nfc_playlist);

    view_dispatcher_switch_to_view(nfc_playlist->view_dispatcher, NfcPlaylistView_Widget);
}

bool nfc_playlist_nfc_duplicate_scene_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);
    NfcPlaylist* nfc_playlist = context;
    bool consumed = false;
    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case GuiButtonTypeRight: {
            Storage* storage = furi_record_open(RECORD_STORAGE);
            Stream* stream = file_stream_alloc(storage);

            if(file_stream_open(
                   stream,
                   furi_string_get_cstr(nfc_playlist->worker_info.settings->playlist_path),
                   FSAM_READ_WRITE,
                   FSOM_OPEN_EXISTING)) {
                FuriString* line = furi_string_alloc();
                FuriString* tmp_str = furi_string_alloc();

                while(stream_read_line(stream, line)) {
                    furi_string_cat(tmp_str, furi_string_get_cstr(line));
                }
                furi_string_free(line);

                if(!furi_string_empty(tmp_str)) {
                    furi_string_cat(tmp_str, "\n");
                }
                furi_string_cat(
                    tmp_str, furi_string_get_cstr(nfc_playlist->views.file_browser.output));
                stream_clean(stream);
                stream_write_string(stream, tmp_str);
                nfc_playlist->worker_info.settings->playlist_length++;
                furi_string_reset(nfc_playlist->views.file_browser.output);

                file_stream_close(stream);
                furi_string_free(tmp_str);
            }
            stream_free(stream);
            furi_record_close(RECORD_STORAGE);

            furi_string_reset(nfc_playlist->views.file_browser.output);
            scene_manager_search_and_switch_to_previous_scene(
                nfc_playlist->scene_manager, NfcPlaylistScene_PlaylistEdit);
            consumed = true;
            break;
        }
        case GuiButtonTypeLeft:
            furi_string_reset(nfc_playlist->views.file_browser.output);
            scene_manager_previous_scene(nfc_playlist->scene_manager);
            consumed = true;
            break;
        default:
            break;
        }
    }
    return consumed;
}

void nfc_playlist_nfc_duplicate_scene_on_exit(void* context) {
    furi_assert(context);
    NfcPlaylist* nfc_playlist = context;
    widget_reset(nfc_playlist->views.widget);
}
