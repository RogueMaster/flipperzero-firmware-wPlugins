#include "../cogs_mikai.h"
#include <dialogs/dialogs.h>
#include <storage/storage.h>
#include <toolbox/path.h>

enum {
    SaveFileSceneEventInput,
};

static bool cogs_mikai_scene_save_file_validator(const char* text, FuriString* error, void* context) {
    UNUSED(context);

    if(strlen(text) == 0) {
        return true;
    }

    for(size_t i = 0; text[i] != '\0'; i++) {
        char c = text[i];
        if(!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
             (c >= '0' && c <= '9') || c == '_' || c == '-')) {
            furi_string_set(error, "Only a-z, 0-9, _, -");
            return false;
        }
    }

    return true;
}

static void cogs_mikai_scene_save_file_text_input_callback(void* context) {
    COGSMyKaiApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, SaveFileSceneEventInput);
}

static void cogs_mikai_scene_save_file_popup_callback(void* context) {
    COGSMyKaiApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, 0);
}

void cogs_mikai_scene_save_file_on_enter(void* context) {
    COGSMyKaiApp* app = context;
    Popup* popup = app->popup;

    if(!app->mykey.is_loaded) {
        popup_set_header(popup, "Error", 64, 10, AlignCenter, AlignTop);
        popup_set_text(popup, "No card loaded\nRead a card first", 64, 25, AlignCenter, AlignTop);
        popup_set_callback(popup, cogs_mikai_scene_save_file_popup_callback);
        popup_set_context(popup, app);
        popup_set_timeout(popup, 2000);
        popup_enable_timeout(popup);
        view_dispatcher_switch_to_view(app->view_dispatcher, COGSMyKaiViewPopup);
        return;
    }

    TextInput* text_input = app->text_input;
    snprintf(app->text_buffer, sizeof(app->text_buffer), "mykey_save");

    text_input_set_header_text(text_input, "Enter filename (.myk)");
    text_input_set_validator(text_input, cogs_mikai_scene_save_file_validator, NULL);
    text_input_set_result_callback(
        text_input,
        cogs_mikai_scene_save_file_text_input_callback,
        app,
        app->text_buffer,
        sizeof(app->text_buffer),
        false);

    view_dispatcher_switch_to_view(app->view_dispatcher, COGSMyKaiViewTextInput);
}

bool cogs_mikai_scene_save_file_on_event(void* context, SceneManagerEvent event) {
    COGSMyKaiApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == SaveFileSceneEventInput) {
            if(app->text_buffer[0] == '\0') {
                FURI_LOG_W(TAG, "Ignoring empty text_buffer (already processed)");
                consumed = true;
                return consumed;
            }

            char filename[32];
            strncpy(filename, app->text_buffer, sizeof(filename) - 1);
            filename[sizeof(filename) - 1] = '\0';

            text_input_reset(app->text_input);
            memset(app->text_buffer, 0, sizeof(app->text_buffer));

            FuriString* file_path = furi_string_alloc();
            furi_string_printf(file_path, "/ext/apps_data/cogs_mikai/%s.myk", filename);

            FURI_LOG_I(TAG, "Attempting to save file: %s", furi_string_get_cstr(file_path));
            Storage* storage = furi_record_open(RECORD_STORAGE);
            storage_simply_mkdir(storage, "/ext/apps_data/cogs_mikai");

            File* file = storage_file_alloc(storage);
            bool success = false;

            if(storage_file_open(file, furi_string_get_cstr(file_path), FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
                const char* header = "COGES_MYKEY_V1\n";
                storage_file_write(file, header, strlen(header));

                FuriString* line = furi_string_alloc();
                furi_string_printf(line, "UID: %016llX\n", app->mykey.uid);
                storage_file_write(file, furi_string_get_cstr(line), furi_string_size(line));

                furi_string_printf(line, "ENCRYPTION_KEY: %08lX\n", app->mykey.encryption_key);
                storage_file_write(file, furi_string_get_cstr(line), furi_string_size(line));

                for(size_t i = 0; i < SRIX4K_BLOCKS; i++) {
                    furi_string_printf(line, "BLOCK_%03zu: %08lX\n", i, app->mykey.eeprom[i]);
                    storage_file_write(file, furi_string_get_cstr(line), furi_string_size(line));
                }

                furi_string_free(line);
                storage_file_close(file);
                success = true;
            }

            storage_file_free(file);
            furi_record_close(RECORD_STORAGE);

            Popup* popup = app->popup;
            if(success) {
                popup_set_header(popup, "Success!", 64, 10, AlignCenter, AlignTop);
                popup_set_text(popup, "File saved to\napps_data/cogs_mikai/", 64, 25, AlignCenter, AlignTop);
                notification_message(app->notifications, &sequence_success);
                FURI_LOG_I(TAG, "File saved: %s", furi_string_get_cstr(file_path));
            } else {
                popup_set_header(popup, "Error", 64, 10, AlignCenter, AlignTop);
                popup_set_text(popup, "Failed to create file", 64, 25, AlignCenter, AlignTop);
                notification_message(app->notifications, &sequence_error);
                FURI_LOG_E(TAG, "Failed to save file: %s", furi_string_get_cstr(file_path));
            }

            furi_string_free(file_path);

            popup_set_callback(popup, cogs_mikai_scene_save_file_popup_callback);
            popup_set_context(popup, app);
            popup_set_timeout(popup, 3000);
            popup_enable_timeout(popup);
            view_dispatcher_switch_to_view(app->view_dispatcher, COGSMyKaiViewPopup);

            consumed = true;
        } else {
            scene_manager_search_and_switch_to_previous_scene(app->scene_manager, COGSMyKaiSceneStart);
            consumed = true;
        }
    }

    return consumed;
}

void cogs_mikai_scene_save_file_on_exit(void* context) {
    COGSMyKaiApp* app = context;
    text_input_reset(app->text_input);
    popup_reset(app->popup);
}
