#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <storage/storage.h>

#include "dnd_data.h"

#define DND_CHARACTER_COLLECTION_WINDOW    4U
#define DND_CHARACTER_PROFICIENCY_TYPE_LEN 16U

typedef struct {
    char type[DND_CHARACTER_PROFICIENCY_TYPE_LEN];
    char name[DND_CATALOG_NAME_LEN];
} DndCharacterProficiency;

void dnd_character_languages_path(char* output, size_t size, uint32_t profile);
void dnd_character_proficiencies_path(char* output, size_t size, uint32_t profile);

bool dnd_character_languages_count(Storage* storage, uint32_t profile, uint16_t* total);
bool dnd_character_languages_load_window(
    Storage* storage,
    uint32_t profile,
    uint16_t start,
    char entries[DND_CHARACTER_COLLECTION_WINDOW][DND_CATALOG_NAME_LEN],
    uint8_t* count,
    uint16_t* total);
bool dnd_character_languages_append(Storage* storage, uint32_t profile, const char* name);
bool dnd_character_languages_delete(Storage* storage, uint32_t profile, uint16_t logical_index);
bool dnd_character_languages_contains(
    Storage* storage,
    uint32_t profile,
    const char* name,
    bool* found);

bool dnd_character_proficiencies_count(Storage* storage, uint32_t profile, uint16_t* total);
bool dnd_character_proficiencies_load_window(
    Storage* storage,
    uint32_t profile,
    uint16_t start,
    DndCharacterProficiency entries[DND_CHARACTER_COLLECTION_WINDOW],
    uint8_t* count,
    uint16_t* total);
bool dnd_character_proficiencies_append(
    Storage* storage,
    uint32_t profile,
    const char* type,
    const char* name);
bool dnd_character_proficiencies_delete(Storage* storage, uint32_t profile, uint16_t logical_index);
bool dnd_character_proficiencies_contains(
    Storage* storage,
    uint32_t profile,
    const char* type,
    const char* name,
    bool* found);

bool dnd_character_languages_replace(
    Storage* storage,
    uint32_t profile,
    uint16_t index,
    const char* name);
bool dnd_character_proficiencies_replace(
    Storage* storage,
    uint32_t profile,
    uint16_t index,
    const char* type,
    const char* name);
