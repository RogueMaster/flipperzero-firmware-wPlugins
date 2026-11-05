#pragma once

#include <gui/view.h>
#include "../helpers/nfc_rfid_detector_types.h"
#include "../helpers/nfc_rfid_detector_event.h"
#include "../helpers/nfc_rfid_detector_lf_meter.h"

typedef struct NfcRfidDetectorFieldPresence NfcRfidDetectorFieldPresence;

typedef void (
    *NfcRfidDetectorFieldPresenceCallback)(NfcRfidDetectorCustomEvent event, void* context);

void nfc_rfid_detector_view_field_presence_set_callback(
    NfcRfidDetectorFieldPresence* instance,
    NfcRfidDetectorFieldPresenceCallback callback,
    void* context);

void nfc_rfid_detector_view_field_presence_update(
    NfcRfidDetectorFieldPresence* instance,
    bool nfc_field,
    const NfcRfidDetectorLfMeterResult* lf_result);

// OK pressed since the last call (restart the LF average)
bool nfc_rfid_detector_view_field_presence_take_restart(NfcRfidDetectorFieldPresence* instance);
// crystal correction for this Flipper and its ± (< 0: not known), 0.1 ppm units (+ means the crystal runs fast)
void nfc_rfid_detector_view_field_presence_set_cal(
    NfcRfidDetectorFieldPresence* instance,
    int16_t cal,
    int16_t cal_sig);

NfcRfidDetectorFieldPresence* nfc_rfid_detector_view_field_presence_alloc();

void nfc_rfid_detector_view_field_presence_free(NfcRfidDetectorFieldPresence* instance);

View* nfc_rfid_detector_view_field_presence_get_view(NfcRfidDetectorFieldPresence* instance);
