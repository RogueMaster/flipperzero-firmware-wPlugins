#include "mfdesfire_auth_i.h"

bool mfdesfire_auth_load_settings(MfDesApp* instance) {
    bool result = false;
    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* file = flipper_format_file_alloc(storage);
    FuriString* temp_str = furi_string_alloc();
    uint32_t version;

    do {
        if(!flipper_format_file_open_existing(file, SAVE_PATH)) break;

        // Read and verify header
        if(!flipper_format_read_header(file, temp_str, &version)) break;
        if(!furi_string_equal_str(temp_str, SAVE_FILE_HEADER)) break;
        if(version != SAVE_FILE_VERSION) break;

        // Extract filename from full path
        const char* filename = strrchr(furi_string_get_cstr(instance->selected_card_path), '/');
        if(filename) {
            filename++; // Skip the '/' character
        } else {
            filename = furi_string_get_cstr(instance->selected_card_path);
        }

        FuriString* key_name = furi_string_alloc();

        furi_string_printf(key_name, "Card_Path_%s", filename);
        if(flipper_format_read_string(file, furi_string_get_cstr(key_name), temp_str)) {
            if(furi_string_equal(temp_str, instance->selected_card_path)) {
                furi_string_printf(key_name, "Initial_Vector_%s", filename);
                if(!flipper_format_read_hex(
                       file,
                       furi_string_get_cstr(key_name),
                       instance->initial_vector,
                       INITIAL_VECTOR_SIZE))
                    break;

                furi_string_printf(key_name, "Key_%s", filename);
                if(!flipper_format_read_hex(
                       file, furi_string_get_cstr(key_name), instance->key, KEY_SIZE))
                    break;

                result = true;
            }
        }

        furi_string_free(key_name);
    } while(false);

    furi_string_free(temp_str);
    flipper_format_free(file);
    furi_record_close(RECORD_STORAGE);
    return result;
}

bool mfdesfire_auth_save_settings(MfDesApp* app, uint32_t index) {
    bool result = false;
    // UNUSED(index);
    // MfDesfireAuthApp* app = context;
    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* file = flipper_format_file_alloc(storage);

    // Extract filename from full path
    const char* filename = strrchr(furi_string_get_cstr(app->selected_card_path), '/');
    if(filename) {
        filename++; // Skip the '/' character
    } else {
        filename = furi_string_get_cstr(app->selected_card_path);
    }

    do {
        // Open existing or create a new one
        if(!flipper_format_file_open_existing(file, SAVE_PATH)) {
            flipper_format_file_open_new(file, SAVE_PATH);
            // Write the header to a new file
            if(!flipper_format_write_header_cstr(file, SAVE_FILE_HEADER, SAVE_FILE_VERSION)) break;
        };

        // Prepare key variable
        FuriString* key_name = furi_string_alloc();

        // Set key for card path
        furi_string_printf(key_name, "Card_Path_%s", filename);
        if(!flipper_format_insert_or_update_string(
               file, furi_string_get_cstr(key_name), app->selected_card_path))
            break;

        if(index == MfDesAppByteInputIV) {
            furi_string_printf(key_name, "Initial_Vector_%s", filename);
            if(!flipper_format_insert_or_update_hex(
                   file, furi_string_get_cstr(key_name), app->initial_vector, INITIAL_VECTOR_SIZE))
                break;
        }

        if(index == MfDesAppByteInputKey) {
            furi_string_printf(key_name, "Key_%s", filename);
            if(!flipper_format_insert_or_update_hex(
                   file, furi_string_get_cstr(key_name), app->key, KEY_SIZE))
                break;
        }

        furi_string_free(key_name);
        result = true;
    } while(false);

    flipper_format_free(file);
    furi_record_close(RECORD_STORAGE);
    return result;
}

// /**
//  * @brief Callback pro popup timeout.
//  */
// static void mfdesfire_auth_popup_callback(void* context) {
//     furi_assert(context);
//     MfDesApp* instance = context;
//     view_dispatcher_switch_to_view(instance->view_dispatcher, MfDesAppViewSubmenu);
// }

static bool mfdes_app_custom_event_callback(void* context, uint32_t event) {
    furi_assert(context);
    MfDesApp* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool mfdes_back_event_callback(void* context) {
    furi_assert(context);
    MfDesApp* instance = context;
    return scene_manager_handle_back_event(instance->scene_manager);
}

/**
 * @brief Alokuje a inicializuje zdroje aplikace.
 */
MfDesApp* mfdesfire_auth_app_alloc() {
    MfDesApp* instance = malloc(sizeof(MfDesApp));

    // Open GUI record
    instance->gui = furi_record_open(RECORD_GUI);

    // Open Notification record
    instance->notifications = furi_record_open(RECORD_NOTIFICATION);

    // View dispatcher settings
    instance->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(instance->view_dispatcher, instance);
    view_dispatcher_set_custom_event_callback(
        instance->view_dispatcher, mfdes_app_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        instance->view_dispatcher,
        mfdes_back_event_callback); // Tohle se zavola kdyz kratce kliknu tlacitko zpet
    view_dispatcher_attach_to_gui(
        instance->view_dispatcher, instance->gui, ViewDispatcherTypeFullscreen);

    instance->selected_card_path = furi_string_alloc();
    // memset(instance->initial_vector, 0, INITIAL_VECTOR_SIZE);
    // memset(instance->key, 0, KEY_SIZE);

    // Přidání views
    instance->file_browser = file_browser_alloc(
        instance
            ->selected_card_path); // Ulozi vyslednou cestu do selected card path - je to v file_browser.c inner se o to stara
    view_dispatcher_add_view(
        instance->view_dispatcher,
        MfDesAppViewBrowser,
        file_browser_get_view(instance->file_browser));

    instance->submenu = submenu_alloc();
    view_dispatcher_add_view(
        instance->view_dispatcher, MfDesAppViewSubmenu, submenu_get_view(instance->submenu));

    instance->byte_input = byte_input_alloc();
    view_dispatcher_add_view(
        instance->view_dispatcher,
        MfDesAppViewByteInput,
        byte_input_get_view(instance->byte_input));

    instance->popup = popup_alloc();
    view_dispatcher_add_view(
        instance->view_dispatcher, MfDesAppViewPopupAuth, popup_get_view(instance->popup));

    instance->scene_manager = scene_manager_alloc(&desfire_app_scene_handlers, instance);

    return instance;
}

/**
 * @brief Uvolní všechny alokované zdroje aplikace.
 */
void mfdesfire_auth_app_free(MfDesApp* instance) {
    furi_assert(instance);

    // Odstranění views
    view_dispatcher_remove_view(instance->view_dispatcher, MfDesAppViewBrowser);
    view_dispatcher_remove_view(instance->view_dispatcher, MfDesAppViewSubmenu);
    view_dispatcher_remove_view(instance->view_dispatcher, MfDesAppViewByteInput);
    // view_dispatcher_remove_view(instance->view_dispatcher, MfDesAppViewByteInputKey);
    view_dispatcher_remove_view(instance->view_dispatcher, MfDesAppViewPopupAuth);

    // Uvolnění zdrojů
    view_dispatcher_free(instance->view_dispatcher);
    file_browser_stop(instance->file_browser);
    file_browser_free(instance->file_browser);
    submenu_free(instance->submenu);
    byte_input_free(instance->byte_input);
    popup_free(instance->popup);
    furi_string_free(instance->selected_card_path);
    furi_record_close(RECORD_GUI);
    free(instance);
}

int32_t mfdesfire_auth_main(void* p) {
    UNUSED(p);

    MfDesApp* instance = mfdesfire_auth_app_alloc();
    // view_dispatcher_switch_to_view(instance->view_dispatcher, MfDesAppViewBrowser);
    scene_manager_next_scene(instance->scene_manager, MfDesAppViewBrowser);
    view_dispatcher_run(instance->view_dispatcher);

    // FURI_LOG_I(TAG, "Stopping instance & cleaning");
    mfdesfire_auth_app_free(instance);

    return 0;
}
