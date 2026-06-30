#include "passy_i.h"

#define TAG "Passy"

#define PASSY_MRZ_INFO_FILENAME "mrz_info"

bool passy_load_mrz_info(Passy* passy) {
    const char* file_header = "MRZ Info";
    const uint32_t file_version = 1;
    bool parsed = false;
    FlipperFormat* file = flipper_format_file_alloc(passy->storage);
    FuriString* path = furi_string_alloc();
    FuriString* temp_str = furi_string_alloc();
    uint32_t version = 0;

    do {
        furi_string_printf(
            path, "%s/%s%s", STORAGE_APP_DATA_PATH_PREFIX, PASSY_MRZ_INFO_FILENAME, ".txt");
        // Open file
        if(!flipper_format_file_open_existing(file, furi_string_get_cstr(path))) break;
        if(!flipper_format_read_header(file, temp_str, &version)) break;
        if(!furi_string_equal_str(temp_str, file_header) || (version != file_version)) {
            break;
        }
        // passport number
        // dob
        // doe

        if(!flipper_format_read_string(file, "Passport Number", temp_str)) break;
        strlcpy(
            passy->passport_number,
            furi_string_get_cstr(temp_str),
            PASSY_PASSPORT_NUMBER_MAX_LENGTH);
        if(!flipper_format_read_string(file, "Date of Birth", temp_str)) break;
        strlcpy(passy->date_of_birth, furi_string_get_cstr(temp_str), PASSY_DOB_MAX_LENGTH);
        if(!flipper_format_read_string(file, "Date of Expiry", temp_str)) break;
        strlcpy(passy->date_of_expiry, furi_string_get_cstr(temp_str), PASSY_DOE_MAX_LENGTH);

        parsed = true;
    } while(false);

    if(parsed) {
        FURI_LOG_I(TAG, "MRZ Info loaded");
    }

    furi_string_free(path);
    furi_string_free(temp_str);
    flipper_format_free(file);

    return parsed;
}

bool passy_save_mrz_info(Passy* passy) {
    bool saved = false;
    const char* file_header = "MRZ Info";
    const uint32_t file_version = 1;
    FlipperFormat* file = flipper_format_file_alloc(passy->storage);
    FuriString* temp_str = furi_string_alloc();

    do {
        furi_string_printf(
            temp_str, "%s/%s%s", STORAGE_APP_DATA_PATH_PREFIX, PASSY_MRZ_INFO_FILENAME, ".txt");

        // Open file
        if(!flipper_format_file_open_always(file, furi_string_get_cstr(temp_str))) break;

        // Write header
        if(!flipper_format_write_header_cstr(file, file_header, file_version)) break;

        furi_string_set_str(temp_str, passy->passport_number);
        if(!flipper_format_write_string(file, "Passport Number", temp_str)) break;
        furi_string_set_str(temp_str, passy->date_of_birth);
        if(!flipper_format_write_string(file, "Date of Birth", temp_str)) break;
        furi_string_set_str(temp_str, passy->date_of_expiry);
        if(!flipper_format_write_string(file, "Date of Expiry", temp_str)) break;

        saved = true;
    } while(false);

    furi_string_free(temp_str);
    flipper_format_free(file);
    return saved;
}

bool passy_delete_mrz_info(Passy* passy) {
    furi_assert(passy);

    bool deleted = false;
    FuriString* file_path;
    file_path = furi_string_alloc();

    do {
        furi_string_printf(
            file_path, "%s/%s%s", STORAGE_APP_DATA_PATH_PREFIX, PASSY_MRZ_INFO_FILENAME, ".txt");

        if(!storage_simply_remove(passy->storage, furi_string_get_cstr(file_path))) break;
        memset(passy->passport_number, 0, sizeof(passy->passport_number));
        memset(passy->date_of_birth, 0, sizeof(passy->date_of_birth));
        memset(passy->date_of_expiry, 0, sizeof(passy->date_of_expiry));
        deleted = true;
    } while(false);

    if(!deleted) {
        dialog_message_show_storage_error(passy->dialogs, "Can not remove file");
    }

    furi_string_free(file_path);
    return deleted;
}

bool passy_custom_event_callback(void* context, uint32_t event) {
    furi_assert(context);
    Passy* passy = context;
    return scene_manager_handle_custom_event(passy->scene_manager, event);
}

bool passy_back_event_callback(void* context) {
    furi_assert(context);
    Passy* passy = context;
    return scene_manager_handle_back_event(passy->scene_manager);
}

void passy_tick_event_callback(void* context) {
    furi_assert(context);
    Passy* passy = context;
    scene_manager_handle_tick_event(passy->scene_manager);
}

Passy* passy_alloc() {
    Passy* passy = malloc(sizeof(Passy));

    passy->view_dispatcher = view_dispatcher_alloc();
    passy->scene_manager = scene_manager_alloc(&passy_scene_handlers, passy);
    view_dispatcher_set_event_callback_context(passy->view_dispatcher, passy);
    view_dispatcher_set_custom_event_callback(passy->view_dispatcher, passy_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        passy->view_dispatcher, passy_back_event_callback);
    view_dispatcher_set_tick_event_callback(
        passy->view_dispatcher, passy_tick_event_callback, 100);

    passy->nfc = nfc_alloc();

    // Nfc device
    passy->nfc_device = nfc_device_alloc();
    nfc_device_set_loading_callback(passy->nfc_device, passy_show_loading_popup, passy);

    // Open GUI record
    passy->gui = furi_record_open(RECORD_GUI);
    view_dispatcher_attach_to_gui(
        passy->view_dispatcher, passy->gui, ViewDispatcherTypeFullscreen);

    // Open Notification record
    passy->notifications = furi_record_open(RECORD_NOTIFICATION);

    // Submenu
    passy->submenu = submenu_alloc();
    view_dispatcher_add_view(
        passy->view_dispatcher, PassyViewMenu, submenu_get_view(passy->submenu));

    // Popup
    passy->popup = popup_alloc();
    view_dispatcher_add_view(passy->view_dispatcher, PassyViewPopup, popup_get_view(passy->popup));

    // Loading
    passy->loading = loading_alloc();
    view_dispatcher_add_view(
        passy->view_dispatcher, PassyViewLoading, loading_get_view(passy->loading));

    // Text Input
    passy->text_input = text_input_alloc();
    view_dispatcher_add_view(
        passy->view_dispatcher, PassyViewTextInput, text_input_get_view(passy->text_input));

    // Number Input
    passy->number_input = number_input_alloc();
    view_dispatcher_add_view(
        passy->view_dispatcher, PassyViewNumberInput, number_input_get_view(passy->number_input));

    // TextBox
    passy->text_box = text_box_alloc();
    view_dispatcher_add_view(
        passy->view_dispatcher, PassyViewTextBox, text_box_get_view(passy->text_box));
    passy->text_box_store = furi_string_alloc();

    // Custom Widget
    passy->widget = widget_alloc();
    view_dispatcher_add_view(
        passy->view_dispatcher, PassyViewWidget, widget_get_view(passy->widget));

    passy->storage = furi_record_open(RECORD_STORAGE);
    passy->dialogs = furi_record_open(RECORD_DIALOGS);
    passy->load_path = furi_string_alloc();

    passy->DG1 = bit_buffer_alloc(PASSY_DG1_MAX_LENGTH);
    passy->COM = bit_buffer_alloc(PASSY_DG1_MAX_LENGTH);
    passy->dg_header = bit_buffer_alloc(PASSY_DG1_MAX_LENGTH);

    return passy;
}

void passy_free(Passy* passy) {
    furi_assert(passy);

    nfc_free(passy->nfc);

    // Nfc device
    nfc_device_free(passy->nfc_device);

    // Submenu
    view_dispatcher_remove_view(passy->view_dispatcher, PassyViewMenu);
    submenu_free(passy->submenu);

    // Popup
    view_dispatcher_remove_view(passy->view_dispatcher, PassyViewPopup);
    popup_free(passy->popup);

    // Loading
    view_dispatcher_remove_view(passy->view_dispatcher, PassyViewLoading);
    loading_free(passy->loading);

    // TextInput
    view_dispatcher_remove_view(passy->view_dispatcher, PassyViewTextInput);
    text_input_free(passy->text_input);

    // NumberInput
    view_dispatcher_remove_view(passy->view_dispatcher, PassyViewNumberInput);
    number_input_free(passy->number_input);

    // TextBox
    view_dispatcher_remove_view(passy->view_dispatcher, PassyViewTextBox);
    text_box_free(passy->text_box);
    furi_string_free(passy->text_box_store);

    // Custom Widget
    view_dispatcher_remove_view(passy->view_dispatcher, PassyViewWidget);
    widget_free(passy->widget);

    // View Dispatcher
    view_dispatcher_free(passy->view_dispatcher);

    // Scene Manager
    scene_manager_free(passy->scene_manager);

    // GUI
    furi_record_close(RECORD_GUI);
    passy->gui = NULL;

    // Notifications
    furi_record_close(RECORD_NOTIFICATION);
    passy->notifications = NULL;

    furi_string_free(passy->load_path);
    furi_record_close(RECORD_STORAGE);
    furi_record_close(RECORD_DIALOGS);

    bit_buffer_free(passy->DG1);
    bit_buffer_free(passy->COM);
    bit_buffer_free(passy->dg_header);

    free(passy);
}

void passy_text_store_set(Passy* passy, const char* text, ...) {
    va_list args;
    va_start(args, text);

    vsnprintf(passy->text_store, sizeof(passy->text_store), text, args);

    va_end(args);
}

void passy_text_store_clear(Passy* passy) {
    memset(passy->text_store, 0, sizeof(passy->text_store));
}

static const NotificationSequence passy_sequence_blink_start_blue = {
    &message_blink_start_10,
    &message_blink_set_color_blue,
    &message_do_not_reset,
    NULL,
};

static const NotificationSequence passy_sequence_blink_stop = {
    &message_blink_stop,
    NULL,
};

void passy_blink_start(Passy* passy) {
    notification_message(passy->notifications, &passy_sequence_blink_start_blue);
}

void passy_blink_stop(Passy* passy) {
    notification_message(passy->notifications, &passy_sequence_blink_stop);
}

void passy_show_loading_popup(void* context, bool show) {
    Passy* passy = context;

    if(show) {
        // Raise timer priority so that animations can play
        furi_timer_set_thread_priority(FuriTimerThreadPriorityElevated);
        view_dispatcher_switch_to_view(passy->view_dispatcher, PassyViewLoading);
    } else {
        // Restore default timer priority
        furi_timer_set_thread_priority(FuriTimerThreadPriorityNormal);
    }
}

int32_t passy_app(void* p) {
    UNUSED(p);
    Passy* passy = passy_alloc();

    scene_manager_next_scene(passy->scene_manager, PassySceneMainMenu);

    view_dispatcher_run(passy->view_dispatcher);

    passy_free(passy);

    return 0;
}
