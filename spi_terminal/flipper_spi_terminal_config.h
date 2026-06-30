#pragma once

#include "flipper_spi_terminal_app.h"

#define SPI_TERM_LAST_SETTINGS_DIR EXT_PATH("apps_data/flipper_spi_terminal")
#define SPI_TERM_LAST_SETTINGS_PATH \
    EXT_PATH("apps_data/flipper_spi_terminal/last_settings.settings")
#define SPI_TERM_LAST_SETTING_FILE_TYPE    "Flipper SPI-Terminal Setting File"
#define SPI_TERM_LAST_SETTING_FILE_VERSION 1

#define SPI_TERM_LAST_SETTING_DEBUG_DATA_KEY "Debug Data"
#define SPI_TERM_LAST_SETTING_DEBUG_DATA_DESCRIPTION \
    "Can be used to set data, wich is automatically loaded into the terminal"

// Value array declarations
#define ADD_CONFIG_ENTRY(                                                                           \
    label, helpText, name, type, defaultValue, valueIndexFunc, field, valuesCount, values, strings) \
    extern const type spi_config_##name##_values[valuesCount];
#include "flipper_spi_terminal_config_declarations.h"
#undef ADD_CONFIG_ENTRY

// Value string array declarations
#define ADD_CONFIG_ENTRY(                                                                           \
    label, helpText, name, type, defaultValue, valueIndexFunc, field, valuesCount, values, strings) \
    extern const char* const spi_config_##name##_strings[valuesCount];
#include "flipper_spi_terminal_config_declarations.h"
#undef ADD_CONFIG_ENTRY

void flipper_spi_terminal_config_log(FlipperSPITerminalAppConfig* config);
void flipper_spi_terminal_config_defaults(FlipperSPITerminalAppConfig* config);
bool flipper_spi_terminal_config_load(FlipperSPITerminalAppConfig* config);
bool flipper_spi_terminal_config_save(FlipperSPITerminalAppConfig* config);

void flipper_spi_terminal_config_debug_print_saved();
