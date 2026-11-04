#include "sniffer_card_app.h"

static bool sniffer_card_app_custom_event_callback(void* context, uint32_t event) {
    furi_assert(context);
    SnifferCardApp* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool sniffer_card_app_back_event_callback(void* context) {
    furi_assert(context);
    SnifferCardApp* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

void sniffer_card_app_take_result(SnifferCardApp* app) {
    furi_assert(app);

    const SnifferTrace* traces = NULL;
    const size_t count = sniffer_card_poller_get_traces(app->poller, &traces);

    furi_check(count <= COUNT_OF(app->traces));
    memcpy(app->traces, traces, count * sizeof(SnifferTrace));
    app->trace_count = count;
    app->read_truncated = sniffer_card_poller_read_was_truncated(app->poller);
    app->result = sniffer_card_poller_get_result(app->poller);
}

void sniffer_card_app_run_mfkey(SnifferCardApp* app) {
    furi_assert(app);

    Loader* loader = furi_record_open(RECORD_LOADER);
    loader_enqueue_launch(loader, SNIFFER_CARD_MFKEY_PATH, NULL, LoaderDeferredLaunchFlagGui);
    furi_record_close(RECORD_LOADER);

    // The loader starts the queued app once this one is gone, so leaving is part of launching.
    view_dispatcher_stop(app->view_dispatcher);
}

void sniffer_card_app_uid_text(const uint8_t* uid, size_t uid_len, FuriString* text) {
    furi_assert(uid);
    furi_assert(text);

    furi_string_reset(text);
    for(size_t i = 0; i < uid_len; i++) {
        furi_string_cat_printf(text, i ? " %02X" : "%02X", uid[i]);
    }
}

static SnifferCardApp* sniffer_card_app_alloc(void) {
    SnifferCardApp* app = malloc(sizeof(SnifferCardApp));

    app->gui = furi_record_open(RECORD_GUI);
    app->dialogs = furi_record_open(RECORD_DIALOGS);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    app->scene_manager = scene_manager_alloc(&sniffer_card_app_scene_handlers, app);
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, sniffer_card_app_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, sniffer_card_app_back_event_callback);
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    app->byte_input = byte_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, SnifferCardAppViewByteInput, byte_input_get_view(app->byte_input));

    app->popup = popup_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, SnifferCardAppViewPopup, popup_get_view(app->popup));

    app->submenu = submenu_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, SnifferCardAppViewSubmenu, submenu_get_view(app->submenu));

    app->widget = widget_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, SnifferCardAppViewWidget, widget_get_view(app->widget));

    app->nfc = nfc_alloc();
    app->poller = sniffer_card_poller_alloc(app->nfc);

    app->result = SnifferCardPollerResultWaiting;
    app->trace_count = 0;
    app->read_truncated = false;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    app->mfkey_installed = storage_common_exists(storage, SNIFFER_CARD_MFKEY_PATH);
    furi_record_close(RECORD_STORAGE);
    memset(&app->target, 0, sizeof(app->target));
    memset(app->uid_input, 0, sizeof(app->uid_input));
    app->uid_input_len = 0;
    app->text = furi_string_alloc();

    return app;
}

static void sniffer_card_app_free(SnifferCardApp* app) {
    sniffer_card_poller_free(app->poller);
    nfc_free(app->nfc);

    view_dispatcher_remove_view(app->view_dispatcher, SnifferCardAppViewByteInput);
    view_dispatcher_remove_view(app->view_dispatcher, SnifferCardAppViewPopup);
    view_dispatcher_remove_view(app->view_dispatcher, SnifferCardAppViewSubmenu);
    view_dispatcher_remove_view(app->view_dispatcher, SnifferCardAppViewWidget);

    byte_input_free(app->byte_input);
    popup_free(app->popup);
    submenu_free(app->submenu);
    widget_free(app->widget);

    view_dispatcher_free(app->view_dispatcher);
    scene_manager_free(app->scene_manager);

    furi_string_free(app->text);

    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_DIALOGS);
    furi_record_close(RECORD_GUI);

    free(app);
}

int32_t sniffer_card_app(void* p) {
    UNUSED(p);

    SnifferCardApp* app = sniffer_card_app_alloc();

    scene_manager_next_scene(app->scene_manager, SnifferCardAppSceneStart);

    view_dispatcher_run(app->view_dispatcher);

    sniffer_card_app_free(app);

    return 0;
}
