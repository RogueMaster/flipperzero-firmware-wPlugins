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
#include "../flippass_open_offers.h"
#include "flippass_scene.h"
#include "flippass_scene_status.h"
#include <flipper_format/flipper_format.h>
#include <stdio.h>
#include <toolbox/path.h>

typedef enum {
    FlipPassPasswordEntryStateInput = 0,
    FlipPassPasswordEntryStateDerivationPrompt,
    FlipPassPasswordEntryStateUncompressedPrompt,
} FlipPassPasswordEntryState;

static bool flippass_scene_password_entry_is_file_modify(const App* app) {
    return app->editor_mode == FlipPassEditorModeModifyDatabase &&
           app->editor_return_scene == FlipPassScene_FileBrowser;
}

static void flippass_scene_password_entry_dialog_callback(DialogExResult result, void* context) {
    App* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, result);
}

static void flippass_scene_password_entry_set_header(App* app) {
    furi_assert(app);

    if(app->idle_lock_active) {
        snprintf(app->password_header, sizeof(app->password_header), "%s", "Unlock Session");
        if(app->file_path != NULL && !furi_string_empty(app->file_path)) {
            FuriString* file_name = furi_string_alloc();
            path_extract_filename(app->file_path, file_name, true);
            if(!furi_string_empty(file_name)) {
                snprintf(
                    app->password_header,
                    sizeof(app->password_header),
                    "Unlock Session: %s",
                    furi_string_get_cstr(file_name));
            }
            furi_string_free(file_name);
        }
        text_input_set_header_text(app->text_input, app->password_header);
        return;
    }

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
            flippass_scene_password_entry_is_file_modify(app) ? "Pass for Mod. %s" : "Pass for %s",
            furi_string_get_cstr(file_name));
    }
    furi_string_free(file_name);

    text_input_set_header_text(app->text_input, app->password_header);
}

#if FLIPPASS_ENABLE_DEBUG_UNLOCK_HOOK
static bool flippass_scene_password_entry_bool_text(const char* text) {
    return text != NULL && (strcmp(text, "1") == 0 || strcmp(text, "true") == 0 ||
                            strcmp(text, "yes") == 0 || strcmp(text, "on") == 0);
}

static bool flippass_scene_password_entry_try_debug_bool(
    FlipperFormat* file,
    const char* key,
    FuriString* buffer,
    bool* out_value) {
    furi_assert(file);
    furi_assert(key);
    furi_assert(buffer);
    furi_assert(out_value);

    furi_string_reset(buffer);
    flipper_format_rewind(file);
    if(!flipper_format_read_string(file, key, buffer)) {
        return false;
    }

    *out_value = flippass_scene_password_entry_bool_text(furi_string_get_cstr(buffer));
    return true;
}

static bool flippass_scene_password_entry_try_debug_unlock(App* app) {
    bool triggered = false;
    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* file = flipper_format_file_alloc(storage);
    FuriString* password = furi_string_alloc();
    FuriString* backend = furi_string_alloc();
    FuriString* allow_ext = furi_string_alloc();
    FuriString* continue_on_ext = furi_string_alloc();
    FuriString* modify = furi_string_alloc();

    if(flipper_format_file_open_existing(file, FLIPPASS_DEBUG_UNLOCK_FILE_PATH) &&
       flippass_secure_read_encrypted_string(file, "password", password)) {
        flipper_format_rewind(file);
        if(flipper_format_read_string(file, "backend", backend)) {
            const KDBXVaultBackend parsed_backend =
                flippass_db_parse_backend_hint(furi_string_get_cstr(backend));
            if(parsed_backend != KDBXVaultBackendNone) {
                app->requested_vault_backend = parsed_backend;
            }
        }

        app->allow_ext_vault_promotion = app->always_allow_ext ||
                                         app->requested_vault_backend != KDBXVaultBackendRam;
        flippass_scene_password_entry_try_debug_bool(
            file, "allow_ext", allow_ext, &app->allow_ext_vault_promotion);
        flippass_scene_password_entry_try_debug_bool(
            file, "continue_on_ext", continue_on_ext, &app->debug_auto_continue_vault_fallback);
        bool debug_modify = false;
        flippass_scene_password_entry_try_debug_bool(file, "modify", modify, &debug_modify);
        if(debug_modify) {
            app->editor_mode = FlipPassEditorModeModifyDatabase;
            app->editor_parent_mode = FlipPassEditorModeNone;
            app->editor_text_target = FlipPassEditorTextTargetNone;
            app->editor_group = NULL;
            app->editor_entry = NULL;
            app->editor_selected_index = 4U;
            app->editor_return_scene = FlipPassScene_FileBrowser;
            app->editor_close_after_commit = true;
            app->editor_database_password[0] = '\0';
        }
        snprintf(
            app->master_password,
            sizeof(app->master_password),
            "%s",
            furi_string_get_cstr(password));
        FLIPPASS_DIAGNOSTIC_LOG(
            app,
            "DEBUG_UNLOCK_HOOK backend=%s allow_ext=%u continue_on_ext=%u",
            kdbx_vault_backend_label(app->requested_vault_backend),
            app->allow_ext_vault_promotion ? 1U : 0U,
            app->debug_auto_continue_vault_fallback ? 1U : 0U);
        triggered = true;
    }

    flipper_format_file_close(file);
    flipper_format_free(file);
    flippass_secure_delete_file_with_storage(storage, FLIPPASS_DEBUG_UNLOCK_FILE_PATH);
    furi_record_close(RECORD_STORAGE);
    furi_string_free(password);
    furi_string_free(backend);
    furi_string_free(allow_ext);
    furi_string_free(continue_on_ext);
    furi_string_free(modify);

    if(triggered) {
        flippass_clear_text_buffer(app);
        scene_manager_next_scene(app->scene_manager, FlipPassScene_DbEntries);
    }

    return triggered;
}
#endif

static void flippass_scene_password_entry_start_unlock(App* app) {
    furi_assert(app);

    const KDBXVaultBackend requested_backend = app->requested_vault_backend;
    const bool allow_ext_promotion = app->allow_ext_vault_promotion;
    flippass_reset_database(app);
    app->requested_vault_backend = requested_backend;
    app->allow_ext_vault_promotion = allow_ext_promotion;
    flippass_clear_text_buffer(app);
    scene_manager_next_scene(app->scene_manager, FlipPassScene_DbEntries);
}

static void flippass_scene_password_entry_trigger_unlock(App* app, const char* password) {
    furi_assert(app);
    furi_assert(password);

    if(password != app->master_password) {
        snprintf(app->master_password, sizeof(app->master_password), "%s", password);
    }
    flippass_scene_password_entry_start_unlock(app);
}

static void flippass_scene_password_entry_show_derivation_prompt(App* app) {
    furi_assert(app);

    dialog_ex_reset(app->dialog_ex);
    dialog_ex_set_header(app->dialog_ex, "Save Derivation?", 64, 4, AlignCenter, AlignTop);
    dialog_ex_set_text(
        app->dialog_ex, "Speed up future opens for this database?", 4, 18, AlignLeft, AlignTop);
    dialog_ex_set_left_button_text(app->dialog_ex, "Yes");
    dialog_ex_set_right_button_text(app->dialog_ex, "No");
    dialog_ex_set_result_callback(app->dialog_ex, flippass_scene_password_entry_dialog_callback);
    dialog_ex_set_context(app->dialog_ex, app);
    scene_manager_set_scene_state(
        app->scene_manager,
        FlipPassScene_PasswordEntry,
        FlipPassPasswordEntryStateDerivationPrompt);
    view_dispatcher_switch_to_view(app->view_dispatcher, AppViewDialogEx);
}

static void flippass_scene_password_entry_finish_derivation_prompt(App* app, bool save_cache) {
    furi_assert(app);

    app->derivation_cache_save_requested = save_cache;
    dialog_ex_reset(app->dialog_ex);
    scene_manager_set_scene_state(
        app->scene_manager, FlipPassScene_PasswordEntry, FlipPassPasswordEntryStateInput);
    flippass_scene_password_entry_start_unlock(app);
}

static void flippass_scene_password_entry_show_uncompressed_prompt(App* app) {
    dialog_ex_reset(app->dialog_ex);
    dialog_ex_set_header(app->dialog_ex, "Save Uncompressed?", 64, 4, AlignCenter, AlignTop);
    dialog_ex_set_text(
        app->dialog_ex,
        "Save without compression\nand open database?\nHistory is omitted.",
        4,
        18,
        AlignLeft,
        AlignTop);
    dialog_ex_set_left_button_text(app->dialog_ex, "Yes");
    dialog_ex_set_right_button_text(app->dialog_ex, "No");
    dialog_ex_set_result_callback(app->dialog_ex, flippass_scene_password_entry_dialog_callback);
    dialog_ex_set_context(app->dialog_ex, app);
    scene_manager_set_scene_state(
        app->scene_manager,
        FlipPassScene_PasswordEntry,
        FlipPassPasswordEntryStateUncompressedPrompt);
    view_dispatcher_switch_to_view(app->view_dispatcher, AppViewDialogEx);
}

static void flippass_scene_password_entry_finish_uncompressed_prompt(App* app, bool accepted) {
    app->save_uncompressed_requested = accepted;
    if(accepted) {
        app->editor_mode = FlipPassEditorModeNone;
        app->editor_parent_mode = FlipPassEditorModeNone;
        app->editor_text_target = FlipPassEditorTextTargetNone;
        app->editor_group = NULL;
        app->editor_entry = NULL;
        app->editor_selected_index = 0U;
        app->editor_return_scene = FlipPassScene_DbEntries;
        app->editor_close_after_commit = false;
        app->editor_database_password[0] = '\0';
    }
    dialog_ex_reset(app->dialog_ex);
    scene_manager_set_scene_state(
        app->scene_manager, FlipPassScene_PasswordEntry, FlipPassPasswordEntryStateInput);
    flippass_scene_password_entry_set_header(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, AppViewPasswordEntry);
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
    app->allow_ext_vault_promotion = app->always_allow_ext ||
                                     app->requested_vault_backend != KDBXVaultBackendRam;
    if(!app->save_uncompressed_requested && flippass_derivation_cache_should_offer(app)) {
        snprintf(app->master_password, sizeof(app->master_password), "%s", app->text_buffer);
        flippass_clear_text_buffer(app);
        flippass_scene_password_entry_show_derivation_prompt(app);
        return;
    }
    flippass_scene_password_entry_trigger_unlock(app, app->text_buffer);
}

static void flippass_scene_password_entry_idle_callback(void* context) {
    App* app = context;

    if(flippass_session_verify_password(app, app->text_buffer)) {
        app->idle_lock_active = false;
        app->idle_lock_failed_attempts = 0U;
        flippass_clear_text_buffer(app);
        flippass_clear_master_password(app);
        if(!scene_manager_previous_scene(app->scene_manager)) {
            flippass_request_exit(app);
        }
        return;
    }

    flippass_clear_text_buffer(app);
    flippass_clear_master_password(app);
    app->idle_lock_failed_attempts++;
    const uint8_t max_attempts = app->idle_unlock_attempts > 0U ?
                                     app->idle_unlock_attempts :
                                     FLIPPASS_DEFAULT_IDLE_UNLOCK_ATTEMPTS;
    if(app->idle_lock_failed_attempts >= max_attempts) {
        FLIPPASS_LOG_EVENT(app, "IDLE_LOCK_FAILED_CLOSE");
        flippass_close_database(app);
        scene_manager_search_and_switch_to_another_scene(
            app->scene_manager, FlipPassScene_FileBrowser);
        return;
    }

    flippass_scene_status_show(
        app, "Unlock Failed", "Wrong database password.", FlipPassScene_PasswordEntry);
    scene_manager_next_scene(app->scene_manager, FlipPassScene_Status);
}

/**
 * @brief Handler for the on_enter event of the password entry scene.
 * @param context The application context.
 */
void flippass_scene_password_entry_on_enter(void* context) {
    App* app = context;
    flippass_ui_ensure_text_input(app);
    flippass_ui_ensure_dialog_ex(app);
    flippass_clear_text_buffer(app);
    app->debug_auto_continue_vault_fallback = false;
    app->derivation_cache_save_requested = false;
    scene_manager_set_scene_state(
        app->scene_manager, FlipPassScene_PasswordEntry, FlipPassPasswordEntryStateInput);
    FLIPPASS_LOG_EVENT(app, "SCENE password_entry");
    if(!app->close_test_logged) {
        FLIPPASS_BENCH_LOG(app, "CLOSE_TEST_START");
        app->close_test_logged = true;
    }
    flippass_scene_password_entry_set_header(app);
    if(app->idle_lock_active) {
        text_input_set_result_callback(
            app->text_input,
            flippass_scene_password_entry_idle_callback,
            app,
            app->text_buffer,
            TEXT_BUFFER_SIZE,
            true);
        text_input_set_is_password(app->text_input, true);
        text_input_set_for_open(app->text_input, true);
        view_dispatcher_switch_to_view(app->view_dispatcher, AppViewPasswordEntry);
        return;
    }
#if FLIPPASS_ENABLE_DEBUG_UNLOCK_HOOK
    if(app->launch_request == NULL && flippass_scene_password_entry_try_debug_unlock(app)) {
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
    text_input_set_is_password(app->text_input, true);
    text_input_set_for_open(app->text_input, true);
    if(app->open_header_gzip) {
        app->open_header_gzip = false;
        const bool same_file = app->last_open_file_path != NULL &&
                               furi_string_cmp(app->last_open_file_path, app->file_path) == 0;
        if(flippass_open_offer_due(same_file ? app->last_open_count : 0U, 1U)) {
            flippass_scene_password_entry_show_uncompressed_prompt(app);
            return;
        }
    }
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
    const uint32_t scene_state =
        scene_manager_get_scene_state(app->scene_manager, FlipPassScene_PasswordEntry);

    if(scene_state == FlipPassPasswordEntryStateUncompressedPrompt) {
        if(event.type == SceneManagerEventTypeBack ||
           (event.type == SceneManagerEventTypeCustom && event.event == DialogExResultRight)) {
            flippass_scene_password_entry_finish_uncompressed_prompt(app, false);
        } else if(event.type == SceneManagerEventTypeCustom && event.event == DialogExResultLeft) {
            flippass_scene_password_entry_finish_uncompressed_prompt(app, true);
        }
        return true;
    }

    if(scene_state == FlipPassPasswordEntryStateDerivationPrompt) {
        if(event.type == SceneManagerEventTypeBack) {
            flippass_scene_password_entry_finish_derivation_prompt(app, false);
            return true;
        }

        if(event.type == SceneManagerEventTypeCustom) {
            if(event.event == DialogExResultLeft) {
                flippass_scene_password_entry_finish_derivation_prompt(app, true);
            } else if(event.event == DialogExResultRight) {
                flippass_scene_password_entry_finish_derivation_prompt(app, false);
            }
            return true;
        }
    }

    if(event.type == SceneManagerEventTypeBack) {
        app->save_uncompressed_requested = false;
        flippass_clear_text_buffer(app);
        flippass_clear_master_password(app);
        if(app->idle_lock_active) {
            flippass_request_exit(app);
            consumed = true;
            return consumed;
        }
        if(flippass_scene_password_entry_is_file_modify(app) && !app->database_loaded) {
            app->editor_mode = FlipPassEditorModeNone;
            app->editor_text_target = FlipPassEditorTextTargetNone;
            app->editor_group = NULL;
            app->editor_entry = NULL;
            app->editor_selected_index = 0U;
            app->editor_return_scene = FlipPassScene_FileBrowser;
            app->editor_close_after_commit = false;
        }
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
    if(app->text_input != NULL) {
        text_input_reset(app->text_input);
    }
    if(app->dialog_ex != NULL) {
        dialog_ex_reset(app->dialog_ex);
    }
    flippass_clear_text_buffer(app);
}
