#include "flippass_editor_crud_plugin.h"

#include "../kdbx/kdbx_constants.h"
#include "../kdbx/memzero.h"
#include "../scenes/flippass_scene.h"
#include "../scenes/flippass_scene_editor.h"
#include "../scenes/flippass_scene_other_fields.h"

#include <storage/storage.h>
#include <toolbox/path.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char* flippass_editor_crud_alloc_string(const char* value) {
    const char* source = value != NULL ? value : "";
    const size_t size = strlen(source) + 1U;
    char* copy = malloc(size);

    if(copy != NULL) {
        memcpy(copy, source, size);
    }

    return copy;
}

static void flippass_editor_crud_free_custom_field_draft(FlipPassEditorCustomFieldDraft* draft) {
    if(draft == NULL) {
        return;
    }

    if(draft->name != NULL) {
        memzero(draft->name, strlen(draft->name));
        free(draft->name);
    }
    if(draft->value != NULL) {
        memzero(draft->value, strlen(draft->value));
        free(draft->value);
    }
    memzero(draft, sizeof(*draft));
    free(draft);
}

static void flippass_editor_crud_clear_custom_field_drafts(App* app) {
    FlipPassEditorCustomFieldDraft* draft = app->editor_custom_fields;
    while(draft != NULL) {
        FlipPassEditorCustomFieldDraft* next = draft->next;
        flippass_editor_crud_free_custom_field_draft(draft);
        draft = next;
    }

    app->editor_custom_fields = NULL;
    app->editor_custom_field_draft = NULL;
}

static bool flippass_editor_crud_file_name_has_kdbx_extension(const char* name) {
    const char* extension = NULL;

    if(name == NULL) {
        return false;
    }

    extension = strrchr(name, '.');
    if(extension == NULL) {
        return false;
    }

    return (tolower((unsigned char)extension[1]) == 'k') &&
           (tolower((unsigned char)extension[2]) == 'd') &&
           (tolower((unsigned char)extension[3]) == 'b') &&
           (tolower((unsigned char)extension[4]) == 'x') && extension[5] == '\0';
}

static void flippass_editor_crud_clear_context(App* app) {
    flippass_editor_crud_clear_custom_field_drafts(app);
    app->editor_mode = FlipPassEditorModeNone;
    app->editor_parent_mode = FlipPassEditorModeNone;
    app->editor_text_target = FlipPassEditorTextTargetNone;
    app->editor_group = NULL;
    app->editor_entry = NULL;
    app->editor_custom_field = NULL;
    app->editor_custom_field_draft = NULL;
    app->editor_custom_field_protected = false;
    app->editor_custom_field_name[0] = '\0';
    app->editor_custom_field_value[0] = '\0';
    app->editor_otp_kind = FlipPassOtpKindTime;
    app->editor_otp_secret_encoding = FlipPassOtpSecretEncodingBase32;
    app->editor_otp_algorithm = FlipPassOtpAlgorithmSha1;
    app->editor_otp_digits = FLIPPASS_OTP_DEFAULT_DIGITS;
    app->editor_otp_period = FLIPPASS_OTP_DEFAULT_PERIOD;
    app->editor_otp_time_zone_minutes = 0;
    app->editor_otp_settled = false;
    app->editor_otp_secret[0] = '\0';
    snprintf(
        app->editor_otp_counter,
        sizeof(app->editor_otp_counter),
        "%llu",
        (unsigned long long)FLIPPASS_OTP_DEFAULT_COUNTER);
    app->password_gen_target = FlipPassPasswordGenTargetNone;
    app->password_gen_capture_active = false;
    app->password_gen_auto_open_field_name = false;
    app->editor_selected_index = 0U;
    app->editor_return_scene = FlipPassScene_FileBrowser;
    app->editor_idle_lock_minutes = app->idle_lock_minutes;
    app->editor_idle_unlock_attempts = app->idle_unlock_attempts;
    app->editor_idle_exit_minutes = app->idle_exit_minutes;
    app->editor_keyboard_layout_index = 0U;
    app->editor_keyboard_layout_use_alt = true;
    app->editor_keyboard_layout_available = false;
    app->editor_keyboard_layout_path[0] = '\0';
    app->editor_close_after_commit = false;
}

static void flippass_editor_crud_compose_file_name(FuriString* out, const char* name) {
    furi_string_reset(out);
    if(name == NULL || name[0] == '\0') {
        return;
    }

    furi_string_set_str(out, name);
    if(!flippass_editor_crud_file_name_has_kdbx_extension(name)) {
        furi_string_cat_str(out, ".kdbx");
    }
}

static bool flippass_editor_crud_validate_file_component(
    const char* text,
    FuriString* error,
    const char* required_message) {
    if(text == NULL || text[0] == '\0') {
        furi_string_set_str(error, required_message);
        return false;
    }

    for(const char* cursor = text; *cursor != '\0'; cursor++) {
        if(strchr("<>:\"/\\|?*", *cursor) != NULL) {
            furi_string_set_str(error, "Use a valid file name.");
            return false;
        }
    }

    return true;
}

static bool flippass_editor_crud_custom_field_name_is_otp_reserved(const char* name) {
    static const char* reserved[] = {
        "HmacOtp-Secret",
        "HmacOtp-Secret-Hex",
        "HmacOtp-Secret-Base32",
        "HmacOtp-Secret-Base64",
        "HmacOtp-Counter",
        "TimeOtp-Secret",
        "TimeOtp-Secret-Hex",
        "TimeOtp-Secret-Base32",
        "TimeOtp-Secret-Base64",
        "TimeOtp-Length",
        "TimeOtp-Period",
        "TimeOtp-Algorithm",
    };

    for(size_t index = 0U; index < COUNT_OF(reserved); index++) {
        if(name != NULL && strcmp(name, reserved[index]) == 0) {
            return true;
        }
    }
    return false;
}

static bool flippass_editor_crud_custom_field_name_is_standard(const char* name) {
    return name != NULL && (strcmp(name, "Title") == 0 || strcmp(name, "UserName") == 0 ||
                            strcmp(name, "Password") == 0 || strcmp(name, "URL") == 0 ||
                            strcmp(name, "Notes") == 0 || strcmp(name, "UUID") == 0 ||
                            strcmp(name, "AutoType") == 0 ||
                            flippass_editor_crud_custom_field_name_is_otp_reserved(name));
}

static bool flippass_editor_crud_custom_field_name_exists_in_drafts(
    const App* app,
    const char* name,
    const FlipPassEditorCustomFieldDraft* exclude) {
    if(app == NULL || name == NULL || name[0] == '\0') {
        return false;
    }

    for(const FlipPassEditorCustomFieldDraft* draft = app->editor_custom_fields; draft != NULL;
        draft = draft->next) {
        if(draft != exclude && draft->name != NULL && strcmp(draft->name, name) == 0) {
            return true;
        }
    }

    return false;
}

static bool flippass_editor_crud_validate_custom_field_form(App* app, FuriString* error) {
    if(app->editor_custom_field_name[0] == '\0') {
        furi_string_set_str(error, "Field name is required.");
        return false;
    }

    if(app->editor_custom_field_value[0] == '\0') {
        furi_string_set_str(error, "Field value is required.");
        return false;
    }

    if(flippass_editor_crud_custom_field_name_is_standard(app->editor_custom_field_name)) {
        furi_string_set_str(error, "Use a custom field name.");
        return false;
    }

    if((app->editor_entry == NULL) &&
       flippass_editor_crud_custom_field_name_exists_in_drafts(
           app, app->editor_custom_field_name, app->editor_custom_field_draft)) {
        furi_string_set_str(error, "Field name already exists.");
        return false;
    }

    return true;
}

static bool flippass_editor_crud_save_draft_custom_field(App* app, FuriString* error) {
    FlipPassEditorCustomFieldDraft* draft = app->editor_custom_field_draft;

    if(draft == NULL) {
        draft = malloc(sizeof(FlipPassEditorCustomFieldDraft));
        if(draft == NULL) {
            furi_string_set_str(error, "Not enough RAM to add the field.");
            return false;
        }
        memset(draft, 0, sizeof(*draft));
        draft->next = app->editor_custom_fields;
        app->editor_custom_fields = draft;
    }

    char* name = flippass_editor_crud_alloc_string(app->editor_custom_field_name);
    char* value = flippass_editor_crud_alloc_string(app->editor_custom_field_value);
    if(name == NULL || value == NULL) {
        if(name != NULL) {
            free(name);
        }
        if(value != NULL) {
            free(value);
        }
        furi_string_set_str(error, "Not enough RAM to update the field.");
        return false;
    }

    if(draft->name != NULL) {
        memzero(draft->name, strlen(draft->name));
        free(draft->name);
    }
    if(draft->value != NULL) {
        memzero(draft->value, strlen(draft->value));
        free(draft->value);
    }

    draft->name = name;
    draft->value = value;
    draft->protected_value = app->editor_custom_field_protected;
    return true;
}

static bool flippass_editor_crud_delete_draft_custom_field(App* app, FuriString* error) {
    FlipPassEditorCustomFieldDraft* draft = app->editor_custom_field_draft;

    if(draft == NULL) {
        furi_string_set_str(error, "No draft field is selected.");
        return false;
    }

    FlipPassEditorCustomFieldDraft** link = &app->editor_custom_fields;
    while(*link != NULL && *link != draft) {
        link = &(*link)->next;
    }

    if(*link == NULL) {
        furi_string_set_str(error, "The draft field could not be found.");
        return false;
    }

    *link = draft->next;
    flippass_editor_crud_free_custom_field_draft(draft);
    app->editor_custom_field_draft = NULL;
    return true;
}

static void flippass_editor_crud_restore_parent_mode(App* app) {
    if(app->editor_parent_mode == FlipPassEditorModeAddEntry ||
       app->editor_parent_mode == FlipPassEditorModeEditEntry) {
        app->editor_mode = app->editor_parent_mode;
    }
    app->editor_parent_mode = FlipPassEditorModeNone;
    app->editor_custom_field = NULL;
    app->editor_custom_field_draft = NULL;
    app->editor_text_target = FlipPassEditorTextTargetNone;
    app->password_gen_auto_open_field_name = false;
    app->editor_selected_index = FlipPassEditorEntryRowOtherFields;
}

static bool flippass_editor_crud_validate_host(const FlipPassEditorCrudHostApiV1* host_api) {
    return host_api != NULL && host_api->api_version == FLIPPASS_EDITOR_CRUD_HOST_API_VERSION &&
           host_api->create_group != NULL && host_api->update_group != NULL &&
           host_api->delete_group != NULL && host_api->create_entry != NULL &&
           host_api->update_entry != NULL && host_api->delete_entry != NULL &&
           host_api->create_custom_field != NULL && host_api->update_custom_field != NULL &&
           host_api->delete_custom_field != NULL && host_api->save_settings != NULL &&
           host_api->show_status != NULL;
}

static void flippass_editor_crud_show_status(
    const FlipPassEditorCrudHostApiV1* host_api,
    const char* title,
    const char* message,
    uint32_t return_scene) {
    host_api->show_status(host_api->context, title, message, return_scene);
}

static bool
    flippass_editor_crud_execute_commit(App* app, const FlipPassEditorCrudHostApiV1* host_api) {
    FuriString* error = furi_string_alloc();
    FuriString* file_name = furi_string_alloc();
    FuriString* target_path = furi_string_alloc();
    FuriString* dirname = furi_string_alloc();
    Storage* storage = NULL;
    bool ok = false;

    if(app == NULL || !flippass_editor_crud_validate_host(host_api)) {
        if(error != NULL) {
            furi_string_set_str(error, "FlipPass editor CRUD plugin received an invalid request.");
        }
        goto cleanup;
    }

    switch(app->editor_mode) {
    case FlipPassEditorModeNewDirectory:
        storage = furi_record_open(RECORD_STORAGE);
        if(!flippass_editor_crud_validate_file_component(
               app->editor_group_name, error, "Directory name is required.")) {
            break;
        }
        path_concat(
            furi_string_get_cstr(app->browser_directory), app->editor_group_name, target_path);
        if(storage_common_stat(storage, furi_string_get_cstr(target_path), NULL) == FSE_OK) {
            furi_string_set_str(error, "A file or directory with that name already exists.");
            break;
        }
        if(!storage_simply_mkdir(storage, furi_string_get_cstr(target_path))) {
            furi_string_set_str(error, "The directory could not be created.");
            break;
        }
        furi_string_set(app->browser_directory, target_path);
        app->browser_directory_selected_index = 0U;
        ok = true;
        if(ok) {
            flippass_editor_crud_clear_context(app);
            scene_manager_search_and_switch_to_previous_scene(
                app->scene_manager, FlipPassScene_FileBrowser);
        }
        break;
    case FlipPassEditorModeAddGroup:
        ok = host_api->create_group(
            host_api->context,
            app->editor_group != NULL ? app->editor_group : app->current_group,
            app->editor_group_name,
            NULL,
            error);
        if(ok) {
            flippass_editor_crud_clear_context(app);
            scene_manager_previous_scene(app->scene_manager);
        }
        break;
    case FlipPassEditorModeEditGroup:
        ok = host_api->update_group(
            host_api->context, app->editor_group, app->editor_group_name, error);
        if(ok) {
            flippass_editor_crud_clear_context(app);
            scene_manager_previous_scene(app->scene_manager);
        }
        break;
    case FlipPassEditorModeAddEntry: {
        KDBXEntry* created_entry = NULL;
        ok = host_api->create_entry(
            host_api->context,
            app->editor_group != NULL ? app->editor_group : app->current_group,
            app->editor_entry_title,
            app->editor_entry_username,
            app->editor_entry_password,
            app->editor_entry_url,
            app->editor_entry_notes,
            app->editor_entry_autotype,
            &created_entry,
            error);
        for(FlipPassEditorCustomFieldDraft* draft = app->editor_custom_fields; ok && draft != NULL;
            draft = draft->next) {
            ok = host_api->create_custom_field(
                host_api->context,
                created_entry,
                draft->name,
                draft->value,
                draft->protected_value,
                NULL,
                error);
        }
        if(ok) {
            flippass_editor_crud_clear_context(app);
            scene_manager_previous_scene(app->scene_manager);
        }
        break;
    }
    case FlipPassEditorModeEditEntry:
        ok = host_api->update_entry(
            host_api->context,
            app->editor_entry,
            app->editor_entry_title,
            app->editor_entry_username,
            app->editor_entry_password,
            app->editor_entry_url,
            app->editor_entry_notes,
            app->editor_entry_autotype,
            error);
        if(ok) {
            flippass_editor_crud_clear_context(app);
            scene_manager_previous_scene(app->scene_manager);
        }
        break;
    case FlipPassEditorModeAddCustomField:
    case FlipPassEditorModeEditCustomField:
        if(!flippass_editor_crud_validate_custom_field_form(app, error)) {
            break;
        }
        if(app->editor_entry == NULL) {
            ok = flippass_editor_crud_save_draft_custom_field(app, error);
        } else if(app->editor_mode == FlipPassEditorModeAddCustomField) {
            ok = host_api->create_custom_field(
                host_api->context,
                app->editor_entry,
                app->editor_custom_field_name,
                app->editor_custom_field_value,
                app->editor_custom_field_protected,
                NULL,
                error);
        } else {
            ok = host_api->update_custom_field(
                host_api->context,
                app->editor_entry,
                app->editor_custom_field,
                app->editor_custom_field_name,
                app->editor_custom_field_value,
                app->editor_custom_field_protected,
                error);
        }
        if(ok) {
            scene_manager_set_scene_state(
                app->scene_manager, FlipPassScene_OtherFields, FlipPassOtherFieldsModeEditNoAuto);
            flippass_editor_crud_restore_parent_mode(app);
            scene_manager_search_and_switch_to_previous_scene(
                app->scene_manager, FlipPassScene_OtherFields);
        }
        break;
    case FlipPassEditorModeRenameFile:
        storage = furi_record_open(RECORD_STORAGE);
        flippass_editor_crud_compose_file_name(file_name, app->editor_file_name);
        path_extract_dirname(furi_string_get_cstr(app->pending_path), dirname);
        path_concat(furi_string_get_cstr(dirname), furi_string_get_cstr(file_name), target_path);
        if(strcmp(furi_string_get_cstr(app->pending_path), furi_string_get_cstr(target_path)) ==
           0) {
            ok = true;
        } else if(storage_file_exists(storage, furi_string_get_cstr(target_path))) {
            furi_string_set_str(error, "A database with that name already exists.");
        } else if(
            storage_common_rename(
                storage,
                furi_string_get_cstr(app->pending_path),
                furi_string_get_cstr(target_path)) != FSE_OK) {
            furi_string_set_str(error, "The selected database could not be renamed.");
        } else {
            if(strcmp(
                   furi_string_get_cstr(app->last_open_file_path),
                   furi_string_get_cstr(app->pending_path)) == 0) {
                furi_string_set(app->last_open_file_path, target_path);
                host_api->save_settings(host_api->context);
            }
            if(strcmp(
                   furi_string_get_cstr(app->file_path),
                   furi_string_get_cstr(app->pending_path)) == 0) {
                furi_string_set(app->file_path, target_path);
            }
            furi_string_set(app->pending_path, target_path);
            ok = true;
        }
        if(ok) {
            flippass_editor_crud_clear_context(app);
            scene_manager_search_and_switch_to_previous_scene(
                app->scene_manager, FlipPassScene_FileBrowser);
        }
        break;
    case FlipPassEditorModeNewDatabase:
    case FlipPassEditorModeModifyDatabase:
        furi_string_set_str(error, "Database save is handled by the host.");
        break;
    case FlipPassEditorModeNone:
    default:
        break;
    }

cleanup:
    if(storage != NULL) {
        furi_record_close(RECORD_STORAGE);
    }

    if(!ok && error != NULL && !furi_string_empty(error)) {
        const char* title = "Edit Failed";
        if(app != NULL && (app->editor_mode == FlipPassEditorModeNewDatabase ||
                           app->editor_mode == FlipPassEditorModeModifyDatabase)) {
            title = "Save Failed";
        } else if(
            app != NULL && (app->editor_mode == FlipPassEditorModeNewDirectory ||
                            app->editor_mode == FlipPassEditorModeAddGroup ||
                            app->editor_mode == FlipPassEditorModeAddEntry ||
                            app->editor_mode == FlipPassEditorModeAddCustomField)) {
            title = "Create Failed";
        } else if(app != NULL && app->editor_mode == FlipPassEditorModeRenameFile) {
            title = "Rename Failed";
        }
        if(app != NULL && host_api != NULL && flippass_editor_crud_validate_host(host_api)) {
            flippass_editor_crud_show_status(
                host_api, title, furi_string_get_cstr(error), FlipPassScene_Editor);
            scene_manager_next_scene(app->scene_manager, FlipPassScene_Status);
        }
    }

    furi_string_free(error);
    furi_string_free(file_name);
    furi_string_free(target_path);
    furi_string_free(dirname);
    return ok;
}

static bool flippass_editor_crud_execute_delete(
    App* app,
    FlipPassEditorCrudDeleteTarget target,
    const FlipPassEditorCrudHostApiV1* host_api) {
    FuriString* error = furi_string_alloc();
    bool ok = false;

    if(app == NULL || !flippass_editor_crud_validate_host(host_api)) {
        furi_string_set_str(error, "FlipPass editor CRUD plugin received an invalid request.");
        goto cleanup;
    }

    switch(target) {
    case FlipPassEditorCrudDeleteGroup:
        ok = host_api->delete_group(host_api->context, app->editor_group, error);
        if(ok) {
            flippass_editor_crud_clear_context(app);
            scene_manager_search_and_switch_to_previous_scene(
                app->scene_manager, FlipPassScene_DbEntries);
        }
        break;
    case FlipPassEditorCrudDeleteEntry:
        ok = host_api->delete_entry(host_api->context, app->editor_entry, error);
        if(ok) {
            flippass_editor_crud_clear_context(app);
            scene_manager_search_and_switch_to_previous_scene(
                app->scene_manager, FlipPassScene_DbEntries);
        }
        break;
    case FlipPassEditorCrudDeleteField:
        if(app->editor_entry == NULL) {
            ok = flippass_editor_crud_delete_draft_custom_field(app, error);
        } else {
            ok = host_api->delete_custom_field(
                host_api->context, app->editor_entry, app->editor_custom_field, error);
        }
        if(ok) {
            scene_manager_set_scene_state(
                app->scene_manager, FlipPassScene_OtherFields, FlipPassOtherFieldsModeEditNoAuto);
            flippass_editor_crud_restore_parent_mode(app);
            scene_manager_search_and_switch_to_previous_scene(
                app->scene_manager, FlipPassScene_OtherFields);
        }
        break;
    case FlipPassEditorCrudDeleteNone:
    default:
        break;
    }

cleanup:
    if(!ok && app != NULL && !furi_string_empty(error) && host_api != NULL &&
       flippass_editor_crud_validate_host(host_api)) {
        flippass_editor_crud_show_status(
            host_api, "Delete Failed", furi_string_get_cstr(error), FlipPassScene_Editor);
        scene_manager_next_scene(app->scene_manager, FlipPassScene_Status);
    }

    furi_string_free(error);
    return ok;
}

static const FlipPassEditorCrudPluginV1 flippass_editor_crud_plugin = {
    .api_version = FLIPPASS_EDITOR_CRUD_PLUGIN_API_VERSION,
    .execute_commit = flippass_editor_crud_execute_commit,
    .execute_delete = flippass_editor_crud_execute_delete,
};

static const FlipperAppPluginDescriptor flippass_editor_crud_descriptor = {
    .appid = FLIPPASS_EDITOR_CRUD_PLUGIN_APP_ID,
    .ep_api_version = FLIPPASS_EDITOR_CRUD_PLUGIN_API_VERSION,
    .entry_point = &flippass_editor_crud_plugin,
};

const FlipperAppPluginDescriptor* flippass_editor_crud_plugin_ep(void) {
    return &flippass_editor_crud_descriptor;
}
