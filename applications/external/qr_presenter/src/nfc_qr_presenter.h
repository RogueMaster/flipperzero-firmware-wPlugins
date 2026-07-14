#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <storage/storage.h>

#define NFC_QR_APP_ID   "nfc_qr_presenter"
#define NFC_QR_DATA_DIR "/ext/apps_data/nfc_presenter"

#define NFC_QR_MAX_PAYLOADS 16
#define NFC_QR_NAME_LEN     32
#define NFC_QR_PATH_LEN     128
#define NFC_QR_TEXT_MAX     256
#define NFC_QR_NDEF_MAX     384

typedef struct {
    char name[NFC_QR_NAME_LEN];
    char txt_path[NFC_QR_PATH_LEN];
    char ndef_path[NFC_QR_PATH_LEN];
} NfcQrPayload;

typedef enum {
    NfcQrPayloadActionShare,
    NfcQrPayloadActionAdd,
    NfcQrPayloadActionEdit,
    NfcQrPayloadActionDelete,
} NfcQrPayloadAction;

bool nfc_qr_storage_init(Storage* storage);

size_t nfc_qr_storage_scan(Storage* storage, NfcQrPayload* payloads, size_t payloads_capacity);

bool nfc_qr_storage_read_text(
    Storage* storage,
    const NfcQrPayload* payload,
    char* out,
    size_t out_size);

bool nfc_qr_storage_read_ndef(
    Storage* storage,
    const NfcQrPayload* payload,
    uint8_t* out,
    size_t out_size,
    size_t* out_len);

bool nfc_qr_storage_write_payload(
    Storage* storage,
    const char* fixed_name,
    const char* text,
    NfcQrPayload* out_payload);

bool nfc_qr_storage_delete_payload(Storage* storage, const NfcQrPayload* payload);

size_t nfc_qr_build_ndef_message(const char* text, uint8_t* out, size_t out_size);
