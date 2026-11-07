#include "fn_settings.h"

#include <furi.h>
#include <storage/storage.h>

#include <string.h>

#define FN_SETTINGS_PATH APP_DATA_PATH("settings.txt")
#define FN_SETTINGS_DIR  STORAGE_APP_DATA_PATH_PREFIX

bool fn_settings_load_lang(FnLang* lang) {
    if(!lang) return false;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    bool found = false;

    if(storage_file_open(file, FN_SETTINGS_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        char buffer[16] = {0};
        const size_t read = storage_file_read(file, buffer, sizeof(buffer) - 1);
        if(read > 0) {
            if(strstr(buffer, "ru") != NULL) {
                *lang = FnLangRu;
                found = true;
            } else if(strstr(buffer, "en") != NULL) {
                *lang = FnLangEn;
                found = true;
            }
        }
        storage_file_close(file);
    }

    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return found;
}

void fn_settings_save_lang(FnLang lang) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_common_mkdir(storage, FN_SETTINGS_DIR);

    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, FN_SETTINGS_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        const char* text = (lang == FnLangRu) ? "lang=ru\n" : "lang=en\n";
        storage_file_write(file, text, strlen(text));
        storage_file_close(file);
    }

    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}
