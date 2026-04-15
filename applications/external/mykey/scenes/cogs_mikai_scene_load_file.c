#include "../cogs_mikai.h"
#include <dialogs/dialogs.h>
#include <storage/storage.h>
#include <toolbox/path.h>
#include <string.h>

// Manual hex parser (sscanf %X doesn't work reliably on Flipper)
static bool parse_hex64(const char* str, uint64_t* value) {
    if(!str || !value) return false;
    *value = 0;
    for(size_t i = 0; str[i] != '\0' && i < 16; i++) {
        char c = str[i];
        uint8_t digit;
        if(c >= '0' && c <= '9') digit = c - '0';
        else if(c >= 'A' && c <= 'F') digit = c - 'A' + 10;
        else if(c >= 'a' && c <= 'f') digit = c - 'a' + 10;
        else break;
        *value = (*value << 4) | digit;
    }
    return true;
}

static bool parse_hex32(const char* str, uint32_t* value) {
    if(!str || !value) return false;
    *value = 0;
    for(size_t i = 0; str[i] != '\0' && i < 8; i++) {
        char c = str[i];
        uint8_t digit;
        if(c >= '0' && c <= '9') digit = c - '0';
        else if(c >= 'A' && c <= 'F') digit = c - 'A' + 10;
        else if(c >= 'a' && c <= 'f') digit = c - 'a' + 10;
        else break;
        *value = (*value << 4) | digit;
    }
    return true;
}

static void cogs_mikai_scene_load_file_popup_callback(void* context) {
    COGSMyKaiApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, 0);
}

void cogs_mikai_scene_load_file_on_enter(void* context) {
    COGSMyKaiApp* app = context;
    Popup* popup = app->popup;

    // Show file browser starting in the app's data folder
    FuriString* file_path = furi_string_alloc();
    furi_string_set(file_path, "/ext/apps_data/cogs_mikai");

    // Ensure directory exists
    Storage* storage_mkdir = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage_mkdir, "/ext/apps_data/cogs_mikai");
    furi_record_close(RECORD_STORAGE);

    DialogsFileBrowserOptions browser_options;
    dialog_file_browser_set_basic_options(&browser_options, ".myk", NULL);
    browser_options.hide_ext = false;

    if(dialog_file_browser_show(app->dialogs, file_path, file_path, &browser_options)) {
        // User selected a file
        Storage* storage = furi_record_open(RECORD_STORAGE);
        File* file = storage_file_alloc(storage);

        if(storage_file_open(file, furi_string_get_cstr(file_path), FSAM_READ, FSOM_OPEN_EXISTING)) {
            // Read entire file
            size_t file_size = storage_file_size(file);
            char* file_buffer = malloc(file_size + 1);
            bool success = false;

            if(file_buffer) {
                size_t bytes_read = storage_file_read(file, file_buffer, file_size);
                file_buffer[bytes_read] = '\0';

                // Parse the file - manual line parsing without strtok
                if(bytes_read > 14 && strncmp(file_buffer, "COGES_MYKEY_V1", 14) == 0) {
                    char* ptr = file_buffer;
                    char line[128];

                    // Helper to read next line
                    auto bool read_line(char** p, char* buf, size_t max_len) {
                        size_t i = 0;
                        while(**p && **p != '\n' && i < max_len - 1) {
                            buf[i++] = *(*p)++;
                        }
                        buf[i] = '\0';
                        if(**p == '\n') (*p)++;
                        return i > 0;
                    };

                    // Skip header line
                    read_line(&ptr, line, sizeof(line));

                    // Read UID
                    if(read_line(&ptr, line, sizeof(line))) {
                        char* uid_str = strstr(line, "UID: ");
                        if(uid_str && parse_hex64(uid_str + 5, &app->mykey.uid)) {
                            FURI_LOG_I(TAG, "Loaded UID: %016llX", app->mykey.uid);

                            // Read encryption key
                            if(read_line(&ptr, line, sizeof(line))) {
                                char* key_str = strstr(line, "ENCRYPTION_KEY: ");
                                if(key_str && parse_hex32(key_str + 16, &app->mykey.encryption_key)) {
                                    FURI_LOG_I(TAG, "Loaded key: %08lX", app->mykey.encryption_key);
                                    success = true;

                                    // Read blocks
                                    for(size_t i = 0; i < SRIX4K_BLOCKS && success; i++) {
                                        if(read_line(&ptr, line, sizeof(line))) {
                                            char* block_str = strstr(line, ": ");
                                            if(block_str && parse_hex32(block_str + 2, &app->mykey.eeprom[i])) {
                                                // Success
                                            } else {
                                                FURI_LOG_E(TAG, "Failed to parse block %zu: %s", i, line);
                                                success = false;
                                            }
                                        } else {
                                            FURI_LOG_E(TAG, "Failed to read line for block %zu", i);
                                            success = false;
                                        }
                                    }
                                } else {
                                    FURI_LOG_E(TAG, "Failed to parse encryption key: %s", line);
                                }
                            }
                        } else {
                            FURI_LOG_E(TAG, "Failed to parse UID: %s", line);
                        }
                    }
                }

                free(file_buffer);
            }

            storage_file_close(file);

            if(success) {
                app->mykey.is_loaded = true;
                app->mykey.is_modified = false;  // Fresh load from file
                app->mykey.is_reset = mykey_is_reset(&app->mykey);
                app->mykey.current_credit = mykey_get_current_credit(&app->mykey);

                popup_set_header(popup, "Success!", 64, 10, AlignCenter, AlignTop);
                popup_set_text(popup, "Card loaded from file", 64, 25, AlignCenter, AlignTop);
                notification_message(app->notifications, &sequence_success);
            } else {
                popup_set_header(popup, "Error", 64, 10, AlignCenter, AlignTop);
                popup_set_text(popup, "Invalid file format", 64, 25, AlignCenter, AlignTop);
                notification_message(app->notifications, &sequence_error);
            }
        } else {
            popup_set_header(popup, "Error", 64, 10, AlignCenter, AlignTop);
            popup_set_text(popup, "Failed to open file", 64, 25, AlignCenter, AlignTop);
            notification_message(app->notifications, &sequence_error);
        }

        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
    } else {
        // User cancelled - search back to start scene and switch (forces menu rebuild)
        furi_string_free(file_path);
        scene_manager_search_and_switch_to_previous_scene(app->scene_manager, COGSMyKaiSceneStart);
        return;
    }

    furi_string_free(file_path);

    popup_set_callback(popup, cogs_mikai_scene_load_file_popup_callback);
    popup_set_context(popup, app);
    popup_set_timeout(popup, 2000);
    popup_enable_timeout(popup);
    view_dispatcher_switch_to_view(app->view_dispatcher, COGSMyKaiViewPopup);
}

bool cogs_mikai_scene_load_file_on_event(void* context, SceneManagerEvent event) {
    COGSMyKaiApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        // Popup timeout - search back to start scene and switch (forces menu rebuild)
        scene_manager_search_and_switch_to_previous_scene(app->scene_manager, COGSMyKaiSceneStart);
        consumed = true;
    }

    return consumed;
}

void cogs_mikai_scene_load_file_on_exit(void* context) {
    COGSMyKaiApp* app = context;
    popup_reset(app->popup);
}
