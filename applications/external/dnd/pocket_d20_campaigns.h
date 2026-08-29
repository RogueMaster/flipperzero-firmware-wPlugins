#pragma once

#include "pocket_d20.h"

#include <stdbool.h>
#include <stdint.h>
#include <storage/storage.h>

#define POCKET_CAMPAIGN_PACK_VERSION 1U
#define POCKET_CAMPAIGN_APP_VERSION  301U
#define POCKET_CAMPAIGN_ID_LEN       32U

typedef struct {
    char id[POCKET_CAMPAIGN_ID_LEN];
    char name[POCKET_D20_NAME_LEN];
    uint8_t pack_version;
    uint16_t minimum_app;
    uint16_t maximum_app;
    char entry_scene[POCKET_D20_SHORT_LEN];
    char scenes_file[POCKET_D20_SHORT_LEN];
    uint8_t bundled;
} PocketCampaignSummary;

typedef struct {
    uint16_t records;
    uint16_t incompatible;
    uint16_t missing_scene_files;
    uint16_t duplicate_campaign_ids;
    uint16_t duplicate_scene_ids;
    uint16_t missing_entry_scenes;
    uint16_t broken_links;
    char problem_id[POCKET_CAMPAIGN_ID_LEN];
    char problem[48];
} PocketCampaignDiagnostics;

uint16_t pocket_campaign_count(Storage* storage);
bool pocket_campaign_at(Storage* storage, uint16_t index, PocketCampaignSummary* output);
bool pocket_campaign_find(Storage* storage, const char* id, PocketCampaignSummary* output);
bool pocket_campaign_scene_path(
    Storage* storage,
    const PocketCampaignSummary* campaign,
    char* output,
    size_t size);
bool pocket_campaign_progress_load(
    Storage* storage,
    uint32_t profile_id,
    const PocketCampaignSummary* campaign,
    PocketCharacter* character);
bool pocket_campaign_progress_save(
    Storage* storage,
    uint32_t profile_id,
    const PocketCampaignSummary* campaign,
    const PocketCharacter* character);
bool pocket_campaign_migrate_legacy_custom(Storage* storage, uint16_t* copied_files);
void pocket_campaign_diagnose(Storage* storage, PocketCampaignDiagnostics* output);
void pocket_campaign_cache_reset(void);
