#include "../nfc_playlist.h"

typedef enum {
   NfcPlaylistNfcMoveItem_TargetSelector,
   NfcPlaylistNfcMoveItem_DestinationSelector,
   NfcPlaylistNfcMoveItem_MoveItem
} NfcPlaylistNfcMoveItemMenuSelection;

static uint8_t selected_target;
static uint8_t selected_destination;

static void nfc_playlist_nfc_move_item_scene_lock_state_check(void* context) {
   furi_assert(context);
   NfcPlaylist* nfc_playlist = context;

   variable_item_set_locked(
      variable_item_list_get(
         nfc_playlist->views.variable_item_list, NfcPlaylistNfcMoveItem_MoveItem),
      (selected_target == selected_destination),
      "Target\nand\nDestination\nare the same");
}

static void nfc_playlist_nfc_move_item_scene_menu_callback(void* context, uint32_t index) {
   furi_assert(context);
   NfcPlaylist* nfc_playlist = context;
   scene_manager_handle_custom_event(nfc_playlist->scene_manager, index);
}

static void nfc_playlist_nfc_move_item_scene_options_change_callback(VariableItem* item) {
   furi_assert(item);
   NfcPlaylist* nfc_playlist = variable_item_get_context(item);

   uint8_t current_option =
      variable_item_list_get_selected_item_index(nfc_playlist->views.variable_item_list);
   uint8_t option_value_index = variable_item_get_current_value_index(item);

   switch(current_option) {
   case NfcPlaylistNfcMoveItem_TargetSelector: {
      selected_target = option_value_index + 1;
      FuriString* tmp_str = furi_string_alloc_printf("%d", selected_target);
      variable_item_set_current_value_text(item, furi_string_get_cstr(tmp_str));
      furi_string_free(tmp_str);
      break;
   }
   case NfcPlaylistNfcMoveItem_DestinationSelector: {
      selected_destination = option_value_index + 1;
      FuriString* tmp_str = furi_string_alloc_printf("%d", selected_destination);
      variable_item_set_current_value_text(item, furi_string_get_cstr(tmp_str));
      furi_string_free(tmp_str);
      break;
   }
   default:
      break;
   }

   nfc_playlist_nfc_move_item_scene_lock_state_check(nfc_playlist);
}

void nfc_playlist_nfc_move_item_scene_on_enter(void* context) {
   furi_assert(context);
   NfcPlaylist* nfc_playlist = context;

   selected_target = 1;
   selected_destination = 1;

   variable_item_list_set_header(nfc_playlist->views.variable_item_list, "Move NFC Item");

   VariableItem* target_selector = variable_item_list_add(
      nfc_playlist->views.variable_item_list,
      "Select Target",
      nfc_playlist->worker_info.settings->playlist_length,
      nfc_playlist_nfc_move_item_scene_options_change_callback,
      nfc_playlist);
   variable_item_set_current_value_index(target_selector, 0);
   variable_item_set_current_value_text(target_selector, "1");

   VariableItem* destination_selector = variable_item_list_add(
      nfc_playlist->views.variable_item_list,
      "Select Destination",
      nfc_playlist->worker_info.settings->playlist_length,
      nfc_playlist_nfc_move_item_scene_options_change_callback,
      nfc_playlist);
   variable_item_set_current_value_index(destination_selector, 0);
   variable_item_set_current_value_text(destination_selector, "1");

   variable_item_list_add(nfc_playlist->views.variable_item_list, "Move Item", 0, NULL, NULL);

   nfc_playlist_nfc_move_item_scene_lock_state_check(nfc_playlist);

   variable_item_list_set_enter_callback(
      nfc_playlist->views.variable_item_list,
      nfc_playlist_nfc_move_item_scene_menu_callback,
      nfc_playlist);

   view_dispatcher_switch_to_view(nfc_playlist->view_dispatcher, NfcPlaylistView_VariableItemList);
}

bool nfc_playlist_nfc_move_item_scene_on_event(void* context, SceneManagerEvent event) {
   furi_assert(context);
   NfcPlaylist* nfc_playlist = context;
   bool consumed = false;

   if(event.type == SceneManagerEventTypeCustom) {
      switch(event.event) {
      case NfcPlaylistNfcMoveItem_MoveItem: {
         Storage* storage = furi_record_open(RECORD_STORAGE);
         Stream* stream = file_stream_alloc(storage);

         if(file_stream_open(
               stream,
               furi_string_get_cstr(nfc_playlist->worker_info.settings->playlist_path),
               FSAM_READ_WRITE,
               FSOM_OPEN_EXISTING)) {
            FuriString* tmp_target_str = furi_string_alloc();
            FuriString* line = furi_string_alloc();
            uint8_t counter = 0;

            while(stream_read_line(stream, line)) {
               counter++;
               if(counter == selected_target) {
                  furi_string_trim(line);
                  furi_string_cat(tmp_target_str, line);
                  stream_rewind(stream);
                  counter = 0;
                  break;
               }
            }

            FuriString* tmp_new_order_str = furi_string_alloc();

            while(stream_read_line(stream, line)) {
               counter++;

               if(counter == selected_target) {
                  continue;
               }

               if(!furi_string_empty(tmp_new_order_str)) {
                  furi_string_cat(tmp_new_order_str, "\n");
               }

               furi_string_trim(line);

               if(counter == selected_destination) {
                  if(counter == 1) {
                     furi_string_cat_printf(
                        tmp_new_order_str,
                        "%s\n%s",
                        furi_string_get_cstr(tmp_target_str),
                        furi_string_get_cstr(line));
                  } else {
                     furi_string_cat_printf(
                        tmp_new_order_str,
                        "%s\n%s",
                        furi_string_get_cstr(line),
                        furi_string_get_cstr(tmp_target_str));
                  }
                  furi_string_free(tmp_target_str);
               } else {
                  furi_string_cat(tmp_new_order_str, furi_string_get_cstr(line));
               }
            }

            furi_string_free(line);
            stream_clean(stream);
            stream_write_string(stream, tmp_new_order_str);
            furi_string_free(tmp_new_order_str);
            file_stream_close(stream);
         }

         stream_free(stream);
         furi_record_close(RECORD_STORAGE);

         consumed = true;
         break;
      }
      default:
         break;
      }
   }

   return consumed;
}

void nfc_playlist_nfc_move_item_scene_on_exit(void* context) {
   furi_assert(context);
   NfcPlaylist* nfc_playlist = context;
   variable_item_list_reset(nfc_playlist->views.variable_item_list);
}
