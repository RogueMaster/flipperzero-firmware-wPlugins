#include "metroflip_i.h"

#define TAG "Metroflip"
#include "api/metroflip/metroflip_api.h"
#include "api/metroflip/metroflip_api_interface.h"
#include "metroflip_plugins.h"
struct MfClassicKeyCache {
    MfClassicDeviceKeys keys;
    MfClassicKeyType current_key_type;
    uint8_t current_sector;
};

static bool metroflip_custom_event_callback(void* context, uint32_t event) {
    furi_assert(context);
    Metroflip* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool metroflip_back_event_callback(void* context) {
    furi_assert(context);
    Metroflip* app = context;

    return scene_manager_handle_back_event(app->scene_manager);
}

Metroflip* metroflip_alloc() {
    Metroflip* app = malloc(sizeof(Metroflip));
    app->gui = furi_record_open(RECORD_GUI);
    //nfc device
    app->nfc = nfc_alloc();
    app->nfc_device = nfc_device_alloc();
    app->detected_protocols = nfc_detected_protocols_alloc();

    // calypso data
    app->calypso_file_data = furi_string_alloc();

    // key cache
    app->mfc_key_cache = mf_classic_key_cache_alloc();

    // notifs
    app->notifications = furi_record_open(RECORD_NOTIFICATION);
    // View Dispatcher and Scene Manager
    app->view_dispatcher = view_dispatcher_alloc();
    app->scene_manager = scene_manager_alloc(&metroflip_scene_handlers, app);

    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);

    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, metroflip_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, metroflip_back_event_callback);

    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    // Custom Widget
    app->widget = widget_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, MetroflipViewWidget, widget_get_view(app->widget));

    // Gui Modules
    app->submenu = submenu_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, MetroflipViewSubmenu, submenu_get_view(app->submenu));

    app->text_input = text_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, MetroflipViewTextInput, text_input_get_view(app->text_input));

    app->byte_input = byte_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, MetroflipViewByteInput, byte_input_get_view(app->byte_input));

    app->popup = popup_alloc();
    view_dispatcher_add_view(app->view_dispatcher, MetroflipViewPopup, popup_get_view(app->popup));
    app->nfc_device = nfc_device_alloc();

    // TextBox
    app->text_box = text_box_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, MetroflipViewTextBox, text_box_get_view(app->text_box));
    app->text_box_store = furi_string_alloc();

    // Dialog for loading
    app->dialogs = furi_record_open(RECORD_DIALOGS);

    app->data_loaded = false;
    return app;
}

void metroflip_free(Metroflip* app) {
    furi_assert(app);

    //nfc device
    nfc_free(app->nfc);
    nfc_device_free(app->nfc_device);
    nfc_detected_protocols_free(app->detected_protocols);

    //calypso data
    furi_string_free(app->calypso_file_data);

    // key cache
    mf_classic_key_cache_free(app->mfc_key_cache);

    //notifs
    furi_record_close(RECORD_NOTIFICATION);
    app->notifications = NULL;

    // Gui modules
    view_dispatcher_remove_view(app->view_dispatcher, MetroflipViewSubmenu);
    submenu_free(app->submenu);
    view_dispatcher_remove_view(app->view_dispatcher, MetroflipViewTextInput);
    text_input_free(app->text_input);
    view_dispatcher_remove_view(app->view_dispatcher, MetroflipViewByteInput);
    byte_input_free(app->byte_input);
    view_dispatcher_remove_view(app->view_dispatcher, MetroflipViewPopup);
    popup_free(app->popup);

    // Custom Widget
    view_dispatcher_remove_view(app->view_dispatcher, MetroflipViewWidget);
    widget_free(app->widget);

    // TextBox
    view_dispatcher_remove_view(app->view_dispatcher, MetroflipViewTextBox);
    text_box_free(app->text_box);
    furi_string_free(app->text_box_store);

    // View Dispatcher and Scene Manager
    view_dispatcher_free(app->view_dispatcher);
    scene_manager_free(app->scene_manager);

    // Records
    furi_record_close(RECORD_GUI);

    // Dialogs
    furi_record_close(RECORD_DIALOGS);

    free(app);
}

static const NotificationSequence metroflip_app_sequence_blink_start_blue = {
    &message_blink_start_10,
    &message_blink_set_color_blue,
    &message_do_not_reset,
    NULL,
};

static const NotificationSequence metroflip_app_sequence_blink_stop = {
    &message_blink_stop,
    NULL,
};

void metroflip_app_blink_start(Metroflip* metroflip) {
    notification_message(metroflip->notifications, &metroflip_app_sequence_blink_start_blue);
}

void metroflip_app_blink_stop(Metroflip* metroflip) {
    notification_message(metroflip->notifications, &metroflip_app_sequence_blink_stop);
}

void metroflip_exit_widget_callback(GuiButtonType result, InputType type, void* context) {
    Metroflip* app = context;
    UNUSED(result);

    if(type == InputTypeShort) {
        scene_manager_search_and_switch_to_previous_scene(app->scene_manager, MetroflipSceneStart);
    }
}

void metroflip_save_widget_callback(GuiButtonType result, InputType type, void* context) {
    Metroflip* app = context;
    UNUSED(result);

    if(type == InputTypeShort) {
        scene_manager_next_scene(app->scene_manager, MetroflipSceneSave);
    }
}

void metroflip_delete_widget_callback(GuiButtonType result, InputType type, void* context) {
    Metroflip* app = context;
    UNUSED(result);

    if(type == InputTypeShort) {
        scene_manager_next_scene(app->scene_manager, MetroflipSceneDelete);
    }
}

// Calypso

void byte_to_binary(uint8_t byte, char* bits) {
    for(int i = 7; i >= 0; i--) {
        bits[7 - i] = (byte & (1 << i)) ? '1' : '0';
    }
    bits[8] = '\0';
}

int binary_to_decimal(const char binary[]) {
    int decimal = 0;
    int length = strlen(binary);

    for(int i = 0; i < length; i++) {
        decimal = decimal * 2 + (binary[i] - '0');
    }

    return decimal;
}

void locale_format_datetime_cat(FuriString* out, const DateTime* dt, bool time) {
    // helper to print datetimes
    FuriString* s = furi_string_alloc();

    LocaleDateFormat date_format = locale_get_date_format();
    const char* separator = (date_format == LocaleDateFormatDMY) ? "." : "/";
    locale_format_date(s, dt, date_format, separator);
    furi_string_cat(out, s);
    if(time) {
        locale_format_time(s, dt, locale_get_time_format(), false);
        furi_string_cat_printf(out, "  ");
        furi_string_cat(out, s);
    }

    furi_string_free(s);
}

// read file
uint8_t read_file[5] = {0x94, 0xb2, 0x01, 0x04, 0x1D};
//                                 ^^^
//                                 |||
//                                 FID

// select app
uint8_t select_app[8] = {0x94, 0xA4, 0x00, 0x00, 0x02, 0x20, 0x00, 0x00};
//                                                    ^^^^^^^^^
//                                                    |||||||||
//                                                    AID: 20XX

uint8_t apdu_success[2] = {0x90, 0x00};

char* bit_slice(const char* bit_representation, int start, int end) {
    static char bit_slice[32 * 8 + 1];
    strncpy(bit_slice, bit_representation + start, end - start + 1);
    bit_slice[end - start + 1] = '\0';
    return bit_slice;
}

int bit_slice_to_dec(const char* bit_representation, int start, int end) {
    return binary_to_decimal(bit_slice(bit_representation, start, end));
}

extern int32_t metroflip(void* p) {
    UNUSED(p);

    /*
    plugin_manager_load_single(PluginManager * manager, const char* path)
        uint32_t plugin_count = plugin_manager_get_count(manager);
    FURI_LOG_I(TAG, "Loaded %lu plugin(s)", plugin_count);

    for(uint32_t i = 0; i < plugin_count; i++) {
        const MetroflipPlugin* plugin = plugin_manager_get_ep(manager, i);
        FURI_LOG_I(TAG, "plugin name: %s", plugin->name);
    }
    */

    Metroflip* app = metroflip_alloc();
    scene_manager_set_scene_state(app->scene_manager, MetroflipSceneStart, MetroflipSceneAuto);
    scene_manager_next_scene(app->scene_manager, MetroflipSceneStart);
    view_dispatcher_run(app->view_dispatcher);
    metroflip_free(app);
    return 0;
}

KeyfileManager manage_keyfiles(
    char uid_str[],
    const uint8_t* uid,
    size_t uid_len,
    MfClassicKeyCache* instance,
    uint64_t key_mask_a_required,
    uint64_t key_mask_b_required) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* source = storage_file_alloc(storage);
    char source_path[64];
    UNUSED(key_mask_b_required);

    FURI_LOG_I("TAG", "%s", uid_str);
    size_t source_required_size =
        strlen("/ext/nfc/.cache/") + strlen(uid_str) + strlen(".keys") + 1;
    snprintf(source_path, source_required_size, "/ext/nfc/.cache/%s.keys", uid_str);
    bool cache_file = storage_file_open(source, source_path, FSAM_READ, FSOM_OPEN_EXISTING);

    /*-----------------Open assets cache file (if exists)------------*/

    File* dest = storage_file_alloc(storage);
    char dest_path[64];
    size_t dest_required_size =
        strlen("/ext/nfc/assets/.") + strlen(uid_str) + strlen(".keys") + 1;
    snprintf(dest_path, dest_required_size, "/ext/nfc/assets/.%s.keys", uid_str);
    bool dest_cache_file = storage_file_open(dest, dest_path, FSAM_READ, FSOM_OPEN_EXISTING);
    /*-----------------Check cache file------------*/
    if(!cache_file) {
        /*-----------------Check assets cache file------------*/
        FURI_LOG_I("TAG", "cache dont exist, checking assets");

        if(!dest_cache_file) {
            FURI_LOG_I("TAG", "assets dont exist, prompting user to fix..");
            storage_file_close(source);
            storage_file_close(dest);
            return MISSING_KEYFILE;
        } else {
            size_t dest_file_length = storage_file_size(dest);

            FURI_LOG_I(
                "TAG", "assets exist, but cache doesnt, proceeding to copy assets to cache");
            // Close, then open both files
            storage_file_close(source);
            storage_file_close(dest);
            storage_file_open(
                source, source_path, FSAM_WRITE, FSOM_OPEN_ALWAYS); // create new file
            storage_file_open(
                dest, dest_path, FSAM_READ, FSOM_OPEN_EXISTING); // open existing assets keyfile
            FURI_LOG_I("TAG", "creating cache file at %s from %s", source_path, dest_path);
            /*-----Clone keyfile from assets to cache (creates temporary buffer)----*/
            uint8_t* cloned_buffer = malloc(dest_file_length);
            storage_file_read(dest, cloned_buffer, dest_file_length);
            storage_file_write(source, cloned_buffer, dest_file_length);
            free(cloned_buffer);
            storage_file_close(source);
            storage_file_close(dest);
            return SUCCESSFUL;
        }
    } else {
        size_t source_file_length = storage_file_size(source);

        storage_file_close(source);
        mf_classic_key_cache_load(instance, uid, uid_len);

        if(KEY_MASK_BIT_CHECK(key_mask_a_required, instance->keys.key_a_mask) &&
           KEY_MASK_BIT_CHECK(key_mask_b_required, instance->keys.key_b_mask)) {
            FURI_LOG_I("TAG", "cache exist, creating assets cache if not already exists");
            storage_file_close(dest);
            storage_file_close(source);
            storage_file_open(dest, dest_path, FSAM_WRITE, FSOM_OPEN_ALWAYS);
            storage_file_open(source, source_path, FSAM_READ, FSOM_OPEN_EXISTING);
            FURI_LOG_I("TAG", "creating assets cache");
            /*-----Clone keyfile from assets to cache (creates temporary buffer)----*/
            uint8_t* cloned_buffer = malloc(source_file_length);
            storage_file_read(source, cloned_buffer, source_file_length);
            storage_file_write(dest, cloned_buffer, source_file_length);
            free(cloned_buffer);
            storage_file_close(source);
            storage_file_close(dest);
            return SUCCESSFUL;

        } else {
            FURI_LOG_I("TAG", "incomplete cache file, aborting.");
            storage_file_close(source);
            storage_file_close(dest);
            return INCOMPLETE_KEYFILE;
        }
    }
    FURI_LOG_I("TAG", "proceeding to read");
    storage_file_close(source);
    storage_file_close(dest);
}

void uid_to_string(const uint8_t* uid, size_t uid_len, char* uid_str, size_t max_len) {
    size_t pos = 0;

    for(size_t i = 0; i < uid_len && pos + 2 < max_len; ++i) {
        pos += snprintf(&uid_str[pos], max_len - pos, "%02X", uid[i]);
    }

    uid_str[pos] = '\0'; // Null-terminate the string
}

void handle_keyfile_case(
    Metroflip* app,
    const char* message_title,
    const char* log_message,
    FuriString* parsed_data,
    char card_type[]) {
    FURI_LOG_I(card_type, log_message);
    dolphin_deed(DolphinDeedNfcReadSuccess);
    furi_string_reset(parsed_data);

    furi_string_printf(
        parsed_data,
        "\e#%s\n\n"
        "To read a %s, \nyou need to read \nit in NFC "
        "app on \nthe flipper, and it\nneeds to show \n32/32 keys and\n"
        "16/16 sectors read\n"
        "Here is a guide to \nfollow to read \nMIFARE Classic:\n"
        "https://flipper.wiki/mifareclassic/\n"
        "Once completed, Scan again\n\n",
        message_title,
        card_type);

    widget_add_text_scroll_element(app->widget, 0, 0, 128, 64, furi_string_get_cstr(parsed_data));

    widget_add_button_element(
        app->widget, GuiButtonTypeRight, "Exit", metroflip_exit_widget_callback, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, MetroflipViewWidget);
    metroflip_app_blink_stop(app);
}

void metroflip_plugin_manager_alloc(Metroflip* app) {
    app->resolver = composite_api_resolver_alloc();
    composite_api_resolver_add(app->resolver, firmware_api_interface);
    composite_api_resolver_add(app->resolver, metroflip_api_interface);
    app->plugin_manager = plugin_manager_alloc(
        METROFLIP_SUPPORTED_CARD_PLUGIN_APP_ID,
        METROFLIP_SUPPORTED_CARD_PLUGIN_API_VERSION,
        composite_api_resolver_get(app->resolver));
}
