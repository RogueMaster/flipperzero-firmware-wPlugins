#pragma once
#include <notification/notification_messages.h>

#ifdef __cplusplus
extern "C" {
#endif

/** LED states for NFC Playlist operations */
typedef enum {
   NfcPlaylistLedState_Emulating, /**< Operation in progress */
   NfcPlaylistLedState_Delaying,  /**< Delaying between operations */
   NfcPlaylistLedState_Error /**< Operation encountered an error */
} NfcPlaylistLedState;

/**
 * Start the LED worker to indicate a specific state.
 * @param notification_app Pointer to the NotificationApp instance.
 * @param state The LED state to indicate.
 */
void nfc_playlist_led_worker_start(NotificationApp* notification_app, NfcPlaylistLedState state);

/**
 * Stop the LED worker and clear any LED indication.
 * @param notification_app Pointer to the NotificationApp instance.
 */
void nfc_playlist_led_worker_stop(NotificationApp* notification_app);

#ifdef __cplusplus
}
#endif
