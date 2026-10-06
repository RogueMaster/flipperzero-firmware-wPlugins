#include "dnd_splash_image.h"

#include <furi.h>
#include <furi_hal_random.h>
#include <storage/storage.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DND_SPLASH_IMAGE_RECORD "dnd_splash_image_v1"

struct DndSplashImage {
    uint16_t references;
    uint8_t choice;
    bool loaded;
    uint8_t bitmap[DND_SPLASH_IMAGE_BYTES];
};

static const char* const dnd_splash_files[DND_SPLASH_IMAGE_COUNT] = {
    "original.bin",
    "gta.bin",
    "gta_v2.bin",
    "rogue.bin",
    "wizard.bin",
    "ranger.bin",
    "paladin.bin",
    "cleric.bin",
    "bard.bin",
    "artificer.bin",
    "druid.bin",
    "monk.bin",
    "sorcerer.bin",
    "warlock.bin",
    "barbarian.bin",
};

static uint8_t dnd_splash_image_choose(void) {
    const uint32_t limit = UINT32_MAX - (UINT32_MAX % DND_SPLASH_IMAGE_COUNT);
    uint32_t value;
    do {
        value = furi_hal_random_get();
    } while(value >= limit);
    return (uint8_t)(value % DND_SPLASH_IMAGE_COUNT);
}

static bool dnd_splash_image_read(File* file, const char* root, uint8_t choice, uint8_t* bitmap) {
    char path[128];
    int length = snprintf(path, sizeof(path), "%s/%s", root, dnd_splash_files[choice]);
    if(length < 0 || (size_t)length >= sizeof(path)) return false;
    if(!storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_close(file);
        return false;
    }
    bool loaded = storage_file_size(file) == DND_SPLASH_IMAGE_BYTES &&
                  storage_file_read(file, bitmap, DND_SPLASH_IMAGE_BYTES) ==
                      DND_SPLASH_IMAGE_BYTES &&
                  storage_file_get_error(file) == FSE_OK;
    storage_file_close(file);
    return loaded;
}

DndSplashImage* dnd_splash_image_acquire(const char* asset_root) {
    if(!asset_root) return NULL;
    if(furi_record_exists(DND_SPLASH_IMAGE_RECORD)) {
        DndSplashImage* image = furi_record_open(DND_SPLASH_IMAGE_RECORD);
        furi_check(image->references < UINT16_MAX);
        image->references++;
        furi_record_close(DND_SPLASH_IMAGE_RECORD);
        return image;
    }
    DndSplashImage* image = calloc(1, sizeof(DndSplashImage));
    if(!image) return NULL;
    image->references = 1U;
    image->choice = dnd_splash_image_choose();
    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(storage) {
        File* file = storage_file_alloc(storage);
        if(file) {
            const char* alternate = !strcmp(asset_root, DND_SPLASH_IMAGE_HUB_ROOT) ?
                                        DND_SPLASH_IMAGE_FAL_ROOT :
                                        DND_SPLASH_IMAGE_HUB_ROOT;
            uint8_t selected = image->choice;
            for(uint8_t attempt = 0U; attempt < 2U; attempt++) {
                if(attempt && !selected) break;
                image->choice = attempt ? 0U : selected;
                if(dnd_splash_image_read(file, asset_root, image->choice, image->bitmap) ||
                   dnd_splash_image_read(file, alternate, image->choice, image->bitmap)) {
                    image->loaded = true;
                    break;
                }
            }
            storage_file_free(file);
        }
        furi_record_close(RECORD_STORAGE);
    }
    if(!image->loaded) {
        free(image);
        return NULL;
    }
    /* The record owns only heap data, never a callback or pointer into a FAP/FAL.
     * Native records copy their names; the selected bitmap survives module exit. */
    furi_record_create(DND_SPLASH_IMAGE_RECORD, image);
    return image;
}

void dnd_splash_image_draw(Canvas* canvas, const DndSplashImage* image) {
    if(image) {
        canvas_draw_xbm(
            canvas, 0, 0, DND_SPLASH_IMAGE_WIDTH, DND_SPLASH_IMAGE_HEIGHT, image->bitmap);
    } else {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 24, 30, "Loading...");
    }
}

void dnd_splash_image_release(DndSplashImage* image) {
    if(!image) return;
    furi_check(image->references > 0U);
    if(--image->references) return;
    furi_check(furi_record_destroy(DND_SPLASH_IMAGE_RECORD));
    free(image);
}
