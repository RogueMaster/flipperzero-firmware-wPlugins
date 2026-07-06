#include "placeholder.h"
#include <flipper_format/flipper_format.h>

#define PH_CONFIG_BASE     APP_DATA_PATH("configs")
#define PH_CONFIG_FILETYPE "BadUSB Placeholder Config"
#define PH_CONFIG_VERSION  1
#define PH_CONFIG_EXT      ".cfg"

// --- internal helpers ------------------------------------------------------

static bool ph_is_name_char(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
           (c == '_');
}

static void ph_add_unique(PlaceholderMap* map, const char* name) {
    if(map->count >= PLACEHOLDER_MAX) return;
    for(size_t i = 0; i < map->count; i++) {
        if(furi_string_equal_str(map->entries[i].name, name)) return;
    }
    PlaceholderEntry* e = &map->entries[map->count];
    e->name = furi_string_alloc_set_str(name);
    e->value = furi_string_alloc();
    e->resolved = false;
    map->count++;
}

// Create every ancestor directory of `path` (and `path` itself).
static void ph_mkdir_p(Storage* storage, const char* path) {
    FuriString* tmp = furi_string_alloc();
    for(size_t i = 1; path[i] != '\0'; i++) {
        if(path[i] == '/') {
            furi_string_set_strn(tmp, path, i);
            storage_common_mkdir(storage, furi_string_get_cstr(tmp));
        }
    }
    storage_common_mkdir(storage, path);
    furi_string_free(tmp);
}

static void ph_config_dir(FuriString* out, const char* payload_basename) {
    furi_string_printf(out, "%s/%s", PH_CONFIG_BASE, payload_basename);
}

static void
    ph_config_path(FuriString* out, const char* payload_basename, const char* config_name) {
    furi_string_printf(
        out, "%s/%s/%s%s", PH_CONFIG_BASE, payload_basename, config_name, PH_CONFIG_EXT);
}

// --- lifecycle -------------------------------------------------------------

void placeholder_map_init(PlaceholderMap* map) {
    map->count = 0;
    for(size_t i = 0; i < PLACEHOLDER_MAX; i++) {
        map->entries[i].name = NULL;
        map->entries[i].value = NULL;
        map->entries[i].resolved = false;
    }
}

void placeholder_map_reset(PlaceholderMap* map) {
    for(size_t i = 0; i < map->count; i++) {
        if(map->entries[i].name) {
            furi_string_free(map->entries[i].name);
            map->entries[i].name = NULL;
        }
        if(map->entries[i].value) {
            furi_string_free(map->entries[i].value);
            map->entries[i].value = NULL;
        }
        map->entries[i].resolved = false;
    }
    map->count = 0;
}

void placeholder_map_clear_values(PlaceholderMap* map) {
    for(size_t i = 0; i < map->count; i++) {
        if(map->entries[i].value) furi_string_reset(map->entries[i].value);
        map->entries[i].resolved = false;
    }
}

size_t placeholder_map_unresolved_count(const PlaceholderMap* map) {
    size_t n = 0;
    for(size_t i = 0; i < map->count; i++) {
        if(!map->entries[i].resolved) n++;
    }
    return n;
}

// --- scanning --------------------------------------------------------------

// A DuckyScript comment line ("REM ...") contributes no placeholders.
static bool ph_line_is_comment(const char* s) {
    while(*s == ' ' || *s == '\t')
        s++;
    if((s[0] == 'R' || s[0] == 'r') && (s[1] == 'E' || s[1] == 'e') &&
       (s[2] == 'M' || s[2] == 'm')) {
        char c = s[3];
        return c == '\0' || c == ' ' || c == '\t' || c == '\r' || c == '\n';
    }
    return false;
}

// Collect unique [name] tokens from a single line into the map.
static void ph_scan_line(const char* s, PlaceholderMap* map) {
    char name[PLACEHOLDER_NAME_MAX + 1];
    size_t name_len = 0;
    bool in_bracket = false;
    for(size_t i = 0; s[i] != '\0'; i++) {
        char c = s[i];
        if(c == '[') {
            in_bracket = true;
            name_len = 0;
        } else if(in_bracket) {
            if(c == ']') {
                if(name_len > 0) {
                    name[name_len] = '\0';
                    ph_add_unique(map, name);
                }
                in_bracket = false;
            } else if(ph_is_name_char(c) && name_len < PLACEHOLDER_NAME_MAX) {
                name[name_len++] = c;
            } else {
                in_bracket = false; // invalid char -> not a placeholder
            }
        }
    }
}

size_t placeholder_scan_file(Storage* storage, const char* path, PlaceholderMap* map) {
    placeholder_map_reset(map);

    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        FuriString* line = furi_string_alloc();
        uint8_t buf[64];
        size_t rd;
        do {
            rd = storage_file_read(file, buf, sizeof(buf));
            for(size_t i = 0; i < rd; i++) {
                char c = (char)buf[i];
                if(c == '\n') {
                    const char* l = furi_string_get_cstr(line);
                    if(!ph_line_is_comment(l)) ph_scan_line(l, map);
                    furi_string_reset(line);
                } else if(c != '\r') {
                    furi_string_push_back(line, c);
                }
            }
        } while(rd > 0 && map->count < PLACEHOLDER_MAX);
        // final line without trailing newline
        if(map->count < PLACEHOLDER_MAX && furi_string_size(line) > 0) {
            const char* l = furi_string_get_cstr(line);
            if(!ph_line_is_comment(l)) ph_scan_line(l, map);
        }
        furi_string_free(line);
    }
    storage_file_close(file);
    storage_file_free(file);

    return map->count;
}

// --- substitution ----------------------------------------------------------

void placeholder_apply(FuriString* line, const PlaceholderMap* map) {
    if(!map || map->count == 0) return;
    char needle[PLACEHOLDER_NAME_MAX + 3]; // '[' + name + ']' + '\0'
    for(size_t i = 0; i < map->count; i++) {
        if(!map->entries[i].resolved) continue;
        snprintf(needle, sizeof(needle), "[%s]", furi_string_get_cstr(map->entries[i].name));
        furi_string_replace_all_str(line, needle, furi_string_get_cstr(map->entries[i].value));
    }
}

// --- value access ----------------------------------------------------------

void placeholder_set_value(PlaceholderMap* map, size_t index, const char* value) {
    if(index >= map->count) return;
    furi_string_set_str(map->entries[index].value, value);
    map->entries[index].resolved = true;
}

// --- config persistence ----------------------------------------------------

size_t placeholder_config_list(
    Storage* storage,
    const char* payload_basename,
    FuriString* out_names[],
    size_t max_names) {
    FuriString* dir = furi_string_alloc();
    ph_config_dir(dir, payload_basename);

    size_t n = 0;
    File* d = storage_file_alloc(storage);
    if(storage_dir_open(d, furi_string_get_cstr(dir))) {
        FileInfo info;
        char name[128];
        while(n < max_names && storage_dir_read(d, &info, name, sizeof(name))) {
            if(file_info_is_dir(&info)) continue;
            size_t len = strlen(name);
            size_t extlen = strlen(PH_CONFIG_EXT);
            if(len > extlen && strcmp(name + len - extlen, PH_CONFIG_EXT) == 0) {
                out_names[n] = furi_string_alloc();
                furi_string_set_strn(out_names[n], name, len - extlen);
                n++;
            }
        }
    }
    storage_dir_close(d);
    storage_file_free(d);
    furi_string_free(dir);
    return n;
}

bool placeholder_config_load(
    Storage* storage,
    const char* payload_basename,
    const char* config_name,
    PlaceholderMap* map) {
    FuriString* path = furi_string_alloc();
    ph_config_path(path, payload_basename, config_name);

    FlipperFormat* fff = flipper_format_file_alloc(storage);
    FuriString* val = furi_string_alloc();
    bool ok = false;

    if(flipper_format_file_open_existing(fff, furi_string_get_cstr(path))) {
        ok = true;
        for(size_t i = 0; i < map->count; i++) {
            flipper_format_rewind(fff);
            if(flipper_format_read_string(fff, furi_string_get_cstr(map->entries[i].name), val)) {
                furi_string_set(map->entries[i].value, val);
                map->entries[i].resolved = true;
            }
        }
    }

    flipper_format_free(fff);
    furi_string_free(val);
    furi_string_free(path);
    return ok;
}

bool placeholder_config_save(
    Storage* storage,
    const char* payload_basename,
    const char* config_name,
    const PlaceholderMap* map) {
    FuriString* dir = furi_string_alloc();
    ph_config_dir(dir, payload_basename);
    ph_mkdir_p(storage, furi_string_get_cstr(dir));

    FuriString* path = furi_string_alloc();
    ph_config_path(path, payload_basename, config_name);

    FlipperFormat* fff = flipper_format_file_alloc(storage);
    bool ok = false;

    if(flipper_format_file_open_always(fff, furi_string_get_cstr(path))) {
        do {
            if(!flipper_format_write_header_cstr(fff, PH_CONFIG_FILETYPE, PH_CONFIG_VERSION))
                break;
            bool wok = true;
            for(size_t i = 0; i < map->count; i++) {
                if(!map->entries[i].resolved) continue;
                if(!flipper_format_write_string_cstr(
                       fff,
                       furi_string_get_cstr(map->entries[i].name),
                       furi_string_get_cstr(map->entries[i].value))) {
                    wok = false;
                    break;
                }
            }
            ok = wok;
        } while(0);
    }

    flipper_format_free(fff);
    furi_string_free(path);
    furi_string_free(dir);
    return ok;
}
