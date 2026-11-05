#pragma once

#include "nfc_rfid_detector_lf_meter.h"

// The LF field count over many gates, fed into an LF meter (helpers/nfc_rfid_detector_lf_meter.h)
typedef struct NfcRfidDetectorLfCount NfcRfidDetectorLfCount;

NfcRfidDetectorLfCount* nfc_rfid_detector_lf_count_alloc(void);

// Call after furi_hal_rfid_field_detect_stop(). Until then the DMA keeps copying the field count into the LF
// count's buffer.
void nfc_rfid_detector_lf_count_free(NfcRfidDetectorLfCount* count);

// Call after furi_hal_rfid_field_detect_start(). The last result stays on screen, marked as held.
void nfc_rfid_detector_lf_count_start(NfcRfidDetectorLfCount* count);

// Feed the gates that arrived since the last call into the meter, and get its result
void nfc_rfid_detector_lf_count_read(
    NfcRfidDetectorLfCount* count,
    NfcRfidDetectorLfMeterResult* result);

// Restart the meter's average
void nfc_rfid_detector_lf_count_restart(NfcRfidDetectorLfCount* count);
