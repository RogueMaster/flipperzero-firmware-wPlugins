#pragma once

#include "nfc_qr_presenter.h"

#include <furi.h>

typedef enum {
    NfcQrNfcStatusIdle,
    NfcQrNfcStatusDisabled,
    NfcQrNfcStatusStarting,
    NfcQrNfcStatusRunning,
    NfcQrNfcStatusErrorPayload,
    NfcQrNfcStatusErrorNoMemory,
    NfcQrNfcStatusErrorTagBuild,
    NfcQrNfcStatusErrorNfc,
    NfcQrNfcStatusErrorListener,
} NfcQrNfcStatus;

typedef struct {
    FuriThread* thread;
    volatile bool stop_requested;
    volatile bool running;
    volatile NfcQrNfcStatus status;
    uint8_t ndef[NFC_QR_NDEF_MAX];
    size_t ndef_len;
} NfcQrNfcWorker;

void nfc_qr_nfc_worker_init(NfcQrNfcWorker* worker);
bool nfc_qr_nfc_worker_start(NfcQrNfcWorker* worker, const uint8_t* ndef, size_t ndef_len);
void nfc_qr_nfc_worker_stop(NfcQrNfcWorker* worker);
const char* nfc_qr_nfc_worker_status_text(NfcQrNfcStatus status);
