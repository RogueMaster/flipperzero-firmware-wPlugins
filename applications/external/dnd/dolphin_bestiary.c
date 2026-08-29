#include "pocket_d20_monsters.h"
#include "pocket_d20_bestiary_state.h"
#include "pocket_d20_handoff.h"
#include "pocket_d20_packs.h"

#include <furi.h>
#include <gui/gui.h>
#include <gui/modules/text_input.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <input/input.h>
#include <loader/loader.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TAG                      "DolphinBestiary"
#define BESTIARY_WINDOW          35U
#define BESTIARY_MARQUEE_EVENT   0xB357U
#define BESTIARY_LONG_BACK_EVENT 0xB358U
#define BESTIARY_MARQUEE_MS      350U

typedef enum {
    BestiaryViewMain,
    BestiaryViewText,
} BestiaryView;

typedef enum {
    BestiaryScreenHome,
    BestiaryScreenList,
    BestiaryScreenDetail,
    BestiaryScreenDetailLine,
    BestiaryScreenEncounter,
    BestiaryScreenSimulator,
    BestiaryScreenWarnings,
    BestiaryScreenSavedEncounters,
    BestiaryScreenEncounterActions,
    BestiaryScreenFilters,
    BestiaryScreenPacks,
    BestiaryScreenDiagnostics,
    BestiaryScreenEdit,
} BestiaryScreen;

typedef enum {
    BestiaryEditNone,
    BestiaryEditSearch,
    BestiaryEditName,
    BestiaryEditType,
    BestiaryEditSize,
    BestiaryEditSpeed,
    BestiaryEditSkills,
    BestiaryEditDefenses,
    BestiaryEditSenses,
    BestiaryEditLanguages,
    BestiaryEditTraits,
    BestiaryEditActions,
    BestiaryEditExtra,
    BestiaryEditFilterName,
    BestiaryEditEncounterName,
    BestiaryEditEncounterRename,
    BestiaryEditEncounterDuplicate,
} BestiaryEdit;

typedef enum {
    BestiaryListCatalog,
    BestiaryListFavorites,
    BestiaryListRecents,
} BestiaryListMode;

typedef struct {
    Gui* gui;
    Storage* storage;
    ViewDispatcher* dispatcher;
    View* view;
    TextInput* text_input;
    FuriPubSub* input_events;
    FuriPubSubSubscription* input_subscription;
    uint8_t text_input_active;
    FuriTimer* marquee_timer;
    BestiaryScreen screen;
    BestiaryScreen return_screen;
    BestiaryEdit edit;
    uint16_t selection;
    uint16_t scroll;
    char status[32];
    char edit_buffer[POCKET_MONSTER_TEXT_LEN];
    char detail_title[32];
    uint16_t detail_line_offset;
    uint16_t detail_return_scroll;
    uint8_t detail_field;
    uint16_t encounter_return_selection;
    uint16_t encounter_return_scroll;
    BestiaryListMode list_mode;
    uint16_t state_total;
    char state_rows[17][48];
    PocketBestiaryFilterPreset pending_filter;
    uint16_t encounter_action_index;
    char encounter_name[POCKET_BESTIARY_ENCOUNTER_NAME_LEN];
    uint8_t encounter_delete_armed;

    char search[POCKET_MONSTER_NAME_LEN];
    uint8_t max_cr_eighths;
    uint8_t type_filter;
    uint8_t source_filter;
    uint8_t environment_filter;
    uint8_t role_filter;
    uint8_t party_level;
    uint8_t party_size;
    uint8_t difficulty;
    uint8_t encounter_environment;
    uint8_t encounter_role;
    uint8_t encounter_template;
    uint8_t allow_repeats;

    uint16_t monster_total;
    uint8_t monster_total_valid;
    uint16_t page_start;
    uint16_t window_count;
    PocketMonsterSummary* window;
    PocketMonsterSummary selected;
    PocketMonsterDetail* detail;
    PocketMonsterEncounter* encounter;
    uint8_t detail_favorite;
    uint8_t edit_existing;
    uint8_t delete_armed;

    uint16_t diagnostic_valid;
    uint16_t diagnostic_invalid;
    uint16_t diagnostic_recovered;
    uint16_t diagnostic_rolled_back;
    uint8_t bundled_version;
    uint8_t custom_version;
    bool custom_present;
} BestiaryApp;

typedef struct {
    char name[POCKET_MONSTER_NAME_LEN];
    uint16_t hit_points;
    uint8_t armor_class;
    uint8_t quantity;
} BestiaryLaunchMonster;

static uint8_t bestiary_marquee_offset = 0U;

static bool bestiary_launch_dnd(BestiaryApp* app, const char* launch_args);
static bool bestiary_launch_dnd_monsters(
    BestiaryApp* app,
    const PocketMonsterSummary* monsters,
    const uint8_t* quantities,
    uint8_t count);
static bool bestiary_launch_saved_dnd(BestiaryApp* app, uint16_t index);

static const char* const type_names[] = {
    "Any",
    "Aberration",
    "Beast",
    "Celestial",
    "Construct",
    "Dragon",
    "Elemental",
    "Fey",
    "Fiend",
    "Giant",
    "Humanoid",
    "Monstrosity",
    "Ooze",
    "Plant",
    "Swarm",
    "Undead"};
static const char* const environment_names[] =
    {"Any", "Aquatic", "Dungeon", "Planar", "Urban", "Wilderness"};
static const char* const source_names[] =
    {"Any", "Open Reference", "D&Dolphins", "Custom", "Custom Pack"};
static const char* const role_names[] =
    {"Any", "Leader", "Controller", "Skirmisher", "Artillery", "Brute", "Minion"};
static const char* const difficulty_names[] = {"Low", "Moderate", "High"};
static const char* const template_names[] = {"Balanced", "Horde", "Elite"};
static const uint8_t cr_choices[] = {0U,  1U,  2U,  4U,  8U,   16U,  24U,  32U,  40U,  48U, 56U,
                                     64U, 72U, 80U, 96U, 112U, 128U, 144U, 160U, 200U, 240U};

static void bestiary_copy(char* output, size_t size, const char* value) {
    if(!size) return;
    strncpy(output, value ? value : "", size - 1U);
    output[size - 1U] = '\0';
}

static void bestiary_status(BestiaryApp* app, const char* value) {
    if(!app) return;
    bestiary_copy(app->status, sizeof(app->status), value);
}

static bool bestiary_save_party_settings(BestiaryApp* app) {
    if(!app || !app->storage) return false;
    bool saved =
        pocket_bestiary_party_settings_save(app->storage, app->party_level, app->party_size);
    if(!saved) bestiary_status(app, "Party settings save failed");
    return saved;
}

static uint8_t bestiary_warning_count(uint8_t flags) {
    uint8_t count = 0U;
    if(flags & PocketEncounterWarningUnsupportedLeader) ++count;
    if(flags & PocketEncounterWarningExposedArtillery) ++count;
    if(flags & PocketEncounterWarningMinionDensity) ++count;
    return count;
}

static void bestiary_refresh(BestiaryApp* app) {
    if(!app || !app->view) return;
    (void)view_get_model(app->view);
    view_commit_model(app->view, true);
}

static void bestiary_enter(BestiaryApp* app, BestiaryScreen screen) {
    app->screen = screen;
    app->selection = 0U;
    app->scroll = 0U;
    app->status[0] = '\0';
    bestiary_marquee_offset = 0U;
}

static bool bestiary_move_event(const InputEvent* event) {
    return event->type == InputTypeShort || event->type == InputTypeRepeat;
}

static void bestiary_move(BestiaryApp* app, uint16_t count, int8_t delta) {
    if(!count) return;
    int32_t next = (int32_t)app->selection + delta;
    if(next < 0) next = count - 1U;
    if(next >= count) next = 0;
    app->selection = (uint16_t)next;
    bestiary_marquee_offset = 0U;
    if(app->selection < app->scroll) app->scroll = app->selection;
    if(app->selection >= app->scroll + 5U) app->scroll = app->selection - 4U;
}

static bool bestiary_contains(const char* text, const char* query) {
    if(!query[0]) return true;
    for(size_t start = 0U; text[start]; ++start) {
        size_t offset = 0U;
        while(query[offset] && text[start + offset]) {
            char left = text[start + offset];
            char right = query[offset];
            if(left >= 'A' && left <= 'Z') left += 'a' - 'A';
            if(right >= 'A' && right <= 'Z') right += 'a' - 'A';
            if(left != right) break;
            ++offset;
        }
        if(!query[offset]) return true;
    }
    return false;
}

static bool bestiary_filter(const PocketMonsterSummary* monster, void* context) {
    BestiaryApp* app = context;
    return (!app->max_cr_eighths || monster->cr_eighths <= app->max_cr_eighths) &&
           (!app->type_filter || !strcmp(monster->type, type_names[app->type_filter])) &&
           (!app->source_filter || !strcmp(monster->source, source_names[app->source_filter])) &&
           (!app->environment_filter ||
            !strcmp(monster->environment, environment_names[app->environment_filter])) &&
           (!app->role_filter || !strcmp(monster->role, role_names[app->role_filter])) &&
           bestiary_contains(monster->name, app->search);
}

static PocketMonsterFilter bestiary_active_filter(const BestiaryApp* app) {
    return app->search[0] || app->max_cr_eighths || app->type_filter || app->source_filter ||
                   app->environment_filter || app->role_filter ?
               bestiary_filter :
               NULL;
}

static void bestiary_release_window(BestiaryApp* app) {
    free(app->window);
    app->window = NULL;
    app->window_count = 0U;
}

static void bestiary_release_text_input(BestiaryApp* app);

static bool bestiary_load_window(BestiaryApp* app) {
    bestiary_release_text_input(app);
    bestiary_release_window(app);
    app->window = calloc(BESTIARY_WINDOW, sizeof(PocketMonsterSummary));
    if(!app->window) {
        bestiary_status(app, "Not enough memory");
        return false;
    }
    uint16_t* total_output = app->monster_total_valid ? NULL : &app->monster_total;
    app->window_count = pocket_monster_query(
        app->storage,
        bestiary_active_filter(app),
        app,
        app->page_start,
        app->window,
        BESTIARY_WINDOW,
        total_output);
    app->monster_total_valid = 1U;
    if(app->page_start >= app->monster_total && app->page_start) {
        app->page_start = ((app->monster_total ? app->monster_total - 1U : 0U) / BESTIARY_WINDOW) *
                          BESTIARY_WINDOW;
        app->window_count = pocket_monster_query(
            app->storage,
            bestiary_active_filter(app),
            app,
            app->page_start,
            app->window,
            BESTIARY_WINDOW,
            NULL);
    }
    return true;
}

static bool bestiary_load_state_window(BestiaryApp* app, BestiaryListMode mode) {
    bestiary_release_text_input(app);
    bestiary_release_window(app);
    app->list_mode = mode;
    app->monster_total = mode == BestiaryListFavorites ?
                             pocket_bestiary_favorite_count(app->storage) :
                             pocket_bestiary_recent_count(app->storage);
    app->monster_total_valid = 1U;
    if(app->page_start >= app->monster_total && app->page_start)
        app->page_start = ((app->monster_total ? app->monster_total - 1U : 0U) / BESTIARY_WINDOW) *
                          BESTIARY_WINDOW;
    app->window = calloc(BESTIARY_WINDOW, sizeof(PocketMonsterSummary));
    if(!app->window) {
        bestiary_status(app, "Not enough memory");
        return false;
    }
    app->window_count = 0U;
    for(uint16_t index = app->page_start;
        index < app->monster_total && app->window_count < BESTIARY_WINDOW;
        ++index) {
        char id[POCKET_MONSTER_ID_LEN];
        bool found = mode == BestiaryListFavorites ?
                         pocket_bestiary_favorite_at(app->storage, index, id, sizeof(id)) :
                         pocket_bestiary_recent_at(app->storage, index, id, sizeof(id));
        if(found && pocket_monster_find(app->storage, id, &app->window[app->window_count]))
            ++app->window_count;
    }
    return true;
}

static void bestiary_refresh_count(BestiaryApp* app) {
    pocket_monster_query(
        app->storage, bestiary_active_filter(app), app, 0U, NULL, 0U, &app->monster_total);
    app->monster_total_valid = 1U;
}

static void bestiary_cache_filter_rows(BestiaryApp* app) {
    app->state_total = pocket_bestiary_filter_count(app->storage);
    for(uint16_t index = 0U; index < app->state_total && index < 16U; ++index) {
        PocketBestiaryFilterPreset preset;
        if(pocket_bestiary_filter_at(app->storage, index, &preset))
            snprintf(app->state_rows[index], sizeof(app->state_rows[index]), "%s", preset.name);
    }
    snprintf(
        app->state_rows[app->state_total],
        sizeof(app->state_rows[app->state_total]),
        "Save Current Filter");
}

static void bestiary_cache_encounter_rows(BestiaryApp* app) {
    app->state_total = pocket_bestiary_encounter_count(app->storage);
    for(uint16_t index = 0U; index < app->state_total && index < 16U; ++index) {
        PocketSavedEncounter encounter;
        if(pocket_bestiary_encounter_at(app->storage, index, &encounter))
            snprintf(
                app->state_rows[index],
                sizeof(app->state_rows[index]),
                "%s L%u x%u",
                encounter.name,
                encounter.party_level,
                encounter.party_size);
    }
}

static void bestiary_cache_pack_rows(BestiaryApp* app) {
    app->state_total = pocket_pack_count(app->storage, PocketPackMonster);
    for(uint16_t index = 0U; index < app->state_total && index < 16U; ++index) {
        PocketPackSummary pack;
        if(pocket_pack_at(app->storage, PocketPackMonster, index, &pack))
            snprintf(
                app->state_rows[index],
                sizeof(app->state_rows[index]),
                "[%c] %s",
                pack.enabled ? 'x' : ' ',
                pack.name);
    }
    snprintf(
        app->state_rows[app->state_total],
        sizeof(app->state_rows[app->state_total]),
        "Install Inbox Pack");
}

static void bestiary_release_detail(BestiaryApp* app) {
    free(app->detail);
    app->detail = NULL;
}

static void bestiary_release_encounter(BestiaryApp* app) {
    free(app->encounter);
    app->encounter = NULL;
}

static void bestiary_release_text_input(BestiaryApp* app) {
    if(!app->text_input) return;
    view_dispatcher_remove_view(app->dispatcher, BestiaryViewText);
    text_input_free(app->text_input);
    app->text_input = NULL;
    app->text_input_active = 0U;
    app->edit = BestiaryEditNone;
}

static void bestiary_release_monster_memory_for_launch(BestiaryApp* app) {
    if(!app) return;

    /* The deferred loader duplicates the launch path and argument string before
       returning. Release Bestiary's large monster allocations first so that
       duplication has the largest possible heap headroom. */
    if(app->marquee_timer) furi_timer_stop(app->marquee_timer);
    bestiary_release_text_input(app);
    bestiary_release_window(app);
    bestiary_release_detail(app);
    bestiary_release_encounter(app);
    pocket_monster_cache_reset();
}

static void bestiary_cr(char* output, size_t size, uint8_t eighths) {
    if(eighths == 1U)
        snprintf(output, size, "1/8");
    else if(eighths == 2U)
        snprintf(output, size, "1/4");
    else if(eighths == 4U)
        snprintf(output, size, "1/2");
    else
        snprintf(output, size, "%u", eighths / 8U);
}

static uint8_t bestiary_cycle_cr(uint8_t current, int8_t delta) {
    uint8_t index = 0U;
    for(uint8_t i = 0U; i < sizeof(cr_choices); ++i)
        if(cr_choices[i] == current) index = i;
    int16_t next = index + delta;
    if(next < 0) next = sizeof(cr_choices) - 1U;
    if(next >= (int16_t)sizeof(cr_choices)) next = 0;
    return cr_choices[next];
}

static void bestiary_header(Canvas* canvas, const char* title, const char* status) {
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_box(canvas, 0, 0, 128, 10);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 8, title);
    if(status && status[0]) {
        uint16_t width = canvas_string_width(canvas, status);
        if(width < 58U) canvas_draw_str(canvas, 126U - width, 8, status);
    }
    canvas_set_color(canvas, ColorBlack);
}

static void bestiary_row(Canvas* canvas, uint8_t row, bool selected, const char* text) {
    uint8_t y = 11U + row * 10U;
    char display[32];
    size_t length = strlen(text);
    if(selected && length > 25U) {
        size_t cycle = length + 4U;
        size_t start = bestiary_marquee_offset % cycle;
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
        canvas_draw_box(canvas, 0, y, 128, 10);
        canvas_set_color(canvas, ColorWhite);
    }
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 3, y + 8U, display);
    canvas_set_color(canvas, ColorBlack);
}

static void
    bestiary_rows(Canvas* canvas, BestiaryApp* app, const char* const* rows, uint16_t count) {
    for(uint8_t visible = 0U; visible < 5U; ++visible) {
        uint16_t index = app->scroll + visible;
        if(index >= count) break;
        bestiary_row(canvas, visible, index == app->selection, rows[index]);
    }
}

static void bestiary_draw_home(Canvas* canvas, BestiaryApp* app) {
    char browse[40], search[40], cr[32], type[32], source[32], environment[32], role[32];
    char party_level[32], party_size[32], difficulty[32], encounter_environment[32];
    char encounter_role[32], repeats[32], template_row[32];
    char cr_value[8];
    bestiary_cr(cr_value, sizeof(cr_value), app->max_cr_eighths);
    if(app->monster_total_valid)
        snprintf(browse, sizeof(browse), "Browse Monsters (%u)", app->monster_total);
    else
        snprintf(browse, sizeof(browse), "Browse Monsters");
    snprintf(search, sizeof(search), "Search: %.24s", app->search[0] ? app->search : "Any");
    snprintf(cr, sizeof(cr), "Max CR: %s", app->max_cr_eighths ? cr_value : "Any");
    snprintf(type, sizeof(type), "Type: %s", type_names[app->type_filter]);
    snprintf(source, sizeof(source), "Source: %s", source_names[app->source_filter]);
    snprintf(
        environment,
        sizeof(environment),
        "Browse Env: %s",
        environment_names[app->environment_filter]);
    snprintf(role, sizeof(role), "Browse Role: %s", role_names[app->role_filter]);
    snprintf(party_level, sizeof(party_level), "Party Level: %u", app->party_level);
    snprintf(party_size, sizeof(party_size), "Party Size: %u", app->party_size);
    snprintf(difficulty, sizeof(difficulty), "Difficulty: %s", difficulty_names[app->difficulty]);
    snprintf(
        encounter_environment,
        sizeof(encounter_environment),
        "Encounter Env: %s",
        environment_names[app->encounter_environment]);
    snprintf(
        encounter_role,
        sizeof(encounter_role),
        "Encounter Role: %s",
        role_names[app->encounter_role]);
    snprintf(repeats, sizeof(repeats), "Repeat Types: %s", app->allow_repeats ? "Yes" : "No");
    snprintf(
        template_row,
        sizeof(template_row),
        "Template: %s",
        template_names[app->encounter_template]);
    const char* rows[] = {
        browse,
        search,
        cr,
        type,
        source,
        environment,
        role,
        party_level,
        party_size,
        difficulty,
        encounter_environment,
        encounter_role,
        repeats,
        template_row,
        "Generate Encounter",
        "Pack Diagnostics",
        "Create Custom Monster",
        "Favorite Monsters",
        "Recent Monsters",
        "Saved Filters",
        "Saved Encounters",
        "Monster Pack Controls",
        "Open Dungeons & Dolphins"};
    bestiary_header(canvas, "Bestiary v" FAP_VERSION, app->status);
    bestiary_rows(canvas, app, rows, 23U);
}

static void bestiary_draw_list(Canvas* canvas, BestiaryApp* app) {
    char page[24];
    uint16_t page_count = (app->monster_total + BESTIARY_WINDOW - 1U) / BESTIARY_WINDOW;
    snprintf(
        page,
        sizeof(page),
        "Page %u/%u <>",
        app->page_start / BESTIARY_WINDOW + 1U,
        page_count ? page_count : 1U);
    const char* title = app->list_mode == BestiaryListFavorites ? "Favorite Monsters" :
                        app->list_mode == BestiaryListRecents   ? "Recent Monsters" :
                                                                  "Monster Catalog";
    bestiary_header(canvas, title, app->status[0] ? app->status : page);
    for(uint8_t visible = 0U; visible < 5U; ++visible) {
        uint16_t index = app->scroll + visible;
        if(index >= app->window_count) break;
        char cr[8], row[64];
        bestiary_cr(cr, sizeof(cr), app->window[index].cr_eighths);
        snprintf(row, sizeof(row), "%s CR %s", app->window[index].name, cr);
        bestiary_row(canvas, visible, index == app->selection, row);
    }
}

static void bestiary_draw_detail(Canvas* canvas, BestiaryApp* app) {
    if(!app->detail) return;
    PocketMonsterDetail* m = app->detail;
    char cr[8], core[64], type[48], abilities[64], source[40], role[32], delete_row[32];
    char favorite_row[32];
    bestiary_cr(cr, sizeof(cr), m->summary.cr_eighths);
    snprintf(
        core,
        sizeof(core),
        "CR %s XP %lu AC%u HP%u",
        cr,
        (unsigned long)m->summary.xp,
        m->summary.armor_class,
        m->summary.hit_points);
    snprintf(
        abilities,
        sizeof(abilities),
        "S%d D%d C%d I%d W%d C%d",
        m->abilities[0],
        m->abilities[1],
        m->abilities[2],
        m->abilities[3],
        m->abilities[4],
        m->abilities[5]);
    snprintf(type, sizeof(type), "Type: %s", m->summary.type);
    snprintf(source, sizeof(source), "Source: %s", m->summary.source);
    snprintf(role, sizeof(role), "Role: %s", m->summary.role);
    snprintf(
        delete_row,
        sizeof(delete_row),
        "%s",
        app->delete_armed ? "OK again: delete custom" : "Delete Custom Monster");
    snprintf(
        favorite_row,
        sizeof(favorite_row),
        "%s",
        app->detail_favorite ? "Remove Favorite" : "Add Favorite");
    const char* rows[] = {
        core,
        type,
        source,
        role,
        m->size_alignment,
        m->speed,
        abilities,
        m->skills,
        m->defenses,
        m->senses,
        m->languages,
        m->traits,
        m->actions,
        m->extra,
        favorite_row,
        "Add to Initiative",
        "Edit Custom Monster",
        delete_row};
    uint8_t row_count = !strcmp(m->summary.source, "Custom") ? 18U : 16U;
    bestiary_header(canvas, m->summary.name, app->status);
    bestiary_rows(canvas, app, rows, row_count);
}

static const char* bestiary_next_text_line(const char* cursor, char* output, size_t output_size) {
    if(!cursor || !output_size) return NULL;
    while(*cursor == ' ' || *cursor == '\r' || *cursor == '\n')
        ++cursor;
    if(!*cursor) {
        output[0] = '\0';
        return NULL;
    }
    const size_t maximum = 25U;
    size_t length = 0U;
    size_t last_space = SIZE_MAX;
    while(cursor[length] && cursor[length] != '\r' && cursor[length] != '\n' && length < maximum) {
        if(cursor[length] == ' ') last_space = length;
        ++length;
    }
    if(length == maximum && cursor[length] && cursor[length] != ' ' && cursor[length] != '\r' &&
       cursor[length] != '\n' && last_space != SIZE_MAX)
        length = last_space;
    if(length >= output_size) length = output_size - 1U;
    memcpy(output, cursor, length);
    while(length && output[length - 1U] == ' ')
        --length;
    output[length] = '\0';
    cursor += length;
    while(*cursor == ' ' || *cursor == '\r' || *cursor == '\n')
        ++cursor;
    return cursor;
}

static uint16_t bestiary_text_line_count(const char* text) {
    uint16_t count = 0U;
    char line[21];
    const char* cursor = text;
    while(cursor && *cursor) {
        const char* next = bestiary_next_text_line(cursor, line, sizeof(line));
        if(!next || next == cursor) break;
        ++count;
        cursor = next;
    }
    return count ? count : 1U;
}

static void bestiary_draw_detail_line(Canvas* canvas, BestiaryApp* app) {
    uint16_t line_count = bestiary_text_line_count(app->edit_buffer);
    uint16_t last_line = app->detail_line_offset + 5U;
    if(last_line > line_count) last_line = line_count;
    /* Three uint16_t values need up to 17 visible characters plus NUL. */
    char position[24];
    snprintf(
        position,
        sizeof(position),
        "%u-%u/%u",
        app->detail_line_offset + 1U,
        last_line,
        line_count);
    bestiary_header(canvas, app->detail_title, position);
    canvas_draw_frame(canvas, 0, 10, 128, 54);
    canvas_set_font(canvas, FontSecondary);
    const char* cursor = app->edit_buffer;
    char line[21];
    for(uint16_t skip = 0U; skip < app->detail_line_offset && cursor && *cursor; ++skip)
        cursor = bestiary_next_text_line(cursor, line, sizeof(line));
    for(uint8_t row = 0U; row < 5U && cursor && *cursor; ++row) {
        cursor = bestiary_next_text_line(cursor, line, sizeof(line));
        canvas_draw_str(canvas, 4, (uint8_t)(20U + row * 10U), line);
    }
}

static void bestiary_draw_encounter(Canvas* canvas, BestiaryApp* app) {
    if(!app->encounter) return;
    char title[48];
    snprintf(
        title,
        sizeof(title),
        "Encounter %lu/%lu XP",
        (unsigned long)app->encounter->spent,
        (unsigned long)app->encounter->budget);
    bestiary_header(canvas, title, app->status);
    PocketEncounterComposition composition;
    pocket_monster_analyze_composition(app->encounter, app->party_size, &composition);
    uint8_t warning_count = bestiary_warning_count(composition.warning_flags);
    for(uint8_t visible = 0U; visible < 5U; ++visible) {
        uint16_t index = app->scroll + visible;
        if(index >= app->encounter->count + 4U) break;
        char row[64], cr[8];
        if(index < app->encounter->count) {
            bestiary_cr(cr, sizeof(cr), app->encounter->monsters[index].cr_eighths);
            snprintf(
                row,
                sizeof(row),
                "%ux %s CR%s",
                app->encounter->quantities[index],
                app->encounter->monsters[index].name,
                cr);
        } else if(index == app->encounter->count) {
            snprintf(row, sizeof(row), "Difficulty Simulator");
        } else if(index == app->encounter->count + 1U) {
            snprintf(row, sizeof(row), "Save Encounter");
        } else if(index == app->encounter->count + 2U) {
            if(warning_count)
                snprintf(
                    row,
                    sizeof(row),
                    "Composition: %u warning%s",
                    warning_count,
                    warning_count == 1U ? "" : "s");
            else
                snprintf(row, sizeof(row), "Composition: OK");
        } else {
            snprintf(row, sizeof(row), "Add to Initiative");
        }
        bestiary_row(canvas, visible, index == app->selection, row);
    }
}

static void bestiary_draw_simulator(Canvas* canvas, BestiaryApp* app) {
    if(!app->encounter) return;
    PocketEncounterSimulation simulation;
    pocket_monster_simulate(app->encounter, app->party_level, app->party_size, &simulation);
    char subtitle[40];
    snprintf(
        subtitle,
        sizeof(subtitle),
        "%s %lu XP",
        difficulty_names[simulation.classification],
        (unsigned long)simulation.spent);
    bestiary_header(canvas, "Difficulty Simulator", subtitle);
    for(uint8_t visible = 0U; visible < 5U; ++visible) {
        uint16_t index = app->scroll + visible;
        if(index >= app->encounter->count + 2U) break;
        char row[64];
        if(index == 0U)
            snprintf(row, sizeof(row), "Party Level: %u", app->party_level);
        else if(index == 1U)
            snprintf(row, sizeof(row), "Party Size: %u", app->party_size);
        else {
            uint8_t monster = (uint8_t)(index - 2U);
            snprintf(
                row,
                sizeof(row),
                "%ux %s",
                app->encounter->quantities[monster],
                app->encounter->monsters[monster].name);
        }
        bestiary_row(canvas, visible, index == app->selection, row);
    }
}

static void bestiary_draw_warnings(Canvas* canvas, BestiaryApp* app) {
    if(!app->encounter) return;
    PocketEncounterComposition composition;
    pocket_monster_analyze_composition(app->encounter, app->party_size, &composition);
    char summary[48];
    snprintf(
        summary,
        sizeof(summary),
        "Total %u L%u A%u M%u",
        composition.total_creatures,
        composition.leaders,
        composition.artillery,
        composition.minions);
    const char* rows[4];
    uint8_t count = 0U;
    rows[count++] = summary;
    if(composition.warning_flags & PocketEncounterWarningUnsupportedLeader)
        rows[count++] = "Leader lacks support";
    if(composition.warning_flags & PocketEncounterWarningExposedArtillery)
        rows[count++] = "Artillery is exposed";
    if(composition.warning_flags & PocketEncounterWarningMinionDensity)
        rows[count++] = "Minion density is high";
    if(count == 1U) rows[count++] = "No warnings detected";
    bestiary_header(canvas, "Composition Warnings", "OK/Back: encounter");
    bestiary_rows(canvas, app, rows, count);
}

static void bestiary_draw_saved_encounters(Canvas* canvas, BestiaryApp* app) {
    bestiary_header(canvas, "Saved Encounters", app->status);
    for(uint8_t visible = 0U; visible < 5U; ++visible) {
        uint16_t index = app->scroll + visible;
        if(index >= app->state_total) break;
        bestiary_row(canvas, visible, index == app->selection, app->state_rows[index]);
    }
}

static void bestiary_draw_encounter_actions(Canvas* canvas, BestiaryApp* app) {
    char title[48];
    snprintf(
        title,
        sizeof(title),
        "Saved: %.32s",
        app->encounter_name[0] ? app->encounter_name : "Encounter");
    const char* delete_row = app->encounter_delete_armed ? "OK again: Delete" : "Delete Encounter";
    const char* rows[] = {
        "Resume Encounter",
        "Add to Initiative",
        "Rename Encounter",
        "Duplicate Encounter",
        "Archive Encounter",
        delete_row,
    };
    bestiary_header(canvas, title, app->status);
    bestiary_rows(canvas, app, rows, 6U);
}

static void bestiary_draw_filters(Canvas* canvas, BestiaryApp* app) {
    bestiary_header(canvas, "Saved Filters", app->status);
    uint16_t count = app->state_total + 1U;
    for(uint8_t visible = 0U; visible < 5U; ++visible) {
        uint16_t index = app->scroll + visible;
        if(index >= count) break;
        bestiary_row(canvas, visible, index == app->selection, app->state_rows[index]);
    }
}

static void bestiary_draw_packs(Canvas* canvas, BestiaryApp* app) {
    bestiary_header(canvas, "Monster Packs", app->status);
    uint16_t count = app->state_total + 1U;
    for(uint8_t visible = 0U; visible < 5U; ++visible) {
        uint16_t index = app->scroll + visible;
        if(index >= count) break;
        bestiary_row(canvas, visible, index == app->selection, app->state_rows[index]);
    }
}

static void bestiary_draw_diagnostics(Canvas* canvas, BestiaryApp* app) {
    char total[32], valid[32], invalid[32], bundled[32], custom[32], recovered[32],
        rolled_back[32], heap[32];
    snprintf(total, sizeof(total), "Records: %u", app->diagnostic_valid + app->diagnostic_invalid);
    snprintf(valid, sizeof(valid), "Valid: %u", app->diagnostic_valid);
    snprintf(invalid, sizeof(invalid), "Invalid: %u", app->diagnostic_invalid);
    snprintf(bundled, sizeof(bundled), "Bundled Pack: v%u", app->bundled_version);
    snprintf(custom, sizeof(custom), "Custom Pack: %s", app->custom_present ? "present" : "none");
    snprintf(recovered, sizeof(recovered), "Recovered: %u", app->diagnostic_recovered);
    snprintf(rolled_back, sizeof(rolled_back), "Rolled Back: %u", app->diagnostic_rolled_back);
    snprintf(heap, sizeof(heap), "Free Heap: %lu", (unsigned long)memmgr_get_free_heap());
    const char* rows[] = {
        total, valid, invalid, bundled, custom, recovered, rolled_back, heap, "OK: Rescan"};
    bestiary_header(canvas, "Pack Diagnostics", app->status);
    bestiary_rows(canvas, app, rows, 9U);
}

static void bestiary_draw_edit(Canvas* canvas, BestiaryApp* app) {
    if(!app->detail) return;
    PocketMonsterDetail* m = app->detail;
    char cr[24], xp[24], ac[24], hp[24], environment[32], role[32], ability[6][20];
    char cr_value[8];
    bestiary_cr(cr_value, sizeof(cr_value), m->summary.cr_eighths);
    snprintf(cr, sizeof(cr), "CR: %s", cr_value);
    snprintf(xp, sizeof(xp), "XP: %lu", (unsigned long)m->summary.xp);
    snprintf(ac, sizeof(ac), "AC: %u", m->summary.armor_class);
    snprintf(hp, sizeof(hp), "HP: %u", m->summary.hit_points);
    snprintf(environment, sizeof(environment), "Env: %.26s", m->summary.environment);
    snprintf(role, sizeof(role), "Role: %s", m->summary.role);
    static const char* const labels[] = {"STR", "DEX", "CON", "INT", "WIS", "CHA"};
    for(uint8_t i = 0U; i < 6U; ++i)
        snprintf(ability[i], sizeof(ability[i]), "%s: %d", labels[i], m->abilities[i]);
    const char* rows[] = {
        m->summary.name,
        cr,
        xp,
        ac,
        hp,
        m->summary.type,
        environment,
        role,
        m->size_alignment,
        m->speed,
        ability[0],
        ability[1],
        ability[2],
        ability[3],
        ability[4],
        ability[5],
        m->skills,
        m->defenses,
        m->senses,
        m->languages,
        m->traits,
        m->actions,
        m->extra,
        app->edit_existing ? "Update Custom Monster" : "Save Custom Monster"};
    bestiary_header(canvas, app->edit_existing ? "Edit Custom" : "New Custom", app->status);
    bestiary_rows(canvas, app, rows, 24U);
}

static void bestiary_draw(Canvas* canvas, void* model) {
    BestiaryApp* app = *(BestiaryApp**)model;
    canvas_clear(canvas);
    switch(app->screen) {
    case BestiaryScreenHome:
        bestiary_draw_home(canvas, app);
        break;
    case BestiaryScreenList:
        bestiary_draw_list(canvas, app);
        break;
    case BestiaryScreenDetail:
        bestiary_draw_detail(canvas, app);
        break;
    case BestiaryScreenDetailLine:
        bestiary_draw_detail_line(canvas, app);
        break;
    case BestiaryScreenEncounter:
        bestiary_draw_encounter(canvas, app);
        break;
    case BestiaryScreenSimulator:
        bestiary_draw_simulator(canvas, app);
        break;
    case BestiaryScreenWarnings:
        bestiary_draw_warnings(canvas, app);
        break;
    case BestiaryScreenSavedEncounters:
        bestiary_draw_saved_encounters(canvas, app);
        break;
    case BestiaryScreenEncounterActions:
        bestiary_draw_encounter_actions(canvas, app);
        break;
    case BestiaryScreenFilters:
        bestiary_draw_filters(canvas, app);
        break;
    case BestiaryScreenPacks:
        bestiary_draw_packs(canvas, app);
        break;
    case BestiaryScreenDiagnostics:
        bestiary_draw_diagnostics(canvas, app);
        break;
    case BestiaryScreenEdit:
        bestiary_draw_edit(canvas, app);
        break;
    }
}

static void bestiary_begin_text(
    BestiaryApp* app,
    BestiaryEdit target,
    const char* title,
    const char* initial);

static void bestiary_text_done(void* context) {
    BestiaryApp* app = context;
    app->text_input_active = 0U;
    PocketMonsterDetail* m = app->detail;
    BestiaryEdit completed = app->edit;
    switch(app->edit) {
    case BestiaryEditSearch:
        bestiary_copy(app->search, sizeof(app->search), app->edit_buffer);
        break;
    case BestiaryEditName:
        if(m) bestiary_copy(m->summary.name, sizeof(m->summary.name), app->edit_buffer);
        break;
    case BestiaryEditType:
        if(m) bestiary_copy(m->summary.type, sizeof(m->summary.type), app->edit_buffer);
        break;
    case BestiaryEditSize:
        if(m) bestiary_copy(m->size_alignment, sizeof(m->size_alignment), app->edit_buffer);
        break;
    case BestiaryEditSpeed:
        if(m) bestiary_copy(m->speed, sizeof(m->speed), app->edit_buffer);
        break;
    case BestiaryEditSkills:
        if(m) bestiary_copy(m->skills, sizeof(m->skills), app->edit_buffer);
        break;
    case BestiaryEditDefenses:
        if(m) bestiary_copy(m->defenses, sizeof(m->defenses), app->edit_buffer);
        break;
    case BestiaryEditSenses:
        if(m) bestiary_copy(m->senses, sizeof(m->senses), app->edit_buffer);
        break;
    case BestiaryEditLanguages:
        if(m) bestiary_copy(m->languages, sizeof(m->languages), app->edit_buffer);
        break;
    case BestiaryEditTraits:
        if(m) bestiary_copy(m->traits, sizeof(m->traits), app->edit_buffer);
        break;
    case BestiaryEditActions:
        if(m) bestiary_copy(m->actions, sizeof(m->actions), app->edit_buffer);
        break;
    case BestiaryEditExtra:
        if(m) bestiary_copy(m->extra, sizeof(m->extra), app->edit_buffer);
        break;
    case BestiaryEditFilterName:
        bestiary_copy(
            app->pending_filter.name, sizeof(app->pending_filter.name), app->edit_buffer);
        bestiary_status(
            app,
            pocket_bestiary_filter_save(app->storage, &app->pending_filter) ?
                "Filter saved" :
                "Filter save failed");
        bestiary_cache_filter_rows(app);
        break;
    case BestiaryEditEncounterName:
        if(app->encounter && app->edit_buffer[0]) {
            PocketSavedEncounter saved = {0};
            bestiary_copy(saved.name, sizeof(saved.name), app->edit_buffer);
            saved.party_level = app->party_level;
            saved.party_size = app->party_size;
            saved.difficulty = app->difficulty;
            saved.count = app->encounter->count;
            for(uint8_t index = 0U; index < saved.count; ++index) {
                bestiary_copy(
                    saved.monster_ids[index],
                    sizeof(saved.monster_ids[index]),
                    app->encounter->monsters[index].id);
                saved.quantities[index] = app->encounter->quantities[index];
            }
            bool saved_ok = pocket_bestiary_encounter_save(app->storage, &saved);
            if(saved_ok)
                bestiary_copy(app->encounter_name, sizeof(app->encounter_name), saved.name);
            bestiary_status(app, saved_ok ? "Encounter saved" : "Encounter save failed");
        }
        break;
    case BestiaryEditEncounterRename:
        if(app->edit_buffer[0]) {
            bool renamed = pocket_bestiary_encounter_rename(
                app->storage, app->encounter_action_index, app->edit_buffer);
            if(renamed) {
                bestiary_copy(app->encounter_name, sizeof(app->encounter_name), app->edit_buffer);
                bestiary_cache_encounter_rows(app);
            }
            bestiary_status(app, renamed ? "Encounter renamed" : "Rename failed/name used");
        }
        break;
    case BestiaryEditEncounterDuplicate:
        if(app->edit_buffer[0]) {
            bool duplicated = pocket_bestiary_encounter_duplicate(
                app->storage, app->encounter_action_index, app->edit_buffer);
            if(duplicated) bestiary_cache_encounter_rows(app);
            bestiary_status(
                app, duplicated ? "Encounter duplicated" : "Duplicate failed/name used");
        }
        break;
    default:
        break;
    }
    app->edit = BestiaryEditNone;
    if(app->screen == BestiaryScreenHome || completed == BestiaryEditSearch)
        app->monster_total_valid = 0U;
    view_dispatcher_switch_to_view(app->dispatcher, BestiaryViewMain);
    bestiary_refresh(app);
}

static void bestiary_begin_text(
    BestiaryApp* app,
    BestiaryEdit target,
    const char* title,
    const char* initial) {
    if(!app->text_input) {
        app->text_input = text_input_alloc();
        if(!app->text_input) {
            bestiary_status(app, "Text input memory low");
            return;
        }
        view_dispatcher_add_view(
            app->dispatcher, BestiaryViewText, text_input_get_view(app->text_input));
    }
    app->edit = target;
    app->text_input_active = 1U;
    bestiary_copy(app->edit_buffer, sizeof(app->edit_buffer), initial);
    text_input_reset(app->text_input);
    text_input_set_header_text(app->text_input, title);
    text_input_set_result_callback(
        app->text_input,
        bestiary_text_done,
        app,
        app->edit_buffer,
        sizeof(app->edit_buffer),
        false);
    view_dispatcher_switch_to_view(app->dispatcher, BestiaryViewText);
}

static bool bestiary_open_detail(
    BestiaryApp* app,
    const PocketMonsterSummary* summary,
    BestiaryScreen return_screen) {
    bestiary_release_text_input(app);
    bestiary_release_detail(app);
    app->detail = malloc(sizeof(PocketMonsterDetail));
    if(!app->detail || !pocket_monster_load(app->storage, summary, app->detail)) {
        bestiary_release_detail(app);
        bestiary_status(app, "Stat block unavailable");
        return false;
    }
    app->selected = *summary;
    pocket_bestiary_recent_add(app->storage, summary->id);
    app->detail_favorite = pocket_bestiary_favorite_contains(app->storage, summary->id) ? 1U : 0U;
    app->return_screen = return_screen;
    app->delete_armed = 0U;
    bestiary_enter(app, BestiaryScreenDetail);
    bestiary_status(app, "OK opens full stat line");
    return true;
}

static void bestiary_open_detail_line(BestiaryApp* app) {
    if(!app->detail || app->selection >= 14U) return;
    PocketMonsterDetail* m = app->detail;
    uint8_t field = (uint8_t)app->selection;
    const char* value = NULL;
    char cr[8];

    switch(field) {
    case 0U:
        bestiary_copy(app->detail_title, sizeof(app->detail_title), "Core Stats");
        bestiary_cr(cr, sizeof(cr), m->summary.cr_eighths);
        snprintf(
            app->edit_buffer,
            sizeof(app->edit_buffer),
            "Challenge %s; XP %lu; Armor Class %u; Hit Points %u",
            cr,
            (unsigned long)m->summary.xp,
            m->summary.armor_class,
            m->summary.hit_points);
        break;
    case 1U:
        bestiary_copy(app->detail_title, sizeof(app->detail_title), "Creature Type");
        value = m->summary.type;
        break;
    case 2U:
        bestiary_copy(app->detail_title, sizeof(app->detail_title), "Source");
        value = m->summary.source;
        break;
    case 3U:
        bestiary_copy(app->detail_title, sizeof(app->detail_title), "Encounter Role");
        value = m->summary.role;
        break;
    case 4U:
        bestiary_copy(app->detail_title, sizeof(app->detail_title), "Size / Alignment");
        value = m->size_alignment;
        break;
    case 5U:
        bestiary_copy(app->detail_title, sizeof(app->detail_title), "Movement");
        value = m->speed;
        break;
    case 6U:
        bestiary_copy(app->detail_title, sizeof(app->detail_title), "Ability Scores");
        snprintf(
            app->edit_buffer,
            sizeof(app->edit_buffer),
            "STR %d, DEX %d, CON %d, INT %d, WIS %d, CHA %d",
            m->abilities[0],
            m->abilities[1],
            m->abilities[2],
            m->abilities[3],
            m->abilities[4],
            m->abilities[5]);
        break;
    case 7U:
        bestiary_copy(app->detail_title, sizeof(app->detail_title), "Skills");
        value = m->skills;
        break;
    case 8U:
        bestiary_copy(app->detail_title, sizeof(app->detail_title), "Defenses");
        value = m->defenses;
        break;
    case 9U:
        bestiary_copy(app->detail_title, sizeof(app->detail_title), "Senses");
        value = m->senses;
        break;
    case 10U:
        bestiary_copy(app->detail_title, sizeof(app->detail_title), "Languages");
        value = m->languages;
        break;
    case 11U:
        bestiary_copy(app->detail_title, sizeof(app->detail_title), "Traits");
        value = m->traits;
        break;
    case 12U:
        bestiary_copy(app->detail_title, sizeof(app->detail_title), "Actions");
        value = m->actions;
        break;
    case 13U:
        bestiary_copy(app->detail_title, sizeof(app->detail_title), "Extra");
        value = m->extra;
        break;
    default:
        return;
    }

    if(value)
        bestiary_copy(app->edit_buffer, sizeof(app->edit_buffer), value[0] ? value : "(none)");
    app->detail_field = field;
    app->detail_line_offset = 0U;
    app->detail_return_scroll = app->scroll;
    bestiary_enter(app, BestiaryScreenDetailLine);
}

static void bestiary_return_to_detail(BestiaryApp* app) {
    uint8_t field = app->detail_field;
    bestiary_enter(app, BestiaryScreenDetail);
    app->selection = field;
    app->scroll = app->detail_return_scroll;
}

static void bestiary_generate(BestiaryApp* app) {
    bestiary_release_text_input(app);
    bestiary_release_encounter(app);
    app->encounter = calloc(1U, sizeof(PocketMonsterEncounter));
    if(!app->encounter) {
        bestiary_status(app, "Not enough memory");
        return;
    }
    bool generated = pocket_monster_generate(
        app->storage,
        app->party_level,
        app->party_size,
        (PocketEncounterDifficulty)app->difficulty,
        environment_names[app->encounter_environment],
        app->allow_repeats,
        (PocketEncounterTemplate)app->encounter_template,
        role_names[app->encounter_role],
        app->encounter);
    if(!generated) {
        bestiary_release_encounter(app);
        bestiary_status(app, "No encounter matched");
        return;
    }
    bestiary_copy(app->encounter_name, sizeof(app->encounter_name), "Generated Encounter");
    bestiary_enter(app, BestiaryScreenEncounter);
}

static void bestiary_diagnose(BestiaryApp* app) {
    bestiary_release_text_input(app);
    app->diagnostic_valid = 0U;
    app->diagnostic_invalid = 0U;
    pocket_monster_recover_user_pack(
        app->storage, &app->diagnostic_recovered, &app->diagnostic_rolled_back);
    pocket_monster_pack_versions(
        app->storage, &app->bundled_version, &app->custom_version, &app->custom_present);
    pocket_monster_validate_pack(
        app->storage, NULL, &app->diagnostic_valid, &app->diagnostic_invalid);
    bestiary_enter(app, BestiaryScreenDiagnostics);
    bestiary_status(app, app->diagnostic_invalid ? "Issues found" : "Pack valid");
}

static void bestiary_new_custom(BestiaryApp* app) {
    bestiary_release_text_input(app);
    bestiary_release_detail(app);
    app->detail = calloc(1U, sizeof(PocketMonsterDetail));
    if(!app->detail) {
        bestiary_status(app, "Not enough memory");
        return;
    }
    bestiary_copy(app->detail->summary.name, sizeof(app->detail->summary.name), "Custom Monster");
    bestiary_copy(app->detail->summary.type, sizeof(app->detail->summary.type), "Monstrosity");
    bestiary_copy(
        app->detail->summary.environment, sizeof(app->detail->summary.environment), "Wilderness");
    bestiary_copy(app->detail->summary.role, sizeof(app->detail->summary.role), "Skirmisher");
    bestiary_copy(
        app->detail->size_alignment, sizeof(app->detail->size_alignment), "Medium Monstrosity");
    bestiary_copy(app->detail->speed, sizeof(app->detail->speed), "30 ft.");
    bestiary_copy(app->detail->senses, sizeof(app->detail->senses), "Passive Perception 10");
    bestiary_copy(app->detail->languages, sizeof(app->detail->languages), "None");
    bestiary_copy(app->detail->actions, sizeof(app->detail->actions), "None");
    for(uint8_t i = 0U; i < 6U; ++i)
        app->detail->abilities[i] = 10;
    app->detail->summary.cr_eighths = 8U;
    app->detail->summary.xp = 200U;
    app->detail->summary.armor_class = 10U;
    app->detail->summary.hit_points = 10U;
    app->edit_existing = 0U;
    bestiary_enter(app, BestiaryScreenEdit);
}

static void bestiary_back(BestiaryApp* app) {
    switch(app->screen) {
    case BestiaryScreenHome:
        view_dispatcher_stop(app->dispatcher);
        break;
    case BestiaryScreenList:
        bestiary_release_window(app);
        bestiary_enter(app, BestiaryScreenHome);
        break;
    case BestiaryScreenDetail: {
        BestiaryScreen destination = app->return_screen;
        bestiary_release_detail(app);
        if(destination == BestiaryScreenList) {
            if(app->list_mode == BestiaryListCatalog)
                bestiary_load_window(app);
            else
                bestiary_load_state_window(app, app->list_mode);
            bestiary_enter(app, BestiaryScreenList);
        } else {
            bestiary_enter(app, destination);
            if(destination == BestiaryScreenEncounter) {
                app->selection = app->encounter_return_selection;
                app->scroll = app->encounter_return_scroll;
            }
        }
        break;
    }
    case BestiaryScreenDetailLine:
        bestiary_return_to_detail(app);
        break;
    case BestiaryScreenEncounter:
        bestiary_release_encounter(app);
        bestiary_enter(app, BestiaryScreenHome);
        break;
    case BestiaryScreenSimulator:
    case BestiaryScreenWarnings:
        bestiary_enter(app, BestiaryScreenEncounter);
        break;
    case BestiaryScreenEncounterActions: {
        uint16_t selected = app->encounter_action_index;
        app->encounter_delete_armed = 0U;
        bestiary_cache_encounter_rows(app);
        bestiary_enter(app, BestiaryScreenSavedEncounters);
        if(app->state_total) {
            app->selection = selected < app->state_total ? selected : app->state_total - 1U;
            app->scroll = app->selection > 4U ? app->selection - 4U : 0U;
        }
        break;
    }
    case BestiaryScreenSavedEncounters:
    case BestiaryScreenFilters:
    case BestiaryScreenPacks:
    case BestiaryScreenDiagnostics:
        bestiary_enter(app, BestiaryScreenHome);
        break;
    case BestiaryScreenEdit:
        if(app->edit_existing) {
            PocketMonsterSummary summary = app->selected;
            bestiary_release_detail(app);
            bestiary_open_detail(app, &summary, BestiaryScreenHome);
        } else {
            bestiary_release_detail(app);
            bestiary_enter(app, BestiaryScreenHome);
        }
        break;
    }
}

static void bestiary_handle_home(BestiaryApp* app, const InputEvent* event) {
    if(bestiary_move_event(event) && event->key == InputKeyUp)
        bestiary_move(app, 23U, -1);
    else if(bestiary_move_event(event) && event->key == InputKeyDown)
        bestiary_move(app, 23U, 1);
    else if(bestiary_move_event(event) && (event->key == InputKeyLeft || event->key == InputKeyRight)) {
        int8_t delta = event->key == InputKeyRight ? 1 : -1;
        if(app->selection == 2U)
            app->max_cr_eighths = bestiary_cycle_cr(app->max_cr_eighths, delta);
        else if(app->selection == 3U) {
            int16_t value = app->type_filter + delta;
            if(value < 0) value = sizeof(type_names) / sizeof(type_names[0]) - 1U;
            if(value >= (int16_t)(sizeof(type_names) / sizeof(type_names[0]))) value = 0;
            app->type_filter = value;
        } else if(app->selection == 4U) {
            int16_t value = app->source_filter + delta;
            if(value < 0) value = sizeof(source_names) / sizeof(source_names[0]) - 1U;
            if(value >= (int16_t)(sizeof(source_names) / sizeof(source_names[0]))) value = 0;
            app->source_filter = value;
        } else if(app->selection == 5U || app->selection == 10U) {
            uint8_t* target = app->selection == 5U ? &app->environment_filter :
                                                     &app->encounter_environment;
            int16_t value = *target + delta;
            if(value < 0) value = sizeof(environment_names) / sizeof(environment_names[0]) - 1U;
            if(value >= (int16_t)(sizeof(environment_names) / sizeof(environment_names[0])))
                value = 0;
            *target = value;
        } else if(app->selection == 6U || app->selection == 11U) {
            uint8_t* target = app->selection == 6U ? &app->role_filter : &app->encounter_role;
            int16_t value = *target + delta;
            if(value < 0) value = sizeof(role_names) / sizeof(role_names[0]) - 1U;
            if(value >= (int16_t)(sizeof(role_names) / sizeof(role_names[0]))) value = 0;
            *target = value;
        } else if(app->selection == 7U) {
            int16_t value = app->party_level + delta;
            app->party_level = (uint8_t)(value < 1 ? 20 : value > 20 ? 1 : value);
            bestiary_save_party_settings(app);
        } else if(app->selection == 8U) {
            int16_t value = app->party_size + delta;
            app->party_size = (uint8_t)(value < 1 ? 12 : value > 12 ? 1 : value);
            bestiary_save_party_settings(app);
        } else if(app->selection == 9U) {
            int16_t value = app->difficulty + delta;
            app->difficulty = (uint8_t)(value < 0 ? 2 : value > 2 ? 0 : value);
        } else if(app->selection == 12U)
            app->allow_repeats = !app->allow_repeats;
        else if(app->selection == 13U) {
            int16_t value = app->encounter_template + delta;
            app->encounter_template = (uint8_t)(value < 0 ? 2 : value > 2 ? 0 : value);
        } else
            return;
        if(app->selection >= 2U && app->selection <= 6U) app->monster_total_valid = 0U;
    } else if(event->type == InputTypeShort && event->key == InputKeyOk) {
        if(app->selection == 0U) {
            app->page_start = 0U;
            app->list_mode = BestiaryListCatalog;
            if(bestiary_load_window(app)) bestiary_enter(app, BestiaryScreenList);
        } else if(app->selection == 1U) {
            bestiary_begin_text(app, BestiaryEditSearch, "Monster Search", app->search);
        } else if(app->selection == 14U)
            bestiary_generate(app);
        else if(app->selection == 15U)
            bestiary_diagnose(app);
        else if(app->selection == 16U)
            bestiary_new_custom(app);
        else if(app->selection == 17U || app->selection == 18U) {
            app->page_start = 0U;
            BestiaryListMode mode = app->selection == 17U ? BestiaryListFavorites :
                                                            BestiaryListRecents;
            if(bestiary_load_state_window(app, mode)) bestiary_enter(app, BestiaryScreenList);
        } else if(app->selection == 19U) {
            bestiary_cache_filter_rows(app);
            bestiary_enter(app, BestiaryScreenFilters);
        } else if(app->selection == 20U) {
            bestiary_cache_encounter_rows(app);
            bestiary_enter(app, BestiaryScreenSavedEncounters);
        } else if(app->selection == 21U) {
            bestiary_cache_pack_rows(app);
            bestiary_enter(app, BestiaryScreenPacks);
        } else if(app->selection == 22U) {
            bestiary_launch_dnd(app, NULL);
        }
    }
}

static void bestiary_handle_list(BestiaryApp* app, const InputEvent* event) {
    if(bestiary_move_event(event) && event->key == InputKeyUp)
        bestiary_move(app, app->window_count, -1);
    else if(bestiary_move_event(event) && event->key == InputKeyDown)
        bestiary_move(app, app->window_count, 1);
    else if(bestiary_move_event(event) && (event->key == InputKeyLeft || event->key == InputKeyRight)) {
        uint16_t next = app->page_start;
        if(event->key == InputKeyRight && app->page_start + BESTIARY_WINDOW < app->monster_total)
            next += BESTIARY_WINDOW;
        else if(event->key == InputKeyLeft && app->page_start >= BESTIARY_WINDOW)
            next -= BESTIARY_WINDOW;
        if(next != app->page_start) {
            app->page_start = next;
            app->selection = app->scroll = 0U;
            if(app->list_mode == BestiaryListCatalog)
                bestiary_load_window(app);
            else
                bestiary_load_state_window(app, app->list_mode);
        }
    } else if(
        event->type == InputTypeShort && event->key == InputKeyOk &&
        app->selection < app->window_count) {
        PocketMonsterSummary summary = app->window[app->selection];
        bestiary_release_window(app);
        bestiary_open_detail(app, &summary, BestiaryScreenList);
    }
}

static void bestiary_handle_detail(BestiaryApp* app, const InputEvent* event) {
    if(!app->detail) return;
    bool custom = !strcmp(app->detail->summary.source, "Custom");
    uint8_t row_count = custom ? 18U : 16U;
    if(bestiary_move_event(event) && event->key == InputKeyUp) {
        app->delete_armed = 0U;
        bestiary_move(app, row_count, -1);
    } else if(bestiary_move_event(event) && event->key == InputKeyDown) {
        app->delete_armed = 0U;
        bestiary_move(app, row_count, 1);
    } else if(event->type == InputTypeShort && event->key == InputKeyOk) {
        if(app->selection < 14U) {
            bestiary_open_detail_line(app);
        } else if(app->selection == 14U) {
            bool now_favorite = false;
            bool changed = pocket_bestiary_favorite_toggle(
                app->storage, app->detail->summary.id, &now_favorite);
            if(changed) app->detail_favorite = now_favorite ? 1U : 0U;
            bestiary_status(
                app,
                changed ? (now_favorite ? "Favorite added" : "Favorite removed") :
                          "Favorite update failed");
        } else if(app->selection == 15U) {
            bestiary_launch_dnd_monsters(app, &app->detail->summary, NULL, 1U);
        } else if(custom && app->selection == 16U) {
            app->edit_existing = 1U;
            app->selected = app->detail->summary;
            bestiary_enter(app, BestiaryScreenEdit);
        } else if(custom && app->selection == 17U) {
            if(!app->delete_armed) {
                app->delete_armed = 1U;
                bestiary_status(app, "OK again deletes custom");
            } else {
                bool deleted = pocket_monster_delete_custom(app->storage, &app->detail->summary);
                bestiary_release_detail(app);
                bestiary_refresh_count(app);
                bestiary_enter(app, BestiaryScreenHome);
                bestiary_status(app, deleted ? "Custom monster deleted" : "Delete failed");
            }
        }
    } else if(
        event->type == InputTypeLong && event->key == InputKeyOk && custom &&
        app->selection == 17U) {
        if(!app->delete_armed) {
            app->delete_armed = 1U;
            bestiary_status(app, "Hold OK again delete");
        } else {
            bool deleted = pocket_monster_delete_custom(app->storage, &app->detail->summary);
            bestiary_release_detail(app);
            bestiary_refresh_count(app);
            bestiary_enter(app, BestiaryScreenHome);
            bestiary_status(app, deleted ? "Custom monster deleted" : "Delete failed");
        }
    }
}

static void bestiary_handle_detail_line(BestiaryApp* app, const InputEvent* event) {
    uint16_t line_count = bestiary_text_line_count(app->edit_buffer);
    uint16_t maximum = line_count > 5U ? line_count - 5U : 0U;
    if(bestiary_move_event(event) && event->key == InputKeyUp) {
        if(app->detail_line_offset) --app->detail_line_offset;
    } else if(bestiary_move_event(event) && event->key == InputKeyDown) {
        if(app->detail_line_offset < maximum) ++app->detail_line_offset;
    } else if(bestiary_move_event(event) && event->key == InputKeyLeft) {
        app->detail_line_offset = app->detail_line_offset > 5U ? app->detail_line_offset - 5U : 0U;
    } else if(bestiary_move_event(event) && event->key == InputKeyRight) {
        uint16_t next = app->detail_line_offset + 5U;
        app->detail_line_offset = next < maximum ? next : maximum;
    } else if(event->type == InputTypeShort && event->key == InputKeyOk) {
        bestiary_return_to_detail(app);
    }
}

static bool bestiary_launch_dnd(BestiaryApp* app, const char* launch_args) {
    if(!app || !app->dispatcher) return false;

    Loader* loader = furi_record_open(RECORD_LOADER);
    if(!loader) {
        bestiary_status(app, "Loader unavailable");
        return false;
    }

    FURI_LOG_I(TAG, "D&D launch heap before cleanup: %lu", (unsigned long)memmgr_get_free_heap());
    bestiary_release_monster_memory_for_launch(app);
    FURI_LOG_I(TAG, "D&D launch heap after cleanup: %lu", (unsigned long)memmgr_get_free_heap());
    FURI_LOG_I(
        TAG,
        "Queue D&D FAP: %s args_len=%u",
        POCKET_D20_APP_FAP_PATH,
        (unsigned int)(launch_args ? strlen(launch_args) : 0U));
    loader_enqueue_launch(
        loader, POCKET_D20_APP_FAP_PATH, launch_args, LoaderDeferredLaunchFlagGui);
    furi_record_close(RECORD_LOADER);

    view_dispatcher_stop(app->dispatcher);
    return true;
}

static void bestiary_launch_name_sanitize(char* output, size_t size, const char* input) {
    if(!output || !size) return;
    size_t written = 0U;
    if(input) {
        while(*input && written + 1U < size) {
            char value = *input++;
            if(value == ',' || value == ';' || value == '\r' || value == '\n') value = ' ';
            output[written++] = value;
        }
    }
    output[written] = '\0';
}

static bool bestiary_launch_args_append(
    char* args,
    size_t capacity,
    size_t* used,
    uint8_t* emitted,
    const BestiaryLaunchMonster* monster) {
    if(!args || !used || !emitted || !monster) return false;

    char name[POCKET_MONSTER_NAME_LEN];
    bestiary_launch_name_sanitize(name, sizeof(name), monster->name);
    if(!name[0]) bestiary_copy(name, sizeof(name), "Monster");

    uint16_t hp = monster->hit_points > 32767U ? 32767U : monster->hit_points;
    for(uint8_t copy = 0U; copy < monster->quantity && *emitted < POCKET_D20_TRANSFER_MAX;
        ++copy) {
        int written = snprintf(
            args + *used,
            capacity - *used,
            ";%s,%u,%u",
            name,
            (unsigned int)hp,
            (unsigned int)monster->armor_class);
        if(written < 0 || (size_t)written >= capacity - *used) return false;
        *used += (size_t)written;
        ++(*emitted);
    }
    return true;
}

static bool bestiary_launch_args_finish(char* args, size_t capacity, size_t* used) {
    if(!args || !used || *used + 1U >= capacity) return false;
    args[(*used)++] = ';';
    args[*used] = '\0';
    return true;
}

static bool bestiary_launch_dnd_monsters(
    BestiaryApp* app,
    const PocketMonsterSummary* monsters,
    const uint8_t* quantities,
    uint8_t count) {
    if(!app || !monsters || !count) return false;

    BestiaryLaunchMonster launch_monsters[POCKET_MONSTER_ENCOUNTER_MAX];
    uint8_t launch_count = count < POCKET_MONSTER_ENCOUNTER_MAX ? count :
                                                                  POCKET_MONSTER_ENCOUNTER_MAX;
    for(uint8_t index = 0U; index < launch_count; ++index) {
        bestiary_copy(
            launch_monsters[index].name,
            sizeof(launch_monsters[index].name),
            monsters[index].name);
        launch_monsters[index].hit_points = monsters[index].hit_points;
        launch_monsters[index].armor_class = monsters[index].armor_class;
        launch_monsters[index].quantity = quantities ? quantities[index] : 1U;
    }

    /* The summaries needed for the transfer are now compact locals. Free the
       potentially multi-kilobyte browser window and monster offset caches
       before allocating the launch-argument buffer. */
    bestiary_release_window(app);
    pocket_monster_cache_reset();

    char* args = malloc(POCKET_D20_LAUNCH_ARGS_MAX);
    if(!args) {
        bestiary_status(app, "Launch args unavailable");
        return false;
    }

    size_t used = strlen(POCKET_D20_HANDOFF_LAUNCH_ARG);
    memcpy(args, POCKET_D20_HANDOFF_LAUNCH_ARG, used + 1U);
    uint8_t emitted = 0U;
    bool built = true;
    for(uint8_t index = 0U; index < launch_count && emitted < POCKET_D20_TRANSFER_MAX; ++index) {
        if(!launch_monsters[index].quantity) continue;
        if(!bestiary_launch_args_append(
               args, POCKET_D20_LAUNCH_ARGS_MAX, &used, &emitted, &launch_monsters[index])) {
            built = false;
            break;
        }
    }

    if(!emitted || !bestiary_launch_args_finish(args, POCKET_D20_LAUNCH_ARGS_MAX, &used))
        built = false;

    bool launched = false;
    if(built)
        launched = bestiary_launch_dnd(app, args);
    else
        bestiary_status(app, "Initiative args too large");

    free(args);
    return launched;
}

static bool bestiary_launch_saved_dnd(BestiaryApp* app, uint16_t index) {
    if(!app) return false;
    PocketSavedEncounter saved;
    if(!pocket_bestiary_encounter_at(app->storage, index, &saved)) {
        bestiary_status(app, "Saved encounter unavailable");
        return false;
    }

    BestiaryLaunchMonster launch_monsters[POCKET_MONSTER_ENCOUNTER_MAX];
    uint8_t launch_count = 0U;
    for(uint8_t record = 0U; record < saved.count && record < POCKET_MONSTER_ENCOUNTER_MAX;
        ++record) {
        PocketMonsterSummary summary;
        if(!pocket_monster_find(app->storage, saved.monster_ids[record], &summary)) {
            bestiary_status(app, "Saved monster unavailable");
            return false;
        }
        bestiary_copy(
            launch_monsters[launch_count].name,
            sizeof(launch_monsters[launch_count].name),
            summary.name);
        launch_monsters[launch_count].hit_points = summary.hit_points;
        launch_monsters[launch_count].armor_class = summary.armor_class;
        launch_monsters[launch_count].quantity = saved.quantities[record];
        ++launch_count;
    }

    if(!launch_count) {
        bestiary_status(app, "Saved encounter empty");
        return false;
    }

    /* All required monster fields are copied now; release Bestiary's large
       lookup caches before allocating and queuing the cross-FAP arguments. */
    bestiary_release_window(app);
    pocket_monster_cache_reset();

    char* args = malloc(POCKET_D20_LAUNCH_ARGS_MAX);
    if(!args) {
        bestiary_status(app, "Launch args unavailable");
        return false;
    }

    size_t used = strlen(POCKET_D20_HANDOFF_LAUNCH_ARG);
    memcpy(args, POCKET_D20_HANDOFF_LAUNCH_ARG, used + 1U);
    uint8_t emitted = 0U;
    bool built = true;
    for(uint8_t record = 0U; record < launch_count && emitted < POCKET_D20_TRANSFER_MAX;
        ++record) {
        if(!launch_monsters[record].quantity) continue;
        if(!bestiary_launch_args_append(
               args, POCKET_D20_LAUNCH_ARGS_MAX, &used, &emitted, &launch_monsters[record])) {
            built = false;
            break;
        }
    }

    if(!emitted || !bestiary_launch_args_finish(args, POCKET_D20_LAUNCH_ARGS_MAX, &used))
        built = false;

    bool launched = built ? bestiary_launch_dnd(app, args) : false;
    if(!built) bestiary_status(app, "Initiative args too large");
    free(args);
    return launched;
}

static void bestiary_handle_encounter(BestiaryApp* app, const InputEvent* event) {
    if(!app->encounter) return;
    uint16_t count = app->encounter->count + 4U;
    if(bestiary_move_event(event) && event->key == InputKeyUp)
        bestiary_move(app, count, -1);
    else if(bestiary_move_event(event) && event->key == InputKeyDown)
        bestiary_move(app, count, 1);
    else if(
        event->type == InputTypeShort && event->key == InputKeyOk &&
        app->selection < app->encounter->count) {
        PocketMonsterSummary summary = app->encounter->monsters[app->selection];
        app->encounter_return_selection = app->selection;
        app->encounter_return_scroll = app->scroll;
        bestiary_open_detail(app, &summary, BestiaryScreenEncounter);
    } else if(
        event->type == InputTypeShort && event->key == InputKeyOk &&
        app->selection == app->encounter->count) {
        bestiary_enter(app, BestiaryScreenSimulator);
    } else if(
        event->type == InputTypeShort && event->key == InputKeyOk &&
        app->selection == app->encounter->count + 1U) {
        bestiary_begin_text(
            app,
            BestiaryEditEncounterName,
            "Encounter Name",
            app->encounter_name[0] ? app->encounter_name : "Encounter");
    } else if(
        event->type == InputTypeShort && event->key == InputKeyOk &&
        app->selection == app->encounter->count + 2U) {
        bestiary_enter(app, BestiaryScreenWarnings);
    } else if(
        event->type == InputTypeShort && event->key == InputKeyOk &&
        app->selection == app->encounter->count + 3U) {
        bestiary_launch_dnd_monsters(
            app, app->encounter->monsters, app->encounter->quantities, app->encounter->count);
    } else if(event->type == InputTypeLong && event->key == InputKeyOk) {
        bestiary_generate(app);
    }
}

static void bestiary_handle_warnings(BestiaryApp* app, const InputEvent* event) {
    if(event->type == InputTypeShort && event->key == InputKeyOk)
        bestiary_enter(app, BestiaryScreenEncounter);
}

static void bestiary_handle_simulator(BestiaryApp* app, const InputEvent* event) {
    if(!app->encounter) return;
    uint16_t count = app->encounter->count + 2U;
    if(bestiary_move_event(event) && event->key == InputKeyUp)
        bestiary_move(app, count, -1);
    else if(bestiary_move_event(event) && event->key == InputKeyDown)
        bestiary_move(app, count, 1);
    else if(bestiary_move_event(event) && (event->key == InputKeyLeft || event->key == InputKeyRight)) {
        int8_t delta = event->key == InputKeyRight ? 1 : -1;
        if(app->selection == 0U) {
            int16_t value = app->party_level + delta;
            app->party_level = (uint8_t)(value < 1 ? 20 : value > 20 ? 1 : value);
            bestiary_save_party_settings(app);
        } else if(app->selection == 1U) {
            int16_t value = app->party_size + delta;
            app->party_size = (uint8_t)(value < 1 ? 12 : value > 12 ? 1 : value);
            bestiary_save_party_settings(app);
        } else {
            uint8_t monster = (uint8_t)(app->selection - 2U);
            int16_t quantity = app->encounter->quantities[monster] + delta;
            app->encounter->quantities[monster] = (uint8_t)(quantity < 1  ? 1 :
                                                            quantity > 99 ? 99 :
                                                                            quantity);
        }
        PocketEncounterSimulation simulation;
        pocket_monster_simulate(app->encounter, app->party_level, app->party_size, &simulation);
    } else if(event->type == InputTypeShort && event->key == InputKeyOk) {
        bestiary_enter(app, BestiaryScreenEncounter);
    }
}

static bool bestiary_resume_saved(BestiaryApp* app, uint16_t index) {
    PocketSavedEncounter saved;
    if(!pocket_bestiary_encounter_at(app->storage, index, &saved)) return false;
    bestiary_release_encounter(app);
    app->encounter = calloc(1U, sizeof(PocketMonsterEncounter));
    if(!app->encounter) return false;
    app->party_level = saved.party_level;
    app->party_size = saved.party_size;
    bestiary_save_party_settings(app);
    app->difficulty = saved.difficulty < PocketEncounterDifficultyCount ? saved.difficulty :
                                                                          PocketEncounterModerate;
    for(uint8_t record = 0U; record < saved.count; ++record) {
        PocketMonsterSummary summary;
        if(!pocket_monster_find(app->storage, saved.monster_ids[record], &summary)) {
            /* A saved encounter is an atomic composition.  Never resume or hand off a
               silently shortened encounter when one of its stable monster IDs is gone. */
            bestiary_release_encounter(app);
            return false;
        }
        uint8_t destination = app->encounter->count++;
        app->encounter->monsters[destination] = summary;
        app->encounter->quantities[destination] = saved.quantities[record];
    }
    if(app->encounter->count != saved.count) {
        bestiary_release_encounter(app);
        return false;
    }
    PocketEncounterSimulation simulation;
    pocket_monster_simulate(app->encounter, app->party_level, app->party_size, &simulation);
    bestiary_copy(app->encounter_name, sizeof(app->encounter_name), saved.name);
    bestiary_enter(app, BestiaryScreenEncounter);
    bestiary_status(app, "Encounter resumed");
    return true;
}

static void bestiary_handle_saved_encounters(BestiaryApp* app, const InputEvent* event) {
    if(bestiary_move_event(event) && event->key == InputKeyUp)
        bestiary_move(app, app->state_total, -1);
    else if(bestiary_move_event(event) && event->key == InputKeyDown)
        bestiary_move(app, app->state_total, 1);
    else if(
        event->type == InputTypeShort && event->key == InputKeyOk &&
        app->selection < app->state_total) {
        if(!bestiary_resume_saved(app, app->selection))
            bestiary_status(app, "Saved encounter unavailable");
    } else if(
        event->type == InputTypeLong && event->key == InputKeyOk &&
        app->selection < app->state_total) {
        PocketSavedEncounter saved;
        if(!pocket_bestiary_encounter_at(app->storage, app->selection, &saved)) {
            bestiary_status(app, "Saved encounter unavailable");
            return;
        }
        app->encounter_action_index = app->selection;
        app->encounter_delete_armed = 0U;
        bestiary_copy(app->encounter_name, sizeof(app->encounter_name), saved.name);
        bestiary_enter(app, BestiaryScreenEncounterActions);
    }
}

static void bestiary_return_to_saved_actions(BestiaryApp* app, bool changed) {
    uint16_t selected = app->encounter_action_index;
    bestiary_cache_encounter_rows(app);
    bestiary_enter(app, BestiaryScreenSavedEncounters);
    if(app->state_total) {
        app->selection = selected < app->state_total ? selected : app->state_total - 1U;
        app->scroll = app->selection > 4U ? app->selection - 4U : 0U;
    }
    if(changed) bestiary_status(app, "Encounter list updated");
}

static void bestiary_handle_encounter_actions(BestiaryApp* app, const InputEvent* event) {
    if(bestiary_move_event(event) && event->key == InputKeyUp) {
        app->encounter_delete_armed = 0U;
        bestiary_move(app, 6U, -1);
    } else if(bestiary_move_event(event) && event->key == InputKeyDown) {
        app->encounter_delete_armed = 0U;
        bestiary_move(app, 6U, 1);
    } else if(event->type == InputTypeShort && event->key == InputKeyOk) {
        if(app->selection == 0U) {
            if(!bestiary_resume_saved(app, app->encounter_action_index))
                bestiary_status(app, "Saved encounter unavailable");
        } else if(app->selection == 1U) {
            bestiary_launch_saved_dnd(app, app->encounter_action_index);
        } else if(app->selection == 2U) {
            bestiary_begin_text(
                app, BestiaryEditEncounterRename, "Rename Encounter", app->encounter_name);
        } else if(app->selection == 3U) {
            char copy_name[POCKET_BESTIARY_ENCOUNTER_NAME_LEN];
            snprintf(copy_name, sizeof(copy_name), "%.25s Copy", app->encounter_name);
            bestiary_begin_text(app, BestiaryEditEncounterDuplicate, "Duplicate As", copy_name);
        } else if(app->selection == 4U) {
            bool archived =
                pocket_bestiary_encounter_archive(app->storage, app->encounter_action_index);
            if(archived) {
                bestiary_return_to_saved_actions(app, true);
                bestiary_status(app, "Encounter archived");
            } else {
                bestiary_status(app, "Archive failed");
            }
        } else if(app->selection == 5U) {
            if(!app->encounter_delete_armed) {
                app->encounter_delete_armed = 1U;
                bestiary_status(app, "OK again deletes encounter");
            } else {
                bool deleted =
                    pocket_bestiary_encounter_delete(app->storage, app->encounter_action_index);
                app->encounter_delete_armed = 0U;
                if(deleted) {
                    bestiary_return_to_saved_actions(app, true);
                    bestiary_status(app, "Encounter deleted");
                } else {
                    bestiary_status(app, "Delete failed");
                }
            }
        }
    }
}

static void bestiary_handle_filters(BestiaryApp* app, const InputEvent* event) {
    uint16_t count = app->state_total + 1U;
    if(bestiary_move_event(event) && event->key == InputKeyUp)
        bestiary_move(app, count, -1);
    else if(bestiary_move_event(event) && event->key == InputKeyDown)
        bestiary_move(app, count, 1);
    else if(event->type == InputTypeShort && event->key == InputKeyOk) {
        if(app->selection == app->state_total) {
            memset(&app->pending_filter, 0, sizeof(app->pending_filter));
            bestiary_copy(
                app->pending_filter.search, sizeof(app->pending_filter.search), app->search);
            app->pending_filter.max_cr_eighths = app->max_cr_eighths;
            app->pending_filter.type_filter = app->type_filter;
            app->pending_filter.source_filter = app->source_filter;
            app->pending_filter.environment_filter = app->environment_filter;
            app->pending_filter.role_filter = app->role_filter;
            bestiary_begin_text(app, BestiaryEditFilterName, "Filter Name", "My Filter");
        } else {
            PocketBestiaryFilterPreset preset;
            if(!pocket_bestiary_filter_at(app->storage, app->selection, &preset)) return;
            bestiary_copy(app->search, sizeof(app->search), preset.search);
            app->max_cr_eighths = preset.max_cr_eighths;
            app->type_filter = preset.type_filter < sizeof(type_names) / sizeof(type_names[0]) ?
                                   preset.type_filter :
                                   0U;
            app->source_filter = preset.source_filter <
                                         sizeof(source_names) / sizeof(source_names[0]) ?
                                     preset.source_filter :
                                     0U;
            app->environment_filter =
                preset.environment_filter <
                        sizeof(environment_names) / sizeof(environment_names[0]) ?
                    preset.environment_filter :
                    0U;
            app->role_filter = preset.role_filter < sizeof(role_names) / sizeof(role_names[0]) ?
                                   preset.role_filter :
                                   0U;
            app->monster_total_valid = 0U;
            app->page_start = 0U;
            app->list_mode = BestiaryListCatalog;
            if(bestiary_load_window(app)) bestiary_enter(app, BestiaryScreenList);
        }
    } else if(
        event->type == InputTypeLong && event->key == InputKeyOk &&
        app->selection < app->state_total) {
        bool deleted = pocket_bestiary_filter_delete(app->storage, app->selection);
        bestiary_cache_filter_rows(app);
        bestiary_status(app, deleted ? "Filter deleted" : "Delete failed");
    }
}

static void bestiary_handle_packs(BestiaryApp* app, const InputEvent* event) {
    uint16_t count = app->state_total + 1U;
    if(bestiary_move_event(event) && event->key == InputKeyUp)
        bestiary_move(app, count, -1);
    else if(bestiary_move_event(event) && event->key == InputKeyDown)
        bestiary_move(app, count, 1);
    else if(event->type == InputTypeShort && event->key == InputKeyOk) {
        bool changed = false;
        if(app->selection == app->state_total) {
            changed = pocket_pack_install_inbox(
                app->storage, PocketPackMonster, app->status, sizeof(app->status));
        } else {
            PocketPackSummary pack;
            if(pocket_pack_at(app->storage, PocketPackMonster, app->selection, &pack)) {
                changed = pocket_pack_set_enabled(
                    app->storage, PocketPackMonster, pack.id, !pack.enabled);
                bestiary_status(app, changed ? "Pack state updated" : "Pack update failed");
            }
        }
        if(changed) {
            pocket_monster_cache_reset();
            app->monster_total_valid = 0U;
            bestiary_cache_pack_rows(app);
        }
    }
}

static void bestiary_handle_edit(BestiaryApp* app, const InputEvent* event) {
    PocketMonsterDetail* m = app->detail;
    if(!m) return;
    if(bestiary_move_event(event) && event->key == InputKeyUp)
        bestiary_move(app, 24U, -1);
    else if(bestiary_move_event(event) && event->key == InputKeyDown)
        bestiary_move(app, 24U, 1);
    else if(bestiary_move_event(event) && (event->key == InputKeyLeft || event->key == InputKeyRight)) {
        int8_t delta = event->key == InputKeyRight ? 1 : -1;
        if(app->selection == 1U)
            m->summary.cr_eighths = bestiary_cycle_cr(m->summary.cr_eighths, delta);
        else if(app->selection == 2U) {
            int32_t value = (int32_t)m->summary.xp + delta * 25;
            m->summary.xp = (uint32_t)(value < 10 ? 10 : value > 155000 ? 155000 : value);
        } else if(app->selection == 3U) {
            int16_t value = m->summary.armor_class + delta;
            m->summary.armor_class = (uint8_t)(value < 1 ? 1 : value > 30 ? 30 : value);
        } else if(app->selection == 4U) {
            int32_t value = m->summary.hit_points + delta;
            m->summary.hit_points = (uint16_t)(value < 1 ? 1 : value > 999 ? 999 : value);
        } else if(app->selection == 6U || app->selection == 7U) {
            const char* const* names = app->selection == 6U ? environment_names : role_names;
            uint8_t count = app->selection == 6U ?
                                sizeof(environment_names) / sizeof(environment_names[0]) :
                                sizeof(role_names) / sizeof(role_names[0]);
            const char* current = app->selection == 6U ? m->summary.environment : m->summary.role;
            uint8_t current_index = 1U;
            for(uint8_t i = 1U; i < count; ++i)
                if(!strcmp(current, names[i])) current_index = i;
            int16_t value = current_index + delta;
            if(value < 1) value = count - 1U;
            if(value >= count) value = 1U;
            bestiary_copy(
                app->selection == 6U ? m->summary.environment : m->summary.role,
                app->selection == 6U ? sizeof(m->summary.environment) : sizeof(m->summary.role),
                names[value]);
        } else if(app->selection >= 10U && app->selection <= 15U) {
            uint8_t ability = app->selection - 10U;
            int16_t value = m->abilities[ability] + delta;
            m->abilities[ability] = value < 1 ? 1 : value > 30 ? 30 : value;
        }
    } else if(event->type == InputTypeShort && event->key == InputKeyOk) {
        static const BestiaryEdit targets[] = {
            BestiaryEditName,
            BestiaryEditType,
            BestiaryEditSize,
            BestiaryEditSpeed,
            BestiaryEditSkills,
            BestiaryEditDefenses,
            BestiaryEditSenses,
            BestiaryEditLanguages,
            BestiaryEditTraits,
            BestiaryEditActions,
            BestiaryEditExtra};
        static const uint8_t fields[] = {0U, 5U, 8U, 9U, 16U, 17U, 18U, 19U, 20U, 21U, 22U};
        static const char* const titles[] = {
            "Monster Name",
            "Creature Type",
            "Size/Alignment",
            "Movement",
            "Skills",
            "Defenses",
            "Senses",
            "Languages",
            "Traits",
            "Actions",
            "Extra"};
        char* values[] = {
            m->summary.name,
            m->summary.type,
            m->size_alignment,
            m->speed,
            m->skills,
            m->defenses,
            m->senses,
            m->languages,
            m->traits,
            m->actions,
            m->extra};
        for(uint8_t i = 0U; i < sizeof(fields); ++i)
            if(app->selection == fields[i]) {
                bestiary_begin_text(app, targets[i], titles[i], values[i]);
                return;
            }
        if(app->selection == 23U) {
            bool saved = app->edit_existing ? pocket_monster_update_custom(app->storage, m) :
                                              pocket_monster_save_custom(app->storage, m);
            if(saved) {
                bestiary_refresh_count(app);
                app->selected = m->summary;
                app->edit_existing = 0U;
                bestiary_enter(app, BestiaryScreenDetail);
            }
            bestiary_status(app, saved ? "Custom monster saved" : "Custom save failed");
        }
    }
}

static bool bestiary_input(InputEvent* event, void* context) {
    BestiaryApp* app = context;
    if(event->type == InputTypeLong && event->key == InputKeyBack) {
        if(app->screen == BestiaryScreenHome) {
            view_dispatcher_stop(app->dispatcher);
        } else {
            bestiary_release_window(app);
            bestiary_release_detail(app);
            bestiary_release_encounter(app);
            bestiary_enter(app, BestiaryScreenHome);
        }
        bestiary_refresh(app);
        return true;
    }
    if(event->type == InputTypeShort && event->key == InputKeyBack) {
        bestiary_back(app);
        bestiary_refresh(app);
        return true;
    }
    switch(app->screen) {
    case BestiaryScreenHome:
        bestiary_handle_home(app, event);
        break;
    case BestiaryScreenList:
        bestiary_handle_list(app, event);
        break;
    case BestiaryScreenDetail:
        bestiary_handle_detail(app, event);
        break;
    case BestiaryScreenDetailLine:
        bestiary_handle_detail_line(app, event);
        break;
    case BestiaryScreenEncounter:
        bestiary_handle_encounter(app, event);
        break;
    case BestiaryScreenSimulator:
        bestiary_handle_simulator(app, event);
        break;
    case BestiaryScreenWarnings:
        bestiary_handle_warnings(app, event);
        break;
    case BestiaryScreenSavedEncounters:
        bestiary_handle_saved_encounters(app, event);
        break;
    case BestiaryScreenEncounterActions:
        bestiary_handle_encounter_actions(app, event);
        break;
    case BestiaryScreenFilters:
        bestiary_handle_filters(app, event);
        break;
    case BestiaryScreenPacks:
        bestiary_handle_packs(app, event);
        break;
    case BestiaryScreenDiagnostics:
        if(event->type == InputTypeShort && event->key == InputKeyOk)
            bestiary_diagnose(app);
        else if(bestiary_move_event(event) && event->key == InputKeyUp)
            bestiary_move(app, 9U, -1);
        else if(bestiary_move_event(event) && event->key == InputKeyDown)
            bestiary_move(app, 9U, 1);
        break;
    case BestiaryScreenEdit:
        bestiary_handle_edit(app, event);
        break;
    }
    bestiary_refresh(app);
    return true;
}

static void bestiary_marquee_timer_callback(void* context) {
    BestiaryApp* app = context;
    view_dispatcher_send_custom_event(app->dispatcher, BESTIARY_MARQUEE_EVENT);
}

static void bestiary_go_home(BestiaryApp* app) {
    bestiary_release_window(app);
    bestiary_release_detail(app);
    bestiary_release_encounter(app);
    bestiary_enter(app, BestiaryScreenHome);
}

static void bestiary_input_events_callback(const void* value, void* context) {
    BestiaryApp* app = context;
    const InputEvent* event = value;
    if(app->text_input_active && event && event->key == InputKeyBack &&
       event->type == InputTypeLong)
        view_dispatcher_send_custom_event(app->dispatcher, BESTIARY_LONG_BACK_EVENT);
}

static bool bestiary_custom_event(void* context, uint32_t event) {
    BestiaryApp* app = context;
    if(event == BESTIARY_LONG_BACK_EVENT) {
        app->text_input_active = 0U;
        app->edit = BestiaryEditNone;
        view_dispatcher_switch_to_view(app->dispatcher, BestiaryViewMain);
        bestiary_go_home(app);
        bestiary_refresh(app);
        return true;
    }
    if(event != BESTIARY_MARQUEE_EVENT) return false;
    ++bestiary_marquee_offset;
    bestiary_refresh(app);
    return true;
}

static bool bestiary_navigation(void* context) {
    BestiaryApp* app = context;
    app->text_input_active = 0U;
    app->edit = BestiaryEditNone;
    view_dispatcher_switch_to_view(app->dispatcher, BestiaryViewMain);
    bestiary_refresh(app);
    return true;
}

static BestiaryApp* bestiary_alloc(void) {
    BestiaryApp* app = calloc(1U, sizeof(BestiaryApp));
    if(!app) return NULL;
    app->party_level = 1U;
    app->party_size = 4U;
    app->difficulty = PocketEncounterModerate;
    app->allow_repeats = 1U;

    app->storage = furi_record_open(RECORD_STORAGE);
    if(!app->storage) goto fail;
    pocket_bestiary_party_settings_load(app->storage, &app->party_level, &app->party_size);

    uint16_t migrated_files = 0U;
    uint16_t recovered = 0U;
    uint16_t rolled_back = 0U;
    bool migration_ok = pocket_monster_migrate_legacy_custom(app->storage, &migrated_files);
    bool recovery_ok = migration_ok &&
                       pocket_monster_recover_user_pack(app->storage, &recovered, &rolled_back);
    bool installed_packs_ok = pocket_pack_rebuild_enabled(app->storage, PocketPackMonster);
    pocket_monster_cache_reset();

    app->gui = furi_record_open(RECORD_GUI);
    if(!app->gui) goto fail;
    app->dispatcher = view_dispatcher_alloc();
    if(!app->dispatcher) goto fail;
    view_dispatcher_set_event_callback_context(app->dispatcher, app);
    view_dispatcher_set_navigation_event_callback(app->dispatcher, bestiary_navigation);
    view_dispatcher_set_custom_event_callback(app->dispatcher, bestiary_custom_event);

    app->input_events = furi_record_open(RECORD_INPUT_EVENTS);
    if(!app->input_events) goto fail;
    app->input_subscription =
        furi_pubsub_subscribe(app->input_events, bestiary_input_events_callback, app);
    if(!app->input_subscription) goto fail;

    app->view = view_alloc();
    if(!app->view) goto fail;
    view_allocate_model(app->view, ViewModelTypeLockFree, sizeof(BestiaryApp*));
    BestiaryApp** model = view_get_model(app->view);
    if(!model) goto fail;
    *model = app;
    view_commit_model(app->view, false);
    view_set_context(app->view, app);
    view_set_draw_callback(app->view, bestiary_draw);
    view_set_input_callback(app->view, bestiary_input);

    app->marquee_timer =
        furi_timer_alloc(bestiary_marquee_timer_callback, FuriTimerTypePeriodic, app);
    if(!app->marquee_timer) goto fail;

    view_dispatcher_add_view(app->dispatcher, BestiaryViewMain, app->view);
    view_dispatcher_attach_to_gui(app->dispatcher, app->gui, ViewDispatcherTypeFullscreen);
    furi_timer_start(app->marquee_timer, furi_ms_to_ticks(BESTIARY_MARQUEE_MS));
    if(!installed_packs_ok)
        bestiary_status(app, "Installed pack check failed");
    else if(!migration_ok)
        bestiary_status(app, "Custom migration failed");
    else if(!recovery_ok)
        bestiary_status(app, "Custom recovery failed");
    else if(migrated_files)
        bestiary_status(app, "Custom monsters migrated");
    else if(recovered || rolled_back)
        bestiary_status(app, "Custom pack recovered");
    return app;

fail:
    /* Nothing is attached to the dispatcher until all allocations above succeed,
       so a partial startup can be unwound without touching an unregistered view. */
    if(app->marquee_timer) furi_timer_free(app->marquee_timer);
    if(app->view) view_free(app->view);
    if(app->input_subscription && app->input_events)
        furi_pubsub_unsubscribe(app->input_events, app->input_subscription);
    if(app->input_events) furi_record_close(RECORD_INPUT_EVENTS);
    if(app->dispatcher) view_dispatcher_free(app->dispatcher);
    if(app->gui) furi_record_close(RECORD_GUI);
    if(app->storage) furi_record_close(RECORD_STORAGE);
    pocket_monster_cache_reset();
    free(app);
    return NULL;
}

static void bestiary_free(BestiaryApp* app) {
    if(!app) return;
    pocket_monster_cache_reset();
    bestiary_release_window(app);
    bestiary_release_detail(app);
    bestiary_release_encounter(app);
    if(app->dispatcher && app->text_input)
        view_dispatcher_remove_view(app->dispatcher, BestiaryViewText);
    if(app->dispatcher && app->view)
        view_dispatcher_remove_view(app->dispatcher, BestiaryViewMain);
    if(app->text_input) text_input_free(app->text_input);
    if(app->marquee_timer) {
        furi_timer_stop(app->marquee_timer);
        furi_timer_free(app->marquee_timer);
    }
    if(app->input_subscription && app->input_events)
        furi_pubsub_unsubscribe(app->input_events, app->input_subscription);
    if(app->input_events) furi_record_close(RECORD_INPUT_EVENTS);
    if(app->view) view_free(app->view);
    if(app->dispatcher) view_dispatcher_free(app->dispatcher);
    if(app->gui) furi_record_close(RECORD_GUI);
    if(app->storage) furi_record_close(RECORD_STORAGE);
    free(app);
}

int32_t dolphin_bestiary_app(void* context) {
    UNUSED(context);
    BestiaryApp* app = bestiary_alloc();
    if(!app) {
        FURI_LOG_E(TAG, "Allocation failed");
        return -1;
    }
    view_dispatcher_switch_to_view(app->dispatcher, BestiaryViewMain);
    view_dispatcher_run(app->dispatcher);
    bestiary_free(app);
    return 0;
}
