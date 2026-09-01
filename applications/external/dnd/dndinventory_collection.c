#include "dndinventory_collection.h"

#include "dnd_profile_handoff.h"
#include "dnd_data.h"
#include "dndinventory_internal.h"
#include "dnd_rules.h"
#include "dnd_storage.h"
#include "dnd_weapon_rules.h"

#include <furi.h>
#include <gui/gui.h>
#include <gui/modules/number_input.h>
#include <gui/modules/text_input.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <input/input.h>
#include <storage/storage.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TAG                                          "DndInventory"
#define DNDINVENTORY_COLLECTION_VIEW_MAIN            0U
#define DNDINVENTORY_COLLECTION_VIEW_TEXT            1U
#define DNDINVENTORY_COLLECTION_VIEW_NUMBER          2U
#define DNDINVENTORY_COLLECTION_ROWS                 5U
#define DNDINVENTORY_COLLECTION_CATALOG_PAGE         10U
#define DNDINVENTORY_COLLECTION_CATALOG_OFFSET_PAGES 64U
#define DNDINVENTORY_COLLECTION_LINE_MAX             256U
#define DNDINVENTORY_COLLECTION_CATALOG_READ_BUFFER  128U
#define DNDINVENTORY_COLLECTION_ITEM_CATALOG         APP_ASSETS_PATH("catalogs/items.txt")

typedef enum {
    DndInventoryCollectionScreenNoCharacter,
    DndInventoryCollectionScreenList,
    DndInventoryCollectionScreenDetail,
    DndInventoryCollectionScreenCatalog,
    DndInventoryCollectionScreenInventoryTools,
    DndInventoryCollectionScreenCurrency,
    DndInventoryCollectionScreenResources,
} DndInventoryCollectionScreen;

typedef enum {
    DndInventoryCollectionEditNone,
    DndInventoryCollectionEditName,
    DndInventoryCollectionEditDetail,
    DndInventoryCollectionEditAmmoGroup,
} DndInventoryCollectionEdit;

typedef enum {
    DndInventoryGrantAvailable,
    DndInventoryGrantGranted,
    DndInventoryGrantOverrideUsed,
    DndInventoryGrantBlockedByItems,
    DndInventoryGrantReadError,
} DndInventoryGrantState;

typedef enum {
    DndInventoryItemCategoryOther,
    DndInventoryItemCategoryWeapon,
    DndInventoryItemCategoryArmor,
    DndInventoryItemCategoryGear,
    DndInventoryItemCategoryTool,
    DndInventoryItemCategoryMountVehicle,
    DndInventoryItemCategoryPotion,
    DndInventoryItemCategoryRing,
    DndInventoryItemCategoryRod,
    DndInventoryItemCategoryScroll,
    DndInventoryItemCategoryStaff,
    DndInventoryItemCategoryWand,
    DndInventoryItemCategoryWondrous,
} DndInventoryItemCategory;

typedef enum {
    DndInventoryItemFilterAll,
    DndInventoryItemFilterWeapons,
    DndInventoryItemFilterArmor,
    DndInventoryItemFilterAmmunition,
    DndInventoryItemFilterGear,
    DndInventoryItemFilterTools,
    DndInventoryItemFilterMountVehicles,
    DndInventoryItemFilterPotions,
    DndInventoryItemFilterRings,
    DndInventoryItemFilterRods,
    DndInventoryItemFilterScrolls,
    DndInventoryItemFilterStaffs,
    DndInventoryItemFilterWands,
    DndInventoryItemFilterWondrous,
    DndInventoryItemFilterMagic,
    DndInventoryItemFilterCount,
} DndInventoryItemFilter;

typedef struct {
    char name[POCKET_D20_CATALOG_NAME_LEN];
    uint8_t category;
    uint8_t magic;
    uint16_t absolute_index;
} DndInventoryCatalogEntry;

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
} DndInventoryEquipmentPreset;

#define COLLECTION_WEAPON(name, weight, dice, die, versatile, type, properties, ammo) \
    {name, weight, properties, dice, die, versatile, type, 0U, -1, 0U, ammo}
#define COLLECTION_ARMOR(name, weight, base, dex_cap, shield) \
    {name, weight, 0U, 0U, 0U, 0U, PocketDamageBludgeoning, base, dex_cap, shield, ""}

static const DndInventoryEquipmentPreset dndinventory_collection_equipment_presets[] = {
    COLLECTION_WEAPON("Club", 20, 1, 4, 0, PocketDamageBludgeoning, PocketWeaponLight, ""),
    COLLECTION_WEAPON(
        "Dagger",
        10,
        1,
        4,
        0,
        PocketDamagePiercing,
        PocketWeaponFinesse | PocketWeaponLight | PocketWeaponThrown,
        ""),
    COLLECTION_WEAPON("Greatclub", 100, 1, 8, 0, PocketDamageBludgeoning, 0U, ""),
    COLLECTION_WEAPON(
        "Handaxe",
        20,
        1,
        6,
        0,
        PocketDamageSlashing,
        PocketWeaponLight | PocketWeaponThrown,
        ""),
    COLLECTION_WEAPON("Javelin", 20, 1, 6, 0, PocketDamagePiercing, PocketWeaponThrown, ""),
    COLLECTION_WEAPON(
        "Light Hammer",
        20,
        1,
        4,
        0,
        PocketDamageBludgeoning,
        PocketWeaponLight | PocketWeaponThrown,
        ""),
    COLLECTION_WEAPON("Mace", 40, 1, 6, 0, PocketDamageBludgeoning, 0U, ""),
    COLLECTION_WEAPON("Quarterstaff", 40, 1, 6, 8, PocketDamageBludgeoning, 0U, ""),
    COLLECTION_WEAPON("Sickle", 20, 1, 4, 0, PocketDamageSlashing, PocketWeaponLight, ""),
    COLLECTION_WEAPON("Spear", 30, 1, 6, 8, PocketDamagePiercing, PocketWeaponThrown, ""),
    COLLECTION_WEAPON(
        "Dart",
        3,
        1,
        4,
        0,
        PocketDamagePiercing,
        PocketWeaponFinesse | PocketWeaponRanged | PocketWeaponThrown,
        ""),
    COLLECTION_WEAPON(
        "Light Crossbow",
        50,
        1,
        8,
        0,
        PocketDamagePiercing,
        PocketWeaponRanged | PocketWeaponAmmunition,
        "Bolts"),
    COLLECTION_WEAPON(
        "Shortbow",
        20,
        1,
        6,
        0,
        PocketDamagePiercing,
        PocketWeaponRanged | PocketWeaponAmmunition,
        "Arrows"),
    COLLECTION_WEAPON(
        "Sling",
        0,
        1,
        4,
        0,
        PocketDamageBludgeoning,
        PocketWeaponRanged | PocketWeaponAmmunition,
        "Sling bullets"),
    COLLECTION_WEAPON("Battleaxe", 40, 1, 8, 10, PocketDamageSlashing, 0U, ""),
    COLLECTION_WEAPON("Flail", 20, 1, 8, 0, PocketDamageBludgeoning, 0U, ""),
    COLLECTION_WEAPON("Glaive", 60, 1, 10, 0, PocketDamageSlashing, PocketWeaponHeavy, ""),
    COLLECTION_WEAPON("Greataxe", 70, 1, 12, 0, PocketDamageSlashing, PocketWeaponHeavy, ""),
    COLLECTION_WEAPON("Greatsword", 60, 2, 6, 0, PocketDamageSlashing, PocketWeaponHeavy, ""),
    COLLECTION_WEAPON("Halberd", 60, 1, 10, 0, PocketDamageSlashing, PocketWeaponHeavy, ""),
    COLLECTION_WEAPON("Lance", 60, 1, 10, 0, PocketDamagePiercing, PocketWeaponHeavy, ""),
    COLLECTION_WEAPON("Longsword", 30, 1, 8, 10, PocketDamageSlashing, 0U, ""),
    COLLECTION_WEAPON("Maul", 100, 2, 6, 0, PocketDamageBludgeoning, PocketWeaponHeavy, ""),
    COLLECTION_WEAPON("Morningstar", 40, 1, 8, 0, PocketDamagePiercing, 0U, ""),
    COLLECTION_WEAPON("Pike", 180, 1, 10, 0, PocketDamagePiercing, PocketWeaponHeavy, ""),
    COLLECTION_WEAPON("Rapier", 20, 1, 8, 0, PocketDamagePiercing, PocketWeaponFinesse, ""),
    COLLECTION_WEAPON(
        "Scimitar",
        30,
        1,
        6,
        0,
        PocketDamageSlashing,
        PocketWeaponFinesse | PocketWeaponLight,
        ""),
    COLLECTION_WEAPON(
        "Shortsword",
        20,
        1,
        6,
        0,
        PocketDamagePiercing,
        PocketWeaponFinesse | PocketWeaponLight,
        ""),
    COLLECTION_WEAPON("Trident", 40, 1, 8, 10, PocketDamagePiercing, PocketWeaponThrown, ""),
    COLLECTION_WEAPON("Warhammer", 50, 1, 8, 10, PocketDamageBludgeoning, 0U, ""),
    COLLECTION_WEAPON("War Pick", 20, 1, 8, 10, PocketDamagePiercing, 0U, ""),
    COLLECTION_WEAPON("Whip", 30, 1, 4, 0, PocketDamageSlashing, PocketWeaponFinesse, ""),
    COLLECTION_WEAPON(
        "Blowgun",
        10,
        1,
        1,
        0,
        PocketDamagePiercing,
        PocketWeaponRanged | PocketWeaponAmmunition,
        "Needles"),
    COLLECTION_WEAPON(
        "Hand Crossbow",
        30,
        1,
        6,
        0,
        PocketDamagePiercing,
        PocketWeaponRanged | PocketWeaponLight | PocketWeaponAmmunition,
        "Bolts"),
    COLLECTION_WEAPON(
        "Heavy Crossbow",
        180,
        1,
        10,
        0,
        PocketDamagePiercing,
        PocketWeaponRanged | PocketWeaponHeavy | PocketWeaponAmmunition,
        "Bolts"),
    COLLECTION_WEAPON(
        "Longbow",
        20,
        1,
        8,
        0,
        PocketDamagePiercing,
        PocketWeaponRanged | PocketWeaponHeavy | PocketWeaponAmmunition,
        "Arrows"),
    COLLECTION_WEAPON(
        "Musket",
        100,
        1,
        12,
        0,
        PocketDamagePiercing,
        PocketWeaponRanged | PocketWeaponAmmunition,
        "Bullets"),
    COLLECTION_WEAPON(
        "Pistol",
        30,
        1,
        10,
        0,
        PocketDamagePiercing,
        PocketWeaponRanged | PocketWeaponAmmunition,
        "Bullets"),
    COLLECTION_ARMOR("Padded Armor", 80, 11, -1, 0),
    COLLECTION_ARMOR("Leather Armor", 100, 11, -1, 0),
    COLLECTION_ARMOR("Studded Leather Armor", 130, 12, -1, 0),
    COLLECTION_ARMOR("Hide Armor", 120, 12, 2, 0),
    COLLECTION_ARMOR("Chain Shirt", 200, 13, 2, 0),
    COLLECTION_ARMOR("Scale Mail", 450, 14, 2, 0),
    COLLECTION_ARMOR("Breastplate", 200, 14, 2, 0),
    COLLECTION_ARMOR("Half Plate Armor", 400, 15, 2, 0),
    COLLECTION_ARMOR("Ring Mail", 400, 14, 0, 0),
    COLLECTION_ARMOR("Chain Mail", 550, 16, 0, 0),
    COLLECTION_ARMOR("Splint Armor", 600, 17, 0, 0),
    COLLECTION_ARMOR("Plate Armor", 650, 18, 0, 0),
    COLLECTION_ARMOR("Shield", 60, 0, -1, 2),
};

#undef COLLECTION_WEAPON
#undef COLLECTION_ARMOR

static const char* const dndinventory_collection_attack_ability_names[] =
    {"Auto", "Strength", "Dexterity", "Best"};

static const char* const dndinventory_collection_item_filter_names[] = {
    "All",
    "Weapons",
    "Armor",
    "Ammunition",
    "Gear",
    "Tools",
    "Mounts/Vehicles",
    "Potions",
    "Rings",
    "Rods",
    "Scrolls",
    "Staffs",
    "Wands",
    "Wondrous",
    "Magic",
};

typedef struct {
    Gui* gui;
    Storage* storage;
    ViewDispatcher* dispatcher;
    View* view;
    TextInput* text_input;
    NumberInput* number_input;
    DndInventoryAppData data;
    DndInventoryCollectionScreen screen;
    DndInventoryCollectionScreen return_screen;
    DndInventoryCollectionEdit edit;
    uint32_t profile;
    uint8_t have_profile;
    uint8_t return_to_dnd;
    uint8_t total;
    uint8_t cache_start;
    uint32_t record_page_offsets[POCKET_D20_COLLECTION_PAGE_COUNT];
    uint8_t record_offset_valid_pages;
    uint16_t selection;
    uint16_t scroll;
    uint8_t record_index;
    uint8_t detail_selection;
    uint16_t detail_scroll;
    char edit_buffer[POCKET_D20_DETAIL_LEN];
    uint8_t number_field;
    uint8_t input_active;
    char status[32];
    uint8_t status_transient;
    uint8_t action_ack_active;
    uint16_t action_ack_selection;
    DndInventoryCatalogEntry catalog[DNDINVENTORY_COLLECTION_CATALOG_PAGE];
    uint8_t catalog_count;
    uint16_t catalog_page_start;
    uint16_t catalog_offset_base_page;
    uint8_t catalog_has_more;
    uint32_t catalog_page_offsets[DNDINVENTORY_COLLECTION_CATALOG_OFFSET_PAGES];
    uint8_t catalog_offset_valid_pages;
    uint8_t item_filter;
    uint8_t tool_selection;
    uint8_t tool_scroll;
    DndInventoryItemAggregate item_aggregate;
    uint8_t item_aggregate_valid;
    uint8_t grant_state;
} DndInventoryCollectionApp;

static bool dndinventory_collection_load_page(DndInventoryCollectionApp* app, uint8_t start);
static bool dndinventory_collection_save_page(DndInventoryCollectionApp* app);
static bool
    dndinventory_collection_prepare_record(DndInventoryCollectionApp* app, uint8_t logical);
static PocketItem* dndinventory_collection_item(DndInventoryCollectionApp* app, uint8_t logical);
static void dndinventory_collection_begin_text(
    DndInventoryCollectionApp* app,
    DndInventoryCollectionEdit edit,
    const char* header,
    const char* initial);
static bool dndinventory_collection_begin_number(DndInventoryCollectionApp* app, uint8_t field);

static void dndinventory_collection_copy(char* destination, size_t size, const char* source) {
    if(!destination || !size) return;
    if(!source) source = "";
    strncpy(destination, source, size - 1U);
    destination[size - 1U] = '\0';
}

static int16_t dndinventory_collection_clamp_i16(int32_t value, int16_t minimum, int16_t maximum) {
    if(value < minimum) return minimum;
    if(value > maximum) return maximum;
    return (int16_t)value;
}

static uint8_t dndinventory_collection_clamp_u8(int32_t value, uint8_t maximum) {
    if(value < 0) return 0U;
    if(value > maximum) return maximum;
    return (uint8_t)value;
}

typedef struct {
    DndInventoryItemAggregate* aggregate;
} DndInventoryItemAggregateContext;

static bool dndinventory_collection_aggregate_item_record(
    uint8_t logical_index,
    const PocketItem* item,
    void* context) {
    (void)logical_index;
    DndInventoryItemAggregateContext* aggregate_context = context;
    if(!aggregate_context || !aggregate_context->aggregate || !item) return true;
    DndInventoryItemAggregate* aggregate = aggregate_context->aggregate;
    if(item->quantity > 0) {
        int32_t weight = (int32_t)item->weight_tenths * item->quantity;
        int32_t carried = (int32_t)aggregate->carried_weight_tenths + weight;
        aggregate->carried_weight_tenths = carried > INT16_MAX ? INT16_MAX :
                                           carried < INT16_MIN ? INT16_MIN :
                                                                 (int16_t)carried;
        if(item->equipped) {
            int32_t equipped = (int32_t)aggregate->equipped_weight_tenths + weight;
            aggregate->equipped_weight_tenths = equipped > INT16_MAX ? INT16_MAX :
                                                equipped < INT16_MIN ? INT16_MIN :
                                                                       (int16_t)equipped;
        }
    }
    if(item->attuned && aggregate->attuned_count < UINT8_MAX) ++aggregate->attuned_count;
    if(item->equipped) {
        if(item->shield_bonus) {
            uint16_t shield = (uint16_t)aggregate->shield_bonus + item->shield_bonus;
            aggregate->shield_bonus = shield > UINT8_MAX ? UINT8_MAX : (uint8_t)shield;
        }
        if(item->armor_base && !aggregate->armor_base) {
            aggregate->armor_base = item->armor_base;
            aggregate->armor_dex_cap = item->armor_dex_cap;
        }
    }
    return true;
}

static bool dndinventory_collection_item_aggregate(
    Storage* storage,
    uint32_t profile,
    DndInventoryItemAggregate* aggregate,
    uint8_t* total_count) {
    if(!storage || !aggregate) return false;
    memset(aggregate, 0, sizeof(*aggregate));
    aggregate->armor_dex_cap = -1;
    DndInventoryItemAggregateContext context = {.aggregate = aggregate};
    return dnd_storage_visit_items(
        storage, profile, dndinventory_collection_aggregate_item_record, &context, total_count);
}

static uint8_t dndinventory_collection_cycle_die(uint8_t current, int8_t delta) {
    static const uint8_t dice[] = {4U, 6U, 8U, 10U, 12U};
    uint8_t index = 0U;
    for(uint8_t i = 0U; i < sizeof(dice); ++i) {
        if(dice[i] == current) {
            index = i;
            break;
        }
    }
    int16_t next = (int16_t)index + delta;
    if(next < 0) next = (int16_t)sizeof(dice) - 1;
    if(next >= (int16_t)sizeof(dice)) next = 0;
    return dice[next];
}

static void dndinventory_collection_set_status(DndInventoryCollectionApp* app, const char* text) {
    dndinventory_collection_copy(app->status, sizeof(app->status), text);
    app->status_transient = 0U;
}

static void
    dndinventory_collection_set_transient_status(DndInventoryCollectionApp* app, const char* text) {
    dndinventory_collection_copy(app->status, sizeof(app->status), text);
    app->status_transient = 1U;
}

static void dndinventory_collection_draw_header(
    Canvas* canvas,
    DndInventoryCollectionApp* app,
    const char* title,
    const char* status) {
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_box(canvas, 0, 0, 128, 10);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 8, title);

    /* Character ID belongs only to the main list screen. Catalog paging hints
       are shown only inside the explicit Name/catalog picker, not persistently
       on the normal Inventory list. */
    bool main_list = app && app->screen == DndInventoryCollectionScreenList &&
                     app->profile != UINT32_MAX;
    uint16_t status_right = 126U;
    if(main_list) {
        char profile_id[16];
        snprintf(profile_id, sizeof(profile_id), "[%lu]", (unsigned long)app->profile);
        uint16_t id_width = canvas_string_width(canvas, profile_id);
        uint8_t id_x = id_width < 125U ? (uint8_t)(126U - id_width) : 1U;
        canvas_draw_str(canvas, id_x, 8, profile_id);
        status_right = id_x > 2U ? (uint16_t)(id_x - 2U) : 0U;
    }

    if(status && status[0]) {
        uint16_t title_width = canvas_string_width(canvas, title);
        uint16_t status_width = canvas_string_width(canvas, status);
        if(status_width < status_right) {
            uint16_t status_x = status_right - status_width;
            if(status_x > title_width + 4U) canvas_draw_str(canvas, (uint8_t)status_x, 8, status);
        }
    }
    canvas_set_color(canvas, ColorBlack);
}

static void
    dndinventory_collection_draw_row(Canvas* canvas, uint8_t row, bool selected, const char* text) {
    uint8_t y = (uint8_t)(11U + row * 10U);
    char display[27];
    size_t length = strlen(text);
    size_t copy = length > 25U ? 25U : length;
    memcpy(display, text, copy);
    display[copy] = '\0';
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

static void dndinventory_collection_redraw(DndInventoryCollectionApp* app) {
    if(!app || !app->view) return;
    DndInventoryCollectionApp** model = view_get_model(app->view);
    if(!model) return;
    *model = app;
    view_commit_model(app->view, true);
}

static char* dndinventory_collection_trim(char* text) {
    if(!text) return NULL;
    while(*text == ' ' || *text == '\t')
        ++text;
    char* end = text + strlen(text);
    while(end > text && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r'))
        --end;
    *end = '\0';
    return text;
}

static bool dndinventory_collection_equals_ci(const char* left, const char* right) {
    if(!left || !right) return false;
    while(*left && *right) {
        char a = *left++;
        char b = *right++;
        if(a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
        if(b >= 'A' && b <= 'Z') b = (char)(b - 'A' + 'a');
        if(a != b) return false;
    }
    return *left == '\0' && *right == '\0';
}

static uint8_t dndinventory_collection_item_category(const char* category) {
    if(!category) return DndInventoryItemCategoryOther;
    if(strcmp(category, "Weapon") == 0) return DndInventoryItemCategoryWeapon;
    if(strcmp(category, "Armor") == 0) return DndInventoryItemCategoryArmor;
    if(strcmp(category, "Gear") == 0) return DndInventoryItemCategoryGear;
    if(strcmp(category, "Tool") == 0) return DndInventoryItemCategoryTool;
    if(strcmp(category, "Mount/Vehicle") == 0) return DndInventoryItemCategoryMountVehicle;
    if(strcmp(category, "Potion") == 0) return DndInventoryItemCategoryPotion;
    if(strcmp(category, "Ring") == 0) return DndInventoryItemCategoryRing;
    if(strcmp(category, "Rod") == 0) return DndInventoryItemCategoryRod;
    if(strcmp(category, "Scroll") == 0) return DndInventoryItemCategoryScroll;
    if(strcmp(category, "Staff") == 0) return DndInventoryItemCategoryStaff;
    if(strcmp(category, "Wand") == 0) return DndInventoryItemCategoryWand;
    if(strcmp(category, "Wondrous") == 0) return DndInventoryItemCategoryWondrous;
    return DndInventoryItemCategoryOther;
}

static const char* dndinventory_collection_item_mark(uint8_t category) {
    switch(category) {
    case DndInventoryItemCategoryWeapon:
        return "W";
    case DndInventoryItemCategoryArmor:
        return "A";
    case DndInventoryItemCategoryGear:
        return "G";
    case DndInventoryItemCategoryTool:
        return "T";
    case DndInventoryItemCategoryMountVehicle:
        return "M";
    case DndInventoryItemCategoryPotion:
        return "P";
    case DndInventoryItemCategoryRing:
        return "R";
    case DndInventoryItemCategoryRod:
        return "D";
    case DndInventoryItemCategoryScroll:
        return "S";
    case DndInventoryItemCategoryStaff:
        return "F";
    case DndInventoryItemCategoryWand:
        return "N";
    case DndInventoryItemCategoryWondrous:
        return "O";
    default:
        return "?";
    }
}

static bool dndinventory_collection_item_is_ammunition(const char* name) {
    if(!name) return false;
    return strncmp(name, "Ammunition", 10U) == 0 || strcmp(name, "Arrows") == 0 ||
           strcmp(name, "Bolts") == 0 || strncmp(name, "Bullets,", 8U) == 0 ||
           strcmp(name, "Needles") == 0 || strcmp(name, "Case, Crossbow Bolt") == 0 ||
           strcmp(name, "Unbreakable Arrow") == 0 || strstr(name, " Ammunition") != NULL;
}

static bool dndinventory_collection_item_filter_allows(
    DndInventoryCollectionApp* app,
    const char* name,
    uint8_t category,
    bool magic) {
    bool ammunition = dndinventory_collection_item_is_ammunition(name);
    switch(app->item_filter) {
    case DndInventoryItemFilterWeapons:
        return category == DndInventoryItemCategoryWeapon && !ammunition;
    case DndInventoryItemFilterArmor:
        return category == DndInventoryItemCategoryArmor;
    case DndInventoryItemFilterAmmunition:
        return ammunition;
    case DndInventoryItemFilterGear:
        return category == DndInventoryItemCategoryGear && !ammunition;
    case DndInventoryItemFilterTools:
        return category == DndInventoryItemCategoryTool;
    case DndInventoryItemFilterMountVehicles:
        return category == DndInventoryItemCategoryMountVehicle;
    case DndInventoryItemFilterPotions:
        return category == DndInventoryItemCategoryPotion;
    case DndInventoryItemFilterRings:
        return category == DndInventoryItemCategoryRing;
    case DndInventoryItemFilterRods:
        return category == DndInventoryItemCategoryRod;
    case DndInventoryItemFilterScrolls:
        return category == DndInventoryItemCategoryScroll;
    case DndInventoryItemFilterStaffs:
        return category == DndInventoryItemCategoryStaff;
    case DndInventoryItemFilterWands:
        return category == DndInventoryItemCategoryWand;
    case DndInventoryItemFilterWondrous:
        return category == DndInventoryItemCategoryWondrous;
    case DndInventoryItemFilterMagic:
        return magic;
    default:
        return true;
    }
}

static void dndinventory_collection_apply_item_preset(
    PocketItem* item,
    const char* name,
    uint8_t category) {
    item->weight_tenths = 0;
    item->is_weapon = category == DndInventoryItemCategoryWeapon;
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
    for(size_t i = 0U; i < sizeof(dndinventory_collection_equipment_presets) /
                               sizeof(dndinventory_collection_equipment_presets[0]);
        ++i) {
        const DndInventoryEquipmentPreset* preset = &dndinventory_collection_equipment_presets[i];
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
        dndinventory_collection_copy(
            item->ammunition_group, sizeof(item->ammunition_group), preset->ammunition_group);
        break;
    }
}

static void dndinventory_collection_projection_from_state(
    const DndInventoryCharacterState* state,
    DndInventoryProfileProjection* projection) {
    memset(projection, 0, sizeof(*projection));
    if(!state) return;
    dndinventory_collection_copy(projection->name, sizeof(projection->name), state->name);
    dndinventory_collection_copy(projection->species, sizeof(projection->species), state->species);
    dndinventory_collection_copy(
        projection->background, sizeof(projection->background), state->background);
    projection->class_count = state->class_count;
    for(uint8_t i = 0U; i < state->class_count && i < POCKET_D20_MAX_CLASSES; ++i)
        projection->classes[i] = state->classes[i];
    memcpy(projection->ability_scores, state->ability_scores, sizeof(projection->ability_scores));
    projection->armor_class = state->armor_class;
    projection->exhaustion = state->exhaustion;
    projection->encumbrance_mode = state->encumbrance_mode;
    projection->carrying_capacity_override = state->carrying_capacity_override;
}

static void dndinventory_collection_state_from_projection(
    DndInventoryCharacterState* state,
    const DndInventoryProfileProjection* projection) {
    PocketItem* items = state->items;
    uint8_t item_count = state->item_count;
    memset(state, 0, sizeof(*state));
    state->items = items;
    state->item_count = item_count;
    if(!projection) return;
    dndinventory_collection_copy(state->name, sizeof(state->name), projection->name);
    dndinventory_collection_copy(state->species, sizeof(state->species), projection->species);
    dndinventory_collection_copy(
        state->background, sizeof(state->background), projection->background);
    state->class_count = projection->class_count;
    for(uint8_t i = 0U; i < projection->class_count && i < POCKET_D20_MAX_CLASSES; ++i)
        state->classes[i] = projection->classes[i];
    memcpy(state->ability_scores, projection->ability_scores, sizeof(state->ability_scores));
    state->armor_class = projection->armor_class;
    state->exhaustion = projection->exhaustion;
    state->encumbrance_mode = projection->encumbrance_mode;
    state->carrying_capacity_override = projection->carrying_capacity_override;
}

static PocketCharacter* dndinventory_collection_io_character(
    const DndInventoryCharacterState* state,
    bool attach_items) {
    PocketCharacter* io = calloc(1U, sizeof(PocketCharacter));
    if(!io || !state) return io;
    dndinventory_collection_copy(io->name, sizeof(io->name), state->name);
    io->class_count = state->class_count;
    for(uint8_t i = 0U; i < state->class_count && i < POCKET_D20_MAX_CLASSES; ++i)
        io->classes[i] = state->classes[i];
    io->currency_cp = state->currency_cp;
    io->currency_sp = state->currency_sp;
    io->currency_ep = state->currency_ep;
    io->currency_gp = state->currency_gp;
    io->currency_pp = state->currency_pp;
    if(attach_items) {
        io->item_count = state->item_count;
        io->item_capacity = state->item_count;
        io->items = state->items;
    }
    return io;
}

static void dndinventory_collection_free_io_character(PocketCharacter* io, bool owns_items) {
    if(!io) return;
    if(owns_items)
        dnd_data_clear_items(io);
    else
        io->items = NULL;
    free(io);
}

static bool dndinventory_collection_save_character(DndInventoryCollectionApp* app) {
    if(!app || !app->have_profile) return false;
    DndInventoryProfileProjection projection;
    dndinventory_collection_projection_from_state(&app->data.character, &projection);
    bool ok = dnd_profile_projection_save_inventory_owned(app->storage, app->profile, &projection);
    if(ok)
        dndinventory_collection_set_transient_status(app, "Saved");
    else
        dndinventory_collection_set_status(app, "UNSAVED");
    return ok;
}

static bool dndinventory_collection_save_currency(DndInventoryCollectionApp* app) {
    if(!app || !app->have_profile) return false;
    DndInventoryCharacterState* c = &app->data.character;
    int32_t currency[5] = {
        c->currency_cp,
        c->currency_sp,
        c->currency_ep,
        c->currency_gp,
        c->currency_pp,
    };
    PocketCharacter* owner = dndinventory_collection_io_character(c, false);
    bool ok = owner &&
              dnd_storage_save_inventory_currency(app->storage, app->profile, owner, currency);
    dndinventory_collection_free_io_character(owner, false);
    if(ok) app->record_offset_valid_pages = 0U;
    if(ok)
        dndinventory_collection_set_transient_status(app, "Saved");
    else
        dndinventory_collection_set_status(app, "UNSAVED");
    return ok;
}

static bool dndinventory_collection_load_currency(DndInventoryCollectionApp* app) {
    if(!app || !app->have_profile) return false;
    if(!dnd_storage_items_exist(app->storage, app->profile)) return true;
    DndInventoryCharacterState* c = &app->data.character;
    int32_t currency[5];
    bool found = false;
    if(!dnd_storage_load_inventory_currency(app->storage, app->profile, currency, &found))
        return false;
    if(found) {
        c->currency_cp = currency[0];
        c->currency_sp = currency[1];
        c->currency_ep = currency[2];
        c->currency_gp = currency[3];
        c->currency_pp = currency[4];
        return true;
    }

    /* Inventory is the sole currency owner. If another FAP created an
       item-only sidecar, opening DNDInventory initializes its missing metadata
       with an explicit zero balance. No other FAP creates Currency=. */
    c->currency_cp = 0;
    c->currency_sp = 0;
    c->currency_ep = 0;
    c->currency_gp = 0;
    c->currency_pp = 0;
    int32_t zero_currency[5] = {0, 0, 0, 0, 0};
    PocketCharacter* owner = dndinventory_collection_io_character(c, false);
    bool saved = owner && dnd_storage_save_inventory_currency(
                              app->storage, app->profile, owner, zero_currency);
    dndinventory_collection_free_io_character(owner, false);
    if(saved) app->record_offset_valid_pages = 0U;
    return saved;
}

static bool dndinventory_collection_refresh_item_aggregate(DndInventoryCollectionApp* app) {
    if(!app) return false;
    memset(&app->item_aggregate, 0, sizeof(app->item_aggregate));
    app->item_aggregate.armor_dex_cap = -1;
    if(!dnd_storage_items_exist(app->storage, app->profile)) {
        app->item_aggregate_valid = 1U;
        return true;
    }
    uint8_t total = 0U;
    bool ok = dndinventory_collection_item_aggregate(
        app->storage, app->profile, &app->item_aggregate, &total);
    app->item_aggregate_valid = ok ? 1U : 0U;
    return ok;
}

static void dndinventory_collection_refresh_grant_state(DndInventoryCollectionApp* app) {
    if(!app || !app->have_profile) return;
    app->grant_state = DndInventoryGrantAvailable;
    if(!dnd_storage_items_exist(app->storage, app->profile)) return;

    uint8_t grant_marker = 0U;
    if(!dnd_storage_inventory_initial_grant_state(app->storage, app->profile, &grant_marker)) {
        app->grant_state = DndInventoryGrantReadError;
        return;
    }
    if(grant_marker >= 2U) {
        app->grant_state = DndInventoryGrantOverrideUsed;
        return;
    }
    if(grant_marker == 1U) {
        app->grant_state = DndInventoryGrantGranted;
        return;
    }

    uint8_t existing_items = 0U;
    if(!dnd_storage_visit_items(app->storage, app->profile, NULL, NULL, &existing_items)) {
        app->grant_state = DndInventoryGrantReadError;
        return;
    }
    if(existing_items) app->grant_state = DndInventoryGrantBlockedByItems;
}

static bool dndinventory_collection_grant_initial_inventory(DndInventoryCollectionApp* app) {
    if(!app || !app->have_profile) return false;
    if(dnd_storage_items_exist(app->storage, app->profile)) {
        uint8_t grant_marker = 0U;
        if(!dnd_storage_inventory_initial_grant_state(app->storage, app->profile, &grant_marker)) {
            dndinventory_collection_set_status(app, "Inventory read failed");
            return false;
        }
        if(grant_marker >= 2U) {
            app->grant_state = DndInventoryGrantOverrideUsed;
            dndinventory_collection_set_transient_status(app, "Already regranted");
            return true;
        }
        if(grant_marker == 1U) {
            app->grant_state = DndInventoryGrantGranted;
            dndinventory_collection_set_transient_status(app, "Already granted");
            return true;
        }
        uint8_t existing_items = 0U;
        if(!dnd_storage_visit_items(app->storage, app->profile, NULL, NULL, &existing_items)) {
            dndinventory_collection_set_status(app, "Inventory read failed");
            return false;
        }
        if(existing_items) {
            app->grant_state = DndInventoryGrantBlockedByItems;
            dndinventory_collection_set_status(app, "Items already exist");
            return true;
        }
    }
    bool initialized = false;
    if(!dndinventory_items_initialize_inventory(
           app->storage, app->profile, &app->data.character, &initialized)) {
        dndinventory_collection_set_status(app, "Grant failed");
        return false;
    }
    if(!initialized) {
        dndinventory_collection_set_transient_status(app, "No starting gear");
        return true;
    }
    app->record_offset_valid_pages = 0U;
    if(!dndinventory_collection_load_page(app, 0U)) {
        dndinventory_collection_set_status(app, "Granted; reload failed");
        return true;
    }
    app->selection = app->scroll = 0U;
    app->item_aggregate_valid = 0U;
    app->grant_state = DndInventoryGrantGranted;
    app->screen = DndInventoryCollectionScreenList;
    dndinventory_collection_set_transient_status(app, "Granted");
    return true;
}

static bool dndinventory_collection_regrant_initial_inventory(DndInventoryCollectionApp* app) {
    if(!app || !app->have_profile) return false;
    uint8_t grant_marker = 0U;
    if(!dnd_storage_inventory_initial_grant_state(app->storage, app->profile, &grant_marker)) {
        dndinventory_collection_set_status(app, "Inventory read failed");
        return false;
    }
    if(grant_marker >= 2U) {
        app->grant_state = DndInventoryGrantOverrideUsed;
        dndinventory_collection_set_transient_status(app, "Override already used");
        return true;
    }
    if(grant_marker != 1U) {
        dndinventory_collection_set_transient_status(app, "Grant first");
        return true;
    }

    bool regranted = false;
    if(!dndinventory_items_regrant_inventory_once(
           app->storage, app->profile, &app->data.character, &regranted)) {
        dndinventory_collection_set_status(app, "Regrant failed");
        return false;
    }
    if(!regranted) {
        dndinventory_collection_set_status(app, "Regrant unavailable");
        return false;
    }
    app->record_offset_valid_pages = 0U;
    if(!dndinventory_collection_load_page(app, 0U)) {
        dndinventory_collection_set_status(app, "Regranted; reload failed");
        return true;
    }
    app->selection = app->scroll = 0U;
    app->item_aggregate_valid = 0U;
    app->grant_state = DndInventoryGrantOverrideUsed;
    app->screen = DndInventoryCollectionScreenList;
    dndinventory_collection_set_transient_status(app, "Regranted");
    return true;
}

static uint8_t
    dndinventory_collection_local(const DndInventoryCollectionApp* app, uint8_t logical) {
    return (uint8_t)(logical - app->cache_start);
}

static void dndinventory_collection_focus_list(DndInventoryCollectionApp* app, uint8_t logical) {
    app->selection = (uint16_t)logical + 1U;
    uint16_t page_min = (uint16_t)app->cache_start + 1U;
    uint16_t page_max = page_min + POCKET_D20_COLLECTION_CACHE_SIZE - 1U;
    uint16_t max_selection = app->total;
    if(page_max > max_selection) page_max = max_selection;
    uint16_t scroll = app->selection > 4U ? app->selection - 4U : 0U;
    if(scroll && scroll < page_min) scroll = page_min;
    if(scroll + 4U > page_max && page_max >= 4U) scroll = page_max - 4U;
    if(app->selection == 0U) scroll = 0U;
    app->scroll = scroll;
}

typedef struct {
    File* file;
    uint8_t buffer[DNDINVENTORY_COLLECTION_CATALOG_READ_BUFFER];
    uint16_t position;
    uint16_t count;
    uint32_t raw_offset;
} DndInventoryCatalogReader;

static void dndinventory_collection_catalog_reader_init(
    DndInventoryCatalogReader* reader,
    File* file,
    uint32_t raw_offset) {
    memset(reader, 0, sizeof(*reader));
    reader->file = file;
    reader->raw_offset = raw_offset;
}

static bool
    dndinventory_collection_catalog_reader_next(DndInventoryCatalogReader* reader, char* value) {
    if(reader->position >= reader->count) {
        reader->count =
            (uint16_t)storage_file_read(reader->file, reader->buffer, sizeof(reader->buffer));
        reader->position = 0U;
        if(!reader->count) return false;
    }
    *value = (char)reader->buffer[reader->position++];
    if(reader->raw_offset != UINT32_MAX) ++reader->raw_offset;
    return true;
}

static bool dndinventory_collection_catalog_read_line(
    DndInventoryCatalogReader* reader,
    char* line,
    size_t size) {
    if(!reader || !line || size < 2U) return false;
    size_t used = 0U;
    char ch = '\0';
    bool got = false;
    while(true) {
        if(!dndinventory_collection_catalog_reader_next(reader, &ch)) break;
        got = true;
        if(ch == '\n') break;
        if(ch != '\r' && used + 1U < size) line[used++] = ch;
    }
    line[used] = '\0';
    return got || used > 0U;
}

static void dndinventory_collection_reset_catalog_offsets(DndInventoryCollectionApp* app) {
    if(!app) return;
    memset(app->catalog_page_offsets, 0, sizeof(app->catalog_page_offsets));
    app->catalog_page_offsets[0] = 0U;
    app->catalog_offset_base_page = 0U;
    app->catalog_offset_valid_pages = 1U;
}

static void dndinventory_collection_cache_catalog_offset(
    DndInventoryCollectionApp* app,
    uint16_t page_index,
    uint32_t raw_offset) {
    if(!app) return;
    if(!app->catalog_offset_valid_pages) dndinventory_collection_reset_catalog_offsets(app);

    if(page_index < app->catalog_offset_base_page) return;

    uint16_t relative = (uint16_t)(page_index - app->catalog_offset_base_page);
    if(relative >= DNDINVENTORY_COLLECTION_CATALOG_OFFSET_PAGES) {
        uint16_t shift = (uint16_t)(relative - DNDINVENTORY_COLLECTION_CATALOG_OFFSET_PAGES + 1U);
        if(shift >= app->catalog_offset_valid_pages) {
            app->catalog_offset_base_page = page_index;
            app->catalog_page_offsets[0] = raw_offset;
            app->catalog_offset_valid_pages = 1U;
            return;
        }
        memmove(
            app->catalog_page_offsets,
            app->catalog_page_offsets + shift,
            (app->catalog_offset_valid_pages - shift) * sizeof(app->catalog_page_offsets[0]));
        app->catalog_offset_base_page = (uint16_t)(app->catalog_offset_base_page + shift);
        app->catalog_offset_valid_pages = (uint8_t)(app->catalog_offset_valid_pages - shift);
        relative = (uint16_t)(page_index - app->catalog_offset_base_page);
    }

    if(relative < DNDINVENTORY_COLLECTION_CATALOG_OFFSET_PAGES) {
        app->catalog_page_offsets[relative] = raw_offset;
        if(app->catalog_offset_valid_pages <= relative)
            app->catalog_offset_valid_pages = (uint8_t)(relative + 1U);
    }
}

static void dndinventory_collection_list_adjust_scroll(DndInventoryCollectionApp* app) {
    if(!app) return;
    uint16_t count = (uint16_t)app->total + 1U;
    if(!count) return;
    if(app->selection >= count) app->selection = count - 1U;
    if(app->selection == 0U || app->total == 0U) {
        app->scroll = 0U;
        return;
    }

    uint8_t logical = (uint8_t)(app->selection - 1U);
    uint8_t page_start =
        (uint8_t)((logical / POCKET_D20_COLLECTION_CACHE_SIZE) * POCKET_D20_COLLECTION_CACHE_SIZE);
    uint16_t first = (uint16_t)page_start + 1U;
    uint8_t page_records = (uint8_t)(app->total - page_start);
    if(page_records > POCKET_D20_COLLECTION_CACHE_SIZE)
        page_records = POCKET_D20_COLLECTION_CACHE_SIZE;
    uint16_t last = first + page_records - 1U;

    /* Preserve + Add New on the first viewport until a fifth Item actually
       needs the row. Never let a five-row draw cross an eight-record cache page. */
    if(page_start == 0U && app->selection <= 4U) {
        app->scroll = 0U;
        return;
    }
    if(page_records <= DNDINVENTORY_COLLECTION_ROWS) {
        app->scroll = first;
        return;
    }

    uint16_t scroll = app->selection > first + 3U ? app->selection - 4U : first;
    uint16_t maximum = last - (DNDINVENTORY_COLLECTION_ROWS - 1U);
    if(scroll > maximum) scroll = maximum;
    if(scroll < first) scroll = first;
    app->scroll = scroll;
}

static bool
    dndinventory_collection_ensure_list_page(DndInventoryCollectionApp* app, uint16_t selection) {
    if(!app) return false;
    if(selection == 0U) {
        if(app->total && app->cache_start != 0U) return dndinventory_collection_load_page(app, 0U);
        return true;
    }
    uint8_t logical = (uint8_t)(selection - 1U);
    uint8_t target =
        (uint8_t)((logical / POCKET_D20_COLLECTION_CACHE_SIZE) * POCKET_D20_COLLECTION_CACHE_SIZE);
    if(target == app->cache_start) return true;
    return dndinventory_collection_load_page(app, target);
}

static bool dndinventory_collection_move_list(DndInventoryCollectionApp* app, int8_t delta) {
    uint16_t count = (uint16_t)app->total + 1U;
    if(!count) return false;
    int32_t next = (int32_t)app->selection + delta;
    if(next < 0) next = count - 1U;
    if(next >= count) next = 0;
    if(!dndinventory_collection_ensure_list_page(app, (uint16_t)next)) {
        dndinventory_collection_set_status(app, "Read failed");
        return false;
    }
    app->selection = (uint16_t)next;
    app->status[0] = '\0';
    dndinventory_collection_list_adjust_scroll(app);
    return true;
}

static bool dndinventory_collection_page_list(DndInventoryCollectionApp* app, int8_t delta) {
    if(!app || !app->total || !delta) return false;
    uint16_t target = app->cache_start;
    if(delta < 0) {
        if(!app->cache_start) return false;
        target = app->cache_start >= POCKET_D20_COLLECTION_CACHE_SIZE ?
                     (uint16_t)(app->cache_start - POCKET_D20_COLLECTION_CACHE_SIZE) :
                     0U;
    } else {
        target = (uint16_t)app->cache_start + POCKET_D20_COLLECTION_CACHE_SIZE;
        if(target >= app->total) return false;
    }
    if(!dndinventory_collection_load_page(app, (uint8_t)target)) {
        dndinventory_collection_set_status(app, "Read failed");
        return false;
    }
    app->selection = target + 1U;
    app->status[0] = '\0';
    dndinventory_collection_list_adjust_scroll(app);
    return true;
}

static void
    dndinventory_collection_draw_inventory_tools(Canvas* canvas, DndInventoryCollectionApp* app) {
    const char* rows[] = {
        "Currency",
        "Inventory Resources",
        "Grant Initial Inventory",
    };
    if(app->grant_state == DndInventoryGrantGranted)
        rows[2] = "Initial Inv: Granted";
    else if(app->grant_state == DndInventoryGrantOverrideUsed)
        rows[2] = "Initial Inv: Regranted";
    else if(app->grant_state == DndInventoryGrantBlockedByItems)
        rows[2] = "Grant blocked: items";
    else if(app->grant_state == DndInventoryGrantReadError)
        rows[2] = "Grant status read error";
    dndinventory_collection_draw_header(canvas, app, "Inventory Tools", app->status);
    for(uint8_t row = 0U; row < 3U; ++row)
        dndinventory_collection_draw_row(canvas, row, row == app->tool_selection, rows[row]);
}

static void dndinventory_collection_draw_currency(Canvas* canvas, DndInventoryCollectionApp* app) {
    const DndInventoryCharacterState* c = &app->data.character;
    char rows[5][48];
    snprintf(rows[0], sizeof(rows[0]), "Copper: %ld", (long)c->currency_cp);
    snprintf(rows[1], sizeof(rows[1]), "Silver: %ld", (long)c->currency_sp);
    snprintf(rows[2], sizeof(rows[2]), "Electrum: %ld", (long)c->currency_ep);
    snprintf(rows[3], sizeof(rows[3]), "Gold: %ld", (long)c->currency_gp);
    snprintf(rows[4], sizeof(rows[4]), "Platinum: %ld", (long)c->currency_pp);
    dndinventory_collection_draw_header(canvas, app, "Currency", app->status);
    for(uint8_t row = 0U; row < 5U; ++row)
        dndinventory_collection_draw_row(canvas, row, row == app->tool_selection, rows[row]);
}

static void
    dndinventory_collection_draw_resources(Canvas* canvas, DndInventoryCollectionApp* app) {
    const DndInventoryCharacterState* c = &app->data.character;
    const DndInventoryItemAggregate* aggregate = &app->item_aggregate;
    bool aggregate_ok = app->item_aggregate_valid != 0U;
    int16_t carried = aggregate_ok ? aggregate->carried_weight_tenths : 0;
    int16_t equipped = aggregate_ok ? aggregate->equipped_weight_tenths : 0;
    uint8_t attuned = aggregate_ok ? aggregate->attuned_count : 0U;
    int16_t formula_ac = aggregate_ok ? dndinventory_rules_calculated_armor_class(c, aggregate) :
                                        c->armor_class;
    char rows[9][48];
    snprintf(rows[0], sizeof(rows[0]), "Carried: %d.%d lb", carried / 10, abs(carried % 10));
    snprintf(rows[1], sizeof(rows[1]), "Equipped: %d.%d lb", equipped / 10, abs(equipped % 10));
    snprintf(rows[2], sizeof(rows[2]), "Capacity: %d lb", dndinventory_rules_carrying_capacity(c));
    snprintf(
        rows[3], sizeof(rows[3]), "Encumbrance: %s", c->encumbrance_mode ? "Variant" : "Standard");
    snprintf(rows[4], sizeof(rows[4]), "Attuned: %u/3%s", attuned, attuned > 3U ? " !" : "");
    snprintf(rows[5], sizeof(rows[5]), "Formula AC: %d", formula_ac);
    dndinventory_collection_copy(rows[6], sizeof(rows[6]), "Apply armor/shield AC");
    dndinventory_collection_copy(rows[7], sizeof(rows[7]), "Normalize coin values");
    snprintf(rows[8], sizeof(rows[8]), "Capacity override: %d", c->carrying_capacity_override);
    dndinventory_collection_draw_header(canvas, app, "Inventory Resources", app->status);
    for(uint8_t row = 0U; row < DNDINVENTORY_COLLECTION_ROWS; ++row) {
        uint8_t index = (uint8_t)(app->tool_scroll + row);
        if(index >= 9U) break;
        dndinventory_collection_draw_row(canvas, row, index == app->tool_selection, rows[index]);
    }
}

static void dndinventory_collection_release_text(DndInventoryCollectionApp* app) {
    if(!app->text_input || app->input_active) return;
    view_dispatcher_remove_view(app->dispatcher, DNDINVENTORY_COLLECTION_VIEW_TEXT);
    text_input_free(app->text_input);
    app->text_input = NULL;
}

static void dndinventory_collection_release_number(DndInventoryCollectionApp* app) {
    if(!app->number_input || app->input_active) return;
    view_dispatcher_remove_view(app->dispatcher, DNDINVENTORY_COLLECTION_VIEW_NUMBER);
    number_input_free(app->number_input);
    app->number_input = NULL;
}

static bool dndinventory_collection_navigation(void* context) {
    DndInventoryCollectionApp* app = context;
    if(!app) return false;
    app->input_active = 0U;
    app->edit = DndInventoryCollectionEditNone;
    view_dispatcher_switch_to_view(app->dispatcher, DNDINVENTORY_COLLECTION_VIEW_MAIN);
    dndinventory_collection_redraw(app);
    return true;
}

static bool
    dndinventory_collection_load_profile(DndInventoryCollectionApp* app, const char* args) {
    UNUSED(args);
    if(!app || !app->storage) return false;

    /* DNDInventory already links dnd_storage.c for character/collection I/O, so
       use that module's exact Active=<id> reader as the single storage path here.
       This never scans for or substitutes another character. Assign the ID before
       loading the character so a character-load failure still reports the exact
       persisted ID in the header. */
    uint32_t requested = 0U;
    if(!dnd_profile_ref_active_id(app->storage, &requested)) return false;
    app->profile = requested;

    DndInventoryProfileProjection projection;
    if(!dnd_profile_projection_load_inventory(app->storage, requested, &projection)) return false;
    dndinventory_collection_state_from_projection(&app->data.character, &projection);
    return true;
}

static bool dndinventory_collection_load_page(DndInventoryCollectionApp* app, uint8_t start) {
    PocketCharacter* io = dndinventory_collection_io_character(&app->data.character, false);
    if(!io) return false;
    uint8_t total = app->total;
    bool ok = dnd_storage_load_items_window_indexed(
        app->storage,
        app->profile,
        start,
        io,
        &total,
        app->record_page_offsets,
        &app->record_offset_valid_pages);
    if(ok) {
        free(app->data.character.items);
        app->data.character.items = io->items;
        app->data.character.item_count = io->item_count;
        io->items = NULL;
        io->item_count = io->item_capacity = 0U;
        app->total = total;
        app->cache_start = start;
    }
    dndinventory_collection_free_io_character(io, true);
    return ok;
}

static bool dndinventory_collection_save_page(DndInventoryCollectionApp* app) {
    /* DNDInventory owns Inventory metadata. If this is the first real Inventory
       write, establish the sidecar with Currency= before committing the Item
       page. Shared/non-Inventory Item writers deliberately do not do this. */
    if(!dnd_storage_items_exist(app->storage, app->profile)) {
        const DndInventoryCharacterState* c = &app->data.character;
        int32_t currency[5] = {
            c->currency_cp, c->currency_sp, c->currency_ep, c->currency_gp, c->currency_pp};
        PocketCharacter* owner = dndinventory_collection_io_character(c, false);
        bool currency_saved = owner && dnd_storage_save_inventory_currency(
                                           app->storage, app->profile, owner, currency);
        dndinventory_collection_free_io_character(owner, false);
        if(!currency_saved) {
            dndinventory_collection_set_status(app, "UNSAVED");
            return false;
        }
    }
    PocketCharacter* io = dndinventory_collection_io_character(&app->data.character, true);
    bool ok = io &&
              dnd_storage_save_items_window(app->storage, app->profile, app->cache_start, io);
    dndinventory_collection_free_io_character(io, false);
    if(ok) {
        app->item_aggregate_valid = 0U;
        app->record_offset_valid_pages = 0U;
    }
    if(ok)
        dndinventory_collection_set_transient_status(app, "Saved");
    else
        dndinventory_collection_set_status(app, "UNSAVED");
    return ok;
}

static bool
    dndinventory_collection_prepare_record(DndInventoryCollectionApp* app, uint8_t logical) {
    if(logical >= app->total) return false;
    uint8_t target =
        (uint8_t)((logical / POCKET_D20_COLLECTION_CACHE_SIZE) * POCKET_D20_COLLECTION_CACHE_SIZE);
    if(target != app->cache_start && !dndinventory_collection_load_page(app, target)) {
        dndinventory_collection_set_status(app, "Read failed");
        return false;
    }
    return logical >= app->cache_start &&
           logical < (uint8_t)(app->cache_start + app->data.character.item_count);
}

static PocketItem* dndinventory_collection_item(DndInventoryCollectionApp* app, uint8_t logical) {
    if(!dndinventory_collection_prepare_record(app, logical)) return NULL;
    uint8_t local = dndinventory_collection_local(app, logical);
    return local < app->data.character.item_count ? &app->data.character.items[local] : NULL;
}

static PocketItem*
    dndinventory_collection_item_cached(DndInventoryCollectionApp* app, uint8_t logical) {
    if(!app || logical < app->cache_start) return NULL;
    uint8_t local = (uint8_t)(logical - app->cache_start);
    return local < app->data.character.item_count ? &app->data.character.items[local] : NULL;
}

static bool dndinventory_collection_add_blank(DndInventoryCollectionApp* app) {
    if(app->total >= POCKET_D20_MAX_ITEMS) {
        dndinventory_collection_set_status(app, "Collection full");
        return false;
    }
    uint8_t target = (uint8_t)((app->total / POCKET_D20_COLLECTION_CACHE_SIZE) *
                               POCKET_D20_COLLECTION_CACHE_SIZE);
    if(target != app->cache_start && !dndinventory_collection_load_page(app, target)) {
        dndinventory_collection_set_status(app, "Tail read failed");
        return false;
    }
    DndInventoryCharacterState* c = &app->data.character;
    uint8_t expected = (uint8_t)(app->total - target);
    if(c->item_count != expected || c->item_count >= POCKET_D20_COLLECTION_CACHE_SIZE) {
        dndinventory_collection_set_status(app, "Item add failed");
        return false;
    }
    PocketItem* resized = realloc(c->items, (size_t)(c->item_count + 1U) * sizeof(PocketItem));
    if(!resized) {
        dndinventory_collection_set_status(app, "Item add failed");
        return false;
    }
    c->items = resized;
    PocketItem* item = &c->items[c->item_count];
    memset(item, 0, sizeof(*item));
    dndinventory_collection_copy(item->name, sizeof(item->name), "New Item");
    item->quantity = 1;
    item->damage_dice = 1U;
    item->damage_die = 6U;
    item->extra_die = 6U;
    item->add_ability_damage = 1U;
    item->container_index = -1;
    item->armor_dex_cap = -1;
    ++c->item_count;
    app->record_index = app->total++;
    bool saved = dndinventory_collection_save_page(app);
    dndinventory_collection_focus_list(app, app->record_index);
    app->detail_selection = 0U;
    app->detail_scroll = 0U;
    app->screen = DndInventoryCollectionScreenDetail;
    if(saved)
        dndinventory_collection_set_transient_status(app, "Item added");
    else
        dndinventory_collection_set_status(app, "Added - UNSAVED");
    return true;
}

static bool dndinventory_collection_delete_current(DndInventoryCollectionApp* app) {
    if(app->record_index >= app->total) return false;
    PocketCharacter* owner = dndinventory_collection_io_character(&app->data.character, false);
    bool deleted = owner &&
                   dnd_storage_delete_item(app->storage, app->profile, owner, app->record_index);
    dndinventory_collection_free_io_character(owner, false);
    if(!deleted) {
        dndinventory_collection_set_status(app, "Delete failed");
        return false;
    }
    app->record_offset_valid_pages = 0U;
    if(app->total) --app->total;
    uint8_t target = 0U;
    if(app->total) {
        uint8_t logical = app->record_index < app->total ? app->record_index :
                                                           (uint8_t)(app->total - 1U);
        target = (uint8_t)((logical / POCKET_D20_COLLECTION_CACHE_SIZE) *
                           POCKET_D20_COLLECTION_CACHE_SIZE);
        if(!dndinventory_collection_load_page(app, target)) {
            dndinventory_collection_set_status(app, "Deleted; read failed");
            return true;
        }
        dndinventory_collection_focus_list(app, logical);
    } else {
        (void)dndinventory_collection_load_page(app, 0U);
        app->selection = app->scroll = 0U;
    }
    app->screen = DndInventoryCollectionScreenList;
    dndinventory_collection_set_status(app, "Deleted");
    return true;
}

static bool dndinventory_collection_parse_catalog_line(
    DndInventoryCollectionApp* app,
    char* line,
    DndInventoryCatalogEntry* entry) {
    if(!line || !entry) return false;
    char* start = dndinventory_collection_trim(line);
    if(!*start || *start == '#') return false;
    memset(entry, 0, sizeof(*entry));
    char* category = strchr(start, '|');
    if(!category) return false;
    *category++ = '\0';
    char* rarity = strchr(category, '|');
    if(rarity) *rarity++ = '\0';
    char* source = rarity ? strchr(rarity, '|') : NULL;
    if(source) *source++ = '\0';
    UNUSED(source);
    start = dndinventory_collection_trim(start);
    category = dndinventory_collection_trim(category);
    rarity = rarity ? dndinventory_collection_trim(rarity) : NULL;
    entry->category = dndinventory_collection_item_category(category);
    entry->magic = rarity && !dndinventory_collection_equals_ci(rarity, "Mundane");
    if(!dndinventory_collection_item_filter_allows(app, start, entry->category, entry->magic))
        return false;
    dndinventory_collection_copy(entry->name, sizeof(entry->name), start);
    return entry->name[0] != '\0';
}

static bool dndinventory_collection_load_catalog(DndInventoryCollectionApp* app) {
    app->catalog_count = 0U;
    app->catalog_has_more = 0U;

    uint16_t page_index = app->catalog_page_start / DNDINVENTORY_COLLECTION_CATALOG_PAGE;
    if(!app->catalog_offset_valid_pages) dndinventory_collection_reset_catalog_offsets(app);

    if(page_index < app->catalog_offset_base_page)
        dndinventory_collection_reset_catalog_offsets(app);

    uint16_t cached_end =
        (uint16_t)(app->catalog_offset_base_page + app->catalog_offset_valid_pages - 1U);
    uint16_t seek_page = page_index <= cached_end ? page_index : cached_end;
    uint16_t seek_slot = (uint16_t)(seek_page - app->catalog_offset_base_page);

    File* file = storage_file_alloc(app->storage);
    if(!file) return false;
    if(!storage_file_open(
           file, DNDINVENTORY_COLLECTION_ITEM_CATALOG, FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_free(file);
        dndinventory_collection_set_status(app, "Catalog unavailable");
        return false;
    }

    uint32_t raw_offset = app->catalog_page_offsets[seek_slot];
    if(raw_offset && !storage_file_seek(file, raw_offset, true)) {
        storage_file_close(file);
        storage_file_free(file);
        return false;
    }

    DndInventoryCatalogReader reader;
    dndinventory_collection_catalog_reader_init(&reader, file, raw_offset);
    char line[DNDINVENTORY_COLLECTION_LINE_MAX];
    uint16_t matched = (uint16_t)(seek_page * DNDINVENTORY_COLLECTION_CATALOG_PAGE);
    const uint16_t target_start = app->catalog_page_start;
    const uint16_t target_end = (uint16_t)(target_start + DNDINVENTORY_COLLECTION_CATALOG_PAGE);

    while(dndinventory_collection_catalog_read_line(&reader, line, sizeof(line))) {
        DndInventoryCatalogEntry parsed;
        if(!dndinventory_collection_parse_catalog_line(app, line, &parsed)) continue;

        if(matched >= target_end) {
            app->catalog_has_more = 1U;
            break;
        }

        parsed.absolute_index = matched;
        if(matched >= target_start && app->catalog_count < DNDINVENTORY_COLLECTION_CATALOG_PAGE)
            app->catalog[app->catalog_count++] = parsed;
        ++matched;

        if((matched % DNDINVENTORY_COLLECTION_CATALOG_PAGE) == 0U) {
            uint16_t next_page = matched / DNDINVENTORY_COLLECTION_CATALOG_PAGE;
            dndinventory_collection_cache_catalog_offset(app, next_page, reader.raw_offset);
        }
    }

    bool success = storage_file_get_error(file) == FSE_OK;
    storage_file_close(file);
    storage_file_free(file);
    return success;
}

static void dndinventory_collection_open_catalog(DndInventoryCollectionApp* app) {
    app->screen = DndInventoryCollectionScreenCatalog;
    dndinventory_collection_reset_catalog_offsets(app);
    app->catalog_page_start = 0U;
    app->selection = 0U;
    if(!dndinventory_collection_load_catalog(app))
        dndinventory_collection_set_status(app, "Catalog unavailable");
}

static bool dndinventory_collection_apply_catalog(DndInventoryCollectionApp* app) {
    if(app->selection >= app->catalog_count) return false;
    DndInventoryCatalogEntry* selected = &app->catalog[app->selection];
    PocketItem* item = dndinventory_collection_item(app, app->record_index);
    if(!item) return false;
    dndinventory_collection_copy(item->name, sizeof(item->name), selected->name);
    dndinventory_collection_apply_item_preset(item, selected->name, selected->category);
    bool saved = dndinventory_collection_save_page(app);
    app->screen = DndInventoryCollectionScreenDetail;
    app->detail_selection = 0U;
    app->detail_scroll = 0U;
    if(saved)
        dndinventory_collection_set_transient_status(app, "Catalog choice saved");
    else
        dndinventory_collection_set_status(app, "Choice - UNSAVED");
    return saved;
}

static uint8_t dndinventory_collection_detail_count(void) {
    return 36U;
}

static void dndinventory_collection_format_detail(
    DndInventoryCollectionApp* app,
    uint8_t field,
    char* out,
    size_t size) {
    DndInventoryCharacterState* c = &app->data.character;
    PocketItem* item = dndinventory_collection_item_cached(app, app->record_index);
    if(!item) {
        dndinventory_collection_copy(out, size, "Read error");
        return;
    }
    switch(field) {
    case 0:
        snprintf(out, size, "Name: %.31s", item->name);
        break;
    case 1:
        snprintf(out, size, "Notes: %.31s", item->detail);
        break;
    case 2:
        snprintf(out, size, "Stack Qty: %d", item->quantity);
        break;
    case 3:
        snprintf(
            out, size, "Weight: %d.%d lb", item->weight_tenths / 10, abs(item->weight_tenths % 10));
        break;
    case 4:
        snprintf(out, size, "Equipped: %s", item->equipped ? "Yes" : "No");
        break;
    case 5:
        snprintf(out, size, "Attuned: %s", item->attuned ? "Yes" : "No");
        break;
    case 6:
        snprintf(out, size, "Weapon: %s", item->is_weapon ? "Yes" : "No");
        break;
    case 7:
        snprintf(
            out,
            size,
            "Attack ability: %s",
            dndinventory_collection_attack_ability_names[item->attack_ability]);
        break;
    case 8:
        snprintf(out, size, "Proficient: %s", item->proficient ? "Yes" : "No");
        break;
    case 9:
        snprintf(out, size, "Magic bonus: %+d", item->magic_bonus);
        break;
    case 10:
        snprintf(out, size, "Damage dice: %u", item->damage_dice);
        break;
    case 11:
        snprintf(out, size, "Damage die: d%u", item->damage_die);
        break;
    case 12:
        snprintf(out, size, "Versatile: %s", item->versatile_die ? "Yes" : "No");
        break;
    case 13:
        snprintf(out, size, "Versatile die: d%u", item->versatile_die);
        break;
    case 14:
        snprintf(out, size, "Use versatile: %s", item->use_versatile ? "Yes" : "No");
        break;
    case 15:
        snprintf(out, size, "Type: %s", dnd_rules_core_damage_names[item->damage_type]);
        break;
    case 16:
        snprintf(
            out, size, "Finesse: %s", item->weapon_properties & PocketWeaponFinesse ? "Yes" : "No");
        break;
    case 17:
        snprintf(
            out, size, "Ranged: %s", item->weapon_properties & PocketWeaponRanged ? "Yes" : "No");
        break;
    case 18:
        snprintf(
            out, size, "Light: %s", item->weapon_properties & PocketWeaponLight ? "Yes" : "No");
        break;
    case 19:
        snprintf(
            out, size, "Heavy: %s", item->weapon_properties & PocketWeaponHeavy ? "Yes" : "No");
        break;
    case 20:
        snprintf(
            out, size, "Thrown: %s", item->weapon_properties & PocketWeaponThrown ? "Yes" : "No");
        break;
    case 21:
        snprintf(
            out,
            size,
            "Ammunition: %s",
            item->weapon_properties & PocketWeaponAmmunition ? "Yes" : "No");
        break;
    case 22:
        snprintf(out, size, "Add ability dmg: %s", item->add_ability_damage ? "Yes" : "No");
        break;
    case 23:
        snprintf(out, size, "Extra dice: %u", item->extra_dice);
        break;
    case 24:
        snprintf(out, size, "Extra die: d%u", item->extra_die);
        break;
    case 25:
        snprintf(out, size, "Ammo: %d/%d", item->ammo_current, item->ammo_max);
        break;
    case 26:
        snprintf(out, size, "Maximum ammo: %d", item->ammo_max);
        break;
    case 27:
        snprintf(
            out,
            size,
            "Attack %+d / %ud%u",
            dndinventory_rules_weapon_attack_modifier(c, item),
            item->damage_dice,
            item->use_versatile ? item->versatile_die : item->damage_die);
        break;
    case 28:
        if(item->container_index < 0) {
            dndinventory_collection_copy(out, size, "Container: Carried");
        } else {
            PocketItem* container =
                dndinventory_collection_item_cached(app, (uint8_t)item->container_index);
            if(container && container->name[0])
                snprintf(out, size, "Container: %.21s", container->name);
            else
                snprintf(out, size, "Container: Item %u", (unsigned)item->container_index + 1U);
        }
        break;
    case 29:
        snprintf(out, size, "Charges: %d/%d", item->charges_current, item->charges_max);
        break;
    case 30:
        snprintf(out, size, "Charges max: %d", item->charges_max);
        break;
    case 31:
        snprintf(out, size, "Armor base AC: %u", item->armor_base);
        break;
    case 32:
        snprintf(out, size, "Armor DEX cap: %d", item->armor_dex_cap);
        break;
    case 33:
        snprintf(out, size, "Shield AC bonus: %u", item->shield_bonus);
        break;
    case 34:
        snprintf(out, size, "Ammo group: %.23s", item->ammunition_group);
        break;
    default:
        dndinventory_collection_copy(out, size, "Delete item");
        break;
    }
}

static void dndinventory_collection_draw_list(Canvas* canvas, DndInventoryCollectionApp* app) {
    char title[32];
    snprintf(title, sizeof(title), "Inventory %.7s", app->data.character.name);
    dndinventory_collection_draw_header(canvas, app, title, app->status);
    uint16_t count = (uint16_t)app->total + 1U;
    for(uint8_t row = 0U; row < DNDINVENTORY_COLLECTION_ROWS; ++row) {
        uint16_t index = app->scroll + row;
        if(index >= count) break;
        char text[52];
        if(index == 0U) {
            dndinventory_collection_copy(text, sizeof(text), "+ Add New");
        } else {
            uint8_t logical = (uint8_t)(index - 1U);
            if(logical < app->cache_start ||
               logical >= app->cache_start + POCKET_D20_COLLECTION_CACHE_SIZE) {
                dndinventory_collection_copy(text, sizeof(text), "Page unavailable");
            } else {
                uint8_t local = dndinventory_collection_local(app, logical);
                if(local < app->data.character.item_count) {
                    PocketItem* item = &app->data.character.items[local];
                    snprintf(
                        text,
                        sizeof(text),
                        "%c %dx %.40s",
                        item->equipped ? '*' : ' ',
                        item->quantity,
                        item->name);
                } else {
                    dndinventory_collection_copy(text, sizeof(text), "Read error");
                }
            }
        }
        if(app->action_ack_active && index == app->action_ack_selection) {
            char confirmed[52];
            snprintf(confirmed, sizeof(confirmed), "[X] %.46s", text);
            dndinventory_collection_copy(text, sizeof(text), confirmed);
        }
        dndinventory_collection_draw_row(canvas, row, index == app->selection, text);
    }
}

static void dndinventory_collection_draw_detail(Canvas* canvas, DndInventoryCollectionApp* app) {
    dndinventory_collection_draw_header(canvas, app, "Item Editor", app->status);
    uint8_t count = dndinventory_collection_detail_count();
    for(uint8_t row = 0U; row < DNDINVENTORY_COLLECTION_ROWS; ++row) {
        uint16_t field = app->detail_scroll + row;
        if(field >= count) break;
        char text[64];
        dndinventory_collection_format_detail(app, (uint8_t)field, text, sizeof(text));
        dndinventory_collection_draw_row(canvas, row, field == app->detail_selection, text);
    }
}

static void dndinventory_collection_draw_catalog(Canvas* canvas, DndInventoryCollectionApp* app) {
    char title[48];
    snprintf(
        title,
        sizeof(title),
        "Items: %s",
        dndinventory_collection_item_filter_names[app->item_filter]);
    char page[16];
    snprintf(
        page,
        sizeof(page),
        "Page %u%s <>",
        (unsigned)(app->catalog_page_start / DNDINVENTORY_COLLECTION_CATALOG_PAGE + 1U),
        app->catalog_has_more ? "+" : "");
    dndinventory_collection_draw_header(canvas, app, title, app->status[0] ? app->status : page);
    if(!app->catalog_count) {
        dndinventory_collection_draw_row(canvas, 0U, false, "No matching entries");
        return;
    }
    uint16_t scroll = app->selection > 4U ? app->selection - 4U : 0U;
    for(uint8_t row = 0U; row < DNDINVENTORY_COLLECTION_ROWS; ++row) {
        uint16_t index = scroll + row;
        if(index >= app->catalog_count) break;
        DndInventoryCatalogEntry* entry = &app->catalog[index];
        char text[56];
        if(entry->category != DndInventoryItemCategoryOther) {
            snprintf(
                text,
                sizeof(text),
                "%s%c %.45s",
                dndinventory_collection_item_mark(entry->category),
                entry->magic ? '*' : ' ',
                entry->name);
        } else {
            dndinventory_collection_copy(text, sizeof(text), entry->name);
        }
        dndinventory_collection_draw_row(canvas, row, index == app->selection, text);
    }
}

static void dndinventory_collection_draw(Canvas* canvas, void* model) {
    /* View draw callbacks receive the model buffer, not view context. */
    if(!model) return;
    DndInventoryCollectionApp* app = *(DndInventoryCollectionApp**)model;
    if(!app) return;
    canvas_clear(canvas);
    switch(app->screen) {
    case DndInventoryCollectionScreenNoCharacter:
        dndinventory_collection_draw_header(canvas, app, "DNDInventory", NULL);
        dndinventory_collection_draw_row(canvas, 0U, false, "No character");
        dndinventory_collection_draw_row(canvas, 1U, true, "OK: Open DNDolphins");
        break;
    case DndInventoryCollectionScreenList:
        dndinventory_collection_draw_list(canvas, app);
        break;
    case DndInventoryCollectionScreenDetail:
        dndinventory_collection_draw_detail(canvas, app);
        break;
    case DndInventoryCollectionScreenCatalog:
        dndinventory_collection_draw_catalog(canvas, app);
        break;
    case DndInventoryCollectionScreenInventoryTools:
        dndinventory_collection_draw_inventory_tools(canvas, app);
        break;
    case DndInventoryCollectionScreenCurrency:
        dndinventory_collection_draw_currency(canvas, app);
        break;
    case DndInventoryCollectionScreenResources:
        dndinventory_collection_draw_resources(canvas, app);
        break;
    }
}

static void dndinventory_collection_text_done(void* context);
static void dndinventory_collection_number_done(void* context, int32_t number);

static void dndinventory_collection_begin_text(
    DndInventoryCollectionApp* app,
    DndInventoryCollectionEdit edit,
    const char* header,
    const char* initial) {
    dndinventory_collection_release_number(app);
    if(!app->text_input) {
        app->text_input = text_input_alloc();
        if(!app->text_input) {
            dndinventory_collection_set_status(app, "Text memory low");
            return;
        }
        view_dispatcher_add_view(
            app->dispatcher,
            DNDINVENTORY_COLLECTION_VIEW_TEXT,
            text_input_get_view(app->text_input));
    }
    app->edit = edit;
    dndinventory_collection_copy(app->edit_buffer, sizeof(app->edit_buffer), initial);
    text_input_reset(app->text_input);
    text_input_set_header_text(app->text_input, header);
    text_input_set_result_callback(
        app->text_input,
        dndinventory_collection_text_done,
        app,
        app->edit_buffer,
        sizeof(app->edit_buffer),
        false);
    app->input_active = 1U;
    view_dispatcher_switch_to_view(app->dispatcher, DNDINVENTORY_COLLECTION_VIEW_TEXT);
}

static bool dndinventory_collection_number_spec(
    DndInventoryCollectionApp* app,
    uint8_t field,
    const char** header,
    int32_t* value,
    int32_t* minimum,
    int32_t* maximum) {
    *header = NULL;
    *value = 0;
    *minimum = 0;
    *maximum = 999;
    PocketItem* item = dndinventory_collection_item(app, app->record_index);
    if(!item) return false;
    switch(field) {
    case 2U:
        *header = "Stack quantity";
        *value = item->quantity;
        break;
    case 3U:
        *header = "Weight in tenths lb";
        *value = item->weight_tenths;
        *maximum = 9999;
        break;
    case 9U:
        *header = "Magic bonus";
        *value = item->magic_bonus;
        *minimum = -10;
        *maximum = 10;
        break;
    case 10U:
        *header = "Damage dice count";
        *value = item->damage_dice;
        *maximum = 20;
        break;
    case 11U:
        *header = "Damage die sides";
        *value = item->damage_die;
        *minimum = 4;
        *maximum = 12;
        break;
    case 13U:
        *header = "Versatile die sides";
        *value = item->versatile_die;
        *minimum = 4;
        *maximum = 12;
        break;
    case 23U:
        *header = "Extra dice count";
        *value = item->extra_dice;
        *maximum = 20;
        break;
    case 24U:
        *header = "Extra die sides";
        *value = item->extra_die;
        *minimum = 4;
        *maximum = 12;
        break;
    case 25U:
        *header = "Ammo current";
        *value = item->ammo_current;
        *maximum = item->ammo_max;
        break;
    case 26U:
        *header = "Ammo maximum";
        *value = item->ammo_max;
        break;
    case 29U:
        *header = "Charges current";
        *value = item->charges_current;
        *maximum = item->charges_max;
        break;
    case 30U:
        *header = "Charges maximum";
        *value = item->charges_max;
        break;
    case 31U:
        *header = "Armor base";
        *value = item->armor_base;
        *maximum = 30;
        break;
    case 32U:
        *header = "Armor DEX cap";
        *value = item->armor_dex_cap;
        *minimum = -1;
        *maximum = 9;
        break;
    case 33U:
        *header = "Shield bonus";
        *value = item->shield_bonus;
        *maximum = 10;
        break;
    default:
        return false;
    }
    return true;
}

static bool dndinventory_collection_begin_number(DndInventoryCollectionApp* app, uint8_t field) {
    const char* header = NULL;
    int32_t value = 0;
    int32_t minimum = 0;
    int32_t maximum = 0;
    if(!dndinventory_collection_number_spec(app, field, &header, &value, &minimum, &maximum))
        return false;
    dndinventory_collection_release_text(app);
    if(!app->number_input) {
        app->number_input = number_input_alloc();
        if(!app->number_input) {
            dndinventory_collection_set_status(app, "Number memory low");
            return true;
        }
        view_dispatcher_add_view(
            app->dispatcher,
            DNDINVENTORY_COLLECTION_VIEW_NUMBER,
            number_input_get_view(app->number_input));
    }
    app->number_field = field;
    number_input_set_header_text(app->number_input, header);
    number_input_set_result_callback(
        app->number_input, dndinventory_collection_number_done, app, value, minimum, maximum);
    app->input_active = 1U;
    view_dispatcher_switch_to_view(app->dispatcher, DNDINVENTORY_COLLECTION_VIEW_NUMBER);
    return true;
}

static bool dndinventory_collection_begin_currency_number(
    DndInventoryCollectionApp* app,
    uint8_t currency_index) {
    static const char* const names[] = {
        "Copper pieces", "Silver pieces", "Electrum pieces", "Gold pieces", "Platinum pieces"};
    int32_t* values[] = {
        &app->data.character.currency_cp,
        &app->data.character.currency_sp,
        &app->data.character.currency_ep,
        &app->data.character.currency_gp,
        &app->data.character.currency_pp,
    };
    if(currency_index >= 5U) return false;
    dndinventory_collection_release_text(app);
    if(!app->number_input) {
        app->number_input = number_input_alloc();
        if(!app->number_input) {
            dndinventory_collection_set_status(app, "Number memory low");
            return false;
        }
        view_dispatcher_add_view(
            app->dispatcher,
            DNDINVENTORY_COLLECTION_VIEW_NUMBER,
            number_input_get_view(app->number_input));
    }
    app->number_field = (uint8_t)(0x80U | currency_index);
    number_input_set_header_text(app->number_input, names[currency_index]);
    number_input_set_result_callback(
        app->number_input,
        dndinventory_collection_number_done,
        app,
        *values[currency_index],
        0,
        999999999);
    app->input_active = 1U;
    view_dispatcher_switch_to_view(app->dispatcher, DNDINVENTORY_COLLECTION_VIEW_NUMBER);
    return true;
}

static void
    dndinventory_collection_adjust(DndInventoryCollectionApp* app, uint8_t field, int8_t delta) {
    PocketItem* item = dndinventory_collection_item(app, app->record_index);
    if(!item) return;
    switch(field) {
    case 2:
        item->quantity =
            dndinventory_collection_clamp_i16((int32_t)item->quantity + delta, 0, 999);
        break;
    case 3:
        item->weight_tenths =
            dndinventory_collection_clamp_i16((int32_t)item->weight_tenths + delta, 0, 9999);
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
        int16_t next = (int16_t)item->attack_ability + delta;
        if(next < 0) next = (int16_t)PocketAttackAbilityBest;
        if(next > (int16_t)PocketAttackAbilityBest) next = 0;
        item->attack_ability = (uint8_t)next;
        break;
    }
    case 8:
        item->proficient = !item->proficient;
        break;
    case 9:
        item->magic_bonus =
            (int8_t)dndinventory_collection_clamp_i16((int32_t)item->magic_bonus + delta, -10, 10);
        break;
    case 10:
        item->damage_dice =
            dndinventory_collection_clamp_u8((int16_t)item->damage_dice + delta, 20U);
        break;
    case 11:
        item->damage_die = dndinventory_collection_cycle_die(item->damage_die, delta);
        break;
    case 12:
        item->versatile_die = item->versatile_die ? 0U : 8U;
        break;
    case 13:
        item->versatile_die = dndinventory_collection_cycle_die(
            item->versatile_die ? item->versatile_die : 8U, delta);
        break;
    case 14:
        item->use_versatile = !item->use_versatile;
        break;
    case 15: {
        int16_t next = (int16_t)item->damage_type + delta;
        if(next < 0) next = PocketDamageTypeCount - 1U;
        if(next >= PocketDamageTypeCount) next = 0;
        item->damage_type = (uint8_t)next;
        break;
    }
    case 16:
        item->weapon_properties ^= PocketWeaponFinesse;
        break;
    case 17:
        item->weapon_properties ^= PocketWeaponRanged;
        break;
    case 18:
        item->weapon_properties ^= PocketWeaponLight;
        break;
    case 19:
        item->weapon_properties ^= PocketWeaponHeavy;
        break;
    case 20:
        item->weapon_properties ^= PocketWeaponThrown;
        break;
    case 21:
        item->weapon_properties ^= PocketWeaponAmmunition;
        break;
    case 22:
        item->add_ability_damage = !item->add_ability_damage;
        break;
    case 23:
        item->extra_dice =
            dndinventory_collection_clamp_u8((int16_t)item->extra_dice + delta, 20U);
        break;
    case 24:
        item->extra_die = dndinventory_collection_cycle_die(item->extra_die, delta);
        break;
    case 25:
        item->ammo_current = dndinventory_collection_clamp_i16(
            (int32_t)item->ammo_current + delta, 0, item->ammo_max);
        break;
    case 26:
        item->ammo_max =
            dndinventory_collection_clamp_i16((int32_t)item->ammo_max + delta, 0, 999);
        if(item->ammo_current > item->ammo_max) item->ammo_current = item->ammo_max;
        break;
    case 28: {
        int16_t next = (int16_t)item->container_index + delta;
        if(next < -1) next = (int16_t)app->total - 1;
        if(next >= (int16_t)app->total) next = -1;
        if(next == (int16_t)app->record_index) {
            next += delta;
            if(next < -1) next = (int16_t)app->total - 1;
            if(next >= (int16_t)app->total) next = -1;
        }
        item->container_index = (int8_t)next;
        break;
    }
    case 29:
        item->charges_current = dndinventory_collection_clamp_i16(
            (int32_t)item->charges_current + delta, 0, item->charges_max);
        break;
    case 30:
        item->charges_max =
            dndinventory_collection_clamp_i16((int32_t)item->charges_max + delta, 0, 999);
        if(item->charges_current > item->charges_max) item->charges_current = item->charges_max;
        break;
    case 31:
        item->armor_base =
            dndinventory_collection_clamp_u8((int16_t)item->armor_base + delta, 30U);
        break;
    case 32:
        item->armor_dex_cap =
            (int8_t)dndinventory_collection_clamp_i16((int32_t)item->armor_dex_cap + delta, -1, 9);
        break;
    case 33:
        item->shield_bonus =
            dndinventory_collection_clamp_u8((int16_t)item->shield_bonus + delta, 10U);
        break;
    default:
        return;
    }
    (void)dndinventory_collection_save_page(app);
}

static void dndinventory_collection_text_done(void* context) {
    DndInventoryCollectionApp* app = context;
    if(!app) return;
    PocketItem* item = dndinventory_collection_item(app, app->record_index);
    if(item) {
        if(app->edit == DndInventoryCollectionEditName)
            dndinventory_collection_copy(item->name, sizeof(item->name), app->edit_buffer);
        else if(app->edit == DndInventoryCollectionEditDetail)
            dndinventory_collection_copy(item->detail, sizeof(item->detail), app->edit_buffer);
        else if(app->edit == DndInventoryCollectionEditAmmoGroup)
            dndinventory_collection_copy(
                item->ammunition_group, sizeof(item->ammunition_group), app->edit_buffer);
    }
    app->input_active = 0U;
    app->edit = DndInventoryCollectionEditNone;
    (void)dndinventory_collection_save_page(app);
    view_dispatcher_switch_to_view(app->dispatcher, DNDINVENTORY_COLLECTION_VIEW_MAIN);
    dndinventory_collection_redraw(app);
}

static void dndinventory_collection_number_done(void* context, int32_t number) {
    DndInventoryCollectionApp* app = context;
    if(!app) return;
    uint8_t field = app->number_field;
    if(field & 0x80U) {
        uint8_t currency_index = (uint8_t)(field & 0x7FU);
        int32_t* values[] = {
            &app->data.character.currency_cp,
            &app->data.character.currency_sp,
            &app->data.character.currency_ep,
            &app->data.character.currency_gp,
            &app->data.character.currency_pp,
        };
        if(currency_index < 5U) *values[currency_index] = number;
        app->input_active = 0U;
        (void)dndinventory_collection_save_currency(app);
        view_dispatcher_switch_to_view(app->dispatcher, DNDINVENTORY_COLLECTION_VIEW_MAIN);
        dndinventory_collection_redraw(app);
        return;
    }
    PocketItem* item = dndinventory_collection_item(app, app->record_index);
    if(item) {
        switch(field) {
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
            item->damage_die = (uint8_t)number;
            break;
        case 13U:
            item->versatile_die = (uint8_t)number;
            break;
        case 23U:
            item->extra_dice = (uint8_t)number;
            break;
        case 24U:
            item->extra_die = (uint8_t)number;
            break;
        case 25U:
            item->ammo_current = (int16_t)number;
            break;
        case 26U:
            item->ammo_max = (int16_t)number;
            if(item->ammo_current > item->ammo_max) item->ammo_current = item->ammo_max;
            break;
        case 29U:
            item->charges_current = (int16_t)number;
            break;
        case 30U:
            item->charges_max = (int16_t)number;
            if(item->charges_current > item->charges_max)
                item->charges_current = item->charges_max;
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
        default:
            break;
        }
    }
    app->input_active = 0U;
    (void)dndinventory_collection_save_page(app);
    view_dispatcher_switch_to_view(app->dispatcher, DNDINVENTORY_COLLECTION_VIEW_MAIN);
    dndinventory_collection_redraw(app);
}

static void dndinventory_collection_detail_ok(DndInventoryCollectionApp* app) {
    uint8_t field = app->detail_selection;
    PocketItem* item = dndinventory_collection_item(app, app->record_index);
    if(!item) return;
    if(field == 0U)
        dndinventory_collection_open_catalog(app);
    else if(field == 1U)
        dndinventory_collection_begin_text(
            app, DndInventoryCollectionEditDetail, "Item notes", item->detail);
    else if(field == 2U)
        (void)dndinventory_collection_begin_number(app, field);
    else if((field >= 3U && field <= 26U) || (field >= 28U && field <= 33U))
        dndinventory_collection_adjust(app, field, 1);
    else if(field == 34U)
        dndinventory_collection_begin_text(
            app, DndInventoryCollectionEditAmmoGroup, "Ammunition group", item->ammunition_group);
    else if(field == 35U)
        (void)dndinventory_collection_delete_current(app);
}

static void dndinventory_collection_detail_hold_ok(DndInventoryCollectionApp* app) {
    if(dndinventory_collection_begin_number(app, app->detail_selection)) return;
    if(app->detail_selection != 0U) return;
    PocketItem* item = dndinventory_collection_item(app, app->record_index);
    if(item)
        dndinventory_collection_begin_text(
            app, DndInventoryCollectionEditName, "Custom item", item->name);
}

static bool dndinventory_collection_input(InputEvent* event, void* context) {
    DndInventoryCollectionApp* app = context;
    if(!app || !event) return false;
    bool move = event->type == InputTypeShort || event->type == InputTypeRepeat;
    if(event->type != InputTypeShort && event->type != InputTypeRepeat &&
       event->type != InputTypeLong)
        return true;

    if(app->status_transient) {
        app->status[0] = '\0';
        app->status_transient = 0U;
    }
    app->action_ack_active = 0U;

    if(event->key == InputKeyBack && event->type == InputTypeLong) {
        app->return_to_dnd = 0U;
        view_dispatcher_stop(app->dispatcher);
        return true;
    }

    if(event->key == InputKeyBack && event->type == InputTypeShort) {
        if(app->screen == DndInventoryCollectionScreenNoCharacter ||
           app->screen == DndInventoryCollectionScreenList) {
            app->return_to_dnd = 1U;
            view_dispatcher_stop(app->dispatcher);
            return true;
        } else if(app->screen == DndInventoryCollectionScreenDetail) {
            app->screen = DndInventoryCollectionScreenList;
            dndinventory_collection_focus_list(app, app->record_index);
        } else if(app->screen == DndInventoryCollectionScreenCatalog) {
            app->screen = DndInventoryCollectionScreenDetail;
        } else if(app->screen == DndInventoryCollectionScreenInventoryTools) {
            app->screen = DndInventoryCollectionScreenList;
        } else if(
            app->screen == DndInventoryCollectionScreenCurrency ||
            app->screen == DndInventoryCollectionScreenResources) {
            app->screen = DndInventoryCollectionScreenInventoryTools;
            app->tool_selection = 0U;
            app->tool_scroll = 0U;
        }
        dndinventory_collection_redraw(app);
        return true;
    }

    if(app->screen == DndInventoryCollectionScreenNoCharacter) {
        if(event->key == InputKeyOk && event->type == InputTypeShort) {
            app->return_to_dnd = 1U;
            view_dispatcher_stop(app->dispatcher);
        }
        return true;
    }

    if(app->screen == DndInventoryCollectionScreenList) {
        if(event->type == InputTypeLong && event->key == InputKeyUp) {
            dndinventory_collection_refresh_grant_state(app);
            app->screen = DndInventoryCollectionScreenInventoryTools;
            app->tool_selection = 0U;
            app->tool_scroll = 0U;
            app->status[0] = '\0';
        } else if(move && event->key == InputKeyUp) {
            (void)dndinventory_collection_move_list(app, -1);
        } else if(move && event->key == InputKeyDown) {
            (void)dndinventory_collection_move_list(app, 1);
        } else if(
            event->type == InputTypeLong && app->selection &&
            (event->key == InputKeyLeft || event->key == InputKeyRight)) {
            uint8_t logical = (uint8_t)(app->selection - 1U);
            PocketItem* item = dndinventory_collection_item(app, logical);
            if(item) {
                item->quantity = dndinventory_collection_clamp_i16(
                    (int32_t)item->quantity + (event->key == InputKeyRight ? 1 : -1), 0, 999);
                if(dndinventory_collection_save_page(app))
                    dndinventory_collection_set_transient_status(app, "Stack quantity saved");
            }
        } else if(event->type == InputTypeShort && event->key == InputKeyLeft) {
            (void)dndinventory_collection_page_list(app, -1);
        } else if(event->type == InputTypeShort && event->key == InputKeyRight) {
            (void)dndinventory_collection_page_list(app, 1);
        } else if(
            event->key == InputKeyOk &&
            (event->type == InputTypeShort || event->type == InputTypeLong) &&
            app->selection == 0U) {
            (void)dndinventory_collection_add_blank(app);
        } else if(event->key == InputKeyOk && event->type == InputTypeLong && app->selection) {
            uint8_t logical = (uint8_t)(app->selection - 1U);
            PocketItem* item = dndinventory_collection_item(app, logical);
            if(item) {
                item->equipped = !item->equipped;
                bool saved = dndinventory_collection_save_page(app);
                if(saved) {
                    app->action_ack_active = 1U;
                    app->action_ack_selection = app->selection;
                    dndinventory_collection_set_transient_status(
                        app, item->equipped ? "Item equipped" : "Item unequipped");
                }
            }
        } else if(event->key == InputKeyOk && event->type == InputTypeShort && app->selection) {
            app->record_index = (uint8_t)(app->selection - 1U);
            if(dndinventory_collection_prepare_record(app, app->record_index)) {
                app->detail_selection = app->detail_scroll = 0U;
                app->screen = DndInventoryCollectionScreenDetail;
            }
        }
    } else if(app->screen == DndInventoryCollectionScreenInventoryTools) {
        if(event->type == InputTypeShort && event->key == InputKeyUp)
            app->tool_selection = app->tool_selection ? app->tool_selection - 1U : 2U;
        else if(event->type == InputTypeShort && event->key == InputKeyDown)
            app->tool_selection = app->tool_selection < 2U ? app->tool_selection + 1U : 0U;
        else if(event->key == InputKeyOk && event->type == InputTypeLong && app->tool_selection == 2U) {
            (void)dndinventory_collection_regrant_initial_inventory(app);
        } else if(event->key == InputKeyOk && event->type == InputTypeShort) {
            if(app->tool_selection == 0U) {
                app->screen = DndInventoryCollectionScreenCurrency;
                app->tool_selection = app->tool_scroll = 0U;
                app->status[0] = '\0';
            } else if(app->tool_selection == 1U) {
                app->screen = DndInventoryCollectionScreenResources;
                app->tool_selection = app->tool_scroll = 0U;
                if(!dndinventory_collection_refresh_item_aggregate(app))
                    dndinventory_collection_set_status(app, "Inventory read failed");
                else
                    app->status[0] = '\0';
            } else {
                (void)dndinventory_collection_grant_initial_inventory(app);
            }
        }
    } else if(app->screen == DndInventoryCollectionScreenCurrency) {
        int32_t* values[] = {
            &app->data.character.currency_cp,
            &app->data.character.currency_sp,
            &app->data.character.currency_ep,
            &app->data.character.currency_gp,
            &app->data.character.currency_pp,
        };
        if(move && event->key == InputKeyUp)
            app->tool_selection = app->tool_selection ? app->tool_selection - 1U : 4U;
        else if(move && event->key == InputKeyDown)
            app->tool_selection = app->tool_selection < 4U ? app->tool_selection + 1U : 0U;
        else if(move && (event->key == InputKeyLeft || event->key == InputKeyRight)) {
            int64_t next =
                (int64_t)*values[app->tool_selection] + (event->key == InputKeyRight ? 1 : -1);
            if(next < 0) next = 0;
            if(next > 999999999L) next = 999999999L;
            *values[app->tool_selection] = (int32_t)next;
            (void)dndinventory_collection_save_currency(app);
        } else if(event->key == InputKeyOk && event->type == InputTypeShort) {
            (void)dndinventory_collection_begin_currency_number(app, app->tool_selection);
        }
    } else if(app->screen == DndInventoryCollectionScreenResources) {
        DndInventoryCharacterState* c = &app->data.character;
        if(move && event->key == InputKeyUp)
            app->tool_selection = app->tool_selection ? app->tool_selection - 1U : 8U;
        else if(move && event->key == InputKeyDown)
            app->tool_selection = app->tool_selection < 8U ? app->tool_selection + 1U : 0U;
        else if(move && (event->key == InputKeyLeft || event->key == InputKeyRight)) {
            int16_t delta = event->key == InputKeyRight ? 1 : -1;
            if(app->tool_selection == 3U) {
                c->encumbrance_mode = !c->encumbrance_mode;
                (void)dndinventory_collection_save_character(app);
            } else if(app->tool_selection == 8U) {
                c->carrying_capacity_override = dndinventory_collection_clamp_i16(
                    (int32_t)c->carrying_capacity_override + delta, 0, 999);
                (void)dndinventory_collection_save_character(app);
            }
        } else if(event->key == InputKeyOk && event->type == InputTypeShort) {
            if(app->tool_selection == 6U) {
                if(!app->item_aggregate_valid &&
                   !dndinventory_collection_refresh_item_aggregate(app)) {
                    dndinventory_collection_set_status(app, "Inventory read failed");
                } else {
                    c->armor_class =
                        dndinventory_rules_calculated_armor_class(c, &app->item_aggregate);
                    if(dndinventory_collection_save_character(app))
                        dndinventory_collection_set_status(app, "Armor Class applied");
                }
            } else if(app->tool_selection == 7U) {
                dndinventory_rules_normalize_currency(c);
                if(dndinventory_collection_save_currency(app))
                    dndinventory_collection_set_status(app, "Coins normalized");
            } else if(app->tool_selection == 3U) {
                c->encumbrance_mode = !c->encumbrance_mode;
                (void)dndinventory_collection_save_character(app);
            }
        }
        if(app->tool_selection < app->tool_scroll) app->tool_scroll = app->tool_selection;
        if(app->tool_selection >= app->tool_scroll + DNDINVENTORY_COLLECTION_ROWS)
            app->tool_scroll =
                (uint8_t)(app->tool_selection - (DNDINVENTORY_COLLECTION_ROWS - 1U));
    } else if(app->screen == DndInventoryCollectionScreenDetail) {
        uint8_t count = dndinventory_collection_detail_count();
        if(move && event->key == InputKeyUp)
            app->detail_selection = app->detail_selection ? app->detail_selection - 1U :
                                                            count - 1U;
        else if(move && event->key == InputKeyDown)
            app->detail_selection =
                app->detail_selection + 1U < count ? app->detail_selection + 1U : 0U;
        else if(move && (event->key == InputKeyLeft || event->key == InputKeyRight))
            dndinventory_collection_adjust(
                app, app->detail_selection, event->key == InputKeyRight ? 1 : -1);
        else if(event->key == InputKeyOk && event->type == InputTypeLong)
            dndinventory_collection_detail_hold_ok(app);
        else if(event->key == InputKeyOk && event->type == InputTypeShort)
            dndinventory_collection_detail_ok(app);
        if(app->detail_selection < app->detail_scroll) app->detail_scroll = app->detail_selection;
        if(app->detail_selection >= app->detail_scroll + DNDINVENTORY_COLLECTION_ROWS)
            app->detail_scroll = app->detail_selection - (DNDINVENTORY_COLLECTION_ROWS - 1U);
    } else if(app->screen == DndInventoryCollectionScreenCatalog) {
        if(move && event->key == InputKeyUp && app->catalog_count)
            app->selection = app->selection ? app->selection - 1U : app->catalog_count - 1U;
        else if(move && event->key == InputKeyDown && app->catalog_count)
            app->selection = app->selection + 1U < app->catalog_count ? app->selection + 1U : 0U;
        else if(move && event->key == InputKeyLeft && app->catalog_page_start) {
            app->catalog_page_start =
                app->catalog_page_start >= DNDINVENTORY_COLLECTION_CATALOG_PAGE ?
                    app->catalog_page_start - DNDINVENTORY_COLLECTION_CATALOG_PAGE :
                    0U;
            app->selection = 0U;
            (void)dndinventory_collection_load_catalog(app);
        } else if(move && event->key == InputKeyRight && app->catalog_has_more) {
            app->catalog_page_start += DNDINVENTORY_COLLECTION_CATALOG_PAGE;
            app->selection = 0U;
            (void)dndinventory_collection_load_catalog(app);
        } else if(event->key == InputKeyOk && event->type == InputTypeLong) {
            app->item_filter = (uint8_t)((app->item_filter + 1U) % DndInventoryItemFilterCount);
            dndinventory_collection_reset_catalog_offsets(app);
            app->catalog_page_start = 0U;
            app->selection = 0U;
            (void)dndinventory_collection_load_catalog(app);
            dndinventory_collection_set_status(
                app, dndinventory_collection_item_filter_names[app->item_filter]);
        } else if(event->key == InputKeyOk && event->type == InputTypeShort && app->catalog_count) {
            (void)dndinventory_collection_apply_catalog(app);
        }
    }
    dndinventory_collection_redraw(app);
    return true;
}

static DndInventoryCollectionApp* dndinventory_collection_alloc(const char* args) {
    DndInventoryCollectionApp* app = calloc(1U, sizeof(DndInventoryCollectionApp));
    if(!app) return NULL;
    app->gui = furi_record_open(RECORD_GUI);
    app->storage = furi_record_open(RECORD_STORAGE);
    if(!app->gui || !app->storage) goto fail;

    /* Reserve the complete fixed UI/runtime footprint before character and item
       parsing can make variable heap allocations. This mirrors Adventure's startup
       ordering so a successful collection read cannot consume memory required for
       the main view/model. */
    app->dispatcher = view_dispatcher_alloc();
    app->view = view_alloc();
    if(!app->dispatcher || !app->view) goto fail;
    view_dispatcher_set_event_callback_context(app->dispatcher, app);
    view_dispatcher_set_navigation_event_callback(
        app->dispatcher, dndinventory_collection_navigation);
    view_allocate_model(app->view, ViewModelTypeLockFree, sizeof(DndInventoryCollectionApp*));
    DndInventoryCollectionApp** model = view_get_model(app->view);
    if(!model) goto fail;
    *model = app;
    view_commit_model(app->view, false);
    view_set_context(app->view, app);
    view_set_draw_callback(app->view, dndinventory_collection_draw);
    view_set_input_callback(app->view, dndinventory_collection_input);

    app->have_profile = dndinventory_collection_load_profile(app, args) ? 1U : 0U;
    if(app->have_profile) {
        if(!dndinventory_collection_load_currency(app))
            dndinventory_collection_set_status(app, "Currency read failed");
        if(!app->status[0]) dndinventory_collection_set_status(app, "Hold Up: inventory tools");
    }
    if(app->have_profile) {
        /* A missing Inventory sidecar is a valid empty inventory. Always enter
           directly on the list with + Add New selected. Hold Up from this list
           opens Inventory Tools; ordinary Up/Down remains list navigation. */
        if(!dndinventory_collection_load_page(app, 0U))
            dndinventory_collection_set_status(app, "Collection read failed");
        app->selection = 0U;
        app->scroll = 0U;
        app->screen = DndInventoryCollectionScreenList;
    } else {
        app->screen = DndInventoryCollectionScreenNoCharacter;
    }
    view_dispatcher_add_view(app->dispatcher, DNDINVENTORY_COLLECTION_VIEW_MAIN, app->view);
    view_dispatcher_attach_to_gui(app->dispatcher, app->gui, ViewDispatcherTypeFullscreen);
    return app;

fail:
    if(app->text_input) text_input_free(app->text_input);
    if(app->number_input) number_input_free(app->number_input);
    if(app->view) view_free(app->view);
    if(app->dispatcher) view_dispatcher_free(app->dispatcher);
    free(app->data.character.items);
    app->data.character.items = NULL;
    if(app->storage) furi_record_close(RECORD_STORAGE);
    if(app->gui) furi_record_close(RECORD_GUI);
    free(app);
    return NULL;
}

static void dndinventory_collection_free(DndInventoryCollectionApp* app) {
    if(!app) return;
    if(app->dispatcher && app->text_input)
        view_dispatcher_remove_view(app->dispatcher, DNDINVENTORY_COLLECTION_VIEW_TEXT);
    if(app->dispatcher && app->number_input)
        view_dispatcher_remove_view(app->dispatcher, DNDINVENTORY_COLLECTION_VIEW_NUMBER);
    if(app->dispatcher && app->view)
        view_dispatcher_remove_view(app->dispatcher, DNDINVENTORY_COLLECTION_VIEW_MAIN);
    if(app->text_input) text_input_free(app->text_input);
    if(app->number_input) number_input_free(app->number_input);
    if(app->view) view_free(app->view);
    if(app->dispatcher) view_dispatcher_free(app->dispatcher);
    free(app->data.character.items);
    app->data.character.items = NULL;
    if(app->storage) furi_record_close(RECORD_STORAGE);
    if(app->gui) furi_record_close(RECORD_GUI);
    free(app);
}

int32_t dndinventory_collection_run(void* context) {
    DndInventoryCollectionApp* app = dndinventory_collection_alloc(context);
    if(!app) return -1;
    view_dispatcher_switch_to_view(app->dispatcher, DNDINVENTORY_COLLECTION_VIEW_MAIN);
    view_dispatcher_run(app->dispatcher);
    bool return_to_dnd = app->return_to_dnd;
    dndinventory_collection_free(app);
    if(return_to_dnd)
        (void)dnd_handoff_launch_if_present(
            DNDOLPHINS_FAP_PATH, POCKET_D20_RETURN_FOCUS_INVENTORY);
    return 0;
}
