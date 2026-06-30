#include "../nfc_playlist.h"

void nfc_playlist_view_playlist_content_scene_on_enter(void* context) {
   furi_assert(context);
   NfcPlaylist* nfc_playlist = context;

   Storage* storage = furi_record_open(RECORD_STORAGE);
   Stream* stream = file_stream_alloc(storage);

   if(nfc_playlist->worker_info.settings->playlist_length == 0) {
      widget_add_text_box_element(
         nfc_playlist->views.widget,
         4,
         4,
         128,
         64,
         AlignCenter,
         AlignCenter,
         "\ePlaylist is empty\n\n\n\nPress back\e",
         false);
   } else if(file_stream_open(
                stream,
                furi_string_get_cstr(nfc_playlist->worker_info.settings->playlist_path),
                FSAM_READ,
                FSOM_OPEN_EXISTING)) {
      FuriString* line = furi_string_alloc();
      FuriString* tmp_str = furi_string_alloc();

      while(stream_read_line(stream, line)) {
         furi_string_cat(tmp_str, furi_string_get_cstr(line));
      }

      furi_string_free(line);
      file_stream_close(stream);

      widget_add_text_scroll_element(
         nfc_playlist->views.widget, 4, 4, 124, 60, furi_string_get_cstr(tmp_str));
      widget_add_frame_element(nfc_playlist->views.widget, 0, 0, 128, 64, 0);

      furi_string_free(tmp_str);
   } else {
      widget_add_text_box_element(
         nfc_playlist->views.widget,
         0,
         0,
         128,
         64,
         AlignCenter,
         AlignCenter,
         "\eFailed to open playlist\n\n\n\nPress back\e",
         false);
   }

   stream_free(stream);
   furi_record_close(RECORD_STORAGE);

   view_dispatcher_switch_to_view(nfc_playlist->view_dispatcher, NfcPlaylistView_Widget);
}

bool nfc_playlist_view_playlist_content_scene_on_event(void* context, SceneManagerEvent event) {
   UNUSED(context);
   UNUSED(event);
   return false;
}

void nfc_playlist_view_playlist_content_scene_on_exit(void* context) {
   furi_assert(context);
   NfcPlaylist* nfc_playlist = context;
   widget_reset(nfc_playlist->views.widget);
}
