#include "mf_ardf_settings.h"

#include <stdio.h>
#include <string.h>

#ifdef MORSE_FLIPPER_FAP
#include <furi.h>
#include <storage/storage.h>
#define MF_ARDF_CONFIG_PATH APP_DATA_PATH("ardf.bin")
#endif

static const uint16_t intervals_seconds[MF_ARDF_INTERVAL_COUNT] = {
    3U,   5U,   8U,   10U,  12U,  15U,  20U,  24U,  30U,  45U,  60U,  75U,  90U,  105U,
    120U, 180U, 240U, 300U, 360U, 420U, 480U, 540U, 600U, 660U, 720U, 780U, 840U, 900U,
};

void mf_ardf_settings_defaults(MfArdfSettings* settings) {
    if(settings == NULL) return;
    *settings = (MfArdfSettings){
        .mode = MfArdfModeCustom,
        .modulation = MfArdfModulationCw,
        .message = MfArdfMessage1,
        .interval_index = MF_ARDF_DEFAULT_INTERVAL_INDEX,
        .wpm = 10U,
    };
    memcpy(settings->custom, "FOX", 4U);
}

bool mf_ardf_settings_valid(const MfArdfSettings* settings) {
    char normalized[MF_ARDF_CUSTOM_CAPACITY + 1U];
    if(settings == NULL || settings->mode > MfArdfModeStandard ||
       settings->modulation > MfArdfModulationCwfm || settings->message >= MfArdfMessageCount ||
       settings->interval_index >= MF_ARDF_INTERVAL_COUNT || settings->light_assistance > 1U ||
       settings->audio_output > 1U || settings->wpm < 8U || settings->wpm > 30U ||
       settings->selected_row >= MF_ARDF_SETTING_COUNT ||
       memchr(settings->custom, '\0', sizeof(settings->custom)) == NULL)
        return false;
    return mf_ardf_normalize_custom(normalized, sizeof(normalized), settings->custom) != 0U &&
           strcmp(normalized, settings->custom) == 0;
}

bool mf_ardf_settings_decode(MfArdfSettings* settings, const void* bytes, size_t bytes_size) {
    const MfArdfConfigFile* file = bytes;
    if(settings == NULL || file == NULL || bytes_size != sizeof(*file) ||
       file->magic != MF_ARDF_CONFIG_MAGIC || file->version != MF_ARDF_CONFIG_VERSION ||
       file->size != sizeof(*file) || !mf_ardf_settings_valid(&file->settings)) {
        mf_ardf_settings_defaults(settings);
        return false;
    }
    *settings = file->settings;
    return true;
}

void mf_ardf_settings_encode(const MfArdfSettings* settings, MfArdfConfigFile* file) {
    if(file == NULL) return;
    memset(file, 0, sizeof(*file));
    file->magic = MF_ARDF_CONFIG_MAGIC;
    file->version = MF_ARDF_CONFIG_VERSION;
    file->size = sizeof(*file);
    if(settings != NULL && mf_ardf_settings_valid(settings))
        file->settings = *settings;
    else
        mf_ardf_settings_defaults(&file->settings);
}

size_t mf_ardf_normalize_custom(char* output, size_t output_size, const char* input) {
    size_t written = 0U;
    bool pending_space = false;
    if(output == NULL || output_size == 0U) return 0U;
    output[0] = '\0';
    if(input == NULL) return 0U;
    while(*input != '\0' && written < MF_ARDF_CUSTOM_CAPACITY && written + 1U < output_size) {
        unsigned char ch = (unsigned char)*input++;
        if(ch >= 'a' && ch <= 'z') ch = (unsigned char)(ch - ('a' - 'A'));
        if(ch == '_') ch = ' ';
        if(ch == ' ') {
            if(written != 0U) pending_space = true;
            continue;
        }
        if(!((ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9'))) continue;
        if(pending_space && written < MF_ARDF_CUSTOM_CAPACITY && written + 1U < output_size)
            output[written++] = ' ';
        pending_space = false;
        if(written < MF_ARDF_CUSTOM_CAPACITY && written + 1U < output_size)
            output[written++] = (char)ch;
    }
    while(written != 0U && output[written - 1U] == ' ')
        written--;
    output[written] = '\0';
    return written;
}

uint32_t mf_ardf_interval_seconds(uint8_t index) {
    return index < MF_ARDF_INTERVAL_COUNT ? intervals_seconds[index] : 60U;
}

const char* mf_ardf_interval_label(uint8_t index) {
    static char labels[MF_ARDF_INTERVAL_COUNT][9];
    uint32_t seconds;
    if(index >= MF_ARDF_INTERVAL_COUNT) index = MF_ARDF_DEFAULT_INTERVAL_INDEX;
    if(labels[index][0] != '\0') return labels[index];
    seconds = intervals_seconds[index];
    if(seconds < 180U)
        snprintf(labels[index], sizeof(labels[index]), "%lu s", (unsigned long)seconds);
    else
        snprintf(labels[index], sizeof(labels[index]), "%lu min", (unsigned long)(seconds / 60U));
    return labels[index];
}

bool mf_ardf_settings_load(MfArdfSettings* settings) {
#ifdef MORSE_FLIPPER_FAP
    Storage* storage;
    File* file;
    MfArdfConfigFile config;
    bool ok = false;
    if(settings == NULL) return false;
    storage = furi_record_open(RECORD_STORAGE);
    file = storage_file_alloc(storage);
    if(storage_file_open(file, MF_ARDF_CONFIG_PATH, FSAM_READ, FSOM_OPEN_EXISTING) &&
       storage_file_read(file, &config, sizeof(config)) == sizeof(config) &&
       storage_file_eof(file))
        ok = mf_ardf_settings_decode(settings, &config, sizeof(config));
    else
        mf_ardf_settings_defaults(settings);
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return ok;
#else
    mf_ardf_settings_defaults(settings);
    return false;
#endif
}

bool mf_ardf_settings_save(const MfArdfSettings* settings) {
#ifdef MORSE_FLIPPER_FAP
    Storage* storage;
    File* file;
    MfArdfConfigFile config;
    bool ok;
    if(!mf_ardf_settings_valid(settings)) return false;
    mf_ardf_settings_encode(settings, &config);
    storage = furi_record_open(RECORD_STORAGE);
    file = storage_file_alloc(storage);
    ok = storage_file_open(file, MF_ARDF_CONFIG_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS) &&
         storage_file_write(file, &config, sizeof(config)) == sizeof(config);
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return ok;
#else
    return mf_ardf_settings_valid(settings);
#endif
}
