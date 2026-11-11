#include "nfc_compare.h"

static bool nfc_compare_custom_event_callback(void* context, uint32_t event) {
    furi_assert(context);
    NfcCompareApp* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool nfc_compare_back_event_callback(void* context) {
    furi_assert(context);
    NfcCompareApp* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

static NfcCompareApp* nfc_compare_alloc(void) {
    NfcCompareApp* app = malloc(sizeof(NfcCompareApp));

    app->gui = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);
    app->dialogs = furi_record_open(RECORD_DIALOGS);

    app->file_path = furi_string_alloc();
    furi_string_set(app->file_path, NFC_COMPARE_NFC_FOLDER);

    app->view_dispatcher = view_dispatcher_alloc();
    app->scene_manager = scene_manager_alloc(&nfc_compare_scene_handlers, app);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, nfc_compare_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, nfc_compare_back_event_callback);
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    app->widget = widget_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, NfcCompareViewWidget, widget_get_view(app->widget));

    app->saved_device = nfc_device_alloc();
    app->nfc = NULL;
    app->poller = NULL;
    app->saved_uid_len = 0;
    app->read_uid_len = 0;
    app->match = false;

    scene_manager_next_scene(app->scene_manager, NfcCompareSceneFileSelect);

    return app;
}

static void nfc_compare_free(NfcCompareApp* app) {
    furi_assert(app);

    /* The read scene owns the poller/nfc lifecycle and frees them on exit.
     * Guard here in case of an abnormal teardown. */
    if(app->poller) {
        nfc_poller_stop(app->poller);
        nfc_poller_free(app->poller);
        app->poller = NULL;
    }
    if(app->nfc) {
        nfc_free(app->nfc);
        app->nfc = NULL;
    }

    view_dispatcher_remove_view(app->view_dispatcher, NfcCompareViewWidget);
    widget_free(app->widget);

    view_dispatcher_free(app->view_dispatcher);
    scene_manager_free(app->scene_manager);

    nfc_device_free(app->saved_device);
    furi_string_free(app->file_path);

    furi_record_close(RECORD_DIALOGS);
    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);

    free(app);
}

int32_t nfc_compare_app(void* p) {
    UNUSED(p);
    NfcCompareApp* app = nfc_compare_alloc();

    view_dispatcher_run(app->view_dispatcher);

    nfc_compare_free(app);
    return 0;
}
