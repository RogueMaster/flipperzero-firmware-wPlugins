#include "subghz_editor_file.h"

#define TAG "SubGhzEditorFile"

void subghz_editor_model_init(SubGhzEditorModel* model) {
    model->file_path = furi_string_alloc();
    model->file_name = furi_string_alloc();
    model->protocol = furi_string_alloc();
    model->preset_str = furi_string_alloc();
    subghz_editor_model_reset(model);
}

void subghz_editor_model_reset(SubGhzEditorModel* model) {
    furi_string_set_str(model->file_path, SUBGHZ_EDITOR_APP_PATH_FOLDER);
    furi_string_reset(model->file_name);
    furi_string_reset(model->protocol);
    furi_string_reset(model->preset_str);
    memset(model->key_buf, 0, sizeof(model->key_buf));
    memset(model->temp_key_buf, 0, sizeof(model->temp_key_buf));
    memset(model->freq_input_buf, 0, sizeof(model->freq_input_buf));
    memset(model->custom_freq_str, 0, sizeof(model->custom_freq_str));
    model->frequency = 433920000;
    model->initial_frequency = 433920000;
    model->current_freq_index = 6; // default 433.92
    model->is_custom_freq = false;
    model->is_file_loaded = false;
    model->has_key = false;
    model->has_frequency = false;
    model->modified = false;
}

void subghz_editor_model_free(SubGhzEditorModel* model) {
    furi_string_free(model->file_path);
    furi_string_free(model->file_name);
    furi_string_free(model->protocol);
    furi_string_free(model->preset_str);
}

bool subghz_editor_file_load(SubGhzEditorApp* app, const char* path) {
    if(!path || !app) {
        FURI_LOG_E(TAG, "Invalid load arguments: path=%p, app=%p", path, app);
        return false;
    }

    FURI_LOG_I(TAG, "Loading file: %s", path);

    // Make local safe copy of path to avoid use-after-free if path aliases app->model.file_path
    char path_copy[256];
    strncpy(path_copy, path, sizeof(path_copy) - 1);
    path_copy[sizeof(path_copy) - 1] = '\0';

    FlipperFormat* fff = flipper_format_file_alloc(app->storage);
    bool success = false;

    do {
        if(!flipper_format_file_open_existing(fff, path_copy)) {
            FURI_LOG_E(TAG, "Failed to open existing file: %s", path_copy);
            break;
        }

        FuriString* header_type = furi_string_alloc();
        uint32_t version = 0;
        if(!flipper_format_read_header(fff, header_type, &version)) {
            FURI_LOG_E(TAG, "Invalid header in file %s", path_copy);
            furi_string_free(header_type);
            break;
        }
        FURI_LOG_I(
            TAG,
            "File header verified: %s, version %lu",
            furi_string_get_cstr(header_type),
            (unsigned long)version);
        furi_string_free(header_type);

        // Reset previous loaded state safely
        subghz_editor_model_reset(&app->model);
        furi_string_set_str(app->model.file_path, path_copy);
        path_extract_filename(app->model.file_path, app->model.file_name, false);

        // Read Protocol
        flipper_format_rewind(fff);
        if(flipper_format_read_string(fff, "Protocol", app->model.protocol)) {
            FURI_LOG_I(TAG, "Protocol: %s", furi_string_get_cstr(app->model.protocol));
        } else {
            furi_string_set_str(app->model.protocol, "Unknown");
            FURI_LOG_W(TAG, "Protocol tag not found, defaulting to Unknown");
        }

        // Read Preset
        flipper_format_rewind(fff);
        if(flipper_format_read_string(fff, "Preset", app->model.preset_str)) {
            FURI_LOG_I(TAG, "Preset: %s", furi_string_get_cstr(app->model.preset_str));
        }

        // Read Frequency
        flipper_format_rewind(fff);
        if(flipper_format_read_uint32(fff, "Frequency", &app->model.frequency, 1)) {
            app->model.has_frequency = true;
            app->model.initial_frequency = app->model.frequency;
            FURI_LOG_I(TAG, "Frequency found: %lu Hz", (unsigned long)app->model.frequency);

            // Match frequency against presets
            bool match = false;
            for(size_t i = 0; i < SUBGHZ_PRESETS_COUNT; i++) {
                if(subghz_presets[i].frequency == app->model.frequency) {
                    app->model.current_freq_index = (uint8_t)i;
                    app->model.is_custom_freq = false;
                    match = true;
                    FURI_LOG_I(TAG, "Matched standard preset: %s", subghz_presets[i].str);
                    break;
                }
            }
            if(!match) {
                app->model.is_custom_freq = true;
                snprintf(
                    app->model.custom_freq_str,
                    sizeof(app->model.custom_freq_str),
                    "%lu.%02lu MHz",
                    (unsigned long)(app->model.frequency / 1000000),
                    (unsigned long)((app->model.frequency % 1000000) / 10000));
                FURI_LOG_I(TAG, "Custom frequency detected: %s", app->model.custom_freq_str);
            }
        } else {
            FURI_LOG_W(TAG, "Frequency tag not found");
        }

        // Read Key
        flipper_format_rewind(fff);
        FuriString* key_temp = furi_string_alloc();
        if(flipper_format_read_string(fff, "Key", key_temp)) {
            const char* key_cstr = furi_string_get_cstr(key_temp);
            if(strlen(key_cstr) > 0) {
                snprintf(app->model.key_buf, sizeof(app->model.key_buf), "%s", key_cstr);
                app->model.has_key = true;
                FURI_LOG_I(TAG, "Key loaded: %s", app->model.key_buf);
            }
        } else {
            FURI_LOG_I(TAG, "No Key tag in this protocol/file");
        }
        furi_string_free(key_temp);

        app->model.is_file_loaded = true;
        app->model.modified = false;
        success = true;
        FURI_LOG_I(TAG, "File load complete: %s", furi_string_get_cstr(app->model.file_name));
    } while(0);

    flipper_format_free(fff);
    return success;
}

bool subghz_editor_file_save(SubGhzEditorApp* app, const char* target_path) {
    if(!app || !app->model.is_file_loaded || !target_path) {
        FURI_LOG_E(TAG, "Save failed: invalid parameters");
        return false;
    }

    FURI_LOG_I(TAG, "Saving to %s", target_path);
    FlipperFormat* fff = flipper_format_file_alloc(app->storage);
    bool success = false;

    do {
        if(!flipper_format_file_open_existing(fff, target_path)) {
            FURI_LOG_E(TAG, "Save: Failed to open %s", target_path);
            break;
        }

        // Update Frequency
        if(app->model.has_frequency) {
            flipper_format_rewind(fff);
            if(!flipper_format_update_uint32(fff, "Frequency", &app->model.frequency, 1)) {
                FURI_LOG_W(TAG, "Could not update Frequency key");
            } else {
                FURI_LOG_I(TAG, "Saved Frequency: %lu Hz", (unsigned long)app->model.frequency);
            }
        }

        // Update Key
        if(app->model.has_key && strlen(app->model.key_buf) > 0) {
            flipper_format_rewind(fff);
            if(!flipper_format_update_string_cstr(fff, "Key", app->model.key_buf)) {
                FURI_LOG_W(TAG, "Could not update Key");
            } else {
                FURI_LOG_I(TAG, "Saved Key: %s", app->model.key_buf);
            }
        }

        app->model.modified = false;
        success = true;
        FURI_LOG_I(TAG, "Save successful: %s", target_path);
    } while(0);

    flipper_format_free(fff);
    return success;
}

bool subghz_editor_file_save_copy(SubGhzEditorApp* app) {
    if(!app || !app->model.is_file_loaded) {
        FURI_LOG_E(TAG, "Save copy failed: no file loaded");
        return false;
    }

    const char* src_path = furi_string_get_cstr(app->model.file_path);
    FURI_LOG_I(TAG, "Starting save copy from source: %s", src_path);

    FuriString* dst_path = furi_string_alloc();
    FuriString* dirname = furi_string_alloc();
    FuriString* basename = furi_string_alloc();

    path_extract_dirname(src_path, dirname);
    path_extract_filename_no_ext(src_path, basename);

    furi_string_printf(
        dst_path,
        "%s/%s_edit%s",
        furi_string_get_cstr(dirname),
        furi_string_get_cstr(basename),
        SUBGHZ_EDITOR_APP_EXTENSION);

    FURI_LOG_I(TAG, "Target copy path: %s", furi_string_get_cstr(dst_path));

    // Copy original file first to keep all headers and comments intact
    FS_Error fs_res = storage_common_copy(app->storage, src_path, furi_string_get_cstr(dst_path));

    bool success = false;
    if(fs_res == FSE_OK) {
        success = subghz_editor_file_save(app, furi_string_get_cstr(dst_path));
        FURI_LOG_I(TAG, "Save copy completed successfully: %d", success);
    } else {
        FURI_LOG_E(TAG, "Storage copy failed with code %d", fs_res);
    }

    furi_string_free(dst_path);
    furi_string_free(dirname);
    furi_string_free(basename);

    return success;
}
