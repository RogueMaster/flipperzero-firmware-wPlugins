#include "dnd_monster_turn_api.h"
#include "dndbestiary_packs.h"
#include <furi_hal_random.h>
#include <gui/view.h>
#include <input/input.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define DND_MONSTER_TURN_VIEW 0xDB01U
typedef struct {
    char name[28];
    int8_t attack_bonus;
    uint8_t dice_count[2];
    uint8_t dice_sides[2];
    int8_t dice_modifier[2];
    uint8_t damage_terms;
} DndMonsterTurnAttack;
typedef struct {
    ViewDispatcher* dispatcher;
    View* view;
    const DndMonsterDetail* detail;
    DndMonsterDetail* owned_detail;
    DndMonsterTurnAttack turn_attacks[4];
    uint8_t turn_attack_count;
    uint8_t selection;
    uint8_t scroll;
    bool attacks;
    bool direct_attacks;
    bool line_open;
    uint8_t line_field;
    uint16_t line_offset;
    DndPluginUiResult result;
    char status[32];
} DndMonsterTurnApp;
static void dnd_monster_turn_copy(char* out, size_t size, const char* text) {
    if(!size) return;
    strncpy(out, text ? text : "", size - 1);
    out[size - 1] = '\0';
}
static int16_t dnd_monster_turn_parse_signed(const char* text, const char** end) {
    if(!text) return 0;
    int sign = 1;
    if(*text == '+')
        ++text;
    else if(*text == '-') {
        sign = -1;
        ++text;
    }
    int16_t value = 0;
    while(*text >= '0' && *text <= '9') {
        value = (int16_t)(value * 10 + (*text - '0'));
        ++text;
    }
    if(end) *end = text;
    return (int16_t)(value * sign);
}

static bool dnd_monster_turn_parse_dice(
    const char* text,
    uint8_t* count,
    uint8_t* sides,
    int8_t* modifier) {
    if(!text || !count || !sides || !modifier) return false;
    const char* p = text;
    int16_t c = 0;
    while(*p >= '0' && *p <= '9') {
        c = (int16_t)(c * 10 + (*p++ - '0'));
    }
    if(*p != 'd' && *p != 'D') return false;
    ++p;
    int16_t d = 0;
    while(*p >= '0' && *p <= '9') {
        d = (int16_t)(d * 10 + (*p++ - '0'));
    }
    while(*p == ' ')
        ++p;
    int16_t mod = 0;
    if(*p == '+' || *p == '-') mod = dnd_monster_turn_parse_signed(p, NULL);
    if(c < 1 || c > 20 || d < 2 || d > 100 || mod < -50 || mod > 50) return false;
    *count = (uint8_t)c;
    *sides = (uint8_t)d;
    *modifier = (int8_t)mod;
    return true;
}

static void dnd_monster_turn_turn_attack_name(
    char* out,
    size_t size,
    const char* actions,
    const char* attack_roll) {
    const char* last = attack_roll;
    while(last > actions && !(last[-1] == '.' && last[0] == ' '))
        --last;
    const char* name_end = last > actions ? last - 1U : attack_roll;
    const char* start = name_end;
    while(start > actions) {
        if(start[-1] == '.' && start[0] == ' ') {
            ++start;
            break;
        }
        --start;
    }
    while(*start == ' ')
        ++start;
    size_t len = (size_t)(name_end - start);
    if(!len || len >= size) {
        dnd_monster_turn_copy(out, size, "Attack");
        return;
    }
    memcpy(out, start, len);
    out[len] = '\0';
}

static void dnd_monster_turn_prepare_turn_attacks(DndMonsterTurnApp* app) {
    if(!app || !app->detail) return;
    app->turn_attack_count = 0U;
    const char* actions = app->detail->actions;
    const char* cursor = actions;
    while(cursor && *cursor && app->turn_attack_count < 4U) {
        const char* roll = strstr(cursor, "Attack Roll:");
        if(!roll) break;
        DndMonsterTurnAttack* attack = &app->turn_attacks[app->turn_attack_count];
        memset(attack, 0, sizeof(*attack));
        dnd_monster_turn_turn_attack_name(attack->name, sizeof(attack->name), actions, roll);
        const char* bonus = roll + strlen("Attack Roll:");
        while(*bonus == ' ')
            ++bonus;
        int16_t parsed_bonus = dnd_monster_turn_parse_signed(bonus, NULL);
        if(parsed_bonus < -50) parsed_bonus = -50;
        if(parsed_bonus > 50) parsed_bonus = 50;
        attack->attack_bonus = (int8_t)parsed_bonus;

        const char* hit = strstr(roll, "Hit:");
        const char* next_roll = strstr(roll + 1U, "Attack Roll:");
        if(hit && (!next_roll || hit < next_roll)) {
            const char* p = hit;
            while((p = strchr(p, '(')) && attack->damage_terms < 2U &&
                  (!next_roll || p < next_roll)) {
                ++p;
                uint8_t term = attack->damage_terms;
                if(dnd_monster_turn_parse_dice(
                       p,
                       &attack->dice_count[term],
                       &attack->dice_sides[term],
                       &attack->dice_modifier[term]))
                    ++attack->damage_terms;
            }
        }
        ++app->turn_attack_count;
        cursor = next_roll;
    }
}

static int16_t dnd_monster_turn_roll_die(uint8_t sides) {
    if(sides < 2U) return 0;
    return (int16_t)(furi_hal_random_get() % sides) + 1;
}

static int16_t dnd_monster_turn_roll_damage(const DndMonsterTurnAttack* attack) {
    int16_t total = 0;
    if(!attack) return 0;
    for(uint8_t term = 0U; term < attack->damage_terms; ++term) {
        for(uint8_t die = 0U; die < attack->dice_count[term]; ++die)
            total = (int16_t)(total + dnd_monster_turn_roll_die(attack->dice_sides[term]));
        total = (int16_t)(total + attack->dice_modifier[term]);
    }
    return total;
}

static const char*
    dnd_monster_turn_field(const DndMonsterDetail* m, uint8_t field, char* buffer, size_t size) {
    switch(field) {
    case 0:
        snprintf(
            buffer,
            size,
            "CR %u/8 XP%lu AC%u HP%u",
            m->summary.cr_eighths,
            (unsigned long)m->summary.xp,
            m->summary.armor_class,
            m->summary.hit_points);
        return buffer;
    case 1:
        return m->summary.type;
    case 2:
        return m->size_alignment;
    case 3:
        return m->speed;
    case 4:
        snprintf(
            buffer,
            size,
            "S%d D%d C%d I%d W%d C%d",
            m->abilities[0],
            m->abilities[1],
            m->abilities[2],
            m->abilities[3],
            m->abilities[4],
            m->abilities[5]);
        return buffer;
    case 5:
        return m->skills;
    case 6:
        return m->defenses;
    case 7:
        return m->senses;
    case 8:
        return m->languages;
    case 9:
        return m->traits;
    case 10:
        return m->actions;
    case 11:
        return m->extra;
    case 12:
        return m->summary.source;
    default:
        return "Monster Turn Tools";
    }
}
static void dnd_monster_turn_row(Canvas* canvas, uint8_t row, bool selected, const char* text) {
    if(selected) {
        canvas_draw_box(canvas, 0, 13 + row * 10, 128, 10);
        canvas_set_color(canvas, ColorWhite);
    }
    canvas_draw_str(canvas, 2, 21 + row * 10, text);
    canvas_set_color(canvas, ColorBlack);
}
static void dnd_monster_turn_draw(Canvas* canvas, void* model) {
    DndMonsterTurnApp* app = *(DndMonsterTurnApp**)model;
    canvas_clear(canvas);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(
        canvas,
        2,
        9,
        app->status[0] ? app->status :
        app->attacks   ? "Monster Turn Tools" :
                         app->detail->summary.name);
    char buffer[96];
    if(app->line_open) {
        const char* text =
            dnd_monster_turn_field(app->detail, app->line_field, buffer, sizeof(buffer));
        size_t length = strlen(text), start = app->line_offset;
        for(uint8_t row = 0; row < 5 && start < length; ++row, start += 24) {
            char line[25];
            size_t count = length - start;
            if(count > 24) count = 24;
            memcpy(line, text + start, count);
            line[count] = '\0';
            dnd_monster_turn_row(canvas, row, false, line);
        }
    } else if(app->attacks) {
        if(!app->turn_attack_count) {
            dnd_monster_turn_row(canvas, 0, false, "No attack rolls parsed");
            dnd_monster_turn_row(canvas, 1, false, "OK: Actions stat line");
        }
        for(uint8_t row = 0; row < app->turn_attack_count; ++row) {
            const DndMonsterTurnAttack* attack = &app->turn_attacks[row];
            snprintf(buffer, sizeof(buffer), "%.24s %+d", attack->name, attack->attack_bonus);
            dnd_monster_turn_row(canvas, row, row == app->selection, buffer);
        }
    } else {
        for(uint8_t row = 0; row < 5 && app->scroll + row < 14; ++row) {
            const char* text =
                dnd_monster_turn_field(app->detail, app->scroll + row, buffer, sizeof(buffer));
            char visible[29];
            dnd_monster_turn_copy(visible, sizeof(visible), text);
            dnd_monster_turn_row(canvas, row, app->selection == app->scroll + row, visible);
        }
    }
}
static bool dnd_monster_turn_input(InputEvent* event, void* context) {
    DndMonsterTurnApp* app = context;
    if(event->key == InputKeyBack && event->type == InputTypeLong) {
        app->result = DndPluginUiExit;
        view_dispatcher_stop(app->dispatcher);
        return true;
    }
    if(event->key == InputKeyBack && event->type == InputTypeShort) {
        if(app->line_open)
            app->line_open = false;
        else if(app->attacks && !app->direct_attacks) {
            app->attacks = false;
            app->selection = 13;
            app->scroll = 9;
        } else {
            app->result = DndPluginUiReturn;
            view_dispatcher_stop(app->dispatcher);
        }
        app->status[0] = '\0';
        view_commit_model(app->view, true);
        return true;
    }
    bool move = event->type == InputTypeShort || event->type == InputTypeRepeat;
    if(move && (event->key == InputKeyUp || event->key == InputKeyDown)) {
        int delta = event->key == InputKeyUp ? -1 : 1;
        if(app->line_open) {
            char buffer[96];
            const char* text =
                dnd_monster_turn_field(app->detail, app->line_field, buffer, sizeof(buffer));
            if(delta < 0)
                app->line_offset = app->line_offset >= 24 ? app->line_offset - 24 : 0;
            else if((size_t)app->line_offset + 120U < strlen(text))
                app->line_offset += 24;
        } else {
            uint8_t count = app->attacks ? app->turn_attack_count : 14;
            if(count) app->selection = (app->selection + count + delta) % count;
            if(app->selection < app->scroll) app->scroll = app->selection;
            if(app->selection >= app->scroll + 5) app->scroll = app->selection - 4;
        }
    } else if(event->key == InputKeyOk && event->type == InputTypeShort) {
        if(app->line_open)
            app->line_open = false;
        else if(!app->attacks && app->selection == 13) {
            app->attacks = true;
            app->selection = app->scroll = 0;
        } else if(app->attacks && app->turn_attack_count) {
            const DndMonsterTurnAttack* attack = &app->turn_attacks[app->selection];
            int16_t total = dnd_monster_turn_roll_die(20) + attack->attack_bonus;
            int16_t damage = dnd_monster_turn_roll_damage(attack);
            if(attack->damage_terms)
                snprintf(app->status, sizeof(app->status), "Atk %d  Dmg %d", total, damage);
            else
                snprintf(app->status, sizeof(app->status), "Attack %d", total);
        } else {
            app->line_open = true;
            app->line_field = app->attacks ? 10 : app->selection;
            app->line_offset = 0;
        }
    }
    view_commit_model(app->view, true);
    return true;
}
typedef struct {
    const char* name;
    bool homebrew;
} DndMonsterTurnQuery;
static bool dnd_monster_turn_match(const DndMonsterSummary* summary, void* context) {
    const DndMonsterTurnQuery* query = context;
    return !strcmp(summary->name, query->name) &&
           dndbestiary_monsters_source_allowed(summary, query->homebrew);
}
static DndPluginUiResult dnd_monster_turn_run(
    ViewDispatcher* dispatcher,
    Storage* storage,
    const DndMonsterDetail* detail,
    const char* name,
    bool allow_homebrew,
    bool start_with_attacks,
    bool loading_on_return) {
    if(!dispatcher || !storage) return DndPluginUiError;
    DndMonsterTurnApp* app = calloc(1, sizeof(DndMonsterTurnApp));
    if(!app) return DndPluginUiError;
    app->dispatcher = dispatcher;
    app->result = DndPluginUiExit;
    if(detail) {
        if(!dndbestiary_monsters_source_allowed(&detail->summary, allow_homebrew)) goto fail;
        app->detail = detail;
    } else {
        if(!name || !name[0]) goto fail;
        uint16_t migrated = 0, seeded = 0, recovered = 0, rolled_back = 0;
        bool migrated_ok = dndbestiary_monsters_migrate_legacy_custom(storage, &migrated);
        bool seeded_ok = migrated_ok && dndbestiary_monsters_seed_default_custom(storage, &seeded);
        if(seeded_ok)
            (void)dndbestiary_monsters_recover_user_pack(storage, &recovered, &rolled_back);
        (void)dndbestiary_packs_ensure_enabled(storage);
        DndMonsterTurnQuery query = {name, allow_homebrew};
        DndMonsterSummary summary;
        if(dndbestiary_monsters_query(
               storage, dnd_monster_turn_match, &query, 0, &summary, 1, NULL) != 1)
            goto fail;
        app->owned_detail = malloc(sizeof(DndMonsterDetail));
        if(!app->owned_detail || !dndbestiary_monsters_load(storage, &summary, app->owned_detail))
            goto fail;
        app->detail = app->owned_detail;
    }
    app->attacks = app->direct_attacks = start_with_attacks;
    dnd_monster_turn_prepare_turn_attacks(app);
    app->view = view_alloc();
    if(!app->view) goto fail;
    view_allocate_model(app->view, ViewModelTypeLockFree, sizeof(DndMonsterTurnApp*));
    DndMonsterTurnApp** model = view_get_model(app->view);
    if(!model) goto fail;
    *model = app;
    view_set_context(app->view, app);
    view_set_draw_callback(app->view, dnd_monster_turn_draw);
    view_set_input_callback(app->view, dnd_monster_turn_input);
    dnd_plugin_clear_dispatcher(dispatcher);
    view_dispatcher_add_view(dispatcher, DND_MONSTER_TURN_VIEW, app->view);
    view_dispatcher_switch_to_view(dispatcher, DND_MONSTER_TURN_VIEW);
    view_dispatcher_run(dispatcher);
    DndPluginUiResult result = app->result;
    dnd_plugin_clear_dispatcher(dispatcher);
    if(loading_on_return) dnd_plugin_ui_return_loading(dispatcher);
    view_dispatcher_remove_view(dispatcher, DND_MONSTER_TURN_VIEW);
    view_free(app->view);
    free(app->owned_detail);
    free(app);
    dndbestiary_monsters_cache_reset();
    return result;
fail:
    if(app->view) view_free(app->view);
    free(app->owned_detail);
    free(app);
    dndbestiary_monsters_cache_reset();
    return DndPluginUiError;
}
static const DndMonsterTurnApi dnd_monster_turn_api = {
    .size = sizeof(DndMonsterTurnApi),
    .run = dnd_monster_turn_run};
static const FlipperAppPluginDescriptor dnd_monster_turn_descriptor = {
    .appid = DND_MONSTER_TURN_API_ID,
    .ep_api_version = DND_MONSTER_TURN_API_VERSION,
    .entry_point = &dnd_monster_turn_api};
const FlipperAppPluginDescriptor* dnd_monster_turn_ep(void) {
    return &dnd_monster_turn_descriptor;
}
