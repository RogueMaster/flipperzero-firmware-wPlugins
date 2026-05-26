#pragma once

#include "chest_actions.h"
#include "monster_defs.h"
#include "monster_ai.h"

static void test_v03_monster_table_and_boss_floor(void) {
    assert(fr_actor_glyph(FR_MON_WISP) == 'w');
    assert(fr_actor_glyph(FR_MON_KOBOLD) == 'k');
    assert(fr_actor_glyph(FR_MON_CUBE) == 'C');
    assert(fr_actor_glyph(FR_MON_DRAGON) == 'D');
    assert(fr_actor_glyph(FR_MON_WIGHT) == 'G');
    assert(fr_actor_glyph(FR_MON_EEL) == 'e');
    assert(fr_actor_glyph(FR_MON_MIMIC) == 'M');
    assert(fr_actor_glyph(FR_MON_LURKER) == 'L');
    assert(fr_actor_glyph(FR_MON_YONDER_WARDEN) == 'W');
    assert(strcmp(fr_actor_name(FR_MON_ARCHER), "Skeleton Archer") == 0);
    assert(strcmp(fr_actor_name(FR_MON_WIGHT), "Wight") == 0);
    assert(strstr(fr_actor_flavor(FR_MON_ARCHER), "Nothing personal") != NULL);
    assert(strcmp(fr_actor_name(FR_MON_DRAGON), "Dragonling") == 0);
    assert(strcmp(fr_actor_name(FR_MON_EEL), "Eel") == 0);
    assert(strstr(fr_actor_flavor(FR_MON_EEL), "Wet bite") != NULL);
    assert(strcmp(fr_actor_name(FR_MON_MIMIC), "Mimic") == 0);
    assert(strstr(fr_actor_flavor(FR_MON_MIMIC), "Box with opinions") != NULL);
    assert(fr_monster_def(FR_MON_MIMIC)->hp >= 14);
    assert(fr_monster_def(FR_MON_MIMIC)->damage >= 6);
    assert(strcmp(fr_actor_name(FR_MON_LURKER), "Lurker") == 0);
    assert(strstr(fr_actor_flavor(FR_MON_LURKER), "Waited all day") != NULL);
    assert(strcmp(fr_actor_name(FR_MON_YONDER_WARDEN), "Yonder Warden") == 0);
    assert(fr_monster_def(FR_MON_YONDER_WARDEN)->hp == 199);

    FrGame game;
    fr_game_init_class(&game, 99u, FR_CLASS_WARRIOR);
    fr_generate_floor(&game, FR_MAX_FLOORS);
    bool found_orb = false;
    for(uint8_t i = 0; i < FR_MAX_ITEMS; i++) {
        if(game.items[i].active && game.items[i].type == FR_ITEM_ORB) found_orb = true;
    }
    assert(found_orb);
}

static void test_v11_mimic_chest_reveals_and_attacks_once(void) {
    FrGame game;
    make_empty_test_room(&game);
    game.player.hp = 12;
    assert(fr_place_chest(&game, 6, 5, true));
    FrItem* chest = fr_chest_at(&game, 6, 5);
    assert(chest != NULL);

    assert(fr_chest_choice_count(&game, chest) == 0);
    assert(fr_bump_chest(&game, chest).kind == FR_ACTION_ATTACK);
    assert(!chest->active);
    assert(game.player.inv_count == 0);
    FrActor* mimic = fr_actor_at(&game, 6, 5);
    assert(mimic != NULL);
    assert(mimic->type == FR_MON_MIMIC);
    assert(game.player.hp < 12);
    assert(strstr(game.log, "Mimic") != NULL);
}

static void test_v11_lurker_decorator_spawns_rarely(void) {
    bool saw_lurker = false;
    for(uint32_t seed = 1u; seed <= 80u; seed++) {
        FrGame game;
        fr_game_init_class(&game, seed, FR_CLASS_WARRIOR);
        uint8_t run_lurkers = 0;
        for(uint8_t floor = 1; floor <= FR_MAX_FLOORS; floor++) {
            fr_generate_floor(&game, floor);
            uint8_t floor_lurkers = count_actors_of_type(&game, FR_MON_LURKER);
            run_lurkers = (uint8_t)(run_lurkers + floor_lurkers);
            assert(floor_lurkers <= 2);
            for(uint8_t i = 0; i < FR_MAX_ACTORS; i++) {
                FrActor* actor = &game.actors[i];
                if(!actor->active || actor->type != FR_MON_LURKER) continue;
                uint8_t terrain = fr_get_terrain(&game, actor->x, actor->y);
                assert(terrain == FR_TERR_GRASS || terrain == FR_TERR_PUDDLE);
                assert((actor->flags & FR_ACTOR_HIDDEN) != 0);
            }
        }
        if(run_lurkers > 0) saw_lurker = true;
        assert(run_lurkers <= 6);
    }
    assert(saw_lurker);
}

static void test_v11_lurker_hidden_in_grass_and_revealed_by_search(void) {
    FrGame game;
    make_empty_test_room(&game);
    game.player.dex = 9;
    fr_set_terrain(&game, 6, 5, FR_TERR_GRASS);
    FrActor* lurker = fr_spawn_actor(&game, FR_MON_LURKER, 6, 5);
    assert(lurker != NULL);
    assert((lurker->flags & FR_ACTOR_HIDDEN) != 0);
    assert(!fr_actor_visible_to_player(&game, lurker));
    assert(fr_search_nearby(&game));
    assert(fr_actor_visible_to_player(&game, lurker));
}

static void test_v11_eels_move_only_in_water(void) {
    FrGame game;
    make_empty_test_room(&game);
    game.player.x = 4;
    game.player.y = 5;
    fr_set_terrain(&game, 6, 5, FR_TERR_WATER);
    fr_set_terrain(&game, 7, 5, FR_TERR_WATER);
    fr_set_terrain(&game, 7, 6, FR_TERR_PUDDLE);
    fr_set_terrain(&game, 8, 5, FR_TERR_FLOOR);
    FrActor* eel = fr_spawn_actor(&game, FR_MON_EEL, 6, 5);
    assert(eel != NULL);

    assert(fr_try_move_actor_current(&game, eel, 1, 0));
    assert(eel->x == 7 && eel->y == 5);
    assert(fr_try_move_actor_current(&game, eel, 0, 1));
    assert(eel->x == 7 && eel->y == 6);
    assert(!fr_try_move_actor_current(&game, eel, 1, -1));
    assert(eel->x == 7 && eel->y == 6);
    fr_set_terrain(&game, 7, 7, FR_TERR_DOOR_CLOSED);
    assert(!fr_try_move_actor_current(&game, eel, 0, 1));
    assert(fr_get_terrain(&game, 7, 7) == FR_TERR_DOOR_CLOSED);
}

static void test_v12_eels_attack_only_waterborne_player(void) {
    FrGame dry;
    make_empty_test_room(&dry);
    dry.player.x = 5;
    dry.player.y = 5;
    fr_set_terrain(&dry, 6, 5, FR_TERR_WATER);
    FrActor* eel = fr_spawn_actor(&dry, FR_MON_EEL, 6, 5);
    assert(eel != NULL);
    uint8_t hp_before = dry.player.hp;
    fr_actor_turns(&dry);
    assert(dry.player.hp == hp_before);
    assert(dry.mode == FR_MODE_PLAYING);

    FrGame wet;
    make_empty_test_room(&wet);
    wet.player.x = 5;
    wet.player.y = 5;
    fr_set_terrain(&wet, 5, 5, FR_TERR_PUDDLE);
    fr_set_terrain(&wet, 6, 5, FR_TERR_WATER);
    eel = fr_spawn_actor(&wet, FR_MON_EEL, 6, 5);
    assert(eel != NULL);
    hp_before = wet.player.hp;
    fr_actor_turns(&wet);
    assert(wet.player.hp < hp_before);
}

static void test_v11_land_monsters_path_around_deep_water(void) {
    FrGame game;
    make_empty_test_room(&game);
    for(uint8_t y = 4; y <= 6; y++) {
        for(uint8_t x = 4; x <= 8; x++) fr_set_terrain(&game, x, y, FR_TERR_WALL);
    }
    fr_set_terrain(&game, 5, 5, FR_TERR_FLOOR);
    fr_set_terrain(&game, 6, 5, FR_TERR_WATER);
    fr_set_terrain(&game, 7, 5, FR_TERR_FLOOR);
    fr_set_terrain(&game, 8, 5, FR_TERR_FLOOR);
    fr_set_terrain(&game, 5, 6, FR_TERR_FLOOR);
    fr_set_terrain(&game, 6, 6, FR_TERR_FLOOR);
    fr_set_terrain(&game, 7, 6, FR_TERR_FLOOR);
    fr_set_terrain(&game, 8, 6, FR_TERR_FLOOR);
    game.player.x = 8;
    game.player.y = 5;
    FrActor* rat = fr_spawn_actor(&game, FR_MON_RAT, 5, 5);
    assert(rat != NULL);
    rat->flags = FR_ACTOR_CHASES;
    rat->memory = 20;
    rat->target_x = game.player.x;
    rat->target_y = game.player.y;

    assert(fr_rest(&game).kind == FR_ACTION_REST);
    assert(rat->x == 5 && rat->y == 6);
}

static void test_v11_eel_packs_spawn_in_some_flooded_rooms(void) {
    bool saw_eel_pack = false;
    bool saw_water_without_eels = false;
    for(uint32_t seed = 1u; seed <= 240u; seed++) {
        FrGame game;
        fr_game_init_class(&game, seed, FR_CLASS_WARRIOR);
        fr_generate_floor(&game, 5);
        uint8_t deep_water = count_terrain(&game, FR_TERR_WATER);
        uint8_t eels = count_actors_of_type(&game, FR_MON_EEL);
        if(deep_water > 0 && eels == 0) saw_water_without_eels = true;
        if(eels > 0) {
            saw_eel_pack = true;
            assert(eels >= 3 && eels <= 4);
            assert(max_pack_size_of_type(&game, FR_MON_EEL) >= 3);
            for(uint8_t i = 0; i < FR_MAX_ACTORS; i++) {
                FrActor* actor = &game.actors[i];
                if(!actor->active || actor->type != FR_MON_EEL) continue;
                uint8_t terrain = fr_get_terrain(&game, actor->x, actor->y);
                assert(terrain == FR_TERR_WATER || terrain == FR_TERR_PUDDLE);
            }
        }
    }
    assert(saw_eel_pack);
    assert(saw_water_without_eels);
}

static void test_v08_monster_hit_effects(void) {
    FrGame rat_game;
    make_empty_test_room(&rat_game);
    rat_game.seed = 8u;
    FrActor* rat = fr_spawn_actor(&rat_game, FR_MON_RAT, 6, 5);
    assert(rat != NULL);
    rat->hp = rat->max_hp = 9;
    uint8_t rat_hp = rat->hp;
    assert(fr_move_player(&rat_game, 1, 0).kind == FR_ACTION_ATTACK);
    assert(rat->hp == rat_hp);
    assert(strstr(rat_game.log, "You miss Rat") != NULL);

    FrGame snake_game;
    make_empty_test_room(&snake_game);
    snake_game.seed = 26u;
    FrActor* snake = fr_spawn_actor(&snake_game, FR_MON_SNAKE, 6, 5);
    assert(snake != NULL);
    assert(fr_rest(&snake_game).kind == FR_ACTION_REST);
    assert((snake_game.player.effects & FR_FX_POISONED) != 0);

    FrGame bat_game;
    make_empty_test_room(&bat_game);
    bat_game.seed = 38u;
    FrActor* bat = fr_spawn_actor(&bat_game, FR_MON_BAT, 6, 5);
    assert(bat != NULL);
    assert(bat->max_hp >= 4);
    bat->hp = 2;
    assert(fr_rest(&bat_game).kind == FR_ACTION_REST);
    assert(bat->hp == 3);
    assert(strstr(bat_game.log, "sips") != NULL);
}

static void test_v08_wight_visibility_and_fear(void) {
    FrGame game;
    make_empty_test_room(&game);
    game.seed = 26u;
    FrActor* wight = fr_spawn_actor(&game, FR_MON_WIGHT, 6, 5);
    assert(wight != NULL);
    assert(!fr_actor_visible_to_player(&game, wight));
    assert(fr_rest(&game).kind == FR_ACTION_REST);
    assert(fr_actor_visible_to_player(&game, wight));
    assert((game.player.effects & FR_FX_AFRAID) != 0);

    FrGame scout;
    make_empty_test_room(&scout);
    scout.player.class_id = FR_CLASS_RANGER;
    scout.player.dex = 9;
    FrActor* hidden = fr_spawn_actor(&scout, FR_MON_WIGHT, 6, 5);
    assert(hidden != NULL);
    assert(!fr_actor_visible_to_player(&scout, hidden));
    assert(fr_search_nearby(&scout));
    assert(fr_actor_visible_to_player(&scout, hidden));
}

static void test_v071_chase_paths_to_remembered_corner(void) {
    FrGame game;
    make_empty_test_room(&game);
    for(uint8_t y = 0; y < FR_MAP_H; y++) {
        for(uint8_t x = 0; x < FR_MAP_W; x++) fr_set_terrain(&game, x, y, FR_TERR_WALL);
    }
    fr_set_terrain(&game, 8, 5, FR_TERR_FLOOR);
    fr_set_terrain(&game, 7, 5, FR_TERR_FLOOR);
    fr_set_terrain(&game, 7, 6, FR_TERR_FLOOR);
    fr_set_terrain(&game, 7, 7, FR_TERR_FLOOR);
    fr_set_terrain(&game, 8, 7, FR_TERR_FLOOR);
    game.player.x = 4;
    game.player.y = 4;
    FrActor* kobold = fr_spawn_actor(&game, FR_MON_KOBOLD, 8, 5);
    assert(kobold != NULL);
    kobold->target_x = 8;
    kobold->target_y = 7;
    kobold->memory = 20;

    assert(fr_rest(&game).kind == FR_ACTION_REST);
    assert(kobold->x == 7 && kobold->y == 5);
}

static void test_v071_far_warden_moves_every_turn(void) {
    FrGame game;
    make_empty_test_room(&game);
    for(uint8_t x = 4; x <= 20; x++) fr_set_terrain(&game, x, 5, FR_TERR_FLOOR);
    game.player.x = 5;
    game.player.y = 5;
    game.turn = 0;
    FrActor* warden = fr_spawn_actor(&game, FR_MON_YONDER_WARDEN, 20, 5);
    assert(warden != NULL);

    assert(fr_rest(&game).kind == FR_ACTION_REST);
    assert(warden->x == 19);
}

extern bool fr_warden_position(const FrGame* game, uint8_t* floor, uint8_t* x, uint8_t* y);

static void test_v071_warden_keeps_chasing_between_floors(void) {
    FrGame game;
    fr_game_init_class(&game, 909u, FR_CLASS_WARRIOR);
    fr_generate_floor(&game, FR_MAX_FLOORS);
    game.player.has_orb = 1;
    FrActor* warden = fr_spawn_actor(&game, FR_MON_YONDER_WARDEN, game.player.x, (uint8_t)(game.player.y + 2));
    assert(warden != NULL);

    uint8_t before_floor = 0;
    uint8_t before_x = 0;
    uint8_t before_y = 0;
    assert(fr_warden_position(&game, &before_floor, &before_x, &before_y));
    assert(before_floor == FR_MAX_FLOORS);

    uint8_t up_x = 0;
    uint8_t up_y = 0;
    assert(fr_find_first_tile(&game, FR_TERR_STAIRS_UP, &up_x, &up_y));
    game.player.x = up_x;
    game.player.y = up_y;
    assert(fr_ascend(&game).kind == FR_ACTION_DESCEND);
    assert(game.floor == FR_MAX_FLOORS - 1);
    for(uint8_t i = 0; i < FR_MAX_ACTORS; i++) {
        if(game.actors[i].active && game.actors[i].type != FR_MON_YONDER_WARDEN) game.actors[i].active = false;
    }
    for(uint8_t i = 0; i < 10; i++) assert(fr_rest(&game).kind == FR_ACTION_REST);

    uint8_t after_floor = 0;
    uint8_t after_x = 0;
    uint8_t after_y = 0;
    assert(fr_warden_position(&game, &after_floor, &after_x, &after_y));
    assert(after_floor != before_floor || after_x != before_x || after_y != before_y);
}

static void test_bump_attack_kills_rat(void) {
    FrGame game;
    make_empty_test_room(&game);

    FrActor* rat = fr_spawn_actor(&game, FR_MON_RAT, 6, 5);
    assert(rat != NULL);
    assert(rat->hp == 3);

    FrActionResult hit = fr_move_player(&game, 1, 0);
    assert(hit.kind == FR_ACTION_ATTACK);
    assert(game.player.x == 5);
    assert(game.player.y == 5);
    assert(rat->hp < 3);

    for(uint8_t i = 0; i < 4 && rat->active; i++) {
        fr_move_player(&game, 1, 0);
    }

    assert(!rat->active);
}

static void test_v09_pack_aggro_and_bat_blink_strike(void) {
    FrGame pack;
    make_empty_test_room(&pack);
    FrActor* first = fr_spawn_actor(&pack, FR_MON_KOBOLD, 6, 5);
    FrActor* second = fr_spawn_actor(&pack, FR_MON_KOBOLD, 8, 5);
    assert(first != NULL && second != NULL);
    first->pack_id = 7;
    second->pack_id = 7;
    first->flags = 0;
    second->flags = 0;
    first->memory = 0;
    second->memory = 0;
    assert(fr_move_player(&pack, 1, 0).kind == FR_ACTION_ATTACK);
    assert(first->memory > 0);
    assert(second->memory > 0);

    FrGame bat_game;
    make_empty_test_room(&bat_game);
    bat_game.player.hp = bat_game.player.max_hp;
    FrActor* bat = fr_spawn_actor(&bat_game, FR_MON_BAT, 7, 5);
    assert(bat != NULL);
    bat->flags |= FR_ACTOR_CHASES;
    bat->memory = 20;
    uint8_t hp_before = bat_game.player.hp;
    assert(fr_rest(&bat_game).kind == FR_ACTION_REST);
    uint8_t dx = (uint8_t)abs((int)bat->x - (int)bat_game.player.x);
    uint8_t dy = (uint8_t)abs((int)bat->y - (int)bat_game.player.y);
    assert(dx <= 1 && dy <= 1 && (dx + dy) > 0);
    assert(bat_game.player.hp < hp_before);
}

static void test_v09_skeleton_dodges_projectiles_but_not_burst(void) {
    FrGame game;
    make_empty_test_room(&game);
    game.player.class_id = FR_CLASS_RANGER;
    game.player.arrows = 2;
    game.seed = 1u;
    FrActor* archer = fr_spawn_actor(&game, FR_MON_ARCHER, 8, 5);
    assert(archer != NULL);
    uint8_t hp_before = archer->hp;
    assert(fr_try_direction(&game, 1, 0).kind == FR_ACTION_RANGED);
    assert(archer->active);
    assert(archer->hp == hp_before);

    assert(fr_add_inventory(&game, FR_ITEM_SCROLL, FR_SCROLL_FIRE, 1));
    uint8_t hp_after_dodge = archer->hp;
    assert(fr_use_inventory(&game, 0, FR_USE_READ, archer->x, archer->y).kind == FR_ACTION_USE);
    assert(!archer->active || archer->hp < hp_after_dodge || (archer->effects & FR_FX_BURNING) != 0);
}

static void test_v10_skeleton_archer_short_name_and_ranged_ai(void) {
    assert(strcmp(fr_actor_log_name(FR_MON_ARCHER), "S.Archer") == 0);
    assert(strcmp(fr_actor_name(FR_MON_ARCHER), "Skeleton Archer") == 0);

    FrGame shoot;
    make_empty_test_room(&shoot);
    shoot.player.hp = shoot.player.max_hp;
    FrActor* archer = fr_spawn_actor(&shoot, FR_MON_ARCHER, 8, 5);
    assert(archer != NULL);
    archer->flags = FR_ACTOR_CHASES;
    archer->memory = 20;
    assert(fr_rest(&shoot).kind == FR_ACTION_REST);
    assert(shoot.player.hp < shoot.player.max_hp);
    assert(shoot.last_event == FR_EVENT_MON_PROJECTILE);
    assert(shoot.event_x == archer->x && shoot.event_y == archer->y);
    assert(shoot.event_tx == shoot.player.x && shoot.event_ty == shoot.player.y);
    assert(strstr(shoot.log, "S.Archer") != NULL);
    assert(strstr(shoot.log, "Skeleton Archer") == NULL);

    FrGame kite;
    make_empty_test_room(&kite);
    kite.seed = 1u;
    kite.player.hp = kite.player.max_hp;
    archer = fr_spawn_actor(&kite, FR_MON_ARCHER, 6, 5);
    assert(archer != NULL);
    archer->flags = FR_ACTOR_CHASES;
    archer->memory = 20;
    assert(fr_rest(&kite).kind == FR_ACTION_REST);
    assert(kite.player.hp == kite.player.max_hp);
    assert((uint8_t)(abs((int)archer->x - (int)kite.player.x) + abs((int)archer->y - (int)kite.player.y)) >= 2);
}

static void test_v09_cube_blocks_throwing_and_fire_burst_hits_inside(void) {
    FrGame game;
    make_empty_test_room(&game);
    game.player.hp = game.player.max_hp;
    game.seed = 2388811721u;
    FrActor* cube = fr_spawn_actor(&game, FR_MON_CUBE, 6, 5);
    assert(cube != NULL);
    assert(fr_rest(&game).kind == FR_ACTION_REST);
    assert(!cube->active);
    assert(game.player.cube_hp == 20);

    assert(fr_add_inventory(&game, FR_ITEM_THROWABLE, FR_THROW_DART, 3));
    assert(fr_use_inventory(&game, 0, FR_USE_THROW, 7, 5).kind == FR_ACTION_BLOCKED);
    assert(game.player.inv[0].amount == 3);

    assert(fr_add_inventory(&game, FR_ITEM_SCROLL, FR_SCROLL_FIRE, 1));
    uint8_t cube_hp = game.player.cube_hp;
    uint8_t player_hp = game.player.hp;
    assert(fr_use_inventory(&game, 1, FR_USE_READ, game.player.x, game.player.y).kind == FR_ACTION_USE);
    assert(game.player.cube_hp < cube_hp);
    assert(game.player.hp < player_hp);

    uint8_t x = game.player.x;
    cube_hp = game.player.cube_hp;
    assert(fr_move_player(&game, 1, 0).kind == FR_ACTION_ATTACK);
    assert(game.player.x == x);
    assert(game.player.cube_hp < cube_hp);
}

static void test_v10_lone_wounded_monster_can_break_and_flee(void) {
    FrGame game;
    make_empty_test_room(&game);
    game.seed = 1975u;
    game.player.str = 0;
    FrActor* goblin = fr_spawn_actor(&game, FR_MON_GOBLIN, 6, 5);
    assert(goblin != NULL);
    goblin->hp = 4;
    goblin->max_hp = 10;
    goblin->pack_id = 0;

    assert(fr_move_player(&game, 1, 0).kind == FR_ACTION_ATTACK);
    assert(goblin->active);
    assert((goblin->effects & FR_FX_AFRAID) != 0);

    FrGame pack;
    make_empty_test_room(&pack);
    pack.seed = 1975u;
    pack.player.str = 0;
    FrActor* first = fr_spawn_actor(&pack, FR_MON_GOBLIN, 6, 5);
    FrActor* fallen = fr_spawn_actor(&pack, FR_MON_GOBLIN, 8, 5);
    assert(first != NULL && fallen != NULL);
    first->hp = 4;
    first->max_hp = 10;
    first->pack_id = 9;
    fallen->active = false;
    fallen->pack_id = 9;

    assert(fr_move_player(&pack, 1, 0).kind == FR_ACTION_ATTACK);
    assert((first->effects & FR_FX_AFRAID) != 0);
}

static void test_v10_slime_splits_after_surviving_player_hit(void) {
    FrGame game;
    make_empty_test_room(&game);
    game.player.str = 0;
    FrActor* slime = fr_spawn_actor(&game, FR_MON_SLIME, 6, 5);
    assert(slime != NULL);
    slime->hp = 12;
    slime->max_hp = 12;
    slime->pack_id = 0;

    assert(fr_move_player(&game, 1, 0).kind == FR_ACTION_ATTACK);
    assert(slime->active);
    assert(count_actors_of_type(&game, FR_MON_SLIME) == 2);
    uint8_t pack_id = slime->pack_id;
    assert(pack_id != 0);
    uint8_t matching_pack = 0;
    uint8_t matching_hp = 0;
    for(uint8_t i = 0; i < FR_MAX_ACTORS; i++) {
        FrActor* actor = &game.actors[i];
        if(!actor->active || actor->type != FR_MON_SLIME) continue;
        if(actor->pack_id == pack_id) matching_pack++;
        if(actor->hp == slime->hp) matching_hp++;
        assert(abs((int)actor->x - (int)game.player.x) <= 1);
        assert(abs((int)actor->y - (int)game.player.y) <= 1);
    }
    assert(matching_pack == 2);
    assert(matching_hp == 2);
}

static void test_v09_sleeping_monsters_wake_and_idle_monsters_wander(void) {
    FrGame sleeper;
    make_empty_test_room(&sleeper);
    FrActor* goblin = fr_spawn_actor(&sleeper, FR_MON_GOBLIN, 6, 5);
    assert(goblin != NULL);
    goblin->flags |= FR_ACTOR_ASLEEP;
    sleeper.player.hp = sleeper.player.max_hp;
    uint8_t hp_before = sleeper.player.hp;
    assert(fr_rest(&sleeper).kind == FR_ACTION_REST);
    assert((goblin->flags & FR_ACTOR_ASLEEP) == 0);
    assert(sleeper.player.hp == hp_before);

    FrGame wander;
    make_empty_test_room(&wander);
    wander.seed = 7u;
    FrActor* rat = fr_spawn_actor(&wander, FR_MON_RAT, 8, 5);
    assert(rat != NULL);
    rat->flags = 0;
    rat->memory = 0;
    uint8_t start_x = rat->x;
    uint8_t start_y = rat->y;
    hp_before = wander.player.hp;
    bool acted = false;
    for(uint8_t i = 0; i < 16 && rat->active; i++) {
        assert(fr_rest(&wander).kind == FR_ACTION_REST);
        if(rat->x != start_x || rat->y != start_y || wander.player.hp < hp_before) {
            acted = true;
            break;
        }
    }
    assert(acted);

    FrGame roamer;
    make_empty_test_room(&roamer);
    for(uint8_t y = 0; y < FR_MAP_H; y++) {
        for(uint8_t x = 0; x < FR_MAP_W; x++) fr_set_terrain(&roamer, x, y, FR_TERR_WALL);
    }
    for(uint8_t x = 2; x <= 30; x++) fr_set_terrain(&roamer, x, 5, FR_TERR_FLOOR);
    roamer.player.x = 2;
    roamer.player.y = 5;
    roamer.player.hp = roamer.player.max_hp;
    roamer.seed = 17u;
    FrActor* roam_rat = fr_spawn_actor(&roamer, FR_MON_RAT, 25, 5);
    assert(roam_rat != NULL);
    roam_rat->flags = FR_ACTOR_ROAMS;
    roam_rat->memory = 0;
    uint8_t roam_x = roam_rat->x;
    assert(fr_rest(&roamer).kind == FR_ACTION_REST);
    assert(roam_rat->target_x != roam_x || roam_rat->x != roam_x);

    FrGame generated;
    bool saw_sleeping = false;
    for(uint32_t seed = 1u; seed < 12u; seed++) {
        fr_game_init_class(&generated, seed, FR_CLASS_WARRIOR);
        for(uint8_t i = 0; i < FR_MAX_ACTORS; i++) {
            FrActor* actor = &generated.actors[i];
            if(!actor->active || (actor->flags & FR_ACTOR_ASLEEP) == 0) continue;
            saw_sleeping = true;
            assert(actor->type == FR_MON_RAT || actor->type == FR_MON_BAT || actor->type == FR_MON_SNAKE);
        }
    }
    assert(saw_sleeping);
}

static void test_v091_bat_blink_can_land_behind_player(void) {
    FrGame game;
    make_empty_test_room(&game);
    for(uint8_t y = 0; y < FR_MAP_H; y++) {
        for(uint8_t x = 0; x < FR_MAP_W; x++) fr_set_terrain(&game, x, y, FR_TERR_WALL);
    }
    fr_set_terrain(&game, 5, 5, FR_TERR_FLOOR);
    fr_set_terrain(&game, 6, 5, FR_TERR_FLOOR);
    fr_set_terrain(&game, 8, 5, FR_TERR_FLOOR);
    game.player.x = 6;
    game.player.y = 5;
    game.player.hp = game.player.max_hp;
    game.seed = 1u;
    FrActor* bat = fr_spawn_actor(&game, FR_MON_BAT, 8, 5);
    assert(bat != NULL);
    bat->flags = FR_ACTOR_CHASES;
    assert(fr_rest(&game).kind == FR_ACTION_REST);
    assert(bat->x == 5 && bat->y == 5);
    assert(strstr(game.log, "Bat blinks") != NULL);
    assert(game.player.hp < game.player.max_hp);
}

static void test_v091_bat_cannot_attack_diagonal_without_blink(void) {
    FrGame game;
    make_empty_test_room(&game);
    for(uint8_t y = 0; y < FR_MAP_H; y++) {
        for(uint8_t x = 0; x < FR_MAP_W; x++) fr_set_terrain(&game, x, y, FR_TERR_WALL);
    }
    fr_set_terrain(&game, 5, 5, FR_TERR_FLOOR);
    fr_set_terrain(&game, 6, 6, FR_TERR_FLOOR);
    game.player.x = 5;
    game.player.y = 5;
    game.player.hp = game.player.max_hp;
    FrActor* bat = fr_spawn_actor(&game, FR_MON_BAT, 6, 6);
    assert(bat != NULL);
    bat->flags = FR_ACTOR_CHASES;
    assert(fr_rest(&game).kind == FR_ACTION_REST);
    assert(game.player.hp == game.player.max_hp);
    assert(strstr(game.log, "Bat hits you") == NULL);
}

static void test_v091_attacked_bat_keeps_pursuing(void) {
    FrGame game;
    make_empty_test_room(&game);
    game.player.class_id = FR_CLASS_RANGER;
    game.player.dex = 0;
    game.player.arrows = 4;
    game.seed = 99u;
    FrActor* bat = fr_spawn_actor(&game, FR_MON_BAT, 8, 5);
    assert(bat != NULL);
    bat->flags = FR_ACTOR_ASLEEP;
    bat->memory = 0;

    assert(fr_try_direction(&game, 1, 0).kind == FR_ACTION_RANGED);
    assert((bat->flags & FR_ACTOR_ASLEEP) == 0);
    assert((bat->flags & FR_ACTOR_CHASES) != 0);
    assert(bat->memory >= 18);

    game.player.x = 5;
    game.player.y = 6;
    for(uint8_t i = 0; i < 3 && bat->active; i++) assert(fr_rest(&game).kind == FR_ACTION_REST);
    assert(!bat->active || (bat->flags & FR_ACTOR_ASLEEP) == 0);
    assert(!bat->active || (bat->memory > 0 || ((bat->flags & FR_ACTOR_CHASES) != 0)));
}

static void test_v101_early_bats_return_without_pack_spike(void) {
    bool saw_early_bat = false;
    for(uint32_t seed = 1u; seed < 80u; seed++) {
        FrGame game;
        fr_game_init_class(&game, seed, FR_CLASS_WARRIOR);
        if(count_actors_of_type(&game, FR_MON_BAT) > 0) saw_early_bat = true;
        assert(max_pack_size_of_type(&game, FR_MON_BAT) <= 1);
        fr_generate_floor(&game, 6);
        if(count_actors_of_type(&game, FR_MON_BAT) > 0) saw_early_bat = true;
        assert(max_pack_size_of_type(&game, FR_MON_BAT) <= 1);
    }
    assert(saw_early_bat);

    bool saw_mid_bat_pack = false;
    for(uint32_t seed = 1u; seed < 120u && !saw_mid_bat_pack; seed++) {
        FrGame game;
        fr_game_init_class(&game, seed, FR_CLASS_WARRIOR);
        fr_generate_floor(&game, 7);
        saw_mid_bat_pack = max_pack_size_of_type(&game, FR_MON_BAT) >= 2;
    }
    assert(saw_mid_bat_pack);
}

static void test_v102_dragonlings_are_rare_deep_packs(void) {
    bool saw_deep_dragon = false;
    bool saw_deep_dragon_pack = false;
    for(uint32_t seed = 1u; seed < 240u; seed++) {
        FrGame game;
        fr_game_init_class(&game, seed, FR_CLASS_WARRIOR);
        for(uint8_t floor = 1; floor <= 15; floor++) {
            fr_generate_floor(&game, floor);
            assert(count_actors_of_type(&game, FR_MON_DRAGON) == 0);
        }
        for(uint8_t floor = 16; floor <= 18; floor++) {
            fr_generate_floor(&game, floor);
            uint8_t dragons = count_actors_of_type(&game, FR_MON_DRAGON);
            uint8_t pack = max_pack_size_of_type(&game, FR_MON_DRAGON);
            assert(pack <= 4);
            if(dragons > 0) saw_deep_dragon = true;
            if(pack >= 2) saw_deep_dragon_pack = true;
        }
    }
    assert(saw_deep_dragon);
    assert(saw_deep_dragon_pack);
}
