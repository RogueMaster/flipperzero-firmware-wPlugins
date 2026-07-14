#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef struct NotificationApp NotificationApp;

/** 
 * @enum NfcComparatorLedState
 * @brief LED states for NFC Comparator operations.
 */
typedef enum {
   NfcComparatorLedState_Running, /**< Operation in progress */
   NfcComparatorLedState_Complete, /**< Operation completed successfully */
   NfcComparatorLedState_Error /**< Operation encountered an error */
} NfcComparatorLedState;

/**
 * @brief Start the LED worker to indicate a specific state.
 * @param notification_app Pointer to the NotificationApp instance.
 * @param state The LED state to indicate (see NfcComparatorLedState).
 */
void nfc_comparator_led_worker_start(
   NotificationApp* notification_app,
   NfcComparatorLedState state);

/**
 * @brief Stop the LED worker and clear any LED indication.
 * @param notification_app Pointer to the NotificationApp instance.
 */
void nfc_comparator_led_worker_stop(NotificationApp* notification_app);

#ifdef __cplusplus
}
#endif
