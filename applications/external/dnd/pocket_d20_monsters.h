#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <storage/storage.h>

#define POCKET_MONSTER_ID_LEN 32U
#define POCKET_MONSTER_NAME_LEN 40U
#define POCKET_MONSTER_TEXT_LEN 192U
#define POCKET_MONSTER_ENCOUNTER_MAX 12U
#define POCKET_MONSTER_PACK_VERSION 1U

enum {
    PocketMonsterFieldSize = 1U << 0,
    PocketMonsterFieldSpeed = 1U << 1,
    PocketMonsterFieldAbilities = 1U << 2,
    PocketMonsterFieldSenses = 1U << 3,
    PocketMonsterFieldLanguages = 1U << 4,
    PocketMonsterFieldActions = 1U << 5,
    PocketMonsterRequiredFields = 0x3FU,
};

typedef enum {
    PocketEncounterLow,
    PocketEncounterModerate,
    PocketEncounterHigh,
    PocketEncounterDifficultyCount,
} PocketEncounterDifficulty;

typedef enum {
    PocketEncounterBalanced,
    PocketEncounterHorde,
    PocketEncounterElite,
    PocketEncounterTemplateCount,
} PocketEncounterTemplate;

typedef struct {
    char id[POCKET_MONSTER_ID_LEN];
    char name[POCKET_MONSTER_NAME_LEN];
    uint8_t cr_eighths;
    uint32_t xp;
    uint8_t armor_class;
    uint16_t hit_points;
    char type[24];
    char environment[24];
    char source[24];
    char role[16];
} PocketMonsterSummary;

typedef struct {
    PocketMonsterSummary summary;
    char size_alignment[48];
    char speed[64];
    int8_t abilities[6];
    char skills[POCKET_MONSTER_TEXT_LEN];
    char defenses[POCKET_MONSTER_TEXT_LEN];
    char senses[POCKET_MONSTER_TEXT_LEN];
    char languages[96];
    char traits[POCKET_MONSTER_TEXT_LEN];
    char actions[POCKET_MONSTER_TEXT_LEN];
    char extra[POCKET_MONSTER_TEXT_LEN];
    uint16_t present_fields;
} PocketMonsterDetail;

typedef struct {
    PocketMonsterSummary monsters[POCKET_MONSTER_ENCOUNTER_MAX];
    uint8_t quantities[POCKET_MONSTER_ENCOUNTER_MAX];
    uint8_t count;
    uint32_t budget;
    uint32_t spent;
} PocketMonsterEncounter;

typedef bool (*PocketMonsterFilter)(const PocketMonsterSummary* summary, void* context);

uint32_t pocket_monster_xp_budget(
    uint8_t party_level,
    uint8_t party_size,
    PocketEncounterDifficulty difficulty);
uint16_t pocket_monster_count(Storage* storage);
void pocket_monster_validate_pack(
    Storage* storage,
    uint16_t* total,
    uint16_t* valid,
    uint16_t* invalid);
bool pocket_monster_at(Storage* storage, uint16_t index, PocketMonsterSummary* output);
uint16_t pocket_monster_query(
    Storage* storage,
    PocketMonsterFilter filter,
    void* context,
    uint16_t start,
    PocketMonsterSummary* output,
    uint16_t capacity,
    uint16_t* total_matches);
uint16_t pocket_monster_sample(
    Storage* storage,
    PocketMonsterFilter filter,
    void* context,
    PocketMonsterSummary* output,
    uint16_t capacity,
    uint16_t* total_matches);
bool pocket_monster_load(Storage* storage, const PocketMonsterSummary* summary, PocketMonsterDetail* output);
bool pocket_monster_save_custom(Storage* storage, PocketMonsterDetail* detail);
bool pocket_monster_update_custom(Storage* storage, PocketMonsterDetail* detail);
bool pocket_monster_delete_custom(Storage* storage, const PocketMonsterSummary* summary);
bool pocket_monster_recover_user_pack(
    Storage* storage,
    uint16_t* recovered,
    uint16_t* rolled_back);
void pocket_monster_pack_versions(
    Storage* storage,
    uint8_t* bundled_version,
    uint8_t* user_version,
    bool* user_present);
bool pocket_monster_generate(
    Storage* storage,
    uint8_t party_level,
    uint8_t party_size,
    PocketEncounterDifficulty difficulty,
    const char* environment,
    bool allow_repeats,
    PocketEncounterTemplate template_kind,
    const char* preferred_role,
    PocketMonsterEncounter* output);
