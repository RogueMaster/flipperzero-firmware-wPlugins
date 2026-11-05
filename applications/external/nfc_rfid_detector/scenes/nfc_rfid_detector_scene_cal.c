#include "../nfc_rfid_detector_app_i.h"
#include "../views/nfc_rfid_detector_view_cal.h"

// Set the crystal correction by hand.

static void nfc_rfid_detector_scene_cal_callback(NfcRfidDetectorCustomEvent event, void* context) {
    NfcRfidDetectorApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, event);
}

void nfc_rfid_detector_scene_cal_on_enter(void* context) {
    NfcRfidDetectorApp* app = context;
    nfc_rfid_detector_view_cal_set_callback(
        app->nfc_rfid_detector_cal, nfc_rfid_detector_scene_cal_callback, app);
    nfc_rfid_detector_view_cal_set(app->nfc_rfid_detector_cal, app->lf_cal);
    view_dispatcher_switch_to_view(app->view_dispatcher, NfcRfidDetectorViewCal);
}

bool nfc_rfid_detector_scene_cal_on_event(void* context, SceneManagerEvent event) {
    NfcRfidDetectorApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom &&
       event.event == NfcRfidDetectorCustomEventCalSessionStart) {
        scene_manager_next_scene(app->scene_manager, NfcRfidDetectorSceneCalSession);
        consumed = true;
    } else if(
        event.type == SceneManagerEventTypeCustom &&
        event.event == NfcRfidDetectorCustomEventCalSave) {
        int16_t cal = nfc_rfid_detector_view_cal_get(app->nfc_rfid_detector_cal);
        int16_t previous = app->lf_cal;
        int16_t previous_sig = app->lf_cal_sig;
        // a value set by hand has no known ±
        if(cal != app->lf_cal) app->lf_cal_sig = -1;
        app->lf_cal = cal;
        if(nfc_rfid_detector_app_lf_cal_save(app)) {
            nfc_rfid_detector_cal_cli_set_calibration(app->cal_cli, app->lf_cal);
            scene_manager_previous_scene(app->scene_manager);
        } else {
            // Stay on the screen and play the error alert. Going back would look like the result was saved.
            app->lf_cal = previous;
            app->lf_cal_sig = previous_sig;
            notification_message(app->notifications, &sequence_error);
        }
        consumed = true;
    }

    return consumed;
}

void nfc_rfid_detector_scene_cal_on_exit(void* context) {
    UNUSED(context);
}
