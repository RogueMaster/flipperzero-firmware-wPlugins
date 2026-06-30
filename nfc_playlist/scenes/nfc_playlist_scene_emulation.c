#include "../nfc_playlist.h"

static FuriString* header_str_static = NULL;
static FuriString* text_str_static = NULL;

static void nfc_playlist_emulation_scene_dialog_callback(DialogExResult result, void* context) {
   furi_assert(context);
   NfcPlaylist* nfc_playlist = context;

   switch(result) {
   case DialogExResultLeft:
      scene_manager_handle_custom_event(nfc_playlist->scene_manager, GuiButtonTypeLeft);
      break;
   case DialogExResultRight:
      scene_manager_handle_custom_event(nfc_playlist->scene_manager, GuiButtonTypeRight);
      break;
   default:
      break;
   }
}

void nfc_playlist_emulation_scene_on_enter(void* context) {
   furi_assert(context);
   NfcPlaylist* nfc_playlist = context;

   dialog_ex_reset(nfc_playlist->views.dialog_ex);
   dialog_ex_set_context(nfc_playlist->views.dialog_ex, nfc_playlist);
   dialog_ex_set_result_callback(
      nfc_playlist->views.dialog_ex, nfc_playlist_emulation_scene_dialog_callback);
   view_dispatcher_switch_to_view(nfc_playlist->view_dispatcher, NfcPlaylistView_DialogEx);

   nfc_playlist->worker_info.worker =
      nfc_playlist_worker_alloc(nfc_playlist->worker_info.settings);
   nfc_playlist_worker_start(nfc_playlist->worker_info.worker);
}

bool nfc_playlist_emulation_scene_on_event(void* context, SceneManagerEvent event) {
   furi_assert(context);
   NfcPlaylist* nfc_playlist = context;
   bool consumed = false;

   if(event.type == SceneManagerEventTypeBack) {
      nfc_playlist_worker_stop(nfc_playlist->worker_info.worker);
      consumed = true;
   } else if(event.type == SceneManagerEventTypeCustom) {
      switch(event.event) {
      case GuiButtonTypeLeft:
         nfc_playlist_worker_rewind(nfc_playlist->worker_info.worker);
         consumed = true;
         break;
      case GuiButtonTypeRight:
         nfc_playlist_worker_skip(nfc_playlist->worker_info.worker);
         consumed = true;
         break;
      default:
         break;
      }
   } else if(event.type == SceneManagerEventTypeTick) {
      switch(nfc_playlist->worker_info.worker->state) {
      case NfcPlaylistWorkerState_Emulating: {
         if(nfc_playlist->worker_info.settings->emulate_led_indicator) {
            nfc_playlist_led_worker_start(
               nfc_playlist->notification_app, NfcPlaylistLedState_Emulating);
         }
         if(!header_str_static) header_str_static = furi_string_alloc();
         if(!text_str_static) text_str_static = furi_string_alloc();
         FuriString* nfc_card_name = furi_string_alloc();
         path_extract_filename_no_ext(
            furi_string_get_cstr(nfc_playlist->worker_info.worker->nfc_card_path), nfc_card_name);
         furi_string_printf(
            header_str_static, "Emulating:\n%s", furi_string_get_cstr(nfc_card_name));
         furi_string_free(nfc_card_name);
         dialog_ex_set_header(
            nfc_playlist->views.dialog_ex,
            furi_string_get_cstr(header_str_static),
            64,
            5,
            AlignCenter,
            AlignTop);
         if(nfc_playlist->worker_info.settings->time_controls) {
            furi_string_printf(
               text_str_static, "%ds", (nfc_playlist->worker_info.worker->ms_counter / 1000));
            dialog_ex_set_text(
               nfc_playlist->views.dialog_ex,
               furi_string_get_cstr(text_str_static),
               64,
               50,
               AlignCenter,
               AlignTop);
         }
         if(nfc_playlist->worker_info.settings->user_controls) {
            dialog_ex_set_left_button_text(nfc_playlist->views.dialog_ex, "Rewind");
            dialog_ex_set_right_button_text(nfc_playlist->views.dialog_ex, "Skip");
         }
         consumed = true;
         break;
      }
      case NfcPlaylistWorkerState_Delaying: {
         if(nfc_playlist->worker_info.settings->emulate_led_indicator) {
            nfc_playlist_led_worker_start(
               nfc_playlist->notification_app, NfcPlaylistLedState_Delaying);
         }
         if(!text_str_static) text_str_static = furi_string_alloc();
         dialog_ex_set_header(
            nfc_playlist->views.dialog_ex, "Delaying", 64, 5, AlignCenter, AlignTop);
         furi_string_printf(
            text_str_static, "%ds", (nfc_playlist->worker_info.worker->ms_counter / 1000));
         dialog_ex_set_text(
            nfc_playlist->views.dialog_ex,
            furi_string_get_cstr(text_str_static),
            64,
            50,
            AlignCenter,
            AlignTop);
         if(nfc_playlist->worker_info.settings->user_controls) {
            dialog_ex_set_left_button_text(nfc_playlist->views.dialog_ex, "Rewind");
            dialog_ex_set_right_button_text(nfc_playlist->views.dialog_ex, "Skip");
         }
         consumed = true;
         break;
      }
      case NfcPlaylistWorkerState_FailedToLoadPlaylist: {
         if(nfc_playlist->worker_info.settings->emulate_led_indicator) {
            nfc_playlist_led_worker_start(
               nfc_playlist->notification_app, NfcPlaylistLedState_Error);
         }
         nfc_playlist_worker_stop(nfc_playlist->worker_info.worker);
         scene_manager_next_scene(
            nfc_playlist->scene_manager, NfcPlaylistScene_FailedToLoadPlaylist);
         consumed = true;
         break;
      }
      case NfcPlaylistWorkerState_FailedToLoadNfcCard: {
         if(nfc_playlist->worker_info.settings->emulate_led_indicator) {
            nfc_playlist_led_worker_start(
               nfc_playlist->notification_app, NfcPlaylistLedState_Error);
         }
         if(!header_str_static) header_str_static = furi_string_alloc();
         if(!text_str_static) text_str_static = furi_string_alloc();
         furi_string_printf(
            header_str_static,
            "Failed to load:\n%s",
            furi_string_get_cstr(nfc_playlist->worker_info.worker->nfc_card_path));
         dialog_ex_set_header(
            nfc_playlist->views.dialog_ex,
            furi_string_get_cstr(header_str_static),
            64,
            5,
            AlignCenter,
            AlignTop);
         if(nfc_playlist->worker_info.settings->time_controls) {
            furi_string_printf(
               text_str_static, "%ds", (nfc_playlist->worker_info.worker->ms_counter / 1000));
            dialog_ex_set_text(
               nfc_playlist->views.dialog_ex,
               furi_string_get_cstr(text_str_static),
               64,
               50,
               AlignCenter,
               AlignTop);
         }
         if(nfc_playlist->worker_info.settings->user_controls) {
            dialog_ex_set_left_button_text(nfc_playlist->views.dialog_ex, "Rewind");
            dialog_ex_set_right_button_text(nfc_playlist->views.dialog_ex, "Skip");
         }
         consumed = true;
         break;
      }
      case NfcPlaylistWorkerState_InvalidFileType: {
         if(nfc_playlist->worker_info.settings->emulate_led_indicator) {
            nfc_playlist_led_worker_start(
               nfc_playlist->notification_app, NfcPlaylistLedState_Error);
         }
         if(!header_str_static) header_str_static = furi_string_alloc();
         if(!text_str_static) text_str_static = furi_string_alloc();
         furi_string_printf(
            header_str_static,
            "Invalid file type:\n%s",
            furi_string_get_cstr(nfc_playlist->worker_info.worker->nfc_card_path));
         dialog_ex_set_header(
            nfc_playlist->views.dialog_ex,
            furi_string_get_cstr(header_str_static),
            64,
            5,
            AlignCenter,
            AlignTop);
         if(nfc_playlist->worker_info.settings->time_controls) {
            furi_string_printf(
               text_str_static, "%ds", (nfc_playlist->worker_info.worker->ms_counter / 1000));
            dialog_ex_set_text(
               nfc_playlist->views.dialog_ex,
               furi_string_get_cstr(text_str_static),
               64,
               50,
               AlignCenter,
               AlignTop);
         }
         if(nfc_playlist->worker_info.settings->user_controls) {
            dialog_ex_set_left_button_text(nfc_playlist->views.dialog_ex, "Rewind");
            dialog_ex_set_right_button_text(nfc_playlist->views.dialog_ex, "Skip");
         }
         consumed = true;
         break;
      }
      case NfcPlaylistWorkerState_FileDoesNotExist: {
         if(nfc_playlist->worker_info.settings->emulate_led_indicator) {
            nfc_playlist_led_worker_start(
               nfc_playlist->notification_app, NfcPlaylistLedState_Error);
         }
         if(!header_str_static) header_str_static = furi_string_alloc();
         if(!text_str_static) text_str_static = furi_string_alloc();
         furi_string_printf(
            header_str_static,
            "File does not exist:\n%s",
            furi_string_get_cstr(nfc_playlist->worker_info.worker->nfc_card_path));
         dialog_ex_set_header(
            nfc_playlist->views.dialog_ex,
            furi_string_get_cstr(header_str_static),
            64,
            5,
            AlignCenter,
            AlignTop);
         if(nfc_playlist->worker_info.settings->time_controls) {
            furi_string_printf(
               text_str_static, "%ds", (nfc_playlist->worker_info.worker->ms_counter / 1000));
            dialog_ex_set_text(
               nfc_playlist->views.dialog_ex,
               furi_string_get_cstr(text_str_static),
               64,
               50,
               AlignCenter,
               AlignTop);
         }
         if(nfc_playlist->worker_info.settings->user_controls) {
            dialog_ex_set_left_button_text(nfc_playlist->views.dialog_ex, "Rewind");
            dialog_ex_set_right_button_text(nfc_playlist->views.dialog_ex, "Skip");
         }
         consumed = true;
         break;
      }
      case NfcPlaylistWorkerState_Stopped: {
         nfc_playlist_worker_stop(nfc_playlist->worker_info.worker);
         scene_manager_next_scene(nfc_playlist->scene_manager, NfcPlaylistScene_EmulationFinished);
         consumed = true;
         break;
      }
      default:
         break;
      }
   }
   return consumed;
}

void nfc_playlist_emulation_scene_on_exit(void* context) {
   furi_assert(context);
   NfcPlaylist* nfc_playlist = context;
   nfc_playlist_led_worker_stop(nfc_playlist->notification_app);
   dialog_ex_reset(nfc_playlist->views.dialog_ex);
   nfc_playlist_worker_free(nfc_playlist->worker_info.worker);
   nfc_playlist->worker_info.worker = NULL;
   if(header_str_static) {
      furi_string_free(header_str_static);
      header_str_static = NULL;
   }
   if(text_str_static) {
      furi_string_free(text_str_static);
      text_str_static = NULL;
   }
}
