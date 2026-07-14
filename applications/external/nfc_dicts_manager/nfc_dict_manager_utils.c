#include "nfc_dict_manager_utils.h"

void nfc_dict_manager_create_directories(Storage* storage) {
    storage_simply_mkdir(storage, DICT_FOLDER_PATH);
    storage_simply_mkdir(storage, BACKUP_FOLDER_PATH);
}

bool nfc_dict_manager_copy_file(Storage* storage, const char* src, const char* dst) {
    File* src_file = storage_file_alloc(storage);
    File* dst_file = storage_file_alloc(storage);

    bool success = false;

    if(storage_file_open(src_file, src, FSAM_READ, FSOM_OPEN_EXISTING) &&
       storage_file_open(dst_file, dst, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        uint8_t buffer[64];
        size_t bytes_read;
        success = true;

        while((bytes_read = storage_file_read(src_file, buffer, sizeof(buffer))) > 0) {
            if(storage_file_write(dst_file, buffer, bytes_read) != bytes_read) {
                success = false;
                break;
            }
        }
    }

    storage_file_close(src_file);
    storage_file_close(dst_file);
    storage_file_free(src_file);
    storage_file_free(dst_file);

    return success;
}

bool nfc_dict_manager_is_valid_key(const char* key) {
    size_t len = strlen(key);
    if(len != MAX_KEY_LENGTH) return false;

    for(size_t i = 0; i < len; i++) {
        char c = key[i];
        if(!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F'))) {
            return false;
        }
    }
    return true;
}

void nfc_dict_manager_normalize_key(char* key) {
    for(size_t i = 0; key[i]; i++) {
        if(key[i] >= 'a' && key[i] <= 'f') {
            key[i] = key[i] - 'a' + 'A';
        }
    }
}

bool nfc_dict_manager_merge_dictionaries(NfcDictManager* app) {
    FuriString* temp_path = furi_string_alloc();
    FuriString* merged_path = furi_string_alloc();
    FuriString* filename_a = furi_string_alloc();
    FuriString* filename_b = furi_string_alloc();

    bool success = false;

    path_extract_filename(app->selected_dict_a, filename_a, false);
    path_extract_filename(app->selected_dict_b, filename_b, false);

    if(furi_string_end_with_str(filename_a, ".nfc")) {
        furi_string_left(filename_a, furi_string_size(filename_a) - 4);
    }
    if(furi_string_end_with_str(filename_b, ".nfc")) {
        furi_string_left(filename_b, furi_string_size(filename_b) - 4);
    }

    furi_string_printf(temp_path, "%s/temp_merge.nfc", DICT_FOLDER_PATH);
    furi_string_printf(
        merged_path,
        "%s/%s_%s_merged.nfc",
        DICT_FOLDER_PATH,
        furi_string_get_cstr(filename_a),
        furi_string_get_cstr(filename_b));

    File* temp_file = storage_file_alloc(app->storage);

    if(storage_file_open(
           temp_file, furi_string_get_cstr(temp_path), FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        // file A
        if(nfc_dict_manager_append_file_to_file(app->storage, app->selected_dict_a, temp_file)) {
            // file B
            if(nfc_dict_manager_append_file_to_file(
                   app->storage, app->selected_dict_b, temp_file)) {
                storage_file_close(temp_file);

                success = nfc_dict_manager_optimize_file(app->storage, temp_path, merged_path);
            }
        }

        storage_file_close(temp_file);
    }

    storage_file_free(temp_file);

    storage_simply_remove(app->storage, furi_string_get_cstr(temp_path));

    furi_string_free(temp_path);
    furi_string_free(merged_path);
    furi_string_free(filename_a);
    furi_string_free(filename_b);

    return success;
}

bool nfc_dict_manager_optimize_single_dict(NfcDictManager* app, FuriString* dict_path) {
    FuriString* temp_path = furi_string_alloc();
    FuriString* filename = furi_string_alloc();
    FuriString* optimized_path = furi_string_alloc();

    bool success = false;

    path_extract_filename(dict_path, filename, false);
    if(furi_string_end_with_str(filename, ".nfc")) {
        furi_string_left(filename, furi_string_size(filename) - 4);
    }

    furi_string_printf(temp_path, "%s/temp_optimize.nfc", DICT_FOLDER_PATH);
    furi_string_printf(
        optimized_path, "%s/%s_optimized.nfc", DICT_FOLDER_PATH, furi_string_get_cstr(filename));

    File* temp_file = storage_file_alloc(app->storage);
    if(storage_file_open(
           temp_file, furi_string_get_cstr(temp_path), FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        Stream* source_stream = file_stream_alloc(app->storage);
        FuriString* line = furi_string_alloc();

        if(file_stream_open(
               source_stream, furi_string_get_cstr(dict_path), FSAM_READ, FSOM_OPEN_EXISTING)) {
            while(stream_read_line(source_stream, line)) {
                furi_string_trim(line);
                const char* key_str = furi_string_get_cstr(line);

                if(strlen(key_str) > 0) {
                    char key[MAX_KEY_LENGTH + 1];
                    strncpy(key, key_str, MAX_KEY_LENGTH);
                    key[MAX_KEY_LENGTH] = '\0';

                    nfc_dict_manager_normalize_key(key);

                    if(nfc_dict_manager_is_valid_key(key)) {
                        storage_file_write(temp_file, key, strlen(key));
                        storage_file_write(temp_file, "\n", 1);
                    }
                }
            }

            file_stream_close(source_stream);
            success = true;
        }

        stream_free(source_stream);
        furi_string_free(line);
        storage_file_close(temp_file);
    }

    storage_file_free(temp_file);

    if(success) {
        success =
            nfc_dict_manager_remove_duplicates_from_file(app->storage, temp_path, optimized_path);
    }

    storage_simply_remove(app->storage, furi_string_get_cstr(temp_path));

    furi_string_free(temp_path);
    furi_string_free(filename);
    furi_string_free(optimized_path);

    return success;
}

bool nfc_dict_manager_remove_duplicates_from_file(
    Storage* storage,
    FuriString* source_path,
    FuriString* dest_path) {
#define DEDUP_BUFFER_SIZE \
    1000 // se crasha ridurre a 750 poi 500 ma dovrebbe essere stabile anche a 1500 in teoria

    // allocazione dinamica
    char(*keys)[MAX_KEY_LENGTH + 1] = malloc(DEDUP_BUFFER_SIZE * (MAX_KEY_LENGTH + 1));
    int key_count = 0;

    if(!keys) {
        return false;
    }

    File* dest_file = storage_file_alloc(storage);
    FuriString* line = furi_string_alloc();
    bool success = false;

    if(storage_file_open(
           dest_file, furi_string_get_cstr(dest_path), FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        Stream* stream = file_stream_alloc(storage);

        if(file_stream_open(
               stream, furi_string_get_cstr(source_path), FSAM_READ, FSOM_OPEN_EXISTING)) {
            success = true;

            while(stream_read_line(stream, line)) {
                furi_string_trim(line);
                const char* key_str = furi_string_get_cstr(line);

                if(strlen(key_str) == MAX_KEY_LENGTH) {
                    bool duplicate = false;
                    for(int i = 0; i < key_count; i++) {
                        if(strcmp(keys[i], key_str) == 0) {
                            duplicate = true;
                            break;
                        }
                    }

                    if(!duplicate) {
                        if(key_count < DEDUP_BUFFER_SIZE) {
                            strcpy(keys[key_count], key_str);
                            key_count++;
                        } else {
                            for(int i = 0; i < key_count; i++) {
                                storage_file_write(dest_file, keys[i], MAX_KEY_LENGTH);
                                storage_file_write(dest_file, "\n", 1);
                            }
                            key_count = 0;
                            strcpy(keys[key_count], key_str);
                            key_count++;
                        }
                    }
                }
            }

            for(int i = 0; i < key_count; i++) {
                storage_file_write(dest_file, keys[i], MAX_KEY_LENGTH);
                storage_file_write(dest_file, "\n", 1);
            }

            file_stream_close(stream);
        }

        stream_free(stream);
        storage_file_close(dest_file);
    }

    free(keys);
    storage_file_free(dest_file);
    furi_string_free(line);

    return success;
}

bool nfc_dict_manager_backup_dictionaries(NfcDictManager* app) {
    DateTime datetime;
    furi_hal_rtc_get_datetime(&datetime);

    FuriString* backup_name = furi_string_alloc();
    FuriString* source_path = furi_string_alloc();
    FuriString* dest_path = furi_string_alloc();

    bool success = true;

    furi_string_printf(
        backup_name,
        "system_dict_%02d%02d%02d_%02d%02d.nfc",
        datetime.day,
        datetime.month,
        datetime.year % 100,
        datetime.hour,
        datetime.minute);

    furi_string_set(source_path, SYSTEM_DICT_PATH);
    furi_string_printf(dest_path, "%s/%s", BACKUP_FOLDER_PATH, furi_string_get_cstr(backup_name));

    if(storage_file_exists(app->storage, furi_string_get_cstr(source_path))) {
        success &= nfc_dict_manager_copy_file(
            app->storage, furi_string_get_cstr(source_path), furi_string_get_cstr(dest_path));
    }

    furi_string_printf(
        backup_name,
        "user_dict_%02d%02d%02d_%02d%02d.nfc",
        datetime.day,
        datetime.month,
        datetime.year % 100,
        datetime.hour,
        datetime.minute);

    furi_string_set(source_path, USER_DICT_PATH);
    furi_string_printf(dest_path, "%s/%s", BACKUP_FOLDER_PATH, furi_string_get_cstr(backup_name));

    if(storage_file_exists(app->storage, furi_string_get_cstr(source_path))) {
        success &= nfc_dict_manager_copy_file(
            app->storage, furi_string_get_cstr(source_path), furi_string_get_cstr(dest_path));
    }

    furi_string_free(backup_name);
    furi_string_free(source_path);
    furi_string_free(dest_path);

    return success;
}

bool nfc_dict_manager_append_file_to_file(
    Storage* storage,
    FuriString* source_path,
    File* dest_file) {
    File* source_file = storage_file_alloc(storage);
    bool success = false;

    if(storage_file_open(
           source_file, furi_string_get_cstr(source_path), FSAM_READ, FSOM_OPEN_EXISTING)) {
        uint8_t buffer[64];
        size_t bytes_read;
        success = true;

        while((bytes_read = storage_file_read(source_file, buffer, sizeof(buffer))) > 0) {
            if(storage_file_write(dest_file, buffer, bytes_read) != bytes_read) {
                success = false;
                break;
            }
        }

        storage_file_close(source_file);
    }

    storage_file_free(source_file);
    return success;
}

bool nfc_dict_manager_optimize_file(
    Storage* storage,
    FuriString* source_path,
    FuriString* dest_path) {
    File* dest_file = storage_file_alloc(storage);
    FuriString* line = furi_string_alloc();
    bool success = false;

    if(storage_file_open(
           dest_file, furi_string_get_cstr(dest_path), FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        Stream* stream = file_stream_alloc(storage);

        if(file_stream_open(
               stream, furi_string_get_cstr(source_path), FSAM_READ, FSOM_OPEN_EXISTING)) {
            success = true;

            while(stream_read_line(stream, line)) {
                furi_string_trim(line);
                const char* key_str = furi_string_get_cstr(line);

                if(strlen(key_str) > 0) {
                    char key[MAX_KEY_LENGTH + 1];
                    strncpy(key, key_str, MAX_KEY_LENGTH);
                    key[MAX_KEY_LENGTH] = '\0';

                    nfc_dict_manager_normalize_key(key);

                    if(nfc_dict_manager_is_valid_key(key)) {
                        storage_file_write(dest_file, key, strlen(key));
                        storage_file_write(dest_file, "\n", 1);
                    }
                }
            }

            file_stream_close(stream);
        }

        stream_free(stream);
        storage_file_close(dest_file);
    }

    storage_file_free(dest_file);
    furi_string_free(line);

    return success;
}

int nfc_dict_manager_count_lines_in_file(Storage* storage, FuriString* file_path) {
    Stream* stream = file_stream_alloc(storage);
    FuriString* line = furi_string_alloc();
    int count = 0;

    if(file_stream_open(stream, furi_string_get_cstr(file_path), FSAM_READ, FSOM_OPEN_EXISTING)) {
        while(stream_read_line(stream, line)) {
            count++;
        }
        file_stream_close(stream);
    }

    stream_free(stream);
    furi_string_free(line);
    return count;
}

void nfc_dict_manager_submenu_callback(void* context, uint32_t index) {
    NfcDictManager* app = context;
    scene_manager_handle_custom_event(app->scene_manager, index);
}

bool nfc_dict_manager_back_event_callback(void* context) {
    NfcDictManager* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

void nfc_dict_manager_dialog_callback(DialogExResult result, void* context) {
    NfcDictManager* app = context;
    scene_manager_handle_custom_event(app->scene_manager, result);
}

void nfc_dict_manager_success_timer_callback(void* context) {
    NfcDictManager* app = context;
    furi_timer_stop(app->success_timer);
    scene_manager_search_and_switch_to_previous_scene(
        app->scene_manager, NfcDictManagerSceneStart);
}

void nfc_dict_manager_merge_timer_callback(void* context) {
    NfcDictManager* app = context;

    if(app->merge_stage == 0) {
        bool success = nfc_dict_manager_merge_dictionaries(app);
        app->merge_stage = 1;

        if(success) {
            notification_message(app->notification, &sequence_success);
            popup_set_header(app->popup, "Merge complete!", 64, 10, AlignCenter, AlignTop);
            popup_set_text(
                app->popup, "Dictionaries merged\nsuccessfully!", 64, 32, AlignCenter, AlignCenter);
        } else {
            notification_message(app->notification, &sequence_error);
            popup_set_header(app->popup, "Merge failed!", 64, 10, AlignCenter, AlignTop);
            popup_set_text(
                app->popup, "Error merging\ndictionaries!", 64, 32, AlignCenter, AlignCenter);
        }

        furi_timer_start(app->merge_timer, 1000);

    } else {
        furi_timer_stop(app->merge_timer);
        scene_manager_search_and_switch_to_previous_scene(
            app->scene_manager, NfcDictManagerSceneStart);
    }
}

void nfc_dict_manager_optimize_timer_callback(void* context) {
    NfcDictManager* app = context;

    if(app->optimize_stage == 0) {
        bool success = nfc_dict_manager_optimize_single_dict(app, app->current_dict);
        app->optimize_stage = 1;

        if(success) {
            notification_message(app->notification, &sequence_success);
            popup_set_header(app->popup, "Optimization complete!", 64, 10, AlignCenter, AlignTop);
            popup_set_text(
                app->popup,
                "Dictionary optimized\nsuccessfully!",
                64,
                32,
                AlignCenter,
                AlignCenter);
        } else {
            notification_message(app->notification, &sequence_error);
            popup_set_header(app->popup, "Optimization failed!", 64, 10, AlignCenter, AlignTop);
            popup_set_text(
                app->popup, "Error optimizing\ndictionary!", 64, 32, AlignCenter, AlignCenter);
        }

        furi_timer_start(app->optimize_timer, 2000);

    } else {
        furi_timer_stop(app->optimize_timer);
        scene_manager_search_and_switch_to_previous_scene(
            app->scene_manager, NfcDictManagerSceneStart);
    }
}

void nfc_dict_manager_backup_timer_callback(void* context) {
    NfcDictManager* app = context;

    if(app->backup_stage == 0) {
        app->backup_success = nfc_dict_manager_backup_dictionaries(app);
        app->backup_stage = 1;

        if(app->backup_success) {
            notification_message(app->notification, &sequence_success);
            popup_set_header(app->popup, "Backup done!", 64, 10, AlignCenter, AlignTop);
            popup_set_text(
                app->popup,
                "Dictionaries backed up\nsuccessfully!",
                64,
                32,
                AlignCenter,
                AlignCenter);
        } else {
            notification_message(app->notification, &sequence_error);
            popup_set_header(app->popup, "Backup failed!", 64, 10, AlignCenter, AlignTop);
            popup_set_text(
                app->popup, "Error during backup\noperation!", 64, 32, AlignCenter, AlignCenter);
        }

        furi_timer_start(app->backup_timer, 1000);

    } else {
        furi_timer_stop(app->backup_timer);
        scene_manager_previous_scene(app->scene_manager);
    }
}
