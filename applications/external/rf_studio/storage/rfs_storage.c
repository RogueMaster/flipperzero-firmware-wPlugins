#include "rfs_storage.h"

#include <furi_hal_random.h>
#include <furi_hal_rtc.h>
#include <datetime/datetime.h>
#include <toolbox/path.h>

#define SETTINGS_FILETYPE "RF Studio Settings"
#define SETTINGS_VERSION  1

static const char* const radio_mode_names[RfsRadioModeCount] = {"Auto", "Internal", "External"};
static const char* const sort_names[RfsSortCount] = {"Name", "Category", "Last Used"};
static const char* const scope_names[RfsDiscoverCount] = {"Common", "All"};

const char* rfs_radio_mode_name(RfsRadioMode mode) {
    return mode < RfsRadioModeCount ? radio_mode_names[mode] : "?";
}

const char* rfs_sort_name(RfsSort sort) {
    return sort < RfsSortCount ? sort_names[sort] : "?";
}

const char* rfs_discover_scope_name(RfsDiscoverScope scope) {
    return scope < RfsDiscoverCount ? scope_names[scope] : "?";
}

bool rfs_storage_sd_ready(Storage* storage) {
    return storage_sd_status(storage) == FSE_OK;
}

bool rfs_storage_init(Storage* storage) {
    if(!rfs_storage_sd_ready(storage)) return false;
    const char* dirs[] = {
        EXT_PATH("apps_data"),
        RFS_DATA_DIR,
        RFS_CONFIG_DIR,
        RFS_REMOTES_DIR,
        RFS_SIGNALS_DIR,
        RFS_BACKUPS_DIR,
        RFS_EXPORTS_DIR,
        RFS_LOGS_DIR,
        RFS_TMP_DIR,
    };
    bool ok = true;
    for(size_t i = 0; i < COUNT_OF(dirs); i++) {
        if(!storage_simply_mkdir(storage, dirs[i])) ok = false;
    }
    return ok;
}

void rfs_storage_new_id(Storage* storage, char prefix, const char* dir, const char* ext, char* out) {
    FuriString* path = furi_string_alloc();
    for(uint8_t attempt = 0; attempt < 16; attempt++) {
        uint32_t value = furi_hal_random_get();
        snprintf(out, RFS_ID_LEN, "%c%08lX", prefix, (unsigned long)value);
        if(!dir) break;
        furi_string_printf(path, "%s/%s%s", dir, out, ext ? ext : "");
        if(!storage_common_exists(storage, furi_string_get_cstr(path))) break;
    }
    furi_string_free(path);
}

uint32_t rfs_storage_now(void) {
    DateTime dt;
    furi_hal_rtc_get_datetime(&dt);
    return datetime_datetime_to_timestamp(&dt);
}

void rfs_storage_format_time(uint32_t timestamp, FuriString* out) {
    if(timestamp == 0) {
        furi_string_set(out, "never");
        return;
    }
    DateTime dt;
    datetime_timestamp_to_datetime(timestamp, &dt);
    furi_string_printf(
        out, "%04u-%02u-%02u %02u:%02u", dt.year, dt.month, dt.day, dt.hour, dt.minute);
}

FlipperFormat* rfs_storage_atomic_begin(Storage* storage, const char* path) {
    FuriString* tmp = furi_string_alloc_printf("%s.tmp", path);
    FlipperFormat* ff = flipper_format_file_alloc(storage);
    if(!flipper_format_file_open_always(ff, furi_string_get_cstr(tmp))) {
        flipper_format_free(ff);
        ff = NULL;
    }
    furi_string_free(tmp);
    return ff;
}

bool rfs_storage_atomic_commit(Storage* storage, FlipperFormat* ff, const char* path, bool ok) {
    FuriString* tmp = furi_string_alloc_printf("%s.tmp", path);
    const char* tmp_path = furi_string_get_cstr(tmp);
    if(ff) {
        if(!flipper_format_file_close(ff)) ok = false;
        flipper_format_free(ff);
    } else {
        ok = false;
    }

    if(ok) {
        /* The tmp file is complete; replace the original. A crash between remove and rename
         * is repaired by rfs_storage_recover(). */
        FS_Error err = storage_common_remove(storage, path);
        if(err != FSE_OK && err != FSE_NOT_EXIST) ok = false;
        if(ok && storage_common_rename(storage, tmp_path, path) != FSE_OK) ok = false;
    }
    if(!ok) storage_common_remove(storage, tmp_path);
    furi_string_free(tmp);
    return ok;
}

void rfs_storage_recover(Storage* storage, const char* path) {
    FuriString* tmp = furi_string_alloc_printf("%s.tmp", path);
    const char* tmp_path = furi_string_get_cstr(tmp);
    if(storage_common_exists(storage, tmp_path)) {
        if(storage_common_exists(storage, path)) {
            /* Original intact; the tmp file is from an interrupted write. */
            storage_common_remove(storage, tmp_path);
        } else {
            /* Original removed but tmp complete: finish the swap. */
            storage_common_rename(storage, tmp_path, path);
        }
    }
    furi_string_free(tmp);
}

int32_t rfs_storage_copy_dir_files(Storage* storage, const char* src_dir, const char* dst_dir) {
    int32_t copied = 0;
    if(!storage_simply_mkdir(storage, dst_dir)) return -1;
    File* dir = storage_file_alloc(storage);
    FuriString* src = furi_string_alloc();
    FuriString* dst = furi_string_alloc();
    char name[128];
    FileInfo info;
    if(storage_dir_open(dir, src_dir)) {
        while(storage_dir_read(dir, &info, name, sizeof(name))) {
            if(file_info_is_dir(&info)) continue;
            furi_string_printf(src, "%s/%s", src_dir, name);
            furi_string_printf(dst, "%s/%s", dst_dir, name);
            if(storage_common_copy(
                   storage, furi_string_get_cstr(src), furi_string_get_cstr(dst)) == FSE_OK) {
                copied++;
            } else {
                copied = -1;
                break;
            }
        }
    } else {
        copied = -1;
    }
    storage_dir_close(dir);
    storage_file_free(dir);
    furi_string_free(src);
    furi_string_free(dst);
    return copied;
}

bool rfs_storage_backup(Storage* storage, FuriString* out_path) {
    DateTime dt;
    furi_hal_rtc_get_datetime(&dt);
    furi_string_printf(
        out_path,
        "%s/%04u%02u%02u-%02u%02u%02u",
        RFS_BACKUPS_DIR,
        dt.year,
        dt.month,
        dt.day,
        dt.hour,
        dt.minute,
        dt.second);
    const char* root = furi_string_get_cstr(out_path);
    if(!storage_simply_mkdir(storage, root)) return false;

    FuriString* sub = furi_string_alloc();
    bool ok = true;

    furi_string_printf(sub, "%s/remotes", root);
    if(rfs_storage_copy_dir_files(storage, RFS_REMOTES_DIR, furi_string_get_cstr(sub)) < 0)
        ok = false;
    furi_string_printf(sub, "%s/signals", root);
    if(ok && rfs_storage_copy_dir_files(storage, RFS_SIGNALS_DIR, furi_string_get_cstr(sub)) < 0)
        ok = false;
    furi_string_printf(sub, "%s/config", root);
    if(ok && rfs_storage_copy_dir_files(storage, RFS_CONFIG_DIR, furi_string_get_cstr(sub)) < 0)
        ok = false;

    furi_string_free(sub);
    return ok;
}

void rfs_storage_safe_filename(const char* name, FuriString* out) {
    furi_string_reset(out);
    for(const char* p = name; *p; p++) {
        char c = *p;
        bool safe = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                    c == '-' || c == '_' || c == ' ' || c == '.';
        furi_string_push_back(out, safe ? c : '_');
    }
    furi_string_trim(out);
    if(furi_string_empty(out)) furi_string_set(out, "signal");
}

void rfs_settings_defaults(RfsSettings* settings) {
    settings->radio_mode = RfsRadioModeAuto;
    settings->discover_scope = RfsDiscoverAll;
    settings->rssi_threshold = RFS_RSSI_DEFAULT;
    settings->sort = RfsSortName;
    strlcpy(settings->default_preset, "AM650", sizeof(settings->default_preset));
    settings->logging = false;
}

bool rfs_settings_load(Storage* storage, RfsSettings* settings) {
    rfs_settings_defaults(settings);
    rfs_storage_recover(storage, RFS_SETTINGS_PATH);

    FlipperFormat* ff = flipper_format_file_alloc(storage);
    FuriString* str = furi_string_alloc();
    bool ok = false;
    do {
        if(!flipper_format_file_open_existing(ff, RFS_SETTINGS_PATH)) break;
        uint32_t version = 0;
        if(!flipper_format_read_header(ff, str, &version)) break;
        if(furi_string_cmp_str(str, SETTINGS_FILETYPE) != 0) break;

        /* Each key is optional so older files keep their defaults for new settings. */
        uint32_t value;
        int32_t svalue;
        bool flag;
        flipper_format_rewind(ff);
        if(flipper_format_read_uint32(ff, "Radio_mode", &value, 1) && value < RfsRadioModeCount)
            settings->radio_mode = value;
        flipper_format_rewind(ff);
        if(flipper_format_read_uint32(ff, "Discover_scope", &value, 1) && value < RfsDiscoverCount)
            settings->discover_scope = value;
        flipper_format_rewind(ff);
        if(flipper_format_read_int32(ff, "Rssi_threshold", &svalue, 1) && svalue >= RFS_RSSI_MIN &&
           svalue <= RFS_RSSI_MAX)
            settings->rssi_threshold = svalue;
        flipper_format_rewind(ff);
        if(flipper_format_read_uint32(ff, "Sort", &value, 1) && value < RfsSortCount)
            settings->sort = value;
        flipper_format_rewind(ff);
        if(flipper_format_read_string(ff, "Default_preset", str) && !furi_string_empty(str))
            strlcpy(
                settings->default_preset,
                furi_string_get_cstr(str),
                sizeof(settings->default_preset));
        flipper_format_rewind(ff);
        if(flipper_format_read_bool(ff, "Logging", &flag, 1)) settings->logging = flag;
        ok = true;
    } while(false);
    furi_string_free(str);
    flipper_format_free(ff);
    return ok;
}

bool rfs_settings_save(Storage* storage, const RfsSettings* settings) {
    FlipperFormat* ff = rfs_storage_atomic_begin(storage, RFS_SETTINGS_PATH);
    bool ok = ff != NULL;
    if(ok) {
        uint32_t value;
        ok = flipper_format_write_header_cstr(ff, SETTINGS_FILETYPE, SETTINGS_VERSION);
        value = settings->radio_mode;
        ok = ok && flipper_format_write_uint32(ff, "Radio_mode", &value, 1);
        value = settings->discover_scope;
        ok = ok && flipper_format_write_uint32(ff, "Discover_scope", &value, 1);
        ok = ok && flipper_format_write_int32(ff, "Rssi_threshold", &settings->rssi_threshold, 1);
        value = settings->sort;
        ok = ok && flipper_format_write_uint32(ff, "Sort", &value, 1);
        ok = ok &&
             flipper_format_write_string_cstr(ff, "Default_preset", settings->default_preset);
        ok = ok && flipper_format_write_bool(ff, "Logging", &settings->logging, 1);
    }
    return rfs_storage_atomic_commit(storage, ff, RFS_SETTINGS_PATH, ok);
}

static char rfs_lower(char c) {
    return (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
}

int rfs_strcasecmp(const char* a, const char* b) {
    while(*a && *b) {
        char ca = rfs_lower(*a++);
        char cb = rfs_lower(*b++);
        if(ca != cb) return (int)(unsigned char)ca - (int)(unsigned char)cb;
    }
    return (int)(unsigned char)rfs_lower(*a) - (int)(unsigned char)rfs_lower(*b);
}

bool rfs_strcasestr(const char* haystack, const char* needle) {
    if(!needle || !*needle) return true;
    for(const char* h = haystack; *h; h++) {
        const char* a = h;
        const char* b = needle;
        while(*a && *b && rfs_lower(*a) == rfs_lower(*b)) {
            a++;
            b++;
        }
        if(!*b) return true;
    }
    return false;
}
