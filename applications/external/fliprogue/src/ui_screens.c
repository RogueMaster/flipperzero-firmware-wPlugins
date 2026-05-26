#include "ui_screens.h"
#include "chest_actions.h"
#include "look_text.h"
#include "ui_draw.h"

void draw_title(Canvas* canvas, AppContext* app) {
    canvas_set_font(canvas, FontPrimary);
    draw_text_center(canvas, 2, "FlipRogue");
    canvas_set_font(canvas, FontSecondary);
    draw_text_center(canvas, 13, "v1.2");
    for(uint8_t i = 0; i < TITLE_MENU_COUNT; i++) {
        uint8_t y = (uint8_t)(27 + i * 8);
        char entry[24];
        if(i == 0) snprintf(entry, sizeof(entry), "New game");
        else if(i == 1) snprintf(entry, sizeof(entry), "High Scores");
        else if(i == 2) snprintf(entry, sizeof(entry), "Help");
        else {
            snprintf(
                entry,
                sizeof(entry),
                app->selection == i ? "Sound: %s < >" : "Sound: %s",
                app->sound_enabled ? "On" : "Off");
        }
        if(i == app->selection) canvas_draw_str(canvas, 26, y, ">");
        draw_text_center(canvas, (int)y - 7, entry);
    }
    canvas_draw_str(canvas, 74, 61, "Back quits");
}

void draw_class_select(Canvas* canvas, AppContext* app) {
    static const char* entries[] = {"Warrior", "Ranger", "Mage"};
    canvas_set_font(canvas, FontPrimary);
    draw_text_center(canvas, 4, "Choose");
    canvas_set_font(canvas, FontSecondary);
    for(uint8_t i = 0; i < 3; i++) {
        uint8_t y = (uint8_t)(24 + i * 11);
        if(i == app->selection) canvas_draw_str(canvas, 34, y, ">");
        canvas_draw_str(canvas, 46, y, entries[i]);
    }
}

void draw_help_menu(Canvas* canvas, AppContext* app) {
    canvas_set_font(canvas, FontPrimary);
    draw_text_center(canvas, 4, "Help");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_frame(canvas, 2, HELP_FRAME_Y, 124, HELP_FRAME_H);

    if(app->selection < app->scroll) app->scroll = app->selection;
    if(app->selection >= app->scroll + HELP_VISIBLE_ROWS) {
        app->scroll = (uint8_t)(app->selection - HELP_VISIBLE_ROWS + 1);
    }

    for(uint8_t row = 0; row < HELP_VISIBLE_ROWS && row + app->scroll < HELP_TOPIC_COUNT; row++) {
        uint8_t topic = (uint8_t)(row + app->scroll);
        uint8_t y = (uint8_t)(HELP_ROW_Y + row * HELP_ROW_STEP);
        if(topic == app->selection) canvas_draw_str(canvas, 12, y, ">");
        canvas_draw_str(canvas, 24, y, help_topic_name(topic));
    }

    canvas_draw_str(canvas, 5, 61, "Back");
    canvas_draw_str(canvas, 91, 61, "OK open");
}

void draw_help_page(Canvas* canvas, AppContext* app) {
    uint8_t count = help_line_count(app->help_topic);
    canvas_set_font(canvas, FontPrimary);
    draw_text_center(canvas, 4, help_topic_name(app->help_topic));
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_frame(canvas, 2, HELP_FRAME_Y, 124, HELP_FRAME_H);

    FrUiPage page = fr_ui_page(app->scroll, count, HELP_VISIBLE_ROWS);
    app->scroll = page.first;

    for(uint8_t row = 0; row < HELP_VISIBLE_ROWS && row + app->scroll < count; row++) {
        canvas_draw_str(
            canvas,
            8,
            (uint8_t)(HELP_ROW_Y + row * HELP_ROW_STEP),
            help_line(app->help_topic, (uint8_t)(row + app->scroll)));
    }
    canvas_draw_str(canvas, 5, 61, "Back");
    if(count > HELP_VISIBLE_ROWS) {
        char footer[24];
        snprintf(footer, sizeof(footer), "%u/%u", page.page, page.pages);
        canvas_draw_str(canvas, 105, 61, footer);
    }
}

static bool inventory_slot_matches_tab(const FrInvSlot* slot, uint8_t tab) {
    if(tab == 0) return true;
    if(tab == 1) return slot->type == FR_ITEM_POTION;
    if(tab == 2) return slot->type == FR_ITEM_SCROLL;
    if(tab == 3) return slot->type == FR_ITEM_WAND;
    return slot->type == FR_ITEM_GEAR || slot->type == FR_ITEM_ARROWS || slot->type == FR_ITEM_ORB ||
           slot->type == FR_ITEM_THROWABLE || slot->type == FR_ITEM_TRINKET;
}

uint8_t inventory_visible_count(const FrGame* game, uint8_t tab) {
    uint8_t count = 0;
    for(uint8_t i = 0; i < game->player.inv_count; i++) {
        if(inventory_slot_matches_tab(&game->player.inv[i], tab)) count++;
    }
    return count;
}

uint8_t inventory_slot_index_at(const FrGame* game, uint8_t tab, uint8_t visible_index) {
    uint8_t seen = 0;
    for(uint8_t i = 0; i < game->player.inv_count; i++) {
        if(!inventory_slot_matches_tab(&game->player.inv[i], tab)) continue;
        if(seen == visible_index) return i;
        seen++;
    }
    return 0xFF;
}

static bool identify_slot_matches(const FrGame* game, uint8_t index, uint8_t source_index) {
    if(index >= game->player.inv_count || index == source_index) return false;
    const FrInvSlot* slot = &game->player.inv[index];
    if(slot->type == FR_ITEM_POTION) return (game->player.known_potions & (uint16_t)(1u << slot->subtype)) == 0;
    if(slot->type == FR_ITEM_SCROLL) {
        return !fr_player_knows_scrolls(game) &&
               (game->player.known_scrolls & (uint16_t)(1u << slot->subtype)) == 0;
    }
    if(slot->type == FR_ITEM_TRINKET) return (game->player.known_trinkets & (uint16_t)(1u << slot->subtype)) == 0;
    return false;
}

uint8_t identify_visible_count(const FrGame* game, uint8_t source_index) {
    uint8_t count = 0;
    for(uint8_t i = 0; i < game->player.inv_count; i++) {
        if(identify_slot_matches(game, i, source_index)) count++;
    }
    return count;
}

uint8_t identify_slot_index_at(const FrGame* game, uint8_t source_index, uint8_t visible_index) {
    uint8_t seen = 0;
    for(uint8_t i = 0; i < game->player.inv_count; i++) {
        if(!identify_slot_matches(game, i, source_index)) continue;
        if(seen == visible_index) return i;
        seen++;
    }
    return 0xFF;
}

void inventory_clamp_selection(AppContext* app) {
    uint8_t count = inventory_visible_count(app->game, app->inv_tab);
    if(count == 0 || app->selection >= count) app->selection = 0;
}

static void gear_piece(char* out, size_t out_size, const char* name, uint8_t level) {
    if(level > 0) snprintf(out, out_size, "%s+%u", name, level);
    else snprintf(out, out_size, "%s", name);
}

static void inventory_gear_lines(const FrGame* game, char* line1, size_t line1_size, char* line2, size_t line2_size) {
    const FrPlayer* p = &game->player;
    char a[14];
    char b[14];
    char c[14];
    if(p->class_id == FR_CLASS_RANGER) {
        gear_piece(a, sizeof(a), "Dagger", p->dagger_lvl);
        gear_piece(b, sizeof(b), "Bow", p->bow_lvl);
        gear_piece(c, sizeof(c), "Boots", p->feet_lvl);
        snprintf(line1, line1_size, "* %s, %s", a, b);
        gear_piece(a, sizeof(a), "Hood", p->head_lvl);
        gear_piece(b, sizeof(b), "Cloak", p->body_lvl);
        snprintf(line2, line2_size, "* %s, %s, %s", a, b, c);
    } else if(p->class_id == FR_CLASS_MAGE) {
        gear_piece(a, sizeof(a), "Dagger", p->dagger_lvl);
        gear_piece(b, sizeof(b), "Staff", p->staff_lvl);
        gear_piece(c, sizeof(c), "Sandals", p->feet_lvl);
        snprintf(line1, line1_size, "* %s, %s", a, b);
        gear_piece(a, sizeof(a), "Hood", p->head_lvl);
        gear_piece(b, sizeof(b), "Robe", p->body_lvl);
        snprintf(line2, line2_size, "* %s, %s, %s", a, b, c);
    } else {
        gear_piece(a, sizeof(a), "Sword", p->sword_lvl);
        gear_piece(b, sizeof(b), "Shield", p->shield_lvl);
        gear_piece(c, sizeof(c), "Boots", p->feet_lvl);
        snprintf(line1, line1_size, "* %s, %s", a, b);
        gear_piece(a, sizeof(a), "Helm", p->head_lvl);
        gear_piece(b, sizeof(b), "Armor", p->body_lvl);
        snprintf(line2, line2_size, "* %s, %s, %s", a, b, c);
    }
}

void draw_inventory(Canvas* canvas, AppContext* app) {
    FrGame* game = app->game;
    static const char* tabs[INV_TAB_COUNT] = {"All", "Pot", "Scr", "Wnd", "Gear"};
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_box(canvas, 0, 0, SCREEN_W, SCREEN_H);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_frame(canvas, 2, 2, 124, 60);
    canvas_draw_str(canvas, 8, 11, "Inventory");

    for(uint8_t i = 0; i < INV_TAB_COUNT; i++) {
        uint8_t x = (uint8_t)(8 + i * 23);
        if(i == app->inv_tab) canvas_draw_frame(canvas, (uint8_t)(x - 3), 14, 24, 11);
        canvas_draw_str(canvas, x, 22, tabs[i]);
    }

    uint8_t count = inventory_visible_count(game, app->inv_tab);
    if(app->inv_tab == 4) {
        char gear1[56];
        char gear2[56];
        inventory_gear_lines(game, gear1, sizeof(gear1), gear2, sizeof(gear2));
        canvas_draw_str(canvas, 8, 34, gear1);
        canvas_draw_str(canvas, 8, 42, gear2);
    }
    if(count == 0) {
        if(app->inv_tab == 4) {
            canvas_draw_str(canvas, 10, 53, "No gear items");
        } else {
            canvas_draw_str(canvas, 10, 43, "Empty");
        }
        return;
    }

    uint8_t visible_rows = app->inv_tab == 4 ? 1 : INV_VISIBLE_ROWS;
    uint8_t list_y = app->inv_tab == 4 ? 53 : 34;
    uint8_t top = 0;
    if(app->selection >= visible_rows) top = (uint8_t)(app->selection - visible_rows + 1);
    for(uint8_t row = 0; row < visible_rows && row + top < count; row++) {
        uint8_t visible_index = (uint8_t)(top + row);
        uint8_t inv_index = inventory_slot_index_at(game, app->inv_tab, visible_index);
        if(inv_index == 0xFF) continue;
        uint8_t y = (uint8_t)(list_y + row * 8);
        if(visible_index == app->selection) canvas_draw_str(canvas, 8, y, ">");
        canvas_draw_str(canvas, 18, y, fr_inventory_label(game, &game->player.inv[inv_index]));
    }

    char footer[24];
    snprintf(footer, sizeof(footer), "%u/%u", (uint8_t)(app->selection + 1), count);
    canvas_draw_str(canvas, 104, 61, footer);
}

uint8_t item_choice_count(const FrInvSlot* slot) {
    if(!slot) return 0;
    if(slot->type == FR_ITEM_POTION) return 3;
    if(slot->type == FR_ITEM_THROWABLE) return 2;
    if(slot->type == FR_ITEM_TRINKET) return 2;
    return 2;
}

const char* item_choice_label(const FrInvSlot* slot, uint8_t choice) {
    if(!slot) return "";
    if(slot->type == FR_ITEM_POTION) {
        static const char* potion_choices[] = {"Quaff", "Throw", "Drop"};
        return choice < 3 ? potion_choices[choice] : "";
    }
    if(slot->type == FR_ITEM_SCROLL) return choice == 0 ? "Read" : "Drop";
    if(slot->type == FR_ITEM_WAND) return choice == 0 ? "Zap" : "Drop";
    if(slot->type == FR_ITEM_THROWABLE) return choice == 0 ? "Throw" : "Drop";
    if(slot->type == FR_ITEM_TRINKET) {
        if(choice == 0) return (slot->flags & FR_INV_EQUIPPED) != 0 ? "Take off" : "Wear";
        return "Drop";
    }
    return choice == 0 ? "Use" : "Drop";
}

void draw_item_choice(Canvas* canvas, AppContext* app) {
    FrInvSlot* slot = app->item_index < app->game->player.inv_count ? &app->game->player.inv[app->item_index] : NULL;
    uint8_t count = item_choice_count(slot);
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_box(canvas, 16, 10, 96, 48);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_frame(canvas, 16, 10, 96, 48);
    canvas_draw_str(canvas, 24, 21, slot ? fr_inventory_label(app->game, slot) : "Item");
    for(uint8_t i = 0; i < count; i++) {
        uint8_t y = (uint8_t)(32 + i * 8);
        if(i == app->selection) canvas_draw_str(canvas, 24, y, ">");
        canvas_draw_str(canvas, 36, y, item_choice_label(slot, i));
    }
    if(slot) {
        const char* hint = fr_inventory_hint(app->game, slot);
        if(hint[0] != '\0') canvas_draw_str(canvas, 24, count < 3 ? 53 : 55, hint);
    }
}

void draw_chest_choice(Canvas* canvas, AppContext* app) {
    FrItem* chest = app->item_index < FR_MAX_ITEMS ? &app->game->items[app->item_index] : NULL;
    uint8_t count = fr_chest_choice_count(app->game, chest);
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_box(canvas, 8, 8, 112, 50);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_frame(canvas, 8, 8, 112, 50);
    canvas_draw_str(canvas, 18, 19, "Choose one item");
    for(uint8_t i = 0; i < count; i++) {
        uint8_t y = (uint8_t)(31 + i * 8);
        FrInvSlot slot = fr_chest_choice_slot(app->game, chest, i);
        if(i == app->selection) canvas_draw_str(canvas, 16, y, ">");
        canvas_draw_str(canvas, 28, y, fr_inventory_label(app->game, &slot));
    }
    canvas_draw_str(canvas, 18, 57, "OK take  Back");
}

void draw_identify_pick(Canvas* canvas, AppContext* app) {
    FrGame* game = app->game;
    uint8_t count = identify_visible_count(game, app->target_index);
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_box(canvas, 0, 0, SCREEN_W, SCREEN_H);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_frame(canvas, 2, 2, 124, 60);
    canvas_draw_str(canvas, 8, 13, "Identify");
    if(count == 0) {
        canvas_draw_str(canvas, 10, 36, "Nothing unknown");
        return;
    }
    uint8_t top = 0;
    if(app->selection >= INV_VISIBLE_ROWS) top = (uint8_t)(app->selection - INV_VISIBLE_ROWS + 1);
    for(uint8_t row = 0; row < INV_VISIBLE_ROWS && row + top < count; row++) {
        uint8_t visible_index = (uint8_t)(top + row);
        uint8_t inv_index = identify_slot_index_at(game, app->target_index, visible_index);
        if(inv_index == 0xFF) continue;
        uint8_t y = (uint8_t)(27 + row * 8);
        if(visible_index == app->selection) canvas_draw_str(canvas, 8, y, ">");
        canvas_draw_str(canvas, 18, y, fr_inventory_label(game, &game->player.inv[inv_index]));
    }
    canvas_draw_str(canvas, 5, 61, "OK choose Back");
}

void draw_perk_choice(Canvas* canvas, AppContext* app) {
    FrGame* game = app->game;
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_box(canvas, 0, 0, SCREEN_W, SCREEN_H);
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontPrimary);
    draw_text_center(canvas, 4, "Choose Perk");
    canvas_set_font(canvas, FontSecondary);
    for(uint8_t i = 0; i < 5; i++) {
        uint8_t y = (uint8_t)(20 + i * 8);
        if(i == app->selection) canvas_draw_str(canvas, 6, y, ">");
        if((game->player.perks & (uint8_t)(1u << i)) != 0) canvas_draw_str(canvas, 16, y, "*");
        canvas_draw_str(canvas, 26, y, fr_perk_name(game->player.class_id, i));
    }
    canvas_draw_str(canvas, 5, 61, "OK inspect");
}

void draw_perk_detail(Canvas* canvas, AppContext* app) {
    FrGame* game = app->game;
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_box(canvas, 8, 8, 112, 48);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_frame(canvas, 8, 8, 112, 48);
    canvas_draw_str(canvas, 14, 20, fr_perk_name(game->player.class_id, app->perk_choice));
    canvas_draw_str(canvas, 14, 34, fr_perk_desc(game->player.class_id, app->perk_choice));
    canvas_draw_str(canvas, 14, 52, "OK choose  Back");
}

const char* look_hint(AppContext* app) {
    return fr_look_text(app->game, app->cursor_x, app->cursor_y);
}

void draw_monster_card(Canvas* canvas, AppContext* app) {
    FrActor* actor = fr_actor_at(app->game, app->cursor_x, app->cursor_y);
    if(!actor || !fr_actor_visible_to_player(app->game, actor)) return;
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_box(canvas, 8, 7, 112, 50);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_frame(canvas, 8, 7, 112, 50);
    canvas_draw_str(canvas, 14, 18, fr_actor_name(actor->type));
    char stats[32];
    snprintf(stats, sizeof(stats), "HP %u/%u  Hit %u", actor->hp, actor->max_hp, actor->dmg);
    canvas_draw_str(canvas, 14, 30, stats);
    canvas_draw_str(canvas, 14, 41, fr_actor_flavor(actor->type));
    canvas_draw_str(canvas, 14, 53, "OK/Back");
}
