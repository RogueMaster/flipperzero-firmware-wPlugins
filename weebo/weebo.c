#include "weebo_i.h"

#define TAG "weebo"

#define WEEBO_KEY_RETAIL_FILENAME "key_retail"
#define FIGURE_ID_LIST            APP_ASSETS_PATH("figure_ids.nfc")
#define UNPACKED_FIGURE_ID        0x1dc
#define NFC_APP_PATH_PREFIX       "/ext/nfc"

static const char* nfc_resources_header = "Flipper NFC resources";
static const uint32_t nfc_resources_file_version = 1;

bool weebo_load_key_retail(Weebo* weebo) {
    FuriString* path = furi_string_alloc();
    bool parsed = false;
    uint8_t buffer[160];
    memset(buffer, 0, sizeof(buffer));
    Storage* storage = furi_record_open(RECORD_STORAGE);
    Stream* stream = file_stream_alloc(storage);

    do {
        furi_string_printf(
            path, "%s/%s%s", STORAGE_APP_DATA_PATH_PREFIX, WEEBO_KEY_RETAIL_FILENAME, ".bin");

        bool opened =
            file_stream_open(stream, furi_string_get_cstr(path), FSAM_READ, FSOM_OPEN_EXISTING);
        if(!opened) {
            FURI_LOG_E(TAG, "Failed to open file");
            break;
        }

        size_t bytes_read = stream_read(stream, buffer, sizeof(buffer));
        if(bytes_read != sizeof(buffer)) {
            FURI_LOG_E(TAG, "Insufficient data");
            break;
        }

        memcpy(&weebo->keys, buffer, bytes_read);

        // TODO: compare SHA1
        parsed = true;
    } while(false);

    file_stream_close(stream);
    furi_record_close(RECORD_STORAGE);
    furi_string_free(path);

    return parsed;
}

bool weebo_load_figure(Weebo* weebo, FuriString* path, bool show_dialog) {
    bool parsed = false;
    FuriString* reason = furi_string_alloc_set("Couldn't load file");
    uint8_t buffer[NTAG215_SIZE];
    memset(buffer, 0, sizeof(buffer));

    if(weebo->loading_cb) {
        weebo->loading_cb(weebo->loading_cb_ctx, true);
    }

    do {
        NfcDevice* nfc_device = weebo->nfc_device;
        if(!nfc_device_load(nfc_device, furi_string_get_cstr(path))) break;

        NfcProtocol protocol = nfc_device_get_protocol(nfc_device);
        if(protocol != NfcProtocolMfUltralight) {
            furi_string_printf(reason, "Not Ultralight protocol");
            break;
        }

        const MfUltralightData* data = nfc_device_get_data(nfc_device, NfcProtocolMfUltralight);
        if(data->type != MfUltralightTypeNTAG215) {
            furi_string_printf(reason, "Not NTAG215");
            break;
        }

        if(!mf_ultralight_is_all_data_read(data)) {
            //TODO: check is the missing data is the PWD and/or PACK and fill that in
            furi_string_printf(reason, "Incomplete data");
            break;
        }

        uint8_t* uid = data->iso14443_3a_data->uid;
        uint8_t pwd[4];
        weebo_calculate_pwd(uid, pwd);

        if(memcmp(data->page[133].data, pwd, sizeof(pwd)) != 0) {
            furi_string_printf(reason, "Wrong password");
            break;
        }

        for(size_t i = 0; i < 135; i++) {
            memcpy(
                buffer + i * MF_ULTRALIGHT_PAGE_SIZE, data->page[i].data, MF_ULTRALIGHT_PAGE_SIZE);
        }

        if(!nfc3d_amiibo_unpack(&weebo->keys, buffer, weebo->figure)) {
            FURI_LOG_E(TAG, "Failed to unpack");
            break;
        }

        parsed = true;
    } while(false);

    if(weebo->loading_cb) {
        weebo->loading_cb(weebo->loading_cb_ctx, false);
    }

    if((!parsed) && (show_dialog)) {
        dialog_message_show_storage_error(weebo->dialogs, furi_string_get_cstr(reason));
    }

    furi_string_free(reason);
    return parsed;
}

static bool
    weebo_search_data(Storage* storage, const char* file_name, FuriString* key, FuriString* data) {
    bool parsed = false;
    FlipperFormat* file = flipper_format_file_alloc(storage);
    FuriString* temp_str;
    temp_str = furi_string_alloc();

    do {
        // Open file
        if(!flipper_format_file_open_existing(file, file_name)) break;
        // Read file header and version
        uint32_t version = 0;
        if(!flipper_format_read_header(file, temp_str, &version)) break;
        if(furi_string_cmp_str(temp_str, nfc_resources_header) ||
           (version != nfc_resources_file_version))
            break;
        if(!flipper_format_read_string(file, furi_string_get_cstr(key), data)) break;
        parsed = true;
    } while(false);

    furi_string_free(temp_str);
    flipper_format_free(file);
    return parsed;
}

uint16_t weebo_get_figure_id(Weebo* weebo) {
    uint16_t id = 0;
    id |= weebo->figure[UNPACKED_FIGURE_ID + 0] << 8;
    id |= weebo->figure[UNPACKED_FIGURE_ID + 1] << 0;
    FURI_LOG_D(TAG, "id = %04x", id);
    return id;
}

bool weebo_get_figure_form(Weebo* weebo, FuriString* name) {
    bool parsed = false;
    uint8_t form = weebo->figure[UNPACKED_FIGURE_ID + 3];
    FURI_LOG_D(TAG, "form = %02x", form);
    switch(form) {
    case 0x00:
        furi_string_set_str(name, "Figure");
        parsed = true;
        break;
    case 0x01:
        furi_string_set_str(name, "Card");
        parsed = true;
        break;
    case 0x02:
        furi_string_set_str(name, "Yarn");
        parsed = true;
        break;
    default:
        break;
    }

    return parsed;
}

bool weebo_get_figure_series(Weebo* weebo, FuriString* name) {
    bool parsed = false;

    uint8_t series_id = weebo->figure[UNPACKED_FIGURE_ID + 6];
    switch(series_id) {
    case 0x00:
        furi_string_set_str(name, "Smash Bros");
        parsed = true;
        break;
    case 0x01:
        furi_string_set_str(name, "Mario Bros");
        parsed = true;
        break;
    case 0x02:
        furi_string_set_str(name, "Chibi Robo");
        parsed = true;
        break;
    case 0x03:
        furi_string_set_str(name, "Yarn Yoshi");
        parsed = true;
        break;
    case 0x04:
        furi_string_set_str(name, "Splatoon");
        parsed = true;
        break;
    case 0x05:
        furi_string_set_str(name, "Animal Crossing");
        parsed = true;
        break;
    case 0x06:
        furi_string_set_str(name, "8-bit Mario");
        parsed = true;
        break;
    case 0x07:
        furi_string_set_str(name, "Skylanders");
        parsed = true;
        break;
    case 0x09:
        furi_string_set_str(name, "Legend of Zelda");
        parsed = true;
        break;
    case 0x0A:
        furi_string_set_str(name, "Shovel Knight");
        parsed = true;
        break;
    case 0x0C:
        furi_string_set_str(name, "Kirby");
        parsed = true;
        break;
    case 0x0D:
        furi_string_set_str(name, "Pokken");
        parsed = true;
        break;
    case 0x0F:
        furi_string_set_str(name, "Monster Hunter");
        parsed = true;
        break;
    case 0x14:
        furi_string_set_str(name, "Super Mario Cereal");
        parsed = true;
        break;
    case 0x1B:
        furi_string_set_str(name, "Xenoblade Chronicles");
        parsed = true;
        break;
    default:
        break;
    }
    return parsed;
}

bool weebo_get_figure_name(Weebo* weebo, FuriString* name) {
    bool parsed = false;

    uint16_t id = weebo_get_figure_id(weebo);

    FuriString* key = furi_string_alloc_printf("%04x", id);
    if(weebo_search_data(weebo->storage, FIGURE_ID_LIST, key, name)) {
        parsed = true;
    }
    furi_string_free(key);
    return parsed;
}

void weebo_set_loading_callback(Weebo* weebo, WeeboLoadingCallback callback, void* context) {
    furi_assert(weebo);

    weebo->loading_cb = callback;
    weebo->loading_cb_ctx = context;
}

bool weebo_file_select(Weebo* weebo) {
    furi_assert(weebo);
    bool res = false;

    FuriString* weebo_app_folder;

    if(storage_dir_exists(weebo->storage, "/ext/nfc/SmashAmiibo")) {
        weebo_app_folder = furi_string_alloc_set("/ext/nfc/SmashAmiibo");
    } else if(storage_dir_exists(weebo->storage, "/ext/nfc/Amiibo")) {
        weebo_app_folder = furi_string_alloc_set("/ext/nfc/Amiibo");
    } else if(storage_dir_exists(weebo->storage, "/ext/nfc/Amiibos")) {
        weebo_app_folder = furi_string_alloc_set("/ext/nfc/Amiibos");
    } else if(storage_dir_exists(weebo->storage, "/ext/nfc/amiibo")) {
        weebo_app_folder = furi_string_alloc_set("/ext/nfc/amiibo");
    } else if(storage_dir_exists(weebo->storage, "/ext/nfc/amiibos")) {
        weebo_app_folder = furi_string_alloc_set("/ext/nfc/amiibos");
    } else {
        weebo_app_folder = furi_string_alloc_set(NFC_APP_PATH_PREFIX);
    }

    DialogsFileBrowserOptions browser_options;
    dialog_file_browser_set_basic_options(&browser_options, NFC_APP_EXTENSION, &I_Nfc_10px);
    browser_options.base_path = NFC_APP_PATH_PREFIX;

    res = dialog_file_browser_show(
        weebo->dialogs, weebo->load_path, weebo_app_folder, &browser_options);

    furi_string_free(weebo_app_folder);
    if(res) {
        FuriString* filename;
        filename = furi_string_alloc();
        path_extract_filename(weebo->load_path, filename, true);
        strncpy(weebo->file_name, furi_string_get_cstr(filename), WEEBO_FILE_NAME_MAX_LENGTH);
        res = weebo_load_figure(weebo, weebo->load_path, true);
        furi_string_free(filename);
    }

    return res;
}

bool weebo_custom_event_callback(void* context, uint32_t event) {
    furi_assert(context);
    Weebo* weebo = context;
    return scene_manager_handle_custom_event(weebo->scene_manager, event);
}

bool weebo_back_event_callback(void* context) {
    furi_assert(context);
    Weebo* weebo = context;
    return scene_manager_handle_back_event(weebo->scene_manager);
}

void weebo_tick_event_callback(void* context) {
    furi_assert(context);
    Weebo* weebo = context;
    scene_manager_handle_tick_event(weebo->scene_manager);
}

Weebo* weebo_alloc() {
    Weebo* weebo = malloc(sizeof(Weebo));

    weebo->view_dispatcher = view_dispatcher_alloc();
    weebo->scene_manager = scene_manager_alloc(&weebo_scene_handlers, weebo);
    view_dispatcher_set_event_callback_context(weebo->view_dispatcher, weebo);
    view_dispatcher_set_custom_event_callback(weebo->view_dispatcher, weebo_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        weebo->view_dispatcher, weebo_back_event_callback);
    view_dispatcher_set_tick_event_callback(
        weebo->view_dispatcher, weebo_tick_event_callback, 100);

    weebo->nfc = nfc_alloc();

    // Nfc device
    weebo->nfc_device = nfc_device_alloc();
    nfc_device_set_loading_callback(weebo->nfc_device, weebo_show_loading_popup, weebo);

    // Open GUI record
    weebo->gui = furi_record_open(RECORD_GUI);
    view_dispatcher_attach_to_gui(
        weebo->view_dispatcher, weebo->gui, ViewDispatcherTypeFullscreen);

    // Open Notification record
    weebo->notifications = furi_record_open(RECORD_NOTIFICATION);

    // Submenu
    weebo->submenu = submenu_alloc();
    view_dispatcher_add_view(
        weebo->view_dispatcher, WeeboViewMenu, submenu_get_view(weebo->submenu));

    // Popup
    weebo->popup = popup_alloc();
    view_dispatcher_add_view(weebo->view_dispatcher, WeeboViewPopup, popup_get_view(weebo->popup));

    // Loading
    weebo->loading = loading_alloc();
    view_dispatcher_add_view(
        weebo->view_dispatcher, WeeboViewLoading, loading_get_view(weebo->loading));

    // Text Input
    weebo->text_input = text_input_alloc();
    view_dispatcher_add_view(
        weebo->view_dispatcher, WeeboViewTextInput, text_input_get_view(weebo->text_input));

    // Number Input
    weebo->number_input = number_input_alloc();
    view_dispatcher_add_view(
        weebo->view_dispatcher, WeeboViewNumberInput, number_input_get_view(weebo->number_input));

    // TextBox
    weebo->text_box = text_box_alloc();
    view_dispatcher_add_view(
        weebo->view_dispatcher, WeeboViewTextBox, text_box_get_view(weebo->text_box));
    weebo->text_box_store = furi_string_alloc();

    // Custom Widget
    weebo->widget = widget_alloc();
    view_dispatcher_add_view(
        weebo->view_dispatcher, WeeboViewWidget, widget_get_view(weebo->widget));

    weebo->storage = furi_record_open(RECORD_STORAGE);
    weebo->dialogs = furi_record_open(RECORD_DIALOGS);
    weebo->load_path = furi_string_alloc();

    // Initialize file cycling
    weebo->file_list = weebo_file_list_alloc();

    weebo->keys_loaded = false;

    return weebo;
}

void weebo_free(Weebo* weebo) {
    furi_assert(weebo);

    nfc_free(weebo->nfc);

    // Nfc device
    nfc_device_free(weebo->nfc_device);

    // Submenu
    view_dispatcher_remove_view(weebo->view_dispatcher, WeeboViewMenu);
    submenu_free(weebo->submenu);

    // Popup
    view_dispatcher_remove_view(weebo->view_dispatcher, WeeboViewPopup);
    popup_free(weebo->popup);

    // Loading
    view_dispatcher_remove_view(weebo->view_dispatcher, WeeboViewLoading);
    loading_free(weebo->loading);

    // TextInput
    view_dispatcher_remove_view(weebo->view_dispatcher, WeeboViewTextInput);
    text_input_free(weebo->text_input);

    // NumberInput
    view_dispatcher_remove_view(weebo->view_dispatcher, WeeboViewNumberInput);
    number_input_free(weebo->number_input);

    // TextBox
    view_dispatcher_remove_view(weebo->view_dispatcher, WeeboViewTextBox);
    text_box_free(weebo->text_box);
    furi_string_free(weebo->text_box_store);

    // Custom Widget
    view_dispatcher_remove_view(weebo->view_dispatcher, WeeboViewWidget);
    widget_free(weebo->widget);

    // View Dispatcher
    view_dispatcher_free(weebo->view_dispatcher);

    // Scene Manager
    scene_manager_free(weebo->scene_manager);

    // GUI
    furi_record_close(RECORD_GUI);
    weebo->gui = NULL;

    // Notifications
    furi_record_close(RECORD_NOTIFICATION);
    weebo->notifications = NULL;

    furi_string_free(weebo->load_path);

    // Clean up file cycling
    weebo_file_list_free(weebo->file_list);

    furi_record_close(RECORD_STORAGE);
    furi_record_close(RECORD_DIALOGS);

    free(weebo);
}

void weebo_text_store_set(Weebo* weebo, const char* text, ...) {
    va_list args;
    va_start(args, text);

    vsnprintf(weebo->text_store, sizeof(weebo->text_store), text, args);

    va_end(args);
}

void weebo_text_store_clear(Weebo* weebo) {
    memset(weebo->text_store, 0, sizeof(weebo->text_store));
}

static const NotificationSequence weebo_sequence_blink_start_blue = {
    &message_blink_start_10,
    &message_blink_set_color_blue,
    &message_do_not_reset,
    NULL,
};

static const NotificationSequence weebo_sequence_blink_stop = {
    &message_blink_stop,
    NULL,
};

void weebo_blink_start(Weebo* weebo) {
    notification_message(weebo->notifications, &weebo_sequence_blink_start_blue);
}

void weebo_blink_stop(Weebo* weebo) {
    notification_message(weebo->notifications, &weebo_sequence_blink_stop);
}

void weebo_show_loading_popup(void* context, bool show) {
    Weebo* weebo = context;

    if(show) {
        // Raise timer priority so that animations can play
        furi_timer_set_thread_priority(FuriTimerThreadPriorityElevated);
        view_dispatcher_switch_to_view(weebo->view_dispatcher, WeeboViewLoading);
    } else {
        // Restore default timer priority
        furi_timer_set_thread_priority(FuriTimerThreadPriorityNormal);
    }
}

int32_t weebo_app(void* p) {
    UNUSED(p);
    Weebo* weebo = weebo_alloc();

    weebo->keys_loaded = weebo_load_key_retail(weebo);
    if(weebo->keys_loaded) {
        scene_manager_next_scene(weebo->scene_manager, WeeboSceneMainMenu);
    } else {
        scene_manager_next_scene(weebo->scene_manager, WeeboSceneKeysMissing);
    }

    view_dispatcher_run(weebo->view_dispatcher);

    weebo_free(weebo);

    return 0;
}
