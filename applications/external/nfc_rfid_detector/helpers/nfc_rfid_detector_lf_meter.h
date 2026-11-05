#pragma once

#include <stdbool.h>
#include <stdint.h>

// The LF meter adds up the 100 ms gate counts (see nfc_rfid_detector_lf_count.c) while the field is steady. A field
// arriving or leaving gives low counts, so those gates are skipped. The result is preserved after the field goes
// away, until a new steady field appears, or detection is manually restarted.
typedef struct NfcRfidDetectorLfMeter NfcRfidDetectorLfMeter;

typedef struct {
    bool present; // a carrier is present (settling or measuring)
    bool settling; // carrier present, not yet steady
    bool held; // the result is from a field that has gone
    uint32_t last_gate_edges; // edges in the latest gate
    uint64_t edges; // counted into the result
    uint32_t gates; // 100 ms gates counted into the result
    uint32_t runs; // unbroken runs of gates in the result, each good to ±1 edge
} NfcRfidDetectorLfMeterResult;

NfcRfidDetectorLfMeter* nfc_rfid_detector_lf_meter_alloc(void);

void nfc_rfid_detector_lf_meter_free(NfcRfidDetectorLfMeter* meter);

// Restart the average (OK): keeps a steady field measuring, clears a held result
void nfc_rfid_detector_lf_meter_restart(NfcRfidDetectorLfMeter* meter);

void nfc_rfid_detector_lf_meter_feed(NfcRfidDetectorLfMeter* meter, uint32_t gate_edges);

// Counting stopped (e.g. another scene): keep any result as held, as if the field had gone
void nfc_rfid_detector_lf_meter_hold(NfcRfidDetectorLfMeter* meter);

void nfc_rfid_detector_lf_meter_get_result(
    const NfcRfidDetectorLfMeter* meter,
    NfcRfidDetectorLfMeterResult* result);
