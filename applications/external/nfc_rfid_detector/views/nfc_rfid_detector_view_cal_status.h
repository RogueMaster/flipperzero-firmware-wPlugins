#pragma once

#include <gui/view.h>
#include "../helpers/nfc_rfid_detector_types.h"
#include "../helpers/nfc_rfid_detector_event.h"

typedef struct NfcRfidDetectorCalStatus NfcRfidDetectorCalStatus;

typedef enum {
    NfcRfidDetectorCalStatusMeasuring,
    NfcRfidDetectorCalStatusSaved,
    NfcRfidDetectorCalStatusEnded, // the session ended without a result
} NfcRfidDetectorCalStatusState;

typedef void (*NfcRfidDetectorCalStatusCallback)(NfcRfidDetectorCustomEvent event, void* context);

NfcRfidDetectorCalStatus* nfc_rfid_detector_view_cal_status_alloc(void);
void nfc_rfid_detector_view_cal_status_free(NfcRfidDetectorCalStatus* instance);
View* nfc_rfid_detector_view_cal_status_get_view(NfcRfidDetectorCalStatus* instance);
void nfc_rfid_detector_view_cal_status_set_callback(
    NfcRfidDetectorCalStatus* instance,
    NfcRfidDetectorCalStatusCallback callback,
    void* context);
void nfc_rfid_detector_view_cal_status_set_state(
    NfcRfidDetectorCalStatus* instance,
    NfcRfidDetectorCalStatusState state);
// elapsed and total in s, estimate and its ± in 0.1 ppm (sig < 0: no estimate yet)
void nfc_rfid_detector_view_cal_status_set_progress(
    NfcRfidDetectorCalStatus* instance,
    uint16_t elapsed,
    uint16_t total,
    int16_t estimate,
    int16_t sig);
// redraw (the countdown runs on between progress reports)
void nfc_rfid_detector_view_cal_status_redraw(NfcRfidDetectorCalStatus* instance);
// the value saved, margin of error ± (< 0 means not known) and the value it replaced, 0.1 ppm
void nfc_rfid_detector_view_cal_status_set_saved(
    NfcRfidDetectorCalStatus* instance,
    int16_t saved,
    int16_t saved_sig,
    int16_t previous);
