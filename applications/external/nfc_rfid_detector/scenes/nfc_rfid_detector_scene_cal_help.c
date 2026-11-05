#include "../nfc_rfid_detector_app_i.h"

#include <nfc_rfid_detector_icons.h>

// How to calibrate with a computer. The QR code links to the browser-based calibration page.

void nfc_rfid_detector_scene_cal_help_on_enter(void* context) {
    NfcRfidDetectorApp* app = context;
    Widget* widget = app->widget;

    widget_add_string_element(
        widget, 0, 0, AlignLeft, AlignTop, FontPrimary, "Computer calibration");
    widget_add_text_box_element(
        widget,
        0,
        13,
        92,
        51,
        AlignLeft,
        AlignTop,
        "Connect USB and\nkeep this screen\nopen. Then open\nthe link in Chrome\nor Edge:",
        false);
    widget_add_icon_element(widget, 84, 20, &I_Cal_link_qr_42x42);
    view_dispatcher_switch_to_view(app->view_dispatcher, NfcRfidDetectorViewWidget);
}

bool nfc_rfid_detector_scene_cal_help_on_event(void* context, SceneManagerEvent event) {
    NfcRfidDetectorApp* app = context;
    bool consumed = false;

    // a computer started a session
    if(event.type == SceneManagerEventTypeCustom &&
       event.event == NfcRfidDetectorCustomEventCalSessionStart) {
        scene_manager_next_scene(app->scene_manager, NfcRfidDetectorSceneCalSession);
        consumed = true;
    }

    return consumed;
}

void nfc_rfid_detector_scene_cal_help_on_exit(void* context) {
    NfcRfidDetectorApp* app = context;
    widget_reset(app->widget);
}
