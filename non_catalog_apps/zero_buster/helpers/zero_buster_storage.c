#include "zero_buster_storage.h"
#include <datetime/datetime.h>
#include <strings.h>

void save_custom_hid_config(Storage* storage, const FuriHalUsbHidConfig* cfg) {
    storage_simply_mkdir(storage, APP_DATA_DIR);
    File* f = storage_file_alloc(storage);
    if(storage_file_open(f, CUSTOM_HID_FILE, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        char buf[256];
        snprintf(
            buf,
            sizeof(buf),
            "# Custom HID configuration for Zero Buster\n"
            "VID=0x%04lX\n"
            "PID=0x%04lX\n"
            "MANUF=%s\n"
            "PRODUCT=%s\n",
            (unsigned long)cfg->vid,
            (unsigned long)cfg->pid,
            cfg->manuf,
            cfg->product);
        storage_file_write(f, buf, strlen(buf));
        storage_file_close(f);
    }
    storage_file_free(f);
}

void load_custom_hid_config(Storage* storage, FuriHalUsbHidConfig* cfg) {
    cfg->vid = 0x1337;
    cfg->pid = 0x0001;
    strncpy(cfg->manuf, "Custom Corp", sizeof(cfg->manuf) - 1);
    strncpy(cfg->product, "Custom Keyboard", sizeof(cfg->product) - 1);

    storage_simply_mkdir(storage, APP_DATA_DIR);

    FileInfo info;
    if(storage_common_stat(storage, CUSTOM_HID_FILE, &info) != FSE_OK) {
        File* f = storage_file_alloc(storage);
        if(storage_file_open(f, CUSTOM_HID_FILE, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
            const char* templ =
                "# Custom HID configuration for Zero Buster\n"
                "# Modify VID and PID in hex, MANUF and PRODUCT as needed\n"
                "VID=0x1337\n"
                "PID=0x0001\n"
                "MANUF=Custom Corp\n"
                "PRODUCT=Custom Keyboard\n";
            storage_file_write(f, templ, strlen(templ));
            storage_file_close(f);
        }
        storage_file_free(f);
        return;
    }

    File* f = storage_file_alloc(storage);
    if(storage_file_open(f, CUSTOM_HID_FILE, FSAM_READ, FSOM_OPEN_EXISTING)) {
        char buf[256];
        uint16_t bytes = storage_file_read(f, buf, sizeof(buf) - 1);
        if(bytes > 0) {
            buf[bytes] = '\0';
            char* p = buf;
            while(*p) {
                char* end = p;
                while(*end && *end != '\r' && *end != '\n') end++;
                char next_char = *end;
                *end = '\0';

                char* line = p;
                while(*line == ' ' || *line == '\t') line++;

                if(*line != '#' && *line != '\0') {
                    if(strncmp(line, "VID=", 4) == 0) {
                        cfg->vid = strtoul(line + 4, NULL, 16);
                    } else if(strncmp(line, "PID=", 4) == 0) {
                        cfg->pid = strtoul(line + 4, NULL, 16);
                    } else if(strncmp(line, "MANUF=", 6) == 0) {
                        strncpy(cfg->manuf, line + 6, sizeof(cfg->manuf) - 1);
                        cfg->manuf[sizeof(cfg->manuf) - 1] = '\0';
                    } else if(strncmp(line, "PRODUCT=", 8) == 0) {
                        strncpy(cfg->product, line + 8, sizeof(cfg->product) - 1);
                        cfg->product[sizeof(cfg->product) - 1] = '\0';
                    }
                }

                if(next_char == '\0') break;
                p = end + 1;
            }
        }
        storage_file_close(f);
    }
    storage_file_free(f);
}

void ensure_default_payloads(Storage* storage) {
    storage_simply_mkdir(storage, APP_DATA_DIR);
    storage_simply_mkdir(storage, PAYLOAD_DIR);
    storage_simply_mkdir(storage, LOOT_DIR);

    static const char* preset_names[] = {
        "01_linux_system_recon.sh",
        "02_linux_wifi_info.sh",
        "03_linux_desktop_info.sh",
        "04_windows_system_recon.ps1",
        "05_windows_wifi_info.ps1",
        "06_windows_desktop_info.ps1",
    };

    for(size_t i = 0; i < COUNT_OF(preset_names); i++) {
        char destination[64];
        snprintf(destination, sizeof(destination), "%s/%s", PAYLOAD_DIR, preset_names[i]);

        FileInfo info;
        if(storage_common_stat(storage, destination, &info) == FSE_OK) {
            continue;
        }

        char bundled[96];
        snprintf(bundled, sizeof(bundled), "%s/%s", APP_ASSETS_PATH("presets"), preset_names[i]);
        storage_common_copy(storage, bundled, destination);
    }

    File* marker = storage_file_alloc(storage);
    if(storage_file_open(marker, INIT_MARKER_FILE, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        const char* msg = "# Zero Buster initialized marker\n";
        storage_file_write(marker, msg, strlen(msg));
        storage_file_close(marker);
    }
    storage_file_free(marker);
}

void scan_payloads(ZeroBusterApp* app) {
    ensure_default_payloads(app->storage);

    app->payloads.count = 0;
    app->payloads.selected_index = 0;

    File* dir = storage_file_alloc(app->storage);
    if(storage_dir_open(dir, PAYLOAD_DIR)) {
        FileInfo info;
        char name[MAX_PAYLOAD_NAME_LEN];
        while(storage_dir_read(dir, &info, name, sizeof(name))) {
            if(name[0] == '.') continue;
            if(!file_info_is_dir(&info)) {
                strncpy(
                    app->payloads.names[app->payloads.count],
                    name,
                    MAX_PAYLOAD_NAME_LEN - 1);
                app->payloads.names[app->payloads.count][MAX_PAYLOAD_NAME_LEN - 1] = '\0';
                app->payloads.count++;
                if(app->payloads.count >= MAX_PAYLOADS) break;
            }
        }
        storage_dir_close(dir);
    }
    storage_file_free(dir);
}

bool load_layout_keymap(Storage* storage, uint16_t* keymap, const char* path) {
    furi_check(keymap);

    /* Never leave a previous layout active: start from built-in en-US. */
    memset(keymap, 0, LAYOUT_FILE_SIZE);
    memcpy(keymap, hid_asciimap, MIN(sizeof(hid_asciimap), LAYOUT_FILE_SIZE));

    if(!path || !path[0]) return true;

    bool loaded = false;
    File* file = storage_file_alloc(storage);
    uint8_t bytes[LAYOUT_FILE_SIZE];
    if(storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING) &&
       storage_file_size(file) == sizeof(bytes) &&
       storage_file_read(file, bytes, sizeof(bytes)) == sizeof(bytes)) {
        for(size_t i = 0; i < LAYOUT_KEYS; i++) {
            keymap[i] = bytes[2 * i] | ((uint16_t)bytes[2 * i + 1] << 8);
        }
        loaded = true;
    }
    storage_file_close(file);
    storage_file_free(file);

    if(!loaded) FURI_LOG_W(TAG, "Invalid/missing .kl, using built-in en-US");
    return loaded;
}

void scan_layouts(ZeroBusterApp* app) {
    LayoutManager* l = &app->layouts;
    l->count = 0;
    l->selected_index = 0;
    l->builtin_only = false;

    File* dir = storage_file_alloc(app->storage);
    if(storage_dir_open(dir, LAYOUT_DIR)) {
        FileInfo info;
        char name[64];
        while(storage_dir_read(dir, &info, name, sizeof(name))) {
            if(name[0] == '.') continue;
            if(file_info_is_dir(&info)) continue;

            size_t len = strlen(name);
            const size_t ext_len = strlen(LAYOUT_EXTENSION);
            if(len <= ext_len || strcmp(name + len - ext_len, LAYOUT_EXTENSION) != 0) continue;

            size_t stem_len = len - ext_len;
            if(stem_len >= MAX_LAYOUT_NAME_LEN) continue;

            memcpy(l->names[l->count], name, stem_len);
            l->names[l->count][stem_len] = '\0';
            l->count++;
            if(l->count >= MAX_LAYOUTS) break;
        }
        storage_dir_close(dir);
    }
    storage_file_free(dir);

    /* Alphabetical order so left/right browsing is predictable. */
    for(uint32_t i = 1; i < l->count; i++) {
        char key[MAX_LAYOUT_NAME_LEN];
        strncpy(key, l->names[i], sizeof(key));
        int32_t j = (int32_t)i - 1;
        while(j >= 0 && strcasecmp(l->names[j], key) > 0) {
            memcpy(l->names[j + 1], l->names[j], MAX_LAYOUT_NAME_LEN);
            j--;
        }
        memcpy(l->names[j + 1], key, MAX_LAYOUT_NAME_LEN);
    }

    if(l->count == 0) {
        /* No layouts on the SD card: built-in en-US is the only option. */
        strncpy(l->names[0], LAYOUT_DEFAULT_NAME, MAX_LAYOUT_NAME_LEN - 1);
        l->names[0][MAX_LAYOUT_NAME_LEN - 1] = '\0';
        l->count = 1;
        l->builtin_only = true;
    } else {
        for(uint32_t i = 0; i < l->count; i++) {
            if(strcasecmp(l->names[i], LAYOUT_DEFAULT_NAME) == 0) {
                l->selected_index = i;
                break;
            }
        }
    }

    apply_selected_layout(app);
}

void apply_selected_layout(ZeroBusterApp* app) {
    LayoutManager* l = &app->layouts;

    if(l->builtin_only || l->count == 0 || l->selected_index >= l->count) {
        load_layout_keymap(app->storage, l->keymap, NULL);
        return;
    }

    char path[sizeof(LAYOUT_DIR) + MAX_LAYOUT_NAME_LEN + sizeof(LAYOUT_EXTENSION)];
    snprintf(
        path, sizeof(path), "%s/%s%s", LAYOUT_DIR, l->names[l->selected_index], LAYOUT_EXTENSION);
    load_layout_keymap(app->storage, l->keymap, path);
}

const char* current_layout_name(ZeroBusterApp* app) {
    LayoutManager* l = &app->layouts;
    if(l->count == 0) return LAYOUT_DEFAULT_NAME;
    return l->names[l->selected_index < l->count ? l->selected_index : 0];
}

void get_next_loot_filepath(Storage* storage, char* out_path, size_t max_len) {
    storage_simply_mkdir(storage, APP_DATA_DIR);
    storage_simply_mkdir(storage, LOOT_DIR);

    DateTime dt;
    furi_hal_rtc_get_datetime(&dt);

    snprintf(
        out_path,
        max_len,
        "%s/log_%04u%02u%02u_%02u%02u%02u.txt",
        LOOT_DIR,
        dt.year,
        dt.month,
        dt.day,
        dt.hour,
        dt.minute,
        dt.second);

    /* Same-second collision: add a suffix instead of overwriting the log. */
    FileInfo info;
    if(storage_common_stat(storage, out_path, &info) == FSE_OK) {
        for(uint32_t dup = 1; dup < 100; dup++) {
            snprintf(
                out_path,
                max_len,
                "%s/log_%04u%02u%02u_%02u%02u%02u_%lu.txt",
                LOOT_DIR,
                dt.year,
                dt.month,
                dt.day,
                dt.hour,
                dt.minute,
                dt.second,
                (unsigned long)dup);
            if(storage_common_stat(storage, out_path, &info) != FSE_OK) break;
        }
    }
}
