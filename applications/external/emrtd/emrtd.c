/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Filip Sedivy
 *
 * The application object: allocation, teardown and the handful of helpers
 * every scene shares.
 */
#include "emrtd_i.h"

#include <stdarg.h>

#define TAG "eMRTD"

/*
 * The dispatcher callbacks do nothing but hand the event to the scene
 * manager. Keeping them empty of logic is the firmware's convention: the
 * scene on top of the stack is the only place that knows what an event means.
 */
static bool emrtd_custom_event_callback(void* context, uint32_t event) {
    furi_assert(context);
    Emrtd* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool emrtd_back_event_callback(void* context) {
    furi_assert(context);
    Emrtd* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

static void emrtd_tick_event_callback(void* context) {
    furi_assert(context);
    Emrtd* app = context;
    scene_manager_handle_tick_event(app->scene_manager);
}

void emrtd_text_store_set(Emrtd* app, const char* format, ...) {
    furi_assert(app);

    va_list args;
    va_start(args, format);
    vsnprintf(app->text_store, sizeof(app->text_store), format, args);
    va_end(args);
}

void emrtd_text_store_clear(Emrtd* app) {
    furi_assert(app);
    memset(app->text_store, 0, sizeof(app->text_store));
}

/*
 * Blue while the field is on. A passport read takes several seconds, and the
 * only way the user knows the document has to stay still is the light.
 * message_do_not_reset keeps the blink alive when another notification is
 * played over it.
 */
static const NotificationSequence emrtd_sequence_blink_blue = {
    &message_blink_start_10,
    &message_blink_set_color_blue,
    &message_do_not_reset,
    NULL,
};

static const NotificationSequence emrtd_sequence_blink_stop = {
    &message_blink_stop,
    NULL,
};

void emrtd_blink_start(Emrtd* app) {
    furi_assert(app);
    notification_message(app->notifications, &emrtd_sequence_blink_blue);
}

void emrtd_blink_stop(Emrtd* app) {
    furi_assert(app);
    notification_message(app->notifications, &emrtd_sequence_blink_stop);
}

void emrtd_notify_success(Emrtd* app) {
    furi_assert(app);
    notification_message(app->notifications, &sequence_success);
}

void emrtd_notify_error(Emrtd* app) {
    furi_assert(app);
    notification_message(app->notifications, &sequence_error);
}

static Emrtd* emrtd_alloc(void) {
    Emrtd* app = malloc(sizeof(Emrtd));
    memset(app, 0, sizeof(Emrtd));

    app->gui = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);
    app->storage = furi_record_open(RECORD_STORAGE);
    app->dialogs = furi_record_open(RECORD_DIALOGS);

    app->view_dispatcher = view_dispatcher_alloc();
    app->scene_manager = scene_manager_alloc(&emrtd_scene_handlers, app);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, emrtd_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, emrtd_back_event_callback);
    view_dispatcher_set_tick_event_callback(app->view_dispatcher, emrtd_tick_event_callback, 250);
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    app->submenu = submenu_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, EmrtdViewSubmenu, submenu_get_view(app->submenu));

    app->variable_item_list = variable_item_list_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        EmrtdViewVariableItemList,
        variable_item_list_get_view(app->variable_item_list));

    app->popup = popup_alloc();
    view_dispatcher_add_view(app->view_dispatcher, EmrtdViewPopup, popup_get_view(app->popup));

    app->text_input = text_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, EmrtdViewTextInput, text_input_get_view(app->text_input));

    app->number_input = number_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, EmrtdViewNumberInput, number_input_get_view(app->number_input));

    app->text_box = text_box_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, EmrtdViewTextBox, text_box_get_view(app->text_box));

    app->widget = widget_alloc();
    view_dispatcher_add_view(app->view_dispatcher, EmrtdViewWidget, widget_get_view(app->widget));

    app->date_input = emrtd_date_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, EmrtdViewDateInput, emrtd_date_input_get_view(app->date_input));

    app->read_view = emrtd_read_view_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, EmrtdViewRead, emrtd_read_view_get_view(app->read_view));

    app->text_box_store = furi_string_alloc();
    app->file_path = furi_string_alloc_set_str(EMRTD_EXPORT_DIR);

    /*
     * Defaults for a first run. Everything but the EAC protected groups is
     * attempted, the access driver is chosen from EF.CardAccess, and the read
     * is written to the SD card, which is the only way anything survives the
     * app being closed. The APDU trace is off because it is a diagnostic tool
     * and it records the document's own responses in the clear.
     *
     * The credentials are kept on the card, because that is the only place
     * they survive the application being closed and typing a document number
     * and two dates before every read is the thing that makes a reader
     * unusable. They are the key to someone's identity document, so the way
     * out is deliberate and it is stated in two places: Remember on SD in
     * Options turns the storing off, and Document - Forget stored data
     * removes what is already there. See docs/security.md.
     */
    app->config.method = EmrtdAccessMethodAuto;
    app->config.files = emrtd_file_default_mask();
    app->config.export_to_sd = true;
    app->config.write_trace = false;
    app->remember_credentials = true;

    emrtd_settings_load(app);

    return app;
}

static void emrtd_free(Emrtd* app) {
    furi_assert(app);

    /*
     * Back out of the start scene empties the stack before the dispatcher
     * stops, and then this does nothing. A signal from outside - loader close,
     * which ufbt launch sends before every upload - stops the dispatcher with
     * a scene still on top, and its on_exit would never run: the LED would
     * keep blinking, the backlight Donate holds on would stay held for the
     * whole device, and the keys screen would skip its own wipe, which
     * matters only on a build whose allocator does not clear. It has to run
     * first, while every view it resets still exists.
     */
    scene_manager_stop(app->scene_manager);

    /*
     * A scene normally tears the read down in its on_exit handler. Doing it
     * again here costs nothing and guarantees that neither the poller thread
     * nor the NFC hardware lock outlives the application, whichever way the
     * dispatcher was stopped.
     */
    if(app->worker) {
        emrtd_worker_stop(app->worker);
        emrtd_worker_free(app->worker);
        app->worker = NULL;
    }
    if(app->nfc) {
        nfc_free(app->nfc);
        app->nfc = NULL;
    }

    view_dispatcher_remove_view(app->view_dispatcher, EmrtdViewRead);
    emrtd_read_view_free(app->read_view);

    view_dispatcher_remove_view(app->view_dispatcher, EmrtdViewDateInput);
    emrtd_date_input_free(app->date_input);

    view_dispatcher_remove_view(app->view_dispatcher, EmrtdViewWidget);
    widget_free(app->widget);

    view_dispatcher_remove_view(app->view_dispatcher, EmrtdViewTextBox);
    text_box_free(app->text_box);

    view_dispatcher_remove_view(app->view_dispatcher, EmrtdViewNumberInput);
    number_input_free(app->number_input);

    view_dispatcher_remove_view(app->view_dispatcher, EmrtdViewTextInput);
    text_input_free(app->text_input);

    view_dispatcher_remove_view(app->view_dispatcher, EmrtdViewPopup);
    popup_free(app->popup);

    view_dispatcher_remove_view(app->view_dispatcher, EmrtdViewVariableItemList);
    variable_item_list_free(app->variable_item_list);

    view_dispatcher_remove_view(app->view_dispatcher, EmrtdViewSubmenu);
    submenu_free(app->submenu);

    view_dispatcher_free(app->view_dispatcher);
    scene_manager_free(app->scene_manager);

    furi_string_free(app->text_box_store);
    furi_string_free(app->file_path);

    furi_record_close(RECORD_DIALOGS);
    app->dialogs = NULL;
    furi_record_close(RECORD_STORAGE);
    app->storage = NULL;
    furi_record_close(RECORD_NOTIFICATION);
    app->notifications = NULL;
    furi_record_close(RECORD_GUI);
    app->gui = NULL;

    /*
     * The credentials open someone's identity document and the result holds
     * their name and date of birth. free() below clears the block on every
     * firmware this is built for (docs/platform.md, item 23). These wipes
     * make sure this struct's copy also goes on a build that does not clear
     * it. The copies the result screens made are already freed by then.
     */
    emrtd_secure_wipe(&app->config.credentials, sizeof(app->config.credentials));
    emrtd_secure_wipe(&app->result, sizeof(app->result));
    emrtd_secure_wipe(app->text_store, sizeof(app->text_store));

    free(app);
}

int32_t emrtd_app(void* p) {
    UNUSED(p);

    Emrtd* app = emrtd_alloc();

    scene_manager_next_scene(app->scene_manager, EmrtdSceneStart);
    view_dispatcher_run(app->view_dispatcher);

    emrtd_free(app);

    return 0;
}
