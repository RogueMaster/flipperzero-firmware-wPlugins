#include "settings.h"
#include <flipper_format/flipper_format.h>

#define SETTINGS_PATH APP_DATA_PATH("settings.conf")
#define SETTINGS_TYPE "BreachMap Settings"
#define SETTINGS_VER  1

uint32_t breach_pin_hash(const char* pin) {
    uint32_t h = 5381;
    for(const char* p = pin ? pin : ""; *p; p++) {
        h = ((h << 5) + h) + (uint8_t)*p;
    }
    return h ? h : 1;
}

void breach_settings_load(Storage* storage, uint32_t* pin_hash, bool* pin_set) {
    furi_check(storage);
    *pin_hash = 0;
    *pin_set = false;

    FlipperFormat* ff = flipper_format_file_alloc(storage);
    FuriString* type = furi_string_alloc();
    uint32_t ver = 0;
    uint32_t tmp = 0;

    if(flipper_format_file_open_existing(ff, SETTINGS_PATH) &&
       flipper_format_read_header(ff, type, &ver) &&
       furi_string_cmp_str(type, SETTINGS_TYPE) == 0) {
        if(flipper_format_read_uint32(ff, "PinSet", &tmp, 1)) *pin_set = (tmp != 0);
        if(flipper_format_read_uint32(ff, "PinHash", &tmp, 1)) *pin_hash = tmp;
    }

    furi_string_free(type);
    flipper_format_free(ff);
}

bool breach_settings_save(Storage* storage, uint32_t pin_hash, bool pin_set) {
    furi_check(storage);
    storage_common_mkdir(storage, APP_DATA_PATH(""));

    FlipperFormat* ff = flipper_format_file_alloc(storage);
    bool ok = false;
    uint32_t tmp;
    if(flipper_format_file_open_always(ff, SETTINGS_PATH)) {
        do {
            if(!flipper_format_write_header_cstr(ff, SETTINGS_TYPE, SETTINGS_VER)) break;
            tmp = pin_set ? 1 : 0;
            if(!flipper_format_write_uint32(ff, "PinSet", &tmp, 1)) break;
            tmp = pin_hash;
            if(!flipper_format_write_uint32(ff, "PinHash", &tmp, 1)) break;
            ok = true;
        } while(false);
    }
    flipper_format_free(ff);
    return ok;
}
