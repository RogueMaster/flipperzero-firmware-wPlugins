#include "../nfc_rfid_detector_app_i.h"

// A computer is measuring the crystal: show its progress, then save the result.

static void
    nfc_rfid_detector_scene_cal_session_callback(NfcRfidDetectorCustomEvent event, void* context) {
    NfcRfidDetectorApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, event);
}

void nfc_rfid_detector_scene_cal_session_on_enter(void* context) {
    NfcRfidDetectorApp* app = context;
    nfc_rfid_detector_view_cal_status_set_callback(
        app->nfc_rfid_detector_cal_status, nfc_rfid_detector_scene_cal_session_callback, app);
    nfc_rfid_detector_view_cal_status_set_state(
        app->nfc_rfid_detector_cal_status, NfcRfidDetectorCalStatusMeasuring);
    app->cal_saved = false;
    view_dispatcher_switch_to_view(app->view_dispatcher, NfcRfidDetectorViewCalStatus);
}

bool nfc_rfid_detector_scene_cal_session_on_event(void* context, SceneManagerEvent event) {
    NfcRfidDetectorApp* app = context;
    NfcRfidDetectorCalStatus* status = app->nfc_rfid_detector_cal_status;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeTick) {
        nfc_rfid_detector_view_cal_status_redraw(status);
        return false;
    }
    if(event.type != SceneManagerEventTypeCustom) return false;
    switch(event.event) {
    case NfcRfidDetectorCustomEventCalSessionStart:
        nfc_rfid_detector_view_cal_status_set_state(status, NfcRfidDetectorCalStatusMeasuring);
        app->cal_saved = false;
        consumed = true;
        break;
    case NfcRfidDetectorCustomEventCalProgress: {
        NfcRfidDetectorCalCliProgress progress;
        nfc_rfid_detector_cal_cli_get_progress(app->cal_cli, &progress);
        nfc_rfid_detector_view_cal_status_set_progress(
            status, progress.elapsed, progress.total, progress.estimate, progress.estimate_sig);
        consumed = true;
        break;
    }
    case NfcRfidDetectorCustomEventCalReceived: {
        int16_t previous = app->lf_cal;
        int16_t previous_sig = app->lf_cal_sig;
        nfc_rfid_detector_cal_cli_get_received(app->cal_cli, &app->lf_cal, &app->lf_cal_sig);
        if(nfc_rfid_detector_app_lf_cal_save(app)) {
            app->cal_saved = true;
            nfc_rfid_detector_cal_cli_set_calibration(app->cal_cli, app->lf_cal);
            nfc_rfid_detector_view_cal_status_set_saved(
                status, app->lf_cal, app->lf_cal_sig, previous);
            notification_message(app->notifications, &sequence_success);
            nfc_rfid_detector_cal_cli_set_save_result(app->cal_cli, true);
        } else {
            app->lf_cal = previous;
            app->lf_cal_sig = previous_sig;
            nfc_rfid_detector_cal_cli_set_save_result(app->cal_cli, false);
        }
        consumed = true;
        break;
    }
    case NfcRfidDetectorCustomEventCalSessionEnd:
        if(!app->cal_saved) {
            nfc_rfid_detector_view_cal_status_set_state(status, NfcRfidDetectorCalStatusEnded);
        }
        consumed = true;
        break;
    case NfcRfidDetectorCustomEventCalStatusCancel:
        // end the session without saving
        nfc_rfid_detector_cal_cli_cancel(app->cal_cli);
        scene_manager_previous_scene(app->scene_manager);
        consumed = true;
        break;
    case NfcRfidDetectorCustomEventCalStatusOk:
        scene_manager_search_and_switch_to_previous_scene(
            app->scene_manager, NfcRfidDetectorSceneFieldPresence);
        consumed = true;
        break;
    default:
        break;
    }

    return consumed;
}

void nfc_rfid_detector_scene_cal_session_on_exit(void* context) {
    UNUSED(context);
}
