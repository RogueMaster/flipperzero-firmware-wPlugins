#pragma once
#include "dnd_data.h"
#include "dnd_plugin_loader.h"
#define DND_CHARACTER_SHEET_API_ID      "dnd_character_sheet"
#define DND_CHARACTER_SHEET_API_VERSION 1U
#define DND_CHARACTER_SHEET_HUB_PATH    "/ext/apps_data/dndolphins/plugins/dnd_character_sheet.fal"
#define DND_CHARACTER_SHEET_STANDALONE_PATH \
    "/ext/apps_data/dndcharactersheet/plugins/dnd_character_sheet.fal"
typedef struct {
    uint32_t size;
    DndPluginUiResult (
        *run)(ViewDispatcher* dispatcher, const DndCharacter* character, bool loading_on_return);
} DndCharacterSheetApi;
