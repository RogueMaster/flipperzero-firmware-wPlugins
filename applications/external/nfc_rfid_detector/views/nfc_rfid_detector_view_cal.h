#pragma once

#include <gui/view.h>
#include "../helpers/nfc_rfid_detector_types.h"
#include "../helpers/nfc_rfid_detector_event.h"

#define NFC_RFID_DETECTOR_CAL_LIMIT 1000 // ±100.0 ppm, in 0.1 ppm

typedef struct NfcRfidDetectorCal NfcRfidDetectorCal;

typedef void (*NfcRfidDetectorCalCallback)(NfcRfidDetectorCustomEvent event, void* context);

NfcRfidDetectorCal* nfc_rfid_detector_view_cal_alloc(void);
void nfc_rfid_detector_view_cal_free(NfcRfidDetectorCal* instance);
View* nfc_rfid_detector_view_cal_get_view(NfcRfidDetectorCal* instance);
void nfc_rfid_detector_view_cal_set_callback(
    NfcRfidDetectorCal* instance,
    NfcRfidDetectorCalCallback callback,
    void* context);
// crystal correction, 0.1 ppm units (+ means the Flipper's crystal runs fast)
void nfc_rfid_detector_view_cal_set(NfcRfidDetectorCal* instance, int16_t cal);
int16_t nfc_rfid_detector_view_cal_get(NfcRfidDetectorCal* instance);
