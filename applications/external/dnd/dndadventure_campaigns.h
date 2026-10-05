#pragma once

#include "dnd_data.h"

#include <stdbool.h>
#include <stdint.h>
#include <storage/storage.h>

#define DNDADVENTURE_PACK_VERSION    1U
#define DNDADVENTURE_APP_VERSION     301U
#define DNDADVENTURE_CAMPAIGN_ID_LEN 32U

typedef struct {
    char id[DNDADVENTURE_CAMPAIGN_ID_LEN];
    char name[DND_NAME_LEN];
    uint8_t pack_version;
    uint16_t minimum_app;
    uint16_t maximum_app;
    char entry_scene[DND_SHORT_LEN];
    char scenes_file[DND_SHORT_LEN];
    uint8_t bundled;
} DndAdventureCampaignSummary;

typedef struct {
    char campaign[DNDADVENTURE_CAMPAIGN_ID_LEN];
    char scene[DND_SHORT_LEN];
    char checkpoint[DND_SHORT_LEN];
    uint32_t quest_flags;
    uint32_t achievements;
} DndAdventureCampaignProgress;

typedef struct {
    uint16_t records;
    uint16_t incompatible;
    uint16_t missing_scene_files;
    uint16_t duplicate_campaign_ids;
    uint16_t duplicate_scene_ids;
    uint16_t missing_entry_scenes;
    uint16_t broken_links;
    char problem_id[DNDADVENTURE_CAMPAIGN_ID_LEN];
    char problem[48];
} DndAdventureCampaignDiagnostics;

uint16_t dndadventure_campaigns_count(Storage* storage);
bool dndadventure_campaigns_at(
    Storage* storage,
    uint16_t index,
    DndAdventureCampaignSummary* output);
bool dndadventure_campaigns_find(
    Storage* storage,
    const char* id,
    DndAdventureCampaignSummary* output);
bool dndadventure_campaigns_scene_path(
    Storage* storage,
    const DndAdventureCampaignSummary* campaign,
    char* output,
    size_t size);
bool dndadventure_campaigns_active_load(
    Storage* storage,
    uint32_t profile_id,
    char* campaign_id,
    size_t campaign_id_size);
bool dndadventure_campaigns_active_save(
    Storage* storage,
    uint32_t profile_id,
    const char* campaign_id);
bool dndadventure_campaigns_progress_load(
    Storage* storage,
    uint32_t profile_id,
    const DndAdventureCampaignSummary* campaign,
    DndAdventureCampaignProgress* progress);
bool dndadventure_campaigns_progress_save(
    Storage* storage,
    uint32_t profile_id,
    const DndAdventureCampaignSummary* campaign,
    const DndAdventureCampaignProgress* progress);
void dndadventure_campaigns_diagnose(Storage* storage, DndAdventureCampaignDiagnostics* output);
void dndadventure_campaigns_cache_reset(void);
