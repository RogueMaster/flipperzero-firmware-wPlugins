#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define DND_SAVE_VERSION 5U

#define DND_NAME_LEN           32U
#define DND_CHARACTER_NAME_LEN 25U
#define DND_CLASS_NAME_LEN     16U
#define DND_SUBCLASS_NAME_LEN  31U
#define DND_SPELL_NAME_LEN     31U
#define DND_FEATURE_NAME_LEN   31U
#define DND_ITEM_NAME_LEN      47U
#define DND_CATALOG_NAME_LEN   DND_ITEM_NAME_LEN
#define DND_SHORT_LEN          24U
#define DND_DETAIL_LEN         192U
#define DND_GRANT_VALUE_LEN    64U

#define DND_MAX_CLASSES           4U
#define DND_RESIDENT_RECORD_LIMIT 8U
#define DND_MAX_GRANTS            24U
#define DND_MAX_ATTACK_TEMPLATES  10U

#define DND_SKILL_COUNT   18U
#define DND_ABILITY_COUNT 6U
#define DND_SLOT_COUNT    10U

typedef enum {
    DndAbilityStrength,
    DndAbilityDexterity,
    DndAbilityConstitution,
    DndAbilityIntelligence,
    DndAbilityWisdom,
    DndAbilityCharisma,
} DndAbility;

typedef enum {
    DndProficiencyNone,
    DndProficiencyProficient,
    DndProficiencyExpertise,
} DndProficiency;

typedef enum {
    DndRechargeManual,
    DndRechargeTurn,
    DndRechargeEncounter,
    DndRechargeDawn,
    DndRechargeShortOrLong,
    DndRechargeLong,
    DndRechargeCount,
} DndRecharge;

typedef enum {
    DndSizeTiny,
    DndSizeSmall,
    DndSizeMedium,
    DndSizeLarge,
    DndSizeCount,
} DndSize;

typedef enum {
    DndSpellcastingNone,
    DndSpellcastingFull,
    DndSpellcastingHalf,
    DndSpellcastingThird,
    DndSpellcastingPact,
    DndSpellcastingSpellPoints,
    DndSpellcastingCustom,
    DndSpellcastingModeCount,
} DndSpellcastingMode;

typedef enum {
    DndGrantSpecies,
    DndGrantBackground,
    DndGrantFeat,
    DndGrantClassFeature,
    DndGrantSubclassFeature,
    DndGrantItem,
    DndGrantSourceCount,
} DndGrantSource;

typedef enum {
    DndGrantPending,
    DndGrantApplied,
    DndGrantSkipped,
} DndGrantStatus;

typedef enum {
    DndResourceManual,
    DndResourceProficiency,
    DndResourceAbility,
    DndResourceFormulaCount,
} DndResourceFormula;

typedef enum {
    /* Preserve the original numeric values 0-3 for save compatibility. */
    DndAttackTemplateUnarmed = 0,
    DndAttackTemplateSpellAttack = 1,
    DndAttackTemplateSavingThrow = 2,
    DndAttackTemplateCustom = 3,
    DndAttackTemplateGrapple = 4,
    DndAttackTemplateShove = 5,
    DndAttackTemplateTypeCount,
} DndAttackTemplateType;

typedef enum {
    DndAttackAbilityAuto,
    DndAttackAbilityStrength,
    DndAttackAbilityDexterity,
    DndAttackAbilityBest,
} DndAttackAbility;

typedef enum {
    DndDamageBludgeoning,
    DndDamagePiercing,
    DndDamageSlashing,
    DndDamageAcid,
    DndDamageCold,
    DndDamageFire,
    DndDamageForce,
    DndDamageLightning,
    DndDamageNecrotic,
    DndDamagePoison,
    DndDamagePsychic,
    DndDamageRadiant,
    DndDamageThunder,
    DndDamageTypeCount,
} DndDamageType;

enum {
    DndWeaponFinesse = 1U << 0,
    DndWeaponRanged = 1U << 1,
    DndWeaponLight = 1U << 2,
    DndWeaponHeavy = 1U << 3,
    DndWeaponThrown = 1U << 4,
    DndWeaponAmmunition = 1U << 5,
};

typedef struct {
    char name[DND_CLASS_NAME_LEN];
    char subclass[DND_SUBCLASS_NAME_LEN];
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
} DndClassLevel;

typedef struct {
    char name[DND_SPELL_NAME_LEN];
    char detail[DND_DETAIL_LEN];
    uint8_t level;
    uint8_t class_index;
    uint8_t prepared;
    uint8_t ritual;
    char stable_id[DND_SHORT_LEN];
    char source[DND_SHORT_LEN];
    char school[DND_SHORT_LEN];
    uint8_t grant_source;
    uint8_t favorite;
    char grant_name[DND_SHORT_LEN];
} DndSpell;

typedef struct {
    char name[DND_FEATURE_NAME_LEN];
    char detail[DND_DETAIL_LEN];
    int16_t uses_current;
    int16_t uses_max;
    uint8_t class_index;
    uint8_t class_level_gained;
    uint8_t recharge;
    uint8_t resource_formula;
    uint8_t resource_ability;
} DndFeature;

typedef struct {
    char name[DND_ITEM_NAME_LEN];
    char detail[DND_DETAIL_LEN];
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
    int32_t container_index;
    int16_t charges_current;
    int16_t charges_max;
    uint8_t armor_base;
    int8_t armor_dex_cap;
    uint8_t shield_bonus;
    char ammunition_group[DND_SHORT_LEN];
} DndItem;

typedef struct {
    char stable_id[DND_SHORT_LEN];
    char source[DND_SHORT_LEN];
    char option_name[DND_NAME_LEN];
    char prerequisites[DND_NAME_LEN];
    char grant_value[DND_GRANT_VALUE_LEN];
    uint8_t source_type;
    uint8_t class_index;
    uint8_t level_gained;
    uint8_t status;
} DndGrant;

typedef struct {
    char name[DND_NAME_LEN];
    char mastery[DND_SHORT_LEN];
    char damage_type[DND_SHORT_LEN];
    char rider_type[DND_SHORT_LEN];
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
} DndAttackTemplate;

typedef struct {
    char name[DND_CHARACTER_NAME_LEN];
    char player[DND_NAME_LEN];
    char species[DND_NAME_LEN];
    char background[DND_NAME_LEN];
    char alignment[DND_SHORT_LEN];
    char origin_feat[DND_NAME_LEN];
    uint8_t size;
    char senses[DND_DETAIL_LEN];

    uint8_t class_count;
    DndClassLevel classes[DND_MAX_CLASSES];
    uint32_t experience;
    uint8_t milestone_leveling;
    uint8_t inspiration;

    int8_t ability_scores[DND_ABILITY_COUNT];
    uint8_t saving_throw_proficiency[DND_ABILITY_COUNT];
    uint8_t skill_proficiency[DND_SKILL_COUNT];

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
    uint8_t spell_slots_current[DND_SLOT_COUNT];
    uint8_t spell_slots_max[DND_SLOT_COUNT];

    int32_t currency_cp;
    int32_t currency_sp;
    int32_t currency_ep;
    int32_t currency_gp;
    int32_t currency_pp;

    uint8_t spell_count;
    uint8_t spell_capacity;
    void* spell_storage;
    DndSpell* spells;
    uint8_t* spell_known;
    uint8_t* spell_always_prepared;
    uint8_t* spell_free_casts_current;
    uint8_t* spell_free_casts_max;
    uint8_t feature_count;
    uint8_t feature_capacity;
    DndFeature* features;
    uint8_t item_count;
    uint8_t item_capacity;
    DndItem* items;
    int8_t saving_throw_misc[DND_ABILITY_COUNT];
    int8_t skill_misc[DND_SKILL_COUNT];

    char conditions[DND_DETAIL_LEN];
    char concentration[DND_NAME_LEN];
    uint8_t reaction_available;
    char temporary_effects[DND_DETAIL_LEN];
    char resistances[DND_DETAIL_LEN];
    char immunities[DND_DETAIL_LEN];
    char vulnerabilities[DND_DETAIL_LEN];
    char movement_modes[DND_DETAIL_LEN];

    uint8_t grant_count;
    uint8_t grant_capacity;
    DndGrant* grants;
    uint8_t attack_template_count;
    DndAttackTemplate attack_templates[DND_MAX_ATTACK_TEMPLATES];
    uint8_t encumbrance_mode;
    int16_t carrying_capacity_override;

} DndCharacter;

typedef struct {
    DndCharacter character;
} DndSaveData;

void dnd_data_set_defaults(DndSaveData* data);
void dnd_data_clear(DndSaveData* data);
void dnd_data_sanitize(DndSaveData* data);
bool dnd_data_reserve_spells(DndCharacter* character, uint8_t required);
void dnd_data_clear_spells(DndCharacter* character);
bool dnd_data_reserve_features(DndCharacter* character, uint8_t required);
bool dnd_data_reserve_features_exact(DndCharacter* character, uint8_t required);
bool dnd_data_reserve_items(DndCharacter* character, uint8_t required);
void dnd_data_clear_items(DndCharacter* character);
bool dnd_data_reserve_grants(DndCharacter* character, uint8_t required);
bool dnd_data_reserve_grants_exact(DndCharacter* character, uint8_t required);
