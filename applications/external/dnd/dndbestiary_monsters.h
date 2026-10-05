#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <storage/storage.h>

#define DND_MONSTER_ID_LEN        32U
#define DND_MONSTER_NAME_LEN      40U
#define DND_MONSTER_TEXT_LEN      192U
#define DND_MONSTER_ENCOUNTER_MAX 12U
#define DND_MONSTER_PACK_VERSION  1U

enum {
    DndMonsterFieldSize = 1U << 0,
    DndMonsterFieldSpeed = 1U << 1,
    DndMonsterFieldAbilities = 1U << 2,
    DndMonsterFieldSenses = 1U << 3,
    DndMonsterFieldLanguages = 1U << 4,
    DndMonsterFieldActions = 1U << 5,
    DndMonsterFieldInitiative = 1U << 6,
    DndMonsterRequiredFields = 0x3FU,
};

typedef enum {
    DndEncounterLow,
    DndEncounterModerate,
    DndEncounterHigh,
    DndEncounterDifficultyCount,
} DndEncounterDifficulty;

typedef enum {
    DndEncounterBalanced,
    DndEncounterHorde,
    DndEncounterElite,
    DndEncounterTemplateCount,
} DndEncounterTemplate;

typedef struct {
    char id[DND_MONSTER_ID_LEN];
    char name[DND_MONSTER_NAME_LEN];
    uint8_t cr_eighths;
    uint32_t xp;
    uint8_t armor_class;
    uint16_t hit_points;
    char type[24];
    char environment[24];
    char source[24];
    char role[16];
} DndMonsterSummary;

typedef struct {
    DndMonsterSummary summary;
    char size_alignment[48];
    char speed[64];
    int8_t abilities[6];
    int8_t initiative_modifier;
    uint8_t initiative_present;
    char skills[DND_MONSTER_TEXT_LEN];
    char defenses[DND_MONSTER_TEXT_LEN];
    char senses[DND_MONSTER_TEXT_LEN];
    char languages[96];
    char traits[DND_MONSTER_TEXT_LEN];
    char actions[DND_MONSTER_TEXT_LEN];
    char extra[DND_MONSTER_TEXT_LEN];
    uint16_t present_fields;
} DndMonsterDetail;

typedef struct {
    DndMonsterSummary monsters[DND_MONSTER_ENCOUNTER_MAX];
    uint8_t quantities[DND_MONSTER_ENCOUNTER_MAX];
    uint8_t count;
    uint32_t budget;
    uint32_t spent;
} DndMonsterEncounter;

typedef struct {
    uint32_t spent;
    uint32_t low_budget;
    uint32_t moderate_budget;
    uint32_t high_budget;
    DndEncounterDifficulty classification;
} DndEncounterSimulation;

typedef enum {
    DndEncounterWarningUnsupportedLeader = 1U << 0,
    DndEncounterWarningExposedArtillery = 1U << 1,
    DndEncounterWarningMinionDensity = 1U << 2,
} DndEncounterWarning;

typedef struct {
    uint16_t total_creatures;
    uint16_t leaders;
    uint16_t artillery;
    uint16_t frontline;
    uint16_t minions;
    uint8_t warning_flags;
} DndEncounterComposition;

typedef bool (*DndMonsterFilter)(const DndMonsterSummary* summary, void* context);

bool dndbestiary_monsters_source_allowed(const DndMonsterSummary* summary, bool allow_homebrew);
uint32_t dndbestiary_monsters_xp_budget(
    uint8_t party_level,
    uint8_t party_size,
    DndEncounterDifficulty difficulty);
void dndbestiary_monsters_validate_pack(
    Storage* storage,
    uint16_t* total,
    uint16_t* valid,
    uint16_t* invalid);
bool dndbestiary_monsters_find(Storage* storage, const char* id, DndMonsterSummary* output);
bool dndbestiary_monsters_initiative_modifier(
    Storage* storage,
    const DndMonsterSummary* summary,
    int8_t* modifier);
uint16_t dndbestiary_monsters_query(
    Storage* storage,
    DndMonsterFilter filter,
    void* context,
    uint16_t start,
    DndMonsterSummary* output,
    uint16_t capacity,
    uint16_t* total_matches);
uint16_t dndbestiary_monsters_sample(
    Storage* storage,
    DndMonsterFilter filter,
    void* context,
    DndMonsterSummary* output,
    uint16_t capacity,
    uint16_t* total_matches);
bool dndbestiary_monsters_load(
    Storage* storage,
    const DndMonsterSummary* summary,
    DndMonsterDetail* output);
bool dndbestiary_monsters_save_custom(Storage* storage, DndMonsterDetail* detail);
bool dndbestiary_monsters_update_custom(Storage* storage, DndMonsterDetail* detail);
bool dndbestiary_monsters_delete_custom(Storage* storage, const DndMonsterSummary* summary);
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
    DndEncounterDifficulty difficulty,
    const char* environment,
    bool allow_repeats,
    DndEncounterTemplate template_kind,
    const char* preferred_role,
    bool allow_homebrew,
    DndMonsterEncounter* output);
void dndbestiary_monsters_simulate(
    DndMonsterEncounter* encounter,
    uint8_t party_level,
    uint8_t party_size,
    DndEncounterSimulation* output);
void dndbestiary_monsters_analyze_composition(
    const DndMonsterEncounter* encounter,
    uint8_t party_size,
    DndEncounterComposition* output);
