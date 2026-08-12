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
    storage_simply_mkdir(storage, HA_ART_DIR);
    furi_record_close(RECORD_STORAGE);
}

void ha_storage_load_config(HotspotArcadeApp* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* ff = flipper_format_file_alloc(storage);
    FuriString* tmp = furi_string_alloc();
    bool have_ssid = false;
    app->lang[0] = '\0'; // default: English

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
            flipper_format_rewind(ff);
            if(flipper_format_read_string(ff, "Lang", tmp))
                strlcpy(app->lang, furi_string_get_cstr(tmp), sizeof(app->lang));
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
        flipper_format_write_string_cstr(ff, "Lang", app->lang);
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
    app->web_bundle_crc = 0; // set from the "/" object's "crc" if present (else always stream)
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
                uint32_t crc;
                if(strcmp(a->path, "/") == 0 && ha_json_u32(tmp, "crc", &crc))
                    app->web_bundle_crc = crc; // bundle identity for the skip-restream check
                app->asset_count++;
            }
            p = end + 1;
        }
    }
    furi_string_free(man);
    return app->asset_count > 0;
}

// ---------------- Frankendraw artwork (SVG on the SD card) ----------------
//
// A sheet is FD_UNIT (255) units square on the wire; it is written out as a 510x765
// portrait page -- x doubled, y tripled -- because the three panels are drawn on a
// 2:3 sheet on the phone. The file is appended to as the segments arrive, so the
// Flipper never holds a picture in memory either.
#define HA_ART_W    510
#define HA_ART_H    765
#define HA_ART_XS   2 // sheet unit -> page unit, across
#define HA_ART_YS   3 // sheet unit -> page unit, down
#define HA_ART_NICK 24

static void art_write(HotspotArcadeApp* app, const char* s) {
    if(app->art_file) storage_file_write(app->art_file, s, strlen(s));
}

// XML-escape a nickname (player-typed) for the credits line. Bytes >= 0x80 are UTF-8
// and pass through untouched.
static void art_escape(const char* in, char* out, size_t n) {
    size_t o = 0;
    for(const char* p = in ? in : ""; *p && o + 7 < n; p++) {
        const char* rep = *p == '&'  ? "&amp;" :
                          *p == '<'  ? "&lt;" :
                          *p == '>'  ? "&gt;" :
                          *p == '"'  ? "&quot;" :
                          *p == '\'' ? "&apos;" :
                                       NULL;
        if(rep) {
            size_t rl = strlen(rep);
            memcpy(out + o, rep, rl);
            o += rl;
        } else if((unsigned char)*p >= 0x20) {
            out[o++] = *p;
        }
    }
    out[o] = '\0';
}

void ha_art_abort(HotspotArcadeApp* app) {
    if(app->art_file) {
        storage_file_close(app->art_file);
        storage_file_free(app->art_file);
        app->art_file = NULL;
    }
    if(app->art_storage) {
        furi_record_close(RECORD_STORAGE);
        app->art_storage = NULL;
    }
}

void ha_art_begin(HotspotArcadeApp* app, const char* json) {
    ha_art_abort(app); // an interrupted stream must not leak the handle
    int id = 0;
    if(!ha_json_int(json, "id", &id) || id < 0) return;
    // One timestamp per gallery (its first sheet is id 0), so a session's creatures
    // land next to each other in the directory listing.
    if(id == 0 || app->art_stamp[0] == '\0') {
        DateTime dt;
        furi_hal_rtc_get_datetime(&dt);
        // Every field is masked to two digits so -Werror=format-truncation can prove
        // the stamp fits art_stamp[16] (13 chars + NUL); semantically a no-op.
        snprintf(
            app->art_stamp,
            sizeof(app->art_stamp),
            "%02u%02u%02u-%02u%02u%02u",
            (unsigned)(dt.year % 100),
            (unsigned)(dt.month % 100),
            (unsigned)(dt.day % 100),
            (unsigned)(dt.hour % 100),
            (unsigned)(dt.minute % 100),
            (unsigned)(dt.second % 100));
    }

    app->art_storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(app->art_storage, HA_ART_DIR);
    FuriString* path = furi_string_alloc();
    furi_string_printf(path, HA_ART_DIR "/fd-%s-%d.svg", app->art_stamp, id + 1);
    app->art_file = storage_file_alloc(app->art_storage);
    bool ok = storage_file_open(
        app->art_file, furi_string_get_cstr(path), FSAM_WRITE, FSOM_CREATE_ALWAYS);
    furi_string_free(path);
    if(!ok) {
        ha_art_abort(app);
        return;
    }

    art_write(
        app,
        "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"510\" height=\"765\" "
        "viewBox=\"0 0 510 765\">\n"
        "<rect width=\"510\" height=\"765\" fill=\"#EDEDE6\"/>\n"
        "<path d=\"M0 255H510M0 510H510\" stroke=\"#C9C9BE\" stroke-width=\"2\"/>\n");

    // Credits: the three panels' drawers, w0/w1/w2 (an empty one means nobody drew
    // that panel -- its holder had already left).
    char raw[HA_NICK_LEN], who[3][HA_ART_NICK * 6];
    for(int i = 0; i < 3; i++) {
        char key[3] = {'w', (char)('0' + i), '\0'};
        if(!ha_json_str(json, key, raw, sizeof(raw))) raw[0] = '\0';
        art_escape(raw[0] ? raw : "-", who[i], sizeof(who[i]));
    }
    FuriString* line = furi_string_alloc();
    furi_string_printf(
        line,
        "<text x=\"12\" y=\"752\" font-family=\"sans-serif\" font-size=\"18\" "
        "fill=\"#8A8A80\">%s / %s / %s</text>\n",
        who[0],
        who[1],
        who[2]);
    art_write(app, furi_string_get_cstr(line));
    furi_string_free(line);

    art_write(
        app,
        "<g fill=\"none\" stroke=\"#111111\" stroke-width=\"5\" stroke-linecap=\"round\" "
        "stroke-linejoin=\"round\">\n");
}

void ha_art_stroke(HotspotArcadeApp* app, const char* json) {
    if(!app->art_file) return;
    int x0, y0, x1, y1;
    if(!ha_json_int(json, "x0", &x0) || !ha_json_int(json, "y0", &y0) ||
       !ha_json_int(json, "x1", &x1) || !ha_json_int(json, "y1", &y1))
        return;
    char buf[64];
    snprintf(
        buf,
        sizeof(buf),
        "<path d=\"M%d %dL%d %d\"/>\n",
        x0 * HA_ART_XS,
        y0 * HA_ART_YS,
        x1 * HA_ART_XS,
        y1 * HA_ART_YS);
    art_write(app, buf);
}

void ha_art_end(HotspotArcadeApp* app) {
    if(!app->art_file) return;
    art_write(app, "</g>\n</svg>\n");
    ha_art_abort(app);
}
