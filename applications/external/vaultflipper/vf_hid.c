// SPDX-FileCopyrightText: 2026 Sven Würth
// SPDX-License-Identifier: MIT

#include "vf_hid.h"

#include <furi.h>
#include <string.h>

#define VF_LAYOUT_DIR_APP    APP_ASSETS_PATH("layouts")
#define VF_LAYOUT_DIR_BADUSB EXT_PATH("badusb/assets/layouts")
#define VF_HID_WAIT_MS       3000
#define VF_HID_SETTLE_MS     700
#define VF_HID_KEY_DELAY_MS  8
#define VF_MAX_LAYOUTS       48

static const char* const vf_layout_dirs[] = {VF_LAYOUT_DIR_APP, VF_LAYOUT_DIR_BADUSB};

static void vf_set_dead_keys(VfHid* hid) {
    /* Dead keys: ^ and ` on German, Swiss and French layouts, plus ~ (Option+N)
     * on macOS. A following space produces the character itself. */
    hid->dead[0] = 0;
    const char* n = hid->name;
    bool de_fr = strncmp(n, "de-", 3) == 0 || strncmp(n, "fr-", 3) == 0;
    if(!de_fr) return;
    size_t len = strlen(n);
    bool mac = len > 4 && strcmp(n + len - 4, "-mac") == 0;
    strlcpy(hid->dead, mac ? "^`~" : "^`", sizeof(hid->dead));
}

void vf_hid_init(VfHid* hid) {
    memset(hid, 0, sizeof(*hid));
    for(uint8_t c = 0; c < 128; c++) {
        hid->layout[c] = HID_ASCII_TO_KEY(c);
    }
    strlcpy(hid->name, "en-US", sizeof(hid->name));
}

static bool vf_layout_name_ok(const char* name) {
    size_t n = strlen(name);
    if(n == 0 || n > VF_LAYOUT_NAME_MAX) return false;
    for(size_t i = 0; i < n; i++) {
        char c = name[i];
        bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                  c == '-' || c == '_';
        if(!ok) return false; /* no path tricks */
    }
    return true;
}

static bool vf_read_layout(Storage* storage, const char* dir, const char* name, uint16_t* out) {
    FuriString* path = furi_string_alloc_printf("%s/%s.kl", dir, name);
    File* file = storage_file_alloc(storage);
    bool ok = false;
    if(storage_file_open(file, furi_string_get_cstr(path), FSAM_READ, FSOM_OPEN_EXISTING)) {
        ok = storage_file_read(file, out, 256) == 256;
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_string_free(path);
    return ok;
}

bool vf_hid_load_layout(VfHid* hid, Storage* storage, const char* name) {
    if(!name || !vf_layout_name_ok(name)) return false;
    if(strcmp(name, "en-US") == 0) {
        bool active = hid->active;
        FuriHalUsbInterface* prev = hid->prev;
        vf_hid_init(hid);
        hid->active = active;
        hid->prev = prev;
        return true;
    }
    uint16_t layout[128];
    for(size_t i = 0; i < COUNT_OF(vf_layout_dirs); i++) {
        if(vf_read_layout(storage, vf_layout_dirs[i], name, layout)) {
            memcpy(hid->layout, layout, sizeof(layout));
            strlcpy(hid->name, name, sizeof(hid->name));
            vf_set_dead_keys(hid);
            return true;
        }
    }
    return false;
}

void vf_hid_list_layouts(Storage* storage, VfLayoutCallback cb, void* ctx) {
    char names[VF_MAX_LAYOUTS][VF_LAYOUT_NAME_MAX + 1];
    size_t count = 0;
    strlcpy(names[count++], "en-US", sizeof(names[0]));
    cb("en-US", ctx);
    File* dir = storage_file_alloc(storage);
    char fname[64];
    FileInfo info;
    for(size_t d = 0; d < COUNT_OF(vf_layout_dirs); d++) {
        if(storage_dir_open(dir, vf_layout_dirs[d])) {
            while(count < VF_MAX_LAYOUTS && storage_dir_read(dir, &info, fname, sizeof(fname))) {
                size_t n = strlen(fname);
                if(info.flags & FSF_DIRECTORY || n < 4 || strcmp(fname + n - 3, ".kl") != 0)
                    continue;
                fname[n - 3] = 0;
                if(!vf_layout_name_ok(fname)) continue;
                bool dup = false;
                for(size_t i = 0; i < count; i++) {
                    if(strcmp(names[i], fname) == 0) dup = true;
                }
                if(dup) continue;
                strlcpy(names[count++], fname, sizeof(names[0]));
                cb(fname, ctx);
            }
        }
        storage_dir_close(dir);
    }
    storage_file_free(dir);
}

static FuriHalUsbHidConfig vf_usb_config = {
    /* Same IDs as the firmware's HID mode (and BadUSB), with readable names. */
    .vid = 0x046D,
    .pid = 0xC529,
    .manuf = "VaultFlipper",
    .product = "VaultFlipper Keyboard",
};

bool vf_hid_start(VfHid* hid) {
    if(hid->active) return true;
    hid->prev = furi_hal_usb_get_config();
    furi_hal_usb_unlock();
    if(!furi_hal_usb_set_config(&usb_hid, &vf_usb_config)) return false;
    hid->active = true;
    return true;
}

bool vf_hid_connected(const VfHid* hid) {
    return hid->active && furi_hal_hid_is_connected();
}

bool vf_hid_ready(VfHid* hid) {
    if(!vf_hid_start(hid)) return false;
    if(furi_hal_hid_is_connected()) return true; /* already enumerated: type right away */
    for(uint32_t waited = 0; !furi_hal_hid_is_connected() && waited < VF_HID_WAIT_MS;
        waited += 50) {
        furi_delay_ms(50);
    }
    if(!furi_hal_hid_is_connected()) return false;
    furi_delay_ms(VF_HID_SETTLE_MS); /* a fresh keyboard needs a moment on the host */
    return true;
}

bool vf_hid_is_iso(const VfHid* hid) {
    /* ANSI: US-style layouts. Everything else uses the ISO key next to left Shift. */
    return strcmp(hid->name, "en-US") != 0 && strcmp(hid->name, "dvorak") != 0;
}

void vf_hid_key(VfHid* hid, uint16_t key) {
    if(!hid->active || key == HID_KEYBOARD_NONE) return;
    furi_hal_hid_kb_press(key);
    furi_delay_ms(VF_HID_KEY_DELAY_MS);
    furi_hal_hid_kb_release(key);
    furi_delay_ms(VF_HID_KEY_DELAY_MS);
}

void vf_hid_type(VfHid* hid, const char* text) {
    for(const unsigned char* p = (const unsigned char*)text; *p; p++) {
        uint16_t key = *p < 128 ? hid->layout[*p] : HID_KEYBOARD_NONE;
        if(key == HID_KEYBOARD_NONE) {
            /* HID layouts cannot express UTF-8: count each character once */
            if((*p & 0xC0) != 0x80) hid->skipped++;
            continue;
        }
        vf_hid_key(hid, key);
        if(strchr(hid->dead, *p)) vf_hid_key(hid, HID_KEYBOARD_SPACEBAR);
    }
}

void vf_hid_end(VfHid* hid) {
    if(!hid->active) return;
    furi_hal_hid_kb_release_all();
    furi_delay_ms(50);
    furi_hal_usb_set_config(hid->prev, NULL);
    hid->active = false;
}
