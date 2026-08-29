#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <storage/storage.h>

#define POCKET_PACK_ID_LEN   32U
#define POCKET_PACK_NAME_LEN 40U

typedef enum {
    PocketPackCampaign,
    PocketPackMonster,
} PocketPackKind;

typedef struct {
    char id[POCKET_PACK_ID_LEN];
    char name[POCKET_PACK_NAME_LEN];
    uint8_t enabled;
} PocketPackSummary;

uint16_t pocket_pack_count(Storage* storage, PocketPackKind kind);
bool pocket_pack_at(
    Storage* storage,
    PocketPackKind kind,
    uint16_t index,
    PocketPackSummary* output);
bool pocket_pack_install_inbox(
    Storage* storage,
    PocketPackKind kind,
    char* status,
    size_t status_size);
bool pocket_pack_set_enabled(Storage* storage, PocketPackKind kind, const char* id, bool enabled);
bool pocket_pack_rebuild_enabled(Storage* storage, PocketPackKind kind);
