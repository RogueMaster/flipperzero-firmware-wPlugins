#pragma once

typedef enum {
    //NfcRfidDetectorCustomEvent
    NfcRfidDetectorCustomEventStartId = 100,
    NfcRfidDetectorCustomEventCalSave,
    NfcRfidDetectorCustomEventConfig,
    NfcRfidDetectorCustomEventCalReceived,
    NfcRfidDetectorCustomEventCalSessionStart,
    NfcRfidDetectorCustomEventCalProgress,
    NfcRfidDetectorCustomEventCalSessionEnd,
    NfcRfidDetectorCustomEventCalStatusOk,
    NfcRfidDetectorCustomEventCalStatusCancel,
    NfcRfidDetectorCustomEventCalOpen,

} NfcRfidDetectorCustomEvent;
