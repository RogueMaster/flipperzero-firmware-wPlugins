#pragma GCC diagnostic ignored "-Wunused-function"

#if !defined(DND_BUILD_HUB) && !defined(DND_BUILD_COMBAT) && !defined(DND_BUILD_GRANTS)
#define DND_BUILD_HUB 1
#endif

#if defined(DND_BUILD_HUB)
#include "dnd_app_hub.h"
#elif defined(DND_BUILD_COMBAT)
#include "dnd_app_combat.h"
#elif defined(DND_BUILD_GRANTS)
#include "dnd_app_grants.h"
#endif

#include "dnd_charactersheet_api.h"
#include "dnd_journal_api.h"
#include "dnd_spell_damage_api.h"

#define TAG                               "DNDolphins"
#define DNDOLPHINS_MAX_GENERIC_ROLLS      20U
#define DNDOLPHINS_DICE_ANIMATION_FRAMES  8U
#define DNDOLPHINS_LONG_BACK_EVENT        0xD121U
#define DNDOLPHINS_AUTOSAVE_EVENT         0xD122U
#define DNDOLPHINS_DEFERRED_ACTION_EVENT  0xD123U
#define DNDOLPHINS_RELEASE_INPUT_EVENT    0xD124U
#define DNDOLPHINS_UI_TICK_MS             100U
#define DNDOLPHINS_MAX_CATALOG_ENTRIES    24U
#define DNDOLPHINS_CHARACTER_CATALOG_PAGE 8U
#define DNDOLPHINS_MAX_SPELL_ATTACK_ROLLS 12U

typedef enum {
    DndPendingLaunchNone,
    DndPendingLaunchBestiary,
    DndPendingLaunchJournal,
    DndPendingLaunchAdventure,
    DndPendingLaunchAdventureContinue,
    DndPendingLaunchInitiative,
    DndPendingLaunchInventory,
    DndPendingLaunchSpellbook,
    DndPendingLaunchSpellbookMagic,
    DndPendingLaunchCombat,
    DndPendingLaunchGrantsInitial,
    DndPendingLaunchGrantsLevel,
    DndPendingLaunchCharacterSheet,
    DndPendingLaunchBackup,
} DndPendingLaunch;

typedef enum {
    DndDeferredActionNone,
    DndDeferredActionGrantInitialTraits,
    DndDeferredActionApplyLevelGrants,
} DndDeferredAction;
#define DNDOLPHINS_SPELL_PAGE_ENTRIES  10U
#define DNDOLPHINS_MARQUEE_MS          350U
#define DNDOLPHINS_AUTOSAVE_MS         450U
#define DNDOLPHINS_COMBAT_VISIBLE_ROWS 5U
#define DNDOLPHINS_COMBAT_ROW_LEN      64U

typedef enum {
    DndViewMain,
    DndViewTextInput,
    DndViewNumberInput,
} DndViewId;

typedef enum {
    DndScreenHome,
    DndScreenProfiles,
    DndScreenProfileActions,
    DndScreenCharacter,
    DndScreenVitals,
    DndScreenAbilities,
    DndScreenSkills,
    DndScreenGrantReview,
    DndScreenGrantDiagnostics,
    DndScreenGrantEdit,
    DndScreenLevelReview,
    DndScreenLevelChoice,
    DndScreenAsiAbility,
    DndScreenMagic,
    DndScreenRecordList,
    DndScreenRecordDetail,
    DndScreenCatalog,
    DndScreenCombat,
    DndScreenSpellAttacks,
    DndScreenFavoriteSpells,
    DndScreenUtilitySpells,
    DndScreenRituals,
    DndScreenSpellCast,
    DndScreenSpellResult,
    DndScreenAttackTemplates,
    DndScreenAttackTemplateEdit,
    DndScreenDice,
    DndScreenDiceResult,
    DndScreenAttackList,
    DndScreenAmmoChoice,
    DndScreenAttackResult,
    DndScreenSettings,
    DndScreenAbout,
} DndScreen;

typedef enum {
    DndCombatSpellModeAttacks = 0U,
    DndCombatSpellModeFavorites,
    DndCombatSpellModeUtility,
    DndCombatSpellModeRituals,
} DndCombatSpellMode;

typedef enum {
    DndListClasses,
    DndListFeatures,
    DndListLanguages,
    DndListProficiencies,
} DndListKind;

typedef enum {
    DndCatalogClasses,
    DndCatalogSubclasses,
    DndCatalogSpecies,
    DndCatalogBackgrounds,
    DndCatalogAlignments,
    DndCatalogFeats,
    DndCatalogLanguages,
    DndCatalogProficiencies,
    DndCatalogSpells,
    DndCatalogSkills,
    DndCatalogSkillTools,
    DndCatalogSizes,
    DndCatalogGrantOptions,
    DndCatalogCount,
} DndCatalogKind;

typedef enum {
    DndGrantChoiceNone,
    DndGrantChoiceLanguage,
    DndGrantChoiceSpell,
    DndGrantChoiceFeat,
    DndGrantChoiceSkill,
    DndGrantChoiceSkillTool,
    DndGrantChoiceProficiency,
    DndGrantChoiceSize,
    DndGrantChoiceFeature,
} DndGrantChoiceKind;

enum {
    DndClassMaskArtificer = 1U << 0,
    DndClassMaskBarbarian = 1U << 1,
    DndClassMaskBard = 1U << 2,
    DndClassMaskCleric = 1U << 3,
    DndClassMaskDruid = 1U << 4,
    DndClassMaskFighter = 1U << 5,
    DndClassMaskMonk = 1U << 6,
    DndClassMaskPaladin = 1U << 7,
    DndClassMaskRanger = 1U << 8,
    DndClassMaskRogue = 1U << 9,
    DndClassMaskSorcerer = 1U << 10,
    DndClassMaskWarlock = 1U << 11,
    DndClassMaskWizard = 1U << 12,
};

typedef struct {
    const char* name;
    uint8_t level;
    uint16_t class_mask;
} DndBuiltinSpell;

typedef struct {
    const char* name;
    uint16_t class_mask;
} DndBuiltinSubclass;

typedef enum {
    DndItemCategoryOther,
    DndItemCategoryWeapon,
    DndItemCategoryArmor,
    DndItemCategoryGear,
    DndItemCategoryTool,
    DndItemCategoryMountVehicle,
    DndItemCategoryPotion,
    DndItemCategoryRing,
    DndItemCategoryRod,
    DndItemCategoryScroll,
    DndItemCategoryStaff,
    DndItemCategoryWand,
    DndItemCategoryWondrous,
} DndItemCategory;

typedef enum {
    DndItemFilterAll,
    DndItemFilterWeapons,
    DndItemFilterArmor,
    DndItemFilterAmmunition,
    DndItemFilterGear,
    DndItemFilterTools,
    DndItemFilterMagic,
    DndItemFilterCount,
} DndItemFilter;

typedef enum {
    DndEditNone,
    DndEditCharacterName,
    DndEditPlayerName,
    DndEditSpecies,
    DndEditBackground,
    DndEditAlignment,
    DndEditOriginFeat,
    DndEditSenses,
    DndEditConditions,
    DndEditConcentration,
    DndEditTemporaryEffects,
    DndEditResistances,
    DndEditImmunities,
    DndEditVulnerabilities,
    DndEditMovementModes,
    DndEditClassName,
    DndEditSubclass,
    DndEditGrantStableId,
    DndEditGrantSource,
    DndEditGrantOption,
    DndEditGrantPrerequisites,
    DndEditGrantValue,
    DndEditAttackName,
    DndEditAttackMastery,
    DndEditAttackDamageType,
    DndEditAttackRiderType,
    DndEditFeatureName,
    DndEditFeatureDetail,
    DndEditLanguageName,
    DndEditProficiencyName,
} DndEditTarget;

typedef enum {
    DndNumberNone,
    DndNumberCharacter,
    DndNumberVitals,
    DndNumberAbility,
    DndNumberSkill,
    DndNumberMagic,
    DndNumberRecord,
    DndNumberDice,
    DndNumberCombat,
} DndNumberContext;

typedef struct {
    DndPlugin spell_plugin;
    uint16_t combat_spell_count;
    uint16_t combat_spell_capacity;
    uint16_t combat_spell_start;
    uint8_t combat_spell_mode;
    uint16_t* combat_spell_indices;
    uint16_t combat_weapon_count;
    uint16_t combat_weapon_capacity;
    uint16_t combat_weapon_start;
    uint16_t* combat_weapon_indices;
    uint8_t arcane_recovery_active;
    uint8_t arcane_recovery_budget;
    uint8_t arcane_recovery_spent;
    uint8_t arcane_recovery_restored[6];
    uint8_t hit_die_class_index;
    uint16_t attack_item_index;
    uint16_t ammo_choice_indices[8];
    uint8_t ammo_choice_count;
    char ammo_choice_group[DND_SHORT_LEN];
    uint8_t attack_phase;
    DndAttackRoll attack_roll;
    DndDamageRoll damage_roll;
    uint16_t spell_attack_index;
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
    uint8_t spell_cast_attack_natural[DNDOLPHINS_MAX_SPELL_ATTACK_ROLLS];
    int16_t spell_cast_attack_damage[DNDOLPHINS_MAX_SPELL_ATTACK_ROLLS];
    uint8_t spell_cast_natural;
    int16_t spell_cast_attack_total;
    int16_t spell_cast_primary_total;
    int16_t spell_cast_secondary_total;
    int16_t spell_cast_flat_bonus;
    int16_t spell_cast_secondary_flat_bonus;
    int16_t spell_cast_damage_total;
    uint8_t damage_roll_page;
} DndCombatRuntime;

typedef struct {
    DndCatalogKind kind;
    DndEditTarget target;
    uint16_t count;
    uint16_t capacity;
    uint16_t total;
    uint16_t scan_count;
    uint16_t page_start;
    uint16_t page_size;
    uint16_t return_selection;
    void* storage;
    char (*entries)[DND_CATALOG_NAME_LEN];
    uint8_t* levels;
    uint16_t* class_masks;
    uint8_t* has_metadata;
    uint8_t show_all;
    uint8_t has_more;
} DndCatalogRuntime;

typedef struct {
    uint32_t* spellbook_page_offsets;
    uint32_t* items_page_offsets;
    uint32_t* features_page_offsets;
    void* character_collection_page;
    DndDolphinsSpellClassCounts spell_class_counts;
    DndListKind character_collection_kind;
    uint16_t spellbook_cache_start;
    uint16_t items_cache_start;
    uint16_t features_cache_start;
    uint16_t language_cache_start;
    uint16_t proficiency_cache_start;
    uint8_t spellbook_offset_valid_pages;
    uint8_t items_offset_valid_pages;
    uint8_t features_offset_valid_pages;
    uint8_t character_collection_kind_valid;
    uint8_t language_page_count;
    uint8_t proficiency_page_count;
    uint8_t spell_class_counts_valid;
} DndCollectionCacheRuntime;

typedef struct {
    DndRollMode roll_mode;
    uint8_t dice_count;
    uint8_t dice_sides;
    int16_t dice_modifier;
    int16_t dice_result;
    uint8_t dice_first;
    uint8_t dice_second;
    uint8_t dice_guidance;
    uint8_t quick_roll_active;
    uint8_t quick_roll_is_save;
    uint8_t ability_roll_target;
    char quick_roll_label[32];
    uint8_t dice_roll_values[DNDOLPHINS_MAX_GENERIC_ROLLS];
    uint8_t dice_roll_value_count;
    uint16_t dice_roll_sum;
    uint8_t dice_animating;
    uint8_t dice_anim_frame;
    uint8_t dice_anim_sides;
    uint8_t dice_anim_count;
} DndRollRuntime;

typedef struct {
    uint8_t grant_choice_active;
    uint8_t grant_choice_index;
    DndGrantChoiceKind grant_choice_kind;
    uint8_t maximum_level;
    uint8_t include_background;
    uint8_t batches;
    uint8_t initial_languages_done;
    uint8_t scan_complete;
    uint8_t dependency_scan_needed;
    uint8_t dependency_rescan_needed;
    uint32_t metadata_offset;
    uint32_t dependency_metadata_offset;
} DndGrantReviewRuntime;

typedef struct {
    Gui* gui;
    Storage* storage;
    ViewDispatcher* dispatcher;
    View* main_view;
    TextInput* text_input;
    NumberInput* number_input;
    FuriTimer* autosave_timer;

    DndSaveData data;
    DndProfileState* profile_browser;
    DndProfileEntry active_entry;
    uint32_t active_profile;
    uint8_t active_entry_valid;
    DndSettings settings;
    DndCombatRuntime* combat;
    uint8_t catalog_all_available;
    uint32_t saved_fingerprint;
    uint32_t saved_spellbook_fingerprint;
    uint32_t saved_items_fingerprint;
    uint32_t saved_features_fingerprint;
    uint8_t spellbook_loaded;
    uint8_t items_loaded;
    uint8_t features_loaded;
    uint16_t spellbook_total;
    uint16_t items_total;
    uint16_t features_total;
    uint16_t language_total;
    uint16_t proficiency_total;
    bool collection_replace;
    bool character_collections_changed;
    DndCollectionCacheRuntime* collection_cache;
    DndScreen screen;
    DndScreen return_screen;
    DndScreen record_list_return_screen;
    DndListKind list_kind;
    uint16_t selection;
    uint16_t scroll;
    uint16_t home_return_selection;
    uint16_t record_index;
    DndCatalogRuntime* catalog;
    DndGrantReviewRuntime* grant_review;
    uint8_t edit_modifier_mode;
    uint32_t profile_action_id;
    uint8_t active_profile_loaded;
    uint8_t storage_read_only;
    uint8_t storage_unsaved;
    uint8_t autosave_pending;
    uint16_t storage_failure_count;
    DndPendingLaunch pending_launch;
    uint8_t return_to_parent;
    DndDeferredAction deferred_action;
    uint8_t deferred_action_wait_ticks;
    DndEditTarget edit_target;
    char* edit_buffer;
    DndNumberContext number_context;
    uint8_t number_index;
    uint8_t number_aux;
    uint8_t input_module_active;
    FuriPubSub* input_events;
    FuriPubSubSubscription* input_subscription;

    DndRollRuntime* roll;
    uint16_t marquee_elapsed_ms;

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
} DndDolphinsApp;

static bool dndolphins_profile_browser_alloc(DndDolphinsApp* app) {
    if(!app) return false;
    if(app->profile_browser) return true;
    app->profile_browser = calloc(1U, sizeof(DndProfileState));
    return app->profile_browser != NULL;
}

static void dndolphins_profile_browser_release(DndDolphinsApp* app) {
    if(!app || !app->profile_browser) return;
    dnd_storage_profiles_free(app->profile_browser);
    free(app->profile_browser);
    app->profile_browser = NULL;
}

static void dndolphins_active_entry_cache_from_browser(DndDolphinsApp* app) {
    if(!app || !app->profile_browser) return;
    if(app->profile_browser->active_entry_valid &&
       app->profile_browser->active_entry.id == app->active_profile) {
        app->active_entry = app->profile_browser->active_entry;
        app->active_entry_valid = 1U;
    }
}

static bool dndolphins_combat_runtime_alloc(DndDolphinsApp* app) {
#if defined(DND_BUILD_COMBAT)
    if(!app) return false;
    if(app->combat) return true;
    app->combat = calloc(1U, sizeof(DndCombatRuntime));
    return app->combat != NULL;
#else
    UNUSED(app);
    return true;
#endif
}

static void dndolphins_combat_runtime_free(DndDolphinsApp* app) {
    if(!app || !app->combat) return;
#if defined(DND_BUILD_COMBAT)
    dnd_plugin_close(&app->combat->spell_plugin);
#endif
    free(app->combat->combat_spell_indices);
    app->combat->combat_spell_indices = NULL;
    free(app->combat->combat_weapon_indices);
    app->combat->combat_weapon_indices = NULL;
    free(app->combat);
    app->combat = NULL;
}

static bool dndolphins_roll_runtime_alloc(DndDolphinsApp* app) {
    if(!app) return false;
    if(app->roll) return true;
    app->roll = calloc(1U, sizeof(DndRollRuntime));
    if(!app->roll) return false;
    app->roll->roll_mode = DndRollNormal;
    app->roll->dice_count = 1U;
    app->roll->dice_sides = 20U;
    return true;
}

static void dndolphins_roll_runtime_free(DndDolphinsApp* app) {
    if(!app || !app->roll) return;
    free(app->roll);
    app->roll = NULL;
}

static bool dndolphins_grant_review_runtime_alloc(DndDolphinsApp* app) {
    if(!app) return false;
    if(app->grant_review) return true;
    app->grant_review = calloc(1U, sizeof(DndGrantReviewRuntime));
    return app->grant_review != NULL;
}

static void dndolphins_grant_review_runtime_free(DndDolphinsApp* app) {
    if(!app || !app->grant_review) return;
    free(app->grant_review);
    app->grant_review = NULL;
}

static bool dndolphins_collection_cache_runtime_alloc(DndDolphinsApp* app) {
    if(!app) return false;
    if(app->collection_cache) return true;
    app->collection_cache = calloc(1U, sizeof(DndCollectionCacheRuntime));
    return app->collection_cache != NULL;
}

static void dndolphins_collection_cache_runtime_free(DndDolphinsApp* app) {
    if(!app || !app->collection_cache) return;
    free(app->collection_cache->spellbook_page_offsets);
    free(app->collection_cache->items_page_offsets);
    free(app->collection_cache->features_page_offsets);
    free(app->collection_cache->character_collection_page);
    free(app->collection_cache);
    app->collection_cache = NULL;
}

static void dndolphins_collection_cache_runtime_release_if_idle(DndDolphinsApp* app) {
    if(!app || !app->collection_cache) return;
    if(app->spellbook_loaded || app->items_loaded || app->features_loaded ||
       app->collection_cache->character_collection_page ||
       app->collection_cache->spell_class_counts_valid)
        return;
    dndolphins_collection_cache_runtime_free(app);
}

static bool dndolphins_begin_next_level_choice(DndDolphinsApp* app);
static void dndolphins_request_launch(DndDolphinsApp* app, DndPendingLaunch launch);
static void dndolphins_handle_level_review(DndDolphinsApp* app, const InputEvent* event);
static void dndolphins_handle_level_choice(DndDolphinsApp* app, const InputEvent* event);
static void dndolphins_handle_asi_ability(DndDolphinsApp* app, const InputEvent* event);

static bool dndolphins_record_list_prepare_sidecar(DndDolphinsApp* app);
static bool dndolphins_language_cache_ensure(DndDolphinsApp* app, uint16_t logical);
static bool dndolphins_proficiency_cache_ensure(DndDolphinsApp* app, uint16_t logical);
static bool dndolphins_refresh_combat_spell_index(DndDolphinsApp* app);
static bool dndolphins_refresh_favorite_spell_index(DndDolphinsApp* app);
static bool dndolphins_refresh_utility_spell_index(DndDolphinsApp* app);
static bool dndolphins_refresh_ritual_spell_index(DndDolphinsApp* app);
static bool dndolphins_refresh_combat_weapon_index(DndDolphinsApp* app);
static void dndolphins_release_text_input(DndDolphinsApp* app);
static void dndolphins_release_number_input(DndDolphinsApp* app);
static void
    dndolphins_prepare_combat_spell_rows(DndDolphinsApp* app, DndCombatSpellMode spell_mode);
static DndScreen dndolphins_combat_spell_return_screen(const DndDolphinsApp* app);
static void dndolphins_prepare_combat_weapon_rows(DndDolphinsApp* app);

static void dndolphins_text_done(void* context);
static void dndolphins_roll_generic(DndDolphinsApp* app);
static void dndolphins_handle_long_back(DndDolphinsApp* app);
static void dndolphins_quiesce_async(DndDolphinsApp* app);
static bool dndolphins_flush_save(DndDolphinsApp* app, bool report);
static void dndolphins_run_deferred_action(DndDolphinsApp* app);
static void dndolphins_collection_save_failed(DndDolphinsApp* app);
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
    DndolphinsHomeSettings,
    DndolphinsHomeCount,
} DndolphinsHomeIndex;

typedef enum {
    DndolphinsCombatAttackMode = 0,
    DndolphinsCombatWeaponAttacks,
    DndolphinsCombatSpellAttacks,
    DndolphinsCombatFavoriteSpells,
    DndolphinsCombatSpellcastingStats,
    DndolphinsCombatUtilitySpells,
    DndolphinsCombatRituals,
    DndolphinsCombatAttackTemplates,
    DndolphinsCombatJumpToInitiative,
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
    [DndolphinsHomeSettings] = "Settings",
};

static const char* const dndolphins_home_retry_save = "Retry Save";

static bool
    dndolphins_home_index_from_return_focus(const char* args, DndolphinsHomeIndex* home_index) {
    if(!args || !home_index) return false;
    if(strcmp(args, DND_PROFILE_RETURN_FOCUS_INVENTORY) == 0)
        *home_index = DndolphinsHomeInventory;
    else if(strcmp(args, DND_PROFILE_RETURN_FOCUS_SPELLBOOK) == 0)
        *home_index = DndolphinsHomeMagicSpells;
    else if(strcmp(args, DND_PROFILE_RETURN_FOCUS_ADVENTURE) == 0)
        *home_index = DndolphinsHomeAdventure;
    else if(strcmp(args, DND_PROFILE_RETURN_FOCUS_JOURNAL) == 0)
        *home_index = DndolphinsHomeJournal;
    else if(strcmp(args, DND_PROFILE_RETURN_FOCUS_INITIATIVE) == 0)
        *home_index = DndolphinsHomeInitiative;
    else if(strcmp(args, DND_PROFILE_RETURN_FOCUS_BESTIARY) == 0)
        *home_index = DndolphinsHomeBestiary;
    else if(strcmp(args, DND_PROFILE_RETURN_FOCUS_COMBAT) == 0)
        *home_index = DndolphinsHomeCombat;
    else if(strcmp(args, DND_PROFILE_RETURN_FOCUS_CHARACTER) == 0)
        *home_index = DndolphinsHomeCharacter;
    else
        return false;
    return true;
}

static void dndolphins_set_home_focus(DndDolphinsApp* app, DndolphinsHomeIndex home_index) {
    if(!app || home_index >= DndolphinsHomeCount) return;
    app->home_return_selection = (uint16_t)home_index;
    if(app->screen == DndScreenHome) {
        app->selection = (uint16_t)home_index;
        app->scroll = app->selection >= 5U ? (uint16_t)(app->selection - 4U) : 0U;
    }
}

static void dndolphins_apply_return_focus(DndDolphinsApp* app, const char* args) {
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
    "Archive",
    "Delete",
    "Verify Save",
    "Backup & Restore",
};

static const uint8_t dndolphins_die_choices[] = {4U, 6U, 8U, 10U, 12U, 20U, 100U};
static const uint8_t dndolphins_damage_die_choices[] = {4U, 6U, 8U, 10U, 12U};
static const char* const dndolphins_roll_mode_names[] =
    {"Normal", "Advantage", "Disadvantage", "Guidance"};
static const char* const dndolphins_recharge_names[] =
    {"Manual", "Turn", "Encounter", "Dawn", "Short/Long", "Long"};
static const char* const dndolphins_attack_template_type_names[] =
    {"Unarmed", "Spell Attack", "Saving Throw", "Custom", "Grapple", "Shove"};
static const char* const dndolphins_size_names[] = {"Tiny", "Small", "Medium", "Large"};
static const char* const dndolphins_spellcasting_mode_names[] =
    {"None", "Full", "Half", "Third", "Pact", "Spell Points", "Custom"};
static const char* const dndolphins_resource_formula_names[] = {"Manual", "PB", "Ability"};

typedef enum {
    DndSpellSourceUnknown = 0U,
    DndSpellSourceCore,
    DndSpellSourceXanathar,
    DndSpellSourceForgottenRealms,
    DndSpellSourceRavenloft,
    DndSpellSourceOther,
    DndSpellSourceCount,
} DndSpellSource;

/* Group the standard skills by governing ability without changing their save indexes. */
static const uint8_t dndolphins_skill_display_order[DND_SKILL_COUNT] = {
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

/* Minimal SRD picker fallbacks used only when the selected catalog asset is
   missing. Normal browsing streams the asset file and does not merge these. */
static const char* const dndolphins_catalog_classes[] = {
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

static const DndBuiltinSubclass dndolphins_catalog_subclasses[] = {
    {"Path of the Berserker", DndClassMaskBarbarian},
    {"College of Lore", DndClassMaskBard},
    {"Life Domain", DndClassMaskCleric},
    {"Circle of the Land", DndClassMaskDruid},
    {"Champion", DndClassMaskFighter},
    {"Warrior of the Open Hand", DndClassMaskMonk},
    {"Oath of Devotion", DndClassMaskPaladin},
    {"Hunter", DndClassMaskRanger},
    {"Thief", DndClassMaskRogue},
    {"Draconic Sorcery", DndClassMaskSorcerer},
    {"Fiend Patron", DndClassMaskWarlock},
    {"Evoker", DndClassMaskWizard},
};

static const char* const dndolphins_catalog_backgrounds[] =
    {"Acolyte", "Criminal", "Sage", "Soldier"};

static const char* const dndolphins_catalog_species[] = {
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
    "Ability Score Improvement",
    "Alert",
    "Archery",
    "Boon of Combat Prowess",
    "Boon of Dimensional Travel",
    "Boon of Fate",
    "Boon of Irresistible Offense",
    "Boon of Spell Recall",
    "Boon of the Night Spirit",
    "Boon of Truesight",
    "Defense",
    "Grappler",
    "Great Weapon Fighting",
    "Magic Initiate (Cleric)",
    "Magic Initiate (Druid)",
    "Magic Initiate (Wizard)",
    "Savage Attacker",
    "Skilled",
    "Two-Weapon Fighting"};

static const char* const dndolphins_bundled_catalog_paths[DndCatalogCount] = {
    [DndCatalogClasses] = APP_ASSETS_PATH("catalogs/classes.txt"),
    [DndCatalogSubclasses] = APP_ASSETS_PATH("catalogs/subclasses.txt"),
    [DndCatalogSpecies] = APP_ASSETS_PATH("catalogs/species.txt"),
    [DndCatalogBackgrounds] = APP_ASSETS_PATH("catalogs/backgrounds.txt"),
    [DndCatalogAlignments] = APP_ASSETS_PATH("catalogs/alignments.txt"),
    [DndCatalogFeats] = APP_ASSETS_PATH("catalogs/feats.txt"),
    [DndCatalogLanguages] = APP_ASSETS_PATH("catalogs/languages.txt"),
    [DndCatalogProficiencies] = APP_ASSETS_PATH("catalogs/proficiencies.txt"),
    [DndCatalogSpells] = APP_ASSETS_PATH("catalogs/spells.txt"),
    [DndCatalogSkills] = "",
    /* Skill/tool grant choices intentionally reuse the proficiency catalog. */
    [DndCatalogSkillTools] = APP_ASSETS_PATH("catalogs/proficiencies.txt"),
    [DndCatalogSizes] = "",
    [DndCatalogGrantOptions] = "",
};

static const char* const dndolphins_bundled_catalog_all_paths[DndCatalogCount] = {
    [DndCatalogClasses] = APP_ASSETS_PATH("catalogs/classes_All.txt"),
    [DndCatalogSubclasses] = APP_ASSETS_PATH("catalogs/subclasses_All.txt"),
    [DndCatalogSpecies] = APP_ASSETS_PATH("catalogs/species_All.txt"),
    [DndCatalogBackgrounds] = APP_ASSETS_PATH("catalogs/backgrounds_All.txt"),
    [DndCatalogAlignments] = "",
    [DndCatalogFeats] = APP_ASSETS_PATH("catalogs/feats_All.txt"),
    [DndCatalogLanguages] = "",
    [DndCatalogProficiencies] = APP_ASSETS_PATH("catalogs/proficiencies_All.txt"),
    [DndCatalogSpells] = APP_ASSETS_PATH("catalogs/spells_All.txt"),
    [DndCatalogSkills] = "",
    /* Skill/tool grant choices intentionally reuse the proficiency catalog. */
    [DndCatalogSkillTools] = APP_ASSETS_PATH("catalogs/proficiencies_All.txt"),
    [DndCatalogSizes] = "",
    [DndCatalogGrantOptions] = "",
};

static const char* const dndolphins_bundled_metadata_path =
    APP_ASSETS_PATH("metadata/options.txt");
static const char* const dndolphins_bundled_metadata_all_path =
    APP_ASSETS_PATH("metadata/options_All.txt");
static const char* const dndolphins_bundled_catalog_abilities_path =
    APP_ASSETS_PATH("catalogs/abilities.txt");
static const char* const dndolphins_bundled_catalog_abilities_all_path =
    APP_ASSETS_PATH("catalogs/abilities_All.txt");
static const char* const dndolphins_progression_spell_metadata_path =
    APP_ASSETS_PATH("metadata/progression_spells.txt");

static bool dndolphins_catalog_all_files_available(Storage* storage) {
    if(!storage) return false;
    for(uint8_t kind = 0U; kind < DndCatalogCount; ++kind) {
        const char* path = dndolphins_bundled_catalog_all_paths[kind];
        if(path[0] && !storage_file_exists(storage, path)) return false;
    }
    return storage_file_exists(storage, dndolphins_bundled_catalog_abilities_all_path) &&
           storage_file_exists(storage, dndolphins_bundled_metadata_all_path);
}

static const char*
    dndolphins_catalog_path_for_mode(const DndDolphinsApp* app, DndCatalogKind kind) {
    if(!app || kind >= DndCatalogCount) return "";
    const char* all_path = dndolphins_bundled_catalog_all_paths[kind];
    if(app->settings.catalog_all && app->catalog_all_available && all_path[0]) return all_path;
    return dndolphins_bundled_catalog_paths[kind];
}

static const char* dndolphins_abilities_path_for_mode(const DndDolphinsApp* app) {
    if(app && app->settings.catalog_all && app->catalog_all_available)
        return dndolphins_bundled_catalog_abilities_all_path;
    return dndolphins_bundled_catalog_abilities_path;
}

static const char* dndolphins_active_metadata_path(const DndDolphinsApp* app) {
    if(app && app->settings.catalog_all && app->catalog_all_available)
        return dndolphins_bundled_metadata_all_path;
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
        if(value > maximum / 10U || (value == maximum / 10U && digit > maximum % 10U))
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

static bool dndolphins_catalog_runtime_alloc(DndDolphinsApp* app) {
    if(!app) return false;
    if(app->catalog) return true;
    app->catalog = calloc(1U, sizeof(DndCatalogRuntime));
    return app->catalog != NULL;
}

static void dndolphins_catalog_release(DndDolphinsApp* app) {
    if(!app || !app->catalog) return;
    free(app->catalog->storage);
    app->catalog->storage = NULL;
    app->catalog->entries = NULL;
    app->catalog->levels = NULL;
    app->catalog->class_masks = NULL;
    app->catalog->has_metadata = NULL;
    app->catalog->count = 0U;
    app->catalog->capacity = 0U;
}

static void dndolphins_catalog_runtime_release(DndDolphinsApp* app) {
    if(!app || !app->catalog) return;
    dndolphins_catalog_release(app);
    free(app->catalog);
    app->catalog = NULL;
}

static uint16_t dndolphins_catalog_page_limit(const DndDolphinsApp* app) {
    if(app && app->catalog && app->catalog->page_size) return app->catalog->page_size;
    /* Item/Spell catalogs live in their standalone FAPs. DNDolphins only opens
       the remaining character/feature catalogs, which use the normal page. */
    return DNDOLPHINS_MAX_CATALOG_ENTRIES;
}

static bool dndolphins_catalog_ensure_capacity(DndDolphinsApp* app, uint16_t needed) {
    if(!dndolphins_catalog_runtime_alloc(app)) return false;
    if(needed <= app->catalog->capacity) return true;
    const uint16_t page_limit = dndolphins_catalog_page_limit(app);
    if(needed > page_limit || app->catalog->storage) return false;
    const size_t capacity = page_limit;
    size_t bytes = capacity * sizeof(*app->catalog->entries) +
                   capacity * sizeof(*app->catalog->levels) +
                   capacity * sizeof(*app->catalog->class_masks) +
                   capacity * sizeof(*app->catalog->has_metadata);
    uint8_t* cursor = malloc(bytes);
    if(!cursor) return false;
    app->catalog->storage = cursor;
    app->catalog->entries = (void*)cursor;
    cursor += capacity * sizeof(*app->catalog->entries);
    app->catalog->levels = cursor;
    cursor += capacity * sizeof(*app->catalog->levels);
    app->catalog->class_masks = (void*)cursor;
    cursor += capacity * sizeof(*app->catalog->class_masks);
    app->catalog->has_metadata = cursor;
    app->catalog->capacity = (uint16_t)capacity;
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

static void dndolphins_clear_status(DndDolphinsApp* app) {
    app->status[0] = '\0';
}

static void dndolphins_set_status(DndDolphinsApp* app, const char* status) {
    dndolphins_copy(app->status, sizeof(app->status), status);
}

static bool dndolphins_status_is_one_shot_success(const DndDolphinsApp* app) {
    if(!app || !app->status[0]) return false;
    return !strcmp(app->status, "Saved") || !strcmp(app->status, "Already saved") ||
           !strcmp(app->status, "Catalog choice saved") || !strcmp(app->status, "Granted");
}

static void dndolphins_clear_action_ack(DndDolphinsApp* app) {
    app->action_ack_active = 0U;
}

static void dndolphins_confirm_action(DndDolphinsApp* app, const char* status) {
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

static void dndolphins_refresh(DndDolphinsApp* app) {
    /* Hydrate only from the event/update path, never from a Canvas callback. */
    bool ok = true;
#if !defined(DND_BUILD_COMBAT)
    if(app->screen == DndScreenRecordList)
        ok = dndolphins_record_list_prepare_sidecar(app);
    else if(app->screen == DndScreenRecordDetail && app->list_kind == DndListLanguages)
        ok = dndolphins_language_cache_ensure(app, app->record_index);
    else if(app->screen == DndScreenRecordDetail && app->list_kind == DndListProficiencies)
        ok = dndolphins_proficiency_cache_ensure(app, app->record_index);
#endif
    if(!ok) dndolphins_set_status(app, "Page read failed");
    (void)view_get_model(app->main_view);
    view_commit_model(app->main_view, true);
}

static void dndolphins_autosave_timer_callback(void* context) {
    DndDolphinsApp* app = context;
    view_dispatcher_send_custom_event(app->dispatcher, DNDOLPHINS_AUTOSAVE_EVENT);
}

static void dndolphins_input_events_callback(const void* value, void* context) {
    DndDolphinsApp* app = context;
    const InputEvent* event = value;
    if(app->input_module_active && event && event->key == InputKeyBack &&
       event->type == InputTypeLong)
        view_dispatcher_send_custom_event(app->dispatcher, DNDOLPHINS_LONG_BACK_EVENT);
}

static bool dndolphins_input_hook_acquire(DndDolphinsApp* app) {
    if(!app) return false;
    if(app->input_subscription && app->input_events) return true;
    app->input_events = furi_record_open(RECORD_INPUT_EVENTS);
    if(!app->input_events) return false;
    app->input_subscription =
        furi_pubsub_subscribe(app->input_events, dndolphins_input_events_callback, app);
    if(!app->input_subscription) {
        furi_record_close(RECORD_INPUT_EVENTS);
        app->input_events = NULL;
        return false;
    }
    return true;
}

static void dndolphins_input_hook_release(DndDolphinsApp* app) {
    if(!app) return;
    if(app->input_subscription && app->input_events)
        furi_pubsub_unsubscribe(app->input_events, app->input_subscription);
    app->input_subscription = NULL;
    if(app->input_events) furi_record_close(RECORD_INPUT_EVENTS);
    app->input_events = NULL;
}

static void dndolphins_quiesce_async(DndDolphinsApp* app) {
    if(!app) return;
    dndolphins_input_hook_release(app);
    if(app->autosave_timer) furi_timer_stop(app->autosave_timer);
}

static void dndolphins_start_dice_animation(DndDolphinsApp* app, uint8_t count, uint8_t sides) {
    if(!dndolphins_roll_runtime_alloc(app)) return;
    if(app->settings.skip_dice_loading) {
        app->roll->dice_animating = 0U;
        app->roll->dice_anim_frame = 0U;
        app->marquee_elapsed_ms = 0U;
        return;
    }
    app->roll->dice_animating = 1U;
    app->roll->dice_anim_frame = 0U;
    app->roll->dice_anim_count = count ? count : 1U;
    app->roll->dice_anim_sides = sides >= 2U ? sides : 20U;
    app->marquee_elapsed_ms = 0U;
}

static void dndolphins_tick_event_callback(void* context) {
    DndDolphinsApp* app = context;
    bool refresh = false;

    if(app->deferred_action != DndDeferredActionNone && app->deferred_action_wait_ticks) {
        --app->deferred_action_wait_ticks;
        if(!app->deferred_action_wait_ticks)
            view_dispatcher_send_custom_event(app->dispatcher, DNDOLPHINS_DEFERRED_ACTION_EVENT);
    }

    if(app->roll && app->roll->dice_animating) {
        ++app->roll->dice_anim_frame;
        if(app->roll->dice_anim_frame >= DNDOLPHINS_DICE_ANIMATION_FRAMES) {
            app->roll->dice_animating = 0U;
            app->marquee_elapsed_ms = 0U;
        }
        refresh = true;
    } else {
        app->marquee_elapsed_ms += DNDOLPHINS_UI_TICK_MS;
        if(app->marquee_elapsed_ms >= DNDOLPHINS_MARQUEE_MS) {
            app->marquee_elapsed_ms -= DNDOLPHINS_MARQUEE_MS;
            ++dndolphins_marquee_offset;
            refresh = true;
        }
    }

    if(refresh) dndolphins_refresh(app);
}

static bool dndolphins_custom_event_callback(void* context, uint32_t event) {
    DndDolphinsApp* app = context;
    if(event == DNDOLPHINS_AUTOSAVE_EVENT) {
        dndolphins_flush_save(app, false);
        dndolphins_refresh(app);
        return true;
    }
    if(event == DNDOLPHINS_LONG_BACK_EVENT) {
        app->input_module_active = 0U;
        app->edit_target = DndEditNone;
        app->number_context = DndNumberNone;
        dndolphins_input_hook_release(app);
        view_dispatcher_switch_to_view(app->dispatcher, DndViewMain);
        dndolphins_release_text_input(app);
        dndolphins_release_number_input(app);
        dndolphins_handle_long_back(app);
        dndolphins_refresh(app);
        return true;
    }
    if(event == DNDOLPHINS_RELEASE_INPUT_EVENT) {
        dndolphins_release_text_input(app);
        dndolphins_release_number_input(app);
        return true;
    }
    if(event == DNDOLPHINS_DEFERRED_ACTION_EVENT) {
        dndolphins_run_deferred_action(app);
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

static uint32_t dndolphins_data_fingerprint(const DndSaveData* data) {
    uint32_t hash = 2166136261UL;
    const DndCharacter* character = &data->character;
    const uint8_t* character_bytes = (const uint8_t*)character;

    /* Spells and items are independent sidecar collections. Their counts, capacities,
       pointers, and record contents must never make the core character look dirty just
       because a collection was hydrated or released. */
    hash = dndolphins_hash_bytes(hash, character_bytes, offsetof(DndCharacter, spell_count));
    /* Spells, features, items, and applied/pending grants are independent lazy
       collections. Hydrating or releasing one must not dirty the core profile. */
    const size_t stable_middle = offsetof(DndCharacter, saving_throw_misc);
    const size_t grants_begin = offsetof(DndCharacter, grant_count);
    hash =
        dndolphins_hash_bytes(hash, character_bytes + stable_middle, grants_begin - stable_middle);
    const size_t stable_tail = offsetof(DndCharacter, attack_template_count);
    hash = dndolphins_hash_bytes(
        hash, character_bytes + stable_tail, sizeof(DndCharacter) - stable_tail);
    return hash;
}

static uint32_t dndolphins_spellbook_fingerprint(const DndCharacter* character) {
    uint32_t hash = 2166136261UL;
    hash = dndolphins_hash_bytes(hash, &character->spell_count, sizeof(character->spell_count));
    if(character->spell_count && character->spells && character->spell_known &&
       character->spell_always_prepared && character->spell_free_casts_current &&
       character->spell_free_casts_max) {
        hash = dndolphins_hash_bytes(
            hash, character->spells, (size_t)character->spell_count * sizeof(DndSpell));
        hash = dndolphins_hash_bytes(hash, character->spell_known, character->spell_count);
        hash =
            dndolphins_hash_bytes(hash, character->spell_always_prepared, character->spell_count);
        hash = dndolphins_hash_bytes(
            hash, character->spell_free_casts_current, character->spell_count);
        hash =
            dndolphins_hash_bytes(hash, character->spell_free_casts_max, character->spell_count);
    }
    return hash;
}

static uint32_t dndolphins_items_fingerprint(const DndCharacter* character) {
    uint32_t hash = 2166136261UL;
    hash = dndolphins_hash_bytes(hash, &character->item_count, sizeof(character->item_count));
    if(character->item_count && character->items)
        hash = dndolphins_hash_bytes(
            hash, character->items, (size_t)character->item_count * sizeof(DndItem));
    return hash;
}

static uint32_t dndolphins_features_fingerprint(const DndCharacter* character) {
    uint32_t hash = 2166136261UL;
    hash =
        dndolphins_hash_bytes(hash, &character->feature_count, sizeof(character->feature_count));
    if(character->feature_count && character->features)
        hash = dndolphins_hash_bytes(
            hash, character->features, (size_t)character->feature_count * sizeof(DndFeature));
    return hash;
}

static bool dndolphins_page_offsets_ensure(uint32_t** offsets, size_t count) {
    if(!offsets || !count) return false;
    if(*offsets) return true;
    *offsets = calloc(count, sizeof(uint32_t));
    return *offsets != NULL;
}

static void dndolphins_page_offsets_release(uint32_t** offsets, uint8_t* valid_pages) {
    if(!offsets) return;
    free(*offsets);
    *offsets = NULL;
    if(valid_pages) *valid_pages = 0U;
}

static bool dndolphins_load_features_page(DndDolphinsApp* app, uint16_t start) {
    if(!dndolphins_collection_cache_runtime_alloc(app)) return false;
    if(!app->active_profile_loaded) return false;
    if(!dndolphins_page_offsets_ensure(
           &app->collection_cache->features_page_offsets, DND_PROGRESS_PAGE_COUNT))
        return false;
    uint16_t total = app->features_total;
    if(!dndolphins_progression_store_features_load_window_indexed(
           app->storage,
           app->active_profile,
           start,
           &app->data.character,
           &total,
           app->collection_cache->features_page_offsets,
           &app->collection_cache->features_offset_valid_pages)) {
        dndolphins_set_status(app, "Features read failed");
        return false;
    }
    app->features_total = total;
    app->collection_cache->features_cache_start = start;
    app->features_loaded = 1U;
    app->saved_features_fingerprint = dndolphins_features_fingerprint(&app->data.character);
    return true;
}

static bool dndolphins_load_features(DndDolphinsApp* app) {
    if(app->features_loaded) return true;
    return dndolphins_load_features_page(app, 0U);
}

static bool dndolphins_save_features_if_changed(DndDolphinsApp* app) {
    if(!app->features_loaded) return true;
    uint32_t fingerprint = dndolphins_features_fingerprint(&app->data.character);
    if(fingerprint == app->saved_features_fingerprint) return true;
    if(app->storage_read_only) return false;
    if(!dndolphins_progression_store_features_save_window(
           app->storage,
           app->active_profile,
           app->collection_cache->features_cache_start,
           &app->data.character)) {
        dndolphins_collection_save_failed(app);
        return false;
    }
    app->saved_features_fingerprint = fingerprint;
    app->collection_cache->features_offset_valid_pages = 0U;
    return true;
}

static bool dndolphins_feature_cache_ensure(DndDolphinsApp* app, uint16_t logical_index) {
    if(!app->features_loaded && !dndolphins_load_features(app)) return false;
    if(logical_index >= app->features_total) return false;
    if(logical_index >= app->collection_cache->features_cache_start &&
       logical_index <
           app->collection_cache->features_cache_start + app->data.character.feature_count)
        return true;
    if(!dndolphins_save_features_if_changed(app)) return false;
    dnd_data_reserve_features_exact(&app->data.character, 0U);
    app->data.character.feature_count = 0U;
    app->features_loaded = 0U;
    uint16_t start = (logical_index / DND_PROGRESS_CACHE_SIZE) * DND_PROGRESS_CACHE_SIZE;
    return dndolphins_load_features_page(app, start);
}

static uint8_t dndolphins_feature_local_index(const DndDolphinsApp* app, uint16_t logical_index) {
    return (uint8_t)(logical_index - app->collection_cache->features_cache_start);
}

static DndFeature*
    dndolphins_feature_at(DndDolphinsApp* app, uint16_t logical_index, uint8_t* local_out) {
    if(!dndolphins_feature_cache_ensure(app, logical_index)) return NULL;
    uint8_t local = dndolphins_feature_local_index(app, logical_index);
    if(local >= app->data.character.feature_count) return NULL;
    if(local_out) *local_out = local;
    return &app->data.character.features[local];
}

static DndFeature*
    dndolphins_feature_at_cached(DndDolphinsApp* app, uint16_t logical_index, uint8_t* local_out) {
    if(!app->features_loaded || logical_index < app->collection_cache->features_cache_start)
        return NULL;
    uint16_t local = logical_index - app->collection_cache->features_cache_start;
    if(local >= app->data.character.feature_count) return NULL;
    if(local_out) *local_out = local;
    return &app->data.character.features[local];
}

static bool dndolphins_release_features(DndDolphinsApp* app) {
    if(!app || !app->collection_cache) return !app || !app->features_loaded;
    if(!app->features_loaded) {
        dndolphins_page_offsets_release(
            &app->collection_cache->features_page_offsets,
            &app->collection_cache->features_offset_valid_pages);
        dndolphins_collection_cache_runtime_release_if_idle(app);
        return true;
    }
    bool saved = dndolphins_save_features_if_changed(app);
    if(saved) {
        dnd_data_reserve_features_exact(&app->data.character, 0U);
        app->data.character.feature_count = 0U;
        app->features_loaded = 0U;
        app->collection_cache->features_cache_start = 0U;
        dndolphins_page_offsets_release(
            &app->collection_cache->features_page_offsets,
            &app->collection_cache->features_offset_valid_pages);
        dndolphins_collection_cache_runtime_release_if_idle(app);
    }
    return saved;
}

static bool dndolphins_load_spellbook_page(DndDolphinsApp* app, uint16_t start) {
    if(!dndolphins_collection_cache_runtime_alloc(app)) return false;
    if(!app->active_profile_loaded) return false;
    if(!dndolphins_page_offsets_ensure(
           &app->collection_cache->spellbook_page_offsets, DND_STORAGE_COLLECTION_PAGE_COUNT))
        return false;
    uint16_t total = app->spellbook_total;
    if(!dnd_storage_load_spellbook_window_indexed(
           app->storage,
           app->active_profile,
           start,
           &app->data.character,
           &total,
           app->collection_cache->spellbook_page_offsets,
           &app->collection_cache->spellbook_offset_valid_pages)) {
        dndolphins_set_status(app, "Spellbook read failed");
        return false;
    }
    app->spellbook_total = total;
    app->collection_cache->spellbook_cache_start = start;
    app->spellbook_loaded = 1U;
    app->saved_spellbook_fingerprint = dndolphins_spellbook_fingerprint(&app->data.character);
    return true;
}

static bool dndolphins_load_items_page(DndDolphinsApp* app, uint16_t start) {
    if(!dndolphins_collection_cache_runtime_alloc(app)) return false;
    if(!app->active_profile_loaded) return false;
    if(!dndolphins_page_offsets_ensure(
           &app->collection_cache->items_page_offsets, DND_STORAGE_COLLECTION_PAGE_COUNT))
        return false;
    uint16_t total = app->items_total;
    if(!dnd_storage_load_items_window_indexed(
           app->storage,
           app->active_profile,
           start,
           &app->data.character,
           &total,
           app->collection_cache->items_page_offsets,
           &app->collection_cache->items_offset_valid_pages)) {
        dndolphins_set_status(app, "Items read failed");
        return false;
    }
    app->items_total = total;
    app->collection_cache->items_cache_start = start;
    app->items_loaded = 1U;
    app->saved_items_fingerprint = dndolphins_items_fingerprint(&app->data.character);
    return true;
}

static bool dndolphins_load_spellbook(DndDolphinsApp* app) {
    if(app->spellbook_loaded) return true;
    return dndolphins_load_spellbook_page(app, 0U);
}

static bool dndolphins_load_items(DndDolphinsApp* app) {
    if(app->items_loaded) return true;
    return dndolphins_load_items_page(app, 0U);
}

static void dndolphins_collection_save_failed(DndDolphinsApp* app) {
    /* A collection write failure is retryable and must not poison core profile
       storage state. The resident collection fingerprint remains dirty, which is
       sufficient for the next autosave/close to retry only that collection. */
    if(app->storage_failure_count < UINT16_MAX) ++app->storage_failure_count;
    dndolphins_set_status(app, "UNSAVED - retry SD");
}

static bool dndolphins_save_spellbook_if_changed(DndDolphinsApp* app) {
    if(!app->spellbook_loaded) return true;
    uint32_t fingerprint = dndolphins_spellbook_fingerprint(&app->data.character);
    if(fingerprint == app->saved_spellbook_fingerprint) return true;
    if(app->storage_read_only) return false;
    if(!dnd_storage_save_spellbook_window(
           app->storage,
           app->active_profile,
           app->collection_cache->spellbook_cache_start,
           &app->data.character)) {
        dndolphins_collection_save_failed(app);
        return false;
    }
    app->saved_spellbook_fingerprint = fingerprint;
    app->collection_cache->spell_class_counts_valid = 0U;
    app->collection_cache->spellbook_offset_valid_pages = 0U;
    return true;
}

static bool dndolphins_save_items_if_changed(DndDolphinsApp* app) {
    if(!app->items_loaded) return true;
    uint32_t fingerprint = dndolphins_items_fingerprint(&app->data.character);
    if(fingerprint == app->saved_items_fingerprint) return true;
    if(app->storage_read_only) return false;
    if(!dnd_storage_save_items_window(
           app->storage,
           app->active_profile,
           app->collection_cache->items_cache_start,
           &app->data.character)) {
        dndolphins_collection_save_failed(app);
        return false;
    }
    app->saved_items_fingerprint = fingerprint;
    app->collection_cache->items_offset_valid_pages = 0U;
    return true;
}

static bool dndolphins_spell_cache_ensure(DndDolphinsApp* app, uint16_t logical_index) {
    if(!app->spellbook_loaded && !dndolphins_load_spellbook(app)) return false;
    if(logical_index >= app->spellbook_total) return false;
    if(logical_index >= app->collection_cache->spellbook_cache_start &&
       logical_index <
           app->collection_cache->spellbook_cache_start + app->data.character.spell_count)
        return true;
    if(!dndolphins_save_spellbook_if_changed(app)) return false;
    dnd_data_clear_spells(&app->data.character);
    app->spellbook_loaded = 0U;
    uint16_t start =
        (logical_index / DND_STORAGE_COLLECTION_CACHE_SIZE) * DND_STORAGE_COLLECTION_CACHE_SIZE;
    return dndolphins_load_spellbook_page(app, start);
}

static bool dndolphins_item_cache_ensure(DndDolphinsApp* app, uint16_t logical_index) {
    if(!app->items_loaded && !dndolphins_load_items(app)) return false;
    if(logical_index >= app->items_total) return false;
    if(logical_index >= app->collection_cache->items_cache_start &&
       logical_index < app->collection_cache->items_cache_start + app->data.character.item_count)
        return true;
    if(!dndolphins_save_items_if_changed(app)) return false;
    dnd_data_clear_items(&app->data.character);
    app->items_loaded = 0U;
    uint16_t start =
        (logical_index / DND_STORAGE_COLLECTION_CACHE_SIZE) * DND_STORAGE_COLLECTION_CACHE_SIZE;
    return dndolphins_load_items_page(app, start);
}

static uint8_t dndolphins_spell_local_index(const DndDolphinsApp* app, uint16_t logical_index) {
    return (uint8_t)(logical_index - app->collection_cache->spellbook_cache_start);
}

static uint8_t dndolphins_item_local_index(const DndDolphinsApp* app, uint16_t logical_index) {
    return (uint8_t)(logical_index - app->collection_cache->items_cache_start);
}

static DndSpell*
    dndolphins_spell_at(DndDolphinsApp* app, uint16_t logical_index, uint8_t* local_out) {
    if(!dndolphins_spell_cache_ensure(app, logical_index)) return NULL;
    uint8_t local = dndolphins_spell_local_index(app, logical_index);
    if(local >= app->data.character.spell_count) return NULL;
    if(local_out) *local_out = local;
    return &app->data.character.spells[local];
}

static DndItem*
    dndolphins_item_at(DndDolphinsApp* app, uint16_t logical_index, uint8_t* local_out) {
    if(!dndolphins_item_cache_ensure(app, logical_index)) return NULL;
    uint8_t local = dndolphins_item_local_index(app, logical_index);
    if(local >= app->data.character.item_count) return NULL;
    if(local_out) *local_out = local;
    return &app->data.character.items[local];
}

static DndSpell*
    dndolphins_spell_cached_at(DndDolphinsApp* app, uint16_t logical_index, uint8_t* local_out) {
    if(!app || !app->spellbook_loaded ||
       logical_index < app->collection_cache->spellbook_cache_start ||
       logical_index >=
           app->collection_cache->spellbook_cache_start + app->data.character.spell_count)
        return NULL;
    uint8_t local = dndolphins_spell_local_index(app, logical_index);
    if(local >= app->data.character.spell_count) return NULL;
    if(local_out) *local_out = local;
    return &app->data.character.spells[local];
}

static DndItem*
    dndolphins_item_cached_at(DndDolphinsApp* app, uint16_t logical_index, uint8_t* local_out) {
    if(!app || !app->items_loaded || logical_index < app->collection_cache->items_cache_start ||
       logical_index >= app->collection_cache->items_cache_start + app->data.character.item_count)
        return NULL;
    uint8_t local = dndolphins_item_local_index(app, logical_index);
    if(local >= app->data.character.item_count) return NULL;
    if(local_out) *local_out = local;
    return &app->data.character.items[local];
}

static void dndolphins_record_list_scroll_sidecar(DndDolphinsApp* app, uint16_t total) {
    if(app->selection == 0U || total == 0U) {
        app->scroll = 0U;
        return;
    }

    uint16_t logical = app->selection - 1U;
    uint16_t page_start =
        (logical / DND_STORAGE_COLLECTION_CACHE_SIZE) * DND_STORAGE_COLLECTION_CACHE_SIZE;
    uint16_t first = (uint16_t)page_start + 1U;
    uint16_t page_records = total - page_start;
    if(page_records > DND_STORAGE_COLLECTION_CACHE_SIZE)
        page_records = DND_STORAGE_COLLECTION_CACHE_SIZE;
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

static void dndolphins_character_collection_release(DndDolphinsApp* app) {
    if(!app || !app->collection_cache) return;
    free(app->collection_cache->character_collection_page);
    app->collection_cache->character_collection_page = NULL;
    app->collection_cache->character_collection_kind_valid = false;
    app->collection_cache->language_page_count = 0U;
    app->collection_cache->proficiency_page_count = 0U;
    app->collection_cache->language_cache_start = 0U;
    app->collection_cache->proficiency_cache_start = 0U;
    dndolphins_collection_cache_runtime_release_if_idle(app);
}

static bool dndolphins_character_collection_ensure(DndDolphinsApp* app, DndListKind kind) {
    if(!app || (kind != DndListLanguages && kind != DndListProficiencies)) return false;
    if(app->collection_cache && app->collection_cache->character_collection_page &&
       app->collection_cache->character_collection_kind_valid &&
       app->collection_cache->character_collection_kind == kind)
        return true;
    dndolphins_character_collection_release(app);
    /* Releasing the final page also releases its cache descriptor. */
    if(!dndolphins_collection_cache_runtime_alloc(app)) return false;
    const size_t row_size = kind == DndListLanguages ? sizeof(char[DND_CATALOG_NAME_LEN]) :
                                                       sizeof(DndCharacterProficiency);
    app->collection_cache->character_collection_page =
        calloc(DND_CHARACTER_COLLECTION_WINDOW, row_size);
    if(!app->collection_cache->character_collection_page) {
        dndolphins_collection_cache_runtime_release_if_idle(app);
        return false;
    }
    app->collection_cache->character_collection_kind = kind;
    app->collection_cache->character_collection_kind_valid = true;
    return true;
}

static bool dndolphins_load_language_page(DndDolphinsApp* app, uint16_t start) {
    if(!dndolphins_character_collection_ensure(app, DndListLanguages)) return false;
    uint16_t total = app->language_total;
    uint8_t count = 0U;
    char(*languages)[DND_CATALOG_NAME_LEN] = app->collection_cache->character_collection_page;
    if(!dnd_character_languages_load_window(
           app->storage, app->active_profile, start, languages, &count, &total))
        return false;
    app->collection_cache->language_cache_start = start;
    app->collection_cache->language_page_count = count;
    app->collection_cache->proficiency_page_count = 0U;
    app->language_total = total;
    return true;
}

static bool dndolphins_load_proficiency_page(DndDolphinsApp* app, uint16_t start) {
    if(!dndolphins_character_collection_ensure(app, DndListProficiencies)) return false;
    uint16_t total = app->proficiency_total;
    uint8_t count = 0U;
    DndCharacterProficiency* proficiencies = app->collection_cache->character_collection_page;
    if(!dnd_character_proficiencies_load_window(
           app->storage, app->active_profile, start, proficiencies, &count, &total))
        return false;
    app->collection_cache->proficiency_cache_start = start;
    app->collection_cache->proficiency_page_count = count;
    app->collection_cache->language_page_count = 0U;
    app->proficiency_total = total;
    return true;
}

static bool dndolphins_language_cache_ensure(DndDolphinsApp* app, uint16_t logical) {
    if(!app->collection_cache)
        return dndolphins_load_language_page(
            app,
            (uint16_t)((logical / DND_CHARACTER_COLLECTION_WINDOW) *
                       DND_CHARACTER_COLLECTION_WINDOW));
    if(logical < app->collection_cache->language_cache_start ||
       logical >= (uint16_t)app->collection_cache->language_cache_start +
                      app->collection_cache->language_page_count) {
        uint16_t start = (uint16_t)((logical / DND_CHARACTER_COLLECTION_WINDOW) *
                                    DND_CHARACTER_COLLECTION_WINDOW);
        return dndolphins_load_language_page(app, start);
    }
    return true;
}

static bool dndolphins_proficiency_cache_ensure(DndDolphinsApp* app, uint16_t logical) {
    if(!app->collection_cache)
        return dndolphins_load_proficiency_page(
            app,
            (uint16_t)((logical / DND_CHARACTER_COLLECTION_WINDOW) *
                       DND_CHARACTER_COLLECTION_WINDOW));
    if(logical < app->collection_cache->proficiency_cache_start ||
       logical >= (uint16_t)app->collection_cache->proficiency_cache_start +
                      app->collection_cache->proficiency_page_count) {
        uint16_t start = (uint16_t)((logical / DND_CHARACTER_COLLECTION_WINDOW) *
                                    DND_CHARACTER_COLLECTION_WINDOW);
        return dndolphins_load_proficiency_page(app, start);
    }
    return true;
}

static const char* dndolphins_language_at_cached(DndDolphinsApp* app, uint16_t logical) {
    if(!app->collection_cache) return NULL;
    if(logical < app->collection_cache->language_cache_start) return NULL;
    uint16_t local = logical - app->collection_cache->language_cache_start;
    if(!app->collection_cache->character_collection_page ||
       !app->collection_cache->character_collection_kind_valid ||
       app->collection_cache->character_collection_kind != DndListLanguages)
        return NULL;
    char(*languages)[DND_CATALOG_NAME_LEN] = app->collection_cache->character_collection_page;
    return local < app->collection_cache->language_page_count ? languages[local] : NULL;
}

static const DndCharacterProficiency*
    dndolphins_proficiency_at_cached(DndDolphinsApp* app, uint16_t logical) {
    if(!app->collection_cache) return NULL;
    if(logical < app->collection_cache->proficiency_cache_start) return NULL;
    uint16_t local = logical - app->collection_cache->proficiency_cache_start;
    if(!app->collection_cache->character_collection_page ||
       !app->collection_cache->character_collection_kind_valid ||
       app->collection_cache->character_collection_kind != DndListProficiencies)
        return NULL;
    DndCharacterProficiency* proficiencies = app->collection_cache->character_collection_page;
    return local < app->collection_cache->proficiency_page_count ? &proficiencies[local] : NULL;
}

static bool dndolphins_record_list_prepare_sidecar(DndDolphinsApp* app) {
    uint16_t total = 0U;
    if(app->list_kind == DndListFeatures)
        total = app->features_total;
    else if(app->list_kind == DndListLanguages)
        total = app->language_total;
    else if(app->list_kind == DndListProficiencies)
        total = app->proficiency_total;
    else
        return true;

    if(total) {
        uint16_t logical = app->selection ? app->selection - 1U : 0U;
        if(logical >= total) logical = total - 1U;
        bool ok =
            app->list_kind == DndListFeatures  ? dndolphins_feature_cache_ensure(app, logical) :
            app->list_kind == DndListLanguages ? dndolphins_language_cache_ensure(app, logical) :
                                                 dndolphins_proficiency_cache_ensure(app, logical);
        if(!ok) return false;
    }
    dndolphins_record_list_scroll_sidecar(app, total);
    return true;
}

static void dndolphins_record_list_focus(DndDolphinsApp* app, uint16_t logical_index) {
    uint16_t total = app->list_kind == DndListFeatures      ? app->features_total :
                     app->list_kind == DndListLanguages     ? app->language_total :
                     app->list_kind == DndListProficiencies ? app->proficiency_total :
                                                              0U;
    if(!total) {
        app->selection = 0U;
        app->scroll = 0U;
        return;
    }
    if(logical_index >= total) logical_index = total - 1U;
    app->selection = (uint16_t)logical_index + 1U;
    if(!dndolphins_record_list_prepare_sidecar(app))
        dndolphins_set_status(app, "Collection read failed");
}

static bool dndolphins_spell_class_counts_cached(DndDolphinsApp* app) {
    if(!dndolphins_collection_cache_runtime_alloc(app)) return false;
    if(!app->collection_cache->spell_class_counts_valid) {
        uint16_t total = 0U;
        if(!dndolphins_spells_class_counts(
               app->storage,
               app->active_profile,
               &app->collection_cache->spell_class_counts,
               &total))
            return false;
        app->spellbook_total = total;
        app->collection_cache->spell_class_counts_valid = 1U;
    }
    return true;
}

static uint8_t dndolphins_class_prepared_count_cached(DndDolphinsApp* app, uint8_t class_index) {
    return app->collection_cache && app->collection_cache->spell_class_counts_valid &&
                   class_index < DND_MAX_CLASSES ?
               app->collection_cache->spell_class_counts.prepared[class_index] :
               0U;
}

static uint8_t dndolphins_class_known_count_cached(DndDolphinsApp* app, uint8_t class_index) {
    return app->collection_cache && app->collection_cache->spell_class_counts_valid &&
                   class_index < DND_MAX_CLASSES ?
               app->collection_cache->spell_class_counts.known[class_index] :
               0U;
}

static bool dndolphins_release_spellbook(DndDolphinsApp* app) {
    if(!app || !app->collection_cache) return !app || !app->spellbook_loaded;
    if(!app->spellbook_loaded) {
        dndolphins_page_offsets_release(
            &app->collection_cache->spellbook_page_offsets,
            &app->collection_cache->spellbook_offset_valid_pages);
        dndolphins_collection_cache_runtime_release_if_idle(app);
        return true;
    }
    bool saved = dndolphins_save_spellbook_if_changed(app);
    if(saved) {
        dnd_data_clear_spells(&app->data.character);
        app->spellbook_loaded = 0U;
        app->collection_cache->spellbook_cache_start = 0U;
        dndolphins_page_offsets_release(
            &app->collection_cache->spellbook_page_offsets,
            &app->collection_cache->spellbook_offset_valid_pages);
        dndolphins_collection_cache_runtime_release_if_idle(app);
    }
    return saved;
}

static bool dndolphins_release_items(DndDolphinsApp* app) {
    if(!app || !app->collection_cache) return !app || !app->items_loaded;
    if(!app->items_loaded) {
        dndolphins_page_offsets_release(
            &app->collection_cache->items_page_offsets,
            &app->collection_cache->items_offset_valid_pages);
        dndolphins_collection_cache_runtime_release_if_idle(app);
        return true;
    }
    bool saved = dndolphins_save_items_if_changed(app);
    if(saved) {
        dnd_data_clear_items(&app->data.character);
        app->items_loaded = 0U;
        app->collection_cache->items_cache_start = 0U;
        dndolphins_page_offsets_release(
            &app->collection_cache->items_page_offsets,
            &app->collection_cache->items_offset_valid_pages);
        dndolphins_collection_cache_runtime_release_if_idle(app);
    }
    return saved;
}

static bool dndolphins_save_now(DndDolphinsApp* app, bool report) {
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
    bool main_changed = app->storage_unsaved || app->character_collections_changed ||
                        fingerprint != app->saved_fingerprint;
    bool spellbook_changed = app->spellbook_loaded &&
                             spellbook_fingerprint != app->saved_spellbook_fingerprint;
    bool items_changed = app->items_loaded && items_fingerprint != app->saved_items_fingerprint;
    bool features_changed = app->features_loaded &&
                            features_fingerprint != app->saved_features_fingerprint;
    if(!main_changed && !spellbook_changed && !items_changed && !features_changed) {
        if(report) dndolphins_set_status(app, "Already saved");
        return true;
    }

    bool result = true;
    bool main_write_failed = false;
    if(main_changed) {
        bool active_found = app->active_entry_valid && app->active_entry.id == app->active_profile;
        result =
            active_found ?
                dnd_storage_save_profile_known_updated(
                    app->storage, &app->active_entry, &app->data) :
                dnd_storage_save_profile_updated(app->storage, app->active_profile, &app->data);
        main_write_failed = !result;
        if(result) {
            app->active_entry.id = app->active_profile;
            app->active_entry.level = dnd_rules_core_total_level(&app->data.character);
            dndolphins_copy(
                app->active_entry.name, sizeof(app->active_entry.name), app->data.character.name);
            app->active_entry_valid = 1U;
            if(app->profile_browser) {
                for(uint8_t i = 0U; i < app->profile_browser->cache_count; ++i) {
                    if(app->profile_browser->entries[i].id != app->active_profile) continue;
                    app->profile_browser->entries[i].level =
                        dnd_rules_core_total_level(&app->data.character);
                    dndolphins_copy(
                        app->profile_browser->entries[i].name,
                        sizeof(app->profile_browser->entries[i].name),
                        app->data.character.name);
                    break;
                }
            }
            app->character_collections_changed = false;
            app->saved_fingerprint = fingerprint;
            app->storage_unsaved = 0U;
        }
    }
    if(result && spellbook_changed) {
        result = dnd_storage_save_spellbook_window(
            app->storage,
            app->active_profile,
            app->collection_cache->spellbook_cache_start,
            &app->data.character);
        if(result) app->saved_spellbook_fingerprint = spellbook_fingerprint;
    }
    if(result && items_changed) {
        result = dnd_storage_save_items_window(
            app->storage,
            app->active_profile,
            app->collection_cache->items_cache_start,
            &app->data.character);
        if(result) app->saved_items_fingerprint = items_fingerprint;
    }
    if(result && features_changed) {
        result = dndolphins_progression_store_features_save_window(
            app->storage,
            app->active_profile,
            app->collection_cache->features_cache_start,
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
    if(report || !result)
        dndolphins_set_status(app, result ? "Saved" : "UNSAVED - SD unavailable");
    return result;
}

static bool dndolphins_flush_save(DndDolphinsApp* app, bool report) {
    if(app->autosave_timer) {
        furi_timer_stop(app->autosave_timer);
        furi_timer_free(app->autosave_timer);
        app->autosave_timer = NULL;
    }
    app->autosave_pending = 0U;
    return dndolphins_save_now(app, report);
}

static bool dndolphins_save(DndDolphinsApp* app, bool report) {
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
    bool items_changed = app->items_loaded && dndolphins_items_fingerprint(&app->data.character) !=
                                                  app->saved_items_fingerprint;
    bool features_changed = app->features_loaded &&
                            dndolphins_features_fingerprint(&app->data.character) !=
                                app->saved_features_fingerprint;
    if(!app->storage_unsaved && !app->character_collections_changed &&
       fingerprint == app->saved_fingerprint && !spellbook_changed && !items_changed &&
       !features_changed) {
        if(app->autosave_timer) {
            furi_timer_stop(app->autosave_timer);
            furi_timer_free(app->autosave_timer);
            app->autosave_timer = NULL;
        }
        app->autosave_pending = 0U;
        return true;
    }
    if(!app->autosave_timer) {
        app->autosave_timer =
            furi_timer_alloc(dndolphins_autosave_timer_callback, FuriTimerTypeOnce, app);
        if(!app->autosave_timer) return dndolphins_save_now(app, false);
    }
    app->autosave_pending = 1U;
    furi_timer_stop(app->autosave_timer);
    if(furi_timer_start(app->autosave_timer, furi_ms_to_ticks(DNDOLPHINS_AUTOSAVE_MS)) !=
       FuriStatusOk) {
        app->autosave_pending = 0U;
        return dndolphins_save_now(app, false);
    }
    return true;
}

static uint16_t dndolphins_profile_count(const DndDolphinsApp* app) {
    return app && app->profile_browser ? app->profile_browser->count : 0U;
}

static const DndProfileEntry*
    dndolphins_profile_entry_at(DndDolphinsApp* app, uint16_t list_index) {
    if(!app || !app->profile_browser) return NULL;
    return dnd_storage_profiles_entry_at(app->storage, app->profile_browser, list_index);
}

static const DndProfileEntry*
    dndolphins_profile_entry_cached_at(const DndDolphinsApp* app, uint16_t list_index) {
    if(!app || !app->profile_browser || list_index >= app->profile_browser->count ||
       !app->profile_browser->cache_count || list_index < app->profile_browser->cache_start ||
       list_index >=
           (uint16_t)(app->profile_browser->cache_start + app->profile_browser->cache_count))
        return NULL;
    return &app->profile_browser->entries[list_index - app->profile_browser->cache_start];
}

static uint32_t dndolphins_profile_id_at(DndDolphinsApp* app, uint16_t list_index) {
    const DndProfileEntry* entry = dndolphins_profile_entry_at(app, list_index);
    return entry ? entry->id : UINT32_MAX;
}

static bool dndolphins_profile_exists(DndDolphinsApp* app, uint32_t profile) {
    return dnd_storage_profiles_find(app->storage, profile, NULL);
}

static bool dndolphins_profile_include_active(DndDolphinsApp* app) {
    if(app->active_entry_valid && app->active_entry.id == app->active_profile) {
        if(app->profile_browser) {
            app->profile_browser->active_entry = app->active_entry;
            app->profile_browser->active_entry_valid = 1U;
        }
        return true;
    }
    DndProfileEntry entry;
    if(!dnd_storage_profiles_find(app->storage, app->active_profile, &entry)) return false;
    app->active_entry = entry;
    app->active_entry_valid = 1U;
    if(app->profile_browser) {
        app->profile_browser->active_entry = entry;
        app->profile_browser->active_entry_valid = 1U;
    }
    return true;
}

static bool dndolphins_profile_browser_refresh(DndDolphinsApp* app) {
    if(!dndolphins_profile_browser_alloc(app)) return false;
    app->profile_browser->active_profile = app->active_profile;
    bool ok = dnd_storage_profiles_refresh(app->storage, app->profile_browser);
    if(ok) (void)dndolphins_profile_include_active(app);
    return ok;
}

static bool dndolphins_profile_browser_save(DndDolphinsApp* app) {
    if(!app || !app->profile_browser) return false;
    app->profile_browser->active_profile = app->active_profile;
    if(app->active_entry_valid && app->active_entry.id == app->active_profile) {
        app->profile_browser->active_entry = app->active_entry;
        app->profile_browser->active_entry_valid = 1U;
    }
    return dnd_storage_profiles_save(app->storage, app->profile_browser);
}

static bool dndolphins_screen_uses_spellbook(const DndDolphinsApp* app, DndScreen screen) {
    UNUSED(app);
#if defined(DND_BUILD_COMBAT)
    return screen == DndScreenSpellAttacks || screen == DndScreenFavoriteSpells ||
           screen == DndScreenUtilitySpells || screen == DndScreenRituals ||
           screen == DndScreenSpellCast || screen == DndScreenSpellResult;
#else
    UNUSED(screen);
    return false;
#endif
}

static bool dndolphins_screen_uses_items(const DndDolphinsApp* app, DndScreen screen) {
    UNUSED(app);
#if defined(DND_BUILD_COMBAT)
    return screen == DndScreenAttackList || screen == DndScreenAmmoChoice ||
           screen == DndScreenAttackResult;
#else
    UNUSED(screen);
    return false;
#endif
}

static bool dndolphins_screen_uses_features(const DndDolphinsApp* app, DndScreen screen) {
    if((screen == DndScreenRecordList || screen == DndScreenRecordDetail) &&
       app->list_kind == DndListFeatures)
        return true;
    if(screen == DndScreenCatalog && app->catalog && app->catalog->kind == DndCatalogFeats &&
       app->list_kind == DndListFeatures && app->level_choice_mode != 3U)
        return true;
    return false;
}

static bool dndolphins_screen_uses_roll(DndScreen screen) {
#if defined(DND_BUILD_COMBAT)
    return screen == DndScreenCombat || screen == DndScreenMagic ||
           screen == DndScreenSpellAttacks || screen == DndScreenFavoriteSpells ||
           screen == DndScreenUtilitySpells || screen == DndScreenRituals ||
           screen == DndScreenSpellCast || screen == DndScreenSpellResult ||
           screen == DndScreenAttackTemplates || screen == DndScreenAttackTemplateEdit ||
           screen == DndScreenAttackList || screen == DndScreenAmmoChoice ||
           screen == DndScreenAttackResult || screen == DndScreenDice ||
           screen == DndScreenDiceResult;
#else
    return screen == DndScreenAbilities || screen == DndScreenSkills || screen == DndScreenDice ||
           screen == DndScreenDiceResult;
#endif
}

static bool
    dndolphins_screen_uses_spell_class_counts(const DndDolphinsApp* app, DndScreen screen) {
#if defined(DND_BUILD_COMBAT)
    UNUSED(app);
    return screen == DndScreenMagic;
#else
    return app && screen == DndScreenRecordDetail && app->list_kind == DndListClasses;
#endif
}

static bool dndolphins_screen_uses_spell_resolver(DndScreen screen) {
    return screen == DndScreenSpellAttacks || screen == DndScreenFavoriteSpells ||
           screen == DndScreenUtilitySpells || screen == DndScreenSpellCast ||
           screen == DndScreenSpellResult;
}
static bool dndolphins_spell_plugin_require(DndDolphinsApp* app) {
    if(!app || !app->combat) return false;
    if(app->combat->spell_plugin.api) return true;
    DndPluginLoadResult loaded = dnd_plugin_open(
        &app->combat->spell_plugin,
        app->storage,
        DND_SPELL_DAMAGE_COMBAT_PATH,
        DND_SPELL_DAMAGE_API_ID,
        DND_SPELL_DAMAGE_API_VERSION,
        sizeof(DndSpellDamageApi));
    const DndSpellDamageApi* api = app->combat->spell_plugin.api;
    if(loaded == DndPluginLoadOk && api->damage_spec) return true;
    dnd_plugin_close(&app->combat->spell_plugin);
    dndolphins_set_status(app, dnd_plugin_load_message(loaded));
    return false;
}
static bool dndolphins_spell_damage_spec(
    DndDolphinsApp* app,
    const DndSpell* spell,
    uint8_t cast_level,
    uint8_t character_level,
    int8_t modifier,
    DndSpellDamageSpec* output) {
    if(!dndolphins_spell_plugin_require(app)) return false;
    const DndSpellDamageApi* api = app->combat->spell_plugin.api;
    return api->damage_spec(spell, cast_level, character_level, modifier, output);
}

static void dndolphins_enter_screen(DndDolphinsApp* app, DndScreen screen) {
#if defined(DND_BUILD_COMBAT)
    if(dndolphins_screen_uses_spell_resolver(screen)) {
        if(!dndolphins_spell_plugin_require(app)) return;
    } else if(app->combat) {
        dnd_plugin_close(&app->combat->spell_plugin);
    }
#endif
    DndScreen previous = app->screen;
    const bool next_uses_roll = dndolphins_screen_uses_roll(screen);
    const bool next_uses_spell_class_counts =
        dndolphins_screen_uses_spell_class_counts(app, screen);
    if(next_uses_roll && !dndolphins_roll_runtime_alloc(app)) {
        dndolphins_set_status(app, "Roll memory unavailable");
        return;
    }
    bool previous_profile_screen = previous == DndScreenProfiles ||
                                   previous == DndScreenProfileActions;
    bool next_profile_screen = screen == DndScreenProfiles || screen == DndScreenProfileActions;
    bool profile_browser_failed = false;
    if(next_profile_screen && !app->profile_browser)
        profile_browser_failed = !dndolphins_profile_browser_refresh(app);
    if(previous_profile_screen && !next_profile_screen) dndolphins_profile_browser_release(app);
    if(previous == DndScreenHome && screen != DndScreenHome)
        app->home_return_selection = app->selection;
    /* Catalog state is screen-owned. Free the descriptor and its page before
       pending saves allocate file objects/buffers. */
    if(previous == DndScreenCatalog && screen != DndScreenCatalog)
        dndolphins_catalog_runtime_release(app);
    if(previous != screen && app->autosave_pending) dndolphins_flush_save(app, false);

    bool needs_spellbook = dndolphins_screen_uses_spellbook(app, screen);
    bool needs_items = dndolphins_screen_uses_items(app, screen);
    bool needs_features = dndolphins_screen_uses_features(app, screen);
    bool character_collection_kind = app->list_kind == DndListLanguages ||
                                     app->list_kind == DndListProficiencies;
    bool previous_character_collection = character_collection_kind &&
                                         (previous == DndScreenRecordList ||
                                          previous == DndScreenRecordDetail ||
                                          previous == DndScreenCatalog);
    bool next_character_collection = character_collection_kind &&
                                     (screen == DndScreenRecordList ||
                                      screen == DndScreenRecordDetail ||
                                      screen == DndScreenCatalog);
    if(previous_character_collection && !next_character_collection)
        dndolphins_character_collection_release(app);
    if(!next_uses_roll && app->roll) dndolphins_roll_runtime_free(app);
    if(!next_uses_spell_class_counts && app->collection_cache &&
       app->collection_cache->spell_class_counts_valid) {
        app->collection_cache->spell_class_counts_valid = 0U;
        dndolphins_collection_cache_runtime_release_if_idle(app);
    }
    if(screen == DndScreenRecordDetail && app->list_kind == DndListClasses)
        if(app->collection_cache) app->collection_cache->spell_class_counts_valid = 0U;
    bool collection_failed = profile_browser_failed;
    if(screen == DndScreenRecordDetail && app->list_kind == DndListClasses &&
       !dndolphins_spell_class_counts_cached(app))
        collection_failed = true;
#if defined(DND_BUILD_COMBAT)
    if(screen == DndScreenMagic && !dndolphins_spell_class_counts_cached(app)) {
        collection_failed = true;
        dndolphins_set_status(app, "Spell count read failed");
    }
#endif
    if(!needs_spellbook && app->spellbook_loaded && !dndolphins_release_spellbook(app))
        collection_failed = true;
    if(!needs_items && app->items_loaded && !dndolphins_release_items(app))
        collection_failed = true;
#if defined(DND_BUILD_COMBAT)
    if(!needs_spellbook && app->combat->combat_spell_indices) {
        free(app->combat->combat_spell_indices);
        app->combat->combat_spell_indices = NULL;
        app->combat->combat_spell_count = 0U;
    }
    if(!needs_items && app->combat->combat_weapon_indices) {
        free(app->combat->combat_weapon_indices);
        app->combat->combat_weapon_indices = NULL;
        app->combat->combat_weapon_count = 0U;
    }
#endif
    if(!needs_features && app->features_loaded && !dndolphins_release_features(app))
        collection_failed = true;
    /* Item/Spell pages are true combat-lazy state. Entering a collection-backed
       combat list builds its bounded logical index, then hydrates only the five
       visible row labels before drawing. Canvas callbacks stay RAM-only. */
    if(needs_features && !app->features_loaded && !dndolphins_load_features(app))
        collection_failed = true;
#if defined(DND_BUILD_COMBAT)
    if(screen == DndScreenSpellAttacks && !dndolphins_refresh_combat_spell_index(app))
        collection_failed = true;
    if(screen == DndScreenFavoriteSpells && !dndolphins_refresh_favorite_spell_index(app))
        collection_failed = true;
    if(screen == DndScreenUtilitySpells && !dndolphins_refresh_utility_spell_index(app))
        collection_failed = true;
    if(screen == DndScreenRituals && !dndolphins_refresh_ritual_spell_index(app))
        collection_failed = true;
    if(screen == DndScreenAttackList && !dndolphins_refresh_combat_weapon_index(app))
        collection_failed = true;
#endif
#if !defined(DND_BUILD_COMBAT)
    if(screen == DndScreenCharacter) {
        if(!dnd_character_languages_count(app->storage, app->active_profile, &app->language_total))
            collection_failed = true;
        if(!dnd_character_proficiencies_count(
               app->storage, app->active_profile, &app->proficiency_total))
            collection_failed = true;
    }
#endif

    app->screen = screen;
    app->selection = 0U;
    app->scroll = 0U;
    if(screen == DndScreenHome && previous != DndScreenHome) {
        app->selection = app->home_return_selection;
        app->scroll = app->selection >= 5U ? (uint16_t)(app->selection - 4U) : 0U;
    }
#if defined(DND_BUILD_COMBAT)
    if(screen == DndScreenSpellAttacks)
        dndolphins_prepare_combat_spell_rows(app, DndCombatSpellModeAttacks);
    else if(screen == DndScreenFavoriteSpells)
        dndolphins_prepare_combat_spell_rows(app, DndCombatSpellModeFavorites);
    else if(screen == DndScreenUtilitySpells)
        dndolphins_prepare_combat_spell_rows(app, DndCombatSpellModeUtility);
    else if(screen == DndScreenRituals)
        dndolphins_prepare_combat_spell_rows(app, DndCombatSpellModeRituals);
    else if(screen == DndScreenAttackList)
        dndolphins_prepare_combat_weapon_rows(app);
#endif
    if(screen == DndScreenProfiles) {
        uint16_t count = dndolphins_profile_count(app);
        if(count) (void)dnd_storage_profiles_window(app->storage, app->profile_browser, 0U);
    }
#if defined(DND_BUILD_COMBAT)
    if(app->settings.debug && (screen == DndScreenCombat || screen == DndScreenSpellAttacks ||
                               screen == DndScreenUtilitySpells || screen == DndScreenRituals ||
                               screen == DndScreenAttackList))
        FURI_LOG_I(
            TAG,
            "Heap screen=%u free=%lu",
            (unsigned int)screen,
            (unsigned long)memmgr_get_free_heap());
#endif
    dndolphins_clear_action_ack(app);
    app->edit_modifier_mode = 0U;
    dndolphins_marquee_offset = 0U;
    if(app->storage_unsaved)
        dndolphins_set_status(app, "UNSAVED - retry SD");
    else if(!collection_failed)
        dndolphins_clear_status(app);
}

static void dndolphins_release_staged_records(DndDolphinsApp* app) {
    if(!app) return;
    dnd_data_reserve_grants_exact(&app->data.character, 0U);
    app->data.character.grant_count = 0U;
}

static void dndolphins_switch_profile(DndDolphinsApp* app, uint32_t profile) {
    if(!dndolphins_profile_exists(app, profile)) return;
    if(profile == app->active_profile) {
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
    dndolphins_release_staged_records(app);
    app->spellbook_loaded = 0U;
    app->items_loaded = 0U;
    app->features_loaded = 0U;
    app->spellbook_total = 0U;
    app->items_total = 0U;
    app->features_total = 0U;
    dndolphins_collection_cache_runtime_free(app);
    uint32_t previous_profile = app->active_profile;
    app->active_profile = profile;
#if defined(DND_BUILD_COMBAT)
    app->combat->arcane_recovery_active = 0U;
#endif
    bool recovered_backup = false;
    bool loaded = dnd_storage_load_profile(app->storage, profile, &app->data, &recovered_backup);
    app->active_profile_loaded = loaded ? 1U : 0U;
    bool character_ready = loaded;
    if(loaded && recovered_backup)
        character_ready = dnd_storage_recover_profile_backup(app->storage, profile, &app->data);
    if(!loaded) {
        /* Loading a target profile resets the parse buffer to defaults on failure.
           Immediately restore the previously flushed character so transient or
           damaged profiles can never expose/save a synthetic New Hero over it. */
        bool previous_recovered = false;
        app->active_profile = previous_profile;
        app->active_profile_loaded =
            dnd_storage_load_profile(
                app->storage, previous_profile, &app->data, &previous_recovered) ?
                1U :
                0U;
        if(app->active_profile_loaded && previous_recovered)
            dnd_storage_recover_profile_backup(app->storage, previous_profile, &app->data);
        app->storage_unsaved = 0U;
        dndolphins_active_entry_cache_from_browser(app);
        app->saved_fingerprint = dndolphins_data_fingerprint(&app->data);
        dndolphins_enter_screen(app, DndScreenHome);
        dndolphins_set_status(app, "Profile preserved - load failed");
        return;
    }
    bool metadata_saved = dndolphins_profile_browser_refresh(app);
    dndolphins_profile_include_active(app);
    metadata_saved = metadata_saved && dndolphins_profile_browser_save(app);
    if(character_ready && metadata_saved)
        app->saved_fingerprint = dndolphins_data_fingerprint(&app->data);
    dndolphins_enter_screen(app, DndScreenHome);
    if(!character_ready || !metadata_saved)
        dndolphins_set_status(app, "Profile save failed");
    else if(recovered_backup)
        dndolphins_set_status(app, "Backup recovered");
    else if(loaded)
        dndolphins_set_status(app, "Character switched");
    else
        dndolphins_set_status(app, "Fresh character");
}

static void dndolphins_create_profile(DndDolphinsApp* app) {
    if(app->active_profile_loaded && !dndolphins_flush_save(app, false)) {
        dndolphins_set_status(app, "Save failed");
        return;
    }
    dnd_data_clear_spells(&app->data.character);
    dnd_data_clear_items(&app->data.character);
    dnd_data_reserve_features_exact(&app->data.character, 0U);
    app->data.character.feature_count = 0U;
    dndolphins_release_staged_records(app);
    app->spellbook_loaded = 0U;
    app->items_loaded = 0U;
    app->features_loaded = 0U;
    app->spellbook_total = 0U;
    app->items_total = 0U;
    app->features_total = 0U;
    dndolphins_collection_cache_runtime_free(app);
    uint32_t profile = dnd_storage_profiles_next_id(app->profile_browser);
    if(profile == UINT32_MAX && ((app->profile_browser->reserved_id_seen &&
                                  app->profile_browser->highest_reserved_id == UINT32_MAX) ||
                                 dndolphins_profile_exists(app, UINT32_MAX))) {
        dndolphins_set_status(app, "Profile IDs exhausted");
        return;
    }
    uint32_t previous_profile = app->active_profile;
#if defined(DND_BUILD_COMBAT)
    app->combat->arcane_recovery_active = 0U;
#endif
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
        app->active_profile = previous_profile;
        app->active_profile_loaded =
            dnd_storage_load_profile(app->storage, previous_profile, &app->data, &recovered);
        dndolphins_profile_browser_refresh(app);
        dndolphins_profile_include_active(app);
        app->saved_fingerprint = dndolphins_data_fingerprint(&app->data);
        app->storage_read_only = 1U;
        app->storage_unsaved = 1U;
        dndolphins_enter_screen(app, DndScreenProfiles);
        dndolphins_set_status(app, "New character save failed");
        return;
    }
    app->active_profile_loaded = 1U;
    app->active_profile = profile;
    /* New-format character collections start clean; no legacy language/training
       migration is performed. Baseline languages are reviewed through Grant
       Initial Traits rather than being silently written during profile creation. */
    app->character_collections_changed = false;
    app->language_total = 0U;
    app->proficiency_total = 0U;
    dnd_data_clear_spells(&app->data.character);
    dnd_data_clear_items(&app->data.character);
    dnd_data_reserve_features_exact(&app->data.character, 0U);
    app->data.character.feature_count = 0U;
    dndolphins_release_staged_records(app);
    app->spellbook_loaded = 0U;
    app->items_loaded = 0U;
    app->features_loaded = 0U;
    app->spellbook_total = 0U;
    app->items_total = 0U;
    app->features_total = 0U;
    dndolphins_collection_cache_runtime_free(app);
    bool metadata_saved = dndolphins_profile_browser_refresh(app);
    dndolphins_profile_include_active(app);
    metadata_saved = metadata_saved && dndolphins_profile_browser_save(app);
    if(character_saved && metadata_saved)
        app->saved_fingerprint = dndolphins_data_fingerprint(&app->data);
    dndolphins_enter_screen(app, DndScreenCharacter);
    dndolphins_set_status(
        app, character_saved && metadata_saved ? "New character" : "Save failed");
}

static bool dndolphins_delete_profile(DndDolphinsApp* app, uint32_t profile) {
    if(!app || !dndolphins_profile_exists(app, profile)) return false;

    const bool deleting_active = profile == app->active_profile;
    DndProfileEntry replacement;
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
        dndolphins_release_staged_records(app);
        app->spellbook_loaded = 0U;
        app->items_loaded = 0U;
        app->features_loaded = 0U;
        app->spellbook_total = 0U;
        app->items_total = 0U;
        app->features_total = 0U;
        dndolphins_collection_cache_runtime_free(app);
        app->active_profile_loaded = 0U;
    }

    if(!dnd_storage_delete_profile(app->storage, profile)) return false;
    if(!dndolphins_profile_browser_refresh(app)) return false;

    if(deleting_active && have_replacement &&
       dnd_storage_profiles_find(app->storage, replacement.id, NULL)) {
        app->active_profile = replacement.id;
        bool recovered_backup = false;
        bool loaded =
            dnd_storage_load_profile(app->storage, replacement.id, &app->data, &recovered_backup);
        app->active_profile_loaded = loaded ? 1U : 0U;
        if(loaded && recovered_backup)
            loaded = dnd_storage_recover_profile_backup(app->storage, replacement.id, &app->data);
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
        app->active_profile = 0U;
        dnd_data_clear(&app->data);
        dnd_data_set_defaults(&app->data);
        app->active_profile_loaded = 0U;
        app->saved_fingerprint = dndolphins_data_fingerprint(&app->data);
    }

    if(!dndolphins_profile_browser_save(app)) return false;
    app->storage_read_only = 0U;
    app->storage_unsaved = 0U;
    app->selection = 0U;
    app->scroll = 0U;
    return true;
}

static uint8_t dndolphins_wizard_level(const DndCharacter* character) {
    for(uint8_t i = 0U; i < character->class_count; ++i)
        if(strcmp(character->classes[i].name, "Wizard") == 0) return character->classes[i].level;
    return 0U;
}

#if defined(DND_BUILD_COMBAT)
static bool dndolphins_begin_arcane_recovery(DndDolphinsApp* app) {
    DndCharacter* character = &app->data.character;
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
    app->combat->arcane_recovery_active = 1U;
    app->combat->arcane_recovery_budget = (wizard_level + 1U) / 2U;
    app->combat->arcane_recovery_spent = 0U;
    memset(
        app->combat->arcane_recovery_restored, 0, sizeof(app->combat->arcane_recovery_restored));
    dndolphins_enter_screen(app, DndScreenMagic);
    app->selection = 6U;
    app->scroll = 2U;
    dndolphins_set_status(app, "Choose slots, OK done");
    return true;
}

#else
static bool dndolphins_begin_arcane_recovery(DndDolphinsApp* app) {
    (void)app;
    return false;
}
#endif

static void dndolphins_menu_move(DndDolphinsApp* app, uint16_t count, int8_t delta) {
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
    if(proficiency == DndProficiencyExpertise) return "E";
    if(proficiency == DndProficiencyProficient) return "P";
    return "-";
}

static uint8_t dndolphins_cycle_die(uint8_t current, int8_t delta, bool damage_only) {
    const uint8_t* choices = damage_only ? dndolphins_damage_die_choices : dndolphins_die_choices;
    uint8_t count = damage_only ? sizeof(dndolphins_damage_die_choices) :
                                  sizeof(dndolphins_die_choices);
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
    if(strcmp(name, "Artificer") == 0) return DndClassMaskArtificer;
    if(strcmp(name, "Barbarian") == 0) return DndClassMaskBarbarian;
    if(strcmp(name, "Bard") == 0) return DndClassMaskBard;
    if(strcmp(name, "Cleric") == 0) return DndClassMaskCleric;
    if(strcmp(name, "Druid") == 0) return DndClassMaskDruid;
    if(strcmp(name, "Fighter") == 0) return DndClassMaskFighter;
    if(strcmp(name, "Monk") == 0) return DndClassMaskMonk;
    if(strcmp(name, "Paladin") == 0) return DndClassMaskPaladin;
    if(strcmp(name, "Ranger") == 0) return DndClassMaskRanger;
    if(strcmp(name, "Rogue") == 0) return DndClassMaskRogue;
    if(strcmp(name, "Sorcerer") == 0) return DndClassMaskSorcerer;
    if(strcmp(name, "Warlock") == 0) return DndClassMaskWarlock;
    if(strcmp(name, "Wizard") == 0) return DndClassMaskWizard;
    return 0U;
}

/* Mechanical class defaults are runtime rules, not catalog fallback data.
   The class catalogs contain names only, so Hit Die/spellcasting setup remains
   keyed by recognized class name even when the picker came from an asset. */
static void dndolphins_configure_class_defaults(DndClassLevel* level) {
    uint16_t mask = dndolphins_class_mask_from_name(level->name);
    if(mask & (DndClassMaskBarbarian))
        level->hit_die = 12U;
    else if(mask & (DndClassMaskFighter | DndClassMaskPaladin | DndClassMaskRanger))
        level->hit_die = 10U;
    else if(
        mask & (DndClassMaskBard | DndClassMaskCleric | DndClassMaskDruid | DndClassMaskMonk |
                DndClassMaskRogue | DndClassMaskWarlock | DndClassMaskArtificer))
        level->hit_die = 8U;
    else
        level->hit_die = 6U;
    /* Keep class/subclass spellcasting classification in one place so selecting
       Eldritch Knight or Arcane Trickster cannot drift from multiclass math. */
    (void)dndolphins_spells_refresh_class_spellcasting(level);
}

static uint8_t dndolphins_primary_class_save_mask(const char* class_name) {
    if(!class_name) return 0U;
#define SAVE_PAIR(a, b) ((uint8_t)((1U << (a)) | (1U << (b))))
    if(!strcmp(class_name, "Artificer"))
        return SAVE_PAIR(DndAbilityConstitution, DndAbilityIntelligence);
    if(!strcmp(class_name, "Barbarian"))
        return SAVE_PAIR(DndAbilityStrength, DndAbilityConstitution);
    if(!strcmp(class_name, "Bard")) return SAVE_PAIR(DndAbilityDexterity, DndAbilityCharisma);
    if(!strcmp(class_name, "Cleric")) return SAVE_PAIR(DndAbilityWisdom, DndAbilityCharisma);
    if(!strcmp(class_name, "Druid")) return SAVE_PAIR(DndAbilityIntelligence, DndAbilityWisdom);
    if(!strcmp(class_name, "Fighter"))
        return SAVE_PAIR(DndAbilityStrength, DndAbilityConstitution);
    if(!strcmp(class_name, "Monk")) return SAVE_PAIR(DndAbilityStrength, DndAbilityDexterity);
    if(!strcmp(class_name, "Paladin")) return SAVE_PAIR(DndAbilityWisdom, DndAbilityCharisma);
    if(!strcmp(class_name, "Ranger")) return SAVE_PAIR(DndAbilityStrength, DndAbilityDexterity);
    if(!strcmp(class_name, "Rogue")) return SAVE_PAIR(DndAbilityDexterity, DndAbilityIntelligence);
    if(!strcmp(class_name, "Sorcerer"))
        return SAVE_PAIR(DndAbilityConstitution, DndAbilityCharisma);
    if(!strcmp(class_name, "Warlock")) return SAVE_PAIR(DndAbilityWisdom, DndAbilityCharisma);
    if(!strcmp(class_name, "Wizard")) return SAVE_PAIR(DndAbilityIntelligence, DndAbilityWisdom);
#undef SAVE_PAIR
    return 0U;
}

static void dndolphins_clear_untouched_primary_class_saves(
    DndCharacter* character,
    const char* old_class,
    const char* new_class) {
    if(!character || !old_class || !new_class || !strcmp(old_class, new_class)) return;
    uint8_t current = 0U;
    for(uint8_t ability = 0U; ability < DND_ABILITY_COUNT; ++ability)
        if(character->saving_throw_proficiency[ability]) current |= (uint8_t)(1U << ability);
    uint8_t old_mask = dndolphins_primary_class_save_mask(old_class);
    /* Class saving-throw proficiencies are structured grants now. When a user
       changes an otherwise untouched primary class, clear the old automatic pair
       but do not grant the new pair here; Grant Initial Traits reviews each new
       save before it is applied. Deliberate manual/additional save proficiencies
       are preserved because their provenance cannot safely be inferred. */
    if(old_mask == 0U || current != old_mask) return;
    for(uint8_t ability = 0U; ability < DND_ABILITY_COUNT; ++ability)
        character->saving_throw_proficiency[ability] = 0U;
}

static uint16_t dndolphins_character_class_mask(const DndCharacter* character) {
    uint16_t mask = 0U;
    if(!character) return 0U;
    for(uint8_t i = 0U; i < character->class_count; ++i)
        mask |= dndolphins_class_mask_from_name(character->classes[i].name);
    return mask;
}

static uint16_t dndolphins_class_mask_from_list(char* text) {
    if(!text) return 0U;
    while(*text == ' ' || *text == '\t')
        ++text;
    if(!strcmp(text, "All") || !strcmp(text, "Any")) return 0x1FFFU;
    uint16_t mask = 0U;
    char* cursor = text;
    while(cursor && *cursor) {
        char* comma = strchr(cursor, ',');
        if(comma) *comma = '\0';
        while(*cursor == ' ' || *cursor == '\t')
            ++cursor;
        char* end = cursor + strlen(cursor);
        while(end > cursor && (end[-1] == ' ' || end[-1] == '\t'))
            *--end = '\0';
        mask |= dndolphins_class_mask_from_name(cursor);
        cursor = comma ? comma + 1U : NULL;
    }
    return mask;
}

static uint8_t dndolphins_proficiency_type_code(const char* type) {
    if(type && !strcmp(type, "Armor")) return 1U;
    if(type && !strcmp(type, "Weapon")) return 2U;
    if(type && !strcmp(type, "Tool")) return 3U;
    return 0U;
}

static const char* dndolphins_proficiency_type_name(uint8_t type) {
    return type == 1U ? "Armor" : type == 2U ? "Weapon" : type == 3U ? "Tool" : "Other";
}

static bool
    dndolphins_subclass_allowed(const DndDolphinsApp* app, uint16_t class_mask, bool has_metadata) {
    if(app->catalog->show_all) return true;
    if(!has_metadata || app->record_index >= app->data.character.class_count) return false;
    uint16_t selected_class =
        dndolphins_class_mask_from_name(app->data.character.classes[app->record_index].name);
    return selected_class && (class_mask & selected_class);
}

static bool dndolphins_feat_is_repeatable(const char* name) {
    return name &&
           (!strcmp(name, "Ability Score Improvement") || !strcmp(name, "Magic Initiate") ||
            !strcmp(name, "Magic Initiate (Cleric)") || !strcmp(name, "Magic Initiate (Druid)") ||
            !strcmp(name, "Magic Initiate (Wizard)") || !strcmp(name, "Skilled"));
}

static bool dndolphins_feat_is_origin(const char* name) {
    return name &&
           (!strcmp(name, "Alert") || !strcmp(name, "Crafter") || !strcmp(name, "Healer") ||
            !strcmp(name, "Lucky") || !strcmp(name, "Magic Initiate") ||
            !strcmp(name, "Magic Initiate (Cleric)") || !strcmp(name, "Magic Initiate (Druid)") ||
            !strcmp(name, "Magic Initiate (Wizard)") || !strcmp(name, "Musician") ||
            !strcmp(name, "Savage Attacker") || !strcmp(name, "Skilled") ||
            !strcmp(name, "Tavern Brawler") || !strcmp(name, "Tough") ||
            !strcmp(name, "Cult of the Dragon Initiate") ||
            !strcmp(name, "Emerald Enclave Fledgling") || !strcmp(name, "Harper Agent") ||
            !strcmp(name, "Lords' Alliance Agent") || !strcmp(name, "Purple Dragon Rook") ||
            !strcmp(name, "Spellfire Spark") || !strcmp(name, "Tyro of the Gauntlet") ||
            !strcmp(name, "Zhentarim Ruffian"));
}

static bool dndolphins_feat_is_fighting_style(const char* name) {
    return name &&
           (!strcmp(name, "Archery") || !strcmp(name, "Defense") ||
            !strcmp(name, "Great Weapon Fighting") || !strcmp(name, "Two-Weapon Fighting"));
}

static bool dndolphins_feat_is_epic_boon(const char* name) {
    return name && !strncmp(name, "Boon of ", 8U);
}

static bool dndolphins_character_has_fighting_style_feature(const DndCharacter* c) {
    if(!c) return false;
    for(uint8_t i = 0U; i < c->class_count; ++i) {
        const DndClassLevel* level = &c->classes[i];
        if(!strcmp(level->name, "Fighter") && level->level >= 1U) return true;
        if((!strcmp(level->name, "Paladin") || !strcmp(level->name, "Ranger")) &&
           level->level >= 2U)
            return true;
    }
    return false;
}

static bool dndolphins_character_has_spellcasting_feature(const DndCharacter* c) {
    if(!c) return false;
    for(uint8_t i = 0U; i < c->class_count; ++i) {
        if(c->classes[i].spellcasting_mode != DndSpellcastingNone) return true;
    }
    return false;
}

static bool dndolphins_feat_allowed(DndDolphinsApp* app, const char* name) {
    if(!app || !name || !name[0]) return false;
    /* Manual Features & Perks editing remains an unrestricted catalog. The
       prerequisite filter is only for an actual level-up/progression feat choice. */
    if(app->level_choice_mode != 3U || app->catalog->show_all) return true;

    const DndCharacter* c = &app->data.character;
    uint8_t total_level = dnd_rules_core_total_level(c);
    bool recognized = false;
    bool allowed = true;

    if(!strcmp(name, "Ability Score Improvement")) {
        recognized = true;
        allowed = total_level >= 4U;
    } else if(!strcmp(name, "Grappler")) {
        recognized = true;
        allowed = total_level >= 4U && (c->ability_scores[DndAbilityStrength] >= 13 ||
                                        c->ability_scores[DndAbilityDexterity] >= 13);
    } else if(dndolphins_feat_is_fighting_style(name)) {
        recognized = true;
        allowed = dndolphins_character_has_fighting_style_feature(c);
    } else if(dndolphins_feat_is_epic_boon(name)) {
        recognized = true;
        allowed = total_level >= 19U;
        if(allowed && !strcmp(name, "Boon of Spell Recall"))
            allowed = dndolphins_character_has_spellcasting_feature(c);
    } else if(
        !strcmp(name, "Alert") || !strcmp(name, "Magic Initiate") ||
        !strcmp(name, "Magic Initiate (Cleric)") || !strcmp(name, "Magic Initiate (Druid)") ||
        !strcmp(name, "Magic Initiate (Wizard)") || !strcmp(name, "Savage Attacker") ||
        !strcmp(name, "Skilled") || !strcmp(name, "Crafter") || !strcmp(name, "Healer") ||
        !strcmp(name, "Lucky") || !strcmp(name, "Musician") || !strcmp(name, "Tavern Brawler") ||
        !strcmp(name, "Tough")) {
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
           app->storage, app->active_profile, name, &found))
        return false; /* Allowed fails closed when duplicate eligibility cannot be verified. */
    return !found;
}

static const char* dndolphins_catalog_title(const DndDolphinsApp* app) {
    switch(app->catalog->kind) {
    case DndCatalogClasses:
        return "Choose Class";
    case DndCatalogSubclasses:
        return app->catalog->show_all ? "Subclasses: All" : "Choose Subclass";
    case DndCatalogSpecies:
        return "Choose Species";
    case DndCatalogBackgrounds:
        return "Choose Background";
    case DndCatalogAlignments:
        return "Choose Alignment";
    case DndCatalogFeats:
        if(app->level_choice_mode == 3U)
            return app->catalog->show_all ? "Feats: All" : "Feats: Allowed";
        return "Choose Feat/Perk";
    case DndCatalogLanguages:
        return "Choose Language";
    case DndCatalogProficiencies:
        return app->catalog->show_all ? "Proficiencies: All" : "Proficiencies: Allowed";
    case DndCatalogSpells:
        return "Choose Spell";
    case DndCatalogSkills:
        return "Choose Skill";
    case DndCatalogSkillTools:
        return "Choose Skill / Tool";
    case DndCatalogSizes:
        return "Choose Size";
    case DndCatalogGrantOptions:
#if defined(DND_BUILD_GRANTS)
        return "Choose Feature Option";
#else
        return "Choose Name";
#endif
    default:
        return "Choose Name";
    }
}

static bool dndolphins_choice_options_contains(const char* prerequisites, const char* value) {
    if(!prerequisites || !value || !value[0]) return false;
    const char* options = strstr(prerequisites, "Options: ");
    if(!options) return false;
    options += 9U;
    const size_t value_len = strlen(value);
    while(*options) {
        while(*options == ' ' || *options == '\t')
            ++options;
        const char* end = strchr(options, ';');
        size_t len = end ? (size_t)(end - options) : strlen(options);
        while(len && (options[len - 1U] == ' ' || options[len - 1U] == '\t'))
            --len;
        if(len == value_len && !strncmp(options, value, len)) return true;
        if(!end) break;
        options = end + 1U;
    }
    return false;
}

static bool dndolphins_class_skill_choice_allowed(const DndGrant* grant, const char* skill) {
    if(!grant || !skill) return true;
    if(strstr(grant->prerequisites, "Options: "))
        return dndolphins_choice_options_contains(grant->prerequisites, skill);
    if(strstr(grant->prerequisites, "Scholar skills")) {
        return !strcmp(skill, "Arcana") || !strcmp(skill, "History") ||
               !strcmp(skill, "Investigation") || !strcmp(skill, "Medicine") ||
               !strcmp(skill, "Nature") || !strcmp(skill, "Religion");
    }
    if(strncmp(grant->stable_id, "cls-", 4U)) return true;
    const char* c = grant->option_name;
    if(!strcmp(c, "Artificer"))
        return !strcmp(skill, "Arcana") || !strcmp(skill, "History") ||
               !strcmp(skill, "Investigation") || !strcmp(skill, "Medicine") ||
               !strcmp(skill, "Nature") || !strcmp(skill, "Perception") ||
               !strcmp(skill, "Sleight of Hand");
    if(!strcmp(c, "Barbarian"))
        return !strcmp(skill, "Animal Handling") || !strcmp(skill, "Athletics") ||
               !strcmp(skill, "Intimidation") || !strcmp(skill, "Nature") ||
               !strcmp(skill, "Perception") || !strcmp(skill, "Survival");
    if(!strcmp(c, "Bard")) return true;
    if(!strcmp(c, "Cleric"))
        return !strcmp(skill, "History") || !strcmp(skill, "Insight") ||
               !strcmp(skill, "Medicine") || !strcmp(skill, "Persuasion") ||
               !strcmp(skill, "Religion");
    if(!strcmp(c, "Druid"))
        return !strcmp(skill, "Arcana") || !strcmp(skill, "Animal Handling") ||
               !strcmp(skill, "Insight") || !strcmp(skill, "Medicine") ||
               !strcmp(skill, "Nature") || !strcmp(skill, "Perception") ||
               !strcmp(skill, "Religion") || !strcmp(skill, "Survival");
    if(!strcmp(c, "Fighter"))
        return !strcmp(skill, "Acrobatics") || !strcmp(skill, "Animal Handling") ||
               !strcmp(skill, "Athletics") || !strcmp(skill, "History") ||
               !strcmp(skill, "Insight") || !strcmp(skill, "Intimidation") ||
               !strcmp(skill, "Perception") || !strcmp(skill, "Persuasion") ||
               !strcmp(skill, "Survival");
    if(!strcmp(c, "Monk"))
        return !strcmp(skill, "Acrobatics") || !strcmp(skill, "Athletics") ||
               !strcmp(skill, "History") || !strcmp(skill, "Insight") ||
               !strcmp(skill, "Religion") || !strcmp(skill, "Stealth");
    if(!strcmp(c, "Paladin"))
        return !strcmp(skill, "Athletics") || !strcmp(skill, "Insight") ||
               !strcmp(skill, "Intimidation") || !strcmp(skill, "Medicine") ||
               !strcmp(skill, "Persuasion") || !strcmp(skill, "Religion");
    if(!strcmp(c, "Ranger"))
        return !strcmp(skill, "Animal Handling") || !strcmp(skill, "Athletics") ||
               !strcmp(skill, "Insight") || !strcmp(skill, "Investigation") ||
               !strcmp(skill, "Nature") || !strcmp(skill, "Perception") ||
               !strcmp(skill, "Stealth") || !strcmp(skill, "Survival");
    if(!strcmp(c, "Rogue"))
        return !strcmp(skill, "Acrobatics") || !strcmp(skill, "Athletics") ||
               !strcmp(skill, "Deception") || !strcmp(skill, "Insight") ||
               !strcmp(skill, "Intimidation") || !strcmp(skill, "Investigation") ||
               !strcmp(skill, "Perception") || !strcmp(skill, "Persuasion") ||
               !strcmp(skill, "Sleight of Hand") || !strcmp(skill, "Stealth");
    if(!strcmp(c, "Sorcerer"))
        return !strcmp(skill, "Arcana") || !strcmp(skill, "Deception") ||
               !strcmp(skill, "Insight") || !strcmp(skill, "Intimidation") ||
               !strcmp(skill, "Persuasion") || !strcmp(skill, "Religion");
    if(!strcmp(c, "Warlock"))
        return !strcmp(skill, "Arcana") || !strcmp(skill, "Deception") ||
               !strcmp(skill, "History") || !strcmp(skill, "Intimidation") ||
               !strcmp(skill, "Investigation") || !strcmp(skill, "Nature") ||
               !strcmp(skill, "Religion");
    if(!strcmp(c, "Wizard"))
        return !strcmp(skill, "Arcana") || !strcmp(skill, "History") ||
               !strcmp(skill, "Insight") || !strcmp(skill, "Investigation") ||
               !strcmp(skill, "Medicine") || !strcmp(skill, "Nature") ||
               !strcmp(skill, "Religion");
    return true;
}

static bool dndolphins_tool_choice_allowed(const DndGrant* grant, const char* tool) {
    if(!grant || !tool) return true;
    if(strstr(grant->prerequisites, "Artisan or Musical")) {
        return !strcmp(tool, "Musical Instrument") || strstr(tool, "Supplies") ||
               strstr(tool, "Tools") || strstr(tool, "Utensils");
    }
    if(strstr(grant->prerequisites, "Musical Instrument"))
        return !strcmp(tool, "Musical Instrument");
    if(strstr(grant->prerequisites, "Gaming Set")) return !strcmp(tool, "Gaming Set");
    if(strstr(grant->prerequisites, "Artisan")) {
        return strstr(tool, "Supplies") || strstr(tool, "Tools") || strstr(tool, "Utensils");
    }
    return true;
}

static bool dndolphins_catalog_add_metadata(
    DndDolphinsApp* app,
    const char* name,
    uint8_t level,
    uint16_t class_mask,
    bool has_metadata) {
#if defined(DND_BUILD_GRANTS)
    if((app->catalog->kind == DndCatalogSkills || app->catalog->kind == DndCatalogSkillTools) &&
       app->grant_review->grant_choice_active &&
       app->grant_review->grant_choice_index < app->data.character.grant_count && level == 4U &&
       !dndolphins_class_skill_choice_allowed(
           &app->data.character.grants[app->grant_review->grant_choice_index], name))
        return true;
    if(app->catalog->kind == DndCatalogSkills && app->grant_review->grant_choice_active &&
       app->grant_review->grant_choice_index < app->data.character.grant_count && level == 4U) {
        const DndGrant* grant = &app->data.character.grants[app->grant_review->grant_choice_index];
        for(uint8_t i = 0U; i < DND_SKILL_COUNT; ++i) {
            if(strcmp(name, dnd_rules_core_skill_names[i])) continue;
            if(!strncmp(grant->grant_value, "expertise=", 10U)) {
                if(app->data.character.skill_proficiency[i] == 0U ||
                   app->data.character.skill_proficiency[i] >= 2U)
                    return true;
            } else if(app->data.character.skill_proficiency[i] > 0U) {
                return true;
            }
            break;
        }
    }
    if(app->catalog->kind == DndCatalogLanguages && app->grant_review->grant_choice_active) {
        bool owned = false;
        if(!dnd_character_languages_contains(app->storage, app->active_profile, name, &owned) ||
           owned)
            return true;
        if(app->grant_review->grant_choice_index < app->data.character.grant_count) {
            const DndGrant* grant =
                &app->data.character.grants[app->grant_review->grant_choice_index];
            static const char* const standard_languages[] = {
                "Common",
                "Common Sign Language",
                "Draconic",
                "Dwarvish",
                "Elvish",
                "Giant",
                "Gnomish",
                "Goblin",
                "Halfling",
                "Orc",
            };
            static const char* const haunted_exotic_languages[] = {
                "Abyssal",
                "Celestial",
                "Deep Speech",
                "Draconic",
                "Infernal",
                "Primordial",
                "Sylvan",
                "Undercommon",
            };
            bool standard = false;
            bool haunted_exotic = false;
            for(size_t i = 0U; i < sizeof(standard_languages) / sizeof(standard_languages[0]); ++i)
                if(!strcmp(name, standard_languages[i])) {
                    standard = true;
                    break;
                }
            for(size_t i = 0U;
                i < sizeof(haunted_exotic_languages) / sizeof(haunted_exotic_languages[0]);
                ++i)
                if(!strcmp(name, haunted_exotic_languages[i])) {
                    haunted_exotic = true;
                    break;
                }
            if(strstr(grant->prerequisites, "Standard language") && !standard) return true;
            if(strstr(grant->prerequisites, "Haunted exotic language") && !haunted_exotic)
                return true;
            if(strstr(grant->prerequisites, "Draconic if unknown")) {
                bool has_draconic = false;
                if(!dnd_character_languages_contains(
                       app->storage, app->active_profile, "Draconic", &has_draconic))
                    return true;
                if(!has_draconic && strcmp(name, "Draconic")) return true;
            }
        }
    }
    if(app->catalog->kind == DndCatalogSizes && app->grant_review->grant_choice_active &&
       app->grant_review->grant_choice_index < app->data.character.grant_count) {
        const DndGrant* grant = &app->data.character.grants[app->grant_review->grant_choice_index];
        if(strstr(grant->prerequisites, "Small or Medium") && strcmp(name, "Small") &&
           strcmp(name, "Medium"))
            return true;
    }
#endif
    if(app->catalog->kind == DndCatalogSubclasses &&
       !dndolphins_subclass_allowed(app, class_mask, has_metadata)) {
        return false;
    }
    /* ASI is already represented by the two explicit ability-score options on
       the level-choice screen. Do not expose a no-effect ASI Feature row from
       the nested feat picker, even in All mode. */
    if(app->catalog->kind == DndCatalogFeats && app->level_choice_mode == 3U &&
       !strcmp(name, "Ability Score Improvement"))
        return true;
#if defined(DND_BUILD_GRANTS)
    if(app->catalog->kind == DndCatalogFeats && app->grant_review->grant_choice_active &&
       app->grant_review->grant_choice_index < app->data.character.grant_count) {
        DndGrant* grant = &app->data.character.grants[app->grant_review->grant_choice_index];
        if((!strncmp(grant->grant_value, "origin_feat=", 12U) ||
            (!strncmp(grant->grant_value, "feat=", 5U) &&
             strstr(grant->prerequisites, "Origin"))) &&
           !dndolphins_feat_is_origin(name))
            return true;
        if((!strncmp(grant->grant_value, "origin_feat=", 12U) ||
            strstr(grant->prerequisites, "Origin")) &&
           !strcmp(name, "Magic Initiate"))
            return true;
        if(strstr(grant->prerequisites, "Fighting Style") &&
           !dndolphins_feat_is_fighting_style(name))
            return true;
    }
#endif
    if(app->catalog->kind == DndCatalogFeats && !dndolphins_feat_allowed(app, name)) return true;
    if(app->catalog->kind == DndCatalogProficiencies && !app->catalog->show_all
#if defined(DND_BUILD_GRANTS)
       && !app->grant_review->grant_choice_active
#endif
    ) {
        uint16_t character_mask = dndolphins_character_class_mask(&app->data.character);
        /* Heavy armor and primary-class tool/weapon choices are not automatic
           multiclass proficiencies. Already granted/owned rows remain eligible. */
        if(!strcmp(name, "Heavy Armor") || level == 3U || (level == 2U && class_mask != 0x1FFFU)) {
            character_mask =
                app->data.character.class_count ?
                    dndolphins_class_mask_from_name(app->data.character.classes[0].name) :
                    0U;
            if(level == 3U && !strcmp(name, "Thieves' Tools"))
                character_mask = dndolphins_character_class_mask(&app->data.character);
            if(level == 2U) {
                uint16_t martial_classes = dndolphins_class_mask_from_name("Barbarian") |
                                           dndolphins_class_mask_from_name("Fighter") |
                                           dndolphins_class_mask_from_name("Paladin") |
                                           dndolphins_class_mask_from_name("Ranger");
                character_mask |= dndolphins_character_class_mask(&app->data.character) &
                                  martial_classes;
            }
        }
        bool eligible = has_metadata && (class_mask & character_mask);
        if(!eligible) {
            bool owned = false;
            if(!dnd_character_proficiencies_contains(
                   app->storage,
                   app->active_profile,
                   dndolphins_proficiency_type_name(level),
                   name,
                   &owned) ||
               !owned)
                return true;
        }
    }
#if defined(DND_BUILD_GRANTS)
    if((app->catalog->kind == DndCatalogProficiencies ||
        app->catalog->kind == DndCatalogSkillTools) &&
       app->grant_review->grant_choice_active && level >= 1U && level <= 3U) {
        bool owned = false;
        if(!dnd_character_proficiencies_contains(
               app->storage,
               app->active_profile,
               dndolphins_proficiency_type_name(level),
               name,
               &owned) ||
           owned)
            return true;
    }
#endif
    if(!name[0]) return false;
    uint16_t page_limit = dndolphins_catalog_page_limit(app);
    uint16_t absolute_index = app->catalog->scan_count++;
    if(absolute_index < app->catalog->page_start ||
       absolute_index >= app->catalog->page_start + page_limit)
        return true;
    for(uint16_t i = 0U; i < app->catalog->count; ++i) {
        if(strcmp(app->catalog->entries[i], name) == 0) {
            if(has_metadata) {
                app->catalog->levels[i] = level;
                app->catalog->class_masks[i] |= class_mask;
                app->catalog->has_metadata[i] = 1U;
            }
            return true;
        }
    }
    if(app->catalog->count >= page_limit ||
       !dndolphins_catalog_ensure_capacity(app, app->catalog->count + 1U)) {
        dndolphins_set_status(app, "Catalog memory full");
        return false;
    }
    dndolphins_copy(
        app->catalog->entries[app->catalog->count],
        sizeof(app->catalog->entries[app->catalog->count]),
        name);
    app->catalog->levels[app->catalog->count] = level;
    app->catalog->class_masks[app->catalog->count] = class_mask;
    app->catalog->has_metadata[app->catalog->count] = has_metadata ? 1U : 0U;
    ++app->catalog->count;
    return true;
}

static bool dndolphins_catalog_page_complete(const DndDolphinsApp* app) {
    return app->catalog->scan_count >
           app->catalog->page_start + dndolphins_catalog_page_limit(app);
}

static bool dndolphins_catalog_add(DndDolphinsApp* app, const char* name) {
    return dndolphins_catalog_add_metadata(app, name, 0U, 0U, false);
}

static DndGrant* dndolphins_active_choice_grant(DndDolphinsApp* app);

static void dndolphins_catalog_add_builtins(DndDolphinsApp* app, DndCatalogKind kind) {
    const char* const* entries = NULL;
    size_t count = 0U;
    switch(kind) {
    case DndCatalogClasses:
        entries = dndolphins_catalog_classes;
        count = sizeof(dndolphins_catalog_classes) / sizeof(dndolphins_catalog_classes[0]);
        break;
    case DndCatalogSubclasses:
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
    case DndCatalogSpecies:
        entries = dndolphins_catalog_species;
        count = sizeof(dndolphins_catalog_species) / sizeof(dndolphins_catalog_species[0]);
        break;
    case DndCatalogBackgrounds:
        entries = dndolphins_catalog_backgrounds;
        count = sizeof(dndolphins_catalog_backgrounds) / sizeof(dndolphins_catalog_backgrounds[0]);
        break;
    case DndCatalogAlignments:
        entries = dndolphins_catalog_alignments;
        count = sizeof(dndolphins_catalog_alignments) / sizeof(dndolphins_catalog_alignments[0]);
        break;
    case DndCatalogFeats:
        entries = dndolphins_catalog_feats;
        count = sizeof(dndolphins_catalog_feats) / sizeof(dndolphins_catalog_feats[0]);
        break;
    case DndCatalogSkills:
    case DndCatalogSkillTools:
        for(size_t i = 0U; i < DND_SKILL_COUNT; ++i) {
            dndolphins_catalog_add_metadata(app, dnd_rules_core_skill_names[i], 4U, 0U, true);
            if(dndolphins_catalog_page_complete(app)) break;
        }
        if(kind == DndCatalogSkills) return;
        break;
    case DndCatalogSizes:
        entries = dndolphins_size_names;
        count = sizeof(dndolphins_size_names) / sizeof(dndolphins_size_names[0]);
        break;
    case DndCatalogGrantOptions: {
#if defined(DND_BUILD_GRANTS)
        DndGrant* grant = dndolphins_active_choice_grant(app);
        if(!grant) return;
        const char* fixed_options[2] = {NULL, NULL};
        if(strstr(grant->prerequisites, "Divine Order")) {
            fixed_options[0] = "Protector";
            fixed_options[1] = "Thaumaturge";
        } else if(strstr(grant->prerequisites, "Primal Order")) {
            fixed_options[0] = "Magician";
            fixed_options[1] = "Warden";
        } else if(strstr(grant->prerequisites, "Fighting Style")) {
            const char* class_name = grant->class_index < app->data.character.class_count ?
                                         app->data.character.classes[grant->class_index].name :
                                         "";
            fixed_options[0] = "Fighting Style Feat";
            if(!strcmp(class_name, "Paladin"))
                fixed_options[1] = "Blessed Warrior";
            else if(!strcmp(class_name, "Ranger"))
                fixed_options[1] = "Druidic Warrior";
        }
        if(fixed_options[0]) {
            for(uint8_t i = 0U; i < 2U && fixed_options[i]; ++i)
                dndolphins_catalog_add(app, fixed_options[i]);
            return;
        }
        const char* options = strstr(grant->prerequisites, "Options: ");
        if(!options) return;
        options += 9U;
        char buffer[DND_NAME_LEN];
        dndolphins_copy(buffer, sizeof(buffer), options);
        char* cursor = buffer;
        while(cursor && cursor[0]) {
            char* separator = strchr(cursor, ';');
            if(separator) *separator = '\0';
            while(*cursor == ' ' || *cursor == '\t')
                ++cursor;
            if(cursor[0]) dndolphins_catalog_add(app, cursor);
            if(dndolphins_catalog_page_complete(app) || !separator) break;
            cursor = separator + 1U;
        }
        return;
#else
        UNUSED(app);
        return;
#endif
    }
    default:
        break;
    }
    for(size_t i = 0U; i < count; ++i) {
        dndolphins_catalog_add(app, entries[i]);
        if(dndolphins_catalog_page_complete(app)) break;
    }
}

static DndGrant* dndolphins_active_choice_grant(DndDolphinsApp* app) {
    if(!app || !app->grant_review || !app->grant_review->grant_choice_active ||
       app->grant_review->grant_choice_index >= app->data.character.grant_count)
        return NULL;
    return &app->data.character.grants[app->grant_review->grant_choice_index];
}

static uint8_t
    dndolphins_grant_choice_spell_max_level(const DndDolphinsApp* app, const DndGrant* grant) {
    if(!app || !grant) return 9U;
    if(strstr(grant->stable_id, "cantrip") || strstr(grant->prerequisites, "cantrip")) return 0U;
    const char* arcanum = strstr(grant->prerequisites, "Mystic Arcanum L");
    if(arcanum) {
        uint32_t exact = 0U;
        if(dndolphins_parse_u32_strict(arcanum + strlen("Mystic Arcanum L"), 9U, &exact))
            return (uint8_t)exact;
    }
    if(strstr(grant->option_name, "Magic Initiate")) return 1U;
    if(strstr(grant->prerequisites, "Max spell level 3")) return 3U;
    if(grant->class_index >= app->data.character.class_count) return 9U;
    DndClassLevel class_level = app->data.character.classes[grant->class_index];
    if(grant->level_gained && grant->level_gained < class_level.level)
        class_level.level = grant->level_gained;
    return dnd_spell_eligibility_class_max_spell_level(&class_level);
}

static uint16_t
    dndolphins_grant_choice_spell_class_mask(const DndDolphinsApp* app, const DndGrant* grant) {
    if(!app || !grant) return 0U;
    if(strstr(grant->option_name, "Magic Initiate (Cleric)"))
        return dndolphins_class_mask_from_name("Cleric");
    if(strstr(grant->option_name, "Magic Initiate (Druid)"))
        return dndolphins_class_mask_from_name("Druid");
    if(strstr(grant->option_name, "Magic Initiate (Wizard)"))
        return dndolphins_class_mask_from_name("Wizard");
    if(strstr(grant->prerequisites, "Cleric cantrip"))
        return dndolphins_class_mask_from_name("Cleric");
    if(strstr(grant->prerequisites, "Cleric spell"))
        return dndolphins_class_mask_from_name("Cleric");
    if(strstr(grant->prerequisites, "Druid cantrip"))
        return dndolphins_class_mask_from_name("Druid");
    if(strstr(grant->prerequisites, "Wizard cantrip"))
        return dndolphins_class_mask_from_name("Wizard");
    if(strstr(grant->prerequisites, "Wizard spell"))
        return dndolphins_class_mask_from_name("Wizard");
    if(!strcmp(grant->option_name, "Eldritch Knight") ||
       !strcmp(grant->option_name, "Arcane Trickster"))
        return dndolphins_class_mask_from_name("Wizard");
    if(strstr(grant->prerequisites, "Cleric, Druid, Wizard"))
        return dndolphins_class_mask_from_name("Cleric") |
               dndolphins_class_mask_from_name("Druid") |
               dndolphins_class_mask_from_name("Wizard");
    if(grant->class_index < app->data.character.class_count)
        return dndolphins_class_mask_from_name(
            app->data.character.classes[grant->class_index].name);
    return 0U;
}

static const char* dndolphins_grant_choice_spell_school(const DndGrant* grant) {
    if(!grant) return NULL;
    if(!strcmp(grant->option_name, "Abjurer")) return "Abjuration";
    if(!strcmp(grant->option_name, "Diviner")) return "Divination";
    if(!strcmp(grant->option_name, "Evoker")) return "Evocation";
    if(!strcmp(grant->option_name, "Illusionist")) return "Illusion";
    return NULL;
}

static void dndolphins_catalog_process_line(DndDolphinsApp* app, char* line) {
    char* start = line;
    while(*start == ' ' || *start == '\t')
        ++start;
    char* end = start + strlen(start);
    while(end > start && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r'))
        --end;
    *end = '\0';
    if(!start[0] || start[0] == '#') return;
    if(app->catalog->kind == DndCatalogSpells) {
        char* fields[6] = {0};
        uint8_t field_count = 0U;
        char* cursor = start;
        while(field_count < 6U) {
            fields[field_count++] = cursor;
            char* separator = strchr(cursor, '|');
            if(!separator) break;
            *separator = '\0';
            cursor = separator + 1U;
        }
        if(field_count < 3U) return;
        uint32_t level_u32 = 0U;
        if(!dndolphins_parse_u32_strict(fields[1], 9U, &level_u32)) return;
        uint16_t mask = dndolphins_class_mask_from_list(fields[2]);
#if defined(DND_BUILD_GRANTS)
        DndGrant* grant = dndolphins_active_choice_grant(app);
        uint8_t max_level = grant ? dndolphins_grant_choice_spell_max_level(app, grant) : 9U;
        if(level_u32 > max_level) return;
        if(grant && strstr(grant->stable_id, "cantrip") && level_u32 != 0U) return;
        if(grant && strstr(grant->option_name, "Magic Initiate") &&
           !strstr(grant->stable_id, "cantrip") && level_u32 != 1U)
            return;
        if(grant && strstr(grant->prerequisites, "Mystic Arcanum L") && level_u32 != max_level)
            return;
        uint16_t required = grant ? dndolphins_grant_choice_spell_class_mask(app, grant) : 0U;
        if(required && !(mask & required)) return;
        if(grant && strstr(grant->prerequisites, "Options: ") &&
           !dndolphins_choice_options_contains(grant->prerequisites, fields[0]))
            return;
        const char* school = grant ? dndolphins_grant_choice_spell_school(grant) : NULL;
        if(school && (field_count < 4U || strcmp(fields[3], school))) return;
#endif
        dndolphins_catalog_add_metadata(app, fields[0], (uint8_t)level_u32, mask, true);
        return;
    }
    if(app->catalog->kind == DndCatalogProficiencies ||
       app->catalog->kind == DndCatalogSkillTools) {
        char* first = strchr(start, '|');
        if(!first) return;
        *first = '\0';
        char* name = first + 1U;
        char* second = strchr(name, '|');
        if(!second) return;
        *second = '\0';
        char* classes = second + 1U;
        uint8_t type = dndolphins_proficiency_type_code(start);
        uint16_t mask = dndolphins_class_mask_from_list(classes);
        if(app->catalog->kind == DndCatalogSkillTools && type != 3U) return;
#if defined(DND_BUILD_GRANTS)
        if(app->catalog->kind == DndCatalogProficiencies &&
           app->grant_review->grant_choice_active &&
           app->grant_review->grant_choice_index < app->data.character.grant_count) {
            const char* payload =
                app->data.character.grants[app->grant_review->grant_choice_index].grant_value;
            uint8_t required_type = !strncmp(payload, "armor=", 6U)  ? 1U :
                                    !strncmp(payload, "weapon=", 7U) ? 2U :
                                    !strncmp(payload, "tool=", 5U)   ? 3U :
                                                                       0U;
            if(required_type && type != required_type) return;
            if(required_type == 3U &&
               !dndolphins_tool_choice_allowed(
                   &app->data.character.grants[app->grant_review->grant_choice_index], name))
                return;
        }
        if(app->catalog->kind == DndCatalogSkillTools && app->grant_review->grant_choice_active &&
           app->grant_review->grant_choice_index < app->data.character.grant_count &&
           !dndolphins_tool_choice_allowed(
               &app->data.character.grants[app->grant_review->grant_choice_index], name))
            return;
#endif
        if(type) dndolphins_catalog_add_metadata(app, name, type, mask, mask != 0U);
        return;
    }
    if(app->catalog->kind == DndCatalogSubclasses) {
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

static bool dndolphins_catalog_load_path(DndDolphinsApp* app, const char* path) {
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

static void dndolphins_catalog_load_external(DndDolphinsApp* app, DndCatalogKind kind) {
    if(kind >= DndCatalogCount) return;
    if(kind == DndCatalogSkills || kind == DndCatalogSizes) return;
    bool complete = dndolphins_catalog_load_path(app, dndolphins_catalog_path_for_mode(app, kind));
    if(complete && kind == DndCatalogFeats)
        (void)dndolphins_catalog_load_path(app, dndolphins_abilities_path_for_mode(app));
}

static uint8_t dndolphins_grant_source_from_text(const char* text) {
    if(strcmp(text, "species") == 0) return DndGrantSpecies;
    if(strcmp(text, "background") == 0) return DndGrantBackground;
    if(strcmp(text, "feat") == 0) return DndGrantFeat;
    if(strcmp(text, "class_feature") == 0) return DndGrantClassFeature;
    if(strcmp(text, "subclass_feature") == 0) return DndGrantSubclassFeature;
    if(strcmp(text, "item") == 0) return DndGrantItem;
    return DndGrantSourceCount;
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
    if(class_name && strcmp(class_name, "Fighter") == 0 && (level == 6U || level == 14U))
        return true;
    if(class_name && strcmp(class_name, "Rogue") == 0 && level == 10U) return true;
    return false;
}

static void dndolphins_level_choice_id(
    char* out,
    size_t size,
    const DndCharacter* c,
    uint8_t class_index,
    uint8_t level) {
    const char* name = class_index < c->class_count ? c->classes[class_index].name : "class";
    snprintf(out, size, "asi_%.12s_%u", name, level);
    for(char* p = out; *p; ++p)
        if(*p == ' ') *p = '_';
}

static bool dndolphins_level_choice_done(DndDolphinsApp* app, uint8_t class_index, uint8_t level) {
    char id[DND_SHORT_LEN];
    dndolphins_level_choice_id(id, sizeof(id), &app->data.character, class_index, level);
    return dndolphins_progression_store_applied_exists(app->storage, app->active_profile, id);
}

static bool dndolphins_begin_next_level_choice(DndDolphinsApp* app) {
    DndCharacter* c = &app->data.character;
    app->level_choice_class_index = 0U;
    app->level_choice_level = 0U;
    app->level_choice_mode = 0U;
    app->level_choice_first_ability = UINT8_MAX;
    app->level_choice_first_score = 0;
    for(uint8_t ci = 0U; ci < c->class_count; ++ci) {
        for(uint8_t level = 1U; level <= c->classes[ci].level; ++level) {
            if(dndolphins_class_asi_level(c->classes[ci].name, level) &&
               !dndolphins_level_choice_done(app, ci, level)) {
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
    DndDolphinsApp* app,
    uint8_t class_index,
    uint8_t old_level,
    uint8_t old_pb,
    uint8_t old_cantrips,
    uint8_t old_prepared,
    const uint8_t old_slots[DND_SLOT_COUNT]) {
    if(!app || class_index >= app->data.character.class_count) return;
    DndCharacter* character = &app->data.character;
    DndClassLevel* class_level = &character->classes[class_index];
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
        memcmp(old_slots, character->spell_slots_max, DND_SLOT_COUNT) != 0;
    app->level_review_choose_spells = class_level->cantrip_limit > old_cantrips ||
                                      class_level->prepared_limit > old_prepared;
    app->level_review_pending_choice = dndolphins_begin_next_level_choice(app) ? 1U : 0U;
    app->return_screen = DndScreenRecordDetail;
    dndolphins_enter_screen(app, DndScreenLevelReview);
    app->selection = 0U;
    app->scroll = 0U;
}

static bool dndolphins_complete_level_choice(DndDolphinsApp* app, const char* result) {
    (void)result;
    DndCharacter* c = &app->data.character;
    char id[DND_SHORT_LEN];
    dndolphins_level_choice_id(
        id, sizeof(id), c, app->level_choice_class_index, app->level_choice_level);
    return dndolphins_progression_store_mark_applied(app->storage, app->active_profile, id);
}

typedef struct {
    const char* stable_id;
    bool found;
} DndDolphinsSpellStableIdLookup;

static bool dndolphins_spell_stable_id_visitor(
    uint16_t logical_index,
    const DndSpell* spell,
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

static bool dndolphins_spell_stable_id_exists(DndDolphinsApp* app, const char* stable_id) {
    if(!app || !stable_id || !stable_id[0]) return false;
    DndDolphinsSpellStableIdLookup lookup = {.stable_id = stable_id, .found = false};
    if(!dnd_storage_visit_spells(
           app->storage, app->active_profile, dndolphins_spell_stable_id_visitor, &lookup, NULL))
        return false;
    return lookup.found;
}

static bool dndolphins_csv_contains(const char* csv, const char* value) {
    if(!csv || !value || !value[0]) return false;
    const size_t value_len = strlen(value);
    const char* cursor = csv;
    while(*cursor) {
        while(*cursor == ' ' || *cursor == ',')
            ++cursor;
        const char* end = strchr(cursor, ',');
        size_t len = end ? (size_t)(end - cursor) : strlen(cursor);
        while(len && cursor[len - 1U] == ' ')
            --len;
        if(len == value_len && strncmp(cursor, value, len) == 0) return true;
        if(!end) break;
        cursor = end + 1U;
    }
    return false;
}

static bool dndolphins_grant_payload_satisfied(DndDolphinsApp* app, const char* grant_value) {
    if(!app || !grant_value || !grant_value[0]) return false;
    DndCharacter* character = &app->data.character;
    char payload[DND_GRANT_VALUE_LEN];
    dndolphins_copy(payload, sizeof(payload), grant_value);
    char* separator = strchr(payload, '=');
    if(!separator) return false;
    *separator = '\0';
    const char* value = separator + 1U;

    if(strcmp(payload, "origin_feat") == 0) return strcmp(character->origin_feat, value) == 0;
    if(strcmp(payload, "skill") == 0) {
        for(uint8_t i = 0U; i < DND_SKILL_COUNT; ++i)
            if(!strcmp(value, dnd_rules_core_skill_names[i]))
                return character->skill_proficiency[i] > 0U;
        return false;
    }
    if(strcmp(payload, "expertise") == 0) {
        for(uint8_t i = 0U; i < DND_SKILL_COUNT; ++i)
            if(!strcmp(value, dnd_rules_core_skill_names[i]))
                return character->skill_proficiency[i] >= 2U;
        return false;
    }
    if(strcmp(payload, "save") == 0) {
        for(uint8_t i = 0U; i < DND_ABILITY_COUNT; ++i)
            if(!strcmp(value, dnd_rules_core_ability_names[i]))
                return character->saving_throw_proficiency[i] != 0U;
        return false;
    }
    if(strcmp(payload, "language") == 0) {
        bool found = false;
        return dnd_character_languages_contains(
                   app->storage, app->active_profile, value, &found) &&
               found;
    }
    if(strcmp(payload, "tool") == 0 || strcmp(payload, "armor") == 0 ||
       strcmp(payload, "weapon") == 0) {
        const char* type = !strcmp(payload, "tool")  ? "Tool" :
                           !strcmp(payload, "armor") ? "Armor" :
                                                       "Weapon";
        bool found = false;
        return dnd_character_proficiencies_contains(
                   app->storage, app->active_profile, type, value, &found) &&
               found;
    }
    if(strcmp(payload, "senses") == 0) return strcmp(character->senses, value) == 0;
    if(strcmp(payload, "resistance") == 0)
        return dndolphins_csv_contains(character->resistances, value);
    if(strcmp(payload, "speed") == 0) {
        uint32_t speed = 0U;
        return dndolphins_parse_u32_strict(value, 255U, &speed) &&
               character->speed == (int16_t)speed;
    }
    if(strcmp(payload, "size") == 0) {
        for(uint8_t i = 0U; i < DndSizeCount; ++i)
            if(strcmp(value, dndolphins_size_names[i]) == 0) return character->size == i;
        return false;
    }
    if(strcmp(payload, "feature") == 0 || strcmp(payload, "feat") == 0 ||
       strcmp(payload, "feat_long") == 0 || strcmp(payload, "feat_pb") == 0) {
        bool found = false;
        return dndolphins_progression_store_features_contains_name(
                   app->storage, app->active_profile, value, &found) &&
               found;
    }
    return false;
}

static bool dndolphins_grant_stable_id_exists(
    DndDolphinsApp* app,
    const char* stable_id,
    const char* grant_value) {
    DndCharacter* character = &app->data.character;
    if(!stable_id || !stable_id[0]) return false;
    for(uint8_t i = 0U; i < character->grant_count; ++i)
        if(strcmp(character->grants[i].stable_id, stable_id) == 0) return true;

    /* Persisted character/collection state is authoritative. Applied-grant marker
       files are only an audit trail and must never make a missing deterministic
       grant look complete. This also avoids reopening/scanning appliedgrants for
       every candidate, which made the explicit grant commands unnecessarily slow. */
    if(grant_value && strstr(grant_value, "Freepick"))
        return dndolphins_progression_store_applied_exists(
            app->storage, app->active_profile, stable_id);
    if(grant_value && strncmp(grant_value, "spell=", 6U) == 0)
        return dndolphins_spell_stable_id_exists(app, stable_id);
    return dndolphins_grant_payload_satisfied(app, grant_value);
}

static uint8_t dndolphins_stage_grants_up_to_owned(
    DndDolphinsApp* app,
    uint8_t source_type,
    const char* option,
    uint8_t maximum_level,
    uint8_t owner_class_index,
    uint8_t owner_level) {
    DndCharacter* character = &app->data.character;
    File* file = storage_file_alloc(app->storage);
    if(!file) return 0U;
    if(!storage_file_open(
           file, dndolphins_active_metadata_path(app), FSAM_READ, FSOM_OPEN_EXISTING)) {
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
            if(!line[0] || line[0] == '#' || character->grant_count >= DND_MAX_GRANTS) continue;
            char* fields[8];
            if(dndolphins_split_metadata(line, fields) != 8U ||
               dndolphins_grant_source_from_text(fields[2]) != source_type ||
               strcmp(fields[3], option) != 0 || !fields[7][0])
                continue;
            char stable_id[DND_SHORT_LEN];
            dndolphins_copy(stable_id, sizeof(stable_id), fields[0]);
            if(dndolphins_grant_stable_id_exists(app, stable_id, fields[7])) continue;
            uint32_t level_gained = 0U;
            if(!dndolphins_parse_u32_strict(fields[5], UINT8_MAX, &level_gained)) continue;
            if(level_gained > maximum_level) continue;
            if(source_type == DndGrantClassFeature || source_type == DndGrantSubclassFeature) {
                uint8_t class_index =
                    owner_class_index < character->class_count ? owner_class_index : 0U;
                if(level_gained == 0U || level_gained > character->classes[class_index].level)
                    continue;
                if(strstr(fields[4], "Primary") && class_index != 0U) continue;
                if(strstr(fields[4], "Multiclass") && class_index == 0U) continue;
            } else if(source_type == DndGrantSpecies) {
                uint8_t total_level = dnd_rules_core_total_level(character);
                if(total_level < 1U) total_level = 1U;
                if(level_gained > total_level) continue;
            } else if(source_type == DndGrantFeat) {
                uint8_t total_level = dnd_rules_core_total_level(character);
                if(level_gained > total_level) continue;
            }
            if(!dnd_data_reserve_grants(character, character->grant_count + 1U)) continue;
            DndGrant* grant = &character->grants[character->grant_count++];
            memset(grant, 0, sizeof(*grant));
            dndolphins_copy(grant->stable_id, sizeof(grant->stable_id), stable_id);
            dndolphins_copy(grant->source, sizeof(grant->source), fields[1]);
            dndolphins_copy(grant->option_name, sizeof(grant->option_name), fields[3]);
            dndolphins_copy(grant->prerequisites, sizeof(grant->prerequisites), fields[4]);
            dndolphins_copy(grant->grant_value, sizeof(grant->grant_value), fields[7]);
            grant->source_type = source_type;
            grant->class_index = source_type == DndGrantSpecies             ? 0U :
                                 owner_class_index < character->class_count ? owner_class_index :
                                                                              0U;
            grant->level_gained = source_type == DndGrantFeat && level_gained == 0U &&
                                          owner_level ?
                                      owner_level :
                                      (uint8_t)level_gained;
            grant->status = DndGrantPending;
            ++staged;
        }
    }
    storage_file_close(file);
    storage_file_free(file);
    return staged;
}

static uint8_t dndolphins_stage_grants_up_to(
    DndDolphinsApp* app,
    uint8_t source_type,
    const char* option,
    uint8_t maximum_level) {
    uint8_t owner_class_index =
        app && app->record_index < app->data.character.class_count ? app->record_index : 0U;
    uint8_t owner_level = app && owner_class_index < app->data.character.class_count ?
                              app->data.character.classes[owner_class_index].level :
                              0U;
    return dndolphins_stage_grants_up_to_owned(
        app, source_type, option, maximum_level, owner_class_index, owner_level);
}

static uint8_t
    dndolphins_stage_grants(DndDolphinsApp* app, uint8_t source_type, const char* option) {
    return dndolphins_stage_grants_up_to(app, source_type, option, UINT8_MAX);
}

static void dndolphins_stage_or_defer_feat_dependencies(
    DndDolphinsApp* app,
    const char* option,
    uint8_t owner_class_index,
    uint8_t owner_level) {
    if(!app || !option || !option[0]) return;
    /* While a grant review is active, never rescan the metadata file once per
       applied Feature/Feat. Record one deferred dependency pass instead. It runs
       after the current bounded batch is complete and its resident rows have
       been released, so Apply All remains O(one metadata pass) rather than
       O(grants x metadata). Catalog/level-choice feat selections outside a grant
       review still resolve their one owner immediately. */
    if(app->grant_review && (app->screen == DndScreenGrantReview || app->grant_review->batches)) {
        app->grant_review->dependency_scan_needed = 1U;
        /* If a dependency pass is already paused beyond byte zero, a newly
           acquired feature may own a feat row that occurred before the saved
           cursor. Finish the current forward pass, then make one clean rescan
           generation. This is bounded by dependency depth, never by draw/tick. */
        if(app->grant_review->dependency_metadata_offset)
            app->grant_review->dependency_rescan_needed = 1U;
        return;
    }
    (void)dndolphins_stage_grants_up_to_owned(
        app, DndGrantFeat, option, UINT8_MAX, owner_class_index, owner_level);
}

static void dndolphins_append_csv_unique(char* destination, size_t size, const char* value) {
    if(!destination || !size || !value || !value[0]) return;
    const size_t value_len = strlen(value);
    const char* cursor = destination;
    while(*cursor) {
        while(*cursor == ' ' || *cursor == ',')
            ++cursor;
        const char* end = strchr(cursor, ',');
        size_t len = end ? (size_t)(end - cursor) : strlen(cursor);
        while(len && cursor[len - 1U] == ' ')
            --len;
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

static bool dndolphins_lookup_bundled_spell_level(
    DndDolphinsApp* app,
    const char* spell_name,
    uint8_t* level_out) {
    if(!app || !spell_name || !spell_name[0] || !level_out) return false;
    File* file = storage_file_alloc(app->storage);
    if(!file ||
       !storage_file_open(
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

static void dndolphins_apply_grant(DndDolphinsApp* app, DndGrant* grant) {
    DndCharacter* character = &app->data.character;
    char payload[DND_GRANT_VALUE_LEN];
    dndolphins_copy(payload, sizeof(payload), grant->grant_value);
    char* separator = strchr(payload, '=');
    if(!separator) {
        grant->status = DndGrantSkipped;
        return;
    }
    *separator = '\0';
    const char* value = separator + 1U;
    bool applied = false;
    if(strcmp(payload, "origin_feat") == 0) {
        dndolphins_copy(character->origin_feat, sizeof(character->origin_feat), value);
        applied = true;
        if(strncmp(value, "Freepick", 8U) != 0)
            dndolphins_stage_or_defer_feat_dependencies(
                app, value, grant->class_index, grant->level_gained);
    } else if(strcmp(payload, "skill") == 0) {
        for(uint8_t i = 0U; i < DND_SKILL_COUNT; ++i) {
            if(!strcmp(value, dnd_rules_core_skill_names[i])) {
                if(character->skill_proficiency[i] < 1U) character->skill_proficiency[i] = 1U;
                applied = true;
                break;
            }
        }
    } else if(strcmp(payload, "expertise") == 0) {
        for(uint8_t i = 0U; i < DND_SKILL_COUNT; ++i) {
            if(!strcmp(value, dnd_rules_core_skill_names[i])) {
                character->skill_proficiency[i] = 2U;
                applied = true;
                break;
            }
        }
    } else if(strcmp(payload, "save") == 0) {
        for(uint8_t i = 0U; i < DND_ABILITY_COUNT; ++i) {
            if(!strcmp(value, dnd_rules_core_ability_names[i])) {
                character->saving_throw_proficiency[i] = 1U;
                applied = true;
                break;
            }
        }
    } else if(strcmp(payload, "language") == 0) {
        applied = dnd_character_languages_append(app->storage, app->active_profile, value);
        if(applied)
            (void)dnd_character_languages_count(
                app->storage, app->active_profile, &app->language_total);
        if(applied) app->character_collections_changed = true;
    } else if(
        strcmp(payload, "tool") == 0 || strcmp(payload, "armor") == 0 ||
        strcmp(payload, "weapon") == 0) {
        const char* type = !strcmp(payload, "tool")  ? "Tool" :
                           !strcmp(payload, "armor") ? "Armor" :
                                                       "Weapon";
        applied =
            dnd_character_proficiencies_append(app->storage, app->active_profile, type, value);
        if(applied)
            (void)dnd_character_proficiencies_count(
                app->storage, app->active_profile, &app->proficiency_total);
        if(applied) app->character_collections_changed = true;
    } else if(strcmp(payload, "senses") == 0) {
        dndolphins_copy(character->senses, sizeof(character->senses), value);
        applied = true;
    } else if(strcmp(payload, "size") == 0) {
        for(uint8_t i = 0U; i < DndSizeCount; ++i) {
            if(strcmp(value, dndolphins_size_names[i]) == 0) {
                character->size = i;
                applied = true;
                break;
            }
        }
    } else if(strcmp(payload, "resistance") == 0) {
        dndolphins_append_csv_unique(
            character->resistances, sizeof(character->resistances), value);
        applied = true;
    } else if(strcmp(payload, "speed") == 0) {
        uint32_t speed = 0U;
        if(dndolphins_parse_u32_strict(value, 255U, &speed)) {
            character->speed = (int16_t)speed;
            applied = true;
        }
    } else if(strcmp(payload, "feat") == 0) {
        DndFeature feature;
        memset(&feature, 0, sizeof(feature));
        dndolphins_copy(feature.name, sizeof(feature.name), value);
        feature.class_index = grant->class_index;
        feature.class_level_gained = grant->level_gained;
        applied = dndolphins_progression_store_features_append(
            app->storage, app->active_profile, &feature);
        if(applied) {
            (void)dndolphins_progression_store_features_count(
                app->storage, app->active_profile, &app->features_total);
            dndolphins_stage_or_defer_feat_dependencies(
                app, value, grant->class_index, grant->level_gained);
        }
    } else if(strcmp(payload, "item") == 0) {
        DndItem item;
        memset(&item, 0, sizeof(item));
        dndolphins_copy(item.name, sizeof(item.name), value);
        item.quantity = 1;
        item.container_index = -1;
        applied = dnd_storage_append_item(app->storage, app->active_profile, character, &item);
        if(applied) ++app->items_total;
    } else if(
        strcmp(payload, "feature") == 0 || strcmp(payload, "feat_long") == 0 ||
        strcmp(payload, "feat_pb") == 0) {
        DndFeature feature;
        memset(&feature, 0, sizeof(feature));
        dndolphins_copy(feature.name, sizeof(feature.name), value);
        feature.class_index = grant->class_index;
        feature.class_level_gained = grant->level_gained;
        if(strcmp(payload, "feat_long") == 0) {
            feature.uses_current = 1;
            feature.uses_max = 1;
            feature.recharge = DndRechargeLong;
        } else if(strcmp(payload, "feat_pb") == 0) {
            feature.uses_current = dnd_rules_core_proficiency_bonus(character);
            feature.uses_max = feature.uses_current;
            feature.recharge = DndRechargeLong;
            feature.resource_formula = DndResourceProficiency;
        }
        applied = dndolphins_progression_store_features_append(
            app->storage, app->active_profile, &feature);
        if(applied) {
            uint16_t total = 0U;
            if(dndolphins_progression_store_features_count(
                   app->storage, app->active_profile, &total))
                app->features_total = total;
            dndolphins_stage_or_defer_feat_dependencies(
                app, value, grant->class_index, grant->level_gained);
        }
    } else if(strcmp(payload, "spell") == 0) {
        uint16_t total_spells = 0U;
        if(!dnd_storage_visit_spells(
               app->storage, app->active_profile, NULL, NULL, &total_spells)) {
            applied = false;
        } else {
            DndSpell spell;
            memset(&spell, 0, sizeof(spell));
            dndolphins_copy(spell.name, sizeof(spell.name), value);
            dndolphins_copy(spell.source, sizeof(spell.source), grant->source);
            dndolphins_copy(spell.grant_name, sizeof(spell.grant_name), grant->option_name);
            dndolphins_copy(spell.stable_id, sizeof(spell.stable_id), grant->stable_id);
            spell.class_index = grant->class_index;
            spell.grant_source = grant->source_type;
            (void)dndolphins_lookup_bundled_spell_level(app, value, &spell.level);
            uint8_t free_cast =
                (grant->source_type == DndGrantSpecies && grant->level_gained >= 3U) ||
                        strstr(grant->prerequisites, "Free cast") ||
                        strstr(grant->prerequisites, "Mystic Arcanum") ?
                    1U :
                    0U;
            if(dnd_storage_append_spell(
                   app->storage,
                   app->active_profile,
                   character,
                   &spell,
                   1U,
                   1U,
                   free_cast,
                   free_cast)) {
                if(app->collection_cache) app->collection_cache->spell_class_counts_valid = 0U;
                app->spellbook_total = total_spells + 1U;
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
            app->storage, app->active_profile, grant->stable_id);
    }
    grant->status = applied ? DndGrantApplied : DndGrantSkipped;
}

typedef struct {
    char feat_name[DND_NAME_LEN];
    uint8_t feat_lookup_valid;
    uint8_t feat_found;
    uint8_t feat_class_index;
    uint8_t feat_level_gained;
} DndDolphinsGrantScanCache;

static bool dndolphins_stage_character_grant_line(
    DndDolphinsApp* app,
    char* line,
    uint8_t maximum_level,
    bool include_background,
    uint8_t* staged,
    DndDolphinsGrantScanCache* cache) {
    if(!app || !line || !staged || !line[0] || line[0] == '#') return true;
    DndCharacter* character = &app->data.character;
    if(character->grant_count >= DND_MAX_GRANTS) return true;

    char* fields[8];
    if(dndolphins_split_metadata(line, fields) != 8U || !fields[7][0]) return true;
    uint8_t source_type = dndolphins_grant_source_from_text(fields[2]);
    if(source_type != DndGrantSpecies && source_type != DndGrantBackground &&
       source_type != DndGrantFeat && source_type != DndGrantClassFeature &&
       source_type != DndGrantSubclassFeature)
        return true;

    uint32_t level_gained_u32 = 0U;
    if(!dndolphins_parse_u32_strict(fields[5], UINT8_MAX, &level_gained_u32)) return true;
    uint8_t level_gained = (uint8_t)level_gained_u32;
    if(level_gained > maximum_level) return true;

    uint8_t class_index = 0U;
    bool matches = false;
    if(source_type == DndGrantSpecies) {
        uint8_t total_level = dnd_rules_core_total_level(character);
        if(total_level < 1U) total_level = 1U;
        matches = strcmp(fields[3], character->species) == 0 && level_gained <= total_level;
    } else if(source_type == DndGrantBackground) {
        matches = include_background && strcmp(fields[3], character->background) == 0;
    } else if(source_type == DndGrantFeat) {
        uint8_t total_level = dnd_rules_core_total_level(character);
        bool feature_found = false;
        DndFeature owner_feature;
        memset(&owner_feature, 0, sizeof(owner_feature));
        bool is_origin = strcmp(character->origin_feat, fields[3]) == 0;
        if(!is_origin) {
            if(cache && cache->feat_lookup_valid && strcmp(cache->feat_name, fields[3]) == 0) {
                feature_found = cache->feat_found != 0U;
                owner_feature.class_index = cache->feat_class_index;
                owner_feature.class_level_gained = cache->feat_level_gained;
            } else {
                if(!dndolphins_progression_store_features_find_name(
                       app->storage,
                       app->active_profile,
                       fields[3],
                       &owner_feature,
                       &feature_found))
                    return false;
                if(cache) {
                    dndolphins_copy(cache->feat_name, sizeof(cache->feat_name), fields[3]);
                    cache->feat_lookup_valid = 1U;
                    cache->feat_found = feature_found ? 1U : 0U;
                    cache->feat_class_index = owner_feature.class_index;
                    cache->feat_level_gained = owner_feature.class_level_gained;
                }
            }
        }
        if(feature_found) class_index = owner_feature.class_index;
        matches = (is_origin || feature_found) && level_gained <= total_level;
    } else {
        if(level_gained == 0U) return true;
        for(uint8_t i = 0U; i < character->class_count; ++i) {
            const char* option = source_type == DndGrantClassFeature ?
                                     character->classes[i].name :
                                     character->classes[i].subclass;
            if(source_type == DndGrantSubclassFeature &&
               (!option[0] || strcmp(option, "None") == 0))
                continue;
            if(strcmp(fields[3], option) == 0 && level_gained <= character->classes[i].level) {
                if(strstr(fields[4], "Primary") && i != 0U) continue;
                if(strstr(fields[4], "Multiclass") && i == 0U) continue;
                class_index = i;
                matches = true;
                break;
            }
        }
    }
    char stable_id[DND_SHORT_LEN];
    dndolphins_copy(stable_id, sizeof(stable_id), fields[0]);
    if(!matches || dndolphins_grant_stable_id_exists(app, stable_id, fields[7])) return true;
    if(!dnd_data_reserve_grants(character, character->grant_count + 1U)) return false;

    DndGrant* grant = &character->grants[character->grant_count++];
    memset(grant, 0, sizeof(*grant));
    dndolphins_copy(grant->stable_id, sizeof(grant->stable_id), stable_id);
    dndolphins_copy(grant->source, sizeof(grant->source), fields[1]);
    dndolphins_copy(grant->option_name, sizeof(grant->option_name), fields[3]);
    dndolphins_copy(grant->prerequisites, sizeof(grant->prerequisites), fields[4]);
    dndolphins_copy(grant->grant_value, sizeof(grant->grant_value), fields[7]);
    grant->source_type = source_type;
    grant->class_index = class_index;
    grant->level_gained = source_type == DndGrantFeat && level_gained == 0U && cache &&
                                  cache->feat_found ?
                              cache->feat_level_gained :
                              level_gained;
    grant->status = DndGrantPending;
    ++*staged;
    return true;
}

static bool dndolphins_stage_synthetic_grant(
    DndDolphinsApp* app,
    const char* stable_id,
    const char* option_name,
    const char* prerequisites,
    const char* grant_value,
    uint8_t source_type,
    uint8_t* staged) {
    DndCharacter* character = &app->data.character;
    if(character->grant_count >= DND_MAX_GRANTS ||
       dndolphins_grant_stable_id_exists(app, stable_id, grant_value))
        return true;
    if(!dnd_data_reserve_grants(character, character->grant_count + 1U)) return false;
    DndGrant* grant = &character->grants[character->grant_count++];
    memset(grant, 0, sizeof(*grant));
    dndolphins_copy(grant->stable_id, sizeof(grant->stable_id), stable_id);
    dndolphins_copy(grant->source, sizeof(grant->source), "Core");
    dndolphins_copy(grant->option_name, sizeof(grant->option_name), option_name);
    dndolphins_copy(grant->prerequisites, sizeof(grant->prerequisites), prerequisites);
    dndolphins_copy(grant->grant_value, sizeof(grant->grant_value), grant_value);
    grant->source_type = source_type;
    grant->status = DndGrantPending;
    ++*staged;
    return true;
}

static bool dndolphins_stage_initial_languages(DndDolphinsApp* app, uint8_t* staged) {
    return dndolphins_stage_synthetic_grant(
               app,
               "origin-language-common",
               "Languages",
               "Common",
               "language=Common",
               DndGrantBackground,
               staged) &&
           dndolphins_stage_synthetic_grant(
               app,
               "origin-language-choice-1",
               "Language Choice 1",
               "Choose a Standard language",
               "language=Freepick",
               DndGrantBackground,
               staged) &&
           dndolphins_stage_synthetic_grant(
               app,
               "origin-language-choice-2",
               "Language Choice 2",
               "Choose a Standard language",
               "language=Freepick",
               DndGrantBackground,
               staged);
}

static bool dndolphins_stage_owned_feat_dependencies(
    DndDolphinsApp* app,
    uint8_t* staged_out,
    bool* complete_out) {
    if(staged_out) *staged_out = 0U;
    if(complete_out) *complete_out = false;
    if(!app) return false;

    uint8_t staged = 0U;
    bool complete = false;
    bool ok = true;
    uint8_t generations = 0U;
    /* A dependency generation is one forward metadata pass. If applying rows
       from a paused generation creates new feature owners, finish that pass and
       perform exactly one follow-up generation from byte zero. This prevents a
       24-row review page from degenerating into 24 full-file rescans. */
    do {
        if(++generations > 8U) {
            dndolphins_set_status(app, "Grant dependency cap");
            ok = false;
            break;
        }
        File* file = storage_file_alloc(app->storage);
        if(!file) return false;
        if(!storage_file_open(
               file, dndolphins_active_metadata_path(app), FSAM_READ, FSOM_OPEN_EXISTING)) {
            storage_file_free(file);
            return false;
        }
        if(app->grant_review->dependency_metadata_offset &&
           !storage_file_seek(file, app->grant_review->dependency_metadata_offset, true)) {
            storage_file_close(file);
            storage_file_free(file);
            return false;
        }

        char line[256];
        size_t position = 0U;
        uint8_t buffer[512];
        size_t count = 0U;
        bool queue_full = false;
        uint32_t raw_offset = app->grant_review->dependency_metadata_offset;
        DndDolphinsGrantScanCache cache;
        memset(&cache, 0, sizeof(cache));
        while(ok && !queue_full &&
              (count = storage_file_read(file, buffer, sizeof(buffer))) > 0U) {
            for(size_t i = 0U; i < count; ++i) {
                char byte = (char)buffer[i];
                if(raw_offset != UINT32_MAX) ++raw_offset;
                if(byte != '\n' && position + 1U < sizeof(line)) {
                    if(byte != '\r') line[position++] = byte;
                    continue;
                }
                if(byte != '\n') {
                    position = 0U;
                    continue;
                }
                line[position] = '\0';
                position = 0U;
                if(strstr(line, "|feat|") && !dndolphins_stage_character_grant_line(
                                                 app, line, UINT8_MAX, false, &staged, &cache)) {
                    ok = false;
                    break;
                }
                if(app->data.character.grant_count >= DND_MAX_GRANTS) {
                    queue_full = true;
                    break;
                }
            }
        }
        if(ok && !queue_full && position) {
            line[position] = '\0';
            if(strstr(line, "|feat|"))
                ok = dndolphins_stage_character_grant_line(
                    app, line, UINT8_MAX, false, &staged, &cache);
        }
        if(storage_file_get_error(file) != FSE_OK) ok = false;
        storage_file_close(file);
        storage_file_free(file);
        if(!ok) break;

        if(queue_full) {
            app->grant_review->dependency_metadata_offset = raw_offset;
            complete = false;
            break;
        }

        app->grant_review->dependency_metadata_offset = 0U;
        if(app->grant_review->dependency_rescan_needed) {
            app->grant_review->dependency_rescan_needed = 0U;
            /* No user callbacks occur inside this scan, so this follow-up pass
               cannot set rescan_needed again until control returns to review. */
            continue;
        }
        complete = true;
    } while(ok && app->data.character.grant_count < DND_MAX_GRANTS);

    if(staged_out) *staged_out = staged;
    if(complete_out) *complete_out = ok && complete;
    return ok;
}

static bool dndolphins_stage_character_grants(
    DndDolphinsApp* app,
    uint8_t maximum_level,
    bool include_background,
    uint8_t* staged_out) {
    if(!app || !dndolphins_grant_review_runtime_alloc(app)) return false;
    if(app->features_loaded && !dndolphins_release_features(app)) return false;
    dndolphins_release_staged_records(app);

    uint8_t staged = 0U;
    if(app->grant_review->dependency_scan_needed) {
        uint8_t dependency_staged = 0U;
        bool dependency_complete = false;
        if(!dndolphins_stage_owned_feat_dependencies(app, &dependency_staged, &dependency_complete))
            return false;
        staged = dependency_staged;
        if(dependency_complete) app->grant_review->dependency_scan_needed = 0U;
        if(app->data.character.grant_count >= DND_MAX_GRANTS) {
            if(staged_out) *staged_out = staged;
            return true;
        }
    }
    if(include_background && !app->grant_review->initial_languages_done) {
        if(!dndolphins_stage_initial_languages(app, &staged)) return false;
        app->grant_review->initial_languages_done = 1U;
        if(app->data.character.grant_count >= DND_MAX_GRANTS) {
            if(staged_out) *staged_out = staged;
            return true;
        }
    }
    if(app->grant_review->scan_complete) {
        if(staged_out) *staged_out = staged;
        return true;
    }

    File* file = storage_file_alloc(app->storage);
    if(!file) return false;
    if(!storage_file_open(
           file, dndolphins_active_metadata_path(app), FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_free(file);
        return false;
    }
    if(app->grant_review->metadata_offset &&
       !storage_file_seek(file, app->grant_review->metadata_offset, true)) {
        storage_file_close(file);
        storage_file_free(file);
        return false;
    }

    /* Grant review is an explicit user action. Keep its metadata work to one
       forward pass across review batches: when the bounded 24-row resident
       queue fills, remember the exact byte consumed and resume there after the
       user finishes the batch. Nothing here is called from draw/tick paths. */
    char line[256];
    size_t position = 0U;
    uint8_t buffer[512];
    bool ok = true;
    bool queue_full = false;
    size_t count = 0U;
    uint32_t raw_offset = app->grant_review->metadata_offset;
    DndDolphinsGrantScanCache cache;
    memset(&cache, 0, sizeof(cache));
    while(ok && !queue_full && (count = storage_file_read(file, buffer, sizeof(buffer))) > 0U) {
        for(size_t i = 0U; i < count; ++i) {
            char byte = (char)buffer[i];
            if(raw_offset != UINT32_MAX) ++raw_offset;
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
                   app, line, maximum_level, include_background, &staged, &cache)) {
                ok = false;
                break;
            }
            if(app->data.character.grant_count >= DND_MAX_GRANTS) {
                queue_full = true;
                break;
            }
        }
    }
    if(ok && !queue_full && position && app->data.character.grant_count < DND_MAX_GRANTS) {
        line[position] = '\0';
        ok = dndolphins_stage_character_grant_line(
            app, line, maximum_level, include_background, &staged, &cache);
    }
    if(storage_file_get_error(file) != FSE_OK) ok = false;
    storage_file_close(file);
    storage_file_free(file);
    if(ok) {
        app->grant_review->metadata_offset = raw_offset;
        if(!queue_full) app->grant_review->scan_complete = 1U;
    }
    if(staged_out) *staged_out = staged;
    return ok;
}

static bool dndolphins_grants_have_pending(const DndCharacter* character) {
    if(!character) return false;
    for(uint8_t i = 0U; i < character->grant_count; ++i)
        if(character->grants[i].status == DndGrantPending) return true;
    return false;
}

static bool dndolphins_grants_have_skipped(const DndCharacter* character) {
    if(!character) return false;
    for(uint8_t i = 0U; i < character->grant_count; ++i)
        if(character->grants[i].status == DndGrantSkipped) return true;
    return false;
}

static bool dndolphins_stage_next_grant_review_batch(DndDolphinsApp* app, uint8_t* staged_out) {
    if(!app) return false;
    if(app->grant_review->batches >= 16U) {
        if(staged_out) *staged_out = 0U;
        return app->grant_review->scan_complete != 0U;
    }
    ++app->grant_review->batches;
    return dndolphins_stage_character_grants(
        app,
        app->grant_review->maximum_level,
        app->grant_review->include_background != 0U,
        staged_out);
}

static bool dndolphins_advance_grant_review_if_complete(DndDolphinsApp* app) {
    if(!app || dndolphins_grants_have_pending(&app->data.character) ||
       dndolphins_grants_have_skipped(&app->data.character))
        return false;
    if(!dndolphins_flush_save(app, false)) {
        dndolphins_set_status(app, "Grant save failed");
        return true;
    }
    uint8_t staged = 0U;
    if(!dndolphins_stage_next_grant_review_batch(app, &staged)) {
        dndolphins_set_status(app, "Next grant batch failed");
        return true;
    }
    if(staged) {
        app->selection = 0U;
        app->scroll = 0U;
        snprintf(app->status, sizeof(app->status), "%u more grants to review", staged);
        return true;
    }
    dndolphins_release_staged_records(app);
    app->grant_review->batches = 0U;
    app->grant_review->include_background = 0U;
    app->grant_review->initial_languages_done = 0U;
    app->grant_review->scan_complete = 0U;
    app->grant_review->dependency_scan_needed = 0U;
    app->grant_review->dependency_rescan_needed = 0U;
    app->grant_review->metadata_offset = 0U;
    app->grant_review->dependency_metadata_offset = 0U;
    app->grant_review->maximum_level = 0U;
#if defined(DND_BUILD_GRANTS)
    app->return_to_parent = 1U;
    view_dispatcher_stop(app->dispatcher);
#else
    dndolphins_enter_screen(app, app->return_screen);
    dndolphins_set_status(app, "All reviewed grants applied");
#endif
    return true;
}

static void dndolphins_schedule_deferred_action(DndDolphinsApp* app, DndDeferredAction action) {
    if(!app || action == DndDeferredActionNone || app->deferred_action != DndDeferredActionNone)
        return;
    app->deferred_action = action;
    app->deferred_action_wait_ticks = 1U;
    dndolphins_set_status(app, "Applying");
    dndolphins_refresh(app);
}

static void dndolphins_run_deferred_action(DndDolphinsApp* app) {
#if defined(DND_BUILD_GRANTS)
    if(!app) return;
    DndDeferredAction action = app->deferred_action;
    app->deferred_action = DndDeferredActionNone;
    app->deferred_action_wait_ticks = 0U;

    if(action != DndDeferredActionGrantInitialTraits &&
       action != DndDeferredActionApplyLevelGrants)
        return;

    DndCharacter* character = &app->data.character;
    bool progression_changed = false;
    for(uint8_t i = 0U; i < character->class_count; ++i)
        if(dndolphins_spells_apply_level_progression(character, i)) progression_changed = true;

    const bool initial = action == DndDeferredActionGrantInitialTraits;
    if(!dndolphins_flush_save(app, false)) {
        dndolphins_set_status(app, "Progression save failed");
        return;
    }
    app->grant_review->maximum_level = initial ? 1U : UINT8_MAX;
    app->grant_review->include_background = initial ? 1U : 0U;
    app->grant_review->batches = 0U;
    app->grant_review->initial_languages_done = 0U;
    app->grant_review->scan_complete = 0U;
    app->grant_review->dependency_scan_needed = 0U;
    app->grant_review->dependency_rescan_needed = 0U;
    app->grant_review->metadata_offset = 0U;
    app->grant_review->dependency_metadata_offset = 0U;
    uint8_t staged = 0U;
    if(!dndolphins_stage_next_grant_review_batch(app, &staged)) {
        dndolphins_set_status(
            app, initial ? "Initial grant scan failed" : "Level grant scan failed");
        return;
    }
    if(staged) {
        app->return_screen = DndScreenCharacter;
        dndolphins_enter_screen(app, DndScreenGrantReview);
        snprintf(app->status, sizeof(app->status), "%u grants: OK apply/choose", staged);
        return;
    }
    dndolphins_release_staged_records(app);
    if(progression_changed)
        dndolphins_confirm_action(app, "Progression updated; no new grants");
    else
        dndolphins_confirm_action(app, "No new grants");
#else
    if(!app) return;
    app->deferred_action = DndDeferredActionNone;
    app->deferred_action_wait_ticks = 0U;
#endif
}

static void dndolphins_catalog_load_page(DndDolphinsApp* app) {
    if(!dndolphins_catalog_runtime_alloc(app)) {
        dndolphins_set_status(app, "Catalog memory failed");
        return;
    }
    DndCatalogKind kind = app->catalog->kind;
    const char* selected_path = dndolphins_catalog_path_for_mode(app, kind);
    dndolphins_catalog_release(app);
    app->catalog->scan_count = 0U;
    app->catalog->has_more = 0U;
    if(!selected_path[0] || !storage_file_exists(app->storage, selected_path) ||
       kind == DndCatalogSkillTools)
        dndolphins_catalog_add_builtins(app, kind);
    dndolphins_catalog_load_external(app, kind);
    if(dndolphins_catalog_page_complete(app)) app->catalog->has_more = 1U;
    app->catalog->total = app->catalog->scan_count;
    if(app->catalog->page_start >= app->catalog->total && app->catalog->page_start) {
        uint16_t page_limit = dndolphins_catalog_page_limit(app);
        app->catalog->page_start =
            ((app->catalog->total ? app->catalog->total - 1U : 0U) / page_limit) * page_limit;
        dndolphins_catalog_release(app);
        app->catalog->scan_count = 0U;
        app->catalog->has_more = 0U;
        if(!selected_path[0] || !storage_file_exists(app->storage, selected_path))
            dndolphins_catalog_add_builtins(app, kind);
        dndolphins_catalog_load_external(app, kind);
        if(dndolphins_catalog_page_complete(app)) app->catalog->has_more = 1U;
        app->catalog->total = app->catalog->scan_count;
    }
}

static void dndolphins_open_catalog(
    DndDolphinsApp* app,
    DndCatalogKind kind,
    DndEditTarget target,
    const char* current) {
    dndolphins_release_text_input(app);
    dndolphins_release_number_input(app);
    if(!dndolphins_catalog_runtime_alloc(app)) {
        dndolphins_set_status(app, "Catalog memory failed");
        return;
    }
    app->catalog->kind = kind;
    app->catalog->target = target;
    app->catalog->page_size = (kind == DndCatalogLanguages || kind == DndCatalogProficiencies) ?
                                  DNDOLPHINS_CHARACTER_CATALOG_PAGE :
                                  DNDOLPHINS_MAX_CATALOG_ENTRIES;
    app->return_screen = app->screen;
    app->catalog->return_selection = app->selection;
    app->catalog->show_all = 0U;
    app->catalog->page_start = 0U;
    dndolphins_catalog_load_page(app);
    dndolphins_enter_screen(app, DndScreenCatalog);
    for(uint16_t i = 0U; i < app->catalog->count; ++i) {
        if(strcmp(app->catalog->entries[i], current) == 0) {
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
    DndDolphinsApp* app,
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

static uint16_t dndolphins_list_count(const DndDolphinsApp* app) {
    const DndCharacter* character = &app->data.character;
    switch(app->list_kind) {
    case DndListClasses:
        return character->class_count;
    case DndListFeatures:
        return app->features_total;
    case DndListLanguages:
        return app->language_total;
    case DndListProficiencies:
        return app->proficiency_total;
    default:
        return 0U;
    }
}

static const char* dndolphins_list_title(DndListKind kind) {
    switch(kind) {
    case DndListClasses:
        return "Classes";
    case DndListFeatures:
        return "Features";
    case DndListLanguages:
        return "Languages";
    case DndListProficiencies:
        return "Proficiencies";
    default:
        return "List";
    }
}

static void
    dndolphins_format_list_entry(DndDolphinsApp* app, uint16_t index, char* output, size_t size) {
    DndCharacter* character = &app->data.character;
    switch(app->list_kind) {
    case DndListClasses: {
        const DndClassLevel* class_level = &character->classes[index];
        snprintf(output, size, "%.31s L%u", class_level->name, class_level->level);
        break;
    }
    case DndListFeatures: {
        DndFeature* feature = dndolphins_feature_at_cached(app, index, NULL);
        if(!feature)
            dndolphins_copy(output, size, "<read error>");
        else
            dndolphins_format_labeled_text(output, size, NULL, feature->name);
        break;
    }
    case DndListLanguages: {
        const char* language = dndolphins_language_at_cached(app, index);
        dndolphins_format_labeled_text(output, size, NULL, language ? language : "<read error>");
        break;
    }
    case DndListProficiencies: {
        const DndCharacterProficiency* proficiency = dndolphins_proficiency_at_cached(app, index);
        if(proficiency)
            snprintf(output, size, "%s: %s", proficiency->type, proficiency->name);
        else
            dndolphins_copy(output, size, "<read error>");
        break;
    }
    default:
        dndolphins_copy(output, size, "Unavailable");
        break;
    }
}

static bool dndolphins_refresh_combat_weapon_index(DndDolphinsApp* app) {
    if(!app->combat->combat_weapon_indices) {
        app->combat->combat_weapon_capacity = DND_STORAGE_COLLECTION_CACHE_SIZE;
        app->combat->combat_weapon_indices = malloc(
            app->combat->combat_weapon_capacity * sizeof(uint16_t) +
            DNDOLPHINS_COMBAT_VISIBLE_ROWS * DNDOLPHINS_COMBAT_ROW_LEN);
        if(!app->combat->combat_weapon_indices) return false;
    }
    app->combat->combat_weapon_start = 0U;
    return dndolphins_weapon_combat_items_collect_weapon_indices(
        app->storage,
        app->active_profile,
        0U,
        app->combat->combat_weapon_indices,
        app->combat->combat_weapon_capacity,
        &app->combat->combat_weapon_count,
        &app->items_total);
}

static uint16_t dndolphins_weapon_count(DndDolphinsApp* app) {
    return app->combat->combat_weapon_count;
}

static uint16_t dndolphins_weapon_index(DndDolphinsApp* app, uint16_t weapon_number) {
    if(!app->combat->combat_weapon_indices || weapon_number >= app->combat->combat_weapon_count)
        return UINT16_MAX;
    if(weapon_number < app->combat->combat_weapon_start ||
       weapon_number - app->combat->combat_weapon_start >= app->combat->combat_weapon_capacity) {
        uint16_t start = (weapon_number / DND_STORAGE_COLLECTION_CACHE_SIZE) *
                         DND_STORAGE_COLLECTION_CACHE_SIZE;
        if(!dndolphins_weapon_combat_items_collect_weapon_indices(
               app->storage,
               app->active_profile,
               start,
               app->combat->combat_weapon_indices,
               app->combat->combat_weapon_capacity,
               &app->combat->combat_weapon_count,
               &app->items_total))
            return UINT16_MAX;
        app->combat->combat_weapon_start = start;
    }
    return app->combat->combat_weapon_indices[weapon_number - app->combat->combat_weapon_start];
}

static char* dndolphins_combat_weapon_row(DndDolphinsApp* app, uint8_t visible) {
    if(!app || !app->combat->combat_weapon_indices || visible >= DNDOLPHINS_COMBAT_VISIBLE_ROWS)
        return NULL;
    return (char*)(app->combat->combat_weapon_indices + app->combat->combat_weapon_capacity) +
           ((size_t)visible * DNDOLPHINS_COMBAT_ROW_LEN);
}

static void dndolphins_prepare_combat_weapon_rows(DndDolphinsApp* app) {
    if(!app || !app->combat->combat_weapon_indices) return;
    for(uint8_t visible = 0U; visible < DNDOLPHINS_COMBAT_VISIBLE_ROWS; ++visible) {
        char* row = dndolphins_combat_weapon_row(app, visible);
        if(!row) continue;
        row[0] = '\0';
        uint16_t weapon_number = app->scroll + visible;
        if(weapon_number >= app->combat->combat_weapon_count) continue;
        uint16_t item_index = dndolphins_weapon_index(app, weapon_number);
        if(item_index == UINT16_MAX) continue;
        DndItem* item = dndolphins_item_at(app, item_index, NULL);
        if(!item) continue;
        snprintf(
            row,
            DNDOLPHINS_COMBAT_ROW_LEN,
            "%s %+d %ud%u",
            item->name,
            dnd_weapon_rules_attack_modifier(&app->data.character, item),
            item->damage_dice,
            item->use_versatile ? item->versatile_die : item->damage_die);
    }
}

static uint8_t dndolphins_record_detail_count(const DndDolphinsApp* app) {
    switch(app->list_kind) {
    case DndListClasses:
        return 18U;
    case DndListFeatures:
        return 10U;
    case DndListLanguages:
        return 2U;
    case DndListProficiencies:
        return 3U;
    default:
        return 0U;
    }
}

static void dndolphins_release_text_input(DndDolphinsApp* app) {
    if(!app || app->input_module_active) return;
    if(app->text_input) {
        view_dispatcher_remove_view(app->dispatcher, DndViewTextInput);
        text_input_free(app->text_input);
        app->text_input = NULL;
    }
    free(app->edit_buffer);
    app->edit_buffer = NULL;
}

static void dndolphins_release_number_input(DndDolphinsApp* app) {
    if(!app->number_input || app->input_module_active) return;
    view_dispatcher_remove_view(app->dispatcher, DndViewNumberInput);
    number_input_free(app->number_input);
    app->number_input = NULL;
}

static void dndolphins_begin_text(
    DndDolphinsApp* app,
    DndEditTarget target,
    const char* header,
    const char* initial) {
    dndolphins_release_number_input(app);
    if(!app->edit_buffer) {
        app->edit_buffer = malloc(DND_DETAIL_LEN);
        if(!app->edit_buffer) {
            dndolphins_set_status(app, "Text buffer memory low");
            return;
        }
    }
    if(!app->text_input) {
        app->text_input = text_input_alloc();
        if(!app->text_input) {
            free(app->edit_buffer);
            app->edit_buffer = NULL;
            dndolphins_set_status(app, "Text input memory low");
            return;
        }
        view_dispatcher_add_view(
            app->dispatcher, DndViewTextInput, text_input_get_view(app->text_input));
    }
    app->edit_target = target;
    app->input_module_active = 1U;
    (void)dndolphins_input_hook_acquire(app);
    dndolphins_copy(app->edit_buffer, DND_DETAIL_LEN, initial);
    text_input_reset(app->text_input);
    text_input_set_header_text(app->text_input, header);
    text_input_set_result_callback(
        app->text_input, dndolphins_text_done, app, app->edit_buffer, DND_DETAIL_LEN, false);
    view_dispatcher_switch_to_view(app->dispatcher, DndViewTextInput);
}

static uint8_t dndolphins_nearest_die(int32_t number, bool damage_only) {
    const uint8_t* choices = damage_only ? dndolphins_damage_die_choices : dndolphins_die_choices;
    uint8_t count = damage_only ? sizeof(dndolphins_damage_die_choices) :
                                  sizeof(dndolphins_die_choices);
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

static void dndolphins_begin_quick_roll(
    DndDolphinsApp* app,
    const char* label,
    int16_t modifier,
    bool saving_throw);

static void dndolphins_number_done(void* context, int32_t number) {
    DndDolphinsApp* app = context;
    DndCharacter* character = &app->data.character;
    DndNumberContext completed_context = app->number_context;
    switch(app->number_context) {
    case DndNumberCharacter:
        if(app->number_index == 7U) character->experience = (uint32_t)number;
        break;
    case DndNumberVitals:
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
    case DndNumberAbility:
        if(app->number_index < DND_ABILITY_COUNT) {
            if(app->number_aux)
                character->saving_throw_misc[app->number_index] = (int8_t)number;
            else
                character->ability_scores[app->number_index] = (int8_t)number;
        }
        break;
    case DndNumberSkill:
        if(app->number_index < DND_SKILL_COUNT)
            character->skill_misc[app->number_index] = (int8_t)number;
        break;
#if defined(DND_BUILD_COMBAT)
    case DndNumberMagic:
        if(app->number_index == 3U)
            character->spell_attack_misc = (int8_t)number;
        else if(app->number_index == 4U)
            character->spell_save_misc = (int8_t)number;
        else if(app->number_index >= 8U && app->number_index <= 16U) {
            uint8_t level = app->number_index - 7U;
            uint8_t* slots = app->number_aux ? character->spell_slots_max :
                                               character->spell_slots_current;
            slots[level] = (uint8_t)number;
            if(character->spell_slots_current[level] > character->spell_slots_max[level])
                character->spell_slots_current[level] = character->spell_slots_max[level];
        }
        break;
#else
    case DndNumberMagic:
        break;
#endif
    case DndNumberRecord:
        if(app->record_index >= dndolphins_list_count(app)) break;
        if(app->list_kind == DndListClasses) {
            uint8_t previous_total_level = dnd_rules_core_total_level(character);
            uint8_t previous_pb = dnd_rules_core_proficiency_bonus(character);
            uint8_t previous_slots[DND_SLOT_COUNT];
            memcpy(previous_slots, character->spell_slots_max, sizeof(previous_slots));
            DndClassLevel* level = &character->classes[app->record_index];
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
        } else if(app->list_kind == DndListFeatures) {
            DndFeature* feature = dndolphins_feature_at(app, app->record_index, NULL);
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
    case DndNumberDice:
        if(app->number_index == 0U)
            app->roll->dice_count = (uint8_t)number;
        else if(app->number_index == 1U)
            app->roll->dice_sides = dndolphins_nearest_die(number, false);
        else if(app->number_index == 2U)
            app->roll->dice_modifier = (int16_t)number;
        app->roll->roll_mode = DndRollNormal;
        app->roll->dice_roll_value_count = 0U;
        break;
    case DndNumberCombat:
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
    case DndNumberNone:
        break;
    }
    UNUSED(completed_context);
    app->number_context = DndNumberNone;
    app->input_module_active = 0U;
    dndolphins_input_hook_release(app);
    dndolphins_save(app, false);
    view_dispatcher_switch_to_view(app->dispatcher, DndViewMain);
    view_dispatcher_send_custom_event(app->dispatcher, DNDOLPHINS_RELEASE_INPUT_EVENT);
    dndolphins_refresh(app);
}

static void dndolphins_begin_number(
    DndDolphinsApp* app,
    DndNumberContext context,
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
            app->dispatcher, DndViewNumberInput, number_input_get_view(app->number_input));
    }
    app->number_context = context;
    app->number_index = index;
    app->number_aux = aux;
    app->input_module_active = 1U;
    (void)dndolphins_input_hook_acquire(app);
    number_input_set_header_text(app->number_input, header);
    number_input_set_result_callback(
        app->number_input, dndolphins_number_done, app, value, minimum, maximum);
    view_dispatcher_switch_to_view(app->dispatcher, DndViewNumberInput);
}

static uint16_t dndolphins_home_count(const DndDolphinsApp* app) {
    return (uint16_t)DndolphinsHomeCount + (app->storage_unsaved ? 1U : 0U);
}

static const char* dndolphins_home_item_at(uint16_t index) {
    if(index < (uint16_t)DndolphinsHomeCount) return dndolphins_home_items[index];
    return dndolphins_home_retry_save;
}

#if defined(DND_BUILD_HUB)
static void dndolphins_draw_home_graphical(Canvas* canvas, DndDolphinsApp* app) {
    enum {
        Columns = 4U,
        Rows = 2U,
        PageSize = Columns * Rows
    };
    static const int32_t x_positions[Columns] = {4, 36, 68, 100};
    static const int32_t y_positions[Rows] = {12, 39};

    uint16_t count = dndolphins_home_count(app);
    if(!count) return;
    uint16_t selected = app->selection < count ? app->selection : 0U;
    uint16_t page_start = (uint16_t)((selected / PageSize) * PageSize);
    /* The selected menu name owns the Graphical header; suppress transient status
       text here so long status strings can never obscure the highlighted name. */
    dndolphins_draw_header(canvas, dndolphins_home_item_at(selected), NULL);

    for(uint8_t slot = 0U; slot < PageSize; ++slot) {
        uint16_t index = page_start + slot;
        if(index >= count) break;
        uint8_t column = slot % Columns;
        uint8_t row = slot / Columns;
        int32_t x = x_positions[column];
        int32_t y = y_positions[row];
        if(index == selected) canvas_draw_frame(canvas, x - 2, y - 1, 29, 26);
        dndolphins_menu_graphics_draw_icon(canvas, index, x, y);
    }
}
#endif

static void dndolphins_draw_home(Canvas* canvas, DndDolphinsApp* app) {
#if defined(DND_BUILD_HUB)
    if(app->settings.menu_type == DndMenuTypeGraphical) {
        dndolphins_draw_home_graphical(canvas, app);
        return;
    }
#endif
    char title[48];
    snprintf(title, sizeof(title), "D&D v" DND_RELEASE_VERSION " %.27s", app->data.character.name);
    dndolphins_draw_header(canvas, title, app->status);
    uint16_t count = dndolphins_home_count(app);
    for(uint8_t visible = 0U; visible < 5U; ++visible) {
        uint16_t index = app->scroll + visible;
        if(index >= count) break;
        dndolphins_draw_row(
            canvas, visible, index == app->selection, dndolphins_home_item_at(index));
    }
}

static void dndolphins_draw_profiles(Canvas* canvas, DndDolphinsApp* app) {
    uint16_t count = dndolphins_profile_count(app);
    dndolphins_draw_header(canvas, "Characters - hold OK actions", app->status);
    for(uint8_t visible = 0U; visible < 5U; ++visible) {
        uint16_t index = app->scroll + visible;
        if(index > count) break;
        char row[48];
        if(index == count) {
            dndolphins_copy(row, sizeof(row), "+ New Character");
        } else {
            const DndProfileEntry* entry = dndolphins_profile_entry_cached_at(app, index);
            if(entry) {
                snprintf(
                    row,
                    sizeof(row),
                    "%c #%lu L%u %.24s",
                    entry->id == app->active_profile ? '*' : ' ',
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

static void dndolphins_draw_profile_actions(Canvas* canvas, DndDolphinsApp* app) {
    char title[48];
    snprintf(title, sizeof(title), "Character #%lu", (unsigned long)app->profile_action_id);
    dndolphins_draw_header(canvas, title, app->status);
    dndolphins_draw_menu_rows(
        canvas,
        app,
        dndolphins_profile_actions,
        sizeof(dndolphins_profile_actions) / sizeof(dndolphins_profile_actions[0]));
}

static void dndolphins_draw_settings(Canvas* canvas, DndDolphinsApp* app) {
    char rows[6][48];
    const char* row_ptrs[6] = {rows[0], rows[1], rows[2], rows[3], rows[4], rows[5]};
    snprintf(
        rows[0],
        sizeof(rows[0]),
        "Skip Dice Loading: %s",
        app->settings.skip_dice_loading ? "On" : "Off");
    snprintf(rows[1], sizeof(rows[1]), "Debug: %s", app->settings.debug ? "On" : "Off");
    snprintf(
        rows[2], sizeof(rows[2]), "Get Elevated: %s", app->settings.extra_items ? "420" : "Off");
    snprintf(
        rows[3],
        sizeof(rows[3]),
        "Catalog: %s",
        app->catalog_all_available && app->settings.catalog_all ? "All" : "SRD");
    snprintf(rows[4], sizeof(rows[4]), "Homebrew: %s", app->settings.homebrew ? "Yes" : "No");
    snprintf(
        rows[5],
        sizeof(rows[5]),
        "Menu Type: %s",
        app->settings.menu_type == DndMenuTypeGraphical ? "Graphical" : "Text");
    dndolphins_draw_header(canvas, "Settings", app->status);
    dndolphins_draw_menu_rows(canvas, app, row_ptrs, 6U);
}

static void dndolphins_draw_character(Canvas* canvas, DndDolphinsApp* app) {
    DndCharacter* character = &app->data.character;
    char rows[16][48];
    const char* row_ptrs[16];
    for(uint8_t i = 0U; i < 16U; ++i)
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
    snprintf(rows[9], sizeof(rows[9]), "Languages (%u)", app->language_total);
    snprintf(rows[10], sizeof(rows[10]), "Proficiencies (%u)", app->proficiency_total);
    snprintf(rows[11], sizeof(rows[11]), "Inspiration: %s", character->inspiration ? "Yes" : "No");
    snprintf(rows[12], sizeof(rows[12]), "Level Choices");
    snprintf(rows[13], sizeof(rows[13]), "Grant Review");
    snprintf(rows[14], sizeof(rows[14]), "Character Sheet");
    dndolphins_draw_header(canvas, "Character", app->status);
    dndolphins_draw_menu_rows(canvas, app, row_ptrs, 15U);
}

static void dndolphins_draw_vitals(Canvas* canvas, DndDolphinsApp* app) {
    DndCharacter* character = &app->data.character;
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
        rows[5],
        sizeof(rows[5]),
        "Initiative: %+d",
        dndolphins_rules_character_initiative_modifier(character));
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
    dndolphins_draw_menu_rows(canvas, app, row_ptrs, 17U);
}

static void dndolphins_draw_abilities(Canvas* canvas, DndDolphinsApp* app) {
    DndCharacter* character = &app->data.character;
    char rows[DND_ABILITY_COUNT][48];
    const char* row_ptrs[DND_ABILITY_COUNT];
    for(uint8_t i = 0U; i < DND_ABILITY_COUNT; ++i) {
        row_ptrs[i] = rows[i];
        int16_t check_modifier = dnd_rules_core_ability_modifier(character->ability_scores[i]);
        int16_t save_modifier = dnd_rules_core_saving_throw_modifier(character, i);
        if(app->roll->ability_roll_target == 0U) {
            snprintf(
                rows[i],
                sizeof(rows[i]),
                "[%s %d %+d] Save %s%+d",
                dnd_rules_core_ability_names[i],
                character->ability_scores[i],
                check_modifier,
                dndolphins_proficiency_mark(character->saving_throw_proficiency[i]),
                save_modifier);
        } else {
            snprintf(
                rows[i],
                sizeof(rows[i]),
                "%s %d %+d [Save %s%+d]",
                dnd_rules_core_ability_names[i],
                character->ability_scores[i],
                check_modifier,
                dndolphins_proficiency_mark(character->saving_throw_proficiency[i]),
                save_modifier);
        }
    }
    dndolphins_draw_header(
        canvas,
        app->roll->ability_roll_target ? "< Check | SAVE >" : "< CHECK | Save >",
        app->status);
    dndolphins_draw_menu_rows(canvas, app, row_ptrs, DND_ABILITY_COUNT);
}

static void dndolphins_draw_skills(Canvas* canvas, DndDolphinsApp* app) {
    DndCharacter* character = &app->data.character;
    char title[32];
    snprintf(
        title,
        sizeof(title),
        "Skills PB+%u <> %s OK roll",
        dnd_rules_core_proficiency_bonus(character),
        app->edit_modifier_mode ? "misc" : "prof");
    dndolphins_draw_header(canvas, title, app->status);
    for(uint8_t visible = 0U; visible < 5U; ++visible) {
        uint16_t display_index = app->scroll + visible;
        if(display_index >= DND_SKILL_COUNT) break;
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

static DndGrantChoiceKind dndolphins_grant_choice_kind(const DndGrant* grant) {
    if(!grant || !strstr(grant->grant_value, "Freepick")) return DndGrantChoiceNone;
    if(!strncmp(grant->grant_value, "language=", 9U)) return DndGrantChoiceLanguage;
    if(!strncmp(grant->grant_value, "spell=", 6U)) return DndGrantChoiceSpell;
    if(!strncmp(grant->grant_value, "origin_feat=", 12U) ||
       !strncmp(grant->grant_value, "feat=", 5U))
        return DndGrantChoiceFeat;
    if(!strncmp(grant->grant_value, "skill=", 6U) ||
       !strncmp(grant->grant_value, "expertise=", 10U))
        return DndGrantChoiceSkill;
    if(!strncmp(grant->grant_value, "proficiency=", 12U)) return DndGrantChoiceSkillTool;
    if(!strncmp(grant->grant_value, "tool=", 5U) || !strncmp(grant->grant_value, "armor=", 6U) ||
       !strncmp(grant->grant_value, "weapon=", 7U))
        return DndGrantChoiceProficiency;
    if(!strncmp(grant->grant_value, "size=", 5U)) return DndGrantChoiceSize;
    if(!strncmp(grant->grant_value, "feature=", 8U)) return DndGrantChoiceFeature;
    return DndGrantChoiceNone;
}

static const char* dndolphins_grant_choice_label(DndGrantChoiceKind kind) {
    switch(kind) {
    case DndGrantChoiceLanguage:
        return "Language";
    case DndGrantChoiceSpell:
        return "Spell";
    case DndGrantChoiceFeat:
        return "Feat";
    case DndGrantChoiceSkill:
        return "Skill";
    case DndGrantChoiceSkillTool:
        return "Skill/Tool";
    case DndGrantChoiceProficiency:
        return "Proficiency";
    case DndGrantChoiceSize:
        return "Size";
    case DndGrantChoiceFeature:
        return "Feature";
    default:
        return "Choice";
    }
}

static void dndolphins_open_grant_choice(DndDolphinsApp* app, uint8_t grant_index) {
    if(!app || grant_index >= app->data.character.grant_count) return;
    DndGrant* grant = &app->data.character.grants[grant_index];
    DndGrantChoiceKind kind = dndolphins_grant_choice_kind(grant);
    if(kind == DndGrantChoiceNone) return;
    app->grant_review->grant_choice_active = 1U;
    app->grant_review->grant_choice_index = grant_index;
    app->grant_review->grant_choice_kind = kind;
    DndCatalogKind catalog = DndCatalogLanguages;
    DndEditTarget target = DndEditLanguageName;
    switch(kind) {
    case DndGrantChoiceSpell:
        catalog = DndCatalogSpells;
        break;
    case DndGrantChoiceFeat:
        catalog = DndCatalogFeats;
        target = DndEditFeatureName;
        break;
    case DndGrantChoiceSkill:
        catalog = DndCatalogSkills;
        break;
    case DndGrantChoiceSkillTool:
        catalog = DndCatalogSkillTools;
        break;
    case DndGrantChoiceProficiency:
        catalog = DndCatalogProficiencies;
        target = DndEditProficiencyName;
        break;
    case DndGrantChoiceSize:
        catalog = DndCatalogSizes;
        break;
    case DndGrantChoiceFeature:
        catalog = DndCatalogGrantOptions;
        target = DndEditFeatureName;
        break;
    default:
        break;
    }
    app->return_screen = DndScreenGrantReview;
    if(!dndolphins_catalog_runtime_alloc(app)) {
        app->grant_review->grant_choice_active = 0U;
        app->grant_review->grant_choice_kind = DndGrantChoiceNone;
        dndolphins_set_status(app, "Catalog memory failed");
        return;
    }
    app->catalog->kind = catalog;
    app->catalog->target = target;
    app->catalog->page_start = 0U;
    app->catalog->show_all = 0U;
    app->catalog->return_selection = grant_index + 1U;
    app->selection = 0U;
    app->scroll = 0U;
    dndolphins_catalog_load_page(app);
    dndolphins_enter_screen(app, DndScreenCatalog);
    if(kind == DndGrantChoiceSpell) {
        uint8_t max_level = dndolphins_grant_choice_spell_max_level(app, grant);
        const char* school = dndolphins_grant_choice_spell_school(grant);
        if(school)
            snprintf(app->status, sizeof(app->status), "%s spells up to L%u", school, max_level);
        else
            snprintf(app->status, sizeof(app->status), "Allowed spells up to L%u", max_level);
    } else {
        snprintf(
            app->status, sizeof(app->status), "Choose %s", dndolphins_grant_choice_label(kind));
    }
}

static bool dndolphins_lookup_bundled_spell_path(
    DndDolphinsApp* app,
    const char* path,
    const char* spell_name,
    DndSpell* spell) {
    if(!app || !path || !path[0] || !spell_name || !spell_name[0] || !spell) return false;
    File* file = storage_file_alloc(app->storage);
    if(!file || !storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        if(file) storage_file_free(file);
        return false;
    }
    char line[192];
    size_t position = 0U;
    uint8_t buffer[256];
    bool found = false;
    size_t count = 0U;
    while(!found && (count = storage_file_read(file, buffer, sizeof(buffer))) > 0U) {
        for(size_t i = 0U; i < count; ++i) {
            char byte = (char)buffer[i];
            if(byte != '\n' && position + 1U < sizeof(line)) {
                if(byte != '\r') line[position++] = byte;
                continue;
            }
            line[position] = '\0';
            position = 0U;
            if(!line[0] || line[0] == '#') continue;
            char* fields[6] = {0};
            uint8_t field_count = 0U;
            char* cursor = line;
            while(field_count < 6U) {
                fields[field_count++] = cursor;
                char* separator = strchr(cursor, '|');
                if(!separator) break;
                *separator = '\0';
                cursor = separator + 1U;
            }
            if(field_count < 2U || strcmp(fields[0], spell_name)) continue;
            uint32_t level = 0U;
            if(!dndolphins_parse_u32_strict(fields[1], 9U, &level)) continue;
            memset(spell, 0, sizeof(*spell));
            dndolphins_copy(spell->name, sizeof(spell->name), fields[0]);
            spell->level = (uint8_t)level;
            if(field_count > 3U) dndolphins_copy(spell->school, sizeof(spell->school), fields[3]);
            if(field_count > 4U) spell->ritual = !strcmp(fields[4], "Yes") ? 1U : 0U;
            if(field_count > 5U) dndolphins_copy(spell->source, sizeof(spell->source), fields[5]);
            found = true;
            break;
        }
    }
    storage_file_close(file);
    storage_file_free(file);
    return found;
}

static bool
    dndolphins_lookup_bundled_spell(DndDolphinsApp* app, const char* spell_name, DndSpell* spell) {
    if(dndolphins_lookup_bundled_spell_path(
           app, dndolphins_bundled_catalog_paths[DndCatalogSpells], spell_name, spell))
        return true;
    return dndolphins_lookup_bundled_spell_path(
        app, dndolphins_bundled_catalog_all_paths[DndCatalogSpells], spell_name, spell);
}

typedef struct {
    const char* name;
    uint16_t total;
    bool found;
} DndDolphinsSpellChoiceLookup;

static bool dndolphins_spell_choice_lookup_visitor(
    uint16_t logical_index,
    const DndSpell* spell,
    uint8_t known,
    uint8_t always_prepared,
    uint8_t free_casts_current,
    uint8_t free_casts_max,
    void* context) {
    UNUSED(logical_index);
    UNUSED(always_prepared);
    UNUSED(free_casts_current);
    UNUSED(free_casts_max);
    DndDolphinsSpellChoiceLookup* lookup = context;
    if(!lookup || !spell) return false;
    if(lookup->total < UINT16_MAX) ++lookup->total;
    if(known && lookup->name && !strcmp(spell->name, lookup->name)) {
        lookup->found = true;
        return false;
    }
    return true;
}

static bool dndolphins_apply_grant_choice_selection(DndDolphinsApp* app, const char* selected) {
    DndGrant* grant = dndolphins_active_choice_grant(app);
    if(!grant || !selected || !selected[0]) return false;
    bool applied = false;
    switch(app->grant_review->grant_choice_kind) {
    case DndGrantChoiceLanguage:
        applied = dnd_character_languages_append(app->storage, app->active_profile, selected);
        if(applied) {
            (void)dnd_character_languages_count(
                app->storage, app->active_profile, &app->language_total);
            app->character_collections_changed = true;
        }
        break;
    case DndGrantChoiceSkill: {
        bool expertise = !strncmp(grant->grant_value, "expertise=", 10U);
        for(uint8_t i = 0U; i < DND_SKILL_COUNT; ++i)
            if(!strcmp(selected, dnd_rules_core_skill_names[i])) {
                if(expertise)
                    app->data.character.skill_proficiency[i] = 2U;
                else if(app->data.character.skill_proficiency[i] < 1U)
                    app->data.character.skill_proficiency[i] = 1U;
                applied = true;
                break;
            }
        break;
    }
    case DndGrantChoiceSkillTool: {
        uint8_t metadata = app->catalog->levels[app->selection];
        if(metadata == 4U) {
            for(uint8_t i = 0U; i < DND_SKILL_COUNT; ++i)
                if(!strcmp(selected, dnd_rules_core_skill_names[i])) {
                    if(app->data.character.skill_proficiency[i] < 1U)
                        app->data.character.skill_proficiency[i] = 1U;
                    applied = true;
                    break;
                }
        } else {
            applied = dnd_character_proficiencies_append(
                app->storage, app->active_profile, "Tool", selected);
            if(applied) {
                (void)dnd_character_proficiencies_count(
                    app->storage, app->active_profile, &app->proficiency_total);
                app->character_collections_changed = true;
            }
        }
        break;
    }
    case DndGrantChoiceProficiency: {
        char payload[DND_GRANT_VALUE_LEN];
        dndolphins_copy(payload, sizeof(payload), grant->grant_value);
        char* separator = strchr(payload, '=');
        if(!separator) break;
        *separator = '\0';
        const char* type = !strcmp(payload, "armor")  ? "Armor" :
                           !strcmp(payload, "weapon") ? "Weapon" :
                                                        "Tool";
        uint8_t metadata = app->catalog->levels[app->selection];
        if(metadata && strcmp(type, dndolphins_proficiency_type_name(metadata))) {
            applied = false;
            break;
        }
        applied =
            dnd_character_proficiencies_append(app->storage, app->active_profile, type, selected);
        if(applied) {
            (void)dnd_character_proficiencies_count(
                app->storage, app->active_profile, &app->proficiency_total);
            app->character_collections_changed = true;
        }
        break;
    }
    case DndGrantChoiceSize:
        for(uint8_t i = 0U; i < DndSizeCount; ++i)
            if(!strcmp(selected, dndolphins_size_names[i])) {
                app->data.character.size = i;
                applied = true;
                break;
            }
        break;
    case DndGrantChoiceFeat: {
        bool origin_choice = !strncmp(grant->grant_value, "origin_feat=", 12U);
        if(origin_choice) {
            dndolphins_copy(
                app->data.character.origin_feat,
                sizeof(app->data.character.origin_feat),
                selected);
            applied = true;
        } else {
            DndFeature feature;
            memset(&feature, 0, sizeof(feature));
            dndolphins_copy(feature.name, sizeof(feature.name), selected);
            feature.class_index = grant->class_index;
            feature.class_level_gained = grant->level_gained;
            applied = dndolphins_progression_store_features_append(
                app->storage, app->active_profile, &feature);
            if(applied)
                (void)dndolphins_progression_store_features_count(
                    app->storage, app->active_profile, &app->features_total);
        }
        if(applied)
            dndolphins_stage_or_defer_feat_dependencies(
                app, selected, grant->class_index, grant->level_gained);
        break;
    }
    case DndGrantChoiceFeature: {
        DndFeature feature;
        memset(&feature, 0, sizeof(feature));
        dndolphins_copy(feature.name, sizeof(feature.name), selected);
        feature.class_index = grant->class_index;
        feature.class_level_gained = grant->level_gained;
        applied = dndolphins_progression_store_features_append(
            app->storage, app->active_profile, &feature);
        if(applied) {
            (void)dndolphins_progression_store_features_count(
                app->storage, app->active_profile, &app->features_total);
            dndolphins_stage_or_defer_feat_dependencies(
                app, selected, grant->class_index, grant->level_gained);
        }
        break;
    }
    case DndGrantChoiceSpell: {
        DndSpell spell;
        if(!dndolphins_lookup_bundled_spell(app, selected, &spell)) break;
        dndolphins_copy(spell.grant_name, sizeof(spell.grant_name), grant->option_name);
        dndolphins_copy(spell.stable_id, sizeof(spell.stable_id), grant->stable_id);
        spell.class_index = grant->class_index;
        spell.grant_source = grant->source_type;
        uint8_t always_prepared = 0U;
        uint8_t free_cast = 0U;
        bool wizard_school = dndolphins_grant_choice_spell_school(grant) != NULL;
        bool wizard_learning = !strcmp(grant->option_name, "Wizard");
        if(strstr(grant->option_name, "Magic Initiate")) {
            always_prepared = 1U;
            free_cast = spell.level == 1U ? 1U : 0U;
        } else if(grant->source_type == DndGrantSpecies) {
            always_prepared = 1U;
            free_cast = spell.level ? 1U : 0U;
        } else if(strstr(grant->prerequisites, "Mystic Arcanum")) {
            always_prepared = 1U;
            free_cast = 1U;
        } else if(!wizard_school && !wizard_learning) {
            always_prepared = 1U;
        }
        DndDolphinsSpellChoiceLookup lookup = {.name = selected, .total = 0U, .found = false};
        if(!dnd_storage_visit_spells(
               app->storage,
               app->active_profile,
               dndolphins_spell_choice_lookup_visitor,
               &lookup,
               NULL))
            break;
        if(lookup.found) {
            dndolphins_set_status(app, "Spell already known");
            break;
        }
        applied = dnd_storage_append_spell(
            app->storage,
            app->active_profile,
            &app->data.character,
            &spell,
            1U,
            always_prepared,
            free_cast,
            free_cast);
        if(applied) {
            app->spellbook_total = lookup.total + 1U;
            if(app->collection_cache) app->collection_cache->spell_class_counts_valid = 0U;
        }
        break;
    }
    default:
        break;
    }
    if(applied) {
        grant->status = DndGrantApplied;
        if(grant->stable_id[0])
            (void)dndolphins_progression_store_mark_applied(
                app->storage, app->active_profile, grant->stable_id);
    }
    return applied;
}

static void dndolphins_draw_grant_review(Canvas* canvas, DndDolphinsApp* app) {
    const DndCharacter* c = &app->data.character;
    dndolphins_draw_header(canvas, "Review grants before apply", app->status);
    uint16_t last_row = c->grant_count + 1U + (app->settings.debug ? 1U : 0U);
    for(uint8_t visible = 0U; visible < 5U; ++visible) {
        uint16_t row_index = app->scroll + visible;
        if(row_index > last_row) break;
        char row[64];
        if(row_index == 0U) {
            dndolphins_copy(row, sizeof(row), "Apply all pending");
        } else if(row_index <= c->grant_count) {
            const DndGrant* grant = &c->grants[row_index - 1U];
            char mark = grant->status == DndGrantApplied ? 'A' :
                        grant->status == DndGrantSkipped ? 'S' :
                                                           '?';
            DndGrantChoiceKind choice = dndolphins_grant_choice_kind(grant);
            if(choice != DndGrantChoiceNone && grant->status == DndGrantPending) {
                if(choice == DndGrantChoiceSpell)
                    snprintf(
                        row,
                        sizeof(row),
                        "%c CHOICE %s <=L%u",
                        mark,
                        dndolphins_grant_choice_label(choice),
                        dndolphins_grant_choice_spell_max_level(app, grant));
                else
                    snprintf(
                        row,
                        sizeof(row),
                        "%c CHOICE %s: %.24s",
                        mark,
                        dndolphins_grant_choice_label(choice),
                        grant->option_name);
            } else {
                snprintf(
                    row,
                    sizeof(row),
                    "%c %.28s: %.28s",
                    mark,
                    grant->option_name,
                    grant->grant_value);
            }
        } else if(row_index == c->grant_count + 1U) {
            dndolphins_copy(row, sizeof(row), "+ Add Custom Grant");
        } else {
            dndolphins_copy(row, sizeof(row), "Progression Diagnostics");
        }
        dndolphins_draw_row(canvas, visible, row_index == app->selection, row);
    }
}

static void dndolphins_draw_grant_diagnostics(Canvas* canvas, DndDolphinsApp* app) {
    const DndCharacter* c = &app->data.character;
    uint8_t applied = 0U, pending = 0U, skipped = 0U, choices = 0U;
    for(uint8_t i = 0U; i < c->grant_count; ++i) {
        const DndGrant* grant = &c->grants[i];
        if(grant->status == DndGrantApplied)
            ++applied;
        else if(grant->status == DndGrantSkipped)
            ++skipped;
        else {
            ++pending;
            if(dndolphins_grant_choice_kind(grant) != DndGrantChoiceNone) ++choices;
        }
    }
    dndolphins_draw_header(canvas, "Progression Diagnostics", app->status);
    char rows[5][64];
    snprintf(rows[0], sizeof(rows[0]), "Applied: %u", applied);
    snprintf(rows[1], sizeof(rows[1]), "Pending: %u", pending);
    snprintf(rows[2], sizeof(rows[2]), "Pending choices: %u", choices);
    snprintf(rows[3], sizeof(rows[3]), "Skipped/review: %u", skipped);
    if(c->grant_count && app->selection < c->grant_count) {
        const DndGrant* g = &c->grants[app->selection];
        const char* reason = g->status == DndGrantApplied ? "Applied" :
                             g->status == DndGrantSkipped ? "Skipped/manual review" :
                             dndolphins_grant_choice_kind(g) != DndGrantChoiceNone ?
                                                            "Pending choice" :
                                                            "Pending apply";
        snprintf(rows[4], sizeof(rows[4]), "%.16s: %s", g->option_name, reason);
    } else {
        dndolphins_copy(rows[4], sizeof(rows[4]), "Up/Down inspect grants");
    }
    for(uint8_t row = 0U; row < 5U; ++row)
        dndolphins_draw_row(canvas, row, row == 4U, rows[row]);
}

static void dndolphins_draw_level_review(Canvas* canvas, DndDolphinsApp* app) {
    const DndCharacter* character = &app->data.character;
    const char* class_name = app->level_review_class_index < character->class_count ?
                                 character->classes[app->level_review_class_index].name :
                                 "Class";
    char title[48];
    snprintf(
        title,
        sizeof(title),
        "%.13s L%u -> L%u",
        class_name,
        app->level_review_old_level,
        app->level_review_new_level);
    dndolphins_draw_header(canvas, title, "OK next / Hold grants");

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
    snprintf(
        rows[2],
        sizeof(rows[2]),
        "Spell slots: %s",
        app->level_review_slots_changed ? "updated" : "unchanged");
    snprintf(rows[3], sizeof(rows[3]), "HP/HD advanced; grants explicit");
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

static void dndolphins_draw_level_choice(Canvas* canvas, DndDolphinsApp* app) {
    if(!app->level_choice_level) {
        dndolphins_draw_header(canvas, "Level Choices", app->status);
        dndolphins_draw_row(canvas, 0U, true, "No pending choices");
        dndolphins_draw_row(canvas, 1U, false, "OK: Back");
        return;
    }
    char title[48];
    const DndCharacter* c = &app->data.character;
    const char* class_name = app->level_choice_class_index < c->class_count ?
                                 c->classes[app->level_choice_class_index].name :
                                 "Class";
    snprintf(title, sizeof(title), "%.18s L%u choice", class_name, app->level_choice_level);
    dndolphins_draw_header(canvas, title, app->status);
    static const char* const rows[] = {
        "ASI +2 one ability", "ASI +1 two abilities", "Choose Feat", "Later"};
    dndolphins_draw_menu_rows(canvas, app, rows, 4U);
}

static void dndolphins_draw_asi_ability(Canvas* canvas, DndDolphinsApp* app) {
    DndCharacter* c = &app->data.character;
    char rows[DND_ABILITY_COUNT][32];
    const char* ptrs[DND_ABILITY_COUNT];
    for(uint8_t i = 0U; i < DND_ABILITY_COUNT; ++i) {
        snprintf(
            rows[i],
            sizeof(rows[i]),
            "%s: %d",
            dnd_rules_core_ability_names[i],
            c->ability_scores[i]);
        ptrs[i] = rows[i];
    }
    dndolphins_draw_header(
        canvas,
        app->level_choice_mode == 1U                        ? "ASI +2: choose ability" :
        app->level_choice_first_ability < DND_ABILITY_COUNT ? "ASI +1: second ability" :
                                                              "ASI +1: first ability",
        app->status);
    dndolphins_draw_menu_rows(canvas, app, ptrs, DND_ABILITY_COUNT);
}

static void dndolphins_draw_grant_edit(Canvas* canvas, DndDolphinsApp* app) {
    if(app->record_index >= app->data.character.grant_count) return;
    const DndGrant* grant = &app->data.character.grants[app->record_index];
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
        grant->status == DndGrantApplied ? "Applied" :
        grant->status == DndGrantSkipped ? "Skipped" :
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

static int8_t dndolphins_attack_template_ability_modifier(
    const DndCharacter* character,
    const DndAttackTemplate* attack) {
    if(!character || !attack || attack->ability >= DND_ABILITY_COUNT) return 0;
    return dnd_rules_core_ability_modifier(character->ability_scores[attack->ability]);
}

static int8_t dndolphins_unarmed_attack_modifier(
    const DndCharacter* character,
    const DndAttackTemplate* attack) {
    int16_t value = dndolphins_attack_template_ability_modifier(character, attack) +
                    dnd_rules_core_proficiency_bonus(character) + attack->attack_misc +
                    dnd_rules_core_exhaustion_penalty(character);
    return (int8_t)dndolphins_clamp_i16(value, -30, 30);
}

static int16_t
    dndolphins_unarmed_damage(const DndCharacter* character, const DndAttackTemplate* attack) {
    int16_t value = 1 + dndolphins_attack_template_ability_modifier(character, attack);
    return value > 0 ? value : 0;
}

static uint8_t
    dndolphins_unarmed_save_dc(const DndCharacter* character, const DndAttackTemplate* attack) {
    int16_t value = 8 + dndolphins_attack_template_ability_modifier(character, attack) +
                    dnd_rules_core_proficiency_bonus(character);
    return (uint8_t)dndolphins_clamp_i16(value, 0, 30);
}

typedef enum {
    DndAttackTemplateDisplayTemplate,
    DndAttackTemplateDisplayNew,
} DndAttackTemplateDisplayKind;

static uint16_t dndolphins_attack_template_display_count(const DndCharacter* character) {
    return character ? (uint16_t)character->attack_template_count + 1U : 1U;
}

static DndAttackTemplateDisplayKind dndolphins_attack_template_display_kind(
    const DndCharacter* character,
    uint16_t display_index,
    uint16_t* template_index) {
    if(template_index) *template_index = UINT16_MAX;
    if(character && display_index < character->attack_template_count) {
        if(template_index) *template_index = display_index;
        return DndAttackTemplateDisplayTemplate;
    }
    return DndAttackTemplateDisplayNew;
}

static void dndolphins_draw_attack_templates(Canvas* canvas, DndDolphinsApp* app) {
    const DndCharacter* c = &app->data.character;
    dndolphins_draw_header(canvas, "Attack Templates: OK use", app->status);
    uint16_t total = dndolphins_attack_template_display_count(c);
    for(uint8_t visible = 0U; visible < 5U; ++visible) {
        uint16_t display_index = app->scroll + visible;
        if(display_index >= total) break;
        uint16_t template_index = UINT16_MAX;
        DndAttackTemplateDisplayKind kind =
            dndolphins_attack_template_display_kind(c, display_index, &template_index);
        char row[48];
        if(kind == DndAttackTemplateDisplayNew) {
            snprintf(row, sizeof(row), "+ New Attack Template");
        } else if(template_index < c->attack_template_count) {
            const DndAttackTemplate* attack = &c->attack_templates[template_index];
            if(attack->type == DndAttackTemplateUnarmed) {
                snprintf(
                    row,
                    sizeof(row),
                    "%.20s Atk%+d Dmg%d",
                    attack->name,
                    dndolphins_unarmed_attack_modifier(c, attack),
                    dndolphins_unarmed_damage(c, attack));
            } else if(
                attack->type == DndAttackTemplateGrapple ||
                attack->type == DndAttackTemplateShove) {
                snprintf(
                    row,
                    sizeof(row),
                    "%.20s STR/DEX DC %u",
                    attack->name,
                    dndolphins_unarmed_save_dc(c, attack));
            } else {
                snprintf(
                    row,
                    sizeof(row),
                    "%.30s %ud%u+%d",
                    attack->name,
                    attack->damage_dice,
                    attack->damage_die,
                    attack->attack_misc);
            }
        } else {
            snprintf(row, sizeof(row), "Unavailable");
        }
        dndolphins_draw_row(canvas, visible, display_index == app->selection, row);
    }
}

static void dndolphins_draw_attack_template_edit(Canvas* canvas, DndDolphinsApp* app) {
    if(app->record_index >= app->data.character.attack_template_count) return;
    const DndAttackTemplate* attack = &app->data.character.attack_templates[app->record_index];
    char type[32], ability[32], save[32], attack_misc[24], dc[24], damage_dice[24], damage_die[24],
        rider_dice[24], rider_die[24], recharge[32];
    snprintf(type, sizeof(type), "Type: %s", dndolphins_attack_template_type_names[attack->type]);
    snprintf(
        ability, sizeof(ability), "Ability: %s", dnd_rules_core_ability_names[attack->ability]);
    snprintf(save, sizeof(save), "Save: %s", dnd_rules_core_ability_names[attack->save_ability]);
    snprintf(attack_misc, sizeof(attack_misc), "Attack misc: %+d", attack->attack_misc);
    snprintf(dc, sizeof(dc), "Save DC: %u", attack->save_dc);
    snprintf(damage_dice, sizeof(damage_dice), "Damage dice: %u", attack->damage_dice);
    snprintf(damage_die, sizeof(damage_die), "Damage die: d%u", attack->damage_die);
    snprintf(rider_dice, sizeof(rider_dice), "Rider dice: %u", attack->rider_dice);
    snprintf(rider_die, sizeof(rider_die), "Rider die: d%u", attack->rider_die);
    snprintf(
        recharge, sizeof(recharge), "Recharge: %s", dndolphins_recharge_names[attack->recharge]);
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

#if defined(DND_BUILD_COMBAT)
static uint16_t dndolphins_class_knowable_spell_count(
    const DndClassLevel* class_level,
    uint16_t granted_count) {
    if(!class_level || class_level->spellcasting_mode == DndSpellcastingNone) return granted_count;
    uint16_t base = class_level->cantrip_limit;
    if(!strcmp(class_level->name, "Wizard"))
        base += class_level->spellbook_size;
    else
        base += class_level->prepared_limit;
    return base + granted_count;
}

static void dndolphins_spell_totals(
    DndDolphinsApp* app,
    uint16_t* known,
    uint16_t* knowable,
    uint16_t* granted) {
    if(known) *known = 0U;
    if(knowable) *knowable = 0U;
    if(granted) *granted = 0U;
    if(!app || !app->collection_cache || !app->collection_cache->spell_class_counts_valid) return;
    for(uint8_t i = 0U; i < app->data.character.class_count; ++i) {
        uint16_t class_known = app->collection_cache->spell_class_counts.known[i];
        uint16_t class_granted = app->collection_cache->spell_class_counts.granted[i];
        if(known) *known += class_known;
        if(granted) *granted += class_granted;
        if(knowable)
            *knowable += dndolphins_class_knowable_spell_count(
                &app->data.character.classes[i], class_granted);
    }
}

static void dndolphins_draw_magic(Canvas* canvas, DndDolphinsApp* app) {
    DndCharacter* character = &app->data.character;
    char rows[17][48];
    const char* row_ptrs[17];
    for(uint8_t i = 0U; i < 17U; ++i)
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
    uint16_t known_total = 0U, knowable_total = 0U, granted_total = 0U;
    dndolphins_spell_totals(app, &known_total, &knowable_total, &granted_total);
    snprintf(
        rows[5],
        sizeof(rows[5]),
        "Known %u / knowable %u (free %u)",
        known_total,
        knowable_total,
        granted_total);
    snprintf(rows[6], sizeof(rows[6]), "Slots: <> avail / hold <> max");
    uint8_t wizard_level = dndolphins_wizard_level(character);
    if(app->combat->arcane_recovery_active)
        snprintf(
            rows[7],
            sizeof(rows[7]),
            "Arcane Recovery: %u left",
            app->combat->arcane_recovery_budget - app->combat->arcane_recovery_spent);
    else if(!wizard_level)
        snprintf(rows[7], sizeof(rows[7]), "Arcane Recovery: no Wizard");
    else
        snprintf(
            rows[7],
            sizeof(rows[7]),
            "Arcane Recovery: %s",
            character->arcane_recovery_used ? "Used" : "Ready");
    for(uint8_t level = 1U; level <= 9U; ++level) {
        snprintf(
            rows[level + 7U],
            sizeof(rows[level + 6U]),
            "Level %u slots: %u/%u",
            level,
            character->spell_slots_current[level],
            character->spell_slots_max[level]);
    }
    dndolphins_draw_header(
        canvas,
        app->combat->arcane_recovery_active ? "Magic: Arcane Recovery" : "Magic",
        app->status);
    dndolphins_draw_menu_rows(canvas, app, row_ptrs, 16U);
}

#endif

static void dndolphins_draw_catalog(Canvas* canvas, DndDolphinsApp* app) {
    char page[24];
    uint16_t page_number = app->catalog->page_start / dndolphins_catalog_page_limit(app) + 1U;
    snprintf(page, sizeof(page), "Page %u%s <>", page_number, app->catalog->has_more ? "+" : "");
    dndolphins_draw_header(
        canvas, dndolphins_catalog_title(app), app->status[0] ? app->status : page);
    if(app->catalog->count == 0U) {
        dndolphins_draw_row(canvas, 0U, false, "Catalog is empty");
        return;
    }
    for(uint8_t visible = 0U; visible < 5U; ++visible) {
        uint16_t index = app->scroll + visible;
        if(index >= app->catalog->count) break;
        char row[48];
        dndolphins_copy(row, sizeof(row), app->catalog->entries[index]);
        dndolphins_draw_row(canvas, visible, index == app->selection, row);
    }
}

static void dndolphins_draw_record_list(Canvas* canvas, DndDolphinsApp* app) {
    uint16_t count = dndolphins_list_count(app);
    char page[16];
    page[0] = '\0';
    if(count > DND_CHARACTER_COLLECTION_WINDOW && app->list_kind != DndListClasses) {
        uint16_t logical = app->selection ? app->selection - 1U : 0U;
        if(logical >= count) logical = count - 1U;
        uint16_t page_number = logical / DND_CHARACTER_COLLECTION_WINDOW + 1U;
        snprintf(page, sizeof(page), "Pg%u<>", page_number);
    }
    dndolphins_draw_header(
        canvas, dndolphins_list_title(app->list_kind), app->status[0] ? app->status : page);
    for(uint8_t visible = 0U; visible < 5U; ++visible) {
        uint16_t index = app->scroll + visible;
        if(index >= count + 1U) break;
        char row[64];
        if(index == 0U)
            snprintf(
                row,
                sizeof(row),
                "%s",
                app->list_kind == DndListLanguages     ? "+ New Language" :
                app->list_kind == DndListProficiencies ? "+ New Proficiency" :
                                                         "+ Add New");
        else
            dndolphins_format_list_entry(app, index - 1U, row, sizeof(row));
        if(app->action_ack_active && app->action_ack_screen == (uint8_t)app->screen &&
           app->action_ack_selection == index)
            dndolphins_prefix_action_mark(row, sizeof(row));
        dndolphins_draw_row(canvas, visible, index == app->selection, row);
    }
}

static void
    dndolphins_format_record_detail(DndDolphinsApp* app, uint8_t field, char* output, size_t size) {
    DndCharacter* character = &app->data.character;
    uint16_t index = app->record_index;
    if(app->list_kind == DndListClasses) {
        const DndClassLevel* c = &character->classes[index];
        if(field == 0U)
            dndolphins_format_labeled_text(output, size, "Name: ", c->name);
        else if(field == 1U)
            dndolphins_format_labeled_text(output, size, "Subclass: ", c->subclass);
        else if(field == 2U)
            snprintf(output, size, "Class level: %u", c->level);
        else if(field == 3U)
            snprintf(output, size, "Hit Point Die: d%u", c->hit_die);
        else if(field == 4U)
            snprintf(output, size, "Class Hit Dice: %u/%u", c->hit_dice_current, c->hit_dice_max);
        else if(field == 5U)
            snprintf(output, size, "Class Hit Dice max: %u", c->hit_dice_max);
        else if(field == 6U)
            dndolphins_format_labeled_text(
                output,
                size,
                "Casting mode: ",
                dndolphins_spellcasting_mode_names[c->spellcasting_mode]);
        else if(field == 7U)
            dndolphins_format_labeled_text(
                output,
                size,
                "Casting ability: ",
                dnd_rules_core_ability_names[c->spellcasting_ability]);
        else if(field == 8U)
            snprintf(output, size, "Cantrip limit: %u", c->cantrip_limit);
        else if(field == 9U)
            snprintf(
                output,
                size,
                "Known %u Prep %u/%u",
                dndolphins_class_known_count_cached(app, index),
                dndolphins_class_prepared_count_cached(app, index),
                c->prepared_limit);
        else if(field == 10U)
            snprintf(output, size, "Spellbook size: %u", c->spellbook_size);
        else if(field == 11U)
            snprintf(output, size, "Pact slot level: %u", c->pact_slot_level);
        else if(field == 12U)
            snprintf(output, size, "Pact slots: %u/%u", c->pact_slots_current, c->pact_slots_max);
        else if(field == 13U)
            snprintf(output, size, "Pact slots max: %u", c->pact_slots_max);
        else if(field == 14U)
            snprintf(output, size, "Mystic Arcanum: 0x%X", c->mystic_arcanum_mask);
        else if(field == 15U)
            snprintf(
                output, size, "Spell points: %u/%u", c->spell_points_current, c->spell_points_max);
        else if(field == 16U)
            snprintf(output, size, "Spell points max: %u", c->spell_points_max);
        else
            dndolphins_copy(output, size, "Delete class");
    } else if(app->list_kind == DndListFeatures) {
        const DndFeature* f = dndolphins_feature_at_cached(app, index, NULL);
        if(!f) {
            dndolphins_copy(output, size, "Read error");
            return;
        }
        const char* cn = f->class_index < character->class_count ?
                             character->classes[f->class_index].name :
                             "General";
        if(field == 0U)
            dndolphins_format_labeled_text(output, size, "Name: ", f->name);
        else if(field == 1U)
            dndolphins_format_labeled_text(output, size, "Notes: ", f->detail);
        else if(field == 2U)
            dndolphins_format_labeled_text(output, size, "Source class: ", cn);
        else if(field == 3U)
            snprintf(output, size, "Gained at class L%u", f->class_level_gained);
        else if(field == 4U)
            snprintf(output, size, "Uses: %d/%d", f->uses_current, f->uses_max);
        else if(field == 5U)
            snprintf(output, size, "Maximum uses: %d", f->uses_max);
        else if(field == 6U)
            snprintf(output, size, "Recharge: %s", dndolphins_recharge_names[f->recharge]);
        else if(field == 7U)
            snprintf(
                output,
                size,
                "Resource formula: %s",
                dndolphins_resource_formula_names[f->resource_formula]);
        else if(field == 8U)
            snprintf(
                output,
                size,
                "Resource ability: %s",
                dnd_rules_core_ability_names[f->resource_ability]);
        else
            dndolphins_copy(output, size, "Delete feature");
    } else if(app->list_kind == DndListLanguages) {
        const char* language = dndolphins_language_at_cached(app, index);
        if(field == 0U)
            dndolphins_format_labeled_text(
                output, size, "Language: ", language ? language : "<read error>");
        else
            dndolphins_copy(output, size, "Delete language");
    } else if(app->list_kind == DndListProficiencies) {
        const DndCharacterProficiency* proficiency = dndolphins_proficiency_at_cached(app, index);
        if(!proficiency) {
            dndolphins_copy(output, size, "Read error");
            return;
        }
        if(field == 0U)
            dndolphins_format_labeled_text(output, size, "Type: ", proficiency->type);
        else if(field == 1U)
            dndolphins_format_labeled_text(output, size, "Proficiency: ", proficiency->name);
        else
            dndolphins_copy(output, size, "Delete proficiency");
    } else
        dndolphins_copy(output, size, "Unavailable");
}

static void dndolphins_draw_record_detail(Canvas* canvas, DndDolphinsApp* app) {
    uint8_t count = dndolphins_record_detail_count(app);
    dndolphins_draw_header(canvas, dndolphins_list_title(app->list_kind), app->status);
    for(uint8_t visible = 0U; visible < 5U; ++visible) {
        uint16_t field = app->scroll + visible;
        if(field >= count) break;
        char row[DND_DETAIL_LEN + 32U];
        dndolphins_format_record_detail(app, (uint8_t)field, row, sizeof(row));
        dndolphins_draw_row(canvas, visible, field == app->selection, row);
    }
}

static void
    dndolphins_format_combat_row(DndDolphinsApp* app, uint8_t index, char* row, size_t size) {
    DndCharacter* character = &app->data.character;
    switch((DndolphinsCombatIndex)index) {
    case DndolphinsCombatAttackMode:
        snprintf(row, size, "Attack mode: %s", dndolphins_roll_mode_names[app->roll->roll_mode]);
        break;
    case DndolphinsCombatWeaponAttacks:
        dndolphins_copy(row, size, "Weapon Attacks");
        break;
    case DndolphinsCombatSpellAttacks:
        dndolphins_copy(row, size, "Spell Attacks");
        break;
    case DndolphinsCombatFavoriteSpells:
        dndolphins_copy(row, size, "Favorite Spells");
        break;
    case DndolphinsCombatSpellcastingStats:
        snprintf(
            row,
            size,
            "Spell %.3s Atk%+d DC%d",
            dnd_rules_core_ability_names[character->spellcasting_ability],
            dndolphins_spells_attack_modifier(character),
            dndolphins_spells_save_dc(character));
        break;
    case DndolphinsCombatUtilitySpells:
        dndolphins_copy(row, size, "Combat Utility Spells");
        break;
    case DndolphinsCombatRituals:
        dndolphins_copy(row, size, "Rituals");
        break;
    case DndolphinsCombatAttackTemplates:
        snprintf(row, size, "Attack Templates (%u)", character->attack_template_count);
        break;
    case DndolphinsCombatJumpToInitiative:
        dndolphins_copy(row, size, "Jump to Initiative");
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
            character->classes[app->combat->hit_die_class_index].name,
            character->classes[app->combat->hit_die_class_index].hit_die,
            character->classes[app->combat->hit_die_class_index].hit_dice_current,
            character->classes[app->combat->hit_die_class_index].hit_dice_max);
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
    if(index <= DndolphinsCombatJumpToInitiative) return "Combat: Attacks";
    if(index <= DndolphinsCombatLongRest) return "Combat: Recovery";
    if(index <= DndolphinsCombatTemporaryEffects) return "Combat: Status";
    return "Combat: Defenses";
}

static void dndolphins_draw_combat(Canvas* canvas, DndDolphinsApp* app) {
    dndolphins_draw_header(canvas, dndolphins_combat_section(app->selection), app->status);
    for(uint8_t visible = 0U; visible < 5U; ++visible) {
        uint16_t index = app->scroll + visible;
        if(index >= DndolphinsCombatCount) break;
        char row[48];
        dndolphins_format_combat_row(app, (uint8_t)index, row, sizeof(row));
        dndolphins_draw_row(canvas, visible, index == app->selection, row);
    }
}

static void dndolphins_begin_quick_roll(
    DndDolphinsApp* app,
    const char* label,
    int16_t modifier,
    bool saving_throw) {
    if(!dndolphins_roll_runtime_alloc(app)) {
        dndolphins_set_status(app, "Roll memory unavailable");
        return;
    }
    if(!app) return;
    app->roll->quick_roll_active = 1U;
    app->roll->quick_roll_is_save = saving_throw ? 1U : 0U;
    dndolphins_copy(app->roll->quick_roll_label, sizeof(app->roll->quick_roll_label), label);
    app->roll->dice_count = 1U;
    app->roll->dice_sides = 20U;
    app->roll->dice_modifier = modifier;
    app->roll->roll_mode = DndRollNormal;
    app->roll->dice_result = 0;
    app->roll->dice_first = 0U;
    app->roll->dice_second = 0U;
    app->roll->dice_guidance = 0U;
    app->roll->dice_roll_value_count = 0U;
    app->selection = 3U; /* Mode first: Normal / Advantage / Disadvantage / Guidance. */
    app->scroll = 0U;
    dndolphins_enter_screen(app, DndScreenDice);
}

static void dndolphins_draw_dice(Canvas* canvas, DndDolphinsApp* app) {
    char rows[5][48];
    const char* row_ptrs[5];
    for(uint8_t i = 0U; i < 5U; ++i)
        row_ptrs[i] = rows[i];
    snprintf(rows[0], sizeof(rows[0]), "Dice count: %u", app->roll->dice_count);
    snprintf(rows[1], sizeof(rows[1]), "Die: d%u", app->roll->dice_sides);
    snprintf(rows[2], sizeof(rows[2]), "Modifier: %+d", app->roll->dice_modifier);
    snprintf(
        rows[3], sizeof(rows[3]), "Mode: %s", dndolphins_roll_mode_names[app->roll->roll_mode]);
    if(app->roll->dice_roll_value_count && app->roll->dice_guidance) {
        snprintf(
            rows[4],
            sizeof(rows[4]),
            "Roll %u + Guidance %u = %d",
            app->roll->dice_first,
            app->roll->dice_guidance,
            app->roll->dice_result);
    } else if(app->roll->dice_roll_value_count && app->roll->dice_second) {
        snprintf(
            rows[4],
            sizeof(rows[4]),
            "Roll %u/%u = %d",
            app->roll->dice_first,
            app->roll->dice_second,
            app->roll->dice_result);
    } else if(app->roll->dice_roll_value_count) {
        snprintf(rows[4], sizeof(rows[4]), "Roll result: %d", app->roll->dice_result);
    } else {
        snprintf(rows[4], sizeof(rows[4]), "Roll now");
    }
    dndolphins_draw_header(
        canvas,
        app->roll->quick_roll_active ? app->roll->quick_roll_label : "Dice Roller",
        app->status);
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
    for(uint8_t i = 0U; i < count && start + i < DNDOLPHINS_MAX_GENERIC_ROLLS; ++i) {
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

static void dndolphins_draw_dice_result(Canvas* canvas, DndDolphinsApp* app) {
    char title[48];
    char rows[5][48];
    const char* row_ptrs[5];
    for(uint8_t i = 0U; i < 5U; ++i) {
        rows[i][0] = '\0';
        row_ptrs[i] = rows[i];
    }

    if(app->roll->dice_guidance) {
        snprintf(title, sizeof(title), "Guidance d20+d4 - total %d", app->roll->dice_result);
        snprintf(rows[0], sizeof(rows[0]), "d20: %u", app->roll->dice_first);
        snprintf(rows[1], sizeof(rows[1]), "Guidance d4: %u", app->roll->dice_guidance);
        snprintf(rows[2], sizeof(rows[2]), "Dice sum: %u", app->roll->dice_roll_sum);
        snprintf(rows[3], sizeof(rows[3]), "Modifier: %+d", app->roll->dice_modifier);
        snprintf(rows[4], sizeof(rows[4]), "Total: %d (OK reroll)", app->roll->dice_result);
    } else if(app->roll->dice_second) {
        uint8_t chosen =
            app->roll->roll_mode == DndRollAdvantage ?
                (app->roll->dice_first > app->roll->dice_second ? app->roll->dice_first :
                                                                  app->roll->dice_second) :
                (app->roll->dice_first < app->roll->dice_second ? app->roll->dice_first :
                                                                  app->roll->dice_second);
        snprintf(
            title,
            sizeof(title),
            "%s d20 - total %d",
            app->roll->roll_mode == DndRollAdvantage ? "Advantage" : "Disadvantage",
            app->roll->dice_result);
        snprintf(
            rows[0],
            sizeof(rows[0]),
            "Rolls: %u, %u",
            app->roll->dice_first,
            app->roll->dice_second);
        snprintf(rows[1], sizeof(rows[1]), "Dice sum: %u", app->roll->dice_roll_sum);
        snprintf(rows[2], sizeof(rows[2]), "Chosen: %u", chosen);
        snprintf(rows[3], sizeof(rows[3]), "Modifier: %+d", app->roll->dice_modifier);
        snprintf(rows[4], sizeof(rows[4]), "Total: %d (OK reroll)", app->roll->dice_result);
    } else if(app->roll->dice_roll_value_count > 1U) {
        snprintf(
            title,
            sizeof(title),
            "%ud%u sum %u total %d",
            app->roll->dice_roll_value_count,
            app->roll->dice_sides,
            app->roll->dice_roll_sum,
            app->roll->dice_result);
        for(uint8_t row = 0U; row < 5U; ++row) {
            uint8_t start = row * 4U;
            if(start >= app->roll->dice_roll_value_count) break;
            uint8_t remaining = app->roll->dice_roll_value_count - start;
            dndolphins_format_roll_row(
                rows[row],
                sizeof(rows[row]),
                app->roll->dice_roll_values,
                start,
                remaining > 4U ? 4U : remaining);
        }
    } else {
        snprintf(
            title, sizeof(title), "d%u total %d", app->roll->dice_sides, app->roll->dice_result);
        snprintf(rows[0], sizeof(rows[0]), "Roll: %u", app->roll->dice_first);
        snprintf(rows[1], sizeof(rows[1]), "Dice sum: %u", app->roll->dice_roll_sum);
        snprintf(rows[2], sizeof(rows[2]), "Modifier: %+d", app->roll->dice_modifier);
        snprintf(rows[4], sizeof(rows[4]), "OK to reroll");
    }
    dndolphins_draw_header(canvas, title, app->status);
    for(uint8_t row = 0U; row < 5U; ++row) {
        if(rows[row][0]) dndolphins_draw_row(canvas, row, false, row_ptrs[row]);
    }
}

static uint8_t dndolphins_spell_casting_ability(DndDolphinsApp* app, uint16_t logical_index) {
    DndSpell* spell = dndolphins_spell_at(app, logical_index, NULL);
    return dndolphins_spells_casting_ability_for(&app->data.character, spell);
}

static int8_t dndolphins_spell_attack_modifier_for(DndDolphinsApp* app, uint16_t logical_index) {
    DndSpell* spell = dndolphins_spell_at(app, logical_index, NULL);
    return dndolphins_spells_attack_modifier_for(&app->data.character, spell);
}

static int8_t
    dndolphins_spell_attack_modifier_cached_for(DndDolphinsApp* app, uint16_t logical_index) {
    DndSpell* spell = dndolphins_spell_cached_at(app, logical_index, NULL);
    return dndolphins_spells_attack_modifier_for(&app->data.character, spell);
}

static int8_t dndolphins_spell_save_dc_cached_for(DndDolphinsApp* app, uint16_t logical_index) {
    DndSpell* spell = dndolphins_spell_cached_at(app, logical_index, NULL);
    return dndolphins_spells_save_dc_for(&app->data.character, spell);
}

static uint8_t dndolphins_build_spell_cast_options(
    DndDolphinsApp* app,
    uint16_t logical_index,
    DndSpellCastOption* options,
    uint8_t capacity) {
    uint8_t local = 0U;
    DndSpell* spell = dndolphins_spell_at(app, logical_index, &local);
    if(!spell) return 0U;
    DndCharacter* character = &app->data.character;
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
    DndDolphinsApp* app,
    uint16_t logical_index,
    DndSpellCastOption* options,
    uint8_t capacity) {
    uint8_t local = 0U;
    DndSpell* spell = dndolphins_spell_cached_at(app, logical_index, &local);
    if(!spell) return 0U;
    DndCharacter* character = &app->data.character;
    return dndolphins_spells_build_cast_options(
        character,
        spell,
        character->spell_known[local],
        character->spell_always_prepared[local],
        character->spell_free_casts_current[local],
        options,
        capacity);
}

typedef struct {
    uint16_t* indices;
    uint16_t start;
    uint16_t capacity;
    uint16_t count;
} DndFavoriteSpellIndexContext;

static bool dndolphins_favorite_spell_index_visitor(
    uint16_t logical_index,
    const DndSpell* spell,
    uint8_t known,
    uint8_t always_prepared,
    uint8_t free_casts_current,
    uint8_t free_casts_max,
    void* context) {
    (void)known;
    (void)always_prepared;
    (void)free_casts_current;
    (void)free_casts_max;
    DndFavoriteSpellIndexContext* scan = context;
    if(!scan || !spell || !spell->favorite) return true;
    if(scan->count >= scan->start && scan->count - scan->start < scan->capacity)
        scan->indices[scan->count - scan->start] = logical_index;
    ++scan->count;
    return true;
}

static bool dndolphins_collect_favorite_spell_indices(
    DndDolphinsApp* app,
    uint16_t start,
    uint16_t* indices,
    uint16_t capacity,
    uint16_t* count) {
    DndFavoriteSpellIndexContext context = {
        .indices = indices,
        .start = start,
        .capacity = capacity,
        .count = 0U,
    };
    uint16_t total = 0U;
    bool ok = dnd_storage_visit_spells(
        app->storage,
        app->active_profile,
        dndolphins_favorite_spell_index_visitor,
        &context,
        &total);
    *count = context.count;
    app->spellbook_total = total;
    return ok;
}

static bool dndolphins_load_combat_spell_indices(DndDolphinsApp* app, uint16_t start) {
    DndSpellDamageResolver resolver = NULL;
    if(app->combat->combat_spell_mode == DndCombatSpellModeAttacks ||
       app->combat->combat_spell_mode == DndCombatSpellModeUtility) {
        if(!dndolphins_spell_plugin_require(app)) return false;
        const DndSpellDamageApi* api = app->combat->spell_plugin.api;
        resolver = api->damage_spec;
    }
    bool ok = false;
    if(app->combat->combat_spell_mode == DndCombatSpellModeFavorites)
        ok = dndolphins_collect_favorite_spell_indices(
            app,
            start,
            app->combat->combat_spell_indices,
            app->combat->combat_spell_capacity,
            &app->combat->combat_spell_count);
    else if(app->combat->combat_spell_mode == DndCombatSpellModeRituals)
        ok = dndolphins_spells_collect_ritual_indices(
            app->storage,
            app->active_profile,
            &app->data.character,
            start,
            app->combat->combat_spell_indices,
            app->combat->combat_spell_capacity,
            &app->combat->combat_spell_count,
            &app->spellbook_total);
    else if(app->combat->combat_spell_mode == DndCombatSpellModeUtility)
        ok = dndolphins_spells_collect_utility_indices(
            app->storage,
            app->active_profile,
            &app->data.character,
            resolver,
            start,
            app->combat->combat_spell_indices,
            app->combat->combat_spell_capacity,
            &app->combat->combat_spell_count,
            &app->spellbook_total);
    else
        ok = dndolphins_spells_collect_combat_indices(
            app->storage,
            app->active_profile,
            &app->data.character,
            resolver,
            start,
            app->combat->combat_spell_indices,
            app->combat->combat_spell_capacity,
            &app->combat->combat_spell_count,
            &app->spellbook_total);
    if(ok) app->combat->combat_spell_start = start;
    return ok;
}

static bool
    dndolphins_refresh_spell_index_common(DndDolphinsApp* app, DndCombatSpellMode spell_mode) {
    if(!app->combat->combat_spell_indices) {
        app->combat->combat_spell_capacity = DND_STORAGE_COLLECTION_CACHE_SIZE;
        app->combat->combat_spell_indices = malloc(
            app->combat->combat_spell_capacity * sizeof(uint16_t) +
            DNDOLPHINS_COMBAT_VISIBLE_ROWS * DNDOLPHINS_COMBAT_ROW_LEN);
        if(!app->combat->combat_spell_indices) return false;
    }
    app->combat->combat_spell_mode = (uint8_t)spell_mode;
    return dndolphins_load_combat_spell_indices(app, 0U);
}

static bool dndolphins_refresh_combat_spell_index(DndDolphinsApp* app) {
    return dndolphins_refresh_spell_index_common(app, DndCombatSpellModeAttacks);
}
static bool dndolphins_refresh_favorite_spell_index(DndDolphinsApp* app) {
    return dndolphins_refresh_spell_index_common(app, DndCombatSpellModeFavorites);
}
static bool dndolphins_refresh_utility_spell_index(DndDolphinsApp* app) {
    return dndolphins_refresh_spell_index_common(app, DndCombatSpellModeUtility);
}
static bool dndolphins_refresh_ritual_spell_index(DndDolphinsApp* app) {
    return dndolphins_refresh_spell_index_common(app, DndCombatSpellModeRituals);
}

static uint16_t dndolphins_combat_spell_count(DndDolphinsApp* app) {
    return app->combat->combat_spell_count;
}

static uint16_t dndolphins_combat_spell_index(DndDolphinsApp* app, uint16_t display_index) {
    if(!app->combat->combat_spell_indices || display_index >= app->combat->combat_spell_count)
        return UINT16_MAX;
    if(display_index < app->combat->combat_spell_start ||
       display_index - app->combat->combat_spell_start >= app->combat->combat_spell_capacity) {
        uint16_t start = (display_index / DND_STORAGE_COLLECTION_CACHE_SIZE) *
                         DND_STORAGE_COLLECTION_CACHE_SIZE;
        if(!dndolphins_load_combat_spell_indices(app, start)) return UINT16_MAX;
    }
    return app->combat->combat_spell_indices[display_index - app->combat->combat_spell_start];
}

static char* dndolphins_combat_spell_row(DndDolphinsApp* app, uint8_t visible) {
    if(!app || !app->combat->combat_spell_indices || visible >= DNDOLPHINS_COMBAT_VISIBLE_ROWS)
        return NULL;
    return (char*)(app->combat->combat_spell_indices + app->combat->combat_spell_capacity) +
           ((size_t)visible * DNDOLPHINS_COMBAT_ROW_LEN);
}

static void
    dndolphins_prepare_combat_spell_rows(DndDolphinsApp* app, DndCombatSpellMode spell_mode) {
    if(!app || !app->combat->combat_spell_indices) return;
    for(uint8_t visible = 0U; visible < DNDOLPHINS_COMBAT_VISIBLE_ROWS; ++visible) {
        char* row = dndolphins_combat_spell_row(app, visible);
        if(!row) continue;
        row[0] = '\0';
        uint16_t display_index = app->scroll + visible;
        if(display_index >= app->combat->combat_spell_count) continue;
        uint16_t spell_index = dndolphins_combat_spell_index(app, display_index);
        if(spell_index == UINT16_MAX) continue;
        DndSpell* spell = dndolphins_spell_at(app, spell_index, NULL);
        if(!spell) continue;
        if(spell_mode == DndCombatSpellModeRituals) {
            snprintf(row, DNDOLPHINS_COMBAT_ROW_LEN, "L%u %s", spell->level, spell->name);
            continue;
        }
        DndSpellDamageSpec damage;
        uint8_t ability = dndolphins_spells_casting_ability_for(&app->data.character, spell);
        int8_t ability_modifier =
            dnd_rules_core_ability_modifier(app->data.character.ability_scores[ability]);
        bool has_damage = dndolphins_spell_damage_spec(
            app,
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
        char casting_suffix[16];
        int8_t spell_modifier = dndolphins_spells_attack_modifier_for(&app->data.character, spell);
        snprintf(
            casting_suffix,
            sizeof(casting_suffix),
            " (%.3s/%+d)",
            dnd_rules_core_ability_names[ability],
            spell_modifier);
        char prefix[8];
        char suffix[48];
        if(spell->level)
            snprintf(prefix, sizeof(prefix), "L%u ", spell->level);
        else
            snprintf(prefix, sizeof(prefix), "C ");
        snprintf(
            suffix,
            sizeof(suffix),
            "%s%s%s",
            casting_suffix,
            spell->ritual ? " [R]" : "",
            dice_suffix);
        size_t prefix_len = strlen(prefix);
        size_t suffix_len = strlen(suffix);
        size_t max_name = prefix_len + suffix_len + 1U < DNDOLPHINS_COMBAT_ROW_LEN ?
                              DNDOLPHINS_COMBAT_ROW_LEN - prefix_len - suffix_len - 1U :
                              0U;
        size_t name_len = strlen(spell->name);
        if(name_len > max_name) name_len = max_name;
        memcpy(row, prefix, prefix_len);
        memcpy(row + prefix_len, spell->name, name_len);
        memcpy(row + prefix_len + name_len, suffix, suffix_len);
        row[prefix_len + name_len + suffix_len] = '\0';
    }
}

static const char* dndolphins_spell_cast_resource_name(uint8_t resource) {
    switch(resource) {
    case DndSpellCastCantrip:
        return "Cantrip";
    case DndSpellCastFree:
        return "Free cast";
    case DndSpellCastSlot:
        return "Spell slot";
    case DndSpellCastPact:
        return "Pact slot";
    case DndSpellCastPoints:
        return "Spell points";
    case DndSpellCastRitual:
        return "Ritual +10 min";
    default:
        return "Cast";
    }
}

static void dndolphins_format_spell_cast_option(
    DndDolphinsApp* app,
    const DndSpellCastOption* option,
    char* output,
    size_t size) {
    const DndCharacter* character = &app->data.character;
    switch(option->resource) {
    case DndSpellCastCantrip:
        snprintf(output, size, "Cast cantrip");
        break;
    case DndSpellCastFree: {
        uint8_t local = 0U;
        uint8_t remaining = 0U;
        if(dndolphins_spell_cached_at(app, app->combat->spell_attack_index, &local))
            remaining = character->spell_free_casts_current[local];
        snprintf(output, size, "Free cast L%u (%u left)", option->level, remaining);
        break;
    }
    case DndSpellCastSlot:
        snprintf(
            output,
            size,
            "L%u slot (%u left)",
            option->level,
            character->spell_slots_current[option->level]);
        break;
    case DndSpellCastPact:
        snprintf(
            output,
            size,
            "Pact L%u (%u left)",
            option->level,
            character->classes[option->class_index].pact_slots_current);
        break;
    case DndSpellCastPoints:
        snprintf(
            output,
            size,
            "L%u points (%u cost)",
            option->level,
            dndolphins_spells_point_cost(option->level));
        break;
    case DndSpellCastRitual:
        snprintf(output, size, "Ritual (+10 minutes)");
        break;
    default:
        output[0] = '\0';
        break;
    }
}

static bool
    dndolphins_consume_spell_cast_resource(DndDolphinsApp* app, const DndSpellCastOption* option) {
    DndCharacter* character = &app->data.character;
    switch(option->resource) {
    case DndSpellCastCantrip:
    case DndSpellCastRitual:
        return true;
    case DndSpellCastFree: {
        uint8_t local = 0U;
        if(!dndolphins_spell_at(app, app->combat->spell_attack_index, &local) ||
           !character->spell_free_casts_current[local])
            return false;
        --character->spell_free_casts_current[local];
        return true;
    }
    case DndSpellCastSlot:
        if(option->level >= DND_SLOT_COUNT || !character->spell_slots_current[option->level])
            return false;
        --character->spell_slots_current[option->level];
        return true;
    case DndSpellCastPact:
        if(option->class_index >= character->class_count ||
           !character->classes[option->class_index].pact_slots_current)
            return false;
        --character->classes[option->class_index].pact_slots_current;
        return true;
    case DndSpellCastPoints: {
        if(option->class_index >= character->class_count) return false;
        uint8_t cost = dndolphins_spells_point_cost(option->level);
        if(!cost || character->classes[option->class_index].spell_points_current < cost)
            return false;
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

static void dndolphins_cast_spell(DndDolphinsApp* app, const DndSpellCastOption* option) {
    if(!dndolphins_spell_plugin_require(app)) return;
    DndCharacter* character = &app->data.character;
    if(app->combat->spell_attack_index >= app->spellbook_total) {
        dndolphins_set_status(app, "Casting resource unavailable");
        return;
    }
    DndSpell* spell = dndolphins_spell_at(app, app->combat->spell_attack_index, NULL);
    if(!spell) {
        dndolphins_set_status(app, "Spell read failed");
        return;
    }
    if(!dndolphins_consume_spell_cast_resource(app, option)) {
        dndolphins_set_status(app, "Casting resource unavailable");
        return;
    }

    app->combat->spell_cast_level = option->level;
    app->combat->spell_cast_resource = option->resource;
    app->combat->spell_cast_class_index = option->class_index;
    app->combat->spell_cast_primary_total = 0;
    app->combat->spell_cast_secondary_total = 0;
    app->combat->spell_cast_flat_bonus = 0;
    app->combat->spell_cast_secondary_flat_bonus = 0;
    app->combat->spell_cast_damage_total = 0;
    app->combat->spell_cast_natural = 0U;
    app->combat->spell_cast_attack_total = 0;
    app->combat->spell_cast_resolution = DndSpellResolutionNone;
    app->combat->spell_cast_secondary_resolution = DndSpellResolutionNone;
    app->combat->spell_cast_secondary_relation = 0U;
    app->combat->spell_cast_derived_effect = DndSpellDerivedNone;
    app->combat->spell_cast_from_notes = 0U;
    app->combat->spell_cast_primary_dice = 0U;
    app->combat->spell_cast_primary_die = 0U;
    app->combat->spell_cast_secondary_dice = 0U;
    app->combat->spell_cast_secondary_die = 0U;
    app->combat->spell_cast_attack_roll_count = 0U;
    memset(
        app->combat->spell_cast_attack_natural, 0, sizeof(app->combat->spell_cast_attack_natural));
    memset(
        app->combat->spell_cast_attack_damage, 0, sizeof(app->combat->spell_cast_attack_damage));

    uint8_t ability = dndolphins_spell_casting_ability(app, app->combat->spell_attack_index);
    int8_t ability_modifier = dnd_rules_core_ability_modifier(character->ability_scores[ability]);
    DndSpellDamageSpec damage;
    if(dndolphins_spell_damage_spec(
           app,
           spell,
           option->level,
           dnd_rules_core_total_level(character),
           ability_modifier,
           &damage)) {
        app->combat->spell_cast_primary_dice = damage.primary_dice;
        app->combat->spell_cast_primary_die = damage.primary_die;
        app->combat->spell_cast_secondary_dice = damage.secondary_dice;
        app->combat->spell_cast_secondary_die = damage.secondary_die;
        app->combat->spell_cast_flat_bonus = damage.flat_bonus;
        app->combat->spell_cast_secondary_flat_bonus = damage.secondary_flat_bonus;
        app->combat->spell_cast_resolution = damage.resolution;
        app->combat->spell_cast_secondary_resolution = damage.secondary_resolution;
        app->combat->spell_cast_secondary_relation = damage.secondary_relation;
        app->combat->spell_cast_derived_effect = damage.derived_effect;
        app->combat->spell_cast_from_notes = damage.from_notes;

        if(damage.resolution == DndSpellResolutionAttack) {
            uint8_t attack_count = damage.attack_rolls ? damage.attack_rolls : 1U;
            if(attack_count > DNDOLPHINS_MAX_SPELL_ATTACK_ROLLS)
                attack_count = DNDOLPHINS_MAX_SPELL_ATTACK_ROLLS;
            app->combat->spell_cast_attack_roll_count = attack_count;
            int8_t attack_modifier =
                dndolphins_spell_attack_modifier_for(app, app->combat->spell_attack_index);
            for(uint8_t index = 0U; index < attack_count; ++index) {
                int16_t primary = 0;
                int16_t secondary = 0;
                if(damage.primary_dice && damage.primary_die) {
                    if(damage.special == DndSpellSpecialSorcerousBurst) {
                        uint8_t rolled_dice = damage.primary_dice;
                        primary = dndolphins_roll_sorcerous_burst(
                            damage.primary_dice, ability_modifier, &rolled_dice);
                        if(index == 0U) app->combat->spell_cast_primary_dice = rolled_dice;
                    } else {
                        primary = (int16_t)dnd_rules_core_roll_dice(
                            damage.primary_dice, damage.primary_die);
                    }
                }
                if(!damage.secondary_relation && damage.secondary_dice && damage.secondary_die)
                    secondary = (int16_t)dnd_rules_core_roll_dice(
                        damage.secondary_dice, damage.secondary_die);
                int16_t attack_damage = primary + secondary + damage.flat_bonus;
                uint8_t natural = dndolphins_dice_roll_d20_mode(app->roll->roll_mode);
                app->combat->spell_cast_attack_natural[index] = natural;
                app->combat->spell_cast_attack_damage[index] = attack_damage;
                app->combat->spell_cast_damage_total += attack_damage;
                if(index == 0U) {
                    app->combat->spell_cast_primary_total = primary;
                    app->combat->spell_cast_secondary_total = secondary;
                    app->combat->spell_cast_natural = natural;
                    app->combat->spell_cast_attack_total = natural + attack_modifier;
                }
            }
            if(damage.secondary_relation && damage.secondary_dice && damage.secondary_die)
                app->combat->spell_cast_secondary_total =
                    (int16_t)dnd_rules_core_roll_dice(damage.secondary_dice, damage.secondary_die);
        } else if(damage.roll_instances > 1U) {
            uint8_t roll_count = damage.roll_instances;
            if(roll_count > DNDOLPHINS_MAX_SPELL_ATTACK_ROLLS)
                roll_count = DNDOLPHINS_MAX_SPELL_ATTACK_ROLLS;
            app->combat->spell_cast_attack_roll_count = roll_count;
            for(uint8_t index = 0U; index < roll_count; ++index) {
                int16_t primary = 0;
                if(damage.primary_dice && damage.primary_die)
                    primary =
                        (int16_t)dnd_rules_core_roll_dice(damage.primary_dice, damage.primary_die);
                int16_t value = primary + damage.flat_bonus;
                app->combat->spell_cast_attack_damage[index] = value;
                app->combat->spell_cast_damage_total += value;
                if(index == 0U) app->combat->spell_cast_primary_total = primary;
            }
        } else {
            if(damage.primary_dice && damage.primary_die)
                app->combat->spell_cast_primary_total =
                    (int16_t)dnd_rules_core_roll_dice(damage.primary_dice, damage.primary_die);
            if(damage.secondary_dice && damage.secondary_die)
                app->combat->spell_cast_secondary_total =
                    (int16_t)dnd_rules_core_roll_dice(damage.secondary_dice, damage.secondary_die);
            app->combat->spell_cast_damage_total =
                app->combat->spell_cast_primary_total + damage.flat_bonus;
            if(!damage.secondary_relation)
                app->combat->spell_cast_damage_total +=
                    app->combat->spell_cast_secondary_total + damage.secondary_flat_bonus;
        }
    }

    if(option->resource == DndSpellCastFree) (void)dndolphins_save_spellbook_if_changed(app);
    dndolphins_save(app, false);
    dndolphins_enter_screen(app, DndScreenSpellResult);
}

static void dndolphins_draw_spell_attacks(Canvas* canvas, DndDolphinsApp* app) {
    uint16_t count = dndolphins_combat_spell_count(app);
    char title[32];
    snprintf(title, sizeof(title), "Spells: %s", dndolphins_roll_mode_names[app->roll->roll_mode]);
    dndolphins_draw_header(canvas, title, app->status);
    if(!count) {
        dndolphins_draw_row(canvas, 0U, false, "No combat spells");
        dndolphins_draw_row(canvas, 1U, false, "Prepare or add XdY notes");
        return;
    }
    for(uint8_t visible = 0U; visible < DNDOLPHINS_COMBAT_VISIBLE_ROWS; ++visible) {
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

static void dndolphins_draw_favorite_spells(Canvas* canvas, DndDolphinsApp* app) {
    uint16_t count = dndolphins_combat_spell_count(app);
    dndolphins_draw_header(canvas, "Favorite Spells", app->status);
    if(!count) {
        dndolphins_draw_row(canvas, 0U, false, "No favorite spells");
        dndolphins_draw_row(canvas, 1U, false, "Set in DNDSpellbook");
        return;
    }
    for(uint8_t visible = 0U; visible < DNDOLPHINS_COMBAT_VISIBLE_ROWS; ++visible) {
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

static void dndolphins_draw_utility_spells(Canvas* canvas, DndDolphinsApp* app) {
    uint16_t count = dndolphins_combat_spell_count(app);
    dndolphins_draw_header(canvas, "Combat Utility Spells", app->status);
    if(!count) {
        dndolphins_draw_row(canvas, 0U, false, "No available utility spells");
        return;
    }
    for(uint8_t visible = 0U; visible < DNDOLPHINS_COMBAT_VISIBLE_ROWS; ++visible) {
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

static void dndolphins_draw_rituals(Canvas* canvas, DndDolphinsApp* app) {
    uint16_t count = dndolphins_combat_spell_count(app);
    dndolphins_draw_header(canvas, "Rituals", app->status);
    if(!count) {
        dndolphins_draw_row(canvas, 0U, false, "No known Wizard rituals");
        dndolphins_draw_row(canvas, 1U, false, "Ritual Adept: spellbook");
        return;
    }
    for(uint8_t visible = 0U; visible < DNDOLPHINS_COMBAT_VISIBLE_ROWS; ++visible) {
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

static void dndolphins_draw_spell_cast(Canvas* canvas, DndDolphinsApp* app) {
    if(app->combat->spell_attack_index >= app->spellbook_total) return;
    DndSpellCastOption options[DNDOLPHINS_MAX_SPELL_CAST_OPTIONS];
    uint8_t count = dndolphins_build_spell_cast_options_cached(
        app, app->combat->spell_attack_index, options, DNDOLPHINS_MAX_SPELL_CAST_OPTIONS);
    if(count > DNDOLPHINS_MAX_SPELL_CAST_OPTIONS) count = DNDOLPHINS_MAX_SPELL_CAST_OPTIONS;
    DndSpell* spell = dndolphins_spell_cached_at(app, app->combat->spell_attack_index, NULL);
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
    case DndSpellResolutionAttack:
        return "Attack";
    case DndSpellResolutionSave:
        return "Save";
    case DndSpellResolutionAutomatic:
        return "Auto";
    case DndSpellResolutionTriggered:
        return "Trigger";
    case DndSpellResolutionHealing:
        return "Heal";
    case DndSpellResolutionTemporaryHP:
        return "Temp HP";
    case DndSpellResolutionMitigation:
        return "Reduce";
    case DndSpellResolutionTransfer:
        return "Self dmg";
    case DndSpellResolutionVitality:
        return "HP + max";
    default:
        return "Roll";
    }
}

static const char* dndolphins_spell_secondary_relation_label(uint8_t relation) {
    switch(relation) {
    case DndSpellSecondaryAlternative:
        return "Choose one effect";
    case DndSpellSecondaryLater:
        return "Initial / later rolls";
    case DndSpellSecondaryIndependent:
        return "Independent effect rolls";
    default:
        return "Separate effect rolls";
    }
}

static int16_t dndolphins_spell_component_total(int16_t dice_total, int16_t flat_bonus) {
    return (int16_t)(dice_total + flat_bonus);
}

static void dndolphins_draw_spell_result(Canvas* canvas, DndDolphinsApp* app) {
    if(app->combat->spell_attack_index >= app->spellbook_total) return;
    DndSpell* spell = dndolphins_spell_cached_at(app, app->combat->spell_attack_index, NULL);
    if(!spell) return;
    dndolphins_draw_header(canvas, spell->name, app->status);
    char row[64];

    if(app->combat->spell_cast_resolution == DndSpellResolutionAttack &&
       app->combat->spell_cast_attack_roll_count > 1U) {
        const char* resource = "Cast";
        switch(app->combat->spell_cast_resource) {
        case DndSpellCastCantrip:
            resource = "Cantrip";
            break;
        case DndSpellCastFree:
            resource = "Free";
            break;
        case DndSpellCastSlot:
            resource = "Slot";
            break;
        case DndSpellCastPact:
            resource = "Pact";
            break;
        case DndSpellCastPoints:
            resource = "Points";
            break;
        case DndSpellCastRitual:
            resource = "Ritual";
            break;
        default:
            break;
        }
        if(app->combat->spell_cast_level)
            snprintf(
                row,
                sizeof(row),
                "L%u %s | %u attacks",
                app->combat->spell_cast_level,
                resource,
                app->combat->spell_cast_attack_roll_count);
        else
            snprintf(
                row,
                sizeof(row),
                "%s | %u attacks",
                resource,
                app->combat->spell_cast_attack_roll_count);
        dndolphins_draw_row(canvas, 0U, false, row);

        int8_t attack_modifier =
            dndolphins_spell_attack_modifier_cached_for(app, app->combat->spell_attack_index);
        for(uint8_t visible = 0U; visible < 4U; ++visible) {
            uint8_t index = (uint8_t)(app->scroll + visible);
            if(index >= app->combat->spell_cast_attack_roll_count) break;
            int16_t attack_total = app->combat->spell_cast_attack_natural[index] + attack_modifier;
            snprintf(
                row,
                sizeof(row),
                "%u) %u%+d=%d D%d",
                index + 1U,
                app->combat->spell_cast_attack_natural[index],
                attack_modifier,
                attack_total,
                app->combat->spell_cast_attack_damage[index]);
            dndolphins_draw_row(canvas, visible + 1U, false, row);
        }
        return;
    }

    if(app->combat->spell_cast_resolution != DndSpellResolutionAttack &&
       app->combat->spell_cast_attack_roll_count > 1U) {
        const char* resource =
            dndolphins_spell_cast_resource_name(app->combat->spell_cast_resource);
        if(app->combat->spell_cast_level)
            snprintf(
                row,
                sizeof(row),
                "L%u %s | %u rolls",
                app->combat->spell_cast_level,
                resource,
                app->combat->spell_cast_attack_roll_count);
        else
            snprintf(
                row,
                sizeof(row),
                "%s | %u rolls",
                resource,
                app->combat->spell_cast_attack_roll_count);
        dndolphins_draw_row(canvas, 0U, false, row);
        for(uint8_t visible = 0U; visible < 4U; ++visible) {
            uint8_t index = (uint8_t)(app->scroll + visible);
            if(index >= app->combat->spell_cast_attack_roll_count) break;
            snprintf(
                row,
                sizeof(row),
                "%u) %ud%u%+d = %d",
                index + 1U,
                app->combat->spell_cast_primary_dice,
                app->combat->spell_cast_primary_die,
                app->combat->spell_cast_flat_bonus,
                app->combat->spell_cast_attack_damage[index]);
            dndolphins_draw_row(canvas, visible + 1U, false, row);
        }
        return;
    }

    if(app->combat->spell_cast_resource == DndSpellCastRitual)
        snprintf(row, sizeof(row), "Ritual cast: +10 minutes");
    else if(app->combat->spell_cast_level)
        snprintf(
            row,
            sizeof(row),
            "Cast L%u - %s",
            app->combat->spell_cast_level,
            dndolphins_spell_cast_resource_name(app->combat->spell_cast_resource));
    else
        snprintf(row, sizeof(row), "Cantrip cast");
    dndolphins_draw_row(canvas, 0U, false, row);

    if(app->combat->spell_cast_resolution == DndSpellResolutionAttack) {
        snprintf(
            row,
            sizeof(row),
            "Attack d20 %u %+d = %d",
            app->combat->spell_cast_natural,
            dndolphins_spell_attack_modifier_cached_for(app, app->combat->spell_attack_index),
            app->combat->spell_cast_attack_total);
    } else if(app->combat->spell_cast_resolution == DndSpellResolutionSave) {
        snprintf(
            row,
            sizeof(row),
            "Target save DC %d",
            dndolphins_spell_save_dc_cached_for(app, app->combat->spell_attack_index));
    } else if(app->combat->spell_cast_resolution == DndSpellResolutionAutomatic) {
        snprintf(row, sizeof(row), "Automatic / no attack roll");
    } else if(app->combat->spell_cast_resolution == DndSpellResolutionTriggered) {
        snprintf(row, sizeof(row), "Damage after trigger/hit");
    } else if(app->combat->spell_cast_resolution == DndSpellResolutionHealing) {
        snprintf(row, sizeof(row), "Healing roll");
    } else if(app->combat->spell_cast_resolution == DndSpellResolutionTemporaryHP) {
        snprintf(row, sizeof(row), "Temporary HP roll");
    } else if(app->combat->spell_cast_resolution == DndSpellResolutionMitigation) {
        snprintf(row, sizeof(row), "Damage reduction roll");
    } else if(app->combat->spell_cast_resolution == DndSpellResolutionTransfer) {
        snprintf(row, sizeof(row), "Self damage; target heals x2");
    } else if(app->combat->spell_cast_resolution == DndSpellResolutionVitality) {
        snprintf(row, sizeof(row), "HP and HP maximum increase");
    } else {
        snprintf(
            row,
            sizeof(row),
            "Spell +%d / DC %d",
            dndolphins_spell_attack_modifier_cached_for(app, app->combat->spell_attack_index),
            dndolphins_spell_save_dc_cached_for(app, app->combat->spell_attack_index));
    }
    dndolphins_draw_row(canvas, 1U, false, row);

    if(!app->combat->spell_cast_primary_dice && !app->combat->spell_cast_secondary_dice &&
       !app->combat->spell_cast_flat_bonus && !app->combat->spell_cast_secondary_flat_bonus) {
        dndolphins_draw_row(canvas, 2U, false, "No mapped combat roll");
        dndolphins_draw_row(canvas, 3U, false, "Cast/resource recorded");
    } else if(app->combat->spell_cast_secondary_relation) {
        int16_t primary_total = dndolphins_spell_component_total(
            app->combat->spell_cast_primary_total, app->combat->spell_cast_flat_bonus);
        int16_t secondary_total = dndolphins_spell_component_total(
            app->combat->spell_cast_secondary_total, app->combat->spell_cast_secondary_flat_bonus);
        if(app->combat->spell_cast_primary_dice)
            snprintf(
                row,
                sizeof(row),
                "%s %ud%u%+d=%d",
                dndolphins_spell_resolution_label(app->combat->spell_cast_resolution),
                app->combat->spell_cast_primary_dice,
                app->combat->spell_cast_primary_die,
                app->combat->spell_cast_flat_bonus,
                primary_total);
        else
            snprintf(
                row,
                sizeof(row),
                "%s %+d",
                dndolphins_spell_resolution_label(app->combat->spell_cast_resolution),
                app->combat->spell_cast_flat_bonus);
        dndolphins_draw_row(canvas, 2U, false, row);

        if(app->combat->spell_cast_secondary_dice)
            snprintf(
                row,
                sizeof(row),
                "%s %ud%u%+d=%d",
                dndolphins_spell_resolution_label(app->combat->spell_cast_secondary_resolution),
                app->combat->spell_cast_secondary_dice,
                app->combat->spell_cast_secondary_die,
                app->combat->spell_cast_secondary_flat_bonus,
                secondary_total);
        else
            snprintf(
                row,
                sizeof(row),
                "%s %+d",
                dndolphins_spell_resolution_label(app->combat->spell_cast_secondary_resolution),
                app->combat->spell_cast_secondary_flat_bonus);
        dndolphins_draw_row(canvas, 3U, false, row);
        dndolphins_draw_row(
            canvas,
            4U,
            false,
            dndolphins_spell_secondary_relation_label(app->combat->spell_cast_secondary_relation));
    } else {
        if(app->combat->spell_cast_primary_dice) {
            snprintf(
                row,
                sizeof(row),
                app->combat->spell_cast_from_notes ? "Notes %ud%u = %d" : "%ud%u = %d",
                app->combat->spell_cast_primary_dice,
                app->combat->spell_cast_primary_die,
                app->combat->spell_cast_primary_total);
            dndolphins_draw_row(canvas, 2U, false, row);
        }
        if(app->combat->spell_cast_secondary_dice) {
            snprintf(
                row,
                sizeof(row),
                "+ %ud%u = %d",
                app->combat->spell_cast_secondary_dice,
                app->combat->spell_cast_secondary_die,
                app->combat->spell_cast_secondary_total);
            dndolphins_draw_row(canvas, 3U, false, row);
        } else if(app->combat->spell_cast_flat_bonus) {
            snprintf(row, sizeof(row), "Modifier: %+d", app->combat->spell_cast_flat_bonus);
            dndolphins_draw_row(canvas, 3U, false, row);
        }

        if(app->combat->spell_cast_derived_effect == DndSpellDerivedHealHalfPrimary) {
            snprintf(row, sizeof(row), "Heal: %d", app->combat->spell_cast_damage_total / 2);
        } else if(app->combat->spell_cast_derived_effect == DndSpellDerivedHealDoublePrimary) {
            snprintf(
                row, sizeof(row), "Heal target: %d", app->combat->spell_cast_damage_total * 2);
        } else if(app->combat->spell_cast_resolution == DndSpellResolutionHealing) {
            snprintf(row, sizeof(row), "Healing total: %d", app->combat->spell_cast_damage_total);
        } else if(app->combat->spell_cast_resolution == DndSpellResolutionTemporaryHP) {
            snprintf(row, sizeof(row), "Temp HP: %d", app->combat->spell_cast_damage_total);
        } else if(app->combat->spell_cast_resolution == DndSpellResolutionMitigation) {
            snprintf(row, sizeof(row), "Reduce by: %d", app->combat->spell_cast_damage_total);
        } else if(app->combat->spell_cast_resolution == DndSpellResolutionVitality) {
            snprintf(row, sizeof(row), "HP & max: +%d", app->combat->spell_cast_damage_total);
        } else {
            snprintf(row, sizeof(row), "Damage total: %d", app->combat->spell_cast_damage_total);
        }
        dndolphins_draw_row(canvas, 4U, false, row);
    }
}

static void dndolphins_draw_attack_list(Canvas* canvas, DndDolphinsApp* app) {
    uint16_t count = dndolphins_weapon_count(app);
    char title[32];
    snprintf(
        title, sizeof(title), "Attacks: %s", dndolphins_roll_mode_names[app->roll->roll_mode]);
    dndolphins_draw_header(canvas, title, app->status);
    if(count == 0U) {
        dndolphins_draw_row(canvas, 0U, false, "No weapon items");
        dndolphins_draw_row(canvas, 1U, false, "Add one in Inventory");
        return;
    }
    for(uint8_t visible = 0U; visible < DNDOLPHINS_COMBAT_VISIBLE_ROWS; ++visible) {
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

static void dndolphins_draw_ammo_choice(Canvas* canvas, DndDolphinsApp* app) {
    dndolphins_draw_header(canvas, "Choose Ammunition", app->status);
    for(uint8_t visible = 0U; visible < 5U; ++visible) {
        uint16_t choice = app->scroll + visible;
        if(choice >= app->combat->ammo_choice_count) break;
        char row[48];
        if(dndolphins_item_cache_ensure(app, app->combat->ammo_choice_indices[choice])) {
            DndItem* item =
                dndolphins_item_cached_at(app, app->combat->ammo_choice_indices[choice], NULL);
            if(item)
                snprintf(row, sizeof(row), "%.31s x%d", item->name, item->quantity);
            else
                dndolphins_copy(row, sizeof(row), "Ammo unavailable");
        } else {
            dndolphins_copy(row, sizeof(row), "Ammo unavailable");
        }
        dndolphins_draw_row(canvas, visible, choice == app->selection, row);
    }
}

static void dndolphins_draw_attack_result(Canvas* canvas, DndDolphinsApp* app) {
    DndItem* item = dndolphins_item_cached_at(app, app->combat->attack_item_index, NULL);
    if(!item) return;
    dndolphins_draw_header(canvas, item->name, app->status);
    char row[64];
    if(app->combat->attack_phase == 0U) {
        if(app->combat->attack_roll.second_die) {
            snprintf(
                row,
                sizeof(row),
                "d20: %u / %u",
                app->combat->attack_roll.first_die,
                app->combat->attack_roll.second_die);
        } else {
            snprintf(row, sizeof(row), "d20: %u", app->combat->attack_roll.first_die);
        }
        dndolphins_draw_row(canvas, 0U, false, row);
        uint8_t detail_row = 1U;
        if(app->combat->attack_roll.second_die) {
            snprintf(
                row,
                sizeof(row),
                "Dice sum: %u",
                app->combat->attack_roll.first_die + app->combat->attack_roll.second_die);
            dndolphins_draw_row(canvas, detail_row++, false, row);
        }
        snprintf(row, sizeof(row), "Modifier: %+d", app->combat->attack_roll.modifier);
        dndolphins_draw_row(canvas, detail_row++, false, row);
        snprintf(row, sizeof(row), "Attack total: %d", app->combat->attack_roll.total);
        dndolphins_draw_row(canvas, detail_row++, false, row);
        if(app->combat->attack_roll.critical)
            snprintf(row, sizeof(row), "Critical! OK damage");
        else if(app->combat->attack_roll.automatic_miss)
            snprintf(row, sizeof(row), "Natural 1 - miss");
        else
            snprintf(row, sizeof(row), "OK damage; Right crit");
        dndolphins_draw_row(canvas, detail_row, false, row);
        if(detail_row < 4U) dndolphins_draw_row(canvas, 4U, false, "Up: reroll attack");
    } else {
        uint8_t roll_count =
            app->combat->damage_roll.weapon_roll_count + app->combat->damage_roll.extra_roll_count;
        if(roll_count > 1U) {
            uint8_t page_count = (roll_count + 15U) / 16U;
            if(app->combat->damage_roll_page >= page_count)
                app->combat->damage_roll_page = page_count - 1U;
            char damage_title[48];
            snprintf(
                damage_title,
                sizeof(damage_title),
                "Damage %u/%u - total %d",
                app->combat->damage_roll_page + 1U,
                page_count,
                app->combat->damage_roll.total);
            dndolphins_draw_header(canvas, damage_title, app->status);
            uint8_t start = app->combat->damage_roll_page * 16U;
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
                        roll_index < app->combat->damage_roll.weapon_roll_count ? 'W' : 'E',
                        app->combat->damage_roll.rolls[roll_index]);
                    if(written < 0 || (size_t)written >= sizeof(values) - position) break;
                    position += (size_t)written;
                }
                dndolphins_draw_row(canvas, display_row, false, values);
            }
            snprintf(
                row,
                sizeof(row),
                "Sum %d %+d = %d",
                app->combat->damage_roll.weapon_total + app->combat->damage_roll.extra_total,
                app->combat->damage_roll.modifier,
                app->combat->damage_roll.total);
            dndolphins_draw_row(canvas, 4U, false, row);
        } else {
            snprintf(
                row,
                sizeof(row),
                "%s damage",
                app->combat->damage_roll.critical ? "Critical" : "Normal");
            dndolphins_draw_row(canvas, 0U, false, row);
            snprintf(row, sizeof(row), "Weapon dice: %d", app->combat->damage_roll.weapon_total);
            dndolphins_draw_row(canvas, 1U, false, row);
            snprintf(row, sizeof(row), "Extra dice: %d", app->combat->damage_roll.extra_total);
            dndolphins_draw_row(canvas, 2U, false, row);
            snprintf(row, sizeof(row), "Modifier: %+d", app->combat->damage_roll.modifier);
            dndolphins_draw_row(canvas, 3U, false, row);
            snprintf(row, sizeof(row), "Total: %d (OK reroll)", app->combat->damage_roll.total);
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

static void dndolphins_draw_dice_animation(Canvas* canvas, DndDolphinsApp* app) {
    char title[40];
    snprintf(
        title,
        sizeof(title),
        "Rolling %ud%u...",
        app->roll->dice_anim_count,
        app->roll->dice_anim_sides);
    dndolphins_draw_header(canvas, title, NULL);
    uint8_t visible_dice = app->roll->dice_anim_count > 1U ? 3U : 1U;
    for(uint8_t i = 0U; i < visible_dice; ++i) {
        int32_t x = visible_dice == 1U ? 64 : 22 + (i * 42);
        uint8_t face =
            (uint8_t)(((app->roll->dice_anim_frame * 7U) + (i * 5U) + app->roll->dice_anim_sides) %
                      app->roll->dice_anim_sides) +
            1U;
        dndolphins_draw_animated_die(canvas, x, 34, app->roll->dice_anim_frame + i, face);
    }
    canvas_draw_frame(canvas, 14, 55, 100, 5);
    uint8_t progress =
        (uint8_t)(((app->roll->dice_anim_frame + 1U) * 98U) / DNDOLPHINS_DICE_ANIMATION_FRAMES);
    canvas_draw_box(canvas, 15, 56, progress, 3);
}

static void dndolphins_draw_callback(Canvas* canvas, void* model) {
    DndDolphinsApp* app = *(DndDolphinsApp**)model;
    canvas_clear(canvas);
    if(app->roll && app->roll->dice_animating) {
        dndolphins_draw_dice_animation(canvas, app);
        return;
    }
    switch(app->screen) {
#if defined(DND_BUILD_HUB)
    case DndScreenHome:
        dndolphins_draw_home(canvas, app);
        break;
    case DndScreenProfiles:
        dndolphins_draw_profiles(canvas, app);
        break;
    case DndScreenProfileActions:
        dndolphins_draw_profile_actions(canvas, app);
        break;
    case DndScreenCharacter:
        dndolphins_draw_character(canvas, app);
        break;
    case DndScreenVitals:
        dndolphins_draw_vitals(canvas, app);
        break;
    case DndScreenAbilities:
        dndolphins_draw_abilities(canvas, app);
        break;
    case DndScreenSkills:
        dndolphins_draw_skills(canvas, app);
        break;
    case DndScreenLevelReview:
        dndolphins_draw_level_review(canvas, app);
        break;
    case DndScreenLevelChoice:
        dndolphins_draw_level_choice(canvas, app);
        break;
    case DndScreenAsiAbility:
        dndolphins_draw_asi_ability(canvas, app);
        break;
    case DndScreenRecordList:
        dndolphins_draw_record_list(canvas, app);
        break;
    case DndScreenRecordDetail:
        dndolphins_draw_record_detail(canvas, app);
        break;
    case DndScreenCatalog:
        dndolphins_draw_catalog(canvas, app);
        break;
    case DndScreenDice:
        dndolphins_draw_dice(canvas, app);
        break;
    case DndScreenDiceResult:
        dndolphins_draw_dice_result(canvas, app);
        break;
    case DndScreenSettings:
        dndolphins_draw_settings(canvas, app);
        break;
#elif defined(DND_BUILD_COMBAT)
    case DndScreenCombat:
        dndolphins_draw_combat(canvas, app);
        break;
    case DndScreenSpellAttacks:
        dndolphins_draw_spell_attacks(canvas, app);
        break;
    case DndScreenFavoriteSpells:
        dndolphins_draw_favorite_spells(canvas, app);
        break;
    case DndScreenUtilitySpells:
        dndolphins_draw_utility_spells(canvas, app);
        break;
    case DndScreenRituals:
        dndolphins_draw_rituals(canvas, app);
        break;
    case DndScreenSpellCast:
        dndolphins_draw_spell_cast(canvas, app);
        break;
    case DndScreenSpellResult:
        dndolphins_draw_spell_result(canvas, app);
        break;
    case DndScreenAttackTemplates:
        dndolphins_draw_attack_templates(canvas, app);
        break;
    case DndScreenAttackTemplateEdit:
        dndolphins_draw_attack_template_edit(canvas, app);
        break;
    case DndScreenAttackList:
        dndolphins_draw_attack_list(canvas, app);
        break;
    case DndScreenAmmoChoice:
        dndolphins_draw_ammo_choice(canvas, app);
        break;
    case DndScreenAttackResult:
        dndolphins_draw_attack_result(canvas, app);
        break;
    case DndScreenMagic:
        dndolphins_draw_magic(canvas, app);
        break;
#elif defined(DND_BUILD_GRANTS)
    case DndScreenGrantReview:
        dndolphins_draw_grant_review(canvas, app);
        break;
    case DndScreenGrantDiagnostics:
        dndolphins_draw_grant_diagnostics(canvas, app);
        break;
    case DndScreenGrantEdit:
        dndolphins_draw_grant_edit(canvas, app);
        break;
    case DndScreenCatalog:
        dndolphins_draw_catalog(canvas, app);
        break;
#endif
    default:
        break;
    }
}

static void dndolphins_open_list(DndDolphinsApp* app, DndListKind kind, DndScreen return_screen) {
    dndolphins_release_text_input(app);
    dndolphins_release_number_input(app);
    app->list_kind = kind;
    app->record_list_return_screen = return_screen;
    if(kind == DndListLanguages) {
        app->language_total = 0U;
        if(app->collection_cache) {
            app->collection_cache->language_page_count = 0U;
            app->collection_cache->language_cache_start = 0U;
        }
        if(!dndolphins_load_language_page(app, 0U))
            dndolphins_set_status(app, "Language read failed");
    } else if(kind == DndListProficiencies) {
        app->proficiency_total = 0U;
        if(app->collection_cache) {
            app->collection_cache->proficiency_page_count = 0U;
            app->collection_cache->proficiency_cache_start = 0U;
        }
        if(!dndolphins_load_proficiency_page(app, 0U))
            dndolphins_set_status(app, "Proficiency read failed");
    }
    dndolphins_enter_screen(app, DndScreenRecordList);
}

/* The proven pre-sidecar implementation grew the resident collection before
   saving. Preserve that lifecycle with bounded paging by first making the real
   tail page resident, then growing that page and committing it immediately. */

static bool dndolphins_feature_prepare_append_page(DndDolphinsApp* app) {
    if(!app->features_loaded && !dndolphins_load_features(app)) return false;
    const uint16_t target_start =
        (uint16_t)((app->features_total / DND_PROGRESS_CACHE_SIZE) * DND_PROGRESS_CACHE_SIZE);
    if(app->collection_cache->features_cache_start != target_start) {
        if(!dndolphins_save_features_if_changed(app)) return false;
        dnd_data_reserve_features_exact(&app->data.character, 0U);
        app->data.character.feature_count = 0U;
        app->features_loaded = 0U;
        if(!dndolphins_load_features_page(app, target_start)) return false;
    }

    DndCharacter* character = &app->data.character;
    const uint8_t expected = (uint8_t)(app->features_total - target_start);
    if(character->feature_count != expected || character->feature_count >= DND_PROGRESS_CACHE_SIZE)
        return false;
    return dnd_data_reserve_features(character, (uint8_t)(character->feature_count + 1U));
}

static bool dndolphins_add_record(DndDolphinsApp* app) {
    dndolphins_release_text_input(app);
    dndolphins_release_number_input(app);
    DndCharacter* character = &app->data.character;
    switch(app->list_kind) {
    case DndListLanguages:
    case DndListProficiencies:
        return false;
    case DndListClasses:
        if(character->class_count >= DND_MAX_CLASSES ||
           dnd_rules_core_total_level(character) >= 20U)
            return false;
        app->record_index = character->class_count++;
        memset(&character->classes[app->record_index], 0, sizeof(DndClassLevel));
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
        character->classes[app->record_index].spellcasting_ability = DndAbilityIntelligence;
        break;
    case DndListFeatures: {
        if(!dndolphins_feature_prepare_append_page(app)) return false;
        const uint8_t local = character->feature_count;
        DndFeature* feature = &character->features[local];
        memset(feature, 0, sizeof(*feature));
        dndolphins_copy(feature->name, sizeof(feature->name), "New Feature");
        feature->class_index = 0U;
        feature->class_level_gained = character->classes[0].level;
        ++character->feature_count;
        app->record_index = app->features_total++;
        (void)dndolphins_save_features_if_changed(app);
        break;
    }
    }
    dndolphins_save(app, false);
    dndolphins_enter_screen(app, DndScreenRecordDetail);
    return true;
}

static void dndolphins_delete_record(DndDolphinsApp* app) {
    DndCharacter* character = &app->data.character;
    uint16_t index = app->record_index;
    switch(app->list_kind) {
    case DndListClasses:
        if(character->class_count <= 1U) {
            dndolphins_set_status(app, "Keep at least one class");
            return;
        }
        memmove(
            &character->classes[index],
            &character->classes[index + 1U],
            (character->class_count - index - 1U) * sizeof(DndClassLevel));
        --character->class_count;
        memset(&character->classes[character->class_count], 0, sizeof(DndClassLevel));
        if(!dndolphins_progression_store_features_remap_classes(
               app->storage, app->active_profile, (uint8_t)index)) {
            dndolphins_set_status(app, "Feature update failed");
            return;
        }
        if(!dnd_storage_remap_spell_classes(app->storage, app->active_profile, character, index)) {
            dndolphins_set_status(app, "Spellbook update failed");
            return;
        }
        if(app->collection_cache) app->collection_cache->spell_class_counts_valid = 0U;
        break;
    case DndListFeatures:
        if(index >= app->features_total || !dndolphins_save_features_if_changed(app) ||
           !dndolphins_progression_store_features_delete(
               app->storage, app->active_profile, index)) {
            dndolphins_set_status(app, "Feature delete failed");
            return;
        }
        --app->features_total;
        dnd_data_reserve_features_exact(character, 0U);
        character->feature_count = 0U;
        app->features_loaded = 0U;
        app->collection_cache->features_cache_start = 0U;
        if(app->features_total) {
            uint16_t target = index < app->features_total ? index :
                                                            (uint16_t)(app->features_total - 1U);
            if(!dndolphins_feature_cache_ensure(app, target)) {
                dndolphins_set_status(app, "Features read failed");
                return;
            }
        }
        break;
    case DndListLanguages:
        if(index >= app->language_total ||
           !dnd_character_languages_delete(app->storage, app->active_profile, index)) {
            dndolphins_set_status(app, "Language delete failed");
            return;
        }
        app->character_collections_changed = true;
        --app->language_total;
        app->collection_cache->language_page_count = 0U;
        app->collection_cache->language_cache_start = 0U;
        if(app->language_total) {
            uint16_t target = index < app->language_total ? index : app->language_total - 1U;
            if(!dndolphins_load_language_page(
                   app,
                   (target / DND_CHARACTER_COLLECTION_WINDOW) * DND_CHARACTER_COLLECTION_WINDOW)) {
                dndolphins_set_status(app, "Language read failed");
                return;
            }
        }
        break;
    case DndListProficiencies:
        if(index >= app->proficiency_total ||
           !dnd_character_proficiencies_delete(app->storage, app->active_profile, index)) {
            dndolphins_set_status(app, "Proficiency delete failed");
            return;
        }
        app->character_collections_changed = true;
        --app->proficiency_total;
        app->collection_cache->proficiency_page_count = 0U;
        app->collection_cache->proficiency_cache_start = 0U;
        if(app->proficiency_total) {
            uint16_t target = index < app->proficiency_total ? index : app->proficiency_total - 1U;
            if(!dndolphins_load_proficiency_page(
                   app,
                   (target / DND_CHARACTER_COLLECTION_WINDOW) * DND_CHARACTER_COLLECTION_WINDOW)) {
                dndolphins_set_status(app, "Proficiency read failed");
                return;
            }
        }
        break;
    }
    dndolphins_save(app, false);
    dndolphins_enter_screen(app, DndScreenRecordList);
    uint16_t remaining = app->list_kind == DndListFeatures      ? app->features_total :
                         app->list_kind == DndListLanguages     ? app->language_total :
                         app->list_kind == DndListProficiencies ? app->proficiency_total :
                                                                  0U;
    if(remaining) {
        uint16_t target = index < remaining ? index : remaining - 1U;
        dndolphins_record_list_focus(app, target);
    }
}

static void dndolphins_text_done(void* context) {
    DndDolphinsApp* app = context;
    app->input_module_active = 0U;
    DndCharacter* character = &app->data.character;
    uint16_t index = app->record_index;
    DndEditTarget completed_target = app->edit_target;
    switch(app->edit_target) {
    case DndEditCharacterName:
        dndolphins_copy(character->name, sizeof(character->name), app->edit_buffer);
        break;
    case DndEditPlayerName:
        dndolphins_copy(character->player, sizeof(character->player), app->edit_buffer);
        break;
    case DndEditSpecies:
        dndolphins_copy(character->species, sizeof(character->species), app->edit_buffer);
        break;
    case DndEditBackground:
        dndolphins_copy(character->background, sizeof(character->background), app->edit_buffer);
        break;
    case DndEditAlignment:
        dndolphins_copy(character->alignment, sizeof(character->alignment), app->edit_buffer);
        break;
    case DndEditOriginFeat:
        dndolphins_copy(character->origin_feat, sizeof(character->origin_feat), app->edit_buffer);
        break;
    case DndEditSenses:
        dndolphins_copy(character->senses, sizeof(character->senses), app->edit_buffer);
        break;
    case DndEditConditions:
        dndolphins_copy(character->conditions, sizeof(character->conditions), app->edit_buffer);
        break;
    case DndEditConcentration:
        dndolphins_copy(
            character->concentration, sizeof(character->concentration), app->edit_buffer);
        break;
    case DndEditTemporaryEffects:
        dndolphins_copy(
            character->temporary_effects, sizeof(character->temporary_effects), app->edit_buffer);
        break;
    case DndEditResistances:
        dndolphins_copy(character->resistances, sizeof(character->resistances), app->edit_buffer);
        break;
    case DndEditImmunities:
        dndolphins_copy(character->immunities, sizeof(character->immunities), app->edit_buffer);
        break;
    case DndEditVulnerabilities:
        dndolphins_copy(
            character->vulnerabilities, sizeof(character->vulnerabilities), app->edit_buffer);
        break;
    case DndEditMovementModes:
        dndolphins_copy(
            character->movement_modes, sizeof(character->movement_modes), app->edit_buffer);
        break;
    case DndEditClassName:
        dndolphins_copy(
            character->classes[index].name,
            sizeof(character->classes[index].name),
            app->edit_buffer);
        dndolphins_configure_class_defaults(&character->classes[index]);
        dndolphins_spells_initialize_spell_slots_if_unset(character);
        dndolphins_spells_apply_level_progression(character, index);
        break;
    case DndEditSubclass:
        dndolphins_copy(
            character->classes[index].subclass,
            sizeof(character->classes[index].subclass),
            app->edit_buffer);
        dndolphins_spells_refresh_class_spellcasting(&character->classes[index]);
        dndolphins_spells_initialize_spell_slots_if_unset(character);
        dndolphins_spells_apply_level_progression(character, index);
        break;
    case DndEditGrantStableId:
        if(index < character->grant_count)
            dndolphins_copy(
                character->grants[index].stable_id,
                sizeof(character->grants[index].stable_id),
                app->edit_buffer);
        break;
    case DndEditGrantSource:
        if(index < character->grant_count)
            dndolphins_copy(
                character->grants[index].source,
                sizeof(character->grants[index].source),
                app->edit_buffer);
        break;
    case DndEditGrantOption:
        if(index < character->grant_count)
            dndolphins_copy(
                character->grants[index].option_name,
                sizeof(character->grants[index].option_name),
                app->edit_buffer);
        break;
    case DndEditGrantPrerequisites:
        if(index < character->grant_count)
            dndolphins_copy(
                character->grants[index].prerequisites,
                sizeof(character->grants[index].prerequisites),
                app->edit_buffer);
        break;
    case DndEditGrantValue:
        if(index < character->grant_count)
            dndolphins_copy(
                character->grants[index].grant_value,
                sizeof(character->grants[index].grant_value),
                app->edit_buffer);
        break;
    case DndEditAttackName:
        if(index < character->attack_template_count)
            dndolphins_copy(
                character->attack_templates[index].name,
                sizeof(character->attack_templates[index].name),
                app->edit_buffer);
        break;
    case DndEditAttackMastery:
        if(index < character->attack_template_count)
            dndolphins_copy(
                character->attack_templates[index].mastery,
                sizeof(character->attack_templates[index].mastery),
                app->edit_buffer);
        break;
    case DndEditAttackDamageType:
        if(index < character->attack_template_count)
            dndolphins_copy(
                character->attack_templates[index].damage_type,
                sizeof(character->attack_templates[index].damage_type),
                app->edit_buffer);
        break;
    case DndEditAttackRiderType:
        if(index < character->attack_template_count)
            dndolphins_copy(
                character->attack_templates[index].rider_type,
                sizeof(character->attack_templates[index].rider_type),
                app->edit_buffer);
        break;
    case DndEditFeatureName: {
        DndFeature* feature = dndolphins_feature_at(app, index, NULL);
        if(feature) {
            dndolphins_copy(feature->name, sizeof(feature->name), app->edit_buffer);
            (void)dndolphins_save_features_if_changed(app);
        }
        break;
    }
    case DndEditFeatureDetail: {
        DndFeature* feature = dndolphins_feature_at(app, index, NULL);
        if(feature) {
            dndolphins_copy(feature->detail, sizeof(feature->detail), app->edit_buffer);
            (void)dndolphins_save_features_if_changed(app);
        }
        break;
    }
    case DndEditLanguageName:
    case DndEditProficiencyName:
        break;
    case DndEditNone:
        break;
    default:
        break;
    }
    app->edit_target = DndEditNone;
    switch(completed_target) {
    default:
        break;
    }
    dndolphins_save(app, false);
    dndolphins_input_hook_release(app);
    view_dispatcher_switch_to_view(app->dispatcher, DndViewMain);
    view_dispatcher_send_custom_event(app->dispatcher, DNDOLPHINS_RELEASE_INPUT_EVENT);
    dndolphins_refresh(app);
}

static bool dndolphins_is_move_event(const InputEvent* event) {
    return event->type == InputTypeShort || event->type == InputTypeRepeat;
}

static void dndolphins_handle_back(DndDolphinsApp* app) {
    switch(app->screen) {
    case DndScreenHome:
        dndolphins_flush_save(app, false);
        view_dispatcher_stop(app->dispatcher);
        break;
    case DndScreenRecordList:
        dndolphins_enter_screen(app, app->record_list_return_screen);
        break;
    case DndScreenProfileActions:
        dndolphins_enter_screen(app, DndScreenProfiles);
        break;
    case DndScreenSettings:
        dndolphins_enter_screen(app, DndScreenHome);
        break;
    case DndScreenRecordDetail:
        dndolphins_enter_screen(app, DndScreenRecordList);
        {
            app->selection = app->record_index + 1U;
            if(app->selection >= 5U) app->scroll = app->selection - 4U;
        }
        break;
    case DndScreenCatalog: {
        const uint16_t return_selection = app->catalog ? app->catalog->return_selection : 0U;
        const DndEditTarget catalog_target = app->catalog ? app->catalog->target : DndEditNone;
        if(app->grant_review && app->grant_review->grant_choice_active) {
            app->grant_review->grant_choice_active = 0U;
            app->grant_review->grant_choice_kind = DndGrantChoiceNone;
        }
        if(app->level_choice_mode == 3U && catalog_target == DndEditFeatureName) {
            DndCharacter* c = &app->data.character;
            dnd_data_reserve_features_exact(c, 0U);
            c->feature_count = 0U;
            app->level_choice_mode = 0U;
        }
        dndolphins_enter_screen(app, app->return_screen);
        app->selection = return_selection;
        if(app->selection >= 5U) app->scroll = app->selection - 4U;
        break;
    }
#if defined(DND_BUILD_COMBAT)
    case DndScreenMagic:
        app->combat->arcane_recovery_active = 0U;
        dndolphins_enter_screen(app, DndScreenCombat);
        break;
#endif
    case DndScreenGrantReview:
        dndolphins_release_staged_records(app);
        app->grant_review->batches = 0U;
        app->grant_review->include_background = 0U;
        app->grant_review->maximum_level = 0U;
        app->grant_review->dependency_scan_needed = 0U;
        app->grant_review->dependency_rescan_needed = 0U;
        app->grant_review->scan_complete = 0U;
        app->grant_review->metadata_offset = 0U;
        app->grant_review->dependency_metadata_offset = 0U;
#if defined(DND_BUILD_GRANTS)
        app->return_to_parent = 1U;
        view_dispatcher_stop(app->dispatcher);
#else
        dndolphins_enter_screen(app, app->return_screen);
#endif
        break;

    case DndScreenGrantDiagnostics:
        dndolphins_enter_screen(app, DndScreenGrantReview);
        break;
    case DndScreenGrantEdit:
        dndolphins_enter_screen(app, DndScreenGrantReview);
        break;
    case DndScreenLevelReview:
        dndolphins_enter_screen(app, DndScreenRecordDetail);
        app->selection = 2U;
        app->scroll = 0U;
        break;
    case DndScreenLevelChoice:
    case DndScreenAsiAbility:
        app->level_choice_first_ability = UINT8_MAX;
        app->level_choice_first_score = 0;
        dndolphins_enter_screen(app, app->return_screen);
        break;
    case DndScreenCombat:
#if defined(DND_BUILD_COMBAT)
        app->return_to_parent = 1U;
        view_dispatcher_stop(app->dispatcher);
#else
        dndolphins_enter_screen(app, DndScreenHome);
#endif
        break;
    case DndScreenSpellAttacks:
    case DndScreenFavoriteSpells:
    case DndScreenUtilitySpells:
    case DndScreenRituals:
        dndolphins_enter_screen(app, DndScreenCombat);
        break;
    case DndScreenSpellCast:
        dndolphins_enter_screen(app, dndolphins_combat_spell_return_screen(app));
        break;
    case DndScreenSpellResult:
        dndolphins_enter_screen(app, dndolphins_combat_spell_return_screen(app));
        break;
    case DndScreenAttackTemplates:
        dndolphins_enter_screen(app, DndScreenCombat);
        break;
    case DndScreenAttackTemplateEdit:
        dndolphins_enter_screen(app, DndScreenAttackTemplates);
        break;
    case DndScreenAttackList:
    case DndScreenAmmoChoice:
    case DndScreenAttackResult:
        dndolphins_enter_screen(app, DndScreenCombat);
        break;
    case DndScreenDiceResult:
        dndolphins_enter_screen(app, DndScreenDice);
        break;
    default:
        dndolphins_enter_screen(app, DndScreenHome);
        break;
    }
}

static void dndolphins_handle_long_back(DndDolphinsApp* app) {
#if defined(DND_BUILD_COMBAT) || defined(DND_BUILD_GRANTS)
    app->return_to_parent = 0U;
    dndolphins_flush_save(app, false);
    dndolphins_catalog_runtime_release(app);
    view_dispatcher_stop(app->dispatcher);
    return;
#else
    if(app->screen == DndScreenHome) {
        dndolphins_flush_save(app, false);
        view_dispatcher_stop(app->dispatcher);
        return;
    }
    if(app->roll) app->roll->dice_animating = 0U;
    dndolphins_catalog_runtime_release(app);
    dndolphins_enter_screen(app, DndScreenHome);
    app->marquee_elapsed_ms = 0U;
#endif
}

static void dndolphins_handle_profiles(DndDolphinsApp* app, const InputEvent* event) {
    uint16_t profile_count = dndolphins_profile_count(app);
    uint16_t row_count = profile_count + 1U;
    if(dndolphins_is_move_event(event) && event->key == InputKeyUp) {
        uint16_t previous_scroll = app->scroll;
        dndolphins_menu_move(app, row_count, -1);
        if(app->scroll != previous_scroll && app->scroll < profile_count)
            (void)dnd_storage_profiles_window(app->storage, app->profile_browser, app->scroll);
    } else if(dndolphins_is_move_event(event) && event->key == InputKeyDown) {
        uint16_t previous_scroll = app->scroll;
        dndolphins_menu_move(app, row_count, 1);
        if(app->scroll != previous_scroll && app->scroll < profile_count)
            (void)dnd_storage_profiles_window(app->storage, app->profile_browser, app->scroll);
    } else if(event->type == InputTypeShort && event->key == InputKeyOk) {
        if(app->selection == profile_count)
            dndolphins_create_profile(app);
        else
            dndolphins_switch_profile(app, dndolphins_profile_id_at(app, app->selection));
    } else if(
        event->type == InputTypeLong && event->key == InputKeyOk &&
        app->selection < profile_count) {
        app->profile_action_id = dndolphins_profile_id_at(app, app->selection);
        dndolphins_enter_screen(app, DndScreenProfileActions);
    }
}

static void dndolphins_profile_actions_to_list(DndDolphinsApp* app) {
    dndolphins_enter_screen(app, DndScreenProfiles);
}

static void dndolphins_handle_profile_actions(DndDolphinsApp* app, const InputEvent* event) {
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
            if(profile != app->active_profile)
                dndolphins_set_status(app, "Switch before rename");
            else
                dndolphins_begin_text(
                    app, DndEditCharacterName, "Character name", app->data.character.name);
        } else if(app->selection == 2U) {
            uint32_t destination = dnd_storage_profiles_next_id(app->profile_browser);
            bool duplicated =
                (profile != app->active_profile || dndolphins_flush_save(app, false)) &&
                !(destination == UINT32_MAX && dndolphins_profile_exists(app, UINT32_MAX)) &&
                dnd_storage_duplicate_profile(app->storage, profile, destination) &&
                dndolphins_profile_browser_refresh(app) && dndolphins_profile_browser_save(app);
            dndolphins_profile_actions_to_list(app);
            dndolphins_set_status(app, duplicated ? "Character duplicated" : "Duplicate failed");
        } else if(app->selection == 3U) {
            if(profile == app->active_profile) {
                dndolphins_set_status(app, "Switch before archive");
            } else {
                bool archived = dnd_storage_archive_profile(app->storage, profile) &&
                                dndolphins_profile_browser_refresh(app) &&
                                dndolphins_profile_browser_save(app);
                dndolphins_profile_actions_to_list(app);
                dndolphins_set_status(app, archived ? "Character archived" : "Archive failed");
            }
        } else if(app->selection == 4U) {
            bool deleted = dndolphins_delete_profile(app, profile);
            dndolphins_profile_actions_to_list(app);
            dndolphins_set_status(app, deleted ? "Character deleted" : "Delete failed");
        } else if(app->selection == 5U) {
            bool verified =
                (profile != app->active_profile || dndolphins_flush_save(app, false)) &&
                dnd_storage_verify_profile(app->storage, profile);
            if(verified)
                dndolphins_confirm_action(app, "Profile readable");
            else
                dndolphins_set_status(app, "Save damaged/incompatible");
        } else if(app->selection == 6U) {
            if(profile != app->active_profile)
                dndolphins_set_status(app, "Switch before backup");
            else if(!dndolphins_flush_save(app, false))
                dndolphins_set_status(app, "Save before backup failed");
            else
                dndolphins_request_launch(app, DndPendingLaunchBackup);
        }
    }
}

static void dndolphins_handle_settings(DndDolphinsApp* app, const InputEvent* event) {
    if(dndolphins_is_move_event(event) && event->key == InputKeyUp)
        dndolphins_menu_move(app, 6U, -1);
    else if(dndolphins_is_move_event(event) && event->key == InputKeyDown)
        dndolphins_menu_move(app, 6U, 1);
    else if(
        (dndolphins_is_move_event(event) &&
         (event->key == InputKeyLeft || event->key == InputKeyRight)) ||
        (event->type == InputTypeShort && event->key == InputKeyOk)) {
        DndSettings previous = app->settings;
        if(app->selection == 0U)
            app->settings.skip_dice_loading = !app->settings.skip_dice_loading;
        else if(app->selection == 1U)
            app->settings.debug = !app->settings.debug;
        else if(app->selection == 2U)
            app->settings.extra_items = !app->settings.extra_items;
        else if(app->selection == 3U) {
            if(!app->catalog_all_available) {
                app->settings.catalog_all = 0U;
                dndolphins_set_status(app, "Catalog: SRD only");
                return;
            }
            app->settings.catalog_all = !app->settings.catalog_all;
        } else if(app->selection == 4U)
            app->settings.homebrew = !app->settings.homebrew;
        else if(app->selection == 5U)
            app->settings.menu_type = app->settings.menu_type == DndMenuTypeGraphical ?
                                          DndMenuTypeText :
                                          DndMenuTypeGraphical;
        else
            return;
        if(!dnd_settings_save(app->storage, &app->settings)) {
            app->settings = previous;
            dndolphins_set_status(app, "Settings save failed");
            return;
        }
        if(app->selection == 2U) {
            bool ok =
                !app->settings.extra_items || !app->active_profile_loaded ||
                (dndolphins_save_items_if_changed(app) &&
                 dnd_extra_items_grant(app->storage, app->active_profile, &app->data.character));
            if(!ok) {
                app->settings = previous;
                (void)dnd_settings_save(app->storage, &previous);
                dndolphins_set_status(app, "Get Elevated grant failed");
                return;
            }
            if(app->collection_cache) app->collection_cache->items_offset_valid_pages = 0U;
            if(app->items_loaded) (void)dndolphins_load_items_page(app, 0U);
            dndolphins_set_status(
                app, app->settings.extra_items ? "Get Elevated: 420" : "Get Elevated: Off");
        } else if(app->selection == 0U)
            dndolphins_set_status(
                app,
                app->settings.skip_dice_loading ? "Dice animation skipped" :
                                                  "Dice animation enabled");
        else if(app->selection == 1U)
            dndolphins_set_status(app, app->settings.debug ? "Debug enabled" : "Debug disabled");
        else if(app->selection == 3U)
            dndolphins_set_status(
                app, app->settings.catalog_all ? "Catalog: All" : "Catalog: SRD");
        else if(app->selection == 4U)
            dndolphins_set_status(app, app->settings.homebrew ? "Homebrew: Yes" : "Homebrew: No");
        else if(app->selection == 5U)
            dndolphins_set_status(
                app,
                app->settings.menu_type == DndMenuTypeGraphical ? "Menu Type: Graphical" :
                                                                  "Menu Type: Text");
        if(app->settings.debug)
            FURI_LOG_I(
                TAG,
                "Settings: SkipDiceLoading=%u Debug=%u CatalogAll=%u Homebrew=%u MenuType=%u",
                app->settings.skip_dice_loading,
                app->settings.debug,
                app->settings.catalog_all,
                app->settings.homebrew,
                app->settings.menu_type);
    }
}

static void dndolphins_request_launch(DndDolphinsApp* app, DndPendingLaunch launch) {
    if(!app) return;

    /* Preserve real character changes before tearing the app down. A missing
       active character is not a launch blocker. Companion apps resolve the
       exact persisted Active= profile themselves. */
    if(app->active_profile_loaded && !dndolphins_flush_save(app, false) &&
       launch != DndPendingLaunchBestiary) {
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

#if defined(DND_BUILD_HUB)
static void dndolphins_home_graphical_move(DndDolphinsApp* app, uint16_t count, int key) {
    if(!app || !count) return;
    uint16_t selected = app->selection < count ? app->selection : 0U;
    if(key == InputKeyLeft) {
        selected = selected ? selected - 1U : count - 1U;
    } else if(key == InputKeyRight) {
        selected = selected + 1U < count ? selected + 1U : 0U;
    } else if(key == InputKeyUp) {
        if(selected >= 4U) {
            selected -= 4U;
        } else {
            uint16_t target = selected;
            while(target + 4U < count)
                target += 4U;
            selected = target;
        }
    } else if(key == InputKeyDown) {
        if(selected + 4U < count) {
            selected += 4U;
        } else {
            uint16_t target = selected % 4U;
            selected = target < count ? target : 0U;
        }
    } else {
        return;
    }
    app->selection = selected;
    app->scroll = selected >= 5U ? (uint16_t)(selected - 4U) : 0U;
    dndolphins_marquee_offset = 0U;
}
#endif

static void dndolphins_handle_home(DndDolphinsApp* app, const InputEvent* event) {
    uint16_t count = dndolphins_home_count(app);
#if defined(DND_BUILD_HUB)
    if(app->settings.menu_type == DndMenuTypeGraphical && dndolphins_is_move_event(event) &&
       (event->key == InputKeyUp || event->key == InputKeyDown || event->key == InputKeyLeft ||
        event->key == InputKeyRight)) {
        dndolphins_home_graphical_move(app, count, event->key);
        return;
    }
#endif
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
            dndolphins_profile_browser_refresh(app);
            dndolphins_profile_include_active(app);
            dndolphins_enter_screen(app, DndScreenProfiles);
            break;
        case DndolphinsHomeCharacter:
            dndolphins_enter_screen(app, DndScreenCharacter);
            break;
        case DndolphinsHomeVitals:
            dndolphins_enter_screen(app, DndScreenVitals);
            break;
        case DndolphinsHomeAbilitiesSaves:
            dndolphins_enter_screen(app, DndScreenAbilities);
            break;
        case DndolphinsHomeSkills:
            dndolphins_enter_screen(app, DndScreenSkills);
            break;
        case DndolphinsHomeFeaturesPerks:
            dndolphins_open_list(app, DndListFeatures, DndScreenHome);
            break;
        case DndolphinsHomeInventory:
            dndolphins_request_launch(app, DndPendingLaunchInventory);
            break;
        case DndolphinsHomeMagicSpells:
            dndolphins_request_launch(app, DndPendingLaunchSpellbookMagic);
            break;
        case DndolphinsHomeBestiary:
            dndolphins_request_launch(app, DndPendingLaunchBestiary);
            break;
        case DndolphinsHomeInitiative:
            dndolphins_request_launch(app, DndPendingLaunchInitiative);
            break;
        case DndolphinsHomeCombat:
#if defined(DND_BUILD_COMBAT)
            app->combat->hit_die_class_index = 0U;
            if(app->roll->roll_mode == DndRollGuidance) app->roll->roll_mode = DndRollNormal;
            dndolphins_enter_screen(app, DndScreenCombat);
#else
            dndolphins_request_launch(app, DndPendingLaunchCombat);
#endif
            break;
        case DndolphinsHomeDiceRoller:
            if(!dndolphins_roll_runtime_alloc(app)) {
                dndolphins_set_status(app, "Roll memory unavailable");
                break;
            }
            app->roll->quick_roll_active = 0U;
            app->roll->quick_roll_label[0] = '\0';
            dndolphins_enter_screen(app, DndScreenDice);
            break;
        case DndolphinsHomeAdventure:
            dndolphins_request_launch(app, DndPendingLaunchAdventure);
            break;
        case DndolphinsHomeJournal:
            dndolphins_request_launch(app, DndPendingLaunchJournal);
            break;
        case DndolphinsHomeSettings:
            app->catalog_all_available =
                dndolphins_catalog_all_files_available(app->storage) ? 1U : 0U;
            if(!app->catalog_all_available && app->settings.catalog_all) {
                app->settings.catalog_all = 0U;
                (void)dnd_settings_save(app->storage, &app->settings);
            }
            dndolphins_enter_screen(app, DndScreenSettings);
            break;
        case DndolphinsHomeCount:
            break;
        }
    }
}

static void dndolphins_handle_character(DndDolphinsApp* app, const InputEvent* event) {
    DndCharacter* character = &app->data.character;
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
            DndNumberCharacter,
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
            dndolphins_begin_text(app, DndEditSpecies, "Custom species", character->species);
        else if(app->selection == 3U)
            dndolphins_begin_text(
                app, DndEditBackground, "Custom background", character->background);
        else
            dndolphins_begin_text(app, DndEditAlignment, "Custom alignment", character->alignment);
    } else if(event->type == InputTypeShort && event->key == InputKeyOk) {
        switch(app->selection) {
        case 0:
            dndolphins_begin_text(app, DndEditCharacterName, "Character name", character->name);
            break;
        case 1:
            dndolphins_begin_text(app, DndEditPlayerName, "Player name", character->player);
            break;
        case 2:
            dndolphins_open_catalog(app, DndCatalogSpecies, DndEditSpecies, character->species);
            break;
        case 3:
            dndolphins_open_catalog(
                app, DndCatalogBackgrounds, DndEditBackground, character->background);
            break;
        case 4:
            dndolphins_open_catalog(
                app, DndCatalogAlignments, DndEditAlignment, character->alignment);
            break;
        case 5:
            dndolphins_open_list(app, DndListClasses, DndScreenCharacter);
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
            dndolphins_open_list(app, DndListLanguages, DndScreenCharacter);
            break;
        case 10:
            dndolphins_open_list(app, DndListProficiencies, DndScreenCharacter);
            break;
        case 11:
            character->inspiration = !character->inspiration;
            dndolphins_save(app, false);
            break;
        case 12:
            (void)dndolphins_begin_next_level_choice(app);
            app->return_screen = DndScreenCharacter;
            dndolphins_enter_screen(app, DndScreenLevelChoice);
            app->selection = 0U;
            app->scroll = 0U;
            if(!app->level_choice_level) dndolphins_set_status(app, "No pending ASI/Feat choices");
            break;
        case 13:
#if defined(DND_BUILD_GRANTS)
            app->deferred_action = DndDeferredActionApplyLevelGrants;
            dndolphins_run_deferred_action(app);
#else
            dndolphins_request_launch(app, DndPendingLaunchGrantsLevel);
#endif
            break;
        case 14:
            dndolphins_request_launch(app, DndPendingLaunchCharacterSheet);
            break;
        default:
            break;
        }
    }
}

static void dndolphins_handle_vitals(DndDolphinsApp* app, const InputEvent* event) {
    DndCharacter* character = &app->data.character;
    if(dndolphins_is_move_event(event) && event->key == InputKeyUp)
        dndolphins_menu_move(app, 17U, -1);
    else if(dndolphins_is_move_event(event) && event->key == InputKeyDown)
        dndolphins_menu_move(app, 17U, 1);
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
            character->hp_temporary =
                dndolphins_clamp_i16(character->hp_temporary + delta, 0, 999);
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
            character->death_successes =
                dndolphins_clamp_u8(character->death_successes + delta, 3U);
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
            app, DndNumberVitals, (uint8_t)app->selection, 0U, header, value, minimum, maximum);
    }
}

static void dndolphins_handle_abilities(DndDolphinsApp* app, const InputEvent* event) {
    DndCharacter* character = &app->data.character;
    if(dndolphins_is_move_event(event) && event->key == InputKeyUp) {
        dndolphins_menu_move(app, DND_ABILITY_COUNT, -1);
    } else if(dndolphins_is_move_event(event) && event->key == InputKeyDown) {
        dndolphins_menu_move(app, DND_ABILITY_COUNT, 1);
    } else if(
        dndolphins_is_move_event(event) &&
        (event->key == InputKeyLeft || event->key == InputKeyRight)) {
        app->roll->ability_roll_target = event->key == InputKeyRight ? 1U : 0U;
    } else if(event->type == InputTypeShort && event->key == InputKeyOk) {
        uint8_t index = (uint8_t)app->selection;
        char label[32];
        if(app->roll->ability_roll_target) {
            snprintf(label, sizeof(label), "%s Save", dnd_rules_core_ability_names[index]);
            dndolphins_begin_quick_roll(
                app, label, dnd_rules_core_saving_throw_modifier(character, index), true);
        } else {
            snprintf(label, sizeof(label), "%s Check", dnd_rules_core_ability_names[index]);
            dndolphins_begin_quick_roll(
                app,
                label,
                dnd_rules_core_ability_modifier(character->ability_scores[index]),
                false);
        }
    } else if(event->type == InputTypeLong && event->key == InputKeyOk) {
        uint8_t index = (uint8_t)app->selection;
        if(app->roll->ability_roll_target) {
            character->saving_throw_proficiency[index] =
                (character->saving_throw_proficiency[index] + 1U) % 2U;
            dndolphins_save(app, false);
            dndolphins_set_status(
                app,
                character->saving_throw_proficiency[index] ? "Save proficiency: Yes" :
                                                             "Save proficiency: No");
        } else {
            dndolphins_begin_number(
                app,
                DndNumberAbility,
                index,
                0U,
                "Ability score",
                character->ability_scores[index],
                1,
                30);
        }
    }
}

static void dndolphins_handle_skills(DndDolphinsApp* app, const InputEvent* event) {
    DndCharacter* character = &app->data.character;
    if(dndolphins_is_move_event(event) && event->key == InputKeyUp)
        dndolphins_menu_move(app, DND_SKILL_COUNT, -1);
    else if(dndolphins_is_move_event(event) && event->key == InputKeyDown)
        dndolphins_menu_move(app, DND_SKILL_COUNT, 1);
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
            if(proficiency < 0) proficiency = DndProficiencyExpertise;
            if(proficiency > DndProficiencyExpertise) proficiency = DndProficiencyNone;
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
            DndNumberSkill,
            index,
            0U,
            "Skill misc modifier",
            character->skill_misc[index],
            -20,
            20);
    } else if(event->type == InputTypeShort && event->key == InputKeyOk) {
        uint8_t index = dndolphins_skill_display_order[app->selection];
        dndolphins_begin_quick_roll(
            app,
            dnd_rules_core_skill_names[index],
            dnd_rules_core_skill_modifier(character, index),
            false);
    }
}

static void dndolphins_handle_level_review(DndDolphinsApp* app, const InputEvent* event) {
    if(event->key != InputKeyOk) return;
    if(event->type == InputTypeLong) {
#if defined(DND_BUILD_GRANTS)
        dndolphins_schedule_deferred_action(app, DndDeferredActionApplyLevelGrants);
#else
        dndolphins_request_launch(app, DndPendingLaunchGrantsLevel);
#endif
        return;
    }
    if(event->type != InputTypeShort) return;
    if(app->level_review_pending_choice) {
        app->return_screen = DndScreenRecordDetail;
        dndolphins_enter_screen(app, DndScreenLevelChoice);
        app->selection = 0U;
        app->scroll = 0U;
    } else {
        dndolphins_enter_screen(app, DndScreenRecordDetail);
        app->selection = 2U;
        app->scroll = 0U;
    }
}

static void dndolphins_handle_level_choice(DndDolphinsApp* app, const InputEvent* event) {
    if(!app->level_choice_level) {
        if(event->type == InputTypeShort && event->key == InputKeyOk)
            dndolphins_enter_screen(app, app->return_screen);
        return;
    }
    if(dndolphins_is_move_event(event) && event->key == InputKeyUp)
        dndolphins_menu_move(app, 4U, -1);
    else if(dndolphins_is_move_event(event) && event->key == InputKeyDown)
        dndolphins_menu_move(app, 4U, 1);
    else if(event->type == InputTypeShort && event->key == InputKeyOk) {
        if(app->selection == 0U || app->selection == 1U) {
            app->level_choice_mode = app->selection == 0U ? 1U : 2U;
            app->level_choice_first_ability = UINT8_MAX;
            app->level_choice_first_score = 0;
            dndolphins_enter_screen(app, DndScreenAsiAbility);
        } else if(app->selection == 2U) {
            DndCharacter* c = &app->data.character;
            if(app->features_loaded && !dndolphins_release_features(app)) {
                dndolphins_set_status(app, "Feature save failed");
                return;
            }
            uint16_t feature_total = 0U;
            if(!dndolphins_progression_store_features_count(
                   app->storage, app->active_profile, &feature_total)) {
                dndolphins_set_status(app, "Feature count failed");
                return;
            }
            if(!dnd_data_reserve_features_exact(c, 1U)) {
                dndolphins_set_status(app, "Feature list full");
                return;
            }
            c->feature_count = 1U;
            app->record_index = 0U;
            memset(&c->features[0], 0, sizeof(DndFeature));
            c->features[0].class_index = app->level_choice_class_index;
            c->features[0].class_level_gained = app->level_choice_level;
            app->level_choice_mode = 3U;
            dndolphins_open_catalog(app, DndCatalogFeats, DndEditFeatureName, "");
        } else {
            app->level_choice_mode = 0U;
            dndolphins_enter_screen(app, app->return_screen);
            dndolphins_set_status(app, "Level choice left pending");
        }
    }
}

static void dndolphins_handle_asi_ability(DndDolphinsApp* app, const InputEvent* event) {
    DndCharacter* c = &app->data.character;
    if(dndolphins_is_move_event(event) && event->key == InputKeyUp)
        dndolphins_menu_move(app, DND_ABILITY_COUNT, -1);
    else if(dndolphins_is_move_event(event) && event->key == InputKeyDown)
        dndolphins_menu_move(app, DND_ABILITY_COUNT, 1);
    else if(event->type == InputTypeShort && event->key == InputKeyOk) {
        uint8_t ability = (uint8_t)app->selection;
        if(app->level_choice_mode == 1U) {
            if(c->ability_scores[ability] >= 20) {
                dndolphins_set_status(app, "Ability already 20");
                return;
            }
            int8_t old_score = c->ability_scores[ability];
            int16_t next = old_score + 2;
            c->ability_scores[ability] = next > 20 ? 20 : (int8_t)next;
            if(!dndolphins_complete_level_choice(app, "ASI +2")) {
                c->ability_scores[ability] = old_score;
                dndolphins_set_status(app, "Could not record choice");
                return;
            }
        } else if(app->level_choice_mode == 2U) {
            if(c->ability_scores[ability] >= 20) {
                dndolphins_set_status(app, "Ability already 20");
                return;
            }
            if(app->level_choice_first_ability >= DND_ABILITY_COUNT) {
                /* The first pick is selection-only. Do not mutate either score
                   until the second, different ability is confirmed. */
                app->level_choice_first_ability = ability;
                app->level_choice_first_score = c->ability_scores[ability];
                dndolphins_set_status(app, "Choose second ability");
                return;
            }
            if(ability == app->level_choice_first_ability) {
                dndolphins_set_status(app, "Choose different ability");
                return;
            }
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
        } else
            return;
        dndolphins_save(app, false);
        app->level_choice_mode = 0U;
        app->level_choice_first_ability = UINT8_MAX;
        app->level_choice_first_score = 0;
        dndolphins_enter_screen(app, app->return_screen);
        dndolphins_set_status(app, "Level choice applied");
    }
}

static void dndolphins_handle_grant_diagnostics(DndDolphinsApp* app, const InputEvent* event) {
    uint8_t count = app->data.character.grant_count ? app->data.character.grant_count : 1U;
    if(dndolphins_is_move_event(event) && event->key == InputKeyUp)
        dndolphins_menu_move(app, count, -1);
    else if(dndolphins_is_move_event(event) && event->key == InputKeyDown)
        dndolphins_menu_move(app, count, 1);
    else if(
        event->type == InputTypeShort && event->key == InputKeyOk &&
        app->data.character.grant_count) {
        const DndGrant* g = &app->data.character.grants[app->selection];
        snprintf(
            app->status,
            sizeof(app->status),
            "%.12s | %.14s",
            g->stable_id[0] ? g->stable_id : "no-id",
            g->grant_value);
    }
}

static void dndolphins_handle_grant_review(DndDolphinsApp* app, const InputEvent* event) {
    DndCharacter* c = &app->data.character;
    uint16_t count = c->grant_count + 2U + (app->settings.debug ? 1U : 0U);
    if(dndolphins_is_move_event(event) && event->key == InputKeyUp)
        dndolphins_menu_move(app, count, -1);
    else if(dndolphins_is_move_event(event) && event->key == InputKeyDown)
        dndolphins_menu_move(app, count, 1);

    else if(
        event->type == InputTypeLong && event->key == InputKeyLeft && app->selection &&
        app->selection <= c->grant_count) {
        DndGrant* grant = &c->grants[app->selection - 1U];
        if(grant->status == DndGrantPending) grant->status = DndGrantSkipped;
        dndolphins_save(app, false);
    } else if(
        event->type == InputTypeLong && event->key == InputKeyOk && app->selection &&
        app->selection <= c->grant_count) {
        app->record_index = app->selection - 1U;
        dndolphins_enter_screen(app, DndScreenGrantEdit);
    } else if(event->type == InputTypeShort && event->key == InputKeyOk) {
        if(app->selection == 0U) {
            uint8_t applied = 0U;
            uint8_t choices = 0U;
            const uint8_t initial_count = c->grant_count;
            for(uint8_t i = 0U; i < initial_count; ++i) {
                if(c->grants[i].status != DndGrantPending) continue;
                if(dndolphins_grant_choice_kind(&c->grants[i]) != DndGrantChoiceNone) {
                    ++choices;
                    continue;
                }
                dndolphins_apply_grant(app, &c->grants[i]);
                if(c->grants[i].status == DndGrantApplied) ++applied;
            }
            if(choices)
                snprintf(
                    app->status, sizeof(app->status), "%u applied; choose %u", applied, choices);
            else
                snprintf(app->status, sizeof(app->status), "%u grants applied", applied);
            dndolphins_save(app, false);
            (void)dndolphins_advance_grant_review_if_complete(app);
        } else if(app->selection <= c->grant_count) {
            DndGrant* grant = &c->grants[app->selection - 1U];
            if(grant->status == DndGrantPending) {
                DndGrantChoiceKind choice = dndolphins_grant_choice_kind(grant);
                if(choice != DndGrantChoiceNone) {
                    dndolphins_open_grant_choice(app, app->selection - 1U);
                    return;
                }
                dndolphins_apply_grant(app, grant);
                dndolphins_save(app, false);
                if(grant->status == DndGrantApplied)
                    dndolphins_set_status(app, "Applied (A)");
                else
                    dndolphins_set_status(app, "Grant needs review");
                (void)dndolphins_advance_grant_review_if_complete(app);
            } else if(grant->status == DndGrantSkipped) {
                grant->status = DndGrantPending;
                dndolphins_save(app, false);
                dndolphins_set_status(app, "Pending again");
            }
        } else if(app->settings.debug && app->selection == c->grant_count + 2U) {
            app->selection = 0U;
            app->scroll = 0U;
            dndolphins_enter_screen(app, DndScreenGrantDiagnostics);
        } else if(
            app->selection == c->grant_count + 1U && c->grant_count < DND_MAX_GRANTS &&
            dnd_data_reserve_grants(c, c->grant_count + 1U)) {
            app->record_index = c->grant_count++;
            DndGrant* grant = &c->grants[app->record_index];
            memset(grant, 0, sizeof(*grant));
            snprintf(
                grant->stable_id,
                sizeof(grant->stable_id),
                "custom_grant_%u",
                app->record_index + 1U);
            dndolphins_copy(grant->source, sizeof(grant->source), "Custom");
            dndolphins_copy(grant->option_name, sizeof(grant->option_name), "Custom Grant");
            dndolphins_copy(grant->prerequisites, sizeof(grant->prerequisites), "None");
            dndolphins_copy(
                grant->grant_value, sizeof(grant->grant_value), "feature=Custom Feature");
            grant->source_type = DndGrantFeat;
            grant->status = DndGrantPending;
            dndolphins_save(app, false);
            dndolphins_enter_screen(app, DndScreenGrantEdit);
        }
    }
}

static void dndolphins_handle_grant_edit(DndDolphinsApp* app, const InputEvent* event) {
    DndCharacter* c = &app->data.character;
    if(app->record_index >= c->grant_count) return;
    DndGrant* grant = &c->grants[app->record_index];
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
            if(value < 0) value = DndGrantSourceCount - 1U;
            if(value >= DndGrantSourceCount) value = 0;
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
            grant->level_gained =
                (uint8_t)dndolphins_clamp_i16(grant->level_gained + delta, 0, 20);
        else if(app->selection == 8U) {
            int16_t value = grant->status + delta;
            if(value < 0) value = DndGrantSkipped;
            if(value > DndGrantSkipped) value = DndGrantPending;
            grant->status = (uint8_t)value;
        } else
            return;
        dndolphins_save(app, false);
    } else if(event->type == InputTypeShort && event->key == InputKeyOk) {
        if(app->selection == 0U)
            dndolphins_begin_text(app, DndEditGrantStableId, "Stable grant ID", grant->stable_id);
        else if(app->selection == 1U)
            dndolphins_begin_text(app, DndEditGrantSource, "Source label", grant->source);
        else if(app->selection == 3U)
            dndolphins_begin_text(app, DndEditGrantOption, "Option name", grant->option_name);
        else if(app->selection == 4U)
            dndolphins_begin_text(
                app, DndEditGrantPrerequisites, "Prerequisites", grant->prerequisites);
        else if(app->selection == 7U)
            dndolphins_begin_text(
                app, DndEditGrantValue, "Grant payload key=value", grant->grant_value);
        else if(app->selection == 9U) {
            memmove(
                &c->grants[app->record_index],
                &c->grants[app->record_index + 1U],
                (c->grant_count - app->record_index - 1U) * sizeof(DndGrant));
            --c->grant_count;
            dndolphins_save(app, false);
            dndolphins_enter_screen(app, DndScreenGrantReview);
        }
    }
}

static void dndolphins_handle_attack_templates(DndDolphinsApp* app, const InputEvent* event) {
    DndCharacter* c = &app->data.character;
    uint16_t count = dndolphins_attack_template_display_count(c);
    uint16_t template_index = UINT16_MAX;
    DndAttackTemplateDisplayKind kind =
        dndolphins_attack_template_display_kind(c, app->selection, &template_index);
    if(dndolphins_is_move_event(event) && event->key == InputKeyUp)
        dndolphins_menu_move(app, count, -1);
    else if(dndolphins_is_move_event(event) && event->key == InputKeyDown)
        dndolphins_menu_move(app, count, 1);
    else if(
        dndolphins_is_move_event(event) && kind == DndAttackTemplateDisplayTemplate &&
        template_index < c->attack_template_count &&
        (event->key == InputKeyLeft || event->key == InputKeyRight)) {
        DndAttackTemplate* attack = &c->attack_templates[template_index];
        attack->attack_misc = (int8_t)dndolphins_clamp_i16(
            attack->attack_misc + (event->key == InputKeyRight ? 1 : -1), -20, 20);
        dndolphins_save(app, false);
    } else if(
        event->type == InputTypeLong && event->key == InputKeyOk &&
        kind == DndAttackTemplateDisplayTemplate && template_index < c->attack_template_count) {
        app->record_index = template_index;
        dndolphins_enter_screen(app, DndScreenAttackTemplateEdit);
    } else if(
        event->type == InputTypeShort && event->key == InputKeyOk &&
        kind == DndAttackTemplateDisplayNew) {
        if(c->attack_template_count >= DND_MAX_ATTACK_TEMPLATES) {
            dndolphins_set_status(app, "Template limit reached");
            return;
        }
        app->record_index = c->attack_template_count++;
        DndAttackTemplate* attack = &c->attack_templates[app->record_index];
        memset(attack, 0, sizeof(*attack));
        dndolphins_copy(attack->name, sizeof(attack->name), "Custom Attack");
        dndolphins_copy(attack->damage_type, sizeof(attack->damage_type), "Bludgeoning");
        dndolphins_copy(attack->rider_type, sizeof(attack->rider_type), "None");
        dndolphins_copy(attack->mastery, sizeof(attack->mastery), "None");
        attack->type = DndAttackTemplateCustom;
        attack->ability = DndAbilityStrength;
        attack->save_ability = DndAbilityDexterity;
        attack->damage_dice = 1U;
        attack->damage_die = 6U;
        attack->rider_die = 6U;
        dndolphins_save(app, false);
        dndolphins_enter_screen(app, DndScreenAttackTemplateEdit);
    } else if(
        event->type == InputTypeShort && event->key == InputKeyOk &&
        kind == DndAttackTemplateDisplayTemplate && template_index < c->attack_template_count) {
        DndAttackTemplate* attack = &c->attack_templates[template_index];
        if(attack->type == DndAttackTemplateGrapple || attack->type == DndAttackTemplateShove) {
            char status[48];
            snprintf(
                status,
                sizeof(status),
                "%s: STR/DEX save DC %u",
                attack->type == DndAttackTemplateGrapple ? "Grapple" : "Shove",
                dndolphins_unarmed_save_dc(c, attack));
            dndolphins_set_status(app, status);
            return;
        }

        bool unarmed = attack->type == DndAttackTemplateUnarmed;
        if(unarmed) {
            /* SRD 5.2.1 Unarmed Strike damage option: d20 + ability + PB to hit;
               on a hit damage is 1 + ability. Grapple/Shove are independent
               save-DC templates and therefore never share this attack roll. */
            if(app->roll->roll_mode > DndRollDisadvantage) app->roll->roll_mode = DndRollNormal;
            app->roll->dice_count = 1U;
            app->roll->dice_sides = 20U;
            app->roll->dice_modifier = dndolphins_unarmed_attack_modifier(c, attack);
            dndolphins_roll_generic(app);
            char status[48];
            snprintf(status, sizeof(status), "Hit dmg %d", dndolphins_unarmed_damage(c, attack));
            dndolphins_set_status(app, status);
        } else {
            app->roll->roll_mode = DndRollNormal;
            app->roll->dice_count = attack->damage_dice ? attack->damage_dice : 1U;
            app->roll->dice_sides = attack->damage_die >= 2U ? attack->damage_die : 1U;
            app->roll->dice_modifier = attack->attack_misc;
            dndolphins_roll_generic(app);
        }
    }
}

static void dndolphins_handle_attack_template_edit(DndDolphinsApp* app, const InputEvent* event) {
    DndCharacter* c = &app->data.character;
    if(app->record_index >= c->attack_template_count) return;
    DndAttackTemplate* attack = &c->attack_templates[app->record_index];
    if(dndolphins_is_move_event(event) && event->key == InputKeyUp)
        dndolphins_menu_move(app, 16U, -1);
    else if(dndolphins_is_move_event(event) && event->key == InputKeyDown)
        dndolphins_menu_move(app, 16U, 1);
    else if(
        dndolphins_is_move_event(event) &&
        (event->key == InputKeyLeft || event->key == InputKeyRight)) {
        int16_t delta = event->key == InputKeyRight ? 1 : -1;
        if(app->selection == 1U) {
            int16_t value = attack->type + delta;
            if(value < 0) value = DndAttackTemplateTypeCount - 1U;
            if(value >= DndAttackTemplateTypeCount) value = 0;
            attack->type = (uint8_t)value;
        } else if(app->selection == 2U || app->selection == 3U) {
            uint8_t* ability = app->selection == 2U ? &attack->ability : &attack->save_ability;
            int16_t value = *ability + delta;
            if(value < 0) value = DndAbilityCharisma;
            if(value > DndAbilityCharisma) value = DndAbilityStrength;
            *ability = (uint8_t)value;
        } else if(app->selection == 4U)
            attack->attack_misc =
                (int8_t)dndolphins_clamp_i16(attack->attack_misc + delta, -20, 20);
        else if(app->selection == 5U)
            attack->save_dc = (uint8_t)dndolphins_clamp_i16(attack->save_dc + delta, 0, 30);
        else if(app->selection == 6U)
            attack->damage_dice =
                (uint8_t)dndolphins_clamp_i16(attack->damage_dice + delta, 0, 20);
        else if(app->selection == 7U)
            attack->damage_die = dndolphins_cycle_die(attack->damage_die, delta, false);
        else if(app->selection == 10U)
            attack->rider_dice = (uint8_t)dndolphins_clamp_i16(attack->rider_dice + delta, 0, 20);
        else if(app->selection == 11U)
            attack->rider_die = dndolphins_cycle_die(attack->rider_die, delta, false);
        else if(app->selection == 13U) {
            int16_t value = attack->recharge + delta;
            if(value < 0) value = DndRechargeCount - 1U;
            if(value >= DndRechargeCount) value = 0;
            attack->recharge = (uint8_t)value;
        } else
            return;
        dndolphins_save(app, false);
    } else if(event->type == InputTypeShort && event->key == InputKeyOk) {
        if(app->selection == 0U)
            dndolphins_begin_text(app, DndEditAttackName, "Attack template name", attack->name);
        else if(app->selection == 8U)
            dndolphins_begin_text(
                app, DndEditAttackDamageType, "Damage type", attack->damage_type);
        else if(app->selection == 9U)
            dndolphins_begin_text(app, DndEditAttackMastery, "Mastery property", attack->mastery);
        else if(app->selection == 12U)
            dndolphins_begin_text(app, DndEditAttackRiderType, "Rider type", attack->rider_type);
        else if(app->selection == 14U) {
            memmove(
                &c->attack_templates[app->record_index],
                &c->attack_templates[app->record_index + 1U],
                (c->attack_template_count - app->record_index - 1U) * sizeof(DndAttackTemplate));
            --c->attack_template_count;
            dndolphins_save(app, false);
            dndolphins_enter_screen(app, DndScreenAttackTemplates);
        }
    }
}

#if defined(DND_BUILD_COMBAT)
static void dndolphins_handle_magic(DndDolphinsApp* app, const InputEvent* event) {
    DndCharacter* character = &app->data.character;
    if(dndolphins_is_move_event(event) && event->key == InputKeyUp)
        dndolphins_menu_move(app, 17U, -1);
    else if(dndolphins_is_move_event(event) && event->key == InputKeyDown)
        dndolphins_menu_move(app, 17U, 1);
    else if(
        dndolphins_is_move_event(event) &&
        (event->key == InputKeyLeft || event->key == InputKeyRight)) {
        int8_t delta = event->key == InputKeyRight ? 1 : -1;
        if(app->combat->arcane_recovery_active) {
            if(app->selection < 8U || app->selection > 12U) {
                dndolphins_set_status(app, "Choose a level 1-5 slot");
                return;
            }
            uint8_t level = app->selection - 7U;
            if(delta > 0) {
                uint8_t remaining =
                    app->combat->arcane_recovery_budget - app->combat->arcane_recovery_spent;
                if(level > remaining) {
                    dndolphins_set_status(app, "Not enough recovery");
                    return;
                }
                if(character->spell_slots_current[level] >= character->spell_slots_max[level]) {
                    dndolphins_set_status(app, "That slot level is full");
                    return;
                }
                ++character->spell_slots_current[level];
                ++app->combat->arcane_recovery_restored[level];
                app->combat->arcane_recovery_spent += level;
                character->arcane_recovery_used = 1U;
            } else {
                if(!app->combat->arcane_recovery_restored[level]) {
                    dndolphins_set_status(app, "Nothing to undo here");
                    return;
                }
                --character->spell_slots_current[level];
                --app->combat->arcane_recovery_restored[level];
                app->combat->arcane_recovery_spent -= level;
                if(!app->combat->arcane_recovery_spent) character->arcane_recovery_used = 0U;
            }
            dndolphins_save(app, false);
            dndolphins_set_status(app, "<> recover, row 7 done");
        } else if(app->selection == 1U) {
            int16_t ability = character->spellcasting_ability + delta;
            if(ability < 0) ability = DndAbilityCharisma;
            if(ability > DndAbilityCharisma) ability = DndAbilityStrength;
            character->spellcasting_ability = (uint8_t)ability;
        } else if(app->selection == 3U) {
            character->spell_attack_misc =
                (int8_t)dndolphins_clamp_i16(character->spell_attack_misc + delta, -20, 20);
        } else if(app->selection == 4U) {
            character->spell_save_misc =
                (int8_t)dndolphins_clamp_i16(character->spell_save_misc + delta, -20, 20);
        } else if(app->selection >= 8U && app->selection <= 16U) {
            if(event->type != InputTypeShort) return;
            uint8_t level = app->selection - 7U;
            character->spell_slots_current[level] = dndolphins_clamp_u8(
                character->spell_slots_current[level] + delta, character->spell_slots_max[level]);
            dndolphins_set_status(app, "Available slots changed");
        } else {
            return;
        }
        dndolphins_save(app, false);
    } else if(
        !app->combat->arcane_recovery_active && event->type == InputTypeLong &&
        (event->key == InputKeyLeft || event->key == InputKeyRight) && app->selection >= 8U &&
        app->selection <= 16U) {
        int8_t delta = event->key == InputKeyRight ? 1 : -1;
        uint8_t level = app->selection - 7U;
        character->spell_slots_max[level] =
            dndolphins_clamp_u8(character->spell_slots_max[level] + delta, 20U);
        if(character->spell_slots_current[level] > character->spell_slots_max[level])
            character->spell_slots_current[level] = character->spell_slots_max[level];
        dndolphins_save(app, false);
        dndolphins_set_status(app, "Maximum slots changed");
    } else if(event->type == InputTypeLong && event->key == InputKeyOk) {
        if(app->combat->arcane_recovery_active) {
            dndolphins_set_status(app, "Finish recovery first");
        } else if(app->selection == 0U) {
            dndolphins_request_launch(app, DndPendingLaunchSpellbook);
        } else if(app->selection == 2U) {
            dndolphins_spells_recalculate_multiclass_slots(character);
            dndolphins_save(app, false);
            dndolphins_confirm_action(app, "Class slots recalculated");
        } else if(app->selection == 3U || app->selection == 4U) {
            bool attack = app->selection == 3U;
            dndolphins_begin_number(
                app,
                DndNumberMagic,
                (uint8_t)app->selection,
                0U,
                attack ? "Spell attack misc" : "Spell save DC misc",
                attack ? character->spell_attack_misc : character->spell_save_misc,
                -20,
                20);
        } else if(app->selection >= 8U && app->selection <= 16U) {
            uint8_t level = app->selection - 7U;
            dndolphins_begin_number(
                app,
                DndNumberMagic,
                (uint8_t)app->selection,
                1U,
                "Maximum spell slots",
                character->spell_slots_max[level],
                0,
                20);
        }
    } else if(event->type == InputTypeShort && event->key == InputKeyOk) {
        if(app->selection == 0U)
            dndolphins_request_launch(app, DndPendingLaunchSpellbook);
        else if(app->selection == 7U) {
            if(app->combat->arcane_recovery_active) {
                app->combat->arcane_recovery_active = 0U;
                dndolphins_set_status(
                    app,
                    app->combat->arcane_recovery_spent ? "Arcane Recovery used" :
                                                         "Recovery skipped");
            } else if(character->arcane_recovery_used) {
                dndolphins_set_status(app, "Recovery already used");
            } else {
                dndolphins_set_status(app, "Finish a Short Rest first");
            }
        } else if(
            !app->combat->arcane_recovery_active && app->selection >= 8U &&
            app->selection <= 16U) {
            uint8_t level = app->selection - 7U;
            dndolphins_begin_number(
                app,
                DndNumberMagic,
                (uint8_t)app->selection,
                0U,
                "Available spell slots",
                character->spell_slots_current[level],
                0,
                character->spell_slots_max[level]);
        }
    }
}

#endif

static void dndolphins_handle_record_list(DndDolphinsApp* app, const InputEvent* event) {
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
    } else if(
        event->type == InputTypeShort && app->list_kind != DndListClasses &&
        (event->key == InputKeyLeft || event->key == InputKeyRight) && count > 1U) {
        uint16_t logical = app->selection ? app->selection - 1U : 0U;
        uint16_t target =
            (logical / DND_CHARACTER_COLLECTION_WINDOW) * DND_CHARACTER_COLLECTION_WINDOW;
        if(event->key == InputKeyRight && target + DND_CHARACTER_COLLECTION_WINDOW < count - 1U)
            target += DND_CHARACTER_COLLECTION_WINDOW;
        else if(event->key == InputKeyLeft && target >= DND_CHARACTER_COLLECTION_WINDOW)
            target -= DND_CHARACTER_COLLECTION_WINDOW;
        dndolphins_record_list_focus(app, target);
    } else if(event->type == InputTypeShort && event->key == InputKeyOk) {
        if(app->selection == 0U) {
            app->collection_replace = false;
            if(app->list_kind == DndListLanguages) {
                dndolphins_open_catalog(app, DndCatalogLanguages, DndEditLanguageName, "");
            } else if(app->list_kind == DndListProficiencies) {
                dndolphins_open_catalog(app, DndCatalogProficiencies, DndEditProficiencyName, "");
            } else if(!dndolphins_add_record(app)) {
                bool full = app->list_kind == DndListClasses &&
                            (app->data.character.class_count >= DND_MAX_CLASSES ||
                             dnd_rules_core_total_level(&app->data.character) >= 20U);
                dndolphins_set_status(app, full ? "List is full" : "Add failed - retry SD");
            }
        } else {
            app->record_index = app->selection - 1U;
            dndolphins_enter_screen(app, DndScreenRecordDetail);
        }
    }
}

static void dndolphins_adjust_record(DndDolphinsApp* app, int8_t delta) {
    DndCharacter* character = &app->data.character;
    uint16_t index = app->record_index;
    uint8_t field = app->selection;
    switch(app->list_kind) {
    case DndListClasses: {
        uint8_t previous_total_level = dnd_rules_core_total_level(character);
        uint8_t previous_pb = dnd_rules_core_proficiency_bonus(character);
        uint8_t previous_slots[DND_SLOT_COUNT];
        memcpy(previous_slots, character->spell_slots_max, sizeof(previous_slots));
        DndClassLevel* class_level = &character->classes[index];
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
            class_level->hit_dice_current = dndolphins_clamp_u8(
                class_level->hit_dice_current + delta, class_level->hit_dice_max);
        } else if(field == 5U) {
            class_level->hit_dice_max =
                dndolphins_clamp_u8(class_level->hit_dice_max + delta, 20U);
            if(class_level->hit_dice_current > class_level->hit_dice_max)
                class_level->hit_dice_current = class_level->hit_dice_max;
        } else if(field == 6U) {
            int16_t mode = class_level->spellcasting_mode + delta;
            if(mode < 0) mode = DndSpellcastingModeCount - 1U;
            if(mode >= DndSpellcastingModeCount) mode = 0;
            class_level->spellcasting_mode = (uint8_t)mode;
        } else if(field == 7U) {
            int16_t ability = class_level->spellcasting_ability + delta;
            if(ability < 0) ability = DndAbilityCharisma;
            if(ability > DndAbilityCharisma) ability = DndAbilityStrength;
            class_level->spellcasting_ability = (uint8_t)ability;
        } else if(field == 8U) {
            class_level->cantrip_limit =
                dndolphins_clamp_u8(class_level->cantrip_limit + delta, 30U);
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
            class_level->pact_slots_max =
                dndolphins_clamp_u8(class_level->pact_slots_max + delta, 8U);
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
    case DndListFeatures: {
        DndFeature* feature = dndolphins_feature_at(app, index, NULL);
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
            if(recharge < DndRechargeManual) recharge = DndRechargeCount - 1U;
            if(recharge >= DndRechargeCount) recharge = DndRechargeManual;
            feature->recharge = (uint8_t)recharge;
        } else if(field == 7U) {
            int16_t formula = feature->resource_formula + delta;
            if(formula < 0) formula = DndResourceFormulaCount - 1U;
            if(formula >= DndResourceFormulaCount) formula = 0;
            feature->resource_formula = (uint8_t)formula;
            feature->uses_max = dndolphins_rules_character_feature_max_uses(character, feature);
        } else if(field == 8U) {
            int16_t ability = feature->resource_ability + delta;
            if(ability < 0) ability = DndAbilityCharisma;
            if(ability > DndAbilityCharisma) ability = DndAbilityStrength;
            feature->resource_ability = (uint8_t)ability;
            feature->uses_max = dndolphins_rules_character_feature_max_uses(character, feature);
        } else {
            return;
        }
        (void)dndolphins_save_features_if_changed(app);
        break;
    }
    case DndListLanguages:
    case DndListProficiencies:
        return;
    }
    dndolphins_save(app, false);
}

static void dndolphins_handle_record_detail_ok(DndDolphinsApp* app) {
    DndCharacter* character = &app->data.character;
    uint16_t index = app->record_index;
    uint8_t field = app->selection;
    switch(app->list_kind) {
    case DndListClasses:
        if(field == 0U)
            dndolphins_open_catalog(
                app, DndCatalogClasses, DndEditClassName, character->classes[index].name);
        else if(field == 1U)
            dndolphins_open_catalog(
                app, DndCatalogSubclasses, DndEditSubclass, character->classes[index].subclass);
        else if(field >= 2U && field <= 16U)
            dndolphins_adjust_record(app, 1);
        else
            dndolphins_delete_record(app);
        break;
    case DndListFeatures: {
        DndFeature* feature = dndolphins_feature_at(app, index, NULL);
        if(!feature) return;
        if(field == 0U)
            dndolphins_open_catalog(app, DndCatalogFeats, DndEditFeatureName, feature->name);
        else if(field == 1U)
            dndolphins_begin_text(app, DndEditFeatureDetail, "Feature notes", feature->detail);
        else if(field < 9U)
            dndolphins_adjust_record(app, 1);
        else
            dndolphins_delete_record(app);
        break;
    }
    case DndListLanguages:
        if(field == 0U) {
            app->collection_replace = true;
            dndolphins_open_catalog(app, DndCatalogLanguages, DndEditLanguageName, "");
        }
        if(field == 1U) dndolphins_delete_record(app);
        break;
    case DndListProficiencies:
        if(field < 2U) {
            app->collection_replace = true;
            dndolphins_open_catalog(app, DndCatalogProficiencies, DndEditProficiencyName, "");
        }
        if(field == 2U) dndolphins_delete_record(app);
        break;
    }
}

static bool dndolphins_begin_record_number(DndDolphinsApp* app) {
    DndCharacter* character = &app->data.character;
    uint16_t index = app->record_index;
    uint8_t field = (uint8_t)app->selection;
    const char* header = NULL;
    int32_t value = 0;
    int32_t minimum = 0;
    int32_t maximum = 999;
    if(app->list_kind == DndListClasses) {
        DndClassLevel* level = &character->classes[index];
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
    } else if(app->list_kind == DndListFeatures) {
        DndFeature* feature = dndolphins_feature_at(app, index, NULL);
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
    dndolphins_begin_number(app, DndNumberRecord, field, 0U, header, value, minimum, maximum);
    return true;
}

static void dndolphins_handle_record_detail_custom_name(DndDolphinsApp* app) {
    DndCharacter* character = &app->data.character;
    uint16_t index = app->record_index;
    if(app->list_kind == DndListClasses && app->selection == 0U)
        dndolphins_begin_text(
            app, DndEditClassName, "Custom class", character->classes[index].name);
    else if(app->list_kind == DndListClasses && app->selection == 1U)
        dndolphins_begin_text(
            app, DndEditSubclass, "Custom subclass", character->classes[index].subclass);
    else if(app->list_kind == DndListFeatures && app->selection == 0U) {
        DndFeature* feature = dndolphins_feature_at(app, index, NULL);
        if(feature)
            dndolphins_begin_text(app, DndEditFeatureName, "Custom feat/perk", feature->name);
    }
}

static void dndolphins_handle_record_detail(DndDolphinsApp* app, const InputEvent* event) {
    uint8_t count = dndolphins_record_detail_count(app);
    if(dndolphins_is_move_event(event) && event->key == InputKeyUp)
        dndolphins_menu_move(app, count, -1);
    else if(dndolphins_is_move_event(event) && event->key == InputKeyDown)
        dndolphins_menu_move(app, count, 1);
    else if(
        dndolphins_is_move_event(event) &&
        (event->key == InputKeyLeft || event->key == InputKeyRight))
        dndolphins_adjust_record(app, event->key == InputKeyRight ? 1 : -1);
    else if(event->type == InputTypeLong && event->key == InputKeyOk) {
        if(!dndolphins_begin_record_number(app)) dndolphins_handle_record_detail_custom_name(app);
    } else if(event->type == InputTypeShort && event->key == InputKeyOk)
        dndolphins_handle_record_detail_ok(app);
}

static void dndolphins_apply_catalog_selection(DndDolphinsApp* app) {
    if(app->selection >= app->catalog->count) return;
    char selected[DND_CATALOG_NAME_LEN];
    dndolphins_copy(selected, sizeof(selected), app->catalog->entries[app->selection]);
    DndCharacter* character = &app->data.character;
    uint16_t index = app->record_index;
#if defined(DND_BUILD_GRANTS)
    if(app->grant_review->grant_choice_active) {
        app->status[0] = '\0';
        bool applied = dndolphins_apply_grant_choice_selection(app, selected);
        uint16_t return_selection = app->catalog->return_selection;
        app->grant_review->grant_choice_active = 0U;
        app->grant_review->grant_choice_kind = DndGrantChoiceNone;
        dndolphins_catalog_release(app);
        if(!applied) {
            dndolphins_enter_screen(app, DndScreenGrantReview);
            app->selection = return_selection;
            if(!app->status[0]) dndolphins_set_status(app, "Choice could not be applied");
            return;
        }
        dndolphins_save(app, false);
        dndolphins_enter_screen(app, DndScreenGrantReview);
        app->selection = return_selection;
        if(app->selection >= 5U) app->scroll = app->selection - 4U;
        dndolphins_set_status(app, "Choice applied (A)");
        (void)dndolphins_advance_grant_review_if_complete(app);
        return;
    }
#endif
    uint8_t grant_source = DndGrantSourceCount;
    bool save_features_after_catalog = false;
    switch(app->catalog->target) {
    case DndEditClassName: {
        bool newly_added_level = !strcmp(character->classes[index].name, "New Class") &&
                                 character->classes[index].level == 1U &&
                                 character->hit_dice_max < dnd_rules_core_total_level(character);
        char previous_class[DND_CLASS_NAME_LEN];
        dndolphins_copy(previous_class, sizeof(previous_class), character->classes[index].name);
        dndolphins_copy(
            character->classes[index].name, sizeof(character->classes[index].name), selected);
        if(index == 0U)
            dndolphins_clear_untouched_primary_class_saves(character, previous_class, selected);
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
    case DndEditSubclass:
        dndolphins_copy(
            character->classes[index].subclass,
            sizeof(character->classes[index].subclass),
            selected);
        dndolphins_spells_refresh_class_spellcasting(&character->classes[index]);
        dndolphins_spells_initialize_spell_slots_if_unset(character);
        dndolphins_spells_apply_level_progression(character, index);
        /* Subclass grants are explicit through Grant Initial Traits / Apply Level Grants. */
        break;
    case DndEditSpecies:
        dndolphins_copy(character->species, sizeof(character->species), selected);
        /* Species traits are intentionally gated behind Grant Initial Traits. */
        break;
    case DndEditFeatureName:
        grant_source = DndGrantFeat;
        if(app->level_choice_mode == 3U) {
            if(!character->feature_count || !character->features) {
                dndolphins_set_status(app, "Feat choice unavailable");
                return;
            }
            DndFeature feature = character->features[0];
            dndolphins_copy(feature.name, sizeof(feature.name), selected);
            if(!dndolphins_progression_store_features_append(
                   app->storage, app->active_profile, &feature) ||
               !dndolphins_complete_level_choice(app, "feat")) {
                dndolphins_set_status(app, "Could not record feat choice");
                return;
            }
            dnd_data_reserve_features_exact(character, 0U);
            character->feature_count = 0U;
            app->features_total = 0U;
            (void)dndolphins_progression_store_features_count(
                app->storage, app->active_profile, &app->features_total);
            app->level_choice_mode = 0U;
            app->return_screen = DndScreenCharacter;
        } else {
            DndFeature* feature = dndolphins_feature_at(app, index, NULL);
            if(!feature) {
                dndolphins_set_status(app, "Feature read failed");
                return;
            }
            dndolphins_copy(feature->name, sizeof(feature->name), selected);
            save_features_after_catalog = true;
        }
        break;
    case DndEditLanguageName: {
        bool ok = app->collection_replace ?
                      dnd_character_languages_replace(
                          app->storage, app->active_profile, app->record_index, selected) :
                      dnd_character_languages_append(app->storage, app->active_profile, selected);
        if(!ok) {
            dndolphins_set_status(app, "Language add failed");
            return;
        }
        (void)dnd_character_languages_count(
            app->storage, app->active_profile, &app->language_total);
        if(app->collection_cache) {
            app->collection_cache->language_page_count = 0U;
            app->collection_cache->language_cache_start = 0U;
        }
        app->character_collections_changed = true;
        app->catalog->return_selection = app->collection_replace ? 0U : app->language_total;
        break;
    }
    case DndEditProficiencyName: {
        uint8_t type_code = app->catalog->levels[app->selection];
        const char* type = dndolphins_proficiency_type_name(type_code);
        bool ok = app->collection_replace ?
                      dnd_character_proficiencies_replace(
                          app->storage, app->active_profile, app->record_index, type, selected) :
                      dnd_character_proficiencies_append(
                          app->storage, app->active_profile, type, selected);
        if(!ok) {
            dndolphins_set_status(app, "Proficiency add failed");
            return;
        }
        (void)dnd_character_proficiencies_count(
            app->storage, app->active_profile, &app->proficiency_total);
        if(app->collection_cache) {
            app->collection_cache->proficiency_page_count = 0U;
            app->collection_cache->proficiency_cache_start = 0U;
        }
        app->character_collections_changed = true;
        app->catalog->return_selection = app->collection_replace ? 1U : app->proficiency_total;
        break;
    }
    case DndEditBackground:
        dndolphins_copy(character->background, sizeof(character->background), selected);
        /* Background traits are intentionally gated behind Grant Initial Traits. */
        break;
    case DndEditAlignment:
        dndolphins_copy(character->alignment, sizeof(character->alignment), selected);
        break;
    default:
        return;
    }
    if(save_features_after_catalog && !dndolphins_save_features_if_changed(app)) {
        dndolphins_set_status(app, "Feature save failed");
        return;
    }
    const uint16_t return_selection = app->catalog->return_selection;
    dndolphins_catalog_release(app);
    if(grant_source < DndGrantSourceCount) dndolphins_release_staged_records(app);
#if defined(DND_BUILD_HUB)
    /* The saved Feature is the cross-FAP source of its dependent grants.
       Review belongs to DNDGrants, whose scan includes owned Feats. */
    dndolphins_save(app, false);
    DndScreen destination = app->return_screen;
    dndolphins_enter_screen(app, destination);
    app->selection = return_selection;
    if(app->selection >= 5U) app->scroll = app->selection - 4U;
    if(grant_source < DndGrantSourceCount) {
        dndolphins_request_launch(app, DndPendingLaunchGrantsLevel);
        return;
    }
#else
    uint8_t staged = grant_source < DndGrantSourceCount ?
                         dndolphins_stage_grants(app, grant_source, selected) :
                         0U;
    dndolphins_save(app, false);
    DndScreen destination = app->return_screen;
    if(staged) {
        app->return_screen = destination;
        dndolphins_enter_screen(app, DndScreenGrantReview);
        snprintf(app->status, sizeof(app->status), "%u grants to review", staged);
        return;
    }
    dndolphins_enter_screen(app, destination);
    app->selection = return_selection;
    if(app->selection >= 5U) app->scroll = app->selection - 4U;
#endif
    dndolphins_set_status(app, "Catalog choice saved");
}

static void dndolphins_handle_catalog(DndDolphinsApp* app, const InputEvent* event) {
    if(app->catalog->count && dndolphins_is_move_event(event) && event->key == InputKeyUp)
        dndolphins_menu_move(app, app->catalog->count, -1);
    else if(app->catalog->count && dndolphins_is_move_event(event) && event->key == InputKeyDown)
        dndolphins_menu_move(app, app->catalog->count, 1);
    else if(
        dndolphins_is_move_event(event) &&
        (event->key == InputKeyLeft || event->key == InputKeyRight)) {
        uint16_t next_start = app->catalog->page_start;
        uint16_t page_limit = dndolphins_catalog_page_limit(app);
        if(event->key == InputKeyRight && app->catalog->has_more)
            next_start += page_limit;
        else if(event->key == InputKeyLeft && app->catalog->page_start >= page_limit)
            next_start -= page_limit;
        if(next_start != app->catalog->page_start) {
            app->catalog->page_start = next_start;
            app->selection = 0U;
            app->scroll = 0U;
            dndolphins_catalog_load_page(app);
        }
    } else if(
        event->type == InputTypeLong && event->key == InputKeyOk &&
        (app->catalog->kind == DndCatalogSubclasses ||
         app->catalog->kind == DndCatalogProficiencies ||
         (app->catalog->kind == DndCatalogFeats && app->level_choice_mode == 3U))) {
        app->catalog->show_all = !app->catalog->show_all;
        app->catalog->page_start = 0U;
        app->selection = 0U;
        app->scroll = 0U;
        dndolphins_catalog_load_page(app);
        if(app->catalog->kind == DndCatalogFeats)
            dndolphins_set_status(app, app->catalog->show_all ? "All feats" : "Allowed feats");
        else if(app->catalog->kind == DndCatalogProficiencies)
            dndolphins_set_status(
                app, app->catalog->show_all ? "All proficiencies" : "Allowed proficiencies");
        else
            dndolphins_set_status(app, app->catalog->show_all ? "Showing all" : "Class filter");
    } else if(event->type == InputTypeShort && event->key == InputKeyOk)
        dndolphins_apply_catalog_selection(app);
}

static void dndolphins_handle_spell_attacks(DndDolphinsApp* app, const InputEvent* event) {
    uint16_t count = dndolphins_combat_spell_count(app);
    if(!count) return;
    if(dndolphins_is_move_event(event) && event->key == InputKeyUp) {
        dndolphins_menu_move(app, count, -1);
        dndolphins_prepare_combat_spell_rows(app, DndCombatSpellModeAttacks);
    } else if(dndolphins_is_move_event(event) && event->key == InputKeyDown) {
        dndolphins_menu_move(app, count, 1);
        dndolphins_prepare_combat_spell_rows(app, DndCombatSpellModeAttacks);
    } else if(event->type == InputTypeShort && event->key == InputKeyOk) {
        uint16_t spell_index = dndolphins_combat_spell_index(app, app->selection);
        if(spell_index == UINT16_MAX) return;
        app->combat->spell_attack_index = spell_index;
        DndSpellCastOption options[DNDOLPHINS_MAX_SPELL_CAST_OPTIONS];
        uint8_t option_count = dndolphins_build_spell_cast_options(
            app, spell_index, options, DNDOLPHINS_MAX_SPELL_CAST_OPTIONS);
        if(!option_count) {
            dndolphins_set_status(app, "No casting resource");
            return;
        }
        if(option_count == 1U) {
            dndolphins_cast_spell(app, &options[0]);
            return;
        }
        dndolphins_enter_screen(app, DndScreenSpellCast);
    }
}

static void dndolphins_handle_favorite_spells(DndDolphinsApp* app, const InputEvent* event) {
    uint16_t count = dndolphins_combat_spell_count(app);
    if(!count) return;
    if(dndolphins_is_move_event(event) && event->key == InputKeyUp) {
        dndolphins_menu_move(app, count, -1);
        dndolphins_prepare_combat_spell_rows(app, DndCombatSpellModeFavorites);
    } else if(dndolphins_is_move_event(event) && event->key == InputKeyDown) {
        dndolphins_menu_move(app, count, 1);
        dndolphins_prepare_combat_spell_rows(app, DndCombatSpellModeFavorites);
    } else if(event->type == InputTypeShort && event->key == InputKeyOk) {
        uint16_t spell_index = dndolphins_combat_spell_index(app, app->selection);
        if(spell_index == UINT16_MAX) return;
        app->combat->spell_attack_index = spell_index;
        DndSpellCastOption options[DNDOLPHINS_MAX_SPELL_CAST_OPTIONS];
        uint8_t option_count = dndolphins_build_spell_cast_options(
            app, spell_index, options, DNDOLPHINS_MAX_SPELL_CAST_OPTIONS);
        if(!option_count) {
            dndolphins_set_status(app, "No casting resource");
            return;
        }
        if(option_count == 1U) {
            dndolphins_cast_spell(app, &options[0]);
            return;
        }
        dndolphins_enter_screen(app, DndScreenSpellCast);
    }
}

static void dndolphins_handle_utility_spells(DndDolphinsApp* app, const InputEvent* event) {
    uint16_t count = dndolphins_combat_spell_count(app);
    if(!count) return;
    if(dndolphins_is_move_event(event) && event->key == InputKeyUp) {
        dndolphins_menu_move(app, count, -1);
        dndolphins_prepare_combat_spell_rows(app, DndCombatSpellModeUtility);
    } else if(dndolphins_is_move_event(event) && event->key == InputKeyDown) {
        dndolphins_menu_move(app, count, 1);
        dndolphins_prepare_combat_spell_rows(app, DndCombatSpellModeUtility);
    } else if(event->type == InputTypeShort && event->key == InputKeyOk) {
        uint16_t spell_index = dndolphins_combat_spell_index(app, app->selection);
        if(spell_index == UINT16_MAX) return;
        app->combat->spell_attack_index = spell_index;
        DndSpellCastOption options[DNDOLPHINS_MAX_SPELL_CAST_OPTIONS];
        uint8_t option_count = dndolphins_build_spell_cast_options(
            app, spell_index, options, DNDOLPHINS_MAX_SPELL_CAST_OPTIONS);
        if(!option_count) {
            dndolphins_set_status(app, "No casting resource");
            return;
        }
        if(option_count == 1U) {
            dndolphins_cast_spell(app, &options[0]);
            return;
        }
        dndolphins_enter_screen(app, DndScreenSpellCast);
    }
}

static void dndolphins_handle_rituals(DndDolphinsApp* app, const InputEvent* event) {
    uint16_t count = dndolphins_combat_spell_count(app);
    if(!count) return;
    if(dndolphins_is_move_event(event) && event->key == InputKeyUp) {
        dndolphins_menu_move(app, count, -1);
        dndolphins_prepare_combat_spell_rows(app, DndCombatSpellModeRituals);
    } else if(dndolphins_is_move_event(event) && event->key == InputKeyDown) {
        dndolphins_menu_move(app, count, 1);
        dndolphins_prepare_combat_spell_rows(app, DndCombatSpellModeRituals);
    } else if(event->type == InputTypeShort && event->key == InputKeyOk) {
        uint16_t spell_index = dndolphins_combat_spell_index(app, app->selection);
        if(spell_index == UINT16_MAX) return;
        DndSpell* spell = dndolphins_spell_at(app, spell_index, NULL);
        if(!spell) {
            dndolphins_set_status(app, "Spell read failed");
            return;
        }
        app->combat->spell_attack_index = spell_index;
        DndSpellCastOption option = {
            .level = spell->level,
            .resource = DndSpellCastRitual,
            .class_index = spell->class_index,
        };
        dndolphins_cast_spell(app, &option);
    }
}

static DndScreen dndolphins_combat_spell_return_screen(const DndDolphinsApp* app) {
    if(app && app->combat->combat_spell_mode == DndCombatSpellModeFavorites)
        return DndScreenFavoriteSpells;
    if(app && app->combat->combat_spell_mode == DndCombatSpellModeUtility)
        return DndScreenUtilitySpells;
    if(app && app->combat->combat_spell_mode == DndCombatSpellModeRituals) return DndScreenRituals;
    return DndScreenSpellAttacks;
}

static void dndolphins_handle_spell_cast(DndDolphinsApp* app, const InputEvent* event) {
    DndSpellCastOption options[DNDOLPHINS_MAX_SPELL_CAST_OPTIONS];
    uint8_t count = dndolphins_build_spell_cast_options(
        app, app->combat->spell_attack_index, options, DNDOLPHINS_MAX_SPELL_CAST_OPTIONS);
    if(count > DNDOLPHINS_MAX_SPELL_CAST_OPTIONS) count = DNDOLPHINS_MAX_SPELL_CAST_OPTIONS;
    if(!count) {
        dndolphins_enter_screen(app, dndolphins_combat_spell_return_screen(app));
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

static void dndolphins_handle_spell_result(DndDolphinsApp* app, const InputEvent* event) {
    if(app->combat->spell_cast_resource == DndSpellCastRitual && event->type == InputTypeShort &&
       event->key == InputKeyOk) {
        dndolphins_enter_screen(app, DndScreenRituals);
        return;
    }
    if(app->combat->spell_cast_attack_roll_count > 4U && dndolphins_is_move_event(event)) {
        uint8_t maximum_scroll = app->combat->spell_cast_attack_roll_count - 4U;
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
    DndSpellCastOption options[DNDOLPHINS_MAX_SPELL_CAST_OPTIONS];
    uint8_t count = dndolphins_build_spell_cast_options(
        app, app->combat->spell_attack_index, options, DNDOLPHINS_MAX_SPELL_CAST_OPTIONS);
    if(count == 1U) {
        dndolphins_enter_screen(app, DndScreenSpellCast);
        app->selection = 0U;
    } else if(count > 1U) {
        dndolphins_enter_screen(app, DndScreenSpellCast);
    } else {
        dndolphins_enter_screen(app, dndolphins_combat_spell_return_screen(app));
    }
}

static void dndolphins_handle_combat(DndDolphinsApp* app, const InputEvent* event) {
    DndCharacter* character = &app->data.character;
    if(dndolphins_is_move_event(event) && event->key == InputKeyUp)
        dndolphins_menu_move(app, DndolphinsCombatCount, -1);
    else if(dndolphins_is_move_event(event) && event->key == InputKeyDown)
        dndolphins_menu_move(app, DndolphinsCombatCount, 1);
    else if(
        dndolphins_is_move_event(event) &&
        (event->key == InputKeyLeft || event->key == InputKeyRight)) {
        int16_t delta = event->key == InputKeyRight ? 1 : -1;
        if(app->selection == DndolphinsCombatAttackMode) {
            int16_t mode = app->roll->roll_mode + delta;
            if(mode < DndRollNormal) mode = DndRollDisadvantage;
            if(mode > DndRollDisadvantage) mode = DndRollNormal;
            app->roll->roll_mode = (DndRollMode)mode;
            return;
        } else if(app->selection == DndolphinsCombatHp)
            character->hp_current = dndolphins_clamp_i16(character->hp_current + delta, 0, 999);
        else if(app->selection == DndolphinsCombatTemporaryHp)
            character->hp_temporary =
                dndolphins_clamp_i16(character->hp_temporary + delta, 0, 999);
        else if(app->selection == DndolphinsCombatSpendHitDie) {
            int16_t class_index = app->combat->hit_die_class_index + delta;
            if(class_index < 0) class_index = character->class_count - 1U;
            if(class_index >= character->class_count) class_index = 0;
            app->combat->hit_die_class_index = (uint8_t)class_index;
            return;
        } else if(app->selection == DndolphinsCombatReaction)
            character->reaction_available = !character->reaction_available;
        else if(app->selection == DndolphinsCombatDeathSuccesses)
            character->death_successes =
                dndolphins_clamp_u8(character->death_successes + delta, 3U);
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
         (app->selection >= DndolphinsCombatDeathSuccesses &&
          app->selection <= DndolphinsCombatExhaustion))) {
        const char* header = app->selection == DndolphinsCombatHp             ? "Current HP" :
                             app->selection == DndolphinsCombatTemporaryHp    ? "Temporary HP" :
                             app->selection == DndolphinsCombatDeathSuccesses ? "Death successes" :
                             app->selection == DndolphinsCombatDeathFailures  ? "Death failures" :
                                                                                "Exhaustion";
        int32_t value =
            app->selection == DndolphinsCombatHp             ? character->hp_current :
            app->selection == DndolphinsCombatTemporaryHp    ? character->hp_temporary :
            app->selection == DndolphinsCombatDeathSuccesses ? character->death_successes :
            app->selection == DndolphinsCombatDeathFailures  ? character->death_failures :
                                                               character->exhaustion;
        int32_t maximum = app->selection <= DndolphinsCombatTemporaryHp   ? 999 :
                          app->selection <= DndolphinsCombatDeathFailures ? 3 :
                                                                            6;
        dndolphins_begin_number(
            app, DndNumberCombat, (uint8_t)app->selection, 0U, header, value, 0, maximum);
    } else if(event->type == InputTypeShort && event->key == InputKeyOk) {
        switch(app->selection) {
        case DndolphinsCombatAttackMode: {
            int16_t mode = app->roll->roll_mode + 1;
            if(mode > DndRollDisadvantage) mode = DndRollNormal;
            app->roll->roll_mode = (DndRollMode)mode;
            break;
        }
        case DndolphinsCombatWeaponAttacks:
            dndolphins_enter_screen(app, DndScreenAttackList);
            break;
        case DndolphinsCombatSpellAttacks:
            dndolphins_enter_screen(app, DndScreenSpellAttacks);
            break;
        case DndolphinsCombatFavoriteSpells:
            dndolphins_enter_screen(app, DndScreenFavoriteSpells);
            break;
        case DndolphinsCombatSpellcastingStats:
            dndolphins_enter_screen(app, DndScreenMagic);
            break;
        case DndolphinsCombatUtilitySpells:
            dndolphins_enter_screen(app, DndScreenUtilitySpells);
            break;
        case DndolphinsCombatRituals:
            dndolphins_enter_screen(app, DndScreenRituals);
            break;
        case DndolphinsCombatAttackTemplates:
            dndolphins_enter_screen(app, DndScreenAttackTemplates);
            break;
        case DndolphinsCombatJumpToInitiative:
            dndolphins_request_launch(app, DndPendingLaunchInitiative);
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
                   app->storage, app->active_profile, character, DndFeatureRechargeShortRest)) {
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
            } else if(!character->classes[app->combat->hit_die_class_index].hit_dice_current) {
                dndolphins_set_status(app, "No Hit Dice left");
            } else {
                uint8_t roll = 0U;
                int16_t healed = dndolphins_rules_character_spend_class_hit_die(
                    character, app->combat->hit_die_class_index, &roll);
                int8_t constitution = dnd_rules_core_ability_modifier(
                    character->ability_scores[DndAbilityConstitution]);
                dndolphins_save(app, false);
                snprintf(
                    app->status,
                    sizeof(app->status),
                    "d%u:%u %+d, healed %d",
                    character->classes[app->combat->hit_die_class_index].hit_die,
                    roll,
                    constitution,
                    healed);
                dndolphins_start_dice_animation(
                    app, 1U, character->classes[app->combat->hit_die_class_index].hit_die);
            }
            break;
        case DndolphinsCombatLongRest:
            if(character->hp_current < 1) {
                dndolphins_set_status(app, "Need at least 1 HP");
                break;
            }
            dndolphins_rules_character_long_rest(character);
            if(!dndolphins_progression_store_features_recharge(
                   app->storage, app->active_profile, character, DndFeatureRechargeLongRest)) {
                dndolphins_set_status(app, "Feature recharge failed");
                break;
            }
            if(!dnd_storage_reset_spell_free_casts(app->storage, app->active_profile, character)) {
                dndolphins_set_status(app, "Spellbook update failed");
                break;
            }
            dndolphins_save(app, false);
            dndolphins_confirm_action(app, "Long rest applied");
            break;
        case DndolphinsCombatConditions:
            dndolphins_begin_text(app, DndEditConditions, "Conditions", character->conditions);
            break;
        case DndolphinsCombatConcentration:
            dndolphins_begin_text(
                app, DndEditConcentration, "Concentration", character->concentration);
            break;
        case DndolphinsCombatReaction:
            character->reaction_available = !character->reaction_available;
            dndolphins_save(app, false);
            break;
        case DndolphinsCombatTemporaryEffects:
            dndolphins_begin_text(
                app, DndEditTemporaryEffects, "Temporary effects", character->temporary_effects);
            break;
        case DndolphinsCombatResistances:
            dndolphins_begin_text(app, DndEditResistances, "Resistances", character->resistances);
            break;
        case DndolphinsCombatImmunities:
            dndolphins_begin_text(app, DndEditImmunities, "Immunities", character->immunities);
            break;
        case DndolphinsCombatVulnerabilities:
            dndolphins_begin_text(
                app, DndEditVulnerabilities, "Vulnerabilities", character->vulnerabilities);
            break;
        case DndolphinsCombatSenses:
            dndolphins_begin_text(app, DndEditSenses, "Senses", character->senses);
            break;
        case DndolphinsCombatMovement:
            dndolphins_begin_text(
                app, DndEditMovementModes, "Movement modes", character->movement_modes);
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

static void dndolphins_roll_generic(DndDolphinsApp* app) {
    app->roll->dice_first = 0U;
    app->roll->dice_second = 0U;
    app->roll->dice_guidance = 0U;
    app->roll->dice_roll_value_count = 0U;
    app->roll->dice_roll_sum = 0U;
    memset(app->roll->dice_roll_values, 0, sizeof(app->roll->dice_roll_values));
    if(app->roll->roll_mode == DndRollGuidance && app->roll->dice_count == 1U &&
       app->roll->dice_sides == 20U) {
        app->roll->dice_first = (uint8_t)dnd_rules_core_roll_dice(1U, 20U);
        app->roll->dice_guidance = (uint8_t)dnd_rules_core_roll_dice(1U, 4U);
        app->roll->dice_roll_values[0] = app->roll->dice_first;
        app->roll->dice_roll_values[1] = app->roll->dice_guidance;
        app->roll->dice_roll_value_count = 2U;
        app->roll->dice_roll_sum = app->roll->dice_first + app->roll->dice_guidance;
        app->roll->dice_result = (int16_t)app->roll->dice_roll_sum + app->roll->dice_modifier;
    } else if(
        (app->roll->roll_mode == DndRollAdvantage ||
         app->roll->roll_mode == DndRollDisadvantage) &&
        app->roll->dice_count == 1U && app->roll->dice_sides == 20U) {
        app->roll->dice_first = (uint8_t)dnd_rules_core_roll_dice(1U, 20U);
        app->roll->dice_second = (uint8_t)dnd_rules_core_roll_dice(1U, 20U);
        app->roll->dice_roll_values[0] = app->roll->dice_first;
        app->roll->dice_roll_values[1] = app->roll->dice_second;
        app->roll->dice_roll_value_count = 2U;
        app->roll->dice_roll_sum = app->roll->dice_first + app->roll->dice_second;
        uint8_t chosen =
            app->roll->roll_mode == DndRollAdvantage ?
                (app->roll->dice_first > app->roll->dice_second ? app->roll->dice_first :
                                                                  app->roll->dice_second) :
                (app->roll->dice_first < app->roll->dice_second ? app->roll->dice_first :
                                                                  app->roll->dice_second);
        app->roll->dice_result = chosen + app->roll->dice_modifier;
    } else {
        app->roll->dice_roll_value_count = app->roll->dice_count;
        app->roll->dice_roll_sum = dndolphins_dice_roll_values(
            app->roll->dice_count,
            app->roll->dice_sides,
            app->roll->dice_roll_values,
            sizeof(app->roll->dice_roll_values));
        if(app->roll->dice_count == 1U) app->roll->dice_first = app->roll->dice_roll_values[0];
        app->roll->dice_result = (int16_t)app->roll->dice_roll_sum + app->roll->dice_modifier;
    }
    dndolphins_enter_screen(app, DndScreenDiceResult);
    dndolphins_start_dice_animation(app, app->roll->dice_roll_value_count, app->roll->dice_sides);
}

static void dndolphins_handle_dice(DndDolphinsApp* app, const InputEvent* event) {
    if(dndolphins_is_move_event(event) && event->key == InputKeyUp)
        dndolphins_menu_move(app, 5U, -1);
    else if(dndolphins_is_move_event(event) && event->key == InputKeyDown)
        dndolphins_menu_move(app, 5U, 1);
    else if(
        dndolphins_is_move_event(event) &&
        (event->key == InputKeyLeft || event->key == InputKeyRight)) {
        int8_t delta = event->key == InputKeyRight ? 1 : -1;
        if(app->selection == 0U) {
            app->roll->dice_count =
                (uint8_t)dndolphins_clamp_i16(app->roll->dice_count + delta, 1, 20);
            if(app->roll->roll_mode != DndRollNormal) app->roll->roll_mode = DndRollNormal;
        } else if(app->selection == 1U) {
            app->roll->dice_sides = dndolphins_cycle_die(app->roll->dice_sides, delta, false);
            if(app->roll->roll_mode != DndRollNormal) app->roll->roll_mode = DndRollNormal;
        } else if(app->selection == 2U)
            app->roll->dice_modifier =
                dndolphins_clamp_i16(app->roll->dice_modifier + delta, -99, 99);
        else if(app->selection == 3U) {
            int16_t mode = app->roll->roll_mode + delta;
            if(mode < 0) mode = DndRollGuidance;
            if(mode > DndRollGuidance) mode = DndRollNormal;
            app->roll->roll_mode = (DndRollMode)mode;
            if(app->roll->roll_mode != DndRollNormal) {
                app->roll->dice_count = 1U;
                app->roll->dice_sides = 20U;
            }
        } else {
            return;
        }
        app->roll->dice_result = 0;
        app->roll->dice_second = 0U;
        app->roll->dice_guidance = 0U;
        app->roll->dice_roll_value_count = 0U;
    } else if(event->type == InputTypeLong && event->key == InputKeyOk && app->selection <= 2U) {
        const char* header = app->selection == 0U ? "Dice count" :
                             app->selection == 1U ? "Die sides" :
                                                    "Roll modifier";
        int32_t value = app->selection == 0U ? app->roll->dice_count :
                        app->selection == 1U ? app->roll->dice_sides :
                                               app->roll->dice_modifier;
        int32_t minimum = app->selection == 0U ? 1 : app->selection == 1U ? 2 : -99;
        int32_t maximum = app->selection == 0U ? 20 : app->selection == 1U ? 100 : 99;
        dndolphins_begin_number(
            app, DndNumberDice, (uint8_t)app->selection, 0U, header, value, minimum, maximum);
    } else if(event->type == InputTypeShort && event->key == InputKeyOk) {
        if(app->selection == 4U) dndolphins_roll_generic(app);
    }
}

static void dndolphins_handle_dice_result(DndDolphinsApp* app, const InputEvent* event) {
    if(event->type == InputTypeShort && event->key == InputKeyOk) dndolphins_roll_generic(app);
}

typedef struct {
    const char* ammunition_group;
    uint16_t* logical_indices;
    uint8_t capacity;
    uint8_t count;
} DndDolphinsAmmunitionLookup;

static char dndolphins_ascii_lower(char value) {
    return value >= 'A' && value <= 'Z' ? (char)(value - 'A' + 'a') : value;
}

static bool dndolphins_contains_case_insensitive(const char* text, const char* token) {
    if(!text || !token || !token[0]) return false;
    for(const char* start = text; *start; ++start) {
        const char* left = start;
        const char* right = token;
        while(*left && *right && dndolphins_ascii_lower(*left) == dndolphins_ascii_lower(*right)) {
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

    char singular[DND_SHORT_LEN];
    dndolphins_copy(singular, sizeof(singular), group);
    size_t length = strlen(singular);
    if(length > 1U && (singular[length - 1U] == 's' || singular[length - 1U] == 'S')) {
        singular[length - 1U] = '\0';
        return dndolphins_contains_case_insensitive(name, singular);
    }
    return false;
}

static const char* dndolphins_weapon_ammunition_group(const DndItem* weapon) {
    if(!weapon) return "";
    if(weapon->ammunition_group[0]) return weapon->ammunition_group;
    if(!(weapon->weapon_properties & DndWeaponAmmunition)) return "";

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
    uint16_t logical_index,
    const DndItem* item,
    void* context) {
    DndDolphinsAmmunitionLookup* lookup = context;
    if(!lookup || !item || !lookup->ammunition_group) return false;
    if(!item->is_weapon && item->quantity > 0 &&
       (dndolphins_ammunition_name_matches(item->name, lookup->ammunition_group) ||
        (item->ammunition_group[0] &&
         dndolphins_ammunition_name_matches(item->ammunition_group, lookup->ammunition_group)))) {
        if(lookup->count < lookup->capacity)
            lookup->logical_indices[lookup->count] = logical_index;
        if(lookup->count < UINT8_MAX) ++lookup->count;
        return lookup->count < lookup->capacity;
    }
    return true;
}

static uint8_t dndolphins_find_loose_ammunition(
    DndDolphinsApp* app,
    const char* ammunition_group,
    uint16_t* indices,
    uint8_t capacity) {
    if(!app || !ammunition_group || !ammunition_group[0] || !indices || !capacity) return 0U;
    DndDolphinsAmmunitionLookup lookup = {
        .ammunition_group = ammunition_group,
        .logical_indices = indices,
        .capacity = capacity,
        .count = 0U,
    };
    if(!dnd_storage_visit_items(
           app->storage, app->active_profile, dndolphins_ammunition_stack_visitor, &lookup, NULL))
        return 0U;
    return lookup.count > capacity ? capacity : lookup.count;
}

static bool dndolphins_consume_ammunition_index(
    DndDolphinsApp* app,
    uint16_t weapon_index,
    uint16_t ammunition_index) {
    if(!app || !dndolphins_item_cache_ensure(app, ammunition_index)) return false;
    DndItem* ammunition = dndolphins_item_cached_at(app, ammunition_index, NULL);
    if(!ammunition || ammunition->quantity <= 0) return false;
    --ammunition->quantity;
    if(!dndolphins_save_items_if_changed(app)) return false;
    return dndolphins_item_cache_ensure(app, weapon_index);
}

static void dndolphins_finish_selected_attack(DndDolphinsApp* app) {
    DndItem* item = dndolphins_item_cached_at(app, app->combat->attack_item_index, NULL);
    if(!item) {
        dndolphins_set_status(app, "Weapon reload failed");
        return;
    }
    DndItem weapon = *item;
    app->combat->attack_roll =
        dndolphins_weapon_combat_roll_attack(&app->data.character, &weapon, app->roll->roll_mode);
    app->combat->attack_phase = 0U;
    dndolphins_enter_screen(app, DndScreenAttackResult);
    dndolphins_start_dice_animation(app, app->combat->attack_roll.second_die ? 2U : 1U, 20U);
}

static void dndolphins_roll_selected_attack(DndDolphinsApp* app) {
    uint16_t count = dndolphins_weapon_count(app);
    if(count == 0U) return;
    app->combat->attack_item_index = dndolphins_weapon_index(app, app->selection);
    if(app->combat->attack_item_index == UINT16_MAX) return;
    DndItem* item = dndolphins_item_at(app, app->combat->attack_item_index, NULL);
    if(!item) return;
    DndItem weapon = *item;
    if(item->weapon_properties & DndWeaponAmmunition) {
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
            dndolphins_save(app, false);
        } else {
            const char* group = dndolphins_weapon_ammunition_group(&weapon);
            app->combat->ammo_choice_count =
                dndolphins_find_loose_ammunition(app, group, app->combat->ammo_choice_indices, 8U);
            if(!app->combat->ammo_choice_count) {
                dndolphins_set_status(app, "No ammunition");
                return;
            }
            if(app->combat->ammo_choice_count > 1U) {
                dndolphins_copy(
                    app->combat->ammo_choice_group, sizeof(app->combat->ammo_choice_group), group);
                dndolphins_enter_screen(app, DndScreenAmmoChoice);
                return;
            }
            if(!dndolphins_consume_ammunition_index(
                   app, app->combat->attack_item_index, app->combat->ammo_choice_indices[0])) {
                dndolphins_set_status(app, "Ammo save failed");
                return;
            }
            dndolphins_save(app, false);
        }
    }
    dndolphins_finish_selected_attack(app);
}

static void dndolphins_handle_attack_list(DndDolphinsApp* app, const InputEvent* event) {
    uint16_t count = dndolphins_weapon_count(app);
    if(dndolphins_is_move_event(event) && event->key == InputKeyUp) {
        dndolphins_menu_move(app, count, -1);
        dndolphins_prepare_combat_weapon_rows(app);
    } else if(dndolphins_is_move_event(event) && event->key == InputKeyDown) {
        dndolphins_menu_move(app, count, 1);
        dndolphins_prepare_combat_weapon_rows(app);
    } else if(
        dndolphins_is_move_event(event) &&
        (event->key == InputKeyLeft || event->key == InputKeyRight)) {
        int16_t mode = app->roll->roll_mode + (event->key == InputKeyRight ? 1 : -1);
        if(mode < 0) mode = DndRollDisadvantage;
        if(mode > DndRollDisadvantage) mode = DndRollNormal;
        app->roll->roll_mode = (DndRollMode)mode;
    } else if(event->type == InputTypeShort && event->key == InputKeyOk) {
        dndolphins_roll_selected_attack(app);
    }
}

static void dndolphins_handle_ammo_choice(DndDolphinsApp* app, const InputEvent* event) {
    if(dndolphins_is_move_event(event) && event->key == InputKeyUp)
        dndolphins_menu_move(app, app->combat->ammo_choice_count, -1);
    else if(dndolphins_is_move_event(event) && event->key == InputKeyDown)
        dndolphins_menu_move(app, app->combat->ammo_choice_count, 1);
    else if(event->type == InputTypeShort && event->key == InputKeyOk) {
        if(app->selection >= app->combat->ammo_choice_count) return;
        if(!dndolphins_consume_ammunition_index(
               app,
               app->combat->attack_item_index,
               app->combat->ammo_choice_indices[app->selection])) {
            dndolphins_set_status(app, "Ammo save failed");
            return;
        }
        dndolphins_save(app, false);
        dndolphins_finish_selected_attack(app);
    }
}

static void dndolphins_handle_attack_result(DndDolphinsApp* app, const InputEvent* event) {
    DndItem* item = dndolphins_item_at(app, app->combat->attack_item_index, NULL);
    if(!item) return;
    if(app->combat->attack_phase == 0U) {
        if(event->type == InputTypeShort && event->key == InputKeyOk) {
            app->combat->damage_roll = dndolphins_weapon_combat_roll_damage(
                &app->data.character, item, app->combat->attack_roll.critical);
            app->combat->attack_phase = 1U;
            app->combat->damage_roll_page = 0U;
            uint8_t count = app->combat->damage_roll.weapon_roll_count +
                            app->combat->damage_roll.extra_roll_count;
            if(count)
                dndolphins_start_dice_animation(
                    app,
                    count,
                    item->use_versatile && item->versatile_die >= 2U ? item->versatile_die :
                                                                       item->damage_die);
        } else if(event->type == InputTypeShort && event->key == InputKeyRight) {
            app->combat->damage_roll =
                dndolphins_weapon_combat_roll_damage(&app->data.character, item, true);
            app->combat->attack_phase = 1U;
            app->combat->damage_roll_page = 0U;
            uint8_t count = app->combat->damage_roll.weapon_roll_count +
                            app->combat->damage_roll.extra_roll_count;
            if(count)
                dndolphins_start_dice_animation(
                    app,
                    count,
                    item->use_versatile && item->versatile_die >= 2U ? item->versatile_die :
                                                                       item->damage_die);
        } else if(event->type == InputTypeShort && event->key == InputKeyUp) {
            app->combat->attack_roll = dndolphins_weapon_combat_roll_attack(
                &app->data.character, item, app->roll->roll_mode);
            dndolphins_start_dice_animation(
                app, app->combat->attack_roll.second_die ? 2U : 1U, 20U);
        }
    } else {
        uint8_t roll_count =
            app->combat->damage_roll.weapon_roll_count + app->combat->damage_roll.extra_roll_count;
        uint8_t page_count = roll_count > 1U ? (roll_count + 15U) / 16U : 1U;
        if(event->type == InputTypeShort && event->key == InputKeyUp && page_count > 1U) {
            if(app->combat->damage_roll_page == 0U)
                app->combat->damage_roll_page = page_count - 1U;
            else
                --app->combat->damage_roll_page;
        } else if(event->type == InputTypeShort && event->key == InputKeyDown && page_count > 1U) {
            app->combat->damage_roll_page = (app->combat->damage_roll_page + 1U) % page_count;
        } else if(event->type == InputTypeShort && event->key == InputKeyOk) {
            app->combat->damage_roll = dndolphins_weapon_combat_roll_damage(
                &app->data.character, item, app->combat->damage_roll.critical);
            app->combat->damage_roll_page = 0U;
            roll_count = app->combat->damage_roll.weapon_roll_count +
                         app->combat->damage_roll.extra_roll_count;
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
    DndDolphinsApp* app = context;
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
    if(app->roll && app->roll->dice_animating) {
        if(event->type == InputTypeShort && event->key == InputKeyBack) {
            app->roll->dice_animating = 0U;
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
#if defined(DND_BUILD_HUB)
    case DndScreenHome:
        dndolphins_handle_home(app, event);
        break;
    case DndScreenProfiles:
        dndolphins_handle_profiles(app, event);
        break;
    case DndScreenProfileActions:
        dndolphins_handle_profile_actions(app, event);
        break;
    case DndScreenCharacter:
        dndolphins_handle_character(app, event);
        break;
    case DndScreenVitals:
        dndolphins_handle_vitals(app, event);
        break;
    case DndScreenAbilities:
        dndolphins_handle_abilities(app, event);
        break;
    case DndScreenSkills:
        dndolphins_handle_skills(app, event);
        break;
    case DndScreenLevelReview:
        dndolphins_handle_level_review(app, event);
        break;
    case DndScreenLevelChoice:
        dndolphins_handle_level_choice(app, event);
        break;
    case DndScreenAsiAbility:
        dndolphins_handle_asi_ability(app, event);
        break;
    case DndScreenRecordList:
        dndolphins_handle_record_list(app, event);
        break;
    case DndScreenRecordDetail:
        dndolphins_handle_record_detail(app, event);
        break;
    case DndScreenCatalog:
        dndolphins_handle_catalog(app, event);
        break;
    case DndScreenDice:
        dndolphins_handle_dice(app, event);
        break;
    case DndScreenDiceResult:
        dndolphins_handle_dice_result(app, event);
        break;
    case DndScreenSettings:
        dndolphins_handle_settings(app, event);
        break;
#elif defined(DND_BUILD_COMBAT)
    case DndScreenCombat:
        dndolphins_handle_combat(app, event);
        break;
    case DndScreenSpellAttacks:
        dndolphins_handle_spell_attacks(app, event);
        break;
    case DndScreenFavoriteSpells:
        dndolphins_handle_favorite_spells(app, event);
        break;
    case DndScreenUtilitySpells:
        dndolphins_handle_utility_spells(app, event);
        break;
    case DndScreenRituals:
        dndolphins_handle_rituals(app, event);
        break;
    case DndScreenSpellCast:
        dndolphins_handle_spell_cast(app, event);
        break;
    case DndScreenSpellResult:
        dndolphins_handle_spell_result(app, event);
        break;
    case DndScreenAttackTemplates:
        dndolphins_handle_attack_templates(app, event);
        break;
    case DndScreenAttackTemplateEdit:
        dndolphins_handle_attack_template_edit(app, event);
        break;
    case DndScreenAttackList:
        dndolphins_handle_attack_list(app, event);
        break;
    case DndScreenAmmoChoice:
        dndolphins_handle_ammo_choice(app, event);
        break;
    case DndScreenAttackResult:
        dndolphins_handle_attack_result(app, event);
        break;
    case DndScreenMagic:
        dndolphins_handle_magic(app, event);
        break;
#elif defined(DND_BUILD_GRANTS)
    case DndScreenGrantReview:
        dndolphins_handle_grant_review(app, event);
        break;
    case DndScreenGrantDiagnostics:
        dndolphins_handle_grant_diagnostics(app, event);
        break;
    case DndScreenGrantEdit:
        dndolphins_handle_grant_edit(app, event);
        break;
    case DndScreenCatalog:
        dndolphins_handle_catalog(app, event);
        break;
#endif
    default:
        break;
    }
    dndolphins_refresh(app);
    return true;
}

static bool dndolphins_navigation_callback(void* context) {
    DndDolphinsApp* app = context;
    app->input_module_active = 0U;
    app->number_context = DndNumberNone;
    app->edit_target = DndEditNone;
    dndolphins_input_hook_release(app);
    view_dispatcher_switch_to_view(app->dispatcher, DndViewMain);
    view_dispatcher_send_custom_event(app->dispatcher, DNDOLPHINS_RELEASE_INPUT_EVENT);
    dndolphins_refresh(app);
    return true;
}

static bool dndolphins_reserve_core_ui(DndDolphinsApp* app) {
    if(!app) return false;
    app->dispatcher = view_dispatcher_alloc();
    if(!app->dispatcher) return false;
    view_dispatcher_set_event_callback_context(app->dispatcher, app);
    view_dispatcher_set_navigation_event_callback(app->dispatcher, dndolphins_navigation_callback);
    view_dispatcher_set_custom_event_callback(app->dispatcher, dndolphins_custom_event_callback);
    view_dispatcher_set_tick_event_callback(
        app->dispatcher, dndolphins_tick_event_callback, DNDOLPHINS_UI_TICK_MS);

    app->main_view = view_alloc();
    if(!app->main_view) return false;
    view_allocate_model(app->main_view, ViewModelTypeLockFree, sizeof(DndDolphinsApp*));
    DndDolphinsApp** model = view_get_model(app->main_view);
    if(!model) return false;
    *model = app;
    view_commit_model(app->main_view, false);
    view_set_context(app->main_view, app);
    view_set_draw_callback(app->main_view, dndolphins_draw_callback);
    view_set_input_callback(app->main_view, dndolphins_input_callback);
    return true;
}

static DndDolphinsApp* dndolphins_app_alloc(void) {
    DndDolphinsApp* app = malloc(sizeof(DndDolphinsApp));
    if(!app) {
        FURI_LOG_E(
            TAG,
            "Unable to allocate %u-byte app state; free heap=%lu",
            (unsigned int)sizeof(DndDolphinsApp),
            (unsigned long)memmgr_get_free_heap());
        return NULL;
    }
    memset(app, 0, sizeof(*app));

    app->gui = furi_record_open(RECORD_GUI);
    if(!app->gui) goto fail;
    app->storage = furi_record_open(RECORD_STORAGE);
    if(!app->storage) goto fail;
    if(!dnd_settings_load(app->storage, &app->settings)) dnd_settings_defaults(&app->settings);
    if(app->settings.debug)
        FURI_LOG_I(
            TAG,
            "Heap after app state free=%lu state=%u",
            (unsigned long)memmgr_get_free_heap(),
            (unsigned int)sizeof(DndDolphinsApp));
    /* Do not probe the complete catalog set during app startup. A persisted All
       choice is trusted until Settings is opened; Settings performs the
       authoritative availability scan and falls back to SRD if required. */
    app->catalog_all_available = app->settings.catalog_all ? 1U : 0U;
    /* Reserve only the always-visible core GUI blocks while the heap is clean.
       Optional editors, timers, profile browsing state, catalogs and indexes are
       allocated at first use and released when their workflow ends. */
    if(!dndolphins_reserve_core_ui(app)) {
        FURI_LOG_E(
            TAG,
            "Core UI allocation failed; free heap=%lu",
            (unsigned long)memmgr_get_free_heap());
        goto fail;
    }
    if(app->settings.debug)
        FURI_LOG_I(TAG, "Heap after core UI free=%lu", (unsigned long)memmgr_get_free_heap());
    /* Relocate only legacy character ch*.txt files. Files are moved unchanged;
       the tolerant field-name loader interprets whatever recognized data exists.
       A failed relocation is treated conservatively as existing user data so a
       fresh New Hero cannot be created over a migration problem. */
    bool legacy_move_ok = dnd_storage_move_legacy_profiles(app->storage);
    if(!dndolphins_profile_browser_alloc(app)) goto fail;
    bool profiles_loaded = dnd_storage_profiles_load(app->storage, app->profile_browser);
    app->active_profile = app->profile_browser->active_profile;
    dndolphins_active_entry_cache_from_browser(app);
    /* Only create a fresh character after a successful profile-directory scan proves
       there is no existing primary character file. A failed scan is treated
       conservatively as existing user data. SHD history files are never primary-profile candidates. */
    bool character_file_available = app->profile_browser->character_file_seen ||
                                    !app->profile_browser->scan_succeeded || !legacy_move_ok;
    bool recovered_backup = false;
    bool loaded = false;
    bool recovered_next_profile = false;
    uint32_t first_profile = app->active_profile;
    uint32_t candidate = first_profile;

    /* A stale or unreadable active profile must never leave default New Hero data
       eligible for autosave. Try the selected ID first, then advance through real
       character files by ID and wrap once. */
    uint16_t attempts = app->profile_browser->count ? app->profile_browser->count : 1U;
    for(uint16_t attempt = 0U; attempt < attempts; ++attempt) {
        bool candidate_recovered = false;
        if(dnd_storage_load_profile(app->storage, candidate, &app->data, &candidate_recovered)) {
            loaded = true;
            recovered_backup = candidate_recovered;
            app->active_profile = candidate;
            recovered_next_profile = candidate != first_profile;
            break;
        }
        DndProfileEntry next;
        if(!app->profile_browser->count ||
           !dnd_storage_profiles_next_after(app->storage, candidate, &next) ||
           next.id == candidate || next.id == first_profile)
            break;
        candidate = next.id;
    }

    app->active_profile_loaded = loaded ? 1U : 0U;
    bool character_ready = loaded;
    bool metadata_saved = true;
    if(loaded && recovered_backup)
        character_ready =
            dnd_storage_recover_profile_backup(app->storage, app->active_profile, &app->data);

    /* New Hero is created only when storage positively contains no primary
       character data. SHD history files never participate in this decision. */
    if(!loaded && !character_file_available) {
        dnd_data_clear(&app->data);
        dnd_data_set_defaults(&app->data);
        app->active_profile = 0U;
        app->active_profile_loaded = 1U;
        character_ready = dnd_storage_save_profile(app->storage, app->active_profile, &app->data);
        dnd_data_clear_spells(&app->data.character);
        dnd_data_clear_items(&app->data.character);
        dnd_data_reserve_features_exact(&app->data.character, 0U);
        app->data.character.feature_count = 0U;
        dndolphins_release_staged_records(app);
        app->spellbook_loaded = 0U;
        app->items_loaded = 0U;
        app->features_loaded = 0U;
        if(!character_ready) {
            dnd_storage_delete_profile(app->storage, app->active_profile);
            app->active_profile_loaded = 0U;
        }
    }
    bool spell_slots_initialized = false;
    if(app->active_profile_loaded) {
        bool spellcasting_repaired = false;
        for(uint8_t i = 0U; i < app->data.character.class_count; ++i)
            if(dndolphins_spells_refresh_class_spellcasting(&app->data.character.classes[i]))
                spellcasting_repaired = true;
        spell_slots_initialized =
            dndolphins_spells_initialize_spell_slots_if_unset(&app->data.character);
        if(spellcasting_repaired && !spell_slots_initialized)
            dndolphins_spells_recalculate_multiclass_slots(&app->data.character);
        if(spellcasting_repaired || spell_slots_initialized)
            character_ready =
                character_ready &&
                dnd_storage_save_profile_updated(app->storage, app->active_profile, &app->data);
    }
    if(app->active_profile_loaded) {
        bool active_included = dndolphins_profile_include_active(app);
        metadata_saved = active_included && dndolphins_profile_browser_save(app);
    }
    app->saved_fingerprint = dndolphins_data_fingerprint(&app->data);
    if(!character_ready || !metadata_saved) {
        app->storage_read_only = 1U;
        app->storage_unsaved = app->active_profile_loaded ? 1U : 0U;
    }

    app->screen = DndScreenHome;
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

    dndolphins_profile_browser_release(app);

    view_dispatcher_add_view(app->dispatcher, DndViewMain, app->main_view);
    view_dispatcher_attach_to_gui(app->dispatcher, app->gui, ViewDispatcherTypeFullscreen);
    if(app->settings.debug)
        FURI_LOG_I(TAG, "Heap app ready free=%lu", (unsigned long)memmgr_get_free_heap());
    return app;

fail:
    dndolphins_quiesce_async(app);
    if(app->text_input) text_input_free(app->text_input);
    if(app->number_input) number_input_free(app->number_input);
    if(app->autosave_timer) furi_timer_free(app->autosave_timer);
    if(app->main_view) view_free(app->main_view);
    if(app->dispatcher) view_dispatcher_free(app->dispatcher);
    dndolphins_catalog_runtime_release(app);
    dndolphins_character_collection_release(app);
    dndolphins_combat_runtime_free(app);
    dndolphins_roll_runtime_free(app);
    dndolphins_grant_review_runtime_free(app);
    dndolphins_profile_browser_release(app);
    dndolphins_collection_cache_runtime_free(app);
    free(app->edit_buffer);
    app->edit_buffer = NULL;
    dnd_data_clear(&app->data);
    if(app->storage) furi_record_close(RECORD_STORAGE);
    if(app->gui) furi_record_close(RECORD_GUI);
    free(app);
    return NULL;
}

static void dndolphins_app_free(DndDolphinsApp* app) {
    furi_assert(app);

    /* Quiesce asynchronous callbacks before any UI or app state is released. */
    dndolphins_quiesce_async(app);

    dndolphins_flush_save(app, false);
    dndolphins_catalog_runtime_release(app);
    dndolphins_character_collection_release(app);

    if(app->dispatcher && app->number_input)
        view_dispatcher_remove_view(app->dispatcher, DndViewNumberInput);
    if(app->dispatcher && app->text_input)
        view_dispatcher_remove_view(app->dispatcher, DndViewTextInput);
    if(app->dispatcher && app->main_view)
        view_dispatcher_remove_view(app->dispatcher, DndViewMain);
    if(app->text_input) text_input_free(app->text_input);
    if(app->number_input) number_input_free(app->number_input);
    if(app->main_view) view_free(app->main_view);

    if(app->autosave_timer) furi_timer_free(app->autosave_timer);
    if(app->dispatcher) view_dispatcher_free(app->dispatcher);
    dndolphins_combat_runtime_free(app);
    dndolphins_roll_runtime_free(app);
    dndolphins_grant_review_runtime_free(app);
    dndolphins_profile_browser_release(app);
    dndolphins_collection_cache_runtime_free(app);
    free(app->edit_buffer);
    app->edit_buffer = NULL;
    dnd_data_clear(&app->data);
    if(app->storage) furi_record_close(RECORD_STORAGE);
    if(app->gui) furi_record_close(RECORD_GUI);
    free(app);
}

#if defined(DND_BUILD_HUB)
static bool dndolphins_reload_after_journal(DndDolphinsApp* app) {
    dndolphins_character_collection_release(app);
    dndolphins_collection_cache_runtime_free(app);
    dnd_data_clear(&app->data);
    dnd_data_set_defaults(&app->data);
    app->spellbook_loaded = app->items_loaded = app->features_loaded = 0;
    app->spellbook_total = app->items_total = app->features_total = 0;
    app->language_total = app->proficiency_total = 0;
    app->character_collections_changed = false;
    app->autosave_pending = 0;
    bool recovered = false;
    bool loaded =
        dnd_storage_load_profile(app->storage, app->active_profile, &app->data, &recovered);
    app->active_profile_loaded = loaded;
    app->storage_read_only = !loaded || recovered;
    app->storage_unsaved = 0;
    app->saved_fingerprint = dndolphins_data_fingerprint(&app->data);
    app->saved_spellbook_fingerprint = dndolphins_spellbook_fingerprint(&app->data.character);
    app->saved_items_fingerprint = dndolphins_items_fingerprint(&app->data.character);
    app->saved_features_fingerprint = dndolphins_features_fingerprint(&app->data.character);
    app->active_entry_valid = 0;
    if(loaded)
        (void)dndolphins_profile_include_active(app);
    else
        dndolphins_set_status(app, "Reload failed - saves paused");
    return loaded;
}
static DndPluginUiResult dndolphins_run_ui_plugin(DndDolphinsApp* app, DndPendingLaunch launch) {
    DndPlugin plugin = {0};
    DndPluginLoading loading = {0};
    dnd_plugin_loading_begin(&loading, app->dispatcher, app->storage);
    bool journal = launch == DndPendingLaunchJournal;
    DndPluginLoadResult loaded = dnd_plugin_open(
        &plugin,
        app->storage,
        journal ? DND_JOURNAL_HUB_PATH : DND_CHARACTER_SHEET_HUB_PATH,
        journal ? DND_JOURNAL_API_ID : DND_CHARACTER_SHEET_API_ID,
        journal ? DND_JOURNAL_API_VERSION : DND_CHARACTER_SHEET_API_VERSION,
        journal ? sizeof(DndJournalApi) : sizeof(DndCharacterSheetApi));
    DndPluginUiResult result = DndPluginUiError;
    bool ran = false;
    if(loaded == DndPluginLoadOk) {
        if(journal) {
            const DndJournalApi* api = plugin.api;
            if(api->run) {
                ran = true;
                result = api->run(
                    app->dispatcher,
                    app->storage,
                    app->active_profile,
                    app->active_profile_loaded != 0,
                    loading.view != NULL);
            }
        } else {
            const DndCharacterSheetApi* api = plugin.api;
            if(api->run) {
                ran = true;
                result = api->run(
                    app->dispatcher,
                    app->active_profile_loaded ? &app->data.character : NULL,
                    loading.view != NULL);
            }
        }
    }
    dnd_plugin_close(&plugin);
    if(journal && ran && app->active_profile_loaded) (void)dndolphins_reload_after_journal(app);
    view_dispatcher_set_event_callback_context(app->dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->dispatcher, dndolphins_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(app->dispatcher, dndolphins_navigation_callback);
    view_dispatcher_set_tick_event_callback(
        app->dispatcher, dndolphins_tick_event_callback, furi_ms_to_ticks(DNDOLPHINS_UI_TICK_MS));
    if(!ran)
        dndolphins_set_status(app, dnd_plugin_load_message(loaded));
    else if(result == DndPluginUiError)
        dndolphins_set_status(app, "FAL unavailable - retry");
    view_dispatcher_switch_to_view(app->dispatcher, 0U);
    dnd_plugin_loading_end(&loading, app->dispatcher);
    return result;
}
#endif

#if defined(DND_BUILD_HUB)
int32_t dnd_app_hub_run(void* context) {
    const char* launch_args = (const char*)context;
    DndSplash* splash = dndolphins_splash_begin(!launch_args || launch_args[0] == '\0');

    DndDolphinsApp* app = dndolphins_app_alloc();
    if(!app) {
        dndolphins_splash_end(splash);
        return -1;
    }
    dndolphins_apply_return_focus(app, (const char*)context);
    dndolphins_splash_wait(splash);
    view_dispatcher_switch_to_view(app->dispatcher, DndViewMain);
    dnd_handoff_ready(DNDOLPHINS_FAP_PATH);
    dndolphins_splash_end(splash);
    while(true) {
        view_dispatcher_run(app->dispatcher);
        if(app->pending_launch != DndPendingLaunchCharacterSheet &&
           app->pending_launch != DndPendingLaunchJournal)
            break;
        DndPendingLaunch launch = app->pending_launch;
        app->pending_launch = DndPendingLaunchNone;
        dndolphins_quiesce_async(app);
        dndolphins_release_text_input(app);
        dndolphins_release_number_input(app);
        dndolphins_catalog_runtime_release(app);
        dndolphins_profile_browser_release(app);
        DndPluginUiResult result = dndolphins_run_ui_plugin(app, launch);
        if(result == DndPluginUiExit) break;
        if(result == DndPluginUiAdventure) {
            app->pending_launch = DndPendingLaunchAdventureContinue;
            break;
        }
        view_dispatcher_switch_to_view(app->dispatcher, DndViewMain);
        dndolphins_refresh(app);
    }

    DndPendingLaunch pending_launch = app->pending_launch;
    bool launch_ok = true;

    if(pending_launch != DndPendingLaunchNone) {
        const char* launch_path =
            pending_launch == DndPendingLaunchJournal ? DNDJOURNAL_FAP_PATH :
            (pending_launch == DndPendingLaunchAdventure ||
             pending_launch == DndPendingLaunchAdventureContinue) ?
                                                        DNDADVENTURE_FAP_PATH :
            pending_launch == DndPendingLaunchInitiative ? DNDINITIATIVE_FAP_PATH :
            pending_launch == DndPendingLaunchInventory  ? DNDINVENTORY_FAP_PATH :
            (pending_launch == DndPendingLaunchSpellbook ||
             pending_launch == DndPendingLaunchSpellbookMagic) ?
                                                          DNDSPELLBOOK_FAP_PATH :
            pending_launch == DndPendingLaunchCombat         ? DNDCOMBAT_FAP_PATH :
            pending_launch == DndPendingLaunchCharacterSheet ? DNDCHARACTERSHEET_FAP_PATH :
            pending_launch == DndPendingLaunchBackup         ? DNDBACKUP_FAP_PATH :
            (pending_launch == DndPendingLaunchGrantsInitial ||
             pending_launch == DndPendingLaunchGrantsLevel) ?
                                                       DNDGRANTS_FAP_PATH :
                                                       DNDBESTIARY_FAP_PATH;
        const char* args = pending_launch == DndPendingLaunchGrantsInitial ?
                               "initial" :
                           pending_launch == DndPendingLaunchGrantsLevel ?
                               "level" :
                           pending_launch == DndPendingLaunchSpellbookMagic ?
                               DND_SPELLBOOK_LAUNCH_MAGIC :
                           pending_launch == DndPendingLaunchAdventureContinue ?
                               DND_PROFILE_HANDOFF_ADVENTURE_CONTINUE :
                               NULL;
        launch_ok = dnd_handoff_launch(launch_path, args);
    }
    dndolphins_app_free(app);
    return launch_ok ? 0 : -1;
}
#elif defined(DND_BUILD_COMBAT)
int32_t dnd_app_combat_run(void* context) {
    UNUSED(context);
    DndDolphinsApp* app = dndolphins_app_alloc();
    if(!app) return -1;
    if(!dndolphins_combat_runtime_alloc(app) || !dndolphins_roll_runtime_alloc(app)) {
        dndolphins_app_free(app);
        return -1;
    }
    app->screen = DndScreenCombat;
    app->return_screen = DndScreenCombat;
    app->selection = 0U;
    app->scroll = 0U;
    app->combat->hit_die_class_index = 0U;
    if(app->roll->roll_mode == DndRollGuidance) app->roll->roll_mode = DndRollNormal;
    view_dispatcher_switch_to_view(app->dispatcher, DndViewMain);
    dnd_handoff_ready(DNDCOMBAT_FAP_PATH);
    view_dispatcher_run(app->dispatcher);

    DndPendingLaunch pending_launch = app->pending_launch;
    bool return_to_parent = app->return_to_parent != 0U;
    bool launch_ok = true;
    if(pending_launch == DndPendingLaunchInitiative)
        launch_ok = dnd_handoff_launch(DNDINITIATIVE_FAP_PATH, DND_INITIATIVE_LAUNCH_FROM_COMBAT);
    if(return_to_parent)
        (void)dnd_handoff_launch_if_present(DNDOLPHINS_FAP_PATH, DND_PROFILE_RETURN_FOCUS_COMBAT);
    dndolphins_app_free(app);
    return launch_ok ? 0 : -1;
}
#elif defined(DND_BUILD_GRANTS)
int32_t dnd_app_grants_run(void* context) {
    const char* args = (const char*)context;
    DndDolphinsApp* app = dndolphins_app_alloc();
    if(!app) return -1;
    if(!dndolphins_grant_review_runtime_alloc(app)) {
        dndolphins_app_free(app);
        return -1;
    }
    if(app->settings.catalog_all)
        app->catalog_all_available = dndolphins_catalog_all_files_available(app->storage) ? 1U :
                                                                                            0U;
    if(!app->catalog_all_available) app->settings.catalog_all = 0U;
    app->return_screen = DndScreenGrantReview;
    app->deferred_action = (args && strcmp(args, "initial") == 0) ?
                               DndDeferredActionGrantInitialTraits :
                               DndDeferredActionApplyLevelGrants;
    dndolphins_run_deferred_action(app);
    if(app->screen != DndScreenGrantReview) {
        (void)dnd_handoff_launch_if_present(
            DNDOLPHINS_FAP_PATH, DND_PROFILE_RETURN_FOCUS_CHARACTER);
        dndolphins_app_free(app);
        return 0;
    }
    view_dispatcher_switch_to_view(app->dispatcher, DndViewMain);
    dnd_handoff_ready(DNDGRANTS_FAP_PATH);
    view_dispatcher_run(app->dispatcher);
    bool return_to_parent = app->return_to_parent != 0U;
    if(return_to_parent)
        (void)dnd_handoff_launch_if_present(
            DNDOLPHINS_FAP_PATH, DND_PROFILE_RETURN_FOCUS_CHARACTER);
    dndolphins_app_free(app);
    return 0;
}
#endif
