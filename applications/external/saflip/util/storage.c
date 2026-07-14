// Based on code from https://github.com/bettse/picopass

#include "storage.h"
#include "flipper_format.h"

static const char* saflip_file_header = "Saflip Credential File";
static const uint32_t saflip_file_version = 1;

// TODO: Is there a better way to do this than having each item listed separately?

FuriString* datetime_datetime_to_string(DateTime* datetime) {
    furi_check(datetime);

    return furi_string_alloc_printf(
        "%04hu-%02hhu-%02hhu %02hhu:%02hhu:%02hhu",
        datetime->year,
        datetime->month,
        datetime->day,
        datetime->hour,
        datetime->minute,
        datetime->second);
}

void datetime_string_to_datetime(FuriString* string, DateTime* datetime) {
    furi_check(datetime);
    furi_check(string);

    sscanf(
        furi_string_get_cstr(string),
        "%04hu-%02hhu-%02hhu %02hhu:%02hhu:%02hhu",
        &datetime->year,
        &datetime->month,
        &datetime->day,
        &datetime->hour,
        &datetime->minute,
        &datetime->second);
}

bool saflip_load_file(SaflipApp* app, const char* path) {
    bool parsed = false;
    FlipperFormat* file = flipper_format_file_alloc(app->storage);
    FuriString* temp_str = furi_string_alloc();
    bool deprecated_version = false;

    do {
        if(!flipper_format_file_open_existing(file, path)) break;

        // Read and verify file header
        uint32_t version = 0;
        if(!flipper_format_read_header(file, temp_str, &version)) break;
        if(!furi_string_equal_str(temp_str, saflip_file_header) ||
           (version != saflip_file_version)) {
            deprecated_version = true;
            break;
        }

        // Parse file
        uint32_t temp_uint32;
        bool temp_bool;

        if(flipper_format_read_uint32(file, "UID Length", &temp_uint32, 1)) {
            app->uid_len = temp_uint32;
        } else
            break;
        if(!flipper_format_read_hex(file, "UID", app->uid, app->uid_len)) break;

        if(flipper_format_read_uint32(file, "Format", &temp_uint32, 1)) {
            app->data->format = temp_uint32;
        } else
            break;
        if(flipper_format_read_uint32(file, "Card Level", &temp_uint32, 1)) {
            app->data->card_level = temp_uint32;
        } else
            break;

        if(flipper_format_read_uint32(file, "Card Type", &temp_uint32, 1)) {
            app->data->card_type = temp_uint32;
        } else
            break;

        if(flipper_format_read_uint32(file, "Card ID", &temp_uint32, 1)) {
            app->data->card_id = temp_uint32;
        } else
            break;

        if(flipper_format_read_bool(file, "Is Opening Key", &temp_bool, 1)) {
            app->data->opening_key = temp_bool;
        } else
            break;

        if(flipper_format_read_uint32(file, "Key/Lock ID", &temp_uint32, 1)) {
            app->data->lock_id = temp_uint32;
        } else
            break;

        if(flipper_format_read_uint32(file, "Pass Number/Areas", &temp_uint32, 1)) {
            app->data->pass_number = temp_uint32;
        } else
            break;

        if(flipper_format_read_uint32(file, "Sequence & Combination", &temp_uint32, 1)) {
            app->data->sequence_and_combination = temp_uint32;
        } else
            break;

        if(flipper_format_read_bool(file, "Deadbolt Override", &temp_bool, 1)) {
            app->data->deadbolt_override = temp_bool;
        } else
            break;

        if(flipper_format_read_uint32(file, "Restricted Days", &temp_uint32, 1)) {
            app->data->restricted_days = temp_uint32;
        } else
            break;

        if(flipper_format_read_uint32(file, "Property ID", &temp_uint32, 1)) {
            app->data->property_id = temp_uint32;
        } else
            break;

        if(flipper_format_read_string(file, "Creation", temp_str)) {
            datetime_string_to_datetime(temp_str, &app->data->creation);
        } else
            break;

        if(flipper_format_read_string(file, "Expiration", temp_str)) {
            datetime_string_to_datetime(temp_str, &app->data->expire);
        } else
            break;

        // ---

        if(flipper_format_read_uint32(file, "Variable Keys", &temp_uint32, 1)) {
            app->variable_keys = temp_uint32;
        } else
            break;

        for(size_t i = 0; i < app->variable_keys; i++) {
            furi_string_printf(temp_str, "Variable Key %d Creation", i + 1);
            if(flipper_format_read_string(file, furi_string_get_cstr(temp_str), temp_str)) {
                datetime_string_to_datetime(temp_str, &app->keys[i].creation);
            } else
                break;

            furi_string_printf(temp_str, "Variable Key %d Lock ID", i + 1);
            if(flipper_format_read_uint32(file, furi_string_get_cstr(temp_str), &temp_uint32, 1)) {
                app->keys[i].lock_id = temp_uint32;
            } else
                break;

            furi_string_printf(temp_str, "Variable Key %d Inhibit", i + 1);
            if(!flipper_format_read_bool(
                   file, furi_string_get_cstr(temp_str), &app->keys[i].inhibit, 1))
                break;

            furi_string_printf(temp_str, "Variable Key %d Use Optional", i + 1);
            if(!flipper_format_read_bool(
                   file, furi_string_get_cstr(temp_str), &app->keys[i].use_optional, 1))
                break;
        }

        // ---

        if(flipper_format_read_uint32(file, "Log Entries", &temp_uint32, 1)) {
            app->log_entries = temp_uint32;
        } else
            break;

        for(size_t i = 0; i < app->log_entries; i++) {
            furi_string_printf(temp_str, "Log %d Time", i + 1);
            if(flipper_format_read_string(file, furi_string_get_cstr(temp_str), temp_str)) {
                datetime_string_to_datetime(temp_str, &app->log[i].time);
            } else
                break;

            furi_string_printf(temp_str, "Log %d Time Set", i + 1);
            if(!flipper_format_read_bool(
                   file, furi_string_get_cstr(temp_str), &app->log[i].time_is_set, 1))
                break;

            furi_string_printf(temp_str, "Log %d Is DST", i + 1);
            if(!flipper_format_read_bool(
                   file, furi_string_get_cstr(temp_str), &app->log[i].is_dst, 1))
                break;

            furi_string_printf(temp_str, "Log %d Lock ID", i + 1);
            if(flipper_format_read_uint32(file, furi_string_get_cstr(temp_str), &temp_uint32, 1)) {
                app->log[i].lock_id = temp_uint32;
            } else
                break;

            furi_string_printf(temp_str, "Log %d Deadbolt", i + 1);
            if(!flipper_format_read_bool(
                   file, furi_string_get_cstr(temp_str), &app->log[i].deadbolt, 1))
                break;

            furi_string_printf(temp_str, "Log %d New Key", i + 1);
            if(!flipper_format_read_bool(
                   file, furi_string_get_cstr(temp_str), &app->log[i].new_key, 1))
                break;

            furi_string_printf(temp_str, "Log %d Let Open", i + 1);
            if(!flipper_format_read_bool(
                   file, furi_string_get_cstr(temp_str), &app->log[i].let_open, 1))
                break;

            furi_string_printf(temp_str, "Log %d Lock Problem", i + 1);
            if(!flipper_format_read_bool(
                   file, furi_string_get_cstr(temp_str), &app->log[i].lock_problem, 1))
                break;

            furi_string_printf(temp_str, "Log %d Lock Latched", i + 1);
            if(!flipper_format_read_bool(
                   file, furi_string_get_cstr(temp_str), &app->log[i].lock_latched, 1))
                break;

            furi_string_printf(temp_str, "Log %d Low Battery", i + 1);
            if(!flipper_format_read_bool(
                   file, furi_string_get_cstr(temp_str), &app->log[i].low_battery, 1))
                break;

            furi_string_printf(temp_str, "Log %d Diagnostic Code", i + 1);
            if(flipper_format_read_uint32(file, furi_string_get_cstr(temp_str), &temp_uint32, 1)) {
                app->log[i].diagnostic_code = temp_uint32;
            } else
                break;
        }

        parsed = true;
    } while(false);

    if(!parsed) {
        if(deprecated_version) {
            dialog_message_show_storage_error(app->dialogs_app, "File format deprecated");
        } else {
            dialog_message_show_storage_error(app->dialogs_app, "Can not parse\nfile");
        }
    }

    furi_string_free(temp_str);
    flipper_format_free(file);

    return parsed;
}

bool saflip_save_file(SaflipApp* app, const char* path) {
    bool parsed = false;
    FlipperFormat* file = flipper_format_file_alloc(app->storage);
    FuriString* temp_str = furi_string_alloc();

    do {
        if(!flipper_format_file_open_always(file, path)) {
            FURI_LOG_E(TAG, "Failed to save to path '%s'", path);
            break;
        }

        furi_string_set_str(temp_str, saflip_file_header);
        if(!flipper_format_write_header(file, temp_str, saflip_file_version)) {
            FURI_LOG_E(TAG, "Failed to write file header");
            break;
        }

        // Parse data
        uint32_t temp_uint32;
        bool temp_bool;

        temp_uint32 = app->uid_len;
        if(!flipper_format_write_uint32(file, "UID Length", &temp_uint32, 1)) break;
        if(!flipper_format_write_hex(file, "UID", app->uid, app->uid_len)) break;

        temp_uint32 = app->data->format;
        if(!flipper_format_write_uint32(file, "Format", &temp_uint32, 1)) break;

        temp_uint32 = app->data->card_level;
        if(!flipper_format_write_uint32(file, "Card Level", &temp_uint32, 1)) break;

        temp_uint32 = app->data->card_type;
        if(!flipper_format_write_uint32(file, "Card Type", &temp_uint32, 1)) break;

        temp_uint32 = app->data->card_id;
        if(!flipper_format_write_uint32(file, "Card ID", &temp_uint32, 1)) break;

        temp_bool = app->data->opening_key;
        if(!flipper_format_write_bool(file, "Is Opening Key", &temp_bool, 1)) break;

        temp_uint32 = app->data->lock_id;
        if(!flipper_format_write_uint32(file, "Key/Lock ID", &temp_uint32, 1)) break;

        temp_uint32 = app->data->pass_number;
        if(!flipper_format_write_uint32(file, "Pass Number/Areas", &temp_uint32, 1)) break;

        temp_uint32 = app->data->sequence_and_combination;
        if(!flipper_format_write_uint32(file, "Sequence & Combination", &temp_uint32, 1)) break;

        temp_bool = app->data->deadbolt_override;
        if(!flipper_format_write_bool(file, "Deadbolt Override", &temp_bool, 1)) break;

        temp_uint32 = app->data->restricted_days;
        if(!flipper_format_write_uint32(file, "Restricted Days", &temp_uint32, 1)) break;

        temp_uint32 = app->data->property_id;
        if(!flipper_format_write_uint32(file, "Property ID", &temp_uint32, 1)) break;

        if(!flipper_format_write_string(
               file, "Creation", datetime_datetime_to_string(&app->data->creation)))
            break;

        if(!flipper_format_write_string(
               file, "Expiration", datetime_datetime_to_string(&app->data->expire)))
            break;

        flipper_format_write_empty_line(file);

        temp_uint32 = app->variable_keys;
        if(!flipper_format_write_uint32(file, "Variable Keys", &temp_uint32, 1)) break;

        temp_uint32 = app->log_entries;
        if(!flipper_format_write_uint32(file, "Log Entries", &temp_uint32, 1)) break;

        for(size_t i = 0; i < app->variable_keys; i++) {
            flipper_format_write_empty_line(file);

            furi_string_printf(temp_str, "Variable Key %d Time", i + 1);
            if(!flipper_format_write_string(
                   file,
                   furi_string_get_cstr(temp_str),
                   datetime_datetime_to_string(&app->keys[i].creation)))
                break;

            furi_string_printf(temp_str, "Variable Key %d Lock ID", i + 1);
            temp_uint32 = app->keys[i].lock_id;
            if(!flipper_format_write_uint32(file, furi_string_get_cstr(temp_str), &temp_uint32, 1))
                break;

            furi_string_printf(temp_str, "Variable Key %d Inhibit", i + 1);
            if(!flipper_format_write_bool(
                   file, furi_string_get_cstr(temp_str), &app->keys[i].inhibit, 1))
                break;

            furi_string_printf(temp_str, "Variable Key %d Use Optional", i + 1);
            if(!flipper_format_write_bool(
                   file, furi_string_get_cstr(temp_str), &app->keys[i].use_optional, 1))
                break;
        }

        for(size_t i = 0; i < app->log_entries; i++) {
            flipper_format_write_empty_line(file);

            furi_string_printf(temp_str, "Log %d Time", i + 1);
            if(!flipper_format_write_string(
                   file,
                   furi_string_get_cstr(temp_str),
                   datetime_datetime_to_string(&app->log[i].time)))
                break;

            furi_string_printf(temp_str, "Log %d Time Set", i + 1);
            if(!flipper_format_write_bool(
                   file, furi_string_get_cstr(temp_str), &app->log[i].time_is_set, 1))
                break;

            furi_string_printf(temp_str, "Log %d Is DST", i + 1);
            if(!flipper_format_write_bool(
                   file, furi_string_get_cstr(temp_str), &app->log[i].is_dst, 1))
                break;

            furi_string_printf(temp_str, "Log %d Lock ID", i + 1);
            temp_uint32 = app->log[i].lock_id;
            if(!flipper_format_write_uint32(file, furi_string_get_cstr(temp_str), &temp_uint32, 1))
                break;

            furi_string_printf(temp_str, "Log %d Deadbolt", i + 1);
            if(!flipper_format_write_bool(
                   file, furi_string_get_cstr(temp_str), &app->log[i].deadbolt, 1))
                break;

            furi_string_printf(temp_str, "Log %d New Key", i + 1);
            if(!flipper_format_write_bool(
                   file, furi_string_get_cstr(temp_str), &app->log[i].new_key, 1))
                break;

            furi_string_printf(temp_str, "Log %d Let Open", i + 1);
            if(!flipper_format_write_bool(
                   file, furi_string_get_cstr(temp_str), &app->log[i].let_open, 1))
                break;

            furi_string_printf(temp_str, "Log %d Lock Problem", i + 1);
            if(!flipper_format_write_bool(
                   file, furi_string_get_cstr(temp_str), &app->log[i].lock_problem, 1))
                break;

            furi_string_printf(temp_str, "Log %d Lock Latched", i + 1);
            if(!flipper_format_write_bool(
                   file, furi_string_get_cstr(temp_str), &app->log[i].lock_latched, 1))
                break;

            furi_string_printf(temp_str, "Log %d Low Battery", i + 1);
            if(!flipper_format_write_bool(
                   file, furi_string_get_cstr(temp_str), &app->log[i].low_battery, 1))
                break;

            furi_string_printf(temp_str, "Log %d Diagnostic Code", i + 1);
            temp_uint32 = app->log[i].diagnostic_code;
            if(!flipper_format_write_uint32(file, furi_string_get_cstr(temp_str), &temp_uint32, 1))
                break;
        }

        parsed = true;
    } while(false);

    if(!parsed) {
        dialog_message_show_storage_error(app->dialogs_app, "Can not save\nfile");
    }

    furi_string_free(temp_str);
    flipper_format_free(file);

    return parsed;
}
