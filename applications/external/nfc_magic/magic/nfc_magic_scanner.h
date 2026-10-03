#pragma once

#include "protocols/gen4/gen4.h"
#include <nfc/nfc.h>
#include <nfc/protocols/iso14443_3a/iso14443_3a.h>
#include "protocols/nfc_magic_protocols.h"
#include "protocols/gen2/gen2_poller.h"
#include "protocols/uscuid_ul/uscuid_ul_poller.h"
#include <storage/storage.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    NfcMagicScannerEventTypeDetected,
    NfcMagicScannerEventTypeDetectedNotMagic,
    NfcMagicScannerEventTypeNotDetected,
} NfcMagicScannerEventType;

typedef struct {
    NfcMagicProtocol protocol;
    Gen2Type gen2_type; // Valid when protocol == NfcMagicProtocolGen2
    uint8_t gen1_uid_len; // Gen1 UID length class (4/7) derived from uid_len; 0 if not Gen1
    UscuidUlData uscuid_ul; // Valid when protocol == NfcMagicProtocolUscuidUl
    uint8_t uid[ISO14443_3A_MAX_UID_SIZE]; // UID from the standard activation
    uint8_t uid_len; // Bytes valid in uid; 0 when the card never activated (backdoor-only)
} NfcMagicScannerEventData;

typedef struct {
    NfcMagicScannerEventType type;
    NfcMagicScannerEventData data;
} NfcMagicScannerEvent;

typedef void (*NfcMagicScannerCallback)(NfcMagicScannerEvent event, void* context);

typedef struct NfcMagicScanner NfcMagicScanner;

// storage is used to look up the NFC app's per-UID MIFARE Classic key cache, whose sector-0 keys
// widen the Gen2 CUID write probe beyond the default FF..FF. It is borrowed, never freed here, and
// must outlive the scanner.
NfcMagicScanner* nfc_magic_scanner_alloc(Nfc* nfc, Storage* storage);

void nfc_magic_scanner_free(NfcMagicScanner* instance);

void nfc_magic_scanner_set_gen4_password(NfcMagicScanner* instance, Gen4Password password);

void nfc_magic_scanner_start(
    NfcMagicScanner* instance,
    NfcMagicScannerCallback callback,
    void* context);

void nfc_magic_scanner_stop(NfcMagicScanner* instance);

const Gen4* nfc_magic_scanner_get_gen4_data(NfcMagicScanner* instance);

#ifdef __cplusplus
}
#endif
