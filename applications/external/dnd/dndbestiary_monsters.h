#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <storage/storage.h>

#define POCKET_MONSTER_ID_LEN        32U
#define POCKET_MONSTER_NAME_LEN      40U
#define POCKET_MONSTER_TEXT_LEN      192U
#define POCKET_MONSTER_ENCOUNTER_MAX 12U
#define POCKET_MONSTER_PACK_VERSION  1U

enum {
    PocketMonsterFieldSize = 1U << 0,
    PocketMonsterFieldSpeed = 1U << 1,
    PocketMonsterFieldAbilities = 1U << 2,
    PocketMonsterFieldSenses = 1U << 3,
    PocketMonsterFieldLanguages = 1U << 4,
    PocketMonsterFieldActions = 1U << 5,
    PocketMonsterFieldInitiative = 1U << 6,
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
    int8_t initiative_modifier;
    uint8_t initiative_present;
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

typedef struct {
    uint32_t spent;
    uint32_t low_budget;
    uint32_t moderate_budget;
    uint32_t high_budget;
    PocketEncounterDifficulty classification;
} PocketEncounterSimulation;

typedef enum {
    PocketEncounterWarningUnsupportedLeader = 1U << 0,
    PocketEncounterWarningExposedArtillery = 1U << 1,
    PocketEncounterWarningMinionDensity = 1U << 2,
} PocketEncounterWarning;

typedef struct {
    uint16_t total_creatures;
    uint16_t leaders;
    uint16_t artillery;
    uint16_t frontline;
    uint16_t minions;
    uint8_t warning_flags;
} PocketEncounterComposition;

typedef bool (*PocketMonsterFilter)(const PocketMonsterSummary* summary, void* context);

uint32_t dndbestiary_monsters_xp_budget(
    uint8_t party_level,
    uint8_t party_size,
    PocketEncounterDifficulty difficulty);
void dndbestiary_monsters_validate_pack(
    Storage* storage,
    uint16_t* total,
    uint16_t* valid,
    uint16_t* invalid);
bool dndbestiary_monsters_find(Storage* storage, const char* id, PocketMonsterSummary* output);
bool dndbestiary_monsters_initiative_modifier(
    Storage* storage,
    const PocketMonsterSummary* summary,
    int8_t* modifier);
uint16_t dndbestiary_monsters_query(
    Storage* storage,
    PocketMonsterFilter filter,
    void* context,
    uint16_t start,
    PocketMonsterSummary* output,
    uint16_t capacity,
    uint16_t* total_matches);
uint16_t dndbestiary_monsters_sample(
    Storage* storage,
    PocketMonsterFilter filter,
    void* context,
    PocketMonsterSummary* output,
    uint16_t capacity,
    uint16_t* total_matches);
bool dndbestiary_monsters_load(
    Storage* storage,
    const PocketMonsterSummary* summary,
    PocketMonsterDetail* output);
bool dndbestiary_monsters_save_custom(Storage* storage, PocketMonsterDetail* detail);
bool dndbestiary_monsters_update_custom(Storage* storage, PocketMonsterDetail* detail);
bool dndbestiary_monsters_delete_custom(Storage* storage, const PocketMonsterSummary* summary);
bool dndbestiary_monsters_migrate_legacy_custom(Storage* storage, uint16_t* copied_files);
bool dndbestiary_monsters_seed_default_custom(Storage* storage, uint16_t* copied_files);
bool dndbestiary_monsters_recover_user_pack(
    Storage* storage,
    uint16_t* recovered,
    uint16_t* rolled_back);
void dndbestiary_monsters_cache_reset(void);
void dndbestiary_monsters_pack_versions(
    Storage* storage,
    uint8_t* bundled_version,
    uint8_t* user_version,
    bool* user_present);
bool dndbestiary_monsters_generate(
    Storage* storage,
    uint8_t party_level,
    uint8_t party_size,
    PocketEncounterDifficulty difficulty,
    const char* environment,
    bool allow_repeats,
    PocketEncounterTemplate template_kind,
    const char* preferred_role,
    PocketMonsterEncounter* output);
void dndbestiary_monsters_simulate(
    PocketMonsterEncounter* encounter,
    uint8_t party_level,
    uint8_t party_size,
    PocketEncounterSimulation* output);
void dndbestiary_monsters_analyze_composition(
    const PocketMonsterEncounter* encounter,
    uint8_t party_size,
    PocketEncounterComposition* output);
