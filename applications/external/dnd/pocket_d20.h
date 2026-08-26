#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define POCKET_D20_SAVE_VERSION 2U

#define POCKET_D20_NAME_LEN 32U
#define POCKET_D20_SHORT_LEN 24U
#define POCKET_D20_DETAIL_LEN 192U

#define POCKET_D20_MAX_SPELLS 24U
#define POCKET_D20_MAX_CLASSES 4U
#define POCKET_D20_MAX_FEATURES 20U
#define POCKET_D20_MAX_ITEMS 24U
#define POCKET_D20_MAX_LANGUAGES 12U
#define POCKET_D20_MAX_JOURNAL 24U
#define POCKET_D20_MAX_PARTY 16U
#define POCKET_D20_MAX_INITIATIVE 24U
#define POCKET_D20_MAX_GRANTS 24U
#define POCKET_D20_MAX_ATTACK_TEMPLATES 8U
#define POCKET_D20_MAX_ENCOUNTER_HISTORY 16U

#define POCKET_D20_SKILL_COUNT 18U
#define POCKET_D20_ABILITY_COUNT 6U
#define POCKET_D20_SLOT_COUNT 10U

typedef enum {
    PocketAbilityStrength,
    PocketAbilityDexterity,
    PocketAbilityConstitution,
    PocketAbilityIntelligence,
    PocketAbilityWisdom,
    PocketAbilityCharisma,
} PocketAbility;

typedef enum {
    PocketProficiencyNone,
    PocketProficiencyProficient,
    PocketProficiencyExpertise,
} PocketProficiency;

typedef enum {
    PocketRechargeManual,
    PocketRechargeTurn,
    PocketRechargeEncounter,
    PocketRechargeDawn,
    PocketRechargeShortOrLong,
    PocketRechargeLong,
    PocketRechargeCount,
} PocketRecharge;

typedef enum {
    PocketSizeTiny,
    PocketSizeSmall,
    PocketSizeMedium,
    PocketSizeLarge,
    PocketSizeCount,
} PocketSize;

typedef enum {
    PocketSpellcastingNone,
    PocketSpellcastingFull,
    PocketSpellcastingHalf,
    PocketSpellcastingThird,
    PocketSpellcastingPact,
    PocketSpellcastingSpellPoints,
    PocketSpellcastingCustom,
    PocketSpellcastingModeCount,
} PocketSpellcastingMode;

typedef enum {
    PocketGrantSpecies,
    PocketGrantBackground,
    PocketGrantFeat,
    PocketGrantClassFeature,
    PocketGrantSubclassFeature,
    PocketGrantItem,
    PocketGrantSourceCount,
} PocketGrantSource;

typedef enum {
    PocketGrantPending,
    PocketGrantApplied,
    PocketGrantSkipped,
} PocketGrantStatus;

typedef enum {
    PocketResourceManual,
    PocketResourceProficiency,
    PocketResourceAbility,
    PocketResourceFormulaCount,
} PocketResourceFormula;

typedef enum {
    PocketAttackTemplateUnarmed,
    PocketAttackTemplateSpellAttack,
    PocketAttackTemplateSavingThrow,
    PocketAttackTemplateCustom,
    PocketAttackTemplateTypeCount,
} PocketAttackTemplateType;

typedef enum {
    PocketAttackAbilityAuto,
    PocketAttackAbilityStrength,
    PocketAttackAbilityDexterity,
    PocketAttackAbilityBest,
} PocketAttackAbility;

typedef enum {
    PocketDamageBludgeoning,
    PocketDamagePiercing,
    PocketDamageSlashing,
    PocketDamageAcid,
    PocketDamageCold,
    PocketDamageFire,
    PocketDamageForce,
    PocketDamageLightning,
    PocketDamageNecrotic,
    PocketDamagePoison,
    PocketDamagePsychic,
    PocketDamageRadiant,
    PocketDamageThunder,
    PocketDamageTypeCount,
} PocketDamageType;

typedef enum {
    PocketJournalQuick,
    PocketJournalAdventure,
    PocketJournalItem,
    PocketJournalMilestone,
    PocketJournalCategoryCount,
} PocketJournalCategory;

enum {
    PocketWeaponFinesse = 1U << 0,
    PocketWeaponRanged = 1U << 1,
    PocketWeaponLight = 1U << 2,
    PocketWeaponHeavy = 1U << 3,
    PocketWeaponThrown = 1U << 4,
    PocketWeaponAmmunition = 1U << 5,
};

typedef struct {
    char name[POCKET_D20_NAME_LEN];
    char subclass[POCKET_D20_NAME_LEN];
    uint8_t level;
    uint8_t hit_die;
    uint8_t hit_dice_current;
    uint8_t hit_dice_max;
    uint8_t spellcasting_mode;
    uint8_t spellcasting_ability;
    uint8_t cantrip_limit;
    uint8_t prepared_limit;
    uint16_t spellbook_size;
    uint8_t pact_slot_level;
    uint8_t pact_slots_current;
    uint8_t pact_slots_max;
    uint16_t mystic_arcanum_mask;
    uint16_t spell_points_current;
    uint16_t spell_points_max;
} PocketClassLevel;

typedef struct {
    char name[POCKET_D20_NAME_LEN];
    char detail[POCKET_D20_DETAIL_LEN];
    uint8_t level;
    uint8_t class_index;
    uint8_t prepared;
    uint8_t ritual;
    char stable_id[POCKET_D20_SHORT_LEN];
    char source[POCKET_D20_SHORT_LEN];
    char school[POCKET_D20_SHORT_LEN];
    uint8_t grant_source;
    char grant_name[POCKET_D20_SHORT_LEN];
} PocketSpell;

typedef struct {
    char name[POCKET_D20_NAME_LEN];
    char detail[POCKET_D20_DETAIL_LEN];
    int16_t uses_current;
    int16_t uses_max;
    uint8_t class_index;
    uint8_t class_level_gained;
    uint8_t recharge;
    uint8_t resource_formula;
    uint8_t resource_ability;
} PocketFeature;

typedef struct {
    char name[POCKET_D20_NAME_LEN];
    char detail[POCKET_D20_DETAIL_LEN];
    int16_t quantity;
    int16_t weight_tenths;
    uint8_t equipped;
    uint8_t attuned;
    uint8_t is_weapon;
    uint8_t attack_ability;
    uint8_t proficient;
    int8_t magic_bonus;
    uint8_t damage_dice;
    uint8_t damage_die;
    uint8_t versatile_die;
    uint8_t use_versatile;
    uint8_t damage_type;
    uint8_t add_ability_damage;
    uint8_t extra_dice;
    uint8_t extra_die;
    uint16_t weapon_properties;
    int16_t ammo_current;
    int16_t ammo_max;
    int8_t container_index;
    int16_t charges_current;
    int16_t charges_max;
    uint8_t armor_base;
    int8_t armor_dex_cap;
    uint8_t shield_bonus;
    char ammunition_group[POCKET_D20_SHORT_LEN];
} PocketItem;

typedef struct {
    char stable_id[POCKET_D20_SHORT_LEN];
    char source[POCKET_D20_SHORT_LEN];
    char option_name[POCKET_D20_NAME_LEN];
    char prerequisites[POCKET_D20_NAME_LEN];
    char grant_value[POCKET_D20_NAME_LEN];
    uint8_t source_type;
    uint8_t class_index;
    uint8_t level_gained;
    uint8_t status;
} PocketGrant;

typedef struct {
    char name[POCKET_D20_NAME_LEN];
    char mastery[POCKET_D20_SHORT_LEN];
    char damage_type[POCKET_D20_SHORT_LEN];
    char rider_type[POCKET_D20_SHORT_LEN];
    uint8_t type;
    uint8_t ability;
    uint8_t save_ability;
    int8_t attack_misc;
    uint8_t save_dc;
    uint8_t damage_dice;
    uint8_t damage_die;
    uint8_t rider_dice;
    uint8_t rider_die;
    uint8_t recharge;
} PocketAttackTemplate;

typedef struct {
    char title[POCKET_D20_NAME_LEN];
    char body[POCKET_D20_DETAIL_LEN];
    uint8_t category;
    uint8_t completed;
    uint8_t level_granted;
    uint8_t class_index;
} PocketJournalEntry;

typedef struct {
    char name[POCKET_D20_SHORT_LEN];
    int8_t initiative_modifier;
    int16_t hp_current;
    int16_t hp_max;
    int16_t armor_class;
} PocketPartyMember;

typedef struct {
    char name[POCKET_D20_SHORT_LEN];
    int8_t initiative_modifier;
    int16_t initiative_total;
    uint8_t is_player_character;
    int16_t hp_current;
    int16_t hp_max;
    int16_t armor_class;
    char conditions[POCKET_D20_SHORT_LEN];
} PocketInitiativeEntry;

typedef struct {
    uint16_t round;
    uint8_t current_turn;
    uint8_t kind;
    uint8_t target;
    int16_t value_before;
    int16_t value_after;
} PocketEncounterHistory;

typedef enum {
    PocketHistoryTurn,
    PocketHistoryParticipantHp,
    PocketHistoryFeatureResource,
    PocketHistoryKindCount,
} PocketHistoryKind;

typedef struct {
    uint8_t active;
    uint16_t round;
    uint8_t current_turn;
    uint8_t count;
    PocketInitiativeEntry entries[POCKET_D20_MAX_INITIATIVE];
} PocketInitiativeState;

typedef struct {
    char name[POCKET_D20_NAME_LEN];
    char player[POCKET_D20_NAME_LEN];
    char species[POCKET_D20_NAME_LEN];
    char background[POCKET_D20_NAME_LEN];
    char alignment[POCKET_D20_SHORT_LEN];
    char other_proficiencies[POCKET_D20_DETAIL_LEN];
    char origin_feat[POCKET_D20_NAME_LEN];
    char tool_proficiencies[POCKET_D20_DETAIL_LEN];
    char armor_training[POCKET_D20_DETAIL_LEN];
    char weapon_training[POCKET_D20_DETAIL_LEN];
    uint8_t size;
    char senses[POCKET_D20_DETAIL_LEN];

    uint8_t class_count;
    PocketClassLevel classes[POCKET_D20_MAX_CLASSES];
    uint32_t experience;
    uint8_t milestone_leveling;
    uint8_t inspiration;

    int8_t ability_scores[POCKET_D20_ABILITY_COUNT];
    uint8_t saving_throw_proficiency[POCKET_D20_ABILITY_COUNT];
    uint8_t skill_proficiency[POCKET_D20_SKILL_COUNT];

    int16_t hp_current;
    int16_t hp_max;
    int16_t hp_temporary;
    int16_t armor_class;
    int16_t speed;
    int8_t initiative_misc;
    uint8_t exhaustion;
    uint8_t death_successes;
    uint8_t death_failures;
    uint8_t hit_die;
    uint8_t hit_dice_current;
    uint8_t hit_dice_max;

    uint8_t spellcasting_ability;
    int8_t spell_attack_misc;
    int8_t spell_save_misc;
    uint8_t arcane_recovery_used;
    uint8_t spell_slots_current[POCKET_D20_SLOT_COUNT];
    uint8_t spell_slots_max[POCKET_D20_SLOT_COUNT];

    int32_t currency_cp;
    int32_t currency_sp;
    int32_t currency_ep;
    int32_t currency_gp;
    int32_t currency_pp;

    uint8_t spell_count;
    uint8_t spell_capacity;
    void* spell_storage;
    PocketSpell* spells;
    uint8_t* spell_known;
    uint8_t* spell_always_prepared;
    uint8_t* spell_free_casts_current;
    uint8_t* spell_free_casts_max;
    uint8_t feature_count;
    uint8_t feature_capacity;
    PocketFeature* features;
    uint8_t item_count;
    uint8_t item_capacity;
    PocketItem* items;
    uint8_t language_count;
    char languages[POCKET_D20_MAX_LANGUAGES][POCKET_D20_SHORT_LEN];
    uint8_t journal_count;
    uint8_t journal_capacity;
    PocketJournalEntry* journal;

    int8_t saving_throw_misc[POCKET_D20_ABILITY_COUNT];
    int8_t skill_misc[POCKET_D20_SKILL_COUNT];

    char conditions[POCKET_D20_DETAIL_LEN];
    char concentration[POCKET_D20_NAME_LEN];
    uint8_t reaction_available;
    char temporary_effects[POCKET_D20_DETAIL_LEN];
    char resistances[POCKET_D20_DETAIL_LEN];
    char immunities[POCKET_D20_DETAIL_LEN];
    char vulnerabilities[POCKET_D20_DETAIL_LEN];
    char movement_modes[POCKET_D20_DETAIL_LEN];

    uint8_t grant_count;
    uint8_t grant_capacity;
    PocketGrant* grants;
    uint8_t attack_template_count;
    PocketAttackTemplate attack_templates[POCKET_D20_MAX_ATTACK_TEMPLATES];
    uint8_t encumbrance_mode;
    int16_t carrying_capacity_override;

    char adventure_campaign[POCKET_D20_SHORT_LEN];
    char adventure_scene[POCKET_D20_SHORT_LEN];
    char adventure_checkpoint[POCKET_D20_SHORT_LEN];
    uint32_t adventure_quest_flags;
    uint32_t adventure_achievements;
} PocketCharacter;

typedef struct {
    PocketCharacter character;
    uint8_t party_count;
    PocketPartyMember party[POCKET_D20_MAX_PARTY];
    PocketInitiativeState initiative;
    uint8_t encounter_history_count;
    PocketEncounterHistory encounter_history[POCKET_D20_MAX_ENCOUNTER_HISTORY];
} PocketSaveData;

void pocket_d20_data_set_defaults(PocketSaveData* data);
void pocket_d20_data_clear(PocketSaveData* data);
void pocket_d20_data_sanitize(PocketSaveData* data);
bool pocket_d20_data_reserve_spells(PocketCharacter* character, uint8_t required);
bool pocket_d20_data_reserve_features(PocketCharacter* character, uint8_t required);
bool pocket_d20_data_reserve_items(PocketCharacter* character, uint8_t required);
bool pocket_d20_data_reserve_journal(PocketCharacter* character, uint8_t required);
bool pocket_d20_data_reserve_grants(PocketCharacter* character, uint8_t required);
