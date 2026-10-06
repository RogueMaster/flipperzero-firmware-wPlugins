#pragma once

#include <stdbool.h>
#include <nfc/nfc_poller.h>
#include <nfc/nfc_scanner.h>
#include <notification/notification.h>

/** Shared scanner lifecycle for Magic Check and UID Changer. */
void mtools_card_scan_start(
    NfcScanner* scanner,
    bool* scanning,
    NotificationApp* notifications,
    NfcScannerCallback callback,
    void* context);
void mtools_card_scan_stop(NfcScanner* scanner, bool* scanning);
void mtools_card_reader_stop(
    NfcScanner* scanner,
    bool* scanning,
    NfcPoller** poller,
    NotificationApp* notifications);
