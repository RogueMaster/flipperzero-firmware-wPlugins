#pragma once

#include <stdint.h>

typedef struct BitBuffer BitBuffer;
typedef struct Nfc Nfc;

typedef enum {
    NfcErrorNone = 0,
    NfcErrorGeneric = 1,
} NfcError;

typedef enum {
    NfcModeListener = 0,
} NfcMode;

typedef enum {
    NfcTechIso14443a = 0,
} NfcTech;

typedef enum {
    NfcCommandContinue = 0,
    NfcCommandSleep = 1,
    NfcCommandReset = 2,
    NfcCommandStop = 3,
} NfcCommand;

typedef enum {
    NfcEventTypeListenerActivated = 0,
    NfcEventTypeFieldOff = 1,
    NfcEventTypeRxEnd = 2,
} NfcEventType;

typedef struct {
    NfcEventType type;
    struct {
        BitBuffer *buffer;
    } data;
} NfcEvent;

typedef NfcCommand (*NfcEventCallback)(NfcEvent event, void *context);

Nfc *nfc_alloc(void);
void nfc_free(Nfc *nfc);
void nfc_start(Nfc *nfc, NfcEventCallback callback, void *context);
void nfc_stop(Nfc *nfc);
void nfc_set_fdt_listen_fc(Nfc *nfc, uint32_t fdt_listen_fc);
void nfc_config(Nfc *nfc, NfcMode mode, NfcTech tech);
NfcError nfc_listener_tx(Nfc *nfc, const BitBuffer *buffer);
void nfc_iso14443a_listener_set_col_res_data(Nfc *nfc, const uint8_t *uid, uint8_t uid_len,
                                             const uint8_t *atqa, uint8_t sak);
