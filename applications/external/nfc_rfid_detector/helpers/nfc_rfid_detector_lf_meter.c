#include "nfc_rfid_detector_lf_meter.h"

#include <furi.h>
#include <string.h>

#define LF_METER_GATE_MIN 8000 // edges per 100 ms: 80 kHz
#define LF_METER_GATE_MAX 20000 // 200 kHz
#define LF_METER_SETTLE   5
#define LF_METER_TOL      4 // the most a gate's edges may differ from the steady count
#define LF_METER_HOLDBACK 3
#define LF_METER_LOST     3

typedef enum {
    NfcRfidDetectorLfMeterStateIdle, // no carrier
    NfcRfidDetectorLfMeterStateSettling, // carrier seen, not yet steady
    NfcRfidDetectorLfMeterStateMeasuring,
} NfcRfidDetectorLfMeterState;

struct NfcRfidDetectorLfMeter {
    NfcRfidDetectorLfMeterState state;
    uint32_t settle[LF_METER_SETTLE];
    uint8_t settle_count;
    uint32_t steady_edges; // the settling gates' median, the reference for later gates
    uint32_t holdback[LF_METER_HOLDBACK];
    uint8_t holdback_count;
    uint8_t bad_gates; // bad gates in a row
    uint64_t edges; // counted into the result
    uint32_t gates; // 100 ms gates counted into the result
    uint32_t runs; // unbroken runs of gates in the result, each good to ±1 edge
    bool run_open; // the last gate counted continues a run
    bool held; // edges and gates are from a field that has gone
    uint32_t last_gate_edges;
};

static bool nfc_rfid_detector_lf_meter_is_carrier(uint32_t gate_edges) {
    return gate_edges >= LF_METER_GATE_MIN && gate_edges <= LF_METER_GATE_MAX;
}

static uint32_t nfc_rfid_detector_lf_meter_difference(uint32_t first, uint32_t second) {
    return first > second ? first - second : second - first;
}

// Median of the settling gates, or 0 if any is more than TOL edges off it (the field isn't steady yet)
static uint32_t nfc_rfid_detector_lf_meter_settled(const NfcRfidDetectorLfMeter* meter) {
    uint32_t sorted[LF_METER_SETTLE];
    memcpy(sorted, meter->settle, sizeof(sorted));
    for(int i = 1; i < LF_METER_SETTLE; i++) {
        for(int j = i; j > 0 && sorted[j - 1] > sorted[j]; j--) {
            uint32_t swap = sorted[j];
            sorted[j] = sorted[j - 1];
            sorted[j - 1] = swap;
        }
    }
    uint32_t median = sorted[LF_METER_SETTLE / 2];
    for(int i = 0; i < LF_METER_SETTLE; i++) {
        if(nfc_rfid_detector_lf_meter_difference(sorted[i], median) > LF_METER_TOL) return 0;
    }
    return median;
}

static void nfc_rfid_detector_lf_meter_clear_result(NfcRfidDetectorLfMeter* meter) {
    meter->edges = 0;
    meter->gates = 0;
    meter->runs = 0;
    meter->run_open = false;
    meter->held = false;
    meter->holdback_count = 0;
}

static void nfc_rfid_detector_lf_meter_lose(NfcRfidDetectorLfMeter* meter) {
    meter->held = meter->gates > 0;
    meter->holdback_count = 0;
    meter->settle_count = 0;
    meter->bad_gates = 0;
    meter->state = NfcRfidDetectorLfMeterStateIdle;
}

NfcRfidDetectorLfMeter* nfc_rfid_detector_lf_meter_alloc(void) {
    NfcRfidDetectorLfMeter* meter = malloc(sizeof(NfcRfidDetectorLfMeter));
    memset(meter, 0, sizeof(NfcRfidDetectorLfMeter));
    return meter;
}

void nfc_rfid_detector_lf_meter_free(NfcRfidDetectorLfMeter* meter) {
    furi_assert(meter);
    free(meter);
}

void nfc_rfid_detector_lf_meter_restart(NfcRfidDetectorLfMeter* meter) {
    furi_assert(meter);
    nfc_rfid_detector_lf_meter_clear_result(meter);
}

void nfc_rfid_detector_lf_meter_feed(NfcRfidDetectorLfMeter* meter, uint32_t gate_edges) {
    furi_assert(meter);
    meter->last_gate_edges = gate_edges;

    if(!nfc_rfid_detector_lf_meter_is_carrier(gate_edges)) {
        if(meter->state != NfcRfidDetectorLfMeterStateIdle) nfc_rfid_detector_lf_meter_lose(meter);
        return;
    }

    if(meter->state != NfcRfidDetectorLfMeterStateMeasuring) {
        meter->state = NfcRfidDetectorLfMeterStateSettling;
        // drop the oldest settling gate
        if(meter->settle_count == LF_METER_SETTLE) {
            memmove(
                meter->settle,
                meter->settle + 1,
                sizeof(meter->settle[0]) * (LF_METER_SETTLE - 1));
            meter->settle_count--;
        }
        meter->settle[meter->settle_count++] = gate_edges;
        if(meter->settle_count == LF_METER_SETTLE) {
            uint32_t median = nfc_rfid_detector_lf_meter_settled(meter);
            if(median) {
                // a new steady field replaces any held result
                nfc_rfid_detector_lf_meter_clear_result(meter);
                meter->steady_edges = median;
                meter->bad_gates = 0;
                meter->state = NfcRfidDetectorLfMeterStateMeasuring;
            }
        }
        return;
    }

    if(nfc_rfid_detector_lf_meter_difference(gate_edges, meter->steady_edges) <= LF_METER_TOL) {
        meter->bad_gates = 0;
        if(meter->holdback_count == LF_METER_HOLDBACK) {
            meter->edges += meter->holdback[0];
            meter->gates++;
            if(!meter->run_open) {
                meter->runs++;
                meter->run_open = true;
            }
            memmove(
                meter->holdback,
                meter->holdback + 1,
                sizeof(meter->holdback[0]) * (LF_METER_HOLDBACK - 1));
            meter->holdback_count--;
        }
        meter->holdback[meter->holdback_count++] = gate_edges;
    } else {
        // Also drop the gates just before a bad one. The field is going, and they may already be weak.
        meter->holdback_count = 0;
        meter->run_open = false;
        if(++meter->bad_gates >= LF_METER_LOST) nfc_rfid_detector_lf_meter_lose(meter);
    }
}

void nfc_rfid_detector_lf_meter_hold(NfcRfidDetectorLfMeter* meter) {
    furi_assert(meter);
    nfc_rfid_detector_lf_meter_lose(meter);
}

void nfc_rfid_detector_lf_meter_get_result(
    const NfcRfidDetectorLfMeter* meter,
    NfcRfidDetectorLfMeterResult* result) {
    furi_assert(meter);
    furi_assert(result);
    result->present = meter->state != NfcRfidDetectorLfMeterStateIdle;
    result->settling = meter->state == NfcRfidDetectorLfMeterStateSettling;
    result->held = meter->held;
    result->last_gate_edges = meter->last_gate_edges;
    result->edges = meter->edges;
    result->gates = meter->gates;
    result->runs = meter->runs;
}
