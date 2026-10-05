#include "dnd_data.h"
#include "dnd_profile_handoff.h"
#include "dnd_rules.h"
#include "dnd_charactersheet_api.h"

#include <furi.h>
#include <gui/gui.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <input/input.h>
#include <storage/storage.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DNDCHARACTER_SHEET_PAGE_COUNT 10U

typedef struct {
    ViewDispatcher* dispatcher;
    View* view;
    const DndCharacter* character;
    uint8_t page;
    DndPluginUiResult result;
} DndCharacterSheetApp;

static void cs_text(Canvas* canvas, int x, int y, int font, const char* text) {
    canvas_set_font(canvas, font);
    canvas_draw_str(canvas, x, y, text ? text : "");
}

static void cs_center(Canvas* canvas, int x, int y, int w, int font, const char* text) {
    canvas_set_font(canvas, font);
    int sw = canvas_string_width(canvas, text ? text : "");
    canvas_draw_str(canvas, x + (w - sw) / 2, y, text ? text : "");
}

static void
    cs_box(Canvas* canvas, int x, int y, int w, int h, const char* label, const char* value) {
    canvas_draw_rframe(canvas, x, y, w, h, 2);
    if(label) cs_center(canvas, x, y + 8, w, FontSecondary, label);
    if(value) cs_center(canvas, x, y + h - 4, w, FontPrimary, value);
}

static void cs_header(Canvas* canvas, const DndCharacterSheetApp* app, const char* title) {
    char page[12];
    snprintf(
        page, sizeof(page), "%u/%u", (unsigned)(app->page + 1U), DNDCHARACTER_SHEET_PAGE_COUNT);
    cs_text(canvas, 2, 8, FontSecondary, title);
    int pw;
    canvas_set_font(canvas, FontSecondary);
    pw = canvas_string_width(canvas, page);
    canvas_draw_str(canvas, 126 - pw, 8, page);
    canvas_draw_line(canvas, 0, 10, 127, 10);
}

static void cs_draw_overview(Canvas* canvas, const DndCharacterSheetApp* app) {
    const DndCharacter* c = app->character;
    char line[64], ac[8], init[8], speed[10], hp[18];
    cs_header(canvas, app, "CHARACTER SHEET");
    snprintf(line, sizeof(line), "%.19s", c->name[0] ? c->name : "No Character");
    cs_text(canvas, 2, 20, FontPrimary, line);
    char classline[64] = "";
    size_t used = 0U;
    for(uint8_t i = 0; i < c->class_count && i < DND_MAX_CLASSES; ++i) {
        int n = snprintf(
            classline + used,
            sizeof(classline) - used,
            "%s%.10s %u",
            i ? "/" : "",
            c->classes[i].name,
            c->classes[i].level);
        if(n < 0 || (size_t)n >= sizeof(classline) - used) break;
        used += (size_t)n;
    }
    cs_text(canvas, 2, 29, FontSecondary, classline[0] ? classline : "No class");
    snprintf(line, sizeof(line), "%.16s | %.16s", c->species, c->background);
    cs_text(canvas, 2, 38, FontSecondary, line);
    snprintf(ac, sizeof(ac), "%d", c->armor_class);
    snprintf(
        init,
        sizeof(init),
        "%+d",
        dnd_rules_core_ability_modifier(c->ability_scores[DndAbilityDexterity]) +
            c->initiative_misc + dnd_rules_core_exhaustion_penalty(c));
    snprintf(speed, sizeof(speed), "%d", c->speed);
    cs_box(canvas, 1, 42, 30, 21, "AC", ac);
    cs_box(canvas, 33, 42, 30, 21, "INIT", init);
    cs_box(canvas, 65, 42, 30, 21, "SPD", speed);
    snprintf(hp, sizeof(hp), "%d/%d", c->hp_current, c->hp_max);
    cs_box(canvas, 97, 42, 30, 21, "HP", hp);
}

static void cs_draw_abilities(Canvas* canvas, const DndCharacterSheetApp* app) {
    const DndCharacter* c = app->character;
    static const char* abbr[DND_ABILITY_COUNT] = {"STR", "DEX", "CON", "INT", "WIS", "CHA"};
    cs_header(canvas, app, "ABILITIES");
    for(uint8_t i = 0; i < DND_ABILITY_COUNT; ++i) {
        int col = i % 3U, row = i / 3U;
        int x = 1 + col * 42, y = 12 + row * 26;
        char value[18];
        snprintf(
            value,
            sizeof(value),
            "%d  %+d",
            c->ability_scores[i],
            dnd_rules_core_ability_modifier(c->ability_scores[i]));
        cs_box(canvas, x, y, 40, 24, abbr[i], value);
    }
}

static void cs_draw_saves(Canvas* canvas, const DndCharacterSheetApp* app) {
    const DndCharacter* c = app->character;
    static const char* abbr[DND_ABILITY_COUNT] = {"STR", "DEX", "CON", "INT", "WIS", "CHA"};
    cs_header(canvas, app, "SAVES / PASSIVES");
    char line[48];
    snprintf(
        line,
        sizeof(line),
        "PB +%u   Inspiration %s",
        dnd_rules_core_proficiency_bonus(c),
        c->inspiration ? "Yes" : "No");
    cs_text(canvas, 2, 19, FontSecondary, line);
    for(uint8_t i = 0; i < DND_ABILITY_COUNT; ++i) {
        int col = i % 3U, row = i / 3U;
        int x = 1 + col * 42, y = 22 + row * 18;
        char value[18];
        snprintf(
            value,
            sizeof(value),
            "%c%s %+d",
            c->saving_throw_proficiency[i] ? '*' : ' ',
            abbr[i],
            dnd_rules_core_saving_throw_modifier(c, i));
        cs_box(canvas, x, y, 40, 16, NULL, value);
    }
    snprintf(
        line,
        sizeof(line),
        "PP %d  PI %d  PINV %d",
        10 + dnd_rules_core_skill_base_modifier(c, 11U),
        10 + dnd_rules_core_skill_base_modifier(c, 6U),
        10 + dnd_rules_core_skill_base_modifier(c, 8U));
    cs_text(canvas, 2, 62, FontSecondary, line);
}

static void cs_draw_skills(Canvas* canvas, const DndCharacterSheetApp* app, uint8_t start) {
    const DndCharacter* c = app->character;
    cs_header(canvas, app, start ? "SKILLS II" : "SKILLS I");
    uint8_t end = (uint8_t)(start + 9U);
    if(end > DND_SKILL_COUNT) end = DND_SKILL_COUNT;
    for(uint8_t i = start; i < end; ++i) {
        uint8_t local = (uint8_t)(i - start);
        int col = local >= 5U ? 1 : 0;
        int row = local >= 5U ? local - 5U : local;
        int x = col ? 65 : 2;
        int y = 20 + row * 10;
        char line[32];
        const char* name = dnd_rules_core_skill_names[i];
        snprintf(
            line,
            sizeof(line),
            "%c%-12.12s %+d",
            c->skill_proficiency[i] ? '*' : ' ',
            name,
            dnd_rules_core_skill_modifier(c, i));
        cs_text(canvas, x, y, FontSecondary, line);
    }
}

static void cs_draw_combat(Canvas* canvas, const DndCharacterSheetApp* app) {
    const DndCharacter* c = app->character;
    cs_header(canvas, app, "COMBAT / HEALTH");
    char value[24];
    snprintf(value, sizeof(value), "%d", c->armor_class);
    cs_box(canvas, 1, 12, 30, 20, "AC", value);
    snprintf(
        value,
        sizeof(value),
        "%+d",
        dnd_rules_core_ability_modifier(c->ability_scores[DndAbilityDexterity]) +
            c->initiative_misc + dnd_rules_core_exhaustion_penalty(c));
    cs_box(canvas, 33, 12, 30, 20, "INIT", value);
    snprintf(value, sizeof(value), "%d", c->speed);
    cs_box(canvas, 65, 12, 30, 20, "SPEED", value);
    snprintf(value, sizeof(value), "%d", c->hp_temporary);
    cs_box(canvas, 97, 12, 30, 20, "TEMP", value);
    snprintf(value, sizeof(value), "%d/%d", c->hp_current, c->hp_max);
    cs_box(canvas, 1, 34, 40, 28, "HIT POINTS", value);
    snprintf(value, sizeof(value), "%u/%u d%u", c->hit_dice_current, c->hit_dice_max, c->hit_die);
    cs_box(canvas, 43, 34, 40, 28, "HIT DICE", value);
    snprintf(value, sizeof(value), "S%u F%u", c->death_successes, c->death_failures);
    cs_box(canvas, 85, 34, 42, 28, "DEATH SAVES", value);
}

static void cs_draw_identity(Canvas* canvas, const DndCharacterSheetApp* app) {
    const DndCharacter* c = app->character;
    cs_header(canvas, app, "IDENTITY / CLASS");
    char line[64];
    snprintf(line, sizeof(line), "Player: %.24s", c->player);
    cs_text(canvas, 2, 20, FontSecondary, line);
    snprintf(line, sizeof(line), "Species: %.23s", c->species);
    cs_text(canvas, 2, 29, FontSecondary, line);
    snprintf(line, sizeof(line), "Background: %.20s", c->background);
    cs_text(canvas, 2, 38, FontSecondary, line);
    snprintf(line, sizeof(line), "Alignment: %.20s", c->alignment);
    cs_text(canvas, 2, 47, FontSecondary, line);
    snprintf(
        line,
        sizeof(line),
        "Level %u  XP %lu  Size %u",
        dnd_rules_core_total_level(c),
        (unsigned long)c->experience,
        (unsigned)c->size);
    cs_text(canvas, 2, 56, FontSecondary, line);
    snprintf(line, sizeof(line), "Origin Feat: %.20s", c->origin_feat);
    cs_text(canvas, 2, 64, FontSecondary, line);
}

static void cs_draw_magic(Canvas* canvas, const DndCharacterSheetApp* app) {
    const DndCharacter* c = app->character;
    cs_header(canvas, app, "SPELLCASTING");
    uint8_t ability = c->spellcasting_ability < DND_ABILITY_COUNT ? c->spellcasting_ability :
                                                                    DndAbilityIntelligence;
    int8_t mod = dnd_rules_core_ability_modifier(c->ability_scores[ability]);
    int attack = mod + dnd_rules_core_proficiency_bonus(c) + c->spell_attack_misc;
    int dc = 8 + mod + dnd_rules_core_proficiency_bonus(c) + c->spell_save_misc;
    char line[64];
    snprintf(
        line,
        sizeof(line),
        "Ability %.3s   ATK %+d   DC %d",
        dnd_rules_core_ability_names[ability],
        attack,
        dc);
    cs_text(canvas, 2, 20, FontSecondary, line);
    for(uint8_t level = 1U; level <= 9U; ++level) {
        uint8_t local = (uint8_t)(level - 1U);
        int col = local >= 5U ? 1 : 0;
        int row = local >= 5U ? local - 5U : local;
        int x = col ? 65 : 2;
        int y = 31 + row * 8;
        snprintf(
            line,
            sizeof(line),
            "L%u  %u/%u",
            level,
            c->spell_slots_current[level],
            c->spell_slots_max[level]);
        cs_text(canvas, x, y, FontSecondary, line);
    }
}

static void cs_draw_state(Canvas* canvas, const DndCharacterSheetApp* app) {
    const DndCharacter* c = app->character;
    cs_header(canvas, app, "STATE / DEFENSES");
    char line[64];
    snprintf(
        line,
        sizeof(line),
        "Exhaustion %u  Reaction %s",
        c->exhaustion,
        c->reaction_available ? "Ready" : "Used");
    cs_text(canvas, 2, 19, FontSecondary, line);
    snprintf(line, sizeof(line), "Cond: %.25s", c->conditions[0] ? c->conditions : "--");
    cs_text(canvas, 2, 28, FontSecondary, line);
    snprintf(line, sizeof(line), "Conc: %.25s", c->concentration[0] ? c->concentration : "--");
    cs_text(canvas, 2, 37, FontSecondary, line);
    snprintf(line, sizeof(line), "Res: %.26s", c->resistances[0] ? c->resistances : "--");
    cs_text(canvas, 2, 46, FontSecondary, line);
    snprintf(line, sizeof(line), "Imm: %.26s", c->immunities[0] ? c->immunities : "--");
    cs_text(canvas, 2, 55, FontSecondary, line);
    snprintf(line, sizeof(line), "Vuln: %.25s", c->vulnerabilities[0] ? c->vulnerabilities : "--");
    cs_text(canvas, 2, 64, FontSecondary, line);
}

static void cs_draw_senses(Canvas* canvas, const DndCharacterSheetApp* app) {
    const DndCharacter* c = app->character;
    cs_header(canvas, app, "SENSES / MOVEMENT");
    char line[64];
    snprintf(
        line,
        sizeof(line),
        "Passive Perception %d",
        10 + dnd_rules_core_skill_base_modifier(c, 11U));
    cs_text(canvas, 2, 20, FontSecondary, line);
    snprintf(
        line, sizeof(line), "Passive Insight %d", 10 + dnd_rules_core_skill_base_modifier(c, 6U));
    cs_text(canvas, 2, 30, FontSecondary, line);
    snprintf(
        line,
        sizeof(line),
        "Passive Investigation %d",
        10 + dnd_rules_core_skill_base_modifier(c, 8U));
    cs_text(canvas, 2, 40, FontSecondary, line);
    snprintf(line, sizeof(line), "Senses: %.24s", c->senses[0] ? c->senses : "--");
    cs_text(canvas, 2, 51, FontSecondary, line);
    snprintf(
        line, sizeof(line), "Movement: %.21s", c->movement_modes[0] ? c->movement_modes : "--");
    cs_text(canvas, 2, 61, FontSecondary, line);
}

static void dndcharactersheet_draw(Canvas* canvas, void* model) {
    DndCharacterSheetApp* app = *(DndCharacterSheetApp**)model;
    canvas_clear(canvas);
    if(!app->character) {
        cs_text(canvas, 12, 28, FontPrimary, "No active character");
        cs_text(canvas, 14, 43, FontSecondary, "Back: DNDolphins");
        return;
    }
    switch(app->page) {
    case 0:
        cs_draw_overview(canvas, app);
        break;
    case 1:
        cs_draw_abilities(canvas, app);
        break;
    case 2:
        cs_draw_saves(canvas, app);
        break;
    case 3:
        cs_draw_skills(canvas, app, 0U);
        break;
    case 4:
        cs_draw_skills(canvas, app, 9U);
        break;
    case 5:
        cs_draw_combat(canvas, app);
        break;
    case 6:
        cs_draw_identity(canvas, app);
        break;
    case 7:
        cs_draw_magic(canvas, app);
        break;
    case 8:
        cs_draw_state(canvas, app);
        break;
    default:
        cs_draw_senses(canvas, app);
        break;
    }
}

static bool dndcharactersheet_input(InputEvent* event, void* context) {
    DndCharacterSheetApp* app = context;
    if((event->type == InputTypeShort || event->type == InputTypeRepeat) &&
       (event->key == InputKeyRight || event->key == InputKeyDown)) {
        app->page = (uint8_t)((app->page + 1U) % DNDCHARACTER_SHEET_PAGE_COUNT);
        view_commit_model(app->view, true);
        return true;
    }
    if((event->type == InputTypeShort || event->type == InputTypeRepeat) &&
       (event->key == InputKeyLeft || event->key == InputKeyUp)) {
        app->page = app->page ? (uint8_t)(app->page - 1U) : (DNDCHARACTER_SHEET_PAGE_COUNT - 1U);
        view_commit_model(app->view, true);
        return true;
    }
    if(event->type == InputTypeShort && event->key == InputKeyBack) {
        app->result = DndPluginUiReturn;
        view_dispatcher_stop(app->dispatcher);
        return true;
    }
    if(event->type == InputTypeLong && event->key == InputKeyBack) {
        app->result = DndPluginUiExit;
        view_dispatcher_stop(app->dispatcher);
        return true;
    }
    return false;
}

#define DND_CHARACTER_SHEET_VIEW 0xDC01U
static DndPluginUiResult dndcharactersheet_run(
    ViewDispatcher* dispatcher,
    const DndCharacter* character,
    bool loading_on_return) {
    if(!dispatcher) return DndPluginUiError;
    DndCharacterSheetApp* app = calloc(1, sizeof(DndCharacterSheetApp));
    if(!app) return DndPluginUiError;
    app->dispatcher = dispatcher;
    app->character = character;
    app->result = DndPluginUiExit;
    app->view = view_alloc();
    if(!app->view) {
        free(app);
        return DndPluginUiError;
    }
    view_allocate_model(app->view, ViewModelTypeLockFree, sizeof(DndCharacterSheetApp*));
    DndCharacterSheetApp** model = view_get_model(app->view);
    if(!model) {
        view_free(app->view);
        free(app);
        return DndPluginUiError;
    }
    *model = app;
    view_set_context(app->view, app);
    view_set_draw_callback(app->view, dndcharactersheet_draw);
    view_set_input_callback(app->view, dndcharactersheet_input);
    dnd_plugin_clear_dispatcher(dispatcher);
    view_dispatcher_add_view(dispatcher, DND_CHARACTER_SHEET_VIEW, app->view);
    view_dispatcher_switch_to_view(dispatcher, DND_CHARACTER_SHEET_VIEW);
    view_dispatcher_run(dispatcher);
    DndPluginUiResult result = app->result;
    dnd_plugin_clear_dispatcher(dispatcher);
    if(loading_on_return) dnd_plugin_ui_return_loading(dispatcher);
    view_dispatcher_remove_view(dispatcher, DND_CHARACTER_SHEET_VIEW);
    view_free(app->view);
    free(app);
    return result;
}
static const DndCharacterSheetApi dndcharactersheet_api = {
    .size = sizeof(DndCharacterSheetApi),
    .run = dndcharactersheet_run,
};
static const FlipperAppPluginDescriptor dndcharactersheet_descriptor = {
    .appid = DND_CHARACTER_SHEET_API_ID,
    .ep_api_version = DND_CHARACTER_SHEET_API_VERSION,
    .entry_point = &dndcharactersheet_api,
};
const FlipperAppPluginDescriptor* dnd_character_sheet_ep(void) {
    return &dndcharactersheet_descriptor;
}
