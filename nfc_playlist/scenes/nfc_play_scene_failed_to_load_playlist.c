#include "../nfc_playlist.h"

static void nfc_playlist_failed_to_load_playlist_scene_menu_callback(
   GuiButtonType result,
   InputType type,
   void* context) {
   furi_assert(context);
   NfcPlaylist* nfc_playlist = context;
   if(type == InputTypeShort) {
      view_dispatcher_send_custom_event(nfc_playlist->view_dispatcher, result);
   }
}

void nfc_playlist_failed_to_load_playlist_scene_on_enter(void* context) {
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
      "\e#Failed to load playlist!\e#",
      false);
   widget_add_button_element(
      nfc_playlist->views.widget,
      GuiButtonTypeLeft,
      "Try Again",
      nfc_playlist_failed_to_load_playlist_scene_menu_callback,
      nfc_playlist);
   widget_add_button_element(
      nfc_playlist->views.widget,
      GuiButtonTypeRight,
      "Main Menu",
      nfc_playlist_failed_to_load_playlist_scene_menu_callback,
      nfc_playlist);

   view_dispatcher_switch_to_view(nfc_playlist->view_dispatcher, NfcPlaylistView_Widget);
}

bool nfc_playlist_failed_to_load_playlist_scene_on_event(void* context, SceneManagerEvent event) {
   furi_assert(context);
   NfcPlaylist* nfc_playlist = context;
   bool consumed = false;
   if(event.type == SceneManagerEventTypeBack) {
      scene_manager_search_and_switch_to_previous_scene(
         nfc_playlist->scene_manager, NfcPlaylistScene_MainMenu);
      consumed = true;
   } else if(event.type == SceneManagerEventTypeCustom) {
      switch(event.event) {
      case GuiButtonTypeLeft:
         scene_manager_previous_scene(nfc_playlist->scene_manager);
         consumed = true;
         break;
      case GuiButtonTypeRight:
         scene_manager_search_and_switch_to_previous_scene(
            nfc_playlist->scene_manager, NfcPlaylistScene_MainMenu);
         consumed = true;
         break;
      default:
         break;
      }
   }
   return consumed;
}

void nfc_playlist_failed_to_load_playlist_scene_on_exit(void* context) {
   furi_assert(context);
   NfcPlaylist* nfc_playlist = context;
   widget_reset(nfc_playlist->views.widget);
}
