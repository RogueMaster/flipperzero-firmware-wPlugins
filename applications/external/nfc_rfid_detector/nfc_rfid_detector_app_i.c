#include "nfc_rfid_detector_app_i.h"

#include <furi.h>
#include <storage/storage.h>
#include <stdlib.h>
#include <stdio.h>

#define TAG "NfcRfidDetector"

void nfc_rfid_detector_app_field_presence_start(NfcRfidDetectorApp* app) {
    furi_assert(app);

    // start the field presence rfid detection
    furi_hal_rfid_field_detect_start();
    nfc_rfid_detector_lf_count_start(app->lf_count);

    // start the field presence nfc detection
    furi_hal_nfc_acquire();
    furi_hal_nfc_field_detect_start();
}

void nfc_rfid_detector_app_field_presence_stop(NfcRfidDetectorApp* app) {
    furi_assert(app);

    // stop the field presence rfid detection
    furi_hal_rfid_field_detect_stop();

    // stop the field presence nfc detection
    furi_hal_nfc_field_detect_stop();
    furi_hal_nfc_release();
}

bool nfc_rfid_detector_app_field_presence_is_nfc(NfcRfidDetectorApp* app) {
    furi_assert(app);

    // check if the field presence is nfc
    bool is_present = furi_hal_nfc_field_is_present();

    return is_present;
}

bool nfc_rfid_detector_app_field_presence_is_rfid(
    NfcRfidDetectorApp* app,
    NfcRfidDetectorLfMeterResult* result) {
    furi_assert(app);
    furi_assert(result);

    nfc_rfid_detector_lf_count_read(app->lf_count, result);
    return result->present;
}

void nfc_rfid_detector_app_field_presence_rfid_restart(NfcRfidDetectorApp* app) {
    furi_assert(app);
    nfc_rfid_detector_lf_count_restart(app->lf_count);
}

#define LF_CAL_PATH APP_DATA_PATH("lf_cal_ppm10.txt")

void nfc_rfid_detector_app_lf_cal_load(NfcRfidDetectorApp* app) {
    furi_assert(app);
    app->lf_cal = 0;
    app->lf_cal_sig = -1;
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, LF_CAL_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        char buf[16] = {0};
        storage_file_read(file, buf, sizeof(buf) - 1);
        // the value, then optionally its ± (a value set by hand has none)
        int saved = 0;
        int saved_sig = -1;
        int fields = sscanf(buf, "%d %d", &saved, &saved_sig);
        if(fields >= 1 && saved >= -NFC_RFID_DETECTOR_CAL_LIMIT &&
           saved <= NFC_RFID_DETECTOR_CAL_LIMIT) {
            app->lf_cal = (int16_t)saved;
            if(fields == 2 && saved_sig >= 0 && saved_sig <= NFC_RFID_DETECTOR_CAL_LIMIT) {
                app->lf_cal_sig = (int16_t)saved_sig;
            }
        }
    }
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

bool nfc_rfid_detector_app_lf_cal_save(NfcRfidDetectorApp* app) {
    furi_assert(app);
    bool saved = false;
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, LF_CAL_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        char buf[16];
        int length = app->lf_cal_sig >= 0 ?
                         snprintf(buf, sizeof(buf), "%d %d\n", app->lf_cal, app->lf_cal_sig) :
                         snprintf(buf, sizeof(buf), "%d\n", app->lf_cal);
        saved = storage_file_write(file, buf, length) == (size_t)length;
    }
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return saved;
}
