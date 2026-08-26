#include "pocket_d20.h"
#include "pocket_d20_campaigns.h"
#include "pocket_d20_rules.h"
#include "pocket_d20_storage.h"

#include <furi.h>
#include <gui/gui.h>
#include <gui/modules/number_input.h>
#include <gui/modules/text_input.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <input/input.h>
#include <loader/loader.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TAG "PocketD20"
#define POCKET_D20_MAX_GENERIC_ROLLS 20U
#define POCKET_D20_DICE_ANIMATION_FRAMES 8U
#define POCKET_D20_DICE_ANIMATION_EVENT 0xD120U
#define POCKET_D20_LONG_BACK_EVENT 0xD121U
#define POCKET_D20_MAX_CATALOG_ENTRIES 50U
#define POCKET_D20_SPELL_PAGE_ENTRIES 10U
#define POCKET_D20_MARQUEE_MS 350U

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
    PocketScreenBuilder,
    PocketScreenGrantReview,
    PocketScreenGrantEdit,
    PocketScreenCatalogDiagnostics,
    PocketScreenMagic,
    PocketScreenSpellFilters,
    PocketScreenCurrency,
    PocketScreenResources,
    PocketScreenRecordList,
    PocketScreenRecordDetail,
    PocketScreenCatalog,
    PocketScreenCombat,
    PocketScreenCombatSheet,
    PocketScreenAttackTemplates,
    PocketScreenAttackTemplateEdit,
    PocketScreenDice,
    PocketScreenDiceResult,
    PocketScreenAttackList,
    PocketScreenAttackResult,
    PocketScreenInitiativeMenu,
    PocketScreenInitiativeSetup,
    PocketScreenInitiativeCombat,
    PocketScreenInitiativeEdit,
    PocketScreenCampaigns,
    PocketScreenCampaignDiagnostics,
    PocketScreenAdventure,
    PocketScreenMonsters,
    PocketScreenMonsterList,
    PocketScreenMonsterDetail,
    PocketScreenEncounter,
    PocketScreenMonsterDiagnostics,
    PocketScreenMonsterEdit,
    PocketScreenStressTest,
    PocketScreenAbout,
} PocketScreen;

typedef enum {
    PocketListClasses,
    PocketListSpells,
    PocketListFeatures,
    PocketListItems,
    PocketListLanguages,
    PocketListJournal,
    PocketListParty,
} PocketListKind;

typedef enum {
    PocketCatalogClasses,
    PocketCatalogSubclasses,
    PocketCatalogSpecies,
    PocketCatalogBackgrounds,
    PocketCatalogAlignments,
    PocketCatalogSpells,
    PocketCatalogFeats,
    PocketCatalogItems,
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

#define POCKET_ADVENTURE_MAX_CHOICES 4U

typedef struct {
    char label[POCKET_D20_NAME_LEN];
    int8_t skill;
    uint8_t dc;
    char success_scene[POCKET_D20_SHORT_LEN];
    char failure_scene[POCKET_D20_SHORT_LEN];
    char reward_item[POCKET_D20_NAME_LEN];
    char milestone[POCKET_D20_NAME_LEN];
    uint8_t quest_flag;
    uint8_t achievement;
} PocketAdventureChoice;

typedef struct {
    char id[POCKET_D20_SHORT_LEN];
    char title[POCKET_D20_NAME_LEN];
    char body[POCKET_D20_DETAIL_LEN];
    char sprite[POCKET_D20_SHORT_LEN];
    uint8_t choice_count;
    PocketAdventureChoice choices[POCKET_ADVENTURE_MAX_CHOICES];
} PocketAdventureScene;

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
    PocketEditSpellName,
    PocketEditSpellDetail,
    PocketEditSpellStableId,
    PocketEditSpellSource,
    PocketEditSpellSchool,
    PocketEditSpellGrantName,
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
    PocketEditItemName,
    PocketEditItemDetail,
    PocketEditItemAmmoGroup,
    PocketEditLanguageName,
    PocketEditJournalTitle,
    PocketEditJournalBody,
    PocketEditPartyName,
    PocketEditTemporaryInitiativeName,
    PocketEditInitiativeConditions,
    PocketEditMonsterSearch,
    PocketEditMonsterName,
    PocketEditMonsterType,
    PocketEditMonsterSize,
    PocketEditMonsterSpeed,
    PocketEditMonsterSkills,
    PocketEditMonsterDefenses,
    PocketEditMonsterSenses,
    PocketEditMonsterLanguages,
    PocketEditMonsterTraits,
    PocketEditMonsterActions,
    PocketEditMonsterExtra,
} PocketEditTarget;

typedef enum {
    PocketNumberNone,
    PocketNumberCurrency,
    PocketNumberCharacter,
    PocketNumberVitals,
    PocketNumberAbility,
    PocketNumberSkill,
    PocketNumberMagic,
    PocketNumberRecord,
    PocketNumberDice,
    PocketNumberCombat,
    PocketNumberInitiative,
} PocketNumberContext;

typedef struct {
    Gui* gui;
    Storage* storage;
    ViewDispatcher* dispatcher;
    View* main_view;
    TextInput* text_input;
    NumberInput* number_input;
    FuriTimer* dice_timer;

    PocketSaveData data;
    PocketProfileState profiles;
    uint32_t saved_fingerprint;
    PocketScreen screen;
    PocketScreen return_screen;
    PocketScreen record_list_return_screen;
    PocketListKind list_kind;
    uint16_t selection;
    uint16_t scroll;
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
    char(*catalog_entries)[POCKET_D20_NAME_LEN];
    uint8_t* catalog_levels;
    uint16_t* catalog_class_masks;
    uint8_t* catalog_has_metadata;
    uint8_t* catalog_item_categories;
    uint8_t* catalog_item_magic;
    uint8_t* catalog_schools;
    uint8_t* catalog_sources;
    uint8_t* catalog_ritual;
    uint8_t catalog_show_all;
    uint8_t catalog_has_more;
    int8_t spell_filter_level;
    uint8_t spell_filter_class;
    uint8_t spell_filter_ritual;
    uint8_t spell_filter_school;
    uint8_t spell_filter_source;
    uint8_t spell_filter_prepared;
    uint16_t diagnostics_records;
    uint16_t diagnostics_invalid;
    uint16_t diagnostics_duplicates;
    uint16_t diagnostics_catalogs;
    uint8_t edit_slot_max;
    uint8_t edit_modifier_mode;
    uint8_t arcane_recovery_active;
    uint8_t arcane_recovery_budget;
    uint8_t arcane_recovery_spent;
    uint8_t arcane_recovery_restored[6];
    uint8_t hit_die_class_index;
    uint32_t profile_action_id;
    PocketAdventureScene* adventure_scene;
    uint16_t campaign_count;
    PocketCampaignSummary campaign_active;
    uint8_t campaign_active_valid;
    PocketCampaignDiagnostics campaign_diagnostics;
    int16_t adventure_last_total;
    uint8_t adventure_last_natural;
    uint8_t storage_read_only;
    uint8_t storage_unsaved;
    uint16_t storage_failure_count;
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

    uint8_t attack_item_index;
    uint8_t attack_phase;
    PocketAttackRoll attack_roll;
    PocketDamageRoll damage_roll;

    char status[32];
} PocketD20App;

static void pocket_text_done(void* context);
static void pocket_roll_generic(PocketD20App* app);
static void pocket_handle_long_back(PocketD20App* app);
static void pocket_adventure_release(PocketD20App* app);
static bool pocket_adventure_load(PocketD20App* app);
static void pocket_release_text_input(PocketD20App* app);
static void pocket_release_number_input(PocketD20App* app);
static void pocket_history_push(
    PocketD20App* app,
    uint8_t kind,
    uint8_t target,
    int16_t before,
    int16_t after);

static uint8_t pocket_marquee_offset = 0U;

static const char* const pocket_home_items[] = {
    "Characters", "Character", "Vitals", "Abilities & Saves", "Skills",
    "Magic & Spells", "Features & Perks", "Inventory", "Currency",
    "Inventory Resources", "Journal", "Adventure", "Combat", "Initiative",
    "Dice Roller", "Retry Save / Status", "Open Dolphin Bestiary",
};

static const char* const pocket_profile_actions[] = {
    "Switch / Open", "Rename Active", "Duplicate", "Export", "Import First Export",
    "Archive", "Delete", "Verify Save", "Restore Backup",
};

static const uint8_t pocket_die_choices[] = {4U, 6U, 8U, 10U, 12U, 20U, 100U};
static const uint8_t pocket_damage_die_choices[] = {4U, 6U, 8U, 10U, 12U};
static const char* const pocket_roll_mode_names[] = {
    "Normal", "Advantage", "Disadvantage", "Guidance"};
static const char* const pocket_attack_ability_names[] = {"Auto", "Strength", "Dexterity", "Best"};
static const char* const pocket_recharge_names[] = {
    "Manual", "Turn", "Encounter", "Dawn", "Short/Long", "Long"};
static const char* const pocket_attack_template_type_names[] = {
    "Unarmed", "Spell Attack", "Saving Throw", "Custom"};
static const char* const pocket_size_names[] = {"Tiny", "Small", "Medium", "Large"};
static const char* const pocket_spellcasting_mode_names[] = {
    "None", "Full", "Half", "Third", "Pact", "Spell Points", "Custom"};
static const char* const pocket_resource_formula_names[] = {"Manual", "PB", "Ability"};
#if 0 /* v2.6: monster UI moved to the separate Dolphin Bestiary FAP. */
static const char* const pocket_monster_type_names[] = {
    "Any", "Aberration", "Beast", "Celestial", "Construct", "Dragon", "Elemental",
    "Fey", "Fiend", "Giant", "Humanoid", "Monstrosity", "Ooze", "Plant", "Undead"};
static const char* const pocket_monster_environment_names[] = {
    "Any", "Aquatic", "Dungeon", "Planar", "Urban", "Wilderness"};
static const char* const pocket_monster_source_names[] = {
    "Any", "Open Reference", "D&Dolphins", "Custom"};
static const char* const pocket_monster_role_names[] = {
    "Any", "Leader", "Controller", "Skirmisher", "Artillery", "Brute", "Minion"};
static const char* const pocket_encounter_template_names[] = {"Balanced", "Horde", "Elite"};
#endif
static const char* const pocket_spell_school_names[] = {
    "Any", "Abjuration", "Conjuration", "Divination", "Enchantment",
    "Evocation", "Illusion", "Necromancy", "Transmutation"};

/* Group the standard skills by governing ability without changing their save indexes. */
static const uint8_t pocket_skill_display_order[POCKET_D20_SKILL_COUNT] = {
    3U,                    /* STR: Athletics */
    0U, 15U, 16U,         /* DEX: Acrobatics, Sleight of Hand, Stealth */
    2U, 5U, 8U, 10U, 14U, /* INT: Arcana, History, Investigation, Nature, Religion */
    1U, 6U, 9U, 11U, 17U, /* WIS: Animal Handling, Insight, Medicine, Perception, Survival */
    4U, 7U, 12U, 13U,     /* CHA: Deception, Intimidation, Performance, Persuasion */
};

static const char* const pocket_catalog_classes[] = {
    "Artificer", "Barbarian", "Bard", "Cleric", "Druid", "Fighter", "Monk",
    "Paladin", "Ranger", "Rogue", "Sorcerer", "Warlock", "Wizard"};

static const PocketBuiltinSubclass pocket_catalog_subclasses[] = {
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

static const char* const pocket_catalog_backgrounds[] = {
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

static const char* const pocket_catalog_species[] = {
    "Aasimar",
    "Black Dragonborn", "Blue Dragonborn", "Brass Dragonborn", "Bronze Dragonborn",
    "Copper Dragonborn", "Gold Dragonborn", "Green Dragonborn", "Red Dragonborn",
    "Silver Dragonborn", "White Dragonborn",
    "Dwarf", "Drow Elf", "High Elf", "Wood Elf", "Forest Gnome", "Rock Gnome",
    "Cloud Giant Goliath", "Fire Giant Goliath", "Frost Giant Goliath",
    "Hill Giant Goliath", "Stone Giant Goliath", "Storm Giant Goliath",
    "Halfling", "Human", "Orc", "Abyssal Tiefling", "Chthonic Tiefling",
    "Infernal Tiefling"};

static const char* const pocket_catalog_alignments[] = {
    "Lawful Good", "Neutral Good", "Chaotic Good",
    "Lawful Neutral", "True Neutral", "Chaotic Neutral",
    "Lawful Evil", "Neutral Evil", "Chaotic Evil"};

static const char* const pocket_catalog_feats[] = {
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

static const PocketBuiltinSpell pocket_catalog_spells[] = {
    {"Acid Splash", 0U, PocketClassMaskSorcerer | PocketClassMaskWizard},
    {"Aid", 2U, PocketClassMaskCleric | PocketClassMaskPaladin | PocketClassMaskRanger},
    {"Alarm", 1U, PocketClassMaskRanger | PocketClassMaskWizard},
    {"Animal Friendship", 1U, PocketClassMaskBard | PocketClassMaskDruid | PocketClassMaskRanger},
    {"Aura of Life", 4U, PocketClassMaskCleric | PocketClassMaskPaladin},
    {"Bless", 1U, PocketClassMaskCleric | PocketClassMaskPaladin},
    {"Burning Hands", 1U, PocketClassMaskSorcerer | PocketClassMaskWizard},
    {"Charm Monster", 4U, PocketClassMaskBard | PocketClassMaskDruid | PocketClassMaskSorcerer | PocketClassMaskWarlock | PocketClassMaskWizard},
    {"Charm Person", 1U, PocketClassMaskBard | PocketClassMaskDruid | PocketClassMaskSorcerer | PocketClassMaskWarlock | PocketClassMaskWizard},
    {"Chromatic Orb", 1U, PocketClassMaskSorcerer | PocketClassMaskWizard},
    {"Cure Wounds", 1U, PocketClassMaskBard | PocketClassMaskCleric | PocketClassMaskDruid | PocketClassMaskPaladin | PocketClassMaskRanger},
    {"Detect Magic", 1U, PocketClassMaskBard | PocketClassMaskCleric | PocketClassMaskDruid | PocketClassMaskPaladin | PocketClassMaskRanger | PocketClassMaskSorcerer | PocketClassMaskWizard},
    {"Dispel Magic", 3U, PocketClassMaskBard | PocketClassMaskCleric | PocketClassMaskDruid | PocketClassMaskPaladin | PocketClassMaskSorcerer | PocketClassMaskWarlock | PocketClassMaskWizard},
    {"Dissonant Whispers", 1U, PocketClassMaskBard},
    {"Divine Smite", 1U, PocketClassMaskPaladin},
    {"Dragon's Breath", 2U, PocketClassMaskSorcerer | PocketClassMaskWizard},
    {"Elementalism", 0U, PocketClassMaskDruid | PocketClassMaskSorcerer | PocketClassMaskWizard},
    {"Ensnaring Strike", 1U, PocketClassMaskRanger},
    {"Fire Bolt", 0U, PocketClassMaskSorcerer | PocketClassMaskWizard},
    {"Fireball", 3U, PocketClassMaskSorcerer | PocketClassMaskWizard},
    {"Guidance", 0U, PocketClassMaskCleric | PocketClassMaskDruid},
    {"Healing Word", 1U, PocketClassMaskBard | PocketClassMaskCleric | PocketClassMaskDruid},
    {"Hex", 1U, PocketClassMaskWarlock},
    {"Hold Person", 2U, PocketClassMaskBard | PocketClassMaskCleric | PocketClassMaskDruid | PocketClassMaskSorcerer | PocketClassMaskWarlock | PocketClassMaskWizard},
    {"Ice Knife", 1U, PocketClassMaskDruid | PocketClassMaskSorcerer | PocketClassMaskWizard},
    {"Identify", 1U, PocketClassMaskBard | PocketClassMaskWizard},
    {"Invisibility", 2U, PocketClassMaskBard | PocketClassMaskSorcerer | PocketClassMaskWarlock | PocketClassMaskWizard},
    {"Light", 0U, PocketClassMaskBard | PocketClassMaskCleric | PocketClassMaskSorcerer | PocketClassMaskWizard},
    {"Mage Armor", 1U, PocketClassMaskSorcerer | PocketClassMaskWizard},
    {"Mage Hand", 0U, PocketClassMaskBard | PocketClassMaskSorcerer | PocketClassMaskWarlock | PocketClassMaskWizard},
    {"Magic Missile", 1U, PocketClassMaskSorcerer | PocketClassMaskWizard},
    {"Mind Spike", 2U, PocketClassMaskSorcerer | PocketClassMaskWarlock | PocketClassMaskWizard},
    {"Phantasmal Force", 2U, PocketClassMaskBard | PocketClassMaskSorcerer | PocketClassMaskWizard},
    {"Power Word Heal", 9U, PocketClassMaskBard | PocketClassMaskCleric},
    {"Ray of Sickness", 1U, PocketClassMaskSorcerer | PocketClassMaskWizard},
    {"Searing Smite", 1U, PocketClassMaskPaladin},
    {"Shield", 1U, PocketClassMaskSorcerer | PocketClassMaskWizard},
    {"Shocking Grasp", 0U, PocketClassMaskSorcerer | PocketClassMaskWizard},
    {"Sorcerous Burst", 0U, PocketClassMaskSorcerer},
    {"Starry Wisp", 0U, PocketClassMaskBard | PocketClassMaskDruid},
    {"Summon Dragon", 5U, PocketClassMaskDruid | PocketClassMaskSorcerer | PocketClassMaskWizard},
    {"Thaumaturgy", 0U, PocketClassMaskCleric},
    {"Thunderwave", 1U, PocketClassMaskBard | PocketClassMaskDruid | PocketClassMaskSorcerer | PocketClassMaskWizard},
    {"Tsunami", 8U, PocketClassMaskDruid},
    {"Vitriolic Sphere", 4U, PocketClassMaskSorcerer | PocketClassMaskWizard},
};

static const char* const pocket_catalog_items[] = {
    "Battleaxe", "Club", "Dagger", "Dart", "Greatclub", "Greataxe", "Greatsword",
    "Handaxe", "Javelin", "Light Crossbow", "Longbow", "Longsword", "Mace",
    "Musket", "Pistol", "Quarterstaff", "Rapier", "Shortbow", "Shortsword", "Spear",
    "Bead of Nourishment", "Cloak of Invisibility", "Elixir of Health", "Energy Bow",
    "Gloves of Thievery", "Hat of Many Spells", "Potion of Healing",
    "Potion of Invulnerability", "Potion of Longevity", "Potion of Vitality",
    "Quarterstaff of the Acrobat", "Rod of Resurrection", "Sending Stones",
    "Sentinel Shield", "Shield of the Cavalier", "Thunderous Greatclub",
};

static const char* const pocket_bundled_catalog_paths[PocketCatalogCount] = {
    APP_ASSETS_PATH("catalogs/classes.txt"),
    APP_ASSETS_PATH("catalogs/subclasses.txt"),
    APP_ASSETS_PATH("catalogs/species.txt"),
    APP_ASSETS_PATH("catalogs/backgrounds.txt"),
    APP_ASSETS_PATH("catalogs/alignments.txt"),
    APP_ASSETS_PATH("catalogs/spells.txt"),
    APP_ASSETS_PATH("catalogs/feats.txt"),
    APP_ASSETS_PATH("catalogs/items.txt"),
};

static const char* const pocket_bundled_metadata_path = APP_ASSETS_PATH("metadata/options.txt");
static const char* const pocket_bundled_catalog_abilities_path =
    APP_ASSETS_PATH("catalogs/abilities.txt");

static const char* pocket_active_metadata_path(Storage* storage) {
    UNUSED(storage);
    return pocket_bundled_metadata_path;
}

static void pocket_copy(char* destination, size_t size, const char* source) {
    if(size == 0U) return;
    strncpy(destination, source, size - 1U);
    destination[size - 1U] = '\0';
}

/*
 * Append display text without asking snprintf to prove that a persistent field
 * fits in a smaller UI row. RogueMaster treats -Wformat-truncation as an error,
 * so rows are bounded explicitly here. Detail rows are sized for a complete
 * persistent text field, and the row renderer horizontally scrolls that text.
 */
static void pocket_format_labeled_text(
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

static void pocket_catalog_release(PocketD20App* app) {
    free(app->catalog_storage);
    app->catalog_storage = NULL;
    app->catalog_entries = NULL;
    app->catalog_levels = NULL;
    app->catalog_class_masks = NULL;
    app->catalog_has_metadata = NULL;
    app->catalog_item_categories = NULL;
    app->catalog_item_magic = NULL;
    app->catalog_schools = NULL;
    app->catalog_sources = NULL;
    app->catalog_ritual = NULL;
    app->catalog_count = 0U;
    app->catalog_capacity = 0U;
}

static uint16_t pocket_catalog_page_limit(const PocketD20App* app) {
    if(app->catalog_page_size) return app->catalog_page_size;
    return app->catalog_kind == PocketCatalogSpells ? POCKET_D20_SPELL_PAGE_ENTRIES :
                                                      POCKET_D20_MAX_CATALOG_ENTRIES;
}

static bool pocket_catalog_ensure_capacity(PocketD20App* app, uint16_t needed) {
    if(needed <= app->catalog_capacity) return true;
    const uint16_t page_limit = pocket_catalog_page_limit(app);
    if(needed > page_limit || app->catalog_storage) return false;
    const size_t capacity = page_limit;
    const size_t bytes =
        capacity * sizeof(*app->catalog_entries) +
        capacity * sizeof(*app->catalog_levels) +
        capacity * sizeof(*app->catalog_class_masks) +
        capacity * sizeof(*app->catalog_has_metadata) +
        capacity * sizeof(*app->catalog_item_categories) +
        capacity * sizeof(*app->catalog_item_magic) +
        capacity * sizeof(*app->catalog_schools) +
        capacity * sizeof(*app->catalog_sources) +
        capacity * sizeof(*app->catalog_ritual);
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
    app->catalog_item_categories = cursor;
    cursor += capacity * sizeof(*app->catalog_item_categories);
    app->catalog_item_magic = cursor;
    cursor += capacity * sizeof(*app->catalog_item_magic);
    app->catalog_schools = cursor;
    cursor += capacity * sizeof(*app->catalog_schools);
    app->catalog_sources = cursor;
    cursor += capacity * sizeof(*app->catalog_sources);
    app->catalog_ritual = cursor;
    app->catalog_capacity = (uint16_t)capacity;
    return true;
}

static int16_t pocket_clamp_i16(int16_t value, int16_t minimum, int16_t maximum) {
    if(value < minimum) return minimum;
    if(value > maximum) return maximum;
    return value;
}

static uint8_t pocket_clamp_u8(int16_t value, uint8_t maximum) {
    if(value < 0) return 0U;
    if(value > maximum) return maximum;
    return (uint8_t)value;
}

static void pocket_clear_status(PocketD20App* app) {
    app->status[0] = '\0';
}

static void pocket_set_status(PocketD20App* app, const char* status) {
    pocket_copy(app->status, sizeof(app->status), status);
}

static void pocket_refresh(PocketD20App* app) {
    (void)view_get_model(app->main_view);
    view_commit_model(app->main_view, true);
}

static void pocket_dice_timer_callback(void* context) {
    PocketD20App* app = context;
    view_dispatcher_send_custom_event(app->dispatcher, POCKET_D20_DICE_ANIMATION_EVENT);
}

static void pocket_input_events_callback(const void* value, void* context) {
    PocketD20App* app = context;
    const InputEvent* event = value;
    if(app->input_module_active && event && event->key == InputKeyBack &&
       event->type == InputTypeLong)
        view_dispatcher_send_custom_event(app->dispatcher, POCKET_D20_LONG_BACK_EVENT);
}

static void pocket_start_dice_animation(PocketD20App* app, uint8_t count, uint8_t sides) {
    app->dice_animating = 1U;
    app->dice_anim_frame = 0U;
    app->dice_anim_count = count ? count : 1U;
    app->dice_anim_sides = sides >= 2U ? sides : 20U;
    furi_timer_stop(app->dice_timer);
    furi_timer_start(app->dice_timer, furi_ms_to_ticks(100U));
}

static bool pocket_custom_event_callback(void* context, uint32_t event) {
    PocketD20App* app = context;
    if(event == POCKET_D20_LONG_BACK_EVENT) {
        app->input_module_active = 0U;
        app->edit_target = PocketEditNone;
        app->number_context = PocketNumberNone;
        view_dispatcher_switch_to_view(app->dispatcher, PocketViewMain);
        pocket_handle_long_back(app);
        pocket_refresh(app);
        return true;
    }
    if(event != POCKET_D20_DICE_ANIMATION_EVENT) return false;
    if(app->dice_animating) {
        ++app->dice_anim_frame;
        if(app->dice_anim_frame >= POCKET_D20_DICE_ANIMATION_FRAMES) {
            app->dice_animating = 0U;
            furi_timer_start(app->dice_timer, furi_ms_to_ticks(POCKET_D20_MARQUEE_MS));
        }
        pocket_refresh(app);
    } else {
        ++pocket_marquee_offset;
        pocket_refresh(app);
    }
    return true;
}

static uint32_t pocket_data_fingerprint(const PocketSaveData* data) {
    uint32_t hash = 2166136261UL;
    const uint8_t* bytes = (const uint8_t*)data;
#define POCKET_HASH_BYTES(pointer, length)                 \
    do {                                                   \
        const uint8_t* hash_bytes = (const uint8_t*)(pointer); \
        for(size_t hash_i = 0U; hash_i < (length); ++hash_i) { \
            hash ^= hash_bytes[hash_i];                    \
            hash *= 16777619UL;                            \
        }                                                  \
    } while(false)
    POCKET_HASH_BYTES(bytes, sizeof(*data));
    const PocketCharacter* character = &data->character;
    if(character->spell_count) {
        POCKET_HASH_BYTES(
            character->spells, (size_t)character->spell_count * sizeof(PocketSpell));
        POCKET_HASH_BYTES(character->spell_known, character->spell_count);
        POCKET_HASH_BYTES(character->spell_always_prepared, character->spell_count);
        POCKET_HASH_BYTES(character->spell_free_casts_current, character->spell_count);
        POCKET_HASH_BYTES(character->spell_free_casts_max, character->spell_count);
    }
    if(character->feature_count)
        POCKET_HASH_BYTES(
            character->features, (size_t)character->feature_count * sizeof(PocketFeature));
    if(character->item_count)
        POCKET_HASH_BYTES(
            character->items, (size_t)character->item_count * sizeof(PocketItem));
    if(character->journal_count)
        POCKET_HASH_BYTES(
            character->journal, (size_t)character->journal_count * sizeof(PocketJournalEntry));
    if(character->grant_count)
        POCKET_HASH_BYTES(
            character->grants, (size_t)character->grant_count * sizeof(PocketGrant));
#undef POCKET_HASH_BYTES
    return hash;
}

static PocketProfileEntry* pocket_active_profile_entry(PocketD20App* app) {
    for(uint16_t i = 0U; i < app->profiles.count; ++i)
        if(app->profiles.entries[i].id == app->profiles.active_profile)
            return &app->profiles.entries[i];
    return NULL;
}

static bool pocket_save(PocketD20App* app, bool report) {
    if(app->storage_read_only) {
        app->storage_unsaved = 1U;
        pocket_set_status(app, "UNSAVED - retry SD");
        return false;
    }
    uint32_t fingerprint = pocket_data_fingerprint(&app->data);
    if(!app->storage_unsaved && fingerprint == app->saved_fingerprint) {
        if(report) pocket_set_status(app, "Already saved");
        return true;
    }
    PocketProfileEntry* active = pocket_active_profile_entry(app);
    bool result = active ?
                      pocket_d20_storage_save_profile_known(app->storage, active, &app->data) :
                      pocket_d20_storage_save_profile(
                          app->storage, app->profiles.active_profile, &app->data);
    if(result) {
        if(active) {
            active->level = pocket_d20_total_level(&app->data.character);
            pocket_copy(active->name, sizeof(active->name), app->data.character.name);
        }
        app->saved_fingerprint = fingerprint;
        app->storage_unsaved = 0U;
    } else {
        app->storage_read_only = 1U;
        app->storage_unsaved = 1U;
        if(app->storage_failure_count < UINT16_MAX) ++app->storage_failure_count;
    }
    if(report || !result)
        pocket_set_status(app, result ? "Saved" : "UNSAVED - SD unavailable");
    return result;
}

static uint16_t pocket_profile_count(const PocketD20App* app) {
    return app->profiles.count;
}

static uint32_t pocket_profile_id_at(const PocketD20App* app, uint16_t list_index) {
    if(list_index >= app->profiles.count) return UINT32_MAX;
    return app->profiles.entries[list_index].id;
}

static bool pocket_profile_exists(const PocketD20App* app, uint32_t profile) {
    for(uint16_t i = 0U; i < app->profiles.count; ++i)
        if(app->profiles.entries[i].id == profile) return true;
    return false;
}

static bool pocket_profile_include_active(PocketD20App* app) {
    if(pocket_profile_exists(app, app->profiles.active_profile)) return true;
    if(app->profiles.count == app->profiles.capacity) {
        uint16_t next_capacity = app->profiles.capacity ?
                                     (uint16_t)(app->profiles.capacity * 2U) : 8U;
        if(next_capacity <= app->profiles.capacity) return false;
        PocketProfileEntry* resized = realloc(
            app->profiles.entries,
            (size_t)next_capacity * sizeof(PocketProfileEntry));
        if(!resized) return false;
        app->profiles.entries = resized;
        app->profiles.capacity = next_capacity;
    }
    PocketProfileEntry* entry = &app->profiles.entries[app->profiles.count++];
    memset(entry, 0, sizeof(*entry));
    entry->id = app->profiles.active_profile;
    entry->level = pocket_d20_total_level(&app->data.character);
    pocket_copy(entry->name, sizeof(entry->name), app->data.character.name);
    return true;
}

static void pocket_enter_screen(PocketD20App* app, PocketScreen screen) {
    app->screen = screen;
    app->selection = 0U;
    app->scroll = 0U;
    app->edit_modifier_mode = 0U;
    pocket_marquee_offset = 0U;
    if(app->storage_unsaved) pocket_set_status(app, "UNSAVED - retry SD");
    else pocket_clear_status(app);
}

static void pocket_switch_profile(PocketD20App* app, uint32_t profile) {
    if(!pocket_profile_exists(app, profile)) return;
    if(profile == app->profiles.active_profile) {
        pocket_set_status(app, "Already active");
        return;
    }
    if(!pocket_save(app, false)) {
        pocket_set_status(app, "Save failed");
        return;
    }

    app->profiles.active_profile = profile;
    app->arcane_recovery_active = 0U;
    app->campaign_active_valid = 0U;
    bool recovered_backup = false;
    bool loaded = pocket_d20_storage_load_profile(
        app->storage, profile, &app->data, &recovered_backup);
    bool character_ready = loaded;
    if(!loaded || recovered_backup)
        character_ready = pocket_d20_storage_save_profile(app->storage, profile, &app->data);
    bool metadata_saved = pocket_d20_profiles_refresh(app->storage, &app->profiles);
    pocket_profile_include_active(app);
    metadata_saved = metadata_saved && pocket_d20_profiles_save(app->storage, &app->profiles);
    if(character_ready && metadata_saved)
        app->saved_fingerprint = pocket_data_fingerprint(&app->data);
    pocket_enter_screen(app, PocketScreenHome);
    if(!character_ready || !metadata_saved)
        pocket_set_status(app, "Profile save failed");
    else if(recovered_backup)
        pocket_set_status(app, "Backup recovered");
    else if(loaded)
        pocket_set_status(app, "Character switched");
    else
        pocket_set_status(app, "Fresh character");
}

static void pocket_create_profile(PocketD20App* app) {
    if(!pocket_save(app, false)) {
        pocket_set_status(app, "Save failed");
        return;
    }
    uint32_t profile = pocket_d20_profiles_next_id(&app->profiles);
    if(profile == UINT32_MAX && pocket_profile_exists(app, UINT32_MAX)) {
        pocket_set_status(app, "Profile IDs exhausted");
        return;
    }
    uint32_t previous_profile = app->profiles.active_profile;
    app->arcane_recovery_active = 0U;
    app->campaign_active_valid = 0U;
    pocket_d20_data_clear(&app->data);
    pocket_d20_data_set_defaults(&app->data);
    snprintf(
        app->data.character.name,
        sizeof(app->data.character.name),
        "New Hero %lu",
        (unsigned long)(profile + 1U));
    bool character_saved =
        pocket_d20_storage_save_profile(app->storage, profile, &app->data);
    if(!character_saved) {
        bool recovered = false;
        app->profiles.active_profile = previous_profile;
        pocket_d20_storage_load_profile(
            app->storage, previous_profile, &app->data, &recovered);
        pocket_d20_profiles_refresh(app->storage, &app->profiles);
        pocket_profile_include_active(app);
        app->saved_fingerprint = pocket_data_fingerprint(&app->data);
        app->storage_read_only = 1U;
        app->storage_unsaved = 1U;
        pocket_enter_screen(app, PocketScreenProfiles);
        pocket_set_status(app, "New character save failed");
        return;
    }
    app->profiles.active_profile = profile;
    bool metadata_saved = pocket_d20_profiles_refresh(app->storage, &app->profiles);
    pocket_profile_include_active(app);
    metadata_saved = metadata_saved && pocket_d20_profiles_save(app->storage, &app->profiles);
    if(character_saved && metadata_saved)
        app->saved_fingerprint = pocket_data_fingerprint(&app->data);
    pocket_enter_screen(app, PocketScreenCharacter);
    pocket_set_status(
        app,
        character_saved && metadata_saved ? "New character" : "Save failed");
}

static void pocket_delete_profile(PocketD20App* app, uint32_t profile) {
    if(profile == 0U) {
        pocket_set_status(app, "Main cannot delete");
        return;
    }
    if(profile == app->profiles.active_profile) {
        pocket_set_status(app, "Switch before delete");
        return;
    }
    if(!pocket_profile_exists(app, profile)) return;
    bool character_deleted = pocket_d20_storage_delete_profile(app->storage, profile);
    bool metadata_saved = pocket_d20_profiles_refresh(app->storage, &app->profiles) &&
                          pocket_d20_profiles_save(app->storage, &app->profiles);
    app->selection = 0U;
    app->scroll = 0U;
    pocket_set_status(
        app,
        metadata_saved && character_deleted ? "Character deleted" : "Delete failed");
}

static uint8_t pocket_wizard_level(const PocketCharacter* character) {
    for(uint8_t i = 0U; i < character->class_count; ++i)
        if(strcmp(character->classes[i].name, "Wizard") == 0)
            return character->classes[i].level;
    return 0U;
}

static bool pocket_begin_arcane_recovery(PocketD20App* app) {
    PocketCharacter* character = &app->data.character;
    uint8_t wizard_level = pocket_wizard_level(character);
    if(!wizard_level) {
        pocket_set_status(app, "No Wizard class found");
        return false;
    }
    if(character->arcane_recovery_used) {
        pocket_set_status(app, "Recovery already used");
        return false;
    }
    bool has_expended_slot = false;
    for(uint8_t level = 1U; level <= 5U; ++level)
        if(character->spell_slots_current[level] < character->spell_slots_max[level])
            has_expended_slot = true;
    if(!has_expended_slot) {
        pocket_set_status(app, "No eligible slots spent");
        return false;
    }
    app->arcane_recovery_active = 1U;
    app->arcane_recovery_budget = (wizard_level + 1U) / 2U;
    app->arcane_recovery_spent = 0U;
    memset(app->arcane_recovery_restored, 0, sizeof(app->arcane_recovery_restored));
    pocket_enter_screen(app, PocketScreenMagic);
    app->selection = 6U;
    app->scroll = 2U;
    pocket_set_status(app, "Choose slots, OK done");
    return true;
}

static void pocket_menu_move(PocketD20App* app, uint16_t count, int8_t delta) {
    if(count == 0U) return;
    int32_t next = (int32_t)app->selection + delta;
    if(next < 0) next = count - 1U;
    if(next >= count) next = 0;
    app->selection = (uint16_t)next;
    pocket_marquee_offset = 0U;
    if(app->selection < app->scroll) app->scroll = app->selection;
    if(app->selection >= app->scroll + 5U) app->scroll = app->selection - 4U;
}

static const char* pocket_proficiency_mark(uint8_t proficiency) {
    if(proficiency == PocketProficiencyExpertise) return "E";
    if(proficiency == PocketProficiencyProficient) return "P";
    return "-";
}

static char pocket_spell_status(const PocketCharacter* character, uint8_t index) {
    if(character->spell_always_prepared[index]) return 'A';
    if(character->spells[index].prepared) return 'P';
    if(character->spell_known[index]) return 'K';
    return '-';
}

static uint8_t pocket_cycle_die(uint8_t current, int8_t delta, bool damage_only) {
    const uint8_t* choices = damage_only ? pocket_damage_die_choices : pocket_die_choices;
    uint8_t count = damage_only ? sizeof(pocket_damage_die_choices) : sizeof(pocket_die_choices);
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

static uint16_t pocket_class_mask_from_name(const char* name) {
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

static void pocket_configure_class_defaults(PocketClassLevel* level) {
    uint16_t mask = pocket_class_mask_from_name(level->name);
    if(mask & (PocketClassMaskBarbarian)) level->hit_die = 12U;
    else if(mask & (PocketClassMaskFighter | PocketClassMaskPaladin | PocketClassMaskRanger))
        level->hit_die = 10U;
    else if(mask & (PocketClassMaskBard | PocketClassMaskCleric | PocketClassMaskDruid |
                    PocketClassMaskMonk | PocketClassMaskRogue | PocketClassMaskWarlock |
                    PocketClassMaskArtificer))
        level->hit_die = 8U;
    else
        level->hit_die = 6U;
    if(mask & (PocketClassMaskBard | PocketClassMaskCleric | PocketClassMaskDruid |
               PocketClassMaskSorcerer | PocketClassMaskWizard))
        level->spellcasting_mode = PocketSpellcastingFull;
    else if(mask & (PocketClassMaskArtificer | PocketClassMaskPaladin | PocketClassMaskRanger))
        level->spellcasting_mode = PocketSpellcastingHalf;
    else if(mask & PocketClassMaskWarlock)
        level->spellcasting_mode = PocketSpellcastingPact;
    else
        level->spellcasting_mode = PocketSpellcastingNone;
    if(mask & (PocketClassMaskBard | PocketClassMaskPaladin | PocketClassMaskSorcerer |
               PocketClassMaskWarlock))
        level->spellcasting_ability = PocketAbilityCharisma;
    else if(mask & (PocketClassMaskCleric | PocketClassMaskDruid | PocketClassMaskRanger))
        level->spellcasting_ability = PocketAbilityWisdom;
    else
        level->spellcasting_ability = PocketAbilityIntelligence;
}

static bool pocket_subclass_allowed(
    const PocketD20App* app,
    uint16_t class_mask,
    bool has_metadata) {
    if(app->catalog_show_all) return true;
    if(!has_metadata || app->record_index >= app->data.character.class_count) return false;
    uint16_t selected_class = pocket_class_mask_from_name(
        app->data.character.classes[app->record_index].name);
    return selected_class && (class_mask & selected_class);
}

static uint8_t pocket_class_max_spell_level(const PocketClassLevel* class_level) {
    uint8_t level = class_level->level;
    uint16_t mask = pocket_class_mask_from_name(class_level->name);
    if(mask & (PocketClassMaskBard | PocketClassMaskCleric | PocketClassMaskDruid |
               PocketClassMaskSorcerer | PocketClassMaskWizard)) {
        uint8_t maximum = (level + 1U) / 2U;
        return maximum > 9U ? 9U : maximum;
    }
    if(mask & (PocketClassMaskArtificer | PocketClassMaskPaladin | PocketClassMaskRanger)) {
        uint8_t maximum = (level + 3U) / 4U;
        return maximum > 5U ? 5U : maximum;
    }
    if(mask & PocketClassMaskWarlock) {
        uint8_t maximum = (level + 1U) / 2U;
        return maximum > 5U ? 5U : maximum;
    }
    return 0U;
}

static bool pocket_spell_allowed(
    const PocketD20App* app,
    uint8_t level,
    uint16_t class_mask,
    bool has_metadata) {
    if(app->catalog_show_all) return true;
    if(!has_metadata || app->record_index >= app->data.character.spell_count) return false;
    const PocketSpell* spell = &app->data.character.spells[app->record_index];
    uint8_t class_index = app->spell_filter_class < app->data.character.class_count ?
                              app->spell_filter_class : spell->class_index;
    if(class_index >= app->data.character.class_count) return false;
    const PocketClassLevel* class_level = &app->data.character.classes[class_index];
    uint16_t selected_class = pocket_class_mask_from_name(class_level->name);
    return selected_class && (class_mask & selected_class) &&
           level <= pocket_class_max_spell_level(class_level);
}

static const char* pocket_catalog_title(const PocketD20App* app) {
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
    case PocketCatalogSpells:
        return app->catalog_show_all ? "Spells: All" : "Spells: Allowed";
    case PocketCatalogFeats:
        return "Choose Feat/Perk";
    case PocketCatalogItems:
        return "Choose Item";
    default:
        return "Choose Name";
    }
}

static bool pocket_catalog_add_metadata(
    PocketD20App* app,
    const char* name,
    uint8_t level,
    uint16_t class_mask,
    bool has_metadata) {
    if(app->catalog_kind == PocketCatalogSpells) {
        if(!pocket_spell_allowed(app, level, class_mask, has_metadata)) return false;
    } else if(app->catalog_kind == PocketCatalogSubclasses &&
              !pocket_subclass_allowed(app, class_mask, has_metadata)) {
        return false;
    }
    if(!name[0]) return false;
    uint16_t page_limit = pocket_catalog_page_limit(app);
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
       !pocket_catalog_ensure_capacity(app, app->catalog_count + 1U)) {
        pocket_set_status(app, "Catalog memory full");
        return false;
    }
    pocket_copy(
        app->catalog_entries[app->catalog_count],
        sizeof(app->catalog_entries[app->catalog_count]),
        name);
    app->catalog_levels[app->catalog_count] = level;
    app->catalog_class_masks[app->catalog_count] = class_mask;
    app->catalog_has_metadata[app->catalog_count] = has_metadata ? 1U : 0U;
    app->catalog_item_categories[app->catalog_count] = 0U;
    app->catalog_item_magic[app->catalog_count] = 0U;
    app->catalog_schools[app->catalog_count] = 0U;
    app->catalog_sources[app->catalog_count] = 0U;
    app->catalog_ritual[app->catalog_count] = 0U;
    ++app->catalog_count;
    return true;
}

static bool pocket_catalog_page_complete(const PocketD20App* app) {
    return app->catalog_scan_count >
           app->catalog_page_start + pocket_catalog_page_limit(app);
}

static bool pocket_catalog_add(PocketD20App* app, const char* name) {
    return pocket_catalog_add_metadata(app, name, 0U, 0U, false);
}

static uint8_t pocket_item_category_from_name(const char* category) {
    if(strcmp(category, "Weapon") == 0) return PocketItemCategoryWeapon;
    if(strcmp(category, "Armor") == 0) return PocketItemCategoryArmor;
    if(strcmp(category, "Gear") == 0) return PocketItemCategoryGear;
    if(strcmp(category, "Tool") == 0) return PocketItemCategoryTool;
    if(strcmp(category, "Mount/Vehicle") == 0) return PocketItemCategoryMountVehicle;
    if(strcmp(category, "Potion") == 0) return PocketItemCategoryPotion;
    if(strcmp(category, "Ring") == 0) return PocketItemCategoryRing;
    if(strcmp(category, "Rod") == 0) return PocketItemCategoryRod;
    if(strcmp(category, "Scroll") == 0) return PocketItemCategoryScroll;
    if(strcmp(category, "Staff") == 0) return PocketItemCategoryStaff;
    if(strcmp(category, "Wand") == 0) return PocketItemCategoryWand;
    if(strcmp(category, "Wondrous") == 0) return PocketItemCategoryWondrous;
    return PocketItemCategoryOther;
}

static const char* pocket_item_category_mark(uint8_t category) {
    switch(category) {
    case PocketItemCategoryWeapon:
        return "W";
    case PocketItemCategoryArmor:
        return "A";
    case PocketItemCategoryGear:
        return "G";
    case PocketItemCategoryTool:
        return "T";
    case PocketItemCategoryMountVehicle:
        return "M";
    case PocketItemCategoryPotion:
        return "P";
    case PocketItemCategoryRing:
        return "R";
    case PocketItemCategoryRod:
        return "D";
    case PocketItemCategoryScroll:
        return "S";
    case PocketItemCategoryStaff:
        return "F";
    case PocketItemCategoryWand:
        return "N";
    case PocketItemCategoryWondrous:
        return "O";
    default:
        return "?";
    }
}

typedef struct {
    const char* name;
    int16_t weight_tenths;
    uint16_t properties;
    uint8_t damage_dice;
    uint8_t damage_die;
    uint8_t versatile_die;
    uint8_t damage_type;
    uint8_t armor_base;
    int8_t armor_dex_cap;
    uint8_t shield_bonus;
    const char* ammunition_group;
} PocketEquipmentPreset;

#define WEAPON(name, weight, dice, die, versatile, type, properties, ammo) \
    {name, weight, properties, dice, die, versatile, type, 0U, -1, 0U, ammo}
#define ARMOR(name, weight, base, dex_cap, shield) \
    {name, weight, 0U, 0U, 0U, 0U, PocketDamageBludgeoning, base, dex_cap, shield, ""}

static const PocketEquipmentPreset pocket_equipment_presets[] = {
    WEAPON("Club", 20, 1, 4, 0, PocketDamageBludgeoning, PocketWeaponLight, ""),
    WEAPON("Dagger", 10, 1, 4, 0, PocketDamagePiercing,
        PocketWeaponFinesse | PocketWeaponLight | PocketWeaponThrown, ""),
    WEAPON("Greatclub", 100, 1, 8, 0, PocketDamageBludgeoning, 0U, ""),
    WEAPON("Handaxe", 20, 1, 6, 0, PocketDamageSlashing,
        PocketWeaponLight | PocketWeaponThrown, ""),
    WEAPON("Javelin", 20, 1, 6, 0, PocketDamagePiercing, PocketWeaponThrown, ""),
    WEAPON("Light Hammer", 20, 1, 4, 0, PocketDamageBludgeoning,
        PocketWeaponLight | PocketWeaponThrown, ""),
    WEAPON("Mace", 40, 1, 6, 0, PocketDamageBludgeoning, 0U, ""),
    WEAPON("Quarterstaff", 40, 1, 6, 8, PocketDamageBludgeoning, 0U, ""),
    WEAPON("Sickle", 20, 1, 4, 0, PocketDamageSlashing, PocketWeaponLight, ""),
    WEAPON("Spear", 30, 1, 6, 8, PocketDamagePiercing, PocketWeaponThrown, ""),
    WEAPON("Dart", 3, 1, 4, 0, PocketDamagePiercing,
        PocketWeaponFinesse | PocketWeaponRanged | PocketWeaponThrown, ""),
    WEAPON("Light Crossbow", 50, 1, 8, 0, PocketDamagePiercing,
        PocketWeaponRanged | PocketWeaponAmmunition, "Bolts"),
    WEAPON("Shortbow", 20, 1, 6, 0, PocketDamagePiercing,
        PocketWeaponRanged | PocketWeaponAmmunition, "Arrows"),
    WEAPON("Sling", 0, 1, 4, 0, PocketDamageBludgeoning,
        PocketWeaponRanged | PocketWeaponAmmunition, "Sling bullets"),
    WEAPON("Battleaxe", 40, 1, 8, 10, PocketDamageSlashing, 0U, ""),
    WEAPON("Flail", 20, 1, 8, 0, PocketDamageBludgeoning, 0U, ""),
    WEAPON("Glaive", 60, 1, 10, 0, PocketDamageSlashing, PocketWeaponHeavy, ""),
    WEAPON("Greataxe", 70, 1, 12, 0, PocketDamageSlashing, PocketWeaponHeavy, ""),
    WEAPON("Greatsword", 60, 2, 6, 0, PocketDamageSlashing, PocketWeaponHeavy, ""),
    WEAPON("Halberd", 60, 1, 10, 0, PocketDamageSlashing, PocketWeaponHeavy, ""),
    WEAPON("Lance", 60, 1, 10, 0, PocketDamagePiercing, PocketWeaponHeavy, ""),
    WEAPON("Longsword", 30, 1, 8, 10, PocketDamageSlashing, 0U, ""),
    WEAPON("Maul", 100, 2, 6, 0, PocketDamageBludgeoning, PocketWeaponHeavy, ""),
    WEAPON("Morningstar", 40, 1, 8, 0, PocketDamagePiercing, 0U, ""),
    WEAPON("Pike", 180, 1, 10, 0, PocketDamagePiercing, PocketWeaponHeavy, ""),
    WEAPON("Rapier", 20, 1, 8, 0, PocketDamagePiercing, PocketWeaponFinesse, ""),
    WEAPON("Scimitar", 30, 1, 6, 0, PocketDamageSlashing,
        PocketWeaponFinesse | PocketWeaponLight, ""),
    WEAPON("Shortsword", 20, 1, 6, 0, PocketDamagePiercing,
        PocketWeaponFinesse | PocketWeaponLight, ""),
    WEAPON("Trident", 40, 1, 8, 10, PocketDamagePiercing, PocketWeaponThrown, ""),
    WEAPON("Warhammer", 50, 1, 8, 10, PocketDamageBludgeoning, 0U, ""),
    WEAPON("War Pick", 20, 1, 8, 10, PocketDamagePiercing, 0U, ""),
    WEAPON("Whip", 30, 1, 4, 0, PocketDamageSlashing, PocketWeaponFinesse, ""),
    WEAPON("Blowgun", 10, 1, 1, 0, PocketDamagePiercing,
        PocketWeaponRanged | PocketWeaponAmmunition, "Needles"),
    WEAPON("Hand Crossbow", 30, 1, 6, 0, PocketDamagePiercing,
        PocketWeaponRanged | PocketWeaponLight | PocketWeaponAmmunition, "Bolts"),
    WEAPON("Heavy Crossbow", 180, 1, 10, 0, PocketDamagePiercing,
        PocketWeaponRanged | PocketWeaponHeavy | PocketWeaponAmmunition, "Bolts"),
    WEAPON("Longbow", 20, 1, 8, 0, PocketDamagePiercing,
        PocketWeaponRanged | PocketWeaponHeavy | PocketWeaponAmmunition, "Arrows"),
    WEAPON("Musket", 100, 1, 12, 0, PocketDamagePiercing,
        PocketWeaponRanged | PocketWeaponAmmunition, "Bullets"),
    WEAPON("Pistol", 30, 1, 10, 0, PocketDamagePiercing,
        PocketWeaponRanged | PocketWeaponAmmunition, "Bullets"),
    ARMOR("Padded Armor", 80, 11, -1, 0),
    ARMOR("Leather Armor", 100, 11, -1, 0),
    ARMOR("Studded Leather Armor", 130, 12, -1, 0),
    ARMOR("Hide Armor", 120, 12, 2, 0),
    ARMOR("Chain Shirt", 200, 13, 2, 0),
    ARMOR("Scale Mail", 450, 14, 2, 0),
    ARMOR("Breastplate", 200, 14, 2, 0),
    ARMOR("Half Plate Armor", 400, 15, 2, 0),
    ARMOR("Ring Mail", 400, 14, 0, 0),
    ARMOR("Chain Mail", 550, 16, 0, 0),
    ARMOR("Splint Armor", 600, 17, 0, 0),
    ARMOR("Plate Armor", 650, 18, 0, 0),
    ARMOR("Shield", 60, 0, -1, 2),
};

#undef WEAPON
#undef ARMOR

static void pocket_apply_equipment_preset(
    PocketItem* item,
    const char* name,
    uint8_t category) {
    item->weight_tenths = 0;
    item->is_weapon = category == PocketItemCategoryWeapon;
    item->attack_ability = PocketAttackAbilityAuto;
    item->damage_dice = item->is_weapon ? 1U : 0U;
    item->damage_die = item->is_weapon ? 6U : 0U;
    item->versatile_die = 0U;
    item->damage_type = PocketDamageBludgeoning;
    item->add_ability_damage = item->is_weapon;
    item->weapon_properties = 0U;
    item->armor_base = 0U;
    item->armor_dex_cap = -1;
    item->shield_bonus = 0U;
    item->ammunition_group[0] = '\0';
    for(size_t i = 0U; i < sizeof(pocket_equipment_presets) / sizeof(pocket_equipment_presets[0]);
        ++i) {
        const PocketEquipmentPreset* preset = &pocket_equipment_presets[i];
        if(strcmp(name, preset->name) != 0) continue;
        item->weight_tenths = preset->weight_tenths;
        item->is_weapon = preset->damage_dice > 0U;
        item->damage_dice = preset->damage_dice;
        item->damage_die = preset->damage_die;
        item->versatile_die = preset->versatile_die;
        item->damage_type = preset->damage_type;
        item->weapon_properties = preset->properties;
        item->armor_base = preset->armor_base;
        item->armor_dex_cap = preset->armor_dex_cap;
        item->shield_bonus = preset->shield_bonus;
        item->add_ability_damage = item->is_weapon;
        pocket_copy(
            item->ammunition_group,
            sizeof(item->ammunition_group),
            preset->ammunition_group);
        break;
    }
}

static void pocket_catalog_add_item(
    PocketD20App* app,
    const char* name,
    uint8_t category,
    bool magic) {
    if(!pocket_catalog_add(app, name)) return;
    for(uint16_t i = 0U; i < app->catalog_count; ++i) {
        if(strcmp(app->catalog_entries[i], name) == 0) {
            app->catalog_item_categories[i] = category;
            app->catalog_item_magic[i] = magic ? 1U : 0U;
            return;
        }
    }
}

static void pocket_catalog_add_builtins(PocketD20App* app, PocketCatalogKind kind) {
    const char* const* entries = NULL;
    size_t count = 0U;
    switch(kind) {
    case PocketCatalogClasses:
        entries = pocket_catalog_classes;
        count = sizeof(pocket_catalog_classes) / sizeof(pocket_catalog_classes[0]);
        break;
    case PocketCatalogSubclasses:
        for(size_t i = 0U;
            i < sizeof(pocket_catalog_subclasses) / sizeof(pocket_catalog_subclasses[0]);
            ++i) {
            pocket_catalog_add_metadata(
                app,
                pocket_catalog_subclasses[i].name,
                0U,
                pocket_catalog_subclasses[i].class_mask,
                true);
            if(pocket_catalog_page_complete(app)) break;
        }
        return;
    case PocketCatalogSpecies:
        entries = pocket_catalog_species;
        count = sizeof(pocket_catalog_species) / sizeof(pocket_catalog_species[0]);
        break;
    case PocketCatalogBackgrounds:
        entries = pocket_catalog_backgrounds;
        count = sizeof(pocket_catalog_backgrounds) / sizeof(pocket_catalog_backgrounds[0]);
        break;
    case PocketCatalogAlignments:
        entries = pocket_catalog_alignments;
        count = sizeof(pocket_catalog_alignments) / sizeof(pocket_catalog_alignments[0]);
        break;
    case PocketCatalogSpells:
        for(size_t i = 0U; i < sizeof(pocket_catalog_spells) / sizeof(pocket_catalog_spells[0]);
            ++i) {
            pocket_catalog_add_metadata(
                app,
                pocket_catalog_spells[i].name,
                pocket_catalog_spells[i].level,
                pocket_catalog_spells[i].class_mask,
                true);
            if(pocket_catalog_page_complete(app)) break;
        }
        return;
    case PocketCatalogFeats:
        entries = pocket_catalog_feats;
        count = sizeof(pocket_catalog_feats) / sizeof(pocket_catalog_feats[0]);
        break;
    case PocketCatalogItems:
        entries = pocket_catalog_items;
        count = sizeof(pocket_catalog_items) / sizeof(pocket_catalog_items[0]);
        break;
    default:
        break;
    }
    for(size_t i = 0U; i < count; ++i) {
        pocket_catalog_add(app, entries[i]);
        if(pocket_catalog_page_complete(app)) break;
    }
}

static void pocket_catalog_process_line(PocketD20App* app, char* line) {
    char* start = line;
    while(*start == ' ' || *start == '\t') ++start;
    char* end = start + strlen(start);
    while(end > start && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r')) --end;
    *end = '\0';
    if(!start[0] || start[0] == '#') return;
    if(app->catalog_kind == PocketCatalogItems) {
        char* category_separator = strchr(start, '|');
        if(!category_separator) {
            pocket_catalog_add_item(app, start, PocketItemCategoryOther, false);
            return;
        }
        *category_separator = '\0';
        char* category = category_separator + 1U;
        char* rarity_separator = strchr(category, '|');
        if(rarity_separator) *rarity_separator = '\0';
        char* category_end = category + strlen(category);
        while(category_end > category && (category_end[-1] == ' ' || category_end[-1] == '\t'))
            --category_end;
        *category_end = '\0';
        bool magic = false;
        if(rarity_separator) {
            char* rarity = rarity_separator + 1U;
            char* source_separator = strchr(rarity, '|');
            if(source_separator) *source_separator = '\0';
            while(*rarity == ' ' || *rarity == '\t') ++rarity;
            magic = strcmp(rarity, "Mundane") != 0;
        }
        pocket_catalog_add_item(app, start, pocket_item_category_from_name(category), magic);
        return;
    }
    if(app->catalog_kind == PocketCatalogSubclasses) {
        char* class_separator = strrchr(start, '|');
        if(!class_separator) {
            pocket_catalog_add_metadata(app, start, 0U, 0U, false);
            return;
        }
        *class_separator = '\0';
        char* class_name = class_separator + 1U;
        while(*class_name == ' ' || *class_name == '\t') ++class_name;
        uint16_t mask = pocket_class_mask_from_name(class_name);
        pocket_catalog_add_metadata(app, start, 0U, mask, mask != 0U);
        return;
    }
    if(app->catalog_kind != PocketCatalogSpells) {
        pocket_catalog_add(app, start);
        return;
    }

    char* level_separator = strchr(start, '|');
    if(!level_separator) {
        pocket_catalog_add_metadata(app, start, 0U, 0U, false);
        return;
    }
    *level_separator = '\0';
    char* class_separator = strchr(level_separator + 1U, '|');
    if(!class_separator) {
        pocket_catalog_add_metadata(app, start, 0U, 0U, false);
        return;
    }
    *class_separator = '\0';
    long parsed_level = strtol(level_separator + 1U, NULL, 10);
    if(parsed_level < 0 || parsed_level > 9) {
        pocket_catalog_add_metadata(app, start, 0U, 0U, false);
        return;
    }
    uint16_t mask = 0U;
    char* metadata_separator = strchr(class_separator + 1U, '|');
    char* school = NULL;
    char* ritual = NULL;
    char* source = NULL;
    if(metadata_separator) {
        *metadata_separator = '\0';
        school = metadata_separator + 1U;
        char* ritual_separator = strchr(school, '|');
        if(ritual_separator) {
            *ritual_separator = '\0';
            ritual = ritual_separator + 1U;
            char* source_separator = strchr(ritual, '|');
            if(source_separator) {
                *source_separator = '\0';
                source = source_separator + 1U;
            }
        }
    }
    char* class_name = class_separator + 1U;
    while(class_name && class_name[0]) {
        char* comma = strchr(class_name, ',');
        if(comma) *comma = '\0';
        while(*class_name == ' ' || *class_name == '\t') ++class_name;
        char* class_end = class_name + strlen(class_name);
        while(class_end > class_name && (class_end[-1] == ' ' || class_end[-1] == '\t'))
            --class_end;
        *class_end = '\0';
        mask |= pocket_class_mask_from_name(class_name);
        class_name = comma ? comma + 1U : NULL;
    }
    pocket_catalog_add_metadata(app, start, (uint8_t)parsed_level, mask, mask != 0U);
    for(uint16_t i = 0U; i < app->catalog_count; ++i) {
        if(strcmp(app->catalog_entries[i], start) == 0) {
            if(school) {
                for(uint8_t school_index = 1U; school_index < 9U; ++school_index)
                    if(strcmp(school, pocket_spell_school_names[school_index]) == 0)
                        app->catalog_schools[i] = school_index;
            }
            if(source) app->catalog_sources[i] = strcmp(source, "Core") == 0 ? 1U : 2U;
            if(ritual) app->catalog_ritual[i] = strcmp(ritual, "1") == 0 || strcmp(ritual, "Yes") == 0;
            break;
        }
    }
}

static bool pocket_catalog_load_path(PocketD20App* app, const char* path) {
    File* file = storage_file_alloc(app->storage);
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
                pocket_catalog_process_line(app, line);
                position = 0U;
                if(pocket_catalog_page_complete(app)) {
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
        pocket_catalog_process_line(app, line);
    }
finished:
    storage_file_close(file);
    storage_file_free(file);
    return complete;
}

static void pocket_catalog_load_external(PocketD20App* app, PocketCatalogKind kind) {
    if(kind >= PocketCatalogCount) return;
    bool complete = pocket_catalog_load_path(app, pocket_bundled_catalog_paths[kind]);
    if(complete && kind == PocketCatalogFeats)
        pocket_catalog_load_path(app, pocket_bundled_catalog_abilities_path);
}

static uint8_t pocket_grant_source_from_text(const char* text) {
    if(strcmp(text, "species") == 0) return PocketGrantSpecies;
    if(strcmp(text, "background") == 0) return PocketGrantBackground;
    if(strcmp(text, "feat") == 0) return PocketGrantFeat;
    if(strcmp(text, "class_feature") == 0) return PocketGrantClassFeature;
    if(strcmp(text, "subclass_feature") == 0) return PocketGrantSubclassFeature;
    if(strcmp(text, "item") == 0) return PocketGrantItem;
    return PocketGrantSourceCount;
}

static bool pocket_metadata_option_valid(const char* text) {
    return pocket_grant_source_from_text(text) < PocketGrantSourceCount ||
           strcmp(text, "spell") == 0 || strcmp(text, "ability") == 0 ||
           strcmp(text, "class") == 0;
}

static uint8_t pocket_split_metadata(char* line, char* fields[8]) {
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

static uint8_t pocket_split_fields(char* line, char** fields, uint8_t maximum) {
    uint8_t count = 0U;
    char* cursor = line;
    while(count < maximum) {
        fields[count++] = cursor;
        char* separator = strchr(cursor, '|');
        if(!separator) break;
        *separator = '\0';
        cursor = separator + 1U;
    }
    return count;
}

static void pocket_adventure_release(PocketD20App* app) {
    free(app->adventure_scene);
    app->adventure_scene = NULL;
}

static void pocket_adventure_process_line(
    PocketAdventureScene* scene,
    const char* target,
    char* line,
    bool* found) {
    if(!line[0] || line[0] == '#') return;
    char* fields[11];
    uint8_t count = pocket_split_fields(line, fields, 11U);
    if(count == 5U && strcmp(fields[0], "S") == 0 && strcmp(fields[1], target) == 0) {
        pocket_copy(scene->id, sizeof(scene->id), fields[1]);
        pocket_copy(scene->title, sizeof(scene->title), fields[2]);
        pocket_copy(scene->body, sizeof(scene->body), fields[3]);
        pocket_copy(scene->sprite, sizeof(scene->sprite), fields[4]);
        *found = true;
    } else if(count == 11U && strcmp(fields[0], "C") == 0 &&
              strcmp(fields[1], target) == 0 &&
              scene->choice_count < POCKET_ADVENTURE_MAX_CHOICES) {
        PocketAdventureChoice* choice = &scene->choices[scene->choice_count++];
        pocket_copy(choice->label, sizeof(choice->label), fields[2]);
        choice->skill = (int8_t)strtol(fields[3], NULL, 10);
        choice->dc = (uint8_t)strtoul(fields[4], NULL, 10);
        pocket_copy(choice->success_scene, sizeof(choice->success_scene), fields[5]);
        pocket_copy(choice->failure_scene, sizeof(choice->failure_scene), fields[6]);
        pocket_copy(choice->reward_item, sizeof(choice->reward_item), fields[7]);
        pocket_copy(choice->milestone, sizeof(choice->milestone), fields[8]);
        choice->quest_flag = (uint8_t)strtoul(fields[9], NULL, 10);
        choice->achievement = (uint8_t)strtoul(fields[10], NULL, 10);
    }
}

static bool pocket_campaign_resolve_active(PocketD20App* app) {
    if(app->campaign_active_valid &&
       !strcmp(app->campaign_active.id, app->data.character.adventure_campaign))
        return true;
    PocketCampaignSummary campaign;
    bool found = app->data.character.adventure_campaign[0] &&
                 pocket_campaign_find(
                     app->storage, app->data.character.adventure_campaign, &campaign);
    if(!found) found = pocket_campaign_at(app->storage, 0U, &campaign);
    if(!found) {
        app->campaign_active_valid = 0U;
        return false;
    }
    app->campaign_active = campaign;
    app->campaign_active_valid = 1U;
    if(strcmp(app->data.character.adventure_campaign, campaign.id))
        pocket_campaign_progress_load(
            app->storage, app->profiles.active_profile, &campaign, &app->data.character);
    return true;
}

static bool pocket_campaign_save_active_progress(PocketD20App* app) {
    return pocket_campaign_resolve_active(app) &&
           pocket_campaign_progress_save(
               app->storage,
               app->profiles.active_profile,
               &app->campaign_active,
               &app->data.character);
}

static bool pocket_adventure_load(PocketD20App* app) {
    pocket_adventure_release(app);
    PocketAdventureScene* scene = malloc(sizeof(PocketAdventureScene));
    if(!scene) {
        pocket_set_status(app, "Adventure memory low");
        return false;
    }
    memset(scene, 0, sizeof(*scene));
    char path[192];
    if(!pocket_campaign_resolve_active(app) ||
       !pocket_campaign_scene_path(
           app->storage, &app->campaign_active, path, sizeof(path))) {
        free(scene);
        pocket_set_status(app, "Campaign manifest missing");
        return false;
    }
    File* file = storage_file_alloc(app->storage);
    if(!storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_free(file);
        free(scene);
        pocket_set_status(app, "Campaign file missing");
        return false;
    }
    char line[512];
    size_t position = 0U;
    char byte = '\0';
    bool found = false;
    while(storage_file_read(file, &byte, 1U) == 1U) {
        if(byte == '\n') {
            line[position] = '\0';
            pocket_adventure_process_line(
                scene, app->data.character.adventure_scene, line, &found);
            position = 0U;
        } else if(byte != '\r' && position + 1U < sizeof(line)) {
            line[position++] = byte;
        }
    }
    if(position) {
        line[position] = '\0';
        pocket_adventure_process_line(
            scene, app->data.character.adventure_scene, line, &found);
    }
    storage_file_close(file);
    storage_file_free(file);
    if(!found || !scene->choice_count) {
        free(scene);
        pocket_set_status(app, "Scene invalid");
        return false;
    }
    app->adventure_scene = scene;
    return true;
}

static void pocket_adventure_reward_item(PocketCharacter* character, const char* name) {
    if(!name[0] || strcmp(name, "-") == 0) return;
    for(uint8_t i = 0U; i < character->item_count; ++i) {
        if(strcmp(character->items[i].name, name) == 0) {
            if(character->items[i].quantity < 999) ++character->items[i].quantity;
            return;
        }
    }
    if(character->item_count >= POCKET_D20_MAX_ITEMS) return;
    if(!pocket_d20_data_reserve_items(character, character->item_count + 1U)) return;
    PocketItem* item = &character->items[character->item_count++];
    memset(item, 0, sizeof(*item));
    pocket_copy(item->name, sizeof(item->name), name);
    pocket_copy(item->detail, sizeof(item->detail), "Adventure reward");
    item->quantity = 1;
    item->container_index = -1;
    item->armor_dex_cap = -1;
}

static void pocket_adventure_reward_milestone(PocketCharacter* character, const char* title) {
    if(!title[0] || strcmp(title, "-") == 0) return;
    for(uint8_t i = 0U; i < character->journal_count; ++i)
        if(character->journal[i].category == PocketJournalMilestone &&
           strcmp(character->journal[i].title, title) == 0)
            return;
    if(character->journal_count >= POCKET_D20_MAX_JOURNAL) return;
    if(!pocket_d20_data_reserve_journal(character, character->journal_count + 1U)) return;
    PocketJournalEntry* entry = &character->journal[character->journal_count++];
    memset(entry, 0, sizeof(*entry));
    pocket_copy(entry->title, sizeof(entry->title), title);
    pocket_copy(entry->body, sizeof(entry->body), "Milestone earned in Adventure mode.");
    entry->category = PocketJournalMilestone;
}

static uint32_t pocket_string_hash(const char* value) {
    uint32_t hash = 2166136261UL;
    while(*value) {
        hash ^= (uint8_t)*value++;
        hash *= 16777619UL;
    }
    return hash;
}

static void pocket_run_catalog_diagnostics(PocketD20App* app) {
    pocket_catalog_release(app);
    app->diagnostics_records = 0U;
    app->diagnostics_invalid = 0U;
    app->diagnostics_duplicates = 0U;
    app->diagnostics_catalogs = 0U;
    File* file = storage_file_alloc(app->storage);
    if(!storage_file_open(
           file, pocket_active_metadata_path(app->storage), FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_free(file);
        return;
    }
    char line[256];
    size_t position = 0U;
    char byte = '\0';
    uint32_t* seen_ids = NULL;
    uint16_t seen_count = 0U;
    uint16_t seen_capacity = 0U;
    while(storage_file_read(file, &byte, 1U) == 1U) {
        if(byte != '\n' && position + 1U < sizeof(line)) {
            if(byte != '\r') line[position++] = byte;
            continue;
        }
        line[position] = '\0';
        position = 0U;
        if(!line[0] || line[0] == '#') continue;
        char* fields[8];
        uint8_t count = pocket_split_metadata(line, fields);
        ++app->diagnostics_records;
        bool valid = count == 8U && fields[0][0] && fields[1][0] && fields[2][0] &&
                     fields[3][0] && pocket_metadata_option_valid(fields[2]);
        char* end = NULL;
        long level = count == 8U ? strtol(fields[5], &end, 10) : -1;
        if(!valid || !end || *end || level < 0 || level > 20) {
            ++app->diagnostics_invalid;
            continue;
        }
        uint32_t id_hash = pocket_string_hash(fields[0]);
        bool duplicate = false;
        for(uint16_t i = 0U; i < seen_count; ++i)
            if(seen_ids[i] == id_hash) duplicate = true;
        if(duplicate)
            ++app->diagnostics_duplicates;
        else {
            if(seen_count == seen_capacity) {
                uint16_t next_capacity = seen_capacity ? (uint16_t)(seen_capacity * 2U) : 64U;
                uint32_t* resized = realloc(seen_ids, (size_t)next_capacity * sizeof(uint32_t));
                if(!resized) {
                    ++app->diagnostics_invalid;
                    continue;
                }
                seen_ids = resized;
                seen_capacity = next_capacity;
            }
            seen_ids[seen_count++] = id_hash;
        }
    }
    storage_file_close(file);
    storage_file_free(file);
    free(seen_ids);
    for(uint8_t kind = 0U; kind < PocketCatalogCount; ++kind)
        if(storage_file_exists(app->storage, pocket_bundled_catalog_paths[kind]))
            ++app->diagnostics_catalogs;
    pocket_catalog_release(app);
}

static uint8_t pocket_stage_grants(PocketD20App* app, uint8_t source_type, const char* option) {
    PocketCharacter* character = &app->data.character;
    File* file = storage_file_alloc(app->storage);
    if(!storage_file_open(
           file, pocket_active_metadata_path(app->storage), FSAM_READ, FSOM_OPEN_EXISTING)) {
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
            if(!line[0] || line[0] == '#' ||
               character->grant_count >= POCKET_D20_MAX_GRANTS)
                continue;
            char* fields[8];
            if(pocket_split_metadata(line, fields) != 8U ||
               pocket_grant_source_from_text(fields[2]) != source_type ||
               strcmp(fields[3], option) != 0 || !fields[7][0])
                continue;
            if(!pocket_d20_data_reserve_grants(character, character->grant_count + 1U))
                continue;
            PocketGrant* grant = &character->grants[character->grant_count++];
            memset(grant, 0, sizeof(*grant));
            pocket_copy(grant->stable_id, sizeof(grant->stable_id), fields[0]);
            pocket_copy(grant->source, sizeof(grant->source), fields[1]);
            pocket_copy(grant->option_name, sizeof(grant->option_name), fields[3]);
            pocket_copy(grant->prerequisites, sizeof(grant->prerequisites), fields[4]);
            pocket_copy(grant->grant_value, sizeof(grant->grant_value), fields[7]);
            grant->source_type = source_type;
            grant->class_index =
                app->record_index < character->class_count ? app->record_index : 0U;
            grant->level_gained = (uint8_t)strtoul(fields[5], NULL, 10);
            grant->status = PocketGrantPending;
            ++staged;
        }
    }
    storage_file_close(file);
    storage_file_free(file);
    return staged;
}

static void pocket_apply_grant(PocketD20App* app, PocketGrant* grant) {
    PocketCharacter* character = &app->data.character;
    char payload[POCKET_D20_NAME_LEN];
    pocket_copy(payload, sizeof(payload), grant->grant_value);
    char* separator = strchr(payload, '=');
    if(!separator) {
        grant->status = PocketGrantSkipped;
        return;
    }
    *separator = '\0';
    const char* value = separator + 1U;
    if(strcmp(payload, "origin_feat") == 0)
        pocket_copy(character->origin_feat, sizeof(character->origin_feat), value);
    else if(strcmp(payload, "tool") == 0)
        pocket_copy(character->tool_proficiencies, sizeof(character->tool_proficiencies), value);
    else if(strcmp(payload, "armor") == 0)
        pocket_copy(character->armor_training, sizeof(character->armor_training), value);
    else if(strcmp(payload, "weapon") == 0)
        pocket_copy(character->weapon_training, sizeof(character->weapon_training), value);
    else if(strcmp(payload, "senses") == 0)
        pocket_copy(character->senses, sizeof(character->senses), value);
    else if(strcmp(payload, "size") == 0) {
        for(uint8_t i = 0U; i < PocketSizeCount; ++i)
            if(strcmp(value, pocket_size_names[i]) == 0) character->size = i;
    } else if(strcmp(payload, "feature") == 0 &&
              character->feature_count < POCKET_D20_MAX_FEATURES &&
              pocket_d20_data_reserve_features(character, character->feature_count + 1U)) {
        PocketFeature* feature = &character->features[character->feature_count++];
        memset(feature, 0, sizeof(*feature));
        pocket_copy(feature->name, sizeof(feature->name), value);
        feature->class_index = grant->class_index;
        feature->class_level_gained = grant->level_gained;
    } else if(strcmp(payload, "spell") == 0 &&
              character->spell_count < POCKET_D20_MAX_SPELLS &&
              pocket_d20_data_reserve_spells(character, character->spell_count + 1U)) {
        uint8_t index = character->spell_count++;
        PocketSpell* spell = &character->spells[index];
        memset(spell, 0, sizeof(*spell));
        pocket_copy(spell->name, sizeof(spell->name), value);
        pocket_copy(spell->source, sizeof(spell->source), grant->source);
        pocket_copy(spell->grant_name, sizeof(spell->grant_name), grant->option_name);
        pocket_copy(spell->stable_id, sizeof(spell->stable_id), grant->stable_id);
        spell->class_index = grant->class_index;
        spell->grant_source = grant->source_type;
        character->spell_known[index] = 1U;
        character->spell_always_prepared[index] = 1U;
    }
    *separator = '=';
    grant->status = PocketGrantApplied;
}

static bool pocket_tracked_spell_matches_filter(const PocketD20App* app, const char* name) {
    if(!app->spell_filter_prepared) return true;
    const PocketCharacter* character = &app->data.character;
    for(uint8_t i = 0U; i < character->spell_count; ++i) {
        if(strcmp(character->spells[i].name, name) != 0) continue;
        if(app->spell_filter_prepared == 1U) return character->spells[i].prepared;
        if(app->spell_filter_prepared == 2U) return character->spell_known[i];
        return character->spell_always_prepared[i];
    }
    return false;
}

static void pocket_catalog_apply_spell_filters(PocketD20App* app) {
    if(app->catalog_kind != PocketCatalogSpells) return;
    uint16_t output = 0U;
    for(uint16_t i = 0U; i < app->catalog_count; ++i) {
        bool keep = true;
        if(app->spell_filter_level >= 0 && app->catalog_levels[i] != (uint8_t)app->spell_filter_level)
            keep = false;
        if(app->spell_filter_ritual && !app->catalog_ritual[i]) keep = false;
        if(app->spell_filter_school && app->catalog_schools[i] != app->spell_filter_school)
            keep = false;
        if(app->spell_filter_source == 1U && app->catalog_sources[i] != 1U)
            keep = false;
        if(app->spell_filter_source == 2U && app->catalog_sources[i] != 2U)
            keep = false;
        if(!pocket_tracked_spell_matches_filter(app, app->catalog_entries[i])) keep = false;
        if(!keep) continue;
        if(output != i) {
            memcpy(app->catalog_entries[output], app->catalog_entries[i], POCKET_D20_NAME_LEN);
            app->catalog_levels[output] = app->catalog_levels[i];
            app->catalog_class_masks[output] = app->catalog_class_masks[i];
            app->catalog_has_metadata[output] = app->catalog_has_metadata[i];
            app->catalog_schools[output] = app->catalog_schools[i];
            app->catalog_sources[output] = app->catalog_sources[i];
            app->catalog_ritual[output] = app->catalog_ritual[i];
        }
        ++output;
    }
    app->catalog_count = output;
}

static bool pocket_catalog_spell_after(
    const PocketD20App* app,
    uint16_t left,
    uint16_t right) {
    uint8_t left_level = app->catalog_has_metadata[left] ? app->catalog_levels[left] : UINT8_MAX;
    uint8_t right_level =
        app->catalog_has_metadata[right] ? app->catalog_levels[right] : UINT8_MAX;
    if(left_level != right_level) return left_level > right_level;
    return strcmp(app->catalog_entries[left], app->catalog_entries[right]) > 0;
}

static void pocket_catalog_swap(PocketD20App* app, uint16_t left, uint16_t right) {
    char name[POCKET_D20_NAME_LEN];
    memcpy(name, app->catalog_entries[left], sizeof(name));
    memcpy(app->catalog_entries[left], app->catalog_entries[right], sizeof(name));
    memcpy(app->catalog_entries[right], name, sizeof(name));
#define POCKET_SWAP_VALUE(values, type)      \
    do {                                     \
        type temporary = (values)[left];     \
        (values)[left] = (values)[right];    \
        (values)[right] = temporary;         \
    } while(false)
    POCKET_SWAP_VALUE(app->catalog_levels, uint8_t);
    POCKET_SWAP_VALUE(app->catalog_class_masks, uint16_t);
    POCKET_SWAP_VALUE(app->catalog_has_metadata, uint8_t);
    POCKET_SWAP_VALUE(app->catalog_item_categories, uint8_t);
    POCKET_SWAP_VALUE(app->catalog_item_magic, uint8_t);
    POCKET_SWAP_VALUE(app->catalog_schools, uint8_t);
    POCKET_SWAP_VALUE(app->catalog_sources, uint8_t);
    POCKET_SWAP_VALUE(app->catalog_ritual, uint8_t);
#undef POCKET_SWAP_VALUE
}

static void pocket_catalog_sort_spells(PocketD20App* app) {
    if(app->catalog_kind != PocketCatalogSpells) return;
    for(uint16_t index = 1U; index < app->catalog_count; ++index) {
        uint16_t position = index;
        while(position && pocket_catalog_spell_after(app, position - 1U, position)) {
            pocket_catalog_swap(app, position - 1U, position);
            --position;
        }
    }
}

static void pocket_catalog_load_page(PocketD20App* app) {
    PocketCatalogKind kind = app->catalog_kind;
    pocket_catalog_release(app);
    app->catalog_scan_count = 0U;
    app->catalog_has_more = 0U;
    if(!storage_file_exists(app->storage, pocket_bundled_catalog_paths[kind]))
        pocket_catalog_add_builtins(app, kind);
    pocket_catalog_load_external(app, kind);
    if(pocket_catalog_page_complete(app)) app->catalog_has_more = 1U;
    app->catalog_total = app->catalog_scan_count;
    pocket_catalog_apply_spell_filters(app);
    pocket_catalog_sort_spells(app);
    if(app->catalog_page_start >= app->catalog_total && app->catalog_page_start) {
        uint16_t page_limit = pocket_catalog_page_limit(app);
        app->catalog_page_start =
            ((app->catalog_total ? app->catalog_total - 1U : 0U) /
             page_limit) * page_limit;
        pocket_catalog_release(app);
        app->catalog_scan_count = 0U;
        app->catalog_has_more = 0U;
        if(!storage_file_exists(app->storage, pocket_bundled_catalog_paths[kind]))
            pocket_catalog_add_builtins(app, kind);
        pocket_catalog_load_external(app, kind);
        if(pocket_catalog_page_complete(app)) app->catalog_has_more = 1U;
        app->catalog_total = app->catalog_scan_count;
        pocket_catalog_apply_spell_filters(app);
        pocket_catalog_sort_spells(app);
    }
}

static void pocket_open_catalog(
    PocketD20App* app,
    PocketCatalogKind kind,
    PocketEditTarget target,
    const char* current) {
    pocket_release_text_input(app);
    pocket_release_number_input(app);
    app->catalog_kind = kind;
    app->catalog_target = target;
    app->catalog_page_size = kind == PocketCatalogSpells ? POCKET_D20_SPELL_PAGE_ENTRIES :
                                                          POCKET_D20_MAX_CATALOG_ENTRIES;
    app->return_screen = app->screen;
    app->catalog_return_selection = app->selection;
    app->catalog_show_all = 0U;
    app->catalog_page_start = 0U;
    pocket_catalog_load_page(app);
    pocket_enter_screen(app, PocketScreenCatalog);
    for(uint16_t i = 0U; i < app->catalog_count; ++i) {
        if(strcmp(app->catalog_entries[i], current) == 0) {
            app->selection = i;
            if(i >= 5U) app->scroll = i - 4U;
            break;
        }
    }
}

static void pocket_draw_header(Canvas* canvas, const char* title, const char* status) {
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

static void pocket_truncate(char* output, size_t size, const char* text, size_t maximum) {
    if(size == 0U) return;
    size_t length = strlen(text);
    if(length <= maximum) {
        pocket_copy(output, size, text);
        return;
    }
    size_t copy_length = maximum > 3U ? maximum - 3U : maximum;
    if(copy_length >= size) copy_length = size - 1U;
    memcpy(output, text, copy_length);
    output[copy_length] = '\0';
    if(maximum > 3U && copy_length + 3U < size) strcat(output, "...");
}

static void pocket_draw_row(Canvas* canvas, uint8_t row, bool selected, const char* text) {
    uint8_t y = (uint8_t)(11U + (row * 10U));
    char display[32];
    size_t length = strlen(text);
    if(selected && length > 20U) {
        size_t cycle = length + 4U;
        size_t start = pocket_marquee_offset % cycle;
        for(size_t i = 0U; i < 20U; ++i) {
            size_t position = (start + i) % cycle;
            display[i] = position < length ? text[position] : ' ';
        }
        display[20] = '\0';
    } else {
        size_t copy = length > 20U ? 20U : length;
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

static void pocket_draw_menu_rows(
    Canvas* canvas,
    PocketD20App* app,
    const char* const* rows,
    uint16_t count) {
    for(uint8_t visible = 0U; visible < 5U; ++visible) {
        uint16_t index = app->scroll + visible;
        if(index >= count) break;
        pocket_draw_row(canvas, visible, index == app->selection, rows[index]);
    }
}

static uint8_t pocket_list_count(const PocketD20App* app) {
    const PocketCharacter* character = &app->data.character;
    switch(app->list_kind) {
    case PocketListClasses:
        return character->class_count;
    case PocketListSpells:
        return character->spell_count;
    case PocketListFeatures:
        return character->feature_count;
    case PocketListItems:
        return character->item_count;
    case PocketListLanguages:
        return character->language_count;
    case PocketListJournal:
        return character->journal_count;
    case PocketListParty:
        return app->data.party_count;
    default:
        return 0U;
    }
}

static const char* pocket_list_title(PocketListKind kind) {
    switch(kind) {
    case PocketListClasses:
        return "Classes";
    case PocketListSpells:
        return "Spells";
    case PocketListFeatures:
        return "Features / Perks";
    case PocketListItems:
        return "Inventory";
    case PocketListLanguages:
        return "Languages";
    case PocketListJournal:
        return "Journal";
    case PocketListParty:
        return "Party Roster";
    default:
        return "List";
    }
}

static void pocket_format_list_entry(
    const PocketD20App* app,
    uint8_t index,
    char* output,
    size_t size) {
    const PocketCharacter* character = &app->data.character;
    switch(app->list_kind) {
    case PocketListClasses: {
        const PocketClassLevel* class_level = &character->classes[index];
        snprintf(output, size, "%.31s L%u", class_level->name, class_level->level);
        break;
    }
    case PocketListSpells: {
        const PocketSpell* spell = &character->spells[index];
        snprintf(
            output,
            size,
            "%c%c L%u %.31s",
            pocket_spell_status(character, index),
            character->spell_free_casts_current[index] ? 'F' : ' ',
            spell->level,
            spell->name);
        break;
    }
    case PocketListFeatures: {
        const PocketFeature* feature = &character->features[index];
        pocket_format_labeled_text(output, size, NULL, feature->name);
        break;
    }
    case PocketListItems: {
        const PocketItem* item = &character->items[index];
        snprintf(
            output,
            size,
            "%c %dx %.31s",
            item->equipped ? '*' : ' ',
            item->quantity,
            item->name);
        break;
    }
    case PocketListLanguages:
        pocket_format_labeled_text(output, size, NULL, character->languages[index]);
        break;
    case PocketListJournal: {
        const PocketJournalEntry* entry = &character->journal[index];
        snprintf(
            output,
            size,
            "%c %.15s: %.31s",
            entry->completed ? 'X' : ' ',
            pocket_d20_journal_category_names[entry->category],
            entry->title);
        break;
    }
    case PocketListParty: {
        const PocketPartyMember* member = &app->data.party[index];
        snprintf(
            output,
            size,
            "%.23s %+d HP%d/%d AC%d",
            member->name,
            member->initiative_modifier,
            member->hp_current,
            member->hp_max,
            member->armor_class);
        break;
    }
    }
}

static uint8_t pocket_weapon_count(const PocketD20App* app) {
    uint8_t count = 0U;
    for(uint8_t i = 0U; i < app->data.character.item_count; ++i) {
        if(app->data.character.items[i].is_weapon) ++count;
    }
    return count;
}

static uint8_t pocket_weapon_index(const PocketD20App* app, uint8_t weapon_number) {
    uint8_t count = 0U;
    for(uint8_t i = 0U; i < app->data.character.item_count; ++i) {
        if(app->data.character.items[i].is_weapon) {
            if(count == weapon_number) return i;
            ++count;
        }
    }
    return 0U;
}

static uint8_t pocket_record_detail_count(const PocketD20App* app) {
    switch(app->list_kind) {
    case PocketListClasses:
        return 18U;
    case PocketListSpells:
        return 17U;
    case PocketListFeatures:
        return 10U;
    case PocketListItems:
        return 36U;
    case PocketListLanguages:
        return 2U;
    case PocketListJournal:
        return 8U;
    case PocketListParty:
        return 6U;
    default:
        return 0U;
    }
}

static void pocket_release_text_input(PocketD20App* app) {
    if(!app->text_input || app->input_module_active) return;
    view_dispatcher_remove_view(app->dispatcher, PocketViewTextInput);
    text_input_free(app->text_input);
    app->text_input = NULL;
}

static void pocket_release_number_input(PocketD20App* app) {
    if(!app->number_input || app->input_module_active) return;
    view_dispatcher_remove_view(app->dispatcher, PocketViewNumberInput);
    number_input_free(app->number_input);
    app->number_input = NULL;
}

static void pocket_begin_text(
    PocketD20App* app,
    PocketEditTarget target,
    const char* header,
    const char* initial) {
    pocket_release_number_input(app);
    if(!app->text_input) {
        app->text_input = text_input_alloc();
        if(!app->text_input) {
            pocket_set_status(app, "Text input memory low");
            return;
        }
        view_dispatcher_add_view(
            app->dispatcher, PocketViewTextInput, text_input_get_view(app->text_input));
    }
    app->edit_target = target;
    app->input_module_active = 1U;
    pocket_copy(app->edit_buffer, sizeof(app->edit_buffer), initial);
    text_input_reset(app->text_input);
    text_input_set_header_text(app->text_input, header);
    text_input_set_result_callback(
        app->text_input,
        pocket_text_done,
        app,
        app->edit_buffer,
        sizeof(app->edit_buffer),
        false);
    view_dispatcher_switch_to_view(app->dispatcher, PocketViewTextInput);
}

static uint8_t pocket_nearest_die(int32_t number, bool damage_only) {
    const uint8_t* choices = damage_only ? pocket_damage_die_choices : pocket_die_choices;
    uint8_t count = damage_only ? sizeof(pocket_damage_die_choices) : sizeof(pocket_die_choices);
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

static void pocket_number_done(void* context, int32_t number) {
    PocketD20App* app = context;
    PocketCharacter* character = &app->data.character;
    switch(app->number_context) {
    case PocketNumberCurrency: {
        int32_t* values[] = {&character->currency_cp, &character->currency_sp,
            &character->currency_ep, &character->currency_gp, &character->currency_pp};
        if(app->number_index < 5U) *values[app->number_index] = number;
        break;
    }
    case PocketNumberCharacter:
        if(app->number_index == 7U) character->experience = (uint32_t)number;
        break;
    case PocketNumberVitals:
        switch(app->number_index) {
        case 0U: character->hp_current = (int16_t)number; break;
        case 1U: character->hp_max = (int16_t)number; break;
        case 2U: character->hp_temporary = (int16_t)number; break;
        case 3U: character->armor_class = (int16_t)number; break;
        case 4U: character->speed = (int16_t)number; break;
        case 5U:
        case 6U: character->initiative_misc = (int8_t)number; break;
        case 7U: character->exhaustion = (uint8_t)number; break;
        case 8U: character->death_successes = (uint8_t)number; break;
        case 9U: character->death_failures = (uint8_t)number; break;
        case 10U: character->hit_die = pocket_nearest_die(number, true); break;
        case 11U: character->hit_dice_current = (uint8_t)number; break;
        case 12U: character->hit_dice_max = (uint8_t)number; break;
        case 13U: character->skill_misc[11U] = (int8_t)number; break;
        case 14U: character->skill_misc[6U] = (int8_t)number; break;
        case 15U: character->skill_misc[8U] = (int8_t)number; break;
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
        if(app->record_index >= pocket_list_count(app)) break;
        if(app->list_kind == PocketListClasses) {
            PocketClassLevel* level = &character->classes[app->record_index];
            switch(app->number_index) {
            case 2U: level->level = (uint8_t)number; break;
            case 3U: level->hit_die = pocket_nearest_die(number, true); break;
            case 4U: level->hit_dice_current = (uint8_t)number; break;
            case 5U: level->hit_dice_max = (uint8_t)number; break;
            case 8U: level->cantrip_limit = (uint8_t)number; break;
            case 9U: level->prepared_limit = (uint8_t)number; break;
            case 10U: level->spellbook_size = (uint16_t)number; break;
            case 11U: level->pact_slot_level = (uint8_t)number; break;
            case 12U: level->pact_slots_current = (uint8_t)number; break;
            case 13U: level->pact_slots_max = (uint8_t)number; break;
            case 15U: level->spell_points_current = (uint16_t)number; break;
            case 16U: level->spell_points_max = (uint16_t)number; break;
            }
            if(level->hit_dice_current > level->hit_dice_max)
                level->hit_dice_current = level->hit_dice_max;
            if(level->pact_slots_current > level->pact_slots_max)
                level->pact_slots_current = level->pact_slots_max;
            if(level->spell_points_current > level->spell_points_max)
                level->spell_points_current = level->spell_points_max;
        } else if(app->list_kind == PocketListSpells) {
            if(app->number_index == 3U)
                character->spells[app->record_index].level = (uint8_t)number;
            else if(app->number_index == 8U)
                character->spell_free_casts_current[app->record_index] = (uint8_t)number;
            else if(app->number_index == 9U)
                character->spell_free_casts_max[app->record_index] = (uint8_t)number;
            if(character->spell_free_casts_current[app->record_index] >
               character->spell_free_casts_max[app->record_index])
                character->spell_free_casts_current[app->record_index] =
                    character->spell_free_casts_max[app->record_index];
        } else if(app->list_kind == PocketListFeatures) {
            PocketFeature* feature = &character->features[app->record_index];
            if(app->number_index == 3U) feature->class_level_gained = (uint8_t)number;
            else if(app->number_index == 4U) feature->uses_current = (int16_t)number;
            else if(app->number_index == 5U) feature->uses_max = (int16_t)number;
            if(feature->uses_current > feature->uses_max)
                feature->uses_current = feature->uses_max;
        } else if(app->list_kind == PocketListItems) {
            PocketItem* item = &character->items[app->record_index];
            switch(app->number_index) {
            case 2U: item->quantity = (int16_t)number; break;
            case 3U: item->weight_tenths = (int16_t)number; break;
            case 9U: item->magic_bonus = (int8_t)number; break;
            case 10U: item->damage_dice = (uint8_t)number; break;
            case 11U: item->damage_die = pocket_nearest_die(number, true); break;
            case 13U: item->versatile_die = pocket_nearest_die(number, true); break;
            case 23U: item->extra_dice = (uint8_t)number; break;
            case 24U: item->extra_die = pocket_nearest_die(number, true); break;
            case 25U: item->ammo_current = (int16_t)number; break;
            case 26U: item->ammo_max = (int16_t)number; break;
            case 29U: item->charges_current = (int16_t)number; break;
            case 30U: item->charges_max = (int16_t)number; break;
            case 31U: item->armor_base = (uint8_t)number; break;
            case 32U: item->armor_dex_cap = (int8_t)number; break;
            case 33U: item->shield_bonus = (uint8_t)number; break;
            }
            if(item->ammo_current > item->ammo_max) item->ammo_current = item->ammo_max;
            if(item->charges_current > item->charges_max)
                item->charges_current = item->charges_max;
        } else if(app->list_kind == PocketListParty) {
            PocketPartyMember* member = &app->data.party[app->record_index];
            if(app->number_index == 1U)
                member->initiative_modifier = (int8_t)number;
            else if(app->number_index == 2U)
                member->armor_class = (int16_t)number;
            else if(app->number_index == 3U)
                member->hp_current = (int16_t)number;
            else if(app->number_index == 4U)
                member->hp_max = (int16_t)number;
        }
        break;
    case PocketNumberDice:
        if(app->number_index == 0U) app->dice_count = (uint8_t)number;
        else if(app->number_index == 1U) app->dice_sides = pocket_nearest_die(number, false);
        else if(app->number_index == 2U) app->dice_modifier = (int16_t)number;
        app->roll_mode = PocketRollNormal;
        app->dice_roll_value_count = 0U;
        break;
    case PocketNumberCombat:
        if(app->number_index == 3U) character->hp_current = (int16_t)number;
        else if(app->number_index == 4U) character->hp_temporary = (int16_t)number;
        else if(app->number_index == 17U) character->death_successes = (uint8_t)number;
        else if(app->number_index == 18U) character->death_failures = (uint8_t)number;
        else if(app->number_index == 19U) character->exhaustion = (uint8_t)number;
        break;
    case PocketNumberInitiative:
        if(app->record_index < app->data.initiative.count) {
            PocketInitiativeEntry* entry = &app->data.initiative.entries[app->record_index];
            if(app->number_index == 1U)
                entry->initiative_total = (int16_t)number;
            else if(app->number_index == 2U)
                entry->initiative_modifier = (int8_t)number;
            else if(app->number_index == 3U)
                entry->armor_class = (int16_t)number;
            else if(app->number_index == 4U)
                entry->hp_current = (int16_t)number;
            else if(app->number_index == 5U)
                entry->hp_max = (int16_t)number;
            if(entry->is_player_character) {
                character->hp_current = entry->hp_current;
                character->hp_max = entry->hp_max;
                character->armor_class = entry->armor_class;
            }
        }
        break;
    case PocketNumberNone:
        break;
    }
    app->number_context = PocketNumberNone;
    app->input_module_active = 0U;
    pocket_save(app, false);
    view_dispatcher_switch_to_view(app->dispatcher, PocketViewMain);
    pocket_refresh(app);
}

static void pocket_begin_number(
    PocketD20App* app,
    PocketNumberContext context,
    uint8_t index,
    uint8_t aux,
    const char* header,
    int32_t value,
    int32_t minimum,
    int32_t maximum) {
    pocket_release_text_input(app);
    if(!app->number_input) {
        app->number_input = number_input_alloc();
        if(!app->number_input) {
            pocket_set_status(app, "Number input memory low");
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
        app->number_input,
        pocket_number_done,
        app,
        value,
        minimum,
        maximum);
    view_dispatcher_switch_to_view(app->dispatcher, PocketViewNumberInput);
}

static void pocket_draw_home(Canvas* canvas, PocketD20App* app) {
    char title[48];
    snprintf(
        title,
        sizeof(title),
        "D&D v" FAP_VERSION " - %.27s",
        app->data.character.name);
    pocket_draw_header(canvas, title, app->status);
    pocket_draw_menu_rows(
        canvas,
        app,
        pocket_home_items,
        sizeof(pocket_home_items) / sizeof(pocket_home_items[0]));
}

static void pocket_draw_profiles(Canvas* canvas, PocketD20App* app) {
    uint16_t count = pocket_profile_count(app);
    pocket_draw_header(canvas, "Characters - hold OK actions", app->status);
    for(uint8_t visible = 0U; visible < 5U; ++visible) {
        uint16_t index = app->scroll + visible;
        if(index > count) break;
        char row[48];
        if(index == count) {
            pocket_copy(row, sizeof(row), "+ New Character");
        } else {
            const PocketProfileEntry* entry = &app->profiles.entries[index];
            snprintf(
                row,
                sizeof(row),
                "%c #%lu L%u %.24s",
                entry->id == app->profiles.active_profile ? '*' : ' ',
                (unsigned long)entry->id,
                entry->level,
                entry->name[0] ? entry->name : "Unnamed");
        }
        pocket_draw_row(canvas, visible, index == app->selection, row);
    }
}

static void pocket_draw_profile_actions(Canvas* canvas, PocketD20App* app) {
    char title[48];
    snprintf(title, sizeof(title), "Character #%lu", (unsigned long)app->profile_action_id);
    pocket_draw_header(canvas, title, app->status);
    pocket_draw_menu_rows(
        canvas,
        app,
        pocket_profile_actions,
        sizeof(pocket_profile_actions) / sizeof(pocket_profile_actions[0]));
}

static void pocket_draw_character(Canvas* canvas, PocketD20App* app) {
    PocketCharacter* character = &app->data.character;
    char rows[12][48];
    const char* row_ptrs[12];
    for(uint8_t i = 0U; i < 12U; ++i) row_ptrs[i] = rows[i];

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
        pocket_d20_total_level(character),
        pocket_d20_proficiency_bonus(character));
    snprintf(rows[7], sizeof(rows[7]), "XP: %lu", (unsigned long)character->experience);
    snprintf(
        rows[8],
        sizeof(rows[8]),
        "Leveling: %s",
        character->milestone_leveling ? "Milestone" : "XP");
    snprintf(rows[9], sizeof(rows[9]), "Languages (%u)", character->language_count);
    snprintf(rows[10], sizeof(rows[10]), "Other proficiencies");
    snprintf(rows[11], sizeof(rows[11]), "Inspiration: %s", character->inspiration ? "Yes" : "No");
    pocket_draw_header(canvas, "Character", app->status);
    pocket_draw_menu_rows(canvas, app, row_ptrs, 12U);
}

static void pocket_draw_vitals(Canvas* canvas, PocketD20App* app) {
    PocketCharacter* character = &app->data.character;
    char rows[16][40];
    const char* row_ptrs[16];
    for(uint8_t i = 0U; i < 16U; ++i) row_ptrs[i] = rows[i];

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
            pocket_d20_effective_speed(character));
    else
        snprintf(rows[4], sizeof(rows[4]), "Speed: %d ft", character->speed);
    snprintf(
        rows[5],
        sizeof(rows[5]),
        "Initiative: %+d",
        pocket_d20_initiative_modifier(character));
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
        10 + pocket_d20_skill_base_modifier(character, 11U));
    snprintf(
        rows[14],
        sizeof(rows[14]),
        "Pass. Insight: %d",
        10 + pocket_d20_skill_base_modifier(character, 6U));
    snprintf(
        rows[15],
        sizeof(rows[15]),
        "Pass. Invest.: %d",
        10 + pocket_d20_skill_base_modifier(character, 8U));
    pocket_draw_header(canvas, "Vitals - hold OK: number", app->status);
    pocket_draw_menu_rows(canvas, app, row_ptrs, 16U);
}

static void pocket_draw_abilities(Canvas* canvas, PocketD20App* app) {
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
                pocket_d20_ability_names[i],
                character->saving_throw_misc[i],
                pocket_d20_saving_throw_modifier(character, i));
        } else {
            snprintf(
                rows[i],
                sizeof(rows[i]),
                "%s %d(%+d) %s Save %+d",
                pocket_d20_ability_names[i],
                character->ability_scores[i],
                pocket_d20_ability_modifier(character->ability_scores[i]),
                pocket_proficiency_mark(character->saving_throw_proficiency[i]),
                pocket_d20_saving_throw_modifier(character, i));
        }
    }
    pocket_draw_header(
        canvas,
        app->edit_modifier_mode ? "Saves: <> misc; hold OK #" : "Abilities: <> score; hold OK #",
        app->status);
    pocket_draw_menu_rows(canvas, app, row_ptrs, POCKET_D20_ABILITY_COUNT);
}

static void pocket_draw_skills(Canvas* canvas, PocketD20App* app) {
    PocketCharacter* character = &app->data.character;
    char title[32];
    snprintf(
        title,
        sizeof(title),
        "Skills PB+%u: <> %s",
        pocket_d20_proficiency_bonus(character),
        app->edit_modifier_mode ? "misc" : "prof");
    pocket_draw_header(canvas, title, app->status);
    for(uint8_t visible = 0U; visible < 5U; ++visible) {
        uint16_t display_index = app->scroll + visible;
        if(display_index >= POCKET_D20_SKILL_COUNT) break;
        uint8_t index = pocket_skill_display_order[display_index];
        uint8_t ability = pocket_d20_skill_abilities[index];
        char row[48];
        if(app->edit_modifier_mode) {
            snprintf(
                row,
                sizeof(row),
                "%s %s M%+d=%+d",
                pocket_d20_ability_names[ability],
                pocket_d20_skill_names[index],
                character->skill_misc[index],
                pocket_d20_skill_modifier(character, index));
        } else {
            snprintf(
                row,
                sizeof(row),
                "%s %s %s %+d",
                pocket_d20_ability_names[ability],
                pocket_d20_skill_names[index],
                pocket_proficiency_mark(character->skill_proficiency[index]),
                pocket_d20_skill_modifier(character, index));
        }
        pocket_draw_row(canvas, visible, display_index == app->selection, row);
    }
}

static __attribute__((unused)) void pocket_draw_builder(Canvas* canvas, PocketD20App* app) {
    const PocketCharacter* c = &app->data.character;
    char rows[11][48];
    const char* row_ptrs[11];
    for(uint8_t i = 0U; i < 11U; ++i) row_ptrs[i] = rows[i];
    snprintf(rows[0], sizeof(rows[0]), "Species: %.31s", c->species);
    snprintf(rows[1], sizeof(rows[1]), "Background: %.31s", c->background);
    snprintf(rows[2], sizeof(rows[2]), "Origin feat: %.31s", c->origin_feat);
    snprintf(rows[3], sizeof(rows[3]), "Tools: %.36s", c->tool_proficiencies);
    snprintf(rows[4], sizeof(rows[4]), "Armor: %.36s", c->armor_training);
    snprintf(rows[5], sizeof(rows[5]), "Weapons: %.34s", c->weapon_training);
    snprintf(rows[6], sizeof(rows[6]), "Size: %s", pocket_size_names[c->size]);
    snprintf(rows[7], sizeof(rows[7]), "Senses: %.35s", c->senses);
    uint8_t pending = 0U;
    for(uint8_t i = 0U; i < c->grant_count; ++i)
        if(c->grants[i].status == PocketGrantPending) ++pending;
    snprintf(rows[8], sizeof(rows[8]), "Review grants: %u pending", pending);
    snprintf(rows[9], sizeof(rows[9]), "Catalog diagnostics");
    snprintf(rows[10], sizeof(rows[10]), "Import validation: strict");
    pocket_draw_header(canvas, "Rules-aware Builder", app->status);
    pocket_draw_menu_rows(canvas, app, row_ptrs, 11U);
}

static void pocket_draw_grant_review(Canvas* canvas, PocketD20App* app) {
    const PocketCharacter* c = &app->data.character;
    pocket_draw_header(canvas, "Review grants before apply", app->status);
    for(uint8_t visible = 0U; visible < 5U; ++visible) {
        uint16_t row_index = app->scroll + visible;
        if(row_index > c->grant_count + 1U) break;
        char row[64];
        if(row_index == 0U) {
            pocket_copy(row, sizeof(row), "Apply all pending");
        } else if(row_index <= c->grant_count) {
            const PocketGrant* grant = &c->grants[row_index - 1U];
            char mark = grant->status == PocketGrantApplied ? 'A' :
                        grant->status == PocketGrantSkipped ? 'S' : '?';
            snprintf(
                row,
                sizeof(row),
                "%c %.28s: %.28s",
                mark,
                grant->option_name,
                grant->grant_value);
        } else {
            pocket_copy(row, sizeof(row), "+ Add Custom Grant");
        }
        pocket_draw_row(canvas, visible, row_index == app->selection, row);
    }
}

static void pocket_draw_grant_edit(Canvas* canvas, PocketD20App* app) {
    if(app->record_index >= app->data.character.grant_count) return;
    const PocketGrant* grant = &app->data.character.grants[app->record_index];
    char stable_id[48], source[48], source_type[32], option[48], prerequisites[48],
        class_name[40], level[24], payload[48], status[24];
    static const char* const sources[] = {
        "Species", "Background", "Feat", "Class Feature", "Subclass", "Item"};
    snprintf(stable_id, sizeof(stable_id), "Stable ID: %.36s", grant->stable_id);
    snprintf(source, sizeof(source), "Source: %.38s", grant->source);
    snprintf(source_type, sizeof(source_type), "Option type: %s", sources[grant->source_type]);
    snprintf(option, sizeof(option), "Option: %.38s", grant->option_name);
    snprintf(prerequisites, sizeof(prerequisites), "Requires: %.36s", grant->prerequisites);
    snprintf(class_name, sizeof(class_name), "Class: %s",
             grant->class_index < app->data.character.class_count ?
                 app->data.character.classes[grant->class_index].name : "General");
    snprintf(level, sizeof(level), "Gained level: %u", grant->level_gained);
    snprintf(payload, sizeof(payload), "Payload: %.37s", grant->grant_value);
    snprintf(status, sizeof(status), "Status: %s",
             grant->status == PocketGrantApplied ? "Applied" :
             grant->status == PocketGrantSkipped ? "Skipped" : "Pending");
    const char* rows[] = {stable_id, source, source_type, option, prerequisites, class_name,
                          level, payload, status, "Delete Grant"};
    pocket_draw_header(canvas, "Structured Grant Editor", app->status);
    pocket_draw_menu_rows(canvas, app, rows, 10U);
}

static __attribute__((unused)) void pocket_draw_catalog_diagnostics(Canvas* canvas, PocketD20App* app) {
    char rows[5][48];
    const char* ptrs[5] = {rows[0], rows[1], rows[2], rows[3], rows[4]};
    snprintf(rows[0], sizeof(rows[0]), "Catalog files: %u/%u", app->diagnostics_catalogs, PocketCatalogCount);
    snprintf(rows[1], sizeof(rows[1]), "Metadata records: %u", app->diagnostics_records);
    snprintf(rows[2], sizeof(rows[2]), "Invalid records: %u", app->diagnostics_invalid);
    snprintf(rows[3], sizeof(rows[3]), "Duplicate IDs: %u", app->diagnostics_duplicates);
    snprintf(rows[4], sizeof(rows[4]), "OK: scan again");
    pocket_draw_header(canvas, "Catalog Diagnostics", app->status);
    pocket_draw_menu_rows(canvas, app, ptrs, 5U);
}

static void pocket_draw_spell_filters(Canvas* canvas, PocketD20App* app) {
    const PocketCharacter* c = &app->data.character;
    char rows[6][48];
    const char* ptrs[6] = {rows[0], rows[1], rows[2], rows[3], rows[4], rows[5]};
    snprintf(rows[0], sizeof(rows[0]), "Level: %s", app->spell_filter_level < 0 ? "Any" :
        app->spell_filter_level == 0 ? "Cantrip" : "1-9 selected");
    snprintf(rows[1], sizeof(rows[1]), "Class: %s", app->spell_filter_class < c->class_count ?
        c->classes[app->spell_filter_class].name : "Current spell");
    snprintf(rows[2], sizeof(rows[2]), "Ritual: %s", app->spell_filter_ritual ? "Only" : "Any");
    snprintf(rows[3], sizeof(rows[3]), "School: %s", pocket_spell_school_names[app->spell_filter_school]);
    snprintf(rows[4], sizeof(rows[4]), "Source: %s", app->spell_filter_source == 1U ? "Core" :
        app->spell_filter_source == 2U ? "Add-on" : "Any");
    snprintf(rows[5], sizeof(rows[5]), "Status: %s", app->spell_filter_prepared == 1U ? "Prepared" :
        app->spell_filter_prepared == 2U ? "Known" :
        app->spell_filter_prepared == 3U ? "Always" : "Any");
    pocket_draw_header(canvas, "Spell Filters: <>", app->status);
    pocket_draw_menu_rows(canvas, app, ptrs, 6U);
}

static void pocket_draw_resources(Canvas* canvas, PocketD20App* app) {
    const PocketCharacter* c = &app->data.character;
    char rows[9][48];
    const char* ptrs[9];
    for(uint8_t i = 0U; i < 9U; ++i) ptrs[i] = rows[i];
    snprintf(rows[0], sizeof(rows[0]), "Carried: %d.%d lb", pocket_d20_carried_weight_tenths(c) / 10, abs(pocket_d20_carried_weight_tenths(c) % 10));
    snprintf(rows[1], sizeof(rows[1]), "Equipped: %d.%d lb", pocket_d20_equipped_weight_tenths(c) / 10, abs(pocket_d20_equipped_weight_tenths(c) % 10));
    snprintf(rows[2], sizeof(rows[2]), "Capacity: %d lb", pocket_d20_carrying_capacity(c));
    snprintf(rows[3], sizeof(rows[3]), "Encumbrance: %s", c->encumbrance_mode ? "Variant" : "Standard");
    snprintf(rows[4], sizeof(rows[4]), "Attuned: %u/3%s", pocket_d20_attuned_count(c), pocket_d20_attuned_count(c) > 3U ? " !" : "");
    snprintf(rows[5], sizeof(rows[5]), "Formula AC: %d", pocket_d20_calculated_armor_class(c));
    snprintf(rows[6], sizeof(rows[6]), "Apply armor/shield AC");
    snprintf(rows[7], sizeof(rows[7]), "Normalize coin values");
    snprintf(rows[8], sizeof(rows[8]), "Capacity override: %d", c->carrying_capacity_override);
    pocket_draw_header(canvas, "Inventory Resources", app->status);
    pocket_draw_menu_rows(canvas, app, ptrs, 9U);
}

static __attribute__((unused)) void pocket_draw_combat_sheet(Canvas* canvas, PocketD20App* app) {
    const PocketCharacter* c = &app->data.character;
    char rows[11][48];
    const char* ptrs[11];
    for(uint8_t i = 0U; i < 11U; ++i) ptrs[i] = rows[i];
    snprintf(rows[0], sizeof(rows[0]), "Conditions: %.32s", c->conditions);
    snprintf(
        rows[1],
        sizeof(rows[1]),
        "Concentration: %.31s",
        c->concentration[0] ? c->concentration : "None");
    snprintf(rows[2], sizeof(rows[2]), "Reaction: %s", c->reaction_available ? "Ready" : "Used");
    snprintf(rows[3], sizeof(rows[3]), "Temp effects: %.30s", c->temporary_effects);
    snprintf(rows[4], sizeof(rows[4]), "Resist: %.35s", c->resistances);
    snprintf(rows[5], sizeof(rows[5]), "Immune: %.35s", c->immunities);
    snprintf(rows[6], sizeof(rows[6]), "Vulnerable: %.31s", c->vulnerabilities);
    snprintf(rows[7], sizeof(rows[7]), "Senses: %.35s", c->senses);
    snprintf(rows[8], sizeof(rows[8]), "Movement: %.33s", c->movement_modes);
    snprintf(rows[9], sizeof(rows[9]), "Attack templates (%u)", c->attack_template_count);
    snprintf(rows[10], sizeof(rows[10]), "Initiative HP/AC + undo");
    pocket_draw_header(canvas, "Combat Sheet", app->status);
    pocket_draw_menu_rows(canvas, app, ptrs, 11U);
}

static void pocket_draw_attack_templates(Canvas* canvas, PocketD20App* app) {
    const PocketCharacter* c = &app->data.character;
    pocket_draw_header(canvas, "Attack Templates: OK roll", app->status);
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
        } else break;
        pocket_draw_row(canvas, visible, index == app->selection, row);
    }
}

static void pocket_draw_attack_template_edit(Canvas* canvas, PocketD20App* app) {
    if(app->record_index >= app->data.character.attack_template_count) return;
    const PocketAttackTemplate* attack =
        &app->data.character.attack_templates[app->record_index];
    char type[32], ability[32], save[32], attack_misc[24], dc[24], damage_dice[24],
        damage_die[24], rider_dice[24], rider_die[24], recharge[32];
    snprintf(type, sizeof(type), "Type: %s", pocket_attack_template_type_names[attack->type]);
    snprintf(ability, sizeof(ability), "Ability: %s", pocket_d20_ability_names[attack->ability]);
    snprintf(save, sizeof(save), "Save: %s", pocket_d20_ability_names[attack->save_ability]);
    snprintf(attack_misc, sizeof(attack_misc), "Attack misc: %+d", attack->attack_misc);
    snprintf(dc, sizeof(dc), "Save DC: %u", attack->save_dc);
    snprintf(damage_dice, sizeof(damage_dice), "Damage dice: %u", attack->damage_dice);
    snprintf(damage_die, sizeof(damage_die), "Damage die: d%u", attack->damage_die);
    snprintf(rider_dice, sizeof(rider_dice), "Rider dice: %u", attack->rider_dice);
    snprintf(rider_die, sizeof(rider_die), "Rider die: d%u", attack->rider_die);
    snprintf(recharge, sizeof(recharge), "Recharge: %s", pocket_recharge_names[attack->recharge]);
    const char* rows[] = {attack->name, type, ability, save, attack_misc, dc, damage_dice,
        damage_die, attack->damage_type, attack->mastery, rider_dice, rider_die,
        attack->rider_type, recharge, "Delete Template"};
    pocket_draw_header(canvas, "Attack Template Editor", app->status);
    pocket_draw_menu_rows(canvas, app, rows, 15U);
}

static void pocket_draw_magic(Canvas* canvas, PocketD20App* app) {
    PocketCharacter* character = &app->data.character;
    char rows[17][48];
    const char* row_ptrs[17];
    for(uint8_t i = 0U; i < 17U; ++i) row_ptrs[i] = rows[i];
    snprintf(rows[0], sizeof(rows[0]), "Spells (%u) / hold: filters", character->spell_count);
    snprintf(
        rows[1],
        sizeof(rows[1]),
        "Casting ability: %s",
        pocket_d20_ability_names[character->spellcasting_ability]);
    snprintf(
        rows[2],
        sizeof(rows[2]),
        "PB +%u Atk %+d DC %d (hold recalc)",
        pocket_d20_proficiency_bonus(character),
        pocket_d20_spell_attack_modifier(character),
        pocket_d20_spell_save_dc(character));
    snprintf(
        rows[3],
        sizeof(rows[3]),
        "Spell attack misc: %+d",
        character->spell_attack_misc);
    snprintf(
        rows[4],
        sizeof(rows[4]),
        "Spell save misc: %+d",
        character->spell_save_misc);
    snprintf(
        rows[5],
        sizeof(rows[5]),
        "Edit slots: %s",
        app->edit_slot_max ? "Maximum" : "Current");
    uint8_t wizard_level = pocket_wizard_level(character);
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
    snprintf(rows[16], sizeof(rows[16]), "Back to Main Menu");
    pocket_draw_header(
        canvas,
        app->arcane_recovery_active ? "Magic: Arcane Recovery" : "Magic",
        app->status);
    pocket_draw_menu_rows(canvas, app, row_ptrs, 17U);
}

static void pocket_draw_currency(Canvas* canvas, PocketD20App* app) {
    const PocketCharacter* character = &app->data.character;
    char rows[5][40];
    const char* row_ptrs[5];
    for(uint8_t i = 0U; i < 5U; ++i) row_ptrs[i] = rows[i];
    snprintf(rows[0], sizeof(rows[0]), "Copper (CP): %ld", (long)character->currency_cp);
    snprintf(rows[1], sizeof(rows[1]), "Silver (SP): %ld", (long)character->currency_sp);
    snprintf(rows[2], sizeof(rows[2]), "Electrum (EP): %ld", (long)character->currency_ep);
    snprintf(rows[3], sizeof(rows[3]), "Gold (GP): %ld", (long)character->currency_gp);
    snprintf(rows[4], sizeof(rows[4]), "Platinum (PP): %ld", (long)character->currency_pp);
    pocket_draw_header(canvas, "Currency - OK: number input", app->status);
    pocket_draw_menu_rows(canvas, app, row_ptrs, 5U);
}

static void pocket_draw_catalog(Canvas* canvas, PocketD20App* app) {
    char page[24];
    uint16_t page_number = app->catalog_page_start / pocket_catalog_page_limit(app) + 1U;
    snprintf(
        page,
        sizeof(page),
        "Page %u%s <>",
        page_number,
        app->catalog_has_more ? "+" : "");
    pocket_draw_header(canvas, pocket_catalog_title(app), app->status[0] ? app->status : page);
    if(app->catalog_count == 0U) {
        pocket_draw_row(
            canvas,
            0U,
            false,
            app->catalog_kind == PocketCatalogSpells ? "No allowed spells (hold OK)" :
                                                       "Catalog is empty");
        return;
    }
    for(uint8_t visible = 0U; visible < 5U; ++visible) {
        uint16_t index = app->scroll + visible;
        if(index >= app->catalog_count) break;
        char row[48];
        if(app->catalog_kind == PocketCatalogSpells && app->catalog_has_metadata[index])
            snprintf(
                row,
                sizeof(row),
                "L%u %.31s",
                app->catalog_levels[index],
                app->catalog_entries[index]);
        else if(app->catalog_kind == PocketCatalogItems &&
                app->catalog_item_categories[index] != PocketItemCategoryOther)
            snprintf(
                row,
                sizeof(row),
                "%s%s %.31s",
                pocket_item_category_mark(app->catalog_item_categories[index]),
                app->catalog_item_magic[index] ? "*" : " ",
                app->catalog_entries[index]);
        else
            pocket_copy(row, sizeof(row), app->catalog_entries[index]);
        pocket_draw_row(
            canvas,
            visible,
            index == app->selection,
            row);
    }
}

static void pocket_draw_record_list(Canvas* canvas, PocketD20App* app) {
    uint8_t count = pocket_list_count(app);
    pocket_draw_header(canvas, pocket_list_title(app->list_kind), app->status);
    for(uint8_t visible = 0U; visible < 5U; ++visible) {
        uint16_t index = app->scroll + visible;
        if(index >= (uint16_t)count + 1U) break;
        char row[64];
        if(index == 0U) {
            snprintf(row, sizeof(row), "+ Add New");
        } else {
            pocket_format_list_entry(app, (uint8_t)(index - 1U), row, sizeof(row));
        }
        pocket_draw_row(canvas, visible, index == app->selection, row);
    }
}

static void pocket_format_record_detail(
    const PocketD20App* app,
    uint8_t field,
    char* output,
    size_t size) {
    const PocketCharacter* character = &app->data.character;
    uint8_t index = app->record_index;
    switch(app->list_kind) {
    case PocketListClasses: {
        const PocketClassLevel* class_level = &character->classes[index];
        if(field == 0U) pocket_format_labeled_text(output, size, "Name: ", class_level->name);
        else if(field == 1U)
            pocket_format_labeled_text(output, size, "Subclass: ", class_level->subclass);
        else if(field == 2U) snprintf(output, size, "Class level: %u", class_level->level);
        else if(field == 3U) snprintf(output, size, "Hit Point Die: d%u", class_level->hit_die);
        else if(field == 4U) snprintf(output, size, "Class Hit Dice: %u/%u", class_level->hit_dice_current, class_level->hit_dice_max);
        else if(field == 5U) snprintf(output, size, "Class Hit Dice max: %u", class_level->hit_dice_max);
        else if(field == 6U)
            pocket_format_labeled_text(
                output,
                size,
                "Casting mode: ",
                pocket_spellcasting_mode_names[class_level->spellcasting_mode]);
        else if(field == 7U)
            pocket_format_labeled_text(
                output,
                size,
                "Casting ability: ",
                pocket_d20_ability_names[class_level->spellcasting_ability]);
        else if(field == 8U) snprintf(output, size, "Cantrip limit: %u", class_level->cantrip_limit);
        else if(field == 9U) snprintf(output, size, "Prepared %u/%u", pocket_d20_class_prepared_count(character, index), class_level->prepared_limit);
        else if(field == 10U) snprintf(output, size, "Spellbook size: %u", class_level->spellbook_size);
        else if(field == 11U) snprintf(output, size, "Pact slot level: %u", class_level->pact_slot_level);
        else if(field == 12U) snprintf(output, size, "Pact slots: %u/%u", class_level->pact_slots_current, class_level->pact_slots_max);
        else if(field == 13U) snprintf(output, size, "Pact slots max: %u", class_level->pact_slots_max);
        else if(field == 14U) snprintf(output, size, "Mystic Arcanum: 0x%X", class_level->mystic_arcanum_mask);
        else if(field == 15U) snprintf(output, size, "Spell points: %u/%u", class_level->spell_points_current, class_level->spell_points_max);
        else if(field == 16U) snprintf(output, size, "Spell points max: %u", class_level->spell_points_max);
        else snprintf(output, size, "Delete class");
        break;
    }
    case PocketListSpells: {
        const PocketSpell* spell = &character->spells[index];
        if(field == 0U) pocket_format_labeled_text(output, size, "Name: ", spell->name);
        else if(field == 1U) pocket_format_labeled_text(output, size, "Notes: ", spell->detail);
        else if(field == 2U)
            pocket_format_labeled_text(
                output,
                size,
                "Source class: ",
                spell->class_index < character->class_count ?
                    character->classes[spell->class_index].name :
                    "Primary");
        else if(field == 3U) snprintf(output, size, "Level: %u", spell->level);
        else if(field == 4U)
            snprintf(output, size, "Known: %s", character->spell_known[index] ? "Yes" : "No");
        else if(field == 5U)
            snprintf(output, size, "Prepared: %s", spell->prepared ? "Yes" : "No");
        else if(field == 6U)
            snprintf(
                output,
                size,
                "Always prepared: %s",
                character->spell_always_prepared[index] ? "Yes" : "No");
        else if(field == 7U) snprintf(output, size, "Ritual: %s", spell->ritual ? "Yes" : "No");
        else if(field == 8U)
            snprintf(
                output,
                size,
                "Free casts: %u/%u",
                character->spell_free_casts_current[index],
                character->spell_free_casts_max[index]);
        else if(field == 9U)
            snprintf(
                output,
                size,
                "Free casts max: %u",
                character->spell_free_casts_max[index]);
        else if(field == 10U)
            pocket_copy(
                output,
                size,
                character->spell_free_casts_current[index] ? "Use one free cast" :
                                                               "No free casts left");
        else if(field == 11U)
            pocket_format_labeled_text(output, size, "Stable ID: ", spell->stable_id);
        else if(field == 12U)
            pocket_format_labeled_text(output, size, "Source: ", spell->source);
        else if(field == 13U)
            pocket_format_labeled_text(output, size, "School: ", spell->school);
        else if(field == 14U)
            pocket_format_labeled_text(output, size, "Grant source: ", spell->grant_name);
        else if(field == 15U) snprintf(output, size, "Grant type: %u", spell->grant_source);
        else
            snprintf(output, size, "Delete spell");
        break;
    }
    case PocketListFeatures: {
        const PocketFeature* feature = &character->features[index];
        const char* class_name = feature->class_index < character->class_count ?
                                     character->classes[feature->class_index].name :
                                     "General";
        if(field == 0U) pocket_format_labeled_text(output, size, "Name: ", feature->name);
        else if(field == 1U)
            pocket_format_labeled_text(output, size, "Notes: ", feature->detail);
        else if(field == 2U)
            pocket_format_labeled_text(output, size, "Source class: ", class_name);
        else if(field == 3U)
            snprintf(output, size, "Gained at class L%u", feature->class_level_gained);
        else if(field == 4U)
            snprintf(output, size, "Uses: %d/%d", feature->uses_current, feature->uses_max);
        else if(field == 5U) snprintf(output, size, "Maximum uses: %d", feature->uses_max);
        else if(field == 6U)
            snprintf(output, size, "Recharge: %s", pocket_recharge_names[feature->recharge]);
        else if(field == 7U)
            snprintf(output, size, "Resource formula: %s", pocket_resource_formula_names[feature->resource_formula]);
        else if(field == 8U)
            snprintf(output, size, "Resource ability: %s", pocket_d20_ability_names[feature->resource_ability]);
        else snprintf(output, size, "Delete feature");
        break;
    }
    case PocketListItems: {
        const PocketItem* item = &character->items[index];
        switch(field) {
        case 0:
            pocket_format_labeled_text(output, size, "Name: ", item->name);
            break;
        case 1:
            pocket_format_labeled_text(output, size, "Notes: ", item->detail);
            break;
        case 2:
            snprintf(output, size, "Quantity: %d", item->quantity);
            break;
        case 3:
            snprintf(output, size, "Weight: %d.%d lb", item->weight_tenths / 10, abs(item->weight_tenths % 10));
            break;
        case 4:
            snprintf(output, size, "Equipped: %s", item->equipped ? "Yes" : "No");
            break;
        case 5:
            snprintf(output, size, "Attuned: %s", item->attuned ? "Yes" : "No");
            break;
        case 6:
            snprintf(output, size, "Weapon: %s", item->is_weapon ? "Yes" : "No");
            break;
        case 7:
            snprintf(output, size, "Attack ability: %s", pocket_attack_ability_names[item->attack_ability]);
            break;
        case 8:
            snprintf(output, size, "Proficient: %s", item->proficient ? "Yes" : "No");
            break;
        case 9:
            snprintf(output, size, "Magic bonus: %+d", item->magic_bonus);
            break;
        case 10:
            snprintf(output, size, "Damage dice: %u", item->damage_dice);
            break;
        case 11:
            snprintf(output, size, "Damage die: d%u", item->damage_die);
            break;
        case 12:
            snprintf(output, size, "Versatile: %s", item->versatile_die ? "Yes" : "No");
            break;
        case 13:
            snprintf(output, size, "Versatile die: d%u", item->versatile_die);
            break;
        case 14:
            snprintf(output, size, "Use versatile: %s", item->use_versatile ? "Yes" : "No");
            break;
        case 15:
            snprintf(output, size, "Type: %s", pocket_d20_damage_names[item->damage_type]);
            break;
        case 16:
            snprintf(output, size, "Finesse: %s", (item->weapon_properties & PocketWeaponFinesse) ? "Yes" : "No");
            break;
        case 17:
            snprintf(output, size, "Ranged: %s", (item->weapon_properties & PocketWeaponRanged) ? "Yes" : "No");
            break;
        case 18:
            snprintf(output, size, "Light: %s", (item->weapon_properties & PocketWeaponLight) ? "Yes" : "No");
            break;
        case 19:
            snprintf(output, size, "Heavy: %s", (item->weapon_properties & PocketWeaponHeavy) ? "Yes" : "No");
            break;
        case 20:
            snprintf(output, size, "Thrown: %s", (item->weapon_properties & PocketWeaponThrown) ? "Yes" : "No");
            break;
        case 21:
            snprintf(output, size, "Ammunition: %s", (item->weapon_properties & PocketWeaponAmmunition) ? "Yes" : "No");
            break;
        case 22:
            snprintf(output, size, "Add ability dmg: %s", item->add_ability_damage ? "Yes" : "No");
            break;
        case 23:
            snprintf(output, size, "Extra dice: %u", item->extra_dice);
            break;
        case 24:
            snprintf(output, size, "Extra die: d%u", item->extra_die);
            break;
        case 25:
            snprintf(output, size, "Ammo: %d/%d", item->ammo_current, item->ammo_max);
            break;
        case 26:
            snprintf(output, size, "Maximum ammo: %d", item->ammo_max);
            break;
        case 27:
            snprintf(
                output,
                size,
                "Attack %+d / %ud%u",
                pocket_d20_weapon_attack_modifier(character, item),
                item->damage_dice,
                item->use_versatile ? item->versatile_die : item->damage_die);
            break;
        case 28:
            snprintf(output, size, "Container: %s", item->container_index < 0 ? "Carried" : "Inside item");
            break;
        case 29:
            snprintf(output, size, "Charges: %d/%d", item->charges_current, item->charges_max);
            break;
        case 30:
            snprintf(output, size, "Charges max: %d", item->charges_max);
            break;
        case 31:
            snprintf(output, size, "Armor base AC: %u", item->armor_base);
            break;
        case 32:
            snprintf(output, size, "Armor DEX cap: %d", item->armor_dex_cap);
            break;
        case 33:
            snprintf(output, size, "Shield AC bonus: %u", item->shield_bonus);
            break;
        case 34:
            pocket_format_labeled_text(output, size, "Ammo group: ", item->ammunition_group);
            break;
        default:
            snprintf(output, size, "Delete item");
            break;
        }
        break;
    }
    case PocketListLanguages:
        if(field == 0U)
            pocket_format_labeled_text(output, size, "Language: ", character->languages[index]);
        else snprintf(output, size, "Delete language");
        break;
    case PocketListJournal: {
        const PocketJournalEntry* entry = &character->journal[index];
        const char* class_name = entry->class_index < character->class_count ?
                                     character->classes[entry->class_index].name :
                                     "Primary";
        if(field == 0U)
            pocket_format_labeled_text(
                output,
                size,
                "Category: ",
                pocket_d20_journal_category_names[entry->category]);
        else if(field == 1U)
            pocket_format_labeled_text(output, size, "Title: ", entry->title);
        else if(field == 2U)
            pocket_format_labeled_text(output, size, "Body: ", entry->body);
        else if(field == 3U) snprintf(output, size, "Complete: %s", entry->completed ? "Yes" : "No");
        else if(field == 4U)
            pocket_format_labeled_text(output, size, "Level class: ", class_name);
        else if(field == 5U)
            pocket_copy(
                output,
                size,
                entry->level_granted ? "Level already applied" : "Apply milestone level");
        else if(field == 6U) snprintf(output, size, "Create inventory item");
        else snprintf(output, size, "Delete journal entry");
        break;
    }
    case PocketListParty: {
        const PocketPartyMember* member = &app->data.party[index];
        if(field == 0U) pocket_format_labeled_text(output, size, "Name: ", member->name);
        else if(field == 1U) snprintf(output, size, "Initiative mod: %+d", member->initiative_modifier);
        else if(field == 2U) snprintf(output, size, "Armor Class: %d", member->armor_class);
        else if(field == 3U) snprintf(output, size, "Current HP: %d", member->hp_current);
        else if(field == 4U) snprintf(output, size, "Maximum HP: %d", member->hp_max);
        else snprintf(output, size, "Delete party member");
        break;
    }
    }
}

static void pocket_draw_record_detail(Canvas* canvas, PocketD20App* app) {
    uint8_t count = pocket_record_detail_count(app);
    pocket_draw_header(canvas, pocket_list_title(app->list_kind), app->status);
    for(uint8_t visible = 0U; visible < 5U; ++visible) {
        uint16_t field = app->scroll + visible;
        if(field >= count) break;
        char row[POCKET_D20_DETAIL_LEN + 32U];
        pocket_format_record_detail(app, (uint8_t)field, row, sizeof(row));
        pocket_draw_row(canvas, visible, field == app->selection, row);
    }
}

static void pocket_draw_combat(Canvas* canvas, PocketD20App* app) {
    PocketCharacter* character = &app->data.character;
    char rows[20][48];
    const char* row_ptrs[20];
    for(uint8_t i = 0U; i < 20U; ++i) row_ptrs[i] = rows[i];
    snprintf(rows[0], sizeof(rows[0]), "Weapon Attacks");
    snprintf(rows[1], sizeof(rows[1]), "Attack Templates (%u)", character->attack_template_count);
    snprintf(rows[2], sizeof(rows[2]), "Initiative Tracker");
    snprintf(rows[3], sizeof(rows[3]), "HP: %d/%d", character->hp_current, character->hp_max);
    snprintf(rows[4], sizeof(rows[4]), "Temporary HP: %d", character->hp_temporary);
    snprintf(rows[5], sizeof(rows[5]), "Short Rest");
    snprintf(
        rows[6],
        sizeof(rows[6]),
        "Spend %.22s d%u: %u/%u",
        character->classes[app->hit_die_class_index].name,
        character->classes[app->hit_die_class_index].hit_die,
        character->classes[app->hit_die_class_index].hit_dice_current,
        character->classes[app->hit_die_class_index].hit_dice_max);
    snprintf(rows[7], sizeof(rows[7]), "Long Rest");
    snprintf(rows[8], sizeof(rows[8]), "Conditions: %.32s", character->conditions);
    snprintf(
        rows[9],
        sizeof(rows[9]),
        "Concentration: %.31s",
        character->concentration[0] ? character->concentration : "None");
    snprintf(rows[10], sizeof(rows[10]), "Reaction: %s", character->reaction_available ? "Ready" : "Used");
    snprintf(rows[11], sizeof(rows[11]), "Temp effects: %.30s", character->temporary_effects);
    snprintf(rows[12], sizeof(rows[12]), "Resist: %.35s", character->resistances);
    snprintf(rows[13], sizeof(rows[13]), "Immune: %.35s", character->immunities);
    snprintf(rows[14], sizeof(rows[14]), "Vulnerable: %.31s", character->vulnerabilities);
    snprintf(rows[15], sizeof(rows[15]), "Senses: %.35s", character->senses);
    snprintf(rows[16], sizeof(rows[16]), "Movement: %.33s", character->movement_modes);
    snprintf(rows[17], sizeof(rows[17]), "Death success: %u", character->death_successes);
    snprintf(rows[18], sizeof(rows[18]), "Death failure: %u", character->death_failures);
    snprintf(rows[19], sizeof(rows[19]), "Exhaustion: %u", character->exhaustion);
    pocket_draw_header(canvas, "Combat", app->status);
    pocket_draw_menu_rows(canvas, app, row_ptrs, 20U);
}

static void pocket_draw_dice(Canvas* canvas, PocketD20App* app) {
    char rows[5][48];
    const char* row_ptrs[5];
    for(uint8_t i = 0U; i < 5U; ++i) row_ptrs[i] = rows[i];
    snprintf(rows[0], sizeof(rows[0]), "Dice count: %u", app->dice_count);
    snprintf(rows[1], sizeof(rows[1]), "Die: d%u", app->dice_sides);
    snprintf(rows[2], sizeof(rows[2]), "Modifier: %+d", app->dice_modifier);
    snprintf(rows[3], sizeof(rows[3]), "Mode: %s", pocket_roll_mode_names[app->roll_mode]);
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
    pocket_draw_header(canvas, "Dice Roller", app->status);
    pocket_draw_menu_rows(canvas, app, row_ptrs, 5U);
}

static void pocket_format_roll_row(
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

static void pocket_draw_dice_result(Canvas* canvas, PocketD20App* app) {
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
        uint8_t chosen = app->roll_mode == PocketRollAdvantage ?
                             (app->dice_first > app->dice_second ? app->dice_first :
                                                                   app->dice_second) :
                             (app->dice_first < app->dice_second ? app->dice_first :
                                                                   app->dice_second);
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
            pocket_format_roll_row(
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
    pocket_draw_header(canvas, title, app->status);
    for(uint8_t row = 0U; row < 5U; ++row) {
        if(rows[row][0]) pocket_draw_row(canvas, row, false, row_ptrs[row]);
    }
}

static void pocket_draw_attack_list(Canvas* canvas, PocketD20App* app) {
    uint8_t count = pocket_weapon_count(app);
    char title[32];
    snprintf(title, sizeof(title), "Attacks: %s", pocket_roll_mode_names[app->roll_mode]);
    pocket_draw_header(canvas, title, app->status);
    if(count == 0U) {
        pocket_draw_row(canvas, 0U, false, "No weapon items");
        pocket_draw_row(canvas, 1U, false, "Add one in Inventory");
        return;
    }
    for(uint8_t visible = 0U; visible < 5U; ++visible) {
        uint16_t weapon_number = app->scroll + visible;
        if(weapon_number >= count) break;
        uint8_t item_index = pocket_weapon_index(app, weapon_number);
        const PocketItem* item = &app->data.character.items[item_index];
        char row[64];
        snprintf(
            row,
            sizeof(row),
            "%s %+d %ud%u",
            item->name,
            pocket_d20_weapon_attack_modifier(&app->data.character, item),
            item->damage_dice,
            item->use_versatile ? item->versatile_die : item->damage_die);
        pocket_draw_row(canvas, visible, weapon_number == app->selection, row);
    }
}

static void pocket_draw_attack_result(Canvas* canvas, PocketD20App* app) {
    const PocketItem* item = &app->data.character.items[app->attack_item_index];
    pocket_draw_header(canvas, item->name, app->status);
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
        pocket_draw_row(canvas, 0U, false, row);
        uint8_t detail_row = 1U;
        if(app->attack_roll.second_die) {
            snprintf(
                row,
                sizeof(row),
                "Dice sum: %u",
                app->attack_roll.first_die + app->attack_roll.second_die);
            pocket_draw_row(canvas, detail_row++, false, row);
        }
        snprintf(row, sizeof(row), "Modifier: %+d", app->attack_roll.modifier);
        pocket_draw_row(canvas, detail_row++, false, row);
        snprintf(row, sizeof(row), "Attack total: %d", app->attack_roll.total);
        pocket_draw_row(canvas, detail_row++, false, row);
        if(app->attack_roll.critical)
            snprintf(row, sizeof(row), "Critical! OK damage");
        else if(app->attack_roll.automatic_miss)
            snprintf(row, sizeof(row), "Natural 1 - miss");
        else
            snprintf(row, sizeof(row), "OK damage; Right crit");
        pocket_draw_row(canvas, detail_row, false, row);
        if(detail_row < 4U) pocket_draw_row(canvas, 4U, false, "Up: reroll attack");
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
            pocket_draw_header(canvas, damage_title, app->status);
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
                pocket_draw_row(canvas, display_row, false, values);
            }
            snprintf(
                row,
                sizeof(row),
                "Sum %d %+d = %d",
                app->damage_roll.weapon_total + app->damage_roll.extra_total,
                app->damage_roll.modifier,
                app->damage_roll.total);
            pocket_draw_row(canvas, 4U, false, row);
        } else {
            snprintf(
                row,
                sizeof(row),
                "%s damage",
                app->damage_roll.critical ? "Critical" : "Normal");
            pocket_draw_row(canvas, 0U, false, row);
            snprintf(row, sizeof(row), "Weapon dice: %d", app->damage_roll.weapon_total);
            pocket_draw_row(canvas, 1U, false, row);
            snprintf(row, sizeof(row), "Extra dice: %d", app->damage_roll.extra_total);
            pocket_draw_row(canvas, 2U, false, row);
            snprintf(row, sizeof(row), "Modifier: %+d", app->damage_roll.modifier);
            pocket_draw_row(canvas, 3U, false, row);
            snprintf(row, sizeof(row), "Total: %d (OK reroll)", app->damage_roll.total);
            pocket_draw_row(canvas, 4U, false, row);
        }
    }
}

static void pocket_draw_initiative_menu(Canvas* canvas, PocketD20App* app) {
    char rows[6][48];
    const char* row_ptrs[6];
    for(uint8_t i = 0U; i < 6U; ++i) row_ptrs[i] = rows[i];
    snprintf(rows[0], sizeof(rows[0]), "Start New Combat");
    snprintf(
        rows[1],
        sizeof(rows[1]),
        "Resume Combat%s",
        app->data.initiative.active ? "" : " (none)");
    snprintf(rows[2], sizeof(rows[2]), "Party Roster (%u)", app->data.party_count);
    snprintf(rows[3], sizeof(rows[3]), "Edit Current Order");
    snprintf(rows[4], sizeof(rows[4]), "End Current Combat");
    snprintf(rows[5], sizeof(rows[5]), "Undo last change (%u)", app->data.encounter_history_count);
    pocket_draw_header(canvas, "Initiative", app->status);
    pocket_draw_menu_rows(canvas, app, row_ptrs, 6U);
}

static void pocket_draw_initiative_setup(Canvas* canvas, PocketD20App* app) {
    PocketInitiativeState* initiative = &app->data.initiative;
    pocket_draw_header(canvas, "Set Initiative: <> / hold OK", app->status);
    uint16_t count = initiative->count + 2U;
    for(uint8_t visible = 0U; visible < 5U; ++visible) {
        uint16_t index = app->scroll + visible;
        if(index >= count) break;
        char row[48];
        if(index < initiative->count) {
            PocketInitiativeEntry* entry = &initiative->entries[index];
            snprintf(
                row,
                sizeof(row),
                "%.16s: %d HP%d AC%d",
                entry->name,
                entry->initiative_total,
                entry->hp_current,
                entry->armor_class);
        } else if(index == initiative->count) {
            snprintf(row, sizeof(row), "+ Temporary participant");
        } else {
            snprintf(row, sizeof(row), "Begin Combat");
        }
        pocket_draw_row(canvas, visible, index == app->selection, row);
    }
}

static void pocket_draw_initiative_combat(Canvas* canvas, PocketD20App* app) {
    PocketInitiativeState* initiative = &app->data.initiative;
    char title[32];
    snprintf(title, sizeof(title), "Round %u - OK Next", initiative->round);
    pocket_draw_header(canvas, title, app->status);
    for(uint8_t visible = 0U; visible < 5U; ++visible) {
        uint16_t index = app->scroll + visible;
        if(index >= initiative->count) break;
        PocketInitiativeEntry* entry = &initiative->entries[index];
        char row[48];
        snprintf(
            row,
            sizeof(row),
            "%c %.10s I%d HP%d AC%d %.5s",
            index == initiative->current_turn ? '>' : ' ',
            entry->name,
            entry->initiative_total,
            entry->hp_current,
            entry->armor_class,
            entry->conditions);
        pocket_draw_row(canvas, visible, index == app->selection, row);
    }
}

static void pocket_draw_initiative_edit(Canvas* canvas, PocketD20App* app) {
    pocket_draw_header(canvas, "Edit Participant", app->status);
    if(app->record_index >= app->data.initiative.count) return;
    const PocketInitiativeEntry* entry = &app->data.initiative.entries[app->record_index];
    char rows[8][48];
    const char* row_ptrs[8];
    for(uint8_t i = 0U; i < 8U; ++i) row_ptrs[i] = rows[i];
    snprintf(rows[0], sizeof(rows[0]), "Name: %.23s", entry->name);
    snprintf(rows[1], sizeof(rows[1]), "Initiative roll: %d", entry->initiative_total);
    snprintf(rows[2], sizeof(rows[2]), "Modifier: %+d", entry->initiative_modifier);
    snprintf(rows[3], sizeof(rows[3]), "Armor Class: %d", entry->armor_class);
    snprintf(rows[4], sizeof(rows[4]), "Current HP: %d", entry->hp_current);
    snprintf(rows[5], sizeof(rows[5]), "Maximum HP: %d", entry->hp_max);
    snprintf(rows[6], sizeof(rows[6]), "Conditions: %.23s", entry->conditions);
    snprintf(
        rows[7],
        sizeof(rows[7]),
        "%s",
        app->initiative_delete_armed ? "OK again: confirm delete" : "Delete from combat");
    pocket_draw_menu_rows(canvas, app, row_ptrs, 8U);
}

#if 0 /* v2.6: legacy embedded bestiary removed from the character FAP. */
static void pocket_monsters_release(PocketD20App* app) {
    free(app->monster_detail);
    free(app->monster_encounter);
    app->monster_detail = NULL;
    app->monster_encounter = NULL;
}

static bool pocket_monster_matches(const PocketD20App* app, const PocketMonsterSummary* monster) {
    if(app->monster_max_cr_eighths && monster->cr_eighths > app->monster_max_cr_eighths)
        return false;
    if(app->monster_type_filter &&
       strcmp(monster->type, pocket_monster_type_names[app->monster_type_filter]))
        return false;
    if(app->monster_source_filter &&
       strcmp(monster->source, pocket_monster_source_names[app->monster_source_filter]))
        return false;
    if(app->monster_environment_filter &&
       strcmp(monster->environment,
              pocket_monster_environment_names[app->monster_environment_filter]))
        return false;
    if(app->monster_search[0]) {
        const char* name = monster->name;
        const char* query = app->monster_search;
        bool found = false;
        for(size_t start = 0U; name[start] && !found; ++start) {
            size_t offset = 0U;
            while(query[offset] && name[start + offset]) {
                char left = name[start + offset], right = query[offset];
                if(left >= 'A' && left <= 'Z') left = (char)(left + ('a' - 'A'));
                if(right >= 'A' && right <= 'Z') right = (char)(right + ('a' - 'A'));
                if(left != right) break;
                ++offset;
            }
            found = query[offset] == '\0';
        }
        if(!found) return false;
    }
    return true;
}

static uint16_t pocket_monster_filtered_count(PocketD20App* app) {
    uint16_t total = pocket_monster_count(app->storage), count = 0U;
    PocketMonsterSummary monster;
    for(uint16_t i = 0U; i < total; ++i)
        if(pocket_monster_at(app->storage, i, &monster) && pocket_monster_matches(app, &monster))
            ++count;
    return count;
}

static bool pocket_monster_filtered_at(
    PocketD20App* app,
    uint16_t wanted,
    PocketMonsterSummary* output) {
    uint16_t total = pocket_monster_count(app->storage), found = 0U;
    for(uint16_t i = 0U; i < total; ++i) {
        if(!pocket_monster_at(app->storage, i, output) || !pocket_monster_matches(app, output))
            continue;
        if(found++ == wanted) return true;
    }
    return false;
}

static void pocket_monster_cr(char* output, size_t size, uint8_t eighths) {
    if(eighths == 1U) snprintf(output, size, "1/8");
    else if(eighths == 2U) snprintf(output, size, "1/4");
    else if(eighths == 4U) snprintf(output, size, "1/2");
    else snprintf(output, size, "%u", eighths / 8U);
}

static __attribute__((unused)) void pocket_draw_monsters(Canvas* canvas, PocketD20App* app) {
    char level[32], size[32], difficulty[32], count[32], search[32], cr_filter[32], type_filter[32],
        source_filter[32], browse_environment[32], environment[32], repeats[32], template_name[32],
        role[32];
    static const char* names[] = {"Low", "Moderate", "High"};
    snprintf(count, sizeof(count), "Browse Stat Blocks (%u)", app->monster_count);
    snprintf(level, sizeof(level), "Party Level: %u", app->encounter_party_level);
    snprintf(size, sizeof(size), "Party Size: %u", app->encounter_party_size);
    snprintf(difficulty, sizeof(difficulty), "Difficulty: %s", names[app->encounter_difficulty]);
    if(app->monster_max_cr_eighths) {
        char cr[8]; pocket_monster_cr(cr, sizeof(cr), app->monster_max_cr_eighths);
        snprintf(cr_filter, sizeof(cr_filter), "Max CR: %s", cr);
    } else snprintf(cr_filter, sizeof(cr_filter), "Max CR: Any");
    snprintf(type_filter, sizeof(type_filter), "Type: %s", pocket_monster_type_names[app->monster_type_filter]);
    snprintf(source_filter, sizeof(source_filter), "Source: %s", pocket_monster_source_names[app->monster_source_filter]);
    snprintf(browse_environment, sizeof(browse_environment), "Browse Env: %s",
             pocket_monster_environment_names[app->monster_environment_filter]);
    snprintf(search, sizeof(search), "Search: %.22s", app->monster_search[0] ? app->monster_search : "Any");
    snprintf(environment, sizeof(environment), "Environment: %s", pocket_monster_environment_names[app->encounter_environment]);
    snprintf(repeats, sizeof(repeats), "Repeat types: %s", app->encounter_allow_repeats ? "Yes" : "No");
    snprintf(template_name, sizeof(template_name), "Template: %s", pocket_encounter_template_names[app->encounter_template]);
    snprintf(role, sizeof(role), "Preferred Role: %s", pocket_monster_role_names[app->encounter_role]);
    const char* rows[] = {count, search, cr_filter, type_filter, source_filter, browse_environment,
        level, size, difficulty, environment, repeats, template_name, role,
        "Generate Encounter", "Pack Diagnostics", "Create Custom Monster"};
    pocket_draw_header(canvas, "Monsters & Encounters", app->status);
    pocket_draw_menu_rows(canvas, app, rows, 16U);
}

static __attribute__((unused)) void pocket_draw_monster_list(Canvas* canvas, PocketD20App* app) {
    pocket_draw_header(canvas, "Monster Stat Blocks", app->status);
    for(uint8_t row = 0U; row < 5U; ++row) {
        uint16_t index = app->scroll + row;
        if(index >= app->monster_count) break;
        PocketMonsterSummary monster;
        char cr[8], text[64];
        if(!pocket_monster_filtered_at(app, index, &monster)) continue;
        pocket_monster_cr(cr, sizeof(cr), monster.cr_eighths);
        snprintf(text, sizeof(text), "%s  CR %s", monster.name, cr);
        pocket_draw_row(canvas, row, index == app->selection, text);
    }
}

static __attribute__((unused)) void pocket_draw_monster_detail(Canvas* canvas, PocketD20App* app) {
    PocketMonsterDetail* m = app->monster_detail;
    if(!m) return;
    char cr[8], core[64], abilities[64];
    pocket_monster_cr(cr, sizeof(cr), m->summary.cr_eighths);
    snprintf(core, sizeof(core), "CR %s XP %lu AC %u HP %u", cr,
             (unsigned long)m->summary.xp, m->summary.armor_class, m->summary.hit_points);
    snprintf(abilities, sizeof(abilities), "S%d D%d C%d I%d W%d C%d",
             m->abilities[0], m->abilities[1], m->abilities[2], m->abilities[3],
             m->abilities[4], m->abilities[5]);
    char source[48], role[32];
    snprintf(source, sizeof(source), "Source: %s", m->summary.source);
    snprintf(role, sizeof(role), "Role: %s", m->summary.role);
    const char* rows[] = {core, m->summary.type, source, role, m->size_alignment, m->speed,
        abilities, m->skills, m->defenses, m->senses, m->languages, m->traits, m->actions,
        m->extra};
    pocket_draw_header(canvas, m->summary.name, "Up/Down: fields");
    for(uint8_t row = 0U; row < 5U; ++row) {
        uint16_t index = app->scroll + row;
        if(index >= 14U) break;
        pocket_draw_row(canvas, row, index == app->selection, rows[index]);
    }
}

static __attribute__((unused)) void pocket_draw_encounter(Canvas* canvas, PocketD20App* app) {
    PocketMonsterEncounter* encounter = app->monster_encounter;
    if(!encounter) return;
    char title[48];
    snprintf(title, sizeof(title), "Encounter %lu/%lu XP", (unsigned long)encounter->spent,
             (unsigned long)encounter->budget);
    pocket_draw_header(canvas, title, app->status);
    for(uint8_t row = 0U; row < 4U; ++row) {
        uint16_t index = app->scroll + row;
        if(index >= encounter->count) break;
        char text[64], cr[8];
        pocket_monster_cr(cr, sizeof(cr), encounter->monsters[index].cr_eighths);
        snprintf(text, sizeof(text), "%ux %s (CR %s)", encounter->quantities[index],
                 encounter->monsters[index].name, cr);
        pocket_draw_row(canvas, row, index == app->selection, text);
    }
    pocket_draw_row(canvas, 4U, false, "OK stats; hold > initiative");
}

static __attribute__((unused)) void pocket_draw_monster_diagnostics(Canvas* canvas, PocketD20App* app) {
    char total[32], valid[32], missing[32], fields[32], duplicates[32], bundled[32], user[32],
        problem[48], file[48], issue[48], recovered[32], rolled_back[32], heap[32], block[32];
    snprintf(total, sizeof(total), "Index records: %u", pocket_monster_count(app->storage));
    snprintf(valid, sizeof(valid), "Valid blocks: %u", app->monster_diag_valid);
    snprintf(missing, sizeof(missing), "Missing/invalid: %u", app->monster_diag_missing);
    snprintf(fields, sizeof(fields), "Field errors: %u", app->monster_diag_fields);
    snprintf(duplicates, sizeof(duplicates), "Duplicate IDs: %u", app->monster_diag_duplicates);
    snprintf(bundled, sizeof(bundled), "Bundled pack: v%u", app->monster_diag_bundled_version);
    if(app->monster_diag_user_present)
        snprintf(user, sizeof(user), "User pack: v%u", app->monster_diag_user_version);
    else snprintf(user, sizeof(user), "User pack: none");
    if(app->monster_diag_problem_valid) {
        snprintf(problem, sizeof(problem), "ID: %s", app->monster_diag_problem.id);
        snprintf(file, sizeof(file), "File: %s.txt", app->monster_diag_problem.id);
        snprintf(issue, sizeof(issue), "Issue: %s", app->monster_diag_issue);
    } else {
        snprintf(problem, sizeof(problem), "ID: none");
        snprintf(file, sizeof(file), "File: none");
        snprintf(issue, sizeof(issue), "Issue: none");
    }
    snprintf(heap, sizeof(heap), "Free heap: %lu", (unsigned long)memmgr_get_free_heap());
    snprintf(block, sizeof(block), "Max block: %lu", (unsigned long)memmgr_heap_get_max_free_block());
    snprintf(recovered, sizeof(recovered), "Recovered: %u", app->monster_diag_recovered);
    snprintf(rolled_back, sizeof(rolled_back), "Rolled back: %u", app->monster_diag_rolled_back);
    const char* rows[] = {total, valid, missing, fields, duplicates, bundled, user,
        problem, file, issue, recovered, rolled_back, heap, block, "OK: repair/rescan"};
    pocket_draw_header(canvas, "Monster Pack Diagnostics", app->status);
    pocket_draw_menu_rows(canvas, app, rows, 15U);
}

static __attribute__((unused)) void pocket_draw_monster_edit(Canvas* canvas, PocketD20App* app) {
    PocketMonsterDetail* m = app->monster_detail;
    if(!m) return;
    char cr[24], xp[24], ac[24], hp[24], environment[32], role[32], ability[6][24];
    char cr_value[8]; pocket_monster_cr(cr_value, sizeof(cr_value), m->summary.cr_eighths);
    snprintf(cr, sizeof(cr), "CR: %s", cr_value);
    snprintf(xp, sizeof(xp), "XP: %lu", (unsigned long)m->summary.xp);
    snprintf(ac, sizeof(ac), "AC: %u", m->summary.armor_class);
    snprintf(hp, sizeof(hp), "HP: %u", m->summary.hit_points);
    snprintf(environment, sizeof(environment), "Environment: %.18s", m->summary.environment);
    snprintf(role, sizeof(role), "Role: %.18s", m->summary.role);
    static const char* labels[] = {"STR", "DEX", "CON", "INT", "WIS", "CHA"};
    for(uint8_t i = 0U; i < 6U; ++i)
        snprintf(ability[i], sizeof(ability[i]), "%s: %d", labels[i], m->abilities[i]);
    const char* rows[] = {m->summary.name, cr, xp, ac, hp, m->summary.type, environment,
        role, m->size_alignment, m->speed, ability[0], ability[1], ability[2], ability[3],
        ability[4], ability[5], m->skills, m->defenses, m->senses, m->languages, m->traits,
        m->actions, m->extra, app->monster_edit_existing ? "Update Custom Monster" :
        "Save Custom Monster"};
    pocket_draw_header(canvas, app->monster_edit_existing ? "Edit Monster" : "Custom Monster", app->status);
    pocket_draw_menu_rows(canvas, app, rows, 24U);
}

static void pocket_run_stress_test(PocketD20App* app) {
    app->stress_heap_before = memmgr_get_free_heap();
    if(!app->stress_low_water || app->stress_heap_before < app->stress_low_water)
        app->stress_low_water = app->stress_heap_before;
    for(uint8_t cycle = 0U; cycle < 10U; ++cycle) {
        uint16_t failures_before = app->stress_failures;
        app->catalog_kind = PocketCatalogClasses;
        pocket_catalog_release(app);
        pocket_catalog_add_builtins(app, PocketCatalogClasses);
        pocket_catalog_load_external(app, PocketCatalogClasses);
        if(!app->catalog_count) ++app->stress_failures;
        pocket_catalog_release(app);

        PocketCampaignDiagnostics campaign_diagnostics;
        pocket_campaign_diagnose(app->storage, &campaign_diagnostics);
        if(!campaign_diagnostics.records) ++app->stress_failures;

        PocketMonsterDetail* detail = malloc(sizeof(PocketMonsterDetail));
        PocketMonsterSummary summary;
        if(!detail || !pocket_monster_at(app->storage, 0U, &summary) ||
           !pocket_monster_load(app->storage, &summary, detail)) ++app->stress_failures;
        free(detail);

        PocketProfileState profiles;
        pocket_d20_profiles_set_defaults(&profiles);
        if(!pocket_d20_profiles_load(app->storage, &profiles)) ++app->stress_failures;
        pocket_d20_profiles_free(&profiles);

        PocketMonsterEncounter* encounter = calloc(1U, sizeof(PocketMonsterEncounter));
        uint16_t monster_total = pocket_monster_count(app->storage);
        if(!encounter || !monster_total ||
           !pocket_monster_at(app->storage, cycle % monster_total, &encounter->monsters[0]))
            ++app->stress_failures;
        free(encounter);

        uint32_t free_heap = memmgr_get_free_heap();
        if(free_heap < app->stress_low_water) app->stress_low_water = free_heap;
        if(app->stress_failures == failures_before && free_heap + 256U < app->stress_heap_before)
            ++app->stress_failures;
        ++app->stress_cycles;
    }
    app->stress_heap_after = memmgr_get_free_heap();
    app->stress_max_block = memmgr_heap_get_max_free_block();
    pocket_set_status(app, app->stress_failures ? "Stress test found issues" : "Stress cycle passed");
}

static __attribute__((unused)) void pocket_draw_stress_test(Canvas* canvas, PocketD20App* app) {
    char cycles[32], failures[32], before[32], after[32], low[32], block[32];
    snprintf(cycles, sizeof(cycles), "Cycles: %u", app->stress_cycles);
    snprintf(failures, sizeof(failures), "Failures: %u", app->stress_failures);
    snprintf(before, sizeof(before), "Heap before: %lu", (unsigned long)app->stress_heap_before);
    snprintf(after, sizeof(after), "Heap after: %lu", (unsigned long)app->stress_heap_after);
    snprintf(low, sizeof(low), "Low water: %lu", (unsigned long)app->stress_low_water);
    snprintf(block, sizeof(block), "Max block: %lu", (unsigned long)app->stress_max_block);
    const char* rows[] = {cycles, failures, before, after, low, block, "OK: run 10 cycles"};
    pocket_draw_header(canvas, "Allocation Stress Test", app->status);
    pocket_draw_menu_rows(canvas, app, rows, 7U);
}
#endif

static void pocket_draw_animated_die(
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

static void pocket_draw_dice_animation(Canvas* canvas, PocketD20App* app) {
    char title[40];
    snprintf(
        title,
        sizeof(title),
        "Rolling %ud%u...",
        app->dice_anim_count,
        app->dice_anim_sides);
    pocket_draw_header(canvas, title, NULL);
    uint8_t visible_dice = app->dice_anim_count > 1U ? 3U : 1U;
    for(uint8_t i = 0U; i < visible_dice; ++i) {
        int32_t x = visible_dice == 1U ? 64 : 22 + (i * 42);
        uint8_t face =
            (uint8_t)(((app->dice_anim_frame * 7U) + (i * 5U) + app->dice_anim_sides) %
                      app->dice_anim_sides) +
            1U;
        pocket_draw_animated_die(canvas, x, 34, app->dice_anim_frame + i, face);
    }
    canvas_draw_frame(canvas, 14, 55, 100, 5);
    uint8_t progress = (uint8_t)(((app->dice_anim_frame + 1U) * 98U) /
                                 POCKET_D20_DICE_ANIMATION_FRAMES);
    canvas_draw_box(canvas, 15, 56, progress, 3);
}

static void pocket_draw_adventure_sprite(
    Canvas* canvas,
    const PocketAdventureScene* scene) {
    canvas_draw_frame(canvas, 2, 13, 27, 27);
    if(strcmp(scene->sprite, "dolphin") == 0) {
        canvas_draw_line(canvas, 6, 28, 13, 22);
        canvas_draw_line(canvas, 13, 22, 23, 24);
        canvas_draw_line(canvas, 23, 24, 16, 30);
        canvas_draw_line(canvas, 16, 30, 8, 31);
        canvas_draw_line(canvas, 8, 31, 6, 28);
        canvas_draw_line(canvas, 18, 23, 21, 19);
        canvas_draw_line(canvas, 9, 29, 5, 34);
        canvas_draw_dot(canvas, 20, 25);
    } else if(strcmp(scene->sprite, "pearl") == 0) {
        canvas_draw_circle(canvas, 15, 26, 8);
        canvas_draw_circle(canvas, 15, 26, 5);
        canvas_draw_dot(canvas, 13, 23);
    } else {
        canvas_draw_line(canvas, 4, 31, 9, 27);
        canvas_draw_line(canvas, 9, 27, 14, 31);
        canvas_draw_line(canvas, 14, 31, 19, 27);
        canvas_draw_line(canvas, 19, 27, 26, 31);
        canvas_draw_line(canvas, 4, 35, 9, 32);
        canvas_draw_line(canvas, 9, 32, 14, 35);
        canvas_draw_line(canvas, 14, 35, 19, 32);
        canvas_draw_line(canvas, 19, 32, 26, 35);
    }
}

static void pocket_draw_campaigns(Canvas* canvas, PocketD20App* app) {
    pocket_draw_header(canvas, "Campaign Packs", app->status);
    uint16_t rows = app->campaign_count + 1U;
    for(uint8_t visible = 0U; visible < 5U; ++visible) {
        uint16_t index = app->scroll + visible;
        if(index >= rows) break;
        char row[48];
        if(index < app->campaign_count) {
            PocketCampaignSummary campaign;
            if(!pocket_campaign_at(app->storage, index, &campaign)) continue;
            snprintf(row, sizeof(row), "%c %s",
                     !strcmp(campaign.id, app->data.character.adventure_campaign) ? '*' : ' ',
                     campaign.name);
        } else {
            snprintf(row, sizeof(row), "Campaign Diagnostics");
        }
        pocket_draw_row(canvas, visible, index == app->selection, row);
    }
}

static void pocket_draw_campaign_diagnostics(Canvas* canvas, PocketD20App* app) {
    PocketCampaignDiagnostics* d = &app->campaign_diagnostics;
    char records[32], incompatible[32], files[32], campaigns[32], scenes[32], entry[32],
        links[32], problem[48], detail[48];
    snprintf(records, sizeof(records), "Manifests: %u", d->records);
    snprintf(incompatible, sizeof(incompatible), "Incompatible: %u", d->incompatible);
    snprintf(files, sizeof(files), "Missing files: %u", d->missing_scene_files);
    snprintf(campaigns, sizeof(campaigns), "Duplicate packs: %u", d->duplicate_campaign_ids);
    snprintf(scenes, sizeof(scenes), "Duplicate scenes: %u", d->duplicate_scene_ids);
    snprintf(entry, sizeof(entry), "Missing entries: %u", d->missing_entry_scenes);
    snprintf(links, sizeof(links), "Broken links: %u", d->broken_links);
    snprintf(problem, sizeof(problem), "ID: %s", d->problem_id[0] ? d->problem_id : "none");
    snprintf(detail, sizeof(detail), "Issue: %.39s", d->problem[0] ? d->problem : "none");
    const char* rows[] = {records, incompatible, files, campaigns, scenes, entry, links,
                          problem, detail, "OK: rescan"};
    pocket_draw_header(canvas, "Campaign Diagnostics", app->status);
    pocket_draw_menu_rows(canvas, app, rows, 10U);
}

static void pocket_draw_adventure(Canvas* canvas, PocketD20App* app) {
    const PocketAdventureScene* scene = app->adventure_scene;
    if(!scene) {
        pocket_draw_header(canvas, "Adventure", "No scene");
        return;
    }
    char title[48];
    snprintf(title, sizeof(title), "Adventure: %s", scene->title);
    pocket_draw_header(canvas, title, app->status);
    pocket_draw_adventure_sprite(canvas, scene);
    char first[20];
    char second[20];
    pocket_truncate(first, sizeof(first), scene->body, 17U);
    const char* remainder = strlen(scene->body) > 17U ? scene->body + 17U : "";
    pocket_truncate(second, sizeof(second), remainder, 17U);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 32, 21, first);
    canvas_draw_str(canvas, 32, 31, second);
    uint8_t visible = scene->choice_count > 2U ? 2U : scene->choice_count;
    for(uint8_t row = 0U; row < visible; ++row) {
        uint8_t index = app->scroll + row;
        if(index >= scene->choice_count) break;
        char choice[48];
        if(scene->choices[index].skill >= 0 &&
           (uint8_t)scene->choices[index].skill < POCKET_D20_SKILL_COUNT)
            snprintf(
                choice,
                sizeof(choice),
                "%.24s [%.12s %u]",
                scene->choices[index].label,
                pocket_d20_skill_names[(uint8_t)scene->choices[index].skill],
                scene->choices[index].dc);
        else
            pocket_copy(choice, sizeof(choice), scene->choices[index].label);
        pocket_draw_row(canvas, (uint8_t)(3U + row), index == app->selection, choice);
    }
}

static void pocket_draw_callback(Canvas* canvas, void* model) {
    PocketD20App* app = *(PocketD20App**)model;
    canvas_clear(canvas);
    if(app->dice_animating) {
        pocket_draw_dice_animation(canvas, app);
        return;
    }
    switch(app->screen) {
    case PocketScreenHome:
        pocket_draw_home(canvas, app);
        break;
    case PocketScreenProfiles:
        pocket_draw_profiles(canvas, app);
        break;
    case PocketScreenProfileActions:
        pocket_draw_profile_actions(canvas, app);
        break;
    case PocketScreenCharacter:
        pocket_draw_character(canvas, app);
        break;
    case PocketScreenVitals:
        pocket_draw_vitals(canvas, app);
        break;
    case PocketScreenAbilities:
        pocket_draw_abilities(canvas, app);
        break;
    case PocketScreenSkills:
        pocket_draw_skills(canvas, app);
        break;
    case PocketScreenGrantReview:
        pocket_draw_grant_review(canvas, app);
        break;
    case PocketScreenGrantEdit:
        pocket_draw_grant_edit(canvas, app);
        break;
    case PocketScreenMagic:
        pocket_draw_magic(canvas, app);
        break;
    case PocketScreenSpellFilters:
        pocket_draw_spell_filters(canvas, app);
        break;
    case PocketScreenCurrency:
        pocket_draw_currency(canvas, app);
        break;
    case PocketScreenResources:
        pocket_draw_resources(canvas, app);
        break;
    case PocketScreenRecordList:
        pocket_draw_record_list(canvas, app);
        break;
    case PocketScreenRecordDetail:
        pocket_draw_record_detail(canvas, app);
        break;
    case PocketScreenCatalog:
        pocket_draw_catalog(canvas, app);
        break;
    case PocketScreenCombat:
        pocket_draw_combat(canvas, app);
        break;
    case PocketScreenAttackTemplates:
        pocket_draw_attack_templates(canvas, app);
        break;
    case PocketScreenAttackTemplateEdit:
        pocket_draw_attack_template_edit(canvas, app);
        break;
    case PocketScreenDice:
        pocket_draw_dice(canvas, app);
        break;
    case PocketScreenDiceResult:
        pocket_draw_dice_result(canvas, app);
        break;
    case PocketScreenAttackList:
        pocket_draw_attack_list(canvas, app);
        break;
    case PocketScreenAttackResult:
        pocket_draw_attack_result(canvas, app);
        break;
    case PocketScreenInitiativeMenu:
        pocket_draw_initiative_menu(canvas, app);
        break;
    case PocketScreenInitiativeSetup:
        pocket_draw_initiative_setup(canvas, app);
        break;
    case PocketScreenInitiativeCombat:
        pocket_draw_initiative_combat(canvas, app);
        break;
    case PocketScreenInitiativeEdit:
        pocket_draw_initiative_edit(canvas, app);
        break;
    case PocketScreenCampaigns:
        pocket_draw_campaigns(canvas, app);
        break;
    case PocketScreenCampaignDiagnostics:
        pocket_draw_campaign_diagnostics(canvas, app);
        break;
    case PocketScreenAdventure:
        pocket_draw_adventure(canvas, app);
        break;
    default:
        break;
    }
}

static void pocket_open_list(
    PocketD20App* app,
    PocketListKind kind,
    PocketScreen return_screen) {
    pocket_release_text_input(app);
    pocket_release_number_input(app);
    app->list_kind = kind;
    app->record_list_return_screen = return_screen;
    pocket_enter_screen(app, PocketScreenRecordList);
}

static bool pocket_add_record(PocketD20App* app) {
    pocket_release_text_input(app);
    pocket_release_number_input(app);
    PocketCharacter* character = &app->data.character;
    switch(app->list_kind) {
    case PocketListClasses:
        if(character->class_count >= POCKET_D20_MAX_CLASSES ||
           pocket_d20_total_level(character) >= 20U)
            return false;
        app->record_index = character->class_count++;
        memset(&character->classes[app->record_index], 0, sizeof(PocketClassLevel));
        pocket_copy(
            character->classes[app->record_index].name,
            sizeof(character->classes[app->record_index].name),
            "New Class");
        pocket_copy(
            character->classes[app->record_index].subclass,
            sizeof(character->classes[app->record_index].subclass),
            "None");
        character->classes[app->record_index].level = 1U;
        character->classes[app->record_index].hit_die = 8U;
        character->classes[app->record_index].hit_dice_current = 1U;
        character->classes[app->record_index].hit_dice_max = 1U;
        character->classes[app->record_index].spellcasting_ability = PocketAbilityIntelligence;
        break;
    case PocketListSpells:
        if(character->spell_count >= POCKET_D20_MAX_SPELLS ||
           !pocket_d20_data_reserve_spells(character, character->spell_count + 1U))
            return false;
        app->record_index = character->spell_count++;
        memset(&character->spells[app->record_index], 0, sizeof(PocketSpell));
        character->spell_known[app->record_index] = 1U;
        character->spell_always_prepared[app->record_index] = 0U;
        character->spell_free_casts_current[app->record_index] = 0U;
        character->spell_free_casts_max[app->record_index] = 0U;
        pocket_copy(
            character->spells[app->record_index].name,
            sizeof(character->spells[app->record_index].name),
            "New Spell");
        break;
    case PocketListFeatures:
        if(character->feature_count >= POCKET_D20_MAX_FEATURES ||
           !pocket_d20_data_reserve_features(character, character->feature_count + 1U))
            return false;
        app->record_index = character->feature_count++;
        memset(&character->features[app->record_index], 0, sizeof(PocketFeature));
        pocket_copy(
            character->features[app->record_index].name,
            sizeof(character->features[app->record_index].name),
            "New Feature");
        character->features[app->record_index].class_index = 0U;
        character->features[app->record_index].class_level_gained =
            character->classes[0].level;
        break;
    case PocketListItems: {
        if(character->item_count >= POCKET_D20_MAX_ITEMS ||
           !pocket_d20_data_reserve_items(character, character->item_count + 1U))
            return false;
        app->record_index = character->item_count++;
        PocketItem* item = &character->items[app->record_index];
        memset(item, 0, sizeof(*item));
        pocket_copy(item->name, sizeof(item->name), "New Item");
        item->quantity = 1;
        item->damage_dice = 1U;
        item->damage_die = 6U;
        item->extra_die = 6U;
        item->add_ability_damage = 1U;
        item->container_index = -1;
        item->armor_dex_cap = -1;
        break;
    }
    case PocketListLanguages:
        if(character->language_count >= POCKET_D20_MAX_LANGUAGES) return false;
        app->record_index = character->language_count++;
        memset(character->languages[app->record_index], 0, POCKET_D20_SHORT_LEN);
        pocket_copy(
            character->languages[app->record_index],
            POCKET_D20_SHORT_LEN,
            "New Language");
        break;
    case PocketListJournal:
        if(character->journal_count >= POCKET_D20_MAX_JOURNAL ||
           !pocket_d20_data_reserve_journal(character, character->journal_count + 1U))
            return false;
        app->record_index = character->journal_count++;
        memset(&character->journal[app->record_index], 0, sizeof(PocketJournalEntry));
        pocket_copy(
            character->journal[app->record_index].title,
            sizeof(character->journal[app->record_index].title),
            "New Note");
        break;
    case PocketListParty:
        if(app->data.party_count >= POCKET_D20_MAX_PARTY) return false;
        app->record_index = app->data.party_count++;
        memset(&app->data.party[app->record_index], 0, sizeof(PocketPartyMember));
        pocket_copy(
            app->data.party[app->record_index].name,
            sizeof(app->data.party[app->record_index].name),
            "Party Member");
        app->data.party[app->record_index].hp_current = 1;
        app->data.party[app->record_index].hp_max = 1;
        app->data.party[app->record_index].armor_class = 10;
        break;
    }
    pocket_save(app, false);
    pocket_enter_screen(app, PocketScreenRecordDetail);
    return true;
}

static void pocket_delete_record(PocketD20App* app) {
    PocketCharacter* character = &app->data.character;
    uint8_t index = app->record_index;
    switch(app->list_kind) {
    case PocketListClasses:
        if(character->class_count <= 1U) {
            pocket_set_status(app, "Keep at least one class");
            return;
        }
        memmove(
            &character->classes[index],
            &character->classes[index + 1U],
            (character->class_count - index - 1U) * sizeof(PocketClassLevel));
        --character->class_count;
        memset(&character->classes[character->class_count], 0, sizeof(PocketClassLevel));
        for(uint8_t i = 0U; i < character->feature_count; ++i) {
            if(character->features[i].class_index == index)
                character->features[i].class_index = 0U;
            else if(character->features[i].class_index > index)
                --character->features[i].class_index;
        }
        for(uint8_t i = 0U; i < character->spell_count; ++i) {
            if(character->spells[i].class_index == index)
                character->spells[i].class_index = 0U;
            else if(character->spells[i].class_index > index)
                --character->spells[i].class_index;
        }
        for(uint8_t i = 0U; i < character->journal_count; ++i) {
            if(character->journal[i].class_index == index)
                character->journal[i].class_index = 0U;
            else if(character->journal[i].class_index > index)
                --character->journal[i].class_index;
        }
        break;
    case PocketListSpells:
        memmove(
            &character->spells[index],
            &character->spells[index + 1U],
            (character->spell_count - index - 1U) * sizeof(PocketSpell));
        memmove(
            &character->spell_known[index],
            &character->spell_known[index + 1U],
            character->spell_count - index - 1U);
        memmove(
            &character->spell_always_prepared[index],
            &character->spell_always_prepared[index + 1U],
            character->spell_count - index - 1U);
        memmove(
            &character->spell_free_casts_current[index],
            &character->spell_free_casts_current[index + 1U],
            character->spell_count - index - 1U);
        memmove(
            &character->spell_free_casts_max[index],
            &character->spell_free_casts_max[index + 1U],
            character->spell_count - index - 1U);
        --character->spell_count;
        memset(&character->spells[character->spell_count], 0, sizeof(PocketSpell));
        character->spell_known[character->spell_count] = 0U;
        character->spell_always_prepared[character->spell_count] = 0U;
        character->spell_free_casts_current[character->spell_count] = 0U;
        character->spell_free_casts_max[character->spell_count] = 0U;
        break;
    case PocketListFeatures:
        memmove(
            &character->features[index],
            &character->features[index + 1U],
            (character->feature_count - index - 1U) * sizeof(PocketFeature));
        --character->feature_count;
        memset(&character->features[character->feature_count], 0, sizeof(PocketFeature));
        break;
    case PocketListItems:
        memmove(
            &character->items[index],
            &character->items[index + 1U],
            (character->item_count - index - 1U) * sizeof(PocketItem));
        --character->item_count;
        memset(&character->items[character->item_count], 0, sizeof(PocketItem));
        break;
    case PocketListLanguages:
        memmove(
            &character->languages[index],
            &character->languages[index + 1U],
            (character->language_count - index - 1U) * POCKET_D20_SHORT_LEN);
        --character->language_count;
        memset(character->languages[character->language_count], 0, POCKET_D20_SHORT_LEN);
        break;
    case PocketListJournal:
        memmove(
            &character->journal[index],
            &character->journal[index + 1U],
            (character->journal_count - index - 1U) * sizeof(PocketJournalEntry));
        --character->journal_count;
        memset(&character->journal[character->journal_count], 0, sizeof(PocketJournalEntry));
        break;
    case PocketListParty:
        memmove(
            &app->data.party[index],
            &app->data.party[index + 1U],
            (app->data.party_count - index - 1U) * sizeof(PocketPartyMember));
        --app->data.party_count;
        memset(&app->data.party[app->data.party_count], 0, sizeof(PocketPartyMember));
        break;
    }
    pocket_save(app, false);
    pocket_enter_screen(app, PocketScreenRecordList);
}

static void pocket_text_done(void* context) {
    PocketD20App* app = context;
    app->input_module_active = 0U;
    PocketCharacter* character = &app->data.character;
    uint8_t index = app->record_index;
    switch(app->edit_target) {
    case PocketEditCharacterName:
        pocket_copy(character->name, sizeof(character->name), app->edit_buffer);
        break;
    case PocketEditPlayerName:
        pocket_copy(character->player, sizeof(character->player), app->edit_buffer);
        break;
    case PocketEditSpecies:
        pocket_copy(character->species, sizeof(character->species), app->edit_buffer);
        break;
    case PocketEditBackground:
        pocket_copy(character->background, sizeof(character->background), app->edit_buffer);
        break;
    case PocketEditAlignment:
        pocket_copy(character->alignment, sizeof(character->alignment), app->edit_buffer);
        break;
    case PocketEditOtherProficiencies:
        pocket_copy(
            character->other_proficiencies,
            sizeof(character->other_proficiencies),
            app->edit_buffer);
        break;
    case PocketEditOriginFeat:
        pocket_copy(character->origin_feat, sizeof(character->origin_feat), app->edit_buffer);
        break;
    case PocketEditToolProficiencies:
        pocket_copy(character->tool_proficiencies, sizeof(character->tool_proficiencies), app->edit_buffer);
        break;
    case PocketEditArmorTraining:
        pocket_copy(character->armor_training, sizeof(character->armor_training), app->edit_buffer);
        break;
    case PocketEditWeaponTraining:
        pocket_copy(character->weapon_training, sizeof(character->weapon_training), app->edit_buffer);
        break;
    case PocketEditSenses:
        pocket_copy(character->senses, sizeof(character->senses), app->edit_buffer);
        break;
    case PocketEditConditions:
        pocket_copy(character->conditions, sizeof(character->conditions), app->edit_buffer);
        break;
    case PocketEditConcentration:
        pocket_copy(character->concentration, sizeof(character->concentration), app->edit_buffer);
        break;
    case PocketEditTemporaryEffects:
        pocket_copy(character->temporary_effects, sizeof(character->temporary_effects), app->edit_buffer);
        break;
    case PocketEditResistances:
        pocket_copy(character->resistances, sizeof(character->resistances), app->edit_buffer);
        break;
    case PocketEditImmunities:
        pocket_copy(character->immunities, sizeof(character->immunities), app->edit_buffer);
        break;
    case PocketEditVulnerabilities:
        pocket_copy(character->vulnerabilities, sizeof(character->vulnerabilities), app->edit_buffer);
        break;
    case PocketEditMovementModes:
        pocket_copy(character->movement_modes, sizeof(character->movement_modes), app->edit_buffer);
        break;
    case PocketEditClassName:
        pocket_copy(
            character->classes[index].name,
            sizeof(character->classes[index].name),
            app->edit_buffer);
        break;
    case PocketEditSubclass:
        pocket_copy(
            character->classes[index].subclass,
            sizeof(character->classes[index].subclass),
            app->edit_buffer);
        break;
    case PocketEditSpellName:
        pocket_copy(
            character->spells[index].name,
            sizeof(character->spells[index].name),
            app->edit_buffer);
        break;
    case PocketEditSpellDetail:
        pocket_copy(
            character->spells[index].detail,
            sizeof(character->spells[index].detail),
            app->edit_buffer);
        break;
    case PocketEditSpellStableId:
        pocket_copy(character->spells[index].stable_id, sizeof(character->spells[index].stable_id), app->edit_buffer);
        break;
    case PocketEditSpellSource:
        pocket_copy(character->spells[index].source, sizeof(character->spells[index].source), app->edit_buffer);
        break;
    case PocketEditSpellSchool:
        pocket_copy(character->spells[index].school, sizeof(character->spells[index].school), app->edit_buffer);
        break;
    case PocketEditSpellGrantName:
        pocket_copy(character->spells[index].grant_name, sizeof(character->spells[index].grant_name), app->edit_buffer);
        break;
    case PocketEditGrantStableId:
        if(index < character->grant_count) pocket_copy(character->grants[index].stable_id,
            sizeof(character->grants[index].stable_id), app->edit_buffer);
        break;
    case PocketEditGrantSource:
        if(index < character->grant_count) pocket_copy(character->grants[index].source,
            sizeof(character->grants[index].source), app->edit_buffer);
        break;
    case PocketEditGrantOption:
        if(index < character->grant_count) pocket_copy(character->grants[index].option_name,
            sizeof(character->grants[index].option_name), app->edit_buffer);
        break;
    case PocketEditGrantPrerequisites:
        if(index < character->grant_count) pocket_copy(character->grants[index].prerequisites,
            sizeof(character->grants[index].prerequisites), app->edit_buffer);
        break;
    case PocketEditGrantValue:
        if(index < character->grant_count) pocket_copy(character->grants[index].grant_value,
            sizeof(character->grants[index].grant_value), app->edit_buffer);
        break;
    case PocketEditAttackName:
        if(index < character->attack_template_count) pocket_copy(
            character->attack_templates[index].name,
            sizeof(character->attack_templates[index].name), app->edit_buffer);
        break;
    case PocketEditAttackMastery:
        if(index < character->attack_template_count) pocket_copy(
            character->attack_templates[index].mastery,
            sizeof(character->attack_templates[index].mastery), app->edit_buffer);
        break;
    case PocketEditAttackDamageType:
        if(index < character->attack_template_count) pocket_copy(
            character->attack_templates[index].damage_type,
            sizeof(character->attack_templates[index].damage_type), app->edit_buffer);
        break;
    case PocketEditAttackRiderType:
        if(index < character->attack_template_count) pocket_copy(
            character->attack_templates[index].rider_type,
            sizeof(character->attack_templates[index].rider_type), app->edit_buffer);
        break;
    case PocketEditFeatureName:
        pocket_copy(
            character->features[index].name,
            sizeof(character->features[index].name),
            app->edit_buffer);
        break;
    case PocketEditFeatureDetail:
        pocket_copy(
            character->features[index].detail,
            sizeof(character->features[index].detail),
            app->edit_buffer);
        break;
    case PocketEditItemName:
        pocket_copy(
            character->items[index].name,
            sizeof(character->items[index].name),
            app->edit_buffer);
        break;
    case PocketEditItemDetail:
        pocket_copy(
            character->items[index].detail,
            sizeof(character->items[index].detail),
            app->edit_buffer);
        break;
    case PocketEditItemAmmoGroup:
        pocket_copy(character->items[index].ammunition_group, sizeof(character->items[index].ammunition_group), app->edit_buffer);
        break;
    case PocketEditLanguageName:
        pocket_copy(character->languages[index], POCKET_D20_SHORT_LEN, app->edit_buffer);
        break;
    case PocketEditJournalTitle:
        pocket_copy(
            character->journal[index].title,
            sizeof(character->journal[index].title),
            app->edit_buffer);
        break;
    case PocketEditJournalBody:
        pocket_copy(
            character->journal[index].body,
            sizeof(character->journal[index].body),
            app->edit_buffer);
        break;
    case PocketEditPartyName:
        pocket_copy(
            app->data.party[index].name,
            sizeof(app->data.party[index].name),
            app->edit_buffer);
        break;
    case PocketEditTemporaryInitiativeName:
        pocket_copy(
            app->data.initiative.entries[index].name,
            sizeof(app->data.initiative.entries[index].name),
            app->edit_buffer);
        break;
    case PocketEditInitiativeConditions:
        pocket_copy(
            app->data.initiative.entries[index].conditions,
            sizeof(app->data.initiative.entries[index].conditions),
            app->edit_buffer);
        break;
#if 0 /* v2.6: monster text editing moved to Dolphin Bestiary. */
    case PocketEditMonsterSearch:
        pocket_copy(app->monster_search, sizeof(app->monster_search), app->edit_buffer);
        app->monster_count = 0U;
        app->screen = PocketScreenMonsters;
        app->selection = 1U;
        app->scroll = 0U;
        break;
    case PocketEditMonsterName:
        if(app->monster_detail) pocket_copy(app->monster_detail->summary.name,
            sizeof(app->monster_detail->summary.name), app->edit_buffer);
        break;
    case PocketEditMonsterType:
        if(app->monster_detail) pocket_copy(app->monster_detail->summary.type,
            sizeof(app->monster_detail->summary.type), app->edit_buffer);
        break;
    case PocketEditMonsterSize:
        if(app->monster_detail) pocket_copy(app->monster_detail->size_alignment,
            sizeof(app->monster_detail->size_alignment), app->edit_buffer);
        break;
    case PocketEditMonsterSpeed:
        if(app->monster_detail) pocket_copy(app->monster_detail->speed,
            sizeof(app->monster_detail->speed), app->edit_buffer);
        break;
    case PocketEditMonsterSkills:
        if(app->monster_detail) pocket_copy(app->monster_detail->skills,
            sizeof(app->monster_detail->skills), app->edit_buffer);
        break;
    case PocketEditMonsterDefenses:
        if(app->monster_detail) pocket_copy(app->monster_detail->defenses,
            sizeof(app->monster_detail->defenses), app->edit_buffer);
        break;
    case PocketEditMonsterSenses:
        if(app->monster_detail) pocket_copy(app->monster_detail->senses,
            sizeof(app->monster_detail->senses), app->edit_buffer);
        break;
    case PocketEditMonsterLanguages:
        if(app->monster_detail) pocket_copy(app->monster_detail->languages,
            sizeof(app->monster_detail->languages), app->edit_buffer);
        break;
    case PocketEditMonsterTraits:
        if(app->monster_detail) pocket_copy(app->monster_detail->traits,
            sizeof(app->monster_detail->traits), app->edit_buffer);
        break;
    case PocketEditMonsterActions:
        if(app->monster_detail) pocket_copy(app->monster_detail->actions,
            sizeof(app->monster_detail->actions), app->edit_buffer);
        break;
    case PocketEditMonsterExtra:
        if(app->monster_detail) pocket_copy(app->monster_detail->extra,
            sizeof(app->monster_detail->extra), app->edit_buffer);
        break;
#endif
    case PocketEditNone:
        break;
    default:
        break;
    }
    app->edit_target = PocketEditNone;
    pocket_save(app, false);
    view_dispatcher_switch_to_view(app->dispatcher, PocketViewMain);
    pocket_refresh(app);
}

static bool pocket_is_move_event(const InputEvent* event) {
    return event->type == InputTypeShort || event->type == InputTypeRepeat;
}

static void pocket_handle_back(PocketD20App* app) {
    switch(app->screen) {
    case PocketScreenHome:
        pocket_save(app, false);
        view_dispatcher_stop(app->dispatcher);
        break;
    case PocketScreenRecordList:
        pocket_enter_screen(app, app->record_list_return_screen);
        break;
    case PocketScreenProfileActions:
        pocket_enter_screen(app, PocketScreenProfiles);
        break;
    case PocketScreenRecordDetail:
        pocket_enter_screen(app, PocketScreenRecordList);
        app->selection = app->record_index + 1U;
        break;
    case PocketScreenCatalog:
        pocket_catalog_release(app);
        pocket_enter_screen(app, app->return_screen);
        app->selection = app->catalog_return_selection;
        if(app->selection >= 5U) app->scroll = app->selection - 4U;
        break;
    case PocketScreenMagic:
        app->arcane_recovery_active = 0U;
        pocket_enter_screen(app, PocketScreenHome);
        break;
    case PocketScreenGrantReview:
        pocket_enter_screen(app, app->return_screen);
        break;
    case PocketScreenGrantEdit:
        pocket_enter_screen(app, PocketScreenGrantReview);
        break;
    case PocketScreenCatalogDiagnostics:
        pocket_enter_screen(app, PocketScreenBuilder);
        break;
    case PocketScreenSpellFilters:
        pocket_enter_screen(app, PocketScreenMagic);
        break;
    case PocketScreenAttackTemplates:
        pocket_enter_screen(app, PocketScreenCombat);
        break;
    case PocketScreenAttackTemplateEdit:
        pocket_enter_screen(app, PocketScreenAttackTemplates);
        break;
    case PocketScreenAttackList:
    case PocketScreenAttackResult:
        pocket_enter_screen(app, PocketScreenCombat);
        break;
    case PocketScreenDiceResult:
        pocket_enter_screen(app, PocketScreenDice);
        break;
    case PocketScreenInitiativeSetup:
    case PocketScreenInitiativeCombat:
        pocket_enter_screen(app, PocketScreenInitiativeMenu);
        break;
    case PocketScreenInitiativeEdit:
        pocket_enter_screen(app, PocketScreenInitiativeCombat);
        app->selection = app->record_index;
        if(app->selection >= 5U) app->scroll = app->selection - 4U;
        break;
    case PocketScreenAdventure:
        pocket_campaign_save_active_progress(app);
        pocket_adventure_release(app);
        pocket_enter_screen(app, PocketScreenHome);
        break;
    case PocketScreenCampaigns:
        pocket_enter_screen(app, PocketScreenHome);
        break;
    case PocketScreenCampaignDiagnostics:
        pocket_enter_screen(app, PocketScreenCampaigns);
        break;
    default:
        pocket_enter_screen(app, PocketScreenHome);
        break;
    }
}

static void pocket_handle_long_back(PocketD20App* app) {
    if(app->screen == PocketScreenHome) {
        pocket_save(app, false);
        view_dispatcher_stop(app->dispatcher);
        return;
    }
    app->dice_animating = 0U;
    app->arcane_recovery_active = 0U;
    pocket_catalog_release(app);
    if(app->screen == PocketScreenAdventure) pocket_campaign_save_active_progress(app);
    pocket_adventure_release(app);
    pocket_enter_screen(app, PocketScreenHome);
    furi_timer_start(app->dice_timer, furi_ms_to_ticks(POCKET_D20_MARQUEE_MS));
}

static void pocket_handle_profiles(PocketD20App* app, const InputEvent* event) {
    uint16_t profile_count = pocket_profile_count(app);
    uint16_t row_count = profile_count + 1U;
    if(pocket_is_move_event(event) && event->key == InputKeyUp)
        pocket_menu_move(app, row_count, -1);
    else if(pocket_is_move_event(event) && event->key == InputKeyDown)
        pocket_menu_move(app, row_count, 1);
    else if(event->type == InputTypeShort && event->key == InputKeyOk) {
        if(app->selection == profile_count)
            pocket_create_profile(app);
        else
            pocket_switch_profile(app, pocket_profile_id_at(app, app->selection));
    } else if(event->type == InputTypeLong && event->key == InputKeyOk &&
              app->selection < profile_count) {
        app->profile_action_id = pocket_profile_id_at(app, app->selection);
        pocket_enter_screen(app, PocketScreenProfileActions);
    }
}

static void pocket_profile_actions_to_list(PocketD20App* app) {
    app->screen = PocketScreenProfiles;
    app->selection = 0U;
    app->scroll = 0U;
}

static void pocket_handle_profile_actions(PocketD20App* app, const InputEvent* event) {
    uint16_t count = sizeof(pocket_profile_actions) / sizeof(pocket_profile_actions[0]);
    if(pocket_is_move_event(event) && event->key == InputKeyUp)
        pocket_menu_move(app, count, -1);
    else if(pocket_is_move_event(event) && event->key == InputKeyDown)
        pocket_menu_move(app, count, 1);
    else if(event->type == InputTypeShort && event->key == InputKeyOk) {
        uint32_t profile = app->profile_action_id;
        if(app->selection == 0U) {
            pocket_switch_profile(app, profile);
        } else if(app->selection == 1U) {
            if(profile != app->profiles.active_profile) {
                pocket_set_status(app, "Switch before rename");
            } else {
                pocket_begin_text(
                    app,
                    PocketEditCharacterName,
                    "Character name",
                    app->data.character.name);
            }
        } else if(app->selection == 2U) {
            uint32_t destination = pocket_d20_profiles_next_id(&app->profiles);
            bool duplicated =
                !(destination == UINT32_MAX && pocket_profile_exists(app, UINT32_MAX)) &&
                pocket_d20_storage_duplicate_profile(app->storage, profile, destination) &&
                pocket_d20_profiles_refresh(app->storage, &app->profiles) &&
                pocket_d20_profiles_save(app->storage, &app->profiles);
            pocket_profile_actions_to_list(app);
            pocket_set_status(app, duplicated ? "Character duplicated" : "Duplicate failed");
        } else if(app->selection == 3U) {
            bool exported =
                (profile != app->profiles.active_profile || pocket_save(app, false)) &&
                pocket_d20_storage_export_profile(app->storage, profile);
            pocket_set_status(app, exported ? "Export written" : "Export failed");
        } else if(app->selection == 4U) {
            uint32_t previous = app->profiles.active_profile;
            uint32_t destination = pocket_d20_profiles_next_id(&app->profiles);
            bool imported = pocket_save(app, false) &&
                            !(destination == UINT32_MAX &&
                              pocket_profile_exists(app, UINT32_MAX)) &&
                            pocket_d20_storage_import_first(
                                app->storage, destination, &app->data);
            if(imported) {
                app->profiles.active_profile = destination;
                app->campaign_active_valid = 0U;
                imported = pocket_d20_profiles_refresh(app->storage, &app->profiles) &&
                           pocket_d20_profiles_save(app->storage, &app->profiles);
                app->saved_fingerprint = pocket_data_fingerprint(&app->data);
                pocket_enter_screen(app, PocketScreenHome);
                pocket_set_status(app, imported ? "Character imported" : "Import metadata failed");
            } else {
                bool recovered = false;
                pocket_d20_storage_load_profile(
                    app->storage, previous, &app->data, &recovered);
                app->saved_fingerprint = pocket_data_fingerprint(&app->data);
                pocket_set_status(app, "No valid export");
            }
        } else if(app->selection == 5U) {
            if(profile == app->profiles.active_profile) {
                pocket_set_status(app, "Switch before archive");
            } else {
                bool archived = pocket_d20_storage_archive_profile(app->storage, profile) &&
                                pocket_d20_profiles_refresh(app->storage, &app->profiles) &&
                                pocket_d20_profiles_save(app->storage, &app->profiles);
                pocket_profile_actions_to_list(app);
                pocket_set_status(app, archived ? "Character archived" : "Archive failed");
            }
        } else if(app->selection == 6U) {
            pocket_delete_profile(app, profile);
            pocket_profile_actions_to_list(app);
        } else if(app->selection == 7U) {
            bool verified = pocket_d20_storage_verify_profile(app->storage, profile);
            pocket_set_status(app, verified ? "Checksum verified" : "Save damaged/incompatible");
        } else if(profile != app->profiles.active_profile) {
            pocket_set_status(app, "Switch before restore");
        } else {
            bool restored = pocket_d20_storage_restore_backup(
                app->storage, profile, &app->data);
            if(!restored) {
                bool recovered = false;
                pocket_d20_storage_load_profile(
                    app->storage, profile, &app->data, &recovered);
            }
            pocket_d20_profiles_refresh(app->storage, &app->profiles);
            pocket_d20_profiles_save(app->storage, &app->profiles);
            app->saved_fingerprint = pocket_data_fingerprint(&app->data);
            pocket_set_status(app, restored ? "Backup restored" : "No valid backup");
        }
    }
}

static void pocket_handle_home(PocketD20App* app, const InputEvent* event) {
    uint16_t count = sizeof(pocket_home_items) / sizeof(pocket_home_items[0]);
    if(pocket_is_move_event(event) && event->key == InputKeyUp)
        pocket_menu_move(app, count, -1);
    else if(pocket_is_move_event(event) && event->key == InputKeyDown)
        pocket_menu_move(app, count, 1);
    else if(event->type == InputTypeShort && event->key == InputKeyOk) {
        switch(app->selection) {
        case 0:
            pocket_d20_profiles_refresh(app->storage, &app->profiles);
            pocket_profile_include_active(app);
            pocket_enter_screen(app, PocketScreenProfiles);
            break;
        case 1:
            pocket_enter_screen(app, PocketScreenCharacter);
            break;
        case 2:
            pocket_enter_screen(app, PocketScreenVitals);
            break;
        case 3:
            pocket_enter_screen(app, PocketScreenAbilities);
            break;
        case 4:
            pocket_enter_screen(app, PocketScreenSkills);
            break;
        case 5:
            pocket_enter_screen(app, PocketScreenMagic);
            break;
        case 6:
            pocket_open_list(app, PocketListFeatures, PocketScreenHome);
            break;
        case 7:
            pocket_open_list(app, PocketListItems, PocketScreenHome);
            break;
        case 8:
            pocket_enter_screen(app, PocketScreenCurrency);
            break;
        case 9:
            pocket_enter_screen(app, PocketScreenResources);
            break;
        case 10:
            pocket_open_list(app, PocketListJournal, PocketScreenHome);
            break;
        case 11:
            app->campaign_count = pocket_campaign_count(app->storage);
            if(app->campaign_count)
                pocket_enter_screen(app, PocketScreenCampaigns);
            else
                pocket_set_status(app, "No campaign manifests");
            break;
        case 12:
            app->hit_die_class_index = 0U;
            if(app->roll_mode == PocketRollGuidance) app->roll_mode = PocketRollNormal;
            pocket_enter_screen(app, PocketScreenCombat);
            break;
        case 13:
            pocket_enter_screen(app, PocketScreenInitiativeMenu);
            break;
        case 14:
            pocket_enter_screen(app, PocketScreenDice);
            break;
        case 15:
            app->storage_read_only = 0U;
            pocket_save(app, true);
            break;
        case 16: {
            if(app->storage_unsaved) {
                pocket_set_status(app, "Retry save before switching");
                break;
            }
            pocket_save(app, false);
            Loader* loader = furi_record_open(RECORD_LOADER);
            loader_enqueue_launch(
                loader,
                "/ext/apps/Games/dolphin_bestiary.fap",
                NULL,
                LoaderDeferredLaunchFlagGui);
            furi_record_close(RECORD_LOADER);
            view_dispatcher_stop(app->dispatcher);
            break;
        }
        }
    }
}

static void pocket_handle_character(PocketD20App* app, const InputEvent* event) {
    PocketCharacter* character = &app->data.character;
    if(pocket_is_move_event(event) && event->key == InputKeyUp)
        pocket_menu_move(app, 12U, -1);
    else if(pocket_is_move_event(event) && event->key == InputKeyDown)
        pocket_menu_move(app, 12U, 1);
    else if(pocket_is_move_event(event) &&
            (event->key == InputKeyLeft || event->key == InputKeyRight)) {
        int8_t delta = event->key == InputKeyRight ? 1 : -1;
        if(app->selection == 7U) {
            int64_t value = (int64_t)character->experience + (delta * 100);
            if(value < 0) value = 0;
            if(value > 1000000) value = 1000000;
            character->experience = (uint32_t)value;
            pocket_save(app, false);
        } else if(app->selection == 8U) {
            character->milestone_leveling = !character->milestone_leveling;
            pocket_save(app, false);
        } else if(app->selection == 11U) {
            character->inspiration = !character->inspiration;
            pocket_save(app, false);
        }
    } else if(event->type == InputTypeLong && event->key == InputKeyOk &&
              app->selection == 7U) {
        pocket_begin_number(
            app,
            PocketNumberCharacter,
            7U,
            0U,
            "Experience points",
            (int32_t)character->experience,
            0,
            1000000);
    } else if(event->type == InputTypeLong && event->key == InputKeyOk &&
              (app->selection == 2U || app->selection == 3U || app->selection == 4U)) {
        if(app->selection == 2U)
            pocket_begin_text(app, PocketEditSpecies, "Custom species", character->species);
        else if(app->selection == 3U)
            pocket_begin_text(app, PocketEditBackground, "Custom background", character->background);
        else
            pocket_begin_text(app, PocketEditAlignment, "Custom alignment", character->alignment);
    } else if(event->type == InputTypeShort && event->key == InputKeyOk) {
        switch(app->selection) {
        case 0:
            pocket_begin_text(app, PocketEditCharacterName, "Character name", character->name);
            break;
        case 1:
            pocket_begin_text(app, PocketEditPlayerName, "Player name", character->player);
            break;
        case 2:
            pocket_open_catalog(app, PocketCatalogSpecies, PocketEditSpecies, character->species);
            break;
        case 3:
            pocket_open_catalog(
                app,
                PocketCatalogBackgrounds,
                PocketEditBackground,
                character->background);
            break;
        case 4:
            pocket_open_catalog(
                app,
                PocketCatalogAlignments,
                PocketEditAlignment,
                character->alignment);
            break;
        case 5:
            pocket_open_list(app, PocketListClasses, PocketScreenCharacter);
            break;
        case 7:
            character->experience += 100U;
            pocket_save(app, false);
            break;
        case 8:
            character->milestone_leveling = !character->milestone_leveling;
            pocket_save(app, false);
            break;
        case 9:
            pocket_open_list(app, PocketListLanguages, PocketScreenCharacter);
            break;
        case 10:
            pocket_begin_text(
                app,
                PocketEditOtherProficiencies,
                "Other proficiencies",
                character->other_proficiencies);
            break;
        case 11:
            character->inspiration = !character->inspiration;
            pocket_save(app, false);
            break;
        default:
            break;
        }
    }
}

static void pocket_handle_vitals(PocketD20App* app, const InputEvent* event) {
    PocketCharacter* character = &app->data.character;
    if(pocket_is_move_event(event) && event->key == InputKeyUp)
        pocket_menu_move(app, 16U, -1);
    else if(pocket_is_move_event(event) && event->key == InputKeyDown)
        pocket_menu_move(app, 16U, 1);
    else if(pocket_is_move_event(event) &&
            (event->key == InputKeyLeft || event->key == InputKeyRight)) {
        int16_t delta = event->key == InputKeyRight ? 1 : -1;
        switch(app->selection) {
        case 0:
            character->hp_current = pocket_clamp_i16(character->hp_current + delta, 0, 999);
            break;
        case 1:
            character->hp_max = pocket_clamp_i16(character->hp_max + delta, 1, 999);
            if(character->hp_current > character->hp_max) character->hp_current = character->hp_max;
            break;
        case 2:
            character->hp_temporary = pocket_clamp_i16(character->hp_temporary + delta, 0, 999);
            break;
        case 3:
            character->armor_class = pocket_clamp_i16(character->armor_class + delta, 0, 99);
            break;
        case 4:
            character->speed = pocket_clamp_i16(character->speed + (delta * 5), 0, 255);
            break;
        case 5:
        case 6:
            character->initiative_misc =
                (int8_t)pocket_clamp_i16(character->initiative_misc + delta, -20, 20);
            break;
        case 7:
            character->exhaustion = pocket_clamp_u8(character->exhaustion + delta, 6U);
            break;
        case 8:
            character->death_successes =
                pocket_clamp_u8(character->death_successes + delta, 3U);
            break;
        case 9:
            character->death_failures =
                pocket_clamp_u8(character->death_failures + delta, 3U);
            break;
        case 10:
            character->hit_die = pocket_cycle_die(character->hit_die, delta, true);
            break;
        case 11:
            character->hit_dice_current =
                pocket_clamp_u8(character->hit_dice_current + delta, character->hit_dice_max);
            break;
        case 12:
            character->hit_dice_max = pocket_clamp_u8(character->hit_dice_max + delta, 20U);
            if(character->hit_dice_current > character->hit_dice_max)
                character->hit_dice_current = character->hit_dice_max;
            break;
        case 13:
            character->skill_misc[11U] = (int8_t)pocket_clamp_i16(
                character->skill_misc[11U] + delta, -20, 20);
            break;
        case 14:
            character->skill_misc[6U] = (int8_t)pocket_clamp_i16(
                character->skill_misc[6U] + delta, -20, 20);
            break;
        case 15:
            character->skill_misc[8U] = (int8_t)pocket_clamp_i16(
                character->skill_misc[8U] + delta, -20, 20);
            break;
        default:
            return;
        }
        pocket_save(app, false);
    } else if(event->type == InputTypeLong && event->key == InputKeyOk) {
        const char* header = "Numeric value";
        int32_t value = 0;
        int32_t minimum = 0;
        int32_t maximum = 999;
        switch(app->selection) {
        case 0U: header = "Current HP"; value = character->hp_current; break;
        case 1U: header = "Maximum HP"; value = character->hp_max; minimum = 1; break;
        case 2U: header = "Temporary HP"; value = character->hp_temporary; break;
        case 3U: header = "Armor Class"; value = character->armor_class; maximum = 99; break;
        case 4U: header = "Speed in feet"; value = character->speed; maximum = 255; break;
        case 5U:
        case 6U:
            header = "Initiative misc";
            value = character->initiative_misc;
            minimum = -20;
            maximum = 20;
            break;
        case 7U: header = "Exhaustion"; value = character->exhaustion; maximum = 6; break;
        case 8U: header = "Death successes"; value = character->death_successes; maximum = 3; break;
        case 9U: header = "Death failures"; value = character->death_failures; maximum = 3; break;
        case 10U: header = "Hit Point Die"; value = character->hit_die; minimum = 4; maximum = 12; break;
        case 11U:
            header = "Hit Dice current";
            value = character->hit_dice_current;
            maximum = character->hit_dice_max;
            break;
        case 12U: header = "Hit Dice maximum"; value = character->hit_dice_max; maximum = 20; break;
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
        pocket_begin_number(
            app, PocketNumberVitals, (uint8_t)app->selection, 0U,
            header, value, minimum, maximum);
    }
}

static void pocket_handle_abilities(PocketD20App* app, const InputEvent* event) {
    PocketCharacter* character = &app->data.character;
    if(pocket_is_move_event(event) && event->key == InputKeyUp)
        pocket_menu_move(app, POCKET_D20_ABILITY_COUNT, -1);
    else if(pocket_is_move_event(event) && event->key == InputKeyDown)
        pocket_menu_move(app, POCKET_D20_ABILITY_COUNT, 1);
    else if(pocket_is_move_event(event) &&
            (event->key == InputKeyLeft || event->key == InputKeyRight)) {
        int8_t delta = event->key == InputKeyRight ? 1 : -1;
        uint8_t index = app->selection;
        if(app->edit_modifier_mode) {
            character->saving_throw_misc[index] = (int8_t)pocket_clamp_i16(
                character->saving_throw_misc[index] + delta, -20, 20);
        } else {
            character->ability_scores[index] = (int8_t)pocket_clamp_i16(
                character->ability_scores[index] + delta, 1, 30);
        }
        pocket_save(app, false);
    } else if(event->type == InputTypeLong &&
              (event->key == InputKeyLeft || event->key == InputKeyRight)) {
        app->edit_modifier_mode = !app->edit_modifier_mode;
        pocket_set_status(app, app->edit_modifier_mode ? "Editing save misc" : "Editing scores");
    } else if(event->type == InputTypeLong && event->key == InputKeyOk) {
        uint8_t index = (uint8_t)app->selection;
        pocket_begin_number(
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
        pocket_save(app, false);
    }
}

static void pocket_handle_skills(PocketD20App* app, const InputEvent* event) {
    PocketCharacter* character = &app->data.character;
    if(pocket_is_move_event(event) && event->key == InputKeyUp)
        pocket_menu_move(app, POCKET_D20_SKILL_COUNT, -1);
    else if(pocket_is_move_event(event) && event->key == InputKeyDown)
        pocket_menu_move(app, POCKET_D20_SKILL_COUNT, 1);
    else if(pocket_is_move_event(event) &&
            (event->key == InputKeyLeft || event->key == InputKeyRight)) {
        int8_t delta = event->key == InputKeyRight ? 1 : -1;
        uint8_t index = pocket_skill_display_order[app->selection];
        if(app->edit_modifier_mode) {
            character->skill_misc[index] = (int8_t)pocket_clamp_i16(
                character->skill_misc[index] + delta, -20, 20);
        } else {
            int16_t proficiency = character->skill_proficiency[index] + delta;
            if(proficiency < 0) proficiency = PocketProficiencyExpertise;
            if(proficiency > PocketProficiencyExpertise) proficiency = PocketProficiencyNone;
            character->skill_proficiency[index] = (uint8_t)proficiency;
        }
        pocket_save(app, false);
    } else if(event->type == InputTypeLong &&
              (event->key == InputKeyLeft || event->key == InputKeyRight)) {
        app->edit_modifier_mode = !app->edit_modifier_mode;
        pocket_set_status(app, app->edit_modifier_mode ? "Editing skill misc" : "Editing proficiency");
    } else if(event->type == InputTypeLong && event->key == InputKeyOk) {
        uint8_t index = pocket_skill_display_order[app->selection];
        app->edit_modifier_mode = 1U;
        pocket_begin_number(
            app,
            PocketNumberSkill,
            index,
            0U,
            "Skill misc modifier",
            character->skill_misc[index],
            -20,
            20);
    } else if(event->type == InputTypeShort && event->key == InputKeyOk) {
        uint8_t index = pocket_skill_display_order[app->selection];
        character->skill_proficiency[index] =
            (character->skill_proficiency[index] + 1U) % 3U;
        pocket_save(app, false);
    }
}

static __attribute__((unused)) void pocket_handle_builder(PocketD20App* app, const InputEvent* event) {
    PocketCharacter* c = &app->data.character;
    if(pocket_is_move_event(event) && event->key == InputKeyUp)
        pocket_menu_move(app, 11U, -1);
    else if(pocket_is_move_event(event) && event->key == InputKeyDown)
        pocket_menu_move(app, 11U, 1);
    else if(pocket_is_move_event(event) &&
            (event->key == InputKeyLeft || event->key == InputKeyRight) &&
            app->selection == 6U) {
        int16_t next = c->size + (event->key == InputKeyRight ? 1 : -1);
        if(next < 0) next = PocketSizeCount - 1U;
        if(next >= PocketSizeCount) next = 0;
        c->size = (uint8_t)next;
        pocket_save(app, false);
    } else if(event->type == InputTypeLong && event->key == InputKeyOk) {
        if(app->selection == 0U)
            pocket_begin_text(app, PocketEditSpecies, "Custom species", c->species);
        else if(app->selection == 1U)
            pocket_begin_text(app, PocketEditBackground, "Custom background", c->background);
    } else if(event->type == InputTypeShort && event->key == InputKeyOk) {
        switch(app->selection) {
        case 0:
            pocket_open_catalog(app, PocketCatalogSpecies, PocketEditSpecies, c->species);
            break;
        case 1:
            pocket_open_catalog(app, PocketCatalogBackgrounds, PocketEditBackground, c->background);
            break;
        case 2:
            pocket_begin_text(app, PocketEditOriginFeat, "Origin feat", c->origin_feat);
            break;
        case 3:
            pocket_begin_text(app, PocketEditToolProficiencies, "Tool proficiencies", c->tool_proficiencies);
            break;
        case 4:
            pocket_begin_text(app, PocketEditArmorTraining, "Armor training", c->armor_training);
            break;
        case 5:
            pocket_begin_text(app, PocketEditWeaponTraining, "Weapon training", c->weapon_training);
            break;
        case 6:
            c->size = (c->size + 1U) % PocketSizeCount;
            pocket_save(app, false);
            break;
        case 7:
            pocket_begin_text(app, PocketEditSenses, "Senses", c->senses);
            break;
        case 8:
            app->return_screen = PocketScreenBuilder;
            pocket_enter_screen(app, PocketScreenGrantReview);
            break;
        case 9:
        case 10:
            pocket_run_catalog_diagnostics(app);
            pocket_enter_screen(app, PocketScreenCatalogDiagnostics);
            pocket_set_status(app, app->diagnostics_invalid ? "Validation failed" : "Catalogs valid");
            break;
        }
    }
}

static void pocket_handle_grant_review(PocketD20App* app, const InputEvent* event) {
    PocketCharacter* c = &app->data.character;
    uint16_t count = c->grant_count + 2U;
    if(pocket_is_move_event(event) && event->key == InputKeyUp)
        pocket_menu_move(app, count, -1);
    else if(pocket_is_move_event(event) && event->key == InputKeyDown)
        pocket_menu_move(app, count, 1);
    else if(event->type == InputTypeLong && event->key == InputKeyLeft &&
            app->selection && app->selection <= c->grant_count) {
        PocketGrant* grant = &c->grants[app->selection - 1U];
        if(grant->status == PocketGrantPending) grant->status = PocketGrantSkipped;
        pocket_save(app, false);
    } else if(event->type == InputTypeLong && event->key == InputKeyOk &&
              app->selection && app->selection <= c->grant_count) {
        app->record_index = app->selection - 1U;
        pocket_enter_screen(app, PocketScreenGrantEdit);
    } else if(event->type == InputTypeShort && event->key == InputKeyOk) {
        if(app->selection == 0U) {
            for(uint8_t i = 0U; i < c->grant_count; ++i)
                if(c->grants[i].status == PocketGrantPending) pocket_apply_grant(app, &c->grants[i]);
            pocket_set_status(app, "Pending grants applied");
        } else if(app->selection <= c->grant_count) {
            PocketGrant* grant = &c->grants[app->selection - 1U];
            if(grant->status == PocketGrantPending)
                pocket_apply_grant(app, grant);
            else if(grant->status == PocketGrantSkipped)
                grant->status = PocketGrantPending;
        } else if(c->grant_count < POCKET_D20_MAX_GRANTS &&
                  pocket_d20_data_reserve_grants(c, c->grant_count + 1U)) {
            app->record_index = c->grant_count++;
            PocketGrant* grant = &c->grants[app->record_index];
            memset(grant, 0, sizeof(*grant));
            snprintf(grant->stable_id, sizeof(grant->stable_id), "custom_grant_%u",
                     app->record_index + 1U);
            pocket_copy(grant->source, sizeof(grant->source), "Custom");
            pocket_copy(grant->option_name, sizeof(grant->option_name), "Custom Grant");
            pocket_copy(grant->prerequisites, sizeof(grant->prerequisites), "None");
            pocket_copy(grant->grant_value, sizeof(grant->grant_value),
                        "feature=Custom Feature");
            grant->source_type = PocketGrantFeat;
            grant->status = PocketGrantPending;
            pocket_save(app, false);
            pocket_enter_screen(app, PocketScreenGrantEdit);
        }
        pocket_save(app, false);
    }
}

static void pocket_handle_grant_edit(PocketD20App* app, const InputEvent* event) {
    PocketCharacter* c = &app->data.character;
    if(app->record_index >= c->grant_count) return;
    PocketGrant* grant = &c->grants[app->record_index];
    if(pocket_is_move_event(event) && event->key == InputKeyUp)
        pocket_menu_move(app, 10U, -1);
    else if(pocket_is_move_event(event) && event->key == InputKeyDown)
        pocket_menu_move(app, 10U, 1);
    else if(pocket_is_move_event(event) &&
            (event->key == InputKeyLeft || event->key == InputKeyRight)) {
        int16_t delta = event->key == InputKeyRight ? 1 : -1;
        if(app->selection == 2U) {
            int16_t value = grant->source_type + delta;
            if(value < 0) value = PocketGrantSourceCount - 1U;
            if(value >= PocketGrantSourceCount) value = 0;
            grant->source_type = (uint8_t)value;
        } else if(app->selection == 5U) {
            if(!c->class_count) grant->class_index = 0U;
            else {
                int16_t value = grant->class_index + delta;
                if(value < 0) value = c->class_count - 1U;
                if(value >= c->class_count) value = 0;
                grant->class_index = (uint8_t)value;
            }
        } else if(app->selection == 6U)
            grant->level_gained = (uint8_t)pocket_clamp_i16(
                grant->level_gained + delta, 0, 20);
        else if(app->selection == 8U) {
            int16_t value = grant->status + delta;
            if(value < 0) value = PocketGrantSkipped;
            if(value > PocketGrantSkipped) value = PocketGrantPending;
            grant->status = (uint8_t)value;
        } else return;
        pocket_save(app, false);
    } else if(event->type == InputTypeShort && event->key == InputKeyOk) {
        if(app->selection == 0U) pocket_begin_text(app, PocketEditGrantStableId,
            "Stable grant ID", grant->stable_id);
        else if(app->selection == 1U) pocket_begin_text(app, PocketEditGrantSource,
            "Source label", grant->source);
        else if(app->selection == 3U) pocket_begin_text(app, PocketEditGrantOption,
            "Option name", grant->option_name);
        else if(app->selection == 4U) pocket_begin_text(app, PocketEditGrantPrerequisites,
            "Prerequisites", grant->prerequisites);
        else if(app->selection == 7U) pocket_begin_text(app, PocketEditGrantValue,
            "Grant payload key=value", grant->grant_value);
        else if(app->selection == 9U) {
            memmove(&c->grants[app->record_index], &c->grants[app->record_index + 1U],
                    (c->grant_count - app->record_index - 1U) * sizeof(PocketGrant));
            --c->grant_count;
            pocket_save(app, false);
            pocket_enter_screen(app, PocketScreenGrantReview);
        }
    }
}

static __attribute__((unused)) void pocket_handle_catalog_diagnostics(PocketD20App* app, const InputEvent* event) {
    if(event->type == InputTypeShort && event->key == InputKeyOk) {
        pocket_run_catalog_diagnostics(app);
        pocket_set_status(app, app->diagnostics_invalid ? "Validation failed" : "Catalogs valid");
    }
}

static void pocket_handle_spell_filters(PocketD20App* app, const InputEvent* event) {
    PocketCharacter* c = &app->data.character;
    if(pocket_is_move_event(event) && event->key == InputKeyUp)
        pocket_menu_move(app, 6U, -1);
    else if(pocket_is_move_event(event) && event->key == InputKeyDown)
        pocket_menu_move(app, 6U, 1);
    else if(pocket_is_move_event(event) &&
            (event->key == InputKeyLeft || event->key == InputKeyRight)) {
        int8_t delta = event->key == InputKeyRight ? 1 : -1;
        if(app->selection == 0U) {
            int16_t next = app->spell_filter_level + delta;
            if(next < -1) next = 9;
            if(next > 9) next = -1;
            app->spell_filter_level = (int8_t)next;
        } else if(app->selection == 1U) {
            int16_t next = app->spell_filter_class == UINT8_MAX ? -1 : app->spell_filter_class;
            next += delta;
            if(next < -1) next = c->class_count - 1U;
            if(next >= c->class_count) next = -1;
            app->spell_filter_class = next < 0 ? UINT8_MAX : (uint8_t)next;
        } else if(app->selection == 2U)
            app->spell_filter_ritual = !app->spell_filter_ritual;
        else if(app->selection == 3U) {
            int16_t next = app->spell_filter_school + delta;
            if(next < 0) next = 8;
            if(next > 8) next = 0;
            app->spell_filter_school = (uint8_t)next;
        } else if(app->selection == 4U) {
            int16_t next = app->spell_filter_source + delta;
            if(next < 0) next = 2;
            if(next > 2) next = 0;
            app->spell_filter_source = (uint8_t)next;
        } else {
            int16_t next = app->spell_filter_prepared + delta;
            if(next < 0) next = 3;
            if(next > 3) next = 0;
            app->spell_filter_prepared = (uint8_t)next;
        }
    }
}

static void pocket_handle_resources(PocketD20App* app, const InputEvent* event) {
    PocketCharacter* c = &app->data.character;
    if(pocket_is_move_event(event) && event->key == InputKeyUp)
        pocket_menu_move(app, 9U, -1);
    else if(pocket_is_move_event(event) && event->key == InputKeyDown)
        pocket_menu_move(app, 9U, 1);
    else if(pocket_is_move_event(event) &&
            (event->key == InputKeyLeft || event->key == InputKeyRight)) {
        int16_t delta = event->key == InputKeyRight ? 1 : -1;
        if(app->selection == 3U)
            c->encumbrance_mode = !c->encumbrance_mode;
        else if(app->selection == 8U)
            c->carrying_capacity_override = pocket_clamp_i16(c->carrying_capacity_override + delta, 0, 999);
        else
            return;
        pocket_save(app, false);
    } else if(event->type == InputTypeShort && event->key == InputKeyOk) {
        if(app->selection == 6U) {
            c->armor_class = pocket_d20_calculated_armor_class(c);
            pocket_set_status(app, "Armor Class applied");
        } else if(app->selection == 7U) {
            pocket_d20_normalize_currency(c);
            pocket_set_status(app, "Coins normalized");
        } else if(app->selection == 3U) {
            c->encumbrance_mode = !c->encumbrance_mode;
        } else {
            return;
        }
        pocket_save(app, false);
    }
}

static __attribute__((unused)) void pocket_handle_combat_sheet(PocketD20App* app, const InputEvent* event) {
    PocketCharacter* c = &app->data.character;
    if(pocket_is_move_event(event) && event->key == InputKeyUp)
        pocket_menu_move(app, 11U, -1);
    else if(pocket_is_move_event(event) && event->key == InputKeyDown)
        pocket_menu_move(app, 11U, 1);
    else if(event->type == InputTypeShort && event->key == InputKeyOk) {
        switch(app->selection) {
        case 0: pocket_begin_text(app, PocketEditConditions, "Conditions", c->conditions); break;
        case 1: pocket_begin_text(app, PocketEditConcentration, "Concentration", c->concentration); break;
        case 2: c->reaction_available = !c->reaction_available; pocket_save(app, false); break;
        case 3: pocket_begin_text(app, PocketEditTemporaryEffects, "Temporary effects", c->temporary_effects); break;
        case 4: pocket_begin_text(app, PocketEditResistances, "Resistances", c->resistances); break;
        case 5: pocket_begin_text(app, PocketEditImmunities, "Immunities", c->immunities); break;
        case 6: pocket_begin_text(app, PocketEditVulnerabilities, "Vulnerabilities", c->vulnerabilities); break;
        case 7: pocket_begin_text(app, PocketEditSenses, "Senses", c->senses); break;
        case 8: pocket_begin_text(app, PocketEditMovementModes, "Movement modes", c->movement_modes); break;
        case 9: pocket_enter_screen(app, PocketScreenAttackTemplates); break;
        case 10: pocket_enter_screen(app, PocketScreenInitiativeMenu); break;
        }
    }
}

static void pocket_handle_attack_templates(PocketD20App* app, const InputEvent* event) {
    PocketCharacter* c = &app->data.character;
    uint16_t count = c->attack_template_count + 1U;
    if(pocket_is_move_event(event) && event->key == InputKeyUp)
        pocket_menu_move(app, count, -1);
    else if(pocket_is_move_event(event) && event->key == InputKeyDown)
        pocket_menu_move(app, count, 1);
    else if(pocket_is_move_event(event) &&
            app->selection < c->attack_template_count &&
            (event->key == InputKeyLeft || event->key == InputKeyRight)) {
        PocketAttackTemplate* attack = &c->attack_templates[app->selection];
        attack->attack_misc = (int8_t)pocket_clamp_i16(
            attack->attack_misc + (event->key == InputKeyRight ? 1 : -1), -20, 20);
        pocket_save(app, false);
    } else if(event->type == InputTypeLong && event->key == InputKeyOk &&
              app->selection < c->attack_template_count) {
        app->record_index = app->selection;
        pocket_enter_screen(app, PocketScreenAttackTemplateEdit);
    } else if(event->type == InputTypeShort && event->key == InputKeyOk &&
              app->selection == c->attack_template_count) {
        if(c->attack_template_count >= POCKET_D20_MAX_ATTACK_TEMPLATES) {
            pocket_set_status(app, "Template limit reached");
            return;
        }
        app->record_index = c->attack_template_count++;
        PocketAttackTemplate* attack = &c->attack_templates[app->record_index];
        memset(attack, 0, sizeof(*attack));
        pocket_copy(attack->name, sizeof(attack->name), "Custom Attack");
        pocket_copy(attack->damage_type, sizeof(attack->damage_type), "Bludgeoning");
        pocket_copy(attack->rider_type, sizeof(attack->rider_type), "None");
        pocket_copy(attack->mastery, sizeof(attack->mastery), "None");
        attack->type = PocketAttackTemplateCustom;
        attack->ability = PocketAbilityStrength;
        attack->save_ability = PocketAbilityDexterity;
        attack->damage_dice = 1U;
        attack->damage_die = 6U;
        attack->rider_die = 6U;
        pocket_save(app, false);
        pocket_enter_screen(app, PocketScreenAttackTemplateEdit);
    } else if(event->type == InputTypeShort && event->key == InputKeyOk &&
              app->selection < c->attack_template_count) {
        PocketAttackTemplate* attack = &c->attack_templates[app->selection];
        app->dice_count = attack->damage_dice ? attack->damage_dice : 1U;
        app->dice_sides = attack->damage_die >= 2U ? attack->damage_die : 1U;
        app->dice_modifier = attack->attack_misc;
        app->roll_mode = PocketRollNormal;
        pocket_roll_generic(app);
    }
}

static void pocket_handle_attack_template_edit(PocketD20App* app, const InputEvent* event) {
    PocketCharacter* c = &app->data.character;
    if(app->record_index >= c->attack_template_count) return;
    PocketAttackTemplate* attack = &c->attack_templates[app->record_index];
    if(pocket_is_move_event(event) && event->key == InputKeyUp)
        pocket_menu_move(app, 15U, -1);
    else if(pocket_is_move_event(event) && event->key == InputKeyDown)
        pocket_menu_move(app, 15U, 1);
    else if(pocket_is_move_event(event) &&
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
            attack->attack_misc = (int8_t)pocket_clamp_i16(attack->attack_misc + delta, -20, 20);
        else if(app->selection == 5U)
            attack->save_dc = (uint8_t)pocket_clamp_i16(attack->save_dc + delta, 0, 30);
        else if(app->selection == 6U)
            attack->damage_dice = (uint8_t)pocket_clamp_i16(attack->damage_dice + delta, 0, 20);
        else if(app->selection == 7U)
            attack->damage_die = pocket_cycle_die(attack->damage_die, delta, false);
        else if(app->selection == 10U)
            attack->rider_dice = (uint8_t)pocket_clamp_i16(attack->rider_dice + delta, 0, 20);
        else if(app->selection == 11U)
            attack->rider_die = pocket_cycle_die(attack->rider_die, delta, false);
        else if(app->selection == 13U) {
            int16_t value = attack->recharge + delta;
            if(value < 0) value = PocketRechargeCount - 1U;
            if(value >= PocketRechargeCount) value = 0;
            attack->recharge = (uint8_t)value;
        } else return;
        pocket_save(app, false);
    } else if(event->type == InputTypeShort && event->key == InputKeyOk) {
        if(app->selection == 0U) pocket_begin_text(app, PocketEditAttackName,
            "Attack template name", attack->name);
        else if(app->selection == 8U) pocket_begin_text(app, PocketEditAttackDamageType,
            "Damage type", attack->damage_type);
        else if(app->selection == 9U) pocket_begin_text(app, PocketEditAttackMastery,
            "Mastery property", attack->mastery);
        else if(app->selection == 12U) pocket_begin_text(app, PocketEditAttackRiderType,
            "Rider type", attack->rider_type);
        else if(app->selection == 14U) {
            memmove(&c->attack_templates[app->record_index],
                    &c->attack_templates[app->record_index + 1U],
                    (c->attack_template_count - app->record_index - 1U) *
                        sizeof(PocketAttackTemplate));
            --c->attack_template_count;
            pocket_save(app, false);
            pocket_enter_screen(app, PocketScreenAttackTemplates);
        }
    }
}

static void pocket_handle_campaigns(PocketD20App* app, const InputEvent* event) {
    uint16_t count = app->campaign_count + 1U;
    if(pocket_is_move_event(event) && event->key == InputKeyUp)
        pocket_menu_move(app, count, -1);
    else if(pocket_is_move_event(event) && event->key == InputKeyDown)
        pocket_menu_move(app, count, 1);
    else if(event->type == InputTypeShort && event->key == InputKeyOk) {
        if(app->selection == app->campaign_count) {
            pocket_campaign_diagnose(app->storage, &app->campaign_diagnostics);
            pocket_enter_screen(app, PocketScreenCampaignDiagnostics);
            return;
        }
        PocketCampaignSummary next;
        if(!pocket_campaign_at(app->storage, app->selection, &next)) {
            pocket_set_status(app, "Campaign record invalid");
            return;
        }
        if(next.pack_version != POCKET_CAMPAIGN_PACK_VERSION ||
           next.minimum_app > POCKET_CAMPAIGN_APP_VERSION ||
           (next.maximum_app && next.maximum_app < POCKET_CAMPAIGN_APP_VERSION)) {
            pocket_set_status(app, "Campaign incompatible");
            return;
        }
        if(app->campaign_active_valid) pocket_campaign_save_active_progress(app);
        pocket_campaign_progress_load(
            app->storage, app->profiles.active_profile, &next, &app->data.character);
        app->campaign_active = next;
        app->campaign_active_valid = 1U;
        if(pocket_adventure_load(app)) {
            pocket_save(app, false);
            pocket_enter_screen(app, PocketScreenAdventure);
        } else {
            pocket_set_status(app, "Campaign scene invalid");
        }
    }
}

static void pocket_handle_campaign_diagnostics(PocketD20App* app, const InputEvent* event) {
    if(pocket_is_move_event(event) && event->key == InputKeyUp)
        pocket_menu_move(app, 10U, -1);
    else if(pocket_is_move_event(event) && event->key == InputKeyDown)
        pocket_menu_move(app, 10U, 1);
    else if(event->type == InputTypeShort && event->key == InputKeyOk) {
        pocket_campaign_diagnose(app->storage, &app->campaign_diagnostics);
        pocket_set_status(app,
            app->campaign_diagnostics.incompatible ||
            app->campaign_diagnostics.missing_scene_files ||
            app->campaign_diagnostics.duplicate_campaign_ids ||
            app->campaign_diagnostics.duplicate_scene_ids ||
            app->campaign_diagnostics.missing_entry_scenes ||
            app->campaign_diagnostics.broken_links ? "Pack needs attention" : "Campaign packs OK");
    }
}

static void pocket_handle_adventure(PocketD20App* app, const InputEvent* event) {
    PocketAdventureScene* scene = app->adventure_scene;
    if(!scene || !scene->choice_count) return;
    if(pocket_is_move_event(event) && event->key == InputKeyUp) {
        if(app->selection == 0U)
            app->selection = scene->choice_count - 1U;
        else
            --app->selection;
        if(app->selection < app->scroll) app->scroll = app->selection;
        if(app->selection >= app->scroll + 2U) app->scroll = app->selection - 1U;
    } else if(pocket_is_move_event(event) && event->key == InputKeyDown) {
        app->selection = (app->selection + 1U) % scene->choice_count;
        if(app->selection < app->scroll) app->scroll = app->selection;
        if(app->selection >= app->scroll + 2U) app->scroll = app->selection - 1U;
    } else if(event->type == InputTypeLong && event->key == InputKeyOk) {
        pocket_copy(
            app->data.character.adventure_checkpoint,
            sizeof(app->data.character.adventure_checkpoint),
            app->data.character.adventure_scene);
        pocket_save(app, false);
        pocket_campaign_save_active_progress(app);
        pocket_set_status(app, "Checkpoint saved");
    } else if(event->type == InputTypeLong && event->key == InputKeyLeft) {
        pocket_copy(
            app->data.character.adventure_scene,
            sizeof(app->data.character.adventure_scene),
            app->data.character.adventure_checkpoint);
        if(pocket_adventure_load(app)) {
            app->selection = 0U;
            app->scroll = 0U;
            pocket_save(app, false);
            pocket_campaign_save_active_progress(app);
            pocket_set_status(app, "Checkpoint loaded");
        }
    } else if(event->type == InputTypeLong && event->key == InputKeyRight) {
        pocket_copy(
            app->data.character.adventure_scene,
            sizeof(app->data.character.adventure_scene),
            app->campaign_active.entry_scene);
        if(pocket_adventure_load(app)) {
            app->selection = 0U;
            app->scroll = 0U;
            pocket_save(app, false);
            pocket_campaign_save_active_progress(app);
            pocket_set_status(app, "Adventure restarted");
        }
    } else if(event->type == InputTypeShort && event->key == InputKeyOk) {
        PocketAdventureChoice choice = scene->choices[app->selection];
        uint8_t natural = 0U;
        int8_t modifier = 0;
        bool passed = true;
        if(choice.skill >= 0 && (uint8_t)choice.skill < POCKET_D20_SKILL_COUNT) {
            natural = (uint8_t)pocket_d20_roll_dice(1U, 20U);
            modifier = pocket_d20_skill_modifier(
                &app->data.character, (uint8_t)choice.skill);
            app->adventure_last_natural = natural;
            app->adventure_last_total = (int16_t)natural + modifier;
            passed = app->adventure_last_total >= choice.dc;
        }
        bool first_reward = choice.quest_flag >= 32U ||
                            !(app->data.character.adventure_quest_flags &
                              (1UL << choice.quest_flag));
        if(passed && first_reward) {
            pocket_adventure_reward_item(&app->data.character, choice.reward_item);
            pocket_adventure_reward_milestone(&app->data.character, choice.milestone);
            if(choice.quest_flag < 32U)
                app->data.character.adventure_quest_flags |= 1UL << choice.quest_flag;
            if(choice.achievement < 32U)
                app->data.character.adventure_achievements |= 1UL << choice.achievement;
        }
        const char* next = passed ? choice.success_scene : choice.failure_scene;
        char previous[POCKET_D20_SHORT_LEN];
        pocket_copy(previous, sizeof(previous), app->data.character.adventure_scene);
        if(next[0] && strcmp(next, "-") != 0)
            pocket_copy(
                app->data.character.adventure_scene,
                sizeof(app->data.character.adventure_scene),
                next);
        if(!pocket_adventure_load(app)) {
            pocket_copy(
                app->data.character.adventure_scene,
                sizeof(app->data.character.adventure_scene),
                previous);
            pocket_adventure_load(app);
            pocket_set_status(app, "Next scene missing");
            return;
        }
        app->selection = 0U;
        app->scroll = 0U;
        pocket_save(app, false);
        pocket_campaign_save_active_progress(app);
        if(choice.skill >= 0) {
            char result[32];
            snprintf(
                result,
                sizeof(result),
                "d20 %u%+d=%d %s",
                natural,
                modifier,
                app->adventure_last_total,
                passed ? "pass" : "fail");
            pocket_set_status(app, result);
            pocket_start_dice_animation(app, 1U, 20U);
        } else {
            pocket_set_status(app, "Choice applied");
        }
    }
}

static void pocket_handle_magic(PocketD20App* app, const InputEvent* event) {
    PocketCharacter* character = &app->data.character;
    if(pocket_is_move_event(event) && event->key == InputKeyUp)
        pocket_menu_move(app, 17U, -1);
    else if(pocket_is_move_event(event) && event->key == InputKeyDown)
        pocket_menu_move(app, 17U, 1);
    else if(pocket_is_move_event(event) &&
            (event->key == InputKeyLeft || event->key == InputKeyRight)) {
        int8_t delta = event->key == InputKeyRight ? 1 : -1;
        if(app->arcane_recovery_active) {
            if(app->selection < 7U || app->selection > 11U) {
                pocket_set_status(app, "Choose a level 1-5 slot");
                return;
            }
            uint8_t level = app->selection - 6U;
            if(delta > 0) {
                uint8_t remaining = app->arcane_recovery_budget - app->arcane_recovery_spent;
                if(level > remaining) {
                    pocket_set_status(app, "Not enough recovery");
                    return;
                }
                if(character->spell_slots_current[level] >=
                   character->spell_slots_max[level]) {
                    pocket_set_status(app, "That slot level is full");
                    return;
                }
                ++character->spell_slots_current[level];
                ++app->arcane_recovery_restored[level];
                app->arcane_recovery_spent += level;
                character->arcane_recovery_used = 1U;
            } else {
                if(!app->arcane_recovery_restored[level]) {
                    pocket_set_status(app, "Nothing to undo here");
                    return;
                }
                --character->spell_slots_current[level];
                --app->arcane_recovery_restored[level];
                app->arcane_recovery_spent -= level;
                if(!app->arcane_recovery_spent) character->arcane_recovery_used = 0U;
            }
            pocket_save(app, false);
            pocket_set_status(app, "<> recover, row 6 done");
        } else if(app->selection == 1U) {
            int16_t ability = character->spellcasting_ability + delta;
            if(ability < 0) ability = PocketAbilityCharisma;
            if(ability > PocketAbilityCharisma) ability = PocketAbilityStrength;
            character->spellcasting_ability = (uint8_t)ability;
        } else if(app->selection == 3U) {
            character->spell_attack_misc = (int8_t)pocket_clamp_i16(
                character->spell_attack_misc + delta, -20, 20);
        } else if(app->selection == 4U) {
            character->spell_save_misc = (int8_t)pocket_clamp_i16(
                character->spell_save_misc + delta, -20, 20);
        } else if(app->selection == 5U) {
            app->edit_slot_max = !app->edit_slot_max;
            return;
        } else if(app->selection >= 7U && app->selection <= 15U) {
            uint8_t level = app->selection - 6U;
            uint8_t* slots = app->edit_slot_max ? character->spell_slots_max :
                                                   character->spell_slots_current;
            slots[level] = pocket_clamp_u8(slots[level] + delta, 20U);
            if(character->spell_slots_current[level] > character->spell_slots_max[level])
                character->spell_slots_current[level] = character->spell_slots_max[level];
        } else {
            return;
        }
        pocket_save(app, false);
    } else if(event->type == InputTypeLong && event->key == InputKeyOk) {
        if(app->arcane_recovery_active) {
            pocket_set_status(app, "Finish recovery first");
        } else if(app->selection == 0U) {
            pocket_enter_screen(app, PocketScreenSpellFilters);
            pocket_set_status(app, "Filters affect next picker");
        } else if(app->selection == 2U) {
            pocket_d20_recalculate_multiclass_slots(character);
            pocket_save(app, false);
            pocket_set_status(app, "Multiclass slots recalculated");
        } else if(app->selection == 3U || app->selection == 4U) {
            bool attack = app->selection == 3U;
            pocket_begin_number(
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
            uint8_t value = app->edit_slot_max ? character->spell_slots_max[level] :
                                                character->spell_slots_current[level];
            pocket_begin_number(
                app,
                PocketNumberMagic,
                (uint8_t)app->selection,
                app->edit_slot_max,
                app->edit_slot_max ? "Maximum spell slots" : "Current spell slots",
                value,
                0,
                20);
        }
    } else if(event->type == InputTypeShort && event->key == InputKeyOk) {
        if(app->selection == 0U)
            pocket_open_list(app, PocketListSpells, PocketScreenMagic);
        else if(app->selection == 5U)
            app->edit_slot_max = !app->edit_slot_max;
        else if(app->selection == 6U) {
            if(app->arcane_recovery_active) {
                app->arcane_recovery_active = 0U;
                pocket_set_status(
                    app,
                    app->arcane_recovery_spent ? "Arcane Recovery used" :
                                                 "Recovery skipped");
            } else if(character->arcane_recovery_used) {
                pocket_set_status(app, "Recovery already used");
            } else {
                pocket_set_status(app, "Finish a Short Rest first");
            }
        } else if(app->selection == 16U) {
            app->arcane_recovery_active = 0U;
            pocket_enter_screen(app, PocketScreenHome);
        }
    }
}

static void pocket_handle_currency(PocketD20App* app, const InputEvent* event) {
    PocketCharacter* character = &app->data.character;
    int32_t* values[5] = {
        &character->currency_cp,
        &character->currency_sp,
        &character->currency_ep,
        &character->currency_gp,
        &character->currency_pp,
    };
    if(pocket_is_move_event(event) && event->key == InputKeyUp)
        pocket_menu_move(app, 5U, -1);
    else if(pocket_is_move_event(event) && event->key == InputKeyDown)
        pocket_menu_move(app, 5U, 1);
    else if(pocket_is_move_event(event) &&
            (event->key == InputKeyLeft || event->key == InputKeyRight)) {
        int64_t next = *values[app->selection] +
                       (event->key == InputKeyRight ? 1 : -1);
        if(next < 0) next = 0;
        if(next > 999999999L) next = 999999999L;
        *values[app->selection] = (int32_t)next;
        pocket_save(app, false);
    } else if(event->type == InputTypeShort && event->key == InputKeyOk) {
        static const char* const names[] = {
            "Copper pieces", "Silver pieces", "Electrum pieces", "Gold pieces", "Platinum pieces"};
        pocket_begin_number(
            app,
            PocketNumberCurrency,
            (uint8_t)app->selection,
            0U,
            names[app->selection],
            *values[app->selection],
            0,
            999999999L);
    }
}

static void pocket_handle_record_list(PocketD20App* app, const InputEvent* event) {
    uint16_t count = pocket_list_count(app) + 1U;
    if(pocket_is_move_event(event) && event->key == InputKeyUp)
        pocket_menu_move(app, count, -1);
    else if(pocket_is_move_event(event) && event->key == InputKeyDown)
        pocket_menu_move(app, count, 1);
    else if(event->type == InputTypeShort && event->key == InputKeyOk) {
        if(app->selection == 0U) {
            if(!pocket_add_record(app)) pocket_set_status(app, "List is full");
        } else {
            app->record_index = app->selection - 1U;
            pocket_enter_screen(app, PocketScreenRecordDetail);
        }
    }
}

static void pocket_toggle_item_property(PocketItem* item, uint16_t property) {
    item->weapon_properties ^= property;
}

static void pocket_adjust_record(PocketD20App* app, int8_t delta) {
    PocketCharacter* character = &app->data.character;
    uint8_t index = app->record_index;
    uint8_t field = app->selection;
    switch(app->list_kind) {
    case PocketListClasses: {
        PocketClassLevel* class_level = &character->classes[index];
        if(field == 2U) {
            uint8_t total = pocket_d20_total_level(character);
            int16_t maximum = 20 - (total - class_level->level);
            class_level->level = (uint8_t)pocket_clamp_i16(
                class_level->level + delta, 1, maximum < 1 ? 1 : maximum);
        } else if(field == 3U) {
            class_level->hit_die = pocket_cycle_die(class_level->hit_die, delta, true);
        } else if(field == 4U) {
            class_level->hit_dice_current = pocket_clamp_u8(
                class_level->hit_dice_current + delta, class_level->hit_dice_max);
        } else if(field == 5U) {
            class_level->hit_dice_max = pocket_clamp_u8(class_level->hit_dice_max + delta, 20U);
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
            class_level->cantrip_limit = pocket_clamp_u8(class_level->cantrip_limit + delta, 30U);
        } else if(field == 9U) {
            class_level->prepared_limit = pocket_clamp_u8(class_level->prepared_limit + delta, 50U);
        } else if(field == 10U) {
            class_level->spellbook_size = (uint16_t)pocket_clamp_i16(class_level->spellbook_size + delta, 0, 999);
        } else if(field == 11U) {
            class_level->pact_slot_level = pocket_clamp_u8(class_level->pact_slot_level + delta, 5U);
        } else if(field == 12U) {
            class_level->pact_slots_current = pocket_clamp_u8(
                class_level->pact_slots_current + delta, class_level->pact_slots_max);
        } else if(field == 13U) {
            class_level->pact_slots_max = pocket_clamp_u8(class_level->pact_slots_max + delta, 8U);
            if(class_level->pact_slots_current > class_level->pact_slots_max)
                class_level->pact_slots_current = class_level->pact_slots_max;
        } else if(field == 14U) {
            uint8_t level = delta > 0 ? 6U : 9U;
            class_level->mystic_arcanum_mask ^= (uint16_t)(1U << level);
        } else if(field == 15U) {
            class_level->spell_points_current = (uint16_t)pocket_clamp_i16(
                class_level->spell_points_current + delta, 0, class_level->spell_points_max);
        } else if(field == 16U) {
            class_level->spell_points_max = (uint16_t)pocket_clamp_i16(
                class_level->spell_points_max + delta, 0, 999);
            if(class_level->spell_points_current > class_level->spell_points_max)
                class_level->spell_points_current = class_level->spell_points_max;
        } else {
            return;
        }
        break;
    }
    case PocketListSpells: {
        PocketSpell* spell = &character->spells[index];
        if(field == 2U) {
            int16_t class_index = spell->class_index + delta;
            if(class_index < 0) class_index = character->class_count - 1U;
            if(class_index >= character->class_count) class_index = 0;
            spell->class_index = (uint8_t)class_index;
        } else if(field == 3U)
            spell->level = pocket_clamp_u8(spell->level + delta, 9U);
        else if(field == 4U) {
            character->spell_known[index] = !character->spell_known[index];
            if(!character->spell_known[index]) {
                spell->prepared = 0U;
                character->spell_always_prepared[index] = 0U;
            }
        } else if(field == 5U) {
            spell->prepared = !spell->prepared;
            if(spell->prepared) character->spell_known[index] = 1U;
        } else if(field == 6U) {
            character->spell_always_prepared[index] =
                !character->spell_always_prepared[index];
            if(character->spell_always_prepared[index])
                character->spell_known[index] = 1U;
        } else if(field == 7U) {
            spell->ritual = !spell->ritual;
        } else if(field == 8U) {
            character->spell_free_casts_current[index] = pocket_clamp_u8(
                character->spell_free_casts_current[index] + delta,
                character->spell_free_casts_max[index]);
        } else if(field == 9U) {
            character->spell_free_casts_max[index] = pocket_clamp_u8(
                character->spell_free_casts_max[index] + delta, 20U);
            if(character->spell_free_casts_current[index] >
               character->spell_free_casts_max[index])
                character->spell_free_casts_current[index] =
                    character->spell_free_casts_max[index];
        } else if(field == 15U) {
            int16_t source = spell->grant_source + delta;
            if(source < 0) source = PocketGrantSourceCount - 1U;
            if(source >= PocketGrantSourceCount) source = 0;
            spell->grant_source = (uint8_t)source;
        } else {
            return;
        }
        break;
    }
    case PocketListFeatures: {
        PocketFeature* feature = &character->features[index];
        if(field == 2U) {
            int16_t class_index = feature->class_index + delta;
            if(class_index < 0) class_index = character->class_count - 1U;
            if(class_index >= character->class_count) class_index = 0;
            feature->class_index = (uint8_t)class_index;
        } else if(field == 3U) {
            feature->class_level_gained =
                pocket_clamp_u8(feature->class_level_gained + delta, 20U);
        } else if(field == 4U) {
            int16_t before = feature->uses_current;
            feature->uses_current =
                pocket_clamp_i16(feature->uses_current + delta, 0, feature->uses_max);
            if(app->data.initiative.active && feature->uses_current != before)
                pocket_history_push(
                    app,
                    PocketHistoryFeatureResource,
                    index,
                    before,
                    feature->uses_current);
        } else if(field == 5U) {
            feature->uses_max = pocket_clamp_i16(feature->uses_max + delta, 0, 99);
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
            feature->uses_max = pocket_d20_feature_max_uses(character, feature);
        } else if(field == 8U) {
            int16_t ability = feature->resource_ability + delta;
            if(ability < 0) ability = PocketAbilityCharisma;
            if(ability > PocketAbilityCharisma) ability = PocketAbilityStrength;
            feature->resource_ability = (uint8_t)ability;
            feature->uses_max = pocket_d20_feature_max_uses(character, feature);
        } else {
            return;
        }
        break;
    }
    case PocketListItems: {
        PocketItem* item = &character->items[index];
        switch(field) {
        case 2:
            item->quantity = pocket_clamp_i16(item->quantity + delta, 0, 999);
            break;
        case 3:
            item->weight_tenths = pocket_clamp_i16(item->weight_tenths + delta, 0, 9999);
            break;
        case 4:
            item->equipped = !item->equipped;
            break;
        case 5:
            item->attuned = !item->attuned;
            break;
        case 6:
            item->is_weapon = !item->is_weapon;
            break;
        case 7: {
            int16_t ability = item->attack_ability + delta;
            if(ability < 0) ability = PocketAttackAbilityBest;
            if(ability > PocketAttackAbilityBest) ability = PocketAttackAbilityAuto;
            item->attack_ability = (uint8_t)ability;
            break;
        }
        case 8:
            item->proficient = !item->proficient;
            break;
        case 9:
            item->magic_bonus = (int8_t)pocket_clamp_i16(item->magic_bonus + delta, -10, 10);
            break;
        case 10:
            item->damage_dice = pocket_clamp_u8(item->damage_dice + delta, 20U);
            break;
        case 11:
            item->damage_die = pocket_cycle_die(item->damage_die, delta, true);
            break;
        case 12:
            if(item->versatile_die)
                item->versatile_die = 0U;
            else
                item->versatile_die = pocket_cycle_die(item->damage_die, 1, true);
            break;
        case 13:
            item->versatile_die = pocket_cycle_die(
                item->versatile_die ? item->versatile_die : item->damage_die,
                delta,
                true);
            break;
        case 14:
            item->use_versatile = !item->use_versatile;
            break;
        case 15: {
            int16_t type = item->damage_type + delta;
            if(type < 0) type = PocketDamageTypeCount - 1U;
            if(type >= PocketDamageTypeCount) type = 0;
            item->damage_type = (uint8_t)type;
            break;
        }
        case 16:
            pocket_toggle_item_property(item, PocketWeaponFinesse);
            break;
        case 17:
            pocket_toggle_item_property(item, PocketWeaponRanged);
            break;
        case 18:
            pocket_toggle_item_property(item, PocketWeaponLight);
            break;
        case 19:
            pocket_toggle_item_property(item, PocketWeaponHeavy);
            break;
        case 20:
            pocket_toggle_item_property(item, PocketWeaponThrown);
            break;
        case 21:
            pocket_toggle_item_property(item, PocketWeaponAmmunition);
            break;
        case 22:
            item->add_ability_damage = !item->add_ability_damage;
            break;
        case 23:
            item->extra_dice = pocket_clamp_u8(item->extra_dice + delta, 20U);
            break;
        case 24:
            item->extra_die = pocket_cycle_die(item->extra_die, delta, true);
            break;
        case 25:
            item->ammo_current = pocket_clamp_i16(item->ammo_current + delta, 0, item->ammo_max);
            break;
        case 26:
            item->ammo_max = pocket_clamp_i16(item->ammo_max + delta, 0, 999);
            if(item->ammo_current > item->ammo_max) item->ammo_current = item->ammo_max;
            break;
        case 28:
            item->container_index = (int8_t)pocket_clamp_i16(
                item->container_index + delta, -1, character->item_count - 1U);
            if(item->container_index == (int8_t)index) item->container_index = -1;
            break;
        case 29:
            item->charges_current = pocket_clamp_i16(item->charges_current + delta, 0, item->charges_max);
            break;
        case 30:
            item->charges_max = pocket_clamp_i16(item->charges_max + delta, 0, 999);
            if(item->charges_current > item->charges_max) item->charges_current = item->charges_max;
            break;
        case 31:
            item->armor_base = pocket_clamp_u8(item->armor_base + delta, 30U);
            break;
        case 32:
            item->armor_dex_cap = (int8_t)pocket_clamp_i16(item->armor_dex_cap + delta, -1, 9);
            break;
        case 33:
            item->shield_bonus = pocket_clamp_u8(item->shield_bonus + delta, 10U);
            break;
        default:
            return;
        }
        break;
    }
    case PocketListJournal: {
        PocketJournalEntry* entry = &character->journal[index];
        if(field == 0U) {
            int16_t category = entry->category + delta;
            if(category < 0) category = PocketJournalCategoryCount - 1U;
            if(category >= PocketJournalCategoryCount) category = 0;
            entry->category = (uint8_t)category;
        } else if(field == 3U) {
            entry->completed = !entry->completed;
        } else if(field == 4U) {
            int16_t class_index = entry->class_index + delta;
            if(class_index < 0) class_index = character->class_count - 1U;
            if(class_index >= character->class_count) class_index = 0;
            entry->class_index = (uint8_t)class_index;
        } else {
            return;
        }
        break;
    }
    case PocketListParty:
        if(field == 1U) {
            app->data.party[index].initiative_modifier = (int8_t)pocket_clamp_i16(
                app->data.party[index].initiative_modifier + delta, -50, 50);
        } else if(field == 2U) {
            app->data.party[index].armor_class = pocket_clamp_i16(
                app->data.party[index].armor_class + delta, 0, 99);
        } else if(field == 3U) {
            app->data.party[index].hp_current = pocket_clamp_i16(
                app->data.party[index].hp_current + delta, -999, 999);
        } else if(field == 4U) {
            app->data.party[index].hp_max = pocket_clamp_i16(
                app->data.party[index].hp_max + delta, 0, 999);
        } else {
            return;
        }
        break;
    case PocketListLanguages:
        return;
    }
    pocket_save(app, false);
}

static void pocket_create_item_from_journal(PocketD20App* app) {
    PocketCharacter* character = &app->data.character;
    if(character->item_count >= POCKET_D20_MAX_ITEMS) {
        pocket_set_status(app, "Inventory is full");
        return;
    }
    if(!pocket_d20_data_reserve_items(character, character->item_count + 1U)) {
        pocket_set_status(app, "Inventory memory low");
        return;
    }
    PocketJournalEntry* entry = &character->journal[app->record_index];
    uint8_t item_index = character->item_count++;
    PocketItem* item = &character->items[item_index];
    memset(item, 0, sizeof(*item));
    pocket_copy(item->name, sizeof(item->name), entry->title);
    pocket_copy(item->detail, sizeof(item->detail), entry->body);
    item->quantity = 1;
    item->damage_dice = 1U;
    item->damage_die = 6U;
    item->extra_die = 6U;
    item->add_ability_damage = 1U;
    item->container_index = -1;
    item->armor_dex_cap = -1;
    app->list_kind = PocketListItems;
    app->record_index = item_index;
    pocket_save(app, false);
    pocket_enter_screen(app, PocketScreenRecordDetail);
}

static void pocket_apply_milestone_level(PocketD20App* app) {
    PocketCharacter* character = &app->data.character;
    PocketJournalEntry* entry = &character->journal[app->record_index];
    if(entry->category != PocketJournalMilestone) {
        pocket_set_status(app, "Set category Milestone");
        return;
    }
    if(entry->level_granted) {
        pocket_set_status(app, "Level already applied");
        return;
    }
    if(pocket_d20_total_level(character) >= 20U) {
        pocket_set_status(app, "Maximum total level");
        return;
    }
    if(entry->class_index >= character->class_count) entry->class_index = 0U;
    ++character->classes[entry->class_index].level;
    ++character->classes[entry->class_index].hit_dice_max;
    ++character->classes[entry->class_index].hit_dice_current;
    entry->completed = 1U;
    entry->level_granted = 1U;
    character->hit_dice_max = pocket_clamp_u8(character->hit_dice_max + 1, 20U);
    pocket_save(app, false);
    pocket_set_status(app, "Class level increased");
}

static void pocket_handle_record_detail_ok(PocketD20App* app) {
    PocketCharacter* character = &app->data.character;
    uint8_t index = app->record_index;
    uint8_t field = app->selection;
    switch(app->list_kind) {
    case PocketListClasses:
        if(field == 0U)
            pocket_open_catalog(
                app,
                PocketCatalogClasses,
                PocketEditClassName,
                character->classes[index].name);
        else if(field == 1U)
            pocket_open_catalog(
                app,
                PocketCatalogSubclasses,
                PocketEditSubclass,
                character->classes[index].subclass);
        else if(field >= 2U && field <= 16U)
            pocket_adjust_record(app, 1);
        else
            pocket_delete_record(app);
        break;
    case PocketListSpells:
        if(field == 0U)
            pocket_open_catalog(
                app,
                PocketCatalogSpells,
                PocketEditSpellName,
                character->spells[index].name);
        else if(field == 1U)
            pocket_begin_text(app, PocketEditSpellDetail, "Spell notes", character->spells[index].detail);
        else if(field < 10U)
            pocket_adjust_record(app, 1);
        else if(field == 10U) {
            if(character->spell_free_casts_current[index]) {
                --character->spell_free_casts_current[index];
                pocket_save(app, false);
                pocket_set_status(app, "Free cast used");
            } else {
                pocket_set_status(app, "No free casts left");
            }
        } else if(field == 11U)
            pocket_begin_text(app, PocketEditSpellStableId, "Stable ID", character->spells[index].stable_id);
        else if(field == 12U)
            pocket_begin_text(app, PocketEditSpellSource, "Spell source", character->spells[index].source);
        else if(field == 13U)
            pocket_begin_text(app, PocketEditSpellSchool, "Spell school", character->spells[index].school);
        else if(field == 14U)
            pocket_begin_text(app, PocketEditSpellGrantName, "Grant source name", character->spells[index].grant_name);
        else if(field == 15U)
            pocket_adjust_record(app, 1);
        else {
            pocket_delete_record(app);
        }
        break;
    case PocketListFeatures:
        if(field == 0U)
            pocket_open_catalog(
                app,
                PocketCatalogFeats,
                PocketEditFeatureName,
                character->features[index].name);
        else if(field == 1U)
            pocket_begin_text(app, PocketEditFeatureDetail, "Feature notes", character->features[index].detail);
        else if(field < 9U)
            pocket_adjust_record(app, 1);
        else
            pocket_delete_record(app);
        break;
    case PocketListItems:
        if(field == 0U)
            pocket_open_catalog(
                app,
                PocketCatalogItems,
                PocketEditItemName,
                character->items[index].name);
        else if(field == 1U)
            pocket_begin_text(app, PocketEditItemDetail, "Item notes", character->items[index].detail);
        else if((field >= 2U && field <= 26U) || (field >= 28U && field <= 33U))
            pocket_adjust_record(app, 1);
        else if(field == 34U)
            pocket_begin_text(
                app,
                PocketEditItemAmmoGroup,
                "Ammunition group",
                character->items[index].ammunition_group);
        else if(field == 35U)
            pocket_delete_record(app);
        break;
    case PocketListLanguages:
        if(field == 0U)
            pocket_begin_text(
                app,
                PocketEditLanguageName,
                "Language",
                character->languages[index]);
        else
            pocket_delete_record(app);
        break;
    case PocketListJournal:
        if(field == 0U || field == 3U || field == 4U)
            pocket_adjust_record(app, 1);
        else if(field == 1U)
            pocket_begin_text(
                app,
                PocketEditJournalTitle,
                "Journal title",
                character->journal[index].title);
        else if(field == 2U)
            pocket_begin_text(
                app,
                PocketEditJournalBody,
                "Journal note",
                character->journal[index].body);
        else if(field == 5U)
            pocket_apply_milestone_level(app);
        else if(field == 6U)
            pocket_create_item_from_journal(app);
        else
            pocket_delete_record(app);
        break;
    case PocketListParty:
        if(field == 0U)
            pocket_begin_text(
                app,
                PocketEditPartyName,
                "Party member",
                app->data.party[index].name);
        else if(field >= 1U && field <= 4U)
            pocket_adjust_record(app, 1);
        else
            pocket_delete_record(app);
        break;
    }
}

static bool pocket_begin_record_number(PocketD20App* app) {
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
            maximum = 20 - (pocket_d20_total_level(character) - level->level);
            if(maximum < 1) maximum = 1;
            minimum = 1;
            break;
        case 3U: header = "Hit Point Die"; value = level->hit_die; minimum = 4; maximum = 12; break;
        case 4U: header = "Hit Dice current"; value = level->hit_dice_current; maximum = level->hit_dice_max; break;
        case 5U: header = "Hit Dice maximum"; value = level->hit_dice_max; maximum = 20; break;
        case 8U: header = "Cantrip limit"; value = level->cantrip_limit; maximum = 30; break;
        case 9U: header = "Prepared limit"; value = level->prepared_limit; maximum = 50; break;
        case 10U: header = "Spellbook size"; value = level->spellbook_size; break;
        case 11U: header = "Pact slot level"; value = level->pact_slot_level; maximum = 5; break;
        case 12U: header = "Pact slots current"; value = level->pact_slots_current; maximum = level->pact_slots_max; break;
        case 13U: header = "Pact slots maximum"; value = level->pact_slots_max; maximum = 8; break;
        case 15U: header = "Spell points current"; value = level->spell_points_current; maximum = level->spell_points_max; break;
        case 16U: header = "Spell points maximum"; value = level->spell_points_max; break;
        default: return false;
        }
    } else if(app->list_kind == PocketListSpells) {
        if(field == 3U) {
            header = "Spell level";
            value = character->spells[index].level;
            maximum = 9;
        } else if(field == 8U) {
            header = "Free casts current";
            value = character->spell_free_casts_current[index];
            maximum = character->spell_free_casts_max[index];
        } else if(field == 9U) {
            header = "Free casts maximum";
            value = character->spell_free_casts_max[index];
            maximum = 20;
        } else {
            return false;
        }
    } else if(app->list_kind == PocketListFeatures) {
        PocketFeature* feature = &character->features[index];
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
    } else if(app->list_kind == PocketListItems) {
        PocketItem* item = &character->items[index];
        switch(field) {
        case 2U: header = "Item quantity"; value = item->quantity; break;
        case 3U: header = "Weight in tenths lb"; value = item->weight_tenths; maximum = 9999; break;
        case 9U: header = "Magic bonus"; value = item->magic_bonus; minimum = -10; maximum = 10; break;
        case 10U: header = "Damage dice count"; value = item->damage_dice; maximum = 20; break;
        case 11U: header = "Damage die sides"; value = item->damage_die; minimum = 4; maximum = 12; break;
        case 13U: header = "Versatile die sides"; value = item->versatile_die; minimum = 4; maximum = 12; break;
        case 23U: header = "Extra dice count"; value = item->extra_dice; maximum = 20; break;
        case 24U: header = "Extra die sides"; value = item->extra_die; minimum = 4; maximum = 12; break;
        case 25U: header = "Ammo current"; value = item->ammo_current; maximum = item->ammo_max; break;
        case 26U: header = "Ammo maximum"; value = item->ammo_max; break;
        case 29U: header = "Charges current"; value = item->charges_current; maximum = item->charges_max; break;
        case 30U: header = "Charges maximum"; value = item->charges_max; break;
        case 31U: header = "Armor base"; value = item->armor_base; maximum = 30; break;
        case 32U: header = "Armor DEX cap"; value = item->armor_dex_cap; minimum = -1; maximum = 9; break;
        case 33U: header = "Shield bonus"; value = item->shield_bonus; maximum = 10; break;
        default: return false;
        }
    } else if(app->list_kind == PocketListParty) {
        PocketPartyMember* member = &app->data.party[index];
        if(field == 1U) {
            header = "Initiative modifier";
            value = member->initiative_modifier;
            minimum = -50;
            maximum = 50;
        } else if(field == 2U) {
            header = "Armor Class";
            value = member->armor_class;
            maximum = 99;
        } else if(field == 3U) {
            header = "Current HP";
            value = member->hp_current;
            minimum = -999;
            maximum = 999;
        } else if(field == 4U) {
            header = "Maximum HP";
            value = member->hp_max;
            maximum = 999;
        } else {
            return false;
        }
    } else {
        return false;
    }
    pocket_begin_number(
        app, PocketNumberRecord, field, 0U, header, value, minimum, maximum);
    return true;
}

static void pocket_handle_record_detail_custom_name(PocketD20App* app) {
    PocketCharacter* character = &app->data.character;
    uint8_t index = app->record_index;
    if(app->list_kind == PocketListClasses && app->selection == 0U)
        pocket_begin_text(
            app, PocketEditClassName, "Custom class", character->classes[index].name);
    else if(app->list_kind == PocketListClasses && app->selection == 1U)
        pocket_begin_text(
            app, PocketEditSubclass, "Custom subclass", character->classes[index].subclass);
    else if(app->list_kind == PocketListSpells && app->selection == 0U)
        pocket_begin_text(
            app, PocketEditSpellName, "Custom spell", character->spells[index].name);
    else if(app->list_kind == PocketListFeatures && app->selection == 0U)
        pocket_begin_text(
            app, PocketEditFeatureName, "Custom feat/perk", character->features[index].name);
    else if(app->list_kind == PocketListItems && app->selection == 0U)
        pocket_begin_text(
            app, PocketEditItemName, "Custom item", character->items[index].name);
}

static void pocket_handle_record_detail(PocketD20App* app, const InputEvent* event) {
    uint8_t count = pocket_record_detail_count(app);
    if(pocket_is_move_event(event) && event->key == InputKeyUp)
        pocket_menu_move(app, count, -1);
    else if(pocket_is_move_event(event) && event->key == InputKeyDown)
        pocket_menu_move(app, count, 1);
    else if(pocket_is_move_event(event) &&
            (event->key == InputKeyLeft || event->key == InputKeyRight))
        pocket_adjust_record(app, event->key == InputKeyRight ? 1 : -1);
    else if(event->type == InputTypeLong && event->key == InputKeyOk) {
        if(!pocket_begin_record_number(app)) pocket_handle_record_detail_custom_name(app);
    }
    else if(event->type == InputTypeShort && event->key == InputKeyOk)
        pocket_handle_record_detail_ok(app);
}

static void pocket_apply_catalog_selection(PocketD20App* app) {
    if(app->selection >= app->catalog_count) return;
    char selected[POCKET_D20_NAME_LEN];
    pocket_copy(selected, sizeof(selected), app->catalog_entries[app->selection]);
    uint8_t selected_level = app->catalog_levels[app->selection];
    uint8_t selected_has_metadata = app->catalog_has_metadata[app->selection];
    uint8_t selected_item_category = app->catalog_item_categories[app->selection];
    uint8_t selected_school = app->catalog_schools[app->selection];
    uint8_t selected_source = app->catalog_sources[app->selection];
    uint8_t selected_ritual = app->catalog_ritual[app->selection];
    PocketCharacter* character = &app->data.character;
    uint8_t index = app->record_index;
    uint8_t grant_source = PocketGrantSourceCount;
    switch(app->catalog_target) {
    case PocketEditClassName:
        pocket_copy(character->classes[index].name, sizeof(character->classes[index].name), selected);
        pocket_configure_class_defaults(&character->classes[index]);
        grant_source = PocketGrantClassFeature;
        break;
    case PocketEditSubclass:
        pocket_copy(
            character->classes[index].subclass,
            sizeof(character->classes[index].subclass),
            selected);
        grant_source = PocketGrantSubclassFeature;
        break;
    case PocketEditSpecies:
        pocket_copy(character->species, sizeof(character->species), selected);
        grant_source = PocketGrantSpecies;
        break;
    case PocketEditSpellName:
        pocket_copy(character->spells[index].name, sizeof(character->spells[index].name), selected);
        if(selected_has_metadata) character->spells[index].level = selected_level;
        if(selected_has_metadata) {
            snprintf(
                character->spells[index].stable_id,
                sizeof(character->spells[index].stable_id),
                "spell-%u-%u",
                character->spells[index].level,
                app->catalog_page_start + app->selection);
            character->spells[index].school[0] = '\0';
            if(selected_school)
                pocket_copy(
                    character->spells[index].school,
                    sizeof(character->spells[index].school),
                    pocket_spell_school_names[selected_school]);
            pocket_copy(
                character->spells[index].source,
                sizeof(character->spells[index].source),
                selected_source == 1U ? "Core" : "Add-on");
            character->spells[index].ritual = selected_ritual;
        }
        break;
    case PocketEditFeatureName:
        pocket_copy(character->features[index].name, sizeof(character->features[index].name), selected);
        grant_source = PocketGrantFeat;
        break;
    case PocketEditItemName:
        pocket_copy(character->items[index].name, sizeof(character->items[index].name), selected);
        pocket_apply_equipment_preset(
            &character->items[index], selected, selected_item_category);
        grant_source = PocketGrantItem;
        break;
    case PocketEditBackground:
        pocket_copy(character->background, sizeof(character->background), selected);
        grant_source = PocketGrantBackground;
        break;
    case PocketEditAlignment:
        pocket_copy(character->alignment, sizeof(character->alignment), selected);
        break;
    default:
        return;
    }
    pocket_catalog_release(app);
    uint8_t staged = grant_source < PocketGrantSourceCount ?
                         pocket_stage_grants(app, grant_source, selected) : 0U;
    pocket_save(app, false);
    PocketScreen destination = app->return_screen;
    if(staged) {
        app->return_screen = destination;
        pocket_enter_screen(app, PocketScreenGrantReview);
        snprintf(app->status, sizeof(app->status), "%u grants to review", staged);
        return;
    }
    pocket_enter_screen(
        app,
        destination);
    app->selection = app->catalog_return_selection;
    if(app->selection >= 5U) app->scroll = app->selection - 4U;
    pocket_set_status(app, "Catalog choice saved");
}

static void pocket_handle_catalog(PocketD20App* app, const InputEvent* event) {
    if(app->catalog_count && pocket_is_move_event(event) && event->key == InputKeyUp)
        pocket_menu_move(app, app->catalog_count, -1);
    else if(app->catalog_count && pocket_is_move_event(event) && event->key == InputKeyDown)
        pocket_menu_move(app, app->catalog_count, 1);
    else if(pocket_is_move_event(event) &&
            (event->key == InputKeyLeft || event->key == InputKeyRight)) {
        uint16_t next_start = app->catalog_page_start;
        uint16_t page_limit = pocket_catalog_page_limit(app);
        if(event->key == InputKeyRight && app->catalog_has_more)
            next_start += page_limit;
        else if(event->key == InputKeyLeft && app->catalog_page_start >= page_limit)
            next_start -= page_limit;
        if(next_start != app->catalog_page_start) {
            app->catalog_page_start = next_start;
            app->selection = 0U;
            app->scroll = 0U;
            pocket_catalog_load_page(app);
        }
    } else if(event->type == InputTypeLong && event->key == InputKeyOk &&
            (app->catalog_kind == PocketCatalogSpells ||
             app->catalog_kind == PocketCatalogSubclasses)) {
        app->catalog_show_all = !app->catalog_show_all;
        app->catalog_page_start = 0U;
        app->selection = 0U;
        app->scroll = 0U;
        pocket_catalog_load_page(app);
        pocket_set_status(
            app,
            app->catalog_show_all ? "Showing all" :
            app->catalog_kind == PocketCatalogSpells ? "Class + level filter" : "Class filter");
    } else if(event->type == InputTypeShort && event->key == InputKeyOk)
        pocket_apply_catalog_selection(app);
}

static void pocket_handle_combat(PocketD20App* app, const InputEvent* event) {
    PocketCharacter* character = &app->data.character;
    if(pocket_is_move_event(event) && event->key == InputKeyUp)
        pocket_menu_move(app, 20U, -1);
    else if(pocket_is_move_event(event) && event->key == InputKeyDown)
        pocket_menu_move(app, 20U, 1);
    else if(pocket_is_move_event(event) &&
            (event->key == InputKeyLeft || event->key == InputKeyRight)) {
        int16_t delta = event->key == InputKeyRight ? 1 : -1;
        if(app->selection == 3U)
            character->hp_current = pocket_clamp_i16(character->hp_current + delta, 0, 999);
        else if(app->selection == 4U)
            character->hp_temporary = pocket_clamp_i16(character->hp_temporary + delta, 0, 999);
        else if(app->selection == 6U) {
            int16_t class_index = app->hit_die_class_index + delta;
            if(class_index < 0) class_index = character->class_count - 1U;
            if(class_index >= character->class_count) class_index = 0;
            app->hit_die_class_index = (uint8_t)class_index;
            return;
        }
        else if(app->selection == 10U)
            character->reaction_available = !character->reaction_available;
        else if(app->selection == 17U)
            character->death_successes = pocket_clamp_u8(character->death_successes + delta, 3U);
        else if(app->selection == 18U)
            character->death_failures = pocket_clamp_u8(character->death_failures + delta, 3U);
        else if(app->selection == 19U)
            character->exhaustion = pocket_clamp_u8(character->exhaustion + delta, 6U);
        else
            return;
        pocket_save(app, false);
    } else if(event->type == InputTypeLong && event->key == InputKeyOk &&
              (app->selection == 3U || app->selection == 4U ||
               (app->selection >= 17U && app->selection <= 19U))) {
        const char* header = app->selection == 3U ? "Current HP" :
                             app->selection == 4U ? "Temporary HP" :
                             app->selection == 17U ? "Death successes" :
                             app->selection == 18U ? "Death failures" : "Exhaustion";
        int32_t value = app->selection == 3U ? character->hp_current :
                        app->selection == 4U ? character->hp_temporary :
                        app->selection == 17U ? character->death_successes :
                        app->selection == 18U ? character->death_failures :
                                                character->exhaustion;
        int32_t maximum = app->selection <= 4U ? 999 :
                          app->selection <= 18U ? 3 : 6;
        pocket_begin_number(
            app,
            PocketNumberCombat,
            (uint8_t)app->selection,
            0U,
            header,
            value,
            0,
            maximum);
    } else if(event->type == InputTypeShort && event->key == InputKeyOk) {
        switch(app->selection) {
        case 0:
            pocket_enter_screen(app, PocketScreenAttackList);
            break;
        case 1:
            pocket_enter_screen(app, PocketScreenAttackTemplates);
            break;
        case 2:
            pocket_enter_screen(app, PocketScreenInitiativeMenu);
            break;
        case 3:
            character->hp_current = pocket_clamp_i16(character->hp_current + 1, 0, 999);
            pocket_save(app, false);
            break;
        case 4:
            character->hp_temporary = pocket_clamp_i16(character->hp_temporary + 1, 0, 999);
            pocket_save(app, false);
            break;
        case 5:
            if(character->hp_current < 1) {
                pocket_set_status(app, "Need at least 1 HP");
                break;
            }
            pocket_d20_short_rest(character);
            pocket_save(app, false);
            if(pocket_wizard_level(character) && !character->arcane_recovery_used &&
               pocket_begin_arcane_recovery(app))
                break;
            pocket_set_status(app, "Short rest applied");
            break;
        case 6:
            if(character->hp_current < 1) {
                pocket_set_status(app, "Need at least 1 HP");
            } else if(character->hp_current >= character->hp_max) {
                pocket_set_status(app, "HP already full");
            } else if(!character->classes[app->hit_die_class_index].hit_dice_current) {
                pocket_set_status(app, "No Hit Dice left");
            } else {
                uint8_t roll = 0U;
                int16_t healed = pocket_d20_spend_class_hit_die(
                    character, app->hit_die_class_index, &roll);
                int8_t constitution = pocket_d20_ability_modifier(
                    character->ability_scores[PocketAbilityConstitution]);
                pocket_save(app, false);
                snprintf(
                    app->status,
                    sizeof(app->status),
                    "d%u:%u %+d, healed %d",
                    character->classes[app->hit_die_class_index].hit_die,
                    roll,
                    constitution,
                    healed);
                pocket_start_dice_animation(
                    app, 1U, character->classes[app->hit_die_class_index].hit_die);
            }
            break;
        case 7:
            if(character->hp_current < 1) {
                pocket_set_status(app, "Need at least 1 HP");
                break;
            }
            pocket_d20_long_rest(character);
            pocket_save(app, false);
            pocket_set_status(app, "Long rest applied");
            break;
        case 8:
            pocket_begin_text(app, PocketEditConditions, "Conditions", character->conditions);
            break;
        case 9:
            pocket_begin_text(app, PocketEditConcentration, "Concentration", character->concentration);
            break;
        case 10:
            character->reaction_available = !character->reaction_available;
            pocket_save(app, false);
            break;
        case 11:
            pocket_begin_text(app, PocketEditTemporaryEffects, "Temporary effects", character->temporary_effects);
            break;
        case 12:
            pocket_begin_text(app, PocketEditResistances, "Resistances", character->resistances);
            break;
        case 13:
            pocket_begin_text(app, PocketEditImmunities, "Immunities", character->immunities);
            break;
        case 14:
            pocket_begin_text(app, PocketEditVulnerabilities, "Vulnerabilities", character->vulnerabilities);
            break;
        case 15:
            pocket_begin_text(app, PocketEditSenses, "Senses", character->senses);
            break;
        case 16:
            pocket_begin_text(app, PocketEditMovementModes, "Movement modes", character->movement_modes);
            break;
        case 17:
            character->death_successes = pocket_clamp_u8(character->death_successes + 1, 3U);
            pocket_save(app, false);
            break;
        case 18:
            character->death_failures = pocket_clamp_u8(character->death_failures + 1, 3U);
            pocket_save(app, false);
            break;
        case 19:
            character->exhaustion = pocket_clamp_u8(character->exhaustion + 1, 6U);
            pocket_save(app, false);
            break;
        }
    }
}

static void pocket_roll_generic(PocketD20App* app) {
    app->dice_first = 0U;
    app->dice_second = 0U;
    app->dice_guidance = 0U;
    app->dice_roll_value_count = 0U;
    app->dice_roll_sum = 0U;
    memset(app->dice_roll_values, 0, sizeof(app->dice_roll_values));
    if(app->roll_mode == PocketRollGuidance && app->dice_count == 1U &&
       app->dice_sides == 20U) {
        app->dice_first = (uint8_t)pocket_d20_roll_dice(1U, 20U);
        app->dice_guidance = (uint8_t)pocket_d20_roll_dice(1U, 4U);
        app->dice_roll_values[0] = app->dice_first;
        app->dice_roll_values[1] = app->dice_guidance;
        app->dice_roll_value_count = 2U;
        app->dice_roll_sum = app->dice_first + app->dice_guidance;
        app->dice_result = (int16_t)app->dice_roll_sum + app->dice_modifier;
    } else if((app->roll_mode == PocketRollAdvantage ||
               app->roll_mode == PocketRollDisadvantage) &&
              app->dice_count == 1U && app->dice_sides == 20U) {
        app->dice_first = (uint8_t)pocket_d20_roll_dice(1U, 20U);
        app->dice_second = (uint8_t)pocket_d20_roll_dice(1U, 20U);
        app->dice_roll_values[0] = app->dice_first;
        app->dice_roll_values[1] = app->dice_second;
        app->dice_roll_value_count = 2U;
        app->dice_roll_sum = app->dice_first + app->dice_second;
        uint8_t chosen = app->roll_mode == PocketRollAdvantage ?
                             (app->dice_first > app->dice_second ? app->dice_first : app->dice_second) :
                             (app->dice_first < app->dice_second ? app->dice_first : app->dice_second);
        app->dice_result = chosen + app->dice_modifier;
    } else {
        app->dice_roll_value_count = app->dice_count;
        app->dice_roll_sum = pocket_d20_roll_dice_values(
            app->dice_count,
            app->dice_sides,
            app->dice_roll_values,
            sizeof(app->dice_roll_values));
        if(app->dice_count == 1U) app->dice_first = app->dice_roll_values[0];
        app->dice_result = (int16_t)app->dice_roll_sum + app->dice_modifier;
    }
    pocket_enter_screen(app, PocketScreenDiceResult);
    pocket_start_dice_animation(
        app, app->dice_roll_value_count, app->dice_sides);
}

static void pocket_handle_dice(PocketD20App* app, const InputEvent* event) {
    if(pocket_is_move_event(event) && event->key == InputKeyUp)
        pocket_menu_move(app, 5U, -1);
    else if(pocket_is_move_event(event) && event->key == InputKeyDown)
        pocket_menu_move(app, 5U, 1);
    else if(pocket_is_move_event(event) &&
            (event->key == InputKeyLeft || event->key == InputKeyRight)) {
        int8_t delta = event->key == InputKeyRight ? 1 : -1;
        if(app->selection == 0U) {
            app->dice_count = (uint8_t)pocket_clamp_i16(app->dice_count + delta, 1, 20);
            if(app->roll_mode != PocketRollNormal) app->roll_mode = PocketRollNormal;
        } else if(app->selection == 1U) {
            app->dice_sides = pocket_cycle_die(app->dice_sides, delta, false);
            if(app->roll_mode != PocketRollNormal) app->roll_mode = PocketRollNormal;
        } else if(app->selection == 2U)
            app->dice_modifier = pocket_clamp_i16(app->dice_modifier + delta, -99, 99);
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
    } else if(event->type == InputTypeLong && event->key == InputKeyOk &&
              app->selection <= 2U) {
        const char* header = app->selection == 0U ? "Dice count" :
                             app->selection == 1U ? "Die sides" : "Roll modifier";
        int32_t value = app->selection == 0U ? app->dice_count :
                        app->selection == 1U ? app->dice_sides : app->dice_modifier;
        int32_t minimum = app->selection == 0U ? 1 :
                          app->selection == 1U ? 2 : -99;
        int32_t maximum = app->selection == 0U ? 20 :
                          app->selection == 1U ? 100 : 99;
        pocket_begin_number(
            app,
            PocketNumberDice,
            (uint8_t)app->selection,
            0U,
            header,
            value,
            minimum,
            maximum);
    } else if(event->type == InputTypeShort && event->key == InputKeyOk) {
        if(app->selection == 4U) pocket_roll_generic(app);
    }
}

static void pocket_handle_dice_result(PocketD20App* app, const InputEvent* event) {
    if(event->type == InputTypeShort && event->key == InputKeyOk) pocket_roll_generic(app);
}

static void pocket_roll_selected_attack(PocketD20App* app) {
    uint8_t count = pocket_weapon_count(app);
    if(count == 0U) return;
    app->attack_item_index = pocket_weapon_index(app, app->selection);
    PocketItem* item = &app->data.character.items[app->attack_item_index];
    if((item->weapon_properties & PocketWeaponAmmunition) && item->ammo_current <= 0) {
        pocket_set_status(app, "No ammunition");
        return;
    }
    if(item->weapon_properties & PocketWeaponAmmunition) {
        --item->ammo_current;
        pocket_save(app, false);
    }
    app->attack_roll = pocket_d20_roll_attack(&app->data.character, item, app->roll_mode);
    app->attack_phase = 0U;
    pocket_enter_screen(app, PocketScreenAttackResult);
    pocket_start_dice_animation(app, app->attack_roll.second_die ? 2U : 1U, 20U);
}

static void pocket_handle_attack_list(PocketD20App* app, const InputEvent* event) {
    uint8_t count = pocket_weapon_count(app);
    if(pocket_is_move_event(event) && event->key == InputKeyUp)
        pocket_menu_move(app, count, -1);
    else if(pocket_is_move_event(event) && event->key == InputKeyDown)
        pocket_menu_move(app, count, 1);
    else if(pocket_is_move_event(event) &&
            (event->key == InputKeyLeft || event->key == InputKeyRight)) {
        int16_t mode = app->roll_mode + (event->key == InputKeyRight ? 1 : -1);
        if(mode < 0) mode = PocketRollDisadvantage;
        if(mode > PocketRollDisadvantage) mode = PocketRollNormal;
        app->roll_mode = (PocketRollMode)mode;
    } else if(event->type == InputTypeShort && event->key == InputKeyOk) {
        pocket_roll_selected_attack(app);
    }
}

static void pocket_handle_attack_result(PocketD20App* app, const InputEvent* event) {
    const PocketItem* item = &app->data.character.items[app->attack_item_index];
    if(app->attack_phase == 0U) {
        if(event->type == InputTypeShort && event->key == InputKeyOk) {
            app->damage_roll = pocket_d20_roll_damage(
                &app->data.character,
                item,
                app->attack_roll.critical);
            app->attack_phase = 1U;
            app->damage_roll_page = 0U;
            uint8_t count =
                app->damage_roll.weapon_roll_count + app->damage_roll.extra_roll_count;
            if(count)
                pocket_start_dice_animation(
                    app,
                    count,
                    item->use_versatile && item->versatile_die >= 2U ? item->versatile_die :
                                                                        item->damage_die);
        } else if(event->type == InputTypeShort && event->key == InputKeyRight) {
            app->damage_roll = pocket_d20_roll_damage(&app->data.character, item, true);
            app->attack_phase = 1U;
            app->damage_roll_page = 0U;
            uint8_t count =
                app->damage_roll.weapon_roll_count + app->damage_roll.extra_roll_count;
            if(count)
                pocket_start_dice_animation(
                    app,
                    count,
                    item->use_versatile && item->versatile_die >= 2U ? item->versatile_die :
                                                                        item->damage_die);
        } else if(event->type == InputTypeShort && event->key == InputKeyUp) {
            app->attack_roll = pocket_d20_roll_attack(&app->data.character, item, app->roll_mode);
            pocket_start_dice_animation(
                app, app->attack_roll.second_die ? 2U : 1U, 20U);
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
        } else if(event->type == InputTypeShort && event->key == InputKeyDown &&
                  page_count > 1U) {
            app->damage_roll_page = (app->damage_roll_page + 1U) % page_count;
        } else if(event->type == InputTypeShort && event->key == InputKeyOk) {
            app->damage_roll = pocket_d20_roll_damage(
                &app->data.character,
                item,
                app->damage_roll.critical);
            app->damage_roll_page = 0U;
            roll_count =
                app->damage_roll.weapon_roll_count + app->damage_roll.extra_roll_count;
            if(roll_count)
                pocket_start_dice_animation(
                    app,
                    roll_count,
                    item->use_versatile && item->versatile_die >= 2U ? item->versatile_die :
                                                                        item->damage_die);
        }
    }
}

static void pocket_history_push(
    PocketD20App* app,
    uint8_t kind,
    uint8_t target,
    int16_t before,
    int16_t after) {
    if(app->data.encounter_history_count >= POCKET_D20_MAX_ENCOUNTER_HISTORY) {
        memmove(
            &app->data.encounter_history[0],
            &app->data.encounter_history[1],
            (POCKET_D20_MAX_ENCOUNTER_HISTORY - 1U) * sizeof(PocketEncounterHistory));
        app->data.encounter_history_count = POCKET_D20_MAX_ENCOUNTER_HISTORY - 1U;
    }
    PocketEncounterHistory* history =
        &app->data.encounter_history[app->data.encounter_history_count++];
    history->round = app->data.initiative.round;
    history->current_turn = app->data.initiative.current_turn;
    history->kind = kind;
    history->target = target;
    history->value_before = before;
    history->value_after = after;
}

static bool pocket_history_undo(PocketD20App* app) {
    if(!app->data.encounter_history_count) return false;
    PocketEncounterHistory* history =
        &app->data.encounter_history[--app->data.encounter_history_count];
    app->data.initiative.round = history->round;
    app->data.initiative.current_turn = history->current_turn;
    if(history->kind == PocketHistoryParticipantHp &&
       history->target < app->data.initiative.count) {
        PocketInitiativeEntry* entry = &app->data.initiative.entries[history->target];
        entry->hp_current = history->value_before;
        if(entry->is_player_character) app->data.character.hp_current = history->value_before;
    } else if(history->kind == PocketHistoryFeatureResource &&
              history->target < app->data.character.feature_count) {
        app->data.character.features[history->target].uses_current = history->value_before;
    }
    pocket_save(app, false);
    return true;
}

static void pocket_recover_features(PocketCharacter* character, uint8_t cadence) {
    for(uint8_t i = 0U; i < character->feature_count; ++i) {
        PocketFeature* feature = &character->features[i];
        if(feature->recharge == cadence)
            feature->uses_current = pocket_d20_feature_max_uses(character, feature);
    }
}

static void pocket_start_new_initiative(PocketD20App* app) {
    PocketInitiativeState* initiative = &app->data.initiative;
    memset(initiative, 0, sizeof(*initiative));
    initiative->round = 1U;
    PocketInitiativeEntry* character_entry = &initiative->entries[initiative->count++];
    pocket_copy(
        character_entry->name,
        sizeof(character_entry->name),
        app->data.character.name);
    character_entry->initiative_modifier = pocket_d20_initiative_modifier(&app->data.character);
    character_entry->is_player_character = 1U;
    character_entry->hp_current = app->data.character.hp_current;
    character_entry->hp_max = app->data.character.hp_max;
    character_entry->armor_class = app->data.character.armor_class;
    pocket_copy(character_entry->conditions, sizeof(character_entry->conditions), app->data.character.conditions);
    for(uint8_t i = 0U;
        i < app->data.party_count && initiative->count < POCKET_D20_MAX_INITIATIVE;
        ++i) {
        PocketInitiativeEntry* entry = &initiative->entries[initiative->count++];
        pocket_copy(entry->name, sizeof(entry->name), app->data.party[i].name);
        entry->initiative_modifier = app->data.party[i].initiative_modifier;
        entry->hp_current = app->data.party[i].hp_current;
        entry->hp_max = app->data.party[i].hp_max;
        entry->armor_class = app->data.party[i].armor_class;
    }
    app->data.encounter_history_count = 0U;
    pocket_recover_features(&app->data.character, PocketRechargeEncounter);
    pocket_save(app, false);
    pocket_enter_screen(app, PocketScreenInitiativeSetup);
}

static void pocket_sort_initiative(PocketInitiativeState* initiative) {
    for(uint8_t i = 1U; i < initiative->count; ++i) {
        PocketInitiativeEntry value = initiative->entries[i];
        uint8_t position = i;
        while(position > 0U &&
              initiative->entries[position - 1U].initiative_total < value.initiative_total) {
            initiative->entries[position] = initiative->entries[position - 1U];
            --position;
        }
        initiative->entries[position] = value;
    }
}

static void pocket_handle_initiative_menu(PocketD20App* app, const InputEvent* event) {
    PocketInitiativeState* initiative = &app->data.initiative;
    if(pocket_is_move_event(event) && event->key == InputKeyUp)
        pocket_menu_move(app, 6U, -1);
    else if(pocket_is_move_event(event) && event->key == InputKeyDown)
        pocket_menu_move(app, 6U, 1);
    else if(event->type == InputTypeShort && event->key == InputKeyOk) {
        if(app->selection == 0U)
            pocket_start_new_initiative(app);
        else if(app->selection == 1U) {
            if(initiative->active)
                pocket_enter_screen(app, PocketScreenInitiativeCombat);
            else
                pocket_set_status(app, "No active combat");
        } else if(app->selection == 2U)
            pocket_open_list(app, PocketListParty, PocketScreenInitiativeMenu);
        else if(app->selection == 3U) {
            if(initiative->count)
                pocket_enter_screen(app, PocketScreenInitiativeSetup);
            else
                pocket_set_status(app, "No current order");
        } else if(app->selection == 4U) {
            memset(initiative, 0, sizeof(*initiative));
            initiative->round = 1U;
            app->data.encounter_history_count = 0U;
            pocket_save(app, false);
            pocket_set_status(app, "Combat ended");
        } else {
            pocket_set_status(app, pocket_history_undo(app) ? "Change undone" : "Nothing to undo");
        }
    }
}

static void pocket_handle_initiative_setup(PocketD20App* app, const InputEvent* event) {
    PocketInitiativeState* initiative = &app->data.initiative;
    uint16_t count = initiative->count + 2U;
    if(pocket_is_move_event(event) && event->key == InputKeyUp)
        pocket_menu_move(app, count, -1);
    else if(pocket_is_move_event(event) && event->key == InputKeyDown)
        pocket_menu_move(app, count, 1);
    else if(pocket_is_move_event(event) &&
            app->selection < initiative->count &&
            (event->key == InputKeyLeft || event->key == InputKeyRight)) {
        int16_t delta = event->key == InputKeyRight ? 1 : -1;
        PocketInitiativeEntry* entry = &initiative->entries[app->selection];
        entry->initiative_total = pocket_clamp_i16(entry->initiative_total + delta, -20, 99);
        pocket_save(app, false);
    } else if(event->type == InputTypeLong && event->key == InputKeyOk &&
              app->selection < initiative->count) {
        PocketInitiativeEntry* entry = &initiative->entries[app->selection];
        entry->initiative_total =
            (int16_t)pocket_d20_roll_dice(1U, 20U) + entry->initiative_modifier;
        pocket_save(app, false);
        pocket_start_dice_animation(app, 1U, 20U);
    } else if(event->type == InputTypeShort && event->key == InputKeyOk) {
        if(app->selection < initiative->count) {
            PocketInitiativeEntry* entry = &initiative->entries[app->selection];
            entry->initiative_total =
                (int16_t)pocket_d20_roll_dice(1U, 20U) + entry->initiative_modifier;
            pocket_save(app, false);
            pocket_start_dice_animation(app, 1U, 20U);
        } else if(app->selection == initiative->count) {
            if(initiative->count >= POCKET_D20_MAX_INITIATIVE) {
                pocket_set_status(app, "Initiative list full");
            } else {
                app->record_index = initiative->count++;
                PocketInitiativeEntry* entry = &initiative->entries[app->record_index];
                memset(entry, 0, sizeof(*entry));
                pocket_copy(entry->name, sizeof(entry->name), "Temporary");
                entry->hp_current = 1;
                entry->hp_max = 1;
                entry->armor_class = 10;
                pocket_begin_text(
                    app,
                    PocketEditTemporaryInitiativeName,
                    "Participant name",
                    entry->name);
            }
        } else {
            pocket_sort_initiative(initiative);
            initiative->active = 1U;
            initiative->round = 1U;
            initiative->current_turn = 0U;
            pocket_save(app, false);
            pocket_enter_screen(app, PocketScreenInitiativeCombat);
        }
    }
}

static void pocket_swap_initiative(
    PocketInitiativeState* initiative,
    uint8_t first,
    uint8_t second) {
    PocketInitiativeEntry temporary = initiative->entries[first];
    initiative->entries[first] = initiative->entries[second];
    initiative->entries[second] = temporary;
    if(initiative->current_turn == first)
        initiative->current_turn = second;
    else if(initiative->current_turn == second)
        initiative->current_turn = first;
}

static void pocket_handle_initiative_combat(PocketD20App* app, const InputEvent* event) {
    PocketInitiativeState* initiative = &app->data.initiative;
    if(initiative->count == 0U) return;
    if(pocket_is_move_event(event) && event->key == InputKeyUp)
        pocket_menu_move(app, initiative->count, -1);
    else if(pocket_is_move_event(event) && event->key == InputKeyDown)
        pocket_menu_move(app, initiative->count, 1);
    else if(event->type == InputTypeShort && event->key == InputKeyOk) {
        pocket_history_push(app, PocketHistoryTurn, UINT8_MAX, 0, 0);
        pocket_recover_features(&app->data.character, PocketRechargeTurn);
        ++initiative->current_turn;
        if(initiative->current_turn >= initiative->count) {
            initiative->current_turn = 0U;
            ++initiative->round;
            if(initiative->round == 0U) initiative->round = 1U;
        }
        app->selection = initiative->current_turn;
        if(app->selection < app->scroll) app->scroll = app->selection;
        if(app->selection >= app->scroll + 5U) app->scroll = app->selection - 4U;
        pocket_save(app, false);
    } else if(pocket_is_move_event(event) &&
              (event->key == InputKeyLeft || event->key == InputKeyRight)) {
        PocketInitiativeEntry* entry = &initiative->entries[app->selection];
        int16_t before = entry->hp_current;
        entry->hp_current = pocket_clamp_i16(
            entry->hp_current + (event->key == InputKeyRight ? 1 : -1), -999, 999);
        pocket_history_push(
            app,
            PocketHistoryParticipantHp,
            app->selection,
            before,
            entry->hp_current);
        if(entry->is_player_character) app->data.character.hp_current = entry->hp_current;
        pocket_save(app, false);
    } else if(event->type == InputTypeLong && event->key == InputKeyOk) {
        app->record_index = app->selection;
        app->initiative_delete_armed = 0U;
        pocket_enter_screen(app, PocketScreenInitiativeEdit);
    } else if(event->type == InputTypeLong && event->key == InputKeyUp) {
        PocketInitiativeEntry* entry = &initiative->entries[app->selection];
        entry->armor_class = pocket_clamp_i16(entry->armor_class + 1, 0, 99);
        pocket_save(app, false);
    } else if(event->type == InputTypeLong && event->key == InputKeyDown) {
        app->record_index = app->selection;
        pocket_begin_text(
            app,
            PocketEditInitiativeConditions,
            "Participant conditions",
            initiative->entries[app->selection].conditions);
    } else if(event->type == InputTypeLong && event->key == InputKeyLeft &&
              app->selection > 0U) {
        pocket_swap_initiative(initiative, app->selection, app->selection - 1U);
        --app->selection;
        pocket_save(app, false);
    } else if(event->type == InputTypeLong && event->key == InputKeyRight &&
              app->selection + 1U < initiative->count) {
        pocket_swap_initiative(initiative, app->selection, app->selection + 1U);
        ++app->selection;
        pocket_save(app, false);
    }
}

static void pocket_delete_initiative_participant(PocketD20App* app) {
    PocketInitiativeState* initiative = &app->data.initiative;
    uint8_t index = app->record_index;
    if(index >= initiative->count) return;
    memmove(
        &initiative->entries[index],
        &initiative->entries[index + 1U],
        (initiative->count - index - 1U) * sizeof(PocketInitiativeEntry));
    --initiative->count;
    memset(&initiative->entries[initiative->count], 0, sizeof(PocketInitiativeEntry));
    app->data.encounter_history_count = 0U;
    if(initiative->count == 0U) {
        initiative->active = 0U;
        initiative->current_turn = 0U;
        pocket_save(app, false);
        pocket_enter_screen(app, PocketScreenInitiativeMenu);
        pocket_set_status(app, "Combat list empty");
        return;
    }
    if(initiative->current_turn > index)
        --initiative->current_turn;
    else if(initiative->current_turn >= initiative->count)
        initiative->current_turn = 0U;
    app->selection = index < initiative->count ? index : initiative->count - 1U;
    pocket_save(app, false);
    pocket_enter_screen(app, PocketScreenInitiativeCombat);
    app->selection = index < initiative->count ? index : initiative->count - 1U;
    if(app->selection >= 5U) app->scroll = app->selection - 4U;
    pocket_set_status(app, "Participant removed");
}

static void pocket_handle_initiative_edit(PocketD20App* app, const InputEvent* event) {
    if(app->record_index >= app->data.initiative.count) {
        pocket_enter_screen(app, PocketScreenInitiativeCombat);
        return;
    }
    PocketInitiativeEntry* entry = &app->data.initiative.entries[app->record_index];
    if(pocket_is_move_event(event) && event->key == InputKeyUp) {
        app->initiative_delete_armed = 0U;
        pocket_menu_move(app, 8U, -1);
    } else if(pocket_is_move_event(event) && event->key == InputKeyDown) {
        app->initiative_delete_armed = 0U;
        pocket_menu_move(app, 8U, 1);
    } else if(event->type == InputTypeShort && event->key == InputKeyOk) {
        switch(app->selection) {
        case 0U:
            pocket_begin_text(
                app, PocketEditTemporaryInitiativeName, "Participant name", entry->name);
            break;
        case 1U:
            pocket_begin_number(
                app, PocketNumberInitiative, 1U, 0U, "Initiative roll", entry->initiative_total, -99, 199);
            break;
        case 2U:
            pocket_begin_number(
                app, PocketNumberInitiative, 2U, 0U, "Initiative modifier", entry->initiative_modifier, -50, 50);
            break;
        case 3U:
            pocket_begin_number(
                app, PocketNumberInitiative, 3U, 0U, "Armor Class", entry->armor_class, 0, 99);
            break;
        case 4U:
            pocket_begin_number(
                app, PocketNumberInitiative, 4U, 0U, "Current HP", entry->hp_current, -999, 999);
            break;
        case 5U:
            pocket_begin_number(
                app, PocketNumberInitiative, 5U, 0U, "Maximum HP", entry->hp_max, 0, 999);
            break;
        case 6U:
            pocket_begin_text(
                app,
                PocketEditInitiativeConditions,
                "Participant conditions",
                entry->conditions);
            break;
        default:
            if(app->initiative_delete_armed)
                pocket_delete_initiative_participant(app);
            else {
                app->initiative_delete_armed = 1U;
                pocket_set_status(app, "Press OK again to delete");
            }
            break;
        }
    }
}

#if 0 /* v2.6: legacy monster handlers live in Dolphin Bestiary. */
static bool pocket_open_monster_detail(
    PocketD20App* app,
    const PocketMonsterSummary* summary,
    PocketScreen return_screen) {
    free(app->monster_detail);
    app->monster_detail = malloc(sizeof(PocketMonsterDetail));
    if(!app->monster_detail) {
        pocket_set_status(app, "Not enough memory");
        return false;
    }
    if(!pocket_monster_load(app->storage, summary, app->monster_detail)) {
        free(app->monster_detail);
        app->monster_detail = NULL;
        pocket_set_status(app, "Stat block unavailable");
        return false;
    }
    app->return_screen = return_screen;
    app->monster_delete_armed = 0U;
    app->monster_edit_existing = 0U;
    pocket_enter_screen(app, PocketScreenMonsterDetail);
    return true;
}

static void pocket_generate_encounter(PocketD20App* app) {
    if(!app->monster_encounter)
        app->monster_encounter = malloc(sizeof(PocketMonsterEncounter));
    if(!app->monster_encounter) {
        pocket_set_status(app, "Not enough memory");
        return;
    }
    if(pocket_monster_generate(app->storage, app->encounter_party_level,
       app->encounter_party_size, app->encounter_difficulty,
       pocket_monster_environment_names[app->encounter_environment],
       app->encounter_allow_repeats, app->encounter_template,
       pocket_monster_role_names[app->encounter_role], app->monster_encounter))
        pocket_enter_screen(app, PocketScreenEncounter);
    else
        pocket_set_status(app, "No eligible encounter");
}

static bool pocket_monster_record_issue(
    PocketD20App* app,
    uint16_t index,
    PocketMonsterSummary* summary,
    char* issue,
    size_t issue_size) {
    if(!pocket_monster_at(app->storage, index, summary)) {
        memset(summary, 0, sizeof(*summary));
        snprintf(summary->id, sizeof(summary->id), "index_%u", index);
        snprintf(issue, issue_size, "Invalid index record");
        return true;
    }
    PocketMonsterDetail detail;
    if(!pocket_monster_load(app->storage, summary, &detail)) {
        snprintf(issue, issue_size, "Missing stat block");
        return true;
    }
    uint16_t missing = PocketMonsterRequiredFields & ~detail.present_fields;
    if(missing) {
        const char* field = missing & PocketMonsterFieldSize ? "SizeAlignment" :
            missing & PocketMonsterFieldSpeed ? "Speed" :
            missing & PocketMonsterFieldAbilities ? "Abilities" :
            missing & PocketMonsterFieldSenses ? "Senses" :
            missing & PocketMonsterFieldLanguages ? "Languages" : "Actions";
        snprintf(issue, issue_size, "Missing %s", field);
        return true;
    }
    for(uint16_t prior = 0U; prior < index; ++prior) {
        PocketMonsterSummary previous;
        if(pocket_monster_at(app->storage, prior, &previous) &&
           !strcmp(summary->id, previous.id)) {
            snprintf(issue, issue_size, "Duplicate ID");
            return true;
        }
    }
    return false;
}

static void pocket_monster_select_problem(PocketD20App* app, int8_t direction) {
    uint16_t total = pocket_monster_count(app->storage);
    if(!total || !app->monster_diag_problem_count) return;
    uint16_t index = app->monster_diag_problem_valid ? app->monster_diag_problem_index : 0U;
    for(uint16_t checked = 0U; checked < total; ++checked) {
        if(direction > 0) index = (uint16_t)((index + 1U) % total);
        else index = index ? index - 1U : total - 1U;
        PocketMonsterSummary summary;
        char issue[40];
        if(pocket_monster_record_issue(app, index, &summary, issue, sizeof(issue))) {
            app->monster_diag_problem = summary;
            app->monster_diag_problem_index = index;
            app->monster_diag_problem_valid = 1U;
            pocket_copy(app->monster_diag_issue, sizeof(app->monster_diag_issue), issue);
            return;
        }
    }
}

static void pocket_monster_diagnostics(PocketD20App* app) {
    pocket_monster_recover_user_pack(
        app->storage, &app->monster_diag_recovered, &app->monster_diag_rolled_back);
    app->monster_diag_valid = 0U;
    app->monster_diag_missing = 0U;
    app->monster_diag_duplicates = 0U;
    app->monster_diag_fields = 0U;
    app->monster_diag_problem_count = 0U;
    app->monster_diag_problem_valid = 0U;
    bool user_present = false;
    pocket_monster_pack_versions(app->storage, &app->monster_diag_bundled_version,
        &app->monster_diag_user_version, &user_present);
    app->monster_diag_user_present = user_present;
    uint16_t total = pocket_monster_count(app->storage);
    for(uint16_t i = 0U; i < total; ++i) {
        PocketMonsterSummary current;
        memset(&current, 0, sizeof(current));
        PocketMonsterDetail detail;
        if(!pocket_monster_at(app->storage, i, &current) ||
           !pocket_monster_load(app->storage, &current, &detail)) {
            ++app->monster_diag_missing;
            ++app->monster_diag_problem_count;
            if(!app->monster_diag_problem_valid) {
                app->monster_diag_problem = current;
                app->monster_diag_problem_index = i;
                app->monster_diag_problem_valid = 1U;
                pocket_copy(app->monster_diag_issue, sizeof(app->monster_diag_issue),
                            "Missing stat block");
            }
            continue;
        }
        ++app->monster_diag_valid;
        if((detail.present_fields & PocketMonsterRequiredFields) != PocketMonsterRequiredFields) {
            ++app->monster_diag_fields;
            ++app->monster_diag_problem_count;
            if(!app->monster_diag_problem_valid) {
                app->monster_diag_problem = current;
                app->monster_diag_problem_index = i;
                app->monster_diag_problem_valid = 1U;
                char issue[40];
                pocket_monster_record_issue(app, i, &current, issue, sizeof(issue));
                pocket_copy(app->monster_diag_issue, sizeof(app->monster_diag_issue), issue);
            }
        }
        for(uint16_t prior = 0U; prior < i; ++prior) {
            PocketMonsterSummary previous;
            if(pocket_monster_at(app->storage, prior, &previous) &&
               !strcmp(current.id, previous.id)) {
                ++app->monster_diag_duplicates;
                ++app->monster_diag_problem_count;
                if(!app->monster_diag_problem_valid) {
                    app->monster_diag_problem = current;
                    app->monster_diag_problem_index = i;
                    app->monster_diag_problem_valid = 1U;
                    pocket_copy(app->monster_diag_issue, sizeof(app->monster_diag_issue),
                                "Duplicate ID");
                }
                break;
            }
        }
    }
    bool version_error = app->monster_diag_bundled_version != POCKET_MONSTER_PACK_VERSION ||
        (app->monster_diag_user_present && app->monster_diag_user_version != POCKET_MONSTER_PACK_VERSION);
    pocket_set_status(app, app->monster_diag_missing || app->monster_diag_fields ||
        app->monster_diag_duplicates || version_error ?
        "Pack needs attention" : "Pack OK");
}

static void pocket_begin_custom_monster(PocketD20App* app) {
    free(app->monster_detail);
    app->monster_detail = calloc(1U, sizeof(PocketMonsterDetail));
    if(!app->monster_detail) {
        pocket_set_status(app, "Not enough memory");
        return;
    }
    PocketMonsterDetail* m = app->monster_detail;
    app->monster_edit_existing = 0U;
    app->monster_delete_armed = 0U;
    pocket_copy(m->summary.name, sizeof(m->summary.name), "New Monster");
    pocket_copy(m->summary.type, sizeof(m->summary.type), "Monstrosity");
    pocket_copy(m->summary.environment, sizeof(m->summary.environment), "Wilderness");
    pocket_copy(m->summary.source, sizeof(m->summary.source), "Custom");
    pocket_copy(m->summary.role, sizeof(m->summary.role), "Skirmisher");
    m->summary.cr_eighths = 8U;
    m->summary.xp = 200U;
    m->summary.armor_class = 12U;
    m->summary.hit_points = 10U;
    pocket_copy(m->size_alignment, sizeof(m->size_alignment), "Medium, Neutral");
    pocket_copy(m->speed, sizeof(m->speed), "30 ft.");
    for(uint8_t i = 0U; i < 6U; ++i) m->abilities[i] = 10;
    pocket_copy(m->senses, sizeof(m->senses), "Passive Perception 10");
    pocket_copy(m->languages, sizeof(m->languages), "None");
    pocket_copy(m->traits, sizeof(m->traits), "None");
    pocket_copy(m->actions, sizeof(m->actions), "Slam +2, 3 bludgeoning");
    m->present_fields = PocketMonsterRequiredFields;
    pocket_enter_screen(app, PocketScreenMonsterEdit);
}

static __attribute__((unused)) void pocket_handle_monsters(PocketD20App* app, const InputEvent* event) {
    if(pocket_is_move_event(event) && event->key == InputKeyUp)
        pocket_menu_move(app, 16U, -1);
    else if(pocket_is_move_event(event) && event->key == InputKeyDown)
        pocket_menu_move(app, 16U, 1);
    else if(pocket_is_move_event(event) &&
            (event->key == InputKeyLeft || event->key == InputKeyRight)) {
        int16_t delta = event->key == InputKeyRight ? 1 : -1;
        if(app->selection == 2U) {
            static const uint8_t cr_values[] = {0U, 2U, 4U, 8U, 16U, 24U, 32U, 40U, 64U, 80U, 160U};
            uint8_t position = 0U;
            while(position + 1U < sizeof(cr_values) && cr_values[position] != app->monster_max_cr_eighths) ++position;
            int16_t next = position + delta;
            if(next < 0) next = sizeof(cr_values) - 1U;
            if(next >= (int16_t)sizeof(cr_values)) next = 0;
            app->monster_max_cr_eighths = cr_values[next];
            app->monster_count = pocket_monster_filtered_count(app);
        } else if(app->selection == 3U) {
            int16_t value = app->monster_type_filter + delta;
            uint8_t count = sizeof(pocket_monster_type_names) / sizeof(pocket_monster_type_names[0]);
            if(value < 0) value = count - 1U;
            if(value >= count) value = 0;
            app->monster_type_filter = (uint8_t)value;
            app->monster_count = pocket_monster_filtered_count(app);
        } else if(app->selection == 4U) {
            int16_t value = app->monster_source_filter + delta;
            uint8_t count = sizeof(pocket_monster_source_names) / sizeof(pocket_monster_source_names[0]);
            if(value < 0) value = count - 1U;
            if(value >= count) value = 0;
            app->monster_source_filter = (uint8_t)value;
            app->monster_count = pocket_monster_filtered_count(app);
        } else if(app->selection == 5U) {
            int16_t value = app->monster_environment_filter + delta;
            uint8_t count = sizeof(pocket_monster_environment_names) / sizeof(pocket_monster_environment_names[0]);
            if(value < 0) value = count - 1U;
            if(value >= count) value = 0;
            app->monster_environment_filter = (uint8_t)value;
            app->monster_count = pocket_monster_filtered_count(app);
        } else if(app->selection == 6U)
            app->encounter_party_level = pocket_clamp_u8(app->encounter_party_level + delta, 20U);
        else if(app->selection == 7U)
            app->encounter_party_size = pocket_clamp_u8(app->encounter_party_size + delta, 12U);
        else if(app->selection == 8U) {
            int16_t value = app->encounter_difficulty + delta;
            if(value < 0) value = PocketEncounterHigh;
            if(value >= PocketEncounterDifficultyCount) value = PocketEncounterLow;
            app->encounter_difficulty = (PocketEncounterDifficulty)value;
        } else if(app->selection == 9U) {
            int16_t value = app->encounter_environment + delta;
            uint8_t count = sizeof(pocket_monster_environment_names) / sizeof(pocket_monster_environment_names[0]);
            if(value < 0) value = count - 1U;
            if(value >= count) value = 0;
            app->encounter_environment = (uint8_t)value;
        } else if(app->selection == 10U) {
            app->encounter_allow_repeats = !app->encounter_allow_repeats;
        } else if(app->selection == 11U) {
            int16_t value = app->encounter_template + delta;
            if(value < 0) value = PocketEncounterElite;
            if(value >= PocketEncounterTemplateCount) value = PocketEncounterBalanced;
            app->encounter_template = (PocketEncounterTemplate)value;
        } else if(app->selection == 12U) {
            int16_t value = app->encounter_role + delta;
            uint8_t count = sizeof(pocket_monster_role_names) / sizeof(pocket_monster_role_names[0]);
            if(value < 0) value = count - 1U;
            if(value >= count) value = 0;
            app->encounter_role = (uint8_t)value;
        }
        if(!app->encounter_party_level) app->encounter_party_level = 1U;
        if(!app->encounter_party_size) app->encounter_party_size = 1U;
    } else if(event->type == InputTypeShort && event->key == InputKeyOk) {
        if(app->selection == 0U) {
            if(app->monster_count) pocket_enter_screen(app, PocketScreenMonsterList);
            else pocket_set_status(app, "No monster index");
        } else if(app->selection == 1U) {
            pocket_begin_text(app, PocketEditMonsterSearch, "Monster name (blank=any)", app->monster_search);
        } else if(app->selection == 13U) pocket_generate_encounter(app);
        else if(app->selection == 14U) {
            pocket_monster_diagnostics(app);
            pocket_enter_screen(app, PocketScreenMonsterDiagnostics);
        } else if(app->selection == 15U) pocket_begin_custom_monster(app);
    }
}

static __attribute__((unused)) void pocket_handle_monster_list(PocketD20App* app, const InputEvent* event) {
    if(pocket_is_move_event(event) && event->key == InputKeyUp)
        pocket_menu_move(app, app->monster_count, -1);
    else if(pocket_is_move_event(event) && event->key == InputKeyDown)
        pocket_menu_move(app, app->monster_count, 1);
    else if(event->type == InputTypeShort && event->key == InputKeyOk) {
        PocketMonsterSummary summary;
        if(pocket_monster_filtered_at(app, app->selection, &summary))
            pocket_open_monster_detail(app, &summary, PocketScreenMonsterList);
    }
}

static __attribute__((unused)) void pocket_handle_monster_detail(PocketD20App* app, const InputEvent* event) {
    if(pocket_is_move_event(event) && event->key == InputKeyUp)
        pocket_menu_move(app, 14U, -1);
    else if(pocket_is_move_event(event) && event->key == InputKeyDown)
        pocket_menu_move(app, 14U, 1);
    else if(event->type == InputTypeLong && event->key == InputKeyOk &&
            app->monster_detail && !strcmp(app->monster_detail->summary.source, "Custom")) {
        app->monster_selected = app->monster_detail->summary;
        app->monster_edit_existing = 1U;
        app->monster_delete_armed = 0U;
        pocket_enter_screen(app, PocketScreenMonsterEdit);
    } else if(event->type == InputTypeLong && event->key == InputKeyRight &&
              app->monster_detail && !strcmp(app->monster_detail->summary.source, "Custom")) {
        if(!app->monster_delete_armed) {
            app->monster_delete_armed = 1U;
            pocket_set_status(app, "Hold Right again: delete");
        } else {
            bool deleted = pocket_monster_delete_custom(
                app->storage, &app->monster_detail->summary);
            if(deleted) {
                free(app->monster_detail);
                app->monster_detail = NULL;
                app->monster_count = pocket_monster_filtered_count(app);
                pocket_enter_screen(app, app->return_screen);
            }
            pocket_set_status(app, deleted ? "Custom monster deleted" : "Delete failed");
        }
    }
}

static __attribute__((unused)) void pocket_handle_monster_diagnostics(PocketD20App* app, const InputEvent* event) {
    if(pocket_is_move_event(event) && event->key == InputKeyUp)
        pocket_menu_move(app, 15U, -1);
    else if(pocket_is_move_event(event) && event->key == InputKeyDown)
        pocket_menu_move(app, 15U, 1);
    else if(pocket_is_move_event(event) && app->selection >= 7U && app->selection <= 9U &&
            (event->key == InputKeyLeft || event->key == InputKeyRight))
        pocket_monster_select_problem(app, event->key == InputKeyRight ? 1 : -1);
    else if(event->type == InputTypeShort && event->key == InputKeyOk) {
        if(app->selection >= 7U && app->selection <= 9U &&
           app->monster_diag_problem_valid) {
            pocket_open_monster_detail(
                app, &app->monster_diag_problem, PocketScreenMonsterDiagnostics);
        } else {
            pocket_monster_diagnostics(app);
        }
    }
}

static __attribute__((unused)) void pocket_handle_monster_edit(PocketD20App* app, const InputEvent* event) {
    PocketMonsterDetail* m = app->monster_detail;
    if(!m) return;
    if(pocket_is_move_event(event) && event->key == InputKeyUp)
        pocket_menu_move(app, 24U, -1);
    else if(pocket_is_move_event(event) && event->key == InputKeyDown)
        pocket_menu_move(app, 24U, 1);
    else if(pocket_is_move_event(event) &&
            (event->key == InputKeyLeft || event->key == InputKeyRight)) {
        int16_t delta = event->key == InputKeyRight ? 1 : -1;
        if(app->selection == 1U) {
            int16_t value = m->summary.cr_eighths + delta;
            if(value < 0) value = 0;
            if(value > 160) value = 160;
            m->summary.cr_eighths = (uint8_t)value;
        } else if(app->selection == 2U) {
            int32_t value = (int32_t)m->summary.xp + delta * 25;
            if(value < 10) value = 10;
            if(value > 155000) value = 155000;
            m->summary.xp = (uint32_t)value;
        } else if(app->selection == 3U) {
            m->summary.armor_class = pocket_clamp_u8(m->summary.armor_class + delta, 30U);
            if(m->summary.armor_class < 1U) m->summary.armor_class = 1U;
        } else if(app->selection == 4U) {
            m->summary.hit_points = (uint16_t)pocket_clamp_i16(m->summary.hit_points + delta, 1, 999);
        } else if(app->selection == 6U) {
            int16_t value = app->encounter_environment + delta;
            uint8_t count = sizeof(pocket_monster_environment_names) / sizeof(pocket_monster_environment_names[0]);
            if(value < 1) value = count - 1U;
            if(value >= count) value = 1U;
            app->encounter_environment = (uint8_t)value;
            pocket_copy(m->summary.environment, sizeof(m->summary.environment),
                pocket_monster_environment_names[app->encounter_environment]);
        } else if(app->selection == 7U) {
            int16_t value = app->encounter_role + delta;
            uint8_t count = sizeof(pocket_monster_role_names) / sizeof(pocket_monster_role_names[0]);
            if(value < 1) value = count - 1U;
            if(value >= count) value = 1U;
            app->encounter_role = (uint8_t)value;
            pocket_copy(m->summary.role, sizeof(m->summary.role),
                        pocket_monster_role_names[app->encounter_role]);
        } else if(app->selection >= 10U && app->selection <= 15U) {
            uint8_t ability = app->selection - 10U;
            m->abilities[ability] = (int8_t)pocket_clamp_i16(m->abilities[ability] + delta, 1, 30);
        }
    } else if(event->type == InputTypeShort && event->key == InputKeyOk) {
        if(app->selection == 0U) pocket_begin_text(app, PocketEditMonsterName, "Monster name", m->summary.name);
        else if(app->selection == 5U) pocket_begin_text(app, PocketEditMonsterType, "Creature type", m->summary.type);
        else if(app->selection == 8U) pocket_begin_text(app, PocketEditMonsterSize, "Size and alignment", m->size_alignment);
        else if(app->selection == 9U) pocket_begin_text(app, PocketEditMonsterSpeed, "Movement", m->speed);
        else if(app->selection == 16U) pocket_begin_text(app, PocketEditMonsterSkills, "Skills", m->skills);
        else if(app->selection == 17U) pocket_begin_text(app, PocketEditMonsterDefenses, "Defenses", m->defenses);
        else if(app->selection == 18U) pocket_begin_text(app, PocketEditMonsterSenses, "Senses", m->senses);
        else if(app->selection == 19U) pocket_begin_text(app, PocketEditMonsterLanguages, "Languages", m->languages);
        else if(app->selection == 20U) pocket_begin_text(app, PocketEditMonsterTraits, "Traits", m->traits);
        else if(app->selection == 21U) pocket_begin_text(app, PocketEditMonsterActions, "Actions", m->actions);
        else if(app->selection == 22U) pocket_begin_text(app, PocketEditMonsterExtra, "Extra actions", m->extra);
        else if(app->selection == 23U) {
            bool saved = app->monster_edit_existing ?
                pocket_monster_update_custom(app->storage, m) :
                pocket_monster_save_custom(app->storage, m);
            if(saved) {
                app->monster_count = pocket_monster_filtered_count(app);
                if(app->monster_edit_existing) {
                    app->monster_selected = m->summary;
                    app->monster_edit_existing = 0U;
                    pocket_enter_screen(app, PocketScreenMonsterDetail);
                } else {
                    free(app->monster_detail);
                    app->monster_detail = NULL;
                    pocket_enter_screen(app, PocketScreenMonsters);
                }
            }
            pocket_set_status(app, saved ? "Custom monster saved" : "Custom save failed");
        }
    }
}

static void pocket_encounter_to_initiative(PocketD20App* app) {
    PocketMonsterEncounter* encounter = app->monster_encounter;
    if(!encounter) return;
    PocketInitiativeState* initiative = &app->data.initiative;
    memset(initiative, 0, sizeof(*initiative));
    initiative->round = 1U;
    for(uint8_t i = 0U; i < encounter->count; ++i) {
        PocketMonsterDetail detail;
        bool detailed = pocket_monster_load(app->storage, &encounter->monsters[i], &detail);
        for(uint8_t quantity = 0U; quantity < encounter->quantities[i] &&
             initiative->count < POCKET_D20_MAX_INITIATIVE; ++quantity) {
            PocketInitiativeEntry* entry = &initiative->entries[initiative->count++];
            memset(entry, 0, sizeof(*entry));
            if(encounter->quantities[i] > 1U)
                snprintf(entry->name, sizeof(entry->name), "%.18s %u",
                         encounter->monsters[i].name, quantity + 1U);
            else
                pocket_copy(entry->name, sizeof(entry->name), encounter->monsters[i].name);
            entry->initiative_modifier = detailed ?
                pocket_d20_ability_modifier((uint8_t)detail.abilities[PocketAbilityDexterity]) : 0;
            entry->hp_current = encounter->monsters[i].hit_points;
            entry->hp_max = encounter->monsters[i].hit_points;
            entry->armor_class = encounter->monsters[i].armor_class;
        }
    }
    app->data.encounter_history_count = 0U;
    pocket_save(app, false);
    free(app->monster_detail);
    free(app->monster_encounter);
    pocket_enter_screen(app, PocketScreenInitiativeSetup);
    pocket_set_status(app, "Encounter added");
}

static __attribute__((unused)) void pocket_handle_encounter(PocketD20App* app, const InputEvent* event) {
    if(!app->monster_encounter) return;
    if(pocket_is_move_event(event) && event->key == InputKeyUp)
        pocket_menu_move(app, app->monster_encounter->count, -1);
    else if(pocket_is_move_event(event) && event->key == InputKeyDown)
        pocket_menu_move(app, app->monster_encounter->count, 1);
    else if(event->type == InputTypeShort && event->key == InputKeyOk &&
            app->selection < app->monster_encounter->count)
        pocket_open_monster_detail(app, &app->monster_encounter->monsters[app->selection], PocketScreenEncounter);
    else if(event->type == InputTypeLong && event->key == InputKeyOk)
        pocket_generate_encounter(app);
    else if(event->type == InputTypeLong && event->key == InputKeyRight)
        pocket_encounter_to_initiative(app);
}
#endif

static bool pocket_input_callback(InputEvent* event, void* context) {
    PocketD20App* app = context;
    if(event->type == InputTypeLong && event->key == InputKeyBack) {
        pocket_handle_long_back(app);
        pocket_refresh(app);
        return true;
    }
    if(app->dice_animating) {
        if(event->type == InputTypeShort && event->key == InputKeyBack) {
            app->dice_animating = 0U;
            furi_timer_start(app->dice_timer, furi_ms_to_ticks(POCKET_D20_MARQUEE_MS));
        }
        pocket_refresh(app);
        return true;
    }
    if(event->type == InputTypeShort && event->key == InputKeyBack) {
        pocket_handle_back(app);
        pocket_refresh(app);
        return true;
    }

    switch(app->screen) {
    case PocketScreenHome:
        pocket_handle_home(app, event);
        break;
    case PocketScreenProfiles:
        pocket_handle_profiles(app, event);
        break;
    case PocketScreenProfileActions:
        pocket_handle_profile_actions(app, event);
        break;
    case PocketScreenCharacter:
        pocket_handle_character(app, event);
        break;
    case PocketScreenVitals:
        pocket_handle_vitals(app, event);
        break;
    case PocketScreenAbilities:
        pocket_handle_abilities(app, event);
        break;
    case PocketScreenSkills:
        pocket_handle_skills(app, event);
        break;
    case PocketScreenGrantReview:
        pocket_handle_grant_review(app, event);
        break;
    case PocketScreenGrantEdit:
        pocket_handle_grant_edit(app, event);
        break;
    case PocketScreenMagic:
        pocket_handle_magic(app, event);
        break;
    case PocketScreenSpellFilters:
        pocket_handle_spell_filters(app, event);
        break;
    case PocketScreenCurrency:
        pocket_handle_currency(app, event);
        break;
    case PocketScreenResources:
        pocket_handle_resources(app, event);
        break;
    case PocketScreenRecordList:
        pocket_handle_record_list(app, event);
        break;
    case PocketScreenRecordDetail:
        pocket_handle_record_detail(app, event);
        break;
    case PocketScreenCatalog:
        pocket_handle_catalog(app, event);
        break;
    case PocketScreenCombat:
        pocket_handle_combat(app, event);
        break;
    case PocketScreenAttackTemplates:
        pocket_handle_attack_templates(app, event);
        break;
    case PocketScreenAttackTemplateEdit:
        pocket_handle_attack_template_edit(app, event);
        break;
    case PocketScreenDice:
        pocket_handle_dice(app, event);
        break;
    case PocketScreenDiceResult:
        pocket_handle_dice_result(app, event);
        break;
    case PocketScreenAttackList:
        pocket_handle_attack_list(app, event);
        break;
    case PocketScreenAttackResult:
        pocket_handle_attack_result(app, event);
        break;
    case PocketScreenInitiativeMenu:
        pocket_handle_initiative_menu(app, event);
        break;
    case PocketScreenInitiativeSetup:
        pocket_handle_initiative_setup(app, event);
        break;
    case PocketScreenInitiativeCombat:
        pocket_handle_initiative_combat(app, event);
        break;
    case PocketScreenInitiativeEdit:
        pocket_handle_initiative_edit(app, event);
        break;
    case PocketScreenCampaigns:
        pocket_handle_campaigns(app, event);
        break;
    case PocketScreenCampaignDiagnostics:
        pocket_handle_campaign_diagnostics(app, event);
        break;
    case PocketScreenAdventure:
        pocket_handle_adventure(app, event);
        break;
    default:
        break;
    }
    pocket_refresh(app);
    return true;
}

static bool pocket_navigation_callback(void* context) {
    PocketD20App* app = context;
    app->input_module_active = 0U;
    app->number_context = PocketNumberNone;
    app->edit_target = PocketEditNone;
    view_dispatcher_switch_to_view(app->dispatcher, PocketViewMain);
    pocket_refresh(app);
    return true;
}

static PocketD20App* pocket_app_alloc(void) {
    PocketD20App* app = malloc(sizeof(PocketD20App));
    if(!app) {
        FURI_LOG_E(TAG, "Unable to allocate %u-byte app state", (unsigned int)sizeof(PocketD20App));
        return NULL;
    }
    memset(app, 0, sizeof(*app));

    app->gui = furi_record_open(RECORD_GUI);
    app->storage = furi_record_open(RECORD_STORAGE);
    bool profiles_loaded = pocket_d20_profiles_load(app->storage, &app->profiles);
    bool recovered_backup = false;
    bool loaded = pocket_d20_storage_load_profile(
        app->storage,
        app->profiles.active_profile,
        &app->data,
        &recovered_backup);
    bool character_saved = loaded && !recovered_backup;
    if(!character_saved)
        character_saved = pocket_d20_storage_save_profile(
            app->storage, app->profiles.active_profile, &app->data);
    bool active_included = pocket_profile_include_active(app);
    bool metadata_saved =
        active_included && pocket_d20_profiles_save(app->storage, &app->profiles);
    app->saved_fingerprint = pocket_data_fingerprint(&app->data);
    if(!character_saved || !metadata_saved) {
        app->storage_read_only = 1U;
        app->storage_unsaved = 1U;
    }

    app->screen = PocketScreenHome;
    app->roll_mode = PocketRollNormal;
    app->dice_count = 1U;
    app->dice_sides = 20U;
    app->spell_filter_level = -1;
    app->spell_filter_class = UINT8_MAX;
    if(!character_saved || !metadata_saved)
        pocket_set_status(app, "UNSAVED - retry SD");
    else if(recovered_backup)
        pocket_set_status(app, "Backup recovered");
    else if(loaded)
        pocket_set_status(app, "Loaded");
    else if(profiles_loaded)
        pocket_set_status(app, "Fresh character");
    else
        pocket_set_status(app, "New character");

    app->dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->dispatcher, app);
    view_dispatcher_set_navigation_event_callback(app->dispatcher, pocket_navigation_callback);
    view_dispatcher_set_custom_event_callback(app->dispatcher, pocket_custom_event_callback);
    app->input_events = furi_record_open(RECORD_INPUT_EVENTS);
    app->input_subscription =
        furi_pubsub_subscribe(app->input_events, pocket_input_events_callback, app);
    app->dice_timer =
        furi_timer_alloc(pocket_dice_timer_callback, FuriTimerTypePeriodic, app);

    app->main_view = view_alloc();
    view_allocate_model(app->main_view, ViewModelTypeLockFree, sizeof(PocketD20App*));
    PocketD20App** model = view_get_model(app->main_view);
    *model = app;
    view_commit_model(app->main_view, false);
    view_set_context(app->main_view, app);
    view_set_draw_callback(app->main_view, pocket_draw_callback);
    view_set_input_callback(app->main_view, pocket_input_callback);

    view_dispatcher_add_view(app->dispatcher, PocketViewMain, app->main_view);
    view_dispatcher_attach_to_gui(app->dispatcher, app->gui, ViewDispatcherTypeFullscreen);
    furi_timer_start(app->dice_timer, furi_ms_to_ticks(POCKET_D20_MARQUEE_MS));
    return app;
}

static void pocket_app_free(PocketD20App* app) {
    furi_assert(app);
    pocket_save(app, false);
    pocket_catalog_release(app);
    pocket_adventure_release(app);
    if(app->number_input)
        view_dispatcher_remove_view(app->dispatcher, PocketViewNumberInput);
    if(app->text_input)
        view_dispatcher_remove_view(app->dispatcher, PocketViewTextInput);
    view_dispatcher_remove_view(app->dispatcher, PocketViewMain);
    if(app->text_input) text_input_free(app->text_input);
    if(app->number_input) number_input_free(app->number_input);
    view_free(app->main_view);
    furi_timer_stop(app->dice_timer);
    furi_timer_free(app->dice_timer);
    if(app->input_subscription)
        furi_pubsub_unsubscribe(app->input_events, app->input_subscription);
    if(app->input_events) furi_record_close(RECORD_INPUT_EVENTS);
    view_dispatcher_free(app->dispatcher);
    pocket_d20_profiles_free(&app->profiles);
    pocket_d20_data_clear(&app->data);
    furi_record_close(RECORD_STORAGE);
    furi_record_close(RECORD_GUI);
    free(app);
}

int32_t pocket_d20_app(void* context) {
    UNUSED(context);
    PocketD20App* app = pocket_app_alloc();
    if(!app) return -1;
    view_dispatcher_switch_to_view(app->dispatcher, PocketViewMain);
    view_dispatcher_run(app->dispatcher);
    pocket_app_free(app);
    return 0;
}
