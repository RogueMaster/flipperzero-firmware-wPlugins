#include "asset_packs_i.h"

#include "settings.h"

#include <assets_icons.h>
#include <core/dangerous_defines.h>
#include <furi_hal.h>
#include <gui/icon_i.h>
#include <storage/storage.h>

#define TAG "AssetPacks"

#define ICONS_FMT ASSET_PACKS_PATH "/%s/Icons/%s"
#define FONTS_FMT ASSET_PACKS_PATH "/%s/Fonts/%s.u8f"

// See lib/u8g2/u8g2_font.c
#define U8G2_FONT_DATA_STRUCT_SIZE 23

AssetPacks* asset_packs = NULL;

#define ASSET_PACKS_ICON_MAX_W          128
#define ASSET_PACKS_ICON_MAX_H          64
// Compressed frame headers carry a uint16 payload length plus a 4-byte header.
#define ASSET_PACKS_ICON_MAX_FRAME_SIZE (UINT16_MAX + 4u)

static bool asset_packs_icon_dims_valid(int32_t width, int32_t height) {
    return width > 0 && height > 0 && width <= ASSET_PACKS_ICON_MAX_W &&
           height <= ASSET_PACKS_ICON_MAX_H;
}

// Verify a frame blob is at least as large as what the draw path reads from it.
// Uncompressed (byte 0 == 0): compress_icon_decode points the canvas straight at
// frame+1 and it reads ((w+7)/8)*h raw XBM bytes. Compressed (byte 0 != 0): a
// 4-byte CompressHeader whose bytes 2-3 (LE) hold the compressed payload size;
// the heatshrink decoder reads 4 + that many bytes. Either way the file must
// contain those bytes or the decode path reads past the malloc'd frame buffer.
static bool asset_packs_icon_frame_valid(
    const uint8_t* frame,
    uint64_t frame_size,
    int32_t width,
    int32_t height) {
    if(frame_size < 1) return false;
    if(frame[0] == 0) {
        uint32_t need = 1u + (uint32_t)((width + 7) / 8) * (uint32_t)height;
        return frame_size >= need;
    }
    if(frame_size < 4) return false;
    uint32_t compressed = (uint32_t)frame[2] | ((uint32_t)frame[3] << 8);
    return frame_size >= 4u + compressed;
}

typedef struct {
    Icon icon;
    uint8_t* frames[];
} AnimatedIconSwap;

typedef struct {
    int32_t width;
    int32_t height;
    int32_t frame_rate;
    int32_t frame_count;
} FURI_PACKED AnimatedIconMetaFile;

static void
    load_icon_animated(const Icon* original, const char* name, FuriString* path, File* file) {
    const char* pack = cfw_settings.asset_pack;
    furi_string_printf(path, ICONS_FMT "/meta", pack, name);
    if(storage_file_open(file, furi_string_get_cstr(path), FSAM_READ, FSOM_OPEN_EXISTING)) {
        AnimatedIconMetaFile meta;
        bool ok = storage_file_read(file, &meta, sizeof(meta)) == sizeof(meta);
        storage_file_close(file);

        // frame_count comes straight from an asset-pack meta file. Bound it (as
        // animation_storage.c does) so a crafted/huge value can't wrap the malloc
        // size and overflow the frames[] array.
        if(ok &&
           (meta.frame_count <= 0 || meta.frame_count > 255 || meta.frame_rate < 1 ||
            meta.frame_rate > 255 || !asset_packs_icon_dims_valid(meta.width, meta.height))) {
            ok = false;
        }
        if(ok) {
            AnimatedIconSwap* swap =
                malloc(sizeof(AnimatedIconSwap) + (sizeof(uint8_t*) * meta.frame_count));
            int i = 0;
            for(; i < meta.frame_count; i++) {
                furi_string_printf(path, ICONS_FMT "/frame_%02d.bm", pack, name, i);
                if(storage_file_open(
                       file, furi_string_get_cstr(path), FSAM_READ, FSOM_OPEN_EXISTING)) {
                    uint64_t frame_size = storage_file_size(file);
                    // Reject frames larger than the format can address before allocating.
                    if(frame_size == 0 || frame_size > ASSET_PACKS_ICON_MAX_FRAME_SIZE) {
                        storage_file_close(file);
                        i--;
                        break;
                    }
                    swap->frames[i] = malloc(frame_size);
                    ok = storage_file_read(file, swap->frames[i], frame_size) == frame_size &&
                         asset_packs_icon_frame_valid(
                             swap->frames[i], frame_size, meta.width, meta.height);
                    storage_file_close(file);
                    if(ok) continue;
                } else {
                    storage_file_close(file);
                    i--;
                }
                break;
            }

            if(i == meta.frame_count) {
                FURI_CONST_ASSIGN(swap->icon.width, meta.width);
                FURI_CONST_ASSIGN(swap->icon.height, meta.height);
                FURI_CONST_ASSIGN(swap->icon.frame_count, meta.frame_count);
                FURI_CONST_ASSIGN(swap->icon.frame_rate, meta.frame_rate);
                FURI_CONST_ASSIGN_PTR(swap->icon.frames, swap->frames);

                IconSwapList_push_back(
                    asset_packs->icons,
                    (IconSwap){
                        .original = original,
                        .replaced = &swap->icon,
                    });
            } else {
                for(; i >= 0; i--) {
                    free(swap->frames[i]);
                }
                free(swap);
            }
        }
    }
    storage_file_close(file);
}

typedef struct {
    Icon icon;
    uint8_t* frames[1];
    uint8_t frame[];
} StaticIconSwap;

typedef struct {
    int32_t width;
    int32_t height;
} FURI_PACKED StaticIconBmxHeader;

static void
    load_icon_static(const Icon* original, const char* name, FuriString* path, File* file) {
    furi_string_printf(path, ICONS_FMT ".bmx", cfw_settings.asset_pack, name);
    if(storage_file_open(file, furi_string_get_cstr(path), FSAM_READ, FSOM_OPEN_EXISTING)) {
        StaticIconBmxHeader header;
        // A valid .bmx holds at least the header; a shorter (corrupt) file would
        // underflow frame_size into a huge malloc, so treat it as no frame data
        // and let the read guard below reject it (keeping the built-in icon).
        uint64_t file_size = storage_file_size(file);
        uint64_t frame_size = (file_size >= sizeof(header)) ? (file_size - sizeof(header)) : 0;
        if(frame_size > ASSET_PACKS_ICON_MAX_FRAME_SIZE) {
            storage_file_close(file);
            return;
        }
        StaticIconSwap* swap = malloc(sizeof(StaticIconSwap) + frame_size);

        if(file_size >= sizeof(header) && frame_size <= ASSET_PACKS_ICON_MAX_FRAME_SIZE &&
           storage_file_read(file, &header, sizeof(header)) == sizeof(header) &&
           storage_file_read(file, swap->frame, frame_size) == frame_size &&
           asset_packs_icon_dims_valid(header.width, header.height) &&
           asset_packs_icon_frame_valid(swap->frame, frame_size, header.width, header.height)) {
            FURI_CONST_ASSIGN(swap->icon.width, header.width);
            FURI_CONST_ASSIGN(swap->icon.height, header.height);
            FURI_CONST_ASSIGN(swap->icon.frame_count, 1);
            FURI_CONST_ASSIGN(swap->icon.frame_rate, 0);
            FURI_CONST_ASSIGN_PTR(swap->icon.frames, swap->frames);
            swap->frames[0] = swap->frame;

            IconSwapList_push_back(
                asset_packs->icons,
                (IconSwap){
                    .original = original,
                    .replaced = &swap->icon,
                });
        } else {
            free(swap);
        }
    }
    storage_file_close(file);
}

static void free_icon(const Icon* icon) {
    StaticIconSwap* swap = (void*)icon;
    // StaticIconSwap and AnimatedIconSwap have similar structure, but
    // animated one has frames array of variable length, and frame data is
    // in another allocation, while static includes frame in same allocation
    // By checking if the first frame points to later in same allocation, we
    // can tell if it is static or animated
    if(swap->frames[0] != swap->frame) {
        for(size_t i = 0; i < swap->icon.frame_count; i++) {
            free(swap->frames[i]);
        }
    }
    free(swap);
}

static void load_font(Font font, const char* name, FuriString* path, File* file) {
    furi_string_printf(path, FONTS_FMT, cfw_settings.asset_pack, name);
    if(storage_file_open(file, furi_string_get_cstr(path), FSAM_READ, FSOM_OPEN_EXISTING)) {
        uint64_t size = storage_file_size(file);
        uint8_t* swap = malloc(size);

        if(size > U8G2_FONT_DATA_STRUCT_SIZE && storage_file_read(file, swap, size) == size) {
            asset_packs->fonts[font] = swap;
            CanvasFontParameters* params = malloc(sizeof(CanvasFontParameters));
            // See lib/u8g2/u8g2_font.c
            params->leading_default = swap[10]; // max_char_height
            params->leading_min = params->leading_default - 2; // good enough
            params->height = MAX((int8_t)swap[15], 0); // ascent_para
            params->descender = MAX((int8_t)swap[16], 0); // descent_para
            asset_packs->font_params[font] = params;
        } else {
            free(swap);
        }
    }
    storage_file_close(file);
}

static void free_font(Font font) {
    free(asset_packs->fonts[font]);
    asset_packs->fonts[font] = NULL;
    free(asset_packs->font_params[font]);
    asset_packs->font_params[font] = NULL;
}

static const char* font_names[] = {
    [FontPrimary] = "Primary",
    [FontSecondary] = "Secondary",
    [FontKeyboard] = "Keyboard",
    [FontBigNumbers] = "BigNumbers",
    [FontBatteryPercent] = "BatteryPercent",
};

void asset_packs_init(void) {
    if(asset_packs) return;

    const char* pack = cfw_settings.asset_pack;
    if(pack[0] == '\0') return;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    FuriString* p = furi_string_alloc_printf(ASSET_PACKS_PATH "/%s", pack);
    FileInfo info;
    if(storage_common_stat(storage, furi_string_get_cstr(p), &info) == FSE_OK &&
       info.flags & FSF_DIRECTORY) {
        asset_packs = malloc(sizeof(AssetPacks));
        memset(asset_packs, 0, sizeof(AssetPacks));
        IconSwapList_init(asset_packs->icons);

        File* f = storage_file_alloc(storage);

        furi_string_printf(p, ASSET_PACKS_PATH "/%s/Icons", pack);
        if(storage_common_stat(storage, furi_string_get_cstr(p), &info) == FSE_OK &&
           info.flags & FSF_DIRECTORY) {
            for(size_t i = 0; i < ICON_PATHS_COUNT; i++) {
                if(ICON_PATHS[i].icon->frame_count > 1) {
                    load_icon_animated(ICON_PATHS[i].icon, ICON_PATHS[i].path, p, f);
                } else {
                    load_icon_static(ICON_PATHS[i].icon, ICON_PATHS[i].path, p, f);
                }
            }
        }

        furi_string_printf(p, ASSET_PACKS_PATH "/%s/Fonts", pack);
        if(storage_common_stat(storage, furi_string_get_cstr(p), &info) == FSE_OK &&
           info.flags & FSF_DIRECTORY) {
            for(Font font = 0; font < FontTotalNumber; font++) {
                load_font(font, font_names[font], p, f);
            }
        }

        storage_file_free(f);
    }
    furi_string_free(p);
    furi_record_close(RECORD_STORAGE);
}

void asset_packs_free(void) {
    if(!asset_packs) return;

    for
        M_EACH(icon_swap, asset_packs->icons, IconSwapList_t) {
            free_icon(icon_swap->replaced);
        }
    IconSwapList_clear(asset_packs->icons);

    for(Font font = 0; font < FontTotalNumber; font++) {
        if(asset_packs->fonts[font] != NULL) {
            free_font(font);
        }
    }

    free(asset_packs);
    asset_packs = NULL;
}

const Icon* asset_packs_swap_icon(const Icon* requested) {
    if(!asset_packs) return requested;
    if((uint32_t)requested < FLASH_BASE || (uint32_t)requested > (FLASH_BASE + FLASH_SIZE)) {
        return requested;
    }
    for
        M_EACH(icon_swap, asset_packs->icons, IconSwapList_t) {
            if(icon_swap->original == requested) {
                return icon_swap->replaced;
            }
        }
    return requested;
}
