#include "card_reader.h"

#include <notification/notification_messages.h>

void mtools_card_scan_start(
    NfcScanner* scanner,
    bool* scanning,
    NotificationApp* notifications,
    NfcScannerCallback callback,
    void* context) {
    *scanning = true;
    notification_message(notifications, &sequence_blink_start_blue);
    nfc_scanner_start(scanner, callback, context);
}

void mtools_card_scan_stop(NfcScanner* scanner, bool* scanning) {
    if(*scanning) {
        nfc_scanner_stop(scanner);
        *scanning = false;
    }
}

void mtools_card_reader_stop(
    NfcScanner* scanner,
    bool* scanning,
    NfcPoller** poller,
    NotificationApp* notifications) {
    mtools_card_scan_stop(scanner, scanning);
    if(*poller) {
        nfc_poller_stop(*poller);
        nfc_poller_free(*poller);
        *poller = NULL;
    }
    notification_message(notifications, &sequence_blink_stop);
}
