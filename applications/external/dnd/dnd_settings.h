#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <storage/storage.h>

typedef struct {
    uint8_t skip_dice_loading;
    uint8_t debug;
    uint8_t extra_items;
    uint8_t catalog_all;
    uint8_t homebrew;
    uint8_t menu_type;
} DndSettings;

/* Minimal cross-FAP settings projection. Add fields only when a child FAP
   genuinely needs them; do not copy the complete DndSettings into every app. */
typedef struct {
    uint8_t debug;
    uint8_t homebrew;
} DndSharedSettings;

enum {
    DndMenuTypeText = 0U,
    DndMenuTypeGraphical = 1U,
};

void dnd_settings_defaults(DndSettings* settings);
void dnd_settings_shared_defaults(DndSharedSettings* settings);
/* Best-effort load: defaults are always established first. Missing/unreadable files and
   malformed/truncated lines keep defaults or already-recovered values; false is reserved
   for invalid arguments. */
bool dnd_settings_load(Storage* storage, DndSettings* settings);
bool dnd_settings_load_shared(Storage* storage, DndSharedSettings* settings);
bool dnd_settings_save(Storage* storage, const DndSettings* settings);
