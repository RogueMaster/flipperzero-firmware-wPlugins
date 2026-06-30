#include "nfc_playlist.h"

static bool nfc_playlist_custom_callback(void* context, uint32_t custom_event) {
   furi_assert(context);
   NfcPlaylist* nfc_playlist = context;
   return scene_manager_handle_custom_event(nfc_playlist->scene_manager, custom_event);
}

static bool nfc_playlist_back_event_callback(void* context) {
   furi_assert(context);
   NfcPlaylist* nfc_playlist = context;
   return scene_manager_handle_back_event(nfc_playlist->scene_manager);
}

static void nfc_playlist_tick_event_callback(void* context) {
   furi_assert(context);
   NfcPlaylist* nfc_playlist = context;
   scene_manager_handle_tick_event(nfc_playlist->scene_manager);
}

void nfc_playlist_load_settings(void* context) {
   furi_assert(context);
   NfcPlaylist* nfc_playlist = context;

   nfc_playlist->worker_info.settings->emulate_timeout = default_emulate_timeout;
   nfc_playlist->worker_info.settings->emulate_delay = default_emulate_delay;
   nfc_playlist->worker_info.settings->emulate_led_indicator = default_emulate_led_indicator;
   nfc_playlist->worker_info.settings->skip_error = default_skip_error;
   nfc_playlist->worker_info.settings->loop = default_loop;
   nfc_playlist->worker_info.settings->time_controls = default_time_controls;
   nfc_playlist->worker_info.settings->user_controls = default_user_controls;

   Storage* storage = furi_record_open(RECORD_STORAGE);

   if(storage_file_exists(storage, SETTINGS_LOCATION)) {
      Stream* stream = file_stream_alloc(storage);

      if(file_stream_open(stream, SETTINGS_LOCATION, FSAM_READ, FSOM_OPEN_EXISTING)) {
         FuriString* line = furi_string_alloc();
         FuriString* key = furi_string_alloc();
         FuriString* value = furi_string_alloc();

         while(stream_read_line(stream, line)) {
            if(furi_string_empty(line) || furi_string_start_with_str(line, "#")) continue;

            size_t equal_index = furi_string_search_char(line, '=');
            if(equal_index <= 0) continue;

            furi_string_set_strn(key, furi_string_get_cstr(line), equal_index);
            furi_string_set_str(value, furi_string_get_cstr(line) + equal_index + 1);

            furi_string_trim(key);
            furi_string_trim(value);

            const char* key_str = furi_string_get_cstr(key);
            const char* value_str = furi_string_get_cstr(value);

            if(strcmp(key_str, "emulate_timeout") == 0) {
               int emulate_timeout = atoi(value_str);
               if(emulate_timeout < 0 ||
                  emulate_timeout >=
                     (int)(sizeof(options_emulate_timeout) / sizeof(options_emulate_timeout[0])))
                  continue;
               nfc_playlist->worker_info.settings->emulate_timeout = emulate_timeout;
            } else if(strcmp(key_str, "emulate_delay") == 0) {
               int emulate_delay = atoi(value_str);
               if(emulate_delay < 0 || emulate_delay >= (int)(sizeof(options_emulate_delay) /
                                                              sizeof(options_emulate_delay[0])))
                  continue;
               nfc_playlist->worker_info.settings->emulate_delay = emulate_delay;
            } else if(strcmp(key_str, "emulate_led_indicator") == 0) {
               nfc_playlist->worker_info.settings->emulate_led_indicator =
                  (strcasecmp(value_str, "true") == 0);
            } else if(strcmp(key_str, "skip_error") == 0) {
               nfc_playlist->worker_info.settings->skip_error =
                  (strcasecmp(value_str, "true") == 0);
            } else if(strcmp(key_str, "loop") == 0) {
               nfc_playlist->worker_info.settings->loop = (strcasecmp(value_str, "true") == 0);
            } else if(strcmp(key_str, "time_controls") == 0) {
               nfc_playlist->worker_info.settings->time_controls =
                  (strcasecmp(value_str, "true") == 0);
            } else if(strcmp(key_str, "user_controls") == 0) {
               nfc_playlist->worker_info.settings->user_controls =
                  (strcasecmp(value_str, "true") == 0);
            }
         }

         if(!nfc_playlist->worker_info.settings->time_controls &&
            !nfc_playlist->worker_info.settings->user_controls) {
            nfc_playlist->worker_info.settings->time_controls = true;
            nfc_playlist->worker_info.settings->user_controls = true;
         }

         file_stream_close(stream);
         furi_string_free(line);
         furi_string_free(key);
         furi_string_free(value);
      }
      stream_free(stream);
   }

   furi_record_close(RECORD_STORAGE);
}

void nfc_playlist_save_settings(void* context) {
   furi_assert(context);
   NfcPlaylist* nfc_playlist = context;

   FuriString* tmp_str = furi_string_alloc();

   furi_string_printf(
      tmp_str,
      "emulate_timeout=%d\nemulate_delay=%d\nemulate_led_indicator=%s\nskip_error=%s\nloop=%s\ntime_controls=%s\nuser_controls=%s",
      nfc_playlist->worker_info.settings->emulate_timeout,
      nfc_playlist->worker_info.settings->emulate_delay,
      nfc_playlist->worker_info.settings->emulate_led_indicator ? "true" : "false",
      nfc_playlist->worker_info.settings->skip_error ? "true" : "false",
      nfc_playlist->worker_info.settings->loop ? "true" : "false",
      nfc_playlist->worker_info.settings->time_controls ? "true" : "false",
      nfc_playlist->worker_info.settings->user_controls ? "true" : "false");

   Storage* storage = furi_record_open(RECORD_STORAGE);
   Stream* stream = file_stream_alloc(storage);

   file_stream_open(stream, SETTINGS_LOCATION, FSAM_READ_WRITE, FSOM_OPEN_ALWAYS);
   stream_clean(stream);
   stream_write_string(stream, tmp_str);
   file_stream_close(stream);
   stream_free(stream);
   furi_record_close(RECORD_STORAGE);

   furi_string_free(tmp_str);
}

void nfc_playlist_delete_settings(void* context) {
   furi_assert(context);
   NfcPlaylist* nfc_playlist = context;

   Storage* storage = furi_record_open(RECORD_STORAGE);

   if(storage_file_exists(storage, SETTINGS_LOCATION)) {
      storage_simply_remove(storage, SETTINGS_LOCATION);
   }

   furi_record_close(RECORD_STORAGE);

   nfc_playlist_load_settings(nfc_playlist);
}

static NfcPlaylist* nfc_playlist_alloc() {
   NfcPlaylist* nfc_playlist = malloc(sizeof(NfcPlaylist));
   furi_assert(nfc_playlist);

   nfc_playlist->scene_manager = scene_manager_alloc(&nfc_playlist_scene_handlers, nfc_playlist);
   nfc_playlist->view_dispatcher = view_dispatcher_alloc();

   nfc_playlist->views.submenu = submenu_alloc();
   nfc_playlist->views.widget = widget_alloc();
   nfc_playlist->views.file_browser.output = furi_string_alloc();
   nfc_playlist->views.file_browser.view =
      file_browser_alloc(nfc_playlist->views.file_browser.output);
   nfc_playlist->views.variable_item_list = variable_item_list_alloc();
   nfc_playlist->views.text_input.view = text_input_alloc();
   nfc_playlist->views.dialog_ex = dialog_ex_alloc();

   nfc_playlist->notification_app = furi_record_open(RECORD_NOTIFICATION);

   Storage* storage = furi_record_open(RECORD_STORAGE);
   storage_simply_mkdir(storage, PLAYLIST_DIR);
   furi_record_close(RECORD_STORAGE);

   nfc_playlist->worker_info.settings = malloc(sizeof(NfcPlaylistWorkerSettings));
   furi_assert(nfc_playlist->worker_info.settings);

   nfc_playlist->worker_info.settings->playlist_path = furi_string_alloc();
   nfc_playlist_load_settings(nfc_playlist);

   if(!storage_dir_exists(storage, PLAYLIST_DIR)) {
      storage_simply_mkdir(storage, PLAYLIST_DIR);
   }

   view_dispatcher_set_event_callback_context(nfc_playlist->view_dispatcher, nfc_playlist);
   view_dispatcher_set_custom_event_callback(
      nfc_playlist->view_dispatcher, nfc_playlist_custom_callback);
   view_dispatcher_set_navigation_event_callback(
      nfc_playlist->view_dispatcher, nfc_playlist_back_event_callback);
   view_dispatcher_set_tick_event_callback(
      nfc_playlist->view_dispatcher, nfc_playlist_tick_event_callback, 100);

   view_dispatcher_add_view(
      nfc_playlist->view_dispatcher,
      NfcPlaylistView_Submenu,
      submenu_get_view(nfc_playlist->views.submenu));
   view_dispatcher_add_view(
      nfc_playlist->view_dispatcher,
      NfcPlaylistView_Widget,
      widget_get_view(nfc_playlist->views.widget));
   view_dispatcher_add_view(
      nfc_playlist->view_dispatcher,
      NfcPlaylistView_FileBrowser,
      file_browser_get_view(nfc_playlist->views.file_browser.view));
   view_dispatcher_add_view(
      nfc_playlist->view_dispatcher,
      NfcPlaylistView_VariableItemList,
      variable_item_list_get_view(nfc_playlist->views.variable_item_list));
   view_dispatcher_add_view(
      nfc_playlist->view_dispatcher,
      NfcPlaylistView_TextInput,
      text_input_get_view(nfc_playlist->views.text_input.view));
   view_dispatcher_add_view(
      nfc_playlist->view_dispatcher,
      NfcPlaylistView_DialogEx,
      dialog_ex_get_view(nfc_playlist->views.dialog_ex));
   return nfc_playlist;
}

static void nfc_playlist_free(NfcPlaylist* nfc_playlist) {
   furi_assert(nfc_playlist);

   view_dispatcher_remove_view(nfc_playlist->view_dispatcher, NfcPlaylistView_Submenu);
   view_dispatcher_remove_view(nfc_playlist->view_dispatcher, NfcPlaylistView_Widget);
   view_dispatcher_remove_view(nfc_playlist->view_dispatcher, NfcPlaylistView_VariableItemList);
   view_dispatcher_remove_view(nfc_playlist->view_dispatcher, NfcPlaylistView_FileBrowser);
   view_dispatcher_remove_view(nfc_playlist->view_dispatcher, NfcPlaylistView_TextInput);
   view_dispatcher_remove_view(nfc_playlist->view_dispatcher, NfcPlaylistView_DialogEx);

   scene_manager_free(nfc_playlist->scene_manager);
   view_dispatcher_free(nfc_playlist->view_dispatcher);
   furi_record_close(RECORD_NOTIFICATION);

   submenu_free(nfc_playlist->views.submenu);
   widget_free(nfc_playlist->views.widget);
   file_browser_free(nfc_playlist->views.file_browser.view);
   variable_item_list_free(nfc_playlist->views.variable_item_list);
   text_input_free(nfc_playlist->views.text_input.view);
   dialog_ex_free(nfc_playlist->views.dialog_ex);

   furi_string_free(nfc_playlist->worker_info.settings->playlist_path);
   free(nfc_playlist->worker_info.settings);

   furi_string_free(nfc_playlist->views.file_browser.output);
   free(nfc_playlist);
}

static inline void nfc_playlist_set_log_level() {
#ifdef FURI_DEBUG
   furi_log_set_level(FuriLogLevelTrace);
#else
   furi_log_set_level(FuriLogLevelInfo);
#endif
}

int32_t nfc_playlist_main(void* p) {
   UNUSED(p);

   NfcPlaylist* nfc_playlist = nfc_playlist_alloc();

   nfc_playlist_set_log_level();

   Gui* gui = furi_record_open(RECORD_GUI);
   view_dispatcher_attach_to_gui(nfc_playlist->view_dispatcher, gui, ViewDispatcherTypeFullscreen);
   scene_manager_next_scene(nfc_playlist->scene_manager, NfcPlaylistScene_MainMenu);
   view_dispatcher_run(nfc_playlist->view_dispatcher);

   furi_record_close(RECORD_GUI);
   nfc_playlist_free(nfc_playlist);

   return 0;
}
