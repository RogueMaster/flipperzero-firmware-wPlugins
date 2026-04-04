/**
 * @file flippass_scene_password_entry.c
 * @brief Implementation of the password entry scene.
 *
 * This file contains the implementation of the scene handlers for the
 * password entry scene, including the callback for text input.
 */
#include "flippass_scene_password_entry.h"
#include "../flippass.h"
#include "../flippass_db.h"
#include "flippass_scene.h"
#include <flipper_format/flipper_format.h>
#include <stdio.h>
#include <toolbox/path.h>

static void flippass_scene_password_entry_set_header(App* app) {
    furi_assert(app);

    snprintf(app->password_header, sizeof(app->password_header), "%s", "Enter Password");

    if(app->file_path == NULL || furi_string_empty(app->file_path)) {
        text_input_set_header_text(app->text_input, app->password_header);
        return;
    }

    FuriString* file_name = furi_string_alloc();
    path_extract_filename(app->file_path, file_name, true);
    if(!furi_string_empty(file_name)) {
        snprintf(
            app->password_header,
            sizeof(app->password_header),
            "Pass for %s",
            furi_string_get_cstr(file_name));
    }
    furi_string_free(file_name);

    text_input_set_header_text(app->text_input, app->password_header);
}

#if FLIPPASS_ENABLE_DEBUG_UNLOCK_HOOK
static bool flippass_scene_password_entry_bool_text(const char* text) {
    return text != NULL &&
           (strcmp(text, "1") == 0 || strcmp(text, "true") == 0 || strcmp(text, "yes") == 0 ||
            strcmp(text, "on") == 0);
}
static bool flippass_scene_password_entry_try_debug_unlock(App* app) {
    bool triggered = false;
    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* file = flipper_format_file_alloc(storage);
    FuriString* password = furi_string_alloc();
    FuriString* backend = furi_string_alloc();
    FuriString* allow_ext = furi_string_alloc();

    if(flipper_format_file_open_existing(file, FLIPPASS_DEBUG_UNLOCK_FILE_PATH) &&
       flipper_format_read_string(file, "password", password)) {
        if(flipper_format_read_string(file, "backend", backend)) {
            const KDBXVaultBackend parsed_backend =
                flippass_db_parse_backend_hint(furi_string_get_cstr(backend));
            if(parsed_backend != KDBXVaultBackendNone) {
                app->requested_vault_backend = parsed_backend;
            }
        }

        app->allow_ext_vault_promotion =
            flipper_format_read_string(file, "allow_ext", allow_ext) &&
            flippass_scene_password_entry_bool_text(furi_string_get_cstr(allow_ext));
        snprintf(
            app->master_password,
            sizeof(app->master_password),
            "%s",
            furi_string_get_cstr(password));
        flippass_log_event(
            app,
            "DEBUG_UNLOCK_HOOK backend=%s allow_ext=%u",
            kdbx_vault_backend_label(app->requested_vault_backend),
            app->allow_ext_vault_promotion ? 1U : 0U);
        triggered = true;
    }

    flipper_format_file_close(file);
    flipper_format_free(file);
    storage_simply_remove(storage, FLIPPASS_DEBUG_UNLOCK_FILE_PATH);
    furi_record_close(RECORD_STORAGE);
    furi_string_free(password);
    furi_string_free(backend);
    furi_string_free(allow_ext);

    if(triggered) {
        flippass_clear_text_buffer(app);
        scene_manager_next_scene(app->scene_manager, FlipPassScene_DbEntries);
    }

    return triggered;
}
#endif

static void flippass_scene_password_entry_trigger_unlock(App* app, const char* password) {
    furi_assert(app);
    furi_assert(password);

    const KDBXVaultBackend requested_backend = app->requested_vault_backend;
    const bool allow_ext_promotion = app->allow_ext_vault_promotion;
    flippass_reset_database(app);
    app->requested_vault_backend = requested_backend;
    app->allow_ext_vault_promotion = allow_ext_promotion;
    snprintf(app->master_password, sizeof(app->master_password), "%s", password);
    flippass_clear_text_buffer(app);
    scene_manager_next_scene(app->scene_manager, FlipPassScene_DbEntries);
}

/**
 * @brief Callback function for the text input.
 *
 * This function is called when the user confirms their text input. It
 * transitions to the next scene.
 *
 * @param context The application context.
 */
static void flippass_scene_password_entry_callback(void* context) {
    App* app = context;
    app->allow_ext_vault_promotion = app->requested_vault_backend != KDBXVaultBackendRam;
    flippass_scene_password_entry_trigger_unlock(app, app->text_buffer);
}

/**
 * @brief Handler for the on_enter event of the password entry scene.
 * @param context The application context.
 */
void flippass_scene_password_entry_on_enter(void* context) {
    App* app = context;
    flippass_clear_text_buffer(app);
    flippass_log_event(app, "SCENE password_entry");
    if(!app->close_test_logged) {
        flippass_log_event(app, "CLOSE_TEST_START");
        app->close_test_logged = true;
    }
    flippass_scene_password_entry_set_header(app);
#if FLIPPASS_ENABLE_DEBUG_UNLOCK_HOOK
    if(flippass_scene_password_entry_try_debug_unlock(app)) {
        return;
    }
#endif
    text_input_set_result_callback(
        app->text_input,
        flippass_scene_password_entry_callback,
        app,
        app->text_buffer,
        TEXT_BUFFER_SIZE,
        true);
    view_dispatcher_switch_to_view(app->view_dispatcher, AppViewPasswordEntry);
}

/**
 * @brief Handler for the on_event event of the password entry scene.
 * @param context The application context.
 * @param event The event that was triggered.
 * @return True if the event was handled, false otherwise.
 */
bool flippass_scene_password_entry_on_event(void* context, SceneManagerEvent event) {
    App* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeBack) {
        flippass_clear_text_buffer(app);
        flippass_clear_master_password(app);
        if(!scene_manager_previous_scene(app->scene_manager)) {
            flippass_request_exit(app);
        }
        consumed = true;
    }

    return consumed;
}

/**
 * @brief Handler for the on_exit event of the password entry scene.
 * @param context The application context.
 */
void flippass_scene_password_entry_on_exit(void* context) {
    App* app = context;
    text_input_reset(app->text_input);
    flippass_clear_text_buffer(app);
}
