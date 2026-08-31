#include "dndolphins.h"
#include "dnd_handoff.h"
#include "dnd_fs.h"
#include "dndolphins_rules.h"
#include "dndolphins_items.h"
#include "dndolphins_spells.h"
#include "dndolphins_spell_combat.h"
#include "dndolphins_storage.h"

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
} PocketPendingLaunch;
#define POCKET_D20_SPELL_PAGE_ENTRIES    10U
#define POCKET_D20_MARQUEE_MS            350U
#define POCKET_D20_AUTOSAVE_MS           450U

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
    PocketScreenLevelChoice,
    PocketScreenAsiAbility,
    PocketScreenMagic,
    PocketScreenSpellFilters,
    PocketScreenCurrency,
    PocketScreenResources,
    PocketScreenRecordList,
    PocketScreenRecordDetail,
    PocketScreenCatalog,
    PocketScreenCombat,
    PocketScreenSpellAttacks,
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
    PocketListSpells,
    PocketListFeatures,
    PocketListItems,
    PocketListLanguages,
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
    PocketEditAddSpell,
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
    PocketEditAddItem,
    PocketEditItemDetail,
    PocketEditItemAmmoGroup,
    PocketEditLanguageName,
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
    uint8_t spellbook_loaded;
    uint8_t items_loaded;
    uint8_t spellbook_total;
    uint8_t items_total;
    uint8_t spellbook_cache_start;
    uint8_t items_cache_start;
    PocketD20SpellClassCounts spell_class_counts;
    PocketD20ItemAggregate item_aggregate;
    uint8_t spell_class_counts_valid;
    uint8_t item_aggregate_valid;
    uint8_t combat_spell_count;
    uint8_t combat_spell_indices[POCKET_D20_MAX_SPELLS];
    uint8_t combat_weapon_count;
    uint8_t combat_weapon_indices[POCKET_D20_MAX_ITEMS];
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
    char (*catalog_entries)[POCKET_D20_CATALOG_NAME_LEN];
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
    char pending_launch_args[16];
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

    uint8_t level_choice_class_index;
    uint8_t level_choice_level;
    uint8_t level_choice_mode;
    uint8_t level_choice_first_ability;

    uint8_t action_ack_active;
    uint8_t action_ack_screen;
    uint16_t action_ack_selection;
    char status[32];
} PocketD20App;

static bool pocket_begin_next_level_choice(PocketD20App* app);
static void pocket_handle_level_choice(PocketD20App* app, const InputEvent* event);
static void pocket_handle_asi_ability(PocketD20App* app, const InputEvent* event);

static bool pocket_refresh_combat_spell_index(PocketD20App* app);
static bool pocket_refresh_combat_weapon_index(PocketD20App* app);

static void pocket_text_done(void* context);
static void pocket_roll_generic(PocketD20App* app);
static void pocket_handle_long_back(PocketD20App* app);
static void pocket_release_text_input(PocketD20App* app);
static void pocket_release_number_input(PocketD20App* app);
static void pocket_quiesce_async(PocketD20App* app);
static bool pocket_flush_save(PocketD20App* app, bool report);
static uint8_t pocket_marquee_offset = 0U;

static const char* const pocket_home_items[] = {
    "Characters",
    "Character",
    "Vitals",
    "Abilities & Saves",
    "Skills",
    "Magic & Spells",
    "Features & Perks",
    "Inventory",
    "Currency",
    "Inventory Resources",
    "Journal",
    "Adventure",
    "Bestiary",
    "Combat",
    "Initiative",
    "Dice Roller",
};

static const char* const pocket_home_retry_save = "Retry Save";

static const char* const pocket_profile_actions[] = {
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

static const uint8_t pocket_die_choices[] = {4U, 6U, 8U, 10U, 12U, 20U, 100U};
static const uint8_t pocket_damage_die_choices[] = {4U, 6U, 8U, 10U, 12U};
static const char* const pocket_roll_mode_names[] =
    {"Normal", "Advantage", "Disadvantage", "Guidance"};
static const char* const pocket_attack_ability_names[] = {"Auto", "Strength", "Dexterity", "Best"};
static const char* const pocket_recharge_names[] =
    {"Manual", "Turn", "Encounter", "Dawn", "Short/Long", "Long"};
static const char* const pocket_attack_template_type_names[] =
    {"Unarmed", "Spell Attack", "Saving Throw", "Custom"};
static const char* const pocket_size_names[] = {"Tiny", "Small", "Medium", "Large"};
static const char* const pocket_spellcasting_mode_names[] =
    {"None", "Full", "Half", "Third", "Pact", "Spell Points", "Custom"};
static const char* const pocket_resource_formula_names[] = {"Manual", "PB", "Ability"};
static const char* const pocket_spell_school_names[] = {
    "Any",
    "Abjuration",
    "Conjuration",
    "Divination",
    "Enchantment",
    "Evocation",
    "Illusion",
    "Necromancy",
    "Transmutation"};

/* Group the standard skills by governing ability without changing their save indexes. */
static const uint8_t pocket_skill_display_order[POCKET_D20_SKILL_COUNT] = {
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

static const char* const pocket_catalog_classes[] = {
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

static const char* const pocket_catalog_alignments[] = {
    "Lawful Good",
    "Neutral Good",
    "Chaotic Good",
    "Lawful Neutral",
    "True Neutral",
    "Chaotic Neutral",
    "Lawful Evil",
    "Neutral Evil",
    "Chaotic Evil"};

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
    {"Charm Monster",
     4U,
     PocketClassMaskBard | PocketClassMaskDruid | PocketClassMaskSorcerer |
         PocketClassMaskWarlock | PocketClassMaskWizard},
    {"Charm Person",
     1U,
     PocketClassMaskBard | PocketClassMaskDruid | PocketClassMaskSorcerer |
         PocketClassMaskWarlock | PocketClassMaskWizard},
    {"Chromatic Orb", 1U, PocketClassMaskSorcerer | PocketClassMaskWizard},
    {"Cure Wounds",
     1U,
     PocketClassMaskBard | PocketClassMaskCleric | PocketClassMaskDruid | PocketClassMaskPaladin |
         PocketClassMaskRanger},
    {"Detect Magic",
     1U,
     PocketClassMaskBard | PocketClassMaskCleric | PocketClassMaskDruid | PocketClassMaskPaladin |
         PocketClassMaskRanger | PocketClassMaskSorcerer | PocketClassMaskWizard},
    {"Dispel Magic",
     3U,
     PocketClassMaskBard | PocketClassMaskCleric | PocketClassMaskDruid | PocketClassMaskPaladin |
         PocketClassMaskSorcerer | PocketClassMaskWarlock | PocketClassMaskWizard},
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
    {"Hold Person",
     2U,
     PocketClassMaskBard | PocketClassMaskCleric | PocketClassMaskDruid | PocketClassMaskSorcerer |
         PocketClassMaskWarlock | PocketClassMaskWizard},
    {"Ice Knife", 1U, PocketClassMaskDruid | PocketClassMaskSorcerer | PocketClassMaskWizard},
    {"Identify", 1U, PocketClassMaskBard | PocketClassMaskWizard},
    {"Invisibility",
     2U,
     PocketClassMaskBard | PocketClassMaskSorcerer | PocketClassMaskWarlock |
         PocketClassMaskWizard},
    {"Light",
     0U,
     PocketClassMaskBard | PocketClassMaskCleric | PocketClassMaskSorcerer |
         PocketClassMaskWizard},
    {"Mage Armor", 1U, PocketClassMaskSorcerer | PocketClassMaskWizard},
    {"Mage Hand",
     0U,
     PocketClassMaskBard | PocketClassMaskSorcerer | PocketClassMaskWarlock |
         PocketClassMaskWizard},
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
    {"Thunderwave",
     1U,
     PocketClassMaskBard | PocketClassMaskDruid | PocketClassMaskSorcerer | PocketClassMaskWizard},
    {"Tsunami", 8U, PocketClassMaskDruid},
    {"Vitriolic Sphere", 4U, PocketClassMaskSorcerer | PocketClassMaskWizard},
};

static const char* const pocket_catalog_items[] = {
    "Battleaxe",
    "Club",
    "Dagger",
    "Dart",
    "Greatclub",
    "Greataxe",
    "Greatsword",
    "Handaxe",
    "Javelin",
    "Light Crossbow",
    "Longbow",
    "Longsword",
    "Mace",
    "Musket",
    "Pistol",
    "Quarterstaff",
    "Rapier",
    "Shortbow",
    "Shortsword",
    "Spear",
    "Bead of Nourishment",
    "Cloak of Invisibility",
    "Elixir of Health",
    "Energy Bow",
    "Gloves of Thievery",
    "Hat of Many Spells",
    "Potion of Healing",
    "Potion of Invulnerability",
    "Potion of Longevity",
    "Potion of Vitality",
    "Quarterstaff of the Acrobat",
    "Rod of Resurrection",
    "Sending Stones",
    "Sentinel Shield",
    "Shield of the Cavalier",
    "Thunderous Greatclub",
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

static bool pocket_parse_u32_strict(const char* text, uint32_t maximum, uint32_t* output) {
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
        capacity * sizeof(*app->catalog_entries) + capacity * sizeof(*app->catalog_levels) +
        capacity * sizeof(*app->catalog_class_masks) +
        capacity * sizeof(*app->catalog_has_metadata) +
        capacity * sizeof(*app->catalog_item_categories) +
        capacity * sizeof(*app->catalog_item_magic) + capacity * sizeof(*app->catalog_schools) +
        capacity * sizeof(*app->catalog_sources) + capacity * sizeof(*app->catalog_ritual);
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

static void pocket_clear_action_ack(PocketD20App* app) {
    app->action_ack_active = 0U;
}

static void pocket_confirm_action(PocketD20App* app, const char* status) {
    if(app->storage_unsaved) {
        pocket_set_status(app, "UNSAVED - retry SD");
        return;
    }
    app->action_ack_active = 1U;
    app->action_ack_screen = (uint8_t)app->screen;
    app->action_ack_selection = app->selection;
    pocket_set_status(app, status);
}

static void pocket_prefix_action_mark(char* row, size_t size) {
    if(!row || size < 5U) return;
    size_t length = strlen(row);
    if(length > size - 5U) length = size - 5U;
    memmove(row + 4U, row, length);
    memcpy(row, "[X] ", 4U);
    row[length + 4U] = '\0';
}

static void pocket_refresh(PocketD20App* app) {
    (void)view_get_model(app->main_view);
    view_commit_model(app->main_view, true);
}

static void pocket_autosave_timer_callback(void* context) {
    PocketD20App* app = context;
    view_dispatcher_send_custom_event(app->dispatcher, POCKET_D20_AUTOSAVE_EVENT);
}

static void pocket_input_events_callback(const void* value, void* context) {
    PocketD20App* app = context;
    const InputEvent* event = value;
    if(app->input_module_active && event && event->key == InputKeyBack &&
       event->type == InputTypeLong)
        view_dispatcher_send_custom_event(app->dispatcher, POCKET_D20_LONG_BACK_EVENT);
}

static void pocket_quiesce_async(PocketD20App* app) {
    if(!app) return;
    if(app->input_subscription && app->input_events) {
        furi_pubsub_unsubscribe(app->input_events, app->input_subscription);
        app->input_subscription = NULL;
    }
    if(app->autosave_timer) furi_timer_stop(app->autosave_timer);
}

static void pocket_start_dice_animation(PocketD20App* app, uint8_t count, uint8_t sides) {
    app->dice_animating = 1U;
    app->dice_anim_frame = 0U;
    app->dice_anim_count = count ? count : 1U;
    app->dice_anim_sides = sides >= 2U ? sides : 20U;
    app->marquee_elapsed_ms = 0U;
}

static void pocket_tick_event_callback(void* context) {
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
            ++pocket_marquee_offset;
            refresh = true;
        }
    }

    if(refresh) pocket_refresh(app);
}

static bool pocket_custom_event_callback(void* context, uint32_t event) {
    PocketD20App* app = context;
    if(event == POCKET_D20_AUTOSAVE_EVENT) {
        pocket_flush_save(app, false);
        pocket_refresh(app);
        return true;
    }
    if(event == POCKET_D20_LONG_BACK_EVENT) {
        app->input_module_active = 0U;
        app->edit_target = PocketEditNone;
        app->number_context = PocketNumberNone;
        view_dispatcher_switch_to_view(app->dispatcher, PocketViewMain);
        pocket_handle_long_back(app);
        pocket_refresh(app);
        return true;
    }
    return false;
}

static uint32_t pocket_hash_bytes(uint32_t hash, const void* pointer, size_t length) {
    const uint8_t* bytes = pointer;
    for(size_t i = 0U; i < length; ++i) {
        hash ^= bytes[i];
        hash *= 16777619UL;
    }
    return hash;
}

static uint32_t pocket_data_fingerprint(const PocketSaveData* data) {
    uint32_t hash = 2166136261UL;
    const PocketCharacter* character = &data->character;
    const uint8_t* character_bytes = (const uint8_t*)character;

    /* Spells and items are independent sidecar collections. Their counts, capacities,
       pointers, and record contents must never make the core character look dirty just
       because a collection was hydrated or released. */
    hash = pocket_hash_bytes(hash, character_bytes, offsetof(PocketCharacter, spell_count));
    hash = pocket_hash_bytes(hash, &character->feature_count, sizeof(character->feature_count));
    const size_t stable_middle = offsetof(PocketCharacter, language_count);
    const size_t grant_capacity = offsetof(PocketCharacter, grant_capacity);
    hash = pocket_hash_bytes(
        hash, character_bytes + stable_middle, grant_capacity - stable_middle);
    const size_t stable_tail = offsetof(PocketCharacter, attack_template_count);
    hash = pocket_hash_bytes(
        hash, character_bytes + stable_tail, sizeof(PocketCharacter) - stable_tail);
    if(character->feature_count && character->features)
        hash = pocket_hash_bytes(
            hash, character->features, (size_t)character->feature_count * sizeof(PocketFeature));
    if(character->grant_count && character->grants)
        hash = pocket_hash_bytes(
            hash, character->grants, (size_t)character->grant_count * sizeof(PocketGrant));
    return hash;
}

static uint32_t pocket_spellbook_fingerprint(const PocketCharacter* character) {
    uint32_t hash = 2166136261UL;
    hash = pocket_hash_bytes(hash, &character->spell_count, sizeof(character->spell_count));
    if(character->spell_count && character->spells && character->spell_known &&
       character->spell_always_prepared && character->spell_free_casts_current &&
       character->spell_free_casts_max) {
        hash = pocket_hash_bytes(
            hash, character->spells, (size_t)character->spell_count * sizeof(PocketSpell));
        hash = pocket_hash_bytes(hash, character->spell_known, character->spell_count);
        hash = pocket_hash_bytes(hash, character->spell_always_prepared, character->spell_count);
        hash = pocket_hash_bytes(hash, character->spell_free_casts_current, character->spell_count);
        hash = pocket_hash_bytes(hash, character->spell_free_casts_max, character->spell_count);
    }
    return hash;
}

static uint32_t pocket_items_fingerprint(const PocketCharacter* character) {
    uint32_t hash = 2166136261UL;
    hash = pocket_hash_bytes(hash, &character->item_count, sizeof(character->item_count));
    if(character->item_count && character->items)
        hash = pocket_hash_bytes(
            hash, character->items, (size_t)character->item_count * sizeof(PocketItem));
    return hash;
}

static bool pocket_load_spellbook_page(PocketD20App* app, uint8_t start) {
    if(!app->active_profile_loaded) return false;
    uint8_t total = 0U;
    if(!pocket_d20_storage_load_spellbook_window(
           app->storage, app->profiles.active_profile, start, &app->data.character, &total)) {
        pocket_set_status(app, "Spellbook read failed");
        return false;
    }
    app->spellbook_total = total;
    app->spellbook_cache_start = start;
    app->spellbook_loaded = 1U;
    app->saved_spellbook_fingerprint = pocket_spellbook_fingerprint(&app->data.character);
    return true;
}

static bool pocket_load_items_page(PocketD20App* app, uint8_t start) {
    if(!app->active_profile_loaded) return false;
    uint8_t total = 0U;
    if(!pocket_d20_storage_load_items_window(
           app->storage, app->profiles.active_profile, start, &app->data.character, &total)) {
        pocket_set_status(app, "Items read failed");
        return false;
    }
    app->items_total = total;
    app->items_cache_start = start;
    app->items_loaded = 1U;
    app->saved_items_fingerprint = pocket_items_fingerprint(&app->data.character);
    return true;
}

static bool pocket_load_spellbook(PocketD20App* app) {
    if(app->spellbook_loaded) return true;
    return pocket_load_spellbook_page(app, 0U);
}

static bool pocket_load_items(PocketD20App* app) {
    if(app->items_loaded) return true;
    return pocket_load_items_page(app, 0U);
}

static void pocket_collection_save_failed(PocketD20App* app) {
    /* A collection write failure is retryable and must not poison core profile
       storage state. The resident collection fingerprint remains dirty, which is
       sufficient for the next autosave/close to retry only that collection. */
    if(app->storage_failure_count < UINT16_MAX) ++app->storage_failure_count;
    pocket_set_status(app, "UNSAVED - retry SD");
}

static bool pocket_save_spellbook_if_changed(PocketD20App* app) {
    if(!app->spellbook_loaded) return true;
    uint32_t fingerprint = pocket_spellbook_fingerprint(&app->data.character);
    if(fingerprint == app->saved_spellbook_fingerprint) return true;
    if(app->storage_read_only) return false;
    if(!pocket_d20_storage_save_spellbook_window(
           app->storage,
           app->profiles.active_profile,
           app->spellbook_cache_start,
           &app->data.character)) {
        pocket_collection_save_failed(app);
        return false;
    }
    app->saved_spellbook_fingerprint = fingerprint;
    app->spell_class_counts_valid = 0U;
    return true;
}

static bool pocket_save_items_if_changed(PocketD20App* app) {
    if(!app->items_loaded) return true;
    uint32_t fingerprint = pocket_items_fingerprint(&app->data.character);
    if(fingerprint == app->saved_items_fingerprint) return true;
    if(app->storage_read_only) return false;
    if(!pocket_d20_storage_save_items_window(
           app->storage,
           app->profiles.active_profile,
           app->items_cache_start,
           &app->data.character)) {
        pocket_collection_save_failed(app);
        return false;
    }
    app->saved_items_fingerprint = fingerprint;
    app->item_aggregate_valid = 0U;
    return true;
}

static bool pocket_spell_cache_ensure(PocketD20App* app, uint8_t logical_index) {
    if(!app->spellbook_loaded && !pocket_load_spellbook(app)) return false;
    if(logical_index >= app->spellbook_total) return false;
    if(logical_index >= app->spellbook_cache_start &&
       logical_index < (uint8_t)(app->spellbook_cache_start + app->data.character.spell_count))
        return true;
    if(!pocket_save_spellbook_if_changed(app)) return false;
    pocket_d20_data_clear_spells(&app->data.character);
    app->spellbook_loaded = 0U;
    uint8_t start = (uint8_t)((logical_index / POCKET_D20_COLLECTION_CACHE_SIZE) *
                              POCKET_D20_COLLECTION_CACHE_SIZE);
    return pocket_load_spellbook_page(app, start);
}

static bool pocket_item_cache_ensure(PocketD20App* app, uint8_t logical_index) {
    if(!app->items_loaded && !pocket_load_items(app)) return false;
    if(logical_index >= app->items_total) return false;
    if(logical_index >= app->items_cache_start &&
       logical_index < (uint8_t)(app->items_cache_start + app->data.character.item_count))
        return true;
    if(!pocket_save_items_if_changed(app)) return false;
    pocket_d20_data_clear_items(&app->data.character);
    app->items_loaded = 0U;
    uint8_t start = (uint8_t)((logical_index / POCKET_D20_COLLECTION_CACHE_SIZE) *
                              POCKET_D20_COLLECTION_CACHE_SIZE);
    return pocket_load_items_page(app, start);
}

static uint8_t pocket_spell_local_index(const PocketD20App* app, uint8_t logical_index) {
    return (uint8_t)(logical_index - app->spellbook_cache_start);
}

static uint8_t pocket_item_local_index(const PocketD20App* app, uint8_t logical_index) {
    return (uint8_t)(logical_index - app->items_cache_start);
}

static PocketSpell* pocket_spell_at(PocketD20App* app, uint8_t logical_index, uint8_t* local_out) {
    if(!pocket_spell_cache_ensure(app, logical_index)) return NULL;
    uint8_t local = pocket_spell_local_index(app, logical_index);
    if(local >= app->data.character.spell_count) return NULL;
    if(local_out) *local_out = local;
    return &app->data.character.spells[local];
}

static PocketItem* pocket_item_at(PocketD20App* app, uint8_t logical_index, uint8_t* local_out) {
    if(!pocket_item_cache_ensure(app, logical_index)) return NULL;
    uint8_t local = pocket_item_local_index(app, logical_index);
    if(local >= app->data.character.item_count) return NULL;
    if(local_out) *local_out = local;
    return &app->data.character.items[local];
}

static PocketSpell* pocket_spell_at_cached(
    PocketD20App* app, uint8_t logical_index, uint8_t* local_out) {
    if(!app->spellbook_loaded || logical_index < app->spellbook_cache_start) return NULL;
    uint8_t local = (uint8_t)(logical_index - app->spellbook_cache_start);
    if(local >= app->data.character.spell_count) return NULL;
    if(local_out) *local_out = local;
    return &app->data.character.spells[local];
}

static PocketItem* pocket_item_at_cached(
    PocketD20App* app, uint8_t logical_index, uint8_t* local_out) {
    if(!app->items_loaded || logical_index < app->items_cache_start) return NULL;
    uint8_t local = (uint8_t)(logical_index - app->items_cache_start);
    if(local >= app->data.character.item_count) return NULL;
    if(local_out) *local_out = local;
    return &app->data.character.items[local];
}

static void pocket_record_list_scroll_sidecar(PocketD20App* app, uint8_t total) {
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

static bool pocket_record_list_prepare_sidecar(PocketD20App* app) {
    if(app->list_kind == PocketListItems) {
        if(app->items_total) {
            uint8_t logical = app->selection ? (uint8_t)(app->selection - 1U) : 0U;
            if(logical >= app->items_total) logical = (uint8_t)(app->items_total - 1U);
            if(!pocket_item_cache_ensure(app, logical)) return false;
        }
        pocket_record_list_scroll_sidecar(app, app->items_total);
    } else if(app->list_kind == PocketListSpells) {
        if(app->spellbook_total) {
            uint8_t logical = app->selection ? (uint8_t)(app->selection - 1U) : 0U;
            if(logical >= app->spellbook_total) logical = (uint8_t)(app->spellbook_total - 1U);
            if(!pocket_spell_cache_ensure(app, logical)) return false;
        }
        pocket_record_list_scroll_sidecar(app, app->spellbook_total);
    }
    return true;
}

static void pocket_record_list_focus(PocketD20App* app, uint8_t logical_index) {
    uint8_t total = app->list_kind == PocketListItems ? app->items_total : app->spellbook_total;
    if(!total) {
        app->selection = 0U;
        app->scroll = 0U;
        return;
    }
    if(logical_index >= total) logical_index = (uint8_t)(total - 1U);
    app->selection = (uint16_t)logical_index + 1U;
    if(!pocket_record_list_prepare_sidecar(app)) pocket_set_status(app, "Collection read failed");
}

static bool pocket_spell_class_counts_cached(PocketD20App* app) {
    if(!app->spell_class_counts_valid) {
        uint8_t total = 0U;
        if(!pocket_d20_storage_spell_class_counts(
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

static uint8_t pocket_class_prepared_count_cached(PocketD20App* app, uint8_t class_index) {
    return pocket_spell_class_counts_cached(app) && class_index < POCKET_D20_MAX_CLASSES ?
               app->spell_class_counts.prepared[class_index] :
               0U;
}

static uint8_t pocket_class_known_count_cached(PocketD20App* app, uint8_t class_index) {
    return pocket_spell_class_counts_cached(app) && class_index < POCKET_D20_MAX_CLASSES ?
               app->spell_class_counts.known[class_index] :
               0U;
}

static bool pocket_item_aggregate_streamed(
    PocketD20App* app, PocketD20ItemAggregate* aggregate) {
    if(!app->item_aggregate_valid) {
        uint8_t total = 0U;
        if(!pocket_d20_storage_item_aggregate(
               app->storage,
               app->profiles.active_profile,
               &app->item_aggregate,
               &total))
            return false;
        app->items_total = total;
        app->item_aggregate_valid = 1U;
    }
    *aggregate = app->item_aggregate;
    return true;
}

static bool pocket_reset_all_spell_free_casts(PocketD20App* app) {
    for(uint8_t logical = 0U; logical < app->spellbook_total; ++logical) {
        uint8_t local = 0U;
        PocketSpell* spell = pocket_spell_at(app, logical, &local);
        if(!spell) return false;
        app->data.character.spell_free_casts_current[local] =
            app->data.character.spell_free_casts_max[local];
    }
    return pocket_save_spellbook_if_changed(app);
}

static bool pocket_remap_spell_classes(PocketD20App* app, uint8_t removed_class) {
    for(uint8_t logical = 0U; logical < app->spellbook_total; ++logical) {
        PocketSpell* spell = pocket_spell_at(app, logical, NULL);
        if(!spell) return false;
        if(spell->class_index == removed_class)
            spell->class_index = 0U;
        else if(spell->class_index > removed_class)
            --spell->class_index;
    }
    return pocket_save_spellbook_if_changed(app);
}

static bool pocket_release_spellbook(PocketD20App* app) {
    if(!app->spellbook_loaded) return true;
    bool saved = pocket_save_spellbook_if_changed(app);
    if(saved) {
        pocket_d20_data_clear_spells(&app->data.character);
        app->spellbook_loaded = 0U;
        app->spellbook_cache_start = 0U;
    }
    return saved;
}

static bool pocket_release_items(PocketD20App* app) {
    if(!app->items_loaded) return true;
    bool saved = pocket_save_items_if_changed(app);
    if(saved) {
        pocket_d20_data_clear_items(&app->data.character);
        app->items_loaded = 0U;
        app->items_cache_start = 0U;
    }
    return saved;
}

static bool pocket_save_now(PocketD20App* app, bool report) {
    if(!app->active_profile_loaded) {
        if(report) pocket_set_status(app, "Profile not loaded");
        return false;
    }
    if(app->storage_read_only) {
        app->storage_unsaved = 1U;
        pocket_set_status(app, "UNSAVED - retry SD");
        return false;
    }
    pocket_d20_data_sanitize(&app->data);
    uint32_t fingerprint = pocket_data_fingerprint(&app->data);
    uint32_t spellbook_fingerprint = app->spellbook_loaded ?
                                         pocket_spellbook_fingerprint(&app->data.character) :
                                         app->saved_spellbook_fingerprint;
    uint32_t items_fingerprint = app->items_loaded ?
                                     pocket_items_fingerprint(&app->data.character) :
                                     app->saved_items_fingerprint;
    bool main_changed = app->storage_unsaved || fingerprint != app->saved_fingerprint;
    bool spellbook_changed = app->spellbook_loaded &&
                             spellbook_fingerprint != app->saved_spellbook_fingerprint;
    bool items_changed = app->items_loaded && items_fingerprint != app->saved_items_fingerprint;
    if(!main_changed && !spellbook_changed && !items_changed) {
        if(report) pocket_set_status(app, "Already saved");
        return true;
    }

    bool result = true;
    bool main_write_failed = false;
    if(main_changed) {
        bool active_found = app->profiles.active_entry_valid &&
                            app->profiles.active_entry.id == app->profiles.active_profile;
        result = active_found ?
                     pocket_d20_storage_save_profile_known_updated(
                         app->storage, &app->profiles.active_entry, &app->data) :
                     pocket_d20_storage_save_profile_updated(
                         app->storage, app->profiles.active_profile, &app->data);
        main_write_failed = !result;
        if(result) {
            app->profiles.active_entry.id = app->profiles.active_profile;
            app->profiles.active_entry.level = pocket_d20_total_level(&app->data.character);
            pocket_copy(
                app->profiles.active_entry.name,
                sizeof(app->profiles.active_entry.name),
                app->data.character.name);
            app->profiles.active_entry_valid = 1U;
            for(uint8_t i = 0U; i < app->profiles.cache_count; ++i) {
                if(app->profiles.entries[i].id != app->profiles.active_profile) continue;
                app->profiles.entries[i].level = pocket_d20_total_level(&app->data.character);
                pocket_copy(
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
        result = pocket_d20_storage_save_spellbook_window(
            app->storage,
            app->profiles.active_profile,
            app->spellbook_cache_start,
            &app->data.character);
        if(result) app->saved_spellbook_fingerprint = spellbook_fingerprint;
    }
    if(result && items_changed) {
        result = pocket_d20_storage_save_items_window(
            app->storage,
            app->profiles.active_profile,
            app->items_cache_start,
            &app->data.character);
        if(result) app->saved_items_fingerprint = items_fingerprint;
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
    if(report || !result) pocket_set_status(app, result ? "Saved" : "UNSAVED - SD unavailable");
    return result;
}

static bool pocket_flush_save(PocketD20App* app, bool report) {
    if(app->autosave_timer) furi_timer_stop(app->autosave_timer);
    app->autosave_pending = 0U;
    return pocket_save_now(app, report);
}

static bool pocket_save(PocketD20App* app, bool report) {
    if(report) return pocket_flush_save(app, true);
    if(!app->active_profile_loaded) return false;
    if(app->storage_read_only) {
        app->storage_unsaved = 1U;
        pocket_set_status(app, "UNSAVED - retry SD");
        return false;
    }
    pocket_d20_data_sanitize(&app->data);
    uint32_t fingerprint = pocket_data_fingerprint(&app->data);
    bool spellbook_changed = app->spellbook_loaded &&
                             pocket_spellbook_fingerprint(&app->data.character) !=
                                 app->saved_spellbook_fingerprint;
    bool items_changed = app->items_loaded &&
                         pocket_items_fingerprint(&app->data.character) !=
                             app->saved_items_fingerprint;
    if(!app->storage_unsaved && fingerprint == app->saved_fingerprint &&
       !spellbook_changed && !items_changed) {
        if(app->autosave_timer) furi_timer_stop(app->autosave_timer);
        app->autosave_pending = 0U;
        return true;
    }
    if(!app->autosave_timer) return pocket_save_now(app, false);
    app->autosave_pending = 1U;
    furi_timer_stop(app->autosave_timer);
    if(furi_timer_start(app->autosave_timer, furi_ms_to_ticks(POCKET_D20_AUTOSAVE_MS)) !=
       FuriStatusOk) {
        app->autosave_pending = 0U;
        return pocket_save_now(app, false);
    }
    return true;
}

static uint16_t pocket_profile_count(const PocketD20App* app) {
    return app->profiles.count;
}

static const PocketProfileEntry* pocket_profile_entry_at(PocketD20App* app, uint16_t list_index) {
    return pocket_d20_profiles_entry_at(app->storage, &app->profiles, list_index);
}

static uint32_t pocket_profile_id_at(PocketD20App* app, uint16_t list_index) {
    const PocketProfileEntry* entry = pocket_profile_entry_at(app, list_index);
    return entry ? entry->id : UINT32_MAX;
}

static bool pocket_profile_exists(PocketD20App* app, uint32_t profile) {
    return pocket_d20_profiles_find(app->storage, profile, NULL);
}

static bool pocket_profile_include_active(PocketD20App* app) {
    if(app->profiles.active_entry_valid &&
       app->profiles.active_entry.id == app->profiles.active_profile)
        return true;
    PocketProfileEntry entry;
    if(!pocket_d20_profiles_find(app->storage, app->profiles.active_profile, &entry)) return false;
    app->profiles.active_entry = entry;
    app->profiles.active_entry_valid = 1U;
    return true;
}

static bool pocket_screen_uses_spellbook(const PocketD20App* app, PocketScreen screen) {
    if(screen == PocketScreenMagic || screen == PocketScreenSpellFilters ||
       screen == PocketScreenSpellAttacks || screen == PocketScreenSpellCast ||
       screen == PocketScreenSpellResult)
        return true;
    if((screen == PocketScreenRecordList || screen == PocketScreenRecordDetail) &&
       app->list_kind == PocketListSpells)
        return true;
    if(screen == PocketScreenCatalog && app->catalog_kind == PocketCatalogSpells) return true;
    return false;
}

static bool pocket_screen_uses_items(const PocketD20App* app, PocketScreen screen) {
    /* Inventory Resources scans the complete sidecar directly and intentionally
       does not hydrate an eight-item UI page. */
    if(screen == PocketScreenAttackList || screen == PocketScreenAttackResult)
        return true;
    if((screen == PocketScreenRecordList || screen == PocketScreenRecordDetail) &&
       app->list_kind == PocketListItems)
        return true;
    if(screen == PocketScreenCatalog && app->catalog_kind == PocketCatalogItems) return true;
    return false;
}

static void pocket_enter_screen(PocketD20App* app, PocketScreen screen) {
    PocketScreen previous = app->screen;
    /* Reclaim screen-local working memory before a pending save allocates file objects/buffers. */
    if(previous == PocketScreenCatalog && screen != PocketScreenCatalog) pocket_catalog_release(app);
    if(previous != screen && app->autosave_pending) pocket_flush_save(app, false);

    bool needs_spellbook = pocket_screen_uses_spellbook(app, screen);
    bool needs_items = pocket_screen_uses_items(app, screen);
    if(screen == PocketScreenResources) {
        app->item_aggregate_valid = 0U;
        PocketD20ItemAggregate aggregate;
        (void)pocket_item_aggregate_streamed(app, &aggregate);
    }
    if(screen == PocketScreenRecordDetail && app->list_kind == PocketListClasses)
        app->spell_class_counts_valid = 0U;
    bool collection_failed = false;
    if(!needs_spellbook && app->spellbook_loaded && !pocket_release_spellbook(app))
        collection_failed = true;
    if(!needs_items && app->items_loaded && !pocket_release_items(app))
        collection_failed = true;
    if(needs_spellbook && !app->spellbook_loaded && !pocket_load_spellbook(app))
        collection_failed = true;
    if(needs_items && !app->items_loaded && !pocket_load_items(app))
        collection_failed = true;
    if(screen == PocketScreenSpellAttacks && !pocket_refresh_combat_spell_index(app))
        collection_failed = true;
    if(screen == PocketScreenAttackList && !pocket_refresh_combat_weapon_index(app))
        collection_failed = true;

    app->screen = screen;
    app->selection = 0U;
    app->scroll = 0U;
    if(screen == PocketScreenProfiles) {
        uint16_t count = pocket_profile_count(app);
        if(count)
            (void)pocket_d20_profiles_window(app->storage, &app->profiles, 0U);
    }
    pocket_clear_action_ack(app);
    app->edit_modifier_mode = 0U;
    pocket_marquee_offset = 0U;
    if(app->storage_unsaved)
        pocket_set_status(app, "UNSAVED - retry SD");
    else if(!collection_failed)
        pocket_clear_status(app);
}

static void pocket_switch_profile(PocketD20App* app, uint32_t profile) {
    if(!pocket_profile_exists(app, profile)) return;
    if(profile == app->profiles.active_profile) {
        pocket_set_status(app, "Already active");
        return;
    }
    if(app->active_profile_loaded && !pocket_flush_save(app, false)) {
        pocket_set_status(app, "Save failed");
        return;
    }
    pocket_d20_data_clear_spells(&app->data.character);
    pocket_d20_data_clear_items(&app->data.character);
    app->spellbook_loaded = 0U;
    app->items_loaded = 0U;
    app->spellbook_total = 0U;
    app->items_total = 0U;
    app->spellbook_cache_start = 0U;
    app->items_cache_start = 0U;
    app->spell_class_counts_valid = 0U;
    app->item_aggregate_valid = 0U;
    uint32_t previous_profile = app->profiles.active_profile;
    app->profiles.active_profile = profile;
    app->arcane_recovery_active = 0U;
    bool recovered_backup = false;
    bool loaded =
        pocket_d20_storage_load_profile(app->storage, profile, &app->data, &recovered_backup);
    app->active_profile_loaded = loaded ? 1U : 0U;
    bool character_ready = loaded;
    if(loaded && recovered_backup)
        character_ready = pocket_d20_storage_restore_backup(app->storage, profile, &app->data);
    if(!loaded) {
        /* Loading a target profile resets the parse buffer to defaults on failure.
           Immediately restore the previously flushed character so transient or
           damaged profiles can never expose/save a synthetic New Hero over it. */
        bool previous_recovered = false;
        app->profiles.active_profile = previous_profile;
        app->active_profile_loaded = pocket_d20_storage_load_profile(
                                         app->storage,
                                         previous_profile,
                                         &app->data,
                                         &previous_recovered) ?
                                         1U :
                                         0U;
        if(app->active_profile_loaded && previous_recovered)
            pocket_d20_storage_restore_backup(app->storage, previous_profile, &app->data);
        app->storage_unsaved = 0U;
        app->saved_fingerprint = pocket_data_fingerprint(&app->data);
        pocket_enter_screen(app, PocketScreenHome);
        pocket_set_status(app, "Profile preserved - load failed");
        return;
    }
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
    if(app->active_profile_loaded && !pocket_flush_save(app, false)) {
        pocket_set_status(app, "Save failed");
        return;
    }
    pocket_d20_data_clear_spells(&app->data.character);
    pocket_d20_data_clear_items(&app->data.character);
    app->spellbook_loaded = 0U;
    app->items_loaded = 0U;
    app->spellbook_total = 0U;
    app->items_total = 0U;
    app->spellbook_cache_start = 0U;
    app->items_cache_start = 0U;
    app->spell_class_counts_valid = 0U;
    app->item_aggregate_valid = 0U;
    uint32_t profile = pocket_d20_profiles_next_id(&app->profiles);
    if(profile == UINT32_MAX &&
       ((app->profiles.reserved_id_seen && app->profiles.highest_reserved_id == UINT32_MAX) ||
        pocket_profile_exists(app, UINT32_MAX))) {
        pocket_set_status(app, "Profile IDs exhausted");
        return;
    }
    uint32_t previous_profile = app->profiles.active_profile;
    app->arcane_recovery_active = 0U;
    pocket_d20_data_clear(&app->data);
    pocket_d20_data_set_defaults(&app->data);
    snprintf(
        app->data.character.name,
        sizeof(app->data.character.name),
        "New Hero %lu",
        (unsigned long)(profile + 1U));
    bool character_saved = pocket_d20_storage_save_profile(app->storage, profile, &app->data);
    if(!character_saved) {
        pocket_d20_storage_delete_profile(app->storage, profile);
        bool recovered = false;
        app->profiles.active_profile = previous_profile;
        app->active_profile_loaded = pocket_d20_storage_load_profile(
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
    app->active_profile_loaded = 1U;
    app->profiles.active_profile = profile;
    pocket_d20_data_clear_spells(&app->data.character);
    pocket_d20_data_clear_items(&app->data.character);
    app->spellbook_loaded = 0U;
    app->items_loaded = 0U;
    app->spellbook_total = 0U;
    app->items_total = 0U;
    app->spellbook_cache_start = 0U;
    app->items_cache_start = 0U;
    app->spell_class_counts_valid = 0U;
    app->item_aggregate_valid = 0U;
    bool metadata_saved = pocket_d20_profiles_refresh(app->storage, &app->profiles);
    pocket_profile_include_active(app);
    metadata_saved = metadata_saved && pocket_d20_profiles_save(app->storage, &app->profiles);
    if(character_saved && metadata_saved)
        app->saved_fingerprint = pocket_data_fingerprint(&app->data);
    pocket_enter_screen(app, PocketScreenCharacter);
    pocket_set_status(app, character_saved && metadata_saved ? "New character" : "Save failed");
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
        app, metadata_saved && character_deleted ? "Character deleted" : "Delete failed");
}

static uint8_t pocket_wizard_level(const PocketCharacter* character) {
    for(uint8_t i = 0U; i < character->class_count; ++i)
        if(strcmp(character->classes[i].name, "Wizard") == 0) return character->classes[i].level;
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

static bool
    pocket_subclass_allowed(const PocketD20App* app, uint16_t class_mask, bool has_metadata) {
    if(app->catalog_show_all) return true;
    if(!has_metadata || app->record_index >= app->data.character.class_count) return false;
    uint16_t selected_class =
        pocket_class_mask_from_name(app->data.character.classes[app->record_index].name);
    return selected_class && (class_mask & selected_class);
}

static bool pocket_spell_class_allows(
    const PocketCharacter* character, uint8_t class_index, uint8_t level, uint16_t class_mask) {
    if(!character || class_index >= character->class_count) return false;
    const PocketClassLevel* class_level = &character->classes[class_index];
    uint16_t selected_class = pocket_class_mask_from_name(class_level->name);
    return selected_class && (class_mask & selected_class) &&
           level <= pocket_d20_class_max_spell_level(class_level);
}

static bool pocket_spell_allowed(
    PocketD20App* app,
    uint8_t level,
    uint16_t class_mask,
    bool has_metadata) {
    if(app->catalog_show_all) return true;
    if(!has_metadata) return false;

    /* A concrete class filter restricts the catalog to that class. All Classes
       is the union of spells currently eligible for every class on the character,
       which is the intended multiclass default. It must not depend on the class
       currently stored on the spell being edited. */
    if(app->spell_filter_class < app->data.character.class_count)
        return pocket_spell_class_allows(
            &app->data.character, app->spell_filter_class, level, class_mask);

    for(uint8_t class_index = 0U; class_index < app->data.character.class_count; ++class_index)
        if(pocket_spell_class_allows(&app->data.character, class_index, level, class_mask))
            return true;
    return false;
}

static uint8_t pocket_spell_resolve_class(
    const PocketD20App* app, uint8_t level, uint16_t class_mask, uint8_t preferred) {
    const PocketCharacter* character = &app->data.character;
    if(app->spell_filter_class < character->class_count &&
       pocket_spell_class_allows(character, app->spell_filter_class, level, class_mask))
        return app->spell_filter_class;
    if(preferred < character->class_count &&
       pocket_spell_class_allows(character, preferred, level, class_mask))
        return preferred;
    for(uint8_t class_index = 0U; class_index < character->class_count; ++class_index)
        if(pocket_spell_class_allows(character, class_index, level, class_mask))
            return class_index;
    return preferred < character->class_count ? preferred : 0U;
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
    } else if(
        app->catalog_kind == PocketCatalogSubclasses &&
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
    return app->catalog_scan_count > app->catalog_page_start + pocket_catalog_page_limit(app);
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
    WEAPON(
        "Dagger",
        10,
        1,
        4,
        0,
        PocketDamagePiercing,
        PocketWeaponFinesse | PocketWeaponLight | PocketWeaponThrown,
        ""),
    WEAPON("Greatclub", 100, 1, 8, 0, PocketDamageBludgeoning, 0U, ""),
    WEAPON("Handaxe", 20, 1, 6, 0, PocketDamageSlashing, PocketWeaponLight | PocketWeaponThrown, ""),
    WEAPON("Javelin", 20, 1, 6, 0, PocketDamagePiercing, PocketWeaponThrown, ""),
    WEAPON(
        "Light Hammer",
        20,
        1,
        4,
        0,
        PocketDamageBludgeoning,
        PocketWeaponLight | PocketWeaponThrown,
        ""),
    WEAPON("Mace", 40, 1, 6, 0, PocketDamageBludgeoning, 0U, ""),
    WEAPON("Quarterstaff", 40, 1, 6, 8, PocketDamageBludgeoning, 0U, ""),
    WEAPON("Sickle", 20, 1, 4, 0, PocketDamageSlashing, PocketWeaponLight, ""),
    WEAPON("Spear", 30, 1, 6, 8, PocketDamagePiercing, PocketWeaponThrown, ""),
    WEAPON(
        "Dart",
        3,
        1,
        4,
        0,
        PocketDamagePiercing,
        PocketWeaponFinesse | PocketWeaponRanged | PocketWeaponThrown,
        ""),
    WEAPON(
        "Light Crossbow",
        50,
        1,
        8,
        0,
        PocketDamagePiercing,
        PocketWeaponRanged | PocketWeaponAmmunition,
        "Bolts"),
    WEAPON(
        "Shortbow",
        20,
        1,
        6,
        0,
        PocketDamagePiercing,
        PocketWeaponRanged | PocketWeaponAmmunition,
        "Arrows"),
    WEAPON(
        "Sling",
        0,
        1,
        4,
        0,
        PocketDamageBludgeoning,
        PocketWeaponRanged | PocketWeaponAmmunition,
        "Sling bullets"),
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
    WEAPON(
        "Scimitar",
        30,
        1,
        6,
        0,
        PocketDamageSlashing,
        PocketWeaponFinesse | PocketWeaponLight,
        ""),
    WEAPON(
        "Shortsword",
        20,
        1,
        6,
        0,
        PocketDamagePiercing,
        PocketWeaponFinesse | PocketWeaponLight,
        ""),
    WEAPON("Trident", 40, 1, 8, 10, PocketDamagePiercing, PocketWeaponThrown, ""),
    WEAPON("Warhammer", 50, 1, 8, 10, PocketDamageBludgeoning, 0U, ""),
    WEAPON("War Pick", 20, 1, 8, 10, PocketDamagePiercing, 0U, ""),
    WEAPON("Whip", 30, 1, 4, 0, PocketDamageSlashing, PocketWeaponFinesse, ""),
    WEAPON(
        "Blowgun",
        10,
        1,
        1,
        0,
        PocketDamagePiercing,
        PocketWeaponRanged | PocketWeaponAmmunition,
        "Needles"),
    WEAPON(
        "Hand Crossbow",
        30,
        1,
        6,
        0,
        PocketDamagePiercing,
        PocketWeaponRanged | PocketWeaponLight | PocketWeaponAmmunition,
        "Bolts"),
    WEAPON(
        "Heavy Crossbow",
        180,
        1,
        10,
        0,
        PocketDamagePiercing,
        PocketWeaponRanged | PocketWeaponHeavy | PocketWeaponAmmunition,
        "Bolts"),
    WEAPON(
        "Longbow",
        20,
        1,
        8,
        0,
        PocketDamagePiercing,
        PocketWeaponRanged | PocketWeaponHeavy | PocketWeaponAmmunition,
        "Arrows"),
    WEAPON(
        "Musket",
        100,
        1,
        12,
        0,
        PocketDamagePiercing,
        PocketWeaponRanged | PocketWeaponAmmunition,
        "Bullets"),
    WEAPON(
        "Pistol",
        30,
        1,
        10,
        0,
        PocketDamagePiercing,
        PocketWeaponRanged | PocketWeaponAmmunition,
        "Bullets"),
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

static void pocket_apply_equipment_preset(PocketItem* item, const char* name, uint8_t category) {
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
            item->ammunition_group, sizeof(item->ammunition_group), preset->ammunition_group);
        break;
    }
}

static void
    pocket_catalog_add_item(PocketD20App* app, const char* name, uint8_t category, bool magic) {
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
    while(*start == ' ' || *start == '\t')
        ++start;
    char* end = start + strlen(start);
    while(end > start && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r'))
        --end;
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
            while(*rarity == ' ' || *rarity == '\t')
                ++rarity;
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
        while(*class_name == ' ' || *class_name == '\t')
            ++class_name;
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
    uint32_t parsed_level = 0U;
    if(!pocket_parse_u32_strict(level_separator + 1U, 9U, &parsed_level)) {
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
        while(*class_name == ' ' || *class_name == '\t')
            ++class_name;
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
            if(ritual)
                app->catalog_ritual[i] = strcmp(ritual, "1") == 0 || strcmp(ritual, "Yes") == 0;
            break;
        }
    }
}

static bool pocket_catalog_load_path(PocketD20App* app, const char* path) {
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



static bool pocket_class_asi_level(const char* class_name, uint8_t level) {
    if(level == 4U || level == 8U || level == 12U || level == 16U || level == 19U) return true;
    if(class_name && strcmp(class_name, "Fighter") == 0 && (level == 6U || level == 14U)) return true;
    if(class_name && strcmp(class_name, "Rogue") == 0 && level == 10U) return true;
    return false;
}

static void pocket_level_choice_id(char* out, size_t size, const PocketCharacter* c, uint8_t class_index, uint8_t level) {
    const char* name = class_index < c->class_count ? c->classes[class_index].name : "class";
    snprintf(out, size, "asi_%.12s_%u", name, level);
    for(char* p = out; *p; ++p) if(*p == ' ') *p = '_';
}

static bool pocket_level_choice_done(const PocketCharacter* c, uint8_t class_index, uint8_t level) {
    char id[POCKET_D20_SHORT_LEN];
    pocket_level_choice_id(id, sizeof(id), c, class_index, level);
    for(uint8_t i = 0U; i < c->grant_count; ++i)
        if(strcmp(c->grants[i].stable_id, id) == 0 && c->grants[i].status == PocketGrantApplied) return true;
    return false;
}

static bool pocket_begin_next_level_choice(PocketD20App* app) {
    PocketCharacter* c = &app->data.character;
    for(uint8_t ci = 0U; ci < c->class_count; ++ci) {
        for(uint8_t level = 1U; level <= c->classes[ci].level; ++level) {
            if(pocket_class_asi_level(c->classes[ci].name, level) && !pocket_level_choice_done(c, ci, level)) {
                app->level_choice_class_index = ci;
                app->level_choice_level = level;
                app->level_choice_mode = 0U;
                app->level_choice_first_ability = UINT8_MAX;
                return true;
            }
        }
    }
    return false;
}

static bool pocket_complete_level_choice(PocketD20App* app, const char* result) {
    PocketCharacter* c = &app->data.character;
    char id[POCKET_D20_SHORT_LEN];
    pocket_level_choice_id(id, sizeof(id), c, app->level_choice_class_index, app->level_choice_level);
    for(uint8_t i = 0U; i < c->grant_count; ++i) if(strcmp(c->grants[i].stable_id, id) == 0) return true;
    if(c->grant_count >= POCKET_D20_MAX_GRANTS || !pocket_d20_data_reserve_grants(c, c->grant_count + 1U)) return false;
    PocketGrant* g = &c->grants[c->grant_count++];
    memset(g, 0, sizeof(*g));
    pocket_copy(g->stable_id, sizeof(g->stable_id), id);
    pocket_copy(g->source, sizeof(g->source), "Level Choice");
    pocket_copy(g->option_name, sizeof(g->option_name), result ? result : "ASI/Feat");
    pocket_copy(g->prerequisites, sizeof(g->prerequisites), "Class level");
    pocket_copy(g->grant_value, sizeof(g->grant_value), "choice=completed");
    g->source_type = PocketGrantFeat;
    g->class_index = app->level_choice_class_index;
    g->level_gained = app->level_choice_level;
    g->status = PocketGrantApplied;
    pocket_save(app, false);
    return true;
}

static bool pocket_grant_stable_id_exists(const PocketCharacter* character, const char* stable_id) {
    if(!character || !stable_id || !stable_id[0]) return false;
    for(uint8_t i = 0U; i < character->grant_count; ++i)
        if(strcmp(character->grants[i].stable_id, stable_id) == 0) return true;
    return false;
}

static uint8_t pocket_stage_grants(PocketD20App* app, uint8_t source_type, const char* option) {
    PocketCharacter* character = &app->data.character;
    File* file = storage_file_alloc(app->storage);
    if(!file) return 0U;
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
            if(!line[0] || line[0] == '#' || character->grant_count >= POCKET_D20_MAX_GRANTS)
                continue;
            char* fields[8];
            if(pocket_split_metadata(line, fields) != 8U ||
               pocket_grant_source_from_text(fields[2]) != source_type ||
               strcmp(fields[3], option) != 0 || !fields[7][0] ||
               pocket_grant_stable_id_exists(character, fields[0]))
                continue;
            uint32_t level_gained = 0U;
            if(!pocket_parse_u32_strict(fields[5], UINT8_MAX, &level_gained)) continue;
            if(source_type == PocketGrantClassFeature || source_type == PocketGrantSubclassFeature) {
                uint8_t class_index = app->record_index < character->class_count ? app->record_index : 0U;
                if(level_gained == 0U || level_gained > character->classes[class_index].level) continue;
            }
            if(!pocket_d20_data_reserve_grants(character, character->grant_count + 1U)) continue;
            PocketGrant* grant = &character->grants[character->grant_count++];
            memset(grant, 0, sizeof(*grant));
            pocket_copy(grant->stable_id, sizeof(grant->stable_id), fields[0]);
            pocket_copy(grant->source, sizeof(grant->source), fields[1]);
            pocket_copy(grant->option_name, sizeof(grant->option_name), fields[3]);
            pocket_copy(grant->prerequisites, sizeof(grant->prerequisites), fields[4]);
            pocket_copy(grant->grant_value, sizeof(grant->grant_value), fields[7]);
            grant->source_type = source_type;
            grant->class_index = app->record_index < character->class_count ? app->record_index :
                                                                              0U;
            grant->level_gained = (uint8_t)level_gained;
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
    bool spell_collection_opened = false;
    bool applied = false;
    if(strcmp(payload, "spell") == 0 && !app->spellbook_loaded) {
        if(!pocket_load_spellbook(app)) {
            *separator = '=';
            grant->status = PocketGrantSkipped;
            return;
        }
        spell_collection_opened = true;
    }
    if(strcmp(payload, "origin_feat") == 0) {
        pocket_copy(character->origin_feat, sizeof(character->origin_feat), value);
        applied = true;
    } else if(strcmp(payload, "tool") == 0) {
        pocket_copy(character->tool_proficiencies, sizeof(character->tool_proficiencies), value);
        applied = true;
    } else if(strcmp(payload, "armor") == 0) {
        pocket_copy(character->armor_training, sizeof(character->armor_training), value);
        applied = true;
    } else if(strcmp(payload, "weapon") == 0) {
        pocket_copy(character->weapon_training, sizeof(character->weapon_training), value);
        applied = true;
    } else if(strcmp(payload, "senses") == 0) {
        pocket_copy(character->senses, sizeof(character->senses), value);
        applied = true;
    } else if(strcmp(payload, "size") == 0) {
        for(uint8_t i = 0U; i < PocketSizeCount; ++i) {
            if(strcmp(value, pocket_size_names[i]) == 0) {
                character->size = i;
                applied = true;
                break;
            }
        }
    } else if(strcmp(payload, "feature") == 0) {
        if(character->feature_count < POCKET_D20_MAX_FEATURES &&
           pocket_d20_data_reserve_features(character, character->feature_count + 1U)) {
            PocketFeature* feature = &character->features[character->feature_count++];
            memset(feature, 0, sizeof(*feature));
            pocket_copy(feature->name, sizeof(feature->name), value);
            feature->class_index = grant->class_index;
            feature->class_level_gained = grant->level_gained;
            applied = true;
        }
    } else if(strcmp(payload, "spell") == 0 && app->spellbook_total < POCKET_D20_MAX_SPELLS) {
        PocketSpell spell;
        memset(&spell, 0, sizeof(spell));
        pocket_copy(spell.name, sizeof(spell.name), value);
        pocket_copy(spell.source, sizeof(spell.source), grant->source);
        pocket_copy(spell.grant_name, sizeof(spell.grant_name), grant->option_name);
        pocket_copy(spell.stable_id, sizeof(spell.stable_id), grant->stable_id);
        spell.class_index = grant->class_index;
        spell.grant_source = grant->source_type;
        if(pocket_save_spellbook_if_changed(app) &&
           pocket_d20_storage_append_spell(
               app->storage,
               app->profiles.active_profile,
               character,
               &spell,
               1U,
               1U,
               0U,
               0U)) {
            app->spell_class_counts_valid = 0U;
            ++app->spellbook_total;
            applied = true;
        }
    }
    *separator = '=';
    grant->status = applied ? PocketGrantApplied : PocketGrantSkipped;
    if(spell_collection_opened) pocket_release_spellbook(app);
}

static uint8_t pocket_apply_level_grants(PocketD20App* app, uint8_t class_index) {
    PocketCharacter* c = &app->data.character;
    if(class_index >= c->class_count) return 0U;
    uint8_t saved_index = app->record_index;
    app->record_index = class_index;
    uint8_t before = c->grant_count;
    pocket_stage_grants(app, PocketGrantClassFeature, c->classes[class_index].name);
    if(c->classes[class_index].subclass[0] && strcmp(c->classes[class_index].subclass, "None") != 0)
        pocket_stage_grants(app, PocketGrantSubclassFeature, c->classes[class_index].subclass);
    uint8_t applied = 0U;
    for(uint8_t i = before; i < c->grant_count; ++i) {
        if(c->grants[i].status != PocketGrantPending) continue;
        pocket_apply_grant(app, &c->grants[i]);
        if(c->grants[i].status == PocketGrantApplied) ++applied;
    }
    app->record_index = saved_index;
    return applied;
}

static bool pocket_tracked_spell_matches_filter(PocketD20App* app, const char* name) {
    if(!app->spell_filter_prepared) return true;
    PocketCharacter* character = &app->data.character;
    for(uint8_t i = 0U; i < app->spellbook_total; ++i) {
        uint8_t local = 0U;
        PocketSpell* spell = pocket_spell_at(app, i, &local);
        if(!spell || strcmp(spell->name, name) != 0) continue;
        if(app->spell_filter_prepared == 1U) return spell->prepared;
        if(app->spell_filter_prepared == 2U) return character->spell_known[local];
        return character->spell_always_prepared[local];
    }
    return false;
}

static void pocket_catalog_apply_spell_filters(PocketD20App* app) {
    if(app->catalog_kind != PocketCatalogSpells) return;
    uint16_t output = 0U;
    for(uint16_t i = 0U; i < app->catalog_count; ++i) {
        bool keep = true;
        if(app->spell_filter_level >= 0 &&
           app->catalog_levels[i] != (uint8_t)app->spell_filter_level)
            keep = false;
        if(app->spell_filter_ritual && !app->catalog_ritual[i]) keep = false;
        if(app->spell_filter_school && app->catalog_schools[i] != app->spell_filter_school)
            keep = false;
        if(app->spell_filter_source == 1U && app->catalog_sources[i] != 1U) keep = false;
        if(app->spell_filter_source == 2U && app->catalog_sources[i] != 2U) keep = false;
        if(!pocket_tracked_spell_matches_filter(app, app->catalog_entries[i])) keep = false;
        if(!keep) continue;
        if(output != i) {
            memcpy(
                app->catalog_entries[output],
                app->catalog_entries[i],
                POCKET_D20_CATALOG_NAME_LEN);
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

static bool pocket_catalog_spell_after(const PocketD20App* app, uint16_t left, uint16_t right) {
    uint8_t left_level = app->catalog_has_metadata[left] ? app->catalog_levels[left] : UINT8_MAX;
    uint8_t right_level = app->catalog_has_metadata[right] ? app->catalog_levels[right] :
                                                             UINT8_MAX;
    if(left_level != right_level) return left_level > right_level;
    return strcmp(app->catalog_entries[left], app->catalog_entries[right]) > 0;
}

static void pocket_catalog_swap(PocketD20App* app, uint16_t left, uint16_t right) {
    char name[POCKET_D20_CATALOG_NAME_LEN];
    memcpy(name, app->catalog_entries[left], sizeof(name));
    memcpy(app->catalog_entries[left], app->catalog_entries[right], sizeof(name));
    memcpy(app->catalog_entries[right], name, sizeof(name));
#define POCKET_SWAP_VALUE(values, type)   \
    do {                                  \
        type temporary = (values)[left];  \
        (values)[left] = (values)[right]; \
        (values)[right] = temporary;      \
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
            ((app->catalog_total ? app->catalog_total - 1U : 0U) / page_limit) * page_limit;
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

static void pocket_draw_row(Canvas* canvas, uint8_t row, bool selected, const char* text) {
    uint8_t y = (uint8_t)(11U + (row * 10U));
    char display[32];
    size_t length = strlen(text);
    if(selected && length > 25U) {
        size_t cycle = length + 4U;
        size_t start = pocket_marquee_offset % cycle;
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

static void pocket_draw_menu_rows(
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
            pocket_copy(confirmed, sizeof(confirmed), row);
            pocket_prefix_action_mark(confirmed, sizeof(confirmed));
            row = confirmed;
        }
        pocket_draw_row(canvas, visible, index == app->selection, row);
    }
}

static uint8_t pocket_list_count(const PocketD20App* app) {
    const PocketCharacter* character = &app->data.character;
    switch(app->list_kind) {
    case PocketListClasses:
        return character->class_count;
    case PocketListSpells:
        return app->spellbook_total;
    case PocketListFeatures:
        return character->feature_count;
    case PocketListItems:
        return app->items_total;
    case PocketListLanguages:
        return character->language_count;
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
    default:
        return "List";
    }
}

static void
    pocket_format_list_entry(PocketD20App* app, uint8_t index, char* output, size_t size) {
    PocketCharacter* character = &app->data.character;
    switch(app->list_kind) {
    case PocketListClasses: {
        const PocketClassLevel* class_level = &character->classes[index];
        snprintf(output, size, "%.31s L%u", class_level->name, class_level->level);
        break;
    }
    case PocketListSpells: {
        uint8_t local = 0U;
        PocketSpell* spell = pocket_spell_at_cached(app, index, &local);
        if(!spell) {
            pocket_copy(output, size, "<read error>");
            break;
        }
        snprintf(
            output,
            size,
            "%c%c L%u %.31s",
            pocket_spell_status(character, local),
            character->spell_free_casts_current[local] ? 'F' : ' ',
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
        PocketItem* item = pocket_item_at_cached(app, index, NULL);
        if(!item) {
            pocket_copy(output, size, "<read error>");
            break;
        }
        snprintf(
            output, size, "%c %dx %.31s", item->equipped ? '*' : ' ', item->quantity, item->name);
        break;
    }
    case PocketListLanguages:
        pocket_format_labeled_text(output, size, NULL, character->languages[index]);
        break;
    }
}

static bool pocket_refresh_combat_weapon_index(PocketD20App* app) {
    uint8_t total = 0U;
    if(!pocket_d20_items_collect_weapon_indices(
           app->storage,
           app->profiles.active_profile,
           app->combat_weapon_indices,
           POCKET_D20_MAX_ITEMS,
           &app->combat_weapon_count,
           &total)) {
        pocket_set_status(app, "Items read failed");
        return false;
    }
    app->items_total = total;
    return true;
}

static uint8_t pocket_weapon_count(PocketD20App* app) {
    return app->combat_weapon_count;
}

static uint8_t pocket_weapon_index(PocketD20App* app, uint8_t weapon_number) {
    if(weapon_number >= app->combat_weapon_count) return 0xFFU;
    return app->combat_weapon_indices[weapon_number];
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
        app->text_input, pocket_text_done, app, app->edit_buffer, sizeof(app->edit_buffer), false);
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
    PocketNumberContext completed_context = app->number_context;
    switch(app->number_context) {
    case PocketNumberCurrency: {
        int32_t* values[] = {
            &character->currency_cp,
            &character->currency_sp,
            &character->currency_ep,
            &character->currency_gp,
            &character->currency_pp};
        if(app->number_index < 5U) *values[app->number_index] = number;
        break;
    }
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
            character->hit_die = pocket_nearest_die(number, true);
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
        if(app->record_index >= pocket_list_count(app)) break;
        if(app->list_kind == PocketListClasses) {
            uint8_t previous_total_level = pocket_d20_total_level(character);
            PocketClassLevel* level = &character->classes[app->record_index];
            uint8_t previous_cantrip_limit = level->cantrip_limit;
            uint8_t previous_prepared_limit = level->prepared_limit;
            switch(app->number_index) {
            case 2U:
                level->level = (uint8_t)number;
                break;
            case 3U:
                level->hit_die = pocket_nearest_die(number, true);
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
            if(app->number_index == 2U &&
               pocket_d20_total_level(character) > previous_total_level) {
                dndolphins_apply_experience_floor(character);
                pocket_d20_apply_level_progression(character, app->record_index);
                uint8_t applied = pocket_apply_level_grants(app, app->record_index);
                bool choose_spells = level->cantrip_limit > previous_cantrip_limit ||
                                     level->prepared_limit > previous_prepared_limit;
                bool choose_level = pocket_begin_next_level_choice(app);
                snprintf(
                    app->status,
                    sizeof(app->status),
                    choose_level ? "Level up: ASI/feat choice" : choose_spells ? "Level up: choose spells" :
                    applied ? "Level up: %u trait(s)" : "Level up: rules updated",
                    applied);
            }
        } else if(app->list_kind == PocketListSpells) {
            uint8_t local = 0U;
            PocketSpell* spell = pocket_spell_at(app, app->record_index, &local);
            if(spell) {
                if(app->number_index == 3U)
                    spell->level = (uint8_t)number;
                else if(app->number_index == 8U)
                    character->spell_free_casts_current[local] = (uint8_t)number;
                else if(app->number_index == 9U)
                    character->spell_free_casts_max[local] = (uint8_t)number;
                if(character->spell_free_casts_current[local] >
                   character->spell_free_casts_max[local])
                    character->spell_free_casts_current[local] =
                        character->spell_free_casts_max[local];
            }
        } else if(app->list_kind == PocketListFeatures) {
            PocketFeature* feature = &character->features[app->record_index];
            if(app->number_index == 3U)
                feature->class_level_gained = (uint8_t)number;
            else if(app->number_index == 4U)
                feature->uses_current = (int16_t)number;
            else if(app->number_index == 5U)
                feature->uses_max = (int16_t)number;
            if(feature->uses_current > feature->uses_max)
                feature->uses_current = feature->uses_max;
        } else if(app->list_kind == PocketListItems) {
            PocketItem* item = pocket_item_at(app, app->record_index, NULL);
            if(!item) return;
            switch(app->number_index) {
            case 2U:
                item->quantity = (int16_t)number;
                break;
            case 3U:
                item->weight_tenths = (int16_t)number;
                break;
            case 9U:
                item->magic_bonus = (int8_t)number;
                break;
            case 10U:
                item->damage_dice = (uint8_t)number;
                break;
            case 11U:
                item->damage_die = pocket_nearest_die(number, true);
                break;
            case 13U:
                item->versatile_die = pocket_nearest_die(number, true);
                break;
            case 23U:
                item->extra_dice = (uint8_t)number;
                break;
            case 24U:
                item->extra_die = pocket_nearest_die(number, true);
                break;
            case 25U:
                item->ammo_current = (int16_t)number;
                break;
            case 26U:
                item->ammo_max = (int16_t)number;
                break;
            case 29U:
                item->charges_current = (int16_t)number;
                break;
            case 30U:
                item->charges_max = (int16_t)number;
                break;
            case 31U:
                item->armor_base = (uint8_t)number;
                break;
            case 32U:
                item->armor_dex_cap = (int8_t)number;
                break;
            case 33U:
                item->shield_bonus = (uint8_t)number;
                break;
            }
            if(item->ammo_current > item->ammo_max) item->ammo_current = item->ammo_max;
            if(item->charges_current > item->charges_max)
                item->charges_current = item->charges_max;
        }
        break;
    case PocketNumberDice:
        if(app->number_index == 0U)
            app->dice_count = (uint8_t)number;
        else if(app->number_index == 1U)
            app->dice_sides = pocket_nearest_die(number, false);
        else if(app->number_index == 2U)
            app->dice_modifier = (int16_t)number;
        app->roll_mode = PocketRollNormal;
        app->dice_roll_value_count = 0U;
        break;
    case PocketNumberCombat:
        if(app->number_index == 5U)
            character->hp_current = (int16_t)number;
        else if(app->number_index == 6U)
            character->hp_temporary = (int16_t)number;
        else if(app->number_index == 19U)
            character->death_successes = (uint8_t)number;
        else if(app->number_index == 20U)
            character->death_failures = (uint8_t)number;
        else if(app->number_index == 21U)
            character->exhaustion = (uint8_t)number;
        break;
    case PocketNumberNone:
        break;
    }
    if(completed_context == PocketNumberRecord) {
        if(app->list_kind == PocketListItems)
            (void)pocket_save_items_if_changed(app);
        else if(app->list_kind == PocketListSpells)
            (void)pocket_save_spellbook_if_changed(app);
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
        app->number_input, pocket_number_done, app, value, minimum, maximum);
    view_dispatcher_switch_to_view(app->dispatcher, PocketViewNumberInput);
}

static uint16_t pocket_home_count(const PocketD20App* app) {
    return (uint16_t)(sizeof(pocket_home_items) / sizeof(pocket_home_items[0])) +
           (app->storage_unsaved ? 1U : 0U);
}

static const char* pocket_home_item_at(uint16_t index) {
    const uint16_t base_count = sizeof(pocket_home_items) / sizeof(pocket_home_items[0]);
    if(index < base_count) return pocket_home_items[index];
    return pocket_home_retry_save;
}

static void pocket_draw_home(Canvas* canvas, PocketD20App* app) {
    char title[48];
    snprintf(title, sizeof(title), "D&D v" FAP_VERSION " - %.27s", app->data.character.name);
    pocket_draw_header(canvas, title, app->status);
    uint16_t count = pocket_home_count(app);
    for(uint8_t visible = 0U; visible < 5U; ++visible) {
        uint16_t index = app->scroll + visible;
        if(index >= count) break;
        pocket_draw_row(canvas, visible, index == app->selection, pocket_home_item_at(index));
    }
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
            const PocketProfileEntry* entry = pocket_profile_entry_at(app, index);
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
                pocket_copy(row, sizeof(row), "Character unavailable");
            }
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
    char rows[14][48];
    const char* row_ptrs[14];
    for(uint8_t i = 0U; i < 14U; ++i)
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
    snprintf(rows[12], sizeof(rows[12]), "Level Choices");
    snprintf(rows[13], sizeof(rows[13]), "Grant Initial Traits");
    pocket_draw_header(canvas, "Character", app->status);
    pocket_draw_menu_rows(canvas, app, row_ptrs, 14U);
}

static void pocket_draw_vitals(Canvas* canvas, PocketD20App* app) {
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
            pocket_d20_effective_speed(character));
    else
        snprintf(rows[4], sizeof(rows[4]), "Speed: %d ft", character->speed);
    snprintf(
        rows[5], sizeof(rows[5]), "Initiative: %+d", pocket_d20_initiative_modifier(character));
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
                        grant->status == PocketGrantSkipped ? 'S' :
                                                              '?';
            snprintf(
                row, sizeof(row), "%c %.28s: %.28s", mark, grant->option_name, grant->grant_value);
        } else {
            pocket_copy(row, sizeof(row), "+ Add Custom Grant");
        }
        pocket_draw_row(canvas, visible, row_index == app->selection, row);
    }
}

static void pocket_draw_level_choice(Canvas* canvas, PocketD20App* app) {
    char title[48];
    const PocketCharacter* c = &app->data.character;
    const char* class_name = app->level_choice_class_index < c->class_count ?
                                 c->classes[app->level_choice_class_index].name : "Class";
    snprintf(title, sizeof(title), "%.18s L%u choice", class_name, app->level_choice_level);
    pocket_draw_header(canvas, title, app->status);
    static const char* const rows[] = {"ASI +2 one ability", "ASI +1 two abilities", "Choose Feat", "Later"};
    pocket_draw_menu_rows(canvas, app, rows, 4U);
}

static void pocket_draw_asi_ability(Canvas* canvas, PocketD20App* app) {
    PocketCharacter* c = &app->data.character;
    char rows[POCKET_D20_ABILITY_COUNT][32];
    const char* ptrs[POCKET_D20_ABILITY_COUNT];
    for(uint8_t i = 0U; i < POCKET_D20_ABILITY_COUNT; ++i) {
        snprintf(rows[i], sizeof(rows[i]), "%s: %d", pocket_d20_ability_names[i], c->ability_scores[i]);
        ptrs[i] = rows[i];
    }
    pocket_draw_header(canvas, app->level_choice_mode == 1U ? "ASI +2: choose ability" :
                               app->level_choice_first_ability < POCKET_D20_ABILITY_COUNT ? "ASI +1: second ability" : "ASI +1: first ability", app->status);
    pocket_draw_menu_rows(canvas, app, ptrs, POCKET_D20_ABILITY_COUNT);
}

static void pocket_draw_grant_edit(Canvas* canvas, PocketD20App* app) {
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
    pocket_draw_header(canvas, "Structured Grant Editor", app->status);
    pocket_draw_menu_rows(canvas, app, rows, 10U);
}


static void pocket_draw_spell_filters(Canvas* canvas, PocketD20App* app) {
    const PocketCharacter* c = &app->data.character;
    char rows[6][48];
    const char* ptrs[6] = {rows[0], rows[1], rows[2], rows[3], rows[4], rows[5]};
    snprintf(
        rows[0],
        sizeof(rows[0]),
        "Level: %s",
        app->spell_filter_level < 0  ? "Any" :
        app->spell_filter_level == 0 ? "Cantrip" :
                                       "1-9 selected");
    snprintf(
        rows[1],
        sizeof(rows[1]),
        "Class: %s",
        app->spell_filter_class < c->class_count ? c->classes[app->spell_filter_class].name :
                                                   "All Classes");
    snprintf(rows[2], sizeof(rows[2]), "Ritual: %s", app->spell_filter_ritual ? "Only" : "Any");
    snprintf(
        rows[3],
        sizeof(rows[3]),
        "School: %s",
        pocket_spell_school_names[app->spell_filter_school]);
    snprintf(
        rows[4],
        sizeof(rows[4]),
        "Source: %s",
        app->spell_filter_source == 1U ? "Core" :
        app->spell_filter_source == 2U ? "Add-on" :
                                         "Any");
    snprintf(
        rows[5],
        sizeof(rows[5]),
        "Status: %s",
        app->spell_filter_prepared == 1U ? "Prepared" :
        app->spell_filter_prepared == 2U ? "Known" :
        app->spell_filter_prepared == 3U ? "Always" :
                                           "Any");
    pocket_draw_header(canvas, "Spell Filters: <>", app->status);
    pocket_draw_menu_rows(canvas, app, ptrs, 6U);
}

static void pocket_draw_resources(Canvas* canvas, PocketD20App* app) {
    const PocketCharacter* c = &app->data.character;
    bool aggregate_ok = app->item_aggregate_valid != 0U;
    const PocketD20ItemAggregate* aggregate = &app->item_aggregate;
    int16_t carried = aggregate_ok ? aggregate->carried_weight_tenths : 0;
    int16_t equipped = aggregate_ok ? aggregate->equipped_weight_tenths : 0;
    uint8_t attuned = aggregate_ok ? aggregate->attuned_count : 0U;
    int16_t formula_ac = aggregate_ok ? pocket_d20_items_calculated_armor_class(c, aggregate) :
                                        c->armor_class;
    char rows[9][48];
    const char* ptrs[9];
    for(uint8_t i = 0U; i < 9U; ++i)
        ptrs[i] = rows[i];
    snprintf(
        rows[0],
        sizeof(rows[0]),
        "Carried: %d.%d lb",
        carried / 10,
        abs(carried % 10));
    snprintf(
        rows[1],
        sizeof(rows[1]),
        "Equipped: %d.%d lb",
        equipped / 10,
        abs(equipped % 10));
    snprintf(rows[2], sizeof(rows[2]), "Capacity: %d lb", pocket_d20_carrying_capacity(c));
    snprintf(
        rows[3], sizeof(rows[3]), "Encumbrance: %s", c->encumbrance_mode ? "Variant" : "Standard");
    snprintf(
        rows[4],
        sizeof(rows[4]),
        "Attuned: %u/3%s",
        attuned,
        attuned > 3U ? " !" : "");
    snprintf(rows[5], sizeof(rows[5]), "Formula AC: %d", formula_ac);
    snprintf(rows[6], sizeof(rows[6]), "Apply armor/shield AC");
    snprintf(rows[7], sizeof(rows[7]), "Normalize coin values");
    snprintf(rows[8], sizeof(rows[8]), "Capacity override: %d", c->carrying_capacity_override);
    pocket_draw_header(canvas, "Inventory Resources", app->status);
    pocket_draw_menu_rows(canvas, app, ptrs, 9U);
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
        } else
            break;
        pocket_draw_row(canvas, visible, index == app->selection, row);
    }
}

static void pocket_draw_attack_template_edit(Canvas* canvas, PocketD20App* app) {
    if(app->record_index >= app->data.character.attack_template_count) return;
    const PocketAttackTemplate* attack = &app->data.character.attack_templates[app->record_index];
    char type[32], ability[32], save[32], attack_misc[24], dc[24], damage_dice[24], damage_die[24],
        rider_dice[24], rider_die[24], recharge[32];
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
    pocket_draw_header(canvas, "Attack Template Editor", app->status);
    pocket_draw_menu_rows(canvas, app, rows, 15U);
}

static void pocket_draw_magic(Canvas* canvas, PocketD20App* app) {
    PocketCharacter* character = &app->data.character;
    char rows[16][48];
    const char* row_ptrs[16];
    for(uint8_t i = 0U; i < 16U; ++i)
        row_ptrs[i] = rows[i];
    snprintf(rows[0], sizeof(rows[0]), "Spells (%u) / hold: filters", app->spellbook_total);
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
    snprintf(rows[3], sizeof(rows[3]), "Spell attack misc: %+d", character->spell_attack_misc);
    snprintf(rows[4], sizeof(rows[4]), "Spell save misc: %+d", character->spell_save_misc);
    snprintf(rows[5], sizeof(rows[5]), "Slots: <> avail / hold <> max");
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
    pocket_draw_header(
        canvas, app->arcane_recovery_active ? "Magic: Arcane Recovery" : "Magic", app->status);
    pocket_draw_menu_rows(canvas, app, row_ptrs, 16U);
}

static void pocket_draw_currency(Canvas* canvas, PocketD20App* app) {
    const PocketCharacter* character = &app->data.character;
    char rows[5][40];
    const char* row_ptrs[5];
    for(uint8_t i = 0U; i < 5U; ++i)
        row_ptrs[i] = rows[i];
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
    snprintf(page, sizeof(page), "Page %u%s <>", page_number, app->catalog_has_more ? "+" : "");
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
        else if(
            app->catalog_kind == PocketCatalogItems &&
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
        pocket_draw_row(canvas, visible, index == app->selection, row);
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
        if(app->action_ack_active && app->action_ack_screen == (uint8_t)app->screen &&
           app->action_ack_selection == index)
            pocket_prefix_action_mark(row, sizeof(row));
        pocket_draw_row(canvas, visible, index == app->selection, row);
    }
}

static void
    pocket_format_record_detail(PocketD20App* app, uint8_t field, char* output, size_t size) {
    PocketCharacter* character = &app->data.character;
    uint8_t index = app->record_index;
    switch(app->list_kind) {
    case PocketListClasses: {
        const PocketClassLevel* class_level = &character->classes[index];
        if(field == 0U)
            pocket_format_labeled_text(output, size, "Name: ", class_level->name);
        else if(field == 1U)
            pocket_format_labeled_text(output, size, "Subclass: ", class_level->subclass);
        else if(field == 2U)
            snprintf(output, size, "Class level: %u", class_level->level);
        else if(field == 3U)
            snprintf(output, size, "Hit Point Die: d%u", class_level->hit_die);
        else if(field == 4U)
            snprintf(
                output,
                size,
                "Class Hit Dice: %u/%u",
                class_level->hit_dice_current,
                class_level->hit_dice_max);
        else if(field == 5U)
            snprintf(output, size, "Class Hit Dice max: %u", class_level->hit_dice_max);
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
        else if(field == 8U)
            snprintf(output, size, "Cantrip limit: %u", class_level->cantrip_limit);
        else if(field == 9U)
            snprintf(
                output,
                size,
                "Known %u Prep %u/%u",
                pocket_class_known_count_cached(app, index),
                pocket_class_prepared_count_cached(app, index),
                class_level->prepared_limit);
        else if(field == 10U)
            snprintf(output, size, "Spellbook size: %u", class_level->spellbook_size);
        else if(field == 11U)
            snprintf(output, size, "Pact slot level: %u", class_level->pact_slot_level);
        else if(field == 12U)
            snprintf(
                output,
                size,
                "Pact slots: %u/%u",
                class_level->pact_slots_current,
                class_level->pact_slots_max);
        else if(field == 13U)
            snprintf(output, size, "Pact slots max: %u", class_level->pact_slots_max);
        else if(field == 14U)
            snprintf(output, size, "Mystic Arcanum: 0x%X", class_level->mystic_arcanum_mask);
        else if(field == 15U)
            snprintf(
                output,
                size,
                "Spell points: %u/%u",
                class_level->spell_points_current,
                class_level->spell_points_max);
        else if(field == 16U)
            snprintf(output, size, "Spell points max: %u", class_level->spell_points_max);
        else
            snprintf(output, size, "Delete class");
        break;
    }
    case PocketListSpells: {
        uint8_t local = 0U;
        PocketSpell* spell = pocket_spell_at(app, index, &local);
        if(!spell) {
            pocket_copy(output, size, "Read error");
            break;
        }
        if(field == 0U)
            pocket_format_labeled_text(output, size, "Name: ", spell->name);
        else if(field == 1U)
            pocket_format_labeled_text(output, size, "Notes: ", spell->detail);
        else if(field == 2U)
            pocket_format_labeled_text(
                output,
                size,
                "Source class: ",
                spell->class_index < character->class_count ?
                    character->classes[spell->class_index].name :
                    "Primary");
        else if(field == 3U)
            snprintf(output, size, "Level: %u", spell->level);
        else if(field == 4U)
            snprintf(output, size, "Known: %s", character->spell_known[local] ? "Yes" : "No");
        else if(field == 5U)
            snprintf(output, size, "Prepared: %s", spell->prepared ? "Yes" : "No");
        else if(field == 6U)
            snprintf(
                output,
                size,
                "Always prepared: %s",
                character->spell_always_prepared[local] ? "Yes" : "No");
        else if(field == 7U)
            snprintf(output, size, "Ritual: %s", spell->ritual ? "Yes" : "No");
        else if(field == 8U)
            snprintf(
                output,
                size,
                "Free casts: %u/%u",
                character->spell_free_casts_current[local],
                character->spell_free_casts_max[local]);
        else if(field == 9U)
            snprintf(output, size, "Free casts max: %u", character->spell_free_casts_max[local]);
        else if(field == 10U)
            pocket_copy(
                output,
                size,
                character->spell_free_casts_current[local] ? "Use one free cast" :
                                                             "No free casts left");
        else if(field == 11U)
            pocket_format_labeled_text(output, size, "Stable ID: ", spell->stable_id);
        else if(field == 12U)
            pocket_format_labeled_text(output, size, "Source: ", spell->source);
        else if(field == 13U)
            pocket_format_labeled_text(output, size, "School: ", spell->school);
        else if(field == 14U)
            pocket_format_labeled_text(output, size, "Grant source: ", spell->grant_name);
        else if(field == 15U)
            snprintf(output, size, "Grant type: %u", spell->grant_source);
        else
            snprintf(output, size, "Delete spell");
        break;
    }
    case PocketListFeatures: {
        const PocketFeature* feature = &character->features[index];
        const char* class_name = feature->class_index < character->class_count ?
                                     character->classes[feature->class_index].name :
                                     "General";
        if(field == 0U)
            pocket_format_labeled_text(output, size, "Name: ", feature->name);
        else if(field == 1U)
            pocket_format_labeled_text(output, size, "Notes: ", feature->detail);
        else if(field == 2U)
            pocket_format_labeled_text(output, size, "Source class: ", class_name);
        else if(field == 3U)
            snprintf(output, size, "Gained at class L%u", feature->class_level_gained);
        else if(field == 4U)
            snprintf(output, size, "Uses: %d/%d", feature->uses_current, feature->uses_max);
        else if(field == 5U)
            snprintf(output, size, "Maximum uses: %d", feature->uses_max);
        else if(field == 6U)
            snprintf(output, size, "Recharge: %s", pocket_recharge_names[feature->recharge]);
        else if(field == 7U)
            snprintf(
                output,
                size,
                "Resource formula: %s",
                pocket_resource_formula_names[feature->resource_formula]);
        else if(field == 8U)
            snprintf(
                output,
                size,
                "Resource ability: %s",
                pocket_d20_ability_names[feature->resource_ability]);
        else
            snprintf(output, size, "Delete feature");
        break;
    }
    case PocketListItems: {
        PocketItem* item = pocket_item_at(app, index, NULL);
        if(!item) {
            pocket_copy(output, size, "Read error");
            break;
        }
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
            snprintf(
                output,
                size,
                "Weight: %d.%d lb",
                item->weight_tenths / 10,
                abs(item->weight_tenths % 10));
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
            snprintf(
                output,
                size,
                "Attack ability: %s",
                pocket_attack_ability_names[item->attack_ability]);
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
            snprintf(
                output,
                size,
                "Finesse: %s",
                (item->weapon_properties & PocketWeaponFinesse) ? "Yes" : "No");
            break;
        case 17:
            snprintf(
                output,
                size,
                "Ranged: %s",
                (item->weapon_properties & PocketWeaponRanged) ? "Yes" : "No");
            break;
        case 18:
            snprintf(
                output,
                size,
                "Light: %s",
                (item->weapon_properties & PocketWeaponLight) ? "Yes" : "No");
            break;
        case 19:
            snprintf(
                output,
                size,
                "Heavy: %s",
                (item->weapon_properties & PocketWeaponHeavy) ? "Yes" : "No");
            break;
        case 20:
            snprintf(
                output,
                size,
                "Thrown: %s",
                (item->weapon_properties & PocketWeaponThrown) ? "Yes" : "No");
            break;
        case 21:
            snprintf(
                output,
                size,
                "Ammunition: %s",
                (item->weapon_properties & PocketWeaponAmmunition) ? "Yes" : "No");
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
            snprintf(
                output,
                size,
                "Container: %s",
                item->container_index < 0 ? "Carried" : "Inside item");
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
        else
            snprintf(output, size, "Delete language");
        break;
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

static void pocket_format_combat_row(
    PocketD20App* app,
    uint8_t index,
    char* row,
    size_t size) {
    PocketCharacter* character = &app->data.character;
    switch(index) {
    case 0U:
        snprintf(row, size, "Attack mode: %s", pocket_roll_mode_names[app->roll_mode]);
        break;
    case 1U:
        pocket_copy(row, size, "Weapon Attacks");
        break;
    case 2U:
        pocket_copy(row, size, "Spell Attacks");
        break;
    case 3U:
        snprintf(row, size, "Attack Templates (%u)", character->attack_template_count);
        break;
    case 4U:
        pocket_copy(row, size, "Initiative Tracker");
        break;
    case 5U:
        snprintf(row, size, "HP: %d/%d", character->hp_current, character->hp_max);
        break;
    case 6U:
        snprintf(row, size, "Temporary HP: %d", character->hp_temporary);
        break;
    case 7U:
        pocket_copy(row, size, "Short Rest");
        break;
    case 8U:
        snprintf(
            row,
            size,
            "Spend %.22s d%u: %u/%u",
            character->classes[app->hit_die_class_index].name,
            character->classes[app->hit_die_class_index].hit_die,
            character->classes[app->hit_die_class_index].hit_dice_current,
            character->classes[app->hit_die_class_index].hit_dice_max);
        break;
    case 9U:
        pocket_copy(row, size, "Long Rest");
        break;
    case 10U:
        snprintf(row, size, "Conditions: %.32s", character->conditions);
        break;
    case 11U:
        snprintf(
            row,
            size,
            "Concentration: %.31s",
            character->concentration[0] ? character->concentration : "None");
        break;
    case 12U:
        snprintf(row, size, "Reaction: %s", character->reaction_available ? "Ready" : "Used");
        break;
    case 13U:
        snprintf(row, size, "Temp effects: %.30s", character->temporary_effects);
        break;
    case 14U:
        snprintf(row, size, "Resist: %.35s", character->resistances);
        break;
    case 15U:
        snprintf(row, size, "Immune: %.35s", character->immunities);
        break;
    case 16U:
        snprintf(row, size, "Vulnerable: %.31s", character->vulnerabilities);
        break;
    case 17U:
        snprintf(row, size, "Senses: %.35s", character->senses);
        break;
    case 18U:
        snprintf(row, size, "Movement: %.33s", character->movement_modes);
        break;
    case 19U:
        snprintf(row, size, "Death success: %u", character->death_successes);
        break;
    case 20U:
        snprintf(row, size, "Death failure: %u", character->death_failures);
        break;
    default:
        snprintf(row, size, "Exhaustion: %u", character->exhaustion);
        break;
    }
}

static void pocket_draw_combat(Canvas* canvas, PocketD20App* app) {
    pocket_draw_header(canvas, "Combat", app->status);
    for(uint8_t visible = 0U; visible < 5U; ++visible) {
        uint16_t index = app->scroll + visible;
        if(index >= 22U) break;
        char row[48];
        pocket_format_combat_row(app, (uint8_t)index, row, sizeof(row));
        pocket_draw_row(canvas, visible, index == app->selection, row);
    }
}

static void pocket_draw_dice(Canvas* canvas, PocketD20App* app) {
    char rows[5][48];
    const char* row_ptrs[5];
    for(uint8_t i = 0U; i < 5U; ++i)
        row_ptrs[i] = rows[i];
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


static uint8_t pocket_spell_casting_ability(PocketD20App* app, uint8_t logical_index) {
    PocketSpell* spell = pocket_spell_at(app, logical_index, NULL);
    return pocket_d20_spell_casting_ability_for(&app->data.character, spell);
}

static int8_t pocket_spell_attack_modifier_for(PocketD20App* app, uint8_t logical_index) {
    PocketSpell* spell = pocket_spell_at(app, logical_index, NULL);
    return pocket_d20_spell_attack_modifier_for(&app->data.character, spell);
}

static int8_t pocket_spell_save_dc_for(PocketD20App* app, uint8_t logical_index) {
    PocketSpell* spell = pocket_spell_at(app, logical_index, NULL);
    return pocket_d20_spell_save_dc_for(&app->data.character, spell);
}

static uint8_t pocket_build_spell_cast_options(
    PocketD20App* app,
    uint8_t logical_index,
    PocketSpellCastOption* options,
    uint8_t capacity) {
    uint8_t local = 0U;
    PocketSpell* spell = pocket_spell_at(app, logical_index, &local);
    if(!spell) return 0U;
    PocketCharacter* character = &app->data.character;
    return pocket_d20_spells_build_cast_options(
        character,
        spell,
        character->spell_known[local],
        character->spell_always_prepared[local],
        character->spell_free_casts_current[local],
        options,
        capacity);
}

static bool pocket_refresh_combat_spell_index(PocketD20App* app) {
    uint8_t total = 0U;
    if(!pocket_d20_spells_collect_combat_indices(
           app->storage,
           app->profiles.active_profile,
           &app->data.character,
           app->combat_spell_indices,
           POCKET_D20_MAX_SPELLS,
           &app->combat_spell_count,
           &total)) {
        pocket_set_status(app, "Spellbook read failed");
        return false;
    }
    app->spellbook_total = total;
    return true;
}

static uint8_t pocket_combat_spell_count(PocketD20App* app) {
    return app->combat_spell_count;
}

static uint8_t pocket_combat_spell_index(PocketD20App* app, uint16_t display_index) {
    if(display_index >= app->combat_spell_count) return 0xFFU;
    return app->combat_spell_indices[display_index];
}

static const char* pocket_spell_cast_resource_name(uint8_t resource) {
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

static void pocket_format_spell_cast_option(
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
        if(pocket_spell_at(app, app->spell_attack_index, &local))
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
            pocket_d20_spell_point_cost(option->level));
        break;
    case PocketSpellCastRitual:
        snprintf(output, size, "Ritual (+10 minutes)");
        break;
    default:
        output[0] = '\0';
        break;
    }
}

static bool pocket_consume_spell_cast_resource(
    PocketD20App* app,
    const PocketSpellCastOption* option) {
    PocketCharacter* character = &app->data.character;
    switch(option->resource) {
    case PocketSpellCastCantrip:
    case PocketSpellCastRitual:
        return true;
    case PocketSpellCastFree: {
        uint8_t local = 0U;
        if(!pocket_spell_at(app, app->spell_attack_index, &local) ||
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
        uint8_t cost = pocket_d20_spell_point_cost(option->level);
        if(!cost || character->classes[option->class_index].spell_points_current < cost) return false;
        character->classes[option->class_index].spell_points_current -= cost;
        return true;
    }
    default:
        return false;
    }
}

static int16_t pocket_roll_sorcerous_burst(
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
        uint8_t value = (uint8_t)pocket_d20_roll_dice(1U, 8U);
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

static void pocket_cast_spell(PocketD20App* app, const PocketSpellCastOption* option) {
    PocketCharacter* character = &app->data.character;
    if(app->spell_attack_index >= app->spellbook_total ||
       !pocket_consume_spell_cast_resource(app, option)) {
        pocket_set_status(app, "Casting resource unavailable");
        return;
    }

    PocketSpell* spell = pocket_spell_at(app, app->spell_attack_index, NULL);
    if(!spell) {
        pocket_set_status(app, "Spell read failed");
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

    uint8_t ability = pocket_spell_casting_ability(app, app->spell_attack_index);
    int8_t ability_modifier = pocket_d20_ability_modifier(character->ability_scores[ability]);
    PocketSpellDamageSpec damage;
    if(pocket_d20_spell_damage_spec(
           spell,
           option->level,
           pocket_d20_total_level(character),
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
                pocket_spell_attack_modifier_for(app, app->spell_attack_index);
            for(uint8_t index = 0U; index < attack_count; ++index) {
                int16_t primary = 0;
                int16_t secondary = 0;
                if(damage.primary_dice && damage.primary_die) {
                    if(damage.special == PocketSpellSpecialSorcerousBurst) {
                        uint8_t rolled_dice = damage.primary_dice;
                        primary = pocket_roll_sorcerous_burst(
                            damage.primary_dice, ability_modifier, &rolled_dice);
                        if(index == 0U) app->spell_cast_primary_dice = rolled_dice;
                    } else {
                        primary =
                            (int16_t)pocket_d20_roll_dice(damage.primary_dice, damage.primary_die);
                    }
                }
                if(!damage.secondary_relation && damage.secondary_dice && damage.secondary_die)
                    secondary =
                        (int16_t)pocket_d20_roll_dice(damage.secondary_dice, damage.secondary_die);
                int16_t attack_damage = primary + secondary + damage.flat_bonus;
                uint8_t natural = pocket_d20_roll_d20_mode(app->roll_mode);
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
                    (int16_t)pocket_d20_roll_dice(damage.secondary_dice, damage.secondary_die);
        } else if(damage.roll_instances > 1U) {
            uint8_t roll_count = damage.roll_instances;
            if(roll_count > POCKET_D20_MAX_SPELL_ATTACK_ROLLS)
                roll_count = POCKET_D20_MAX_SPELL_ATTACK_ROLLS;
            app->spell_cast_attack_roll_count = roll_count;
            for(uint8_t index = 0U; index < roll_count; ++index) {
                int16_t primary = 0;
                if(damage.primary_dice && damage.primary_die)
                    primary =
                        (int16_t)pocket_d20_roll_dice(damage.primary_dice, damage.primary_die);
                int16_t value = primary + damage.flat_bonus;
                app->spell_cast_attack_damage[index] = value;
                app->spell_cast_damage_total += value;
                if(index == 0U) app->spell_cast_primary_total = primary;
            }
        } else {
            if(damage.primary_dice && damage.primary_die)
                app->spell_cast_primary_total =
                    (int16_t)pocket_d20_roll_dice(damage.primary_dice, damage.primary_die);
            if(damage.secondary_dice && damage.secondary_die)
                app->spell_cast_secondary_total =
                    (int16_t)pocket_d20_roll_dice(damage.secondary_dice, damage.secondary_die);
            app->spell_cast_damage_total = app->spell_cast_primary_total + damage.flat_bonus;
            if(!damage.secondary_relation)
                app->spell_cast_damage_total +=
                    app->spell_cast_secondary_total + damage.secondary_flat_bonus;
        }
    }

    if(option->resource == PocketSpellCastFree)
        (void)pocket_save_spellbook_if_changed(app);
    pocket_save(app, false);
    pocket_enter_screen(app, PocketScreenSpellResult);
}

static void pocket_draw_spell_attacks(Canvas* canvas, PocketD20App* app) {
    uint8_t count = pocket_combat_spell_count(app);
    char title[32];
    snprintf(title, sizeof(title), "Spells: %s", pocket_roll_mode_names[app->roll_mode]);
    pocket_draw_header(canvas, title, app->status);
    if(!count) {
        pocket_draw_row(canvas, 0U, false, "No combat spells");
        pocket_draw_row(canvas, 1U, false, "Prepare or add XdY notes");
        return;
    }
    for(uint8_t visible = 0U; visible < 5U; ++visible) {
        uint16_t display_index = app->scroll + visible;
        if(display_index >= count) break;
        uint8_t spell_index = pocket_combat_spell_index(app, display_index);
        if(spell_index == 0xFFU) break;
        PocketSpell* spell = pocket_spell_at(app, spell_index, NULL);
        if(!spell) break;
        char row[64];
        PocketSpellDamageSpec damage;
        uint8_t ability = pocket_spell_casting_ability(app, spell_index);
        int8_t ability_modifier =
            pocket_d20_ability_modifier(app->data.character.ability_scores[ability]);
        bool has_damage = pocket_d20_spell_damage_spec(
            spell,
            spell->level,
            pocket_d20_total_level(&app->data.character),
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
                sizeof(row),
                "L%u %s%s%s",
                spell->level,
                spell->name,
                spell->ritual ? " [R]" : "",
                dice_suffix);
        else
            snprintf(row, sizeof(row), "C %s%s", spell->name, dice_suffix);
        pocket_draw_row(canvas, visible, display_index == app->selection, row);
    }
}

static void pocket_draw_spell_cast(Canvas* canvas, PocketD20App* app) {
    if(app->spell_attack_index >= app->spellbook_total) return;
    PocketSpellCastOption options[POCKET_D20_MAX_SPELL_CAST_OPTIONS];
    uint8_t count = pocket_build_spell_cast_options(
        app, app->spell_attack_index, options, POCKET_D20_MAX_SPELL_CAST_OPTIONS);
    if(count > POCKET_D20_MAX_SPELL_CAST_OPTIONS) count = POCKET_D20_MAX_SPELL_CAST_OPTIONS;
    PocketSpell* spell = pocket_spell_at(app, app->spell_attack_index, NULL);
    if(!spell) return;
    pocket_draw_header(canvas, spell->name, app->status);
    for(uint8_t visible = 0U; visible < 5U; ++visible) {
        uint16_t index = app->scroll + visible;
        if(index >= count) break;
        char row[64];
        pocket_format_spell_cast_option(app, &options[index], row, sizeof(row));
        pocket_draw_row(canvas, visible, index == app->selection, row);
    }
}

static const char* pocket_spell_resolution_label(uint8_t resolution) {
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

static const char* pocket_spell_secondary_relation_label(uint8_t relation) {
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

static int16_t pocket_spell_component_total(int16_t dice_total, int16_t flat_bonus) {
    return (int16_t)(dice_total + flat_bonus);
}

static void pocket_draw_spell_result(Canvas* canvas, PocketD20App* app) {
    if(app->spell_attack_index >= app->spellbook_total) return;
    PocketSpell* spell = pocket_spell_at(app, app->spell_attack_index, NULL);
    if(!spell) return;
    pocket_draw_header(canvas, spell->name, app->status);
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
        pocket_draw_row(canvas, 0U, false, row);

        int8_t attack_modifier =
            pocket_spell_attack_modifier_for(app, app->spell_attack_index);
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
            pocket_draw_row(canvas, visible + 1U, false, row);
        }
        return;
    }

    if(app->spell_cast_resolution != PocketSpellResolutionAttack &&
       app->spell_cast_attack_roll_count > 1U) {
        const char* resource = pocket_spell_cast_resource_name(app->spell_cast_resource);
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
        pocket_draw_row(canvas, 0U, false, row);
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
            pocket_draw_row(canvas, visible + 1U, false, row);
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
            pocket_spell_cast_resource_name(app->spell_cast_resource));
    else
        snprintf(row, sizeof(row), "Cantrip cast");
    pocket_draw_row(canvas, 0U, false, row);

    if(app->spell_cast_resolution == PocketSpellResolutionAttack) {
        snprintf(
            row,
            sizeof(row),
            "Attack d20 %u %+d = %d",
            app->spell_cast_natural,
            pocket_spell_attack_modifier_for(app, app->spell_attack_index),
            app->spell_cast_attack_total);
    } else if(app->spell_cast_resolution == PocketSpellResolutionSave) {
        snprintf(
            row,
            sizeof(row),
            "Target save DC %d",
            pocket_spell_save_dc_for(app, app->spell_attack_index));
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
            pocket_spell_attack_modifier_for(app, app->spell_attack_index),
            pocket_spell_save_dc_for(app, app->spell_attack_index));
    }
    pocket_draw_row(canvas, 1U, false, row);

    if(!app->spell_cast_primary_dice && !app->spell_cast_secondary_dice &&
       !app->spell_cast_flat_bonus && !app->spell_cast_secondary_flat_bonus) {
        pocket_draw_row(canvas, 2U, false, "No mapped combat roll");
        pocket_draw_row(canvas, 3U, false, "Cast/resource recorded");
    } else if(app->spell_cast_secondary_relation) {
        int16_t primary_total = pocket_spell_component_total(
            app->spell_cast_primary_total, app->spell_cast_flat_bonus);
        int16_t secondary_total = pocket_spell_component_total(
            app->spell_cast_secondary_total, app->spell_cast_secondary_flat_bonus);
        if(app->spell_cast_primary_dice)
            snprintf(
                row,
                sizeof(row),
                "%s %ud%u%+d=%d",
                pocket_spell_resolution_label(app->spell_cast_resolution),
                app->spell_cast_primary_dice,
                app->spell_cast_primary_die,
                app->spell_cast_flat_bonus,
                primary_total);
        else
            snprintf(
                row,
                sizeof(row),
                "%s %+d",
                pocket_spell_resolution_label(app->spell_cast_resolution),
                app->spell_cast_flat_bonus);
        pocket_draw_row(canvas, 2U, false, row);

        if(app->spell_cast_secondary_dice)
            snprintf(
                row,
                sizeof(row),
                "%s %ud%u%+d=%d",
                pocket_spell_resolution_label(app->spell_cast_secondary_resolution),
                app->spell_cast_secondary_dice,
                app->spell_cast_secondary_die,
                app->spell_cast_secondary_flat_bonus,
                secondary_total);
        else
            snprintf(
                row,
                sizeof(row),
                "%s %+d",
                pocket_spell_resolution_label(app->spell_cast_secondary_resolution),
                app->spell_cast_secondary_flat_bonus);
        pocket_draw_row(canvas, 3U, false, row);
        pocket_draw_row(
            canvas, 4U, false, pocket_spell_secondary_relation_label(app->spell_cast_secondary_relation));
    } else {
        if(app->spell_cast_primary_dice) {
            snprintf(
                row,
                sizeof(row),
                app->spell_cast_from_notes ? "Notes %ud%u = %d" : "%ud%u = %d",
                app->spell_cast_primary_dice,
                app->spell_cast_primary_die,
                app->spell_cast_primary_total);
            pocket_draw_row(canvas, 2U, false, row);
        }
        if(app->spell_cast_secondary_dice) {
            snprintf(
                row,
                sizeof(row),
                "+ %ud%u = %d",
                app->spell_cast_secondary_dice,
                app->spell_cast_secondary_die,
                app->spell_cast_secondary_total);
            pocket_draw_row(canvas, 3U, false, row);
        } else if(app->spell_cast_flat_bonus) {
            snprintf(row, sizeof(row), "Modifier: %+d", app->spell_cast_flat_bonus);
            pocket_draw_row(canvas, 3U, false, row);
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
        pocket_draw_row(canvas, 4U, false, row);
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
        if(item_index == 0xFFU) break;
        PocketItem* item = pocket_item_at(app, item_index, NULL);
        if(!item) break;
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
    PocketItem* item = pocket_item_at(app, app->attack_item_index, NULL);
    if(!item) return;
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
                row, sizeof(row), "%s damage", app->damage_roll.critical ? "Critical" : "Normal");
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
    snprintf(title, sizeof(title), "Rolling %ud%u...", app->dice_anim_count, app->dice_anim_sides);
    pocket_draw_header(canvas, title, NULL);
    uint8_t visible_dice = app->dice_anim_count > 1U ? 3U : 1U;
    for(uint8_t i = 0U; i < visible_dice; ++i) {
        int32_t x = visible_dice == 1U ? 64 : 22 + (i * 42);
        uint8_t face = (uint8_t)(((app->dice_anim_frame * 7U) + (i * 5U) + app->dice_anim_sides) %
                                 app->dice_anim_sides) +
                       1U;
        pocket_draw_animated_die(canvas, x, 34, app->dice_anim_frame + i, face);
    }
    canvas_draw_frame(canvas, 14, 55, 100, 5);
    uint8_t progress =
        (uint8_t)(((app->dice_anim_frame + 1U) * 98U) / POCKET_D20_DICE_ANIMATION_FRAMES);
    canvas_draw_box(canvas, 15, 56, progress, 3);
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
    case PocketScreenLevelChoice:
        pocket_draw_level_choice(canvas, app);
        break;
    case PocketScreenAsiAbility:
        pocket_draw_asi_ability(canvas, app);
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
    case PocketScreenSpellAttacks:
        pocket_draw_spell_attacks(canvas, app);
        break;
    case PocketScreenSpellCast:
        pocket_draw_spell_cast(canvas, app);
        break;
    case PocketScreenSpellResult:
        pocket_draw_spell_result(canvas, app);
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
    default:
        break;
    }
}

static void pocket_open_list(PocketD20App* app, PocketListKind kind, PocketScreen return_screen) {
    pocket_release_text_input(app);
    pocket_release_number_input(app);
    bool inventory_init_failed = false;
    bool sidecar_init_failed = false;
    if(kind == PocketListItems && app->active_profile_loaded) {
        bool initialized = false;
        if(!pocket_d20_items_initialize_inventory(
               app->storage, app->profiles.active_profile, &app->data, &initialized)) {
            /* Default composition is optional for opening Inventory.  Establish a
               minimal live sidecar so manual Add New remains usable. */
            inventory_init_failed = true;
            if(!pocket_d20_storage_ensure_items_sidecar(
                   app->storage, app->profiles.active_profile))
                sidecar_init_failed = true;
        } else if(initialized) {
            pocket_d20_data_clear_items(&app->data.character);
            app->items_loaded = 0U;
            app->items_total = 0U;
            app->items_cache_start = 0U;
            app->item_aggregate_valid = 0U;
            app->saved_fingerprint = pocket_data_fingerprint(&app->data);
            app->storage_unsaved = 0U;
        }
    } else if(kind == PocketListSpells && app->active_profile_loaded) {
        if(!pocket_d20_storage_ensure_spellbook_sidecar(
               app->storage, app->profiles.active_profile))
            sidecar_init_failed = true;
    }
    app->list_kind = kind;
    app->record_list_return_screen = return_screen;
    pocket_enter_screen(app, PocketScreenRecordList);
    if(sidecar_init_failed)
        pocket_set_status(app, "Sidecar create failed - retry SD");
    else if(inventory_init_failed)
        pocket_set_status(app, "Defaults failed; manual add ready");
}

/* The proven pre-sidecar implementation grew the resident collection before
   saving. Preserve that lifecycle with bounded paging by first making the real
   tail page resident, then growing that page and committing it immediately. */
static bool pocket_spell_prepare_append_page(PocketD20App* app) {
    if(!app->spellbook_loaded && !pocket_load_spellbook(app)) return false;
    if(app->spellbook_total >= POCKET_D20_MAX_SPELLS) return false;

    const uint8_t target_start = (uint8_t)(
        (app->spellbook_total / POCKET_D20_COLLECTION_CACHE_SIZE) *
        POCKET_D20_COLLECTION_CACHE_SIZE);
    if(app->spellbook_cache_start != target_start) {
        if(!pocket_save_spellbook_if_changed(app)) return false;
        pocket_d20_data_clear_spells(&app->data.character);
        app->spellbook_loaded = 0U;
        if(!pocket_load_spellbook_page(app, target_start)) return false;
    }

    PocketCharacter* character = &app->data.character;
    const uint8_t expected = (uint8_t)(app->spellbook_total - target_start);
    if(character->spell_count != expected ||
       character->spell_count >= POCKET_D20_COLLECTION_CACHE_SIZE)
        return false;
    return pocket_d20_data_reserve_spells(character, (uint8_t)(character->spell_count + 1U));
}

static bool pocket_item_prepare_append_page(PocketD20App* app) {
    if(!app->items_loaded && !pocket_load_items(app)) return false;
    if(app->items_total >= POCKET_D20_MAX_ITEMS) return false;

    const uint8_t target_start = (uint8_t)(
        (app->items_total / POCKET_D20_COLLECTION_CACHE_SIZE) *
        POCKET_D20_COLLECTION_CACHE_SIZE);
    if(app->items_cache_start != target_start) {
        if(!pocket_save_items_if_changed(app)) return false;
        pocket_d20_data_clear_items(&app->data.character);
        app->items_loaded = 0U;
        if(!pocket_load_items_page(app, target_start)) return false;
    }

    PocketCharacter* character = &app->data.character;
    const uint8_t expected = (uint8_t)(app->items_total - target_start);
    if(character->item_count != expected ||
       character->item_count >= POCKET_D20_COLLECTION_CACHE_SIZE)
        return false;
    return pocket_d20_data_reserve_items(character, (uint8_t)(character->item_count + 1U));
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
    case PocketListSpells: {
        if(!pocket_spell_prepare_append_page(app)) return false;
        PocketCharacter* character = &app->data.character;
        const uint8_t local = character->spell_count;
        PocketSpell* spell = &character->spells[local];
        memset(spell, 0, sizeof(*spell));
        pocket_copy(spell->name, sizeof(spell->name), "New Spell");
        character->spell_known[local] = 1U;
        character->spell_always_prepared[local] = 0U;
        character->spell_free_casts_current[local] = 0U;
        character->spell_free_casts_max[local] = 0U;
        ++character->spell_count;

        app->record_index = app->spellbook_total++;
        app->spell_class_counts_valid = 0U;
        (void)pocket_save_spellbook_if_changed(app);
        break;
    }
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
        character->features[app->record_index].class_level_gained = character->classes[0].level;
        break;
    case PocketListItems: {
        if(!pocket_item_prepare_append_page(app)) return false;
        PocketCharacter* character = &app->data.character;
        const uint8_t local = character->item_count;
        PocketItem* item = &character->items[local];
        memset(item, 0, sizeof(*item));
        pocket_copy(item->name, sizeof(item->name), "New Item");
        item->quantity = 1;
        item->damage_dice = 1U;
        item->damage_die = 6U;
        item->extra_die = 6U;
        item->add_ability_damage = 1U;
        item->container_index = -1;
        item->armor_dex_cap = -1;
        ++character->item_count;

        app->record_index = app->items_total++;
        app->item_aggregate_valid = 0U;
        (void)pocket_save_items_if_changed(app);
        break;
    }
    case PocketListLanguages:
        if(character->language_count >= POCKET_D20_MAX_LANGUAGES) return false;
        app->record_index = character->language_count++;
        memset(character->languages[app->record_index], 0, POCKET_D20_SHORT_LEN);
        pocket_copy(character->languages[app->record_index], POCKET_D20_SHORT_LEN, "New Language");
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
        if(!pocket_load_spellbook(app)) {
            pocket_set_status(app, "Spellbook read failed");
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
        if(!pocket_remap_spell_classes(app, index)) {
            pocket_set_status(app, "Spellbook update failed");
            return;
        }
        break;
    case PocketListSpells:
        if(index >= app->spellbook_total || !pocket_save_spellbook_if_changed(app) ||
           !pocket_d20_storage_delete_spell(
               app->storage, app->profiles.active_profile, character, index)) {
            pocket_set_status(app, "Spell delete failed");
            return;
        }
        --app->spellbook_total;
        app->spell_class_counts_valid = 0U;
        pocket_d20_data_clear_spells(character);
        app->spellbook_loaded = 0U;
        app->spellbook_cache_start = 0U;
        if(app->spellbook_total) {
            uint8_t target = index < app->spellbook_total ? index : (uint8_t)(app->spellbook_total - 1U);
            if(!pocket_spell_cache_ensure(app, target)) {
                pocket_set_status(app, "Spellbook read failed");
                return;
            }
        }
        break;
    case PocketListFeatures:
        memmove(
            &character->features[index],
            &character->features[index + 1U],
            (character->feature_count - index - 1U) * sizeof(PocketFeature));
        --character->feature_count;
        memset(&character->features[character->feature_count], 0, sizeof(PocketFeature));
        pocket_d20_data_reserve_features_exact(character, character->feature_count);
        break;
    case PocketListItems:
        if(index >= app->items_total || !pocket_save_items_if_changed(app) ||
           !pocket_d20_storage_delete_item(
               app->storage, app->profiles.active_profile, character, index)) {
            pocket_set_status(app, "Item delete failed");
            return;
        }
        --app->items_total;
        app->item_aggregate_valid = 0U;
        pocket_d20_data_clear_items(character);
        app->items_loaded = 0U;
        app->items_cache_start = 0U;
        if(app->items_total) {
            uint8_t target = index < app->items_total ? index : (uint8_t)(app->items_total - 1U);
            if(!pocket_item_cache_ensure(app, target)) {
                pocket_set_status(app, "Items read failed");
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
    pocket_save(app, false);
    pocket_enter_screen(app, PocketScreenRecordList);
    if(app->list_kind == PocketListItems && app->items_total) {
        uint8_t target = index < app->items_total ? index : (uint8_t)(app->items_total - 1U);
        pocket_record_list_focus(app, target);
    } else if(app->list_kind == PocketListSpells && app->spellbook_total) {
        uint8_t target =
            index < app->spellbook_total ? index : (uint8_t)(app->spellbook_total - 1U);
        pocket_record_list_focus(app, target);
    }
}

static void pocket_text_done(void* context) {
    PocketD20App* app = context;
    app->input_module_active = 0U;
    PocketCharacter* character = &app->data.character;
    uint8_t index = app->record_index;
    PocketEditTarget completed_target = app->edit_target;
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
        pocket_copy(
            character->tool_proficiencies,
            sizeof(character->tool_proficiencies),
            app->edit_buffer);
        break;
    case PocketEditArmorTraining:
        pocket_copy(
            character->armor_training, sizeof(character->armor_training), app->edit_buffer);
        break;
    case PocketEditWeaponTraining:
        pocket_copy(
            character->weapon_training, sizeof(character->weapon_training), app->edit_buffer);
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
        pocket_copy(
            character->temporary_effects, sizeof(character->temporary_effects), app->edit_buffer);
        break;
    case PocketEditResistances:
        pocket_copy(character->resistances, sizeof(character->resistances), app->edit_buffer);
        break;
    case PocketEditImmunities:
        pocket_copy(character->immunities, sizeof(character->immunities), app->edit_buffer);
        break;
    case PocketEditVulnerabilities:
        pocket_copy(
            character->vulnerabilities, sizeof(character->vulnerabilities), app->edit_buffer);
        break;
    case PocketEditMovementModes:
        pocket_copy(
            character->movement_modes, sizeof(character->movement_modes), app->edit_buffer);
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
    case PocketEditSpellName: {
        PocketSpell* spell = pocket_spell_at(app, index, NULL);
        if(spell) pocket_copy(spell->name, sizeof(spell->name), app->edit_buffer);
        break;
    }
    case PocketEditSpellDetail: {
        PocketSpell* spell = pocket_spell_at(app, index, NULL);
        if(spell) pocket_copy(spell->detail, sizeof(spell->detail), app->edit_buffer);
        break;
    }
    case PocketEditSpellStableId: {
        PocketSpell* spell = pocket_spell_at(app, index, NULL);
        if(spell) pocket_copy(spell->stable_id, sizeof(spell->stable_id), app->edit_buffer);
        break;
    }
    case PocketEditSpellSource: {
        PocketSpell* spell = pocket_spell_at(app, index, NULL);
        if(spell) pocket_copy(spell->source, sizeof(spell->source), app->edit_buffer);
        break;
    }
    case PocketEditSpellSchool: {
        PocketSpell* spell = pocket_spell_at(app, index, NULL);
        if(spell) pocket_copy(spell->school, sizeof(spell->school), app->edit_buffer);
        break;
    }
    case PocketEditSpellGrantName: {
        PocketSpell* spell = pocket_spell_at(app, index, NULL);
        if(spell) pocket_copy(spell->grant_name, sizeof(spell->grant_name), app->edit_buffer);
        break;
    }
    case PocketEditGrantStableId:
        if(index < character->grant_count)
            pocket_copy(
                character->grants[index].stable_id,
                sizeof(character->grants[index].stable_id),
                app->edit_buffer);
        break;
    case PocketEditGrantSource:
        if(index < character->grant_count)
            pocket_copy(
                character->grants[index].source,
                sizeof(character->grants[index].source),
                app->edit_buffer);
        break;
    case PocketEditGrantOption:
        if(index < character->grant_count)
            pocket_copy(
                character->grants[index].option_name,
                sizeof(character->grants[index].option_name),
                app->edit_buffer);
        break;
    case PocketEditGrantPrerequisites:
        if(index < character->grant_count)
            pocket_copy(
                character->grants[index].prerequisites,
                sizeof(character->grants[index].prerequisites),
                app->edit_buffer);
        break;
    case PocketEditGrantValue:
        if(index < character->grant_count)
            pocket_copy(
                character->grants[index].grant_value,
                sizeof(character->grants[index].grant_value),
                app->edit_buffer);
        break;
    case PocketEditAttackName:
        if(index < character->attack_template_count)
            pocket_copy(
                character->attack_templates[index].name,
                sizeof(character->attack_templates[index].name),
                app->edit_buffer);
        break;
    case PocketEditAttackMastery:
        if(index < character->attack_template_count)
            pocket_copy(
                character->attack_templates[index].mastery,
                sizeof(character->attack_templates[index].mastery),
                app->edit_buffer);
        break;
    case PocketEditAttackDamageType:
        if(index < character->attack_template_count)
            pocket_copy(
                character->attack_templates[index].damage_type,
                sizeof(character->attack_templates[index].damage_type),
                app->edit_buffer);
        break;
    case PocketEditAttackRiderType:
        if(index < character->attack_template_count)
            pocket_copy(
                character->attack_templates[index].rider_type,
                sizeof(character->attack_templates[index].rider_type),
                app->edit_buffer);
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
    case PocketEditItemName: {
        PocketItem* item = pocket_item_at(app, index, NULL);
        if(item) pocket_copy(item->name, sizeof(item->name), app->edit_buffer);
        break;
    }
    case PocketEditItemDetail: {
        PocketItem* item = pocket_item_at(app, index, NULL);
        if(item) pocket_copy(item->detail, sizeof(item->detail), app->edit_buffer);
        break;
    }
    case PocketEditItemAmmoGroup: {
        PocketItem* item = pocket_item_at(app, index, NULL);
        if(item) pocket_copy(item->ammunition_group, sizeof(item->ammunition_group), app->edit_buffer);
        break;
    }
    case PocketEditLanguageName:
        pocket_copy(character->languages[index], POCKET_D20_SHORT_LEN, app->edit_buffer);
        break;
    case PocketEditNone:
        break;
    default:
        break;
    }
    app->edit_target = PocketEditNone;
    switch(completed_target) {
    case PocketEditSpellName:
    case PocketEditSpellDetail:
    case PocketEditSpellStableId:
    case PocketEditSpellSource:
    case PocketEditSpellSchool:
    case PocketEditSpellGrantName:
        (void)pocket_save_spellbook_if_changed(app);
        break;
    case PocketEditItemName:
    case PocketEditItemDetail:
    case PocketEditItemAmmoGroup:
        (void)pocket_save_items_if_changed(app);
        break;
    default:
        break;
    }
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
        pocket_flush_save(app, false);
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
        if(app->list_kind == PocketListItems || app->list_kind == PocketListSpells)
            pocket_record_list_focus(app, app->record_index);
        else {
            app->selection = app->record_index + 1U;
            if(app->selection >= 5U) app->scroll = app->selection - 4U;
        }
        break;
    case PocketScreenCatalog:
        if(app->level_choice_mode == 3U && app->catalog_target == PocketEditFeatureName) {
            PocketCharacter* c = &app->data.character;
            if(c->feature_count && app->record_index == c->feature_count - 1U &&
               !c->features[app->record_index].name[0]) {
                --c->feature_count;
                memset(&c->features[c->feature_count], 0, sizeof(PocketFeature));
                pocket_d20_data_reserve_features_exact(c, c->feature_count);
            }
            app->level_choice_mode = 0U;
        }
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
    case PocketScreenLevelChoice:
    case PocketScreenAsiAbility:
        app->level_choice_first_ability = UINT8_MAX;
        pocket_enter_screen(app, app->return_screen);
        break;
    case PocketScreenSpellFilters:
        pocket_enter_screen(app, PocketScreenMagic);
        break;
    case PocketScreenSpellAttacks:
        pocket_enter_screen(app, PocketScreenCombat);
        break;
    case PocketScreenSpellCast:
        pocket_enter_screen(app, PocketScreenSpellAttacks);
        break;
    case PocketScreenSpellResult:
        pocket_enter_screen(app, PocketScreenSpellAttacks);
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
    default:
        pocket_enter_screen(app, PocketScreenHome);
        break;
    }
}

static void pocket_handle_long_back(PocketD20App* app) {
    if(app->screen == PocketScreenHome) {
        pocket_flush_save(app, false);
        view_dispatcher_stop(app->dispatcher);
        return;
    }
    app->dice_animating = 0U;
    app->arcane_recovery_active = 0U;
    pocket_catalog_release(app);
    pocket_enter_screen(app, PocketScreenHome);
    app->marquee_elapsed_ms = 0U;
}

static void pocket_handle_profiles(PocketD20App* app, const InputEvent* event) {
    uint16_t profile_count = pocket_profile_count(app);
    uint16_t row_count = profile_count + 1U;
    if(pocket_is_move_event(event) && event->key == InputKeyUp) {
        uint16_t previous_scroll = app->scroll;
        pocket_menu_move(app, row_count, -1);
        if(app->scroll != previous_scroll && app->scroll < profile_count)
            (void)pocket_d20_profiles_window(app->storage, &app->profiles, app->scroll);
    } else if(pocket_is_move_event(event) && event->key == InputKeyDown) {
        uint16_t previous_scroll = app->scroll;
        pocket_menu_move(app, row_count, 1);
        if(app->scroll != previous_scroll && app->scroll < profile_count)
            (void)pocket_d20_profiles_window(app->storage, &app->profiles, app->scroll);
    }
    else if(event->type == InputTypeShort && event->key == InputKeyOk) {
        if(app->selection == profile_count)
            pocket_create_profile(app);
        else
            pocket_switch_profile(app, pocket_profile_id_at(app, app->selection));
    } else if(
        event->type == InputTypeLong && event->key == InputKeyOk &&
        app->selection < profile_count) {
        app->profile_action_id = pocket_profile_id_at(app, app->selection);
        pocket_enter_screen(app, PocketScreenProfileActions);
    }
}

static void pocket_profile_actions_to_list(PocketD20App* app) {
    pocket_enter_screen(app, PocketScreenProfiles);
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
                    app, PocketEditCharacterName, "Character name", app->data.character.name);
            }
        } else if(app->selection == 2U) {
            uint32_t destination = pocket_d20_profiles_next_id(&app->profiles);
            bool duplicated =
                (profile != app->profiles.active_profile || pocket_flush_save(app, false)) &&
                !(destination == UINT32_MAX && pocket_profile_exists(app, UINT32_MAX)) &&
                pocket_d20_storage_duplicate_profile(app->storage, profile, destination) &&
                pocket_d20_profiles_refresh(app->storage, &app->profiles) &&
                pocket_d20_profiles_save(app->storage, &app->profiles);
            pocket_profile_actions_to_list(app);
            pocket_set_status(app, duplicated ? "Character duplicated" : "Duplicate failed");
        } else if(app->selection == 3U) {
            bool exported =
                (profile != app->profiles.active_profile || pocket_flush_save(app, false)) &&
                pocket_d20_storage_export_profile(app->storage, profile);
            if(exported)
                pocket_confirm_action(app, "Export written");
            else
                pocket_set_status(app, "Export failed");
        } else if(app->selection == 4U) {
            uint32_t previous = app->profiles.active_profile;
            uint32_t destination = pocket_d20_profiles_next_id(&app->profiles);
            bool imported =
                (!app->active_profile_loaded || pocket_flush_save(app, false)) &&
                !(destination == UINT32_MAX && pocket_profile_exists(app, UINT32_MAX)) &&
                pocket_d20_storage_import_first(app->storage, destination, &app->data);
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
                imported = pocket_d20_profiles_refresh(app->storage, &app->profiles) &&
                           pocket_d20_profiles_save(app->storage, &app->profiles);
                app->saved_fingerprint = pocket_data_fingerprint(&app->data);
                pocket_enter_screen(app, PocketScreenHome);
                pocket_set_status(app, imported ? "Character imported" : "Import metadata failed");
            } else {
                bool recovered = false;
                app->active_profile_loaded = pocket_d20_storage_load_profile(
                    app->storage, previous, &app->data, &recovered);
                app->spellbook_loaded = 0U;
                app->items_loaded = 0U;
                app->spellbook_total = 0U;
                app->items_total = 0U;
                app->spellbook_cache_start = 0U;
                app->items_cache_start = 0U;
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
            bool verified =
                (profile != app->profiles.active_profile || pocket_flush_save(app, false)) &&
                pocket_d20_storage_verify_profile(app->storage, profile);
            if(verified)
                pocket_confirm_action(app, "Profile readable");
            else
                pocket_set_status(app, "Save damaged/incompatible");
        } else if(app->selection == 8U && profile != app->profiles.active_profile) {
            pocket_set_status(app, "Switch before restore");
        } else if(app->selection == 8U) {
            bool restored = pocket_d20_storage_restore_backup(app->storage, profile, &app->data);
            if(!restored) {
                bool recovered = false;
                app->active_profile_loaded =
                    pocket_d20_storage_load_profile(app->storage, profile, &app->data, &recovered);
            } else {
                app->active_profile_loaded = 1U;
            }
            pocket_d20_profiles_refresh(app->storage, &app->profiles);
            pocket_d20_profiles_save(app->storage, &app->profiles);
            app->saved_fingerprint = pocket_data_fingerprint(&app->data);
            if(restored)
                pocket_confirm_action(app, "Backup restored");
            else
                pocket_set_status(app, "No valid backup");
        }
    }
}

static void pocket_request_launch(PocketD20App* app, PocketPendingLaunch launch) {
    if(!app) return;

    const bool passes_character_id = launch == PocketPendingLaunchJournal ||
                                     launch == PocketPendingLaunchAdventure ||
                                     launch == PocketPendingLaunchInitiative ||
                                     launch == PocketPendingLaunchBestiary;

    /* Preserve real character changes before tearing the app down. A missing
       active character is not a launch blocker: character companions receive no
       handoff argument and resolve the persisted active profile, falling forward
       to the next canonical character when that reference is stale. */
    if(app->active_profile_loaded && !pocket_flush_save(app, false) &&
       launch != PocketPendingLaunchBestiary) {
        pocket_set_status(app, "Save failed - launch cancelled");
        return;
    }

    app->pending_launch_args[0] = '\0';
    if(passes_character_id && app->active_profile_loaded) {
        int length = snprintf(
            app->pending_launch_args,
            sizeof(app->pending_launch_args),
            "%lu",
            (unsigned long)app->profiles.active_profile);
        if(length <= 0 || (size_t)length >= sizeof(app->pending_launch_args)) {
            app->pending_launch_args[0] = '\0';
            pocket_set_status(app, "Profile ID too long");
            return;
        }
    }

    /* Bestiary is never blocked by missing character state, but when a character
       is loaded its ID is passed so Add to Initiative targets that character. */
    app->pending_launch = launch;

    /* Quiesce callbacks and drop transient catalog storage before returning from
       the dispatcher. dndolphins_app() performs the authoritative full teardown
       before the shared handoff module opens Loader. */
    pocket_quiesce_async(app);
    pocket_catalog_release(app);
    view_dispatcher_stop(app->dispatcher);
}

static void pocket_handle_home(PocketD20App* app, const InputEvent* event) {
    const uint16_t base_count = sizeof(pocket_home_items) / sizeof(pocket_home_items[0]);
    uint16_t count = pocket_home_count(app);
    if(pocket_is_move_event(event) && event->key == InputKeyUp)
        pocket_menu_move(app, count, -1);
    else if(pocket_is_move_event(event) && event->key == InputKeyDown)
        pocket_menu_move(app, count, 1);
    else if(event->type == InputTypeShort && event->key == InputKeyOk) {
        if(app->selection >= base_count) {
            app->storage_read_only = 0U;
            pocket_save(app, true);
            return;
        }
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
            pocket_request_launch(app, PocketPendingLaunchJournal);
            break;
        case 11:
            pocket_request_launch(app, PocketPendingLaunchAdventure);
            break;
        case 12:
            pocket_request_launch(app, PocketPendingLaunchBestiary);
            break;
        case 13:
            app->hit_die_class_index = 0U;
            if(app->roll_mode == PocketRollGuidance) app->roll_mode = PocketRollNormal;
            pocket_enter_screen(app, PocketScreenCombat);
            break;
        case 14:
            pocket_request_launch(app, PocketPendingLaunchInitiative);
            break;
        case 15:
            pocket_enter_screen(app, PocketScreenDice);
            break;
        }
    }
}

static void pocket_handle_character(PocketD20App* app, const InputEvent* event) {
    PocketCharacter* character = &app->data.character;
    if(pocket_is_move_event(event) && event->key == InputKeyUp)
        pocket_menu_move(app, 14U, -1);
    else if(pocket_is_move_event(event) && event->key == InputKeyDown)
        pocket_menu_move(app, 14U, 1);
    else if(
        pocket_is_move_event(event) &&
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
    } else if(event->type == InputTypeLong && event->key == InputKeyOk && app->selection == 7U) {
        pocket_begin_number(
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
            pocket_begin_text(app, PocketEditSpecies, "Custom species", character->species);
        else if(app->selection == 3U)
            pocket_begin_text(
                app, PocketEditBackground, "Custom background", character->background);
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
                app, PocketCatalogBackgrounds, PocketEditBackground, character->background);
            break;
        case 4:
            pocket_open_catalog(
                app, PocketCatalogAlignments, PocketEditAlignment, character->alignment);
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
        case 12:
            if(pocket_begin_next_level_choice(app)) {
                app->return_screen = PocketScreenCharacter;
                pocket_enter_screen(app, PocketScreenLevelChoice);
            } else {
                pocket_set_status(app, "No pending level choices");
            }
            break;
        case 13: {
            pocket_stage_grants(app, PocketGrantSpecies, character->species);
            pocket_stage_grants(app, PocketGrantBackground, character->background);
            uint8_t saved_index = app->record_index;
            for(uint8_t i = 0U; i < character->class_count; ++i) {
                pocket_d20_apply_level_progression(character, i);
                app->record_index = i;
                pocket_stage_grants(app, PocketGrantClassFeature, character->classes[i].name);
                if(character->classes[i].subclass[0] &&
                   strcmp(character->classes[i].subclass, "None") != 0)
                    pocket_stage_grants(
                        app, PocketGrantSubclassFeature, character->classes[i].subclass);
            }
            app->record_index = saved_index;
            pocket_save(app, false);
            uint8_t pending = 0U;
            for(uint8_t i = 0U; i < character->grant_count; ++i)
                if(character->grants[i].status == PocketGrantPending) ++pending;
            snprintf(
                app->status,
                sizeof(app->status),
                pending ? "%u traits: review/apply" : "Initial traits current",
                pending);
            if(pending) {
                app->return_screen = PocketScreenCharacter;
                pocket_enter_screen(app, PocketScreenGrantReview);
            }
            break;
        }
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
    else if(
        pocket_is_move_event(event) &&
        (event->key == InputKeyLeft || event->key == InputKeyRight)) {
        int16_t delta = event->key == InputKeyRight ? 1 : -1;
        switch(app->selection) {
        case 0:
            character->hp_current = pocket_clamp_i16(character->hp_current + delta, 0, 999);
            break;
        case 1:
            character->hp_max = pocket_clamp_i16(character->hp_max + delta, 1, 999);
            if(character->hp_current > character->hp_max)
                character->hp_current = character->hp_max;
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
            character->death_successes = pocket_clamp_u8(character->death_successes + delta, 3U);
            break;
        case 9:
            character->death_failures = pocket_clamp_u8(character->death_failures + delta, 3U);
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
            character->skill_misc[11U] =
                (int8_t)pocket_clamp_i16(character->skill_misc[11U] + delta, -20, 20);
            break;
        case 14:
            character->skill_misc[6U] =
                (int8_t)pocket_clamp_i16(character->skill_misc[6U] + delta, -20, 20);
            break;
        case 15:
            character->skill_misc[8U] =
                (int8_t)pocket_clamp_i16(character->skill_misc[8U] + delta, -20, 20);
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
        pocket_begin_number(
            app, PocketNumberVitals, (uint8_t)app->selection, 0U, header, value, minimum, maximum);
    }
}

static void pocket_handle_abilities(PocketD20App* app, const InputEvent* event) {
    PocketCharacter* character = &app->data.character;
    if(pocket_is_move_event(event) && event->key == InputKeyUp)
        pocket_menu_move(app, POCKET_D20_ABILITY_COUNT, -1);
    else if(pocket_is_move_event(event) && event->key == InputKeyDown)
        pocket_menu_move(app, POCKET_D20_ABILITY_COUNT, 1);
    else if(
        pocket_is_move_event(event) &&
        (event->key == InputKeyLeft || event->key == InputKeyRight)) {
        int8_t delta = event->key == InputKeyRight ? 1 : -1;
        uint8_t index = app->selection;
        if(app->edit_modifier_mode) {
            character->saving_throw_misc[index] =
                (int8_t)pocket_clamp_i16(character->saving_throw_misc[index] + delta, -20, 20);
        } else {
            character->ability_scores[index] =
                (int8_t)pocket_clamp_i16(character->ability_scores[index] + delta, 1, 30);
        }
        pocket_save(app, false);
    } else if(
        event->type == InputTypeLong &&
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
    else if(
        pocket_is_move_event(event) &&
        (event->key == InputKeyLeft || event->key == InputKeyRight)) {
        int8_t delta = event->key == InputKeyRight ? 1 : -1;
        uint8_t index = pocket_skill_display_order[app->selection];
        if(app->edit_modifier_mode) {
            character->skill_misc[index] =
                (int8_t)pocket_clamp_i16(character->skill_misc[index] + delta, -20, 20);
        } else {
            int16_t proficiency = character->skill_proficiency[index] + delta;
            if(proficiency < 0) proficiency = PocketProficiencyExpertise;
            if(proficiency > PocketProficiencyExpertise) proficiency = PocketProficiencyNone;
            character->skill_proficiency[index] = (uint8_t)proficiency;
        }
        pocket_save(app, false);
    } else if(
        event->type == InputTypeLong &&
        (event->key == InputKeyLeft || event->key == InputKeyRight)) {
        app->edit_modifier_mode = !app->edit_modifier_mode;
        pocket_set_status(
            app, app->edit_modifier_mode ? "Editing skill misc" : "Editing proficiency");
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
        character->skill_proficiency[index] = (character->skill_proficiency[index] + 1U) % 3U;
        pocket_save(app, false);
    }
}


static void pocket_handle_level_choice(PocketD20App* app, const InputEvent* event) {
    if(pocket_is_move_event(event) && event->key == InputKeyUp) pocket_menu_move(app, 4U, -1);
    else if(pocket_is_move_event(event) && event->key == InputKeyDown) pocket_menu_move(app, 4U, 1);
    else if(event->type == InputTypeShort && event->key == InputKeyOk) {
        if(app->selection == 0U || app->selection == 1U) {
            app->level_choice_mode = app->selection == 0U ? 1U : 2U;
            app->level_choice_first_ability = UINT8_MAX;
            pocket_enter_screen(app, PocketScreenAsiAbility);
        } else if(app->selection == 2U) {
            PocketCharacter* c = &app->data.character;
            if(c->grant_count >= POCKET_D20_MAX_GRANTS || !pocket_d20_data_reserve_grants(c, c->grant_count + 1U)) {
                pocket_set_status(app, "Grant list full");
                return;
            }
            if(c->feature_count >= POCKET_D20_MAX_FEATURES || !pocket_d20_data_reserve_features(c, c->feature_count + 1U)) {
                pocket_set_status(app, "Feature list full");
                return;
            }
            app->record_index = c->feature_count++;
            memset(&c->features[app->record_index], 0, sizeof(PocketFeature));
            c->features[app->record_index].class_index = app->level_choice_class_index;
            c->features[app->record_index].class_level_gained = app->level_choice_level;
            app->level_choice_mode = 3U;
            pocket_open_catalog(app, PocketCatalogFeats, PocketEditFeatureName, "");
        } else {
            app->level_choice_mode = 0U;
            pocket_enter_screen(app, app->return_screen);
            pocket_set_status(app, "Level choice left pending");
        }
    }
}

static void pocket_handle_asi_ability(PocketD20App* app, const InputEvent* event) {
    PocketCharacter* c = &app->data.character;
    if(pocket_is_move_event(event) && event->key == InputKeyUp) pocket_menu_move(app, POCKET_D20_ABILITY_COUNT, -1);
    else if(pocket_is_move_event(event) && event->key == InputKeyDown) pocket_menu_move(app, POCKET_D20_ABILITY_COUNT, 1);
    else if(event->type == InputTypeShort && event->key == InputKeyOk) {
        uint8_t ability = (uint8_t)app->selection;
        if(app->level_choice_mode == 1U) {
            if(c->ability_scores[ability] >= 20) { pocket_set_status(app, "Ability already 20"); return; }
            int8_t old_score = c->ability_scores[ability];
            int16_t next = old_score + 2;
            c->ability_scores[ability] = next > 20 ? 20 : (int8_t)next;
            if(!pocket_complete_level_choice(app, "ASI +2")) {
                c->ability_scores[ability] = old_score;
                pocket_set_status(app, "Could not record choice");
                return;
            }
        } else if(app->level_choice_mode == 2U) {
            if(c->ability_scores[ability] >= 20) { pocket_set_status(app, "Ability already 20"); return; }
            if(app->level_choice_first_ability >= POCKET_D20_ABILITY_COUNT) {
                app->level_choice_first_ability = ability;
                pocket_set_status(app, "Choose second ability");
                return;
            }
            if(ability == app->level_choice_first_ability) { pocket_set_status(app, "Choose different ability"); return; }
            uint8_t first = app->level_choice_first_ability;
            c->ability_scores[first] += 1;
            c->ability_scores[ability] += 1;
            if(!pocket_complete_level_choice(app, "ASI +1/+1")) {
                c->ability_scores[first] -= 1;
                c->ability_scores[ability] -= 1;
                pocket_set_status(app, "Could not record choice");
                return;
            }
        } else return;
        pocket_save(app, false);
        app->level_choice_mode = 0U;
        app->level_choice_first_ability = UINT8_MAX;
        pocket_enter_screen(app, app->return_screen);
        pocket_set_status(app, "Level choice applied");
    }
}

static void pocket_handle_grant_review(PocketD20App* app, const InputEvent* event) {
    PocketCharacter* c = &app->data.character;
    uint16_t count = c->grant_count + 2U;
    if(pocket_is_move_event(event) && event->key == InputKeyUp)
        pocket_menu_move(app, count, -1);
    else if(pocket_is_move_event(event) && event->key == InputKeyDown)
        pocket_menu_move(app, count, 1);
    else if(
        event->type == InputTypeLong && event->key == InputKeyLeft && app->selection &&
        app->selection <= c->grant_count) {
        PocketGrant* grant = &c->grants[app->selection - 1U];
        if(grant->status == PocketGrantPending) grant->status = PocketGrantSkipped;
        pocket_save(app, false);
    } else if(
        event->type == InputTypeLong && event->key == InputKeyOk && app->selection &&
        app->selection <= c->grant_count) {
        app->record_index = app->selection - 1U;
        pocket_enter_screen(app, PocketScreenGrantEdit);
    } else if(event->type == InputTypeShort && event->key == InputKeyOk) {
        if(app->selection == 0U) {
            for(uint8_t i = 0U; i < c->grant_count; ++i)
                if(c->grants[i].status == PocketGrantPending)
                    pocket_apply_grant(app, &c->grants[i]);
            pocket_set_status(app, "Pending grants applied");
        } else if(app->selection <= c->grant_count) {
            PocketGrant* grant = &c->grants[app->selection - 1U];
            if(grant->status == PocketGrantPending)
                pocket_apply_grant(app, grant);
            else if(grant->status == PocketGrantSkipped)
                grant->status = PocketGrantPending;
        } else if(
            c->grant_count < POCKET_D20_MAX_GRANTS &&
            pocket_d20_data_reserve_grants(c, c->grant_count + 1U)) {
            app->record_index = c->grant_count++;
            PocketGrant* grant = &c->grants[app->record_index];
            memset(grant, 0, sizeof(*grant));
            snprintf(
                grant->stable_id,
                sizeof(grant->stable_id),
                "custom_grant_%u",
                app->record_index + 1U);
            pocket_copy(grant->source, sizeof(grant->source), "Custom");
            pocket_copy(grant->option_name, sizeof(grant->option_name), "Custom Grant");
            pocket_copy(grant->prerequisites, sizeof(grant->prerequisites), "None");
            pocket_copy(grant->grant_value, sizeof(grant->grant_value), "feature=Custom Feature");
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
    else if(
        pocket_is_move_event(event) &&
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
            grant->level_gained = (uint8_t)pocket_clamp_i16(grant->level_gained + delta, 0, 20);
        else if(app->selection == 8U) {
            int16_t value = grant->status + delta;
            if(value < 0) value = PocketGrantSkipped;
            if(value > PocketGrantSkipped) value = PocketGrantPending;
            grant->status = (uint8_t)value;
        } else
            return;
        pocket_save(app, false);
    } else if(event->type == InputTypeShort && event->key == InputKeyOk) {
        if(app->selection == 0U)
            pocket_begin_text(app, PocketEditGrantStableId, "Stable grant ID", grant->stable_id);
        else if(app->selection == 1U)
            pocket_begin_text(app, PocketEditGrantSource, "Source label", grant->source);
        else if(app->selection == 3U)
            pocket_begin_text(app, PocketEditGrantOption, "Option name", grant->option_name);
        else if(app->selection == 4U)
            pocket_begin_text(
                app, PocketEditGrantPrerequisites, "Prerequisites", grant->prerequisites);
        else if(app->selection == 7U)
            pocket_begin_text(
                app, PocketEditGrantValue, "Grant payload key=value", grant->grant_value);
        else if(app->selection == 9U) {
            memmove(
                &c->grants[app->record_index],
                &c->grants[app->record_index + 1U],
                (c->grant_count - app->record_index - 1U) * sizeof(PocketGrant));
            --c->grant_count;
            pocket_save(app, false);
            pocket_enter_screen(app, PocketScreenGrantReview);
        }
    }
}


static void pocket_handle_spell_filters(PocketD20App* app, const InputEvent* event) {
    PocketCharacter* c = &app->data.character;
    if(pocket_is_move_event(event) && event->key == InputKeyUp)
        pocket_menu_move(app, 6U, -1);
    else if(pocket_is_move_event(event) && event->key == InputKeyDown)
        pocket_menu_move(app, 6U, 1);
    else if(
        pocket_is_move_event(event) &&
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
    else if(
        pocket_is_move_event(event) &&
        (event->key == InputKeyLeft || event->key == InputKeyRight)) {
        int16_t delta = event->key == InputKeyRight ? 1 : -1;
        if(app->selection == 3U)
            c->encumbrance_mode = !c->encumbrance_mode;
        else if(app->selection == 8U)
            c->carrying_capacity_override =
                pocket_clamp_i16(c->carrying_capacity_override + delta, 0, 999);
        else
            return;
        pocket_save(app, false);
    } else if(event->type == InputTypeShort && event->key == InputKeyOk) {
        if(app->selection == 6U) {
            PocketD20ItemAggregate aggregate;
            if(!pocket_item_aggregate_streamed(app, &aggregate)) {
                pocket_set_status(app, "Items read failed");
                return;
            }
            c->armor_class = pocket_d20_items_calculated_armor_class(c, &aggregate);
            pocket_confirm_action(app, "Armor Class applied");
        } else if(app->selection == 7U) {
            pocket_d20_normalize_currency(c);
            pocket_confirm_action(app, "Coins normalized");
        } else if(app->selection == 3U) {
            c->encumbrance_mode = !c->encumbrance_mode;
        } else {
            return;
        }
        pocket_save(app, false);
    }
}


static void pocket_handle_attack_templates(PocketD20App* app, const InputEvent* event) {
    PocketCharacter* c = &app->data.character;
    uint16_t count = c->attack_template_count + 1U;
    if(pocket_is_move_event(event) && event->key == InputKeyUp)
        pocket_menu_move(app, count, -1);
    else if(pocket_is_move_event(event) && event->key == InputKeyDown)
        pocket_menu_move(app, count, 1);
    else if(
        pocket_is_move_event(event) && app->selection < c->attack_template_count &&
        (event->key == InputKeyLeft || event->key == InputKeyRight)) {
        PocketAttackTemplate* attack = &c->attack_templates[app->selection];
        attack->attack_misc = (int8_t)pocket_clamp_i16(
            attack->attack_misc + (event->key == InputKeyRight ? 1 : -1), -20, 20);
        pocket_save(app, false);
    } else if(
        event->type == InputTypeLong && event->key == InputKeyOk &&
        app->selection < c->attack_template_count) {
        app->record_index = app->selection;
        pocket_enter_screen(app, PocketScreenAttackTemplateEdit);
    } else if(
        event->type == InputTypeShort && event->key == InputKeyOk &&
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
    } else if(
        event->type == InputTypeShort && event->key == InputKeyOk &&
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
    else if(
        pocket_is_move_event(event) &&
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
        } else
            return;
        pocket_save(app, false);
    } else if(event->type == InputTypeShort && event->key == InputKeyOk) {
        if(app->selection == 0U)
            pocket_begin_text(app, PocketEditAttackName, "Attack template name", attack->name);
        else if(app->selection == 8U)
            pocket_begin_text(app, PocketEditAttackDamageType, "Damage type", attack->damage_type);
        else if(app->selection == 9U)
            pocket_begin_text(app, PocketEditAttackMastery, "Mastery property", attack->mastery);
        else if(app->selection == 12U)
            pocket_begin_text(app, PocketEditAttackRiderType, "Rider type", attack->rider_type);
        else if(app->selection == 14U) {
            memmove(
                &c->attack_templates[app->record_index],
                &c->attack_templates[app->record_index + 1U],
                (c->attack_template_count - app->record_index - 1U) *
                    sizeof(PocketAttackTemplate));
            --c->attack_template_count;
            pocket_save(app, false);
            pocket_enter_screen(app, PocketScreenAttackTemplates);
        }
    }
}

static void pocket_handle_magic(PocketD20App* app, const InputEvent* event) {
    PocketCharacter* character = &app->data.character;
    if(pocket_is_move_event(event) && event->key == InputKeyUp)
        pocket_menu_move(app, 16U, -1);
    else if(pocket_is_move_event(event) && event->key == InputKeyDown)
        pocket_menu_move(app, 16U, 1);
    else if(
        pocket_is_move_event(event) &&
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
                if(character->spell_slots_current[level] >= character->spell_slots_max[level]) {
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
            character->spell_attack_misc =
                (int8_t)pocket_clamp_i16(character->spell_attack_misc + delta, -20, 20);
        } else if(app->selection == 4U) {
            character->spell_save_misc =
                (int8_t)pocket_clamp_i16(character->spell_save_misc + delta, -20, 20);
        } else if(app->selection >= 7U && app->selection <= 15U) {
            if(event->type != InputTypeShort) return;
            uint8_t level = app->selection - 6U;
            character->spell_slots_current[level] = pocket_clamp_u8(
                character->spell_slots_current[level] + delta,
                character->spell_slots_max[level]);
            pocket_set_status(app, "Available slots changed");
        } else {
            return;
        }
        pocket_save(app, false);
    } else if(
        !app->arcane_recovery_active && event->type == InputTypeLong &&
        (event->key == InputKeyLeft || event->key == InputKeyRight) && app->selection >= 7U &&
        app->selection <= 15U) {
        int8_t delta = event->key == InputKeyRight ? 1 : -1;
        uint8_t level = app->selection - 6U;
        character->spell_slots_max[level] =
            pocket_clamp_u8(character->spell_slots_max[level] + delta, 20U);
        if(character->spell_slots_current[level] > character->spell_slots_max[level])
            character->spell_slots_current[level] = character->spell_slots_max[level];
        pocket_save(app, false);
        pocket_set_status(app, "Maximum slots changed");
    } else if(event->type == InputTypeLong && event->key == InputKeyOk) {
        if(app->arcane_recovery_active) {
            pocket_set_status(app, "Finish recovery first");
        } else if(app->selection == 0U) {
            pocket_enter_screen(app, PocketScreenSpellFilters);
            pocket_set_status(app, "Filters affect next picker");
        } else if(app->selection == 2U) {
            pocket_d20_recalculate_multiclass_slots(character);
            pocket_save(app, false);
            pocket_confirm_action(app, "Class slots recalculated");
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
            pocket_begin_number(
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
            pocket_open_list(app, PocketListSpells, PocketScreenMagic);
        else if(app->selection == 6U) {
            if(app->arcane_recovery_active) {
                app->arcane_recovery_active = 0U;
                pocket_set_status(
                    app, app->arcane_recovery_spent ? "Arcane Recovery used" : "Recovery skipped");
            } else if(character->arcane_recovery_used) {
                pocket_set_status(app, "Recovery already used");
            } else {
                pocket_set_status(app, "Finish a Short Rest first");
            }
        } else if(!app->arcane_recovery_active && app->selection >= 7U && app->selection <= 15U) {
            uint8_t level = app->selection - 6U;
            pocket_begin_number(
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
    else if(
        pocket_is_move_event(event) &&
        (event->key == InputKeyLeft || event->key == InputKeyRight)) {
        int64_t next = *values[app->selection] + (event->key == InputKeyRight ? 1 : -1);
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
    if(pocket_is_move_event(event) && event->key == InputKeyUp) {
        uint16_t previous_selection = app->selection;
        uint16_t previous_scroll = app->scroll;
        pocket_menu_move(app, count, -1);
        if(!pocket_record_list_prepare_sidecar(app)) {
            app->selection = previous_selection;
            app->scroll = previous_scroll;
            pocket_set_status(app, "Page load failed - retry SD");
        }
    } else if(pocket_is_move_event(event) && event->key == InputKeyDown) {
        uint16_t previous_selection = app->selection;
        uint16_t previous_scroll = app->scroll;
        pocket_menu_move(app, count, 1);
        if(!pocket_record_list_prepare_sidecar(app)) {
            app->selection = previous_selection;
            app->scroll = previous_scroll;
            pocket_set_status(app, "Page load failed - retry SD");
        }
    } else if(
        (event->type == InputTypeShort || event->type == InputTypeLong) &&
        event->key == InputKeyOk && app->selection == 0U &&
        (app->list_kind == PocketListSpells || app->list_kind == PocketListItems)) {
        /* Add New always creates a blank record and enters its full editor.
           Catalog selection remains available from the Name field inside that editor.
           Accept both short and long OK so neither gesture can strand this row. */
        if(!pocket_add_record(app)) {
            bool full = app->list_kind == PocketListSpells ?
                            app->spellbook_total >= POCKET_D20_MAX_SPELLS :
                            app->items_total >= POCKET_D20_MAX_ITEMS;
            pocket_set_status(app, full ? "List is full" : "Add failed - retry SD");
        }
    } else if(
        event->type == InputTypeLong && event->key == InputKeyOk &&
        app->list_kind == PocketListSpells && app->selection > 0U) {
        uint8_t index = (uint8_t)(app->selection - 1U);
        PocketCharacter* character = &app->data.character;
        uint8_t local = 0U;
        PocketSpell* spell = pocket_spell_at(app, index, &local);
        if(!spell) {
            pocket_set_status(app, "Spell read failed");
        } else if(character->spell_always_prepared[local]) {
            pocket_set_status(app, "Always prepared");
        } else if(!character->spell_known[local]) {
            pocket_set_status(app, "Spell is not known");
        } else {
            spell->prepared = !spell->prepared;
            bool saved = pocket_save_spellbook_if_changed(app);
            pocket_save(app, false);
            if(saved)
                pocket_confirm_action(app, spell->prepared ? "Spell prepared" : "Spell unprepared");
        }
    } else if(
        event->type == InputTypeLong && event->key == InputKeyOk &&
        app->list_kind == PocketListItems && app->selection > 0U) {
        uint8_t index = (uint8_t)(app->selection - 1U);
        PocketItem* item = pocket_item_at(app, index, NULL);
        if(!item) {
            pocket_set_status(app, "Item read failed");
        } else {
            item->equipped = !item->equipped;
            bool saved = pocket_save_items_if_changed(app);
            pocket_save(app, false);
            if(saved)
                pocket_confirm_action(app, item->equipped ? "Item equipped" : "Item unequipped");
        }
    } else if(event->type == InputTypeShort && event->key == InputKeyOk) {
        if(app->selection == 0U) {
            if(!pocket_add_record(app)) {
                bool full = false;
                if(app->list_kind == PocketListClasses)
                    full = app->data.character.class_count >= POCKET_D20_MAX_CLASSES ||
                           pocket_d20_total_level(&app->data.character) >= 20U;
                else if(app->list_kind == PocketListFeatures)
                    full = app->data.character.feature_count >= POCKET_D20_MAX_FEATURES;
                else if(app->list_kind == PocketListLanguages)
                    full = app->data.character.language_count >= POCKET_D20_MAX_LANGUAGES;
                pocket_set_status(app, full ? "List is full" : "Add failed - retry SD");
            }
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
        uint8_t previous_total_level = pocket_d20_total_level(character);
        PocketClassLevel* class_level = &character->classes[index];
        uint8_t previous_cantrip_limit = class_level->cantrip_limit;
        uint8_t previous_prepared_limit = class_level->prepared_limit;
        if(field == 2U) {
            uint8_t total = pocket_d20_total_level(character);
            int16_t maximum = 20 - (total - class_level->level);
            class_level->level = (uint8_t)pocket_clamp_i16(
                class_level->level + delta, 1, maximum < 1 ? 1 : maximum);
        } else if(field == 3U) {
            class_level->hit_die = pocket_cycle_die(class_level->hit_die, delta, true);
        } else if(field == 4U) {
            class_level->hit_dice_current =
                pocket_clamp_u8(class_level->hit_dice_current + delta, class_level->hit_dice_max);
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
            class_level->prepared_limit =
                pocket_clamp_u8(class_level->prepared_limit + delta, 50U);
        } else if(field == 10U) {
            class_level->spellbook_size =
                (uint16_t)pocket_clamp_i16(class_level->spellbook_size + delta, 0, 999);
        } else if(field == 11U) {
            class_level->pact_slot_level =
                pocket_clamp_u8(class_level->pact_slot_level + delta, 5U);
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
            class_level->spell_points_max =
                (uint16_t)pocket_clamp_i16(class_level->spell_points_max + delta, 0, 999);
            if(class_level->spell_points_current > class_level->spell_points_max)
                class_level->spell_points_current = class_level->spell_points_max;
        } else {
            return;
        }
        if(field == 2U && pocket_d20_total_level(character) > previous_total_level) {
            dndolphins_apply_experience_floor(character);
            pocket_d20_apply_level_progression(character, index);
            uint8_t applied = pocket_apply_level_grants(app, index);
            bool choose_spells = class_level->cantrip_limit > previous_cantrip_limit ||
                                 class_level->prepared_limit > previous_prepared_limit;
            bool choose_level = pocket_begin_next_level_choice(app);
            snprintf(
                app->status,
                sizeof(app->status),
                choose_level ? "Level up: ASI/feat choice" : choose_spells ? "Level up: choose spells" :
                applied ? "Level up: %u trait(s)" : "Level up: rules updated",
                applied);
        }
        break;
    }
    case PocketListSpells: {
        uint8_t local = 0U;
        PocketSpell* spell = pocket_spell_at(app, index, &local);
        if(!spell) return;
        if(field == 2U) {
            int16_t class_index = spell->class_index + delta;
            if(class_index < 0) class_index = character->class_count - 1U;
            if(class_index >= character->class_count) class_index = 0;
            spell->class_index = (uint8_t)class_index;
        } else if(field == 3U)
            spell->level = pocket_clamp_u8(spell->level + delta, 9U);
        else if(field == 4U) {
            character->spell_known[local] = !character->spell_known[local];
            if(!character->spell_known[local]) {
                spell->prepared = 0U;
                character->spell_always_prepared[local] = 0U;
            }
        } else if(field == 5U) {
            spell->prepared = !spell->prepared;
            if(spell->prepared) character->spell_known[local] = 1U;
        } else if(field == 6U) {
            character->spell_always_prepared[local] = !character->spell_always_prepared[local];
            if(character->spell_always_prepared[local]) character->spell_known[local] = 1U;
        } else if(field == 7U) {
            spell->ritual = !spell->ritual;
        } else if(field == 8U) {
            character->spell_free_casts_current[local] = pocket_clamp_u8(
                character->spell_free_casts_current[local] + delta,
                character->spell_free_casts_max[local]);
        } else if(field == 9U) {
            character->spell_free_casts_max[local] =
                pocket_clamp_u8(character->spell_free_casts_max[local] + delta, 20U);
            if(character->spell_free_casts_current[local] > character->spell_free_casts_max[local])
                character->spell_free_casts_current[local] =
                    character->spell_free_casts_max[local];
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
            feature->uses_current =
                pocket_clamp_i16(feature->uses_current + delta, 0, feature->uses_max);
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
        PocketItem* item = pocket_item_at(app, index, NULL);
        if(!item) return;
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
                item->versatile_die ? item->versatile_die : item->damage_die, delta, true);
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
                item->container_index + delta, -1, app->items_total ? app->items_total - 1U : 0U);
            if(item->container_index == (int8_t)index) item->container_index = -1;
            break;
        case 29:
            item->charges_current =
                pocket_clamp_i16(item->charges_current + delta, 0, item->charges_max);
            break;
        case 30:
            item->charges_max = pocket_clamp_i16(item->charges_max + delta, 0, 999);
            if(item->charges_current > item->charges_max)
                item->charges_current = item->charges_max;
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
    case PocketListLanguages:
        return;
    }
    if(app->list_kind == PocketListItems)
        (void)pocket_save_items_if_changed(app);
    else if(app->list_kind == PocketListSpells)
        (void)pocket_save_spellbook_if_changed(app);
    pocket_save(app, false);
}

static void pocket_handle_record_detail_ok(PocketD20App* app) {
    PocketCharacter* character = &app->data.character;
    uint8_t index = app->record_index;
    uint8_t field = app->selection;
    switch(app->list_kind) {
    case PocketListClasses:
        if(field == 0U)
            pocket_open_catalog(
                app, PocketCatalogClasses, PocketEditClassName, character->classes[index].name);
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
    case PocketListSpells: {
        uint8_t local = 0U;
        PocketSpell* spell = pocket_spell_at(app, index, &local);
        if(!spell) return;
        if(field == 0U)
            pocket_open_catalog(
                app, PocketCatalogSpells, PocketEditSpellName, spell->name);
        else if(field == 1U)
            pocket_begin_text(
                app, PocketEditSpellDetail, "Spell notes", spell->detail);
        else if(field < 10U)
            pocket_adjust_record(app, 1);
        else if(field == 10U) {
            if(character->spell_free_casts_current[local]) {
                --character->spell_free_casts_current[local];
                (void)pocket_save_spellbook_if_changed(app);
                pocket_save(app, false);
                pocket_set_status(app, "Free cast used");
            } else {
                pocket_set_status(app, "No free casts left");
            }
        } else if(field == 11U)
            pocket_begin_text(
                app, PocketEditSpellStableId, "Stable ID", spell->stable_id);
        else if(field == 12U)
            pocket_begin_text(
                app, PocketEditSpellSource, "Spell source", spell->source);
        else if(field == 13U)
            pocket_begin_text(
                app, PocketEditSpellSchool, "Spell school", spell->school);
        else if(field == 14U)
            pocket_begin_text(
                app,
                PocketEditSpellGrantName,
                "Grant source name",
                spell->grant_name);
        else if(field == 15U)
            pocket_adjust_record(app, 1);
        else {
            pocket_delete_record(app);
        }
        break;
    }
    case PocketListFeatures:
        if(field == 0U)
            pocket_open_catalog(
                app, PocketCatalogFeats, PocketEditFeatureName, character->features[index].name);
        else if(field == 1U)
            pocket_begin_text(
                app, PocketEditFeatureDetail, "Feature notes", character->features[index].detail);
        else if(field < 9U)
            pocket_adjust_record(app, 1);
        else
            pocket_delete_record(app);
        break;
    case PocketListItems: {
        PocketItem* item = pocket_item_at(app, index, NULL);
        if(!item) return;
        if(field == 0U)
            pocket_open_catalog(
                app, PocketCatalogItems, PocketEditItemName, item->name);
        else if(field == 1U)
            pocket_begin_text(
                app, PocketEditItemDetail, "Item notes", item->detail);
        else if((field >= 2U && field <= 26U) || (field >= 28U && field <= 33U))
            pocket_adjust_record(app, 1);
        else if(field == 34U)
            pocket_begin_text(
                app,
                PocketEditItemAmmoGroup,
                "Ammunition group",
                item->ammunition_group);
        else if(field == 35U)
            pocket_delete_record(app);
        break;
    }
    case PocketListLanguages:
        if(field == 0U)
            pocket_begin_text(
                app, PocketEditLanguageName, "Language", character->languages[index]);
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
    } else if(app->list_kind == PocketListSpells) {
        uint8_t local = 0U;
        PocketSpell* spell = pocket_spell_at(app, index, &local);
        if(!spell) return false;
        if(field == 3U) {
            header = "Spell level";
            value = spell->level;
            maximum = 9;
        } else if(field == 8U) {
            header = "Free casts current";
            value = character->spell_free_casts_current[local];
            maximum = character->spell_free_casts_max[local];
        } else if(field == 9U) {
            header = "Free casts maximum";
            value = character->spell_free_casts_max[local];
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
        PocketItem* item = pocket_item_at(app, index, NULL);
        if(!item) return false;
        switch(field) {
        case 2U:
            header = "Item quantity";
            value = item->quantity;
            break;
        case 3U:
            header = "Weight in tenths lb";
            value = item->weight_tenths;
            maximum = 9999;
            break;
        case 9U:
            header = "Magic bonus";
            value = item->magic_bonus;
            minimum = -10;
            maximum = 10;
            break;
        case 10U:
            header = "Damage dice count";
            value = item->damage_dice;
            maximum = 20;
            break;
        case 11U:
            header = "Damage die sides";
            value = item->damage_die;
            minimum = 4;
            maximum = 12;
            break;
        case 13U:
            header = "Versatile die sides";
            value = item->versatile_die;
            minimum = 4;
            maximum = 12;
            break;
        case 23U:
            header = "Extra dice count";
            value = item->extra_dice;
            maximum = 20;
            break;
        case 24U:
            header = "Extra die sides";
            value = item->extra_die;
            minimum = 4;
            maximum = 12;
            break;
        case 25U:
            header = "Ammo current";
            value = item->ammo_current;
            maximum = item->ammo_max;
            break;
        case 26U:
            header = "Ammo maximum";
            value = item->ammo_max;
            break;
        case 29U:
            header = "Charges current";
            value = item->charges_current;
            maximum = item->charges_max;
            break;
        case 30U:
            header = "Charges maximum";
            value = item->charges_max;
            break;
        case 31U:
            header = "Armor base";
            value = item->armor_base;
            maximum = 30;
            break;
        case 32U:
            header = "Armor DEX cap";
            value = item->armor_dex_cap;
            minimum = -1;
            maximum = 9;
            break;
        case 33U:
            header = "Shield bonus";
            value = item->shield_bonus;
            maximum = 10;
            break;
        default:
            return false;
        }
    } else {
        return false;
    }
    pocket_begin_number(app, PocketNumberRecord, field, 0U, header, value, minimum, maximum);
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
    else if(app->list_kind == PocketListSpells && app->selection == 0U) {
        PocketSpell* spell = pocket_spell_at(app, index, NULL);
        if(spell) pocket_begin_text(app, PocketEditSpellName, "Custom spell", spell->name);
    }
    else if(app->list_kind == PocketListFeatures && app->selection == 0U)
        pocket_begin_text(
            app, PocketEditFeatureName, "Custom feat/perk", character->features[index].name);
    else if(app->list_kind == PocketListItems && app->selection == 0U) {
        PocketItem* item = pocket_item_at(app, index, NULL);
        if(item) pocket_begin_text(app, PocketEditItemName, "Custom item", item->name);
    }
}

static void pocket_handle_record_detail(PocketD20App* app, const InputEvent* event) {
    uint8_t count = pocket_record_detail_count(app);
    if(pocket_is_move_event(event) && event->key == InputKeyUp)
        pocket_menu_move(app, count, -1);
    else if(pocket_is_move_event(event) && event->key == InputKeyDown)
        pocket_menu_move(app, count, 1);
    else if(pocket_is_move_event(event) && (event->key == InputKeyLeft || event->key == InputKeyRight))
        pocket_adjust_record(app, event->key == InputKeyRight ? 1 : -1);
    else if(event->type == InputTypeLong && event->key == InputKeyOk) {
        if(!pocket_begin_record_number(app)) pocket_handle_record_detail_custom_name(app);
    } else if(event->type == InputTypeShort && event->key == InputKeyOk)
        pocket_handle_record_detail_ok(app);
}

static void pocket_apply_catalog_selection(PocketD20App* app) {
    if(app->selection >= app->catalog_count) return;
    char selected[POCKET_D20_CATALOG_NAME_LEN];
    pocket_copy(selected, sizeof(selected), app->catalog_entries[app->selection]);
    uint8_t selected_level = app->catalog_levels[app->selection];
    uint8_t selected_has_metadata = app->catalog_has_metadata[app->selection];
    uint16_t selected_class_mask = app->catalog_class_masks[app->selection];
    uint8_t selected_item_category = app->catalog_item_categories[app->selection];
    uint8_t selected_school = app->catalog_schools[app->selection];
    uint8_t selected_source = app->catalog_sources[app->selection];
    uint8_t selected_ritual = app->catalog_ritual[app->selection];
    PocketCharacter* character = &app->data.character;
    uint8_t index = app->record_index;
    uint8_t grant_source = PocketGrantSourceCount;
    bool save_spellbook_after_catalog = false;
    bool save_items_after_catalog = false;
    switch(app->catalog_target) {
    case PocketEditClassName:
        pocket_copy(
            character->classes[index].name, sizeof(character->classes[index].name), selected);
        pocket_configure_class_defaults(&character->classes[index]);
        pocket_d20_initialize_spell_slots_if_unset(character);
        /* Level-1 traits are intentionally gated behind Character > Grant Initial Traits. */
        break;
    case PocketEditSubclass:
        pocket_copy(
            character->classes[index].subclass,
            sizeof(character->classes[index].subclass),
            selected);
        /* Subclass grants are applied by progression, not merely by browsing/selecting. */
        break;
    case PocketEditSpecies:
        pocket_copy(character->species, sizeof(character->species), selected);
        /* Species traits are intentionally gated behind Grant Initial Traits. */
        break;
    case PocketEditAddSpell: {
        if(!pocket_spell_prepare_append_page(app)) {
            pocket_set_status(app, "Spell add failed");
            return;
        }
        const uint8_t local = character->spell_count;
        PocketSpell* spell = &character->spells[local];
        memset(spell, 0, sizeof(*spell));
        pocket_copy(spell->name, sizeof(spell->name), selected);
        if(selected_has_metadata) {
            spell->level = selected_level;
            spell->class_index = pocket_spell_resolve_class(
                app, selected_level, selected_class_mask, spell->class_index);
            snprintf(
                spell->stable_id,
                sizeof(spell->stable_id),
                "spell-%u-%u",
                spell->level,
                app->catalog_page_start + app->selection);
            if(selected_school)
                pocket_copy(
                    spell->school,
                    sizeof(spell->school),
                    pocket_spell_school_names[selected_school]);
            pocket_copy(
                spell->source,
                sizeof(spell->source),
                selected_source == 1U ? "Core" : "Add-on");
            spell->ritual = selected_ritual;
        }
        character->spell_known[local] = 1U;
        character->spell_always_prepared[local] = 0U;
        character->spell_free_casts_current[local] = 0U;
        character->spell_free_casts_max[local] = 0U;
        ++character->spell_count;
        app->record_index = app->spellbook_total++;
        app->spell_class_counts_valid = 0U;
        pocket_catalog_release(app);
        bool saved = pocket_save_spellbook_if_changed(app);
        pocket_enter_screen(app, PocketScreenRecordDetail);
        pocket_set_status(app, saved ? "Spell added" : "Spell added - UNSAVED");
        return;
    }
    case PocketEditSpellName: {
        PocketSpell* spell = pocket_spell_at(app, index, NULL);
        if(!spell) return;
        pocket_copy(spell->name, sizeof(spell->name), selected);
        if(selected_has_metadata) {
            spell->level = selected_level;
            spell->class_index = pocket_spell_resolve_class(
                app, selected_level, selected_class_mask, spell->class_index);
            snprintf(
                spell->stable_id,
                sizeof(spell->stable_id),
                "spell-%u-%u",
                spell->level,
                app->catalog_page_start + app->selection);
            spell->school[0] = '\0';
            if(selected_school)
                pocket_copy(
                    spell->school,
                    sizeof(spell->school),
                    pocket_spell_school_names[selected_school]);
            pocket_copy(
                spell->source,
                sizeof(spell->source),
                selected_source == 1U ? "Core" : "Add-on");
            spell->ritual = selected_ritual;
        }
        save_spellbook_after_catalog = true;
        break;
    }
    case PocketEditFeatureName:
        pocket_copy(
            character->features[index].name, sizeof(character->features[index].name), selected);
        grant_source = PocketGrantFeat;
        if(app->level_choice_mode == 3U) {
            if(!pocket_complete_level_choice(app, "feat")) {
                pocket_set_status(app, "Could not record feat choice");
                return;
            }
            app->level_choice_mode = 0U;
            app->return_screen = PocketScreenCharacter;
        }
        break;
    case PocketEditAddItem: {
        if(!pocket_item_prepare_append_page(app)) {
            pocket_set_status(app, "Item add failed");
            return;
        }
        const uint8_t local = character->item_count;
        PocketItem* item = &character->items[local];
        memset(item, 0, sizeof(*item));
        item->quantity = 1;
        item->damage_dice = 1U;
        item->damage_die = 6U;
        item->extra_die = 6U;
        item->add_ability_damage = 1U;
        item->container_index = -1;
        item->armor_dex_cap = -1;
        pocket_copy(item->name, sizeof(item->name), selected);
        pocket_apply_equipment_preset(item, selected, selected_item_category);
        ++character->item_count;
        app->record_index = app->items_total++;
        app->item_aggregate_valid = 0U;
        pocket_catalog_release(app);
        bool saved = pocket_save_items_if_changed(app);
        uint8_t staged = pocket_stage_grants(app, PocketGrantItem, selected);
        if(staged) {
            pocket_save(app, false);
            app->return_screen = PocketScreenRecordDetail;
            pocket_enter_screen(app, PocketScreenGrantReview);
            snprintf(app->status, sizeof(app->status), "%u grants to review", staged);
            return;
        }
        pocket_enter_screen(app, PocketScreenRecordDetail);
        pocket_set_status(app, saved ? "Item added" : "Item added - UNSAVED");
        return;
    }
    case PocketEditItemName: {
        PocketItem* item = pocket_item_at(app, index, NULL);
        if(!item) return;
        pocket_copy(item->name, sizeof(item->name), selected);
        pocket_apply_equipment_preset(item, selected, selected_item_category);
        save_items_after_catalog = true;
        grant_source = PocketGrantItem;
        break;
    }
    case PocketEditBackground:
        pocket_copy(character->background, sizeof(character->background), selected);
        /* Background traits are intentionally gated behind Grant Initial Traits. */
        break;
    case PocketEditAlignment:
        pocket_copy(character->alignment, sizeof(character->alignment), selected);
        break;
    default:
        return;
    }
    pocket_catalog_release(app);
    if(save_spellbook_after_catalog) (void)pocket_save_spellbook_if_changed(app);
    if(save_items_after_catalog) (void)pocket_save_items_if_changed(app);
    uint8_t staged = grant_source < PocketGrantSourceCount ?
                         pocket_stage_grants(app, grant_source, selected) :
                         0U;
    pocket_save(app, false);
    PocketScreen destination = app->return_screen;
    if(staged) {
        app->return_screen = destination;
        pocket_enter_screen(app, PocketScreenGrantReview);
        snprintf(app->status, sizeof(app->status), "%u grants to review", staged);
        return;
    }
    pocket_enter_screen(app, destination);
    app->selection = app->catalog_return_selection;
    if(app->selection >= 5U) app->scroll = app->selection - 4U;
    pocket_set_status(app, "Catalog choice saved");
}

static void pocket_handle_catalog(PocketD20App* app, const InputEvent* event) {
    if(app->catalog_count && pocket_is_move_event(event) && event->key == InputKeyUp)
        pocket_menu_move(app, app->catalog_count, -1);
    else if(app->catalog_count && pocket_is_move_event(event) && event->key == InputKeyDown)
        pocket_menu_move(app, app->catalog_count, 1);
    else if(
        pocket_is_move_event(event) &&
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
    } else if(
        event->type == InputTypeLong && event->key == InputKeyOk &&
        (app->catalog_kind == PocketCatalogSpells ||
         app->catalog_kind == PocketCatalogSubclasses)) {
        app->catalog_show_all = !app->catalog_show_all;
        app->catalog_page_start = 0U;
        app->selection = 0U;
        app->scroll = 0U;
        pocket_catalog_load_page(app);
        pocket_set_status(
            app,
            app->catalog_show_all                    ? "Showing all" :
            app->catalog_kind == PocketCatalogSpells ? "Class + level filter" :
                                                       "Class filter");
    } else if(event->type == InputTypeShort && event->key == InputKeyOk)
        pocket_apply_catalog_selection(app);
}


static void pocket_handle_spell_attacks(PocketD20App* app, const InputEvent* event) {
    uint8_t count = pocket_combat_spell_count(app);
    if(!count) return;
    if(pocket_is_move_event(event) && event->key == InputKeyUp)
        pocket_menu_move(app, count, -1);
    else if(pocket_is_move_event(event) && event->key == InputKeyDown)
        pocket_menu_move(app, count, 1);
    else if(event->type == InputTypeShort && event->key == InputKeyOk) {
        uint8_t spell_index = pocket_combat_spell_index(app, app->selection);
        if(spell_index == 0xFFU) return;
        app->spell_attack_index = spell_index;
        PocketSpellCastOption options[POCKET_D20_MAX_SPELL_CAST_OPTIONS];
        uint8_t option_count = pocket_build_spell_cast_options(
            app, spell_index, options, POCKET_D20_MAX_SPELL_CAST_OPTIONS);
        if(!option_count) {
            pocket_set_status(app, "No casting resource");
            return;
        }
        if(option_count == 1U) {
            pocket_cast_spell(app, &options[0]);
            return;
        }
        pocket_enter_screen(app, PocketScreenSpellCast);
    }
}

static void pocket_handle_spell_cast(PocketD20App* app, const InputEvent* event) {
    PocketSpellCastOption options[POCKET_D20_MAX_SPELL_CAST_OPTIONS];
    uint8_t count = pocket_build_spell_cast_options(
        app, app->spell_attack_index, options, POCKET_D20_MAX_SPELL_CAST_OPTIONS);
    if(count > POCKET_D20_MAX_SPELL_CAST_OPTIONS) count = POCKET_D20_MAX_SPELL_CAST_OPTIONS;
    if(!count) {
        pocket_enter_screen(app, PocketScreenSpellAttacks);
        pocket_set_status(app, "No casting resource");
        return;
    }
    if(pocket_is_move_event(event) && event->key == InputKeyUp)
        pocket_menu_move(app, count, -1);
    else if(pocket_is_move_event(event) && event->key == InputKeyDown)
        pocket_menu_move(app, count, 1);
    else if(event->type == InputTypeShort && event->key == InputKeyOk && app->selection < count)
        pocket_cast_spell(app, &options[app->selection]);
}

static void pocket_handle_spell_result(PocketD20App* app, const InputEvent* event) {
    if(app->spell_cast_attack_roll_count > 4U && pocket_is_move_event(event)) {
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
    uint8_t count = pocket_build_spell_cast_options(
        app, app->spell_attack_index, options, POCKET_D20_MAX_SPELL_CAST_OPTIONS);
    if(count == 1U) {
        pocket_enter_screen(app, PocketScreenSpellCast);
        app->selection = 0U;
    } else if(count > 1U) {
        pocket_enter_screen(app, PocketScreenSpellCast);
    } else {
        pocket_enter_screen(app, PocketScreenSpellAttacks);
    }
}

static void pocket_handle_combat(PocketD20App* app, const InputEvent* event) {
    PocketCharacter* character = &app->data.character;
    if(pocket_is_move_event(event) && event->key == InputKeyUp)
        pocket_menu_move(app, 22U, -1);
    else if(pocket_is_move_event(event) && event->key == InputKeyDown)
        pocket_menu_move(app, 22U, 1);
    else if(
        pocket_is_move_event(event) &&
        (event->key == InputKeyLeft || event->key == InputKeyRight)) {
        int16_t delta = event->key == InputKeyRight ? 1 : -1;
        if(app->selection == 0U) {
            int16_t mode = app->roll_mode + delta;
            if(mode < PocketRollNormal) mode = PocketRollDisadvantage;
            if(mode > PocketRollDisadvantage) mode = PocketRollNormal;
            app->roll_mode = (PocketRollMode)mode;
            return;
        } else if(app->selection == 5U)
            character->hp_current = pocket_clamp_i16(character->hp_current + delta, 0, 999);
        else if(app->selection == 6U)
            character->hp_temporary = pocket_clamp_i16(character->hp_temporary + delta, 0, 999);
        else if(app->selection == 8U) {
            int16_t class_index = app->hit_die_class_index + delta;
            if(class_index < 0) class_index = character->class_count - 1U;
            if(class_index >= character->class_count) class_index = 0;
            app->hit_die_class_index = (uint8_t)class_index;
            return;
        } else if(app->selection == 12U)
            character->reaction_available = !character->reaction_available;
        else if(app->selection == 19U)
            character->death_successes = pocket_clamp_u8(character->death_successes + delta, 3U);
        else if(app->selection == 20U)
            character->death_failures = pocket_clamp_u8(character->death_failures + delta, 3U);
        else if(app->selection == 21U)
            character->exhaustion = pocket_clamp_u8(character->exhaustion + delta, 6U);
        else
            return;
        pocket_save(app, false);
    } else if(
        event->type == InputTypeLong && event->key == InputKeyOk &&
        (app->selection == 5U || app->selection == 6U ||
         (app->selection >= 19U && app->selection <= 21U))) {
        const char* header = app->selection == 5U  ? "Current HP" :
                             app->selection == 6U  ? "Temporary HP" :
                             app->selection == 19U ? "Death successes" :
                             app->selection == 20U ? "Death failures" :
                                                     "Exhaustion";
        int32_t value = app->selection == 5U  ? character->hp_current :
                        app->selection == 6U  ? character->hp_temporary :
                        app->selection == 19U ? character->death_successes :
                        app->selection == 20U ? character->death_failures :
                                                character->exhaustion;
        int32_t maximum = app->selection <= 6U ? 999 : app->selection <= 20U ? 3 : 6;
        pocket_begin_number(
            app, PocketNumberCombat, (uint8_t)app->selection, 0U, header, value, 0, maximum);
    } else if(event->type == InputTypeShort && event->key == InputKeyOk) {
        switch(app->selection) {
        case 0: {
            int16_t mode = app->roll_mode + 1;
            if(mode > PocketRollDisadvantage) mode = PocketRollNormal;
            app->roll_mode = (PocketRollMode)mode;
            break;
        }
        case 1:
            pocket_enter_screen(app, PocketScreenAttackList);
            break;
        case 2:
            pocket_enter_screen(app, PocketScreenSpellAttacks);
            break;
        case 3:
            pocket_enter_screen(app, PocketScreenAttackTemplates);
            break;
        case 4:
            pocket_request_launch(app, PocketPendingLaunchInitiative);
            break;
        case 5:
            character->hp_current = pocket_clamp_i16(character->hp_current + 1, 0, 999);
            pocket_save(app, false);
            break;
        case 6:
            character->hp_temporary = pocket_clamp_i16(character->hp_temporary + 1, 0, 999);
            pocket_save(app, false);
            break;
        case 7:
            if(character->hp_current < 1) {
                pocket_set_status(app, "Need at least 1 HP");
                break;
            }
            pocket_d20_short_rest(character);
            pocket_save(app, false);
            if(pocket_wizard_level(character) && !character->arcane_recovery_used &&
               pocket_begin_arcane_recovery(app))
                break;
            pocket_confirm_action(app, "Short rest applied");
            break;
        case 8:
            if(character->hp_current < 1) {
                pocket_set_status(app, "Need at least 1 HP");
            } else if(character->hp_current >= character->hp_max) {
                pocket_set_status(app, "HP already full");
            } else if(!character->classes[app->hit_die_class_index].hit_dice_current) {
                pocket_set_status(app, "No Hit Dice left");
            } else {
                uint8_t roll = 0U;
                int16_t healed =
                    pocket_d20_spend_class_hit_die(character, app->hit_die_class_index, &roll);
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
        case 9:
            if(character->hp_current < 1) {
                pocket_set_status(app, "Need at least 1 HP");
                break;
            }
            if(!pocket_load_spellbook(app)) {
                pocket_set_status(app, "Spellbook read failed");
                break;
            }
            pocket_d20_long_rest(character);
            if(!pocket_reset_all_spell_free_casts(app)) {
                pocket_set_status(app, "Spellbook update failed");
                break;
            }
            pocket_save(app, false);
            pocket_release_spellbook(app);
            pocket_confirm_action(app, "Long rest applied");
            break;
        case 10:
            pocket_begin_text(app, PocketEditConditions, "Conditions", character->conditions);
            break;
        case 11:
            pocket_begin_text(
                app, PocketEditConcentration, "Concentration", character->concentration);
            break;
        case 12:
            character->reaction_available = !character->reaction_available;
            pocket_save(app, false);
            break;
        case 13:
            pocket_begin_text(
                app, PocketEditTemporaryEffects, "Temporary effects", character->temporary_effects);
            break;
        case 14:
            pocket_begin_text(app, PocketEditResistances, "Resistances", character->resistances);
            break;
        case 15:
            pocket_begin_text(app, PocketEditImmunities, "Immunities", character->immunities);
            break;
        case 16:
            pocket_begin_text(
                app, PocketEditVulnerabilities, "Vulnerabilities", character->vulnerabilities);
            break;
        case 17:
            pocket_begin_text(app, PocketEditSenses, "Senses", character->senses);
            break;
        case 18:
            pocket_begin_text(
                app, PocketEditMovementModes, "Movement modes", character->movement_modes);
            break;
        case 19:
            character->death_successes = pocket_clamp_u8(character->death_successes + 1, 3U);
            pocket_save(app, false);
            break;
        case 20:
            character->death_failures = pocket_clamp_u8(character->death_failures + 1, 3U);
            pocket_save(app, false);
            break;
        case 21:
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
    if(app->roll_mode == PocketRollGuidance && app->dice_count == 1U && app->dice_sides == 20U) {
        app->dice_first = (uint8_t)pocket_d20_roll_dice(1U, 20U);
        app->dice_guidance = (uint8_t)pocket_d20_roll_dice(1U, 4U);
        app->dice_roll_values[0] = app->dice_first;
        app->dice_roll_values[1] = app->dice_guidance;
        app->dice_roll_value_count = 2U;
        app->dice_roll_sum = app->dice_first + app->dice_guidance;
        app->dice_result = (int16_t)app->dice_roll_sum + app->dice_modifier;
    } else if(
        (app->roll_mode == PocketRollAdvantage || app->roll_mode == PocketRollDisadvantage) &&
        app->dice_count == 1U && app->dice_sides == 20U) {
        app->dice_first = (uint8_t)pocket_d20_roll_dice(1U, 20U);
        app->dice_second = (uint8_t)pocket_d20_roll_dice(1U, 20U);
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
        app->dice_roll_sum = pocket_d20_roll_dice_values(
            app->dice_count, app->dice_sides, app->dice_roll_values, sizeof(app->dice_roll_values));
        if(app->dice_count == 1U) app->dice_first = app->dice_roll_values[0];
        app->dice_result = (int16_t)app->dice_roll_sum + app->dice_modifier;
    }
    pocket_enter_screen(app, PocketScreenDiceResult);
    pocket_start_dice_animation(app, app->dice_roll_value_count, app->dice_sides);
}

static void pocket_handle_dice(PocketD20App* app, const InputEvent* event) {
    if(pocket_is_move_event(event) && event->key == InputKeyUp)
        pocket_menu_move(app, 5U, -1);
    else if(pocket_is_move_event(event) && event->key == InputKeyDown)
        pocket_menu_move(app, 5U, 1);
    else if(
        pocket_is_move_event(event) &&
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
    } else if(event->type == InputTypeLong && event->key == InputKeyOk && app->selection <= 2U) {
        const char* header = app->selection == 0U ? "Dice count" :
                             app->selection == 1U ? "Die sides" :
                                                    "Roll modifier";
        int32_t value = app->selection == 0U ? app->dice_count :
                        app->selection == 1U ? app->dice_sides :
                                               app->dice_modifier;
        int32_t minimum = app->selection == 0U ? 1 : app->selection == 1U ? 2 : -99;
        int32_t maximum = app->selection == 0U ? 20 : app->selection == 1U ? 100 : 99;
        pocket_begin_number(
            app, PocketNumberDice, (uint8_t)app->selection, 0U, header, value, minimum, maximum);
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
    if(app->attack_item_index == 0xFFU) return;
    PocketItem* item = pocket_item_at(app, app->attack_item_index, NULL);
    if(!item) return;
    if((item->weapon_properties & PocketWeaponAmmunition) && item->ammo_current <= 0) {
        pocket_set_status(app, "No ammunition");
        return;
    }
    if(item->weapon_properties & PocketWeaponAmmunition) {
        --item->ammo_current;
        (void)pocket_save_items_if_changed(app);
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
    else if(
        pocket_is_move_event(event) &&
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
    PocketItem* item = pocket_item_at(app, app->attack_item_index, NULL);
    if(!item) return;
    if(app->attack_phase == 0U) {
        if(event->type == InputTypeShort && event->key == InputKeyOk) {
            app->damage_roll =
                pocket_d20_roll_damage(&app->data.character, item, app->attack_roll.critical);
            app->attack_phase = 1U;
            app->damage_roll_page = 0U;
            uint8_t count = app->damage_roll.weapon_roll_count + app->damage_roll.extra_roll_count;
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
            uint8_t count = app->damage_roll.weapon_roll_count + app->damage_roll.extra_roll_count;
            if(count)
                pocket_start_dice_animation(
                    app,
                    count,
                    item->use_versatile && item->versatile_die >= 2U ? item->versatile_die :
                                                                       item->damage_die);
        } else if(event->type == InputTypeShort && event->key == InputKeyUp) {
            app->attack_roll = pocket_d20_roll_attack(&app->data.character, item, app->roll_mode);
            pocket_start_dice_animation(app, app->attack_roll.second_die ? 2U : 1U, 20U);
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
                pocket_d20_roll_damage(&app->data.character, item, app->damage_roll.critical);
            app->damage_roll_page = 0U;
            roll_count = app->damage_roll.weapon_roll_count + app->damage_roll.extra_roll_count;
            if(roll_count)
                pocket_start_dice_animation(
                    app,
                    roll_count,
                    item->use_versatile && item->versatile_die >= 2U ? item->versatile_die :
                                                                       item->damage_die);
        }
    }
}
static bool pocket_input_callback(InputEvent* event, void* context) {
    PocketD20App* app = context;
    /* Text/number modules can be sizable. Once their callback has returned to the
     * main view, reclaim them before processing the next user action. */
    if(!app->input_module_active) {
        pocket_release_text_input(app);
        pocket_release_number_input(app);
    }
    if(event->type == InputTypeShort || event->type == InputTypeLong ||
       event->type == InputTypeRepeat)
        pocket_clear_action_ack(app);
    if(event->type == InputTypeLong && event->key == InputKeyBack) {
        pocket_handle_long_back(app);
        pocket_refresh(app);
        return true;
    }
    if(app->dice_animating) {
        if(event->type == InputTypeShort && event->key == InputKeyBack) {
            app->dice_animating = 0U;
            app->marquee_elapsed_ms = 0U;
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
    case PocketScreenLevelChoice:
        pocket_handle_level_choice(app, event);
        break;
    case PocketScreenAsiAbility:
        pocket_handle_asi_ability(app, event);
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
    case PocketScreenSpellAttacks:
        pocket_handle_spell_attacks(app, event);
        break;
    case PocketScreenSpellCast:
        pocket_handle_spell_cast(app, event);
        break;
    case PocketScreenSpellResult:
        pocket_handle_spell_result(app, event);
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





static bool pocket_reserve_core_ui(PocketD20App* app) {
    if(!app) return false;
    app->dispatcher = view_dispatcher_alloc();
    if(!app->dispatcher) return false;
    view_dispatcher_set_event_callback_context(app->dispatcher, app);
    view_dispatcher_set_navigation_event_callback(app->dispatcher, pocket_navigation_callback);
    view_dispatcher_set_custom_event_callback(app->dispatcher, pocket_custom_event_callback);
    view_dispatcher_set_tick_event_callback(
        app->dispatcher, pocket_tick_event_callback, POCKET_D20_UI_TICK_MS);

    app->autosave_timer = furi_timer_alloc(pocket_autosave_timer_callback, FuriTimerTypeOnce, app);
    if(!app->autosave_timer) return false;

    app->main_view = view_alloc();
    if(!app->main_view) return false;
    view_allocate_model(app->main_view, ViewModelTypeLockFree, sizeof(PocketD20App*));
    PocketD20App** model = view_get_model(app->main_view);
    if(!model) return false;
    *model = app;
    view_commit_model(app->main_view, false);
    view_set_context(app->main_view, app);
    view_set_draw_callback(app->main_view, pocket_draw_callback);
    view_set_input_callback(app->main_view, pocket_input_callback);
    return true;
}

static PocketD20App* pocket_app_alloc(void) {
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
    if(!pocket_reserve_core_ui(app)) goto fail;
    /* Relocate only legacy character ch*.txt files. Files are moved unchanged;
       the tolerant field-name loader interprets whatever recognized data exists.
       A failed relocation is treated conservatively as existing user data so a
       fresh New Hero cannot be created over a migration problem. */
    bool legacy_move_ok = pocket_d20_storage_move_legacy_profiles(app->storage);
    bool profiles_loaded = pocket_d20_profiles_load(app->storage, &app->profiles);
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
        if(pocket_d20_storage_load_profile(
               app->storage, candidate, &app->data, &candidate_recovered)) {
            loaded = true;
            recovered_backup = candidate_recovered;
            app->profiles.active_profile = candidate;
            recovered_next_profile = candidate != first_profile;
            break;
        }
        PocketProfileEntry next;
        if(!app->profiles.count ||
           !pocket_d20_profiles_next_after(app->storage, candidate, &next) ||
           next.id == candidate || next.id == first_profile)
            break;
        candidate = next.id;
    }

    app->active_profile_loaded = loaded ? 1U : 0U;
    bool character_ready = loaded;
    bool metadata_saved = true;
    if(loaded && recovered_backup)
        character_ready = pocket_d20_storage_restore_backup(
            app->storage, app->profiles.active_profile, &app->data);

    /* New Hero is created only when storage positively contains no primary
       character data. Shadow files never participate in this decision. */
    if(!loaded && !character_file_available) {
        pocket_d20_data_clear(&app->data);
        pocket_d20_data_set_defaults(&app->data);
        app->profiles.active_profile = 0U;
        app->active_profile_loaded = 1U;
        character_ready = pocket_d20_storage_save_profile(
            app->storage, app->profiles.active_profile, &app->data);
        pocket_d20_data_clear_spells(&app->data.character);
        pocket_d20_data_clear_items(&app->data.character);
        app->spellbook_loaded = 0U;
        app->items_loaded = 0U;
        if(!character_ready) {
            pocket_d20_storage_delete_profile(app->storage, app->profiles.active_profile);
            app->active_profile_loaded = 0U;
        }
    }
    bool spell_slots_initialized = false;
    if(app->active_profile_loaded) {
        spell_slots_initialized = pocket_d20_initialize_spell_slots_if_unset(&app->data.character);
        if(spell_slots_initialized)
            character_ready = character_ready && pocket_d20_storage_save_profile_updated(
                                                   app->storage,
                                                   app->profiles.active_profile,
                                                   &app->data);
    }
    if(app->active_profile_loaded) {
        bool active_included = pocket_profile_include_active(app);
        metadata_saved = active_included && pocket_d20_profiles_save(app->storage, &app->profiles);
    }
    app->saved_fingerprint = pocket_data_fingerprint(&app->data);
    if(!character_ready || !metadata_saved) {
        app->storage_read_only = 1U;
        app->storage_unsaved = app->active_profile_loaded ? 1U : 0U;
    }

    app->screen = PocketScreenHome;
    app->roll_mode = PocketRollNormal;
    app->dice_count = 1U;
    app->dice_sides = 20U;
    app->spell_filter_level = -1;
    app->spell_filter_class = UINT8_MAX;
    if(!legacy_move_ok && !loaded)
        pocket_set_status(app, "Legacy characters preserved");
    else if(!loaded && character_file_available)
        pocket_set_status(app, "Profile preserved - load failed");
    else if(!character_ready || !metadata_saved)
        pocket_set_status(app, "UNSAVED - retry SD");
    else if(recovered_backup)
        pocket_set_status(app, "Backup recovered");
    else if(recovered_next_profile)
        pocket_set_status(app, "Active character recovered");
    else if(spell_slots_initialized)
        pocket_set_status(app, "Spell slots allocated");
    else if(loaded)
        pocket_set_status(app, "Loaded");
    else if(profiles_loaded)
        pocket_set_status(app, "Fresh character");
    else
        pocket_set_status(app, "New character");

    app->input_events = furi_record_open(RECORD_INPUT_EVENTS);
    if(!app->input_events) goto fail;
    app->input_subscription =
        furi_pubsub_subscribe(app->input_events, pocket_input_events_callback, app);
    if(!app->input_subscription) goto fail;

    view_dispatcher_add_view(app->dispatcher, PocketViewMain, app->main_view);
    view_dispatcher_attach_to_gui(app->dispatcher, app->gui, ViewDispatcherTypeFullscreen);
    return app;

fail:
    pocket_quiesce_async(app);
    if(app->text_input) text_input_free(app->text_input);
    if(app->number_input) number_input_free(app->number_input);
    if(app->input_events) furi_record_close(RECORD_INPUT_EVENTS);
    if(app->autosave_timer) furi_timer_free(app->autosave_timer);
    if(app->main_view) view_free(app->main_view);
    if(app->dispatcher) view_dispatcher_free(app->dispatcher);
    pocket_catalog_release(app);
    pocket_d20_profiles_free(&app->profiles);
    pocket_d20_data_clear(&app->data);
    if(app->storage) furi_record_close(RECORD_STORAGE);
    if(app->gui) furi_record_close(RECORD_GUI);
    free(app);
    return NULL;
}

static void pocket_app_free(PocketD20App* app) {
    furi_assert(app);

    /* Quiesce asynchronous callbacks before any UI or app state is released. */
    pocket_quiesce_async(app);

    pocket_flush_save(app, false);
    pocket_catalog_release(app);

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
    pocket_d20_profiles_free(&app->profiles);
    pocket_d20_data_clear(&app->data);
    if(app->storage) furi_record_close(RECORD_STORAGE);
    if(app->gui) furi_record_close(RECORD_GUI);
    free(app);
}

int32_t dndolphins_app(void* context) {
    UNUSED(context);
    PocketD20App* app = pocket_app_alloc();
    if(!app) return -1;
    view_dispatcher_switch_to_view(app->dispatcher, PocketViewMain);
    view_dispatcher_run(app->dispatcher);

    PocketPendingLaunch pending_launch = app->pending_launch;
    char pending_args[sizeof(app->pending_launch_args)];
    pocket_copy(pending_args, sizeof(pending_args), app->pending_launch_args);
    pocket_app_free(app);

    if(pending_launch != PocketPendingLaunchNone) {
        const char* launch_path = pending_launch == PocketPendingLaunchJournal ?
                                      DNDJOURNAL_FAP_PATH :
                                  pending_launch == PocketPendingLaunchAdventure ?
                                      DNDADVENTURE_FAP_PATH :
                                  pending_launch == PocketPendingLaunchInitiative ?
                                      DNDINITIATIVE_FAP_PATH :
                                      DNDBESTIARY_FAP_PATH;
        const char* args = pending_args[0] ? pending_args : NULL;
        if(!dnd_handoff_launch(launch_path, args)) return -1;
    }
    return 0;
}
