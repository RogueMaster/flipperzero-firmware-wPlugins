#pragma once

#include "dndbestiary_monsters.h"

#include <stdbool.h>
#include <stdint.h>
#include <storage/storage.h>

#define POCKET_BESTIARY_FILTER_NAME_LEN    32U
#define POCKET_BESTIARY_ENCOUNTER_NAME_LEN 32U

typedef struct {
    char name[POCKET_BESTIARY_FILTER_NAME_LEN];
    char search[POCKET_MONSTER_NAME_LEN];
    uint8_t max_cr_eighths;
    uint8_t type_filter;
    uint8_t source_filter;
    uint8_t environment_filter;
    uint8_t role_filter;
} PocketBestiaryFilterPreset;

typedef struct {
    char name[POCKET_BESTIARY_ENCOUNTER_NAME_LEN];
    uint8_t party_level;
    uint8_t party_size;
    uint8_t difficulty;
    uint8_t count;
    char monster_ids[POCKET_MONSTER_ENCOUNTER_MAX][POCKET_MONSTER_ID_LEN];
    uint8_t quantities[POCKET_MONSTER_ENCOUNTER_MAX];
} PocketSavedEncounter;

bool dndbestiary_state_party_settings_load(
    Storage* storage,
    uint8_t* party_level,
    uint8_t* party_size);
bool dndbestiary_state_party_settings_save(
    Storage* storage,
    uint8_t party_level,
    uint8_t party_size);

bool dndbestiary_state_favorite_contains(Storage* storage, const char* id);
bool dndbestiary_state_favorite_toggle(Storage* storage, const char* id, bool* now_favorite);
uint16_t dndbestiary_state_favorite_count(Storage* storage);
bool dndbestiary_state_favorite_at(Storage* storage, uint16_t index, char* id, size_t size);

bool dndbestiary_state_recent_add(Storage* storage, const char* id);
uint16_t dndbestiary_state_recent_count(Storage* storage);
bool dndbestiary_state_recent_at(Storage* storage, uint16_t index, char* id, size_t size);

uint16_t dndbestiary_state_filter_count(Storage* storage);
bool dndbestiary_state_filter_at(
    Storage* storage,
    uint16_t index,
    PocketBestiaryFilterPreset* output);
bool dndbestiary_state_filter_save(Storage* storage, const PocketBestiaryFilterPreset* preset);
bool dndbestiary_state_filter_delete(Storage* storage, uint16_t index);

uint16_t dndbestiary_state_encounter_count(Storage* storage);
bool dndbestiary_state_encounter_at(Storage* storage, uint16_t index, PocketSavedEncounter* output);
bool dndbestiary_state_encounter_save(Storage* storage, const PocketSavedEncounter* encounter);
bool dndbestiary_state_encounter_delete(Storage* storage, uint16_t index);
bool dndbestiary_state_encounter_rename(Storage* storage, uint16_t index, const char* new_name);
bool dndbestiary_state_encounter_duplicate(Storage* storage, uint16_t index, const char* new_name);
bool dndbestiary_state_encounter_archive(Storage* storage, uint16_t index);
