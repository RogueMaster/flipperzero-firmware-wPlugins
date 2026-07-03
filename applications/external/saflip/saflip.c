#include "saflip.h"
#include "furi_hal_rtc.h"
#include "protocols/iso14443_3a/iso14443_3a.h"

bool saflip_custom_event_callback(void* context, uint32_t event) {
    furi_assert(context);
    SaflipApp* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

bool saflip_back_event_callback(void* context) {
    furi_assert(context);
    SaflipApp* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

void saflip_reset_data(SaflipApp* app) {
    // Reset all fields to 0
    app->data->card_level = 0;
    app->data->card_type = 0;
    app->data->card_id = 0;
    app->data->opening_key = 0;
    app->data->lock_id = 0;
    app->data->pass_number = 0;
    app->data->sequence_and_combination = 0;
    app->data->deadbolt_override = 0;
    app->data->restricted_days = 0;
    app->data->property_id = 0;

    // Set creation date/time to now
    DateTime* datetime = malloc(sizeof(DateTime));
    furi_hal_rtc_get_datetime(datetime);
    app->data->creation.year = datetime->year;
    app->data->creation.month = datetime->month;
    app->data->creation.day = datetime->day;
    app->data->creation.hour = datetime->hour;
    app->data->creation.minute = datetime->minute;

    // Set expiration date/time to a week from now
    // Convert to timestamp and back to let datetime.c
    //   handle days per month and leap years and such
    uint32_t timestamp = datetime_datetime_to_timestamp(datetime);
    timestamp += 60 /*secs*/ * 60 /*mins*/ * 24 /*hours*/ * 7 /*days*/;
    datetime_timestamp_to_datetime(timestamp, datetime);

    app->data->expire.year = datetime->year;
    app->data->expire.month = datetime->month;
    app->data->expire.day = datetime->day;
    app->data->expire.hour = datetime->hour;
    app->data->expire.minute = datetime->minute;

    // Use default credential type (Mifare Classic) and UID
    app->data->format = SaflipFormatMifareClassic;
    app->uid_len = 4;
    app->uid[0] = 0xEB;
    app->uid[1] = 0xC7;
    app->uid[2] = 0x04;
    app->uid[3] = 0x4B;

    // Also reset log entries and variable keys
    app->variable_keys = 0;
    app->log_entries = 0;
}

SaflipApp* saflip_app_alloc() {
    SaflipApp* app = malloc(sizeof(SaflipApp));

    // GUI modules
    app->gui = furi_record_open(RECORD_GUI);
    app->view_dispatcher = view_dispatcher_alloc();
    app->scene_manager = scene_manager_alloc(&saflip_scene_handlers, app);
    app->menu = menu_alloc();
    app->popup = popup_alloc();
    app->widget = widget_alloc();
    app->submenu = submenu_alloc();
    app->dialog = dialog_ex_alloc();
    app->text_box = text_box_alloc();
    app->text_input = text_input_alloc();
    app->byte_input = byte_input_alloc();
    app->number_input = number_input_alloc();
    app->date_time_input = date_time_input_alloc();
    app->variable_item_list = variable_item_list_alloc();

    // View Dispatcher
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);
    view_dispatcher_add_view(app->view_dispatcher, SaflipViewMenu, menu_get_view(app->menu));
    view_dispatcher_add_view(app->view_dispatcher, SaflipViewPopup, popup_get_view(app->popup));
    view_dispatcher_add_view(app->view_dispatcher, SaflipViewWidget, widget_get_view(app->widget));
    view_dispatcher_add_view(
        app->view_dispatcher, SaflipViewSubmenu, submenu_get_view(app->submenu));
    view_dispatcher_add_view(
        app->view_dispatcher, SaflipViewDialog, dialog_ex_get_view(app->dialog));
    view_dispatcher_add_view(
        app->view_dispatcher, SaflipViewTextBox, text_box_get_view(app->text_box));
    view_dispatcher_add_view(
        app->view_dispatcher, SaflipViewTextInput, text_input_get_view(app->text_input));
    view_dispatcher_add_view(
        app->view_dispatcher, SaflipViewByteInput, byte_input_get_view(app->byte_input));
    view_dispatcher_add_view(
        app->view_dispatcher, SaflipViewNumberInput, number_input_get_view(app->number_input));
    view_dispatcher_add_view(
        app->view_dispatcher,
        SaflipViewDateTimeInput,
        date_time_input_get_view(app->date_time_input));
    view_dispatcher_add_view(
        app->view_dispatcher,
        SaflipViewVariableItemList,
        variable_item_list_get_view(app->variable_item_list));

    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, saflip_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, saflip_back_event_callback);

    app->nfc = nfc_alloc();
    app->nfc_device = nfc_device_alloc();
    app->nfc_scanner = nfc_scanner_alloc(app->nfc);

    app->notifications = furi_record_open(RECORD_NOTIFICATION);
    app->dialogs_app = furi_record_open(RECORD_DIALOGS);
    app->storage = furi_record_open(RECORD_STORAGE);

    app->data = malloc(sizeof(BasicAccessData));
    app->uid = malloc(ISO14443_3A_MAX_UID_SIZE);

    return app;
}

void saflip_app_free(SaflipApp* app) {
    furi_assert(app);

    free(app->uid);
    free(app->data);

    furi_record_close(RECORD_STORAGE);
    furi_record_close(RECORD_DIALOGS);
    furi_record_close(RECORD_NOTIFICATION);

    nfc_scanner_free(app->nfc_scanner);
    nfc_device_free(app->nfc_device);
    nfc_free(app->nfc);

    // View Dispatcher
    view_dispatcher_remove_view(app->view_dispatcher, SaflipViewMenu);
    view_dispatcher_remove_view(app->view_dispatcher, SaflipViewPopup);
    view_dispatcher_remove_view(app->view_dispatcher, SaflipViewWidget);
    view_dispatcher_remove_view(app->view_dispatcher, SaflipViewDialog);
    view_dispatcher_remove_view(app->view_dispatcher, SaflipViewSubmenu);
    view_dispatcher_remove_view(app->view_dispatcher, SaflipViewTextBox);
    view_dispatcher_remove_view(app->view_dispatcher, SaflipViewTextInput);
    view_dispatcher_remove_view(app->view_dispatcher, SaflipViewByteInput);
    view_dispatcher_remove_view(app->view_dispatcher, SaflipViewNumberInput);
    view_dispatcher_remove_view(app->view_dispatcher, SaflipViewDateTimeInput);
    view_dispatcher_remove_view(app->view_dispatcher, SaflipViewVariableItemList);

    // GUI
    variable_item_list_free(app->variable_item_list);
    date_time_input_free(app->date_time_input);
    number_input_free(app->number_input);
    byte_input_free(app->byte_input);
    text_input_free(app->text_input);
    text_box_free(app->text_box);
    dialog_ex_free(app->dialog);
    submenu_free(app->submenu);
    widget_free(app->widget);
    popup_free(app->popup);
    menu_free(app->menu);
    scene_manager_free(app->scene_manager);
    view_dispatcher_free(app->view_dispatcher);
    furi_record_close(RECORD_GUI);

    free(app);
}

int32_t saflip_app(void* p) {
    UNUSED(p);

    SaflipApp* app = saflip_app_alloc();

    scene_manager_next_scene(app->scene_manager, SaflipSceneStart);

    view_dispatcher_run(app->view_dispatcher);

    saflip_app_free(app);

    return 0;
}
