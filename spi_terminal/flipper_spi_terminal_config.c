#include <furi_hal.h>
#include <furi_hal_spi_types.h>
#include <lib/flipper_format/flipper_format.h>

#include "flipper_spi_terminal.h"
#include "flipper_spi_terminal_app.h"
#include "flipper_spi_terminal_config.h"

// Value array generation
#define ADD_CONFIG_ENTRY(                                                                           \
    label, helpText, name, type, defaultValue, valueIndexFunc, field, valuesCount, values, strings) \
    const type spi_config_##name##_values[valuesCount] = {UNWRAP_ARGS values};
#include "flipper_spi_terminal_config_declarations.h"
#undef ADD_CONFIG_ENTRY

// Value string array generation
#define ADD_CONFIG_ENTRY(                                                                           \
    label, helpText, name, type, defaultValue, valueIndexFunc, field, valuesCount, values, strings) \
    const char* const spi_config_##name##_strings[valuesCount] = {UNWRAP_ARGS strings};
#include "flipper_spi_terminal_config_declarations.h"
#undef ADD_CONFIG_ENTRY

void flipper_spi_terminal_config_defaults(FlipperSPITerminalAppConfig* config) {
    SPI_TERM_LOG_T("Setting default settings...");
    furi_check(config);

#define ADD_CONFIG_ENTRY(                                                                           \
    label, helpText, name, type, defaultValue, valueIndexFunc, field, valuesCount, values, strings) \
    config->field = defaultValue;
#include "flipper_spi_terminal_config_declarations.h"
#undef ADD_CONFIG_ENTRY

    furi_string_reset(config->debug.debug_terminal_data);
}

#define SPI_TERM_LOG_SETTING(label, name, value_string) \
    SPI_TERM_LOG_D("%s (%s): %s", label, #name, str);

void flipper_spi_terminal_config_log(FlipperSPITerminalAppConfig* config) {
    furi_check(config);

    size_t value_index;
    const char* str;

#define ADD_CONFIG_ENTRY(                                                                           \
    label, helpText, name, type, defaultValue, valueIndexFunc, field, valuesCount, values, strings) \
    value_index = (valueIndexFunc)(config->field, spi_config_##name##_values, valuesCount);         \
    str = spi_config_##name##_strings[value_index];                                                 \
    SPI_TERM_LOG_SETTING(label, #name, str);
#include "flipper_spi_terminal_config_declarations.h"
#undef ADD_CONFIG_ENTRY

    SPI_TERM_LOG_SETTING(
        SPI_TERM_LAST_SETTING_DEBUG_DATA_KEY, debug.debug_string, config.debug.debug_string);
}

static bool flipper_spi_terminal_read_config_value(
    FlipperFormat* file,
    const char* key,
    FuriString* target_str) {
    if(!flipper_format_read_string(file, key, target_str)) {
        flipper_format_rewind(file);
        if(!flipper_format_read_string(file, key, target_str)) {
            SPI_TERM_LOG_W("Did not read value for %s!", key);
            return false;
        }
    }

    return true;
}

static bool flipper_spi_terminal_read_config_value_index(
    FlipperFormat* file,
    const char* key,
    FuriString* tmp,
    const char* const strings[],
    size_t count,
    size_t* result) {
    if(!flipper_spi_terminal_read_config_value(file, key, tmp)) {
        return false;
    }

    for(size_t i = 0; i < count; i++) {
        const char* val = strings[i];
        if(furi_string_equal_str(tmp, val)) {
            *result = i;
            return true;
        }
    }

    return false;
}

void flipper_spi_terminal_read_config_values(
    FlipperSPITerminalAppConfig* config,
    FlipperFormat* file) {
    SPI_TERM_LOG_T("Reading settings...");
    furi_check(config);
    furi_check(file);

    FuriString* tmp = furi_string_alloc();
    size_t value_index;

#define ADD_CONFIG_ENTRY(                                                                           \
    label, helpText, name, type, defaultValue, valueIndexFunc, field, valuesCount, values, strings) \
    if(flipper_spi_terminal_read_config_value_index(                                                \
           file, #name, tmp, spi_config_##name##_strings, valuesCount, &value_index)) {             \
        config->field = spi_config_##name##_values[value_index];                                    \
    }
#include "flipper_spi_terminal_config_declarations.h"
#undef ADD_CONFIG_ENTRY

    if(!flipper_spi_terminal_read_config_value(
           file, SPI_TERM_LAST_SETTING_DEBUG_DATA_KEY, config->debug.debug_terminal_data)) {
        furi_string_reset(config->debug.debug_terminal_data);
    }

    furi_string_free(tmp);
}

bool flipper_spi_terminal_write_multiline_comment(FlipperFormat* file, const char* comment) {
    furi_check(file);
    if(comment == NULL) {
        return true;
    }

    FuriString* str = furi_string_alloc();
    bool res = true;

    while(res && *comment != '\0') {
        if(*comment == '\n') {
            if(!furi_string_empty(str)) {
                res = res && flipper_format_write_comment(file, str);
                furi_string_reset(str);
            }
        } else {
            furi_string_push_back(str, *comment);
        }

        comment++;
    }

    if(res && !furi_string_empty(str)) {
        res = res && flipper_format_write_comment(file, str);
    }

    furi_string_free(str);

    return res;
}

static bool flipper_spi_terminal_write_config_value(
    FlipperFormat* file,
    const char* key,
    const char* value,
    const char* description) {
    if(!flipper_format_write_empty_line(file) ||
       !flipper_spi_terminal_write_multiline_comment(file, description)) {
        SPI_TERM_LOG_E("Could not write comment for  %s!", key);
        return false;
    }

    if(!flipper_format_write_string_cstr(file, key, value)) {
        SPI_TERM_LOG_E("Could not write field %s!", key);
        return false;
    }

    return true;
}

bool flipper_spi_terminal_write_config_values(
    FlipperSPITerminalAppConfig* config,
    FlipperFormat* file) {
    SPI_TERM_LOG_T("Writing settings...");
    furi_check(config);
    furi_check(file);

    size_t value_index;
    const char* value_string;

#define ADD_CONFIG_ENTRY(                                                                           \
    label, helpText, name, type, defaultValue, valueIndexFunc, field, valuesCount, values, strings) \
    value_index = (valueIndexFunc)(config->field,                                                   \
                                   spi_config_##name##_values,                                      \
                                   COUNT_OF(spi_config_##name##_values));                           \
    value_string = spi_config_##name##_strings[value_index];                                        \
    if(!flipper_spi_terminal_write_config_value(file, #name, value_string, helpText)) {             \
        return false;                                                                               \
    }
#include "flipper_spi_terminal_config_declarations.h"
#undef ADD_CONFIG_ENTRY

    if(!furi_string_empty(config->debug.debug_terminal_data)) {
        if(!flipper_spi_terminal_write_config_value(
               file,
               SPI_TERM_LAST_SETTING_DEBUG_DATA_KEY,
               furi_string_get_cstr(config->debug.debug_terminal_data),
               SPI_TERM_LAST_SETTING_DEBUG_DATA_DESCRIPTION)) {
            return false;
        }
    }

    return true;
}

bool flipper_spi_terminal_read_config_header(FlipperFormat* file) {
    bool result = false;

    FuriString* filetype = furi_string_alloc();
    uint32_t version = 0;
    if(flipper_format_read_header(file, filetype, &version)) {
        // Ideally, this should never return false.
        // Make sure additional settings are compatible with older versions.
        if(version == SPI_TERM_LAST_SETTING_FILE_VERSION) {
            // Just a 'good practice' sanity check.
            if(furi_string_equal_str(filetype, SPI_TERM_LAST_SETTING_FILE_TYPE)) {
                result = true;
            } else {
                SPI_TERM_LOG_W("Bad filetype!");
            }
        } else {
            SPI_TERM_LOG_W("Bad confing versions");
        }
    } else {
        SPI_TERM_LOG_W("Header read failed!");
    }
    furi_string_free(filetype);

    return result;
}

bool flipper_spi_terminal_config_load(FlipperSPITerminalAppConfig* config) {
    furi_check(config);
    bool result = false;

    SPI_TERM_LOG_T("Reading settings...");

    flipper_spi_terminal_config_defaults(config);

    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(storage_sd_status(storage) == FSE_OK) {
        FlipperFormat* file = flipper_format_file_alloc(storage);

        if(flipper_format_file_open_existing(file, SPI_TERM_LAST_SETTINGS_PATH)) {
            if(flipper_spi_terminal_read_config_header(file)) {
                flipper_spi_terminal_read_config_values(config, file);
                result = true;
            }
        } else {
            SPI_TERM_LOG_W("Can not open file!");
        }

        flipper_format_file_close(file);
        flipper_format_free(file);
    } else {
        SPI_TERM_LOG_W("SD not ready!");
    }
    furi_record_close(RECORD_STORAGE);

    flipper_spi_terminal_config_log(config);

    return result;
}

bool flipper_spi_terminal_config_save(FlipperSPITerminalAppConfig* config) {
    furi_check(config);
    bool result = false;

    SPI_TERM_LOG_T("Writing settings...");

    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(storage_sd_status(storage) == FSE_OK) {
        FS_Error err = storage_common_mkdir(storage, SPI_TERM_LAST_SETTINGS_DIR);
        if(err == FSE_OK || err == FSE_EXIST) {
            FlipperFormat* file = flipper_format_file_alloc(storage);
            if(flipper_format_file_open_always(file, SPI_TERM_LAST_SETTINGS_PATH)) {
                if(flipper_format_write_header_cstr(
                       file, SPI_TERM_LAST_SETTING_FILE_TYPE, SPI_TERM_LAST_SETTING_FILE_VERSION)) {
                    result = flipper_spi_terminal_write_config_values(config, file);
                } else {
                    SPI_TERM_LOG_W("Can not write header!");
                }
            } else {
                SPI_TERM_LOG_W("Can not open file!");
            }

            flipper_format_file_close(file);
            flipper_format_free(file);

        } else {
            SPI_TERM_LOG_W("Can not create dir! err %d", (int)err);
        }
    } else {
        SPI_TERM_LOG_W("SD not ready!");
    }
    furi_record_close(RECORD_STORAGE);

    flipper_spi_terminal_config_log(config);

    return result;
}

void flipper_spi_terminal_config_debug_print_saved() {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(storage_sd_status(storage) == FSE_OK) {
        File* file = storage_file_alloc(storage);
        if(storage_file_open(file, SPI_TERM_LAST_SETTINGS_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
            char buffer[512];
            while(true) {
                size_t read = storage_file_read(file, buffer, sizeof(buffer) - 1);
                if(read == 0) {
                    break;
                }
                buffer[read] = '\0';
                printf(buffer);
            }
        } else {
            printf("Can not open file %s", SPI_TERM_LAST_SETTINGS_PATH);
        }
        storage_file_free(file);
    } else {
        printf("SD not ready!");
    }
    furi_record_close(RECORD_STORAGE);
}
