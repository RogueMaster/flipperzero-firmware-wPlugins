#include "../nfc_playlist.h"

static void nfc_playlist_nfc_add_scene_menu_callback(void* context) {
    furi_assert(context);
    NfcPlaylist* nfc_playlist = context;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    Stream* stream = file_stream_alloc(storage);
    bool item_already_in_playlist = false;

    if(file_stream_open(
           stream,
           furi_string_get_cstr(nfc_playlist->worker_info.settings->playlist_path),
           FSAM_READ_WRITE,
           FSOM_OPEN_EXISTING)) {
        FuriString* line = furi_string_alloc();
        FuriString* tmp_str = furi_string_alloc();
        while(stream_read_line(stream, line)) {
            if(furi_string_cmp(line, nfc_playlist->views.file_browser.output) == 0) {
                item_already_in_playlist = true;
                break;
            }
            furi_string_cat(tmp_str, furi_string_get_cstr(line));
        }
        furi_string_free(line);
        if(!item_already_in_playlist) {
            if(!furi_string_empty(tmp_str)) {
                furi_string_cat(tmp_str, "\n");
            }
            furi_string_cat(
                tmp_str, furi_string_get_cstr(nfc_playlist->views.file_browser.output));
            stream_clean(stream);
            stream_write_string(stream, tmp_str);
            nfc_playlist->worker_info.settings->playlist_length++;
            furi_string_reset(nfc_playlist->views.file_browser.output);
        }
        file_stream_close(stream);
        furi_string_free(tmp_str);
    }

    stream_free(stream);
    furi_record_close(RECORD_STORAGE);
    if(!item_already_in_playlist) {
        scene_manager_previous_scene(nfc_playlist->scene_manager);
    } else {
        scene_manager_next_scene(nfc_playlist->scene_manager, NfcPlaylistScene_NfcDuplicate);
    }
}

void nfc_playlist_nfc_add_scene_on_enter(void* context) {
    furi_assert(context);
    NfcPlaylist* nfc_playlist = context;
    file_browser_configure(
        nfc_playlist->views.file_browser.view,
        ".nfc",
        NFC_ITEM_LOCATION,
        true,
        true,
        &I_Nfc_10px,
        true);
    file_browser_set_callback(
        nfc_playlist->views.file_browser.view,
        nfc_playlist_nfc_add_scene_menu_callback,
        nfc_playlist);
    FuriString* tmp_str = furi_string_alloc_set_str(NFC_ITEM_LOCATION);
    file_browser_start(nfc_playlist->views.file_browser.view, tmp_str);
    furi_string_free(tmp_str);

    view_dispatcher_switch_to_view(nfc_playlist->view_dispatcher, NfcPlaylistView_FileBrowser);
}

bool nfc_playlist_nfc_add_scene_on_event(void* context, SceneManagerEvent event) {
    UNUSED(event);
    UNUSED(context);
    return false;
}

void nfc_playlist_nfc_add_scene_on_exit(void* context) {
    furi_assert(context);
    NfcPlaylist* nfc_playlist = context;
    file_browser_stop(nfc_playlist->views.file_browser.view);
}
