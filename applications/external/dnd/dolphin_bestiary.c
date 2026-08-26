#include "pocket_d20_monsters.h"

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

#define TAG "DolphinBestiary"
#define BESTIARY_WINDOW 50U
#define BESTIARY_MARQUEE_EVENT 0xB357U
#define BESTIARY_LONG_BACK_EVENT 0xB358U
#define BESTIARY_MARQUEE_MS 350U

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
} BestiaryEdit;

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

static uint8_t bestiary_marquee_offset = 0U;

static const char* const type_names[] = {
    "Any", "Aberration", "Beast", "Celestial", "Construct", "Dragon", "Elemental",
    "Fey", "Fiend", "Giant", "Humanoid", "Monstrosity", "Ooze", "Plant", "Swarm",
    "Undead"};
static const char* const environment_names[] = {
    "Any", "Aquatic", "Dungeon", "Planar", "Urban", "Wilderness"};
static const char* const source_names[] = {
    "Any", "Open Reference", "D&Dolphins", "Custom"};
static const char* const role_names[] = {
    "Any", "Leader", "Controller", "Skirmisher", "Artillery", "Brute", "Minion"};
static const char* const difficulty_names[] = {"Low", "Moderate", "High"};
static const char* const template_names[] = {"Balanced", "Horde", "Elite"};
static const uint8_t cr_choices[] = {
    0U, 1U, 2U, 4U, 8U, 16U, 24U, 32U, 40U, 48U, 56U, 64U, 72U, 80U, 96U,
    112U, 128U, 144U, 160U, 200U, 240U};

static void bestiary_copy(char* output, size_t size, const char* value) {
    if(!size) return;
    strncpy(output, value ? value : "", size - 1U);
    output[size - 1U] = '\0';
}

static void bestiary_status(BestiaryApp* app, const char* value) {
    bestiary_copy(app->status, sizeof(app->status), value);
}

static void bestiary_refresh(BestiaryApp* app) {
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
        bestiary_filter,
        app,
        app->page_start,
        app->window,
        BESTIARY_WINDOW,
        total_output);
    app->monster_total_valid = 1U;
    if(app->page_start >= app->monster_total && app->page_start) {
        app->page_start =
            ((app->monster_total ? app->monster_total - 1U : 0U) / BESTIARY_WINDOW) *
            BESTIARY_WINDOW;
        app->window_count = pocket_monster_query(
            app->storage,
            bestiary_filter,
            app,
            app->page_start,
            app->window,
            BESTIARY_WINDOW,
            NULL);
    }
    return true;
}

static void bestiary_refresh_count(BestiaryApp* app) {
    pocket_monster_query(
        app->storage, bestiary_filter, app, 0U, NULL, 0U, &app->monster_total);
    app->monster_total_valid = 1U;
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

static void bestiary_cr(char* output, size_t size, uint8_t eighths) {
    if(eighths == 1U) snprintf(output, size, "1/8");
    else if(eighths == 2U) snprintf(output, size, "1/4");
    else if(eighths == 4U) snprintf(output, size, "1/2");
    else snprintf(output, size, "%u", eighths / 8U);
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
    if(selected && length > 20U) {
        size_t cycle = length + 4U;
        size_t start = bestiary_marquee_offset % cycle;
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
        canvas_draw_box(canvas, 0, y, 128, 10);
        canvas_set_color(canvas, ColorWhite);
    }
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 3, y + 8U, display);
    canvas_set_color(canvas, ColorBlack);
}

static void bestiary_rows(
    Canvas* canvas,
    BestiaryApp* app,
    const char* const* rows,
    uint16_t count) {
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
    snprintf(environment, sizeof(environment), "Browse Env: %s", environment_names[app->environment_filter]);
    snprintf(role, sizeof(role), "Browse Role: %s", role_names[app->role_filter]);
    snprintf(party_level, sizeof(party_level), "Party Level: %u", app->party_level);
    snprintf(party_size, sizeof(party_size), "Party Size: %u", app->party_size);
    snprintf(difficulty, sizeof(difficulty), "Difficulty: %s", difficulty_names[app->difficulty]);
    snprintf(encounter_environment, sizeof(encounter_environment), "Encounter Env: %s", environment_names[app->encounter_environment]);
    snprintf(encounter_role, sizeof(encounter_role), "Encounter Role: %s", role_names[app->encounter_role]);
    snprintf(repeats, sizeof(repeats), "Repeat Types: %s", app->allow_repeats ? "Yes" : "No");
    snprintf(template_row, sizeof(template_row), "Template: %s", template_names[app->encounter_template]);
    const char* rows[] = {
        browse, search, cr, type, source, environment, role, party_level, party_size,
        difficulty, encounter_environment, encounter_role, repeats, template_row,
        "Generate Encounter", "Pack Diagnostics", "Create Custom Monster",
        "Open Dungeons & Dolphins"};
    bestiary_header(canvas, "Bestiary v" FAP_VERSION, app->status);
    bestiary_rows(canvas, app, rows, 18U);
}

static void bestiary_draw_list(Canvas* canvas, BestiaryApp* app) {
    char page[24];
    uint16_t page_count = (app->monster_total + BESTIARY_WINDOW - 1U) / BESTIARY_WINDOW;
    snprintf(page, sizeof(page), "Page %u/%u <>", app->page_start / BESTIARY_WINDOW + 1U,
             page_count ? page_count : 1U);
    bestiary_header(canvas, "Monster Catalog", app->status[0] ? app->status : page);
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
    bestiary_cr(cr, sizeof(cr), m->summary.cr_eighths);
    snprintf(core, sizeof(core), "CR %s XP %lu AC%u HP%u", cr,
             (unsigned long)m->summary.xp, m->summary.armor_class, m->summary.hit_points);
    snprintf(abilities, sizeof(abilities), "S%d D%d C%d I%d W%d C%d",
             m->abilities[0], m->abilities[1], m->abilities[2], m->abilities[3],
             m->abilities[4], m->abilities[5]);
    snprintf(type, sizeof(type), "Type: %s", m->summary.type);
    snprintf(source, sizeof(source), "Source: %s", m->summary.source);
    snprintf(role, sizeof(role), "Role: %s", m->summary.role);
    snprintf(
        delete_row,
        sizeof(delete_row),
        "%s",
        app->delete_armed ? "OK again: delete custom" : "Delete Custom Monster");
    const char* rows[] = {core, type, source, role, m->size_alignment, m->speed,
        abilities, m->skills, m->defenses, m->senses, m->languages, m->traits, m->actions,
        m->extra, "Edit Custom Monster", delete_row};
    uint8_t row_count = !strcmp(m->summary.source, "Custom") ? 16U : 14U;
    bestiary_header(canvas, m->summary.name, app->status);
    bestiary_rows(canvas, app, rows, row_count);
}

static const char* bestiary_next_text_line(
    const char* cursor,
    char* output,
    size_t output_size) {
    if(!cursor || !output_size) return NULL;
    while(*cursor == ' ' || *cursor == '\r' || *cursor == '\n') ++cursor;
    if(!*cursor) {
        output[0] = '\0';
        return NULL;
    }
    const size_t maximum = 20U;
    size_t length = 0U;
    size_t last_space = SIZE_MAX;
    while(cursor[length] && cursor[length] != '\r' && cursor[length] != '\n' &&
          length < maximum) {
        if(cursor[length] == ' ') last_space = length;
        ++length;
    }
    if(length == maximum && cursor[length] && cursor[length] != ' ' &&
       cursor[length] != '\r' && cursor[length] != '\n' && last_space != SIZE_MAX)
        length = last_space;
    if(length >= output_size) length = output_size - 1U;
    memcpy(output, cursor, length);
    while(length && output[length - 1U] == ' ') --length;
    output[length] = '\0';
    cursor += length;
    while(*cursor == ' ' || *cursor == '\r' || *cursor == '\n') ++cursor;
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
    snprintf(title, sizeof(title), "Encounter %lu/%lu XP",
             (unsigned long)app->encounter->spent, (unsigned long)app->encounter->budget);
    bestiary_header(canvas, title, app->status);
    for(uint8_t visible = 0U; visible < 5U; ++visible) {
        uint16_t index = app->scroll + visible;
        if(index >= app->encounter->count) break;
        char row[64], cr[8];
        bestiary_cr(cr, sizeof(cr), app->encounter->monsters[index].cr_eighths);
        snprintf(row, sizeof(row), "%ux %s CR%s", app->encounter->quantities[index],
                 app->encounter->monsters[index].name, cr);
        bestiary_row(canvas, visible, index == app->selection, row);
    }
}

static void bestiary_draw_diagnostics(Canvas* canvas, BestiaryApp* app) {
    char total[32], valid[32], invalid[32], bundled[32], custom[32], recovered[32],
        rolled_back[32], heap[32];
    snprintf(
        total,
        sizeof(total),
        "Records: %u",
        app->diagnostic_valid + app->diagnostic_invalid);
    snprintf(valid, sizeof(valid), "Valid: %u", app->diagnostic_valid);
    snprintf(invalid, sizeof(invalid), "Invalid: %u", app->diagnostic_invalid);
    snprintf(bundled, sizeof(bundled), "Bundled Pack: v%u", app->bundled_version);
    snprintf(custom, sizeof(custom), "Custom Pack: %s", app->custom_present ? "present" : "none");
    snprintf(recovered, sizeof(recovered), "Recovered: %u", app->diagnostic_recovered);
    snprintf(rolled_back, sizeof(rolled_back), "Rolled Back: %u", app->diagnostic_rolled_back);
    snprintf(heap, sizeof(heap), "Free Heap: %lu", (unsigned long)memmgr_get_free_heap());
    const char* rows[] = {total, valid, invalid, bundled, custom, recovered, rolled_back,
                          heap, "OK: Rescan"};
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
    const char* rows[] = {m->summary.name, cr, xp, ac, hp, m->summary.type, environment,
        role, m->size_alignment, m->speed, ability[0], ability[1], ability[2], ability[3],
        ability[4], ability[5], m->skills, m->defenses, m->senses, m->languages, m->traits,
        m->actions, m->extra, app->edit_existing ? "Update Custom Monster" : "Save Custom Monster"};
    bestiary_header(canvas, app->edit_existing ? "Edit Custom" : "New Custom", app->status);
    bestiary_rows(canvas, app, rows, 24U);
}

static void bestiary_draw(Canvas* canvas, void* model) {
    BestiaryApp* app = *(BestiaryApp**)model;
    canvas_clear(canvas);
    switch(app->screen) {
    case BestiaryScreenHome: bestiary_draw_home(canvas, app); break;
    case BestiaryScreenList: bestiary_draw_list(canvas, app); break;
    case BestiaryScreenDetail: bestiary_draw_detail(canvas, app); break;
    case BestiaryScreenDetailLine: bestiary_draw_detail_line(canvas, app); break;
    case BestiaryScreenEncounter: bestiary_draw_encounter(canvas, app); break;
    case BestiaryScreenDiagnostics: bestiary_draw_diagnostics(canvas, app); break;
    case BestiaryScreenEdit: bestiary_draw_edit(canvas, app); break;
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
    switch(app->edit) {
    case BestiaryEditSearch: bestiary_copy(app->search, sizeof(app->search), app->edit_buffer); break;
    case BestiaryEditName: if(m) bestiary_copy(m->summary.name, sizeof(m->summary.name), app->edit_buffer); break;
    case BestiaryEditType: if(m) bestiary_copy(m->summary.type, sizeof(m->summary.type), app->edit_buffer); break;
    case BestiaryEditSize: if(m) bestiary_copy(m->size_alignment, sizeof(m->size_alignment), app->edit_buffer); break;
    case BestiaryEditSpeed: if(m) bestiary_copy(m->speed, sizeof(m->speed), app->edit_buffer); break;
    case BestiaryEditSkills: if(m) bestiary_copy(m->skills, sizeof(m->skills), app->edit_buffer); break;
    case BestiaryEditDefenses: if(m) bestiary_copy(m->defenses, sizeof(m->defenses), app->edit_buffer); break;
    case BestiaryEditSenses: if(m) bestiary_copy(m->senses, sizeof(m->senses), app->edit_buffer); break;
    case BestiaryEditLanguages: if(m) bestiary_copy(m->languages, sizeof(m->languages), app->edit_buffer); break;
    case BestiaryEditTraits: if(m) bestiary_copy(m->traits, sizeof(m->traits), app->edit_buffer); break;
    case BestiaryEditActions: if(m) bestiary_copy(m->actions, sizeof(m->actions), app->edit_buffer); break;
    case BestiaryEditExtra: if(m) bestiary_copy(m->extra, sizeof(m->extra), app->edit_buffer); break;
    default: break;
    }
    app->edit = BestiaryEditNone;
    if(app->screen == BestiaryScreenHome) app->monster_total_valid = 0U;
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
    default: return;
    }

    if(value)
        bestiary_copy(
            app->edit_buffer,
            sizeof(app->edit_buffer),
            value[0] ? value : "(none)");
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
    bestiary_copy(app->detail->summary.environment, sizeof(app->detail->summary.environment), "Wilderness");
    bestiary_copy(app->detail->summary.role, sizeof(app->detail->summary.role), "Skirmisher");
    bestiary_copy(app->detail->size_alignment, sizeof(app->detail->size_alignment), "Medium Monstrosity");
    bestiary_copy(app->detail->speed, sizeof(app->detail->speed), "30 ft.");
    bestiary_copy(app->detail->senses, sizeof(app->detail->senses), "Passive Perception 10");
    bestiary_copy(app->detail->languages, sizeof(app->detail->languages), "None");
    bestiary_copy(app->detail->actions, sizeof(app->detail->actions), "None");
    for(uint8_t i = 0U; i < 6U; ++i) app->detail->abilities[i] = 10;
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
            bestiary_load_window(app);
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
    if(bestiary_move_event(event) && event->key == InputKeyUp) bestiary_move(app, 18U, -1);
    else if(bestiary_move_event(event) && event->key == InputKeyDown) bestiary_move(app, 18U, 1);
    else if(bestiary_move_event(event) &&
            (event->key == InputKeyLeft || event->key == InputKeyRight)) {
        int8_t delta = event->key == InputKeyRight ? 1 : -1;
        if(app->selection == 2U) app->max_cr_eighths = bestiary_cycle_cr(app->max_cr_eighths, delta);
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
            uint8_t* target = app->selection == 5U ? &app->environment_filter : &app->encounter_environment;
            int16_t value = *target + delta;
            if(value < 0) value = sizeof(environment_names) / sizeof(environment_names[0]) - 1U;
            if(value >= (int16_t)(sizeof(environment_names) / sizeof(environment_names[0]))) value = 0;
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
        } else if(app->selection == 8U) {
            int16_t value = app->party_size + delta;
            app->party_size = (uint8_t)(value < 1 ? 12 : value > 12 ? 1 : value);
        } else if(app->selection == 9U) {
            int16_t value = app->difficulty + delta;
            app->difficulty = (uint8_t)(value < 0 ? 2 : value > 2 ? 0 : value);
        } else if(app->selection == 12U) app->allow_repeats = !app->allow_repeats;
        else if(app->selection == 13U) {
            int16_t value = app->encounter_template + delta;
            app->encounter_template = (uint8_t)(value < 0 ? 2 : value > 2 ? 0 : value);
        } else return;
        if(app->selection >= 2U && app->selection <= 6U)
            app->monster_total_valid = 0U;
    } else if(event->type == InputTypeShort && event->key == InputKeyOk) {
        if(app->selection == 0U) {
            app->page_start = 0U;
            if(bestiary_load_window(app)) bestiary_enter(app, BestiaryScreenList);
        } else if(app->selection == 1U) {
            bestiary_begin_text(app, BestiaryEditSearch, "Monster Search", app->search);
        } else if(app->selection == 14U) bestiary_generate(app);
        else if(app->selection == 15U) bestiary_diagnose(app);
        else if(app->selection == 16U) bestiary_new_custom(app);
        else if(app->selection == 17U) {
            Loader* loader = furi_record_open(RECORD_LOADER);
            loader_enqueue_launch(
                loader,
                "/ext/apps/Games/dungeons_and_dolphins.fap",
                NULL,
                LoaderDeferredLaunchFlagGui);
            furi_record_close(RECORD_LOADER);
            view_dispatcher_stop(app->dispatcher);
        }
    }
}

static void bestiary_handle_list(BestiaryApp* app, const InputEvent* event) {
    if(bestiary_move_event(event) && event->key == InputKeyUp) bestiary_move(app, app->window_count, -1);
    else if(bestiary_move_event(event) && event->key == InputKeyDown) bestiary_move(app, app->window_count, 1);
    else if(bestiary_move_event(event) && (event->key == InputKeyLeft || event->key == InputKeyRight)) {
        uint16_t next = app->page_start;
        if(event->key == InputKeyRight && app->page_start + BESTIARY_WINDOW < app->monster_total)
            next += BESTIARY_WINDOW;
        else if(event->key == InputKeyLeft && app->page_start >= BESTIARY_WINDOW)
            next -= BESTIARY_WINDOW;
        if(next != app->page_start) {
            app->page_start = next;
            app->selection = app->scroll = 0U;
            bestiary_load_window(app);
        }
    } else if(event->type == InputTypeShort && event->key == InputKeyOk &&
              app->selection < app->window_count) {
        PocketMonsterSummary summary = app->window[app->selection];
        bestiary_release_window(app);
        bestiary_open_detail(app, &summary, BestiaryScreenList);
    }
}

static void bestiary_handle_detail(BestiaryApp* app, const InputEvent* event) {
    if(!app->detail) return;
    bool custom = !strcmp(app->detail->summary.source, "Custom");
    uint8_t row_count = custom ? 16U : 14U;
    if(bestiary_move_event(event) && event->key == InputKeyUp) {
        app->delete_armed = 0U;
        bestiary_move(app, row_count, -1);
    } else if(bestiary_move_event(event) && event->key == InputKeyDown) {
        app->delete_armed = 0U;
        bestiary_move(app, row_count, 1);
    } else if(event->type == InputTypeShort && event->key == InputKeyOk) {
        if(app->selection < 14U) {
            bestiary_open_detail_line(app);
        } else if(custom && app->selection == 14U) {
            app->edit_existing = 1U;
            app->selected = app->detail->summary;
            bestiary_enter(app, BestiaryScreenEdit);
        } else if(custom && app->selection == 15U) {
            if(!app->delete_armed) {
                app->delete_armed = 1U;
                bestiary_status(app, "OK again deletes custom");
            } else {
                bool deleted =
                    pocket_monster_delete_custom(app->storage, &app->detail->summary);
                bestiary_release_detail(app);
                bestiary_refresh_count(app);
                bestiary_enter(app, BestiaryScreenHome);
                bestiary_status(
                    app, deleted ? "Custom monster deleted" : "Delete failed");
            }
        }
    } else if(event->type == InputTypeLong && event->key == InputKeyOk &&
              custom) {
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
        app->detail_line_offset =
            app->detail_line_offset > 5U ? app->detail_line_offset - 5U : 0U;
    } else if(bestiary_move_event(event) && event->key == InputKeyRight) {
        uint16_t next = app->detail_line_offset + 5U;
        app->detail_line_offset = next < maximum ? next : maximum;
    } else if(event->type == InputTypeShort && event->key == InputKeyOk) {
        bestiary_return_to_detail(app);
    }
}

static void bestiary_handle_encounter(BestiaryApp* app, const InputEvent* event) {
    if(!app->encounter) return;
    if(bestiary_move_event(event) && event->key == InputKeyUp) bestiary_move(app, app->encounter->count, -1);
    else if(bestiary_move_event(event) && event->key == InputKeyDown) bestiary_move(app, app->encounter->count, 1);
    else if(event->type == InputTypeShort && event->key == InputKeyOk &&
            app->selection < app->encounter->count) {
        PocketMonsterSummary summary = app->encounter->monsters[app->selection];
        app->encounter_return_selection = app->selection;
        app->encounter_return_scroll = app->scroll;
        bestiary_open_detail(app, &summary, BestiaryScreenEncounter);
    }
    else if(event->type == InputTypeLong && event->key == InputKeyOk) bestiary_generate(app);
}

static void bestiary_handle_edit(BestiaryApp* app, const InputEvent* event) {
    PocketMonsterDetail* m = app->detail;
    if(!m) return;
    if(bestiary_move_event(event) && event->key == InputKeyUp) bestiary_move(app, 24U, -1);
    else if(bestiary_move_event(event) && event->key == InputKeyDown) bestiary_move(app, 24U, 1);
    else if(bestiary_move_event(event) && (event->key == InputKeyLeft || event->key == InputKeyRight)) {
        int8_t delta = event->key == InputKeyRight ? 1 : -1;
        if(app->selection == 1U) m->summary.cr_eighths = bestiary_cycle_cr(m->summary.cr_eighths, delta);
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
            for(uint8_t i = 1U; i < count; ++i) if(!strcmp(current, names[i])) current_index = i;
            int16_t value = current_index + delta;
            if(value < 1) value = count - 1U;
            if(value >= count) value = 1U;
            bestiary_copy(app->selection == 6U ? m->summary.environment : m->summary.role,
                app->selection == 6U ? sizeof(m->summary.environment) : sizeof(m->summary.role), names[value]);
        } else if(app->selection >= 10U && app->selection <= 15U) {
            uint8_t ability = app->selection - 10U;
            int16_t value = m->abilities[ability] + delta;
            m->abilities[ability] = value < 1 ? 1 : value > 30 ? 30 : value;
        }
    } else if(event->type == InputTypeShort && event->key == InputKeyOk) {
        static const BestiaryEdit targets[] = {
            BestiaryEditName, BestiaryEditType, BestiaryEditSize, BestiaryEditSpeed,
            BestiaryEditSkills, BestiaryEditDefenses, BestiaryEditSenses,
            BestiaryEditLanguages, BestiaryEditTraits, BestiaryEditActions, BestiaryEditExtra};
        static const uint8_t fields[] = {0U, 5U, 8U, 9U, 16U, 17U, 18U, 19U, 20U, 21U, 22U};
        static const char* const titles[] = {"Monster Name", "Creature Type", "Size/Alignment",
            "Movement", "Skills", "Defenses", "Senses", "Languages", "Traits", "Actions", "Extra"};
        char* values[] = {m->summary.name, m->summary.type, m->size_alignment, m->speed,
            m->skills, m->defenses, m->senses, m->languages, m->traits, m->actions, m->extra};
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
    case BestiaryScreenHome: bestiary_handle_home(app, event); break;
    case BestiaryScreenList: bestiary_handle_list(app, event); break;
    case BestiaryScreenDetail: bestiary_handle_detail(app, event); break;
    case BestiaryScreenDetailLine: bestiary_handle_detail_line(app, event); break;
    case BestiaryScreenEncounter: bestiary_handle_encounter(app, event); break;
    case BestiaryScreenDiagnostics:
        if(event->type == InputTypeShort && event->key == InputKeyOk) bestiary_diagnose(app);
        else if(bestiary_move_event(event) && event->key == InputKeyUp) bestiary_move(app, 9U, -1);
        else if(bestiary_move_event(event) && event->key == InputKeyDown) bestiary_move(app, 9U, 1);
        break;
    case BestiaryScreenEdit: bestiary_handle_edit(app, event); break;
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
    app->gui = furi_record_open(RECORD_GUI);
    app->dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->dispatcher, app);
    view_dispatcher_set_navigation_event_callback(app->dispatcher, bestiary_navigation);
    view_dispatcher_set_custom_event_callback(app->dispatcher, bestiary_custom_event);
    app->input_events = furi_record_open(RECORD_INPUT_EVENTS);
    app->input_subscription =
        furi_pubsub_subscribe(app->input_events, bestiary_input_events_callback, app);
    app->view = view_alloc();
    view_allocate_model(app->view, ViewModelTypeLockFree, sizeof(BestiaryApp*));
    BestiaryApp** model = view_get_model(app->view);
    *model = app;
    view_commit_model(app->view, false);
    view_set_context(app->view, app);
    view_set_draw_callback(app->view, bestiary_draw);
    view_set_input_callback(app->view, bestiary_input);
    app->marquee_timer =
        furi_timer_alloc(bestiary_marquee_timer_callback, FuriTimerTypePeriodic, app);
    view_dispatcher_add_view(app->dispatcher, BestiaryViewMain, app->view);
    view_dispatcher_attach_to_gui(app->dispatcher, app->gui, ViewDispatcherTypeFullscreen);
    furi_timer_start(app->marquee_timer, furi_ms_to_ticks(BESTIARY_MARQUEE_MS));
    return app;
}

static void bestiary_free(BestiaryApp* app) {
    if(!app) return;
    bestiary_release_window(app);
    bestiary_release_detail(app);
    bestiary_release_encounter(app);
    if(app->text_input)
        view_dispatcher_remove_view(app->dispatcher, BestiaryViewText);
    view_dispatcher_remove_view(app->dispatcher, BestiaryViewMain);
    if(app->text_input) text_input_free(app->text_input);
    furi_timer_stop(app->marquee_timer);
    furi_timer_free(app->marquee_timer);
    if(app->input_subscription)
        furi_pubsub_unsubscribe(app->input_events, app->input_subscription);
    if(app->input_events) furi_record_close(RECORD_INPUT_EVENTS);
    view_free(app->view);
    view_dispatcher_free(app->dispatcher);
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_STORAGE);
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
