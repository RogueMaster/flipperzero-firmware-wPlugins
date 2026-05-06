#include "../include/nfc_tools_i.h"

static bool nfc_tools_custom_event_callback(void* context, uint32_t event) {
    furi_assert(context);
    NfcToolsApp* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool nfc_tools_back_event_callback(void* context) {
    furi_assert(context);
    NfcToolsApp* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

static NfcToolsApp* nfc_tools_app_alloc(void) {
    NfcToolsApp* app = malloc(sizeof(NfcToolsApp));

    app->gui = furi_record_open(RECORD_GUI);
    app->info_str = furi_string_alloc();
    app->ndef_str = furi_string_alloc();
    app->worker_thread = NULL;
    app->worker_flags = NULL;
    app->detected_protocol = NfcProtocolInvalid;
    app->uid_len = 0;
    app->scan_destination = NfcToolsSceneIdTagInfo;

    app->view_dispatcher = view_dispatcher_alloc();
    app->scene_manager = scene_manager_alloc(&nfc_tools_scene_handlers, app);

    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, nfc_tools_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, nfc_tools_back_event_callback);

    app->submenu = submenu_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, NfcToolsViewMainMenu, submenu_get_view(app->submenu));

    app->popup = popup_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, NfcToolsViewPopup, popup_get_view(app->popup));

    app->text_box = text_box_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, NfcToolsViewTextBox, text_box_get_view(app->text_box));

    app->text_input = text_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, NfcToolsViewTextInput, text_input_get_view(app->text_input));

    app->email_input = email_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, NfcToolsViewEmailInput, email_input_get_view(app->email_input));

    app->mime_input = mime_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, NfcToolsViewMimeInput, mime_input_get_view(app->mime_input));

    app->special_input = special_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        NfcToolsViewSpecialInput,
        special_input_get_view(app->special_input));

    // Second submenu for NDEF records list
    app->submenu2 = submenu_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, NfcToolsViewSubmenu2, submenu_get_view(app->submenu2));

    // QR code view
    app->qr_view = nfc_tools_qr_view_alloc(app);
    view_dispatcher_add_view(app->view_dispatcher, NfcToolsViewQrCode, app->qr_view);

    // Widget view (record detail with button)
    app->widget = widget_alloc();
    view_dispatcher_add_view(app->view_dispatcher, NfcToolsViewWidget, widget_get_view(app->widget));

    app->ndef_record_count   = 0;
    app->ndef_selected_record = 0;

    app->notifications = furi_record_open(RECORD_NOTIFICATION);
    app->nfc = nfc_alloc();

    return app;
}

static void nfc_tools_app_free(NfcToolsApp* app) {
    furi_assert(app);

    nfc_free(app->nfc);
    furi_record_close(RECORD_NOTIFICATION);

    view_dispatcher_remove_view(app->view_dispatcher, NfcToolsViewWidget);
    widget_free(app->widget);

    view_dispatcher_remove_view(app->view_dispatcher, NfcToolsViewQrCode);
    nfc_tools_qr_view_free(app->qr_view);

    view_dispatcher_remove_view(app->view_dispatcher, NfcToolsViewSubmenu2);
    submenu_free(app->submenu2);

    view_dispatcher_remove_view(app->view_dispatcher, NfcToolsViewSpecialInput);
    special_input_free(app->special_input);

    view_dispatcher_remove_view(app->view_dispatcher, NfcToolsViewMimeInput);
    mime_input_free(app->mime_input);

    view_dispatcher_remove_view(app->view_dispatcher, NfcToolsViewEmailInput);
    email_input_free(app->email_input);

    view_dispatcher_remove_view(app->view_dispatcher, NfcToolsViewTextInput);
    text_input_free(app->text_input);

    view_dispatcher_remove_view(app->view_dispatcher, NfcToolsViewTextBox);
    text_box_free(app->text_box);

    view_dispatcher_remove_view(app->view_dispatcher, NfcToolsViewPopup);
    popup_free(app->popup);

    view_dispatcher_remove_view(app->view_dispatcher, NfcToolsViewMainMenu);
    submenu_free(app->submenu);

    scene_manager_free(app->scene_manager);
    view_dispatcher_free(app->view_dispatcher);

    furi_string_free(app->ndef_str);
    furi_string_free(app->info_str);
    furi_record_close(RECORD_GUI);
    free(app);
}

int32_t nfc_tools_app(void* p) {
    UNUSED(p);
    NfcToolsApp* app = nfc_tools_app_alloc();

    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);
    scene_manager_next_scene(app->scene_manager, NfcToolsSceneIdMainMenu);
    view_dispatcher_run(app->view_dispatcher);

    nfc_tools_app_free(app);
    return 0;
}
