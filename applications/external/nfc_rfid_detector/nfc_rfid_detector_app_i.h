#pragma once

#include "helpers/nfc_rfid_detector_types.h"
#include "helpers/nfc_rfid_detector_event.h"
#include "helpers/nfc_rfid_detector_lf_count.h"

#include "scenes/nfc_rfid_detector_scene.h"
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/submenu.h>
#include <gui/modules/widget.h>
#include <notification/notification_messages.h>
#include "views/nfc_rfid_detector_view_field_presence.h"
#include "views/nfc_rfid_detector_view_cal.h"
#include "views/nfc_rfid_detector_view_cal_status.h"
#include "helpers/nfc_rfid_detector_cal_cli.h"

typedef struct NfcRfidDetectorApp NfcRfidDetectorApp;

struct NfcRfidDetectorApp {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;
    NotificationApp* notifications;
    Submenu* submenu;
    Widget* widget;
    NfcRfidDetectorFieldPresence* nfc_rfid_detector_field_presence;
    NfcRfidDetectorCal* nfc_rfid_detector_cal;
    NfcRfidDetectorLfCount* lf_count;
    int16_t lf_cal; // Flipper crystal correction, 0.1 ppm units (+ = its crystal runs fast)
    int16_t lf_cal_sig; // its ±, 0.1 ppm (< 0: not known, e.g. set by hand)
    NfcRfidDetectorCalCli* cal_cli;
    bool cal_saved; // this session's result was saved
    NfcRfidDetectorCalStatus* nfc_rfid_detector_cal_status;
};

void nfc_rfid_detector_app_field_presence_start(NfcRfidDetectorApp* app);
void nfc_rfid_detector_app_field_presence_stop(NfcRfidDetectorApp* app);
bool nfc_rfid_detector_app_field_presence_is_nfc(NfcRfidDetectorApp* app);
bool nfc_rfid_detector_app_field_presence_is_rfid(
    NfcRfidDetectorApp* app,
    NfcRfidDetectorLfMeterResult* result);
void nfc_rfid_detector_app_field_presence_rfid_restart(NfcRfidDetectorApp* app);
void nfc_rfid_detector_app_lf_cal_load(NfcRfidDetectorApp* app);
bool nfc_rfid_detector_app_lf_cal_save(NfcRfidDetectorApp* app);
