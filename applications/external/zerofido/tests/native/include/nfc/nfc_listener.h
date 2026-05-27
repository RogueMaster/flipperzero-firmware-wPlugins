#pragma once

#include "nfc.h"
#include "protocols/nfc_device_base.h"
#include "protocols/nfc_generic_event.h"

typedef struct NfcListener NfcListener;

typedef NfcCommand (*NfcGenericCallback)(NfcGenericEvent event, void *context);

NfcListener *nfc_listener_alloc(Nfc *nfc, NfcProtocol protocol, const NfcDeviceData *data);
void nfc_listener_free(NfcListener *instance);
void nfc_listener_start(NfcListener *instance, NfcGenericCallback callback, void *context);
void nfc_listener_stop(NfcListener *instance);
