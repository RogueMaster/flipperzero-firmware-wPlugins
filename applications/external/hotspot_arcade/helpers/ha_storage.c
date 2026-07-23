#include "ha_storage.h"
#include "../hotspot_arcade_i.h"
#include "../ha_json.h"

#include <storage/storage.h>
#include <flipper_format/flipper_format.h>

void ha_timestamp(FuriString* out) {
    DateTime dt;
    furi_hal_rtc_get_datetime(&dt);
    furi_string_printf(out, "[%02u:%02u:%02u] ", dt.hour, dt.minute, dt.second);
}

void ha_storage_ensure_dirs(void) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, HA_DATA_DIR);
    // The bundled copies come from the fap; these are the optional user drop-in dirs.
    storage_simply_mkdir(storage, HA_USER_WEB_DIR);
    storage_simply_mkdir(storage, HA_USER_TRIVIA_DIR);
    storage_simply_mkdir(storage, HA_LOGS_DIR);
    furi_record_close(RECORD_STORAGE);
}

void ha_storage_load_config(HotspotArcadeApp* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* ff = flipper_format_file_alloc(storage);
    FuriString* tmp = furi_string_alloc();
    bool have_ssid = false;

    if(flipper_format_file_open_existing(ff, HA_CONFIG_PATH)) {
        uint32_t ver = 0;
        if(flipper_format_read_header(ff, tmp, &ver)) {
            flipper_format_rewind(ff);
            if(flipper_format_read_string(ff, "SSID", tmp)) {
                furi_string_set(app->ssid, tmp);
                have_ssid = true;
            }
            uint32_t v = 0;
            flipper_format_rewind(ff);
            if(flipper_format_read_uint32(ff, "Sound", &v, 1)) app->sound_on = (v != 0);
            flipper_format_rewind(ff);
            if(flipper_format_read_uint32(ff, "Vibro", &v, 1)) app->vibro_on = (v != 0);
        }
    }

    flipper_format_free(ff);
    furi_string_free(tmp);
    furi_record_close(RECORD_STORAGE);
    if(!have_ssid) furi_string_set(app->ssid, "Hotspot Arcade");
}

void ha_storage_save_config(HotspotArcadeApp* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* ff = flipper_format_file_alloc(storage);
    if(flipper_format_file_open_always(ff, HA_CONFIG_PATH)) {
        flipper_format_write_header_cstr(ff, "Hotspot Arcade Config", 1);
        flipper_format_write_string_cstr(ff, "SSID", furi_string_get_cstr(app->ssid));
        uint32_t sound = app->sound_on ? 1 : 0;
        uint32_t vibro = app->vibro_on ? 1 : 0;
        flipper_format_write_uint32(ff, "Sound", &sound, 1);
        flipper_format_write_uint32(ff, "Vibro", &vibro, 1);
    }
    flipper_format_free(ff);
    furi_record_close(RECORD_STORAGE);
}

bool ha_storage_read_file(const char* path, FuriString* out, size_t cap) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    furi_string_reset(out);
    size_t total = 0;

    if(storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        // Reserve up front so byte-by-byte appends don't repeatedly realloc (each
        // grow briefly holds old+new: a ~2x peak that can OOM the Flipper).
        uint64_t fsize = storage_file_size(file);
        size_t reserve = (fsize < cap ? (size_t)fsize : cap) + 16;
        furi_string_reserve(out, reserve);
        uint8_t buf[257];
        while(total < cap) {
            size_t want = cap - total;
            if(want > sizeof(buf) - 1) want = sizeof(buf) - 1;
            size_t rd = storage_file_read(file, buf, want);
            if(rd == 0) break;
            for(size_t i = 0; i < rd; i++)
                furi_string_push_back(out, (char)buf[i]);
            total += rd;
        }
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return total > 0;
}

// Copy the substring [start,end) into out (NUL-terminated, capped).
static void slice_to(const char* start, const char* end, char* out, size_t n) {
    size_t len = (size_t)(end - start);
    if(len > n - 1) len = n - 1;
    memcpy(out, start, len);
    out[len] = '\0';
}

bool ha_storage_load_manifest(HotspotArcadeApp* app) {
    app->asset_count = 0;
    FuriString* man = furi_string_alloc();
    // A user bundle in apps_data wins outright (all-or-nothing, so a hand-built bundle
    // is never half-served from the fap's copy); otherwise use the bundled one.
    app->web_dir = HA_USER_WEB_DIR;
    bool ok = ha_storage_read_file(HA_USER_WEB_DIR "/manifest.json", man, 4096);
    if(!ok) {
        app->web_dir = HA_BUNDLED_WEB_DIR;
        ok = ha_storage_read_file(HA_BUNDLED_WEB_DIR "/manifest.json", man, 4096);
    }
    if(ok) {
        const char* s = furi_string_get_cstr(man);
        const char* p = s;
        while(app->asset_count < HA_MAX_ASSETS) {
            const char* obj = strchr(p, '{');
            if(!obj) break;
            const char* end = strchr(obj, '}');
            if(!end) break;
            char tmp[300];
            slice_to(obj, end + 1, tmp, sizeof(tmp));
            HaAsset* a = &app->assets[app->asset_count];
            bool have_file = ha_json_str(tmp, "file", a->file, sizeof(a->file));
            if(have_file) {
                if(!ha_json_str(tmp, "path", a->path, sizeof(a->path)))
                    strlcpy(a->path, "/", sizeof(a->path));
                if(!ha_json_str(tmp, "mime", a->mime, sizeof(a->mime)))
                    strlcpy(a->mime, "application/octet-stream", sizeof(a->mime));
                a->gzip = ha_json_bool(tmp, "gzip");
                app->asset_count++;
            }
            p = end + 1;
        }
    }
    furi_string_free(man);
    return app->asset_count > 0;
}
