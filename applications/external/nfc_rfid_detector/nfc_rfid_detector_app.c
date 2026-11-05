#include "nfc_rfid_detector_app_i.h"

#include <furi.h>
#include <furi_hal.h>
#include <string.h>

static bool nfc_rfid_detector_app_custom_event_callback(void* context, uint32_t event) {
    furi_assert(context);
    NfcRfidDetectorApp* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool nfc_rfid_detector_app_back_event_callback(void* context) {
    furi_assert(context);
    NfcRfidDetectorApp* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

static void nfc_rfid_detector_app_tick_event_callback(void* context) {
    furi_assert(context);
    NfcRfidDetectorApp* app = context;
    scene_manager_handle_tick_event(app->scene_manager);
}

NfcRfidDetectorApp* nfc_rfid_detector_app_alloc() {
    NfcRfidDetectorApp* app = malloc(sizeof(NfcRfidDetectorApp));

    // GUI
    app->gui = furi_record_open(RECORD_GUI);

    // View Dispatcher
    app->view_dispatcher = view_dispatcher_alloc();
    app->scene_manager = scene_manager_alloc(&nfc_rfid_detector_scene_handlers, app);

    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, nfc_rfid_detector_app_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, nfc_rfid_detector_app_back_event_callback);
    view_dispatcher_set_tick_event_callback(
        app->view_dispatcher, nfc_rfid_detector_app_tick_event_callback, 100);

    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    // Open Notification record
    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    // SubMenu
    app->submenu = submenu_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, NfcRfidDetectorViewSubmenu, submenu_get_view(app->submenu));

    // Widget
    app->widget = widget_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, NfcRfidDetectorViewWidget, widget_get_view(app->widget));

    // Field Presence
    app->nfc_rfid_detector_field_presence = nfc_rfid_detector_view_field_presence_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        NfcRfidDetectorViewFieldPresence,
        nfc_rfid_detector_view_field_presence_get_view(app->nfc_rfid_detector_field_presence));

    // Calibration
    app->nfc_rfid_detector_cal = nfc_rfid_detector_view_cal_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        NfcRfidDetectorViewCal,
        nfc_rfid_detector_view_cal_get_view(app->nfc_rfid_detector_cal));
    nfc_rfid_detector_app_lf_cal_load(app);

    // Calibration status
    app->nfc_rfid_detector_cal_status = nfc_rfid_detector_view_cal_status_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        NfcRfidDetectorViewCalStatus,
        nfc_rfid_detector_view_cal_status_get_view(app->nfc_rfid_detector_cal_status));
    app->lf_count = nfc_rfid_detector_lf_count_alloc();
    app->cal_cli = nfc_rfid_detector_cal_cli_alloc(app->view_dispatcher);
    nfc_rfid_detector_cal_cli_set_calibration(app->cal_cli, app->lf_cal);

    scene_manager_next_scene(app->scene_manager, NfcRfidDetectorSceneFieldPresence);

    return app;
}

void nfc_rfid_detector_app_free(NfcRfidDetectorApp* app) {
    furi_assert(app);

    nfc_rfid_detector_cal_cli_free(app->cal_cli);
    nfc_rfid_detector_lf_count_free(app->lf_count);

    // Submenu
    view_dispatcher_remove_view(app->view_dispatcher, NfcRfidDetectorViewSubmenu);
    submenu_free(app->submenu);

    //  Widget
    view_dispatcher_remove_view(app->view_dispatcher, NfcRfidDetectorViewWidget);
    widget_free(app->widget);

    // Field Presence
    view_dispatcher_remove_view(app->view_dispatcher, NfcRfidDetectorViewFieldPresence);
    nfc_rfid_detector_view_field_presence_free(app->nfc_rfid_detector_field_presence);

    // Calibration status
    view_dispatcher_remove_view(app->view_dispatcher, NfcRfidDetectorViewCalStatus);
    nfc_rfid_detector_view_cal_status_free(app->nfc_rfid_detector_cal_status);

    // Calibration
    view_dispatcher_remove_view(app->view_dispatcher, NfcRfidDetectorViewCal);
    nfc_rfid_detector_view_cal_free(app->nfc_rfid_detector_cal);

    // View dispatcher
    view_dispatcher_free(app->view_dispatcher);
    scene_manager_free(app->scene_manager);

    // Notifications
    furi_record_close(RECORD_NOTIFICATION);
    app->notifications = NULL;

    // Close records
    furi_record_close(RECORD_GUI);

    free(app);
}

int32_t nfc_rfid_detector_app(void* p) {
    NfcRfidDetectorApp* nfc_rfid_detector_app = nfc_rfid_detector_app_alloc();

    // `loader open <app> cal` opens Computer calibration directly, with Config and detection behind it
    if(p && strcmp(p, "cal") == 0) {
        scene_manager_next_scene(
            nfc_rfid_detector_app->scene_manager, NfcRfidDetectorSceneCalMenu);
        scene_manager_next_scene(
            nfc_rfid_detector_app->scene_manager, NfcRfidDetectorSceneCalHelp);
    }

    view_dispatcher_run(nfc_rfid_detector_app->view_dispatcher);
    // Stop detection before freeing the app. Until then its DMA keeps copying the field count into the LF count's
    // buffer. The detection scene's on_exit stops it. Back has already run on_exit, but `loader close` skips it.
    // scene_manager_stop() runs on_exit for a scene that is still open, and does nothing after Back.
    scene_manager_stop(nfc_rfid_detector_app->scene_manager);

    nfc_rfid_detector_app_free(nfc_rfid_detector_app);

    return 0;
}
