#include "morse_flipper_rx_settings.h"

#include <string.h>

#ifdef MORSE_FLIPPER_FAP
#include <furi.h>
#include <storage/storage.h>

#define MORSE_FLIPPER_RX_SETTINGS_PATH      APP_DATA_PATH("rx_callsigns.bin")
#define MORSE_FLIPPER_RX_SETTINGS_TEMP_PATH APP_DATA_PATH("rx_callsigns.tmp")
#endif

#define MORSE_FLIPPER_RX_SETTINGS_MAGIC   0x4D465258UL
#define MORSE_FLIPPER_RX_SETTINGS_VERSION 1U

typedef struct {
    uint32_t magic;
    uint8_t version;
    uint8_t length;
    uint8_t wpm;
    uint8_t farnsworth_wpm;
} MorseFlipperRxSettingsRecord;

_Static_assert(sizeof(MorseFlipperRxSettingsRecord) == 8U, "RX settings record size changed");

#ifndef MORSE_FLIPPER_FAP
static MorseFlipperRxSettings morse_flipper_rx_host_settings = {
    .length = 5U,
    .wpm = 12U,
    .farnsworth_wpm = 12U,
};
#endif

void morse_flipper_rx_settings_reset(MorseFlipperRxSettings* settings) {
    if(settings == NULL) return;
    *settings = (MorseFlipperRxSettings){.length = 5U, .wpm = 12U, .farnsworth_wpm = 12U};
}

void morse_flipper_rx_settings_normalize(MorseFlipperRxSettings* settings) {
    if(settings == NULL) return;
    if(settings->length > 5U) settings->length = 5U;
    if(settings->wpm < 10U) settings->wpm = 10U;
    if(settings->wpm > 30U) settings->wpm = 30U;
    if(settings->farnsworth_wpm < 1U) settings->farnsworth_wpm = 1U;
    if(settings->farnsworth_wpm > settings->wpm) settings->farnsworth_wpm = settings->wpm;
}

void morse_flipper_rx_settings_length_bounds(
    uint8_t selection,
    uint8_t* min_length,
    uint8_t* max_length) {
    static const uint8_t mins[] = {4U, 5U, 6U, 4U, 5U, 4U};
    static const uint8_t maxs[] = {4U, 5U, 6U, 5U, 6U, 6U};
    if(selection >= sizeof(mins)) selection = 5U;
    if(min_length != NULL) *min_length = mins[selection];
    if(max_length != NULL) *max_length = maxs[selection];
}

bool morse_flipper_rx_settings_load(MorseFlipperRxSettings* settings) {
    if(settings == NULL) return false;
    morse_flipper_rx_settings_reset(settings);
#ifdef MORSE_FLIPPER_FAP
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    MorseFlipperRxSettingsRecord record = {0};
    bool loaded = false;

    for(uint8_t attempt = 0U; attempt < 2U && !loaded; attempt++) {
        const char* path = attempt == 0U ? MORSE_FLIPPER_RX_SETTINGS_PATH :
                                           MORSE_FLIPPER_RX_SETTINGS_TEMP_PATH;
        loaded = storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING) &&
                 storage_file_size(file) == sizeof(record) &&
                 storage_file_read(file, &record, sizeof(record)) == sizeof(record) &&
                 record.magic == MORSE_FLIPPER_RX_SETTINGS_MAGIC &&
                 record.version == MORSE_FLIPPER_RX_SETTINGS_VERSION;
        loaded = storage_file_close(file) && loaded;
    }
    if(loaded) {
        settings->length = record.length;
        settings->wpm = record.wpm;
        settings->farnsworth_wpm = record.farnsworth_wpm;
    }
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    morse_flipper_rx_settings_normalize(settings);
    return loaded;
#else
    *settings = morse_flipper_rx_host_settings;
    return true;
#endif
}

bool morse_flipper_rx_settings_save(const MorseFlipperRxSettings* settings) {
    if(settings == NULL) return false;
#ifdef MORSE_FLIPPER_FAP
    MorseFlipperRxSettings normalized = *settings;
    MorseFlipperRxSettingsRecord record;
    Storage* storage;
    File* file;
    bool saved;

    morse_flipper_rx_settings_normalize(&normalized);
    record = (MorseFlipperRxSettingsRecord){
        .magic = MORSE_FLIPPER_RX_SETTINGS_MAGIC,
        .version = MORSE_FLIPPER_RX_SETTINGS_VERSION,
        .length = normalized.length,
        .wpm = normalized.wpm,
        .farnsworth_wpm = normalized.farnsworth_wpm,
    };
    storage = furi_record_open(RECORD_STORAGE);
    file = storage_file_alloc(storage);
    storage_common_remove(storage, MORSE_FLIPPER_RX_SETTINGS_TEMP_PATH);
    saved = storage_file_open(
                file, MORSE_FLIPPER_RX_SETTINGS_TEMP_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS) &&
            storage_file_write(file, &record, sizeof(record)) == sizeof(record);
    saved = storage_file_close(file) && saved;
    if(saved)
        saved =
            storage_common_rename(
                storage, MORSE_FLIPPER_RX_SETTINGS_TEMP_PATH, MORSE_FLIPPER_RX_SETTINGS_PATH) ==
            FSE_OK;
    else
        storage_common_remove(storage, MORSE_FLIPPER_RX_SETTINGS_TEMP_PATH);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return saved;
#else
    morse_flipper_rx_host_settings = *settings;
    morse_flipper_rx_settings_normalize(&morse_flipper_rx_host_settings);
    return true;
#endif
}
