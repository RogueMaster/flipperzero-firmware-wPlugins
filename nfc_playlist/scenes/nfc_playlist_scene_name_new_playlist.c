#include "../nfc_playlist.h"

void nfc_playlist_name_new_playlist_scene_menu_callback(void* context) {
   NfcPlaylist* nfc_playlist = context;

   FuriString* file_name = furi_string_alloc_printf(
      "%s%s.txt", PLAYLIST_LOCATION, nfc_playlist->views.text_input.output);
   char const* file_name_cstr = furi_string_get_cstr(file_name);

   Storage* storage = furi_record_open(RECORD_STORAGE);
   File* file = storage_file_alloc(storage);

   bool playlist_exist_already = false;
   if(!storage_file_exists(storage, file_name_cstr)) {
      if(storage_file_open(file, file_name_cstr, FSAM_READ_WRITE, FSOM_CREATE_NEW)) {
         storage_file_close(file);
         furi_string_swap(nfc_playlist->worker_info.settings->playlist_path, file_name);
         nfc_playlist->worker_info.settings->playlist_length = 0;
      }
   } else {
      playlist_exist_already = true;
   }

   furi_string_free(file_name);
   storage_file_free(file);
   furi_record_close(RECORD_STORAGE);

   view_dispatcher_send_custom_event(nfc_playlist->view_dispatcher, playlist_exist_already);
}

void nfc_playlist_name_new_playlist_scene_on_enter(void* context) {
   NfcPlaylist* nfc_playlist = context;

   nfc_playlist->views.text_input.output = malloc(MAX_PLAYLIST_NAME_LEN + 1);
   text_input_set_header_text(nfc_playlist->views.text_input.view, "Enter file name");
   text_input_set_minimum_length(nfc_playlist->views.text_input.view, 1);
   text_input_set_result_callback(
      nfc_playlist->views.text_input.view,
      nfc_playlist_name_new_playlist_scene_menu_callback,
      nfc_playlist,
      nfc_playlist->views.text_input.output,
      MAX_PLAYLIST_NAME_LEN,
      true);

   view_dispatcher_switch_to_view(nfc_playlist->view_dispatcher, NfcPlaylistView_TextInput);
}

bool nfc_playlist_name_new_playlist_scene_on_event(void* context, SceneManagerEvent event) {
   NfcPlaylist* nfc_playlist = context;

   if(event.type == SceneManagerEventTypeCustom) {
      if(event.event) {
         scene_manager_next_scene(
            nfc_playlist->scene_manager, NfcPlaylistScene_ErrorPlaylistAlreadyExists);
      } else {
         scene_manager_search_and_switch_to_previous_scene(
            nfc_playlist->scene_manager, NfcPlaylistScene_MainMenu);
      }
      return true;
   }
   return false;
}

void nfc_playlist_name_new_playlist_scene_on_exit(void* context) {
   NfcPlaylist* nfc_playlist = context;
   free(nfc_playlist->views.text_input.output);
   text_input_reset(nfc_playlist->views.text_input.view);
}
