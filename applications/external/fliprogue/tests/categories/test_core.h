#pragma once

#include "look_text.h"
#include "terrain_effects.h"

static void test_new_game_defaults(void) {
    FrGame game;
    fr_game_init(&game, 1234u);

    assert(FR_MAP_W == 64);
    assert(FR_MAP_H == 32);
    assert(game.floor == 1);
    assert(game.player.class_id == FR_CLASS_WARRIOR);
    assert(game.player.hp == 24);
    assert(game.player.max_hp == 24);
    assert(game.player.str == 6);
    assert(game.player.dex == 3);
    assert(game.player.wil == 3);
    assert(game.player.food >= 3);
    assert(game.player.hunger >= 230);
    assert(fr_count_active_actors(&game) >= 5);
    assert(fr_count_active_items(&game) >= 3);
    assert(count_items_of_type(&game, FR_ITEM_FOOD) >= 1);

    uint8_t sx = 0;
    uint8_t sy = 0;
    assert(fr_find_first_tile(&game, FR_TERR_STAIRS_DOWN, &sx, &sy));
    assert(sx > 0 && sx < FR_MAP_W - 1);
    assert(sy > 0 && sy < FR_MAP_H - 1);
    assert(count_terrain(&game, FR_TERR_STAIRS_DOWN) == 1);

    assert(fr_find_first_tile(&game, FR_TERR_STAIRS_UP, &sx, &sy));
    assert(sx > 0 && sx < FR_MAP_W - 1);
    assert(sy > 0 && sy < FR_MAP_H - 1);
    assert(count_terrain(&game, FR_TERR_STAIRS_UP) == 1);
}

static void test_v072_memory_budget(void) {
    assert(FR_MAX_TILE_DELTAS == 48);
    assert(sizeof(FrFloorState) < 900);
    assert(sizeof(FrGame) < 22000);
}

static void test_v10_ui_page_math(void) {
    FrUiPage empty = fr_ui_page(0, 0, 4);
    assert(empty.first == 0);
    assert(empty.page == 1);
    assert(empty.pages == 1);

    FrUiPage first = fr_ui_page(0, 5, 4);
    assert(first.first == 0);
    assert(first.page == 1);
    assert(first.pages == 2);
    assert(fr_ui_next_page_scroll(first.first, 5, 4) == 1);

    FrUiPage last = fr_ui_page(1, 5, 4);
    assert(last.first == 1);
    assert(last.page == 2);
    assert(last.pages == 2);

    assert(fr_ui_next_page_scroll(4, 10, 4) == 6);
    FrUiPage third = fr_ui_page(6, 10, 4);
    assert(third.page == 3);
    assert(third.pages == 3);
    assert(fr_ui_prev_page_scroll(6, 4) == 2);

    char lines[2][32];
    uint8_t line_count =
        fr_ui_death_log_lines("Rat hits you. Bat blinks. Killed by Bat.", lines, 2, 27);
    assert(line_count == 2);
    assert(strcmp(lines[0], "Rat hits you. Bat blinks.") == 0);
    assert(strcmp(lines[1], "Killed by Bat.") == 0);
}

static void test_v12_look_text_reports_status_and_terrain_effects(void) {
    FrGame game;
    make_empty_test_room(&game);
    game.player.effects = FR_FX_CONFUSED | FR_FX_BURNING;
    game.player.fire_ward_timer = 3;
    const char* you = fr_look_text(&game, game.player.x, game.player.y);
    assert(strstr(you, "You.") != NULL);
    assert(strstr(you, "Burning") != NULL);
    assert(strstr(you, "Ward") != NULL || strstr(you, "Confused") != NULL);

    FrActor* bat = fr_spawn_actor(&game, FR_MON_BAT, 6, 5);
    assert(bat != NULL);
    bat->effects = FR_FX_BURNING | FR_FX_CONFUSED;
    assert(strstr(fr_look_text(&game, 6, 5), "Bat.") != NULL);
    assert(strstr(fr_look_text(&game, 6, 5), "Burning") != NULL);

    fr_ignite_fire_field(&game, game.floor, 7, 5, 0);
    assert(strcmp(fr_look_text(&game, 7, 5), "Ground. Burning.") == 0);
    fr_set_terrain(&game, 8, 5, FR_TERR_WATER);
    assert(strcmp(fr_look_text(&game, 8, 5), "Water.") == 0);
    fr_set_terrain(&game, 4, 5, FR_TERR_ICE);
    assert(strcmp(fr_look_text(&game, 4, 5), "Ice.") == 0);
}

static void test_v02_class_starts(void) {
    FrGame warrior;
    FrGame ranger;
    FrGame mage;
    fr_game_init_class(&warrior, 1u, FR_CLASS_WARRIOR);
    fr_game_init_class(&ranger, 1u, FR_CLASS_RANGER);
    fr_game_init_class(&mage, 1u, FR_CLASS_MAGE);

    assert(FR_MAX_FLOORS == 18);
    assert(warrior.player.max_hp == 24);
    assert(warrior.player.shield_lvl == 1);

    assert(ranger.player.class_id == FR_CLASS_RANGER);
    assert(ranger.player.max_hp == 20);
    assert(ranger.player.arrows >= 6);
    assert(fr_player_sees_traps(&ranger));

    assert(mage.player.class_id == FR_CLASS_MAGE);
    assert(mage.player.max_hp == 18);
    assert(mage.player.charges >= 3);
    assert(fr_player_knows_scrolls(&mage));
    assert(strcmp(fr_player_class_name(FR_CLASS_MAGE), "Mage") == 0);
}

static void test_ranger_line_shot_and_warrior_walks(void) {
    FrGame ranger;
    make_empty_test_room(&ranger);
    ranger.player.class_id = FR_CLASS_RANGER;
    ranger.player.arrows = 3;
    ranger.player.dex = 6;
    FrActor* rat = fr_spawn_actor(&ranger, FR_MON_RAT, 8, 5);
    assert(rat != NULL);

    FrActionResult shot = fr_try_direction(&ranger, 1, 0);
    assert(shot.kind == FR_ACTION_RANGED);
    assert(ranger.player.x == 5);
    assert(ranger.player.arrows == 2);
    assert(rat->hp < rat->max_hp || !rat->active);

    FrGame warrior;
    make_empty_test_room(&warrior);
    warrior.player.class_id = FR_CLASS_WARRIOR;
    fr_spawn_actor(&warrior, FR_MON_RAT, 8, 5);
    FrActionResult walk = fr_try_direction(&warrior, 1, 0);
    assert(walk.kind == FR_ACTION_MOVE);
    assert(warrior.player.x == 6);
}

static void test_ranged_resource_gate(void) {
    FrGame ranger;
    make_empty_test_room(&ranger);
    ranger.player.class_id = FR_CLASS_RANGER;
    ranger.player.arrows = 0;
    assert(!fr_player_can_ranged(&ranger));
    ranger.player.arrows = 1;
    assert(fr_player_can_ranged(&ranger));

    FrGame mage;
    make_empty_test_room(&mage);
    mage.player.class_id = FR_CLASS_MAGE;
    mage.player.charges = 0;
    assert(!fr_player_can_ranged(&mage));
    mage.player.charges = 1;
    assert(fr_player_can_ranged(&mage));

    FrGame warrior;
    make_empty_test_room(&warrior);
    warrior.player.class_id = FR_CLASS_WARRIOR;
    assert(!fr_player_can_ranged(&warrior));
}

static void test_ranged_hit_wakes_non_chasing_bat(void) {
    FrGame game;
    make_empty_test_room(&game);
    game.player.class_id = FR_CLASS_RANGER;
    game.player.arrows = 3;
    game.player.dex = 0;
    FrActor* bat = fr_spawn_actor(&game, FR_MON_BAT, 8, 5);
    assert(bat != NULL);
    bat->hp = 4;
    bat->max_hp = 4;
    bat->flags = 0;
    bat->memory = 0;

    FrActionResult shot = fr_try_direction(&game, 1, 0);
    assert(shot.kind == FR_ACTION_RANGED);
    assert(bat->active);
    assert(bat->memory > 0);
    assert(bat->x == 7 && bat->y == 5);
}

static void test_hunger_states_and_food(void) {
    FrGame game;
    make_empty_test_room(&game);
    game.player.hunger = 130;
    assert(fr_hunger_state(&game) == FR_HUNGER_OK);
    game.player.hunger = 80;
    assert(fr_hunger_state(&game) == FR_HUNGER_HUNGRY);
    game.player.hunger = 0;
    game.turn = 9;
    game.player.hp = 10;
    assert(fr_move_player(&game, 1, 0).kind == FR_ACTION_MOVE);
    assert(game.player.hp == 9);
    game.player.food = 1;
    assert(fr_eat_food(&game).kind == FR_ACTION_USE);
    assert(game.player.food == 0);
    assert(game.player.hunger >= 180);
    assert(fr_hunger_state(&game) == FR_HUNGER_OK);
}

static void test_v07_hunger_ticks_less_often(void) {
    FrGame game;
    make_empty_test_room(&game);
    game.player.hunger = 200;

    assert(fr_rest(&game).kind == FR_ACTION_REST);
    assert(game.player.hunger == 200);
    assert(fr_rest(&game).kind == FR_ACTION_REST);
    assert(game.player.hunger == 199);
}

static void test_v07_orb_slows_hunger_clock(void) {
    FrGame game;
    make_empty_test_room(&game);
    game.player.has_orb = 1;
    game.player.hunger = 200;

    for(uint8_t i = 0; i < 9; i++) {
        assert(fr_rest(&game).kind == FR_ACTION_REST);
        assert(game.player.hunger == 200);
    }
    assert(fr_rest(&game).kind == FR_ACTION_REST);
    assert(game.player.hunger == 199);

    make_empty_test_room(&game);
    game.player.has_orb = 1;
    game.player.hunger = 0;
    game.player.hp = 10;
    game.turn = 9;
    assert(fr_move_player(&game, 1, 0).kind == FR_ACTION_MOVE);
    assert(game.player.hp == 10);

    make_empty_test_room(&game);
    game.player.has_orb = 1;
    game.player.hunger = 0;
    game.player.hp = 10;
    game.turn = 49;
    assert(fr_move_player(&game, 1, 0).kind == FR_ACTION_MOVE);
    assert(game.player.hp == 9);
}

static void test_xp_level_skills(void) {
    FrGame game;
    fr_game_init_class(&game, 55u, FR_CLASS_RANGER);
    assert(game.player.level == 1);
    assert(game.player.skills == 0);
    assert(game.player.perks == 0);
    assert(game.player.pending_perks == 0);
    uint8_t start_arrows = game.player.arrows;
    uint8_t start_bow = game.player.bow_lvl;

    fr_award_xp(&game, 9);
    assert(game.player.xp == 9);
    assert(game.player.level == 1);
    assert(game.player.skills == 0);
    fr_award_xp(&game, 1);
    assert(game.player.xp == 10);
    assert(game.player.level == 2);
    assert(game.player.skills == 0);
    fr_award_xp(&game, 15);
    assert(game.player.xp == 25);
    assert(game.player.level == 2);
    fr_award_xp(&game, 1);
    assert(game.player.xp == 26);
    assert(game.player.level == 3);
    assert((game.player.skills & FR_SKILL_SLOT1) != 0);
    assert(game.player.arrows == start_arrows + 3);
    assert(game.player.pending_perks == 1);
    assert(fr_apply_perk(&game, 1));
    assert(game.player.pending_perks == 0);
    assert((game.player.perks & FR_PERK_2) != 0);
    assert(game.player.bow_lvl == start_bow);

    fr_award_xp(&game, 53);
    assert(game.player.xp == 79);
    assert(game.player.level == 4);
    fr_award_xp(&game, 1);
    assert(game.player.xp == 80);
    assert(game.player.level == 5);
    assert((game.player.skills & FR_SKILL_SLOT2) == 0);
    assert(game.player.pending_perks == 0);
    fr_award_xp(&game, 35);
    assert(game.player.xp == 115);
    assert(game.player.level == 6);
    assert((game.player.skills & FR_SKILL_SLOT2) != 0);
    assert(game.player.bow_lvl == start_bow + 1);
    assert(game.player.pending_perks == 1);
    assert(!fr_apply_perk(&game, 1));
    assert(game.player.pending_perks == 1);
    assert(fr_apply_perk(&game, 0));
    assert(game.player.pending_perks == 0);
    assert((game.player.perks & FR_PERK_1) != 0);
}

static void test_bad_player_effects_change_play(void) {
    FrGame blind;
    make_empty_test_room(&blind);
    blind.player.effects = FR_FX_BLIND;
    blind.player.fx_timer[FR_FX_BLIND_INDEX] = 8;
    fr_update_fov(&blind);
    assert((blind.tiles[5][6] & FR_TILE_VISIBLE) != 0);
    assert((blind.tiles[5][8] & FR_TILE_VISIBLE) == 0);

    FrGame slow;
    make_empty_test_room(&slow);
    slow.player.effects = FR_FX_SLOWED;
    slow.player.fx_timer[FR_FX_SLOWED_INDEX] = 8;
    FrActor* rat = fr_spawn_actor(&slow, FR_MON_RAT, 8, 5);
    assert(rat != NULL);
    assert(fr_rest(&slow).kind == FR_ACTION_REST);
    assert(rat->x == 6);

    FrGame fear;
    make_empty_test_room(&fear);
    fear.player.effects = FR_FX_AFRAID;
    fear.player.fx_timer[FR_FX_AFRAID_INDEX] = 6;
    FrActor* wight = fr_spawn_actor(&fear, FR_MON_WIGHT, 8, 5);
    assert(wight != NULL);
    wight->flags &= (uint8_t)~FR_ACTOR_HIDDEN;
    assert(fr_move_player(&fear, 1, 0).kind == FR_ACTION_MOVE);
    assert(fear.player.x == 4);
}

static void test_v07_death_cause_status(void) {
    FrGame killed;
    make_empty_test_room(&killed);
    killed.player.hp = 1;
    killed.turn = 1;
    FrActor* rat = fr_spawn_actor(&killed, FR_MON_RAT, 6, 5);
    assert(rat != NULL);
    assert(fr_rest(&killed).kind == FR_ACTION_REST);
    assert(killed.mode == FR_MODE_GAME_OVER);
    assert(killed.death_cause == FR_DEATH_KILLED);

    FrGame starved;
    make_empty_test_room(&starved);
    starved.player.hp = 1;
    starved.player.hunger = 0;
    starved.turn = 9;
    assert(fr_move_player(&starved, 1, 0).kind == FR_ACTION_MOVE);
    assert(starved.mode == FR_MODE_GAME_OVER);
    assert(starved.death_cause == FR_DEATH_STARVED);
}

static void test_v10_starving_blocks_wait_without_spending_turn(void) {
    FrGame game;
    make_empty_test_room(&game);
    game.player.hp = 4;
    game.player.hunger = 0;
    game.turn = 99;

    assert(fr_rest(&game).kind == FR_ACTION_BLOCKED);
    assert(game.mode == FR_MODE_PLAYING);
    assert(game.player.hp == 4);
    assert(game.turn == 99);
    assert(strstr(game.log, "Too starved") != NULL);
}

static void test_v11_stunned_player_loses_turns_and_can_die(void) {
    FrGame game;
    make_empty_test_room(&game);
    game.player.effects = FR_FX_STUNNED;
    game.player.fx_timer[FR_FX_STUNNED_INDEX] = 2;
    game.turn = 20;
    assert(fr_move_player(&game, 1, 0).kind == FR_ACTION_REST);
    assert(game.turn == 21);
    assert(game.player.x == 5 && game.player.y == 5);
    assert(game.player.fx_timer[FR_FX_STUNNED_INDEX] == 1);
    assert(strstr(game.log, "Dazed") != NULL);

    make_empty_test_room(&game);
    game.player.hp = 1;
    game.player.effects = FR_FX_STUNNED;
    game.player.fx_timer[FR_FX_STUNNED_INDEX] = 2;
    FrActor* rat = fr_spawn_actor(&game, FR_MON_RAT, 6, 5);
    assert(rat != NULL);
    assert(fr_move_player(&game, 1, 0).kind == FR_ACTION_REST);
    assert(game.mode == FR_MODE_GAME_OVER);
}

static void test_v09_log_braid_caps_two_phrases(void) {
    FrGame game;
    make_empty_test_room(&game);
    game.seed = 1u;
    FrActor* rat = fr_spawn_actor(&game, FR_MON_RAT, 7, 5);
    FrActor* bat = fr_spawn_actor(&game, FR_MON_BAT, 7, 6);
    assert(rat != NULL && bat != NULL);
    rat->hp = rat->max_hp = 5;
    bat->hp = bat->max_hp = 5;
    rat->flags = 0;
    bat->flags = 0;

    assert(fr_add_inventory(&game, FR_ITEM_SCROLL, FR_SCROLL_FIRE, 1));
    assert(fr_use_inventory(&game, 0, FR_USE_READ, 7, 5).kind == FR_ACTION_USE);
    assert(count_log_phrases(game.log) <= 2);
}

static void test_v091_death_log_has_one_cause(void) {
    FrGame game;
    make_empty_test_room(&game);
    game.player.hp = 1;
    game.seed = 1u;
    FrActor* bat = fr_spawn_actor(&game, FR_MON_BAT, 6, 5);
    FrActor* rat = fr_spawn_actor(&game, FR_MON_RAT, 5, 6);
    assert(bat != NULL && rat != NULL);
    bat->dmg = 4;
    rat->dmg = 4;
    bat->flags = FR_ACTOR_CHASES;
    rat->flags = FR_ACTOR_CHASES;

    assert(fr_rest(&game).kind == FR_ACTION_REST);
    assert(game.mode == FR_MODE_GAME_OVER);
    assert(game.death_cause == FR_DEATH_KILLED);
    assert(count_substrings(game.log, "Killed by") == 1);
    assert(strstr(game.log, "hits you") == NULL);
}
