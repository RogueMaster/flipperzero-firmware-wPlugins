#include "nfc_playlist_led_worker.h"

void nfc_playlist_led_worker_start(NotificationApp* notification_app, NfcPlaylistLedState state) {
   switch(state) {
   case NfcPlaylistLedState_Emulating:
      notification_message_block(notification_app, &sequence_blink_start_cyan);
      break;
   case NfcPlaylistLedState_Delaying:
      notification_message_block(notification_app, &sequence_blink_start_yellow);
      break;
   case NfcPlaylistLedState_Error:
      notification_message_block(notification_app, &sequence_blink_start_red);
      break;
   default:
      break;
   }
}

void nfc_playlist_led_worker_stop(NotificationApp* notification_app) {
   notification_message_block(notification_app, &sequence_blink_stop);
}
