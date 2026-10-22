#include "subbrute_settings.h"
#include "subbrute_i.h"

#define TAG "SubBruteSettings"

#define SUBBRUTE_SETTINGS_FILE_TYPE    "Sub-GHz BruteForcer Settings File"
#define SUBBRUTE_SETTINGS_FILE_VERSION 1
#define SUBBRUTE_SETTINGS_PATH         APP_DATA_PATH("bruteforcer.settings")

#define SUBBRUTE_FIELD_LAST_INDEX    "LastIndex"
#define SUBBRUTE_FIELD_REPEAT_VALUES "RepeatValue"
#define SUBBRUTE_FIELD_TIME_DELAY    "TimeDelay"
#define SUBBRUTE_FIELD_TE            "Te"
#define SUBBRUTE_FIELD_OPENCODE      "OpenCode"

SubBruteSettings* subbrute_settings_alloc(void) {
    SubBruteSettings* instance = malloc(sizeof(SubBruteSettings));
    return instance;
}

void subbrute_settings_free(SubBruteSettings* instance) {
    furi_assert(instance);
    free(instance);
}

void subbrute_settings_load(SubBruteSettings* instance) {
    furi_assert(instance);

    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* fff_data_file = flipper_format_file_alloc(storage);

    uint32_t temp_last_index = 0;
    uint8_t temp_repeat_values[SubBruteAttackTotalCount] = {0};
    uint8_t temp_time_delay[SubBruteAttackTotalCount] = {0};
    uint32_t temp_te[SubBruteAttackTotalCount] = {0};
    uint8_t temp_opencode[SubBruteAttackTotalCount] = {0};
    bool was_read_last_index = false;
    bool was_read_repeat_values = false;
    bool was_read_time_delay = false;
    bool was_read_te = false;
    bool was_read_opencode = false;

    storage_common_migrate(
        storage, EXT_PATH("subghz-bruteforcer.settings"), SUBBRUTE_SETTINGS_PATH);

    if(FSE_OK == storage_sd_status(storage) &&
       flipper_format_file_open_existing(fff_data_file, SUBBRUTE_SETTINGS_PATH)) {
        // Rewind before every read: key search goes only forward,
        // so one missing (optional) field would break all following reads
        was_read_last_index = flipper_format_rewind(fff_data_file) &&
                              flipper_format_read_uint32(
                                  fff_data_file, SUBBRUTE_FIELD_LAST_INDEX, &temp_last_index, 1);
        was_read_repeat_values = flipper_format_rewind(fff_data_file) &&
                                 flipper_format_read_hex(
                                     fff_data_file,
                                     SUBBRUTE_FIELD_REPEAT_VALUES,
                                     temp_repeat_values,
                                     SubBruteAttackTotalCount);
        was_read_time_delay = flipper_format_rewind(fff_data_file) &&
                              flipper_format_read_hex(
                                  fff_data_file,
                                  SUBBRUTE_FIELD_TIME_DELAY,
                                  temp_time_delay,
                                  SubBruteAttackTotalCount);
        was_read_te = flipper_format_rewind(fff_data_file) &&
                      flipper_format_read_uint32(
                          fff_data_file, SUBBRUTE_FIELD_TE, temp_te, SubBruteAttackTotalCount);
        was_read_opencode =
            flipper_format_rewind(fff_data_file) &&
            flipper_format_read_hex(
                fff_data_file, SUBBRUTE_FIELD_OPENCODE, temp_opencode, SubBruteAttackTotalCount);
    } else {
        FURI_LOG_E(TAG, "Error open file %s", SUBBRUTE_SETTINGS_PATH);
    }

    if(was_read_last_index && temp_last_index < SubBruteAttackTotalCount) {
        instance->last_index = temp_last_index;
    } else {
        FURI_LOG_W(TAG, "Last used index not found or can't be used!");
        instance->last_index = (uint32_t)SubBruteAttackCAME12bit433;
    }

    for(size_t i = 0; i < SubBruteAttackTotalCount; i++) {
        if(was_read_repeat_values && temp_repeat_values[i] >= SUBBRUTE_MIN_REPEATS &&
           temp_repeat_values[i] <= SUBBRUTE_MAX_REPEATS) {
            instance->repeat_values[i] = temp_repeat_values[i];
        } else {
            instance->repeat_values[i] = subbrute_protocol_repeats_count(i);
        }

        // uint8_t can't exceed SUBBRUTE_MAX_TIME_DELAY
        instance->time_delay[i] = was_read_time_delay ? temp_time_delay[i] :
                                                        SUBBRUTE_DEFAULT_TIME_DELAY;
        instance->te[i] = was_read_te ? temp_te[i] : 0;
        instance->opencode[i] = was_read_opencode ? temp_opencode[i] : 0;
    }

    flipper_format_file_close(fff_data_file);
    flipper_format_free(fff_data_file);
    furi_record_close(RECORD_STORAGE);
}

bool subbrute_settings_save(SubBruteSettings* instance) {
    furi_assert(instance);

    bool saved = false;
    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* file = flipper_format_file_alloc(storage);

    do {
        if(FSE_OK != storage_sd_status(storage)) {
            break;
        }
        // Open file
        if(!flipper_format_file_open_always(file, SUBBRUTE_SETTINGS_PATH)) {
            break;
        }
        // Write header
        if(!flipper_format_write_header_cstr(
               file, SUBBRUTE_SETTINGS_FILE_TYPE, SUBBRUTE_SETTINGS_FILE_VERSION)) {
            break;
        }
        if(!flipper_format_insert_or_update_uint32(
               file, SUBBRUTE_FIELD_LAST_INDEX, &instance->last_index, 1)) {
            break;
        }

        if(!flipper_format_insert_or_update_hex(
               file,
               SUBBRUTE_FIELD_REPEAT_VALUES,
               instance->repeat_values,
               SubBruteAttackTotalCount)) {
            break;
        }
        if(!flipper_format_insert_or_update_hex(
               file, SUBBRUTE_FIELD_TIME_DELAY, instance->time_delay, SubBruteAttackTotalCount)) {
            break;
        }
        if(!flipper_format_insert_or_update_uint32(
               file, SUBBRUTE_FIELD_TE, instance->te, SubBruteAttackTotalCount)) {
            break;
        }
        if(!flipper_format_insert_or_update_hex(
               file, SUBBRUTE_FIELD_OPENCODE, instance->opencode, SubBruteAttackTotalCount)) {
            break;
        }
        saved = true;
    } while(false);

    if(!saved) {
        FURI_LOG_E(TAG, "Error save file %s", SUBBRUTE_SETTINGS_PATH);
    }

    flipper_format_file_close(file);
    flipper_format_free(file);
    furi_record_close(RECORD_STORAGE);

    return saved;
}

void subbrute_settings_set_value(SubBruteSettings* instance, SubBruteAttacks index, uint8_t value) {
    furi_assert(instance);

    instance->repeat_values[index] = value;
}
uint8_t subbrute_settings_get_value(SubBruteSettings* instance, SubBruteAttacks index) {
    furi_assert(instance);

    return instance->repeat_values[index];
}

void subbrute_settings_set_repeats(SubBruteSettings* instance, const uint8_t* repeated_values) {
    furi_assert(instance);

    for(size_t i = 0; i < SubBruteAttackTotalCount; i++) {
        instance->repeat_values[i] = repeated_values[i];
    }
}

uint8_t subbrute_settings_get_current_repeats(SubBruteSettings* instance) {
    furi_assert(instance);

    return instance->repeat_values[instance->last_index];
}
