#include "dfc_i.h"

#define TAG "Dfc"

_Static_assert(
    DFC_BUILD_PROFILE == DFC_PROFILE_FULL_EV1,
    "The Flipper app must be built with the full EV1 profile");

bool dfc_custom_event_callback(void* context, uint32_t event) {
    furi_assert(context);
    Dfc* dfc = context;
    return scene_manager_handle_custom_event(dfc->scene_manager, event);
}

bool dfc_back_event_callback(void* context) {
    furi_assert(context);
    Dfc* dfc = context;
    return scene_manager_handle_back_event(dfc->scene_manager);
}

void dfc_tick_event_callback(void* context) {
    furi_assert(context);
    Dfc* dfc = context;
    scene_manager_handle_tick_event(dfc->scene_manager);
}

Dfc* dfc_alloc() {
    Dfc* dfc = malloc(sizeof(Dfc));
    memset(dfc, 0, sizeof(Dfc));

    dfc->view_dispatcher = view_dispatcher_alloc();
    dfc->scene_manager = scene_manager_alloc(&dfc_scene_handlers, dfc);
    view_dispatcher_set_event_callback_context(dfc->view_dispatcher, dfc);
    view_dispatcher_set_custom_event_callback(dfc->view_dispatcher, dfc_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(dfc->view_dispatcher, dfc_back_event_callback);
    view_dispatcher_set_tick_event_callback(dfc->view_dispatcher, dfc_tick_event_callback, 100);

    dfc->nfc = nfc_alloc();

    // Nfc device
    dfc->nfc_device = nfc_device_alloc();
    nfc_device_set_loading_callback(dfc->nfc_device, dfc_show_loading_popup, dfc);

    // Open GUI record
    dfc->gui = furi_record_open(RECORD_GUI);
    view_dispatcher_attach_to_gui(dfc->view_dispatcher, dfc->gui, ViewDispatcherTypeFullscreen);

    // Open Notification record
    dfc->notifications = furi_record_open(RECORD_NOTIFICATION);

    // Submenu
    dfc->submenu = submenu_alloc();
    view_dispatcher_add_view(dfc->view_dispatcher, DfcViewMenu, submenu_get_view(dfc->submenu));

    // Popup
    dfc->popup = popup_alloc();
    view_dispatcher_add_view(dfc->view_dispatcher, DfcViewPopup, popup_get_view(dfc->popup));

    // Loading
    dfc->loading = loading_alloc();
    view_dispatcher_add_view(dfc->view_dispatcher, DfcViewLoading, loading_get_view(dfc->loading));

    // Text Input
    dfc->text_input = text_input_alloc();
    view_dispatcher_add_view(
        dfc->view_dispatcher, DfcViewTextInput, text_input_get_view(dfc->text_input));

    // TextBox
    dfc->text_box = text_box_alloc();
    view_dispatcher_add_view(
        dfc->view_dispatcher, DfcViewTextBox, text_box_get_view(dfc->text_box));
    dfc->text_box_store = furi_string_alloc();

    // Custom Widget
    dfc->widget = widget_alloc();
    view_dispatcher_add_view(dfc->view_dispatcher, DfcViewWidget, widget_get_view(dfc->widget));

    dfc_credential_storage_init();
    dfc->credential = dfc_credential_alloc();

    // File Browser
    dfc->file_browser = file_browser_alloc(dfc_credential_storage_load_path());
    view_dispatcher_add_view(
        dfc->view_dispatcher, DfcViewFileBrowser, file_browser_get_view(dfc->file_browser));

    return dfc;
}

void dfc_free(Dfc* dfc) {
    furi_assert(dfc);

    if(dfc->listener) {
        nfc_listener_stop(dfc->listener);
        nfc_listener_free(dfc->listener);
        dfc->listener = NULL;
    }

    if(dfc->poller) {
        nfc_poller_stop(dfc->poller);
        nfc_poller_free(dfc->poller);
        dfc->poller = NULL;
    }

    nfc_free(dfc->nfc);

    // Nfc device
    nfc_device_free(dfc->nfc_device);

    // Submenu
    view_dispatcher_remove_view(dfc->view_dispatcher, DfcViewMenu);
    submenu_free(dfc->submenu);

    // Popup
    view_dispatcher_remove_view(dfc->view_dispatcher, DfcViewPopup);
    popup_free(dfc->popup);

    // Loading
    view_dispatcher_remove_view(dfc->view_dispatcher, DfcViewLoading);
    loading_free(dfc->loading);

    // TextInput
    view_dispatcher_remove_view(dfc->view_dispatcher, DfcViewTextInput);
    text_input_free(dfc->text_input);

    // TextBox
    view_dispatcher_remove_view(dfc->view_dispatcher, DfcViewTextBox);
    text_box_free(dfc->text_box);
    furi_string_free(dfc->text_box_store);

    // Custom Widget
    view_dispatcher_remove_view(dfc->view_dispatcher, DfcViewWidget);
    widget_free(dfc->widget);

    // File Browser
    view_dispatcher_remove_view(dfc->view_dispatcher, DfcViewFileBrowser);
    file_browser_free(dfc->file_browser);

    // View Dispatcher
    view_dispatcher_free(dfc->view_dispatcher);

    // Scene Manager
    scene_manager_free(dfc->scene_manager);

    // GUI
    furi_record_close(RECORD_GUI);
    dfc->gui = NULL;

    // Notifications
    furi_record_close(RECORD_NOTIFICATION);
    dfc->notifications = NULL;

    dfc_credential_free(dfc->credential);
    dfc_credential_storage_deinit();

    if(dfc->dfc_emulator) {
        dfc_emulator_free(dfc->dfc_emulator);
        dfc->dfc_emulator = NULL;
    }

    free(dfc);
}

void dfc_text_store_set(Dfc* dfc, const char* text, ...) {
    va_list args;
    va_start(args, text);

    vsnprintf(dfc->text_store, sizeof(dfc->text_store), text, args);

    va_end(args);
}

void dfc_text_store_clear(Dfc* dfc) {
    memset(dfc->text_store, 0, sizeof(dfc->text_store));
}

static const NotificationSequence dfc_sequence_blink_start_blue = {
    &message_blink_start_10,
    &message_blink_set_color_blue,
    &message_do_not_reset,
    NULL,
};

static const NotificationSequence dfc_sequence_blink_stop = {
    &message_blink_stop,
    NULL,
};

void dfc_blink_start(Dfc* dfc) {
    notification_message(dfc->notifications, &dfc_sequence_blink_start_blue);
}

void dfc_blink_stop(Dfc* dfc) {
    notification_message(dfc->notifications, &dfc_sequence_blink_stop);
}

void dfc_show_loading_popup(void* context, bool show) {
    Dfc* dfc = context;

    if(show) {
        // Raise timer priority so that animations can play
        furi_timer_set_thread_priority(FuriTimerThreadPriorityElevated);
        view_dispatcher_switch_to_view(dfc->view_dispatcher, DfcViewLoading);
    } else {
        // Restore default timer priority
        furi_timer_set_thread_priority(FuriTimerThreadPriorityNormal);
    }
}

int32_t dfc_app(void* p) {
    UNUSED(p);
    Dfc* dfc = dfc_alloc();

    scene_manager_next_scene(dfc->scene_manager, DfcSceneMainMenu);

    view_dispatcher_run(dfc->view_dispatcher);

    dfc_free(dfc);

    return 0;
}
