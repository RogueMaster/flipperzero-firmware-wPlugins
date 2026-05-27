#pragma once

#include "nfc_protocol.h"

typedef void NfcGenericInstance;
typedef void NfcGenericEventData;

typedef struct {
    NfcProtocol protocol;
    NfcGenericInstance *instance;
    NfcGenericEventData *event_data;
} NfcGenericEvent;
