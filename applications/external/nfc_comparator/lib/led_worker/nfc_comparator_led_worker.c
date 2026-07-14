#include "nfc_comparator_led_worker_i.h"

void nfc_comparator_led_worker_start(
   NotificationApp* notification_app,
   NfcComparatorLedState state) {
   switch(state) {
   case NfcComparatorLedState_Running:
      notification_message_block(notification_app, &sequence_blink_start_cyan);
      break;
   case NfcComparatorLedState_Complete:
      notification_message_block(notification_app, &sequence_success);
      break;
   case NfcComparatorLedState_Error:
      notification_message_block(notification_app, &sequence_error);
      break;
   default:
      break;
   }
}

void nfc_comparator_led_worker_stop(NotificationApp* notification_app) {
   notification_message_block(notification_app, &sequence_blink_stop);
}
