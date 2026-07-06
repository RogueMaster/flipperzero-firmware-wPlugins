#include "src/settings/cfs_settings.h"

#include <furi.h>
#include <string.h>
#include <flipper_format/flipper_format.h>

#define TAG "CfsSettings"

// Stored as a standard Flipper document (key-value, versioned, human-readable)
// instead of a raw binary blob. Old settings.bin files are ignored -> defaults.
#define CFS_SETTINGS_PATH     APP_DATA_PATH("settings")
#define CFS_SETTINGS_FILETYPE "CFS Tag Writer Settings"
#define CFS_SETTINGS_VERSION  (5)

void cfs_settings_default(CfsSettings* settings) {
    furi_assert(settings);
    memset(settings, 0, sizeof(CfsSettings));
    settings->default_brand = CfsBrandCreality;
    settings->serial_mode = CfsSerialKeepSource;
    memcpy(settings->weight_len, cfs_default_weight_length, sizeof(settings->weight_len));
    settings->serial_counter = 1;
    settings->color_palette = CfsPaletteBasic;
}

void cfs_settings_load(Storage* storage, CfsSettings* settings) {
    furi_assert(storage);
    furi_assert(settings);

    CfsSettings tmp;
    cfs_settings_default(&tmp);

    FlipperFormat* ff = flipper_format_file_alloc(storage);
    FuriString* filetype = furi_string_alloc();
    bool ok = false;
    do {
        if(!flipper_format_file_open_existing(ff, CFS_SETTINGS_PATH)) break;
        uint32_t version = 0;
        if(!flipper_format_read_header(ff, filetype, &version)) break;
        if(furi_string_cmp_str(filetype, CFS_SETTINGS_FILETYPE) != 0 ||
           version != CFS_SETTINGS_VERSION)
            break;

        uint32_t u = 0;
        uint32_t wl[CFS_WEIGHT_COUNT] = {0};
        if(!flipper_format_read_uint32(ff, "DefaultBrand", &u, 1)) break;
        if(u >= CfsBrandCount) break;
        tmp.default_brand = (CfsBrand)u;
        if(!flipper_format_read_uint32(ff, "SerialMode", &u, 1)) break;
        if(u >= CfsSerialModeCount) break;
        tmp.serial_mode = (CfsSerialMode)u;
        if(!flipper_format_read_uint32(ff, "WeightLen", wl, CFS_WEIGHT_COUNT)) break;
        for(size_t i = 0; i < CFS_WEIGHT_COUNT; i++) tmp.weight_len[i] = (uint16_t)wl[i];
        if(!flipper_format_read_uint32(ff, "SerialCounter", &tmp.serial_counter, 1)) break;
        if(!flipper_format_read_uint32(ff, "ColorPalette", &u, 1)) break;
        if(u >= CfsPaletteCount) break;
        tmp.color_palette = (CfsColorPalette)u;
        ok = true;
    } while(false);

    furi_string_free(filetype);
    flipper_format_free(ff);

    // Commit the fully-parsed settings, or clean defaults on any failure (tmp
    // may be partially overwritten if a read broke midway).
    if(ok) {
        *settings = tmp;
    } else {
        cfs_settings_default(settings);
    }
}

bool cfs_settings_save(Storage* storage, const CfsSettings* settings) {
    furi_assert(storage);
    furi_assert(settings);

    uint32_t wl[CFS_WEIGHT_COUNT];
    for(size_t i = 0; i < CFS_WEIGHT_COUNT; i++) wl[i] = settings->weight_len[i];

    FlipperFormat* ff = flipper_format_file_alloc(storage);
    bool ok = false;
    do {
        if(!flipper_format_file_open_always(ff, CFS_SETTINGS_PATH)) break;
        if(!flipper_format_write_header_cstr(ff, CFS_SETTINGS_FILETYPE, CFS_SETTINGS_VERSION)) break;
        uint32_t u = settings->default_brand;
        if(!flipper_format_write_uint32(ff, "DefaultBrand", &u, 1)) break;
        u = settings->serial_mode;
        if(!flipper_format_write_uint32(ff, "SerialMode", &u, 1)) break;
        if(!flipper_format_write_uint32(ff, "WeightLen", wl, CFS_WEIGHT_COUNT)) break;
        if(!flipper_format_write_uint32(ff, "SerialCounter", &settings->serial_counter, 1)) break;
        u = settings->color_palette;
        if(!flipper_format_write_uint32(ff, "ColorPalette", &u, 1)) break;
        ok = true;
    } while(false);

    flipper_format_free(ff);
    if(!ok) FURI_LOG_E(TAG, "Failed to save settings");
    return ok;
}

// Keep labels short (<= 6 chars, like "Random"/"Prompt") -- at 8 chars
// ("Sequence") the value text ran flush against the selector arrows with no
// gap, so treat 8 as already too wide despite fitting the column.
const char* cfs_serial_mode_label(CfsSerialMode mode) {
    switch(mode) {
    case CfsSerialRandom:
        return "Random";
    case CfsSerialKeepSource:
        return "Keep";
    case CfsSerialSequential:
        return "Seq";
    case CfsSerialPrompt:
        return "Prompt";
    default:
        return "?";
    }
}
