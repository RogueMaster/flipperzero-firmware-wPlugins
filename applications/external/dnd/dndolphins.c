#include "dnd_data.h"
#include "dnd_profile_handoff.h"
#include "dnd_fs.h"
#include "dnd_rules.h"
#include "dndolphins_rules_character.h"
#include "dndolphins_weapon_combat.h"
#include "dndolphins_spells.h"
#include "dndolphins_spell_combat.h"
#include "dnd_storage.h"
#include "dndolphins_progression_store.h"

#include <furi.h>
#include <gui/gui.h>
#include <gui/modules/number_input.h>
#include <gui/modules/text_input.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <input/input.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TAG                              "PocketD20"
#define POCKET_D20_MAX_GENERIC_ROLLS     20U
#define POCKET_D20_DICE_ANIMATION_FRAMES 8U
#define POCKET_D20_LONG_BACK_EVENT       0xD121U
#define POCKET_D20_AUTOSAVE_EVENT        0xD122U
#define POCKET_D20_UI_TICK_MS             100U
#define POCKET_D20_MAX_CATALOG_ENTRIES   24U
#define POCKET_D20_MAX_SPELL_ATTACK_ROLLS 12U

typedef enum {
    PocketPendingLaunchNone,
    PocketPendingLaunchBestiary,
    PocketPendingLaunchJournal,
    PocketPendingLaunchAdventure,
    PocketPendingLaunchInitiative,
    PocketPendingLaunchInventory,
    PocketPendingLaunchSpellbook,
} PocketPendingLaunch;
#define POCKET_D20_SPELL_PAGE_ENTRIES    10U
#define POCKET_D20_MARQUEE_MS            350U
#define POCKET_D20_AUTOSAVE_MS           450U
#define POCKET_D20_COMBAT_VISIBLE_ROWS   5U
#define POCKET_D20_COMBAT_ROW_LEN        64U

typedef enum {
    PocketViewMain,
    PocketViewTextInput,
    PocketViewNumberInput,
} PocketViewId;

typedef enum {
    PocketScreenHome,
    PocketScreenProfiles,
    PocketScreenProfileActions,
    PocketScreenCharacter,
    PocketScreenVitals,
    PocketScreenAbilities,
    PocketScreenSkills,
    PocketScreenGrantReview,
    PocketScreenGrantEdit,
    PocketScreenLevelReview,
    PocketScreenLevelChoice,
    PocketScreenAsiAbility,
    PocketScreenMagic,
    PocketScreenRecordList,
    PocketScreenRecordDetail,
    PocketScreenCatalog,
    PocketScreenCombat,
    PocketScreenSpellAttacks,
    PocketScreenRituals,
    PocketScreenSpellCast,
    PocketScreenSpellResult,
    PocketScreenAttackTemplates,
    PocketScreenAttackTemplateEdit,
    PocketScreenDice,
    PocketScreenDiceResult,
    PocketScreenAttackList,
    PocketScreenAttackResult,
    PocketScreenAbout,
} PocketScreen;

typedef enum {
    PocketListClasses,
    PocketListFeatures,
    PocketListLanguages,
} PocketListKind;

typedef enum {
    PocketCatalogClasses,
    PocketCatalogSubclasses,
    PocketCatalogSpecies,
    PocketCatalogBackgrounds,
    PocketCatalogAlignments,
    PocketCatalogFeats,
    PocketCatalogCount,
} PocketCatalogKind;

enum {
    PocketClassMaskArtificer = 1U << 0,
    PocketClassMaskBarbarian = 1U << 1,
    PocketClassMaskBard = 1U << 2,
    PocketClassMaskCleric = 1U << 3,
    PocketClassMaskDruid = 1U << 4,
    PocketClassMaskFighter = 1U << 5,
    PocketClassMaskMonk = 1U << 6,
    PocketClassMaskPaladin = 1U << 7,
    PocketClassMaskRanger = 1U << 8,
    PocketClassMaskRogue = 1U << 9,
    PocketClassMaskSorcerer = 1U << 10,
    PocketClassMaskWarlock = 1U << 11,
    PocketClassMaskWizard = 1U << 12,
};

typedef struct {
    const char* name;
    uint8_t level;
    uint16_t class_mask;
} PocketBuiltinSpell;

typedef struct {
    const char* name;
    uint16_t class_mask;
} PocketBuiltinSubclass;

typedef enum {
    PocketItemCategoryOther,
    PocketItemCategoryWeapon,
    PocketItemCategoryArmor,
    PocketItemCategoryGear,
    PocketItemCategoryTool,
    PocketItemCategoryMountVehicle,
    PocketItemCategoryPotion,
    PocketItemCategoryRing,
    PocketItemCategoryRod,
    PocketItemCategoryScroll,
    PocketItemCategoryStaff,
    PocketItemCategoryWand,
    PocketItemCategoryWondrous,
} PocketItemCategory;

typedef enum {
    PocketItemFilterAll,
    PocketItemFilterWeapons,
    PocketItemFilterArmor,
    PocketItemFilterAmmunition,
    PocketItemFilterGear,
    PocketItemFilterTools,
    PocketItemFilterMagic,
    PocketItemFilterCount,
} PocketItemFilter;

typedef enum {
    PocketEditNone,
    PocketEditCharacterName,
    PocketEditPlayerName,
    PocketEditSpecies,
    PocketEditBackground,
    PocketEditAlignment,
    PocketEditOtherProficiencies,
    PocketEditOriginFeat,
    PocketEditToolProficiencies,
    PocketEditArmorTraining,
    PocketEditWeaponTraining,
    PocketEditSenses,
    PocketEditConditions,
    PocketEditConcentration,
    PocketEditTemporaryEffects,
    PocketEditResistances,
    PocketEditImmunities,
    PocketEditVulnerabilities,
    PocketEditMovementModes,
    PocketEditClassName,
    PocketEditSubclass,
    PocketEditGrantStableId,
    PocketEditGrantSource,
    PocketEditGrantOption,
    PocketEditGrantPrerequisites,
    PocketEditGrantValue,
    PocketEditAttackName,
    PocketEditAttackMastery,
    PocketEditAttackDamageType,
    PocketEditAttackRiderType,
    PocketEditFeatureName,
    PocketEditFeatureDetail,
    PocketEditLanguageName,
} PocketEditTarget;

typedef enum {
    PocketNumberNone,
    PocketNumberCharacter,
    PocketNumberVitals,
    PocketNumberAbility,
    PocketNumberSkill,
    PocketNumberMagic,
    PocketNumberRecord,
    PocketNumberDice,
    PocketNumberCombat,
} PocketNumberContext;

typedef struct {
    Gui* gui;
    Storage* storage;
    ViewDispatcher* dispatcher;
    View* main_view;
    TextInput* text_input;
    NumberInput* number_input;
    FuriTimer* autosave_timer;

    PocketSaveData data;
    PocketProfileState profiles;
    uint32_t saved_fingerprint;
    uint32_t saved_spellbook_fingerprint;
    uint32_t saved_items_fingerprint;
    uint32_t saved_features_fingerprint;
    uint8_t spellbook_loaded;
    uint8_t items_loaded;
    uint8_t features_loaded;
    uint8_t spellbook_total;
    uint8_t items_total;
    uint8_t features_total;
    uint8_t spellbook_cache_start;
    uint8_t items_cache_start;
    uint8_t features_cache_start;
    DndDolphinsSpellClassCounts spell_class_counts;
    uint8_t spell_class_counts_valid;
    uint8_t combat_spell_count;
    uint8_t* combat_spell_indices;
    uint8_t combat_weapon_count;
    uint8_t* combat_weapon_indices;
    PocketScreen screen;
    PocketScreen return_screen;
    PocketScreen record_list_return_screen;
    PocketListKind list_kind;
    uint16_t selection;
    uint16_t scroll;
    uint16_t home_return_selection;
    uint8_t record_index;
    PocketCatalogKind catalog_kind;
    PocketEditTarget catalog_target;
    uint16_t catalog_count;
    uint16_t catalog_capacity;
    uint16_t catalog_total;
    uint16_t catalog_scan_count;
    uint16_t catalog_page_start;
    uint16_t catalog_page_size;
    uint16_t catalog_return_selection;
    void* catalog_storage;
    char (*catalog_entries)[POCKET_D20_CATALOG_NAME_LEN];
    uint8_t* catalog_levels;
    uint16_t* catalog_class_masks;
    uint8_t* catalog_has_metadata;
    uint8_t catalog_show_all;
    uint8_t catalog_has_more;
    uint8_t edit_modifier_mode;
    uint8_t arcane_recovery_active;
    uint8_t arcane_recovery_budget;
    uint8_t arcane_recovery_spent;
    uint8_t arcane_recovery_restored[6];
    uint8_t hit_die_class_index;
    uint32_t profile_action_id;
    uint8_t active_profile_loaded;
    uint8_t storage_read_only;
    uint8_t storage_unsaved;
    uint8_t autosave_pending;
    uint16_t storage_failure_count;
    PocketPendingLaunch pending_launch;
    PocketEditTarget edit_target;
    char edit_buffer[POCKET_D20_DETAIL_LEN];
    PocketNumberContext number_context;
    uint8_t number_index;
    uint8_t number_aux;
    uint8_t input_module_active;
    FuriPubSub* input_events;
    FuriPubSubSubscription* input_subscription;

    PocketRollMode roll_mode;
    uint8_t dice_count;
    uint8_t dice_sides;
    int16_t dice_modifier;
    int16_t dice_result;
    uint8_t dice_first;
    uint8_t dice_second;
    uint8_t dice_guidance;
    uint8_t dice_roll_values[POCKET_D20_MAX_GENERIC_ROLLS];
    uint8_t dice_roll_value_count;
    uint16_t dice_roll_sum;
    uint8_t damage_roll_page;
    uint8_t initiative_delete_armed;
    uint8_t dice_animating;
    uint8_t dice_anim_frame;
    uint8_t dice_anim_sides;
    uint8_t dice_anim_count;
    uint16_t marquee_elapsed_ms;

    uint8_t attack_item_index;
    uint8_t attack_phase;
    PocketAttackRoll attack_roll;
    PocketDamageRoll damage_roll;

    uint8_t spell_attack_index;
    uint8_t spell_cast_level;
    uint8_t spell_cast_resource;
    uint8_t spell_cast_class_index;
    uint8_t spell_cast_primary_dice;
    uint8_t spell_cast_primary_die;
    uint8_t spell_cast_secondary_dice;
    uint8_t spell_cast_secondary_die;
    uint8_t spell_cast_resolution;
    uint8_t spell_cast_secondary_resolution;
    uint8_t spell_cast_secondary_relation;
    uint8_t spell_cast_derived_effect;
    uint8_t spell_cast_from_notes;
    uint8_t spell_cast_attack_roll_count;
    uint8_t spell_cast_attack_natural[POCKET_D20_MAX_SPELL_ATTACK_ROLLS];
    int16_t spell_cast_attack_damage[POCKET_D20_MAX_SPELL_ATTACK_ROLLS];
    uint8_t spell_cast_natural;
    int16_t spell_cast_attack_total;
    int16_t spell_cast_primary_total;
    int16_t spell_cast_secondary_total;
    int16_t spell_cast_flat_bonus;
    int16_t spell_cast_secondary_flat_bonus;
    int16_t spell_cast_damage_total;

    uint8_t level_review_class_index;
    uint8_t level_review_old_level;
    uint8_t level_review_new_level;
    uint8_t level_review_old_pb;
    uint8_t level_review_new_pb;
    uint8_t level_review_old_cantrips;
    uint8_t level_review_new_cantrips;
    uint8_t level_review_old_prepared;
    uint8_t level_review_new_prepared;
    uint8_t level_review_slots_changed;
    uint8_t level_review_choose_spells;
    uint8_t level_review_pending_choice;

    uint8_t level_choice_class_index;
    uint8_t level_choice_level;
    uint8_t level_choice_mode;
    uint8_t level_choice_first_ability;
    int8_t level_choice_first_score;

    uint8_t action_ack_active;
    uint8_t action_ack_screen;
    uint16_t action_ack_selection;
    char status[32];
} PocketD20App;

static bool dndolphins_begin_next_level_choice(PocketD20App* app);
static void dndolphins_handle_level_review(PocketD20App* app, const InputEvent* event);
static void dndolphins_handle_level_choice(PocketD20App* app, const InputEvent* event);
static void dndolphins_handle_asi_ability(PocketD20App* app, const InputEvent* event);

static bool dndolphins_refresh_combat_spell_index(PocketD20App* app);
static bool dndolphins_refresh_ritual_spell_index(PocketD20App* app);
static bool dndolphins_refresh_combat_weapon_index(PocketD20App* app);
static void dndolphins_prepare_combat_spell_rows(PocketD20App* app, bool ritual_mode);
static void dndolphins_prepare_combat_weapon_rows(PocketD20App* app);

static void dndolphins_text_done(void* context);
static void dndolphins_roll_generic(PocketD20App* app);
static void dndolphins_handle_long_back(PocketD20App* app);
static void dndolphins_release_text_input(PocketD20App* app);
static void dndolphins_release_number_input(PocketD20App* app);
static void dndolphins_quiesce_async(PocketD20App* app);
static bool dndolphins_flush_save(PocketD20App* app, bool report);
static void dndolphins_collection_save_failed(PocketD20App* app);
static uint8_t dndolphins_marquee_offset = 0U;

typedef enum {
    DndolphinsHomeCharacters = 0,
    DndolphinsHomeCharacter,
    DndolphinsHomeVitals,
    DndolphinsHomeAbilitiesSaves,
    DndolphinsHomeSkills,
    DndolphinsHomeFeaturesPerks,
    DndolphinsHomeInventory,
    DndolphinsHomeMagicSpells,
    DndolphinsHomeBestiary,
    DndolphinsHomeInitiative,
    DndolphinsHomeCombat,
    DndolphinsHomeDiceRoller,
    DndolphinsHomeAdventure,
    DndolphinsHomeJournal,
    DndolphinsHomeCount,
} DndolphinsHomeIndex;

typedef enum {
    DndolphinsCombatAttackMode = 0,
    DndolphinsCombatWeaponAttacks,
    DndolphinsCombatSpellAttacks,
    DndolphinsCombatRituals,
    DndolphinsCombatAttackTemplates,
    DndolphinsCombatInitiative,
    DndolphinsCombatHp,
    DndolphinsCombatTemporaryHp,
    DndolphinsCombatShortRest,
    DndolphinsCombatSpendHitDie,
    DndolphinsCombatLongRest,
    DndolphinsCombatConditions,
    DndolphinsCombatConcentration,
    DndolphinsCombatReaction,
    DndolphinsCombatTemporaryEffects,
    DndolphinsCombatResistances,
    DndolphinsCombatImmunities,
    DndolphinsCombatVulnerabilities,
    DndolphinsCombatSenses,
    DndolphinsCombatMovement,
    DndolphinsCombatDeathSuccesses,
    DndolphinsCombatDeathFailures,
    DndolphinsCombatExhaustion,
    DndolphinsCombatCount,
} DndolphinsCombatIndex;

static const char* const dndolphins_home_items[DndolphinsHomeCount] = {
    [DndolphinsHomeCharacters] = "Characters",
    [DndolphinsHomeCharacter] = "Character",
    [DndolphinsHomeVitals] = "Vitals",
    [DndolphinsHomeAbilitiesSaves] = "Abilities & Saves",
    [DndolphinsHomeSkills] = "Skills",
    [DndolphinsHomeFeaturesPerks] = "Features & Perks",
    [DndolphinsHomeInventory] = "Inventory",
    [DndolphinsHomeMagicSpells] = "Magic & Spells",
    [DndolphinsHomeBestiary] = "Bestiary",
    [DndolphinsHomeInitiative] = "Initiative",
    [DndolphinsHomeCombat] = "Combat",
    [DndolphinsHomeDiceRoller] = "Dice Roller",
    [DndolphinsHomeAdventure] = "Adventure",
    [DndolphinsHomeJournal] = "Journal",
};

static const char* const dndolphins_home_retry_save = "Retry Save";

static bool dndolphins_home_index_from_return_focus(
    const char* args,
    DndolphinsHomeIndex* home_index) {
    if(!args || !home_index) return false;
    if(strcmp(args, POCKET_D20_RETURN_FOCUS_INVENTORY) == 0)
        *home_index = DndolphinsHomeInventory;
    else if(strcmp(args, POCKET_D20_RETURN_FOCUS_SPELLBOOK) == 0)
        *home_index = DndolphinsHomeMagicSpells;
    else if(strcmp(args, POCKET_D20_RETURN_FOCUS_ADVENTURE) == 0)
        *home_index = DndolphinsHomeAdventure;
    else if(strcmp(args, POCKET_D20_RETURN_FOCUS_JOURNAL) == 0)
        *home_index = DndolphinsHomeJournal;
    else if(strcmp(args, POCKET_D20_RETURN_FOCUS_INITIATIVE) == 0)
        *home_index = DndolphinsHomeInitiative;
    else if(strcmp(args, POCKET_D20_RETURN_FOCUS_BESTIARY) == 0)
        *home_index = DndolphinsHomeBestiary;
    else
        return false;
    return true;
}

static void dndolphins_set_home_focus(PocketD20App* app, DndolphinsHomeIndex home_index) {
    if(!app || home_index >= DndolphinsHomeCount) return;
    app->home_return_selection = (uint16_t)home_index;
    if(app->screen == PocketScreenHome) {
        app->selection = (uint16_t)home_index;
        app->scroll = app->selection >= 5U ? (uint16_t)(app->selection - 4U) : 0U;
    }
}

static void dndolphins_apply_return_focus(PocketD20App* app, const char* args) {
    if(!app || !args) return;
    DndolphinsHomeIndex home_index;
    if(!dndolphins_home_index_from_return_focus(args, &home_index)) return;
    dndolphins_set_home_focus(app, home_index);
    /* Startup is already on Home, but keep this explicit so a future caller can
       apply return focus before/after Home initialization without raw indices. */
    app->selection = app->home_return_selection;
    app->scroll = app->selection >= 5U ? (uint16_t)(app->selection - 4U) : 0U;
}

static const char* const dndolphins_profile_actions[] = {
    "Switch / Open",
    "Rename Active",
    "Duplicate",
    "Export",
    "Import First Export",
    "Archive",
    "Delete",
    "Verify Save",
    "Restore Backup",
};

static const uint8_t dndolphins_die_choices[] = {4U, 6U, 8U, 10U, 12U, 20U, 100U};
static const uint8_t dndolphins_damage_die_choices[] = {4U, 6U, 8U, 10U, 12U};
static const char* const dndolphins_roll_mode_names[] =
    {"Normal", "Advantage", "Disadvantage", "Guidance"};
static const char* const dndolphins_recharge_names[] =
    {"Manual", "Turn", "Encounter", "Dawn", "Short/Long", "Long"};
static const char* const dndolphins_attack_template_type_names[] =
    {"Unarmed", "Spell Attack", "Saving Throw", "Custom"};
static const char* const dndolphins_size_names[] = {"Tiny", "Small", "Medium", "Large"};
static const char* const dndolphins_spellcasting_mode_names[] =
    {"None", "Full", "Half", "Third", "Pact", "Spell Points", "Custom"};
static const char* const dndolphins_resource_formula_names[] = {"Manual", "PB", "Ability"};

typedef enum {
    PocketSpellSourceUnknown = 0U,
    PocketSpellSourceCore,
    PocketSpellSourceXanathar,
    PocketSpellSourceForgottenRealms,
    PocketSpellSourceRavenloft,
    PocketSpellSourceOther,
    PocketSpellSourceCount,
} PocketSpellSource;


/* Group the standard skills by governing ability without changing their save indexes. */
static const uint8_t dndolphins_skill_display_order[POCKET_D20_SKILL_COUNT] = {
    3U, /* STR: Athletics */
    0U,
    15U,
    16U, /* DEX: Acrobatics, Sleight of Hand, Stealth */
    2U,
    5U,
    8U,
    10U,
    14U, /* INT: Arcana, History, Investigation, Nature, Religion */
    1U,
    6U,
    9U,
    11U,
    17U, /* WIS: Animal Handling, Insight, Medicine, Perception, Survival */
    4U,
    7U,
    12U,
    13U, /* CHA: Deception, Intimidation, Performance, Persuasion */
};

static const char* const dndolphins_catalog_classes[] = {
    "Artificer",
    "Barbarian",
    "Bard",
    "Cleric",
    "Druid",
    "Fighter",
    "Monk",
    "Paladin",
    "Ranger",
    "Rogue",
    "Sorcerer",
    "Warlock",
    "Wizard"};

static const PocketBuiltinSubclass dndolphins_catalog_subclasses[] = {
    {"Path of the Berserker", PocketClassMaskBarbarian},
    {"College of Lore", PocketClassMaskBard},
    {"Life Domain", PocketClassMaskCleric},
    {"Circle of the Land", PocketClassMaskDruid},
    {"Champion", PocketClassMaskFighter},
    {"Warrior of the Open Hand", PocketClassMaskMonk},
    {"Oath of Devotion", PocketClassMaskPaladin},
    {"Hunter", PocketClassMaskRanger},
    {"Thief", PocketClassMaskRogue},
    {"Draconic Sorcery", PocketClassMaskSorcerer},
    {"Fiend Patron", PocketClassMaskWarlock},
    {"Evoker", PocketClassMaskWizard},
};

static const char* const dndolphins_catalog_backgrounds[] = {
    "Acolyte",
    "Artisan",
    "Charlatan",
    "Criminal",
    "Entertainer",
    "Farmer",
    "Guard",
    "Guide",
    "Hermit",
    "Merchant",
    "Noble",
    "Sage",
    "Sailor",
    "Scribe",
    "Soldier",
    "Wayfarer",
    "Haunted One",
    "Investigator",
    "Chondathan Freebooter",
    "Dead Magic Dweller",
    "Dragon Cultist",
    "Emerald Enclave Caretaker",
    "Flaming Fist Mercenary",
    "Genie Touched",
    "Harper",
    "Ice Fisher",
    "Knight of the Gauntlet",
    "Lords' Alliance Vassal",
    "Moonwell Pilgrim",
    "Mulhorandi Tomb Raider",
    "Mythalkeeper",
    "Purple Dragon Squire",
    "Rashemi Wanderer",
    "Shadowmasters Exile",
    "Spellfire Initiate",
    "Zhentarim Mercenary",
};

static const char* const dndolphins_catalog_species[] = {
    "Aasimar",
    "Black Dragonborn",
    "Blue Dragonborn",
    "Brass Dragonborn",
    "Bronze Dragonborn",
    "Copper Dragonborn",
    "Gold Dragonborn",
    "Green Dragonborn",
    "Red Dragonborn",
    "Silver Dragonborn",
    "White Dragonborn",
    "Dwarf",
    "Drow Elf",
    "High Elf",
    "Wood Elf",
    "Forest Gnome",
    "Rock Gnome",
    "Cloud Giant Goliath",
    "Fire Giant Goliath",
    "Frost Giant Goliath",
    "Hill Giant Goliath",
    "Stone Giant Goliath",
    "Storm Giant Goliath",
    "Halfling",
    "Human",
    "Orc",
    "Abyssal Tiefling",
    "Chthonic Tiefling",
    "Infernal Tiefling"};

static const char* const dndolphins_catalog_alignments[] = {
    "Lawful Good",
    "Neutral Good",
    "Chaotic Good",
    "Lawful Neutral",
    "True Neutral",
    "Chaotic Neutral",
    "Lawful Evil",
    "Neutral Evil",
    "Chaotic Evil"};

static const char* const dndolphins_catalog_feats[] = {
    "Alert",
    "Magic Initiate",
    "Savage Attacker",
    "Skilled",
    "Ability Score Improvement",
    "Grappler",
    "Archery",
    "Defense",
    "Great Weapon Fighting",
    "Two-Weapon Fighting",
    "Boon of Combat Prowess",
    "Boon of Dimensional Travel",
    "Boon of Fate",
    "Boon of Irresistible Offense",
    "Boon of the Night Spirit",
    "Boon of Spell Recall",
    "Boon of Truesight",
};


static const char* const dndolphins_bundled_catalog_paths[PocketCatalogCount] = {
    APP_ASSETS_PATH("catalogs/classes.txt"),
    APP_ASSETS_PATH("catalogs/subclasses.txt"),
    APP_ASSETS_PATH("catalogs/species.txt"),
    APP_ASSETS_PATH("catalogs/backgrounds.txt"),
    APP_ASSETS_PATH("catalogs/alignments.txt"),
    APP_ASSETS_PATH("catalogs/feats.txt"),
};

static const char* const dndolphins_bundled_metadata_path = APP_ASSETS_PATH("metadata/options.txt");
static const char* const dndolphins_bundled_catalog_abilities_path =
    APP_ASSETS_PATH("catalogs/abilities.txt");
static const char* const dndolphins_progression_spell_metadata_path =
    APP_ASSETS_PATH("metadata/progression_spells.txt");

static const char* dndolphins_active_metadata_path(Storage* storage) {
    UNUSED(storage);
    return dndolphins_bundled_metadata_path;
}

static void dndolphins_copy(char* destination, size_t size, const char* source) {
    if(size == 0U) return;
    strncpy(destination, source, size - 1U);
    destination[size - 1U] = '\0';
}

static bool dndolphins_parse_u32_strict(const char* text, uint32_t maximum, uint32_t* output) {
    if(!text || !text[0] || !output) return false;
    uint32_t value = 0U;
    for(const char* cursor = text; *cursor; ++cursor) {
        if(*cursor < '0' || *cursor > '9') return false;
        uint32_t digit = (uint32_t)(*cursor - '0');
        if(value > maximum / 10U ||
           (value == maximum / 10U && digit > maximum % 10U))
            return false;
        value = value * 10U + digit;
    }
    *output = value;
    return true;
}



/*
 * Append display text without asking snprintf to prove that a persistent field
 * fits in a smaller UI row. RogueMaster treats -Wformat-truncation as an error,
 * so rows are bounded explicitly here. Detail rows are sized for a complete
 * persistent text field, and the row renderer horizontally scrolls that text.
 */
static void dndolphins_format_labeled_text(
    char* destination,
    size_t size,
    const char* label,
    const char* value) {
    if(size == 0U) return;
    size_t position = 0U;
    if(label) {
        while(label[position] && position + 1U < size) {
            destination[position] = label[position];
            ++position;
        }
    }
    if(value) {
        size_t source = 0U;
        while(value[source] && position + 1U < size) {
            destination[position++] = value[source++];
        }
    }
    destination[position] = '\0';
}

static void dndolphins_catalog_release(PocketD20App* app) {
    free(app->catalog_storage);
    app->catalog_storage = NULL;
    app->catalog_entries = NULL;
    app->catalog_levels = NULL;
    app->catalog_class_masks = NULL;
    app->catalog_has_metadata = NULL;
    app->catalog_count = 0U;
    app->catalog_capacity = 0U;
}

static uint16_t dndolphins_catalog_page_limit(const PocketD20App* app) {
    if(app->catalog_page_size) return app->catalog_page_size;
    /* Item/Spell catalogs live in their standalone FAPs. DNDolphins only opens
       the remaining character/feature catalogs, which use the normal page. */
    return POCKET_D20_MAX_CATALOG_ENTRIES;
}

static bool dndolphins_catalog_ensure_capacity(PocketD20App* app, uint16_t needed) {
    if(needed <= app->catalog_capacity) return true;
    const uint16_t page_limit = dndolphins_catalog_page_limit(app);
    if(needed > page_limit || app->catalog_storage) return false;
    const size_t capacity = page_limit;
    size_t bytes =
        capacity * sizeof(*app->catalog_entries) + capacity * sizeof(*app->catalog_levels) +
        capacity * sizeof(*app->catalog_class_masks) +
        capacity * sizeof(*app->catalog_has_metadata);
    uint8_t* cursor = malloc(bytes);
    if(!cursor) return false;
    app->catalog_storage = cursor;
    app->catalog_entries = (void*)cursor;
    cursor += capacity * sizeof(*app->catalog_entries);
    app->catalog_levels = cursor;
    cursor += capacity * sizeof(*app->catalog_levels);
    app->catalog_class_masks = (void*)cursor;
    cursor += capacity * sizeof(*app->catalog_class_masks);
    app->catalog_has_metadata = cursor;
    cursor += capacity * sizeof(*app->catalog_has_metadata);
    app->catalog_capacity = (uint16_t)capacity;
    return true;
}

static int16_t dndolphins_clamp_i16(int16_t value, int16_t minimum, int16_t maximum) {
    if(value < minimum) return minimum;
    if(value > maximum) return maximum;
    return value;
}

static uint8_t dndolphins_clamp_u8(int16_t value, uint8_t maximum) {
    if(value < 0) return 0U;
    if(value > maximum) return maximum;
    return (uint8_t)value;
}

static void dndolphins_clear_status(PocketD20App* app) {
    app->status[0] = '\0';
}

static void dndolphins_set_status(PocketD20App* app, const char* status) {
    dndolphins_copy(app->status, sizeof(app->status), status);
}

static bool dndolphins_status_is_one_shot_success(const PocketD20App* app) {
    if(!app || !app->status[0]) return false;
    return !strcmp(app->status, "Saved") || !strcmp(app->status, "Already saved") ||
           !strcmp(app->status, "Catalog choice saved");
}

static void dndolphins_clear_action_ack(PocketD20App* app) {
    app->action_ack_active = 0U;
}

static void dndolphins_confirm_action(PocketD20App* app, const char* status) {
    if(app->storage_unsaved) {
        dndolphins_set_status(app, "UNSAVED - retry SD");
        return;
    }
    app->action_ack_active = 1U;
    app->action_ack_screen = (uint8_t)app->screen;
    app->action_ack_selection = app->selection;
    dndolphins_set_status(app, status);
}

static void dndolphins_prefix_action_mark(char* row, size_t size) {
    if(!row || size < 5U) return;
    size_t length = strlen(row);
    if(length > size - 5U) length = size - 5U;
    memmove(row + 4U, row, length);
    memcpy(row, "[X] ", 4U);
    row[length + 4U] = '\0';
}

static void dndolphins_refresh(PocketD20App* app) {
    (void)view_get_model(app->main_view);
    view_commit_model(app->main_view, true);
}

static void dndolphins_autosave_timer_callback(void* context) {
    PocketD20App* app = context;
    view_dispatcher_send_custom_event(app->dispatcher, POCKET_D20_AUTOSAVE_EVENT);
}

static void dndolphins_input_events_callback(const void* value, void* context) {
    PocketD20App* app = context;
    const InputEvent* event = value;
    if(app->input_module_active && event && event->key == InputKeyBack &&
       event->type == InputTypeLong)
        view_dispatcher_send_custom_event(app->dispatcher, POCKET_D20_LONG_BACK_EVENT);
}

static void dndolphins_quiesce_async(PocketD20App* app) {
    if(!app) return;
    if(app->input_subscription && app->input_events) {
        furi_pubsub_unsubscribe(app->input_events, app->input_subscription);
        app->input_subscription = NULL;
    }
    if(app->autosave_timer) furi_timer_stop(app->autosave_timer);
}

static void dndolphins_start_dice_animation(PocketD20App* app, uint8_t count, uint8_t sides) {
    app->dice_animating = 1U;
    app->dice_anim_frame = 0U;
    app->dice_anim_count = count ? count : 1U;
    app->dice_anim_sides = sides >= 2U ? sides : 20U;
    app->marquee_elapsed_ms = 0U;
}

static void dndolphins_tick_event_callback(void* context) {
    PocketD20App* app = context;
    bool refresh = false;

    if(app->dice_animating) {
        ++app->dice_anim_frame;
        if(app->dice_anim_frame >= POCKET_D20_DICE_ANIMATION_FRAMES) {
            app->dice_animating = 0U;
            app->marquee_elapsed_ms = 0U;
        }
        refresh = true;
    } else {
        app->marquee_elapsed_ms += POCKET_D20_UI_TICK_MS;
        if(app->marquee_elapsed_ms >= POCKET_D20_MARQUEE_MS) {
            app->marquee_elapsed_ms -= POCKET_D20_MARQUEE_MS;
            ++dndolphins_marquee_offset;
            refresh = true;
        }
    }

    if(refresh) dndolphins_refresh(app);
}

static bool dndolphins_custom_event_callback(void* context, uint32_t event) {
    PocketD20App* app = context;
    if(event == POCKET_D20_AUTOSAVE_EVENT) {
        dndolphins_flush_save(app, false);
        dndolphins_refresh(app);
        return true;
    }
    if(event == POCKET_D20_LONG_BACK_EVENT) {
        app->input_module_active = 0U;
        app->edit_target = PocketEditNone;
        app->number_context = PocketNumberNone;
        view_dispatcher_switch_to_view(app->dispatcher, PocketViewMain);
        dndolphins_handle_long_back(app);
        dndolphins_refresh(app);
        return true;
    }
    return false;
}

static uint32_t dndolphins_hash_bytes(uint32_t hash, const void* pointer, size_t length) {
    const uint8_t* bytes = pointer;
    for(size_t i = 0U; i < length; ++i) {
        hash ^= bytes[i];
        hash *= 16777619UL;
    }
    return hash;
}

static uint32_t dndolphins_data_fingerprint(const PocketSaveData* data) {
    uint32_t hash = 2166136261UL;
    const PocketCharacter* character = &data->character;
    const uint8_t* character_bytes = (const uint8_t*)character;

    /* Spells and items are independent sidecar collections. Their counts, capacities,
       pointers, and record contents must never make the core character look dirty just
       because a collection was hydrated or released. */
    hash = dndolphins_hash_bytes(hash, character_bytes, offsetof(PocketCharacter, spell_count));
    /* Spells, features, items, and applied/pending grants are independent lazy
       collections. Hydrating or releasing one must not dirty the core profile. */
    const size_t stable_middle = offsetof(PocketCharacter, language_count);
    const size_t grants_begin = offsetof(PocketCharacter, grant_count);
    hash = dndolphins_hash_bytes(
        hash, character_bytes + stable_middle, grants_begin - stable_middle);
    const size_t stable_tail = offsetof(PocketCharacter, attack_template_count);
    hash = dndolphins_hash_bytes(
        hash, character_bytes + stable_tail, sizeof(PocketCharacter) - stable_tail);
    return hash;
}

static uint32_t dndolphins_spellbook_fingerprint(const PocketCharacter* character) {
    uint32_t hash = 2166136261UL;
    hash = dndolphins_hash_bytes(hash, &character->spell_count, sizeof(character->spell_count));
    if(character->spell_count && character->spells && character->spell_known &&
       character->spell_always_prepared && character->spell_free_casts_current &&
       character->spell_free_casts_max) {
        hash = dndolphins_hash_bytes(
            hash, character->spells, (size_t)character->spell_count * sizeof(PocketSpell));
        hash = dndolphins_hash_bytes(hash, character->spell_known, character->spell_count);
        hash = dndolphins_hash_bytes(hash, character->spell_always_prepared, character->spell_count);
        hash = dndolphins_hash_bytes(hash, character->spell_free_casts_current, character->spell_count);
        hash = dndolphins_hash_bytes(hash, character->spell_free_casts_max, character->spell_count);
    }
    return hash;
}

static uint32_t dndolphins_items_fingerprint(const PocketCharacter* character) {
    uint32_t hash = 2166136261UL;
    hash = dndolphins_hash_bytes(hash, &character->item_count, sizeof(character->item_count));
    if(character->item_count && character->items)
        hash = dndolphins_hash_bytes(
            hash, character->items, (size_t)character->item_count * sizeof(PocketItem));
    return hash;
}

static uint32_t dndolphins_features_fingerprint(const PocketCharacter* character) {
    uint32_t hash = 2166136261UL;
    hash = dndolphins_hash_bytes(hash, &character->feature_count, sizeof(character->feature_count));
    if(character->feature_count && character->features)
        hash = dndolphins_hash_bytes(
            hash, character->features, (size_t)character->feature_count * sizeof(PocketFeature));
    return hash;
}

static bool dndolphins_load_features_page(PocketD20App* app, uint8_t start) {
    if(!app->active_profile_loaded) return false;
    uint8_t total = 0U;
    if(!dndolphins_progression_store_features_load_window(
           app->storage, app->profiles.active_profile, start, &app->data.character, &total)) {
        dndolphins_set_status(app, "Features read failed");
        return false;
    }
    app->features_total = total;
    app->features_cache_start = start;
    app->features_loaded = 1U;
    app->saved_features_fingerprint = dndolphins_features_fingerprint(&app->data.character);
    return true;
}

static bool dndolphins_load_features(PocketD20App* app) {
    if(app->features_loaded) return true;
    return dndolphins_load_features_page(app, 0U);
}

static bool dndolphins_save_features_if_changed(PocketD20App* app) {
    if(!app->features_loaded) return true;
    uint32_t fingerprint = dndolphins_features_fingerprint(&app->data.character);
    if(fingerprint == app->saved_features_fingerprint) return true;
    if(app->storage_read_only) return false;
    if(!dndolphins_progression_store_features_save_window(
           app->storage,
           app->profiles.active_profile,
           app->features_cache_start,
           &app->data.character)) {
        dndolphins_collection_save_failed(app);
        return false;
    }
    app->saved_features_fingerprint = fingerprint;
    return true;
}

static bool dndolphins_feature_cache_ensure(PocketD20App* app, uint8_t logical_index) {
    if(!app->features_loaded && !dndolphins_load_features(app)) return false;
    if(logical_index >= app->features_total) return false;
    if(logical_index >= app->features_cache_start &&
       logical_index < (uint8_t)(app->features_cache_start + app->data.character.feature_count))
        return true;
    if(!dndolphins_save_features_if_changed(app)) return false;
    dnd_data_reserve_features_exact(&app->data.character, 0U);
    app->data.character.feature_count = 0U;
    app->features_loaded = 0U;
    uint8_t start = (uint8_t)((logical_index / DND_PROGRESS_CACHE_SIZE) * DND_PROGRESS_CACHE_SIZE);
    return dndolphins_load_features_page(app, start);
}

static uint8_t dndolphins_feature_local_index(const PocketD20App* app, uint8_t logical_index) {
    return (uint8_t)(logical_index - app->features_cache_start);
}

static PocketFeature* dndolphins_feature_at(
    PocketD20App* app, uint8_t logical_index, uint8_t* local_out) {
    if(!dndolphins_feature_cache_ensure(app, logical_index)) return NULL;
    uint8_t local = dndolphins_feature_local_index(app, logical_index);
    if(local >= app->data.character.feature_count) return NULL;
    if(local_out) *local_out = local;
    return &app->data.character.features[local];
}

static PocketFeature* dndolphins_feature_at_cached(
    PocketD20App* app, uint8_t logical_index, uint8_t* local_out) {
    if(!app->features_loaded || logical_index < app->features_cache_start) return NULL;
    uint8_t local = (uint8_t)(logical_index - app->features_cache_start);
    if(local >= app->data.character.feature_count) return NULL;
    if(local_out) *local_out = local;
    return &app->data.character.features[local];
}

static bool dndolphins_release_features(PocketD20App* app) {
    if(!app->features_loaded) return true;
    bool saved = dndolphins_save_features_if_changed(app);
    if(saved) {
        dnd_data_reserve_features_exact(&app->data.character, 0U);
        app->data.character.feature_count = 0U;
        app->features_loaded = 0U;
        app->features_cache_start = 0U;
    }
    return saved;
}

static bool dndolphins_load_spellbook_page(PocketD20App* app, uint8_t start) {
    if(!app->active_profile_loaded) return false;
    uint8_t total = 0U;
    if(!dnd_storage_load_spellbook_window(
           app->storage, app->profiles.active_profile, start, &app->data.character, &total)) {
        dndolphins_set_status(app, "Spellbook read failed");
        return false;
    }
    app->spellbook_total = total;
    app->spellbook_cache_start = start;
    app->spellbook_loaded = 1U;
    app->saved_spellbook_fingerprint = dndolphins_spellbook_fingerprint(&app->data.character);
    return true;
}

static bool dndolphins_load_items_page(PocketD20App* app, uint8_t start) {
    if(!app->active_profile_loaded) return false;
    uint8_t total = 0U;
    if(!dnd_storage_load_items_window(
           app->storage, app->profiles.active_profile, start, &app->data.character, &total)) {
        dndolphins_set_status(app, "Items read failed");
        return false;
    }
    app->items_total = total;
    app->items_cache_start = start;
    app->items_loaded = 1U;
    app->saved_items_fingerprint = dndolphins_items_fingerprint(&app->data.character);
    return true;
}

static bool dndolphins_load_spellbook(PocketD20App* app) {
    if(app->spellbook_loaded) return true;
    return dndolphins_load_spellbook_page(app, 0U);
}

static bool dndolphins_load_items(PocketD20App* app) {
    if(app->items_loaded) return true;
    return dndolphins_load_items_page(app, 0U);
}

static void dndolphins_collection_save_failed(PocketD20App* app) {
    /* A collection write failure is retryable and must not poison core profile
       storage state. The resident collection fingerprint remains dirty, which is
       sufficient for the next autosave/close to retry only that collection. */
    if(app->storage_failure_count < UINT16_MAX) ++app->storage_failure_count;
    dndolphins_set_status(app, "UNSAVED - retry SD");
}

static bool dndolphins_save_spellbook_if_changed(PocketD20App* app) {
    if(!app->spellbook_loaded) return true;
    uint32_t fingerprint = dndolphins_spellbook_fingerprint(&app->data.character);
    if(fingerprint == app->saved_spellbook_fingerprint) return true;
    if(app->storage_read_only) return false;
    if(!dnd_storage_save_spellbook_window(
           app->storage,
           app->profiles.active_profile,
           app->spellbook_cache_start,
           &app->data.character)) {
        dndolphins_collection_save_failed(app);
        return false;
    }
    app->saved_spellbook_fingerprint = fingerprint;
    app->spell_class_counts_valid = 0U;
    return true;
}

static bool dndolphins_save_items_if_changed(PocketD20App* app) {
    if(!app->items_loaded) return true;
    uint32_t fingerprint = dndolphins_items_fingerprint(&app->data.character);
    if(fingerprint == app->saved_items_fingerprint) return true;
    if(app->storage_read_only) return false;
    if(!dnd_storage_save_items_window(
           app->storage,
           app->profiles.active_profile,
           app->items_cache_start,
           &app->data.character)) {
        dndolphins_collection_save_failed(app);
        return false;
    }
    app->saved_items_fingerprint = fingerprint;
    return true;
}

static bool dndolphins_spell_cache_ensure(PocketD20App* app, uint8_t logical_index) {
    if(!app->spellbook_loaded && !dndolphins_load_spellbook(app)) return false;
    if(logical_index >= app->spellbook_total) return false;
    if(logical_index >= app->spellbook_cache_start &&
       logical_index < (uint8_t)(app->spellbook_cache_start + app->data.character.spell_count))
        return true;
    if(!dndolphins_save_spellbook_if_changed(app)) return false;
    dnd_data_clear_spells(&app->data.character);
    app->spellbook_loaded = 0U;
    uint8_t start = (uint8_t)((logical_index / POCKET_D20_COLLECTION_CACHE_SIZE) *
                              POCKET_D20_COLLECTION_CACHE_SIZE);
    return dndolphins_load_spellbook_page(app, start);
}

static bool dndolphins_item_cache_ensure(PocketD20App* app, uint8_t logical_index) {
    if(!app->items_loaded && !dndolphins_load_items(app)) return false;
    if(logical_index >= app->items_total) return false;
    if(logical_index >= app->items_cache_start &&
       logical_index < (uint8_t)(app->items_cache_start + app->data.character.item_count))
        return true;
    if(!dndolphins_save_items_if_changed(app)) return false;
    dnd_data_clear_items(&app->data.character);
    app->items_loaded = 0U;
    uint8_t start = (uint8_t)((logical_index / POCKET_D20_COLLECTION_CACHE_SIZE) *
                              POCKET_D20_COLLECTION_CACHE_SIZE);
    return dndolphins_load_items_page(app, start);
}

static uint8_t dndolphins_spell_local_index(const PocketD20App* app, uint8_t logical_index) {
    return (uint8_t)(logical_index - app->spellbook_cache_start);
}

static uint8_t dndolphins_item_local_index(const PocketD20App* app, uint8_t logical_index) {
    return (uint8_t)(logical_index - app->items_cache_start);
}

static PocketSpell* dndolphins_spell_at(PocketD20App* app, uint8_t logical_index, uint8_t* local_out) {
    if(!dndolphins_spell_cache_ensure(app, logical_index)) return NULL;
    uint8_t local = dndolphins_spell_local_index(app, logical_index);
    if(local >= app->data.character.spell_count) return NULL;
    if(local_out) *local_out = local;
    return &app->data.character.spells[local];
}

static PocketItem* dndolphins_item_at(PocketD20App* app, uint8_t logical_index, uint8_t* local_out) {
    if(!dndolphins_item_cache_ensure(app, logical_index)) return NULL;
    uint8_t local = dndolphins_item_local_index(app, logical_index);
    if(local >= app->data.character.item_count) return NULL;
    if(local_out) *local_out = local;
    return &app->data.character.items[local];
}

static PocketSpell* dndolphins_spell_cached_at(
    PocketD20App* app, uint8_t logical_index, uint8_t* local_out) {
    if(!app || !app->spellbook_loaded || logical_index < app->spellbook_cache_start ||
       logical_index >= (uint8_t)(app->spellbook_cache_start + app->data.character.spell_count))
        return NULL;
    uint8_t local = dndolphins_spell_local_index(app, logical_index);
    if(local >= app->data.character.spell_count) return NULL;
    if(local_out) *local_out = local;
    return &app->data.character.spells[local];
}

static PocketItem* dndolphins_item_cached_at(
    PocketD20App* app, uint8_t logical_index, uint8_t* local_out) {
    if(!app || !app->items_loaded || logical_index < app->items_cache_start ||
       logical_index >= (uint8_t)(app->items_cache_start + app->data.character.item_count))
        return NULL;
    uint8_t local = dndolphins_item_local_index(app, logical_index);
    if(local >= app->data.character.item_count) return NULL;
    if(local_out) *local_out = local;
    return &app->data.character.items[local];
}



static void dndolphins_record_list_scroll_sidecar(PocketD20App* app, uint8_t total) {
    if(app->selection == 0U || total == 0U) {
        app->scroll = 0U;
        return;
    }

    uint8_t logical = (uint8_t)(app->selection - 1U);
    uint8_t page_start = (uint8_t)(
        (logical / POCKET_D20_COLLECTION_CACHE_SIZE) * POCKET_D20_COLLECTION_CACHE_SIZE);
    uint16_t first = (uint16_t)page_start + 1U;
    uint8_t page_records = (uint8_t)(total - page_start);
    if(page_records > POCKET_D20_COLLECTION_CACHE_SIZE)
        page_records = POCKET_D20_COLLECTION_CACHE_SIZE;
    uint16_t last = first + page_records - 1U;

    /* Keep a five-row viewport entirely inside one resident collection page.
       Page zero may also include the + Add New row. This prevents drawing from
       crossing a page boundary and triggering storage I/O while the GUI owns
       the canvas callback. */
    if(page_start == 0U && app->selection <= 4U) {
        app->scroll = 0U;
        return;
    }
    if(page_records <= 5U) {
        app->scroll = first;
        return;
    }

    uint16_t scroll = app->selection > first + 3U ? app->selection - 4U : first;
    uint16_t maximum = last - 4U;
    if(scroll > maximum) scroll = maximum;
    if(scroll < first) scroll = first;
    app->scroll = scroll;
}

static bool dndolphins_record_list_prepare_sidecar(PocketD20App* app) {
    if(app->list_kind == PocketListFeatures) {
        if(app->features_total) {
            uint8_t logical = app->selection ? (uint8_t)(app->selection - 1U) : 0U;
            if(logical >= app->features_total) logical = (uint8_t)(app->features_total - 1U);
            if(!dndolphins_feature_cache_ensure(app, logical)) return false;
        }
        dndolphins_record_list_scroll_sidecar(app, app->features_total);
    }
    return true;
}

static void dndolphins_record_list_focus(PocketD20App* app, uint8_t logical_index) {
    uint8_t total = app->list_kind == PocketListFeatures ? app->features_total : 0U;
    if(!total) {
        app->selection = 0U;
        app->scroll = 0U;
        return;
    }
    if(logical_index >= total) logical_index = (uint8_t)(total - 1U);
    app->selection = (uint16_t)logical_index + 1U;
    if(!dndolphins_record_list_prepare_sidecar(app)) dndolphins_set_status(app, "Collection read failed");
}

static bool dndolphins_spell_class_counts_cached(PocketD20App* app) {
    if(!app->spell_class_counts_valid) {
        uint8_t total = 0U;
        if(!dndolphins_spells_class_counts(
               app->storage,
               app->profiles.active_profile,
               &app->spell_class_counts,
               &total))
            return false;
        app->spellbook_total = total;
        app->spell_class_counts_valid = 1U;
    }
    return true;
}

static uint8_t dndolphins_class_prepared_count_cached(PocketD20App* app, uint8_t class_index) {
    return app->spell_class_counts_valid && class_index < POCKET_D20_MAX_CLASSES ?
               app->spell_class_counts.prepared[class_index] :
               0U;
}

static uint8_t dndolphins_class_known_count_cached(PocketD20App* app, uint8_t class_index) {
    return app->spell_class_counts_valid && class_index < POCKET_D20_MAX_CLASSES ?
               app->spell_class_counts.known[class_index] :
               0U;
}


static bool dndolphins_release_spellbook(PocketD20App* app) {
    if(!app->spellbook_loaded) return true;
    bool saved = dndolphins_save_spellbook_if_changed(app);
    if(saved) {
        dnd_data_clear_spells(&app->data.character);
        app->spellbook_loaded = 0U;
        app->spellbook_cache_start = 0U;
    }
    return saved;
}

static bool dndolphins_release_items(PocketD20App* app) {
    if(!app->items_loaded) return true;
    bool saved = dndolphins_save_items_if_changed(app);
    if(saved) {
        dnd_data_clear_items(&app->data.character);
        app->items_loaded = 0U;
        app->items_cache_start = 0U;
    }
    return saved;
}

static bool dndolphins_save_now(PocketD20App* app, bool report) {
    if(!app->active_profile_loaded) {
        if(report) dndolphins_set_status(app, "Profile not loaded");
        return false;
    }
    if(app->storage_read_only) {
        app->storage_unsaved = 1U;
        dndolphins_set_status(app, "UNSAVED - retry SD");
        return false;
    }
    dnd_data_sanitize(&app->data);
    uint32_t fingerprint = dndolphins_data_fingerprint(&app->data);
    uint32_t spellbook_fingerprint = app->spellbook_loaded ?
                                         dndolphins_spellbook_fingerprint(&app->data.character) :
                                         app->saved_spellbook_fingerprint;
    uint32_t items_fingerprint = app->items_loaded ?
                                     dndolphins_items_fingerprint(&app->data.character) :
                                     app->saved_items_fingerprint;
    uint32_t features_fingerprint = app->features_loaded ?
                                        dndolphins_features_fingerprint(&app->data.character) :
                                        app->saved_features_fingerprint;
    bool main_changed = app->storage_unsaved || fingerprint != app->saved_fingerprint;
    bool spellbook_changed = app->spellbook_loaded &&
                             spellbook_fingerprint != app->saved_spellbook_fingerprint;
    bool items_changed = app->items_loaded && items_fingerprint != app->saved_items_fingerprint;
    bool features_changed =
        app->features_loaded && features_fingerprint != app->saved_features_fingerprint;
    if(!main_changed && !spellbook_changed && !items_changed && !features_changed) {
        if(report) dndolphins_set_status(app, "Already saved");
        return true;
    }

    bool result = true;
    bool main_write_failed = false;
    if(main_changed) {
        bool active_found = app->profiles.active_entry_valid &&
                            app->profiles.active_entry.id == app->profiles.active_profile;
        result = active_found ?
                     dnd_storage_save_profile_known_updated(
                         app->storage, &app->profiles.active_entry, &app->data) :
                     dnd_storage_save_profile_updated(
                         app->storage, app->profiles.active_profile, &app->data);
        main_write_failed = !result;
        if(result) {
            app->profiles.active_entry.id = app->profiles.active_profile;
            app->profiles.active_entry.level = dnd_rules_core_total_level(&app->data.character);
            dndolphins_copy(
                app->profiles.active_entry.name,
                sizeof(app->profiles.active_entry.name),
                app->data.character.name);
            app->profiles.active_entry_valid = 1U;
            for(uint8_t i = 0U; i < app->profiles.cache_count; ++i) {
                if(app->profiles.entries[i].id != app->profiles.active_profile) continue;
                app->profiles.entries[i].level = dnd_rules_core_total_level(&app->data.character);
                dndolphins_copy(
                    app->profiles.entries[i].name,
                    sizeof(app->profiles.entries[i].name),
                    app->data.character.name);
                break;
            }
            app->saved_fingerprint = fingerprint;
            app->storage_unsaved = 0U;
        }
    }
    if(result && spellbook_changed) {
        result = dnd_storage_save_spellbook_window(
            app->storage,
            app->profiles.active_profile,
            app->spellbook_cache_start,
            &app->data.character);
        if(result) app->saved_spellbook_fingerprint = spellbook_fingerprint;
    }
    if(result && items_changed) {
        result = dnd_storage_save_items_window(
            app->storage,
            app->profiles.active_profile,
            app->items_cache_start,
            &app->data.character);
        if(result) app->saved_items_fingerprint = items_fingerprint;
    }
    if(result && features_changed) {
        result = dndolphins_progression_store_features_save_window(
            app->storage,
            app->profiles.active_profile,
            app->features_cache_start,
            &app->data.character);
        if(result) app->saved_features_fingerprint = features_fingerprint;
    }

    if(result) {
        app->storage_unsaved = 0U;
    } else {
        /* Only a failed core character write enters read-only protection.
           Spellbook/Inventory failures keep their dirty resident fingerprint and
           remain retryable on the next save, add/delete attempt, or app close. */
        if(main_write_failed) {
            app->storage_read_only = 1U;
            app->storage_unsaved = 1U;
        }
        if(app->storage_failure_count < UINT16_MAX) ++app->storage_failure_count;
    }
    if(report || !result) dndolphins_set_status(app, result ? "Saved" : "UNSAVED - SD unavailable");
    return result;
}

static bool dndolphins_flush_save(PocketD20App* app, bool report) {
    if(app->autosave_timer) furi_timer_stop(app->autosave_timer);
    app->autosave_pending = 0U;
    return dndolphins_save_now(app, report);
}

static bool dndolphins_save(PocketD20App* app, bool report) {
    if(report) return dndolphins_flush_save(app, true);
    if(!app->active_profile_loaded) return false;
    if(app->storage_read_only) {
        app->storage_unsaved = 1U;
        dndolphins_set_status(app, "UNSAVED - retry SD");
        return false;
    }
    dnd_data_sanitize(&app->data);
    uint32_t fingerprint = dndolphins_data_fingerprint(&app->data);
    bool spellbook_changed = app->spellbook_loaded &&
                             dndolphins_spellbook_fingerprint(&app->data.character) !=
                                 app->saved_spellbook_fingerprint;
    bool items_changed = app->items_loaded &&
                         dndolphins_items_fingerprint(&app->data.character) !=
                             app->saved_items_fingerprint;
    bool features_changed = app->features_loaded &&
                            dndolphins_features_fingerprint(&app->data.character) !=
                                app->saved_features_fingerprint;
    if(!app->storage_unsaved && fingerprint == app->saved_fingerprint &&
       !spellbook_changed && !items_changed && !features_changed) {
        if(app->autosave_timer) furi_timer_stop(app->autosave_timer);
        app->autosave_pending = 0U;
        return true;
    }
    if(!app->autosave_timer) return dndolphins_save_now(app, false);
    app->autosave_pending = 1U;
    furi_timer_stop(app->autosave_timer);
    if(furi_timer_start(app->autosave_timer, furi_ms_to_ticks(POCKET_D20_AUTOSAVE_MS)) !=
       FuriStatusOk) {
        app->autosave_pending = 0U;
        return dndolphins_save_now(app, false);
    }
    return true;
}

static uint16_t dndolphins_profile_count(const PocketD20App* app) {
    return app->profiles.count;
}

static const PocketProfileEntry* dndolphins_profile_entry_at(PocketD20App* app, uint16_t list_index) {
    return dnd_storage_profiles_entry_at(app->storage, &app->profiles, list_index);
}

static const PocketProfileEntry* dndolphins_profile_entry_cached_at(
    const PocketD20App* app, uint16_t list_index) {
    if(!app || list_index >= app->profiles.count || !app->profiles.cache_count ||
       list_index < app->profiles.cache_start ||
       list_index >= (uint16_t)(app->profiles.cache_start + app->profiles.cache_count))
        return NULL;
    return &app->profiles.entries[list_index - app->profiles.cache_start];
}

static uint32_t dndolphins_profile_id_at(PocketD20App* app, uint16_t list_index) {
    const PocketProfileEntry* entry = dndolphins_profile_entry_at(app, list_index);
    return entry ? entry->id : UINT32_MAX;
}

static bool dndolphins_profile_exists(PocketD20App* app, uint32_t profile) {
    return dnd_storage_profiles_find(app->storage, profile, NULL);
}

static bool dndolphins_profile_include_active(PocketD20App* app) {
    if(app->profiles.active_entry_valid &&
       app->profiles.active_entry.id == app->profiles.active_profile)
        return true;
    PocketProfileEntry entry;
    if(!dnd_storage_profiles_find(app->storage, app->profiles.active_profile, &entry)) return false;
    app->profiles.active_entry = entry;
    app->profiles.active_entry_valid = 1U;
    return true;
}

static bool dndolphins_screen_uses_spellbook(const PocketD20App* app, PocketScreen screen) {
    UNUSED(app);
    return screen == PocketScreenSpellAttacks || screen == PocketScreenRituals ||
           screen == PocketScreenSpellCast || screen == PocketScreenSpellResult;
}

static bool dndolphins_screen_uses_items(const PocketD20App* app, PocketScreen screen) {
    UNUSED(app);
    /* Only Weapon Combat may hydrate an Item page in the main FAP. */
    return screen == PocketScreenAttackList || screen == PocketScreenAttackResult;
}

static bool dndolphins_screen_uses_features(const PocketD20App* app, PocketScreen screen) {
    if((screen == PocketScreenRecordList || screen == PocketScreenRecordDetail) &&
       app->list_kind == PocketListFeatures)
        return true;
    if(screen == PocketScreenCatalog && app->catalog_kind == PocketCatalogFeats &&
       app->list_kind == PocketListFeatures && app->level_choice_mode != 3U)
        return true;
    return false;
}

static void dndolphins_enter_screen(PocketD20App* app, PocketScreen screen) {
    PocketScreen previous = app->screen;
    if(previous == PocketScreenHome && screen != PocketScreenHome)
        app->home_return_selection = app->selection;
    /* Reclaim screen-local working memory before a pending save allocates file objects/buffers. */
    if(previous == PocketScreenCatalog && screen != PocketScreenCatalog) dndolphins_catalog_release(app);
    if(previous != screen && app->autosave_pending) dndolphins_flush_save(app, false);

    bool needs_spellbook = dndolphins_screen_uses_spellbook(app, screen);
    bool needs_items = dndolphins_screen_uses_items(app, screen);
    bool needs_features = dndolphins_screen_uses_features(app, screen);
    if(screen == PocketScreenRecordDetail && app->list_kind == PocketListClasses)
        app->spell_class_counts_valid = 0U;
    bool collection_failed = false;
    if(screen == PocketScreenRecordDetail && app->list_kind == PocketListClasses &&
       !dndolphins_spell_class_counts_cached(app))
        collection_failed = true;
    if(!needs_spellbook && app->spellbook_loaded && !dndolphins_release_spellbook(app))
        collection_failed = true;
    if(!needs_items && app->items_loaded && !dndolphins_release_items(app))
        collection_failed = true;
    if(!needs_spellbook && app->combat_spell_indices) {
        free(app->combat_spell_indices);
        app->combat_spell_indices = NULL;
        app->combat_spell_count = 0U;
    }
    if(!needs_items && app->combat_weapon_indices) {
        free(app->combat_weapon_indices);
        app->combat_weapon_indices = NULL;
        app->combat_weapon_count = 0U;
    }
    if(!needs_features && app->features_loaded && !dndolphins_release_features(app))
        collection_failed = true;
    /* Item/Spell pages are true combat-lazy state. Entering a collection-backed
       combat list builds its bounded logical index, then hydrates only the five
       visible row labels before drawing. Canvas callbacks stay RAM-only. */
    if(needs_features && !app->features_loaded && !dndolphins_load_features(app))
        collection_failed = true;
    if(screen == PocketScreenSpellAttacks && !dndolphins_refresh_combat_spell_index(app))
        collection_failed = true;
    if(screen == PocketScreenRituals && !dndolphins_refresh_ritual_spell_index(app))
        collection_failed = true;
    if(screen == PocketScreenAttackList && !dndolphins_refresh_combat_weapon_index(app))
        collection_failed = true;

    app->screen = screen;
    app->selection = 0U;
    app->scroll = 0U;
    if(screen == PocketScreenHome && previous != PocketScreenHome) {
        app->selection = app->home_return_selection;
        app->scroll = app->selection >= 5U ? (uint16_t)(app->selection - 4U) : 0U;
    }
    if(screen == PocketScreenSpellAttacks)
        dndolphins_prepare_combat_spell_rows(app, false);
    else if(screen == PocketScreenRituals)
        dndolphins_prepare_combat_spell_rows(app, true);
    else if(screen == PocketScreenAttackList)
        dndolphins_prepare_combat_weapon_rows(app);
    if(screen == PocketScreenProfiles) {
        uint16_t count = dndolphins_profile_count(app);
        if(count)
            (void)dnd_storage_profiles_window(app->storage, &app->profiles, 0U);
    }
    dndolphins_clear_action_ack(app);
    app->edit_modifier_mode = 0U;
    dndolphins_marquee_offset = 0U;
    if(app->storage_unsaved)
        dndolphins_set_status(app, "UNSAVED - retry SD");
    else if(!collection_failed)
        dndolphins_clear_status(app);
}

static void dndolphins_release_pending_grants(PocketD20App* app) {
    if(!app) return;
    dnd_data_reserve_grants_exact(&app->data.character, 0U);
    app->data.character.grant_count = 0U;
}

static void dndolphins_switch_profile(PocketD20App* app, uint32_t profile) {
    if(!dndolphins_profile_exists(app, profile)) return;
    if(profile == app->profiles.active_profile) {
        dndolphins_set_status(app, "Already active");
        return;
    }
    if(app->active_profile_loaded && !dndolphins_flush_save(app, false)) {
        dndolphins_set_status(app, "Save failed");
        return;
    }
    dnd_data_clear_spells(&app->data.character);
    dnd_data_clear_items(&app->data.character);
    dnd_data_reserve_features_exact(&app->data.character, 0U);
    app->data.character.feature_count = 0U;
    dndolphins_release_pending_grants(app);
    app->spellbook_loaded = 0U;
    app->items_loaded = 0U;
    app->features_loaded = 0U;
    app->spellbook_total = 0U;
    app->items_total = 0U;
    app->features_total = 0U;
    app->spellbook_cache_start = 0U;
    app->items_cache_start = 0U;
    app->features_cache_start = 0U;
    app->spell_class_counts_valid = 0U;
    uint32_t previous_profile = app->profiles.active_profile;
    app->profiles.active_profile = profile;
    app->arcane_recovery_active = 0U;
    bool recovered_backup = false;
    bool loaded =
        dnd_storage_load_profile(app->storage, profile, &app->data, &recovered_backup);
    app->active_profile_loaded = loaded ? 1U : 0U;
    bool character_ready = loaded;
    if(loaded && recovered_backup)
        character_ready = dnd_storage_restore_backup(app->storage, profile, &app->data);
    if(!loaded) {
        /* Loading a target profile resets the parse buffer to defaults on failure.
           Immediately restore the previously flushed character so transient or
           damaged profiles can never expose/save a synthetic New Hero over it. */
        bool previous_recovered = false;
        app->profiles.active_profile = previous_profile;
        app->active_profile_loaded = dnd_storage_load_profile(
                                         app->storage,
                                         previous_profile,
                                         &app->data,
                                         &previous_recovered) ?
                                         1U :
                                         0U;
        if(app->active_profile_loaded && previous_recovered)
            dnd_storage_restore_backup(app->storage, previous_profile, &app->data);
        app->storage_unsaved = 0U;
        app->saved_fingerprint = dndolphins_data_fingerprint(&app->data);
        dndolphins_enter_screen(app, PocketScreenHome);
        dndolphins_set_status(app, "Profile preserved - load failed");
        return;
    }
    bool metadata_saved = dnd_storage_profiles_refresh(app->storage, &app->profiles);
    dndolphins_profile_include_active(app);
    metadata_saved = metadata_saved && dnd_storage_profiles_save(app->storage, &app->profiles);
    if(character_ready && metadata_saved)
        app->saved_fingerprint = dndolphins_data_fingerprint(&app->data);
    dndolphins_enter_screen(app, PocketScreenHome);
    if(!character_ready || !metadata_saved)
        dndolphins_set_status(app, "Profile save failed");
    else if(recovered_backup)
        dndolphins_set_status(app, "Backup recovered");
    else if(loaded)
        dndolphins_set_status(app, "Character switched");
    else
        dndolphins_set_status(app, "Fresh character");
}

static void dndolphins_create_profile(PocketD20App* app) {
    if(app->active_profile_loaded && !dndolphins_flush_save(app, false)) {
        dndolphins_set_status(app, "Save failed");
        return;
    }
    dnd_data_clear_spells(&app->data.character);
    dnd_data_clear_items(&app->data.character);
    dnd_data_reserve_features_exact(&app->data.character, 0U);
    app->data.character.feature_count = 0U;
    dndolphins_release_pending_grants(app);
    app->spellbook_loaded = 0U;
    app->items_loaded = 0U;
    app->features_loaded = 0U;
    app->spellbook_total = 0U;
    app->items_total = 0U;
    app->features_total = 0U;
    app->spellbook_cache_start = 0U;
    app->items_cache_start = 0U;
    app->features_cache_start = 0U;
    app->spell_class_counts_valid = 0U;
    uint32_t profile = dnd_storage_profiles_next_id(&app->profiles);
    if(profile == UINT32_MAX &&
       ((app->profiles.reserved_id_seen && app->profiles.highest_reserved_id == UINT32_MAX) ||
        dndolphins_profile_exists(app, UINT32_MAX))) {
        dndolphins_set_status(app, "Profile IDs exhausted");
        return;
    }
    uint32_t previous_profile = app->profiles.active_profile;
    app->arcane_recovery_active = 0U;
    dnd_data_clear(&app->data);
    dnd_data_set_defaults(&app->data);
    snprintf(
        app->data.character.name,
        sizeof(app->data.character.name),
        "New Hero %lu",
        (unsigned long)(profile + 1U));
    bool character_saved = dnd_storage_save_profile(app->storage, profile, &app->data);
    if(!character_saved) {
        dnd_storage_delete_profile(app->storage, profile);
        bool recovered = false;
        app->profiles.active_profile = previous_profile;
        app->active_profile_loaded = dnd_storage_load_profile(
            app->storage, previous_profile, &app->data, &recovered);
        dnd_storage_profiles_refresh(app->storage, &app->profiles);
        dndolphins_profile_include_active(app);
        app->saved_fingerprint = dndolphins_data_fingerprint(&app->data);
        app->storage_read_only = 1U;
        app->storage_unsaved = 1U;
        dndolphins_enter_screen(app, PocketScreenProfiles);
        dndolphins_set_status(app, "New character save failed");
        return;
    }
    app->active_profile_loaded = 1U;
    app->profiles.active_profile = profile;
    dnd_data_clear_spells(&app->data.character);
    dnd_data_clear_items(&app->data.character);
    dnd_data_reserve_features_exact(&app->data.character, 0U);
    app->data.character.feature_count = 0U;
    dndolphins_release_pending_grants(app);
    app->spellbook_loaded = 0U;
    app->items_loaded = 0U;
    app->features_loaded = 0U;
    app->spellbook_total = 0U;
    app->items_total = 0U;
    app->features_total = 0U;
    app->spellbook_cache_start = 0U;
    app->items_cache_start = 0U;
    app->features_cache_start = 0U;
    app->spell_class_counts_valid = 0U;
    bool metadata_saved = dnd_storage_profiles_refresh(app->storage, &app->profiles);
    dndolphins_profile_include_active(app);
    metadata_saved = metadata_saved && dnd_storage_profiles_save(app->storage, &app->profiles);
    if(character_saved && metadata_saved)
        app->saved_fingerprint = dndolphins_data_fingerprint(&app->data);
    dndolphins_enter_screen(app, PocketScreenCharacter);
    dndolphins_set_status(app, character_saved && metadata_saved ? "New character" : "Save failed");
}

static bool dndolphins_delete_profile(PocketD20App* app, uint32_t profile) {
    if(!app || !dndolphins_profile_exists(app, profile)) return false;

    const bool deleting_active = profile == app->profiles.active_profile;
    PocketProfileEntry replacement;
    const bool have_replacement =
        deleting_active && dnd_storage_profiles_next_after(app->storage, profile, &replacement) &&
        replacement.id != profile;

    /* An active profile may be deleted too. Stop any pending autosave before the
       primary file disappears so a delayed callback can never recreate it. */
    if(deleting_active) {
        if(app->autosave_timer) furi_timer_stop(app->autosave_timer);
        app->autosave_pending = 0U;
        dnd_data_clear_spells(&app->data.character);
        dnd_data_clear_items(&app->data.character);
        dnd_data_reserve_features_exact(&app->data.character, 0U);
        app->data.character.feature_count = 0U;
        dndolphins_release_pending_grants(app);
        app->spellbook_loaded = 0U;
        app->items_loaded = 0U;
        app->features_loaded = 0U;
        app->spellbook_total = 0U;
        app->items_total = 0U;
        app->features_total = 0U;
        app->spellbook_cache_start = 0U;
        app->items_cache_start = 0U;
        app->features_cache_start = 0U;
        app->spell_class_counts_valid = 0U;
        app->active_profile_loaded = 0U;
    }

    if(!dnd_storage_delete_profile(app->storage, profile)) return false;
    if(!dnd_storage_profiles_refresh(app->storage, &app->profiles)) return false;

    if(deleting_active && have_replacement &&
       dnd_storage_profiles_find(app->storage, replacement.id, NULL)) {
        app->profiles.active_profile = replacement.id;
        bool recovered_backup = false;
        bool loaded = dnd_storage_load_profile(
            app->storage, replacement.id, &app->data, &recovered_backup);
        app->active_profile_loaded = loaded ? 1U : 0U;
        if(loaded && recovered_backup)
            loaded = dnd_storage_restore_backup(app->storage, replacement.id, &app->data);
        if(!loaded) {
            app->active_profile_loaded = 0U;
            return false;
        }
        app->saved_fingerprint = dndolphins_data_fingerprint(&app->data);
        dndolphins_profile_include_active(app);
    } else if(deleting_active) {
        /* The Characters screen is valid with zero profiles. Keep a harmless RAM
           default for rendering, but do not persist it; + New Character remains
           the only character row until the user explicitly creates one. */
        app->profiles.active_profile = 0U;
        dnd_data_clear(&app->data);
        dnd_data_set_defaults(&app->data);
        app->active_profile_loaded = 0U;
        app->saved_fingerprint = dndolphins_data_fingerprint(&app->data);
    }

    if(!dnd_storage_profiles_save(app->storage, &app->profiles)) return false;
    app->storage_read_only = 0U;
    app->storage_unsaved = 0U;
    app->selection = 0U;
    app->scroll = 0U;
    return true;
}

static uint8_t dndolphins_wizard_level(const PocketCharacter* character) {
    for(uint8_t i = 0U; i < character->class_count; ++i)
        if(strcmp(character->classes[i].name, "Wizard") == 0) return character->classes[i].level;
    return 0U;
}

static bool dndolphins_begin_arcane_recovery(PocketD20App* app) {
    PocketCharacter* character = &app->data.character;
    uint8_t wizard_level = dndolphins_wizard_level(character);
    if(!wizard_level) {
        dndolphins_set_status(app, "No Wizard class found");
        return false;
    }
    if(character->arcane_recovery_used) {
        dndolphins_set_status(app, "Recovery already used");
        return false;
    }
    bool has_expended_slot = false;
    for(uint8_t level = 1U; level <= 5U; ++level)
        if(character->spell_slots_current[level] < character->spell_slots_max[level])
            has_expended_slot = true;
    if(!has_expended_slot) {
        dndolphins_set_status(app, "No eligible slots spent");
        return false;
    }
    app->arcane_recovery_active = 1U;
    app->arcane_recovery_budget = (wizard_level + 1U) / 2U;
    app->arcane_recovery_spent = 0U;
    memset(app->arcane_recovery_restored, 0, sizeof(app->arcane_recovery_restored));
    dndolphins_enter_screen(app, PocketScreenMagic);
    app->selection = 6U;
    app->scroll = 2U;
    dndolphins_set_status(app, "Choose slots, OK done");
    return true;
}

static void dndolphins_menu_move(PocketD20App* app, uint16_t count, int8_t delta) {
    if(count == 0U) return;
    int32_t next = (int32_t)app->selection + delta;
    if(next < 0) next = count - 1U;
    if(next >= count) next = 0;
    app->selection = (uint16_t)next;
    dndolphins_marquee_offset = 0U;
    if(app->selection < app->scroll) app->scroll = app->selection;
    if(app->selection >= app->scroll + 5U) app->scroll = app->selection - 4U;
}

static const char* dndolphins_proficiency_mark(uint8_t proficiency) {
    if(proficiency == PocketProficiencyExpertise) return "E";
    if(proficiency == PocketProficiencyProficient) return "P";
    return "-";
}


static uint8_t dndolphins_cycle_die(uint8_t current, int8_t delta, bool damage_only) {
    const uint8_t* choices = damage_only ? dndolphins_damage_die_choices : dndolphins_die_choices;
    uint8_t count = damage_only ? sizeof(dndolphins_damage_die_choices) : sizeof(dndolphins_die_choices);
    uint8_t index = 0U;
    for(uint8_t i = 0U; i < count; ++i) {
        if(choices[i] == current) {
            index = i;
            break;
        }
    }
    int16_t next = (int16_t)index + delta;
    if(next < 0) next = count - 1U;
    if(next >= count) next = 0;
    return choices[next];
}

static uint16_t dndolphins_class_mask_from_name(const char* name) {
    if(strcmp(name, "Artificer") == 0) return PocketClassMaskArtificer;
    if(strcmp(name, "Barbarian") == 0) return PocketClassMaskBarbarian;
    if(strcmp(name, "Bard") == 0) return PocketClassMaskBard;
    if(strcmp(name, "Cleric") == 0) return PocketClassMaskCleric;
    if(strcmp(name, "Druid") == 0) return PocketClassMaskDruid;
    if(strcmp(name, "Fighter") == 0) return PocketClassMaskFighter;
    if(strcmp(name, "Monk") == 0) return PocketClassMaskMonk;
    if(strcmp(name, "Paladin") == 0) return PocketClassMaskPaladin;
    if(strcmp(name, "Ranger") == 0) return PocketClassMaskRanger;
    if(strcmp(name, "Rogue") == 0) return PocketClassMaskRogue;
    if(strcmp(name, "Sorcerer") == 0) return PocketClassMaskSorcerer;
    if(strcmp(name, "Warlock") == 0) return PocketClassMaskWarlock;
    if(strcmp(name, "Wizard") == 0) return PocketClassMaskWizard;
    return 0U;
}

static void dndolphins_configure_class_defaults(PocketClassLevel* level) {
    uint16_t mask = dndolphins_class_mask_from_name(level->name);
    if(mask & (PocketClassMaskBarbarian))
        level->hit_die = 12U;
    else if(mask & (PocketClassMaskFighter | PocketClassMaskPaladin | PocketClassMaskRanger))
        level->hit_die = 10U;
    else if(
        mask &
        (PocketClassMaskBard | PocketClassMaskCleric | PocketClassMaskDruid | PocketClassMaskMonk |
         PocketClassMaskRogue | PocketClassMaskWarlock | PocketClassMaskArtificer))
        level->hit_die = 8U;
    else
        level->hit_die = 6U;
    /* Keep class/subclass spellcasting classification in one place so selecting
       Eldritch Knight or Arcane Trickster cannot drift from multiclass math. */
    (void)dndolphins_spells_refresh_class_spellcasting(level);
}

static bool
    dndolphins_subclass_allowed(const PocketD20App* app, uint16_t class_mask, bool has_metadata) {
    if(app->catalog_show_all) return true;
    if(!has_metadata || app->record_index >= app->data.character.class_count) return false;
    uint16_t selected_class =
        dndolphins_class_mask_from_name(app->data.character.classes[app->record_index].name);
    return selected_class && (class_mask & selected_class);
}


static bool dndolphins_feat_is_repeatable(const char* name) {
    return name && (!strcmp(name, "Ability Score Improvement") ||
                    !strcmp(name, "Magic Initiate") ||
                    !strcmp(name, "Skilled"));
}

static bool dndolphins_feat_is_fighting_style(const char* name) {
    return name && (!strcmp(name, "Archery") || !strcmp(name, "Defense") ||
                    !strcmp(name, "Great Weapon Fighting") ||
                    !strcmp(name, "Two-Weapon Fighting"));
}

static bool dndolphins_feat_is_epic_boon(const char* name) {
    return name && !strncmp(name, "Boon of ", 8U);
}

static bool dndolphins_character_has_fighting_style_feature(const PocketCharacter* c) {
    if(!c) return false;
    for(uint8_t i = 0U; i < c->class_count; ++i) {
        const PocketClassLevel* level = &c->classes[i];
        if(!strcmp(level->name, "Fighter") && level->level >= 1U) return true;
        if((!strcmp(level->name, "Paladin") || !strcmp(level->name, "Ranger")) &&
           level->level >= 2U)
            return true;
    }
    return false;
}

static bool dndolphins_character_has_spellcasting_feature(const PocketCharacter* c) {
    if(!c) return false;
    for(uint8_t i = 0U; i < c->class_count; ++i) {
        if(c->classes[i].spellcasting_mode != PocketSpellcastingNone) return true;
    }
    return false;
}

static bool dndolphins_feat_allowed(PocketD20App* app, const char* name) {
    if(!app || !name || !name[0]) return false;
    /* Manual Features & Perks editing remains an unrestricted catalog. The
       prerequisite filter is only for an actual level-up/progression feat choice. */
    if(app->level_choice_mode != 3U || app->catalog_show_all) return true;

    const PocketCharacter* c = &app->data.character;
    uint8_t total_level = dnd_rules_core_total_level(c);
    bool recognized = false;
    bool allowed = true;

    if(!strcmp(name, "Ability Score Improvement")) {
        recognized = true;
        allowed = total_level >= 4U;
    } else if(!strcmp(name, "Grappler")) {
        recognized = true;
        allowed = total_level >= 4U &&
                  (c->ability_scores[PocketAbilityStrength] >= 13 ||
                   c->ability_scores[PocketAbilityDexterity] >= 13);
    } else if(dndolphins_feat_is_fighting_style(name)) {
        recognized = true;
        allowed = dndolphins_character_has_fighting_style_feature(c);
    } else if(dndolphins_feat_is_epic_boon(name)) {
        recognized = true;
        allowed = total_level >= 19U;
        if(allowed && !strcmp(name, "Boon of Spell Recall"))
            allowed = dndolphins_character_has_spellcasting_feature(c);
    } else if(!strcmp(name, "Alert") || !strcmp(name, "Magic Initiate") ||
              !strcmp(name, "Savage Attacker") || !strcmp(name, "Skilled")) {
        recognized = true;
        allowed = true;
    }

    /* Allowed is intentionally conservative: only rows whose prerequisites the
       app can positively validate are shown. Custom/unrecognized feat/perk rows
       remain available through Hold OK -> All. */
    if(!recognized) return false;
    if(!allowed) return false;
    if(dndolphins_feat_is_repeatable(name)) return true;

    bool found = false;
    if(!dndolphins_progression_store_features_contains_name(
           app->storage, app->profiles.active_profile, name, &found))
        return false; /* Allowed fails closed when duplicate eligibility cannot be verified. */
    return !found;
}

static const char* dndolphins_catalog_title(const PocketD20App* app) {
    switch(app->catalog_kind) {
    case PocketCatalogClasses:
        return "Choose Class";
    case PocketCatalogSubclasses:
        return app->catalog_show_all ? "Subclasses: All" : "Choose Subclass";
    case PocketCatalogSpecies:
        return "Choose Species";
    case PocketCatalogBackgrounds:
        return "Choose Background";
    case PocketCatalogAlignments:
        return "Choose Alignment";
    case PocketCatalogFeats:
        if(app->level_choice_mode == 3U)
            return app->catalog_show_all ? "Feats: All" : "Feats: Allowed";
        return "Choose Feat/Perk";
    default:
        return "Choose Name";
    }
}


static bool dndolphins_catalog_add_metadata(
    PocketD20App* app,
    const char* name,
    uint8_t level,
    uint16_t class_mask,
    bool has_metadata) {
    if(app->catalog_kind == PocketCatalogSubclasses &&
       !dndolphins_subclass_allowed(app, class_mask, has_metadata)) {
        return false;
    }
    /* ASI is already represented by the two explicit ability-score options on
       the level-choice screen. Do not expose a no-effect ASI Feature row from
       the nested feat picker, even in All mode. */
    if(app->catalog_kind == PocketCatalogFeats && app->level_choice_mode == 3U &&
       !strcmp(name, "Ability Score Improvement"))
        return true;
    if(app->catalog_kind == PocketCatalogFeats && !dndolphins_feat_allowed(app, name))
        return true;
    if(!name[0]) return false;
    uint16_t page_limit = dndolphins_catalog_page_limit(app);
    uint16_t absolute_index = app->catalog_scan_count++;
    if(absolute_index < app->catalog_page_start ||
       absolute_index >= app->catalog_page_start + page_limit)
        return true;
    for(uint16_t i = 0U; i < app->catalog_count; ++i) {
        if(strcmp(app->catalog_entries[i], name) == 0) {
            if(has_metadata) {
                app->catalog_levels[i] = level;
                app->catalog_class_masks[i] |= class_mask;
                app->catalog_has_metadata[i] = 1U;
            }
            return true;
        }
    }
    if(app->catalog_count >= page_limit ||
       !dndolphins_catalog_ensure_capacity(app, app->catalog_count + 1U)) {
        dndolphins_set_status(app, "Catalog memory full");
        return false;
    }
    dndolphins_copy(
        app->catalog_entries[app->catalog_count],
        sizeof(app->catalog_entries[app->catalog_count]),
        name);
    app->catalog_levels[app->catalog_count] = level;
    app->catalog_class_masks[app->catalog_count] = class_mask;
    app->catalog_has_metadata[app->catalog_count] = has_metadata ? 1U : 0U;
    ++app->catalog_count;
    return true;
}


static bool dndolphins_catalog_page_complete(const PocketD20App* app) {
    return app->catalog_scan_count > app->catalog_page_start + dndolphins_catalog_page_limit(app);
}

static bool dndolphins_catalog_add(PocketD20App* app, const char* name) {
    return dndolphins_catalog_add_metadata(app, name, 0U, 0U, false);
}




static void dndolphins_catalog_add_builtins(PocketD20App* app, PocketCatalogKind kind) {
    const char* const* entries = NULL;
    size_t count = 0U;
    switch(kind) {
    case PocketCatalogClasses:
        entries = dndolphins_catalog_classes;
        count = sizeof(dndolphins_catalog_classes) / sizeof(dndolphins_catalog_classes[0]);
        break;
    case PocketCatalogSubclasses:
        for(size_t i = 0U;
            i < sizeof(dndolphins_catalog_subclasses) / sizeof(dndolphins_catalog_subclasses[0]);
            ++i) {
            dndolphins_catalog_add_metadata(
                app,
                dndolphins_catalog_subclasses[i].name,
                0U,
                dndolphins_catalog_subclasses[i].class_mask,
                true);
            if(dndolphins_catalog_page_complete(app)) break;
        }
        return;
    case PocketCatalogSpecies:
        entries = dndolphins_catalog_species;
        count = sizeof(dndolphins_catalog_species) / sizeof(dndolphins_catalog_species[0]);
        break;
    case PocketCatalogBackgrounds:
        entries = dndolphins_catalog_backgrounds;
        count = sizeof(dndolphins_catalog_backgrounds) / sizeof(dndolphins_catalog_backgrounds[0]);
        break;
    case PocketCatalogAlignments:
        entries = dndolphins_catalog_alignments;
        count = sizeof(dndolphins_catalog_alignments) / sizeof(dndolphins_catalog_alignments[0]);
        break;
    case PocketCatalogFeats:
        entries = dndolphins_catalog_feats;
        count = sizeof(dndolphins_catalog_feats) / sizeof(dndolphins_catalog_feats[0]);
        break;
    default:
        break;
    }
    for(size_t i = 0U; i < count; ++i) {
        dndolphins_catalog_add(app, entries[i]);
        if(dndolphins_catalog_page_complete(app)) break;
    }
}

static void dndolphins_catalog_process_line(PocketD20App* app, char* line) {
    char* start = line;
    while(*start == ' ' || *start == '\t')
        ++start;
    char* end = start + strlen(start);
    while(end > start && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r'))
        --end;
    *end = '\0';
    if(!start[0] || start[0] == '#') return;
    if(app->catalog_kind == PocketCatalogSubclasses) {
        char* class_separator = strrchr(start, '|');
        if(!class_separator) {
            dndolphins_catalog_add_metadata(app, start, 0U, 0U, false);
            return;
        }
        *class_separator = '\0';
        char* class_name = class_separator + 1U;
        while(*class_name == ' ' || *class_name == '\t')
            ++class_name;
        uint16_t mask = dndolphins_class_mask_from_name(class_name);
        dndolphins_catalog_add_metadata(app, start, 0U, mask, mask != 0U);
        return;
    }
    dndolphins_catalog_add(app, start);
    return;
}

static bool dndolphins_catalog_load_path(PocketD20App* app, const char* path) {
    File* file = storage_file_alloc(app->storage);
    if(!file) return false;
    if(!storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_free(file);
        return true;
    }
    char line[192];
    size_t position = 0U;
    uint8_t buffer[256];
    bool complete = true;
    size_t count = 0U;
    while((count = storage_file_read(file, buffer, sizeof(buffer))) > 0U) {
        for(size_t i = 0U; i < count; ++i) {
            char byte = (char)buffer[i];
            if(byte == '\n') {
                line[position] = '\0';
                dndolphins_catalog_process_line(app, line);
                position = 0U;
                if(dndolphins_catalog_page_complete(app)) {
                    complete = false;
                    goto finished;
                }
            } else if(position + 1U < sizeof(line)) {
                line[position++] = byte;
            }
        }
    }
    if(position) {
        line[position] = '\0';
        dndolphins_catalog_process_line(app, line);
    }
finished:
    storage_file_close(file);
    storage_file_free(file);
    return complete;
}

static void dndolphins_catalog_load_external(PocketD20App* app, PocketCatalogKind kind) {
    if(kind >= PocketCatalogCount) return;
    bool complete = dndolphins_catalog_load_path(app, dndolphins_bundled_catalog_paths[kind]);
    if(complete && kind == PocketCatalogFeats)
        dndolphins_catalog_load_path(app, dndolphins_bundled_catalog_abilities_path);
}

static uint8_t dndolphins_grant_source_from_text(const char* text) {
    if(strcmp(text, "species") == 0) return PocketGrantSpecies;
    if(strcmp(text, "background") == 0) return PocketGrantBackground;
    if(strcmp(text, "feat") == 0) return PocketGrantFeat;
    if(strcmp(text, "class_feature") == 0) return PocketGrantClassFeature;
    if(strcmp(text, "subclass_feature") == 0) return PocketGrantSubclassFeature;
    if(strcmp(text, "item") == 0) return PocketGrantItem;
    return PocketGrantSourceCount;
}


static uint8_t dndolphins_split_metadata(char* line, char* fields[8]) {
    uint8_t count = 0U;
    char* cursor = line;
    while(count < 8U) {
        fields[count++] = cursor;
        char* separator = strchr(cursor, '|');
        if(!separator) break;
        *separator = '\0';
        cursor = separator + 1U;
    }
    return count;
}



static bool dndolphins_class_asi_level(const char* class_name, uint8_t level) {
    if(level == 4U || level == 8U || level == 12U || level == 16U || level == 19U) return true;
    if(class_name && strcmp(class_name, "Fighter") == 0 && (level == 6U || level == 14U)) return true;
    if(class_name && strcmp(class_name, "Rogue") == 0 && level == 10U) return true;
    return false;
}

static void dndolphins_level_choice_id(char* out, size_t size, const PocketCharacter* c, uint8_t class_index, uint8_t level) {
    const char* name = class_index < c->class_count ? c->classes[class_index].name : "class";
    snprintf(out, size, "asi_%.12s_%u", name, level);
    for(char* p = out; *p; ++p) if(*p == ' ') *p = '_';
}

static bool dndolphins_level_choice_done(PocketD20App* app, uint8_t class_index, uint8_t level) {
    char id[POCKET_D20_SHORT_LEN];
    dndolphins_level_choice_id(id, sizeof(id), &app->data.character, class_index, level);
    return dndolphins_progression_store_applied_exists(app->storage, app->profiles.active_profile, id);
}

static bool dndolphins_begin_next_level_choice(PocketD20App* app) {
    PocketCharacter* c = &app->data.character;
    app->level_choice_class_index = 0U;
    app->level_choice_level = 0U;
    app->level_choice_mode = 0U;
    app->level_choice_first_ability = UINT8_MAX;
    app->level_choice_first_score = 0;
    for(uint8_t ci = 0U; ci < c->class_count; ++ci) {
        for(uint8_t level = 1U; level <= c->classes[ci].level; ++level) {
            if(dndolphins_class_asi_level(c->classes[ci].name, level) && !dndolphins_level_choice_done(app, ci, level)) {
                app->level_choice_class_index = ci;
                app->level_choice_level = level;
                app->level_choice_mode = 0U;
                app->level_choice_first_ability = UINT8_MAX;
                app->level_choice_first_score = 0;
                return true;
            }
        }
    }
    return false;
}

static void dndolphins_begin_level_review(
    PocketD20App* app,
    uint8_t class_index,
    uint8_t old_level,
    uint8_t old_pb,
    uint8_t old_cantrips,
    uint8_t old_prepared,
    const uint8_t old_slots[POCKET_D20_SLOT_COUNT]) {
    if(!app || class_index >= app->data.character.class_count) return;
    PocketCharacter* character = &app->data.character;
    PocketClassLevel* class_level = &character->classes[class_index];
    app->level_review_class_index = class_index;
    app->level_review_old_level = old_level;
    app->level_review_new_level = class_level->level;
    app->level_review_old_pb = old_pb;
    app->level_review_new_pb = dnd_rules_core_proficiency_bonus(character);
    app->level_review_old_cantrips = old_cantrips;
    app->level_review_new_cantrips = class_level->cantrip_limit;
    app->level_review_old_prepared = old_prepared;
    app->level_review_new_prepared = class_level->prepared_limit;
    app->level_review_slots_changed =
        memcmp(old_slots, character->spell_slots_max, POCKET_D20_SLOT_COUNT) != 0;
    app->level_review_choose_spells =
        class_level->cantrip_limit > old_cantrips || class_level->prepared_limit > old_prepared;
    app->level_review_pending_choice = dndolphins_begin_next_level_choice(app) ? 1U : 0U;
    app->return_screen = PocketScreenRecordDetail;
    dndolphins_enter_screen(app, PocketScreenLevelReview);
    app->selection = 0U;
    app->scroll = 0U;
}

static bool dndolphins_complete_level_choice(PocketD20App* app, const char* result) {
    (void)result;
    PocketCharacter* c = &app->data.character;
    char id[POCKET_D20_SHORT_LEN];
    dndolphins_level_choice_id(id, sizeof(id), c, app->level_choice_class_index, app->level_choice_level);
    return dndolphins_progression_store_mark_applied(app->storage, app->profiles.active_profile, id);
}

typedef struct {
    const char* stable_id;
    bool found;
} DndDolphinsSpellStableIdLookup;

static bool dndolphins_spell_stable_id_visitor(
    uint8_t logical_index,
    const PocketSpell* spell,
    uint8_t known,
    uint8_t always_prepared,
    uint8_t free_casts_current,
    uint8_t free_casts_max,
    void* context) {
    UNUSED(logical_index);
    UNUSED(known);
    UNUSED(always_prepared);
    UNUSED(free_casts_current);
    UNUSED(free_casts_max);
    DndDolphinsSpellStableIdLookup* lookup = context;
    if(!lookup || !spell || !lookup->stable_id) return false;
    if(spell->stable_id[0] && strcmp(spell->stable_id, lookup->stable_id) == 0) {
        lookup->found = true;
        return false;
    }
    return true;
}

static bool dndolphins_spell_stable_id_exists(PocketD20App* app, const char* stable_id) {
    if(!app || !stable_id || !stable_id[0]) return false;
    DndDolphinsSpellStableIdLookup lookup = {.stable_id = stable_id, .found = false};
    if(!dnd_storage_visit_spells(
           app->storage,
           app->profiles.active_profile,
           dndolphins_spell_stable_id_visitor,
           &lookup,
           NULL))
        return false;
    return lookup.found;
}

static bool dndolphins_csv_contains(const char* csv, const char* value) {
    if(!csv || !value || !value[0]) return false;
    const size_t value_len = strlen(value);
    const char* cursor = csv;
    while(*cursor) {
        while(*cursor == ' ' || *cursor == ',') ++cursor;
        const char* end = strchr(cursor, ',');
        size_t len = end ? (size_t)(end - cursor) : strlen(cursor);
        while(len && cursor[len - 1U] == ' ') --len;
        if(len == value_len && strncmp(cursor, value, len) == 0) return true;
        if(!end) break;
        cursor = end + 1U;
    }
    return false;
}

static bool dndolphins_grant_payload_satisfied(PocketD20App* app, const char* grant_value) {
    if(!app || !grant_value || !grant_value[0]) return false;
    PocketCharacter* character = &app->data.character;
    char payload[POCKET_D20_NAME_LEN];
    dndolphins_copy(payload, sizeof(payload), grant_value);
    char* separator = strchr(payload, '=');
    if(!separator) return false;
    *separator = '\0';
    const char* value = separator + 1U;

    if(strcmp(payload, "origin_feat") == 0) return strcmp(character->origin_feat, value) == 0;
    if(strcmp(payload, "tool") == 0) return strcmp(character->tool_proficiencies, value) == 0;
    if(strcmp(payload, "armor") == 0) return strcmp(character->armor_training, value) == 0;
    if(strcmp(payload, "weapon") == 0) return strcmp(character->weapon_training, value) == 0;
    if(strcmp(payload, "senses") == 0) return strcmp(character->senses, value) == 0;
    if(strcmp(payload, "resistance") == 0)
        return dndolphins_csv_contains(character->resistances, value);
    if(strcmp(payload, "speed") == 0) {
        uint32_t speed = 0U;
        return dndolphins_parse_u32_strict(value, 255U, &speed) &&
               character->speed == (int16_t)speed;
    }
    if(strcmp(payload, "size") == 0) {
        for(uint8_t i = 0U; i < PocketSizeCount; ++i)
            if(strcmp(value, dndolphins_size_names[i]) == 0) return character->size == i;
        return false;
    }
    if(strcmp(payload, "feature") == 0 || strcmp(payload, "feat_long") == 0 ||
       strcmp(payload, "feat_pb") == 0) {
        bool found = false;
        return dndolphins_progression_store_features_contains_name(
                   app->storage, app->profiles.active_profile, value, &found) &&
               found;
    }
    return false;
}

static bool dndolphins_grant_stable_id_exists(
    PocketD20App* app, const char* stable_id, const char* grant_value) {
    PocketCharacter* character = &app->data.character;
    if(!stable_id || !stable_id[0]) return false;
    for(uint8_t i = 0U; i < character->grant_count; ++i)
        if(strcmp(character->grants[i].stable_id, stable_id) == 0) return true;

    /* Persisted character/collection state is authoritative. Applied-grant marker
       files are only an audit trail and must never make a missing deterministic
       grant look complete. This also avoids reopening/scanning appliedgrants for
       every candidate, which made the explicit grant commands unnecessarily slow. */
    if(grant_value && strncmp(grant_value, "spell=", 6U) == 0)
        return dndolphins_spell_stable_id_exists(app, stable_id);
    return dndolphins_grant_payload_satisfied(app, grant_value);
}

static uint8_t dndolphins_stage_grants_up_to(
    PocketD20App* app, uint8_t source_type, const char* option, uint8_t maximum_level) {
    PocketCharacter* character = &app->data.character;
    File* file = storage_file_alloc(app->storage);
    if(!file) return 0U;
    if(!storage_file_open(
           file, dndolphins_active_metadata_path(app->storage), FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_free(file);
        return 0U;
    }
    uint8_t staged = 0U;
    char line[256];
    size_t position = 0U;
    uint8_t buffer[512];
    size_t count = 0U;
    while((count = storage_file_read(file, buffer, sizeof(buffer))) > 0U) {
        for(size_t i = 0U; i < count; ++i) {
            char byte = (char)buffer[i];
            if(byte != '\n' && position + 1U < sizeof(line)) {
                if(byte != '\r') line[position++] = byte;
                continue;
            }
            line[position] = '\0';
            position = 0U;
            if(!line[0] || line[0] == '#' || character->grant_count >= POCKET_D20_MAX_GRANTS)
                continue;
            char* fields[8];
            if(dndolphins_split_metadata(line, fields) != 8U ||
               dndolphins_grant_source_from_text(fields[2]) != source_type ||
               strcmp(fields[3], option) != 0 || !fields[7][0])
                continue;
            char stable_id[POCKET_D20_SHORT_LEN];
            dndolphins_copy(stable_id, sizeof(stable_id), fields[0]);
            if(dndolphins_grant_stable_id_exists(app, stable_id, fields[7])) continue;
            uint32_t level_gained = 0U;
            if(!dndolphins_parse_u32_strict(fields[5], UINT8_MAX, &level_gained)) continue;
            if(level_gained > maximum_level) continue;
            if(source_type == PocketGrantClassFeature || source_type == PocketGrantSubclassFeature) {
                uint8_t class_index = app->record_index < character->class_count ? app->record_index : 0U;
                if(level_gained == 0U || level_gained > character->classes[class_index].level) continue;
            } else if(source_type == PocketGrantSpecies) {
                uint8_t total_level = dnd_rules_core_total_level(character);
                if(total_level < 1U) total_level = 1U;
                if(level_gained > total_level) continue;
            }
            if(!dnd_data_reserve_grants(character, character->grant_count + 1U)) continue;
            PocketGrant* grant = &character->grants[character->grant_count++];
            memset(grant, 0, sizeof(*grant));
            dndolphins_copy(grant->stable_id, sizeof(grant->stable_id), stable_id);
            dndolphins_copy(grant->source, sizeof(grant->source), fields[1]);
            dndolphins_copy(grant->option_name, sizeof(grant->option_name), fields[3]);
            dndolphins_copy(grant->prerequisites, sizeof(grant->prerequisites), fields[4]);
            dndolphins_copy(grant->grant_value, sizeof(grant->grant_value), fields[7]);
            grant->source_type = source_type;
            grant->class_index = source_type == PocketGrantSpecies ? 0U :
                                 app->record_index < character->class_count ? app->record_index : 0U;
            grant->level_gained = (uint8_t)level_gained;
            grant->status = PocketGrantPending;
            ++staged;
        }
    }
    storage_file_close(file);
    storage_file_free(file);
    return staged;
}

static uint8_t dndolphins_stage_grants(
    PocketD20App* app, uint8_t source_type, const char* option) {
    return dndolphins_stage_grants_up_to(app, source_type, option, UINT8_MAX);
}

static void dndolphins_append_csv_unique(char* destination, size_t size, const char* value) {
    if(!destination || !size || !value || !value[0]) return;
    const size_t value_len = strlen(value);
    const char* cursor = destination;
    while(*cursor) {
        while(*cursor == ' ' || *cursor == ',') ++cursor;
        const char* end = strchr(cursor, ',');
        size_t len = end ? (size_t)(end - cursor) : strlen(cursor);
        while(len && cursor[len - 1U] == ' ') --len;
        if(len == value_len && strncmp(cursor, value, len) == 0) return;
        if(!end) break;
        cursor = end + 1U;
    }
    size_t used = strlen(destination);
    const char* separator = used ? ", " : "";
    size_t separator_len = used ? 2U : 0U;
    if(used + separator_len + value_len + 1U > size) return;
    if(separator_len) {
        destination[used++] = separator[0];
        destination[used++] = separator[1];
    }
    memcpy(destination + used, value, value_len + 1U);
}

static bool dndolphins_lookup_bundled_spell_level(PocketD20App* app, const char* spell_name, uint8_t* level_out) {
    if(!app || !spell_name || !spell_name[0] || !level_out) return false;
    File* file = storage_file_alloc(app->storage);
    if(!file || !storage_file_open(
           file, dndolphins_progression_spell_metadata_path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        if(file) storage_file_free(file);
        return false;
    }
    char line[256];
    size_t used = 0U;
    uint8_t buffer[256];
    bool found = false;
    size_t count = 0U;
    while(!found && (count = storage_file_read(file, buffer, sizeof(buffer))) > 0U) {
        for(size_t i = 0U; i < count; ++i) {
            char byte = (char)buffer[i];
            if(byte != '\n' && used + 1U < sizeof(line)) {
                if(byte != '\r') line[used++] = byte;
                continue;
            }
            line[used] = '\0';
            used = 0U;
            char* separator = strchr(line, '|');
            if(!separator) continue;
            *separator = '\0';
            if(strcmp(line, spell_name) != 0) continue;
            char* level_text = separator + 1U;
            char* second = strchr(level_text, '|');
            if(second) *second = '\0';
            uint32_t parsed = 0U;
            if(dndolphins_parse_u32_strict(level_text, 9U, &parsed)) {
                *level_out = (uint8_t)parsed;
                found = true;
            }
        }
    }
    storage_file_close(file);
    storage_file_free(file);
    return found;
}

static void dndolphins_apply_grant(PocketD20App* app, PocketGrant* grant) {
    PocketCharacter* character = &app->data.character;
    char payload[POCKET_D20_NAME_LEN];
    dndolphins_copy(payload, sizeof(payload), grant->grant_value);
    char* separator = strchr(payload, '=');
    if(!separator) {
        grant->status = PocketGrantSkipped;
        return;
    }
    *separator = '\0';
    const char* value = separator + 1U;
    bool applied = false;
    if(strcmp(payload, "origin_feat") == 0) {
        dndolphins_copy(character->origin_feat, sizeof(character->origin_feat), value);
        applied = true;
    } else if(strcmp(payload, "tool") == 0) {
        dndolphins_copy(character->tool_proficiencies, sizeof(character->tool_proficiencies), value);
        applied = true;
    } else if(strcmp(payload, "armor") == 0) {
        dndolphins_copy(character->armor_training, sizeof(character->armor_training), value);
        applied = true;
    } else if(strcmp(payload, "weapon") == 0) {
        dndolphins_copy(character->weapon_training, sizeof(character->weapon_training), value);
        applied = true;
    } else if(strcmp(payload, "senses") == 0) {
        dndolphins_copy(character->senses, sizeof(character->senses), value);
        applied = true;
    } else if(strcmp(payload, "size") == 0) {
        for(uint8_t i = 0U; i < PocketSizeCount; ++i) {
            if(strcmp(value, dndolphins_size_names[i]) == 0) {
                character->size = i;
                applied = true;
                break;
            }
        }
    } else if(strcmp(payload, "resistance") == 0) {
        dndolphins_append_csv_unique(character->resistances, sizeof(character->resistances), value);
        applied = true;
    } else if(strcmp(payload, "speed") == 0) {
        uint32_t speed = 0U;
        if(dndolphins_parse_u32_strict(value, 255U, &speed)) {
            character->speed = (int16_t)speed;
            applied = true;
        }
    } else if(
        strcmp(payload, "feature") == 0 || strcmp(payload, "feat_long") == 0 ||
        strcmp(payload, "feat_pb") == 0) {
        PocketFeature feature;
        memset(&feature, 0, sizeof(feature));
        dndolphins_copy(feature.name, sizeof(feature.name), value);
        feature.class_index = grant->class_index;
        feature.class_level_gained = grant->level_gained;
        if(strcmp(payload, "feat_long") == 0) {
            feature.uses_current = 1;
            feature.uses_max = 1;
            feature.recharge = PocketRechargeLong;
        } else if(strcmp(payload, "feat_pb") == 0) {
            feature.uses_current = dnd_rules_core_proficiency_bonus(character);
            feature.uses_max = feature.uses_current;
            feature.recharge = PocketRechargeLong;
            feature.resource_formula = PocketResourceProficiency;
        }
        applied = dndolphins_progression_store_features_append(
            app->storage, app->profiles.active_profile, &feature);
        if(applied) {
            uint8_t total = 0U;
            if(dndolphins_progression_store_features_count(app->storage, app->profiles.active_profile, &total))
                app->features_total = total;
        }
    } else if(strcmp(payload, "spell") == 0) {
        uint8_t total_spells = 0U;
        if(!dnd_storage_visit_spells(
               app->storage, app->profiles.active_profile, NULL, NULL, &total_spells) ||
           total_spells >= POCKET_D20_MAX_SPELLS) {
            applied = false;
        } else {
            PocketSpell spell;
            memset(&spell, 0, sizeof(spell));
            dndolphins_copy(spell.name, sizeof(spell.name), value);
            dndolphins_copy(spell.source, sizeof(spell.source), grant->source);
            dndolphins_copy(spell.grant_name, sizeof(spell.grant_name), grant->option_name);
            dndolphins_copy(spell.stable_id, sizeof(spell.stable_id), grant->stable_id);
            spell.class_index = grant->class_index;
            spell.grant_source = grant->source_type;
            (void)dndolphins_lookup_bundled_spell_level(app, value, &spell.level);
            uint8_t free_cast =
                grant->source_type == PocketGrantSpecies && grant->level_gained >= 3U ? 1U : 0U;
            if(dnd_storage_append_spell(
                   app->storage,
                   app->profiles.active_profile,
                   character,
                   &spell,
                   1U,
                   1U,
                   free_cast,
                   free_cast)) {
                app->spell_class_counts_valid = 0U;
                app->spellbook_total = (uint8_t)(total_spells + 1U);
                applied = true;
            }
        }
    }
    *separator = '=';
    if(applied && grant->stable_id[0]) {
        /* Marker persistence is best-effort bookkeeping only. The authoritative
           character/Feature/Spellbook write has already succeeded, so a marker
           write failure must not roll the grant's logical result back to failed. */
        (void)dndolphins_progression_store_mark_applied(
            app->storage, app->profiles.active_profile, grant->stable_id);
    }
    grant->status = applied ? PocketGrantApplied : PocketGrantSkipped;
}

static bool dndolphins_stage_character_grant_line(
    PocketD20App* app,
    char* line,
    uint8_t maximum_level,
    bool include_background,
    uint8_t* staged) {
    if(!app || !line || !staged || !line[0] || line[0] == '#') return true;
    PocketCharacter* character = &app->data.character;
    if(character->grant_count >= POCKET_D20_MAX_GRANTS) return true;

    char* fields[8];
    if(dndolphins_split_metadata(line, fields) != 8U || !fields[7][0]) return true;
    uint8_t source_type = dndolphins_grant_source_from_text(fields[2]);
    if(source_type != PocketGrantSpecies && source_type != PocketGrantBackground &&
       source_type != PocketGrantClassFeature && source_type != PocketGrantSubclassFeature)
        return true;

    uint32_t level_gained_u32 = 0U;
    if(!dndolphins_parse_u32_strict(fields[5], UINT8_MAX, &level_gained_u32)) return true;
    uint8_t level_gained = (uint8_t)level_gained_u32;
    if(level_gained > maximum_level) return true;

    uint8_t class_index = 0U;
    bool matches = false;
    if(source_type == PocketGrantSpecies) {
        uint8_t total_level = dnd_rules_core_total_level(character);
        if(total_level < 1U) total_level = 1U;
        matches = strcmp(fields[3], character->species) == 0 && level_gained <= total_level;
    } else if(source_type == PocketGrantBackground) {
        matches = include_background && strcmp(fields[3], character->background) == 0;
    } else {
        if(level_gained == 0U) return true;
        for(uint8_t i = 0U; i < character->class_count; ++i) {
            const char* option = source_type == PocketGrantClassFeature ?
                                     character->classes[i].name :
                                     character->classes[i].subclass;
            if(source_type == PocketGrantSubclassFeature &&
               (!option[0] || strcmp(option, "None") == 0))
                continue;
            if(strcmp(fields[3], option) == 0 && level_gained <= character->classes[i].level) {
                class_index = i;
                matches = true;
                break;
            }
        }
    }
    if(!matches || dndolphins_grant_stable_id_exists(app, fields[0], fields[7])) return true;
    if(!dnd_data_reserve_grants(character, character->grant_count + 1U)) return false;

    PocketGrant* grant = &character->grants[character->grant_count++];
    memset(grant, 0, sizeof(*grant));
    dndolphins_copy(grant->stable_id, sizeof(grant->stable_id), fields[0]);
    dndolphins_copy(grant->source, sizeof(grant->source), fields[1]);
    dndolphins_copy(grant->option_name, sizeof(grant->option_name), fields[3]);
    dndolphins_copy(grant->prerequisites, sizeof(grant->prerequisites), fields[4]);
    dndolphins_copy(grant->grant_value, sizeof(grant->grant_value), fields[7]);
    grant->source_type = source_type;
    grant->class_index = class_index;
    grant->level_gained = level_gained;
    grant->status = PocketGrantPending;
    ++*staged;
    return true;
}

static bool dndolphins_stage_character_grants(
    PocketD20App* app, uint8_t maximum_level, bool include_background, uint8_t* staged_out) {
    if(!app) return false;
    if(app->features_loaded && !dndolphins_release_features(app)) return false;
    dndolphins_release_pending_grants(app);

    File* file = storage_file_alloc(app->storage);
    if(!file) return false;
    if(!storage_file_open(
           file, dndolphins_active_metadata_path(app->storage), FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_free(file);
        return false;
    }

    uint8_t staged = 0U;
    char line[256];
    size_t position = 0U;
    uint8_t buffer[512];
    bool ok = true;
    size_t count = 0U;
    while(ok && app->data.character.grant_count < POCKET_D20_MAX_GRANTS &&
          (count = storage_file_read(file, buffer, sizeof(buffer))) > 0U) {
        for(size_t i = 0U; i < count; ++i) {
            char byte = (char)buffer[i];
            if(byte != '\n' && position + 1U < sizeof(line)) {
                if(byte != '\r') line[position++] = byte;
                continue;
            }
            if(byte != '\n') {
                /* Oversize metadata lines are ignored as invalid instead of truncated. */
                position = 0U;
                continue;
            }
            line[position] = '\0';
            position = 0U;
            if(!dndolphins_stage_character_grant_line(
                   app, line, maximum_level, include_background, &staged)) {
                ok = false;
                break;
            }
            if(app->data.character.grant_count >= POCKET_D20_MAX_GRANTS) break;
        }
    }
    if(ok && position && app->data.character.grant_count < POCKET_D20_MAX_GRANTS) {
        line[position] = '\0';
        ok = dndolphins_stage_character_grant_line(
            app, line, maximum_level, include_background, &staged);
    }
    if(storage_file_get_error(file) != FSE_OK) ok = false;
    storage_file_close(file);
    storage_file_free(file);
    if(staged_out) *staged_out = staged;
    return ok;
}

static void dndolphins_apply_pending_grants(
    PocketD20App* app, uint8_t* applied_out, uint8_t* failed_out) {
    uint8_t applied = 0U;
    uint8_t failed = 0U;
    PocketCharacter* character = &app->data.character;
    for(uint8_t i = 0U; i < character->grant_count; ++i) {
        if(character->grants[i].status != PocketGrantPending) continue;
        dndolphins_apply_grant(app, &character->grants[i]);
        if(character->grants[i].status == PocketGrantApplied)
            ++applied;
        else
            ++failed;
    }
    if(applied_out) *applied_out = applied;
    if(failed_out) *failed_out = failed;
}

static bool dndolphins_apply_character_grants(
    PocketD20App* app,
    uint8_t maximum_level,
    bool include_background,
    uint16_t* applied_total_out,
    uint8_t* failed_out,
    bool* had_candidates_out) {
    uint16_t applied_total = 0U;
    uint8_t failed = 0U;
    bool had_candidates = false;

    /* The resident grant list is deliberately bounded. Drain successful batches
       in one explicit action so a high-level class + subclass + species can
       exceed POCKET_D20_MAX_GRANTS without requiring repeated menu presses. */
    for(uint8_t batch = 0U; batch < 8U; ++batch) {
        uint8_t staged = 0U;
        if(!dndolphins_stage_character_grants(
               app, maximum_level, include_background, &staged))
            return false;
        if(!staged) {
            dndolphins_release_pending_grants(app);
            break;
        }
        had_candidates = true;

        uint8_t applied = 0U;
        dndolphins_apply_pending_grants(app, &applied, &failed);
        applied_total += applied;
        if(failed) break;

        dndolphins_release_pending_grants(app);
        if(staged < POCKET_D20_MAX_GRANTS) break;
    }

    if(applied_total_out) *applied_total_out = applied_total;
    if(failed_out) *failed_out = failed;
    if(had_candidates_out) *had_candidates_out = had_candidates;
    return true;
}


static void dndolphins_catalog_load_page(PocketD20App* app) {
    PocketCatalogKind kind = app->catalog_kind;
    dndolphins_catalog_release(app);
    app->catalog_scan_count = 0U;
    app->catalog_has_more = 0U;
    if(!storage_file_exists(app->storage, dndolphins_bundled_catalog_paths[kind]))
        dndolphins_catalog_add_builtins(app, kind);
    dndolphins_catalog_load_external(app, kind);
    if(dndolphins_catalog_page_complete(app)) app->catalog_has_more = 1U;
    app->catalog_total = app->catalog_scan_count;
    if(app->catalog_page_start >= app->catalog_total && app->catalog_page_start) {
        uint16_t page_limit = dndolphins_catalog_page_limit(app);
        app->catalog_page_start =
            ((app->catalog_total ? app->catalog_total - 1U : 0U) / page_limit) * page_limit;
        dndolphins_catalog_release(app);
        app->catalog_scan_count = 0U;
        app->catalog_has_more = 0U;
        if(!storage_file_exists(app->storage, dndolphins_bundled_catalog_paths[kind]))
            dndolphins_catalog_add_builtins(app, kind);
        dndolphins_catalog_load_external(app, kind);
        if(dndolphins_catalog_page_complete(app)) app->catalog_has_more = 1U;
        app->catalog_total = app->catalog_scan_count;
    }
}

static void dndolphins_open_catalog(
    PocketD20App* app,
    PocketCatalogKind kind,
    PocketEditTarget target,
    const char* current) {
    dndolphins_release_text_input(app);
    dndolphins_release_number_input(app);
    app->catalog_kind = kind;
    app->catalog_target = target;
    app->catalog_page_size = POCKET_D20_MAX_CATALOG_ENTRIES;
    app->return_screen = app->screen;
    app->catalog_return_selection = app->selection;
    app->catalog_show_all = 0U;
    app->catalog_page_start = 0U;
    dndolphins_catalog_load_page(app);
    dndolphins_enter_screen(app, PocketScreenCatalog);
    for(uint16_t i = 0U; i < app->catalog_count; ++i) {
        if(strcmp(app->catalog_entries[i], current) == 0) {
            app->selection = i;
            if(i >= 5U) app->scroll = i - 4U;
            break;
        }
    }
}

static void dndolphins_draw_header(Canvas* canvas, const char* title, const char* status) {
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_box(canvas, 0, 0, 128, 10);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 8, title);
    if(status && status[0] != '\0') {
        uint16_t width = canvas_string_width(canvas, status);
        if(width < 62U) canvas_draw_str(canvas, 126 - width, 8, status);
    }
    canvas_set_color(canvas, ColorBlack);
}

static void dndolphins_draw_row(Canvas* canvas, uint8_t row, bool selected, const char* text) {
    uint8_t y = (uint8_t)(11U + (row * 10U));
    char display[32];
    size_t length = strlen(text);
    if(selected && length > 25U) {
        size_t cycle = length + 4U;
        size_t start = dndolphins_marquee_offset % cycle;
        for(size_t i = 0U; i < 25U; ++i) {
            size_t position = (start + i) % cycle;
            display[i] = position < length ? text[position] : ' ';
        }
        display[25] = '\0';
    } else {
        size_t copy = length > 25U ? 25U : length;
        memcpy(display, text, copy);
        display[copy] = '\0';
    }
    if(selected) {
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_box(canvas, 0, y, 128, 10);
        canvas_set_color(canvas, ColorWhite);
    } else {
        canvas_set_color(canvas, ColorBlack);
    }
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 3, y + 8U, display);
    canvas_set_color(canvas, ColorBlack);
}

static void dndolphins_draw_menu_rows(
    Canvas* canvas,
    PocketD20App* app,
    const char* const* rows,
    uint16_t count) {
    for(uint8_t visible = 0U; visible < 5U; ++visible) {
        uint16_t index = app->scroll + visible;
        if(index >= count) break;
        const char* row = rows[index];
        char confirmed[32];
        if(app->action_ack_active && app->action_ack_screen == (uint8_t)app->screen &&
           app->action_ack_selection == index) {
            dndolphins_copy(confirmed, sizeof(confirmed), row);
            dndolphins_prefix_action_mark(confirmed, sizeof(confirmed));
            row = confirmed;
        }
        dndolphins_draw_row(canvas, visible, index == app->selection, row);
    }
}

static uint8_t dndolphins_list_count(const PocketD20App* app) {
    const PocketCharacter* character = &app->data.character;
    switch(app->list_kind) {
    case PocketListClasses: return character->class_count;
    case PocketListFeatures: return app->features_total;
    case PocketListLanguages: return character->language_count;
    default: return 0U;
    }
}

static const char* dndolphins_list_title(PocketListKind kind) {
    switch(kind) {
    case PocketListClasses: return "Classes";
    case PocketListFeatures: return "Features / Perks";
    case PocketListLanguages: return "Languages";
    default: return "List";
    }
}

static void dndolphins_format_list_entry(PocketD20App* app, uint8_t index, char* output, size_t size) {
    PocketCharacter* character = &app->data.character;
    switch(app->list_kind) {
    case PocketListClasses: {
        const PocketClassLevel* class_level = &character->classes[index];
        snprintf(output, size, "%.31s L%u", class_level->name, class_level->level);
        break;
    }
    case PocketListFeatures: {
        PocketFeature* feature = dndolphins_feature_at_cached(app, index, NULL);
        if(!feature) dndolphins_copy(output, size, "<read error>");
        else dndolphins_format_labeled_text(output, size, NULL, feature->name);
        break;
    }
    case PocketListLanguages:
        dndolphins_format_labeled_text(output, size, NULL, character->languages[index]);
        break;
    default:
        dndolphins_copy(output, size, "Unavailable");
        break;
    }
}

static bool dndolphins_refresh_combat_weapon_index(PocketD20App* app) {
    if(!app->combat_weapon_indices) {
        app->combat_weapon_indices = malloc(
            POCKET_D20_MAX_ITEMS * sizeof(*app->combat_weapon_indices) +
            POCKET_D20_COMBAT_VISIBLE_ROWS * POCKET_D20_COMBAT_ROW_LEN);
        if(!app->combat_weapon_indices) {
            dndolphins_set_status(app, "Combat memory full");
            return false;
        }
    }
    uint8_t total = 0U;
    if(!dndolphins_weapon_combat_items_collect_weapon_indices(
           app->storage,
           app->profiles.active_profile,
           app->combat_weapon_indices,
           POCKET_D20_MAX_ITEMS,
           &app->combat_weapon_count,
           &total)) {
        dndolphins_set_status(app, "Items read failed");
        return false;
    }
    app->items_total = total;
    return true;
}

static uint8_t dndolphins_weapon_count(PocketD20App* app) {
    return app->combat_weapon_count;
}

static uint8_t dndolphins_weapon_index(PocketD20App* app, uint8_t weapon_number) {
    if(!app->combat_weapon_indices || weapon_number >= app->combat_weapon_count) return 0xFFU;
    return app->combat_weapon_indices[weapon_number];
}

static char* dndolphins_combat_weapon_row(PocketD20App* app, uint8_t visible) {
    if(!app || !app->combat_weapon_indices || visible >= POCKET_D20_COMBAT_VISIBLE_ROWS)
        return NULL;
    return (char*)(app->combat_weapon_indices + POCKET_D20_MAX_ITEMS) +
           ((size_t)visible * POCKET_D20_COMBAT_ROW_LEN);
}

static void dndolphins_prepare_combat_weapon_rows(PocketD20App* app) {
    if(!app || !app->combat_weapon_indices) return;
    for(uint8_t visible = 0U; visible < POCKET_D20_COMBAT_VISIBLE_ROWS; ++visible) {
        char* row = dndolphins_combat_weapon_row(app, visible);
        if(!row) continue;
        row[0] = '\0';
        uint16_t weapon_number = app->scroll + visible;
        if(weapon_number >= app->combat_weapon_count) continue;
        uint8_t item_index = dndolphins_weapon_index(app, (uint8_t)weapon_number);
        if(item_index == 0xFFU) continue;
        PocketItem* item = dndolphins_item_at(app, item_index, NULL);
        if(!item) continue;
        snprintf(
            row,
            POCKET_D20_COMBAT_ROW_LEN,
            "%s %+d %ud%u",
            item->name,
            dnd_weapon_rules_attack_modifier(&app->data.character, item),
            item->damage_dice,
            item->use_versatile ? item->versatile_die : item->damage_die);
    }
}

static uint8_t dndolphins_record_detail_count(const PocketD20App* app) {
    switch(app->list_kind) {
    case PocketListClasses: return 18U;
    case PocketListFeatures: return 10U;
    case PocketListLanguages: return 2U;
    default: return 0U;
    }
}

static void dndolphins_release_text_input(PocketD20App* app) {
    if(!app->text_input || app->input_module_active) return;
    view_dispatcher_remove_view(app->dispatcher, PocketViewTextInput);
    text_input_free(app->text_input);
    app->text_input = NULL;
}

static void dndolphins_release_number_input(PocketD20App* app) {
    if(!app->number_input || app->input_module_active) return;
    view_dispatcher_remove_view(app->dispatcher, PocketViewNumberInput);
    number_input_free(app->number_input);
    app->number_input = NULL;
}

static void dndolphins_begin_text(
    PocketD20App* app,
    PocketEditTarget target,
    const char* header,
    const char* initial) {
    dndolphins_release_number_input(app);
    if(!app->text_input) {
        app->text_input = text_input_alloc();
        if(!app->text_input) {
            dndolphins_set_status(app, "Text input memory low");
            return;
        }
        view_dispatcher_add_view(
            app->dispatcher, PocketViewTextInput, text_input_get_view(app->text_input));
    }
    app->edit_target = target;
    app->input_module_active = 1U;
    dndolphins_copy(app->edit_buffer, sizeof(app->edit_buffer), initial);
    text_input_reset(app->text_input);
    text_input_set_header_text(app->text_input, header);
    text_input_set_result_callback(
        app->text_input, dndolphins_text_done, app, app->edit_buffer, sizeof(app->edit_buffer), false);
    view_dispatcher_switch_to_view(app->dispatcher, PocketViewTextInput);
}

static uint8_t dndolphins_nearest_die(int32_t number, bool damage_only) {
    const uint8_t* choices = damage_only ? dndolphins_damage_die_choices : dndolphins_die_choices;
    uint8_t count = damage_only ? sizeof(dndolphins_damage_die_choices) : sizeof(dndolphins_die_choices);
    uint8_t best = choices[0];
    int32_t best_distance = abs(number - best);
    for(uint8_t i = 1U; i < count; ++i) {
        int32_t distance = abs(number - choices[i]);
        if(distance < best_distance) {
            best = choices[i];
            best_distance = distance;
        }
    }
    return best;
}

static void dndolphins_number_done(void* context, int32_t number) {
    PocketD20App* app = context;
    PocketCharacter* character = &app->data.character;
    PocketNumberContext completed_context = app->number_context;
    switch(app->number_context) {
    case PocketNumberCharacter:
        if(app->number_index == 7U) character->experience = (uint32_t)number;
        break;
    case PocketNumberVitals:
        switch(app->number_index) {
        case 0U:
            character->hp_current = (int16_t)number;
            break;
        case 1U:
            character->hp_max = (int16_t)number;
            break;
        case 2U:
            character->hp_temporary = (int16_t)number;
            break;
        case 3U:
            character->armor_class = (int16_t)number;
            break;
        case 4U:
            character->speed = (int16_t)number;
            break;
        case 5U:
        case 6U:
            character->initiative_misc = (int8_t)number;
            break;
        case 7U:
            character->exhaustion = (uint8_t)number;
            break;
        case 8U:
            character->death_successes = (uint8_t)number;
            break;
        case 9U:
            character->death_failures = (uint8_t)number;
            break;
        case 10U:
            character->hit_die = dndolphins_nearest_die(number, true);
            break;
        case 11U:
            character->hit_dice_current = (uint8_t)number;
            break;
        case 12U:
            character->hit_dice_max = (uint8_t)number;
            break;
        case 13U:
            character->skill_misc[11U] = (int8_t)number;
            break;
        case 14U:
            character->skill_misc[6U] = (int8_t)number;
            break;
        case 15U:
            character->skill_misc[8U] = (int8_t)number;
            break;
        }
        if(character->hp_current > character->hp_max) character->hp_current = character->hp_max;
        if(character->hit_dice_current > character->hit_dice_max)
            character->hit_dice_current = character->hit_dice_max;
        break;
    case PocketNumberAbility:
        if(app->number_index < POCKET_D20_ABILITY_COUNT) {
            if(app->number_aux)
                character->saving_throw_misc[app->number_index] = (int8_t)number;
            else
                character->ability_scores[app->number_index] = (int8_t)number;
        }
        break;
    case PocketNumberSkill:
        if(app->number_index < POCKET_D20_SKILL_COUNT)
            character->skill_misc[app->number_index] = (int8_t)number;
        break;
    case PocketNumberMagic:
        if(app->number_index == 3U)
            character->spell_attack_misc = (int8_t)number;
        else if(app->number_index == 4U)
            character->spell_save_misc = (int8_t)number;
        else if(app->number_index >= 7U && app->number_index <= 15U) {
            uint8_t level = app->number_index - 6U;
            uint8_t* slots = app->number_aux ? character->spell_slots_max :
                                               character->spell_slots_current;
            slots[level] = (uint8_t)number;
            if(character->spell_slots_current[level] > character->spell_slots_max[level])
                character->spell_slots_current[level] = character->spell_slots_max[level];
        }
        break;
    case PocketNumberRecord:
        if(app->record_index >= dndolphins_list_count(app)) break;
        if(app->list_kind == PocketListClasses) {
            uint8_t previous_total_level = dnd_rules_core_total_level(character);
            uint8_t previous_pb = dnd_rules_core_proficiency_bonus(character);
            uint8_t previous_slots[POCKET_D20_SLOT_COUNT];
            memcpy(previous_slots, character->spell_slots_max, sizeof(previous_slots));
            PocketClassLevel* level = &character->classes[app->record_index];
            uint8_t previous_class_level = level->level;
            uint8_t previous_cantrip_limit = level->cantrip_limit;
            uint8_t previous_prepared_limit = level->prepared_limit;
            switch(app->number_index) {
            case 2U:
                level->level = (uint8_t)number;
                break;
            case 3U:
                level->hit_die = dndolphins_nearest_die(number, true);
                break;
            case 4U:
                level->hit_dice_current = (uint8_t)number;
                break;
            case 5U:
                level->hit_dice_max = (uint8_t)number;
                break;
            case 8U:
                level->cantrip_limit = (uint8_t)number;
                break;
            case 9U:
                level->prepared_limit = (uint8_t)number;
                break;
            case 10U:
                level->spellbook_size = (uint16_t)number;
                break;
            case 11U:
                level->pact_slot_level = (uint8_t)number;
                break;
            case 12U:
                level->pact_slots_current = (uint8_t)number;
                break;
            case 13U:
                level->pact_slots_max = (uint8_t)number;
                break;
            case 15U:
                level->spell_points_current = (uint16_t)number;
                break;
            case 16U:
                level->spell_points_max = (uint16_t)number;
                break;
            }
            if(level->hit_dice_current > level->hit_dice_max)
                level->hit_dice_current = level->hit_dice_max;
            if(level->pact_slots_current > level->pact_slots_max)
                level->pact_slots_current = level->pact_slots_max;
            if(level->spell_points_current > level->spell_points_max)
                level->spell_points_current = level->spell_points_max;
            if(app->number_index == 2U) {
                uint8_t current_total_level = dnd_rules_core_total_level(character);
                if(current_total_level > previous_total_level)
                    dndolphins_rules_character_apply_level_increase(
                        character, app->record_index, previous_class_level);
                dndolphins_spells_apply_level_progression(character, app->record_index);
                if(current_total_level > previous_total_level) {
                    dndolphins_rules_character_apply_experience_floor(character);
                    dndolphins_begin_level_review(
                        app,
                        app->record_index,
                        previous_class_level,
                        previous_pb,
                        previous_cantrip_limit,
                        previous_prepared_limit,
                        previous_slots);
                }
            }
        } else if(app->list_kind == PocketListFeatures) {
            PocketFeature* feature = dndolphins_feature_at(app, app->record_index, NULL);
            if(!feature) return;
            if(app->number_index == 3U)
                feature->class_level_gained = (uint8_t)number;
            else if(app->number_index == 4U)
                feature->uses_current = (int16_t)number;
            else if(app->number_index == 5U)
                feature->uses_max = (int16_t)number;
            if(feature->uses_current > feature->uses_max)
                feature->uses_current = feature->uses_max;
            (void)dndolphins_save_features_if_changed(app);
        }
        break;
    case PocketNumberDice:
        if(app->number_index == 0U)
            app->dice_count = (uint8_t)number;
        else if(app->number_index == 1U)
            app->dice_sides = dndolphins_nearest_die(number, false);
        else if(app->number_index == 2U)
            app->dice_modifier = (int16_t)number;
        app->roll_mode = PocketRollNormal;
        app->dice_roll_value_count = 0U;
        break;
    case PocketNumberCombat:
        if(app->number_index == DndolphinsCombatHp)
            character->hp_current = (int16_t)number;
        else if(app->number_index == DndolphinsCombatTemporaryHp)
            character->hp_temporary = (int16_t)number;
        else if(app->number_index == DndolphinsCombatDeathSuccesses)
            character->death_successes = (uint8_t)number;
        else if(app->number_index == DndolphinsCombatDeathFailures)
            character->death_failures = (uint8_t)number;
        else if(app->number_index == DndolphinsCombatExhaustion)
            character->exhaustion = (uint8_t)number;
        break;
    case PocketNumberNone:
        break;
    }
    UNUSED(completed_context);
    app->number_context = PocketNumberNone;
    app->input_module_active = 0U;
    dndolphins_save(app, false);
    view_dispatcher_switch_to_view(app->dispatcher, PocketViewMain);
    dndolphins_refresh(app);
}

static void dndolphins_begin_number(
    PocketD20App* app,
    PocketNumberContext context,
    uint8_t index,
    uint8_t aux,
    const char* header,
    int32_t value,
    int32_t minimum,
    int32_t maximum) {
    dndolphins_release_text_input(app);
    if(!app->number_input) {
        app->number_input = number_input_alloc();
        if(!app->number_input) {
            dndolphins_set_status(app, "Number input memory low");
            return;
        }
        view_dispatcher_add_view(
            app->dispatcher, PocketViewNumberInput, number_input_get_view(app->number_input));
    }
    app->number_context = context;
    app->number_index = index;
    app->number_aux = aux;
    app->input_module_active = 1U;
    number_input_set_header_text(app->number_input, header);
    number_input_set_result_callback(
        app->number_input, dndolphins_number_done, app, value, minimum, maximum);
    view_dispatcher_switch_to_view(app->dispatcher, PocketViewNumberInput);
}

static uint16_t dndolphins_home_count(const PocketD20App* app) {
    return (uint16_t)DndolphinsHomeCount + (app->storage_unsaved ? 1U : 0U);
}

static const char* dndolphins_home_item_at(uint16_t index) {
    if(index < (uint16_t)DndolphinsHomeCount) return dndolphins_home_items[index];
    return dndolphins_home_retry_save;
}

static void dndolphins_draw_home(Canvas* canvas, PocketD20App* app) {
    char title[48];
    snprintf(title, sizeof(title), "D&D v" FAP_VERSION " - %.27s", app->data.character.name);
    dndolphins_draw_header(canvas, title, app->status);
    uint16_t count = dndolphins_home_count(app);
    for(uint8_t visible = 0U; visible < 5U; ++visible) {
        uint16_t index = app->scroll + visible;
        if(index >= count) break;
        dndolphins_draw_row(canvas, visible, index == app->selection, dndolphins_home_item_at(index));
    }
}

static void dndolphins_draw_profiles(Canvas* canvas, PocketD20App* app) {
    uint16_t count = dndolphins_profile_count(app);
    dndolphins_draw_header(canvas, "Characters - hold OK actions", app->status);
    for(uint8_t visible = 0U; visible < 5U; ++visible) {
        uint16_t index = app->scroll + visible;
        if(index > count) break;
        char row[48];
        if(index == count) {
            dndolphins_copy(row, sizeof(row), "+ New Character");
        } else {
            const PocketProfileEntry* entry = dndolphins_profile_entry_cached_at(app, index);
            if(entry) {
                snprintf(
                    row,
                    sizeof(row),
                    "%c #%lu L%u %.24s",
                    entry->id == app->profiles.active_profile ? '*' : ' ',
                    (unsigned long)entry->id,
                    entry->level,
                    entry->name[0] ? entry->name : "Unnamed");
            } else {
                dndolphins_copy(row, sizeof(row), "Character unavailable");
            }
        }
        dndolphins_draw_row(canvas, visible, index == app->selection, row);
    }
}

static void dndolphins_draw_profile_actions(Canvas* canvas, PocketD20App* app) {
    char title[48];
    snprintf(title, sizeof(title), "Character #%lu", (unsigned long)app->profile_action_id);
    dndolphins_draw_header(canvas, title, app->status);
    dndolphins_draw_menu_rows(
        canvas,
        app,
        dndolphins_profile_actions,
        sizeof(dndolphins_profile_actions) / sizeof(dndolphins_profile_actions[0]));
}

static void dndolphins_draw_character(Canvas* canvas, PocketD20App* app) {
    PocketCharacter* character = &app->data.character;
    char rows[15][48];
    const char* row_ptrs[15];
    for(uint8_t i = 0U; i < 15U; ++i)
        row_ptrs[i] = rows[i];

    snprintf(rows[0], sizeof(rows[0]), "Name: %.31s", character->name);
    snprintf(rows[1], sizeof(rows[1]), "Player: %.31s", character->player);
    snprintf(rows[2], sizeof(rows[2]), "Species: %.31s", character->species);
    snprintf(rows[3], sizeof(rows[3]), "Background: %.31s", character->background);
    snprintf(rows[4], sizeof(rows[4]), "Alignment: %.23s", character->alignment);
    snprintf(rows[5], sizeof(rows[5]), "Classes (%u)", character->class_count);
    snprintf(
        rows[6],
        sizeof(rows[6]),
        "Total L%u / PB +%u",
        dnd_rules_core_total_level(character),
        dnd_rules_core_proficiency_bonus(character));
    snprintf(rows[7], sizeof(rows[7]), "XP: %lu", (unsigned long)character->experience);
    snprintf(
        rows[8],
        sizeof(rows[8]),
        "Leveling: %s",
        character->milestone_leveling ? "Milestone" : "XP");
    snprintf(rows[9], sizeof(rows[9]), "Languages (%u)", character->language_count);
    snprintf(rows[10], sizeof(rows[10]), "Other proficiencies");
    snprintf(rows[11], sizeof(rows[11]), "Inspiration: %s", character->inspiration ? "Yes" : "No");
    snprintf(rows[12], sizeof(rows[12]), "Level Choices");
    snprintf(rows[13], sizeof(rows[13]), "Grant Initial Traits");
    snprintf(rows[14], sizeof(rows[14]), "Apply Level Grants");
    dndolphins_draw_header(canvas, "Character", app->status);
    dndolphins_draw_menu_rows(canvas, app, row_ptrs, 15U);
}

static void dndolphins_draw_vitals(Canvas* canvas, PocketD20App* app) {
    PocketCharacter* character = &app->data.character;
    char rows[16][40];
    const char* row_ptrs[16];
    for(uint8_t i = 0U; i < 16U; ++i)
        row_ptrs[i] = rows[i];

    snprintf(rows[0], sizeof(rows[0]), "Current HP: %d", character->hp_current);
    snprintf(rows[1], sizeof(rows[1]), "Maximum HP: %d", character->hp_max);
    snprintf(rows[2], sizeof(rows[2]), "Temporary HP: %d", character->hp_temporary);
    snprintf(rows[3], sizeof(rows[3]), "Armor Class: %d", character->armor_class);
    if(character->exhaustion)
        snprintf(
            rows[4],
            sizeof(rows[4]),
            "Speed: %d -> %d ft",
            character->speed,
            dndolphins_rules_character_effective_speed(character));
    else
        snprintf(rows[4], sizeof(rows[4]), "Speed: %d ft", character->speed);
    snprintf(
        rows[5], sizeof(rows[5]), "Initiative: %+d", dndolphins_rules_character_initiative_modifier(character));
    snprintf(rows[6], sizeof(rows[6]), "Initiative misc: %+d", character->initiative_misc);
    snprintf(rows[7], sizeof(rows[7]), "Exhaustion: %u", character->exhaustion);
    snprintf(rows[8], sizeof(rows[8]), "Death saves: %u/%u", character->death_successes, 3U);
    snprintf(rows[9], sizeof(rows[9]), "Death fails: %u/%u", character->death_failures, 3U);
    snprintf(rows[10], sizeof(rows[10]), "Hit die: d%u", character->hit_die);
    snprintf(rows[11], sizeof(rows[11]), "Hit dice current: %u", character->hit_dice_current);
    snprintf(rows[12], sizeof(rows[12]), "Hit dice maximum: %u", character->hit_dice_max);
    snprintf(
        rows[13],
        sizeof(rows[13]),
        "Pass. Perception: %d",
        10 + dnd_rules_core_skill_base_modifier(character, 11U));
    snprintf(
        rows[14],
        sizeof(rows[14]),
        "Pass. Insight: %d",
        10 + dnd_rules_core_skill_base_modifier(character, 6U));
    snprintf(
        rows[15],
        sizeof(rows[15]),
        "Pass. Invest.: %d",
        10 + dnd_rules_core_skill_base_modifier(character, 8U));
    dndolphins_draw_header(canvas, "Vitals - hold OK: number", app->status);
    dndolphins_draw_menu_rows(canvas, app, row_ptrs, 16U);
}

static void dndolphins_draw_abilities(Canvas* canvas, PocketD20App* app) {
    PocketCharacter* character = &app->data.character;
    char rows[POCKET_D20_ABILITY_COUNT][40];
    const char* row_ptrs[POCKET_D20_ABILITY_COUNT];
    for(uint8_t i = 0U; i < POCKET_D20_ABILITY_COUNT; ++i) {
        row_ptrs[i] = rows[i];
        if(app->edit_modifier_mode) {
            snprintf(
                rows[i],
                sizeof(rows[i]),
                "%s Save M%+d = %+d",
                dnd_rules_core_ability_names[i],
                character->saving_throw_misc[i],
                dnd_rules_core_saving_throw_modifier(character, i));
        } else {
            snprintf(
                rows[i],
                sizeof(rows[i]),
                "%s %d(%+d) %s Save %+d",
                dnd_rules_core_ability_names[i],
                character->ability_scores[i],
                dnd_rules_core_ability_modifier(character->ability_scores[i]),
                dndolphins_proficiency_mark(character->saving_throw_proficiency[i]),
                dnd_rules_core_saving_throw_modifier(character, i));
        }
    }
    dndolphins_draw_header(
        canvas,
        app->edit_modifier_mode ? "Saves: <> misc; hold OK #" : "Abilities: <> score; hold OK #",
        app->status);
    dndolphins_draw_menu_rows(canvas, app, row_ptrs, POCKET_D20_ABILITY_COUNT);
}

static void dndolphins_draw_skills(Canvas* canvas, PocketD20App* app) {
    PocketCharacter* character = &app->data.character;
    char title[32];
    snprintf(
        title,
        sizeof(title),
        "Skills PB+%u: <> %s",
        dnd_rules_core_proficiency_bonus(character),
        app->edit_modifier_mode ? "misc" : "prof");
    dndolphins_draw_header(canvas, title, app->status);
    for(uint8_t visible = 0U; visible < 5U; ++visible) {
        uint16_t display_index = app->scroll + visible;
        if(display_index >= POCKET_D20_SKILL_COUNT) break;
        uint8_t index = dndolphins_skill_display_order[display_index];
        uint8_t ability = dnd_rules_core_skill_abilities[index];
        char row[48];
        if(app->edit_modifier_mode) {
            snprintf(
                row,
                sizeof(row),
                "%s %s M%+d=%+d",
                dnd_rules_core_ability_names[ability],
                dnd_rules_core_skill_names[index],
                character->skill_misc[index],
                dnd_rules_core_skill_modifier(character, index));
        } else {
            snprintf(
                row,
                sizeof(row),
                "%s %s %s %+d",
                dnd_rules_core_ability_names[ability],
                dnd_rules_core_skill_names[index],
                dndolphins_proficiency_mark(character->skill_proficiency[index]),
                dnd_rules_core_skill_modifier(character, index));
        }
        dndolphins_draw_row(canvas, visible, display_index == app->selection, row);
    }
}


static void dndolphins_draw_grant_review(Canvas* canvas, PocketD20App* app) {
    const PocketCharacter* c = &app->data.character;
    dndolphins_draw_header(canvas, "Review grants before apply", app->status);
    for(uint8_t visible = 0U; visible < 5U; ++visible) {
        uint16_t row_index = app->scroll + visible;
        if(row_index > c->grant_count + 1U) break;
        char row[64];
        if(row_index == 0U) {
            dndolphins_copy(row, sizeof(row), "Apply all pending");
        } else if(row_index <= c->grant_count) {
            const PocketGrant* grant = &c->grants[row_index - 1U];
            char mark = grant->status == PocketGrantApplied ? 'A' :
                        grant->status == PocketGrantSkipped ? 'S' :
                                                              '?';
            snprintf(
                row, sizeof(row), "%c %.28s: %.28s", mark, grant->option_name, grant->grant_value);
        } else {
            dndolphins_copy(row, sizeof(row), "+ Add Custom Grant");
        }
        dndolphins_draw_row(canvas, visible, row_index == app->selection, row);
    }
}

static void dndolphins_draw_level_review(Canvas* canvas, PocketD20App* app) {
    const PocketCharacter* character = &app->data.character;
    const char* class_name = app->level_review_class_index < character->class_count ?
                                 character->classes[app->level_review_class_index].name : "Class";
    char title[48];
    snprintf(
        title,
        sizeof(title),
        "%.13s L%u -> L%u",
        class_name,
        app->level_review_old_level,
        app->level_review_new_level);
    dndolphins_draw_header(canvas, title, "OK: continue");

    char rows[5][48];
    snprintf(
        rows[0],
        sizeof(rows[0]),
        "Proficiency: +%u -> +%u",
        app->level_review_old_pb,
        app->level_review_new_pb);
    snprintf(
        rows[1],
        sizeof(rows[1]),
        "Spell limits C%u->%u P%u->%u",
        app->level_review_old_cantrips,
        app->level_review_new_cantrips,
        app->level_review_old_prepared,
        app->level_review_new_prepared);
    snprintf(rows[2], sizeof(rows[2]), "Spell slots: %s", app->level_review_slots_changed ? "updated" : "unchanged");
    snprintf(rows[3], sizeof(rows[3]), "HP/HD advanced; use Apply Level Grants");
    if(app->level_review_pending_choice && app->level_review_choose_spells)
        dndolphins_copy(rows[4], sizeof(rows[4]), "Pending: spells + ASI/Feat");
    else if(app->level_review_pending_choice)
        dndolphins_copy(rows[4], sizeof(rows[4]), "Pending: ASI/Feat choice");
    else if(app->level_review_choose_spells)
        dndolphins_copy(rows[4], sizeof(rows[4]), "Pending: choose spells");
    else
        dndolphins_copy(rows[4], sizeof(rows[4]), "Pending: none");
    for(uint8_t row = 0U; row < 5U; ++row)
        dndolphins_draw_row(canvas, row, false, rows[row]);
}

static void dndolphins_draw_level_choice(Canvas* canvas, PocketD20App* app) {
    if(!app->level_choice_level) {
        dndolphins_draw_header(canvas, "Level Choices", app->status);
        dndolphins_draw_row(canvas, 0U, true, "No pending choices");
        dndolphins_draw_row(canvas, 1U, false, "OK: Back");
        return;
    }
    char title[48];
    const PocketCharacter* c = &app->data.character;
    const char* class_name = app->level_choice_class_index < c->class_count ?
                                 c->classes[app->level_choice_class_index].name : "Class";
    snprintf(title, sizeof(title), "%.18s L%u choice", class_name, app->level_choice_level);
    dndolphins_draw_header(canvas, title, app->status);
    static const char* const rows[] = {"ASI +2 one ability", "ASI +1 two abilities", "Choose Feat", "Later"};
    dndolphins_draw_menu_rows(canvas, app, rows, 4U);
}

static void dndolphins_draw_asi_ability(Canvas* canvas, PocketD20App* app) {
    PocketCharacter* c = &app->data.character;
    char rows[POCKET_D20_ABILITY_COUNT][32];
    const char* ptrs[POCKET_D20_ABILITY_COUNT];
    for(uint8_t i = 0U; i < POCKET_D20_ABILITY_COUNT; ++i) {
        snprintf(rows[i], sizeof(rows[i]), "%s: %d", dnd_rules_core_ability_names[i], c->ability_scores[i]);
        ptrs[i] = rows[i];
    }
    dndolphins_draw_header(canvas, app->level_choice_mode == 1U ? "ASI +2: choose ability" :
                               app->level_choice_first_ability < POCKET_D20_ABILITY_COUNT ? "ASI +1: second ability" : "ASI +1: first ability", app->status);
    dndolphins_draw_menu_rows(canvas, app, ptrs, POCKET_D20_ABILITY_COUNT);
}

static void dndolphins_draw_grant_edit(Canvas* canvas, PocketD20App* app) {
    if(app->record_index >= app->data.character.grant_count) return;
    const PocketGrant* grant = &app->data.character.grants[app->record_index];
    char stable_id[48], source[48], source_type[32], option[48], prerequisites[48], class_name[40],
        level[24], payload[48], status[24];
    static const char* const sources[] = {
        "Species", "Background", "Feat", "Class Feature", "Subclass", "Item"};
    snprintf(stable_id, sizeof(stable_id), "Stable ID: %.36s", grant->stable_id);
    snprintf(source, sizeof(source), "Source: %.38s", grant->source);
    snprintf(source_type, sizeof(source_type), "Option type: %s", sources[grant->source_type]);
    snprintf(option, sizeof(option), "Option: %.38s", grant->option_name);
    snprintf(prerequisites, sizeof(prerequisites), "Requires: %.36s", grant->prerequisites);
    snprintf(
        class_name,
        sizeof(class_name),
        "Class: %s",
        grant->class_index < app->data.character.class_count ?
            app->data.character.classes[grant->class_index].name :
            "General");
    snprintf(level, sizeof(level), "Gained level: %u", grant->level_gained);
    snprintf(payload, sizeof(payload), "Payload: %.37s", grant->grant_value);
    snprintf(
        status,
        sizeof(status),
        "Status: %s",
        grant->status == PocketGrantApplied ? "Applied" :
        grant->status == PocketGrantSkipped ? "Skipped" :
                                              "Pending");
    const char* rows[] = {
        stable_id,
        source,
        source_type,
        option,
        prerequisites,
        class_name,
        level,
        payload,
        status,
        "Delete Grant"};
    dndolphins_draw_header(canvas, "Structured Grant Editor", app->status);
    dndolphins_draw_menu_rows(canvas, app, rows, 10U);
}





static void dndolphins_draw_attack_templates(Canvas* canvas, PocketD20App* app) {
    const PocketCharacter* c = &app->data.character;
    dndolphins_draw_header(canvas, "Attack Templates: OK roll", app->status);
    for(uint8_t visible = 0U; visible < 5U; ++visible) {
        uint16_t index = app->scroll + visible;
        char row[48];
        if(index == c->attack_template_count) {
            snprintf(row, sizeof(row), "+ New Attack Template");
        } else if(index < c->attack_template_count) {
            const PocketAttackTemplate* attack = &c->attack_templates[index];
            snprintf(
                row,
                sizeof(row),
                "%.30s %ud%u+%d",
                attack->name,
                attack->damage_dice,
                attack->damage_die,
                attack->attack_misc);
        } else
            break;
        dndolphins_draw_row(canvas, visible, index == app->selection, row);
    }
}

static void dndolphins_draw_attack_template_edit(Canvas* canvas, PocketD20App* app) {
    if(app->record_index >= app->data.character.attack_template_count) return;
    const PocketAttackTemplate* attack = &app->data.character.attack_templates[app->record_index];
    char type[32], ability[32], save[32], attack_misc[24], dc[24], damage_dice[24], damage_die[24],
        rider_dice[24], rider_die[24], recharge[32];
    snprintf(type, sizeof(type), "Type: %s", dndolphins_attack_template_type_names[attack->type]);
    snprintf(ability, sizeof(ability), "Ability: %s", dnd_rules_core_ability_names[attack->ability]);
    snprintf(save, sizeof(save), "Save: %s", dnd_rules_core_ability_names[attack->save_ability]);
    snprintf(attack_misc, sizeof(attack_misc), "Attack misc: %+d", attack->attack_misc);
    snprintf(dc, sizeof(dc), "Save DC: %u", attack->save_dc);
    snprintf(damage_dice, sizeof(damage_dice), "Damage dice: %u", attack->damage_dice);
    snprintf(damage_die, sizeof(damage_die), "Damage die: d%u", attack->damage_die);
    snprintf(rider_dice, sizeof(rider_dice), "Rider dice: %u", attack->rider_dice);
    snprintf(rider_die, sizeof(rider_die), "Rider die: d%u", attack->rider_die);
    snprintf(recharge, sizeof(recharge), "Recharge: %s", dndolphins_recharge_names[attack->recharge]);
    const char* rows[] = {
        attack->name,
        type,
        ability,
        save,
        attack_misc,
        dc,
        damage_dice,
        damage_die,
        attack->damage_type,
        attack->mastery,
        rider_dice,
        rider_die,
        attack->rider_type,
        recharge,
        "Delete Template"};
    dndolphins_draw_header(canvas, "Attack Template Editor", app->status);
    dndolphins_draw_menu_rows(canvas, app, rows, 15U);
}

static void dndolphins_draw_magic(Canvas* canvas, PocketD20App* app) {
    PocketCharacter* character = &app->data.character;
    char rows[16][48];
    const char* row_ptrs[16];
    for(uint8_t i = 0U; i < 16U; ++i)
        row_ptrs[i] = rows[i];
    snprintf(rows[0], sizeof(rows[0]), "Open Spellbook");
    snprintf(
        rows[1],
        sizeof(rows[1]),
        "Casting ability: %s",
        dnd_rules_core_ability_names[character->spellcasting_ability]);
    snprintf(
        rows[2],
        sizeof(rows[2]),
        "PB +%u Atk %+d DC %d (hold recalc)",
        dnd_rules_core_proficiency_bonus(character),
        dndolphins_spells_attack_modifier(character),
        dndolphins_spells_save_dc(character));
    snprintf(rows[3], sizeof(rows[3]), "Spell attack misc: %+d", character->spell_attack_misc);
    snprintf(rows[4], sizeof(rows[4]), "Spell save misc: %+d", character->spell_save_misc);
    snprintf(rows[5], sizeof(rows[5]), "Slots: <> avail / hold <> max");
    uint8_t wizard_level = dndolphins_wizard_level(character);
    if(app->arcane_recovery_active)
        snprintf(
            rows[6],
            sizeof(rows[6]),
            "Arcane Recovery: %u left",
            app->arcane_recovery_budget - app->arcane_recovery_spent);
    else if(!wizard_level)
        snprintf(rows[6], sizeof(rows[6]), "Arcane Recovery: no Wizard");
    else
        snprintf(
            rows[6],
            sizeof(rows[6]),
            "Arcane Recovery: %s",
            character->arcane_recovery_used ? "Used" : "Ready");
    for(uint8_t level = 1U; level <= 9U; ++level) {
        snprintf(
            rows[level + 6U],
            sizeof(rows[level + 6U]),
            "Level %u slots: %u/%u",
            level,
            character->spell_slots_current[level],
            character->spell_slots_max[level]);
    }
    dndolphins_draw_header(
        canvas, app->arcane_recovery_active ? "Magic: Arcane Recovery" : "Magic", app->status);
    dndolphins_draw_menu_rows(canvas, app, row_ptrs, 16U);
}


static void dndolphins_draw_catalog(Canvas* canvas, PocketD20App* app) {
    char page[24];
    uint16_t page_number = app->catalog_page_start / dndolphins_catalog_page_limit(app) + 1U;
    snprintf(page, sizeof(page), "Page %u%s <>", page_number, app->catalog_has_more ? "+" : "");
    dndolphins_draw_header(canvas, dndolphins_catalog_title(app), app->status[0] ? app->status : page);
    if(app->catalog_count == 0U) {
        dndolphins_draw_row(canvas, 0U, false, "Catalog is empty");
        return;
    }
    for(uint8_t visible = 0U; visible < 5U; ++visible) {
        uint16_t index = app->scroll + visible;
        if(index >= app->catalog_count) break;
        char row[48];
            dndolphins_copy(row, sizeof(row), app->catalog_entries[index]);
        dndolphins_draw_row(canvas, visible, index == app->selection, row);
    }
}

static void dndolphins_draw_record_list(Canvas* canvas, PocketD20App* app) {
    uint8_t count = dndolphins_list_count(app);
    dndolphins_draw_header(canvas, dndolphins_list_title(app->list_kind), app->status);
    for(uint8_t visible = 0U; visible < 5U; ++visible) {
        uint16_t index = app->scroll + visible;
        if(index >= (uint16_t)count + 1U) break;
        char row[64];
        if(index == 0U) {
            snprintf(row, sizeof(row), "+ Add New");
        } else {
            dndolphins_format_list_entry(app, (uint8_t)(index - 1U), row, sizeof(row));
        }
        if(app->action_ack_active && app->action_ack_screen == (uint8_t)app->screen &&
           app->action_ack_selection == index)
            dndolphins_prefix_action_mark(row, sizeof(row));
        dndolphins_draw_row(canvas, visible, index == app->selection, row);
    }
}

static void dndolphins_format_record_detail(PocketD20App* app, uint8_t field, char* output, size_t size) {
    PocketCharacter* character = &app->data.character;
    uint8_t index = app->record_index;
    if(app->list_kind == PocketListClasses) {
        const PocketClassLevel* c = &character->classes[index];
        if(field == 0U) dndolphins_format_labeled_text(output,size,"Name: ",c->name);
        else if(field == 1U) dndolphins_format_labeled_text(output,size,"Subclass: ",c->subclass);
        else if(field == 2U) snprintf(output,size,"Class level: %u",c->level);
        else if(field == 3U) snprintf(output,size,"Hit Point Die: d%u",c->hit_die);
        else if(field == 4U) snprintf(output,size,"Class Hit Dice: %u/%u",c->hit_dice_current,c->hit_dice_max);
        else if(field == 5U) snprintf(output,size,"Class Hit Dice max: %u",c->hit_dice_max);
        else if(field == 6U) dndolphins_format_labeled_text(output,size,"Casting mode: ",dndolphins_spellcasting_mode_names[c->spellcasting_mode]);
        else if(field == 7U) dndolphins_format_labeled_text(output,size,"Casting ability: ",dnd_rules_core_ability_names[c->spellcasting_ability]);
        else if(field == 8U) snprintf(output,size,"Cantrip limit: %u",c->cantrip_limit);
        else if(field == 9U) snprintf(output,size,"Known %u Prep %u/%u",dndolphins_class_known_count_cached(app,index),dndolphins_class_prepared_count_cached(app,index),c->prepared_limit);
        else if(field == 10U) snprintf(output,size,"Spellbook size: %u",c->spellbook_size);
        else if(field == 11U) snprintf(output,size,"Pact slot level: %u",c->pact_slot_level);
        else if(field == 12U) snprintf(output,size,"Pact slots: %u/%u",c->pact_slots_current,c->pact_slots_max);
        else if(field == 13U) snprintf(output,size,"Pact slots max: %u",c->pact_slots_max);
        else if(field == 14U) snprintf(output,size,"Mystic Arcanum: 0x%X",c->mystic_arcanum_mask);
        else if(field == 15U) snprintf(output,size,"Spell points: %u/%u",c->spell_points_current,c->spell_points_max);
        else if(field == 16U) snprintf(output,size,"Spell points max: %u",c->spell_points_max);
        else dndolphins_copy(output,size,"Delete class");
    } else if(app->list_kind == PocketListFeatures) {
        const PocketFeature* f = dndolphins_feature_at_cached(app,index,NULL);
        if(!f) { dndolphins_copy(output,size,"Read error"); return; }
        const char* cn = f->class_index < character->class_count ? character->classes[f->class_index].name : "General";
        if(field == 0U) dndolphins_format_labeled_text(output,size,"Name: ",f->name);
        else if(field == 1U) dndolphins_format_labeled_text(output,size,"Notes: ",f->detail);
        else if(field == 2U) dndolphins_format_labeled_text(output,size,"Source class: ",cn);
        else if(field == 3U) snprintf(output,size,"Gained at class L%u",f->class_level_gained);
        else if(field == 4U) snprintf(output,size,"Uses: %d/%d",f->uses_current,f->uses_max);
        else if(field == 5U) snprintf(output,size,"Maximum uses: %d",f->uses_max);
        else if(field == 6U) snprintf(output,size,"Recharge: %s",dndolphins_recharge_names[f->recharge]);
        else if(field == 7U) snprintf(output,size,"Resource formula: %s",dndolphins_resource_formula_names[f->resource_formula]);
        else if(field == 8U) snprintf(output,size,"Resource ability: %s",dnd_rules_core_ability_names[f->resource_ability]);
        else dndolphins_copy(output,size,"Delete feature");
    } else if(app->list_kind == PocketListLanguages) {
        if(field == 0U) dndolphins_format_labeled_text(output,size,"Language: ",character->languages[index]);
        else dndolphins_copy(output,size,"Delete language");
    } else dndolphins_copy(output,size,"Unavailable");
}

static void dndolphins_draw_record_detail(Canvas* canvas, PocketD20App* app) {
    uint8_t count = dndolphins_record_detail_count(app);
    dndolphins_draw_header(canvas, dndolphins_list_title(app->list_kind), app->status);
    for(uint8_t visible = 0U; visible < 5U; ++visible) {
        uint16_t field = app->scroll + visible;
        if(field >= count) break;
        char row[POCKET_D20_DETAIL_LEN + 32U];
        dndolphins_format_record_detail(app, (uint8_t)field, row, sizeof(row));
        dndolphins_draw_row(canvas, visible, field == app->selection, row);
    }
}

static void dndolphins_format_combat_row(
    PocketD20App* app,
    uint8_t index,
    char* row,
    size_t size) {
    PocketCharacter* character = &app->data.character;
    switch((DndolphinsCombatIndex)index) {
    case DndolphinsCombatAttackMode:
        snprintf(row, size, "Attack mode: %s", dndolphins_roll_mode_names[app->roll_mode]);
        break;
    case DndolphinsCombatWeaponAttacks:
        dndolphins_copy(row, size, "Weapon Attacks");
        break;
    case DndolphinsCombatSpellAttacks:
        dndolphins_copy(row, size, "Spell Attacks");
        break;
    case DndolphinsCombatRituals:
        dndolphins_copy(row, size, "Rituals");
        break;
    case DndolphinsCombatAttackTemplates:
        snprintf(row, size, "Attack Templates (%u)", character->attack_template_count);
        break;
    case DndolphinsCombatInitiative:
        dndolphins_copy(row, size, "Initiative Tracker");
        break;
    case DndolphinsCombatHp:
        snprintf(row, size, "HP: %d/%d", character->hp_current, character->hp_max);
        break;
    case DndolphinsCombatTemporaryHp:
        snprintf(row, size, "Temporary HP: %d", character->hp_temporary);
        break;
    case DndolphinsCombatShortRest:
        dndolphins_copy(row, size, "Short Rest");
        break;
    case DndolphinsCombatSpendHitDie:
        snprintf(
            row,
            size,
            "Spend %.22s d%u: %u/%u",
            character->classes[app->hit_die_class_index].name,
            character->classes[app->hit_die_class_index].hit_die,
            character->classes[app->hit_die_class_index].hit_dice_current,
            character->classes[app->hit_die_class_index].hit_dice_max);
        break;
    case DndolphinsCombatLongRest:
        dndolphins_copy(row, size, "Long Rest");
        break;
    case DndolphinsCombatConditions:
        snprintf(row, size, "Conditions: %.32s", character->conditions);
        break;
    case DndolphinsCombatConcentration:
        snprintf(
            row,
            size,
            "Concentration: %.31s",
            character->concentration[0] ? character->concentration : "None");
        break;
    case DndolphinsCombatReaction:
        snprintf(row, size, "Reaction: %s", character->reaction_available ? "Ready" : "Used");
        break;
    case DndolphinsCombatTemporaryEffects:
        snprintf(row, size, "Temp effects: %.30s", character->temporary_effects);
        break;
    case DndolphinsCombatResistances:
        snprintf(row, size, "Resist: %.35s", character->resistances);
        break;
    case DndolphinsCombatImmunities:
        snprintf(row, size, "Immune: %.35s", character->immunities);
        break;
    case DndolphinsCombatVulnerabilities:
        snprintf(row, size, "Vulnerable: %.31s", character->vulnerabilities);
        break;
    case DndolphinsCombatSenses:
        snprintf(row, size, "Senses: %.35s", character->senses);
        break;
    case DndolphinsCombatMovement:
        snprintf(row, size, "Movement: %.33s", character->movement_modes);
        break;
    case DndolphinsCombatDeathSuccesses:
        snprintf(row, size, "Death success: %u", character->death_successes);
        break;
    case DndolphinsCombatDeathFailures:
        snprintf(row, size, "Death failure: %u", character->death_failures);
        break;
    case DndolphinsCombatExhaustion:
        snprintf(row, size, "Exhaustion: %u", character->exhaustion);
        break;
    case DndolphinsCombatCount:
        dndolphins_copy(row, size, "Unavailable");
        break;
    }
}

static const char* dndolphins_combat_section(uint16_t index) {
    if(index <= DndolphinsCombatAttackTemplates) return "Combat: Attacks";
    if(index <= DndolphinsCombatInitiative) return "Combat: Encounter";
    if(index <= DndolphinsCombatLongRest) return "Combat: Recovery";
    if(index <= DndolphinsCombatTemporaryEffects) return "Combat: Status";
    return "Combat: Defenses";
}

static void dndolphins_draw_combat(Canvas* canvas, PocketD20App* app) {
    dndolphins_draw_header(canvas, dndolphins_combat_section(app->selection), app->status);
    for(uint8_t visible = 0U; visible < 5U; ++visible) {
        uint16_t index = app->scroll + visible;
        if(index >= DndolphinsCombatCount) break;
        char row[48];
        dndolphins_format_combat_row(app, (uint8_t)index, row, sizeof(row));
        dndolphins_draw_row(canvas, visible, index == app->selection, row);
    }
}

static void dndolphins_draw_dice(Canvas* canvas, PocketD20App* app) {
    char rows[5][48];
    const char* row_ptrs[5];
    for(uint8_t i = 0U; i < 5U; ++i)
        row_ptrs[i] = rows[i];
    snprintf(rows[0], sizeof(rows[0]), "Dice count: %u", app->dice_count);
    snprintf(rows[1], sizeof(rows[1]), "Die: d%u", app->dice_sides);
    snprintf(rows[2], sizeof(rows[2]), "Modifier: %+d", app->dice_modifier);
    snprintf(rows[3], sizeof(rows[3]), "Mode: %s", dndolphins_roll_mode_names[app->roll_mode]);
    if(app->dice_roll_value_count && app->dice_guidance) {
        snprintf(
            rows[4],
            sizeof(rows[4]),
            "Roll %u + Guidance %u = %d",
            app->dice_first,
            app->dice_guidance,
            app->dice_result);
    } else if(app->dice_roll_value_count && app->dice_second) {
        snprintf(
            rows[4],
            sizeof(rows[4]),
            "Roll %u/%u = %d",
            app->dice_first,
            app->dice_second,
            app->dice_result);
    } else if(app->dice_roll_value_count) {
        snprintf(rows[4], sizeof(rows[4]), "Roll result: %d", app->dice_result);
    } else {
        snprintf(rows[4], sizeof(rows[4]), "Roll now");
    }
    dndolphins_draw_header(canvas, "Dice Roller", app->status);
    dndolphins_draw_menu_rows(canvas, app, row_ptrs, 5U);
}

static void dndolphins_format_roll_row(
    char* output,
    size_t size,
    const uint8_t* values,
    uint8_t start,
    uint8_t count) {
    output[0] = '\0';
    size_t position = 0U;
    for(uint8_t i = 0U; i < count && start + i < POCKET_D20_MAX_GENERIC_ROLLS; ++i) {
        int written = snprintf(
            output + position,
            size - position,
            "%s%u",
            i ? ", " : "",
            (unsigned int)values[start + i]);
        if(written < 0 || (size_t)written >= size - position) break;
        position += (size_t)written;
    }
}

static void dndolphins_draw_dice_result(Canvas* canvas, PocketD20App* app) {
    char title[48];
    char rows[5][48];
    const char* row_ptrs[5];
    for(uint8_t i = 0U; i < 5U; ++i) {
        rows[i][0] = '\0';
        row_ptrs[i] = rows[i];
    }

    if(app->dice_guidance) {
        snprintf(title, sizeof(title), "Guidance d20+d4 - total %d", app->dice_result);
        snprintf(rows[0], sizeof(rows[0]), "d20: %u", app->dice_first);
        snprintf(rows[1], sizeof(rows[1]), "Guidance d4: %u", app->dice_guidance);
        snprintf(rows[2], sizeof(rows[2]), "Dice sum: %u", app->dice_roll_sum);
        snprintf(rows[3], sizeof(rows[3]), "Modifier: %+d", app->dice_modifier);
        snprintf(rows[4], sizeof(rows[4]), "Total: %d (OK reroll)", app->dice_result);
    } else if(app->dice_second) {
        uint8_t chosen =
            app->roll_mode == PocketRollAdvantage ?
                (app->dice_first > app->dice_second ? app->dice_first : app->dice_second) :
                (app->dice_first < app->dice_second ? app->dice_first : app->dice_second);
        snprintf(
            title,
            sizeof(title),
            "%s d20 - total %d",
            app->roll_mode == PocketRollAdvantage ? "Advantage" : "Disadvantage",
            app->dice_result);
        snprintf(rows[0], sizeof(rows[0]), "Rolls: %u, %u", app->dice_first, app->dice_second);
        snprintf(rows[1], sizeof(rows[1]), "Dice sum: %u", app->dice_roll_sum);
        snprintf(rows[2], sizeof(rows[2]), "Chosen: %u", chosen);
        snprintf(rows[3], sizeof(rows[3]), "Modifier: %+d", app->dice_modifier);
        snprintf(rows[4], sizeof(rows[4]), "Total: %d (OK reroll)", app->dice_result);
    } else if(app->dice_roll_value_count > 1U) {
        snprintf(
            title,
            sizeof(title),
            "%ud%u sum %u total %d",
            app->dice_roll_value_count,
            app->dice_sides,
            app->dice_roll_sum,
            app->dice_result);
        for(uint8_t row = 0U; row < 5U; ++row) {
            uint8_t start = row * 4U;
            if(start >= app->dice_roll_value_count) break;
            uint8_t remaining = app->dice_roll_value_count - start;
            dndolphins_format_roll_row(
                rows[row],
                sizeof(rows[row]),
                app->dice_roll_values,
                start,
                remaining > 4U ? 4U : remaining);
        }
    } else {
        snprintf(title, sizeof(title), "d%u total %d", app->dice_sides, app->dice_result);
        snprintf(rows[0], sizeof(rows[0]), "Roll: %u", app->dice_first);
        snprintf(rows[1], sizeof(rows[1]), "Dice sum: %u", app->dice_roll_sum);
        snprintf(rows[2], sizeof(rows[2]), "Modifier: %+d", app->dice_modifier);
        snprintf(rows[4], sizeof(rows[4]), "OK to reroll");
    }
    dndolphins_draw_header(canvas, title, app->status);
    for(uint8_t row = 0U; row < 5U; ++row) {
        if(rows[row][0]) dndolphins_draw_row(canvas, row, false, row_ptrs[row]);
    }
}


static uint8_t dndolphins_spell_casting_ability(PocketD20App* app, uint8_t logical_index) {
    PocketSpell* spell = dndolphins_spell_at(app, logical_index, NULL);
    return dndolphins_spells_casting_ability_for(&app->data.character, spell);
}

static int8_t dndolphins_spell_attack_modifier_for(PocketD20App* app, uint8_t logical_index) {
    PocketSpell* spell = dndolphins_spell_at(app, logical_index, NULL);
    return dndolphins_spells_attack_modifier_for(&app->data.character, spell);
}

static int8_t dndolphins_spell_attack_modifier_cached_for(
    PocketD20App* app, uint8_t logical_index) {
    PocketSpell* spell = dndolphins_spell_cached_at(app, logical_index, NULL);
    return dndolphins_spells_attack_modifier_for(&app->data.character, spell);
}

static int8_t dndolphins_spell_save_dc_cached_for(PocketD20App* app, uint8_t logical_index) {
    PocketSpell* spell = dndolphins_spell_cached_at(app, logical_index, NULL);
    return dndolphins_spells_save_dc_for(&app->data.character, spell);
}

static uint8_t dndolphins_build_spell_cast_options(
    PocketD20App* app,
    uint8_t logical_index,
    PocketSpellCastOption* options,
    uint8_t capacity) {
    uint8_t local = 0U;
    PocketSpell* spell = dndolphins_spell_at(app, logical_index, &local);
    if(!spell) return 0U;
    PocketCharacter* character = &app->data.character;
    return dndolphins_spells_build_cast_options(
        character,
        spell,
        character->spell_known[local],
        character->spell_always_prepared[local],
        character->spell_free_casts_current[local],
        options,
        capacity);
}

static uint8_t dndolphins_build_spell_cast_options_cached(
    PocketD20App* app,
    uint8_t logical_index,
    PocketSpellCastOption* options,
    uint8_t capacity) {
    uint8_t local = 0U;
    PocketSpell* spell = dndolphins_spell_cached_at(app, logical_index, &local);
    if(!spell) return 0U;
    PocketCharacter* character = &app->data.character;
    return dndolphins_spells_build_cast_options(
        character,
        spell,
        character->spell_known[local],
        character->spell_always_prepared[local],
        character->spell_free_casts_current[local],
        options,
        capacity);
}

static bool dndolphins_refresh_combat_spell_index(PocketD20App* app) {
    if(!app->combat_spell_indices) {
        app->combat_spell_indices = malloc(
            POCKET_D20_MAX_SPELLS * sizeof(*app->combat_spell_indices) +
            POCKET_D20_COMBAT_VISIBLE_ROWS * POCKET_D20_COMBAT_ROW_LEN);
        if(!app->combat_spell_indices) {
            dndolphins_set_status(app, "Combat memory full");
            return false;
        }
    }
    uint8_t total = 0U;
    if(!dndolphins_spells_collect_combat_indices(
           app->storage,
           app->profiles.active_profile,
           &app->data.character,
           app->combat_spell_indices,
           POCKET_D20_MAX_SPELLS,
           &app->combat_spell_count,
           &total)) {
        dndolphins_set_status(app, "Spellbook read failed");
        return false;
    }
    app->spellbook_total = total;
    return true;
}

static bool dndolphins_refresh_ritual_spell_index(PocketD20App* app) {
    if(!app->combat_spell_indices) {
        app->combat_spell_indices = malloc(
            POCKET_D20_MAX_SPELLS * sizeof(*app->combat_spell_indices) +
            POCKET_D20_COMBAT_VISIBLE_ROWS * POCKET_D20_COMBAT_ROW_LEN);
        if(!app->combat_spell_indices) {
            dndolphins_set_status(app, "Ritual memory full");
            return false;
        }
    }
    uint8_t total = 0U;
    if(!dndolphins_spells_collect_ritual_indices(
           app->storage,
           app->profiles.active_profile,
           &app->data.character,
           app->combat_spell_indices,
           POCKET_D20_MAX_SPELLS,
           &app->combat_spell_count,
           &total)) {
        dndolphins_set_status(app, "Spellbook read failed");
        return false;
    }
    app->spellbook_total = total;
    return true;
}

static uint8_t dndolphins_combat_spell_count(PocketD20App* app) {
    return app->combat_spell_count;
}

static uint8_t dndolphins_combat_spell_index(PocketD20App* app, uint16_t display_index) {
    if(!app->combat_spell_indices || display_index >= app->combat_spell_count) return 0xFFU;
    return app->combat_spell_indices[display_index];
}

static char* dndolphins_combat_spell_row(PocketD20App* app, uint8_t visible) {
    if(!app || !app->combat_spell_indices || visible >= POCKET_D20_COMBAT_VISIBLE_ROWS)
        return NULL;
    return (char*)(app->combat_spell_indices + POCKET_D20_MAX_SPELLS) +
           ((size_t)visible * POCKET_D20_COMBAT_ROW_LEN);
}

static void dndolphins_prepare_combat_spell_rows(PocketD20App* app, bool ritual_mode) {
    if(!app || !app->combat_spell_indices) return;
    for(uint8_t visible = 0U; visible < POCKET_D20_COMBAT_VISIBLE_ROWS; ++visible) {
        char* row = dndolphins_combat_spell_row(app, visible);
        if(!row) continue;
        row[0] = '\0';
        uint16_t display_index = app->scroll + visible;
        if(display_index >= app->combat_spell_count) continue;
        uint8_t spell_index = dndolphins_combat_spell_index(app, display_index);
        if(spell_index == 0xFFU) continue;
        PocketSpell* spell = dndolphins_spell_at(app, spell_index, NULL);
        if(!spell) continue;
        if(ritual_mode) {
            snprintf(row, POCKET_D20_COMBAT_ROW_LEN, "L%u %s", spell->level, spell->name);
            continue;
        }
        PocketSpellDamageSpec damage;
        uint8_t ability = dndolphins_spells_casting_ability_for(&app->data.character, spell);
        int8_t ability_modifier =
            dnd_rules_core_ability_modifier(app->data.character.ability_scores[ability]);
        bool has_damage = dndolphins_spell_combat_damage_spec(
            spell,
            spell->level,
            dnd_rules_core_total_level(&app->data.character),
            ability_modifier,
            &damage);
        char dice_suffix[24] = "";
        if(has_damage && damage.primary_dice && damage.primary_die) {
            if(damage.roll_instances > 1U && damage.flat_bonus)
                snprintf(
                    dice_suffix,
                    sizeof(dice_suffix),
                    " [%ux%ud%u%+d]",
                    damage.roll_instances,
                    damage.primary_dice,
                    damage.primary_die,
                    damage.flat_bonus);
            else if(damage.roll_instances > 1U)
                snprintf(
                    dice_suffix,
                    sizeof(dice_suffix),
                    " [%ux%ud%u]",
                    damage.roll_instances,
                    damage.primary_dice,
                    damage.primary_die);
            else if(damage.flat_bonus)
                snprintf(
                    dice_suffix,
                    sizeof(dice_suffix),
                    " [%ud%u%+d]",
                    damage.primary_dice,
                    damage.primary_die,
                    damage.flat_bonus);
            else
                snprintf(
                    dice_suffix,
                    sizeof(dice_suffix),
                    " [%ud%u]",
                    damage.primary_dice,
                    damage.primary_die);
        }
        if(spell->level)
            snprintf(
                row,
                POCKET_D20_COMBAT_ROW_LEN,
                "L%u %s%s%s",
                spell->level,
                spell->name,
                spell->ritual ? " [R]" : "",
                dice_suffix);
        else
            snprintf(
                row,
                POCKET_D20_COMBAT_ROW_LEN,
                "C %s%s",
                spell->name,
                dice_suffix);
    }
}

static const char* dndolphins_spell_cast_resource_name(uint8_t resource) {
    switch(resource) {
    case PocketSpellCastCantrip:
        return "Cantrip";
    case PocketSpellCastFree:
        return "Free cast";
    case PocketSpellCastSlot:
        return "Spell slot";
    case PocketSpellCastPact:
        return "Pact slot";
    case PocketSpellCastPoints:
        return "Spell points";
    case PocketSpellCastRitual:
        return "Ritual +10 min";
    default:
        return "Cast";
    }
}

static void dndolphins_format_spell_cast_option(
    PocketD20App* app,
    const PocketSpellCastOption* option,
    char* output,
    size_t size) {
    const PocketCharacter* character = &app->data.character;
    switch(option->resource) {
    case PocketSpellCastCantrip:
        snprintf(output, size, "Cast cantrip");
        break;
    case PocketSpellCastFree: {
        uint8_t local = 0U;
        uint8_t remaining = 0U;
        if(dndolphins_spell_cached_at(app, app->spell_attack_index, &local))
            remaining = character->spell_free_casts_current[local];
        snprintf(output, size, "Free cast L%u (%u left)", option->level, remaining);
        break;
    }
    case PocketSpellCastSlot:
        snprintf(
            output,
            size,
            "L%u slot (%u left)",
            option->level,
            character->spell_slots_current[option->level]);
        break;
    case PocketSpellCastPact:
        snprintf(
            output,
            size,
            "Pact L%u (%u left)",
            option->level,
            character->classes[option->class_index].pact_slots_current);
        break;
    case PocketSpellCastPoints:
        snprintf(
            output,
            size,
            "L%u points (%u cost)",
            option->level,
            dndolphins_spells_point_cost(option->level));
        break;
    case PocketSpellCastRitual:
        snprintf(output, size, "Ritual (+10 minutes)");
        break;
    default:
        output[0] = '\0';
        break;
    }
}

static bool dndolphins_consume_spell_cast_resource(
    PocketD20App* app,
    const PocketSpellCastOption* option) {
    PocketCharacter* character = &app->data.character;
    switch(option->resource) {
    case PocketSpellCastCantrip:
    case PocketSpellCastRitual:
        return true;
    case PocketSpellCastFree: {
        uint8_t local = 0U;
        if(!dndolphins_spell_at(app, app->spell_attack_index, &local) ||
           !character->spell_free_casts_current[local])
            return false;
        --character->spell_free_casts_current[local];
        return true;
    }
    case PocketSpellCastSlot:
        if(option->level >= POCKET_D20_SLOT_COUNT || !character->spell_slots_current[option->level])
            return false;
        --character->spell_slots_current[option->level];
        return true;
    case PocketSpellCastPact:
        if(option->class_index >= character->class_count ||
           !character->classes[option->class_index].pact_slots_current)
            return false;
        --character->classes[option->class_index].pact_slots_current;
        return true;
    case PocketSpellCastPoints: {
        if(option->class_index >= character->class_count) return false;
        uint8_t cost = dndolphins_spells_point_cost(option->level);
        if(!cost || character->classes[option->class_index].spell_points_current < cost) return false;
        character->classes[option->class_index].spell_points_current -= cost;
        return true;
    }
    default:
        return false;
    }
}

static int16_t dndolphins_roll_sorcerous_burst(
    uint8_t base_dice,
    int8_t spellcasting_modifier,
    uint8_t* rolled_dice) {
    uint8_t extra_limit = spellcasting_modifier > 0 ? (uint8_t)spellcasting_modifier : 0U;
    if(extra_limit > 10U) extra_limit = 10U;

    uint8_t pending = base_dice;
    uint8_t extras = 0U;
    uint8_t total_dice = 0U;
    int16_t total = 0;
    while(pending) {
        --pending;
        uint8_t value = (uint8_t)dnd_rules_core_roll_dice(1U, 8U);
        total += value;
        ++total_dice;
        if(value == 8U && extras < extra_limit) {
            ++extras;
            ++pending;
        }
    }
    if(rolled_dice) *rolled_dice = total_dice;
    return total;
}

static void dndolphins_cast_spell(PocketD20App* app, const PocketSpellCastOption* option) {
    PocketCharacter* character = &app->data.character;
    if(app->spell_attack_index >= app->spellbook_total ||
       !dndolphins_consume_spell_cast_resource(app, option)) {
        dndolphins_set_status(app, "Casting resource unavailable");
        return;
    }

    PocketSpell* spell = dndolphins_spell_at(app, app->spell_attack_index, NULL);
    if(!spell) {
        dndolphins_set_status(app, "Spell read failed");
        return;
    }
    app->spell_cast_level = option->level;
    app->spell_cast_resource = option->resource;
    app->spell_cast_class_index = option->class_index;
    app->spell_cast_primary_total = 0;
    app->spell_cast_secondary_total = 0;
    app->spell_cast_flat_bonus = 0;
    app->spell_cast_secondary_flat_bonus = 0;
    app->spell_cast_damage_total = 0;
    app->spell_cast_natural = 0U;
    app->spell_cast_attack_total = 0;
    app->spell_cast_resolution = PocketSpellResolutionNone;
    app->spell_cast_secondary_resolution = PocketSpellResolutionNone;
    app->spell_cast_secondary_relation = 0U;
    app->spell_cast_derived_effect = PocketSpellDerivedNone;
    app->spell_cast_from_notes = 0U;
    app->spell_cast_primary_dice = 0U;
    app->spell_cast_primary_die = 0U;
    app->spell_cast_secondary_dice = 0U;
    app->spell_cast_secondary_die = 0U;
    app->spell_cast_attack_roll_count = 0U;
    memset(app->spell_cast_attack_natural, 0, sizeof(app->spell_cast_attack_natural));
    memset(app->spell_cast_attack_damage, 0, sizeof(app->spell_cast_attack_damage));

    uint8_t ability = dndolphins_spell_casting_ability(app, app->spell_attack_index);
    int8_t ability_modifier = dnd_rules_core_ability_modifier(character->ability_scores[ability]);
    PocketSpellDamageSpec damage;
    if(dndolphins_spell_combat_damage_spec(
           spell,
           option->level,
           dnd_rules_core_total_level(character),
           ability_modifier,
           &damage)) {
        app->spell_cast_primary_dice = damage.primary_dice;
        app->spell_cast_primary_die = damage.primary_die;
        app->spell_cast_secondary_dice = damage.secondary_dice;
        app->spell_cast_secondary_die = damage.secondary_die;
        app->spell_cast_flat_bonus = damage.flat_bonus;
        app->spell_cast_secondary_flat_bonus = damage.secondary_flat_bonus;
        app->spell_cast_resolution = damage.resolution;
        app->spell_cast_secondary_resolution = damage.secondary_resolution;
        app->spell_cast_secondary_relation = damage.secondary_relation;
        app->spell_cast_derived_effect = damage.derived_effect;
        app->spell_cast_from_notes = damage.from_notes;

        if(damage.resolution == PocketSpellResolutionAttack) {
            uint8_t attack_count = damage.attack_rolls ? damage.attack_rolls : 1U;
            if(attack_count > POCKET_D20_MAX_SPELL_ATTACK_ROLLS)
                attack_count = POCKET_D20_MAX_SPELL_ATTACK_ROLLS;
            app->spell_cast_attack_roll_count = attack_count;
            int8_t attack_modifier =
                dndolphins_spell_attack_modifier_for(app, app->spell_attack_index);
            for(uint8_t index = 0U; index < attack_count; ++index) {
                int16_t primary = 0;
                int16_t secondary = 0;
                if(damage.primary_dice && damage.primary_die) {
                    if(damage.special == PocketSpellSpecialSorcerousBurst) {
                        uint8_t rolled_dice = damage.primary_dice;
                        primary = dndolphins_roll_sorcerous_burst(
                            damage.primary_dice, ability_modifier, &rolled_dice);
                        if(index == 0U) app->spell_cast_primary_dice = rolled_dice;
                    } else {
                        primary =
                            (int16_t)dnd_rules_core_roll_dice(damage.primary_dice, damage.primary_die);
                    }
                }
                if(!damage.secondary_relation && damage.secondary_dice && damage.secondary_die)
                    secondary =
                        (int16_t)dnd_rules_core_roll_dice(damage.secondary_dice, damage.secondary_die);
                int16_t attack_damage = primary + secondary + damage.flat_bonus;
                uint8_t natural = dndolphins_dice_roll_d20_mode(app->roll_mode);
                app->spell_cast_attack_natural[index] = natural;
                app->spell_cast_attack_damage[index] = attack_damage;
                app->spell_cast_damage_total += attack_damage;
                if(index == 0U) {
                    app->spell_cast_primary_total = primary;
                    app->spell_cast_secondary_total = secondary;
                    app->spell_cast_natural = natural;
                    app->spell_cast_attack_total = natural + attack_modifier;
                }
            }
            if(damage.secondary_relation && damage.secondary_dice && damage.secondary_die)
                app->spell_cast_secondary_total =
                    (int16_t)dnd_rules_core_roll_dice(damage.secondary_dice, damage.secondary_die);
        } else if(damage.roll_instances > 1U) {
            uint8_t roll_count = damage.roll_instances;
            if(roll_count > POCKET_D20_MAX_SPELL_ATTACK_ROLLS)
                roll_count = POCKET_D20_MAX_SPELL_ATTACK_ROLLS;
            app->spell_cast_attack_roll_count = roll_count;
            for(uint8_t index = 0U; index < roll_count; ++index) {
                int16_t primary = 0;
                if(damage.primary_dice && damage.primary_die)
                    primary =
                        (int16_t)dnd_rules_core_roll_dice(damage.primary_dice, damage.primary_die);
                int16_t value = primary + damage.flat_bonus;
                app->spell_cast_attack_damage[index] = value;
                app->spell_cast_damage_total += value;
                if(index == 0U) app->spell_cast_primary_total = primary;
            }
        } else {
            if(damage.primary_dice && damage.primary_die)
                app->spell_cast_primary_total =
                    (int16_t)dnd_rules_core_roll_dice(damage.primary_dice, damage.primary_die);
            if(damage.secondary_dice && damage.secondary_die)
                app->spell_cast_secondary_total =
                    (int16_t)dnd_rules_core_roll_dice(damage.secondary_dice, damage.secondary_die);
            app->spell_cast_damage_total = app->spell_cast_primary_total + damage.flat_bonus;
            if(!damage.secondary_relation)
                app->spell_cast_damage_total +=
                    app->spell_cast_secondary_total + damage.secondary_flat_bonus;
        }
    }

    if(option->resource == PocketSpellCastFree)
        (void)dndolphins_save_spellbook_if_changed(app);
    dndolphins_save(app, false);
    dndolphins_enter_screen(app, PocketScreenSpellResult);
}

static void dndolphins_draw_spell_attacks(Canvas* canvas, PocketD20App* app) {
    uint8_t count = dndolphins_combat_spell_count(app);
    char title[32];
    snprintf(title, sizeof(title), "Spells: %s", dndolphins_roll_mode_names[app->roll_mode]);
    dndolphins_draw_header(canvas, title, app->status);
    if(!count) {
        dndolphins_draw_row(canvas, 0U, false, "No combat spells");
        dndolphins_draw_row(canvas, 1U, false, "Prepare or add XdY notes");
        return;
    }
    for(uint8_t visible = 0U; visible < POCKET_D20_COMBAT_VISIBLE_ROWS; ++visible) {
        uint16_t display_index = app->scroll + visible;
        if(display_index >= count) break;
        const char* row = dndolphins_combat_spell_row(app, visible);
        dndolphins_draw_row(
            canvas,
            visible,
            display_index == app->selection,
            row && row[0] ? row : "Spell unavailable");
    }
}

static void dndolphins_draw_rituals(Canvas* canvas, PocketD20App* app) {
    uint8_t count = dndolphins_combat_spell_count(app);
    dndolphins_draw_header(canvas, "Rituals", app->status);
    if(!count) {
        dndolphins_draw_row(canvas, 0U, false, "No known Wizard rituals");
        dndolphins_draw_row(canvas, 1U, false, "Ritual Adept: spellbook");
        return;
    }
    for(uint8_t visible = 0U; visible < POCKET_D20_COMBAT_VISIBLE_ROWS; ++visible) {
        uint16_t display_index = app->scroll + visible;
        if(display_index >= count) break;
        const char* row = dndolphins_combat_spell_row(app, visible);
        dndolphins_draw_row(
            canvas,
            visible,
            display_index == app->selection,
            row && row[0] ? row : "Ritual unavailable");
    }
}

static void dndolphins_draw_spell_cast(Canvas* canvas, PocketD20App* app) {
    if(app->spell_attack_index >= app->spellbook_total) return;
    PocketSpellCastOption options[POCKET_D20_MAX_SPELL_CAST_OPTIONS];
    uint8_t count = dndolphins_build_spell_cast_options_cached(
        app, app->spell_attack_index, options, POCKET_D20_MAX_SPELL_CAST_OPTIONS);
    if(count > POCKET_D20_MAX_SPELL_CAST_OPTIONS) count = POCKET_D20_MAX_SPELL_CAST_OPTIONS;
    PocketSpell* spell = dndolphins_spell_cached_at(app, app->spell_attack_index, NULL);
    if(!spell) return;
    dndolphins_draw_header(canvas, spell->name, app->status);
    for(uint8_t visible = 0U; visible < 5U; ++visible) {
        uint16_t index = app->scroll + visible;
        if(index >= count) break;
        char row[64];
        dndolphins_format_spell_cast_option(app, &options[index], row, sizeof(row));
        dndolphins_draw_row(canvas, visible, index == app->selection, row);
    }
}

static const char* dndolphins_spell_resolution_label(uint8_t resolution) {
    switch(resolution) {
    case PocketSpellResolutionAttack:
        return "Attack";
    case PocketSpellResolutionSave:
        return "Save";
    case PocketSpellResolutionAutomatic:
        return "Auto";
    case PocketSpellResolutionTriggered:
        return "Trigger";
    case PocketSpellResolutionHealing:
        return "Heal";
    case PocketSpellResolutionTemporaryHP:
        return "Temp HP";
    case PocketSpellResolutionMitigation:
        return "Reduce";
    case PocketSpellResolutionTransfer:
        return "Self dmg";
    case PocketSpellResolutionVitality:
        return "HP + max";
    default:
        return "Roll";
    }
}

static const char* dndolphins_spell_secondary_relation_label(uint8_t relation) {
    switch(relation) {
    case PocketSpellSecondaryAlternative:
        return "Choose one effect";
    case PocketSpellSecondaryLater:
        return "Initial / later rolls";
    case PocketSpellSecondaryIndependent:
        return "Independent effect rolls";
    default:
        return "Separate effect rolls";
    }
}

static int16_t dndolphins_spell_component_total(int16_t dice_total, int16_t flat_bonus) {
    return (int16_t)(dice_total + flat_bonus);
}

static void dndolphins_draw_spell_result(Canvas* canvas, PocketD20App* app) {
    if(app->spell_attack_index >= app->spellbook_total) return;
    PocketSpell* spell = dndolphins_spell_cached_at(app, app->spell_attack_index, NULL);
    if(!spell) return;
    dndolphins_draw_header(canvas, spell->name, app->status);
    char row[64];

    if(app->spell_cast_resolution == PocketSpellResolutionAttack &&
       app->spell_cast_attack_roll_count > 1U) {
        const char* resource = "Cast";
        switch(app->spell_cast_resource) {
        case PocketSpellCastCantrip:
            resource = "Cantrip";
            break;
        case PocketSpellCastFree:
            resource = "Free";
            break;
        case PocketSpellCastSlot:
            resource = "Slot";
            break;
        case PocketSpellCastPact:
            resource = "Pact";
            break;
        case PocketSpellCastPoints:
            resource = "Points";
            break;
        case PocketSpellCastRitual:
            resource = "Ritual";
            break;
        default:
            break;
        }
        if(app->spell_cast_level)
            snprintf(
                row,
                sizeof(row),
                "L%u %s | %u attacks",
                app->spell_cast_level,
                resource,
                app->spell_cast_attack_roll_count);
        else
            snprintf(
                row,
                sizeof(row),
                "%s | %u attacks",
                resource,
                app->spell_cast_attack_roll_count);
        dndolphins_draw_row(canvas, 0U, false, row);

        int8_t attack_modifier =
            dndolphins_spell_attack_modifier_cached_for(app, app->spell_attack_index);
        for(uint8_t visible = 0U; visible < 4U; ++visible) {
            uint8_t index = (uint8_t)(app->scroll + visible);
            if(index >= app->spell_cast_attack_roll_count) break;
            int16_t attack_total = app->spell_cast_attack_natural[index] + attack_modifier;
            snprintf(
                row,
                sizeof(row),
                "%u) %u%+d=%d D%d",
                index + 1U,
                app->spell_cast_attack_natural[index],
                attack_modifier,
                attack_total,
                app->spell_cast_attack_damage[index]);
            dndolphins_draw_row(canvas, visible + 1U, false, row);
        }
        return;
    }

    if(app->spell_cast_resolution != PocketSpellResolutionAttack &&
       app->spell_cast_attack_roll_count > 1U) {
        const char* resource = dndolphins_spell_cast_resource_name(app->spell_cast_resource);
        if(app->spell_cast_level)
            snprintf(
                row,
                sizeof(row),
                "L%u %s | %u rolls",
                app->spell_cast_level,
                resource,
                app->spell_cast_attack_roll_count);
        else
            snprintf(row, sizeof(row), "%s | %u rolls", resource, app->spell_cast_attack_roll_count);
        dndolphins_draw_row(canvas, 0U, false, row);
        for(uint8_t visible = 0U; visible < 4U; ++visible) {
            uint8_t index = (uint8_t)(app->scroll + visible);
            if(index >= app->spell_cast_attack_roll_count) break;
            snprintf(
                row,
                sizeof(row),
                "%u) %ud%u%+d = %d",
                index + 1U,
                app->spell_cast_primary_dice,
                app->spell_cast_primary_die,
                app->spell_cast_flat_bonus,
                app->spell_cast_attack_damage[index]);
            dndolphins_draw_row(canvas, visible + 1U, false, row);
        }
        return;
    }

    if(app->spell_cast_resource == PocketSpellCastRitual)
        snprintf(row, sizeof(row), "Ritual cast: +10 minutes");
    else if(app->spell_cast_level)
        snprintf(
            row,
            sizeof(row),
            "Cast L%u - %s",
            app->spell_cast_level,
            dndolphins_spell_cast_resource_name(app->spell_cast_resource));
    else
        snprintf(row, sizeof(row), "Cantrip cast");
    dndolphins_draw_row(canvas, 0U, false, row);

    if(app->spell_cast_resolution == PocketSpellResolutionAttack) {
        snprintf(
            row,
            sizeof(row),
            "Attack d20 %u %+d = %d",
            app->spell_cast_natural,
            dndolphins_spell_attack_modifier_cached_for(app, app->spell_attack_index),
            app->spell_cast_attack_total);
    } else if(app->spell_cast_resolution == PocketSpellResolutionSave) {
        snprintf(
            row,
            sizeof(row),
            "Target save DC %d",
            dndolphins_spell_save_dc_cached_for(app, app->spell_attack_index));
    } else if(app->spell_cast_resolution == PocketSpellResolutionAutomatic) {
        snprintf(row, sizeof(row), "Automatic / no attack roll");
    } else if(app->spell_cast_resolution == PocketSpellResolutionTriggered) {
        snprintf(row, sizeof(row), "Damage after trigger/hit");
    } else if(app->spell_cast_resolution == PocketSpellResolutionHealing) {
        snprintf(row, sizeof(row), "Healing roll");
    } else if(app->spell_cast_resolution == PocketSpellResolutionTemporaryHP) {
        snprintf(row, sizeof(row), "Temporary HP roll");
    } else if(app->spell_cast_resolution == PocketSpellResolutionMitigation) {
        snprintf(row, sizeof(row), "Damage reduction roll");
    } else if(app->spell_cast_resolution == PocketSpellResolutionTransfer) {
        snprintf(row, sizeof(row), "Self damage; target heals x2");
    } else if(app->spell_cast_resolution == PocketSpellResolutionVitality) {
        snprintf(row, sizeof(row), "HP and HP maximum increase");
    } else {
        snprintf(
            row,
            sizeof(row),
            "Spell +%d / DC %d",
            dndolphins_spell_attack_modifier_cached_for(app, app->spell_attack_index),
            dndolphins_spell_save_dc_cached_for(app, app->spell_attack_index));
    }
    dndolphins_draw_row(canvas, 1U, false, row);

    if(!app->spell_cast_primary_dice && !app->spell_cast_secondary_dice &&
       !app->spell_cast_flat_bonus && !app->spell_cast_secondary_flat_bonus) {
        dndolphins_draw_row(canvas, 2U, false, "No mapped combat roll");
        dndolphins_draw_row(canvas, 3U, false, "Cast/resource recorded");
    } else if(app->spell_cast_secondary_relation) {
        int16_t primary_total = dndolphins_spell_component_total(
            app->spell_cast_primary_total, app->spell_cast_flat_bonus);
        int16_t secondary_total = dndolphins_spell_component_total(
            app->spell_cast_secondary_total, app->spell_cast_secondary_flat_bonus);
        if(app->spell_cast_primary_dice)
            snprintf(
                row,
                sizeof(row),
                "%s %ud%u%+d=%d",
                dndolphins_spell_resolution_label(app->spell_cast_resolution),
                app->spell_cast_primary_dice,
                app->spell_cast_primary_die,
                app->spell_cast_flat_bonus,
                primary_total);
        else
            snprintf(
                row,
                sizeof(row),
                "%s %+d",
                dndolphins_spell_resolution_label(app->spell_cast_resolution),
                app->spell_cast_flat_bonus);
        dndolphins_draw_row(canvas, 2U, false, row);

        if(app->spell_cast_secondary_dice)
            snprintf(
                row,
                sizeof(row),
                "%s %ud%u%+d=%d",
                dndolphins_spell_resolution_label(app->spell_cast_secondary_resolution),
                app->spell_cast_secondary_dice,
                app->spell_cast_secondary_die,
                app->spell_cast_secondary_flat_bonus,
                secondary_total);
        else
            snprintf(
                row,
                sizeof(row),
                "%s %+d",
                dndolphins_spell_resolution_label(app->spell_cast_secondary_resolution),
                app->spell_cast_secondary_flat_bonus);
        dndolphins_draw_row(canvas, 3U, false, row);
        dndolphins_draw_row(
            canvas, 4U, false, dndolphins_spell_secondary_relation_label(app->spell_cast_secondary_relation));
    } else {
        if(app->spell_cast_primary_dice) {
            snprintf(
                row,
                sizeof(row),
                app->spell_cast_from_notes ? "Notes %ud%u = %d" : "%ud%u = %d",
                app->spell_cast_primary_dice,
                app->spell_cast_primary_die,
                app->spell_cast_primary_total);
            dndolphins_draw_row(canvas, 2U, false, row);
        }
        if(app->spell_cast_secondary_dice) {
            snprintf(
                row,
                sizeof(row),
                "+ %ud%u = %d",
                app->spell_cast_secondary_dice,
                app->spell_cast_secondary_die,
                app->spell_cast_secondary_total);
            dndolphins_draw_row(canvas, 3U, false, row);
        } else if(app->spell_cast_flat_bonus) {
            snprintf(row, sizeof(row), "Modifier: %+d", app->spell_cast_flat_bonus);
            dndolphins_draw_row(canvas, 3U, false, row);
        }

        if(app->spell_cast_derived_effect == PocketSpellDerivedHealHalfPrimary) {
            snprintf(row, sizeof(row), "Heal: %d", app->spell_cast_damage_total / 2);
        } else if(app->spell_cast_derived_effect == PocketSpellDerivedHealDoublePrimary) {
            snprintf(row, sizeof(row), "Heal target: %d", app->spell_cast_damage_total * 2);
        } else if(app->spell_cast_resolution == PocketSpellResolutionHealing) {
            snprintf(row, sizeof(row), "Healing total: %d", app->spell_cast_damage_total);
        } else if(app->spell_cast_resolution == PocketSpellResolutionTemporaryHP) {
            snprintf(row, sizeof(row), "Temp HP: %d", app->spell_cast_damage_total);
        } else if(app->spell_cast_resolution == PocketSpellResolutionMitigation) {
            snprintf(row, sizeof(row), "Reduce by: %d", app->spell_cast_damage_total);
        } else if(app->spell_cast_resolution == PocketSpellResolutionVitality) {
            snprintf(row, sizeof(row), "HP & max: +%d", app->spell_cast_damage_total);
        } else {
            snprintf(row, sizeof(row), "Damage total: %d", app->spell_cast_damage_total);
        }
        dndolphins_draw_row(canvas, 4U, false, row);
    }

}

static void dndolphins_draw_attack_list(Canvas* canvas, PocketD20App* app) {
    uint8_t count = dndolphins_weapon_count(app);
    char title[32];
    snprintf(title, sizeof(title), "Attacks: %s", dndolphins_roll_mode_names[app->roll_mode]);
    dndolphins_draw_header(canvas, title, app->status);
    if(count == 0U) {
        dndolphins_draw_row(canvas, 0U, false, "No weapon items");
        dndolphins_draw_row(canvas, 1U, false, "Add one in Inventory");
        return;
    }
    for(uint8_t visible = 0U; visible < POCKET_D20_COMBAT_VISIBLE_ROWS; ++visible) {
        uint16_t weapon_number = app->scroll + visible;
        if(weapon_number >= count) break;
        const char* row = dndolphins_combat_weapon_row(app, visible);
        dndolphins_draw_row(
            canvas,
            visible,
            weapon_number == app->selection,
            row && row[0] ? row : "Weapon unavailable");
    }
}

static void dndolphins_draw_attack_result(Canvas* canvas, PocketD20App* app) {
    PocketItem* item = dndolphins_item_cached_at(app, app->attack_item_index, NULL);
    if(!item) return;
    dndolphins_draw_header(canvas, item->name, app->status);
    char row[64];
    if(app->attack_phase == 0U) {
        if(app->attack_roll.second_die) {
            snprintf(
                row,
                sizeof(row),
                "d20: %u / %u",
                app->attack_roll.first_die,
                app->attack_roll.second_die);
        } else {
            snprintf(row, sizeof(row), "d20: %u", app->attack_roll.first_die);
        }
        dndolphins_draw_row(canvas, 0U, false, row);
        uint8_t detail_row = 1U;
        if(app->attack_roll.second_die) {
            snprintf(
                row,
                sizeof(row),
                "Dice sum: %u",
                app->attack_roll.first_die + app->attack_roll.second_die);
            dndolphins_draw_row(canvas, detail_row++, false, row);
        }
        snprintf(row, sizeof(row), "Modifier: %+d", app->attack_roll.modifier);
        dndolphins_draw_row(canvas, detail_row++, false, row);
        snprintf(row, sizeof(row), "Attack total: %d", app->attack_roll.total);
        dndolphins_draw_row(canvas, detail_row++, false, row);
        if(app->attack_roll.critical)
            snprintf(row, sizeof(row), "Critical! OK damage");
        else if(app->attack_roll.automatic_miss)
            snprintf(row, sizeof(row), "Natural 1 - miss");
        else
            snprintf(row, sizeof(row), "OK damage; Right crit");
        dndolphins_draw_row(canvas, detail_row, false, row);
        if(detail_row < 4U) dndolphins_draw_row(canvas, 4U, false, "Up: reroll attack");
    } else {
        uint8_t roll_count =
            app->damage_roll.weapon_roll_count + app->damage_roll.extra_roll_count;
        if(roll_count > 1U) {
            uint8_t page_count = (roll_count + 15U) / 16U;
            if(app->damage_roll_page >= page_count) app->damage_roll_page = page_count - 1U;
            char damage_title[48];
            snprintf(
                damage_title,
                sizeof(damage_title),
                "Damage %u/%u - total %d",
                app->damage_roll_page + 1U,
                page_count,
                app->damage_roll.total);
            dndolphins_draw_header(canvas, damage_title, app->status);
            uint8_t start = app->damage_roll_page * 16U;
            for(uint8_t display_row = 0U; display_row < 4U; ++display_row) {
                uint8_t first = start + (display_row * 4U);
                if(first >= roll_count) break;
                char values[64] = "";
                size_t position = 0U;
                for(uint8_t i = 0U; i < 4U && first + i < roll_count; ++i) {
                    uint8_t roll_index = first + i;
                    int written = snprintf(
                        values + position,
                        sizeof(values) - position,
                        "%s%c%u",
                        i ? " " : "",
                        roll_index < app->damage_roll.weapon_roll_count ? 'W' : 'E',
                        app->damage_roll.rolls[roll_index]);
                    if(written < 0 || (size_t)written >= sizeof(values) - position) break;
                    position += (size_t)written;
                }
                dndolphins_draw_row(canvas, display_row, false, values);
            }
            snprintf(
                row,
                sizeof(row),
                "Sum %d %+d = %d",
                app->damage_roll.weapon_total + app->damage_roll.extra_total,
                app->damage_roll.modifier,
                app->damage_roll.total);
            dndolphins_draw_row(canvas, 4U, false, row);
        } else {
            snprintf(
                row, sizeof(row), "%s damage", app->damage_roll.critical ? "Critical" : "Normal");
            dndolphins_draw_row(canvas, 0U, false, row);
            snprintf(row, sizeof(row), "Weapon dice: %d", app->damage_roll.weapon_total);
            dndolphins_draw_row(canvas, 1U, false, row);
            snprintf(row, sizeof(row), "Extra dice: %d", app->damage_roll.extra_total);
            dndolphins_draw_row(canvas, 2U, false, row);
            snprintf(row, sizeof(row), "Modifier: %+d", app->damage_roll.modifier);
            dndolphins_draw_row(canvas, 3U, false, row);
            snprintf(row, sizeof(row), "Total: %d (OK reroll)", app->damage_roll.total);
            dndolphins_draw_row(canvas, 4U, false, row);
        }
    }
}

static void dndolphins_draw_animated_die(
    Canvas* canvas,
    int32_t center_x,
    int32_t center_y,
    uint8_t frame,
    uint8_t face) {
    if(frame & 1U) {
        canvas_draw_line(canvas, center_x, center_y - 14, center_x + 14, center_y);
        canvas_draw_line(canvas, center_x + 14, center_y, center_x, center_y + 14);
        canvas_draw_line(canvas, center_x, center_y + 14, center_x - 14, center_y);
        canvas_draw_line(canvas, center_x - 14, center_y, center_x, center_y - 14);
    } else {
        int8_t offset = (frame & 2U) ? 2 : 0;
        canvas_draw_rframe(canvas, center_x - 14 + offset, center_y - 14, 28, 28, 4);
    }
    char value[5];
    snprintf(value, sizeof(value), "%u", face);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, center_x, center_y + 1, AlignCenter, AlignCenter, value);
}

static void dndolphins_draw_dice_animation(Canvas* canvas, PocketD20App* app) {
    char title[40];
    snprintf(title, sizeof(title), "Rolling %ud%u...", app->dice_anim_count, app->dice_anim_sides);
    dndolphins_draw_header(canvas, title, NULL);
    uint8_t visible_dice = app->dice_anim_count > 1U ? 3U : 1U;
    for(uint8_t i = 0U; i < visible_dice; ++i) {
        int32_t x = visible_dice == 1U ? 64 : 22 + (i * 42);
        uint8_t face = (uint8_t)(((app->dice_anim_frame * 7U) + (i * 5U) + app->dice_anim_sides) %
                                 app->dice_anim_sides) +
                       1U;
        dndolphins_draw_animated_die(canvas, x, 34, app->dice_anim_frame + i, face);
    }
    canvas_draw_frame(canvas, 14, 55, 100, 5);
    uint8_t progress =
        (uint8_t)(((app->dice_anim_frame + 1U) * 98U) / POCKET_D20_DICE_ANIMATION_FRAMES);
    canvas_draw_box(canvas, 15, 56, progress, 3);
}

static void dndolphins_draw_callback(Canvas* canvas, void* model) {
    PocketD20App* app = *(PocketD20App**)model;
    canvas_clear(canvas);
    if(app->dice_animating) {
        dndolphins_draw_dice_animation(canvas, app);
        return;
    }
    switch(app->screen) {
    case PocketScreenHome:
        dndolphins_draw_home(canvas, app);
        break;
    case PocketScreenProfiles:
        dndolphins_draw_profiles(canvas, app);
        break;
    case PocketScreenProfileActions:
        dndolphins_draw_profile_actions(canvas, app);
        break;
    case PocketScreenCharacter:
        dndolphins_draw_character(canvas, app);
        break;
    case PocketScreenVitals:
        dndolphins_draw_vitals(canvas, app);
        break;
    case PocketScreenAbilities:
        dndolphins_draw_abilities(canvas, app);
        break;
    case PocketScreenSkills:
        dndolphins_draw_skills(canvas, app);
        break;
    case PocketScreenGrantReview:
        dndolphins_draw_grant_review(canvas, app);
        break;
    case PocketScreenGrantEdit:
        dndolphins_draw_grant_edit(canvas, app);
        break;
    case PocketScreenLevelReview:
        dndolphins_draw_level_review(canvas, app);
        break;
    case PocketScreenLevelChoice:
        dndolphins_draw_level_choice(canvas, app);
        break;
    case PocketScreenAsiAbility:
        dndolphins_draw_asi_ability(canvas, app);
        break;
    case PocketScreenMagic:
        dndolphins_draw_magic(canvas, app);
        break;
    case PocketScreenRecordList:
        dndolphins_draw_record_list(canvas, app);
        break;
    case PocketScreenRecordDetail:
        dndolphins_draw_record_detail(canvas, app);
        break;
    case PocketScreenCatalog:
        dndolphins_draw_catalog(canvas, app);
        break;
    case PocketScreenCombat:
        dndolphins_draw_combat(canvas, app);
        break;
    case PocketScreenSpellAttacks:
        dndolphins_draw_spell_attacks(canvas, app);
        break;
    case PocketScreenRituals:
        dndolphins_draw_rituals(canvas, app);
        break;
    case PocketScreenSpellCast:
        dndolphins_draw_spell_cast(canvas, app);
        break;
    case PocketScreenSpellResult:
        dndolphins_draw_spell_result(canvas, app);
        break;
    case PocketScreenAttackTemplates:
        dndolphins_draw_attack_templates(canvas, app);
        break;
    case PocketScreenAttackTemplateEdit:
        dndolphins_draw_attack_template_edit(canvas, app);
        break;
    case PocketScreenDice:
        dndolphins_draw_dice(canvas, app);
        break;
    case PocketScreenDiceResult:
        dndolphins_draw_dice_result(canvas, app);
        break;
    case PocketScreenAttackList:
        dndolphins_draw_attack_list(canvas, app);
        break;
    case PocketScreenAttackResult:
        dndolphins_draw_attack_result(canvas, app);
        break;
    default:
        break;
    }
}

static void dndolphins_open_list(PocketD20App* app, PocketListKind kind, PocketScreen return_screen) {
    dndolphins_release_text_input(app);
    dndolphins_release_number_input(app);
    app->list_kind = kind;
    app->record_list_return_screen = return_screen;
    dndolphins_enter_screen(app, PocketScreenRecordList);
}

/* The proven pre-sidecar implementation grew the resident collection before
   saving. Preserve that lifecycle with bounded paging by first making the real
   tail page resident, then growing that page and committing it immediately. */


static bool dndolphins_feature_prepare_append_page(PocketD20App* app) {
    if(!app->features_loaded && !dndolphins_load_features(app)) return false;
    if(app->features_total >= POCKET_D20_MAX_FEATURES) return false;

    const uint8_t target_start = (uint8_t)(
        (app->features_total / DND_PROGRESS_CACHE_SIZE) * DND_PROGRESS_CACHE_SIZE);
    if(app->features_cache_start != target_start) {
        if(!dndolphins_save_features_if_changed(app)) return false;
        dnd_data_reserve_features_exact(&app->data.character, 0U);
        app->data.character.feature_count = 0U;
        app->features_loaded = 0U;
        if(!dndolphins_load_features_page(app, target_start)) return false;
    }

    PocketCharacter* character = &app->data.character;
    const uint8_t expected = (uint8_t)(app->features_total - target_start);
    if(character->feature_count != expected || character->feature_count >= DND_PROGRESS_CACHE_SIZE)
        return false;
    return dnd_data_reserve_features(character, (uint8_t)(character->feature_count + 1U));
}

static bool dndolphins_add_record(PocketD20App* app) {
    dndolphins_release_text_input(app);
    dndolphins_release_number_input(app);
    PocketCharacter* character = &app->data.character;
    switch(app->list_kind) {
    case PocketListClasses:
        if(character->class_count >= POCKET_D20_MAX_CLASSES ||
           dnd_rules_core_total_level(character) >= 20U)
            return false;
        app->record_index = character->class_count++;
        memset(&character->classes[app->record_index], 0, sizeof(PocketClassLevel));
        dndolphins_copy(
            character->classes[app->record_index].name,
            sizeof(character->classes[app->record_index].name),
            "New Class");
        dndolphins_copy(
            character->classes[app->record_index].subclass,
            sizeof(character->classes[app->record_index].subclass),
            "None");
        character->classes[app->record_index].level = 1U;
        character->classes[app->record_index].hit_die = 8U;
        character->classes[app->record_index].hit_dice_current = 1U;
        character->classes[app->record_index].hit_dice_max = 1U;
        character->classes[app->record_index].spellcasting_ability = PocketAbilityIntelligence;
        break;
    case PocketListFeatures: {
        if(!dndolphins_feature_prepare_append_page(app)) return false;
        const uint8_t local = character->feature_count;
        PocketFeature* feature = &character->features[local];
        memset(feature, 0, sizeof(*feature));
        dndolphins_copy(feature->name, sizeof(feature->name), "New Feature");
        feature->class_index = 0U;
        feature->class_level_gained = character->classes[0].level;
        ++character->feature_count;
        app->record_index = app->features_total++;
        (void)dndolphins_save_features_if_changed(app);
        break;
    }
    case PocketListLanguages:
        if(character->language_count >= POCKET_D20_MAX_LANGUAGES) return false;
        app->record_index = character->language_count++;
        memset(character->languages[app->record_index], 0, POCKET_D20_SHORT_LEN);
        dndolphins_copy(character->languages[app->record_index], POCKET_D20_SHORT_LEN, "New Language");
        break;
    }
    dndolphins_save(app, false);
    dndolphins_enter_screen(app, PocketScreenRecordDetail);
    return true;
}

static void dndolphins_delete_record(PocketD20App* app) {
    PocketCharacter* character = &app->data.character;
    uint8_t index = app->record_index;
    switch(app->list_kind) {
    case PocketListClasses:
        if(character->class_count <= 1U) {
            dndolphins_set_status(app, "Keep at least one class");
            return;
        }
        memmove(
            &character->classes[index],
            &character->classes[index + 1U],
            (character->class_count - index - 1U) * sizeof(PocketClassLevel));
        --character->class_count;
        memset(&character->classes[character->class_count], 0, sizeof(PocketClassLevel));
        if(!dndolphins_progression_store_features_remap_classes(
               app->storage, app->profiles.active_profile, index)) {
            dndolphins_set_status(app, "Feature update failed");
            return;
        }
        if(!dnd_storage_remap_spell_classes(
               app->storage, app->profiles.active_profile, character, index)) {
            dndolphins_set_status(app, "Spellbook update failed");
            return;
        }
        app->spell_class_counts_valid = 0U;
        break;
    case PocketListFeatures:
        if(index >= app->features_total || !dndolphins_save_features_if_changed(app) ||
           !dndolphins_progression_store_features_delete(app->storage, app->profiles.active_profile, index)) {
            dndolphins_set_status(app, "Feature delete failed");
            return;
        }
        --app->features_total;
        dnd_data_reserve_features_exact(character, 0U);
        character->feature_count = 0U;
        app->features_loaded = 0U;
        app->features_cache_start = 0U;
        if(app->features_total) {
            uint8_t target = index < app->features_total ? index : (uint8_t)(app->features_total - 1U);
            if(!dndolphins_feature_cache_ensure(app, target)) {
                dndolphins_set_status(app, "Features read failed");
                return;
            }
        }
        break;
    case PocketListLanguages:
        memmove(
            &character->languages[index],
            &character->languages[index + 1U],
            (character->language_count - index - 1U) * POCKET_D20_SHORT_LEN);
        --character->language_count;
        memset(character->languages[character->language_count], 0, POCKET_D20_SHORT_LEN);
        break;
    }
    dndolphins_save(app, false);
    dndolphins_enter_screen(app, PocketScreenRecordList);
    if(app->list_kind == PocketListFeatures && app->features_total) {
        uint8_t target =
            index < app->features_total ? index : (uint8_t)(app->features_total - 1U);
        dndolphins_record_list_focus(app, target);
    }
}

static void dndolphins_text_done(void* context) {
    PocketD20App* app = context;
    app->input_module_active = 0U;
    PocketCharacter* character = &app->data.character;
    uint8_t index = app->record_index;
    PocketEditTarget completed_target = app->edit_target;
    switch(app->edit_target) {
    case PocketEditCharacterName:
        dndolphins_copy(character->name, sizeof(character->name), app->edit_buffer);
        break;
    case PocketEditPlayerName:
        dndolphins_copy(character->player, sizeof(character->player), app->edit_buffer);
        break;
    case PocketEditSpecies:
        dndolphins_copy(character->species, sizeof(character->species), app->edit_buffer);
        break;
    case PocketEditBackground:
        dndolphins_copy(character->background, sizeof(character->background), app->edit_buffer);
        break;
    case PocketEditAlignment:
        dndolphins_copy(character->alignment, sizeof(character->alignment), app->edit_buffer);
        break;
    case PocketEditOtherProficiencies:
        dndolphins_copy(
            character->other_proficiencies,
            sizeof(character->other_proficiencies),
            app->edit_buffer);
        break;
    case PocketEditOriginFeat:
        dndolphins_copy(character->origin_feat, sizeof(character->origin_feat), app->edit_buffer);
        break;
    case PocketEditToolProficiencies:
        dndolphins_copy(
            character->tool_proficiencies,
            sizeof(character->tool_proficiencies),
            app->edit_buffer);
        break;
    case PocketEditArmorTraining:
        dndolphins_copy(
            character->armor_training, sizeof(character->armor_training), app->edit_buffer);
        break;
    case PocketEditWeaponTraining:
        dndolphins_copy(
            character->weapon_training, sizeof(character->weapon_training), app->edit_buffer);
        break;
    case PocketEditSenses:
        dndolphins_copy(character->senses, sizeof(character->senses), app->edit_buffer);
        break;
    case PocketEditConditions:
        dndolphins_copy(character->conditions, sizeof(character->conditions), app->edit_buffer);
        break;
    case PocketEditConcentration:
        dndolphins_copy(character->concentration, sizeof(character->concentration), app->edit_buffer);
        break;
    case PocketEditTemporaryEffects:
        dndolphins_copy(
            character->temporary_effects, sizeof(character->temporary_effects), app->edit_buffer);
        break;
    case PocketEditResistances:
        dndolphins_copy(character->resistances, sizeof(character->resistances), app->edit_buffer);
        break;
    case PocketEditImmunities:
        dndolphins_copy(character->immunities, sizeof(character->immunities), app->edit_buffer);
        break;
    case PocketEditVulnerabilities:
        dndolphins_copy(
            character->vulnerabilities, sizeof(character->vulnerabilities), app->edit_buffer);
        break;
    case PocketEditMovementModes:
        dndolphins_copy(
            character->movement_modes, sizeof(character->movement_modes), app->edit_buffer);
        break;
    case PocketEditClassName:
        dndolphins_copy(
            character->classes[index].name,
            sizeof(character->classes[index].name),
            app->edit_buffer);
        dndolphins_configure_class_defaults(&character->classes[index]);
        dndolphins_spells_initialize_spell_slots_if_unset(character);
        dndolphins_spells_apply_level_progression(character, index);
        break;
    case PocketEditSubclass:
        dndolphins_copy(
            character->classes[index].subclass,
            sizeof(character->classes[index].subclass),
            app->edit_buffer);
        dndolphins_spells_refresh_class_spellcasting(&character->classes[index]);
        dndolphins_spells_initialize_spell_slots_if_unset(character);
        dndolphins_spells_apply_level_progression(character, index);
        break;
    case PocketEditGrantStableId:
        if(index < character->grant_count)
            dndolphins_copy(
                character->grants[index].stable_id,
                sizeof(character->grants[index].stable_id),
                app->edit_buffer);
        break;
    case PocketEditGrantSource:
        if(index < character->grant_count)
            dndolphins_copy(
                character->grants[index].source,
                sizeof(character->grants[index].source),
                app->edit_buffer);
        break;
    case PocketEditGrantOption:
        if(index < character->grant_count)
            dndolphins_copy(
                character->grants[index].option_name,
                sizeof(character->grants[index].option_name),
                app->edit_buffer);
        break;
    case PocketEditGrantPrerequisites:
        if(index < character->grant_count)
            dndolphins_copy(
                character->grants[index].prerequisites,
                sizeof(character->grants[index].prerequisites),
                app->edit_buffer);
        break;
    case PocketEditGrantValue:
        if(index < character->grant_count)
            dndolphins_copy(
                character->grants[index].grant_value,
                sizeof(character->grants[index].grant_value),
                app->edit_buffer);
        break;
    case PocketEditAttackName:
        if(index < character->attack_template_count)
            dndolphins_copy(
                character->attack_templates[index].name,
                sizeof(character->attack_templates[index].name),
                app->edit_buffer);
        break;
    case PocketEditAttackMastery:
        if(index < character->attack_template_count)
            dndolphins_copy(
                character->attack_templates[index].mastery,
                sizeof(character->attack_templates[index].mastery),
                app->edit_buffer);
        break;
    case PocketEditAttackDamageType:
        if(index < character->attack_template_count)
            dndolphins_copy(
                character->attack_templates[index].damage_type,
                sizeof(character->attack_templates[index].damage_type),
                app->edit_buffer);
        break;
    case PocketEditAttackRiderType:
        if(index < character->attack_template_count)
            dndolphins_copy(
                character->attack_templates[index].rider_type,
                sizeof(character->attack_templates[index].rider_type),
                app->edit_buffer);
        break;
    case PocketEditFeatureName: {
        PocketFeature* feature = dndolphins_feature_at(app, index, NULL);
        if(feature) {
            dndolphins_copy(feature->name, sizeof(feature->name), app->edit_buffer);
            (void)dndolphins_save_features_if_changed(app);
        }
        break;
    }
    case PocketEditFeatureDetail: {
        PocketFeature* feature = dndolphins_feature_at(app, index, NULL);
        if(feature) {
            dndolphins_copy(feature->detail, sizeof(feature->detail), app->edit_buffer);
            (void)dndolphins_save_features_if_changed(app);
        }
        break;
    }
    case PocketEditLanguageName:
        dndolphins_copy(character->languages[index], POCKET_D20_SHORT_LEN, app->edit_buffer);
        break;
    case PocketEditNone:
        break;
    default:
        break;
    }
    app->edit_target = PocketEditNone;
    switch(completed_target) {
    default:
        break;
    }
    dndolphins_save(app, false);
    view_dispatcher_switch_to_view(app->dispatcher, PocketViewMain);
    dndolphins_refresh(app);
}

static bool dndolphins_is_move_event(const InputEvent* event) {
    return event->type == InputTypeShort || event->type == InputTypeRepeat;
}

static void dndolphins_handle_back(PocketD20App* app) {
    switch(app->screen) {
    case PocketScreenHome:
        dndolphins_flush_save(app, false);
        view_dispatcher_stop(app->dispatcher);
        break;
    case PocketScreenRecordList:
        dndolphins_enter_screen(app, app->record_list_return_screen);
        break;
    case PocketScreenProfileActions:
        dndolphins_enter_screen(app, PocketScreenProfiles);
        break;
    case PocketScreenRecordDetail:
        dndolphins_enter_screen(app, PocketScreenRecordList);
        {
            app->selection = app->record_index + 1U;
            if(app->selection >= 5U) app->scroll = app->selection - 4U;
        }
        break;
    case PocketScreenCatalog:
        if(app->level_choice_mode == 3U && app->catalog_target == PocketEditFeatureName) {
            PocketCharacter* c = &app->data.character;
            dnd_data_reserve_features_exact(c, 0U);
            c->feature_count = 0U;
            app->level_choice_mode = 0U;
        }
        dndolphins_catalog_release(app);
        dndolphins_enter_screen(app, app->return_screen);
        app->selection = app->catalog_return_selection;
        if(app->selection >= 5U) app->scroll = app->selection - 4U;
        break;
    case PocketScreenMagic:
        app->arcane_recovery_active = 0U;
        dndolphins_enter_screen(app, PocketScreenHome);
        break;
    case PocketScreenGrantReview:
        dndolphins_release_pending_grants(app);
        dndolphins_enter_screen(app, app->return_screen);
        break;
    case PocketScreenGrantEdit:
        dndolphins_enter_screen(app, PocketScreenGrantReview);
        break;
    case PocketScreenLevelReview:
        dndolphins_enter_screen(app, PocketScreenRecordDetail);
        app->selection = 2U;
        app->scroll = 0U;
        break;
    case PocketScreenLevelChoice:
    case PocketScreenAsiAbility:
        app->level_choice_first_ability = UINT8_MAX;
        app->level_choice_first_score = 0;
        dndolphins_enter_screen(app, app->return_screen);
        break;
    case PocketScreenSpellAttacks:
    case PocketScreenRituals:
        dndolphins_enter_screen(app, PocketScreenCombat);
        break;
    case PocketScreenSpellCast:
        dndolphins_enter_screen(app, PocketScreenSpellAttacks);
        break;
    case PocketScreenSpellResult:
        dndolphins_enter_screen(
            app,
            app->spell_cast_resource == PocketSpellCastRitual ? PocketScreenRituals :
                                                                PocketScreenSpellAttacks);
        break;
    case PocketScreenAttackTemplates:
        dndolphins_enter_screen(app, PocketScreenCombat);
        break;
    case PocketScreenAttackTemplateEdit:
        dndolphins_enter_screen(app, PocketScreenAttackTemplates);
        break;
    case PocketScreenAttackList:
    case PocketScreenAttackResult:
        dndolphins_enter_screen(app, PocketScreenCombat);
        break;
    case PocketScreenDiceResult:
        dndolphins_enter_screen(app, PocketScreenDice);
        break;
    default:
        dndolphins_enter_screen(app, PocketScreenHome);
        break;
    }
}

static void dndolphins_handle_long_back(PocketD20App* app) {
    if(app->screen == PocketScreenHome) {
        dndolphins_flush_save(app, false);
        view_dispatcher_stop(app->dispatcher);
        return;
    }
    app->dice_animating = 0U;
    app->arcane_recovery_active = 0U;
    dndolphins_catalog_release(app);
    dndolphins_enter_screen(app, PocketScreenHome);
    app->marquee_elapsed_ms = 0U;
}

static void dndolphins_handle_profiles(PocketD20App* app, const InputEvent* event) {
    uint16_t profile_count = dndolphins_profile_count(app);
    uint16_t row_count = profile_count + 1U;
    if(dndolphins_is_move_event(event) && event->key == InputKeyUp) {
        uint16_t previous_scroll = app->scroll;
        dndolphins_menu_move(app, row_count, -1);
        if(app->scroll != previous_scroll && app->scroll < profile_count)
            (void)dnd_storage_profiles_window(app->storage, &app->profiles, app->scroll);
    } else if(dndolphins_is_move_event(event) && event->key == InputKeyDown) {
        uint16_t previous_scroll = app->scroll;
        dndolphins_menu_move(app, row_count, 1);
        if(app->scroll != previous_scroll && app->scroll < profile_count)
            (void)dnd_storage_profiles_window(app->storage, &app->profiles, app->scroll);
    }
    else if(event->type == InputTypeShort && event->key == InputKeyOk) {
        if(app->selection == profile_count)
            dndolphins_create_profile(app);
        else
            dndolphins_switch_profile(app, dndolphins_profile_id_at(app, app->selection));
    } else if(
        event->type == InputTypeLong && event->key == InputKeyOk &&
        app->selection < profile_count) {
        app->profile_action_id = dndolphins_profile_id_at(app, app->selection);
        dndolphins_enter_screen(app, PocketScreenProfileActions);
    }
}

static void dndolphins_profile_actions_to_list(PocketD20App* app) {
    dndolphins_enter_screen(app, PocketScreenProfiles);
}

static void dndolphins_handle_profile_actions(PocketD20App* app, const InputEvent* event) {
    uint16_t count = sizeof(dndolphins_profile_actions) / sizeof(dndolphins_profile_actions[0]);
    if(dndolphins_is_move_event(event) && event->key == InputKeyUp)
        dndolphins_menu_move(app, count, -1);
    else if(dndolphins_is_move_event(event) && event->key == InputKeyDown)
        dndolphins_menu_move(app, count, 1);
    else if(event->type == InputTypeShort && event->key == InputKeyOk) {
        uint32_t profile = app->profile_action_id;
        if(app->selection == 0U) {
            dndolphins_switch_profile(app, profile);
        } else if(app->selection == 1U) {
            if(profile != app->profiles.active_profile) {
                dndolphins_set_status(app, "Switch before rename");
            } else {
                dndolphins_begin_text(
                    app, PocketEditCharacterName, "Character name", app->data.character.name);
            }
        } else if(app->selection == 2U) {
            uint32_t destination = dnd_storage_profiles_next_id(&app->profiles);
            bool duplicated =
                (profile != app->profiles.active_profile || dndolphins_flush_save(app, false)) &&
                !(destination == UINT32_MAX && dndolphins_profile_exists(app, UINT32_MAX)) &&
                dnd_storage_duplicate_profile(app->storage, profile, destination) &&
                dnd_storage_profiles_refresh(app->storage, &app->profiles) &&
                dnd_storage_profiles_save(app->storage, &app->profiles);
            dndolphins_profile_actions_to_list(app);
            dndolphins_set_status(app, duplicated ? "Character duplicated" : "Duplicate failed");
        } else if(app->selection == 3U) {
            bool exported =
                (profile != app->profiles.active_profile || dndolphins_flush_save(app, false)) &&
                dnd_storage_export_profile(app->storage, profile);
            if(exported)
                dndolphins_confirm_action(app, "Export written");
            else
                dndolphins_set_status(app, "Export failed");
        } else if(app->selection == 4U) {
            uint32_t previous = app->profiles.active_profile;
            uint32_t destination = dnd_storage_profiles_next_id(&app->profiles);
            bool imported =
                (!app->active_profile_loaded || dndolphins_flush_save(app, false)) &&
                !(destination == UINT32_MAX && dndolphins_profile_exists(app, UINT32_MAX)) &&
                dnd_storage_import_first(app->storage, destination, &app->data);
            if(imported) {
                /* import_first() replaces the core object and collection files. Any page
                   that belonged to the previous profile has already been freed by the
                   core loader and must not remain marked as a valid cache. */
                app->spellbook_loaded = 0U;
                app->items_loaded = 0U;
                app->spellbook_total = 0U;
                app->items_total = 0U;
                app->spellbook_cache_start = 0U;
                app->items_cache_start = 0U;
                app->active_profile_loaded = 1U;
                app->profiles.active_profile = destination;
                imported = dnd_storage_profiles_refresh(app->storage, &app->profiles) &&
                           dnd_storage_profiles_save(app->storage, &app->profiles);
                app->saved_fingerprint = dndolphins_data_fingerprint(&app->data);
                dndolphins_enter_screen(app, PocketScreenHome);
                dndolphins_set_status(app, imported ? "Character imported" : "Import metadata failed");
            } else {
                bool recovered = false;
                app->active_profile_loaded = dnd_storage_load_profile(
                    app->storage, previous, &app->data, &recovered);
                app->spellbook_loaded = 0U;
                app->items_loaded = 0U;
                app->spellbook_total = 0U;
                app->items_total = 0U;
                app->spellbook_cache_start = 0U;
                app->items_cache_start = 0U;
                app->saved_fingerprint = dndolphins_data_fingerprint(&app->data);
                dndolphins_set_status(app, "No valid export");
            }
        } else if(app->selection == 5U) {
            if(profile == app->profiles.active_profile) {
                dndolphins_set_status(app, "Switch before archive");
            } else {
                bool archived = dnd_storage_archive_profile(app->storage, profile) &&
                                dnd_storage_profiles_refresh(app->storage, &app->profiles) &&
                                dnd_storage_profiles_save(app->storage, &app->profiles);
                dndolphins_profile_actions_to_list(app);
                dndolphins_set_status(app, archived ? "Character archived" : "Archive failed");
            }
        } else if(app->selection == 6U) {
            bool deleted = dndolphins_delete_profile(app, profile);
            dndolphins_profile_actions_to_list(app);
            dndolphins_set_status(app, deleted ? "Character deleted" : "Delete failed");
        } else if(app->selection == 7U) {
            bool verified =
                (profile != app->profiles.active_profile || dndolphins_flush_save(app, false)) &&
                dnd_storage_verify_profile(app->storage, profile);
            if(verified)
                dndolphins_confirm_action(app, "Profile readable");
            else
                dndolphins_set_status(app, "Save damaged/incompatible");
        } else if(app->selection == 8U && profile != app->profiles.active_profile) {
            dndolphins_set_status(app, "Switch before restore");
        } else if(app->selection == 8U) {
            bool restored = dnd_storage_restore_backup(app->storage, profile, &app->data);
            if(!restored) {
                bool recovered = false;
                app->active_profile_loaded =
                    dnd_storage_load_profile(app->storage, profile, &app->data, &recovered);
            } else {
                app->active_profile_loaded = 1U;
            }
            dnd_storage_profiles_refresh(app->storage, &app->profiles);
            dnd_storage_profiles_save(app->storage, &app->profiles);
            app->saved_fingerprint = dndolphins_data_fingerprint(&app->data);
            if(restored)
                dndolphins_confirm_action(app, "Backup restored");
            else
                dndolphins_set_status(app, "No valid backup");
        }
    }
}

static void dndolphins_request_launch(PocketD20App* app, PocketPendingLaunch launch) {
    if(!app) return;

    /* Preserve real character changes before tearing the app down. A missing
       active character is not a launch blocker. Companion apps resolve the
       exact persisted Active= profile themselves. */
    if(app->active_profile_loaded && !dndolphins_flush_save(app, false) &&
       launch != PocketPendingLaunchBestiary) {
        dndolphins_set_status(app, "Save failed - launch cancelled");
        return;
    }

    /* Bestiary is never blocked by missing character state. Initiative resolves
       persisted Active= exactly; only absent/unreadable metadata defaults its
       requested ID to 0, and the selected ID must still have a primary profile. */
    app->pending_launch = launch;

    /* Quiesce callbacks and drop transient catalog storage before returning from
       the dispatcher. dndolphins_app() performs the authoritative full teardown
       before the shared handoff module opens Loader. */
    dndolphins_quiesce_async(app);
    dndolphins_catalog_release(app);
    view_dispatcher_stop(app->dispatcher);
}

static void dndolphins_handle_home(PocketD20App* app, const InputEvent* event) {
    uint16_t count = dndolphins_home_count(app);
    if(dndolphins_is_move_event(event) && event->key == InputKeyUp)
        dndolphins_menu_move(app, count, -1);
    else if(dndolphins_is_move_event(event) && event->key == InputKeyDown)
        dndolphins_menu_move(app, count, 1);
    else if(event->type == InputTypeShort && event->key == InputKeyOk) {
        if(app->selection >= (uint16_t)DndolphinsHomeCount) {
            app->storage_read_only = 0U;
            dndolphins_save(app, true);
            return;
        }
        dndolphins_set_home_focus(app, (DndolphinsHomeIndex)app->selection);
        switch((DndolphinsHomeIndex)app->selection) {
        case DndolphinsHomeCharacters:
            dnd_storage_profiles_refresh(app->storage, &app->profiles);
            dndolphins_profile_include_active(app);
            dndolphins_enter_screen(app, PocketScreenProfiles);
            break;
        case DndolphinsHomeCharacter:
            dndolphins_enter_screen(app, PocketScreenCharacter);
            break;
        case DndolphinsHomeVitals:
            dndolphins_enter_screen(app, PocketScreenVitals);
            break;
        case DndolphinsHomeAbilitiesSaves:
            dndolphins_enter_screen(app, PocketScreenAbilities);
            break;
        case DndolphinsHomeSkills:
            dndolphins_enter_screen(app, PocketScreenSkills);
            break;
        case DndolphinsHomeFeaturesPerks:
            dndolphins_open_list(app, PocketListFeatures, PocketScreenHome);
            break;
        case DndolphinsHomeInventory:
            dndolphins_request_launch(app, PocketPendingLaunchInventory);
            break;
        case DndolphinsHomeMagicSpells:
            dndolphins_enter_screen(app, PocketScreenMagic);
            break;
        case DndolphinsHomeBestiary:
            dndolphins_request_launch(app, PocketPendingLaunchBestiary);
            break;
        case DndolphinsHomeInitiative:
            dndolphins_request_launch(app, PocketPendingLaunchInitiative);
            break;
        case DndolphinsHomeCombat:
            app->hit_die_class_index = 0U;
            if(app->roll_mode == PocketRollGuidance) app->roll_mode = PocketRollNormal;
            dndolphins_enter_screen(app, PocketScreenCombat);
            break;
        case DndolphinsHomeDiceRoller:
            dndolphins_enter_screen(app, PocketScreenDice);
            break;
        case DndolphinsHomeAdventure:
            dndolphins_request_launch(app, PocketPendingLaunchAdventure);
            break;
        case DndolphinsHomeJournal:
            dndolphins_request_launch(app, PocketPendingLaunchJournal);
            break;
        case DndolphinsHomeCount:
            break;
        }
    }
}

static void dndolphins_handle_character(PocketD20App* app, const InputEvent* event) {
    PocketCharacter* character = &app->data.character;
    if(dndolphins_is_move_event(event) && event->key == InputKeyUp)
        dndolphins_menu_move(app, 15U, -1);
    else if(dndolphins_is_move_event(event) && event->key == InputKeyDown)
        dndolphins_menu_move(app, 15U, 1);
    else if(
        dndolphins_is_move_event(event) &&
        (event->key == InputKeyLeft || event->key == InputKeyRight)) {
        int8_t delta = event->key == InputKeyRight ? 1 : -1;
        if(app->selection == 7U) {
            int64_t value = (int64_t)character->experience + (delta * 100);
            if(value < 0) value = 0;
            if(value > 1000000) value = 1000000;
            character->experience = (uint32_t)value;
            dndolphins_save(app, false);
        } else if(app->selection == 8U) {
            character->milestone_leveling = !character->milestone_leveling;
            dndolphins_save(app, false);
        } else if(app->selection == 11U) {
            character->inspiration = !character->inspiration;
            dndolphins_save(app, false);
        }
    } else if(event->type == InputTypeLong && event->key == InputKeyOk && app->selection == 7U) {
        dndolphins_begin_number(
            app,
            PocketNumberCharacter,
            7U,
            0U,
            "Experience points",
            (int32_t)character->experience,
            0,
            1000000);
    } else if(
        event->type == InputTypeLong && event->key == InputKeyOk &&
        (app->selection == 2U || app->selection == 3U || app->selection == 4U)) {
        if(app->selection == 2U)
            dndolphins_begin_text(app, PocketEditSpecies, "Custom species", character->species);
        else if(app->selection == 3U)
            dndolphins_begin_text(
                app, PocketEditBackground, "Custom background", character->background);
        else
            dndolphins_begin_text(app, PocketEditAlignment, "Custom alignment", character->alignment);
    } else if(event->type == InputTypeShort && event->key == InputKeyOk) {
        switch(app->selection) {
        case 0:
            dndolphins_begin_text(app, PocketEditCharacterName, "Character name", character->name);
            break;
        case 1:
            dndolphins_begin_text(app, PocketEditPlayerName, "Player name", character->player);
            break;
        case 2:
            dndolphins_open_catalog(app, PocketCatalogSpecies, PocketEditSpecies, character->species);
            break;
        case 3:
            dndolphins_open_catalog(
                app, PocketCatalogBackgrounds, PocketEditBackground, character->background);
            break;
        case 4:
            dndolphins_open_catalog(
                app, PocketCatalogAlignments, PocketEditAlignment, character->alignment);
            break;
        case 5:
            dndolphins_open_list(app, PocketListClasses, PocketScreenCharacter);
            break;
        case 7:
            character->experience += 100U;
            dndolphins_save(app, false);
            break;
        case 8:
            character->milestone_leveling = !character->milestone_leveling;
            dndolphins_save(app, false);
            break;
        case 9:
            dndolphins_open_list(app, PocketListLanguages, PocketScreenCharacter);
            break;
        case 10:
            dndolphins_begin_text(
                app,
                PocketEditOtherProficiencies,
                "Other proficiencies",
                character->other_proficiencies);
            break;
        case 11:
            character->inspiration = !character->inspiration;
            dndolphins_save(app, false);
            break;
        case 12:
            (void)dndolphins_begin_next_level_choice(app);
            app->return_screen = PocketScreenCharacter;
            dndolphins_enter_screen(app, PocketScreenLevelChoice);
            app->selection = 0U;
            app->scroll = 0U;
            if(!app->level_choice_level) dndolphins_set_status(app, "No pending ASI/Feat choices");
            break;
        case 13: {
            bool progression_changed = false;
            for(uint8_t i = 0U; i < character->class_count; ++i)
                if(dndolphins_spells_apply_level_progression(character, i))
                    progression_changed = true;
            uint16_t applied = 0U;
            uint8_t failed = 0U;
            bool had_candidates = false;
            if(!dndolphins_apply_character_grants(
                   app, 1U, true, &applied, &failed, &had_candidates)) {
                dndolphins_set_status(app, "Initial grant failed");
                break;
            }
            UNUSED(had_candidates);
            const bool updated = progression_changed || applied > 0U;
            if(!dndolphins_flush_save(app, false)) {
                dndolphins_set_status(app, "Update save failed");
                break;
            }
            if(failed) {
                app->return_screen = PocketScreenCharacter;
                dndolphins_enter_screen(app, PocketScreenGrantReview);
                snprintf(
                    app->status,
                    sizeof(app->status),
                    updated ? "Updated; %u review" : "%u grants need review",
                    failed);
            } else {
                dndolphins_release_pending_grants(app);
                dndolphins_set_status(app, updated ? "Updated" : "No changes");
            }
            break;
        }
        case 14: {
            bool progression_changed = false;
            for(uint8_t i = 0U; i < character->class_count; ++i)
                if(dndolphins_spells_apply_level_progression(character, i))
                    progression_changed = true;
            uint16_t applied = 0U;
            uint8_t failed = 0U;
            bool had_candidates = false;
            if(!dndolphins_apply_character_grants(
                   app, UINT8_MAX, false, &applied, &failed, &had_candidates)) {
                dndolphins_set_status(app, "Level grant failed");
                break;
            }
            UNUSED(had_candidates);
            const bool updated = progression_changed || applied > 0U;
            if(!dndolphins_flush_save(app, false)) {
                dndolphins_set_status(app, "Update save failed");
                break;
            }
            if(failed) {
                app->return_screen = PocketScreenCharacter;
                dndolphins_enter_screen(app, PocketScreenGrantReview);
                snprintf(
                    app->status,
                    sizeof(app->status),
                    updated ? "Updated; %u review" : "%u grants need review",
                    failed);
            } else {
                dndolphins_release_pending_grants(app);
                dndolphins_set_status(app, updated ? "Updated" : "No changes");
            }
            break;
        }
        default:
            break;
        }
    }
}

static void dndolphins_handle_vitals(PocketD20App* app, const InputEvent* event) {
    PocketCharacter* character = &app->data.character;
    if(dndolphins_is_move_event(event) && event->key == InputKeyUp)
        dndolphins_menu_move(app, 16U, -1);
    else if(dndolphins_is_move_event(event) && event->key == InputKeyDown)
        dndolphins_menu_move(app, 16U, 1);
    else if(
        dndolphins_is_move_event(event) &&
        (event->key == InputKeyLeft || event->key == InputKeyRight)) {
        int16_t delta = event->key == InputKeyRight ? 1 : -1;
        switch(app->selection) {
        case 0:
            character->hp_current = dndolphins_clamp_i16(character->hp_current + delta, 0, 999);
            break;
        case 1:
            character->hp_max = dndolphins_clamp_i16(character->hp_max + delta, 1, 999);
            if(character->hp_current > character->hp_max)
                character->hp_current = character->hp_max;
            break;
        case 2:
            character->hp_temporary = dndolphins_clamp_i16(character->hp_temporary + delta, 0, 999);
            break;
        case 3:
            character->armor_class = dndolphins_clamp_i16(character->armor_class + delta, 0, 99);
            break;
        case 4:
            character->speed = dndolphins_clamp_i16(character->speed + (delta * 5), 0, 255);
            break;
        case 5:
        case 6:
            character->initiative_misc =
                (int8_t)dndolphins_clamp_i16(character->initiative_misc + delta, -20, 20);
            break;
        case 7:
            character->exhaustion = dndolphins_clamp_u8(character->exhaustion + delta, 6U);
            break;
        case 8:
            character->death_successes = dndolphins_clamp_u8(character->death_successes + delta, 3U);
            break;
        case 9:
            character->death_failures = dndolphins_clamp_u8(character->death_failures + delta, 3U);
            break;
        case 10:
            character->hit_die = dndolphins_cycle_die(character->hit_die, delta, true);
            break;
        case 11:
            character->hit_dice_current =
                dndolphins_clamp_u8(character->hit_dice_current + delta, character->hit_dice_max);
            break;
        case 12:
            character->hit_dice_max = dndolphins_clamp_u8(character->hit_dice_max + delta, 20U);
            if(character->hit_dice_current > character->hit_dice_max)
                character->hit_dice_current = character->hit_dice_max;
            break;
        case 13:
            character->skill_misc[11U] =
                (int8_t)dndolphins_clamp_i16(character->skill_misc[11U] + delta, -20, 20);
            break;
        case 14:
            character->skill_misc[6U] =
                (int8_t)dndolphins_clamp_i16(character->skill_misc[6U] + delta, -20, 20);
            break;
        case 15:
            character->skill_misc[8U] =
                (int8_t)dndolphins_clamp_i16(character->skill_misc[8U] + delta, -20, 20);
            break;
        default:
            return;
        }
        dndolphins_save(app, false);
    } else if(event->type == InputTypeLong && event->key == InputKeyOk) {
        const char* header = "Numeric value";
        int32_t value = 0;
        int32_t minimum = 0;
        int32_t maximum = 999;
        switch(app->selection) {
        case 0U:
            header = "Current HP";
            value = character->hp_current;
            break;
        case 1U:
            header = "Maximum HP";
            value = character->hp_max;
            minimum = 1;
            break;
        case 2U:
            header = "Temporary HP";
            value = character->hp_temporary;
            break;
        case 3U:
            header = "Armor Class";
            value = character->armor_class;
            maximum = 99;
            break;
        case 4U:
            header = "Speed in feet";
            value = character->speed;
            maximum = 255;
            break;
        case 5U:
        case 6U:
            header = "Initiative misc";
            value = character->initiative_misc;
            minimum = -20;
            maximum = 20;
            break;
        case 7U:
            header = "Exhaustion";
            value = character->exhaustion;
            maximum = 6;
            break;
        case 8U:
            header = "Death successes";
            value = character->death_successes;
            maximum = 3;
            break;
        case 9U:
            header = "Death failures";
            value = character->death_failures;
            maximum = 3;
            break;
        case 10U:
            header = "Hit Point Die";
            value = character->hit_die;
            minimum = 4;
            maximum = 12;
            break;
        case 11U:
            header = "Hit Dice current";
            value = character->hit_dice_current;
            maximum = character->hit_dice_max;
            break;
        case 12U:
            header = "Hit Dice maximum";
            value = character->hit_dice_max;
            maximum = 20;
            break;
        case 13U:
            header = "Perception misc";
            value = character->skill_misc[11U];
            minimum = -20;
            maximum = 20;
            break;
        case 14U:
            header = "Insight misc";
            value = character->skill_misc[6U];
            minimum = -20;
            maximum = 20;
            break;
        case 15U:
            header = "Investigation misc";
            value = character->skill_misc[8U];
            minimum = -20;
            maximum = 20;
            break;
        }
        dndolphins_begin_number(
            app, PocketNumberVitals, (uint8_t)app->selection, 0U, header, value, minimum, maximum);
    }
}

static void dndolphins_handle_abilities(PocketD20App* app, const InputEvent* event) {
    PocketCharacter* character = &app->data.character;
    if(dndolphins_is_move_event(event) && event->key == InputKeyUp)
        dndolphins_menu_move(app, POCKET_D20_ABILITY_COUNT, -1);
    else if(dndolphins_is_move_event(event) && event->key == InputKeyDown)
        dndolphins_menu_move(app, POCKET_D20_ABILITY_COUNT, 1);
    else if(
        dndolphins_is_move_event(event) &&
        (event->key == InputKeyLeft || event->key == InputKeyRight)) {
        int8_t delta = event->key == InputKeyRight ? 1 : -1;
        uint8_t index = app->selection;
        if(app->edit_modifier_mode) {
            character->saving_throw_misc[index] =
                (int8_t)dndolphins_clamp_i16(character->saving_throw_misc[index] + delta, -20, 20);
        } else {
            character->ability_scores[index] =
                (int8_t)dndolphins_clamp_i16(character->ability_scores[index] + delta, 1, 30);
        }
        dndolphins_save(app, false);
    } else if(
        event->type == InputTypeLong &&
        (event->key == InputKeyLeft || event->key == InputKeyRight)) {
        app->edit_modifier_mode = !app->edit_modifier_mode;
        dndolphins_set_status(app, app->edit_modifier_mode ? "Editing save misc" : "Editing scores");
    } else if(event->type == InputTypeLong && event->key == InputKeyOk) {
        uint8_t index = (uint8_t)app->selection;
        dndolphins_begin_number(
            app,
            PocketNumberAbility,
            index,
            app->edit_modifier_mode,
            app->edit_modifier_mode ? "Saving throw misc" : "Ability score",
            app->edit_modifier_mode ? character->saving_throw_misc[index] :
                                      character->ability_scores[index],
            app->edit_modifier_mode ? -20 : 1,
            app->edit_modifier_mode ? 20 : 30);
    } else if(event->type == InputTypeShort && event->key == InputKeyOk) {
        uint8_t index = app->selection;
        character->saving_throw_proficiency[index] =
            (character->saving_throw_proficiency[index] + 1U) % 2U;
        dndolphins_save(app, false);
    }
}

static void dndolphins_handle_skills(PocketD20App* app, const InputEvent* event) {
    PocketCharacter* character = &app->data.character;
    if(dndolphins_is_move_event(event) && event->key == InputKeyUp)
        dndolphins_menu_move(app, POCKET_D20_SKILL_COUNT, -1);
    else if(dndolphins_is_move_event(event) && event->key == InputKeyDown)
        dndolphins_menu_move(app, POCKET_D20_SKILL_COUNT, 1);
    else if(
        dndolphins_is_move_event(event) &&
        (event->key == InputKeyLeft || event->key == InputKeyRight)) {
        int8_t delta = event->key == InputKeyRight ? 1 : -1;
        uint8_t index = dndolphins_skill_display_order[app->selection];
        if(app->edit_modifier_mode) {
            character->skill_misc[index] =
                (int8_t)dndolphins_clamp_i16(character->skill_misc[index] + delta, -20, 20);
        } else {
            int16_t proficiency = character->skill_proficiency[index] + delta;
            if(proficiency < 0) proficiency = PocketProficiencyExpertise;
            if(proficiency > PocketProficiencyExpertise) proficiency = PocketProficiencyNone;
            character->skill_proficiency[index] = (uint8_t)proficiency;
        }
        dndolphins_save(app, false);
    } else if(
        event->type == InputTypeLong &&
        (event->key == InputKeyLeft || event->key == InputKeyRight)) {
        app->edit_modifier_mode = !app->edit_modifier_mode;
        dndolphins_set_status(
            app, app->edit_modifier_mode ? "Editing skill misc" : "Editing proficiency");
    } else if(event->type == InputTypeLong && event->key == InputKeyOk) {
        uint8_t index = dndolphins_skill_display_order[app->selection];
        app->edit_modifier_mode = 1U;
        dndolphins_begin_number(
            app,
            PocketNumberSkill,
            index,
            0U,
            "Skill misc modifier",
            character->skill_misc[index],
            -20,
            20);
    } else if(event->type == InputTypeShort && event->key == InputKeyOk) {
        uint8_t index = dndolphins_skill_display_order[app->selection];
        character->skill_proficiency[index] = (character->skill_proficiency[index] + 1U) % 3U;
        dndolphins_save(app, false);
    }
}


static void dndolphins_handle_level_review(PocketD20App* app, const InputEvent* event) {
    if(event->type != InputTypeShort || event->key != InputKeyOk) return;
    if(app->level_review_pending_choice) {
        app->return_screen = PocketScreenRecordDetail;
        dndolphins_enter_screen(app, PocketScreenLevelChoice);
        app->selection = 0U;
        app->scroll = 0U;
    } else {
        dndolphins_enter_screen(app, PocketScreenRecordDetail);
        app->selection = 2U;
        app->scroll = 0U;
    }
}

static void dndolphins_handle_level_choice(PocketD20App* app, const InputEvent* event) {
    if(!app->level_choice_level) {
        if(event->type == InputTypeShort && event->key == InputKeyOk)
            dndolphins_enter_screen(app, app->return_screen);
        return;
    }
    if(dndolphins_is_move_event(event) && event->key == InputKeyUp) dndolphins_menu_move(app, 4U, -1);
    else if(dndolphins_is_move_event(event) && event->key == InputKeyDown) dndolphins_menu_move(app, 4U, 1);
    else if(event->type == InputTypeShort && event->key == InputKeyOk) {
        if(app->selection == 0U || app->selection == 1U) {
            app->level_choice_mode = app->selection == 0U ? 1U : 2U;
            app->level_choice_first_ability = UINT8_MAX;
            app->level_choice_first_score = 0;
            dndolphins_enter_screen(app, PocketScreenAsiAbility);
        } else if(app->selection == 2U) {
            PocketCharacter* c = &app->data.character;
            if(app->features_loaded && !dndolphins_release_features(app)) {
                dndolphins_set_status(app, "Feature save failed");
                return;
            }
            uint8_t feature_total = 0U;
            if(!dndolphins_progression_store_features_count(
                   app->storage, app->profiles.active_profile, &feature_total)) {
                dndolphins_set_status(app, "Feature count failed");
                return;
            }
            if(feature_total >= POCKET_D20_MAX_FEATURES ||
               !dnd_data_reserve_features_exact(c, 1U)) {
                dndolphins_set_status(app, "Feature list full");
                return;
            }
            c->feature_count = 1U;
            app->record_index = 0U;
            memset(&c->features[0], 0, sizeof(PocketFeature));
            c->features[0].class_index = app->level_choice_class_index;
            c->features[0].class_level_gained = app->level_choice_level;
            app->level_choice_mode = 3U;
            dndolphins_open_catalog(app, PocketCatalogFeats, PocketEditFeatureName, "");
        } else {
            app->level_choice_mode = 0U;
            dndolphins_enter_screen(app, app->return_screen);
            dndolphins_set_status(app, "Level choice left pending");
        }
    }
}

static void dndolphins_handle_asi_ability(PocketD20App* app, const InputEvent* event) {
    PocketCharacter* c = &app->data.character;
    if(dndolphins_is_move_event(event) && event->key == InputKeyUp) dndolphins_menu_move(app, POCKET_D20_ABILITY_COUNT, -1);
    else if(dndolphins_is_move_event(event) && event->key == InputKeyDown) dndolphins_menu_move(app, POCKET_D20_ABILITY_COUNT, 1);
    else if(event->type == InputTypeShort && event->key == InputKeyOk) {
        uint8_t ability = (uint8_t)app->selection;
        if(app->level_choice_mode == 1U) {
            if(c->ability_scores[ability] >= 20) { dndolphins_set_status(app, "Ability already 20"); return; }
            int8_t old_score = c->ability_scores[ability];
            int16_t next = old_score + 2;
            c->ability_scores[ability] = next > 20 ? 20 : (int8_t)next;
            if(!dndolphins_complete_level_choice(app, "ASI +2")) {
                c->ability_scores[ability] = old_score;
                dndolphins_set_status(app, "Could not record choice");
                return;
            }
        } else if(app->level_choice_mode == 2U) {
            if(c->ability_scores[ability] >= 20) { dndolphins_set_status(app, "Ability already 20"); return; }
            if(app->level_choice_first_ability >= POCKET_D20_ABILITY_COUNT) {
                /* The first pick is selection-only. Do not mutate either score
                   until the second, different ability is confirmed. */
                app->level_choice_first_ability = ability;
                app->level_choice_first_score = c->ability_scores[ability];
                dndolphins_set_status(app, "Choose second ability");
                return;
            }
            if(ability == app->level_choice_first_ability) { dndolphins_set_status(app, "Choose different ability"); return; }
            uint8_t first = app->level_choice_first_ability;
            int8_t first_old = app->level_choice_first_score;
            int8_t second_old = c->ability_scores[ability];
            /* If anything changed the first score while the two-step picker was
               open, cancel rather than compounding a stale +1. */
            if(c->ability_scores[first] != first_old || first_old >= 20) {
                app->level_choice_first_ability = UINT8_MAX;
                app->level_choice_first_score = 0;
                dndolphins_set_status(app, "ASI changed; choose again");
                return;
            }
            c->ability_scores[first] = (int8_t)(first_old + 1);
            c->ability_scores[ability] = (int8_t)(second_old + 1);
            if(!dndolphins_complete_level_choice(app, "ASI +1/+1")) {
                c->ability_scores[first] = first_old;
                c->ability_scores[ability] = second_old;
                dndolphins_set_status(app, "Could not record choice");
                return;
            }
        } else return;
        dndolphins_save(app, false);
        app->level_choice_mode = 0U;
        app->level_choice_first_ability = UINT8_MAX;
        app->level_choice_first_score = 0;
        dndolphins_enter_screen(app, app->return_screen);
        dndolphins_set_status(app, "Level choice applied");
    }
}

static void dndolphins_handle_grant_review(PocketD20App* app, const InputEvent* event) {
    PocketCharacter* c = &app->data.character;
    uint16_t count = c->grant_count + 2U;
    if(dndolphins_is_move_event(event) && event->key == InputKeyUp)
        dndolphins_menu_move(app, count, -1);
    else if(dndolphins_is_move_event(event) && event->key == InputKeyDown)
        dndolphins_menu_move(app, count, 1);
    else if(
        event->type == InputTypeLong && event->key == InputKeyLeft && app->selection &&
        app->selection <= c->grant_count) {
        PocketGrant* grant = &c->grants[app->selection - 1U];
        if(grant->status == PocketGrantPending) grant->status = PocketGrantSkipped;
        dndolphins_save(app, false);
    } else if(
        event->type == InputTypeLong && event->key == InputKeyOk && app->selection &&
        app->selection <= c->grant_count) {
        app->record_index = app->selection - 1U;
        dndolphins_enter_screen(app, PocketScreenGrantEdit);
    } else if(event->type == InputTypeShort && event->key == InputKeyOk) {
        if(app->selection == 0U) {
            for(uint8_t i = 0U; i < c->grant_count; ++i)
                if(c->grants[i].status == PocketGrantPending)
                    dndolphins_apply_grant(app, &c->grants[i]);
            dndolphins_set_status(app, "Pending grants applied");
        } else if(app->selection <= c->grant_count) {
            PocketGrant* grant = &c->grants[app->selection - 1U];
            if(grant->status == PocketGrantPending)
                dndolphins_apply_grant(app, grant);
            else if(grant->status == PocketGrantSkipped)
                grant->status = PocketGrantPending;
        } else if(
            c->grant_count < POCKET_D20_MAX_GRANTS &&
            dnd_data_reserve_grants(c, c->grant_count + 1U)) {
            app->record_index = c->grant_count++;
            PocketGrant* grant = &c->grants[app->record_index];
            memset(grant, 0, sizeof(*grant));
            snprintf(
                grant->stable_id,
                sizeof(grant->stable_id),
                "custom_grant_%u",
                app->record_index + 1U);
            dndolphins_copy(grant->source, sizeof(grant->source), "Custom");
            dndolphins_copy(grant->option_name, sizeof(grant->option_name), "Custom Grant");
            dndolphins_copy(grant->prerequisites, sizeof(grant->prerequisites), "None");
            dndolphins_copy(grant->grant_value, sizeof(grant->grant_value), "feature=Custom Feature");
            grant->source_type = PocketGrantFeat;
            grant->status = PocketGrantPending;
            dndolphins_save(app, false);
            dndolphins_enter_screen(app, PocketScreenGrantEdit);
        }
        dndolphins_save(app, false);
    }
}

static void dndolphins_handle_grant_edit(PocketD20App* app, const InputEvent* event) {
    PocketCharacter* c = &app->data.character;
    if(app->record_index >= c->grant_count) return;
    PocketGrant* grant = &c->grants[app->record_index];
    if(dndolphins_is_move_event(event) && event->key == InputKeyUp)
        dndolphins_menu_move(app, 10U, -1);
    else if(dndolphins_is_move_event(event) && event->key == InputKeyDown)
        dndolphins_menu_move(app, 10U, 1);
    else if(
        dndolphins_is_move_event(event) &&
        (event->key == InputKeyLeft || event->key == InputKeyRight)) {
        int16_t delta = event->key == InputKeyRight ? 1 : -1;
        if(app->selection == 2U) {
            int16_t value = grant->source_type + delta;
            if(value < 0) value = PocketGrantSourceCount - 1U;
            if(value >= PocketGrantSourceCount) value = 0;
            grant->source_type = (uint8_t)value;
        } else if(app->selection == 5U) {
            if(!c->class_count)
                grant->class_index = 0U;
            else {
                int16_t value = grant->class_index + delta;
                if(value < 0) value = c->class_count - 1U;
                if(value >= c->class_count) value = 0;
                grant->class_index = (uint8_t)value;
            }
        } else if(app->selection == 6U)
            grant->level_gained = (uint8_t)dndolphins_clamp_i16(grant->level_gained + delta, 0, 20);
        else if(app->selection == 8U) {
            int16_t value = grant->status + delta;
            if(value < 0) value = PocketGrantSkipped;
            if(value > PocketGrantSkipped) value = PocketGrantPending;
            grant->status = (uint8_t)value;
        } else
            return;
        dndolphins_save(app, false);
    } else if(event->type == InputTypeShort && event->key == InputKeyOk) {
        if(app->selection == 0U)
            dndolphins_begin_text(app, PocketEditGrantStableId, "Stable grant ID", grant->stable_id);
        else if(app->selection == 1U)
            dndolphins_begin_text(app, PocketEditGrantSource, "Source label", grant->source);
        else if(app->selection == 3U)
            dndolphins_begin_text(app, PocketEditGrantOption, "Option name", grant->option_name);
        else if(app->selection == 4U)
            dndolphins_begin_text(
                app, PocketEditGrantPrerequisites, "Prerequisites", grant->prerequisites);
        else if(app->selection == 7U)
            dndolphins_begin_text(
                app, PocketEditGrantValue, "Grant payload key=value", grant->grant_value);
        else if(app->selection == 9U) {
            memmove(
                &c->grants[app->record_index],
                &c->grants[app->record_index + 1U],
                (c->grant_count - app->record_index - 1U) * sizeof(PocketGrant));
            --c->grant_count;
            dndolphins_save(app, false);
            dndolphins_enter_screen(app, PocketScreenGrantReview);
        }
    }
}





static void dndolphins_handle_attack_templates(PocketD20App* app, const InputEvent* event) {
    PocketCharacter* c = &app->data.character;
    uint16_t count = c->attack_template_count + 1U;
    if(dndolphins_is_move_event(event) && event->key == InputKeyUp)
        dndolphins_menu_move(app, count, -1);
    else if(dndolphins_is_move_event(event) && event->key == InputKeyDown)
        dndolphins_menu_move(app, count, 1);
    else if(
        dndolphins_is_move_event(event) && app->selection < c->attack_template_count &&
        (event->key == InputKeyLeft || event->key == InputKeyRight)) {
        PocketAttackTemplate* attack = &c->attack_templates[app->selection];
        attack->attack_misc = (int8_t)dndolphins_clamp_i16(
            attack->attack_misc + (event->key == InputKeyRight ? 1 : -1), -20, 20);
        dndolphins_save(app, false);
    } else if(
        event->type == InputTypeLong && event->key == InputKeyOk &&
        app->selection < c->attack_template_count) {
        app->record_index = app->selection;
        dndolphins_enter_screen(app, PocketScreenAttackTemplateEdit);
    } else if(
        event->type == InputTypeShort && event->key == InputKeyOk &&
        app->selection == c->attack_template_count) {
        if(c->attack_template_count >= POCKET_D20_MAX_ATTACK_TEMPLATES) {
            dndolphins_set_status(app, "Template limit reached");
            return;
        }
        app->record_index = c->attack_template_count++;
        PocketAttackTemplate* attack = &c->attack_templates[app->record_index];
        memset(attack, 0, sizeof(*attack));
        dndolphins_copy(attack->name, sizeof(attack->name), "Custom Attack");
        dndolphins_copy(attack->damage_type, sizeof(attack->damage_type), "Bludgeoning");
        dndolphins_copy(attack->rider_type, sizeof(attack->rider_type), "None");
        dndolphins_copy(attack->mastery, sizeof(attack->mastery), "None");
        attack->type = PocketAttackTemplateCustom;
        attack->ability = PocketAbilityStrength;
        attack->save_ability = PocketAbilityDexterity;
        attack->damage_dice = 1U;
        attack->damage_die = 6U;
        attack->rider_die = 6U;
        dndolphins_save(app, false);
        dndolphins_enter_screen(app, PocketScreenAttackTemplateEdit);
    } else if(
        event->type == InputTypeShort && event->key == InputKeyOk &&
        app->selection < c->attack_template_count) {
        PocketAttackTemplate* attack = &c->attack_templates[app->selection];
        app->dice_count = attack->damage_dice ? attack->damage_dice : 1U;
        app->dice_sides = attack->damage_die >= 2U ? attack->damage_die : 1U;
        app->dice_modifier = attack->attack_misc;
        app->roll_mode = PocketRollNormal;
        dndolphins_roll_generic(app);
    }
}

static void dndolphins_handle_attack_template_edit(PocketD20App* app, const InputEvent* event) {
    PocketCharacter* c = &app->data.character;
    if(app->record_index >= c->attack_template_count) return;
    PocketAttackTemplate* attack = &c->attack_templates[app->record_index];
    if(dndolphins_is_move_event(event) && event->key == InputKeyUp)
        dndolphins_menu_move(app, 15U, -1);
    else if(dndolphins_is_move_event(event) && event->key == InputKeyDown)
        dndolphins_menu_move(app, 15U, 1);
    else if(
        dndolphins_is_move_event(event) &&
        (event->key == InputKeyLeft || event->key == InputKeyRight)) {
        int16_t delta = event->key == InputKeyRight ? 1 : -1;
        if(app->selection == 1U) {
            int16_t value = attack->type + delta;
            if(value < 0) value = PocketAttackTemplateTypeCount - 1U;
            if(value >= PocketAttackTemplateTypeCount) value = 0;
            attack->type = (uint8_t)value;
        } else if(app->selection == 2U || app->selection == 3U) {
            uint8_t* ability = app->selection == 2U ? &attack->ability : &attack->save_ability;
            int16_t value = *ability + delta;
            if(value < 0) value = PocketAbilityCharisma;
            if(value > PocketAbilityCharisma) value = PocketAbilityStrength;
            *ability = (uint8_t)value;
        } else if(app->selection == 4U)
            attack->attack_misc = (int8_t)dndolphins_clamp_i16(attack->attack_misc + delta, -20, 20);
        else if(app->selection == 5U)
            attack->save_dc = (uint8_t)dndolphins_clamp_i16(attack->save_dc + delta, 0, 30);
        else if(app->selection == 6U)
            attack->damage_dice = (uint8_t)dndolphins_clamp_i16(attack->damage_dice + delta, 0, 20);
        else if(app->selection == 7U)
            attack->damage_die = dndolphins_cycle_die(attack->damage_die, delta, false);
        else if(app->selection == 10U)
            attack->rider_dice = (uint8_t)dndolphins_clamp_i16(attack->rider_dice + delta, 0, 20);
        else if(app->selection == 11U)
            attack->rider_die = dndolphins_cycle_die(attack->rider_die, delta, false);
        else if(app->selection == 13U) {
            int16_t value = attack->recharge + delta;
            if(value < 0) value = PocketRechargeCount - 1U;
            if(value >= PocketRechargeCount) value = 0;
            attack->recharge = (uint8_t)value;
        } else
            return;
        dndolphins_save(app, false);
    } else if(event->type == InputTypeShort && event->key == InputKeyOk) {
        if(app->selection == 0U)
            dndolphins_begin_text(app, PocketEditAttackName, "Attack template name", attack->name);
        else if(app->selection == 8U)
            dndolphins_begin_text(app, PocketEditAttackDamageType, "Damage type", attack->damage_type);
        else if(app->selection == 9U)
            dndolphins_begin_text(app, PocketEditAttackMastery, "Mastery property", attack->mastery);
        else if(app->selection == 12U)
            dndolphins_begin_text(app, PocketEditAttackRiderType, "Rider type", attack->rider_type);
        else if(app->selection == 14U) {
            memmove(
                &c->attack_templates[app->record_index],
                &c->attack_templates[app->record_index + 1U],
                (c->attack_template_count - app->record_index - 1U) *
                    sizeof(PocketAttackTemplate));
            --c->attack_template_count;
            dndolphins_save(app, false);
            dndolphins_enter_screen(app, PocketScreenAttackTemplates);
        }
    }
}

static void dndolphins_handle_magic(PocketD20App* app, const InputEvent* event) {
    PocketCharacter* character = &app->data.character;
    if(dndolphins_is_move_event(event) && event->key == InputKeyUp)
        dndolphins_menu_move(app, 16U, -1);
    else if(dndolphins_is_move_event(event) && event->key == InputKeyDown)
        dndolphins_menu_move(app, 16U, 1);
    else if(
        dndolphins_is_move_event(event) &&
        (event->key == InputKeyLeft || event->key == InputKeyRight)) {
        int8_t delta = event->key == InputKeyRight ? 1 : -1;
        if(app->arcane_recovery_active) {
            if(app->selection < 7U || app->selection > 11U) {
                dndolphins_set_status(app, "Choose a level 1-5 slot");
                return;
            }
            uint8_t level = app->selection - 6U;
            if(delta > 0) {
                uint8_t remaining = app->arcane_recovery_budget - app->arcane_recovery_spent;
                if(level > remaining) {
                    dndolphins_set_status(app, "Not enough recovery");
                    return;
                }
                if(character->spell_slots_current[level] >= character->spell_slots_max[level]) {
                    dndolphins_set_status(app, "That slot level is full");
                    return;
                }
                ++character->spell_slots_current[level];
                ++app->arcane_recovery_restored[level];
                app->arcane_recovery_spent += level;
                character->arcane_recovery_used = 1U;
            } else {
                if(!app->arcane_recovery_restored[level]) {
                    dndolphins_set_status(app, "Nothing to undo here");
                    return;
                }
                --character->spell_slots_current[level];
                --app->arcane_recovery_restored[level];
                app->arcane_recovery_spent -= level;
                if(!app->arcane_recovery_spent) character->arcane_recovery_used = 0U;
            }
            dndolphins_save(app, false);
            dndolphins_set_status(app, "<> recover, row 6 done");
        } else if(app->selection == 1U) {
            int16_t ability = character->spellcasting_ability + delta;
            if(ability < 0) ability = PocketAbilityCharisma;
            if(ability > PocketAbilityCharisma) ability = PocketAbilityStrength;
            character->spellcasting_ability = (uint8_t)ability;
        } else if(app->selection == 3U) {
            character->spell_attack_misc =
                (int8_t)dndolphins_clamp_i16(character->spell_attack_misc + delta, -20, 20);
        } else if(app->selection == 4U) {
            character->spell_save_misc =
                (int8_t)dndolphins_clamp_i16(character->spell_save_misc + delta, -20, 20);
        } else if(app->selection >= 7U && app->selection <= 15U) {
            if(event->type != InputTypeShort) return;
            uint8_t level = app->selection - 6U;
            character->spell_slots_current[level] = dndolphins_clamp_u8(
                character->spell_slots_current[level] + delta,
                character->spell_slots_max[level]);
            dndolphins_set_status(app, "Available slots changed");
        } else {
            return;
        }
        dndolphins_save(app, false);
    } else if(
        !app->arcane_recovery_active && event->type == InputTypeLong &&
        (event->key == InputKeyLeft || event->key == InputKeyRight) && app->selection >= 7U &&
        app->selection <= 15U) {
        int8_t delta = event->key == InputKeyRight ? 1 : -1;
        uint8_t level = app->selection - 6U;
        character->spell_slots_max[level] =
            dndolphins_clamp_u8(character->spell_slots_max[level] + delta, 20U);
        if(character->spell_slots_current[level] > character->spell_slots_max[level])
            character->spell_slots_current[level] = character->spell_slots_max[level];
        dndolphins_save(app, false);
        dndolphins_set_status(app, "Maximum slots changed");
    } else if(event->type == InputTypeLong && event->key == InputKeyOk) {
        if(app->arcane_recovery_active) {
            dndolphins_set_status(app, "Finish recovery first");
        } else if(app->selection == 0U) {
            dndolphins_request_launch(app, PocketPendingLaunchSpellbook);
        } else if(app->selection == 2U) {
            dndolphins_spells_recalculate_multiclass_slots(character);
            dndolphins_save(app, false);
            dndolphins_confirm_action(app, "Class slots recalculated");
        } else if(app->selection == 3U || app->selection == 4U) {
            bool attack = app->selection == 3U;
            dndolphins_begin_number(
                app,
                PocketNumberMagic,
                (uint8_t)app->selection,
                0U,
                attack ? "Spell attack misc" : "Spell save DC misc",
                attack ? character->spell_attack_misc : character->spell_save_misc,
                -20,
                20);
        } else if(app->selection >= 7U && app->selection <= 15U) {
            uint8_t level = app->selection - 6U;
            dndolphins_begin_number(
                app,
                PocketNumberMagic,
                (uint8_t)app->selection,
                1U,
                "Maximum spell slots",
                character->spell_slots_max[level],
                0,
                20);
        }
    } else if(event->type == InputTypeShort && event->key == InputKeyOk) {
        if(app->selection == 0U)
            dndolphins_request_launch(app, PocketPendingLaunchSpellbook);
        else if(app->selection == 6U) {
            if(app->arcane_recovery_active) {
                app->arcane_recovery_active = 0U;
                dndolphins_set_status(
                    app, app->arcane_recovery_spent ? "Arcane Recovery used" : "Recovery skipped");
            } else if(character->arcane_recovery_used) {
                dndolphins_set_status(app, "Recovery already used");
            } else {
                dndolphins_set_status(app, "Finish a Short Rest first");
            }
        } else if(!app->arcane_recovery_active && app->selection >= 7U && app->selection <= 15U) {
            uint8_t level = app->selection - 6U;
            dndolphins_begin_number(
                app,
                PocketNumberMagic,
                (uint8_t)app->selection,
                0U,
                "Available spell slots",
                character->spell_slots_current[level],
                0,
                character->spell_slots_max[level]);
        }
    }
}


static void dndolphins_handle_record_list(PocketD20App* app, const InputEvent* event) {
    uint16_t count = dndolphins_list_count(app) + 1U;
    if(dndolphins_is_move_event(event) && event->key == InputKeyUp) {
        uint16_t previous_selection = app->selection;
        uint16_t previous_scroll = app->scroll;
        dndolphins_menu_move(app, count, -1);
        if(!dndolphins_record_list_prepare_sidecar(app)) {
            app->selection = previous_selection;
            app->scroll = previous_scroll;
            dndolphins_set_status(app, "Page load failed - retry SD");
        }
    } else if(dndolphins_is_move_event(event) && event->key == InputKeyDown) {
        uint16_t previous_selection = app->selection;
        uint16_t previous_scroll = app->scroll;
        dndolphins_menu_move(app, count, 1);
        if(!dndolphins_record_list_prepare_sidecar(app)) {
            app->selection = previous_selection;
            app->scroll = previous_scroll;
            dndolphins_set_status(app, "Page load failed - retry SD");
        }
    } else if(event->type == InputTypeShort && event->key == InputKeyOk) {
        if(app->selection == 0U) {
            if(!dndolphins_add_record(app)) {
                bool full = false;
                if(app->list_kind == PocketListClasses)
                    full = app->data.character.class_count >= POCKET_D20_MAX_CLASSES ||
                           dnd_rules_core_total_level(&app->data.character) >= 20U;
                else if(app->list_kind == PocketListFeatures)
                    full = app->features_total >= POCKET_D20_MAX_FEATURES;
                else if(app->list_kind == PocketListLanguages)
                    full = app->data.character.language_count >= POCKET_D20_MAX_LANGUAGES;
                dndolphins_set_status(app, full ? "List is full" : "Add failed - retry SD");
            }
        } else {
            app->record_index = app->selection - 1U;
            dndolphins_enter_screen(app, PocketScreenRecordDetail);
        }
    }
}


static void dndolphins_adjust_record(PocketD20App* app, int8_t delta) {
    PocketCharacter* character = &app->data.character;
    uint8_t index = app->record_index;
    uint8_t field = app->selection;
    switch(app->list_kind) {
    case PocketListClasses: {
        uint8_t previous_total_level = dnd_rules_core_total_level(character);
        uint8_t previous_pb = dnd_rules_core_proficiency_bonus(character);
        uint8_t previous_slots[POCKET_D20_SLOT_COUNT];
        memcpy(previous_slots, character->spell_slots_max, sizeof(previous_slots));
        PocketClassLevel* class_level = &character->classes[index];
        uint8_t previous_class_level = class_level->level;
        uint8_t previous_cantrip_limit = class_level->cantrip_limit;
        uint8_t previous_prepared_limit = class_level->prepared_limit;
        if(field == 2U) {
            uint8_t total = dnd_rules_core_total_level(character);
            int16_t maximum = 20 - (total - class_level->level);
            class_level->level = (uint8_t)dndolphins_clamp_i16(
                class_level->level + delta, 1, maximum < 1 ? 1 : maximum);
        } else if(field == 3U) {
            class_level->hit_die = dndolphins_cycle_die(class_level->hit_die, delta, true);
        } else if(field == 4U) {
            class_level->hit_dice_current =
                dndolphins_clamp_u8(class_level->hit_dice_current + delta, class_level->hit_dice_max);
        } else if(field == 5U) {
            class_level->hit_dice_max = dndolphins_clamp_u8(class_level->hit_dice_max + delta, 20U);
            if(class_level->hit_dice_current > class_level->hit_dice_max)
                class_level->hit_dice_current = class_level->hit_dice_max;
        } else if(field == 6U) {
            int16_t mode = class_level->spellcasting_mode + delta;
            if(mode < 0) mode = PocketSpellcastingModeCount - 1U;
            if(mode >= PocketSpellcastingModeCount) mode = 0;
            class_level->spellcasting_mode = (uint8_t)mode;
        } else if(field == 7U) {
            int16_t ability = class_level->spellcasting_ability + delta;
            if(ability < 0) ability = PocketAbilityCharisma;
            if(ability > PocketAbilityCharisma) ability = PocketAbilityStrength;
            class_level->spellcasting_ability = (uint8_t)ability;
        } else if(field == 8U) {
            class_level->cantrip_limit = dndolphins_clamp_u8(class_level->cantrip_limit + delta, 30U);
        } else if(field == 9U) {
            class_level->prepared_limit =
                dndolphins_clamp_u8(class_level->prepared_limit + delta, 50U);
        } else if(field == 10U) {
            class_level->spellbook_size =
                (uint16_t)dndolphins_clamp_i16(class_level->spellbook_size + delta, 0, 999);
        } else if(field == 11U) {
            class_level->pact_slot_level =
                dndolphins_clamp_u8(class_level->pact_slot_level + delta, 5U);
        } else if(field == 12U) {
            class_level->pact_slots_current = dndolphins_clamp_u8(
                class_level->pact_slots_current + delta, class_level->pact_slots_max);
        } else if(field == 13U) {
            class_level->pact_slots_max = dndolphins_clamp_u8(class_level->pact_slots_max + delta, 8U);
            if(class_level->pact_slots_current > class_level->pact_slots_max)
                class_level->pact_slots_current = class_level->pact_slots_max;
        } else if(field == 14U) {
            uint8_t level = delta > 0 ? 6U : 9U;
            class_level->mystic_arcanum_mask ^= (uint16_t)(1U << level);
        } else if(field == 15U) {
            class_level->spell_points_current = (uint16_t)dndolphins_clamp_i16(
                class_level->spell_points_current + delta, 0, class_level->spell_points_max);
        } else if(field == 16U) {
            class_level->spell_points_max =
                (uint16_t)dndolphins_clamp_i16(class_level->spell_points_max + delta, 0, 999);
            if(class_level->spell_points_current > class_level->spell_points_max)
                class_level->spell_points_current = class_level->spell_points_max;
        } else {
            return;
        }
        if(field == 2U) {
            uint8_t current_total_level = dnd_rules_core_total_level(character);
            if(current_total_level > previous_total_level)
                dndolphins_rules_character_apply_level_increase(
                    character, index, previous_class_level);
            dndolphins_spells_apply_level_progression(character, index);
            if(current_total_level > previous_total_level) {
                dndolphins_rules_character_apply_experience_floor(character);
                dndolphins_begin_level_review(
                    app,
                    index,
                    previous_class_level,
                    previous_pb,
                    previous_cantrip_limit,
                    previous_prepared_limit,
                    previous_slots);
            }
        }
        break;
    }
    case PocketListFeatures: {
        PocketFeature* feature = dndolphins_feature_at(app, index, NULL);
        if(!feature) return;
        if(field == 2U) {
            int16_t class_index = feature->class_index + delta;
            if(class_index < 0) class_index = character->class_count - 1U;
            if(class_index >= character->class_count) class_index = 0;
            feature->class_index = (uint8_t)class_index;
        } else if(field == 3U) {
            feature->class_level_gained =
                dndolphins_clamp_u8(feature->class_level_gained + delta, 20U);
        } else if(field == 4U) {
            feature->uses_current =
                dndolphins_clamp_i16(feature->uses_current + delta, 0, feature->uses_max);
        } else if(field == 5U) {
            feature->uses_max = dndolphins_clamp_i16(feature->uses_max + delta, 0, 99);
            if(feature->uses_current > feature->uses_max)
                feature->uses_current = feature->uses_max;
        } else if(field == 6U) {
            int16_t recharge = feature->recharge + delta;
            if(recharge < PocketRechargeManual) recharge = PocketRechargeCount - 1U;
            if(recharge >= PocketRechargeCount) recharge = PocketRechargeManual;
            feature->recharge = (uint8_t)recharge;
        } else if(field == 7U) {
            int16_t formula = feature->resource_formula + delta;
            if(formula < 0) formula = PocketResourceFormulaCount - 1U;
            if(formula >= PocketResourceFormulaCount) formula = 0;
            feature->resource_formula = (uint8_t)formula;
            feature->uses_max = dndolphins_rules_character_feature_max_uses(character, feature);
        } else if(field == 8U) {
            int16_t ability = feature->resource_ability + delta;
            if(ability < 0) ability = PocketAbilityCharisma;
            if(ability > PocketAbilityCharisma) ability = PocketAbilityStrength;
            feature->resource_ability = (uint8_t)ability;
            feature->uses_max = dndolphins_rules_character_feature_max_uses(character, feature);
        } else {
            return;
        }
        (void)dndolphins_save_features_if_changed(app);
        break;
    }
    case PocketListLanguages:
    }
    dndolphins_save(app, false);
}

static void dndolphins_handle_record_detail_ok(PocketD20App* app) {
    PocketCharacter* character = &app->data.character;
    uint8_t index = app->record_index;
    uint8_t field = app->selection;
    switch(app->list_kind) {
    case PocketListClasses:
        if(field == 0U)
            dndolphins_open_catalog(
                app, PocketCatalogClasses, PocketEditClassName, character->classes[index].name);
        else if(field == 1U)
            dndolphins_open_catalog(
                app,
                PocketCatalogSubclasses,
                PocketEditSubclass,
                character->classes[index].subclass);
        else if(field >= 2U && field <= 16U)
            dndolphins_adjust_record(app, 1);
        else
            dndolphins_delete_record(app);
        break;
    case PocketListFeatures: {
        PocketFeature* feature = dndolphins_feature_at(app, index, NULL);
        if(!feature) return;
        if(field == 0U)
            dndolphins_open_catalog(app, PocketCatalogFeats, PocketEditFeatureName, feature->name);
        else if(field == 1U)
            dndolphins_begin_text(app, PocketEditFeatureDetail, "Feature notes", feature->detail);
        else if(field < 9U)
            dndolphins_adjust_record(app, 1);
        else
            dndolphins_delete_record(app);
        break;
    }
    case PocketListLanguages:
        if(field == 0U)
            dndolphins_begin_text(
                app, PocketEditLanguageName, "Language", character->languages[index]);
        else
            dndolphins_delete_record(app);
        break;
    }
}

static bool dndolphins_begin_record_number(PocketD20App* app) {
    PocketCharacter* character = &app->data.character;
    uint8_t index = app->record_index;
    uint8_t field = (uint8_t)app->selection;
    const char* header = NULL;
    int32_t value = 0;
    int32_t minimum = 0;
    int32_t maximum = 999;
    if(app->list_kind == PocketListClasses) {
        PocketClassLevel* level = &character->classes[index];
        switch(field) {
        case 2U:
            header = "Class level";
            value = level->level;
            maximum = 20 - (dnd_rules_core_total_level(character) - level->level);
            if(maximum < 1) maximum = 1;
            minimum = 1;
            break;
        case 3U:
            header = "Hit Point Die";
            value = level->hit_die;
            minimum = 4;
            maximum = 12;
            break;
        case 4U:
            header = "Hit Dice current";
            value = level->hit_dice_current;
            maximum = level->hit_dice_max;
            break;
        case 5U:
            header = "Hit Dice maximum";
            value = level->hit_dice_max;
            maximum = 20;
            break;
        case 8U:
            header = "Cantrip limit";
            value = level->cantrip_limit;
            maximum = 30;
            break;
        case 9U:
            header = "Prepared limit";
            value = level->prepared_limit;
            maximum = 50;
            break;
        case 10U:
            header = "Spellbook size";
            value = level->spellbook_size;
            break;
        case 11U:
            header = "Pact slot level";
            value = level->pact_slot_level;
            maximum = 5;
            break;
        case 12U:
            header = "Pact slots current";
            value = level->pact_slots_current;
            maximum = level->pact_slots_max;
            break;
        case 13U:
            header = "Pact slots maximum";
            value = level->pact_slots_max;
            maximum = 8;
            break;
        case 15U:
            header = "Spell points current";
            value = level->spell_points_current;
            maximum = level->spell_points_max;
            break;
        case 16U:
            header = "Spell points maximum";
            value = level->spell_points_max;
            break;
        default:
            return false;
        }
    } else if(app->list_kind == PocketListFeatures) {
        PocketFeature* feature = dndolphins_feature_at(app, index, NULL);
        if(!feature) return false;
        if(field == 3U) {
            header = "Class level gained";
            value = feature->class_level_gained;
            maximum = 20;
        } else if(field == 4U) {
            header = "Uses current";
            value = feature->uses_current;
            maximum = feature->uses_max;
        } else if(field == 5U) {
            header = "Uses maximum";
            value = feature->uses_max;
            maximum = 99;
        } else {
            return false;
        }
    } else {
        return false;
    }
    dndolphins_begin_number(app, PocketNumberRecord, field, 0U, header, value, minimum, maximum);
    return true;
}

static void dndolphins_handle_record_detail_custom_name(PocketD20App* app) {
    PocketCharacter* character = &app->data.character;
    uint8_t index = app->record_index;
    if(app->list_kind == PocketListClasses && app->selection == 0U)
        dndolphins_begin_text(
            app, PocketEditClassName, "Custom class", character->classes[index].name);
    else if(app->list_kind == PocketListClasses && app->selection == 1U)
        dndolphins_begin_text(
            app, PocketEditSubclass, "Custom subclass", character->classes[index].subclass);
    else if(app->list_kind == PocketListFeatures && app->selection == 0U) {
        PocketFeature* feature = dndolphins_feature_at(app, index, NULL);
        if(feature) dndolphins_begin_text(app, PocketEditFeatureName, "Custom feat/perk", feature->name);
    }
}

static void dndolphins_handle_record_detail(PocketD20App* app, const InputEvent* event) {
    uint8_t count = dndolphins_record_detail_count(app);
    if(dndolphins_is_move_event(event) && event->key == InputKeyUp)
        dndolphins_menu_move(app, count, -1);
    else if(dndolphins_is_move_event(event) && event->key == InputKeyDown)
        dndolphins_menu_move(app, count, 1);
    else if(dndolphins_is_move_event(event) && (event->key == InputKeyLeft || event->key == InputKeyRight))
        dndolphins_adjust_record(app, event->key == InputKeyRight ? 1 : -1);
    else if(event->type == InputTypeLong && event->key == InputKeyOk) {
        if(!dndolphins_begin_record_number(app)) dndolphins_handle_record_detail_custom_name(app);
    } else if(event->type == InputTypeShort && event->key == InputKeyOk)
        dndolphins_handle_record_detail_ok(app);
}

static void dndolphins_apply_catalog_selection(PocketD20App* app) {
    if(app->selection >= app->catalog_count) return;
    char selected[POCKET_D20_CATALOG_NAME_LEN];
    dndolphins_copy(selected, sizeof(selected), app->catalog_entries[app->selection]);
    PocketCharacter* character = &app->data.character;
    uint8_t index = app->record_index;
    uint8_t grant_source = PocketGrantSourceCount;
    bool save_features_after_catalog = false;
    switch(app->catalog_target) {
    case PocketEditClassName: {
        bool newly_added_level =
            !strcmp(character->classes[index].name, "New Class") &&
            character->classes[index].level == 1U &&
            character->hit_dice_max < dnd_rules_core_total_level(character);
        dndolphins_copy(
            character->classes[index].name, sizeof(character->classes[index].name), selected);
        dndolphins_configure_class_defaults(&character->classes[index]);
        if(newly_added_level) {
            dndolphins_rules_character_apply_level_increase(character, index, 0U);
            dndolphins_rules_character_apply_experience_floor(character);
        }
        dndolphins_spells_initialize_spell_slots_if_unset(character);
        dndolphins_spells_apply_level_progression(character, index);
        /* Level-1 traits are intentionally gated behind Character > Grant Initial Traits. */
        break;
    }
    case PocketEditSubclass:
        dndolphins_copy(
            character->classes[index].subclass,
            sizeof(character->classes[index].subclass),
            selected);
        dndolphins_spells_refresh_class_spellcasting(&character->classes[index]);
        dndolphins_spells_initialize_spell_slots_if_unset(character);
        dndolphins_spells_apply_level_progression(character, index);
        /* Subclass grants are explicit through Grant Initial Traits / Apply Level Grants. */
        break;
    case PocketEditSpecies:
        dndolphins_copy(character->species, sizeof(character->species), selected);
        /* Species traits are intentionally gated behind Grant Initial Traits. */
        break;
    case PocketEditFeatureName:
        grant_source = PocketGrantFeat;
        if(app->level_choice_mode == 3U) {
            if(!character->feature_count || !character->features) {
                dndolphins_set_status(app, "Feat choice unavailable");
                return;
            }
            PocketFeature feature = character->features[0];
            dndolphins_copy(feature.name, sizeof(feature.name), selected);
            if(!dndolphins_progression_store_features_append(
                   app->storage, app->profiles.active_profile, &feature) ||
               !dndolphins_complete_level_choice(app, "feat")) {
                dndolphins_set_status(app, "Could not record feat choice");
                return;
            }
            dnd_data_reserve_features_exact(character, 0U);
            character->feature_count = 0U;
            app->features_total = 0U;
            (void)dndolphins_progression_store_features_count(
                app->storage, app->profiles.active_profile, &app->features_total);
            app->level_choice_mode = 0U;
            app->return_screen = PocketScreenCharacter;
        } else {
            PocketFeature* feature = dndolphins_feature_at(app, index, NULL);
            if(!feature) {
                dndolphins_set_status(app, "Feature read failed");
                return;
            }
            dndolphins_copy(feature->name, sizeof(feature->name), selected);
            save_features_after_catalog = true;
        }
        break;
    case PocketEditBackground:
        dndolphins_copy(character->background, sizeof(character->background), selected);
        /* Background traits are intentionally gated behind Grant Initial Traits. */
        break;
    case PocketEditAlignment:
        dndolphins_copy(character->alignment, sizeof(character->alignment), selected);
        break;
    default:
        return;
    }
    dndolphins_catalog_release(app);
    if(save_features_after_catalog) (void)dndolphins_save_features_if_changed(app);
    if(grant_source < PocketGrantSourceCount) dndolphins_release_pending_grants(app);
    uint8_t staged = grant_source < PocketGrantSourceCount ?
                         dndolphins_stage_grants(app, grant_source, selected) :
                         0U;
    dndolphins_save(app, false);
    PocketScreen destination = app->return_screen;
    if(staged) {
        app->return_screen = destination;
        dndolphins_enter_screen(app, PocketScreenGrantReview);
        snprintf(app->status, sizeof(app->status), "%u grants to review", staged);
        return;
    }
    dndolphins_enter_screen(app, destination);
    app->selection = app->catalog_return_selection;
    if(app->selection >= 5U) app->scroll = app->selection - 4U;
    dndolphins_set_status(app, "Catalog choice saved");
}

static void dndolphins_handle_catalog(PocketD20App* app, const InputEvent* event) {
    if(app->catalog_count && dndolphins_is_move_event(event) && event->key == InputKeyUp)
        dndolphins_menu_move(app, app->catalog_count, -1);
    else if(app->catalog_count && dndolphins_is_move_event(event) && event->key == InputKeyDown)
        dndolphins_menu_move(app, app->catalog_count, 1);
    else if(
        dndolphins_is_move_event(event) &&
        (event->key == InputKeyLeft || event->key == InputKeyRight)) {
        uint16_t next_start = app->catalog_page_start;
        uint16_t page_limit = dndolphins_catalog_page_limit(app);
        if(event->key == InputKeyRight && app->catalog_has_more)
            next_start += page_limit;
        else if(event->key == InputKeyLeft && app->catalog_page_start >= page_limit)
            next_start -= page_limit;
        if(next_start != app->catalog_page_start) {
            app->catalog_page_start = next_start;
            app->selection = 0U;
            app->scroll = 0U;
            dndolphins_catalog_load_page(app);
        }
    } else if(
        event->type == InputTypeLong && event->key == InputKeyOk &&
        (app->catalog_kind == PocketCatalogSubclasses ||
         (app->catalog_kind == PocketCatalogFeats && app->level_choice_mode == 3U))) {
        app->catalog_show_all = !app->catalog_show_all;
        app->catalog_page_start = 0U;
        app->selection = 0U;
        app->scroll = 0U;
        dndolphins_catalog_load_page(app);
        if(app->catalog_kind == PocketCatalogFeats)
            dndolphins_set_status(app, app->catalog_show_all ? "All feats" : "Allowed feats");
        else
            dndolphins_set_status(
                app, app->catalog_show_all ? "Showing all" : "Class filter");
    } else if(event->type == InputTypeShort && event->key == InputKeyOk)
        dndolphins_apply_catalog_selection(app);
}


static void dndolphins_handle_spell_attacks(PocketD20App* app, const InputEvent* event) {
    uint8_t count = dndolphins_combat_spell_count(app);
    if(!count) return;
    if(dndolphins_is_move_event(event) && event->key == InputKeyUp) {
        dndolphins_menu_move(app, count, -1);
        dndolphins_prepare_combat_spell_rows(app, false);
    } else if(dndolphins_is_move_event(event) && event->key == InputKeyDown) {
        dndolphins_menu_move(app, count, 1);
        dndolphins_prepare_combat_spell_rows(app, false);
    }
    else if(event->type == InputTypeShort && event->key == InputKeyOk) {
        uint8_t spell_index = dndolphins_combat_spell_index(app, app->selection);
        if(spell_index == 0xFFU) return;
        app->spell_attack_index = spell_index;
        PocketSpellCastOption options[POCKET_D20_MAX_SPELL_CAST_OPTIONS];
        uint8_t option_count = dndolphins_build_spell_cast_options(
            app, spell_index, options, POCKET_D20_MAX_SPELL_CAST_OPTIONS);
        if(!option_count) {
            dndolphins_set_status(app, "No casting resource");
            return;
        }
        if(option_count == 1U) {
            dndolphins_cast_spell(app, &options[0]);
            return;
        }
        dndolphins_enter_screen(app, PocketScreenSpellCast);
    }
}

static void dndolphins_handle_rituals(PocketD20App* app, const InputEvent* event) {
    uint8_t count = dndolphins_combat_spell_count(app);
    if(!count) return;
    if(dndolphins_is_move_event(event) && event->key == InputKeyUp) {
        dndolphins_menu_move(app, count, -1);
        dndolphins_prepare_combat_spell_rows(app, true);
    } else if(dndolphins_is_move_event(event) && event->key == InputKeyDown) {
        dndolphins_menu_move(app, count, 1);
        dndolphins_prepare_combat_spell_rows(app, true);
    }
    else if(event->type == InputTypeShort && event->key == InputKeyOk) {
        uint8_t spell_index = dndolphins_combat_spell_index(app, app->selection);
        if(spell_index == 0xFFU) return;
        PocketSpell* spell = dndolphins_spell_at(app, spell_index, NULL);
        if(!spell) {
            dndolphins_set_status(app, "Spell read failed");
            return;
        }
        app->spell_attack_index = spell_index;
        PocketSpellCastOption option = {
            .level = spell->level,
            .resource = PocketSpellCastRitual,
            .class_index = spell->class_index,
        };
        dndolphins_cast_spell(app, &option);
    }
}

static void dndolphins_handle_spell_cast(PocketD20App* app, const InputEvent* event) {
    PocketSpellCastOption options[POCKET_D20_MAX_SPELL_CAST_OPTIONS];
    uint8_t count = dndolphins_build_spell_cast_options(
        app, app->spell_attack_index, options, POCKET_D20_MAX_SPELL_CAST_OPTIONS);
    if(count > POCKET_D20_MAX_SPELL_CAST_OPTIONS) count = POCKET_D20_MAX_SPELL_CAST_OPTIONS;
    if(!count) {
        dndolphins_enter_screen(app, PocketScreenSpellAttacks);
        dndolphins_set_status(app, "No casting resource");
        return;
    }
    if(dndolphins_is_move_event(event) && event->key == InputKeyUp)
        dndolphins_menu_move(app, count, -1);
    else if(dndolphins_is_move_event(event) && event->key == InputKeyDown)
        dndolphins_menu_move(app, count, 1);
    else if(event->type == InputTypeShort && event->key == InputKeyOk && app->selection < count)
        dndolphins_cast_spell(app, &options[app->selection]);
}

static void dndolphins_handle_spell_result(PocketD20App* app, const InputEvent* event) {
    if(app->spell_cast_resource == PocketSpellCastRitual &&
       event->type == InputTypeShort && event->key == InputKeyOk) {
        dndolphins_enter_screen(app, PocketScreenRituals);
        return;
    }
    if(app->spell_cast_attack_roll_count > 4U && dndolphins_is_move_event(event)) {
        uint8_t maximum_scroll = app->spell_cast_attack_roll_count - 4U;
        if(event->key == InputKeyUp) {
            if(app->scroll) --app->scroll;
            return;
        }
        if(event->key == InputKeyDown) {
            if(app->scroll < maximum_scroll) ++app->scroll;
            return;
        }
    }
    if(event->type != InputTypeShort || event->key != InputKeyOk) return;
    PocketSpellCastOption options[POCKET_D20_MAX_SPELL_CAST_OPTIONS];
    uint8_t count = dndolphins_build_spell_cast_options(
        app, app->spell_attack_index, options, POCKET_D20_MAX_SPELL_CAST_OPTIONS);
    if(count == 1U) {
        dndolphins_enter_screen(app, PocketScreenSpellCast);
        app->selection = 0U;
    } else if(count > 1U) {
        dndolphins_enter_screen(app, PocketScreenSpellCast);
    } else {
        dndolphins_enter_screen(app, PocketScreenSpellAttacks);
    }
}

static void dndolphins_handle_combat(PocketD20App* app, const InputEvent* event) {
    PocketCharacter* character = &app->data.character;
    if(dndolphins_is_move_event(event) && event->key == InputKeyUp)
        dndolphins_menu_move(app, DndolphinsCombatCount, -1);
    else if(dndolphins_is_move_event(event) && event->key == InputKeyDown)
        dndolphins_menu_move(app, DndolphinsCombatCount, 1);
    else if(
        dndolphins_is_move_event(event) &&
        (event->key == InputKeyLeft || event->key == InputKeyRight)) {
        int16_t delta = event->key == InputKeyRight ? 1 : -1;
        if(app->selection == DndolphinsCombatAttackMode) {
            int16_t mode = app->roll_mode + delta;
            if(mode < PocketRollNormal) mode = PocketRollDisadvantage;
            if(mode > PocketRollDisadvantage) mode = PocketRollNormal;
            app->roll_mode = (PocketRollMode)mode;
            return;
        } else if(app->selection == DndolphinsCombatHp)
            character->hp_current = dndolphins_clamp_i16(character->hp_current + delta, 0, 999);
        else if(app->selection == DndolphinsCombatTemporaryHp)
            character->hp_temporary = dndolphins_clamp_i16(character->hp_temporary + delta, 0, 999);
        else if(app->selection == DndolphinsCombatSpendHitDie) {
            int16_t class_index = app->hit_die_class_index + delta;
            if(class_index < 0) class_index = character->class_count - 1U;
            if(class_index >= character->class_count) class_index = 0;
            app->hit_die_class_index = (uint8_t)class_index;
            return;
        } else if(app->selection == DndolphinsCombatReaction)
            character->reaction_available = !character->reaction_available;
        else if(app->selection == DndolphinsCombatDeathSuccesses)
            character->death_successes = dndolphins_clamp_u8(character->death_successes + delta, 3U);
        else if(app->selection == DndolphinsCombatDeathFailures)
            character->death_failures = dndolphins_clamp_u8(character->death_failures + delta, 3U);
        else if(app->selection == DndolphinsCombatExhaustion)
            character->exhaustion = dndolphins_clamp_u8(character->exhaustion + delta, 6U);
        else
            return;
        dndolphins_save(app, false);
    } else if(
        event->type == InputTypeLong && event->key == InputKeyOk &&
        (app->selection == DndolphinsCombatHp || app->selection == DndolphinsCombatTemporaryHp ||
         (app->selection >= DndolphinsCombatDeathSuccesses && app->selection <= DndolphinsCombatExhaustion))) {
        const char* header = app->selection == DndolphinsCombatHp  ? "Current HP" :
                             app->selection == DndolphinsCombatTemporaryHp  ? "Temporary HP" :
                             app->selection == DndolphinsCombatDeathSuccesses ? "Death successes" :
                             app->selection == DndolphinsCombatDeathFailures ? "Death failures" :
                                                     "Exhaustion";
        int32_t value = app->selection == DndolphinsCombatHp  ? character->hp_current :
                        app->selection == DndolphinsCombatTemporaryHp  ? character->hp_temporary :
                        app->selection == DndolphinsCombatDeathSuccesses ? character->death_successes :
                        app->selection == DndolphinsCombatDeathFailures ? character->death_failures :
                                                character->exhaustion;
        int32_t maximum = app->selection <= DndolphinsCombatTemporaryHp ? 999 : app->selection <= DndolphinsCombatDeathFailures ? 3 : 6;
        dndolphins_begin_number(
            app, PocketNumberCombat, (uint8_t)app->selection, 0U, header, value, 0, maximum);
    } else if(event->type == InputTypeShort && event->key == InputKeyOk) {
        switch(app->selection) {
        case DndolphinsCombatAttackMode: {
            int16_t mode = app->roll_mode + 1;
            if(mode > PocketRollDisadvantage) mode = PocketRollNormal;
            app->roll_mode = (PocketRollMode)mode;
            break;
        }
        case DndolphinsCombatWeaponAttacks:
            dndolphins_enter_screen(app, PocketScreenAttackList);
            break;
        case DndolphinsCombatSpellAttacks:
            dndolphins_enter_screen(app, PocketScreenSpellAttacks);
            break;
        case DndolphinsCombatRituals:
            dndolphins_enter_screen(app, PocketScreenRituals);
            break;
        case DndolphinsCombatAttackTemplates:
            dndolphins_enter_screen(app, PocketScreenAttackTemplates);
            break;
        case DndolphinsCombatInitiative:
            dndolphins_request_launch(app, PocketPendingLaunchInitiative);
            break;
        case DndolphinsCombatHp:
            character->hp_current = dndolphins_clamp_i16(character->hp_current + 1, 0, 999);
            dndolphins_save(app, false);
            break;
        case DndolphinsCombatTemporaryHp:
            character->hp_temporary = dndolphins_clamp_i16(character->hp_temporary + 1, 0, 999);
            dndolphins_save(app, false);
            break;
        case DndolphinsCombatShortRest:
            if(character->hp_current < 1) {
                dndolphins_set_status(app, "Need at least 1 HP");
                break;
            }
            dndolphins_rules_character_short_rest(character);
            if(!dndolphins_progression_store_features_recharge(
                   app->storage,
                   app->profiles.active_profile,
                   character,
                   DndFeatureRechargeShortRest)) {
                dndolphins_set_status(app, "Feature recharge failed");
                break;
            }
            dndolphins_save(app, false);
            if(dndolphins_wizard_level(character) && !character->arcane_recovery_used &&
               dndolphins_begin_arcane_recovery(app))
                break;
            dndolphins_confirm_action(app, "Short rest applied");
            break;
        case DndolphinsCombatSpendHitDie:
            if(character->hp_current < 1) {
                dndolphins_set_status(app, "Need at least 1 HP");
            } else if(character->hp_current >= character->hp_max) {
                dndolphins_set_status(app, "HP already full");
            } else if(!character->classes[app->hit_die_class_index].hit_dice_current) {
                dndolphins_set_status(app, "No Hit Dice left");
            } else {
                uint8_t roll = 0U;
                int16_t healed =
                    dndolphins_rules_character_spend_class_hit_die(character, app->hit_die_class_index, &roll);
                int8_t constitution = dnd_rules_core_ability_modifier(
                    character->ability_scores[PocketAbilityConstitution]);
                dndolphins_save(app, false);
                snprintf(
                    app->status,
                    sizeof(app->status),
                    "d%u:%u %+d, healed %d",
                    character->classes[app->hit_die_class_index].hit_die,
                    roll,
                    constitution,
                    healed);
                dndolphins_start_dice_animation(
                    app, 1U, character->classes[app->hit_die_class_index].hit_die);
            }
            break;
        case DndolphinsCombatLongRest:
            if(character->hp_current < 1) {
                dndolphins_set_status(app, "Need at least 1 HP");
                break;
            }
            dndolphins_rules_character_long_rest(character);
            if(!dndolphins_progression_store_features_recharge(
                   app->storage,
                   app->profiles.active_profile,
                   character,
                   DndFeatureRechargeLongRest)) {
                dndolphins_set_status(app, "Feature recharge failed");
                break;
            }
            if(!dnd_storage_reset_spell_free_casts(
                   app->storage, app->profiles.active_profile, character)) {
                dndolphins_set_status(app, "Spellbook update failed");
                break;
            }
            dndolphins_save(app, false);
            dndolphins_confirm_action(app, "Long rest applied");
            break;
        case DndolphinsCombatConditions:
            dndolphins_begin_text(app, PocketEditConditions, "Conditions", character->conditions);
            break;
        case DndolphinsCombatConcentration:
            dndolphins_begin_text(
                app, PocketEditConcentration, "Concentration", character->concentration);
            break;
        case DndolphinsCombatReaction:
            character->reaction_available = !character->reaction_available;
            dndolphins_save(app, false);
            break;
        case DndolphinsCombatTemporaryEffects:
            dndolphins_begin_text(
                app, PocketEditTemporaryEffects, "Temporary effects", character->temporary_effects);
            break;
        case DndolphinsCombatResistances:
            dndolphins_begin_text(app, PocketEditResistances, "Resistances", character->resistances);
            break;
        case DndolphinsCombatImmunities:
            dndolphins_begin_text(app, PocketEditImmunities, "Immunities", character->immunities);
            break;
        case DndolphinsCombatVulnerabilities:
            dndolphins_begin_text(
                app, PocketEditVulnerabilities, "Vulnerabilities", character->vulnerabilities);
            break;
        case DndolphinsCombatSenses:
            dndolphins_begin_text(app, PocketEditSenses, "Senses", character->senses);
            break;
        case DndolphinsCombatMovement:
            dndolphins_begin_text(
                app, PocketEditMovementModes, "Movement modes", character->movement_modes);
            break;
        case DndolphinsCombatDeathSuccesses:
            character->death_successes = dndolphins_clamp_u8(character->death_successes + 1, 3U);
            dndolphins_save(app, false);
            break;
        case DndolphinsCombatDeathFailures:
            character->death_failures = dndolphins_clamp_u8(character->death_failures + 1, 3U);
            dndolphins_save(app, false);
            break;
        case DndolphinsCombatExhaustion:
            character->exhaustion = dndolphins_clamp_u8(character->exhaustion + 1, 6U);
            dndolphins_save(app, false);
            break;
        }
    }
}

static void dndolphins_roll_generic(PocketD20App* app) {
    app->dice_first = 0U;
    app->dice_second = 0U;
    app->dice_guidance = 0U;
    app->dice_roll_value_count = 0U;
    app->dice_roll_sum = 0U;
    memset(app->dice_roll_values, 0, sizeof(app->dice_roll_values));
    if(app->roll_mode == PocketRollGuidance && app->dice_count == 1U && app->dice_sides == 20U) {
        app->dice_first = (uint8_t)dnd_rules_core_roll_dice(1U, 20U);
        app->dice_guidance = (uint8_t)dnd_rules_core_roll_dice(1U, 4U);
        app->dice_roll_values[0] = app->dice_first;
        app->dice_roll_values[1] = app->dice_guidance;
        app->dice_roll_value_count = 2U;
        app->dice_roll_sum = app->dice_first + app->dice_guidance;
        app->dice_result = (int16_t)app->dice_roll_sum + app->dice_modifier;
    } else if(
        (app->roll_mode == PocketRollAdvantage || app->roll_mode == PocketRollDisadvantage) &&
        app->dice_count == 1U && app->dice_sides == 20U) {
        app->dice_first = (uint8_t)dnd_rules_core_roll_dice(1U, 20U);
        app->dice_second = (uint8_t)dnd_rules_core_roll_dice(1U, 20U);
        app->dice_roll_values[0] = app->dice_first;
        app->dice_roll_values[1] = app->dice_second;
        app->dice_roll_value_count = 2U;
        app->dice_roll_sum = app->dice_first + app->dice_second;
        uint8_t chosen =
            app->roll_mode == PocketRollAdvantage ?
                (app->dice_first > app->dice_second ? app->dice_first : app->dice_second) :
                (app->dice_first < app->dice_second ? app->dice_first : app->dice_second);
        app->dice_result = chosen + app->dice_modifier;
    } else {
        app->dice_roll_value_count = app->dice_count;
        app->dice_roll_sum = dndolphins_dice_roll_values(
            app->dice_count, app->dice_sides, app->dice_roll_values, sizeof(app->dice_roll_values));
        if(app->dice_count == 1U) app->dice_first = app->dice_roll_values[0];
        app->dice_result = (int16_t)app->dice_roll_sum + app->dice_modifier;
    }
    dndolphins_enter_screen(app, PocketScreenDiceResult);
    dndolphins_start_dice_animation(app, app->dice_roll_value_count, app->dice_sides);
}

static void dndolphins_handle_dice(PocketD20App* app, const InputEvent* event) {
    if(dndolphins_is_move_event(event) && event->key == InputKeyUp)
        dndolphins_menu_move(app, 5U, -1);
    else if(dndolphins_is_move_event(event) && event->key == InputKeyDown)
        dndolphins_menu_move(app, 5U, 1);
    else if(
        dndolphins_is_move_event(event) &&
        (event->key == InputKeyLeft || event->key == InputKeyRight)) {
        int8_t delta = event->key == InputKeyRight ? 1 : -1;
        if(app->selection == 0U) {
            app->dice_count = (uint8_t)dndolphins_clamp_i16(app->dice_count + delta, 1, 20);
            if(app->roll_mode != PocketRollNormal) app->roll_mode = PocketRollNormal;
        } else if(app->selection == 1U) {
            app->dice_sides = dndolphins_cycle_die(app->dice_sides, delta, false);
            if(app->roll_mode != PocketRollNormal) app->roll_mode = PocketRollNormal;
        } else if(app->selection == 2U)
            app->dice_modifier = dndolphins_clamp_i16(app->dice_modifier + delta, -99, 99);
        else if(app->selection == 3U) {
            int16_t mode = app->roll_mode + delta;
            if(mode < 0) mode = PocketRollGuidance;
            if(mode > PocketRollGuidance) mode = PocketRollNormal;
            app->roll_mode = (PocketRollMode)mode;
            if(app->roll_mode != PocketRollNormal) {
                app->dice_count = 1U;
                app->dice_sides = 20U;
            }
        } else {
            return;
        }
        app->dice_result = 0;
        app->dice_second = 0U;
        app->dice_guidance = 0U;
        app->dice_roll_value_count = 0U;
    } else if(event->type == InputTypeLong && event->key == InputKeyOk && app->selection <= 2U) {
        const char* header = app->selection == 0U ? "Dice count" :
                             app->selection == 1U ? "Die sides" :
                                                    "Roll modifier";
        int32_t value = app->selection == 0U ? app->dice_count :
                        app->selection == 1U ? app->dice_sides :
                                               app->dice_modifier;
        int32_t minimum = app->selection == 0U ? 1 : app->selection == 1U ? 2 : -99;
        int32_t maximum = app->selection == 0U ? 20 : app->selection == 1U ? 100 : 99;
        dndolphins_begin_number(
            app, PocketNumberDice, (uint8_t)app->selection, 0U, header, value, minimum, maximum);
    } else if(event->type == InputTypeShort && event->key == InputKeyOk) {
        if(app->selection == 4U) dndolphins_roll_generic(app);
    }
}

static void dndolphins_handle_dice_result(PocketD20App* app, const InputEvent* event) {
    if(event->type == InputTypeShort && event->key == InputKeyOk) dndolphins_roll_generic(app);
}

typedef struct {
    const char* ammunition_group;
    uint8_t logical_index;
    bool found;
} DndDolphinsAmmunitionLookup;

static char dndolphins_ascii_lower(char value) {
    return value >= 'A' && value <= 'Z' ? (char)(value - 'A' + 'a') : value;
}

static bool dndolphins_contains_case_insensitive(const char* text, const char* token) {
    if(!text || !token || !token[0]) return false;
    for(const char* start = text; *start; ++start) {
        const char* left = start;
        const char* right = token;
        while(*left && *right &&
              dndolphins_ascii_lower(*left) == dndolphins_ascii_lower(*right)) {
            ++left;
            ++right;
        }
        if(!*right) return true;
    }
    return false;
}

static bool dndolphins_ammunition_name_matches(const char* name, const char* group) {
    if(!name || !group || !group[0]) return false;
    const char* keyword = NULL;
    if(dndolphins_contains_case_insensitive(group, "arrow"))
        keyword = "arrow";
    else if(dndolphins_contains_case_insensitive(group, "bolt"))
        keyword = "bolt";
    else if(dndolphins_contains_case_insensitive(group, "bullet"))
        keyword = "bullet";
    else if(dndolphins_contains_case_insensitive(group, "needle"))
        keyword = "needle";
    if(keyword) return dndolphins_contains_case_insensitive(name, keyword);
    if(dndolphins_contains_case_insensitive(name, group)) return true;

    char singular[POCKET_D20_SHORT_LEN];
    dndolphins_copy(singular, sizeof(singular), group);
    size_t length = strlen(singular);
    if(length > 1U && (singular[length - 1U] == 's' || singular[length - 1U] == 'S')) {
        singular[length - 1U] = '\0';
        return dndolphins_contains_case_insensitive(name, singular);
    }
    return false;
}

static const char* dndolphins_weapon_ammunition_group(const PocketItem* weapon) {
    if(!weapon) return "";
    if(weapon->ammunition_group[0]) return weapon->ammunition_group;
    if(!(weapon->weapon_properties & PocketWeaponAmmunition)) return "";

    /* Old/custom Item records can carry the Ammunition property without the
       newer group field. Derive only the standard weapon-family token so those
       characters can use loose stacks without rewriting their Inventory. */
    if(dndolphins_contains_case_insensitive(weapon->name, "crossbow")) return "bolt";
    if(dndolphins_contains_case_insensitive(weapon->name, "bow")) return "arrow";
    if(dndolphins_contains_case_insensitive(weapon->name, "blowgun")) return "needle";
    if(dndolphins_contains_case_insensitive(weapon->name, "sling")) return "bullet";
    if(dndolphins_contains_case_insensitive(weapon->name, "musket") ||
       dndolphins_contains_case_insensitive(weapon->name, "pistol"))
        return "bullet";
    return "";
}

static bool dndolphins_ammunition_stack_visitor(
    uint8_t logical_index, const PocketItem* item, void* context) {
    DndDolphinsAmmunitionLookup* lookup = context;
    if(!lookup || !item || !lookup->ammunition_group) return false;
    if(!item->is_weapon && item->quantity > 0 &&
       (dndolphins_ammunition_name_matches(item->name, lookup->ammunition_group) ||
        (item->ammunition_group[0] &&
         dndolphins_ammunition_name_matches(
             item->ammunition_group, lookup->ammunition_group)))) {
        lookup->logical_index = logical_index;
        lookup->found = true;
        return false;
    }
    return true;
}

static bool dndolphins_consume_loose_ammunition(
    PocketD20App* app, uint8_t weapon_index, const char* ammunition_group) {
    if(!app || !ammunition_group || !ammunition_group[0]) return false;
    DndDolphinsAmmunitionLookup lookup = {
        .ammunition_group = ammunition_group,
        .logical_index = 0U,
        .found = false,
    };
    if(!dnd_storage_visit_items(
           app->storage,
           app->profiles.active_profile,
           dndolphins_ammunition_stack_visitor,
           &lookup,
           NULL) ||
       !lookup.found)
        return false;
    if(!dndolphins_item_cache_ensure(app, lookup.logical_index)) return false;
    PocketItem* ammunition = dndolphins_item_cached_at(app, lookup.logical_index, NULL);
    if(!ammunition || ammunition->quantity <= 0) return false;
    --ammunition->quantity;
    if(!dndolphins_save_items_if_changed(app)) return false;
    /* Attack/result screens expect the weapon page to be resident. Restore it
       after touching a loose-ammunition stack that may live on another page. */
    return dndolphins_item_cache_ensure(app, weapon_index);
}

static void dndolphins_roll_selected_attack(PocketD20App* app) {
    uint8_t count = dndolphins_weapon_count(app);
    if(count == 0U) return;
    app->attack_item_index = dndolphins_weapon_index(app, app->selection);
    if(app->attack_item_index == 0xFFU) return;
    PocketItem* item = dndolphins_item_at(app, app->attack_item_index, NULL);
    if(!item) return;
    PocketItem weapon = *item;
    if(item->weapon_properties & PocketWeaponAmmunition) {
        if(item->ammo_max > 0 || item->ammo_current > 0) {
            if(item->ammo_current <= 0) {
                dndolphins_set_status(app, "No ammunition");
                return;
            }
            --item->ammo_current;
            if(!dndolphins_save_items_if_changed(app)) {
                dndolphins_set_status(app, "Ammo save failed");
                return;
            }
        } else if(!dndolphins_consume_loose_ammunition(
                      app,
                      app->attack_item_index,
                      dndolphins_weapon_ammunition_group(&weapon))) {
            dndolphins_set_status(app, "No ammunition");
            return;
        }
        dndolphins_save(app, false);
        item = dndolphins_item_cached_at(app, app->attack_item_index, NULL);
        if(!item) {
            dndolphins_set_status(app, "Weapon reload failed");
            return;
        }
        weapon = *item;
    }
    app->attack_roll = dndolphins_weapon_combat_roll_attack(&app->data.character, &weapon, app->roll_mode);
    app->attack_phase = 0U;
    dndolphins_enter_screen(app, PocketScreenAttackResult);
    dndolphins_start_dice_animation(app, app->attack_roll.second_die ? 2U : 1U, 20U);
}

static void dndolphins_handle_attack_list(PocketD20App* app, const InputEvent* event) {
    uint8_t count = dndolphins_weapon_count(app);
    if(dndolphins_is_move_event(event) && event->key == InputKeyUp) {
        dndolphins_menu_move(app, count, -1);
        dndolphins_prepare_combat_weapon_rows(app);
    } else if(dndolphins_is_move_event(event) && event->key == InputKeyDown) {
        dndolphins_menu_move(app, count, 1);
        dndolphins_prepare_combat_weapon_rows(app);
    }
    else if(
        dndolphins_is_move_event(event) &&
        (event->key == InputKeyLeft || event->key == InputKeyRight)) {
        int16_t mode = app->roll_mode + (event->key == InputKeyRight ? 1 : -1);
        if(mode < 0) mode = PocketRollDisadvantage;
        if(mode > PocketRollDisadvantage) mode = PocketRollNormal;
        app->roll_mode = (PocketRollMode)mode;
    } else if(event->type == InputTypeShort && event->key == InputKeyOk) {
        dndolphins_roll_selected_attack(app);
    }
}

static void dndolphins_handle_attack_result(PocketD20App* app, const InputEvent* event) {
    PocketItem* item = dndolphins_item_at(app, app->attack_item_index, NULL);
    if(!item) return;
    if(app->attack_phase == 0U) {
        if(event->type == InputTypeShort && event->key == InputKeyOk) {
            app->damage_roll =
                dndolphins_weapon_combat_roll_damage(&app->data.character, item, app->attack_roll.critical);
            app->attack_phase = 1U;
            app->damage_roll_page = 0U;
            uint8_t count = app->damage_roll.weapon_roll_count + app->damage_roll.extra_roll_count;
            if(count)
                dndolphins_start_dice_animation(
                    app,
                    count,
                    item->use_versatile && item->versatile_die >= 2U ? item->versatile_die :
                                                                       item->damage_die);
        } else if(event->type == InputTypeShort && event->key == InputKeyRight) {
            app->damage_roll = dndolphins_weapon_combat_roll_damage(&app->data.character, item, true);
            app->attack_phase = 1U;
            app->damage_roll_page = 0U;
            uint8_t count = app->damage_roll.weapon_roll_count + app->damage_roll.extra_roll_count;
            if(count)
                dndolphins_start_dice_animation(
                    app,
                    count,
                    item->use_versatile && item->versatile_die >= 2U ? item->versatile_die :
                                                                       item->damage_die);
        } else if(event->type == InputTypeShort && event->key == InputKeyUp) {
            app->attack_roll = dndolphins_weapon_combat_roll_attack(&app->data.character, item, app->roll_mode);
            dndolphins_start_dice_animation(app, app->attack_roll.second_die ? 2U : 1U, 20U);
        }
    } else {
        uint8_t roll_count =
            app->damage_roll.weapon_roll_count + app->damage_roll.extra_roll_count;
        uint8_t page_count = roll_count > 1U ? (roll_count + 15U) / 16U : 1U;
        if(event->type == InputTypeShort && event->key == InputKeyUp && page_count > 1U) {
            if(app->damage_roll_page == 0U)
                app->damage_roll_page = page_count - 1U;
            else
                --app->damage_roll_page;
        } else if(event->type == InputTypeShort && event->key == InputKeyDown && page_count > 1U) {
            app->damage_roll_page = (app->damage_roll_page + 1U) % page_count;
        } else if(event->type == InputTypeShort && event->key == InputKeyOk) {
            app->damage_roll =
                dndolphins_weapon_combat_roll_damage(&app->data.character, item, app->damage_roll.critical);
            app->damage_roll_page = 0U;
            roll_count = app->damage_roll.weapon_roll_count + app->damage_roll.extra_roll_count;
            if(roll_count)
                dndolphins_start_dice_animation(
                    app,
                    roll_count,
                    item->use_versatile && item->versatile_die >= 2U ? item->versatile_die :
                                                                       item->damage_die);
        }
    }
}
static bool dndolphins_input_callback(InputEvent* event, void* context) {
    PocketD20App* app = context;
    /* Text/number modules can be sizable. Once their callback has returned to the
     * main view, reclaim them before processing the next user action. */
    if(!app->input_module_active) {
        dndolphins_release_text_input(app);
        dndolphins_release_number_input(app);
    }
    if(event->type == InputTypeShort || event->type == InputTypeLong ||
       event->type == InputTypeRepeat) {
        if(dndolphins_status_is_one_shot_success(app)) dndolphins_clear_status(app);
        dndolphins_clear_action_ack(app);
    }
    if(event->type == InputTypeLong && event->key == InputKeyBack) {
        dndolphins_handle_long_back(app);
        dndolphins_refresh(app);
        return true;
    }
    if(app->dice_animating) {
        if(event->type == InputTypeShort && event->key == InputKeyBack) {
            app->dice_animating = 0U;
            app->marquee_elapsed_ms = 0U;
        }
        dndolphins_refresh(app);
        return true;
    }
    if(event->type == InputTypeShort && event->key == InputKeyBack) {
        dndolphins_handle_back(app);
        dndolphins_refresh(app);
        return true;
    }

    switch(app->screen) {
    case PocketScreenHome:
        dndolphins_handle_home(app, event);
        break;
    case PocketScreenProfiles:
        dndolphins_handle_profiles(app, event);
        break;
    case PocketScreenProfileActions:
        dndolphins_handle_profile_actions(app, event);
        break;
    case PocketScreenCharacter:
        dndolphins_handle_character(app, event);
        break;
    case PocketScreenVitals:
        dndolphins_handle_vitals(app, event);
        break;
    case PocketScreenAbilities:
        dndolphins_handle_abilities(app, event);
        break;
    case PocketScreenSkills:
        dndolphins_handle_skills(app, event);
        break;
    case PocketScreenGrantReview:
        dndolphins_handle_grant_review(app, event);
        break;
    case PocketScreenGrantEdit:
        dndolphins_handle_grant_edit(app, event);
        break;
    case PocketScreenLevelReview:
        dndolphins_handle_level_review(app, event);
        break;
    case PocketScreenLevelChoice:
        dndolphins_handle_level_choice(app, event);
        break;
    case PocketScreenAsiAbility:
        dndolphins_handle_asi_ability(app, event);
        break;
    case PocketScreenMagic:
        dndolphins_handle_magic(app, event);
        break;
    case PocketScreenRecordList:
        dndolphins_handle_record_list(app, event);
        break;
    case PocketScreenRecordDetail:
        dndolphins_handle_record_detail(app, event);
        break;
    case PocketScreenCatalog:
        dndolphins_handle_catalog(app, event);
        break;
    case PocketScreenCombat:
        dndolphins_handle_combat(app, event);
        break;
    case PocketScreenSpellAttacks:
        dndolphins_handle_spell_attacks(app, event);
        break;
    case PocketScreenRituals:
        dndolphins_handle_rituals(app, event);
        break;
    case PocketScreenSpellCast:
        dndolphins_handle_spell_cast(app, event);
        break;
    case PocketScreenSpellResult:
        dndolphins_handle_spell_result(app, event);
        break;
    case PocketScreenAttackTemplates:
        dndolphins_handle_attack_templates(app, event);
        break;
    case PocketScreenAttackTemplateEdit:
        dndolphins_handle_attack_template_edit(app, event);
        break;
    case PocketScreenDice:
        dndolphins_handle_dice(app, event);
        break;
    case PocketScreenDiceResult:
        dndolphins_handle_dice_result(app, event);
        break;
    case PocketScreenAttackList:
        dndolphins_handle_attack_list(app, event);
        break;
    case PocketScreenAttackResult:
        dndolphins_handle_attack_result(app, event);
        break;
    default:
        break;
    }
    dndolphins_refresh(app);
    return true;
}

static bool dndolphins_navigation_callback(void* context) {
    PocketD20App* app = context;
    app->input_module_active = 0U;
    app->number_context = PocketNumberNone;
    app->edit_target = PocketEditNone;
    view_dispatcher_switch_to_view(app->dispatcher, PocketViewMain);
    dndolphins_refresh(app);
    return true;
}





static bool dndolphins_reserve_core_ui(PocketD20App* app) {
    if(!app) return false;
    app->dispatcher = view_dispatcher_alloc();
    if(!app->dispatcher) return false;
    view_dispatcher_set_event_callback_context(app->dispatcher, app);
    view_dispatcher_set_navigation_event_callback(app->dispatcher, dndolphins_navigation_callback);
    view_dispatcher_set_custom_event_callback(app->dispatcher, dndolphins_custom_event_callback);
    view_dispatcher_set_tick_event_callback(
        app->dispatcher, dndolphins_tick_event_callback, POCKET_D20_UI_TICK_MS);

    app->autosave_timer = furi_timer_alloc(dndolphins_autosave_timer_callback, FuriTimerTypeOnce, app);
    if(!app->autosave_timer) return false;

    app->main_view = view_alloc();
    if(!app->main_view) return false;
    view_allocate_model(app->main_view, ViewModelTypeLockFree, sizeof(PocketD20App*));
    PocketD20App** model = view_get_model(app->main_view);
    if(!model) return false;
    *model = app;
    view_commit_model(app->main_view, false);
    view_set_context(app->main_view, app);
    view_set_draw_callback(app->main_view, dndolphins_draw_callback);
    view_set_input_callback(app->main_view, dndolphins_input_callback);
    return true;
}

static PocketD20App* dndolphins_app_alloc(void) {
    PocketD20App* app = malloc(sizeof(PocketD20App));
    if(!app) {
        FURI_LOG_E(
            TAG, "Unable to allocate %u-byte app state", (unsigned int)sizeof(PocketD20App));
        return NULL;
    }
    memset(app, 0, sizeof(*app));

    app->gui = furi_record_open(RECORD_GUI);
    if(!app->gui) goto fail;
    app->storage = furi_record_open(RECORD_STORAGE);
    if(!app->storage) goto fail;
    /* Reserve core GUI blocks and the autosave timer while the heap is still clean.
       Profile scans and recovery can otherwise fragment the heap before these larger
       allocations are requested on a cold launch. */
    if(!dndolphins_reserve_core_ui(app)) goto fail;
    /* Relocate only legacy character ch*.txt files. Files are moved unchanged;
       the tolerant field-name loader interprets whatever recognized data exists.
       A failed relocation is treated conservatively as existing user data so a
       fresh New Hero cannot be created over a migration problem. */
    bool legacy_move_ok = dnd_storage_move_legacy_profiles(app->storage);
    bool profiles_loaded = dnd_storage_profiles_load(app->storage, &app->profiles);
    /* Only create a fresh character after a successful profile-directory scan proves
       there is no existing primary character file. A failed scan is treated
       conservatively as existing user data. Shadow files are never read or scanned. */
    bool character_file_available =
        app->profiles.character_file_seen || !app->profiles.scan_succeeded || !legacy_move_ok;
    bool recovered_backup = false;
    bool loaded = false;
    bool recovered_next_profile = false;
    uint32_t first_profile = app->profiles.active_profile;
    uint32_t candidate = first_profile;

    /* A stale or unreadable active profile must never leave default New Hero data
       eligible for autosave. Try the selected ID first, then advance through real
       character files by ID and wrap once. */
    uint16_t attempts = app->profiles.count ? app->profiles.count : 1U;
    for(uint16_t attempt = 0U; attempt < attempts; ++attempt) {
        bool candidate_recovered = false;
        if(dnd_storage_load_profile(
               app->storage, candidate, &app->data, &candidate_recovered)) {
            loaded = true;
            recovered_backup = candidate_recovered;
            app->profiles.active_profile = candidate;
            recovered_next_profile = candidate != first_profile;
            break;
        }
        PocketProfileEntry next;
        if(!app->profiles.count ||
           !dnd_storage_profiles_next_after(app->storage, candidate, &next) ||
           next.id == candidate || next.id == first_profile)
            break;
        candidate = next.id;
    }

    app->active_profile_loaded = loaded ? 1U : 0U;
    bool character_ready = loaded;
    bool metadata_saved = true;
    if(loaded && recovered_backup)
        character_ready = dnd_storage_restore_backup(
            app->storage, app->profiles.active_profile, &app->data);

    /* New Hero is created only when storage positively contains no primary
       character data. Shadow files never participate in this decision. */
    if(!loaded && !character_file_available) {
        dnd_data_clear(&app->data);
        dnd_data_set_defaults(&app->data);
        app->profiles.active_profile = 0U;
        app->active_profile_loaded = 1U;
        character_ready = dnd_storage_save_profile(
            app->storage, app->profiles.active_profile, &app->data);
        dnd_data_clear_spells(&app->data.character);
        dnd_data_clear_items(&app->data.character);
        dnd_data_reserve_features_exact(&app->data.character, 0U);
        app->data.character.feature_count = 0U;
        dndolphins_release_pending_grants(app);
        app->spellbook_loaded = 0U;
        app->items_loaded = 0U;
        app->features_loaded = 0U;
        if(!character_ready) {
            dnd_storage_delete_profile(app->storage, app->profiles.active_profile);
            app->active_profile_loaded = 0U;
        }
    }
    bool spell_slots_initialized = false;
    if(app->active_profile_loaded) {
        bool spellcasting_repaired = false;
        for(uint8_t i = 0U; i < app->data.character.class_count; ++i)
            if(dndolphins_spells_refresh_class_spellcasting(&app->data.character.classes[i]))
                spellcasting_repaired = true;
        spell_slots_initialized = dndolphins_spells_initialize_spell_slots_if_unset(&app->data.character);
        if(spellcasting_repaired && !spell_slots_initialized)
            dndolphins_spells_recalculate_multiclass_slots(&app->data.character);
        if(spellcasting_repaired || spell_slots_initialized)
            character_ready = character_ready && dnd_storage_save_profile_updated(
                                                   app->storage,
                                                   app->profiles.active_profile,
                                                   &app->data);
    }
    if(app->active_profile_loaded) {
        bool active_included = dndolphins_profile_include_active(app);
        metadata_saved = active_included && dnd_storage_profiles_save(app->storage, &app->profiles);
    }
    app->saved_fingerprint = dndolphins_data_fingerprint(&app->data);
    if(!character_ready || !metadata_saved) {
        app->storage_read_only = 1U;
        app->storage_unsaved = app->active_profile_loaded ? 1U : 0U;
    }

    app->screen = PocketScreenHome;
    app->roll_mode = PocketRollNormal;
    app->dice_count = 1U;
    app->dice_sides = 20U;
    if(!legacy_move_ok && !loaded)
        dndolphins_set_status(app, "Legacy characters preserved");
    else if(!loaded && character_file_available)
        dndolphins_set_status(app, "Profile preserved - load failed");
    else if(!character_ready || !metadata_saved)
        dndolphins_set_status(app, "UNSAVED - retry SD");
    else if(recovered_backup)
        dndolphins_set_status(app, "Backup recovered");
    else if(recovered_next_profile)
        dndolphins_set_status(app, "Active character recovered");
    else if(spell_slots_initialized)
        dndolphins_set_status(app, "Spell slots allocated");
    else if(loaded)
        dndolphins_clear_status(app);
    else if(profiles_loaded)
        dndolphins_set_status(app, "Fresh character");
    else
        dndolphins_set_status(app, "New character");

    app->input_events = furi_record_open(RECORD_INPUT_EVENTS);
    if(!app->input_events) goto fail;
    app->input_subscription =
        furi_pubsub_subscribe(app->input_events, dndolphins_input_events_callback, app);
    if(!app->input_subscription) goto fail;

    view_dispatcher_add_view(app->dispatcher, PocketViewMain, app->main_view);
    view_dispatcher_attach_to_gui(app->dispatcher, app->gui, ViewDispatcherTypeFullscreen);
    return app;

fail:
    dndolphins_quiesce_async(app);
    if(app->text_input) text_input_free(app->text_input);
    if(app->number_input) number_input_free(app->number_input);
    if(app->input_events) furi_record_close(RECORD_INPUT_EVENTS);
    if(app->autosave_timer) furi_timer_free(app->autosave_timer);
    if(app->main_view) view_free(app->main_view);
    if(app->dispatcher) view_dispatcher_free(app->dispatcher);
    dndolphins_catalog_release(app);
    free(app->combat_spell_indices);
    free(app->combat_weapon_indices);
    dnd_storage_profiles_free(&app->profiles);
    dnd_data_clear(&app->data);
    if(app->storage) furi_record_close(RECORD_STORAGE);
    if(app->gui) furi_record_close(RECORD_GUI);
    free(app);
    return NULL;
}

static void dndolphins_app_free(PocketD20App* app) {
    furi_assert(app);

    /* Quiesce asynchronous callbacks before any UI or app state is released. */
    dndolphins_quiesce_async(app);

    dndolphins_flush_save(app, false);
    dndolphins_catalog_release(app);

    if(app->dispatcher && app->number_input)
        view_dispatcher_remove_view(app->dispatcher, PocketViewNumberInput);
    if(app->dispatcher && app->text_input)
        view_dispatcher_remove_view(app->dispatcher, PocketViewTextInput);
    if(app->dispatcher && app->main_view)
        view_dispatcher_remove_view(app->dispatcher, PocketViewMain);
    if(app->text_input) text_input_free(app->text_input);
    if(app->number_input) number_input_free(app->number_input);
    if(app->main_view) view_free(app->main_view);

    if(app->autosave_timer) furi_timer_free(app->autosave_timer);
    if(app->input_events) furi_record_close(RECORD_INPUT_EVENTS);
    if(app->dispatcher) view_dispatcher_free(app->dispatcher);
    free(app->combat_spell_indices);
    free(app->combat_weapon_indices);
    dnd_storage_profiles_free(&app->profiles);
    dnd_data_clear(&app->data);
    if(app->storage) furi_record_close(RECORD_STORAGE);
    if(app->gui) furi_record_close(RECORD_GUI);
    free(app);
}

int32_t dndolphins_app(void* context) {
    PocketD20App* app = dndolphins_app_alloc();
    if(!app) return -1;
    dndolphins_apply_return_focus(app, (const char*)context);
    view_dispatcher_switch_to_view(app->dispatcher, PocketViewMain);
    view_dispatcher_run(app->dispatcher);

    PocketPendingLaunch pending_launch = app->pending_launch;
    dndolphins_app_free(app);

    if(pending_launch != PocketPendingLaunchNone) {
        const char* launch_path = pending_launch == PocketPendingLaunchJournal ?
                                      DNDJOURNAL_FAP_PATH :
                                  pending_launch == PocketPendingLaunchAdventure ?
                                      DNDADVENTURE_FAP_PATH :
                                  pending_launch == PocketPendingLaunchInitiative ?
                                      DNDINITIATIVE_FAP_PATH :
                                  pending_launch == PocketPendingLaunchInventory ?
                                      DNDINVENTORY_FAP_PATH :
                                  pending_launch == PocketPendingLaunchSpellbook ?
                                      DNDSPELLBOOK_FAP_PATH :
                                      DNDBESTIARY_FAP_PATH;
        if(!dnd_handoff_launch(launch_path, NULL)) return -1;
    }
    return 0;
}
