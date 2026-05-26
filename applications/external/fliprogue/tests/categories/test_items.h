#pragma once

#include "chest_actions.h"
#include "hazards.h"
#include "status_effects.h"
#include "terrain_effects.h"
#include "trinket_effects.h"

static void test_v11_chest_choose_one_and_persists_when_closed_or_full(void) {
    FrGame game;
    make_empty_test_room(&game);
    assert(!fr_place_chest(&game, game.player.x, game.player.y, false));
    assert(fr_place_chest(&game, 6, 5, false));
    FrItem* chest = fr_chest_at(&game, 6, 5);
    assert(chest != NULL);
    assert(fr_item_glyph(chest->type) == 'H');
    assert(fr_chest_choice_count(&game, chest) == 3);

    FrInvSlot first = fr_chest_choice_slot(&game, chest, 0);
    assert(first.type != FR_ITEM_NONE);
    assert(fr_cancel_chest(&game, chest).kind == FR_ACTION_BLOCKED);
    assert(chest->active);

    assert(fr_open_chest_choice(&game, chest, 0).kind == FR_ACTION_USE);
    assert(chest->active);
    assert(fr_chest_at(&game, 6, 5) == chest);
    assert(fr_chest_choice_count(&game, chest) == 0);
    assert(!fr_blocking_item_at(&game, 6, 5));
    assert(game.player.inv_count == 1);
    assert(game.player.inv[0].type == first.type);

    make_empty_test_room(&game);
    assert(fr_place_chest(&game, 6, 5, false));
    chest = fr_chest_at(&game, 6, 5);
    for(uint8_t i = 0; i < FR_INV_CAP; i++) {
        assert(fr_add_inventory(&game, FR_ITEM_WAND, FR_WAND_SPARK, 1));
    }
    assert(fr_open_chest_choice(&game, chest, 1).kind == FR_ACTION_BLOCKED);
    assert(chest->active);
    assert(strstr(game.log, "Pack full") != NULL);
}

static void test_unknown_item_labels_are_unique_and_class_aware(void) {
    FrGame warrior;
    FrGame mage;
    fr_game_init_class(&warrior, 77u, FR_CLASS_WARRIOR);
    fr_game_init_class(&mage, 77u, FR_CLASS_MAGE);

    assert(strcmp(warrior.potion_labels[FR_POTION_HEALING], warrior.potion_labels[FR_POTION_POISON]) != 0);
    assert(strcmp(warrior.scroll_labels[FR_SCROLL_FIRE], warrior.scroll_labels[FR_SCROLL_IDENTIFY]) != 0);

    assert(strstr(fr_potion_label(&warrior, FR_POTION_HEALING), "Potion") != NULL);
    assert(strstr(fr_scroll_label(&warrior, FR_SCROLL_FIRE), "Scroll") != NULL);
    assert(strcmp(fr_scroll_label(&mage, FR_SCROLL_FIRE), "Fire Bloom") == 0);
}

static void test_v071_potion_stacks_consume_one_at_a_time(void) {
    FrGame game;
    make_empty_test_room(&game);
    game.player.hp = 10;

    assert(fr_add_inventory(&game, FR_ITEM_POTION, FR_POTION_HEALING, 1));
    assert(fr_add_inventory(&game, FR_ITEM_POTION, FR_POTION_HEALING, 1));
    assert(fr_add_inventory(&game, FR_ITEM_POTION, FR_POTION_HEALING, 1));
    assert(game.player.inv_count == 1);
    assert(game.player.inv[0].amount == 3);
    assert(strstr(fr_inventory_label(&game, &game.player.inv[0]), "(3)") != NULL);

    assert(fr_use_inventory(&game, 0, FR_USE_QUAFF, game.player.x, game.player.y).kind == FR_ACTION_USE);
    assert(game.player.inv_count == 1);
    assert(game.player.inv[0].amount == 2);

    assert(fr_use_inventory(&game, 0, FR_USE_QUAFF, game.player.x, game.player.y).kind == FR_ACTION_USE);
    assert(game.player.inv_count == 1);
    assert(game.player.inv[0].amount == 1);

    assert(fr_use_inventory(&game, 0, FR_USE_QUAFF, game.player.x, game.player.y).kind == FR_ACTION_USE);
    assert(game.player.inv_count == 0);

    make_empty_test_room(&game);
    for(uint8_t i = 0; i < FR_INV_CAP; i++) {
        assert(fr_add_inventory(&game, FR_ITEM_POTION, FR_POTION_HEALING, 1));
    }
    assert(game.player.inv_count == 1);
    assert(game.player.inv[0].amount == FR_INV_CAP);
    assert(!fr_add_inventory(&game, FR_ITEM_POTION, FR_POTION_HEALING, 1));
}

static void test_inventory_drop_actions(void) {
    FrGame game;
    make_empty_test_room(&game);
    assert(fr_add_inventory(&game, FR_ITEM_POTION, FR_POTION_HEALING, 2));
    assert(fr_add_inventory(&game, FR_ITEM_SCROLL, FR_SCROLL_MAPPING, 1));

    assert(fr_use_inventory(&game, 0, FR_USE_DROP, game.player.x, game.player.y).kind == FR_ACTION_USE);
    assert(game.player.inv_count == 2);
    assert(game.player.inv[0].amount == 1);
    bool found_potion = false;
    for(uint8_t i = 0; i < FR_MAX_ITEMS; i++) {
        if(game.items[i].active && game.items[i].type == FR_ITEM_POTION &&
           game.items[i].subtype == FR_POTION_HEALING && game.items[i].amount == 1 &&
           game.items[i].x == game.player.x && game.items[i].y == game.player.y) {
            found_potion = true;
        }
    }
    assert(found_potion);

    assert(fr_use_inventory(&game, 1, FR_USE_DROP, game.player.x, game.player.y).kind == FR_ACTION_BLOCKED);
    assert(strstr(game.log, "Ground full") != NULL);
    game.items[0].active = false;

    assert(fr_use_inventory(&game, 1, FR_USE_DROP, game.player.x, game.player.y).kind == FR_ACTION_USE);
    assert(game.player.inv_count == 1);
    bool found_scroll = false;
    for(uint8_t i = 0; i < FR_MAX_ITEMS; i++) {
        if(game.items[i].active && game.items[i].type == FR_ITEM_SCROLL &&
           game.items[i].subtype == FR_SCROLL_MAPPING && game.items[i].x == game.player.x &&
           game.items[i].y == game.player.y) {
            found_scroll = true;
        }
    }
    assert(found_scroll);
}

static void test_v101_consumable_stacks_merge_anywhere_and_consume_one(void) {
    FrGame game;
    make_empty_test_room(&game);
    assert(fr_add_inventory(&game, FR_ITEM_POTION, FR_POTION_HEALING, 1));
    assert(fr_add_inventory(&game, FR_ITEM_SCROLL, FR_SCROLL_MAPPING, 1));
    assert(fr_add_inventory(&game, FR_ITEM_POTION, FR_POTION_HEALING, 1));
    assert(fr_add_inventory(&game, FR_ITEM_SCROLL, FR_SCROLL_MAPPING, 1));

    assert(game.player.inv_count == 2);
    assert(game.player.inv[0].type == FR_ITEM_POTION);
    assert(game.player.inv[0].amount == 2);
    assert(game.player.inv[1].type == FR_ITEM_SCROLL);
    assert(game.player.inv[1].amount == 2);
    assert(strstr(fr_inventory_label(&game, &game.player.inv[1]), "(2)") != NULL);

    assert(fr_use_inventory(&game, 1, FR_USE_READ, game.player.x, game.player.y).kind == FR_ACTION_USE);
    assert(game.player.inv_count == 2);
    assert(game.player.inv[1].amount == 1);
}

static void test_v12_successful_item_use_spends_full_game_tick(void) {
    FrGame game;
    make_empty_test_room(&game);
    game.player.hp = 10;
    game.player.effects = FR_FX_BURNING;
    game.player.fx_timer[FR_FX_BURNING_INDEX] = 3;

    assert(fr_add_inventory(&game, FR_ITEM_SCROLL, FR_SCROLL_MAPPING, 1));
    assert(fr_use_inventory(&game, 0, FR_USE_READ, game.player.x, game.player.y).kind == FR_ACTION_USE);
    assert(game.turn == 1);
    assert(game.player.hp == 9);
    assert(game.player.fx_timer[FR_FX_BURNING_INDEX] == 2);
}

static void test_v101_ranged_classes_start_with_more_native_resource(void) {
    FrGame ranger;
    FrGame mage;
    fr_game_init_class(&ranger, 42u, FR_CLASS_RANGER);
    fr_game_init_class(&mage, 42u, FR_CLASS_MAGE);

    assert(ranger.player.arrows >= 12);
    assert(mage.player.charges >= 7);
}

static void test_wand_and_charge_scroll(void) {
    FrGame game;
    make_empty_test_room(&game);
    FrActor* rat = fr_spawn_actor(&game, FR_MON_RAT, 7, 5);
    assert(rat != NULL);

    assert(fr_add_inventory(&game, FR_ITEM_WAND, FR_WAND_SPARK, 1));
    FrActionResult zap = fr_use_inventory(&game, 0, FR_USE_ZAP, rat->x, rat->y);
    assert(zap.kind == FR_ACTION_ZAP);
    assert((rat->effects & FR_FX_BURNING) != 0 || !rat->active);
    assert(game.player.inv_count == 1);
    assert(game.player.inv[0].amount == 0);
    assert(strstr(fr_inventory_label(&game, &game.player.inv[0]), "(0/3)") != NULL);
    assert(fr_use_inventory(&game, 0, FR_USE_ZAP, rat->x, rat->y).kind == FR_ACTION_BLOCKED);
    assert(strstr(game.log, "No charges") != NULL);

    game.player.inv_count = 0;
    memset(game.player.inv, 0, sizeof(game.player.inv));
    assert(fr_add_inventory(&game, FR_ITEM_WAND, FR_WAND_SLOW, 1));
    assert(fr_add_inventory(&game, FR_ITEM_SCROLL, FR_SCROLL_CHARGE, 1));
    assert(fr_use_inventory(&game, 1, FR_USE_READ, game.player.x, game.player.y).kind == FR_ACTION_USE);
    assert(game.player.inv_count == 1);
    assert(game.player.inv[0].type == FR_ITEM_WAND);
    assert(game.player.inv[0].amount == game.player.inv[0].flags);
}

static void test_potion_effect_matrix(void) {
    FrGame game;
    make_empty_test_room(&game);

    game.player.hp = 10;
    assert(fr_add_inventory(&game, FR_ITEM_POTION, FR_POTION_HEALING, 1));
    assert(fr_use_inventory(&game, 0, FR_USE_QUAFF, game.player.x, game.player.y).kind == FR_ACTION_USE);
    assert(game.player.hp == 18);
    assert((game.player.known_potions & (1u << FR_POTION_HEALING)) != 0);

    game.player.str = 6;
    assert(fr_add_inventory(&game, FR_ITEM_POTION, FR_POTION_STRENGTH, 1));
    assert(fr_use_inventory(&game, 0, FR_USE_QUAFF, game.player.x, game.player.y).kind == FR_ACTION_USE);
    assert(game.player.str == 7);

    assert(fr_add_inventory(&game, FR_ITEM_POTION, FR_POTION_SLOW, 1));
    assert(fr_use_inventory(&game, 0, FR_USE_QUAFF, game.player.x, game.player.y).kind == FR_ACTION_USE);
    assert((game.player.effects & FR_FX_SLOWED) != 0);
    assert(game.player.fx_timer[FR_FX_SLOWED_INDEX] == 9);

    assert(fr_add_inventory(&game, FR_ITEM_POTION, FR_POTION_SPEED, 1));
    assert(fr_use_inventory(&game, 0, FR_USE_QUAFF, game.player.x, game.player.y).kind == FR_ACTION_USE);
    assert((game.player.effects & FR_FX_SLOWED) == 0);

    game.player.effects |= FR_FX_BURNING;
    game.player.fx_timer[FR_FX_BURNING_INDEX] = 8;
    assert(fr_add_inventory(&game, FR_ITEM_POTION, FR_POTION_FIRE_WARD, 1));
    assert(fr_use_inventory(&game, 0, FR_USE_QUAFF, game.player.x, game.player.y).kind == FR_ACTION_USE);
    assert((game.player.effects & FR_FX_BURNING) == 0);
    assert(game.player.fire_ward_timer == 9);
    fr_fire_burst(&game, game.player.x, game.player.y);
    assert((game.player.effects & FR_FX_BURNING) == 0);
    fr_tick_effects(&game);
    assert(game.player.fire_ward_timer == 8);

    assert(fr_add_inventory(&game, FR_ITEM_POTION, FR_POTION_POISON, 1));
    assert(fr_use_inventory(&game, 0, FR_USE_QUAFF, game.player.x, game.player.y).kind == FR_ACTION_USE);
    assert((game.player.effects & FR_FX_POISONED) != 0);
    assert(game.player.fx_timer[FR_FX_POISONED_INDEX] == 11);

    assert(fr_add_inventory(&game, FR_ITEM_POTION, FR_POTION_BLINDNESS, 1));
    assert(fr_use_inventory(&game, 0, FR_USE_QUAFF, game.player.x, game.player.y).kind == FR_ACTION_USE);
    assert((game.player.effects & FR_FX_BLIND) != 0);

    assert(fr_add_inventory(&game, FR_ITEM_POTION, FR_POTION_CONFUSION, 1));
    assert(fr_use_inventory(&game, 0, FR_USE_QUAFF, game.player.x, game.player.y).kind == FR_ACTION_USE);
    assert((game.player.effects & FR_FX_CONFUSED) != 0);
}

static void test_v12_fire_potions_and_ash_bead_rules(void) {
    FrGame ward;
    make_empty_test_room(&ward);
    FrActor* rat = fr_spawn_actor(&ward, FR_MON_RAT, 7, 5);
    assert(rat != NULL);
    rat->effects = FR_FX_BURNING;
    rat->fx_timer[FR_FX_BURNING_INDEX] = 5;
    fr_ignite_fire_field(&ward, ward.floor, 7, 5, 0);
    assert(fr_add_inventory(&ward, FR_ITEM_POTION, FR_POTION_FIRE_WARD, 1));
    assert(fr_use_inventory(&ward, 0, FR_USE_THROW, 7, 5).kind == FR_ACTION_USE);
    assert((rat->effects & FR_FX_BURNING) == 0);
    assert(!fr_terrain_fire_at(&ward, ward.floor, 7, 5));

    FrGame frost;
    make_empty_test_room(&frost);
    fr_set_terrain(&frost, 7, 5, FR_TERR_WATER);
    fr_ignite_fire_field(&frost, frost.floor, 6, 5, 0);
    FrActor* bat = fr_spawn_actor(&frost, FR_MON_BAT, 8, 5);
    assert(bat != NULL);
    assert(fr_add_inventory(&frost, FR_ITEM_POTION, FR_POTION_FROST, 1));
    assert(fr_use_inventory(&frost, 0, FR_USE_THROW, 7, 5).kind == FR_ACTION_USE);
    assert(fr_get_terrain(&frost, 7, 5) == FR_TERR_ICE);
    assert(!fr_terrain_fire_at(&frost, frost.floor, 6, 5));
    assert(fr_add_inventory(&frost, FR_ITEM_POTION, FR_POTION_FROST, 1));
    assert(fr_use_inventory(&frost, 0, FR_USE_THROW, bat->x, bat->y).kind == FR_ACTION_USE);
    assert((bat->effects & FR_FX_SLOWED) != 0);

    FrGame flame;
    make_empty_test_room(&flame);
    assert(fr_add_inventory(&flame, FR_ITEM_POTION, FR_POTION_FLAME, 1));
    assert(fr_use_inventory(&flame, 0, FR_USE_THROW, 6, 5).kind == FR_ACTION_USE);
    assert(fr_active_terrain_field_count(&flame) == 1);
    assert(fr_add_inventory(&flame, FR_ITEM_POTION, FR_POTION_FLAME, 1));
    assert(fr_use_inventory(&flame, 0, FR_USE_THROW, 7, 5).kind == FR_ACTION_USE);
    assert(fr_active_terrain_field_count(&flame) == 1);

    FrGame ash;
    make_empty_test_room(&ash);
    ash.player.hp = ash.player.max_hp;
    ash.seed = 2u;
    assert(fr_add_inventory(&ash, FR_ITEM_TRINKET, FR_TRINKET_ASH, 1));
    assert(fr_use_inventory(&ash, 0, FR_USE_EQUIP, ash.player.x, ash.player.y).kind == FR_ACTION_USE);
    uint8_t hp_before = ash.player.hp;
    fr_fire_burst(&ash, ash.player.x, ash.player.y);
    assert(ash.player.hp == hp_before - 1);
    assert((ash.player.effects & FR_FX_BURNING) == 0);
    assert(fr_terrain_fire_at(&ash, ash.floor, ash.player.x, ash.player.y));

    FrGame no_ash;
    make_empty_test_room(&no_ash);
    no_ash.player.hp = no_ash.player.max_hp;
    no_ash.seed = 2u;
    hp_before = no_ash.player.hp;
    fr_fire_burst(&no_ash, no_ash.player.x, no_ash.player.y);
    assert(no_ash.player.hp == hp_before - 3);
    assert((no_ash.player.effects & FR_FX_BURNING) != 0);
}

static void test_scroll_effect_matrix(void) {
    FrGame game;
    make_empty_test_room(&game);

    assert(fr_add_inventory(&game, FR_ITEM_SCROLL, FR_SCROLL_ENCHANT_WEAPON, 1));
    assert(fr_use_inventory(&game, 0, FR_USE_READ, game.player.x, game.player.y).kind == FR_ACTION_USE);
    assert(game.player.sword_lvl == 1);

    assert(fr_add_inventory(&game, FR_ITEM_SCROLL, FR_SCROLL_ENCHANT_ARMOR, 1));
    assert(fr_use_inventory(&game, 0, FR_USE_READ, game.player.x, game.player.y).kind == FR_ACTION_USE);
    assert(game.player.body_lvl == 1);

    game.tiles[20][20] &= (uint8_t)~FR_TILE_EXPLORED;
    assert(fr_add_inventory(&game, FR_ITEM_SCROLL, FR_SCROLL_MAPPING, 1));
    assert(fr_use_inventory(&game, 0, FR_USE_READ, game.player.x, game.player.y).kind == FR_ACTION_USE);
    assert((game.tiles[20][20] & FR_TILE_EXPLORED) != 0);

    assert(fr_add_inventory(&game, FR_ITEM_SCROLL, FR_SCROLL_BLINK, 1));
    assert(fr_use_inventory(&game, 0, FR_USE_READ, 6, 5).kind == FR_ACTION_USE);
    assert(game.player.x == 6 && game.player.y == 5);

    assert(fr_spawn_actor(&game, FR_MON_RAT, 7, 5) != NULL);
    assert(fr_add_inventory(&game, FR_ITEM_SCROLL, FR_SCROLL_BLINK, 1));
    assert(fr_use_inventory(&game, 0, FR_USE_READ, 8, 5).kind == FR_ACTION_USE);
    assert(game.player.x == 8 && game.player.y == 5);

    game.items[0] = (FrItem){true, FR_ITEM_CHEST, 0, 7, 5, 1, 0};
    assert(fr_add_inventory(&game, FR_ITEM_SCROLL, FR_SCROLL_BLINK, 1));
    assert(fr_use_inventory(&game, 0, FR_USE_READ, 7, 5).kind == FR_ACTION_BLOCKED);
    assert(game.player.x == 8 && game.player.y == 5);

    game.items[0].active = false;
    assert(fr_add_inventory(&game, FR_ITEM_SCROLL, FR_SCROLL_BLINK, 1));
    assert(fr_use_inventory(&game, 0, FR_USE_READ, 3, 5).kind == FR_ACTION_USE);
    assert(game.player.x == 4 && game.player.y == 5);

    assert(fr_add_inventory(&game, FR_ITEM_SCROLL, FR_SCROLL_BLINK, 1));
    assert(fr_use_inventory(&game, 0, FR_USE_READ, game.player.x, game.player.y).kind == FR_ACTION_BLOCKED);

    assert(fr_add_inventory(&game, FR_ITEM_SCROLL, FR_SCROLL_BLINK, 1));
    uint8_t inv_before = game.player.inv_count;
    uint16_t known_before = game.player.known_scrolls;
    assert(fr_use_inventory(&game, (uint8_t)(game.player.inv_count - 1), FR_USE_READ, 0, 0).kind == FR_ACTION_BLOCKED);
    assert(game.player.inv_count == inv_before);
    assert(game.player.known_scrolls == known_before);
    assert(strstr(game.log, "Nothing happens") != NULL);
    assert(strstr(game.log, "blink") == NULL);

    FrGame phase;
    make_empty_test_room(&phase);
    for(uint8_t y = 0; y < FR_MAP_H; y++) {
        for(uint8_t x = 0; x < FR_MAP_W; x++) fr_set_terrain(&phase, x, y, FR_TERR_WALL);
    }
    fr_set_terrain(&phase, 5, 5, FR_TERR_FLOOR);
    fr_set_terrain(&phase, 6, 5, FR_TERR_FLOOR);
    fr_set_terrain(&phase, 6, 4, FR_TERR_FLOOR);
    phase.player.x = 5;
    phase.player.y = 5;
    phase.seed = 1u;
    assert(fr_spawn_actor(&phase, FR_MON_RAT, 6, 5) != NULL);
    assert(fr_add_inventory(&phase, FR_ITEM_SCROLL, FR_SCROLL_RANDOM_TELEPORT, 1));
    assert(fr_use_inventory(&phase, 0, FR_USE_READ, phase.player.x, phase.player.y).kind == FR_ACTION_USE);
    assert(phase.player.x == 6 && phase.player.y == 4);
    assert(strstr(phase.log, "Blink fails") == NULL);

    FrGame phase_escape;
    make_empty_test_room(&phase_escape);
    for(uint8_t y = 0; y < FR_MAP_H; y++) {
        for(uint8_t x = 0; x < FR_MAP_W; x++) fr_set_terrain(&phase_escape, x, y, FR_TERR_WALL);
    }
    for(uint8_t y = 2; y <= 12; y++) {
        for(uint8_t x = 2; x <= 18; x++) fr_set_terrain(&phase_escape, x, y, FR_TERR_FLOOR);
    }
    phase_escape.player.x = 8;
    phase_escape.player.y = 7;
    phase_escape.seed = 1u;
    assert(fr_add_inventory(&phase_escape, FR_ITEM_SCROLL, FR_SCROLL_RANDOM_TELEPORT, 1));
    assert(fr_use_inventory(&phase_escape, 0, FR_USE_READ, phase_escape.player.x, phase_escape.player.y).kind == FR_ACTION_USE);
    assert((uint8_t)(fr_abs_i8((int8_t)phase_escape.player.x - 8) + fr_abs_i8((int8_t)phase_escape.player.y - 7)) >= 5);

    FrGame phase_secret;
    make_empty_test_room(&phase_secret);
    for(uint8_t y = 0; y < FR_MAP_H; y++) {
        for(uint8_t x = 0; x < FR_MAP_W; x++) fr_set_terrain(&phase_secret, x, y, FR_TERR_WALL);
    }
    fr_set_terrain(&phase_secret, 5, 5, FR_TERR_FLOOR);
    fr_set_terrain(&phase_secret, 7, 5, FR_TERR_FLOOR);
    phase_secret.player.x = 5;
    phase_secret.player.y = 5;
    phase_secret.seed = 1u;
    phase_secret.tiles[5][6] |= FR_TILE_HIDDEN_DOOR;
    assert(fr_add_inventory(&phase_secret, FR_ITEM_SCROLL, FR_SCROLL_RANDOM_TELEPORT, 1));
    assert(fr_use_inventory(&phase_secret, 0, FR_USE_READ, phase_secret.player.x, phase_secret.player.y).kind == FR_ACTION_USE);
    assert((phase_secret.tiles[5][6] & FR_TILE_HIDDEN_DOOR) == 0);
    assert(fr_get_terrain(&phase_secret, 6, 5) == FR_TERR_DOOR_OPEN);
    assert(phase_secret.player.x != 5 || phase_secret.player.y != 5);

    FrGame reveal;
    make_empty_test_room(&reveal);
    assert(fr_add_inventory(&reveal, FR_ITEM_SCROLL, FR_SCROLL_REVEAL, 1));
    inv_before = reveal.player.inv_count;
    known_before = reveal.player.known_scrolls;
    assert(fr_use_inventory(&reveal, 0, FR_USE_READ, reveal.player.x, reveal.player.y).kind == FR_ACTION_BLOCKED);
    assert(reveal.player.inv_count == inv_before);
    assert(reveal.player.known_scrolls == known_before);
    assert(strstr(reveal.log, "Nothing happens") != NULL);

    FrGame mapping;
    make_empty_test_room(&mapping);
    mapping.tiles[4][6] |= FR_TILE_HIDDEN_DOOR;
    mapping.tiles[6][6] |= FR_TILE_HIDDEN_DOOR;
    assert(fr_add_inventory(&mapping, FR_ITEM_SCROLL, FR_SCROLL_MAPPING, 1));
    assert(fr_use_inventory(&mapping, 0, FR_USE_READ, mapping.player.x, mapping.player.y).kind == FR_ACTION_USE);
    assert((mapping.tiles[4][6] & FR_TILE_HIDDEN_DOOR) == 0);
    assert((mapping.tiles[6][6] & FR_TILE_HIDDEN_DOOR) == 0);
    assert(mapping.last_event == FR_EVENT_SECRET);
    assert(mapping.log[0] != '\0');

    FrActor* rat = fr_spawn_actor(&game, FR_MON_RAT, 8, 5);
    assert(rat != NULL);
    assert(fr_add_inventory(&game, FR_ITEM_SCROLL, FR_SCROLL_FEAR, 1));
    assert(fr_use_inventory(&game, (uint8_t)(game.player.inv_count - 1), FR_USE_READ, rat->x, rat->y).kind == FR_ACTION_USE);
    assert((rat->effects & FR_FX_AFRAID) != 0);
    assert(rat->fx_timer[FR_FX_AFRAID_INDEX] == 7);
}

static void test_v102_scroll_pickup_log_uses_runes_only_when_unknown(void) {
    FrGame game;
    make_empty_test_room(&game);
    game.items[0] = (FrItem){true, FR_ITEM_SCROLL, FR_SCROLL_MAPPING, 6, 5, 1, 0};
    assert(fr_move_player(&game, 1, 0).kind == FR_ACTION_MOVE);
    assert(strstr(game.log, "Picked") != NULL);
    assert(strstr(game.log, "Scroll") == NULL);
}

static void test_wand_effect_matrix(void) {
    FrGame game;
    make_empty_test_room(&game);

    FrActor* slow_rat = fr_spawn_actor(&game, FR_MON_RAT, 7, 5);
    assert(slow_rat != NULL);
    assert(fr_add_inventory(&game, FR_ITEM_WAND, FR_WAND_SLOW, 1));
    assert(fr_use_inventory(&game, 0, FR_USE_ZAP, slow_rat->x, slow_rat->y).kind == FR_ACTION_ZAP);
    assert((slow_rat->effects & FR_FX_SLOWED) != 0);
    assert(slow_rat->fx_timer[FR_FX_SLOWED_INDEX] == 9);
    game.player.inv_count = 0;
    memset(game.player.inv, 0, sizeof(game.player.inv));

    FrActor* venom_rat = fr_spawn_actor(&game, FR_MON_RAT, 8, 6);
    assert(venom_rat != NULL);
    assert(fr_add_inventory(&game, FR_ITEM_WAND, FR_WAND_MARK, 1));
    assert(fr_use_inventory(&game, 0, FR_USE_ZAP, venom_rat->x, venom_rat->y).kind == FR_ACTION_ZAP);
    assert((venom_rat->effects & FR_FX_POISONED) != 0);
    assert(venom_rat->fx_timer[FR_FX_POISONED_INDEX] == 7);
    game.player.inv_count = 0;
    memset(game.player.inv, 0, sizeof(game.player.inv));

    assert(fr_add_inventory(&game, FR_ITEM_WAND, FR_WAND_BLINK, 1));
    assert(fr_use_inventory(&game, 0, FR_USE_ZAP, 4, 4).kind == FR_ACTION_ZAP);
    assert(game.player.x == 4 && game.player.y == 4);
}

static void test_identify_scroll_marks_one_unknown(void) {
    FrGame game;
    make_empty_test_room(&game);
    assert(fr_add_inventory(&game, FR_ITEM_POTION, FR_POTION_HEALING, 1));
    assert(fr_add_inventory(&game, FR_ITEM_SCROLL, FR_SCROLL_FIRE, 1));
    assert(fr_add_inventory(&game, FR_ITEM_SCROLL, FR_SCROLL_IDENTIFY, 1));

    assert(fr_use_inventory(&game, 2, FR_USE_READ, game.player.x, game.player.y).kind == FR_ACTION_USE);
    assert(game.player.known_potions != 0xFFFFu);
    assert(game.player.known_scrolls != 0xFFFFu);
    assert((game.player.known_potions != 0u) || (game.player.known_scrolls != 0u));
}

static void test_inventory_healing_and_fire_scroll(void) {
    FrGame game;
    make_empty_test_room(&game);
    game.player.hp = 10;

    assert(fr_add_inventory(&game, FR_ITEM_POTION, FR_POTION_HEALING, 1));
    FrActionResult heal = fr_use_inventory(&game, 0, FR_USE_QUAFF, game.player.x, game.player.y);
    assert(heal.kind == FR_ACTION_USE);
    assert(game.player.hp == 18);
    assert(game.player.inv_count == 0);

    FrActor* rat = fr_spawn_actor(&game, FR_MON_RAT, 7, 5);
    assert(rat != NULL);
    assert(fr_add_inventory(&game, FR_ITEM_SCROLL, FR_SCROLL_FIRE, 1));

    FrActionResult fire = fr_use_inventory(&game, 0, FR_USE_READ, rat->x, rat->y);
    assert(fire.kind == FR_ACTION_USE);
    assert(!rat->active || (rat->effects & FR_FX_BURNING) != 0);
    assert(!rat->active || rat->fx_timer[FR_FX_BURNING_INDEX] == 4);
}

static void test_v09_throwable_stack_trinket_and_identify_contracts(void) {
    FrGame game;
    make_empty_test_room(&game);

    for(uint8_t i = 0; i < 19; i++) {
        assert(fr_add_inventory(&game, FR_ITEM_POTION, FR_POTION_HEALING, 1));
    }
    assert(game.player.inv_count == 1);
    assert(game.player.inv[0].amount == 19);
    assert(fr_add_inventory(&game, FR_ITEM_THROWABLE, FR_THROW_STONE, 20));
    assert(game.player.inv_count == 2);
    assert(strstr(fr_inventory_label(&game, &game.player.inv[1]), "Stones (20)") != NULL);
    assert(!fr_add_inventory(&game, FR_ITEM_SCROLL, FR_SCROLL_MAPPING, 1));

    game.player.inv_count = 0;
    memset(game.player.inv, 0, sizeof(game.player.inv));
    snprintf(game.trinket_labels[FR_TRINKET_DEW], FR_LABEL_SIZE, "Charm ZUN");
    snprintf(game.trinket_labels[FR_TRINKET_ASH], FR_LABEL_SIZE, "Charm ASH");
    assert(fr_add_inventory(&game, FR_ITEM_TRINKET, FR_TRINKET_DEW, 1));
    assert(strstr(fr_inventory_label(&game, &game.player.inv[0]), "Charm") != NULL);
    assert((game.player.known_trinkets & (1u << FR_TRINKET_DEW)) == 0);
    assert(fr_use_inventory(&game, 0, FR_USE_EQUIP, game.player.x, game.player.y).kind == FR_ACTION_USE);
    assert((game.player.known_trinkets & (1u << FR_TRINKET_DEW)) != 0);
    assert((game.player.inv[0].flags & FR_INV_EQUIPPED) != 0);
    assert(strstr(fr_inventory_label(&game, &game.player.inv[0]), "Dew Charm") != NULL);
    assert(strcmp(game.log, "You wear Dew Charm.") == 0);

    assert(fr_add_inventory(&game, FR_ITEM_TRINKET, FR_TRINKET_ASH, 1));
    assert(fr_use_inventory(&game, 1, FR_USE_EQUIP, game.player.x, game.player.y).kind == FR_ACTION_USE);
    assert((game.player.inv[0].flags & FR_INV_EQUIPPED) == 0);
    assert((game.player.inv[1].flags & FR_INV_EQUIPPED) != 0);
    assert(strcmp(game.log, "You wear Ash Bead.") == 0);

    FrGame identify;
    make_empty_test_room(&identify);
    snprintf(identify.trinket_labels[FR_TRINKET_LOCKET], FR_LABEL_SIZE, "Charm YON");
    assert(fr_add_inventory(&identify, FR_ITEM_POTION, FR_POTION_HEALING, 1));
    assert(fr_add_inventory(&identify, FR_ITEM_TRINKET, FR_TRINKET_LOCKET, 1));
    assert(fr_add_inventory(&identify, FR_ITEM_SCROLL, FR_SCROLL_IDENTIFY, 1));
    assert(fr_use_inventory(&identify, 2, FR_USE_READ, 1, 0xFF).kind == FR_ACTION_USE);
    assert((identify.player.known_trinkets & (1u << FR_TRINKET_LOCKET)) != 0);
    assert((identify.player.known_potions & (1u << FR_POTION_HEALING)) == 0);
}

static void test_v12_inventory_hints_hide_unknowns_and_describe_charms(void) {
    FrGame game;
    make_empty_test_room(&game);
    FrInvSlot cinder = {FR_ITEM_TRINKET, FR_TRINKET_CINDER, 1, FR_INV_EQUIPPED | FR_INV_CURSED};
    FrInvSlot scout = {FR_ITEM_TRINKET, FR_TRINKET_SCOUT, 1, 0};
    FrInvSlot scroll = {FR_ITEM_SCROLL, FR_SCROLL_FIRE, 1, 0};
    FrInvSlot darts = {FR_ITEM_THROWABLE, FR_THROW_DART, 4, 0};

    assert(strcmp(fr_inventory_hint(&game, &cinder), "Unknown Charm") == 0);
    assert(strcmp(fr_inventory_hint(&game, &scroll), "Unknown Scroll") == 0);

    game.player.known_trinkets |= (uint16_t)(1u << FR_TRINKET_CINDER);
    game.player.known_trinkets |= (uint16_t)(1u << FR_TRINKET_SCOUT);
    assert(strcmp(fr_inventory_hint(&game, &cinder), "Burns all. Stuck.") == 0);
    assert(strcmp(fr_inventory_hint(&game, &scout), "Finds shy traps.") == 0);
    assert(strcmp(fr_inventory_hint(&game, &darts), "Sharp. One-way.") == 0);
}

static void test_v12_cinder_charm_bites_every_ten_turns_for_random_damage(void) {
    FrGame game;
    make_empty_test_room(&game);
    game.seed = 7u;
    game.player.hp = 12;
    assert(fr_add_inventory(&game, FR_ITEM_TRINKET, FR_TRINKET_CINDER, 1));
    assert(fr_use_inventory(&game, 0, FR_USE_EQUIP, game.player.x, game.player.y).kind == FR_ACTION_USE);

    for(uint8_t turn = 1; turn < 10; turn++) {
        game.turn = turn;
        fr_tick_trinkets(&game);
        assert(game.player.hp == 12);
    }

    game.turn = 10;
    fr_tick_trinkets(&game);
    assert(game.player.hp >= 10 && game.player.hp <= 11);
    assert(strstr(game.log, "Cinder bites") != NULL);
}
