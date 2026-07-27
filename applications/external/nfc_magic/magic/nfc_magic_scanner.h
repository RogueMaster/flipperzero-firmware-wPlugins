#pragma once

#include "protocols/gen4/gen4.h"
#include <nfc/nfc.h>
#include "protocols/nfc_magic_protocols.h"
#include "protocols/gen2/gen2_poller.h"
#include "protocols/uscuid_ul/uscuid_ul_poller.h"

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
    uint8_t gen1_uid_len; // Gen1 UID length (4/7) when resolved; 0 if unknown or not Gen1
    UscuidUlData uscuid_ul; // Valid when protocol == NfcMagicProtocolUscuidUl
} NfcMagicScannerEventData;

typedef struct {
    NfcMagicScannerEventType type;
    NfcMagicScannerEventData data;
} NfcMagicScannerEvent;

typedef void (*NfcMagicScannerCallback)(NfcMagicScannerEvent event, void* context);

typedef struct NfcMagicScanner NfcMagicScanner;

NfcMagicScanner* nfc_magic_scanner_alloc(Nfc* nfc);

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
