#pragma once

#include <gui/view_dispatcher.h>
#include <stdbool.h>
#include <stdint.h>

// `rfid_detector_cal` CLI command, for calibrating the crystal from a computer (tools/).
// The command shares the field counter's timer (TIM2), runs only while detection is paused,
// and reports to the app through custom events (helpers/nfc_rfid_detector_event.h).
typedef struct NfcRfidDetectorCalCli NfcRfidDetectorCalCli;

typedef struct {
    uint16_t elapsed; // s
    uint16_t total; // s
    int16_t estimate; // running estimate, 0.1 ppm
    int16_t estimate_sig; // its ±, 0.1 ppm (< 0: none yet)
} NfcRfidDetectorCalCliProgress;

// Registers the command
NfcRfidDetectorCalCli* nfc_rfid_detector_cal_cli_alloc(ViewDispatcher* view_dispatcher);

// Deletes the command and waits for any call to return
void nfc_rfid_detector_cal_cli_free(NfcRfidDetectorCalCli* cli);

// false ends a running session and waits for it to release the timer
void nfc_rfid_detector_cal_cli_allow(NfcRfidDetectorCalCli* cli, bool allow);

// End a running session without saving
void nfc_rfid_detector_cal_cli_cancel(NfcRfidDetectorCalCli* cli);

// The saved calibration, which a session reports when it starts, 0.1 ppm
void nfc_rfid_detector_cal_cli_set_calibration(NfcRfidDetectorCalCli* cli, int16_t value);

// After NfcRfidDetectorCustomEventCalProgress
void nfc_rfid_detector_cal_cli_get_progress(
    NfcRfidDetectorCalCli* cli,
    NfcRfidDetectorCalCliProgress* progress);

// After NfcRfidDetectorCustomEventCalReceived: the result from the computer and its ± (< 0: not sent),
// 0.1 ppm. Report whether the result was saved with nfc_rfid_detector_cal_cli_set_save_result().
void nfc_rfid_detector_cal_cli_get_received(
    NfcRfidDetectorCalCli* cli,
    int16_t* value,
    int16_t* sig);

void nfc_rfid_detector_cal_cli_set_save_result(NfcRfidDetectorCalCli* cli, bool saved);
