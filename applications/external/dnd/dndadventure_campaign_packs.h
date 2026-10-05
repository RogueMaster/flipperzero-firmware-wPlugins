#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <storage/storage.h>

#define DNDADVENTURE_PACK_ID_LEN    32U
#define DNDADVENTURE_PACK_NAME_LEN  40U
#define DNDADVENTURE_PACK_ENTRY_LEN 32U

typedef struct {
    char id[DNDADVENTURE_PACK_ID_LEN];
    char name[DNDADVENTURE_PACK_NAME_LEN];
    uint8_t enabled;
} DndAdventureCampaignPackSummary;

typedef struct {
    DndAdventureCampaignPackSummary summary;
    uint8_t pack_version;
    uint16_t minimum_app;
    uint16_t maximum_app;
    char entry_scene[DNDADVENTURE_PACK_ENTRY_LEN];
    uint8_t content_present;
    uint8_t entry_present;
} DndAdventureCampaignPackPreview;

uint16_t dndadventure_campaign_packs_count(Storage* storage);
bool dndadventure_campaign_packs_at(
    Storage* storage,
    uint16_t index,
    DndAdventureCampaignPackSummary* output);
bool dndadventure_campaign_packs_preview_inbox(
    Storage* storage,
    DndAdventureCampaignPackPreview* output,
    char* status,
    size_t status_size);
bool dndadventure_campaign_packs_install_inbox(Storage* storage, char* status, size_t status_size);
bool dndadventure_campaign_packs_set_enabled(Storage* storage, const char* id, bool enabled);
bool dndadventure_campaign_packs_rebuild_enabled(Storage* storage);
bool dndadventure_campaign_packs_ensure_enabled(Storage* storage);
