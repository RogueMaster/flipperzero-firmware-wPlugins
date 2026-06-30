#include "../nfc_playlist.h"

typedef enum {
   NfcPlaylistPlaylistEdit_CreatePlaylist,
   NfcPlaylistPlaylistEdit_DeletePlaylist,
   NfcPlaylistPlaylistEdit_RenamePlaylist,
   NfcPlaylistPlaylistEdit_AddNfcItem,
   NfcPlaylistPlaylistEdit_RemoveNfcItem,
   NfcPlaylistPlaylistEdit_MoveNfcItem,
   NfcPlaylistPlaylistEdit_ViewPlaylistContent
} NfcPlaylistPlaylistEditMenuSelection;

static void nfc_playlist_playlist_edit_scene_menu_callback(void* context, uint32_t index) {
   furi_assert(context);
   NfcPlaylist* nfc_playlist = context;
   scene_manager_handle_custom_event(nfc_playlist->scene_manager, index);
}

void nfc_playlist_playlist_edit_scene_on_enter(void* context) {
   NfcPlaylist* nfc_playlist = context;

   submenu_set_header(nfc_playlist->views.submenu, "Edit Playlist");

   bool playlist_path_empty = furi_string_empty(nfc_playlist->worker_info.settings->playlist_path);

   submenu_add_item(
      nfc_playlist->views.submenu,
      "Create Playlist",
      NfcPlaylistPlaylistEdit_CreatePlaylist,
      nfc_playlist_playlist_edit_scene_menu_callback,
      nfc_playlist);

   submenu_add_lockable_item(
      nfc_playlist->views.submenu,
      "Delete Playlist",
      NfcPlaylistPlaylistEdit_DeletePlaylist,
      nfc_playlist_playlist_edit_scene_menu_callback,
      nfc_playlist,
      playlist_path_empty,
      "No\nplaylist\nselected");

   submenu_add_lockable_item(
      nfc_playlist->views.submenu,
      "Rename Playlist",
      NfcPlaylistPlaylistEdit_RenamePlaylist,
      nfc_playlist_playlist_edit_scene_menu_callback,
      nfc_playlist,
      playlist_path_empty,
      "No\nplaylist\nselected");

   submenu_add_lockable_item(
      nfc_playlist->views.submenu,
      "Add NFC Item",
      NfcPlaylistPlaylistEdit_AddNfcItem,
      nfc_playlist_playlist_edit_scene_menu_callback,
      nfc_playlist,
      playlist_path_empty,
      "No\nplaylist\nselected");

   submenu_add_lockable_item(
      nfc_playlist->views.submenu,
      "Remove NFC Item",
      NfcPlaylistPlaylistEdit_RemoveNfcItem,
      nfc_playlist_playlist_edit_scene_menu_callback,
      nfc_playlist,
      playlist_path_empty,
      "No\nplaylist\nselected");

   submenu_add_lockable_item(
      nfc_playlist->views.submenu,
      "Move NFC Item",
      NfcPlaylistPlaylistEdit_MoveNfcItem,
      nfc_playlist_playlist_edit_scene_menu_callback,
      nfc_playlist,
      playlist_path_empty,
      "No\nplaylist\nselected");

   submenu_add_lockable_item(
      nfc_playlist->views.submenu,
      "View Playlist Content",
      NfcPlaylistPlaylistEdit_ViewPlaylistContent,
      nfc_playlist_playlist_edit_scene_menu_callback,
      nfc_playlist,
      playlist_path_empty,
      "No\nplaylist\nselected");

   view_dispatcher_switch_to_view(nfc_playlist->view_dispatcher, NfcPlaylistView_Submenu);
}

bool nfc_playlist_playlist_edit_scene_on_event(void* context, SceneManagerEvent event) {
   NfcPlaylist* nfc_playlist = context;
   bool consumed = false;
   if(event.type == SceneManagerEventTypeCustom) {
      switch(event.event) {
      case NfcPlaylistPlaylistEdit_CreatePlaylist:
         scene_manager_next_scene(nfc_playlist->scene_manager, NfcPlaylistScene_NameNewPlaylist);
         consumed = true;
         break;
      case NfcPlaylistPlaylistEdit_DeletePlaylist:
         scene_manager_next_scene(nfc_playlist->scene_manager, NfcPlaylistScene_ConfirmDelete);
         consumed = true;
         break;
      case NfcPlaylistPlaylistEdit_RenamePlaylist:
         scene_manager_next_scene(nfc_playlist->scene_manager, NfcPlaylistScene_PlaylistRename);
         consumed = true;
         break;
      case NfcPlaylistPlaylistEdit_AddNfcItem:
         scene_manager_next_scene(nfc_playlist->scene_manager, NfcPlaylistScene_NfcAdd);
         consumed = true;
         break;
      case NfcPlaylistPlaylistEdit_RemoveNfcItem:
         scene_manager_next_scene(nfc_playlist->scene_manager, NfcPlaylistScene_NfcRemove);
         consumed = true;
         break;
      case NfcPlaylistPlaylistEdit_MoveNfcItem:
         scene_manager_next_scene(nfc_playlist->scene_manager, NfcPlaylistScene_NfcMoveItem);
         consumed = true;
         break;
      case NfcPlaylistPlaylistEdit_ViewPlaylistContent:
         scene_manager_next_scene(
            nfc_playlist->scene_manager, NfcPlaylistScene_ViewPlaylistContent);
         consumed = true;
         break;
      default:
         break;
      }
   }
   return consumed;
}

void nfc_playlist_playlist_edit_scene_on_exit(void* context) {
   NfcPlaylist* nfc_playlist = context;
   submenu_reset(nfc_playlist->views.submenu);
}
