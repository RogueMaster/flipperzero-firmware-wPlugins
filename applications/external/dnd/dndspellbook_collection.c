#include "dndspellbook_collection.h"

#include "dnd_profile_handoff.h"
#include "dnd_profile_projection.h"
#include "dnd_fs.h"
#include "dnd_data.h"
#include "dnd_spell_eligibility.h"
#include "dnd_storage.h"
#include "dnd_settings.h"
#include "dndolphins_spells.h"

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

#define TAG                                          "DndSpellbook"
#define DNDSPELLBOOK_COLLECTION_VIEW_MAIN            0U
#define DNDSPELLBOOK_COLLECTION_VIEW_TEXT            1U
#define DNDSPELLBOOK_COLLECTION_VIEW_NUMBER          2U
#define DNDSPELLBOOK_COLLECTION_ROWS                 5U
#define DNDSPELLBOOK_COLLECTION_SEARCH_MIN_CHARS     3U
#define DNDSPELLBOOK_COLLECTION_CATALOG_PAGE         10U
#define DNDSPELLBOOK_COLLECTION_CATALOG_OFFSET_PAGES 64U
#define DNDSPELLBOOK_COLLECTION_LINE_MAX             256U
#define DNDSPELLBOOK_COLLECTION_CATALOG_READ_BUFFER  128U
#define DNDSPELLBOOK_COLLECTION_SPELL_CATALOG        "/ext/apps_assets/dndolphins/catalogs/spells.txt"
#define DNDSPELLBOOK_COLLECTION_SPELL_CATALOG_ALL \
    "/ext/apps_assets/dndolphins/catalogs/spells_All.txt"
#define DNDSPELLBOOK_COLLECTION_SPELL_CATALOG_LEGACY     APP_ASSETS_PATH("catalogs/spells.txt")
#define DNDSPELLBOOK_COLLECTION_SPELL_CATALOG_ALL_LEGACY APP_ASSETS_PATH("catalogs/spells_All.txt")
#define DNDSPELLBOOK_COLLECTION_CATALOG_SOURCE_ALL       0x80000000UL
#define DNDSPELLBOOK_COLLECTION_CATALOG_OFFSET_MASK      0x7FFFFFFFUL
#define DNDSPELLBOOK_COLLECTION_SORT_LINE_MAX            1280U
#define DNDSPELLBOOK_COLLECTION_SORT_COPY_BUFFER         256U

typedef enum {
    DndSpellbookCollectionScreenNoCharacter,
    DndSpellbookCollectionScreenList,
    DndSpellbookCollectionScreenSearch,
    DndSpellbookCollectionScreenDetail,
    DndSpellbookCollectionScreenCatalog,
    DndSpellbookCollectionScreenFilters,
    DndSpellbookCollectionScreenMagic,
} DndSpellbookCollectionScreen;

typedef enum {
    DndSpellbookCollectionEditNone,
    DndSpellbookCollectionEditName,
    DndSpellbookCollectionEditDetail,
    DndSpellbookCollectionEditStableId,
    DndSpellbookCollectionEditSource,
    DndSpellbookCollectionEditSchool,
    DndSpellbookCollectionEditGrantName,
    DndSpellbookCollectionEditSearch,
} DndSpellbookCollectionEdit;

typedef enum {
    DndSpellbookSourceAny,
    DndSpellbookSourceCore,
    DndSpellbookSourceXanathar,
    DndSpellbookSourceForgottenRealms,
    DndSpellbookSourceRavenloft,
    DndSpellbookSourceOther,
    DndSpellbookSourceCount,
} DndSpellbookSource;

enum {
    DndSpellbookClassMaskArtificer = 1U << 0,
    DndSpellbookClassMaskBarbarian = 1U << 1,
    DndSpellbookClassMaskBard = 1U << 2,
    DndSpellbookClassMaskCleric = 1U << 3,
    DndSpellbookClassMaskDruid = 1U << 4,
    DndSpellbookClassMaskFighter = 1U << 5,
    DndSpellbookClassMaskMonk = 1U << 6,
    DndSpellbookClassMaskPaladin = 1U << 7,
    DndSpellbookClassMaskRanger = 1U << 8,
    DndSpellbookClassMaskRogue = 1U << 9,
    DndSpellbookClassMaskSorcerer = 1U << 10,
    DndSpellbookClassMaskWarlock = 1U << 11,
    DndSpellbookClassMaskWizard = 1U << 12,
};

typedef struct {
    char name[DND_CATALOG_NAME_LEN];
    uint8_t level;
    uint16_t class_mask;
    uint8_t school;
    uint8_t source;
    uint8_t ritual;
    uint16_t absolute_index;
} DndSpellbookCatalogEntry;

static const char* const dndspellbook_collection_school_names[] = {
    "Any",
    "Abjuration",
    "Conjuration",
    "Divination",
    "Enchantment",
    "Evocation",
    "Illusion",
    "Necromancy",
    "Transmutation"};
static const char* const dndspellbook_collection_source_names[] =
    {"Any", "Core", "Xanathar", "Forgotten Realms", "Ravenloft", "Other"};
static const char* const dndspellbook_collection_class_names[] = {
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
#define DNDSPELLBOOK_COLLECTION_CLASS_COUNT              13U
#define DNDSPELLBOOK_COLLECTION_FILTER_CHARACTER_CLASSES UINT8_MAX
#define DNDSPELLBOOK_COLLECTION_FILTER_ANY_CLASS         13U

typedef struct {
    char name[DND_CHARACTER_NAME_LEN];
    uint8_t class_count;
    DndClassLevel classes[DND_MAX_CLASSES];
    uint8_t spell_count;
    uint8_t spell_capacity;
    void* spell_storage;
    DndSpell* spells;
    uint8_t* spell_known;
    uint8_t* spell_always_prepared;
    uint8_t* spell_free_casts_current;
    uint8_t* spell_free_casts_max;
    int8_t ability_scores[DND_ABILITY_COUNT];
    uint8_t spellcasting_ability;
    int8_t spell_attack_misc;
    int8_t spell_save_misc;
    uint8_t arcane_recovery_used;
    uint8_t spell_slots_current[DND_SLOT_COUNT];
    uint8_t spell_slots_max[DND_SLOT_COUNT];
} DndSpellbookCharacterState;

typedef struct {
    DndSpellbookCharacterState character;
} DndSpellbookAppData;

typedef struct {
    Gui* gui;
    Storage* storage;
    DndSettings settings;
    ViewDispatcher* dispatcher;
    View* view;
    TextInput* text_input;
    NumberInput* number_input;
    DndSpellbookAppData data;
    DndSpellbookCollectionScreen screen;
    DndSpellbookCollectionEdit edit;
    uint32_t profile;
    uint8_t have_profile;
    uint8_t return_to_dnd;
    uint16_t total;
    uint16_t cache_start;
    uint32_t record_page_offsets[DND_STORAGE_COLLECTION_PAGE_COUNT];
    uint8_t record_offset_valid_pages;
    uint16_t selection;
    uint16_t scroll;
    uint16_t record_index;
    uint8_t detail_selection;
    uint16_t detail_scroll;
    char edit_buffer[DND_DETAIL_LEN];
    uint8_t number_field;
    uint8_t input_active;
    char status[32];
    uint8_t status_transient;
    uint8_t action_ack_active;
    uint16_t action_ack_selection;
    uint8_t sort_pending;
    char search_term[DND_NAME_LEN];
    uint16_t* search_matches;
    uint16_t search_match_count;
    uint16_t search_selection;
    uint16_t search_scroll;
    uint8_t detail_return_search;
    DndSpellbookCatalogEntry catalog[DNDSPELLBOOK_COLLECTION_CATALOG_PAGE];
    uint8_t catalog_count;
    uint16_t catalog_page_start;
    uint16_t catalog_total;
    uint8_t catalog_has_more;
    uint32_t catalog_page_offsets[DNDSPELLBOOK_COLLECTION_CATALOG_OFFSET_PAGES];
    uint8_t catalog_offset_valid_pages;
    int8_t filter_level;
    uint8_t filter_class;
    uint8_t filter_ritual;
    uint8_t filter_school;
    uint8_t filter_source;
    uint8_t filter_status;
    uint8_t filter_show_all;
    uint8_t filter_selection;
    uint8_t catalog_all_available;
    uint8_t canonical_catalog_available;
    uint8_t status_prefilter[128];
    uint8_t status_prefilter_filter;
    uint8_t status_prefilter_valid;
    DndSpellbookCollectionScreen filter_return_screen;
    uint16_t magic_known;
    uint16_t magic_knowable;
    uint16_t magic_granted;
    uint8_t magic_counts_valid;
    uint8_t magic_direct_launch;
    uint16_t magic_return_selection;
    uint16_t magic_return_scroll;
} DndSpellbookCollectionApp;

static bool dndspellbook_collection_load_page(DndSpellbookCollectionApp* app, uint16_t start);
static bool dndspellbook_collection_save_page(DndSpellbookCollectionApp* app);
static bool
    dndspellbook_collection_prepare_record(DndSpellbookCollectionApp* app, uint16_t logical);
static DndSpell* dndspellbook_collection_spell(
    DndSpellbookCollectionApp* app,
    uint16_t logical,
    uint8_t* local_out);
static void dndspellbook_collection_redraw(DndSpellbookCollectionApp* app);
static void dndspellbook_collection_begin_text(
    DndSpellbookCollectionApp* app,
    DndSpellbookCollectionEdit edit,
    const char* header,
    const char* initial);
static bool dndspellbook_collection_begin_number(DndSpellbookCollectionApp* app, uint8_t field);
static void dndspellbook_collection_release_text(DndSpellbookCollectionApp* app);
static void dndspellbook_collection_number_done(void* context, int32_t number);
static bool dndspellbook_magic_save(DndSpellbookCollectionApp* app);
static void dndspellbook_magic_refresh_counts(DndSpellbookCollectionApp* app);

static bool dndspellbook_collection_contains_ci(const char* text, const char* needle) {
    if(!text || !needle || !needle[0]) return false;
    for(const char* start = text; *start; ++start) {
        const char* left = start;
        const char* right = needle;
        while(*left && *right) {
            char a = *left;
            char b = *right;
            if(a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
            if(b >= 'A' && b <= 'Z') b = (char)(b - 'A' + 'a');
            if(a != b) break;
            ++left;
            ++right;
        }
        if(!*right) return true;
    }
    return false;
}

static void dndspellbook_collection_copy(char* destination, size_t size, const char* source) {
    if(!destination || !size) return;
    if(!source) source = "";
    strncpy(destination, source, size - 1U);
    destination[size - 1U] = '\0';
}

static bool dndspellbook_collection_parse_u32(const char* text, uint32_t* output) {
    if(!text || !*text || !output) return false;
    uint32_t value = 0U;
    const char* cursor = text;
    while(*cursor >= '0' && *cursor <= '9') {
        uint32_t digit = (uint32_t)(*cursor - '0');
        if(value > (UINT32_MAX - digit) / 10U) return false;
        value = value * 10U + digit;
        ++cursor;
    }
    if(cursor == text) return false;
    while(*cursor == ' ' || *cursor == '\r' || *cursor == '\n' || *cursor == '\t')
        ++cursor;
    if(*cursor) return false;
    *output = value;
    return true;
}

static uint8_t dndspellbook_collection_clamp_u8(int32_t value, uint8_t maximum) {
    if(value < 0) return 0U;
    if(value > maximum) return maximum;
    return (uint8_t)value;
}

static void dndspellbook_collection_set_status(DndSpellbookCollectionApp* app, const char* text) {
    dndspellbook_collection_copy(app->status, sizeof(app->status), text);
    app->status_transient = 0U;
}

static void
    dndspellbook_collection_set_transient_status(DndSpellbookCollectionApp* app, const char* text) {
    dndspellbook_collection_copy(app->status, sizeof(app->status), text);
    app->status_transient = 1U;
}

static void dndspellbook_collection_draw_header(
    Canvas* canvas,
    DndSpellbookCollectionApp* app,
    const char* title,
    const char* status) {
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_box(canvas, 0, 0, 128, 10);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 8, title);

    /* Character ID belongs only to the main Spellbook list. Catalog paging
       hints are shown only inside the explicit Name/catalog picker. */
    bool main_list = app && app->screen == DndSpellbookCollectionScreenList &&
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
    dndspellbook_collection_draw_row(Canvas* canvas, uint8_t row, bool selected, const char* text) {
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

static void dndspellbook_collection_source_tag(
    const DndSpellbookCharacterState* character,
    const DndSpell* spell,
    char output[4]) {
    if(!output) return;
    output[0] = '\0';
    if(!character || !spell) return;
    const char* source = NULL;
    if(spell->grant_name[0])
        source = spell->grant_name;
    else if(spell->class_index < character->class_count)
        source = character->classes[spell->class_index].name;
    else if(spell->source[0])
        source = spell->source;
    if(!source) return;
    uint8_t used = 0U;
    for(size_t i = 0U; source[i] && used < 3U; ++i) {
        char ch = source[i];
        if(ch >= 'a' && ch <= 'z') ch = (char)(ch - ('a' - 'A'));
        if((ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9')) output[used++] = ch;
    }
    output[used] = '\0';
}

static void dndspellbook_collection_draw_spell_row(
    Canvas* canvas,
    uint8_t row,
    bool selected,
    const char* text,
    const char* source_tag) {
    uint8_t y = (uint8_t)(11U + row * 10U);
    if(selected) {
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_box(canvas, 0, y, 128, 10);
        canvas_set_color(canvas, ColorWhite);
    } else {
        canvas_set_color(canvas, ColorBlack);
    }
    canvas_set_font(canvas, FontSecondary);
    uint8_t tag_x = 126U;
    if(source_tag && source_tag[0]) {
        uint16_t tag_width = canvas_string_width(canvas, source_tag);
        tag_x = tag_width < 123U ? (uint8_t)(125U - tag_width) : 3U;
        canvas_draw_str(canvas, tag_x, y + 8U, source_tag);
    }
    char display[32];
    size_t length = strlen(text ? text : "");
    size_t copy = length < sizeof(display) - 1U ? length : sizeof(display) - 1U;
    memcpy(display, text ? text : "", copy);
    display[copy] = '\0';
    if(tag_x > 8U) {
        uint16_t available = (uint16_t)(tag_x - 6U);
        while(display[0] && canvas_string_width(canvas, display) > available)
            display[strlen(display) - 1U] = '\0';
    }
    canvas_draw_str(canvas, 3, y + 8U, display);
    canvas_set_color(canvas, ColorBlack);
}

static void dndspellbook_collection_redraw(DndSpellbookCollectionApp* app) {
    if(!app || !app->view) return;
    DndSpellbookCollectionApp** model = view_get_model(app->view);
    if(!model) return;
    *model = app;
    view_commit_model(app->view, true);
}

static bool
    dndspellbook_collection_class_uses_wizard_spell_list(const DndClassLevel* class_level) {
    if(!class_level) return false;
    return (!strcmp(class_level->name, "Fighter") &&
            !strcmp(class_level->subclass, "Eldritch Knight")) ||
           (!strcmp(class_level->name, "Rogue") &&
            !strcmp(class_level->subclass, "Arcane Trickster"));
}

static uint16_t dndspellbook_collection_class_mask_from_name(const char* name) {
    if(!name) return 0U;
    if(strcmp(name, "Artificer") == 0) return DndSpellbookClassMaskArtificer;
    if(strcmp(name, "Barbarian") == 0) return DndSpellbookClassMaskBarbarian;
    if(strcmp(name, "Bard") == 0) return DndSpellbookClassMaskBard;
    if(strcmp(name, "Cleric") == 0) return DndSpellbookClassMaskCleric;
    if(strcmp(name, "Druid") == 0) return DndSpellbookClassMaskDruid;
    if(strcmp(name, "Fighter") == 0) return DndSpellbookClassMaskFighter;
    if(strcmp(name, "Monk") == 0) return DndSpellbookClassMaskMonk;
    if(strcmp(name, "Paladin") == 0) return DndSpellbookClassMaskPaladin;
    if(strcmp(name, "Ranger") == 0) return DndSpellbookClassMaskRanger;
    if(strcmp(name, "Rogue") == 0) return DndSpellbookClassMaskRogue;
    if(strcmp(name, "Sorcerer") == 0) return DndSpellbookClassMaskSorcerer;
    if(strcmp(name, "Warlock") == 0) return DndSpellbookClassMaskWarlock;
    if(strcmp(name, "Wizard") == 0) return DndSpellbookClassMaskWizard;
    return 0U;
}

static char* dndspellbook_collection_trim(char* text) {
    if(!text) return text;
    while(*text == ' ' || *text == '\t')
        ++text;
    char* end = text + strlen(text);
    while(end > text && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r' || end[-1] == '\n'))
        --end;
    *end = '\0';
    return text;
}

static bool dndspellbook_collection_equals_ci(const char* left, const char* right) {
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

static uint8_t dndspellbook_collection_school(const char* school) {
    if(!school || !*school) return 0U;
    for(uint8_t i = 1U; i < 9U; ++i)
        if(dndspellbook_collection_equals_ci(school, dndspellbook_collection_school_names[i]))
            return i;
    return 0U;
}

static uint8_t dndspellbook_collection_source(const char* source) {
    if(!source || !*source) return DndSpellbookSourceOther;
    if(dndspellbook_collection_equals_ci(source, "Core") || strstr(source, "SRD"))
        return DndSpellbookSourceCore;
    if(strstr(source, "Xanathar") || strstr(source, "XGE")) return DndSpellbookSourceXanathar;
    if(strstr(source, "Forgotten Realms") || strstr(source, "FR"))
        return DndSpellbookSourceForgottenRealms;
    if(strstr(source, "Ravenloft")) return DndSpellbookSourceRavenloft;
    return DndSpellbookSourceOther;
}

static uint16_t dndspellbook_collection_class_mask(char* classes) {
    uint16_t mask = 0U;
    char* cursor = classes;
    while(cursor && *cursor) {
        char* comma = strchr(cursor, ',');
        if(comma) *comma = '\0';
        mask |= dndspellbook_collection_class_mask_from_name(dndspellbook_collection_trim(cursor));
        if(!comma) break;
        cursor = comma + 1U;
    }
    return mask;
}

static uint16_t dndspellbook_collection_character_class_mask(const DndClassLevel* class_level) {
    if(!class_level) return 0U;
    return dndspellbook_collection_class_uses_wizard_spell_list(class_level) ?
               DndSpellbookClassMaskWizard :
               dndspellbook_collection_class_mask_from_name(class_level->name);
}

static bool dndspellbook_collection_class_allows(
    const DndSpellbookCharacterState* character,
    uint8_t class_index,
    uint8_t level,
    uint16_t mask) {
    if(!character || class_index >= character->class_count) return false;
    const DndClassLevel* class_level = &character->classes[class_index];
    uint16_t selected = dndspellbook_collection_character_class_mask(class_level);
    return selected && (mask & selected) &&
           level <= dnd_spell_eligibility_class_max_spell_level(class_level);
}

static uint16_t dndspellbook_collection_filter_class_mask(uint8_t filter_class) {
    if(filter_class >= DNDSPELLBOOK_COLLECTION_CLASS_COUNT) return 0U;
    return (uint16_t)(1U << filter_class);
}

static const char* dndspellbook_collection_filter_class_name(uint8_t filter_class) {
    if(filter_class == DNDSPELLBOOK_COLLECTION_FILTER_CHARACTER_CLASSES)
        return "Character Classes";
    if(filter_class == DNDSPELLBOOK_COLLECTION_FILTER_ANY_CLASS) return "Any Class";
    if(filter_class < DNDSPELLBOOK_COLLECTION_CLASS_COUNT)
        return dndspellbook_collection_class_names[filter_class];
    return "Character Classes";
}

static bool dndspellbook_collection_character_classes_match(
    const DndSpellbookCharacterState* character,
    uint8_t level,
    uint16_t mask,
    bool enforce_level) {
    if(!character) return false;
    for(uint8_t i = 0U; i < character->class_count; ++i) {
        uint16_t selected = dndspellbook_collection_character_class_mask(&character->classes[i]);
        if(!selected || !(mask & selected)) continue;
        if(!enforce_level ||
           level <= dnd_spell_eligibility_class_max_spell_level(&character->classes[i]))
            return true;
    }
    return false;
}

static bool dndspellbook_collection_selected_class_allowed(
    const DndSpellbookCharacterState* character,
    uint8_t filter_class,
    uint8_t level,
    uint16_t mask) {
    uint16_t selected_filter = dndspellbook_collection_filter_class_mask(filter_class);
    if(!selected_filter || !(mask & selected_filter) || !character) return false;
    for(uint8_t i = 0U; i < character->class_count; ++i) {
        if(dndspellbook_collection_character_class_mask(&character->classes[i]) != selected_filter)
            continue;
        if(level <= dnd_spell_eligibility_class_max_spell_level(&character->classes[i]))
            return true;
    }
    return false;
}

static bool dndspellbook_collection_spell_allowed(
    DndSpellbookCollectionApp* app,
    uint8_t level,
    uint16_t mask) {
    DndSpellbookCharacterState* character = &app->data.character;

    if(app->filter_class == DNDSPELLBOOK_COLLECTION_FILTER_ANY_CLASS) {
        if(app->filter_show_all) return true;
        return dndspellbook_collection_character_classes_match(character, level, mask, true);
    }

    if(app->filter_class == DNDSPELLBOOK_COLLECTION_FILTER_CHARACTER_CLASSES)
        return dndspellbook_collection_character_classes_match(
            character, level, mask, !app->filter_show_all);

    uint16_t selected_filter = dndspellbook_collection_filter_class_mask(app->filter_class);
    if(!selected_filter || !(mask & selected_filter)) return false;
    if(app->filter_show_all) return true;
    return dndspellbook_collection_selected_class_allowed(
        character, app->filter_class, level, mask);
}

static uint8_t dndspellbook_collection_resolve_class(
    DndSpellbookCollectionApp* app,
    uint8_t level,
    uint16_t mask,
    uint8_t preferred) {
    DndSpellbookCharacterState* character = &app->data.character;
    uint16_t selected_filter = dndspellbook_collection_filter_class_mask(app->filter_class);

    if(selected_filter) {
        for(uint8_t i = 0U; i < character->class_count; ++i)
            if(dndspellbook_collection_character_class_mask(&character->classes[i]) ==
                   selected_filter &&
               dndspellbook_collection_class_allows(character, i, level, mask))
                return i;
        if(app->filter_show_all)
            for(uint8_t i = 0U; i < character->class_count; ++i)
                if(dndspellbook_collection_character_class_mask(&character->classes[i]) ==
                       selected_filter &&
                   (mask & selected_filter))
                    return i;
    }

    if(preferred < character->class_count &&
       dndspellbook_collection_class_allows(character, preferred, level, mask))
        return preferred;
    for(uint8_t i = 0U; i < character->class_count; ++i)
        if(dndspellbook_collection_class_allows(character, i, level, mask)) return i;

    if(app->filter_show_all) {
        if(preferred < character->class_count &&
           (mask & dndspellbook_collection_character_class_mask(&character->classes[preferred])))
            return preferred;
        for(uint8_t i = 0U; i < character->class_count; ++i)
            if(mask & dndspellbook_collection_character_class_mask(&character->classes[i]))
                return i;
    }
    return preferred < character->class_count ? preferred : 0U;
}

static void dndspellbook_collection_clear_page(DndSpellbookCharacterState* character) {
    if(!character) return;
    free(character->spell_storage);
    character->spell_storage = NULL;
    character->spells = NULL;
    character->spell_known = NULL;
    character->spell_always_prepared = NULL;
    character->spell_free_casts_current = NULL;
    character->spell_free_casts_max = NULL;
    character->spell_count = 0U;
    character->spell_capacity = 0U;
}

static bool
    dndspellbook_collection_resize_page(DndSpellbookCharacterState* state, uint8_t required) {
    if(!state || required > DND_STORAGE_COLLECTION_CACHE_SIZE) return false;
    DndCharacter* io = calloc(1U, sizeof(DndCharacter));
    if(!io) return false;
    io->spell_storage = state->spell_storage;
    io->spells = state->spells;
    io->spell_known = state->spell_known;
    io->spell_always_prepared = state->spell_always_prepared;
    io->spell_free_casts_current = state->spell_free_casts_current;
    io->spell_free_casts_max = state->spell_free_casts_max;
    io->spell_count = state->spell_count;
    io->spell_capacity = state->spell_capacity;
    if(!dnd_data_reserve_spells(io, required)) {
        free(io);
        return false;
    }
    state->spell_storage = io->spell_storage;
    state->spells = io->spells;
    state->spell_known = io->spell_known;
    state->spell_always_prepared = io->spell_always_prepared;
    state->spell_free_casts_current = io->spell_free_casts_current;
    state->spell_free_casts_max = io->spell_free_casts_max;
    state->spell_capacity = io->spell_capacity;
    free(io);
    return true;
}

static DndCharacter* dndspellbook_collection_io_character(
    const DndSpellbookCharacterState* state,
    bool attach_page) {
    if(!state) return NULL;
    DndCharacter* io = calloc(1U, sizeof(DndCharacter));
    if(!io) return NULL;
    dndspellbook_collection_copy(io->name, sizeof(io->name), state->name);
    io->class_count = state->class_count;
    for(uint8_t i = 0U; i < state->class_count && i < DND_MAX_CLASSES; ++i)
        io->classes[i] = state->classes[i];
    if(attach_page) {
        io->spell_count = state->spell_count;
        io->spell_capacity = state->spell_capacity;
        io->spell_storage = state->spell_storage;
        io->spells = state->spells;
        io->spell_known = state->spell_known;
        io->spell_always_prepared = state->spell_always_prepared;
        io->spell_free_casts_current = state->spell_free_casts_current;
        io->spell_free_casts_max = state->spell_free_casts_max;
    }
    return io;
}

static void dndspellbook_collection_free_io_character(DndCharacter* io, bool owns_page) {
    if(!io) return;
    if(owns_page)
        dnd_data_clear_spells(io);
    else {
        io->spell_storage = NULL;
        io->spells = NULL;
        io->spell_known = NULL;
        io->spell_always_prepared = NULL;
        io->spell_free_casts_current = NULL;
        io->spell_free_casts_max = NULL;
    }
    free(io);
}

static bool dndspellbook_collection_load_page(DndSpellbookCollectionApp* app, uint16_t start) {
    DndCharacter* io = dndspellbook_collection_io_character(&app->data.character, false);
    if(!io) return false;
    uint16_t total = app->total;
    bool ok = dnd_storage_load_spellbook_window_indexed(
        app->storage,
        app->profile,
        start,
        io,
        &total,
        app->record_page_offsets,
        &app->record_offset_valid_pages);
    if(ok) {
        dndspellbook_collection_clear_page(&app->data.character);
        free(app->search_matches);
        app->search_matches = NULL;
        app->data.character.spell_storage = io->spell_storage;
        app->data.character.spells = io->spells;
        app->data.character.spell_known = io->spell_known;
        app->data.character.spell_always_prepared = io->spell_always_prepared;
        app->data.character.spell_free_casts_current = io->spell_free_casts_current;
        app->data.character.spell_free_casts_max = io->spell_free_casts_max;
        app->data.character.spell_count = io->spell_count;
        app->data.character.spell_capacity = io->spell_capacity;
        io->spell_storage = NULL;
        io->spells = NULL;
        io->spell_known = NULL;
        io->spell_always_prepared = NULL;
        io->spell_free_casts_current = NULL;
        io->spell_free_casts_max = NULL;
        io->spell_count = io->spell_capacity = 0U;
        app->total = total;
        app->cache_start = start;
    }
    dndspellbook_collection_free_io_character(io, true);
    return ok;
}

static bool dndspellbook_collection_save_page(DndSpellbookCollectionApp* app) {
    DndCharacter* io = dndspellbook_collection_io_character(&app->data.character, true);
    bool ok = io &&
              dnd_storage_save_spellbook_window(app->storage, app->profile, app->cache_start, io);
    dndspellbook_collection_free_io_character(io, false);
    if(ok) {
        app->record_offset_valid_pages = 0U;
        app->status_prefilter_valid = 0U;
    }
    if(ok)
        dndspellbook_collection_set_transient_status(app, "Saved");
    else
        dndspellbook_collection_set_status(app, "UNSAVED");
    return ok;
}

static bool
    dndspellbook_collection_prepare_record(DndSpellbookCollectionApp* app, uint16_t logical) {
    if(logical >= app->total) return false;
    uint16_t target =
        (logical / DND_STORAGE_COLLECTION_CACHE_SIZE) * DND_STORAGE_COLLECTION_CACHE_SIZE;
    if(target != app->cache_start && !dndspellbook_collection_load_page(app, target)) {
        dndspellbook_collection_set_status(app, "Read failed");
        return false;
    }
    return logical >= app->cache_start &&
           logical < app->cache_start + app->data.character.spell_count;
}

static uint8_t
    dndspellbook_collection_local(const DndSpellbookCollectionApp* app, uint16_t logical) {
    return (uint8_t)(logical - app->cache_start);
}

static DndSpell* dndspellbook_collection_spell(
    DndSpellbookCollectionApp* app,
    uint16_t logical,
    uint8_t* local_out) {
    if(!dndspellbook_collection_prepare_record(app, logical)) return NULL;
    uint8_t local = dndspellbook_collection_local(app, logical);
    if(local >= app->data.character.spell_count) return NULL;
    if(local_out) *local_out = local;
    return &app->data.character.spells[local];
}

static DndSpell* dndspellbook_collection_spell_cached(
    DndSpellbookCollectionApp* app,
    uint16_t logical,
    uint8_t* local_out) {
    if(!app || logical < app->cache_start) return NULL;
    uint16_t local = logical - app->cache_start;
    if(local >= app->data.character.spell_count) return NULL;
    if(local_out) *local_out = local;
    return &app->data.character.spells[local];
}

static void dndspellbook_collection_focus_list(DndSpellbookCollectionApp* app, uint16_t logical) {
    app->selection = (uint16_t)logical + 1U;
    uint16_t page_min = (uint16_t)app->cache_start + 1U;
    uint16_t page_max = page_min + DND_STORAGE_COLLECTION_CACHE_SIZE - 1U;
    uint16_t max_selection = app->total;
    if(page_max > max_selection) page_max = max_selection;
    uint16_t scroll = app->selection > 4U ? app->selection - 4U : 0U;
    if(scroll && scroll < page_min) scroll = page_min;
    if(scroll + 4U > page_max && page_max >= 4U) scroll = page_max - 4U;
    if(app->selection == 0U) scroll = 0U;
    app->scroll = scroll;
}

static bool dndspellbook_collection_sort_spellbook(DndSpellbookCollectionApp* app);

static bool dndspellbook_collection_sort_and_reload(DndSpellbookCollectionApp* app) {
    if(!app || !app->sort_pending) return true;
    if(!dndspellbook_collection_sort_spellbook(app)) {
        dndspellbook_collection_set_status(app, "Sort failed");
        return false;
    }
    app->sort_pending = 0U;
    app->record_offset_valid_pages = 0U;
    if(!dndspellbook_collection_load_page(app, 0U)) {
        dndspellbook_collection_set_status(app, "Sorted; read failed");
        return false;
    }
    app->selection = app->total ? 1U : 0U;
    app->scroll = 0U;
    return true;
}

static bool dndspellbook_collection_add_blank(DndSpellbookCollectionApp* app) {
    uint16_t target =
        (app->total / DND_STORAGE_COLLECTION_CACHE_SIZE) * DND_STORAGE_COLLECTION_CACHE_SIZE;
    if(target != app->cache_start && !dndspellbook_collection_load_page(app, target)) {
        dndspellbook_collection_set_status(app, "Tail read failed");
        return false;
    }
    DndSpellbookCharacterState* character = &app->data.character;
    uint8_t expected = (uint8_t)(app->total - target);
    if(character->spell_count != expected ||
       character->spell_count >= DND_STORAGE_COLLECTION_CACHE_SIZE) {
        dndspellbook_collection_set_status(app, "Spell add failed");
        return false;
    }
    uint8_t required = (uint8_t)(character->spell_count + 1U);
    if(!dndspellbook_collection_resize_page(character, required)) {
        (void)dndspellbook_collection_load_page(app, target);
        dndspellbook_collection_set_status(app, "Spell add failed");
        return false;
    }
    character->spell_always_prepared[character->spell_count] = 0U;
    character->spell_free_casts_current[character->spell_count] = 0U;
    character->spell_free_casts_max[character->spell_count] = 0U;
    uint8_t local = character->spell_count;
    DndSpell* spell = &character->spells[local];
    memset(spell, 0, sizeof(*spell));
    dndspellbook_collection_copy(spell->name, sizeof(spell->name), "New Spell");
    character->spell_known[local] = 1U;
    ++character->spell_count;
    app->record_index = app->total++;
    app->sort_pending = 1U;
    bool saved = dndspellbook_collection_save_page(app);
    dndspellbook_collection_focus_list(app, app->record_index);
    app->detail_selection = 0U;
    app->detail_scroll = 0U;
    app->screen = DndSpellbookCollectionScreenDetail;
    if(saved)
        dndspellbook_collection_set_transient_status(app, "Spell added");
    else
        dndspellbook_collection_set_status(app, "Added - UNSAVED");
    return true;
}

static bool dndspellbook_collection_delete_current(DndSpellbookCollectionApp* app) {
    if(app->record_index >= app->total) return false;
    DndCharacter* owner = dndspellbook_collection_io_character(&app->data.character, false);
    bool deleted = owner &&
                   dnd_storage_delete_spell(app->storage, app->profile, owner, app->record_index);
    dndspellbook_collection_free_io_character(owner, false);
    if(!deleted) {
        dndspellbook_collection_set_status(app, "Delete failed");
        return false;
    }
    app->record_offset_valid_pages = 0U;
    if(app->total) --app->total;
    if(app->total) {
        uint16_t logical = app->record_index < app->total ? app->record_index :
                                                            (uint16_t)(app->total - 1U);
        uint16_t target = (uint16_t)((logical / DND_STORAGE_COLLECTION_CACHE_SIZE) *
                                     DND_STORAGE_COLLECTION_CACHE_SIZE);
        if(!dndspellbook_collection_load_page(app, target)) {
            dndspellbook_collection_set_status(app, "Deleted; read failed");
            return true;
        }
        dndspellbook_collection_focus_list(app, logical);
    } else {
        (void)dndspellbook_collection_load_page(app, 0U);
        app->selection = app->scroll = 0U;
    }
    app->screen = DndSpellbookCollectionScreenList;
    dndspellbook_collection_set_status(app, "Deleted");
    return true;
}

typedef struct {
    File* file;
    uint8_t buffer[DNDSPELLBOOK_COLLECTION_CATALOG_READ_BUFFER];
    uint16_t position;
    uint16_t count;
    uint32_t raw_offset;
} DndSpellbookCatalogReader;

static void dndspellbook_collection_catalog_reader_init(
    DndSpellbookCatalogReader* reader,
    File* file,
    uint32_t raw_offset) {
    memset(reader, 0, sizeof(*reader));
    reader->file = file;
    reader->raw_offset = raw_offset;
}

static bool
    dndspellbook_collection_catalog_reader_next(DndSpellbookCatalogReader* reader, char* value) {
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

static bool dndspellbook_collection_catalog_read_line(
    DndSpellbookCatalogReader* reader,
    char* line,
    size_t size) {
    if(!reader || !line || size < 2U) return false;
    size_t used = 0U;
    char ch = '\0';
    bool got = false;
    while(true) {
        if(!dndspellbook_collection_catalog_reader_next(reader, &ch)) break;
        got = true;
        if(ch == '\n') break;
        if(ch != '\r' && used + 1U < size) line[used++] = ch;
    }
    line[used] = '\0';
    return got || used > 0U;
}

typedef struct {
    uint32_t offset;
    uint8_t level;
    char name[DND_SPELL_NAME_LEN];
} DndSpellbookCollectionSortKey;

static uint8_t dndspellbook_collection_ascii_fold(char value) {
    return (uint8_t)((value >= 'A' && value <= 'Z') ? value + ('a' - 'A') : value);
}

static int dndspellbook_collection_sort_key_compare(
    const DndSpellbookCollectionSortKey* left,
    const DndSpellbookCollectionSortKey* right) {
    if(left->level < right->level) return -1;
    if(left->level > right->level) return 1;
    for(size_t i = 0U; i < DND_SPELL_NAME_LEN; ++i) {
        uint8_t a = dndspellbook_collection_ascii_fold(left->name[i]);
        uint8_t b = dndspellbook_collection_ascii_fold(right->name[i]);
        if(a < b) return -1;
        if(a > b) return 1;
        if(!a) break;
    }
    if(left->offset < right->offset) return -1;
    if(left->offset > right->offset) return 1;
    return 0;
}

static int8_t dndspellbook_collection_hex_value(char value) {
    if(value >= '0' && value <= '9') return (int8_t)(value - '0');
    if(value >= 'A' && value <= 'F') return (int8_t)(value - 'A' + 10);
    if(value >= 'a' && value <= 'f') return (int8_t)(value - 'a' + 10);
    return -1;
}

static void dndspellbook_collection_decode_name(
    char* output,
    size_t output_size,
    const char* begin,
    const char* end) {
    if(!output || !output_size) return;
    size_t used = 0U;
    const char* cursor = begin;
    while(cursor < end && used + 1U < output_size) {
        if(*cursor == '%' && cursor + 2U < end) {
            int8_t high = dndspellbook_collection_hex_value(cursor[1]);
            int8_t low = dndspellbook_collection_hex_value(cursor[2]);
            if(high >= 0 && low >= 0) {
                output[used++] = (char)(((uint8_t)high << 4U) | (uint8_t)low);
                cursor += 3U;
                continue;
            }
        }
        output[used++] = *cursor++;
    }
    output[used] = '\0';
}

static bool dndspellbook_collection_sort_key_from_line(
    const char* line,
    uint32_t offset,
    DndSpellbookCollectionSortKey* key) {
    if(!line || !key || strncmp(line, "S|", 2U)) return false;
    const char* separators[7];
    const char* cursor = line;
    for(uint8_t i = 0U; i < 7U; ++i) {
        cursor = strchr(cursor, '|');
        if(!cursor) return false;
        separators[i] = cursor;
        ++cursor;
    }
    if(separators[1] <= separators[0] + 1U) return false;
    const char* numbers = separators[6] + 1U;
    if(*numbers < '0' || *numbers > '9') return false;
    uint32_t level = 0U;
    while(*numbers >= '0' && *numbers <= '9') {
        level = level * 10U + (uint32_t)(*numbers - '0');
        if(level > 9U) return false;
        ++numbers;
    }
    if(*numbers != ',') return false;
    key->offset = offset;
    key->level = (uint8_t)level;
    dndspellbook_collection_decode_name(
        key->name, sizeof(key->name), separators[0] + 1U, separators[1]);
    return key->name[0] != '\0';
}

static uint8_t
    dndspellbook_collection_character_level(const DndSpellbookCharacterState* character) {
    uint16_t total = 0U;
    if(character) {
        for(uint8_t i = 0U; i < character->class_count; ++i)
            total += character->classes[i].level;
    }
    if(total < 1U) total = 1U;
    return total > 255U ? 255U : (uint8_t)total;
}

static void dndspellbook_collection_safe_filename(char* output, size_t size, const char* name) {
    if(!output || !size) return;
    size_t position = 0U;
    const char* source = name && name[0] ? name : "Unnamed";
    for(size_t i = 0U; source[i] && position + 1U < size; ++i) {
        char value = source[i];
        if((value >= 'A' && value <= 'Z') || (value >= 'a' && value <= 'z') ||
           (value >= '0' && value <= '9') || value == '-') {
            output[position++] = value;
        } else if(position && output[position - 1U] != '_') {
            output[position++] = '_';
        }
    }
    while(position && output[position - 1U] == '_')
        --position;
    if(!position) {
        snprintf(output, size, "Unnamed");
        return;
    }
    output[position] = '\0';
}

static bool dndspellbook_collection_write_raw(File* file, const char* value) {
    if(!file || !value) return false;
    size_t length = strlen(value);
    return storage_file_write(file, value, length) == length;
}

static bool dndspellbook_collection_copy_file(
    Storage* storage,
    const char* source,
    const char* destination) {
    File* input = storage_file_alloc(storage);
    File* output = storage_file_alloc(storage);
    if(!input || !output) {
        if(input) storage_file_free(input);
        if(output) storage_file_free(output);
        return false;
    }
    bool success = storage_file_open(input, source, FSAM_READ, FSOM_OPEN_EXISTING) &&
                   storage_file_open(output, destination, FSAM_WRITE, FSOM_CREATE_ALWAYS);
    uint8_t buffer[DNDSPELLBOOK_COLLECTION_SORT_COPY_BUFFER];
    while(success) {
        size_t count = storage_file_read(input, buffer, sizeof(buffer));
        if(!count) break;
        success = storage_file_write(output, buffer, count) == count;
    }
    if(success) success = storage_file_get_error(input) == FSE_OK && storage_file_sync(output);
    storage_file_close(input);
    storage_file_close(output);
    storage_file_free(input);
    storage_file_free(output);
    return success && storage_file_exists(storage, destination);
}

static bool dndspellbook_collection_sort_spellbook(DndSpellbookCollectionApp* app) {
    if(!app || !app->storage) return false;
    enum {
        SortBatch = 24U
    };
    char live[DND_FS_PATH_LEN];
    snprintf(
        live,
        sizeof(live),
        "%s/spellbook_%lu.txt",
        DND_CHARACTER_DATA_ROOT,
        (unsigned long)app->profile);
    if(!storage_file_exists(app->storage, live)) return true;
    File* input = storage_file_alloc(app->storage);
    if(!input || !storage_file_open(input, live, FSAM_READ, FSOM_OPEN_EXISTING)) {
        if(input) storage_file_free(input);
        return false;
    }
    char* line = malloc(DNDSPELLBOOK_COLLECTION_SORT_LINE_MAX);
    DndSpellbookCollectionSortKey* keys =
        malloc(sizeof(DndSpellbookCollectionSortKey) * SortBatch);
    if(!line || !keys) {
        free(keys);
        free(line);
        storage_file_close(input);
        storage_file_free(input);
        return false;
    }
    /* A sorted live file needs no rewrite, even at startup. */
    DndSpellbookCatalogReader reader;
    dndspellbook_collection_catalog_reader_init(&reader, input, 0U);
    bool ordered = true, have_previous = false;
    DndSpellbookCollectionSortKey previous;
    while(true) {
        uint32_t offset = reader.raw_offset;
        if(!dndspellbook_collection_catalog_read_line(
               &reader, line, DNDSPELLBOOK_COLLECTION_SORT_LINE_MAX))
            break;
        DndSpellbookCollectionSortKey key;
        if(!dndspellbook_collection_sort_key_from_line(line, offset, &key)) continue;
        if(have_previous && dndspellbook_collection_sort_key_compare(&previous, &key) > 0) {
            ordered = false;
            break;
        }
        previous = key;
        have_previous = true;
    }
    if(ordered || storage_file_get_error(input) != FSE_OK) {
        bool ok = storage_file_get_error(input) == FSE_OK;
        free(keys);
        free(line);
        storage_file_close(input);
        storage_file_free(input);
        return ok;
    }
    char safe_name[DND_CHARACTER_NAME_LEN];
    dndspellbook_collection_safe_filename(safe_name, sizeof(safe_name), app->data.character.name);
    char snapshot[DND_FS_PATH_LEN];
    snprintf(
        snapshot,
        sizeof(snapshot),
        "%s/ch_%lu_%s_%u_spellbook.shd",
        DND_CHARACTER_DATA_ROOT,
        (unsigned long)app->profile,
        safe_name,
        dndspellbook_collection_character_level(&app->data.character));
    storage_common_mkdir(app->storage, DND_CHARACTER_DATA_ROOT);
    File* output = storage_file_alloc(app->storage);
    if(!output || !storage_file_open(output, snapshot, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        if(output) storage_file_free(output);
        free(keys);
        free(line);
        storage_file_close(input);
        storage_file_free(input);
        return false;
    }
    bool success = dndspellbook_collection_write_raw(output, "DNDSpellbook=1\n");
    if(success && storage_file_seek(input, 0U, true)) {
        dndspellbook_collection_catalog_reader_init(&reader, input, 0U);
        while(true) {
            uint32_t offset = reader.raw_offset;
            if(!dndspellbook_collection_catalog_read_line(
                   &reader, line, DNDSPELLBOOK_COLLECTION_SORT_LINE_MAX))
                break;
            if(!strncmp(line, "DNDSpellbook=", 13U)) continue;
            DndSpellbookCollectionSortKey key;
            if(dndspellbook_collection_sort_key_from_line(line, offset, &key)) continue;
            if(!dndspellbook_collection_write_raw(output, line) ||
               !dndspellbook_collection_write_raw(output, "\n")) {
                success = false;
                break;
            }
        }
    } else if(success)
        success = false;
    DndSpellbookCollectionSortKey last;
    bool have_last = false;
    while(success) {
        uint8_t count = 0U;
        if(!storage_file_seek(input, 0U, true)) {
            success = false;
            break;
        }
        dndspellbook_collection_catalog_reader_init(&reader, input, 0U);
        while(true) {
            uint32_t offset = reader.raw_offset;
            if(!dndspellbook_collection_catalog_read_line(
                   &reader, line, DNDSPELLBOOK_COLLECTION_SORT_LINE_MAX))
                break;
            DndSpellbookCollectionSortKey key;
            if(!dndspellbook_collection_sort_key_from_line(line, offset, &key)) continue;
            if(have_last && dndspellbook_collection_sort_key_compare(&key, &last) <= 0) continue;
            uint8_t pos = 0U;
            while(pos < count && dndspellbook_collection_sort_key_compare(&keys[pos], &key) <= 0)
                ++pos;
            if(pos >= SortBatch) continue;
            uint8_t tail = count < SortBatch ? count : (uint8_t)(SortBatch - 1U);
            while(tail > pos) {
                keys[tail] = keys[tail - 1U];
                --tail;
            }
            keys[pos] = key;
            if(count < SortBatch) ++count;
        }
        if(storage_file_get_error(input) != FSE_OK) {
            success = false;
            break;
        }
        if(!count) break;
        for(uint8_t i = 0U; i < count; ++i) {
            if(!storage_file_seek(input, keys[i].offset, true)) {
                success = false;
                break;
            }
            dndspellbook_collection_catalog_reader_init(&reader, input, keys[i].offset);
            if(!dndspellbook_collection_catalog_read_line(
                   &reader, line, DNDSPELLBOOK_COLLECTION_SORT_LINE_MAX) ||
               !dndspellbook_collection_write_raw(output, line) ||
               !dndspellbook_collection_write_raw(output, "\n")) {
                success = false;
                break;
            }
        }
        last = keys[count - 1U];
        have_last = true;
    }
    if(success) success = storage_file_sync(output);
    storage_file_close(output);
    storage_file_free(output);
    storage_file_close(input);
    storage_file_free(input);
    if(success) {
        char temp[DND_FS_LONG_PATH_LEN], backup[DND_FS_LONG_PATH_LEN];
        snprintf(temp, sizeof(temp), "%s.sort.tmp", live);
        snprintf(backup, sizeof(backup), "%s.sort.bak", live);
        success = dndspellbook_collection_copy_file(app->storage, snapshot, temp) &&
                  dnd_fs_publish(app->storage, temp, live, backup);
        if(!success) storage_common_remove(app->storage, temp);
    }
    free(keys);
    free(line);
    return success;
}

static bool dndspellbook_collection_use_all_catalog(const DndSpellbookCollectionApp* app);
static uint32_t
    dndspellbook_collection_catalog_offset_encode(bool all_catalog, uint32_t raw_offset);

static void dndspellbook_collection_reset_catalog_offsets(DndSpellbookCollectionApp* app) {
    if(!app) return;
    memset(app->catalog_page_offsets, 0, sizeof(app->catalog_page_offsets));
    app->catalog_page_offsets[0] = dndspellbook_collection_catalog_offset_encode(
        dndspellbook_collection_use_all_catalog(app), 0U);
    app->catalog_offset_valid_pages = 1U;
}

typedef struct {
    const char* name;
    uint8_t filter;
    bool matched;
} DndSpellbookStatusContext;

typedef struct {
    uint8_t* bits;
    uint8_t filter;
} DndSpellbookStatusPrefilterContext;

static uint32_t dndspellbook_collection_name_hash(const char* name) {
    uint32_t hash = 2166136261U;
    if(!name) return hash;
    while(*name) {
        hash ^= (uint8_t)*name++;
        hash *= 16777619U;
    }
    return hash;
}

static void dndspellbook_collection_status_prefilter_mark(uint8_t* bits, const char* name) {
    uint32_t hash = dndspellbook_collection_name_hash(name);
    uint16_t first = (uint16_t)(hash & 1023U);
    uint16_t second = (uint16_t)(((hash >> 16U) ^ (hash * 33U)) & 1023U);
    bits[first >> 3U] |= (uint8_t)(1U << (first & 7U));
    bits[second >> 3U] |= (uint8_t)(1U << (second & 7U));
}

static bool
    dndspellbook_collection_status_prefilter_maybe_has(const uint8_t* bits, const char* name) {
    uint32_t hash = dndspellbook_collection_name_hash(name);
    uint16_t first = (uint16_t)(hash & 1023U);
    uint16_t second = (uint16_t)(((hash >> 16U) ^ (hash * 33U)) & 1023U);
    return (bits[first >> 3U] & (uint8_t)(1U << (first & 7U))) &&
           (bits[second >> 3U] & (uint8_t)(1U << (second & 7U)));
}

static bool dndspellbook_collection_status_prefilter_visitor(
    uint16_t logical_index,
    const DndSpell* spell,
    uint8_t known,
    uint8_t always_prepared,
    uint8_t free_casts_current,
    uint8_t free_casts_max,
    void* context) {
    UNUSED(logical_index);
    UNUSED(free_casts_current);
    UNUSED(free_casts_max);
    DndSpellbookStatusPrefilterContext* prefilter = context;
    if(!prefilter || !prefilter->bits || !spell) return true;
    bool matched = prefilter->filter == 1U ? spell->prepared != 0U :
                   prefilter->filter == 2U ? known != 0U :
                                             always_prepared != 0U;
    if(matched) dndspellbook_collection_status_prefilter_mark(prefilter->bits, spell->name);
    return true;
}

static bool dndspellbook_collection_status_prefilter_build(DndSpellbookCollectionApp* app) {
    if(!app || !app->filter_status) return true;
    memset(app->status_prefilter, 0, sizeof(app->status_prefilter));
    DndSpellbookStatusPrefilterContext prefilter = {
        .bits = app->status_prefilter,
        .filter = app->filter_status,
    };
    if(!dnd_storage_visit_spells(
           app->storage,
           app->profile,
           dndspellbook_collection_status_prefilter_visitor,
           &prefilter,
           NULL)) {
        app->status_prefilter_valid = 0U;
        return false;
    }
    app->status_prefilter_filter = app->filter_status;
    app->status_prefilter_valid = 1U;
    return true;
}

static bool dndspellbook_collection_status_visitor(
    uint16_t logical_index,
    const DndSpell* spell,
    uint8_t known,
    uint8_t always_prepared,
    uint8_t free_casts_current,
    uint8_t free_casts_max,
    void* context) {
    UNUSED(logical_index);
    UNUSED(free_casts_current);
    UNUSED(free_casts_max);
    DndSpellbookStatusContext* status = context;
    if(!status || !spell || strcmp(spell->name, status->name) != 0) return true;
    if(status->filter == 1U)
        status->matched = spell->prepared != 0U;
    else if(status->filter == 2U)
        status->matched = known != 0U;
    else
        status->matched = always_prepared != 0U;
    return !status->matched;
}

static bool
    dndspellbook_collection_status_matches(DndSpellbookCollectionApp* app, const char* name) {
    if(!app->filter_status) return true;
    if((!app->status_prefilter_valid || app->status_prefilter_filter != app->filter_status) &&
       !dndspellbook_collection_status_prefilter_build(app))
        return false;
    if(!dndspellbook_collection_status_prefilter_maybe_has(app->status_prefilter, name))
        return false;
    DndSpellbookStatusContext status = {
        .name = name,
        .filter = app->filter_status,
        .matched = false,
    };
    if(!dnd_storage_visit_spells(
           app->storage, app->profile, dndspellbook_collection_status_visitor, &status, NULL))
        return false;
    return status.matched;
}

static bool dndspellbook_collection_catalog_matches(
    DndSpellbookCollectionApp* app,
    const char* name,
    uint8_t level,
    uint16_t mask,
    uint8_t school,
    uint8_t source,
    bool ritual) {
    if(!dndspellbook_collection_spell_allowed(app, level, mask)) return false;
    if(app->filter_level >= 0 && level != (uint8_t)app->filter_level) return false;
    if(app->filter_ritual && !ritual) return false;
    if(app->filter_school && school != app->filter_school) return false;
    if(app->filter_source && source != app->filter_source) return false;
    if(app->filter_status && !dndspellbook_collection_status_matches(app, name)) return false;
    return true;
}

static bool dndspellbook_collection_catalog_source_allowed(
    const DndSpellbookCollectionApp* app,
    const char* source) {
    if(!app || !source || !source[0]) return false;
    if(!strcmp(source, "Core") || !strcmp(source, "SRD5.2.1")) return true;
    if(!strcmp(source, "Homebrew") || !strcmp(source, "DNDolphins"))
        return app->settings.homebrew != 0U;
    return app->settings.catalog_all != 0U;
}

static bool dndspellbook_collection_use_all_catalog(const DndSpellbookCollectionApp* app) {
    return app && app->settings.catalog_all && app->catalog_all_available;
}

static const char*
    dndspellbook_collection_catalog_path(const DndSpellbookCollectionApp* app, bool all_catalog) {
    if(app && app->canonical_catalog_available)
        return all_catalog ? DNDSPELLBOOK_COLLECTION_SPELL_CATALOG_ALL :
                             DNDSPELLBOOK_COLLECTION_SPELL_CATALOG;
    return all_catalog ? DNDSPELLBOOK_COLLECTION_SPELL_CATALOG_ALL_LEGACY :
                         DNDSPELLBOOK_COLLECTION_SPELL_CATALOG_LEGACY;
}

static uint32_t
    dndspellbook_collection_catalog_offset_encode(bool all_catalog, uint32_t raw_offset) {
    return (raw_offset & DNDSPELLBOOK_COLLECTION_CATALOG_OFFSET_MASK) |
           (all_catalog ? DNDSPELLBOOK_COLLECTION_CATALOG_SOURCE_ALL : 0U);
}

static uint32_t dndspellbook_collection_catalog_offset_raw(uint32_t encoded) {
    return encoded & DNDSPELLBOOK_COLLECTION_CATALOG_OFFSET_MASK;
}

static bool dndspellbook_collection_catalog_offset_all_catalog(uint32_t encoded) {
    return (encoded & DNDSPELLBOOK_COLLECTION_CATALOG_SOURCE_ALL) != 0U;
}

static bool dndspellbook_collection_parse_catalog_line(
    DndSpellbookCollectionApp* app,
    char* line,
    DndSpellbookCatalogEntry* entry) {
    if(!line || !entry) return false;
    char* start = dndspellbook_collection_trim(line);
    if(!*start || *start == '#') return false;
    memset(entry, 0, sizeof(*entry));
    char* level_text = strchr(start, '|');
    if(!level_text) return false;
    *level_text++ = '\0';
    char* classes = strchr(level_text, '|');
    if(!classes) return false;
    *classes++ = '\0';
    char* school = strchr(classes, '|');
    if(!school) return false;
    *school++ = '\0';
    char* ritual = strchr(school, '|');
    if(ritual) *ritual++ = '\0';
    char* source = ritual ? strchr(ritual, '|') : NULL;
    if(source) *source++ = '\0';
    uint32_t level_number = 0U;
    if(!dndspellbook_collection_parse_u32(
           dndspellbook_collection_trim(level_text), &level_number) ||
       level_number > 9U)
        return false;
    uint16_t mask = dndspellbook_collection_class_mask(dndspellbook_collection_trim(classes));
    uint8_t school_id = dndspellbook_collection_school(dndspellbook_collection_trim(school));
    bool ritual_flag =
        ritual &&
        (dndspellbook_collection_equals_ci(dndspellbook_collection_trim(ritual), "Yes") ||
         dndspellbook_collection_equals_ci(dndspellbook_collection_trim(ritual), "True"));
    const char* source_text = source ? dndspellbook_collection_trim(source) : "";
    if(!dndspellbook_collection_catalog_source_allowed(app, source_text)) return false;
    uint8_t source_id = source ? dndspellbook_collection_source(source_text) :
                                 DndSpellbookSourceOther;
    start = dndspellbook_collection_trim(start);
    if(!dndspellbook_collection_catalog_matches(
           app, start, (uint8_t)level_number, mask, school_id, source_id, ritual_flag))
        return false;
    if(strlen(app->search_term) >= DNDSPELLBOOK_COLLECTION_SEARCH_MIN_CHARS &&
       !dndspellbook_collection_contains_ci(start, app->search_term))
        return false;
    dndspellbook_collection_copy(entry->name, sizeof(entry->name), start);
    entry->level = (uint8_t)level_number;
    entry->class_mask = mask;
    entry->school = school_id;
    entry->source = source_id;
    entry->ritual = ritual_flag ? 1U : 0U;
    return entry->name[0] != '\0';
}

static bool dndspellbook_collection_load_catalog(DndSpellbookCollectionApp* app) {
    app->catalog_count = 0U;
    app->catalog_total = 0U;
    app->catalog_has_more = 0U;

    uint16_t page_index = app->catalog_page_start / DNDSPELLBOOK_COLLECTION_CATALOG_PAGE;
    if(page_index >= DNDSPELLBOOK_COLLECTION_CATALOG_OFFSET_PAGES) return false;
    if(!app->catalog_offset_valid_pages) dndspellbook_collection_reset_catalog_offsets(app);

    uint16_t seek_page = page_index;
    if(seek_page >= app->catalog_offset_valid_pages)
        seek_page = (uint16_t)(app->catalog_offset_valid_pages - 1U);

    uint32_t encoded_offset = app->catalog_page_offsets[seek_page];
    bool all_catalog = dndspellbook_collection_catalog_offset_all_catalog(encoded_offset);
    uint32_t raw_offset = dndspellbook_collection_catalog_offset_raw(encoded_offset);

    File* file = storage_file_alloc(app->storage);
    if(!file) return false;
    const char* path = dndspellbook_collection_catalog_path(app, all_catalog);
    if(!storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_free(file);
        dndspellbook_collection_set_status(app, "Catalog unavailable");
        return false;
    }
    if(raw_offset && !storage_file_seek(file, raw_offset, true)) {
        storage_file_close(file);
        storage_file_free(file);
        return false;
    }

    DndSpellbookCatalogReader reader;
    dndspellbook_collection_catalog_reader_init(&reader, file, raw_offset);
    char line[DNDSPELLBOOK_COLLECTION_LINE_MAX];
    uint16_t matched = (uint16_t)(seek_page * DNDSPELLBOOK_COLLECTION_CATALOG_PAGE);
    const uint16_t target_start = app->catalog_page_start;
    const uint16_t target_end = (uint16_t)(target_start + DNDSPELLBOOK_COLLECTION_CATALOG_PAGE);
    bool success = true;

    while(true) {
        while(dndspellbook_collection_catalog_read_line(&reader, line, sizeof(line))) {
            DndSpellbookCatalogEntry parsed;
            if(!dndspellbook_collection_parse_catalog_line(app, line, &parsed)) continue;

            if(matched >= target_end) {
                app->catalog_has_more = 1U;
                goto finished;
            }

            parsed.absolute_index = matched;
            if(matched >= target_start &&
               app->catalog_count < DNDSPELLBOOK_COLLECTION_CATALOG_PAGE)
                app->catalog[app->catalog_count++] = parsed;
            ++matched;

            if((matched % DNDSPELLBOOK_COLLECTION_CATALOG_PAGE) == 0U) {
                uint16_t next_page = matched / DNDSPELLBOOK_COLLECTION_CATALOG_PAGE;
                if(next_page < DNDSPELLBOOK_COLLECTION_CATALOG_OFFSET_PAGES) {
                    app->catalog_page_offsets[next_page] =
                        dndspellbook_collection_catalog_offset_encode(
                            all_catalog, reader.raw_offset);
                    if(app->catalog_offset_valid_pages <= next_page)
                        app->catalog_offset_valid_pages = (uint8_t)(next_page + 1U);
                }
            }
        }
        if(storage_file_get_error(file) != FSE_OK) success = false;
        break;
    }

finished:
    app->catalog_total =
        (uint16_t)(target_start + app->catalog_count + (app->catalog_has_more ? 1U : 0U));
    storage_file_close(file);
    storage_file_free(file);
    return success;
}

static void dndspellbook_collection_open_catalog(DndSpellbookCollectionApp* app) {
    app->search_term[0] = '\0';
    app->screen = DndSpellbookCollectionScreenCatalog;
    dndspellbook_collection_reset_catalog_offsets(app);
    app->catalog_page_start = 0U;
    app->selection = 0U;
    if(!dndspellbook_collection_load_catalog(app))
        dndspellbook_collection_set_status(app, "Catalog unavailable");
}

static bool dndspellbook_collection_apply_catalog(DndSpellbookCollectionApp* app) {
    if(app->selection >= app->catalog_count) return false;
    DndSpellbookCatalogEntry* selected = &app->catalog[app->selection];
    uint8_t local = 0U;
    DndSpell* spell = dndspellbook_collection_spell(app, app->record_index, &local);
    if(!spell) return false;
    dndspellbook_collection_copy(spell->name, sizeof(spell->name), selected->name);
    spell->level = selected->level;
    spell->class_index = dndspellbook_collection_resolve_class(
        app, selected->level, selected->class_mask, spell->class_index);
    snprintf(
        spell->stable_id,
        sizeof(spell->stable_id),
        "spell-%u-%u",
        spell->level,
        selected->absolute_index);
    spell->school[0] = '\0';
    if(selected->school)
        dndspellbook_collection_copy(
            spell->school,
            sizeof(spell->school),
            dndspellbook_collection_school_names[selected->school]);
    spell->source[0] = '\0';
    if(selected->source < DndSpellbookSourceCount)
        dndspellbook_collection_copy(
            spell->source,
            sizeof(spell->source),
            dndspellbook_collection_source_names[selected->source]);
    spell->ritual = selected->ritual;
    app->data.character.spell_known[local] = 1U;
    app->sort_pending = 1U;
    bool saved = dndspellbook_collection_save_page(app);
    app->screen = DndSpellbookCollectionScreenDetail;
    app->detail_selection = 0U;
    app->detail_scroll = 0U;
    if(saved)
        dndspellbook_collection_set_transient_status(app, "Catalog choice saved");
    else
        dndspellbook_collection_set_status(app, "Choice - UNSAVED");
    return saved;
}

static uint16_t dndspellbook_collection_list_count(const DndSpellbookCollectionApp* app) {
    return app ? (uint16_t)app->total + 2U : 0U;
}

static uint16_t dndspellbook_collection_magic_selection(const DndSpellbookCollectionApp* app) {
    return app ? (uint16_t)app->total + 1U : 1U;
}

static bool dndspellbook_collection_selection_is_spell(
    const DndSpellbookCollectionApp* app,
    uint16_t selection) {
    return app && selection >= 1U && selection <= app->total;
}

static uint16_t dndspellbook_collection_selection_spell(uint16_t selection) {
    return (uint16_t)(selection - 1U);
}

static void dndspellbook_collection_list_adjust_scroll(DndSpellbookCollectionApp* app) {
    if(!app) return;
    uint16_t count = dndspellbook_collection_list_count(app);
    if(!count) return;
    if(app->selection >= count) app->selection = count - 1U;

    if(app->selection == 0U || app->total == 0U) {
        app->scroll = 0U;
        return;
    }

    if(dndspellbook_collection_selection_is_spell(app, app->selection)) {
        uint16_t logical = dndspellbook_collection_selection_spell(app->selection);
        uint16_t page_start =
            (logical / DND_STORAGE_COLLECTION_CACHE_SIZE) * DND_STORAGE_COLLECTION_CACHE_SIZE;
        uint16_t first = (uint16_t)page_start + 1U;
        uint16_t page_records = app->total - page_start;
        if(page_records > DND_STORAGE_COLLECTION_CACHE_SIZE)
            page_records = DND_STORAGE_COLLECTION_CACHE_SIZE;
        uint16_t last = first + page_records - 1U;

        /* Keep + Add New visible with up to four spells. A fifth spell is the first
           point where the five-row viewport needs to scroll it away. */
        if(page_start == 0U && app->selection <= 4U) {
            app->scroll = 0U;
            return;
        }
        if(page_records <= DNDSPELLBOOK_COLLECTION_ROWS) {
            app->scroll = first;
            return;
        }

        uint16_t scroll = app->selection > first + 3U ? app->selection - 4U : first;
        uint16_t maximum = last - (DNDSPELLBOOK_COLLECTION_ROWS - 1U);
        if(scroll > maximum) scroll = maximum;
        if(scroll < first) scroll = first;
        app->scroll = scroll;
        return;
    }

    /* Magic & Spells is an end-of-list action, matching Inventory's terminal
       utility rows. Keep it beside the final spell page instead of making the
       draw callback span two cached spell pages. */
    uint16_t scroll = count > DNDSPELLBOOK_COLLECTION_ROWS ? count - DNDSPELLBOOK_COLLECTION_ROWS :
                                                             0U;
    if(app->total) {
        uint16_t page_start = ((app->total - 1U) / DND_STORAGE_COLLECTION_CACHE_SIZE) *
                              DND_STORAGE_COLLECTION_CACHE_SIZE;
        uint16_t first = (uint16_t)page_start + 1U;
        if(scroll < first) scroll = first;
    }
    app->scroll = scroll;
}

static bool
    dndspellbook_collection_ensure_list_page(DndSpellbookCollectionApp* app, uint16_t selection) {
    if(!app) return false;
    if(!app->total) return true;

    uint16_t logical = 0U;
    if(selection == 0U) {
        logical = 0U;
    } else if(dndspellbook_collection_selection_is_spell(app, selection)) {
        logical = dndspellbook_collection_selection_spell(selection);
    } else {
        logical = app->total - 1U;
    }

    uint16_t target =
        (logical / DND_STORAGE_COLLECTION_CACHE_SIZE) * DND_STORAGE_COLLECTION_CACHE_SIZE;
    if(target == app->cache_start && logical >= app->cache_start &&
       logical < app->cache_start + app->data.character.spell_count)
        return true;
    return dndspellbook_collection_load_page(app, target);
}

static bool dndspellbook_collection_move_list(DndSpellbookCollectionApp* app, int8_t delta) {
    uint16_t count = dndspellbook_collection_list_count(app);
    if(!count) return false;
    int32_t next = (int32_t)app->selection + delta;
    if(next < 0) next = count - 1U;
    if(next >= count) next = 0;
    if(!dndspellbook_collection_ensure_list_page(app, (uint16_t)next)) {
        dndspellbook_collection_set_status(app, "Read failed");
        return false;
    }
    app->selection = (uint16_t)next;
    app->status[0] = '\0';
    dndspellbook_collection_list_adjust_scroll(app);
    return true;
}

static bool dndspellbook_collection_page_list(DndSpellbookCollectionApp* app, int8_t delta) {
    if(!app || !app->total || !delta) return false;

    uint16_t current = 0U;
    if(dndspellbook_collection_selection_is_spell(app, app->selection))
        current = dndspellbook_collection_selection_spell(app->selection);
    else if(app->selection == dndspellbook_collection_magic_selection(app))
        current = app->total - 1U;

    uint16_t target = (uint16_t)((current / DND_STORAGE_COLLECTION_CACHE_SIZE) *
                                 DND_STORAGE_COLLECTION_CACHE_SIZE);
    if(delta < 0) {
        if(!target) return false;
        target = target >= DND_STORAGE_COLLECTION_CACHE_SIZE ?
                     target - DND_STORAGE_COLLECTION_CACHE_SIZE :
                     0U;
    } else {
        target += DND_STORAGE_COLLECTION_CACHE_SIZE;
        if(target >= app->total) return false;
    }
    if(!dndspellbook_collection_load_page(app, target)) {
        dndspellbook_collection_set_status(app, "Read failed");
        return false;
    }
    app->selection = target + 1U;
    app->status[0] = '\0';
    dndspellbook_collection_list_adjust_scroll(app);
    return true;
}

static uint8_t dndspellbook_collection_detail_count(void) {
    return 18U;
}

static void dndspellbook_collection_format_detail(
    DndSpellbookCollectionApp* app,
    uint8_t field,
    char* out,
    size_t size) {
    DndSpellbookCharacterState* character = &app->data.character;
    uint8_t local = 0U;
    DndSpell* spell = dndspellbook_collection_spell_cached(app, app->record_index, &local);
    if(!spell) {
        dndspellbook_collection_copy(out, size, "Read error");
        return;
    }
    switch(field) {
    case 0:
        snprintf(out, size, "Name: %.31s", spell->name);
        break;
    case 1:
        snprintf(out, size, "Notes: %.31s", spell->detail);
        break;
    case 2:
        snprintf(
            out,
            size,
            "Source class: %s",
            spell->class_index < character->class_count ?
                character->classes[spell->class_index].name :
                "Primary");
        break;
    case 3:
        snprintf(out, size, "Level: %u", spell->level);
        break;
    case 4:
        snprintf(out, size, "Known: %s", character->spell_known[local] ? "Yes" : "No");
        break;
    case 5:
        snprintf(out, size, "Prepared: %s", spell->prepared ? "Yes" : "No");
        break;
    case 6:
        snprintf(
            out,
            size,
            "Always prepared: %s",
            character->spell_always_prepared[local] ? "Yes" : "No");
        break;
    case 7:
        snprintf(out, size, "Ritual: %s", spell->ritual ? "Yes" : "No");
        break;
    case 8:
        snprintf(
            out,
            size,
            "Free casts: %u/%u",
            character->spell_free_casts_current[local],
            character->spell_free_casts_max[local]);
        break;
    case 9:
        snprintf(out, size, "Free casts max: %u", character->spell_free_casts_max[local]);
        break;
    case 10:
        dndspellbook_collection_copy(
            out,
            size,
            character->spell_free_casts_current[local] ? "Use one free cast" :
                                                         "No free casts left");
        break;
    case 11:
        snprintf(out, size, "Stable ID: %.23s", spell->stable_id);
        break;
    case 12:
        snprintf(out, size, "Source: %.23s", spell->source);
        break;
    case 13:
        snprintf(out, size, "School: %.23s", spell->school);
        break;
    case 14:
        snprintf(out, size, "Grant source: %.23s", spell->grant_name);
        break;
    case 15:
        snprintf(out, size, "Grant type: %u", spell->grant_source);
        break;
    case 16:
        dndspellbook_collection_copy(
            out, size, spell->favorite ? "Favorite: Yes" : "Favorite: No");
        break;
    default:
        dndspellbook_collection_copy(out, size, "Delete spell");
        break;
    }
}

static uint8_t dndspellbook_magic_total_level(const DndSpellbookCharacterState* character) {
    if(!character) return 1U;
    uint8_t level = 0U;
    for(uint8_t i = 0U; i < character->class_count && i < DND_MAX_CLASSES; ++i)
        level += character->classes[i].level;
    if(level < 1U) level = 1U;
    if(level > 20U) level = 20U;
    return level;
}

static uint8_t dndspellbook_magic_proficiency_bonus(const DndSpellbookCharacterState* character) {
    uint8_t level = dndspellbook_magic_total_level(character);
    return (uint8_t)(2U + ((level - 1U) / 4U));
}

static int8_t dndspellbook_magic_ability_modifier(int8_t score) {
    int16_t delta = (int16_t)score - 10;
    return delta >= 0 ? (int8_t)(delta / 2) : (int8_t) - (((-delta) + 1) / 2);
}

static bool dndspellbook_magic_has_wizard(const DndSpellbookCharacterState* character) {
    if(!character) return false;
    for(uint8_t i = 0U; i < character->class_count && i < DND_MAX_CLASSES; ++i)
        if(!strcmp(character->classes[i].name, "Wizard") && character->classes[i].level)
            return true;
    return false;
}

static int8_t dndspellbook_magic_attack_modifier(const DndSpellbookCharacterState* character) {
    if(!character) return 0;
    uint8_t ability = character->spellcasting_ability < DND_ABILITY_COUNT ?
                          character->spellcasting_ability :
                          DndAbilityIntelligence;
    return (int8_t)(dndspellbook_magic_ability_modifier(character->ability_scores[ability]) +
                    dndspellbook_magic_proficiency_bonus(character) +
                    character->spell_attack_misc);
}

static int8_t dndspellbook_magic_save_dc(const DndSpellbookCharacterState* character) {
    if(!character) return 8;
    uint8_t ability = character->spellcasting_ability < DND_ABILITY_COUNT ?
                          character->spellcasting_ability :
                          DndAbilityIntelligence;
    return (int8_t)(8 + dndspellbook_magic_ability_modifier(character->ability_scores[ability]) +
                    dndspellbook_magic_proficiency_bonus(character) + character->spell_save_misc);
}

static uint16_t
    dndspellbook_magic_class_knowable(const DndClassLevel* class_level, uint16_t granted_count) {
    if(!class_level || class_level->spellcasting_mode == DndSpellcastingNone) return granted_count;
    uint16_t base = class_level->cantrip_limit;
    if(!strcmp(class_level->name, "Wizard"))
        base += class_level->spellbook_size;
    else
        base += class_level->prepared_limit;
    return base + granted_count;
}

static void dndspellbook_magic_refresh_counts(DndSpellbookCollectionApp* app) {
    if(!app || !app->have_profile) return;
    DndDolphinsSpellClassCounts counts;
    uint16_t total = 0U;
    if(!dndolphins_spells_class_counts(app->storage, app->profile, &counts, &total)) {
        app->magic_counts_valid = 0U;
        dndspellbook_collection_set_status(app, "Spell count read failed");
        return;
    }
    app->magic_known = 0U;
    app->magic_knowable = 0U;
    app->magic_granted = 0U;
    for(uint8_t i = 0U; i < app->data.character.class_count && i < DND_MAX_CLASSES; ++i) {
        app->magic_known += counts.known[i];
        app->magic_granted += counts.granted[i];
        app->magic_knowable +=
            dndspellbook_magic_class_knowable(&app->data.character.classes[i], counts.granted[i]);
    }
    app->magic_counts_valid = 1U;
}

static bool dndspellbook_magic_save(DndSpellbookCollectionApp* app) {
    if(!app || !app->have_profile) return false;
    DndSpellbookProfileProjection projection;
    memset(&projection, 0, sizeof(projection));
    projection.spellcasting_ability = app->data.character.spellcasting_ability;
    projection.spell_attack_misc = app->data.character.spell_attack_misc;
    projection.spell_save_misc = app->data.character.spell_save_misc;
    projection.arcane_recovery_used = app->data.character.arcane_recovery_used;
    memcpy(
        projection.spell_slots_current,
        app->data.character.spell_slots_current,
        sizeof(projection.spell_slots_current));
    memcpy(
        projection.spell_slots_max,
        app->data.character.spell_slots_max,
        sizeof(projection.spell_slots_max));
    if(!dnd_profile_projection_save_spellbook_magic(app->storage, app->profile, &projection)) {
        dndspellbook_collection_set_status(app, "Magic save failed");
        return false;
    }
    return true;
}

static void dndspellbook_collection_draw_magic(Canvas* canvas, DndSpellbookCollectionApp* app) {
    static const char* const abilities[DND_ABILITY_COUNT] = {
        "STR", "DEX", "CON", "INT", "WIS", "CHA"};
    DndSpellbookCharacterState* character = &app->data.character;
    char rows[17][48];
    const char* row_ptrs[17];
    for(uint8_t i = 0U; i < 17U; ++i)
        row_ptrs[i] = rows[i];
    dndspellbook_collection_copy(rows[0], sizeof(rows[0]), "Open Spellbook");
    uint8_t ability = character->spellcasting_ability < DND_ABILITY_COUNT ?
                          character->spellcasting_ability :
                          DndAbilityIntelligence;
    snprintf(rows[1], sizeof(rows[1]), "Casting ability: %s", abilities[ability]);
    snprintf(
        rows[2],
        sizeof(rows[2]),
        "PB +%u Atk %+d DC %d (hold recalc)",
        dndspellbook_magic_proficiency_bonus(character),
        dndspellbook_magic_attack_modifier(character),
        dndspellbook_magic_save_dc(character));
    snprintf(rows[3], sizeof(rows[3]), "Spell attack misc: %+d", character->spell_attack_misc);
    snprintf(rows[4], sizeof(rows[4]), "Spell save misc: %+d", character->spell_save_misc);
    if(app->magic_counts_valid)
        snprintf(
            rows[5],
            sizeof(rows[5]),
            "Known %u / knowable %u (free %u)",
            app->magic_known,
            app->magic_knowable,
            app->magic_granted);
    else
        dndspellbook_collection_copy(rows[5], sizeof(rows[5]), "Spell counts unavailable");
    dndspellbook_collection_copy(rows[6], sizeof(rows[6]), "Slots: <> avail / hold <> max");
    if(!dndspellbook_magic_has_wizard(character))
        dndspellbook_collection_copy(rows[7], sizeof(rows[7]), "Arcane Recovery: no Wizard");
    else
        snprintf(
            rows[7],
            sizeof(rows[7]),
            "Arcane Recovery: %s",
            character->arcane_recovery_used ? "Used" : "Ready after Short Rest");
    for(uint8_t level = 1U; level <= 9U; ++level)
        snprintf(
            rows[level + 7U],
            sizeof(rows[level + 7U]),
            "Level %u slots: %u/%u",
            level,
            character->spell_slots_current[level],
            character->spell_slots_max[level]);
    dndspellbook_collection_draw_header(canvas, app, "Magic & Spells", app->status);
    for(uint8_t row = 0U; row < DNDSPELLBOOK_COLLECTION_ROWS; ++row) {
        uint16_t index = app->scroll + row;
        if(index >= 17U) break;
        dndspellbook_collection_draw_row(canvas, row, index == app->selection, row_ptrs[index]);
    }
}

static void dndspellbook_magic_move(DndSpellbookCollectionApp* app, int8_t delta) {
    int16_t next = (int16_t)app->selection + delta;
    if(next < 0) next = 16;
    if(next > 16) next = 0;
    app->selection = (uint16_t)next;
    if(app->selection < app->scroll) app->scroll = app->selection;
    if(app->selection >= app->scroll + DNDSPELLBOOK_COLLECTION_ROWS)
        app->scroll = app->selection - (DNDSPELLBOOK_COLLECTION_ROWS - 1U);
}

static bool dndspellbook_magic_begin_number(
    DndSpellbookCollectionApp* app,
    uint8_t field,
    const char* header,
    int32_t value,
    int32_t minimum,
    int32_t maximum) {
    dndspellbook_collection_release_text(app);
    if(!app->number_input) {
        app->number_input = number_input_alloc();
        if(!app->number_input) {
            dndspellbook_collection_set_status(app, "Number memory low");
            return false;
        }
        view_dispatcher_add_view(
            app->dispatcher,
            DNDSPELLBOOK_COLLECTION_VIEW_NUMBER,
            number_input_get_view(app->number_input));
    }
    app->number_field = field;
    app->input_active = 1U;
    number_input_set_header_text(app->number_input, header);
    number_input_set_result_callback(
        app->number_input, dndspellbook_collection_number_done, app, value, minimum, maximum);
    view_dispatcher_switch_to_view(app->dispatcher, DNDSPELLBOOK_COLLECTION_VIEW_NUMBER);
    return true;
}

static void dndspellbook_magic_return_to_list(DndSpellbookCollectionApp* app) {
    if(!app) return;
    app->screen = DndSpellbookCollectionScreenList;
    if(app->magic_direct_launch) {
        app->selection = 0U;
        app->scroll = 0U;
    } else {
        app->selection = app->magic_return_selection;
        app->scroll = app->magic_return_scroll;
    }
    app->magic_direct_launch = 0U;
}

static void dndspellbook_magic_input(DndSpellbookCollectionApp* app, const InputEvent* event) {
    DndSpellbookCharacterState* character = &app->data.character;
    bool move = event->type == InputTypeShort || event->type == InputTypeRepeat;
    if(move && event->key == InputKeyUp) {
        dndspellbook_magic_move(app, -1);
        return;
    }
    if(move && event->key == InputKeyDown) {
        dndspellbook_magic_move(app, 1);
        return;
    }
    if(move && (event->key == InputKeyLeft || event->key == InputKeyRight)) {
        int8_t delta = event->key == InputKeyRight ? 1 : -1;
        if(app->selection == 1U) {
            int16_t ability = (int16_t)character->spellcasting_ability + delta;
            if(ability < 0) ability = DndAbilityCharisma;
            if(ability > DndAbilityCharisma) ability = DndAbilityStrength;
            character->spellcasting_ability = (uint8_t)ability;
        } else if(app->selection == 3U) {
            int16_t value = character->spell_attack_misc + delta;
            if(value < -20) value = -20;
            if(value > 20) value = 20;
            character->spell_attack_misc = (int8_t)value;
        } else if(app->selection == 4U) {
            int16_t value = character->spell_save_misc + delta;
            if(value < -20) value = -20;
            if(value > 20) value = 20;
            character->spell_save_misc = (int8_t)value;
        } else if(app->selection >= 8U && app->selection <= 16U) {
            uint8_t level = (uint8_t)(app->selection - 7U);
            int16_t value = (int16_t)character->spell_slots_current[level] + delta;
            if(value < 0) value = 0;
            if(value > character->spell_slots_max[level])
                value = character->spell_slots_max[level];
            character->spell_slots_current[level] = (uint8_t)value;
        } else {
            return;
        }
        (void)dndspellbook_magic_save(app);
        return;
    }
    if(event->type == InputTypeLong &&
       (event->key == InputKeyLeft || event->key == InputKeyRight) && app->selection >= 8U &&
       app->selection <= 16U) {
        int8_t delta = event->key == InputKeyRight ? 1 : -1;
        uint8_t level = (uint8_t)(app->selection - 7U);
        int16_t value = (int16_t)character->spell_slots_max[level] + delta;
        if(value < 0) value = 0;
        if(value > 20) value = 20;
        character->spell_slots_max[level] = (uint8_t)value;
        if(character->spell_slots_current[level] > character->spell_slots_max[level])
            character->spell_slots_current[level] = character->spell_slots_max[level];
        (void)dndspellbook_magic_save(app);
        return;
    }
    if(event->type == InputTypeLong && event->key == InputKeyOk) {
        if(app->selection == 2U) {
            dndolphins_spells_recalculate_shared_slots(
                character->classes,
                character->class_count,
                character->spell_slots_current,
                character->spell_slots_max);
            (void)dndspellbook_magic_save(app);
            dndspellbook_collection_set_status(app, "Class slots recalculated");
        } else if(app->selection == 3U) {
            (void)dndspellbook_magic_begin_number(
                app, 100U, "Spell attack misc", character->spell_attack_misc, -20, 20);
        } else if(app->selection == 4U) {
            (void)dndspellbook_magic_begin_number(
                app, 101U, "Spell save DC misc", character->spell_save_misc, -20, 20);
        } else if(app->selection >= 8U && app->selection <= 16U) {
            uint8_t level = (uint8_t)(app->selection - 7U);
            (void)dndspellbook_magic_begin_number(
                app,
                (uint8_t)(120U + level),
                "Maximum spell slots",
                character->spell_slots_max[level],
                0,
                20);
        }
        return;
    }
    if(event->type == InputTypeShort && event->key == InputKeyOk) {
        if(app->selection == 0U) {
            dndspellbook_magic_return_to_list(app);
        } else if(app->selection == 7U) {
            dndspellbook_collection_set_status(
                app,
                !dndspellbook_magic_has_wizard(character) ? "Arcane Recovery: no Wizard" :
                character->arcane_recovery_used           ? "Arcane Recovery already used" :
                                                            "Use Short Rest in DNDCombat");
        } else if(app->selection >= 8U && app->selection <= 16U) {
            uint8_t level = (uint8_t)(app->selection - 7U);
            (void)dndspellbook_magic_begin_number(
                app,
                (uint8_t)(110U + level),
                "Available spell slots",
                character->spell_slots_current[level],
                0,
                character->spell_slots_max[level]);
        }
    }
}

typedef struct {
    const char* term;
    uint16_t* matches;
    uint16_t capacity;
    uint16_t count;
} DndSpellbookSearchContext;

static bool dndspellbook_collection_search_count_visitor(
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
    DndSpellbookSearchContext* search = context;
    if(search && spell && dndspellbook_collection_contains_ci(spell->name, search->term) &&
       search->count < UINT16_MAX)
        ++search->count;
    return true;
}

static bool dndspellbook_collection_search_collect_visitor(
    uint16_t logical_index,
    const DndSpell* spell,
    uint8_t known,
    uint8_t always_prepared,
    uint8_t free_casts_current,
    uint8_t free_casts_max,
    void* context) {
    UNUSED(known);
    UNUSED(always_prepared);
    UNUSED(free_casts_current);
    UNUSED(free_casts_max);
    DndSpellbookSearchContext* search = context;
    if(search && spell && dndspellbook_collection_contains_ci(spell->name, search->term) &&
       search->count < search->capacity)
        search->matches[search->count++] = logical_index;
    return true;
}

static void dndspellbook_collection_clear_search(DndSpellbookCollectionApp* app) {
    if(!app) return;
    free(app->search_matches);
    app->search_matches = NULL;
    app->search_match_count = 0U;
    app->search_selection = 0U;
    app->search_scroll = 0U;
}

static bool dndspellbook_collection_build_search(DndSpellbookCollectionApp* app) {
    if(!app || strlen(app->search_term) < DNDSPELLBOOK_COLLECTION_SEARCH_MIN_CHARS) return false;
    dndspellbook_collection_clear_search(app);
    DndSpellbookSearchContext count = {.term = app->search_term};
    if(!dnd_storage_visit_spells(
           app->storage, app->profile, dndspellbook_collection_search_count_visitor, &count, NULL))
        return false;
    if(!count.count) return true;
    if(count.count > 512U) count.count = 512U;
    app->search_matches = malloc((size_t)count.count * sizeof(uint16_t));
    if(!app->search_matches) return false;
    DndSpellbookSearchContext collect = {
        .term = app->search_term,
        .matches = app->search_matches,
        .capacity = count.count,
        .count = 0U,
    };
    if(!dnd_storage_visit_spells(
           app->storage,
           app->profile,
           dndspellbook_collection_search_collect_visitor,
           &collect,
           NULL)) {
        dndspellbook_collection_clear_search(app);
        return false;
    }
    app->search_match_count = collect.count;
    return true;
}

static void dndspellbook_collection_draw_search(Canvas* canvas, DndSpellbookCollectionApp* app) {
    char title[32];
    snprintf(title, sizeof(title), "Search: %.20s", app->search_term);
    dndspellbook_collection_draw_header(canvas, app, title, app->status);
    if(!app->search_match_count) {
        dndspellbook_collection_draw_row(canvas, 0U, false, "No matching spells");
        return;
    }
    for(uint8_t row = 0U; row < DNDSPELLBOOK_COLLECTION_ROWS; ++row) {
        uint16_t result = app->search_scroll + row;
        if(result >= app->search_match_count) break;
        uint16_t logical = app->search_matches[result];
        uint8_t local = 0U;
        DndSpell* spell = dndspellbook_collection_spell(app, logical, &local);
        char text[52];
        char source_tag[4] = "";
        if(spell) {
            dndspellbook_collection_source_tag(&app->data.character, spell, source_tag);
            char status_mark = app->data.character.spell_always_prepared[local] ? 'A' :
                               spell->prepared                                  ? 'P' :
                               app->data.character.spell_known[local]           ? 'K' :
                                                                                  '-';
            snprintf(text, sizeof(text), "%c L%u %.42s", status_mark, spell->level, spell->name);
        } else {
            dndspellbook_collection_copy(text, sizeof(text), "Read error");
        }
        dndspellbook_collection_draw_spell_row(
            canvas, row, result == app->search_selection, text, source_tag);
    }
}

static void dndspellbook_collection_draw_list(Canvas* canvas, DndSpellbookCollectionApp* app) {
    char title[32];
    char page_status[24];
    snprintf(title, sizeof(title), "Spellbook");
    const char* header_status = app->status;
    if(!header_status[0] && app->total > DND_STORAGE_COLLECTION_CACHE_SIZE) {
        uint16_t page = app->cache_start / DND_STORAGE_COLLECTION_CACHE_SIZE + 1U;
        snprintf(page_status, sizeof(page_status), "Pg%u<>", page);
        header_status = page_status;
    }
    dndspellbook_collection_draw_header(canvas, app, title, header_status);
    uint16_t count = dndspellbook_collection_list_count(app);
    uint16_t magic = dndspellbook_collection_magic_selection(app);
    for(uint8_t row = 0U; row < DNDSPELLBOOK_COLLECTION_ROWS; ++row) {
        uint16_t index = app->scroll + row;
        if(index >= count) break;
        char text[52];
        char source_tag[4] = "";
        bool spell_row = false;
        if(index == 0U) {
            dndspellbook_collection_copy(text, sizeof(text), "+ Add New");
        } else if(index == magic) {
            dndspellbook_collection_copy(text, sizeof(text), "Magic & Spells");
        } else if(dndspellbook_collection_selection_is_spell(app, index)) {
            uint16_t logical = dndspellbook_collection_selection_spell(index);
            if(logical < app->cache_start ||
               logical >= app->cache_start + DND_STORAGE_COLLECTION_CACHE_SIZE) {
                dndspellbook_collection_copy(text, sizeof(text), "Page unavailable");
            } else {
                uint8_t local = dndspellbook_collection_local(app, logical);
                if(local < app->data.character.spell_count) {
                    DndSpell* spell = &app->data.character.spells[local];
                    dndspellbook_collection_source_tag(&app->data.character, spell, source_tag);
                    spell_row = true;
                    char status_mark = app->data.character.spell_always_prepared[local] ? 'A' :
                                       spell->prepared                                  ? 'P' :
                                       app->data.character.spell_known[local]           ? 'K' :
                                                                                          '-';
                    char free_mark = app->data.character.spell_free_casts_current[local] ? 'F' :
                                                                                           ' ';
                    snprintf(
                        text,
                        sizeof(text),
                        "%c%c L%u %.41s",
                        status_mark,
                        free_mark,
                        spell->level,
                        spell->name);
                } else {
                    dndspellbook_collection_copy(text, sizeof(text), "Read error");
                }
            }
        } else {
            dndspellbook_collection_copy(text, sizeof(text), "Unavailable");
        }
        if(app->action_ack_active && index == app->action_ack_selection) {
            char confirmed[52];
            snprintf(confirmed, sizeof(confirmed), "[X] %.46s", text);
            dndspellbook_collection_copy(text, sizeof(text), confirmed);
        }
        if(spell_row)
            dndspellbook_collection_draw_spell_row(
                canvas, row, index == app->selection, text, source_tag);
        else
            dndspellbook_collection_draw_row(canvas, row, index == app->selection, text);
    }
}

static void dndspellbook_collection_draw_detail(Canvas* canvas, DndSpellbookCollectionApp* app) {
    dndspellbook_collection_draw_header(canvas, app, "Spell Editor", app->status);
    uint8_t count = dndspellbook_collection_detail_count();
    for(uint8_t row = 0U; row < DNDSPELLBOOK_COLLECTION_ROWS; ++row) {
        uint16_t field = app->detail_scroll + row;
        if(field >= count) break;
        char text[64];
        dndspellbook_collection_format_detail(app, (uint8_t)field, text, sizeof(text));
        dndspellbook_collection_draw_row(canvas, row, field == app->detail_selection, text);
    }
}

static void dndspellbook_collection_draw_catalog(Canvas* canvas, DndSpellbookCollectionApp* app) {
    char title[48];
    snprintf(
        title,
        sizeof(title),
        "Spells: %s",
        dndspellbook_collection_filter_class_name(app->filter_class));
    char page[16];
    snprintf(
        page,
        sizeof(page),
        "Page %u%s <>",
        (unsigned)(app->catalog_page_start / DNDSPELLBOOK_COLLECTION_CATALOG_PAGE + 1U),
        app->catalog_has_more ? "+" : "");
    dndspellbook_collection_draw_header(canvas, app, title, app->status[0] ? app->status : page);
    if(!app->catalog_count) {
        dndspellbook_collection_draw_row(canvas, 0U, false, "No matching entries");
        return;
    }
    uint16_t scroll = app->selection > 4U ? app->selection - 4U : 0U;
    for(uint8_t row = 0U; row < DNDSPELLBOOK_COLLECTION_ROWS; ++row) {
        uint16_t index = scroll + row;
        if(index >= app->catalog_count) break;
        DndSpellbookCatalogEntry* entry = &app->catalog[index];
        char text[56];
        snprintf(text, sizeof(text), "L%u %.49s", entry->level, entry->name);
        dndspellbook_collection_draw_row(canvas, row, index == app->selection, text);
    }
}

static void dndspellbook_collection_draw_filters(Canvas* canvas, DndSpellbookCollectionApp* app) {
    char rows[7][48];
    if(app->filter_level < 0)
        dndspellbook_collection_copy(rows[0], sizeof(rows[0]), "Level: Any");
    else if(app->filter_level == 0)
        dndspellbook_collection_copy(rows[0], sizeof(rows[0]), "Level: Cantrip");
    else
        snprintf(rows[0], sizeof(rows[0]), "Level: %d", app->filter_level);
    snprintf(
        rows[1],
        sizeof(rows[1]),
        "Class: %s",
        dndspellbook_collection_filter_class_name(app->filter_class));
    snprintf(rows[2], sizeof(rows[2]), "Ritual: %s", app->filter_ritual ? "Only" : "Any");
    snprintf(
        rows[3],
        sizeof(rows[3]),
        "School: %s",
        dndspellbook_collection_school_names[app->filter_school]);
    snprintf(
        rows[4],
        sizeof(rows[4]),
        "Source: %s",
        dndspellbook_collection_source_names[app->filter_source]);
    snprintf(
        rows[5],
        sizeof(rows[5]),
        "Status: %s",
        app->filter_status == 1U ? "Prepared" :
        app->filter_status == 2U ? "Known" :
        app->filter_status == 3U ? "Always" :
                                   "Any");
    snprintf(
        rows[6],
        sizeof(rows[6]),
        "Eligibility: %s",
        app->filter_show_all ? "All Spells" : "Allowed");
    dndspellbook_collection_draw_header(canvas, app, "Spell Filters", app->status);
    uint8_t scroll = app->filter_selection > 4U ? app->filter_selection - 4U : 0U;
    for(uint8_t row = 0U; row < DNDSPELLBOOK_COLLECTION_ROWS; ++row) {
        uint8_t index = scroll + row;
        if(index >= 7U) break;
        dndspellbook_collection_draw_row(canvas, row, index == app->filter_selection, rows[index]);
    }
}

static void dndspellbook_collection_draw(Canvas* canvas, void* model) {
    /* View draw callbacks receive the model buffer, not view context. */
    if(!model) return;
    DndSpellbookCollectionApp* app = *(DndSpellbookCollectionApp**)model;
    if(!app) return;
    canvas_clear(canvas);
    switch(app->screen) {
    case DndSpellbookCollectionScreenNoCharacter:
        dndspellbook_collection_draw_header(canvas, app, "DNDSpellbook", NULL);
        dndspellbook_collection_draw_row(canvas, 0U, false, "No character");
        dndspellbook_collection_draw_row(canvas, 1U, true, "OK: Open DNDolphins");
        break;
    case DndSpellbookCollectionScreenList:
        dndspellbook_collection_draw_list(canvas, app);
        break;
    case DndSpellbookCollectionScreenSearch:
        dndspellbook_collection_draw_search(canvas, app);
        break;
    case DndSpellbookCollectionScreenDetail:
        dndspellbook_collection_draw_detail(canvas, app);
        break;
    case DndSpellbookCollectionScreenCatalog:
        dndspellbook_collection_draw_catalog(canvas, app);
        break;
    case DndSpellbookCollectionScreenFilters:
        dndspellbook_collection_draw_filters(canvas, app);
        break;
    case DndSpellbookCollectionScreenMagic:
        dndspellbook_collection_draw_magic(canvas, app);
        break;
    }
}

static void dndspellbook_collection_release_text(DndSpellbookCollectionApp* app) {
    if(!app->text_input || app->input_active) return;
    view_dispatcher_remove_view(app->dispatcher, DNDSPELLBOOK_COLLECTION_VIEW_TEXT);
    text_input_free(app->text_input);
    app->text_input = NULL;
}

static void dndspellbook_collection_release_number(DndSpellbookCollectionApp* app) {
    if(!app->number_input || app->input_active) return;
    view_dispatcher_remove_view(app->dispatcher, DNDSPELLBOOK_COLLECTION_VIEW_NUMBER);
    number_input_free(app->number_input);
    app->number_input = NULL;
}

static void dndspellbook_collection_text_done(void* context);

static void dndspellbook_collection_begin_text(
    DndSpellbookCollectionApp* app,
    DndSpellbookCollectionEdit edit,
    const char* header,
    const char* initial) {
    dndspellbook_collection_release_number(app);
    if(!app->text_input) {
        app->text_input = text_input_alloc();
        if(!app->text_input) {
            dndspellbook_collection_set_status(app, "Text memory low");
            return;
        }
        view_dispatcher_add_view(
            app->dispatcher,
            DNDSPELLBOOK_COLLECTION_VIEW_TEXT,
            text_input_get_view(app->text_input));
    }
    app->edit = edit;
    app->input_active = 1U;
    dndspellbook_collection_copy(app->edit_buffer, sizeof(app->edit_buffer), initial);
    text_input_reset(app->text_input);
    text_input_set_header_text(app->text_input, header);
    text_input_set_result_callback(
        app->text_input,
        dndspellbook_collection_text_done,
        app,
        app->edit_buffer,
        sizeof(app->edit_buffer),
        false);
    view_dispatcher_switch_to_view(app->dispatcher, DNDSPELLBOOK_COLLECTION_VIEW_TEXT);
}

static bool dndspellbook_collection_number_spec(
    DndSpellbookCollectionApp* app,
    uint8_t field,
    const char** header,
    int32_t* value,
    int32_t* minimum,
    int32_t* maximum) {
    *header = NULL;
    *value = 0;
    *minimum = 0;
    *maximum = 999;
    uint8_t local = 0U;
    DndSpell* spell = dndspellbook_collection_spell(app, app->record_index, &local);
    if(!spell) return false;
    if(field == 3U) {
        *header = "Spell level";
        *value = spell->level;
        *maximum = 9;
    } else if(field == 8U) {
        *header = "Free casts current";
        *value = app->data.character.spell_free_casts_current[local];
        *maximum = app->data.character.spell_free_casts_max[local];
    } else if(field == 9U) {
        *header = "Free casts maximum";
        *value = app->data.character.spell_free_casts_max[local];
        *maximum = 20;
    } else {
        return false;
    }
    return true;
}

static bool dndspellbook_collection_begin_number(DndSpellbookCollectionApp* app, uint8_t field) {
    const char* header = NULL;
    int32_t value = 0;
    int32_t minimum = 0;
    int32_t maximum = 0;
    if(!dndspellbook_collection_number_spec(app, field, &header, &value, &minimum, &maximum))
        return false;
    dndspellbook_collection_release_text(app);
    if(!app->number_input) {
        app->number_input = number_input_alloc();
        if(!app->number_input) {
            dndspellbook_collection_set_status(app, "Number memory low");
            return true;
        }
        view_dispatcher_add_view(
            app->dispatcher,
            DNDSPELLBOOK_COLLECTION_VIEW_NUMBER,
            number_input_get_view(app->number_input));
    }
    app->number_field = field;
    app->input_active = 1U;
    number_input_set_header_text(app->number_input, header);
    number_input_set_result_callback(
        app->number_input, dndspellbook_collection_number_done, app, value, minimum, maximum);
    view_dispatcher_switch_to_view(app->dispatcher, DNDSPELLBOOK_COLLECTION_VIEW_NUMBER);
    return true;
}

static void
    dndspellbook_collection_adjust(DndSpellbookCollectionApp* app, uint8_t field, int8_t delta) {
    DndSpellbookCharacterState* character = &app->data.character;
    uint8_t local = 0U;
    DndSpell* spell = dndspellbook_collection_spell(app, app->record_index, &local);
    if(!spell) return;
    if(field == 2U) {
        if(!character->class_count) return;
        int16_t next = (int16_t)spell->class_index + delta;
        if(next < 0) next = (int16_t)character->class_count - 1;
        if(next >= (int16_t)character->class_count) next = 0;
        spell->class_index = (uint8_t)next;
    } else if(field == 3U) {
        spell->level = dndspellbook_collection_clamp_u8((int16_t)spell->level + delta, 9U);
        app->sort_pending = 1U;
    } else if(field == 4U) {
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
        character->spell_free_casts_current[local] = dndspellbook_collection_clamp_u8(
            (int16_t)character->spell_free_casts_current[local] + delta,
            character->spell_free_casts_max[local]);
    } else if(field == 9U) {
        character->spell_free_casts_max[local] = dndspellbook_collection_clamp_u8(
            (int16_t)character->spell_free_casts_max[local] + delta, 20U);
        if(character->spell_free_casts_current[local] > character->spell_free_casts_max[local])
            character->spell_free_casts_current[local] = character->spell_free_casts_max[local];
    } else if(field == 15U) {
        int16_t source = (int16_t)spell->grant_source + delta;
        if(source < 0) source = DndGrantSourceCount - 1U;
        if(source >= DndGrantSourceCount) source = 0;
        spell->grant_source = (uint8_t)source;
    } else {
        return;
    }
    (void)dndspellbook_collection_save_page(app);
}

static void dndspellbook_collection_text_done(void* context) {
    DndSpellbookCollectionApp* app = context;
    if(!app) return;
    if(app->edit == DndSpellbookCollectionEditSearch) {
        app->input_active = 0U;
        app->edit = DndSpellbookCollectionEditNone;
        if(strlen(app->edit_buffer) < DNDSPELLBOOK_COLLECTION_SEARCH_MIN_CHARS) {
            app->search_term[0] = '\0';
            dndspellbook_collection_clear_search(app);
            dndspellbook_collection_set_status(app, "Search needs 3+ chars");
            app->screen = app->filter_return_screen == DndSpellbookCollectionScreenCatalog ?
                              DndSpellbookCollectionScreenCatalog :
                              DndSpellbookCollectionScreenList;
        } else {
            dndspellbook_collection_copy(
                app->search_term, sizeof(app->search_term), app->edit_buffer);
            if(app->filter_return_screen == DndSpellbookCollectionScreenCatalog) {
                dndspellbook_collection_reset_catalog_offsets(app);
                app->catalog_page_start = 0U;
                app->selection = 0U;
                app->screen = DndSpellbookCollectionScreenCatalog;
                (void)dndspellbook_collection_load_catalog(app);
            } else if(!dndspellbook_collection_build_search(app)) {
                dndspellbook_collection_set_status(app, "Search failed");
                app->screen = DndSpellbookCollectionScreenList;
            } else {
                app->screen = DndSpellbookCollectionScreenSearch;
                app->search_selection = app->search_scroll = 0U;
                app->status[0] = '\0';
            }
        }
        view_dispatcher_switch_to_view(app->dispatcher, DNDSPELLBOOK_COLLECTION_VIEW_MAIN);
        dndspellbook_collection_redraw(app);
        return;
    }
    DndSpell* spell = dndspellbook_collection_spell(app, app->record_index, NULL);
    if(spell) {
        if(app->edit == DndSpellbookCollectionEditName)
            dndspellbook_collection_copy(spell->name, sizeof(spell->name), app->edit_buffer);
        else if(app->edit == DndSpellbookCollectionEditDetail)
            dndspellbook_collection_copy(spell->detail, sizeof(spell->detail), app->edit_buffer);
        else if(app->edit == DndSpellbookCollectionEditStableId)
            dndspellbook_collection_copy(
                spell->stable_id, sizeof(spell->stable_id), app->edit_buffer);
        else if(app->edit == DndSpellbookCollectionEditSource)
            dndspellbook_collection_copy(spell->source, sizeof(spell->source), app->edit_buffer);
        else if(app->edit == DndSpellbookCollectionEditSchool)
            dndspellbook_collection_copy(spell->school, sizeof(spell->school), app->edit_buffer);
        else if(app->edit == DndSpellbookCollectionEditGrantName)
            dndspellbook_collection_copy(
                spell->grant_name, sizeof(spell->grant_name), app->edit_buffer);
    }
    if(app->edit == DndSpellbookCollectionEditName) app->sort_pending = 1U;
    app->input_active = 0U;
    app->edit = DndSpellbookCollectionEditNone;
    (void)dndspellbook_collection_save_page(app);
    view_dispatcher_switch_to_view(app->dispatcher, DNDSPELLBOOK_COLLECTION_VIEW_MAIN);
    dndspellbook_collection_redraw(app);
}

static void dndspellbook_collection_number_done(void* context, int32_t number) {
    DndSpellbookCollectionApp* app = context;
    if(!app) return;
    if(app->number_field >= 100U) {
        if(app->number_field == 100U)
            app->data.character.spell_attack_misc = (int8_t)number;
        else if(app->number_field == 101U)
            app->data.character.spell_save_misc = (int8_t)number;
        else if(app->number_field >= 111U && app->number_field <= 119U) {
            uint8_t level = (uint8_t)(app->number_field - 110U);
            app->data.character.spell_slots_current[level] = (uint8_t)number;
        } else if(app->number_field >= 121U && app->number_field <= 129U) {
            uint8_t level = (uint8_t)(app->number_field - 120U);
            app->data.character.spell_slots_max[level] = (uint8_t)number;
            if(app->data.character.spell_slots_current[level] > (uint8_t)number)
                app->data.character.spell_slots_current[level] = (uint8_t)number;
        }
        app->input_active = 0U;
        (void)dndspellbook_magic_save(app);
        view_dispatcher_switch_to_view(app->dispatcher, DNDSPELLBOOK_COLLECTION_VIEW_MAIN);
        dndspellbook_collection_redraw(app);
        return;
    }
    uint8_t local = 0U;
    DndSpell* spell = dndspellbook_collection_spell(app, app->record_index, &local);
    if(spell) {
        if(app->number_field == 3U) {
            spell->level = (uint8_t)number;
            app->sort_pending = 1U;
        } else if(app->number_field == 8U) {
            app->data.character.spell_free_casts_current[local] = (uint8_t)number;
        } else if(app->number_field == 9U) {
            app->data.character.spell_free_casts_max[local] = (uint8_t)number;
            if(app->data.character.spell_free_casts_current[local] > (uint8_t)number)
                app->data.character.spell_free_casts_current[local] = (uint8_t)number;
        }
    }
    app->input_active = 0U;
    (void)dndspellbook_collection_save_page(app);
    view_dispatcher_switch_to_view(app->dispatcher, DNDSPELLBOOK_COLLECTION_VIEW_MAIN);
    dndspellbook_collection_redraw(app);
}

static void dndspellbook_collection_detail_ok(DndSpellbookCollectionApp* app) {
    uint8_t field = app->detail_selection;
    uint8_t local = 0U;
    DndSpell* spell = dndspellbook_collection_spell(app, app->record_index, &local);
    if(!spell) return;
    if(field == 0U)
        dndspellbook_collection_open_catalog(app);
    else if(field == 1U)
        dndspellbook_collection_begin_text(
            app, DndSpellbookCollectionEditDetail, "Spell notes", spell->detail);
    else if(field < 10U)
        dndspellbook_collection_adjust(app, field, 1);
    else if(field == 10U) {
        if(app->data.character.spell_free_casts_current[local]) {
            --app->data.character.spell_free_casts_current[local];
            (void)dndspellbook_collection_save_page(app);
            dndspellbook_collection_set_status(app, "Free cast used");
        } else {
            dndspellbook_collection_set_status(app, "No free casts left");
        }
    } else if(field == 11U)
        dndspellbook_collection_begin_text(
            app, DndSpellbookCollectionEditStableId, "Stable ID", spell->stable_id);
    else if(field == 12U)
        dndspellbook_collection_begin_text(
            app, DndSpellbookCollectionEditSource, "Spell source", spell->source);
    else if(field == 13U)
        dndspellbook_collection_begin_text(
            app, DndSpellbookCollectionEditSchool, "Spell school", spell->school);
    else if(field == 14U)
        dndspellbook_collection_begin_text(
            app, DndSpellbookCollectionEditGrantName, "Grant source name", spell->grant_name);
    else if(field == 15U)
        dndspellbook_collection_adjust(app, field, 1);
    else if(field == 16U) {
        spell->favorite = !spell->favorite;
        if(dndspellbook_collection_save_page(app))
            dndspellbook_collection_set_status(
                app, spell->favorite ? "Favorite added" : "Favorite removed");
        else
            dndspellbook_collection_set_status(app, "Favorite update failed");
    } else
        (void)dndspellbook_collection_delete_current(app);
}

static void dndspellbook_collection_detail_hold_ok(DndSpellbookCollectionApp* app) {
    if(dndspellbook_collection_begin_number(app, app->detail_selection)) return;
    if(app->detail_selection != 0U) return;
    DndSpell* spell = dndspellbook_collection_spell(app, app->record_index, NULL);
    if(spell)
        dndspellbook_collection_begin_text(
            app, DndSpellbookCollectionEditName, "Custom spell", spell->name);
}

static void dndspellbook_collection_filter_adjust(DndSpellbookCollectionApp* app, int8_t delta) {
    switch(app->filter_selection) {
    case 0: {
        int16_t next = app->filter_level + delta;
        if(next < -1) next = 9;
        if(next > 9) next = -1;
        app->filter_level = (int8_t)next;
        break;
    }
    case 1: {
        /* Character Classes is the default, followed by Any Class and every
           supported catalog class. This selector is intentionally independent
           of the character's own class array. */
        int16_t next = -1;
        if(app->filter_class == DNDSPELLBOOK_COLLECTION_FILTER_ANY_CLASS)
            next = 0;
        else if(app->filter_class < DNDSPELLBOOK_COLLECTION_CLASS_COUNT)
            next = (int16_t)app->filter_class + 1;
        next += delta;
        if(next < -1) next = DNDSPELLBOOK_COLLECTION_CLASS_COUNT;
        if(next > (int16_t)DNDSPELLBOOK_COLLECTION_CLASS_COUNT) next = -1;
        if(next < 0)
            app->filter_class = DNDSPELLBOOK_COLLECTION_FILTER_CHARACTER_CLASSES;
        else if(next == 0)
            app->filter_class = DNDSPELLBOOK_COLLECTION_FILTER_ANY_CLASS;
        else
            app->filter_class = (uint8_t)(next - 1);
        if(!dndspellbook_collection_use_all_catalog(app) && app->filter_class == 0U)
            app->filter_class = delta > 0 ? 1U : DNDSPELLBOOK_COLLECTION_FILTER_ANY_CLASS;
        break;
    }
    case 2:
        app->filter_ritual = !app->filter_ritual;
        break;
    case 3: {
        int16_t next = (int16_t)app->filter_school + delta;
        if(next < 0) next = 8;
        if(next > 8) next = 0;
        app->filter_school = (uint8_t)next;
        break;
    }
    case 4: {
        uint8_t maximum = dndspellbook_collection_use_all_catalog(app) ?
                              DndSpellbookSourceCount - 1U :
                              DndSpellbookSourceCore;
        int16_t next = (int16_t)app->filter_source + delta;
        if(next < 0) next = maximum;
        if(next > maximum) next = 0;
        app->filter_source = (uint8_t)next;
        break;
    }
    case 5: {
        int16_t next = (int16_t)app->filter_status + delta;
        if(next < 0) next = 3;
        if(next > 3) next = 0;
        app->filter_status = (uint8_t)next;
        app->status_prefilter_valid = 0U;
        break;
    }
    default:
        app->filter_show_all = !app->filter_show_all;
        break;
    }
}

static bool dndspellbook_collection_input(InputEvent* event, void* context) {
    DndSpellbookCollectionApp* app = context;
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
        if(app->screen == DndSpellbookCollectionScreenNoCharacter ||
           app->screen == DndSpellbookCollectionScreenList) {
            app->return_to_dnd = 1U;
            view_dispatcher_stop(app->dispatcher);
            return true;
        } else if(app->screen == DndSpellbookCollectionScreenSearch) {
            dndspellbook_collection_clear_search(app);
            app->search_term[0] = '\0';
            app->screen = DndSpellbookCollectionScreenList;
            app->status[0] = '\0';
        } else if(app->screen == DndSpellbookCollectionScreenMagic) {
            if(app->magic_direct_launch) {
                app->return_to_dnd = 1U;
                view_dispatcher_stop(app->dispatcher);
                return true;
            }
            dndspellbook_magic_return_to_list(app);
        } else if(app->screen == DndSpellbookCollectionScreenDetail) {
            if(app->detail_return_search) {
                app->detail_return_search = 0U;
                app->screen = DndSpellbookCollectionScreenSearch;
            } else {
                app->screen = DndSpellbookCollectionScreenList;
                if(app->sort_pending)
                    (void)dndspellbook_collection_sort_and_reload(app);
                else
                    dndspellbook_collection_focus_list(app, app->record_index);
            }
        } else if(app->screen == DndSpellbookCollectionScreenCatalog) {
            app->search_term[0] = '\0';
            app->screen = DndSpellbookCollectionScreenDetail;
        } else if(app->screen == DndSpellbookCollectionScreenFilters) {
            app->screen = app->filter_return_screen;
            if(app->screen == DndSpellbookCollectionScreenCatalog) {
                dndspellbook_collection_reset_catalog_offsets(app);
                app->catalog_page_start = 0U;
                app->selection = 0U;
                (void)dndspellbook_collection_load_catalog(app);
            }
        }
        dndspellbook_collection_redraw(app);
        return true;
    }

    if(app->screen == DndSpellbookCollectionScreenNoCharacter) {
        if(event->key == InputKeyOk && event->type == InputTypeShort) {
            app->return_to_dnd = 1U;
            view_dispatcher_stop(app->dispatcher);
        }
        return true;
    }

    if(app->screen == DndSpellbookCollectionScreenList) {
        uint16_t magic = dndspellbook_collection_magic_selection(app);
        if(event->type == InputTypeLong && event->key == InputKeyDown) {
            app->filter_return_screen = DndSpellbookCollectionScreenList;
            dndspellbook_collection_begin_text(
                app, DndSpellbookCollectionEditSearch, "Search (3+ chars)", "");
        } else if(event->type == InputTypeLong && event->key == InputKeyUp) {
            app->filter_return_screen = DndSpellbookCollectionScreenList;
            app->filter_selection = 0U;
            app->screen = DndSpellbookCollectionScreenFilters;
        } else if(move && event->key == InputKeyUp)
            (void)dndspellbook_collection_move_list(app, -1);
        else if(move && event->key == InputKeyDown)
            (void)dndspellbook_collection_move_list(app, 1);
        else if(event->type == InputTypeShort && event->key == InputKeyLeft)
            (void)dndspellbook_collection_page_list(app, -1);
        else if(event->type == InputTypeShort && event->key == InputKeyRight)
            (void)dndspellbook_collection_page_list(app, 1);
        else if(
            event->key == InputKeyOk &&
            (event->type == InputTypeShort || event->type == InputTypeLong) &&
            app->selection == 0U)
            (void)dndspellbook_collection_add_blank(app);
        else if(event->key == InputKeyOk && event->type == InputTypeShort && app->selection == magic) {
            app->magic_direct_launch = 0U;
            app->magic_return_selection = app->selection;
            app->magic_return_scroll = app->scroll;
            dndspellbook_magic_refresh_counts(app);
            app->screen = DndSpellbookCollectionScreenMagic;
            app->selection = 0U;
            app->scroll = 0U;
        } else if(
            event->key == InputKeyOk && event->type == InputTypeLong &&
            dndspellbook_collection_selection_is_spell(app, app->selection)) {
            uint16_t logical = dndspellbook_collection_selection_spell(app->selection);
            uint8_t local = 0U;
            DndSpell* spell = dndspellbook_collection_spell(app, logical, &local);
            if(spell) {
                if(app->data.character.spell_always_prepared[local])
                    dndspellbook_collection_set_transient_status(app, "Always prepared");
                else if(!app->data.character.spell_known[local])
                    dndspellbook_collection_set_status(app, "Spell not known");
                else {
                    spell->prepared = !spell->prepared;
                    bool saved = dndspellbook_collection_save_page(app);
                    if(saved) {
                        app->action_ack_active = 1U;
                        app->action_ack_selection = app->selection;
                        dndspellbook_collection_set_transient_status(
                            app, spell->prepared ? "Spell prepared" : "Spell unprepared");
                    }
                }
            }
        } else if(
            event->key == InputKeyOk && event->type == InputTypeShort &&
            dndspellbook_collection_selection_is_spell(app, app->selection)) {
            app->record_index = dndspellbook_collection_selection_spell(app->selection);
            if(dndspellbook_collection_prepare_record(app, app->record_index)) {
                app->detail_selection = app->detail_scroll = 0U;
                app->detail_return_search = 0U;
                app->screen = DndSpellbookCollectionScreenDetail;
            }
        }
    } else if(app->screen == DndSpellbookCollectionScreenMagic) {
        dndspellbook_magic_input(app, event);
    } else if(app->screen == DndSpellbookCollectionScreenSearch) {
        if(move && event->key == InputKeyUp && app->search_match_count) {
            app->search_selection = app->search_selection ? app->search_selection - 1U :
                                                            app->search_match_count - 1U;
        } else if(move && event->key == InputKeyDown && app->search_match_count) {
            app->search_selection = app->search_selection + 1U < app->search_match_count ?
                                        app->search_selection + 1U :
                                        0U;
        } else if(event->type == InputTypeLong && event->key == InputKeyDown) {
            app->filter_return_screen = DndSpellbookCollectionScreenList;
            dndspellbook_collection_begin_text(
                app, DndSpellbookCollectionEditSearch, "Search (3+ chars)", app->search_term);
            return true;
        } else if(
            event->type == InputTypeShort && event->key == InputKeyOk &&
            app->search_selection < app->search_match_count) {
            app->record_index = app->search_matches[app->search_selection];
            if(dndspellbook_collection_prepare_record(app, app->record_index)) {
                app->detail_selection = app->detail_scroll = 0U;
                app->detail_return_search = 1U;
                app->screen = DndSpellbookCollectionScreenDetail;
            }
        }
        if(app->search_selection < app->search_scroll) app->search_scroll = app->search_selection;
        if(app->search_selection >= app->search_scroll + DNDSPELLBOOK_COLLECTION_ROWS)
            app->search_scroll = app->search_selection - (DNDSPELLBOOK_COLLECTION_ROWS - 1U);
    } else if(app->screen == DndSpellbookCollectionScreenDetail) {
        uint8_t count = dndspellbook_collection_detail_count();
        if(move && event->key == InputKeyUp)
            app->detail_selection = app->detail_selection ? app->detail_selection - 1U :
                                                            count - 1U;
        else if(move && event->key == InputKeyDown)
            app->detail_selection =
                app->detail_selection + 1U < count ? app->detail_selection + 1U : 0U;
        else if(move && (event->key == InputKeyLeft || event->key == InputKeyRight))
            dndspellbook_collection_adjust(
                app, app->detail_selection, event->key == InputKeyRight ? 1 : -1);
        else if(event->key == InputKeyOk && event->type == InputTypeLong)
            dndspellbook_collection_detail_hold_ok(app);
        else if(event->key == InputKeyOk && event->type == InputTypeShort)
            dndspellbook_collection_detail_ok(app);
        if(app->detail_selection < app->detail_scroll) app->detail_scroll = app->detail_selection;
        if(app->detail_selection >= app->detail_scroll + DNDSPELLBOOK_COLLECTION_ROWS)
            app->detail_scroll = app->detail_selection - (DNDSPELLBOOK_COLLECTION_ROWS - 1U);
    } else if(app->screen == DndSpellbookCollectionScreenCatalog) {
        if(move && event->key == InputKeyUp && app->catalog_count)
            app->selection = app->selection ? app->selection - 1U : app->catalog_count - 1U;
        else if(move && event->key == InputKeyDown && app->catalog_count)
            app->selection = app->selection + 1U < app->catalog_count ? app->selection + 1U : 0U;
        else if(move && event->key == InputKeyLeft && app->catalog_page_start) {
            app->catalog_page_start =
                app->catalog_page_start >= DNDSPELLBOOK_COLLECTION_CATALOG_PAGE ?
                    app->catalog_page_start - DNDSPELLBOOK_COLLECTION_CATALOG_PAGE :
                    0U;
            app->selection = 0U;
            (void)dndspellbook_collection_load_catalog(app);
        } else if(move && event->key == InputKeyRight && app->catalog_has_more) {
            app->catalog_page_start += DNDSPELLBOOK_COLLECTION_CATALOG_PAGE;
            app->selection = 0U;
            (void)dndspellbook_collection_load_catalog(app);
        } else if(event->type == InputTypeLong && event->key == InputKeyDown) {
            app->filter_return_screen = DndSpellbookCollectionScreenCatalog;
            dndspellbook_collection_begin_text(
                app, DndSpellbookCollectionEditSearch, "Search (3+ chars)", app->search_term);
        } else if(event->key == InputKeyOk && event->type == InputTypeLong) {
            app->filter_return_screen = DndSpellbookCollectionScreenCatalog;
            app->screen = DndSpellbookCollectionScreenFilters;
            app->filter_selection = 0U;
        } else if(event->key == InputKeyOk && event->type == InputTypeShort && app->catalog_count) {
            (void)dndspellbook_collection_apply_catalog(app);
        }
    } else if(app->screen == DndSpellbookCollectionScreenFilters) {
        if(move && event->key == InputKeyUp)
            app->filter_selection = app->filter_selection ? app->filter_selection - 1U : 6U;
        else if(move && event->key == InputKeyDown)
            app->filter_selection = app->filter_selection < 6U ? app->filter_selection + 1U : 0U;
        else if(move && (event->key == InputKeyLeft || event->key == InputKeyRight))
            dndspellbook_collection_filter_adjust(app, event->key == InputKeyRight ? 1 : -1);
        else if(event->key == InputKeyOk && event->type == InputTypeShort) {
            app->screen = app->filter_return_screen;
            if(app->screen == DndSpellbookCollectionScreenCatalog) {
                dndspellbook_collection_reset_catalog_offsets(app);
                app->catalog_page_start = 0U;
                app->selection = 0U;
                (void)dndspellbook_collection_load_catalog(app);
            }
        }
    }
    dndspellbook_collection_redraw(app);
    return true;
}

static bool dndspellbook_collection_navigation(void* context) {
    DndSpellbookCollectionApp* app = context;
    if(!app) return false;
    app->input_active = 0U;
    app->edit = DndSpellbookCollectionEditNone;
    view_dispatcher_switch_to_view(app->dispatcher, DNDSPELLBOOK_COLLECTION_VIEW_MAIN);
    dndspellbook_collection_redraw(app);
    return true;
}

static bool
    dndspellbook_collection_load_profile(DndSpellbookCollectionApp* app, const char* args) {
    UNUSED(args);
    if(!app || !app->storage) return false;

    /* DNDSpellbook already links dnd_storage.c for character/collection I/O, so
       use that module's exact Active=<id> reader as the single storage path here.
       This never scans for or substitutes another character. Assign the ID before
       loading the character so a character-load failure still reports the exact
       persisted ID in the header. */
    uint32_t requested = 0U;
    if(!dnd_profile_ref_active_id(app->storage, &requested)) return false;
    app->profile = requested;

    DndSpellbookProfileProjection projection;
    if(!dnd_profile_projection_load_spellbook(app->storage, requested, &projection)) return false;
    dndspellbook_collection_copy(
        app->data.character.name, sizeof(app->data.character.name), projection.name);
    app->data.character.class_count = projection.class_count;
    for(uint8_t i = 0U; i < projection.class_count && i < DND_MAX_CLASSES; ++i)
        app->data.character.classes[i] = projection.classes[i];
    memcpy(
        app->data.character.ability_scores,
        projection.ability_scores,
        sizeof(app->data.character.ability_scores));
    app->data.character.spellcasting_ability = projection.spellcasting_ability;
    app->data.character.spell_attack_misc = projection.spell_attack_misc;
    app->data.character.spell_save_misc = projection.spell_save_misc;
    app->data.character.arcane_recovery_used = projection.arcane_recovery_used;
    memcpy(
        app->data.character.spell_slots_current,
        projection.spell_slots_current,
        sizeof(app->data.character.spell_slots_current));
    memcpy(
        app->data.character.spell_slots_max,
        projection.spell_slots_max,
        sizeof(app->data.character.spell_slots_max));
    return true;
}

static DndSpellbookCollectionApp* dndspellbook_collection_alloc(const char* args) {
    DndSpellbookCollectionApp* app = calloc(1U, sizeof(DndSpellbookCollectionApp));
    if(!app) return NULL;
    app->filter_level = -1;
    app->filter_class = DNDSPELLBOOK_COLLECTION_FILTER_CHARACTER_CLASSES;
    app->filter_return_screen = DndSpellbookCollectionScreenList;
    app->gui = furi_record_open(RECORD_GUI);
    app->storage = furi_record_open(RECORD_STORAGE);
    if(!app->gui || !app->storage) goto fail;
    if(!dnd_settings_load(app->storage, &app->settings)) dnd_settings_defaults(&app->settings);
    app->canonical_catalog_available =
        storage_file_exists(app->storage, DNDSPELLBOOK_COLLECTION_SPELL_CATALOG) ? 1U : 0U;
    app->catalog_all_available = storage_file_exists(
                                     app->storage,
                                     app->canonical_catalog_available ?
                                         DNDSPELLBOOK_COLLECTION_SPELL_CATALOG_ALL :
                                         DNDSPELLBOOK_COLLECTION_SPELL_CATALOG_ALL_LEGACY) ?
                                     1U :
                                     0U;

    /* Reserve the complete fixed UI/runtime footprint before character and spell
       parsing can make variable heap allocations. This mirrors Adventure's startup
       ordering so a successful collection read cannot consume memory required for
       the main view/model. */
    app->dispatcher = view_dispatcher_alloc();
    app->view = view_alloc();
    if(!app->dispatcher || !app->view) goto fail;
    view_dispatcher_set_event_callback_context(app->dispatcher, app);
    view_dispatcher_set_navigation_event_callback(
        app->dispatcher, dndspellbook_collection_navigation);
    view_allocate_model(app->view, ViewModelTypeLockFree, sizeof(DndSpellbookCollectionApp*));
    DndSpellbookCollectionApp** model = view_get_model(app->view);
    if(!model) goto fail;
    *model = app;
    view_commit_model(app->view, false);
    view_set_context(app->view, app);
    view_set_draw_callback(app->view, dndspellbook_collection_draw);
    view_set_input_callback(app->view, dndspellbook_collection_input);

    app->have_profile = dndspellbook_collection_load_profile(app, args) ? 1U : 0U;
    if(app->have_profile) {
        /* Keep the owned spellbook in deterministic level/name order. Existing
           sorted sidecars are only scanned; a rewrite happens only when records
           are actually out of order. No second resident spell collection is kept. */
        if(!dndspellbook_collection_sort_spellbook(app))
            dndspellbook_collection_set_status(app, "Sort failed");
        app->record_offset_valid_pages = 0U;
        /* Opening Spellbook is otherwise read-only with respect to its sidecar. A
           missing sidecar is an empty spell list; the first actual Add New/save
           creates the file. Always enter directly on the list with + Add New selected. */
        if(!dndspellbook_collection_load_page(app, 0U))
            dndspellbook_collection_set_status(app, "Collection read failed");
        app->selection = 0U;
        app->scroll = 0U;
        if(args && !strcmp(args, DND_SPELLBOOK_LAUNCH_MAGIC)) {
            app->magic_direct_launch = 1U;
            dndspellbook_magic_refresh_counts(app);
            app->screen = DndSpellbookCollectionScreenMagic;
        } else {
            app->screen = DndSpellbookCollectionScreenList;
        }
    } else {
        app->screen = DndSpellbookCollectionScreenNoCharacter;
    }
    view_dispatcher_add_view(app->dispatcher, DNDSPELLBOOK_COLLECTION_VIEW_MAIN, app->view);
    view_dispatcher_attach_to_gui(app->dispatcher, app->gui, ViewDispatcherTypeFullscreen);
    return app;

fail:
    if(app->text_input) text_input_free(app->text_input);
    if(app->number_input) number_input_free(app->number_input);
    if(app->view) view_free(app->view);
    if(app->dispatcher) view_dispatcher_free(app->dispatcher);
    dndspellbook_collection_clear_page(&app->data.character);
    free(app->search_matches);
    app->search_matches = NULL;
    if(app->storage) furi_record_close(RECORD_STORAGE);
    if(app->gui) furi_record_close(RECORD_GUI);
    free(app);
    return NULL;
}

static void dndspellbook_collection_free(DndSpellbookCollectionApp* app) {
    if(!app) return;
    if(app->dispatcher && app->text_input)
        view_dispatcher_remove_view(app->dispatcher, DNDSPELLBOOK_COLLECTION_VIEW_TEXT);
    if(app->dispatcher && app->number_input)
        view_dispatcher_remove_view(app->dispatcher, DNDSPELLBOOK_COLLECTION_VIEW_NUMBER);
    if(app->dispatcher && app->view)
        view_dispatcher_remove_view(app->dispatcher, DNDSPELLBOOK_COLLECTION_VIEW_MAIN);
    if(app->text_input) text_input_free(app->text_input);
    if(app->number_input) number_input_free(app->number_input);
    if(app->view) view_free(app->view);
    if(app->dispatcher) view_dispatcher_free(app->dispatcher);
    dndspellbook_collection_clear_page(&app->data.character);
    free(app->search_matches);
    app->search_matches = NULL;
    if(app->storage) furi_record_close(RECORD_STORAGE);
    if(app->gui) furi_record_close(RECORD_GUI);
    free(app);
}

int32_t dndspellbook_collection_run(void* context) {
    DndSpellbookCollectionApp* app = dndspellbook_collection_alloc(context);
    if(!app) return -1;
    view_dispatcher_switch_to_view(app->dispatcher, DNDSPELLBOOK_COLLECTION_VIEW_MAIN);
    view_dispatcher_run(app->dispatcher);
    bool return_to_dnd = app->return_to_dnd;
    dndspellbook_collection_free(app);
    if(return_to_dnd)
        (void)dnd_handoff_launch_if_present(
            DNDOLPHINS_FAP_PATH, DND_PROFILE_RETURN_FOCUS_SPELLBOOK);
    return 0;
}
