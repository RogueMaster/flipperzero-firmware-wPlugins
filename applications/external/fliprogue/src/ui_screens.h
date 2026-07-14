#pragma once

#include "ui_internal.h"

void draw_title(Canvas* canvas, AppContext* app);
void draw_class_select(Canvas* canvas, AppContext* app);
void draw_help_menu(Canvas* canvas, AppContext* app);
void draw_help_page(Canvas* canvas, AppContext* app);
uint8_t inventory_visible_count(const FrGame* game, uint8_t tab);
uint8_t inventory_slot_index_at(const FrGame* game, uint8_t tab, uint8_t visible_index);
uint8_t identify_visible_count(const FrGame* game, uint8_t source_index);
uint8_t identify_slot_index_at(const FrGame* game, uint8_t source_index, uint8_t visible_index);
void inventory_clamp_selection(AppContext* app);
void draw_inventory(Canvas* canvas, AppContext* app);
uint8_t item_choice_count(const FrInvSlot* slot);
const char* item_choice_label(const FrInvSlot* slot, uint8_t choice);
void draw_item_choice(Canvas* canvas, AppContext* app);
void draw_chest_choice(Canvas* canvas, AppContext* app);
void draw_identify_pick(Canvas* canvas, AppContext* app);
void draw_perk_choice(Canvas* canvas, AppContext* app);
void draw_perk_detail(Canvas* canvas, AppContext* app);
const char* look_hint(AppContext* app);
void draw_monster_card(Canvas* canvas, AppContext* app);
