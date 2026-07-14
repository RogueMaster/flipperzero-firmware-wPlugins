#pragma once

#include <nfc/nfc.h>

typedef enum {
    NfcLoggerEventError,
    NfcLoggerEventReady,
} NfcLoggerEventType;

typedef struct {
    NfcLoggerEventType type;
    void* data;
} NfcLoggerEvent;

typedef struct _NfcLogger NfcLogger;

// Called when the NFC instance has got a command
typedef NfcCommand (*NfcLoggerCallback)(NfcLoggerEvent event, void* context);

NfcLogger* nfc_sniffer_alloc(Nfc* nfc, NfcTech protocol);
void nfc_sniffer_free(NfcLogger* instance);

void nfc_sniffer_start(NfcLogger* instance, NfcLoggerCallback callback, void* context);
void nfc_sniffer_stop(NfcLogger* instance);
