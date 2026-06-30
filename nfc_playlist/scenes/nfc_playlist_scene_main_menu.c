#include "../nfc_playlist.h"

typedef enum {
   NfcPlaylistMainMenu_Start,
   NfcPlaylistMainMenu_PlaylistSelect,
   NfcPlaylistMainMenu_FileEdit,
   NfcPlaylistMainMenu_Settings
} NfcPlaylistMainMenuMenuSelection;

static void nfc_playlist_main_menu_scene_menu_callback(void* context, uint32_t index) {
   furi_assert(context);
   NfcPlaylist* nfc_playlist = context;
   scene_manager_handle_custom_event(nfc_playlist->scene_manager, index);
}

void nfc_playlist_main_menu_scene_on_enter(void* context) {
   furi_assert(context);
   NfcPlaylist* nfc_playlist = context;

   FuriString* header = furi_string_alloc_printf("NFC Playlist v%s", FAP_VERSION);
   submenu_set_header(nfc_playlist->views.submenu, furi_string_get_cstr(header));
   furi_string_free(header);

   submenu_add_lockable_item(
      nfc_playlist->views.submenu,
      "Start",
      NfcPlaylistMainMenu_Start,
      nfc_playlist_main_menu_scene_menu_callback,
      nfc_playlist,
      furi_string_empty(nfc_playlist->worker_info.settings->playlist_path),
      "No\nplaylist\nselected");

   submenu_add_item(
      nfc_playlist->views.submenu,
      "Select playlist",
      NfcPlaylistMainMenu_PlaylistSelect,
      nfc_playlist_main_menu_scene_menu_callback,
      nfc_playlist);

   submenu_add_item(
      nfc_playlist->views.submenu,
      "Edit playlist",
      NfcPlaylistMainMenu_FileEdit,
      nfc_playlist_main_menu_scene_menu_callback,
      nfc_playlist);

   submenu_add_item(
      nfc_playlist->views.submenu,
      "Settings",
      NfcPlaylistMainMenu_Settings,
      nfc_playlist_main_menu_scene_menu_callback,
      nfc_playlist);

   view_dispatcher_switch_to_view(nfc_playlist->view_dispatcher, NfcPlaylistView_Submenu);
}

bool nfc_playlist_main_menu_scene_on_event(void* context, SceneManagerEvent event) {
   NfcPlaylist* nfc_playlist = context;
   bool consumed = false;
   if(event.type == SceneManagerEventTypeCustom) {
      switch(event.event) {
      case NfcPlaylistMainMenu_Start:
         scene_manager_next_scene(nfc_playlist->scene_manager, NfcPlaylistScene_Emulation);
         consumed = true;
         break;
      case NfcPlaylistMainMenu_PlaylistSelect:
         scene_manager_next_scene(nfc_playlist->scene_manager, NfcPlaylistScene_PlaylistSelect);
         consumed = true;
         break;
      case NfcPlaylistMainMenu_FileEdit:
         scene_manager_next_scene(nfc_playlist->scene_manager, NfcPlaylistScene_PlaylistEdit);
         consumed = true;
         break;
      case NfcPlaylistMainMenu_Settings:
         scene_manager_next_scene(nfc_playlist->scene_manager, NfcPlaylistScene_Settings);
         consumed = true;
         break;
      default:
         break;
      }
   }
   return consumed;
}

void nfc_playlist_main_menu_scene_on_exit(void* context) {
   furi_assert(context);
   NfcPlaylist* nfc_playlist = context;
   submenu_reset(nfc_playlist->views.submenu);
}
