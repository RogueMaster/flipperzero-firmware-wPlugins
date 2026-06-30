#include "../nfc_playlist.h"

static void nfc_playlist_playlist_rename_scene_menu_callback(void* context) {
   furi_assert(context);
   NfcPlaylist* nfc_playlist = context;

   FuriString* old_file_path = furi_string_alloc();
   path_extract_dirname(
      furi_string_get_cstr(nfc_playlist->worker_info.settings->playlist_path), old_file_path);
   FuriString* new_file_path = furi_string_alloc_set(old_file_path);
   path_concat(
      furi_string_get_cstr(old_file_path), nfc_playlist->views.text_input.output, new_file_path);
   furi_string_free(old_file_path);
   furi_string_cat_str(new_file_path, ".txt");

   Storage* storage = furi_record_open(RECORD_STORAGE);

   bool playlist_exist_already = false;
   if(!storage_file_exists(storage, furi_string_get_cstr(new_file_path))) {
      if(storage_common_rename(
            storage,
            furi_string_get_cstr(nfc_playlist->worker_info.settings->playlist_path),
            furi_string_get_cstr(new_file_path)) == FSE_OK) {
         furi_string_swap(nfc_playlist->worker_info.settings->playlist_path, new_file_path);
      }
   } else {
      if(furi_string_cmp(nfc_playlist->worker_info.settings->playlist_path, new_file_path) != 0) {
         playlist_exist_already = true;
      }
   }

   furi_string_free(new_file_path);
   furi_record_close(RECORD_STORAGE);

   view_dispatcher_send_custom_event(nfc_playlist->view_dispatcher, playlist_exist_already);
}

void nfc_playlist_playlist_rename_scene_on_enter(void* context) {
   NfcPlaylist* nfc_playlist = context;

   FuriString* tmp_file_name = furi_string_alloc();
   path_extract_filename_no_ext(
      furi_string_get_cstr(nfc_playlist->worker_info.settings->playlist_path), tmp_file_name);

   nfc_playlist->views.text_input.output = malloc(MAX_PLAYLIST_NAME_LEN + 1);
   strcpy(nfc_playlist->views.text_input.output, furi_string_get_cstr(tmp_file_name));
   furi_string_free(tmp_file_name);

   text_input_set_header_text(nfc_playlist->views.text_input.view, "Enter new file name");
   text_input_set_minimum_length(nfc_playlist->views.text_input.view, 1);
   text_input_set_result_callback(
      nfc_playlist->views.text_input.view,
      nfc_playlist_playlist_rename_scene_menu_callback,
      nfc_playlist,
      nfc_playlist->views.text_input.output,
      MAX_PLAYLIST_NAME_LEN,
      false);

   view_dispatcher_switch_to_view(nfc_playlist->view_dispatcher, NfcPlaylistView_TextInput);
}

bool nfc_playlist_playlist_rename_scene_on_event(void* context, SceneManagerEvent event) {
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

void nfc_playlist_playlist_rename_scene_on_exit(void* context) {
   NfcPlaylist* nfc_playlist = context;
   free(nfc_playlist->views.text_input.output);
   text_input_reset(nfc_playlist->views.text_input.view);
}
