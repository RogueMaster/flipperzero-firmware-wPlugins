#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <storage/storage.h>

#define DND_PACK_ID_LEN   32U
#define DND_PACK_NAME_LEN 40U

typedef struct {
    char id[DND_PACK_ID_LEN];
    char name[DND_PACK_NAME_LEN];
    uint8_t enabled;
} DndBestiaryPackSummary;

uint16_t dndbestiary_packs_count(Storage* storage);
bool dndbestiary_packs_at(Storage* storage, uint16_t index, DndBestiaryPackSummary* output);
bool dndbestiary_packs_install_inbox(Storage* storage, char* status, size_t status_size);
bool dndbestiary_packs_set_enabled(Storage* storage, const char* id, bool enabled);
bool dndbestiary_packs_rebuild_enabled(Storage* storage);
bool dndbestiary_packs_ensure_enabled(Storage* storage);
