#include "settings.h"

#include <furi.h>
#include <storage/storage.h>

#define SETTINGS_PATH  APP_DATA_PATH("settings.bin")
// Sube el magic cuando cambie el significado de un campo: los ajustes viejos
// se descartan y se vuelve a los valores por defecto. En "SMR1" lang 0 era
// espanol; desde "SMR2" lang 0 es ingles.
#define SETTINGS_MAGIC 0x32524D53u // "SMR2"

typedef struct {
    uint32_t magic;
    MorseSettings settings;
} SettingsBlob;

void morse_settings_default(MorseSettings* s) {
    s->lang = 0; // ingles
    s->sound = true;
    s->vibro = true;
    s->led = true;
    s->volume = 3; // 100%
    s->tone_hz = 600;
    s->wpm = 12;
    s->dit_ms = 250;
    s->letter_gap_ms = 1500;
    s->word_gap_ms = 4000;
}

static bool settings_valid(const MorseSettings* s) {
    return s->lang <= 1 && s->wpm >= 5 && s->wpm <= 35 && s->volume <= 3 && s->tone_hz >= 200 &&
           s->tone_hz <= 2000 && s->dit_ms >= 100 && s->dit_ms <= 1000 &&
           s->letter_gap_ms <= 5000 && s->word_gap_ms <= 10000;
}

void morse_settings_load(MorseSettings* s) {
    morse_settings_default(s);
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, SETTINGS_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        SettingsBlob blob;
        if(storage_file_read(file, &blob, sizeof(blob)) == sizeof(blob) &&
           blob.magic == SETTINGS_MAGIC && settings_valid(&blob.settings)) {
            *s = blob.settings;
        }
        storage_file_close(file);
    }
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

void morse_settings_save(const MorseSettings* s) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, SETTINGS_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        SettingsBlob blob = {.magic = SETTINGS_MAGIC, .settings = *s};
        storage_file_write(file, &blob, sizeof(blob));
        storage_file_close(file);
    }
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

float morse_settings_volume_f(const MorseSettings* s) {
    static const float volumes[] = {0.25f, 0.5f, 0.75f, 1.0f};
    return volumes[s->volume <= 3 ? s->volume : 3];
}
