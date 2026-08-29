#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <storage/storage.h>

#define POCKET_CAMPAIGN_PACK_ID_LEN   32U
#define POCKET_CAMPAIGN_PACK_NAME_LEN 40U

typedef struct {
    char id[POCKET_CAMPAIGN_PACK_ID_LEN];
    char name[POCKET_CAMPAIGN_PACK_NAME_LEN];
    uint8_t enabled;
} PocketCampaignPackSummary;

uint16_t pocket_campaign_pack_count(Storage* storage);
bool pocket_campaign_pack_at(Storage* storage, uint16_t index, PocketCampaignPackSummary* output);
bool pocket_campaign_pack_install_inbox(Storage* storage, char* status, size_t status_size);
bool pocket_campaign_pack_set_enabled(Storage* storage, const char* id, bool enabled);
bool pocket_campaign_pack_rebuild_enabled(Storage* storage);
bool pocket_campaign_pack_ensure_enabled(Storage* storage);
