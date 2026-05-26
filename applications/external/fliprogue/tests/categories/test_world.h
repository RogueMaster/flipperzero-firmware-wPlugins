#pragma once

#include "monster_ai.h"
#include "chest_actions.h"
#include "floor_state.h"
#include "game_core.h"
#include "grate_actions.h"
#include "hazards.h"
#include "ice_effects.h"
#include "look_text.h"
#include "placement.h"
#include "pickup_actions.h"
#include "room_templates.h"
#include "shrine_actions.h"
#include "special_floors.h"
#include "terrain_effects.h"
#include "turns.h"

static void test_terrain_traps_and_puddle(void) {
    FrGame ranger;
    make_empty_test_room(&ranger);
    ranger.player.class_id = FR_CLASS_RANGER;
    fr_set_terrain(&ranger, 6, 5, FR_TERR_GRASS);
    assert(fr_try_direction(&ranger, 1, 0).kind == FR_ACTION_MOVE);
    assert(fr_get_terrain(&ranger, 6, 5) == FR_TERR_GRASS_TRAMPLED);
    fr_update_fov(&ranger);
    assert((ranger.tiles[5][8] & FR_TILE_VISIBLE) != 0);

    ranger.player.effects = FR_FX_BURNING;
    ranger.player.fx_timer[FR_FX_BURNING_INDEX] = 8;
    fr_set_terrain(&ranger, 7, 5, FR_TERR_PUDDLE);
    assert(fr_try_direction(&ranger, 1, 0).kind == FR_ACTION_MOVE);
    assert((ranger.player.effects & FR_FX_BURNING) == 0);
    fr_set_terrain(&ranger, 8, 5, FR_TERR_SAND);
    assert(fr_try_direction(&ranger, 1, 0).kind == FR_ACTION_MOVE);
    assert(strstr(ranger.log, "Sand whispers") != NULL);

    FrGame warrior;
    make_empty_test_room(&warrior);
    warrior.player.class_id = FR_CLASS_WARRIOR;
    assert(fr_place_trap(&warrior, 6, 5, FR_TRAP_SNARE));
    assert(fr_trap_at(&warrior, 6, 5) != NULL);
    assert(!fr_player_detects_trap(&warrior, 6, 5));
    assert(fr_try_direction(&warrior, 1, 0).kind == FR_ACTION_MOVE);
    assert((warrior.player.effects & FR_FX_STUNNED) != 0);
    assert(warrior.player.fx_timer[FR_FX_STUNNED_INDEX] == 3);
    assert(strstr(warrior.log, "Snare locks") != NULL);

    FrGame scout;
    make_empty_test_room(&scout);
    scout.player.class_id = FR_CLASS_RANGER;
    assert(fr_place_trap(&scout, 6, 5, FR_TRAP_SNARE));
    assert(fr_player_detects_trap(&scout, 6, 5));
}

static void test_v04_layout_regressions(void) {
    assert(FR_INV_CAP == 20);

    FrGame game;
    fr_game_init_class(&game, 99u, FR_CLASS_WARRIOR);
    assert(fr_get_terrain(&game, 16, 10) != FR_TERR_DOOR_CLOSED);
    assert(count_terrain(&game, FR_TERR_STAIRS_UP) == 1);
    assert(count_terrain(&game, FR_TERR_STAIRS_DOWN) == 1);
    assert(fr_count_active_items(&game) <= 12);

    FrGame seed_a;
    FrGame seed_b;
    fr_game_init_class(&seed_a, 101u, FR_CLASS_WARRIOR);
    fr_game_init_class(&seed_b, 202u, FR_CLASS_WARRIOR);
    assert(terrain_maps_differ(&seed_a, &seed_b));
    assert_doors_are_chokepoints(&seed_a);
    assert_doors_are_chokepoints(&seed_b);

    for(uint32_t seed = 1u; seed < 25u; seed++) {
        fr_game_init_class(&game, seed, FR_CLASS_WARRIOR);
        for(uint8_t floor = 1; floor <= 6; floor++) {
            fr_generate_floor(&game, floor);
            assert_doors_are_chokepoints(&game);
        }
    }
}

static void test_v11_shrine_placement_and_interaction_log(void) {
    FrGame game;
    make_empty_test_room(&game);
    assert(fr_place_shrine(&game, 6, 5));
    assert(fr_get_terrain(&game, 6, 5) == FR_TERR_SHRINE);
    assert(fr_tile_glyph(&game, 6, 5) == 'T');
    assert(!fr_is_walkable(FR_TERR_SHRINE));
    assert(fr_move_player(&game, 1, 0).kind == FR_ACTION_USE);
    assert(game.player.x == 5 && game.player.y == 5);
    assert(strstr(game.log, "old god") != NULL);
}

static void test_v11_grates_block_and_open_from_same_floor_key_or_button(void) {
    FrGame game;
    make_empty_test_room(&game);
    assert(fr_place_grate(&game, 6, 5));
    assert(fr_get_terrain(&game, 6, 5) == FR_TERR_GRATE);
    assert(fr_tile_glyph(&game, 6, 5) == '#');
    assert(fr_move_player(&game, 1, 0).kind == FR_ACTION_BLOCKED);

    FrActor* rat = fr_spawn_actor(&game, FR_MON_RAT, 7, 5);
    assert(rat != NULL);
    assert(!fr_try_move_actor_current(&game, rat, -1, 0));

    assert(fr_place_key(&game, 5, 5));
    fr_pickup_at_player(&game);
    assert(fr_get_terrain(&game, 6, 5) == FR_TERR_FLOOR);
    assert(strstr(game.log, "Grate opens") != NULL);

    make_empty_test_room(&game);
    assert(fr_place_grate(&game, 6, 5));
    assert(fr_place_button(&game, 5, 5));
    assert(fr_rest(&game).kind == FR_ACTION_USE);
    assert(fr_get_terrain(&game, 6, 5) == FR_TERR_FLOOR);
}

static void test_v12_gold_pickup_log_is_compact(void) {
    FrGame game;
    make_empty_test_room(&game);
    game.items[0] = (FrItem){true, FR_ITEM_GOLD, 0, game.player.x, game.player.y, 5, 0};

    fr_pickup_at_player(&game);

    assert(game.player.gold == 5);
    assert(strcmp(game.log, "Picked +5g.") == 0);
}

static void test_v12_chests_block_movement_pathing_and_fire(void) {
    FrGame game;
    make_empty_test_room(&game);
    assert(fr_place_chest(&game, 6, 5, false));
    assert(fr_blocking_item_at(&game, 6, 5));
    assert(fr_adjacent_chest_for_bump(&game, 6, 5) != NULL);
    assert(fr_move_player(&game, 1, 0).kind == FR_ACTION_BLOCKED);
    assert(game.player.x == 5 && game.player.y == 5);

    fr_set_terrain(&game, 5, 4, FR_TERR_WALL);
    fr_set_terrain(&game, 6, 4, FR_TERR_WALL);
    fr_set_terrain(&game, 7, 4, FR_TERR_WALL);
    fr_set_terrain(&game, 5, 6, FR_TERR_WALL);
    fr_set_terrain(&game, 6, 6, FR_TERR_WALL);
    fr_set_terrain(&game, 7, 6, FR_TERR_WALL);
    FrActor* rat = fr_spawn_actor(&game, FR_MON_RAT, 7, 5);
    assert(rat != NULL);
    assert(!fr_try_move_actor_current(&game, rat, -1, 0));
    assert(!fr_path_exists(&game, 5, 5, 7, 5));

    fr_fire_burst(&game, 6, 5);
    assert(!fr_terrain_fire_at(&game, game.floor, 6, 5));
    assert(fr_chest_at(&game, 6, 5) != NULL);
}

static void test_v11_opened_grate_persists_and_reward_pocket_is_visible(void) {
    FrGame game;
    fr_game_init_class(&game, 404u, FR_CLASS_WARRIOR);
    fr_generate_floor(&game, 5);
    uint8_t gx = (uint8_t)(game.player.x + 1);
    uint8_t px = (uint8_t)(game.player.x + 2);
    uint8_t py = game.player.y;
    fr_set_terrain(&game, game.player.x, game.player.y, FR_TERR_FLOOR);
    fr_set_terrain(&game, gx, py, FR_TERR_GRATE);
    fr_set_terrain(&game, px, py, FR_TERR_FLOOR);
    fr_update_fov(&game);
    assert((game.tiles[py][px] & FR_TILE_VISIBLE) != 0);
    assert(fr_place_key(&game, game.player.x, game.player.y));
    fr_pickup_at_player(&game);
    assert(fr_get_terrain(&game, gx, py) == FR_TERR_FLOOR);
    fr_save_current_floor(&game);
    fr_enter_floor(&game, 6, FR_TERR_STAIRS_UP);
    fr_enter_floor(&game, 5, FR_TERR_STAIRS_DOWN);
    assert(fr_get_terrain(&game, gx, py) == FR_TERR_FLOOR);
}

static void test_v11_generated_grate_rewards_have_same_floor_unlocks(void) {
    bool saw_grate_reward = false;
    for(uint32_t seed = 1u; seed <= 120u; seed++) {
        FrGame game;
        fr_game_init_class(&game, seed, FR_CLASS_WARRIOR);
        for(uint8_t floor = 5; floor <= FR_MAX_FLOORS; floor++) {
            fr_generate_floor(&game, floor);
            uint8_t grates = count_terrain(&game, FR_TERR_GRATE);
            if(grates == 0) continue;
            saw_grate_reward = true;
            bool has_key = count_items_of_type(&game, FR_ITEM_KEY) > 0;
            bool has_button = count_terrain(&game, FR_TERR_BUTTON) > 0;
            bool has_chest = count_items_of_type(&game, FR_ITEM_CHEST) > 0;
            assert(has_key || has_button);
            assert(has_chest);
            if(has_key) {
                for(uint8_t i = 0; i < FR_MAX_ITEMS; i++) {
                    if(game.items[i].active && game.items[i].type == FR_ITEM_KEY) {
                        assert(fr_path_exists(&game, game.player.x, game.player.y, game.items[i].x, game.items[i].y));
                        break;
                    }
                }
            } else {
                uint8_t bx = 0;
                uint8_t by = 0;
                assert(fr_find_first_tile(&game, FR_TERR_BUTTON, &bx, &by));
                assert(fr_path_exists(&game, game.player.x, game.player.y, bx, by));
            }
        }
    }
    assert(saw_grate_reward);
}

static void test_every_floor_has_expected_stairs_and_supplies(void) {
    static const uint32_t seeds[] = {11u, 99u, 1234u, 2026u};
    for(uint8_t seed_i = 0; seed_i < sizeof(seeds) / sizeof(seeds[0]); seed_i++) {
        FrGame game;
        fr_game_init_class(&game, seeds[seed_i], FR_CLASS_WARRIOR);
        for(uint8_t floor = 1; floor <= FR_MAX_FLOORS; floor++) {
            fr_generate_floor(&game, floor);
            assert(game.floor == floor);
            assert(count_terrain(&game, FR_TERR_STAIRS_UP) == 1);
            if(floor < FR_MAX_FLOORS) {
                assert(count_terrain(&game, FR_TERR_STAIRS_DOWN) == 1);
            } else {
                assert(count_terrain(&game, FR_TERR_STAIRS_DOWN) == 0);
                assert(count_items_of_type(&game, FR_ITEM_ORB) == 1);
            }
            assert(fr_count_active_actors(&game) >= 5);
        }
    }
}

static void test_v12_arrows_spawn_only_for_ranger(void) {
    static const uint32_t seeds[] = {11u, 99u, 1234u, 2026u};
    bool ranger_saw_arrows = false;
    for(uint8_t seed_i = 0; seed_i < sizeof(seeds) / sizeof(seeds[0]); seed_i++) {
        FrGame warrior;
        FrGame mage;
        FrGame ranger;
        fr_game_init_class(&warrior, seeds[seed_i], FR_CLASS_WARRIOR);
        fr_game_init_class(&mage, seeds[seed_i], FR_CLASS_MAGE);
        fr_game_init_class(&ranger, seeds[seed_i], FR_CLASS_RANGER);
        for(uint8_t floor = 1; floor <= 8; floor++) {
            fr_generate_floor(&warrior, floor);
            fr_generate_floor(&mage, floor);
            fr_generate_floor(&ranger, floor);
            assert(count_items_of_type(&warrior, FR_ITEM_ARROWS) == 0);
            assert(count_items_of_type(&mage, FR_ITEM_ARROWS) == 0);
            if(count_items_of_type(&ranger, FR_ITEM_ARROWS) > 0) ranger_saw_arrows = true;
        }
    }
    assert(ranger_saw_arrows);
}

static void test_v11_room_templates_keep_floor_contracts(void) {
    uint8_t template_mask = 0;
    for(uint32_t seed = 1u; seed <= 64u; seed++) {
        FrGame game;
        fr_game_init_class(&game, seed, FR_CLASS_WARRIOR);
        for(uint8_t floor = 1; floor <= FR_MAX_FLOORS; floor++) {
            uint8_t template_id = fr_room_template_id(seed, floor);
            assert(template_id < FR_ROOM_TEMPLATE_COUNT);
            template_mask |= (uint8_t)(1u << template_id);

            fr_generate_floor(&game, floor);
            assert(count_terrain(&game, FR_TERR_STAIRS_UP) == 1);
            uint8_t up_x = 0;
            uint8_t up_y = 0;
            uint8_t down_x = 0;
            uint8_t down_y = 0;
            assert(fr_find_first_tile(&game, FR_TERR_STAIRS_UP, &up_x, &up_y));
            if(floor < FR_MAX_FLOORS) {
                assert(count_terrain(&game, FR_TERR_STAIRS_DOWN) == 1);
                assert(fr_find_first_tile(&game, FR_TERR_STAIRS_DOWN, &down_x, &down_y));
                assert(fr_path_exists(&game, up_x, up_y, down_x, down_y));
            } else {
                assert(count_terrain(&game, FR_TERR_STAIRS_DOWN) == 0);
                assert(count_items_of_type(&game, FR_ITEM_ORB) == 1);
            }
        }
    }
    assert(template_mask == (uint8_t)((1u << FR_ROOM_TEMPLATE_COUNT) - 1u));
}

static void test_v11_special_floors_and_maze_template_keep_floor_contracts(void) {
    uint8_t special_mask = 0;
    bool saw_maze_template = false;
    for(uint32_t seed = 1u; seed <= 160u; seed++) {
        FrGame game;
        fr_game_init_class(&game, seed, FR_CLASS_WARRIOR);
        for(uint8_t floor = 1; floor <= FR_MAX_FLOORS; floor++) {
            uint8_t special = fr_special_floor_type(seed, floor);
            assert(special < FR_SPECIAL_FLOOR_MAX);
            special_mask |= (uint8_t)(1u << special);
            if(fr_room_template_id(seed, floor) == FR_ROOM_TEMPLATE_MAZE) saw_maze_template = true;

            fr_generate_floor(&game, floor);
            uint8_t up_x = 0;
            uint8_t up_y = 0;
            assert(fr_find_first_tile(&game, FR_TERR_STAIRS_UP, &up_x, &up_y));
            if(floor < FR_MAX_FLOORS) {
                uint8_t down_x = 0;
                uint8_t down_y = 0;
                assert(fr_find_first_tile(&game, FR_TERR_STAIRS_DOWN, &down_x, &down_y));
                assert(fr_path_exists(&game, up_x, up_y, down_x, down_y));
            }
            for(uint8_t i = 0; i < FR_MAX_TRAPS; i++) {
                FrTrap* trap = &game.traps[i];
                if(!trap->active || (trap->type != FR_TRAP_ARROW && trap->type != FR_TRAP_FIRE)) continue;
                assert(trap->source_x != trap->x || trap->source_y != trap->y);
                assert(trap->dir_x != 0 || trap->dir_y != 0);
            }
        }
    }
    assert((special_mask & (1u << FR_SPECIAL_FLOOR_FLOODED)) != 0);
    assert((special_mask & (1u << FR_SPECIAL_FLOOR_TRAPWORKS)) != 0);
    assert((special_mask & (1u << FR_SPECIAL_FLOOR_SHRINE)) != 0);
    assert((special_mask & (1u << FR_SPECIAL_FLOOR_NEST)) != 0);
    assert((special_mask & (1u << FR_SPECIAL_FLOOR_MAZE)) != 0);
    assert(saw_maze_template);
}

static void test_v11_room_decorators_keep_objects_on_valid_tiles(void) {
    for(uint32_t seed = 1u; seed <= 40u; seed++) {
        FrGame game;
        fr_game_init_class(&game, seed, FR_CLASS_WARRIOR);
        for(uint8_t floor = 1; floor <= FR_MAX_FLOORS; floor++) {
            fr_generate_floor(&game, floor);
            for(uint8_t i = 0; i < FR_MAX_ITEMS; i++) {
                const FrItem* item = &game.items[i];
                if(!item->active) continue;
                uint8_t terrain = fr_get_terrain(&game, item->x, item->y);
                assert(terrain_is_walkable_for_test(terrain));
                assert(terrain != FR_TERR_STAIRS_UP && terrain != FR_TERR_STAIRS_DOWN);
                assert(terrain != FR_TERR_DOOR_CLOSED && terrain != FR_TERR_DOOR_OPEN);
                assert(!(item->x == game.player.x && item->y == game.player.y));
            }
            for(uint8_t i = 0; i < FR_MAX_TRAPS; i++) {
                const FrTrap* trap = &game.traps[i];
                if(!trap->active) continue;
                uint8_t terrain = fr_get_terrain(&game, trap->x, trap->y);
                assert(terrain_is_walkable_for_test(terrain));
                assert(terrain != FR_TERR_STAIRS_UP && terrain != FR_TERR_STAIRS_DOWN);
                assert(terrain != FR_TERR_DOOR_CLOSED && terrain != FR_TERR_DOOR_OPEN);
                assert(!(trap->x == game.player.x && trap->y == game.player.y));
                assert(fr_actor_at(&game, trap->x, trap->y) == NULL);
            }
            for(uint8_t i = 0; i < FR_MAX_ACTORS; i++) {
                const FrActor* actor = &game.actors[i];
                if(!actor->active) continue;
                uint8_t terrain = fr_get_terrain(&game, actor->x, actor->y);
                assert(terrain_is_walkable_for_test(terrain));
                assert(terrain != FR_TERR_STAIRS_UP && terrain != FR_TERR_STAIRS_DOWN);
                assert(!(actor->x == game.player.x && actor->y == game.player.y));
            }
        }
    }
}

static void test_v07_food_spawn_scales_by_depth(void) {
    FrGame game;
    fr_game_init_class(&game, 2027u, FR_CLASS_WARRIOR);

    fr_generate_floor(&game, 1);
    assert(count_items_of_type(&game, FR_ITEM_FOOD) >= 1);
    fr_generate_floor(&game, 6);
    assert(count_items_of_type(&game, FR_ITEM_FOOD) >= 1);

    uint8_t middle_with_food = 0;
    uint8_t deep_with_food = 0;
    for(uint32_t seed = 10u; seed < 40u; seed++) {
        fr_game_init_class(&game, seed, FR_CLASS_WARRIOR);
        fr_generate_floor(&game, 7);
        if(count_items_of_type(&game, FR_ITEM_FOOD) > 0) middle_with_food++;
        fr_generate_floor(&game, 13);
        if(count_items_of_type(&game, FR_ITEM_FOOD) > 0) deep_with_food++;
    }
    assert(middle_with_food > 10 && middle_with_food < 28);
    assert(deep_with_food > 3 && deep_with_food < middle_with_food);
}

static void test_stairs_orb_and_victory_summary(void) {
    FrGame game;
    fr_game_init_class(&game, 99u, FR_CLASS_WARRIOR);
    uint8_t up_x = 0;
    uint8_t up_y = 0;
    assert(fr_find_first_tile(&game, FR_TERR_STAIRS_UP, &up_x, &up_y));
    game.player.x = up_x;
    game.player.y = up_y;
    assert(fr_ascend(&game).kind == FR_ACTION_BLOCKED);
    assert(game.mode == FR_MODE_PLAYING);

    uint8_t down_x = 0;
    uint8_t down_y = 0;
    assert(fr_find_first_tile(&game, FR_TERR_STAIRS_DOWN, &down_x, &down_y));
    game.player.x = down_x;
    game.player.y = down_y;
    assert(fr_descend(&game).kind == FR_ACTION_DESCEND);
    assert(game.floor == 2);

    assert(fr_find_first_tile(&game, FR_TERR_STAIRS_UP, &up_x, &up_y));
    game.player.x = up_x;
    game.player.y = up_y;
    assert(fr_ascend(&game).kind == FR_ACTION_DESCEND);
    assert(game.floor == 1);

    fr_generate_floor(&game, FR_MAX_FLOORS);
    bool found_orb = false;
    for(uint8_t i = 0; i < FR_MAX_ITEMS; i++) {
        if(game.items[i].active && game.items[i].type == FR_ITEM_ORB) found_orb = true;
    }
    assert(found_orb);

    game.floor = 1;
    game.player.has_orb = 1;
    assert(fr_find_first_tile(&game, FR_TERR_STAIRS_UP, &up_x, &up_y));
    game.player.x = up_x;
    game.player.y = up_y;
    assert(fr_ascend(&game).kind == FR_ACTION_DESCEND);
    assert(game.mode == FR_MODE_VICTORY);
    assert(strstr(fr_run_summary(&game), "Victory") != NULL);
}

static void test_v071_floor_state_persists_when_returning(void) {
    FrGame game;
    fr_game_init_class(&game, 505u, FR_CLASS_WARRIOR);

    uint8_t mark_x = 0;
    uint8_t mark_y = 0;
    assert(fr_find_first_tile(&game, FR_TERR_STAIRS_UP, &mark_x, &mark_y));
    mark_x = (uint8_t)(mark_x + 1);
    fr_set_terrain(&game, mark_x, mark_y, FR_TERR_PUDDLE);

    uint8_t down_x = 0;
    uint8_t down_y = 0;
    assert(fr_find_first_tile(&game, FR_TERR_STAIRS_DOWN, &down_x, &down_y));
    game.player.x = down_x;
    game.player.y = down_y;
    assert(fr_descend(&game).kind == FR_ACTION_DESCEND);
    assert(game.floor == 2);

    uint8_t up_x = 0;
    uint8_t up_y = 0;
    assert(fr_find_first_tile(&game, FR_TERR_STAIRS_UP, &up_x, &up_y));
    game.player.x = up_x;
    game.player.y = up_y;
    assert(fr_ascend(&game).kind == FR_ACTION_DESCEND);
    assert(game.floor == 1);
    assert(fr_get_terrain(&game, mark_x, mark_y) == FR_TERR_PUDDLE);
}

static void test_v12_tile_delta_budget_keeps_important_changes(void) {
    FrGame game;
    make_empty_test_room(&game);
    game.floor = 1;
    fr_init_floor_state(&game, 1, 5, 5, 10, 5);
    game.suppress_floor_state_save = 1;

    for(uint8_t i = 0; i < FR_MAX_TILE_DELTAS; i++) {
        uint8_t x = (uint8_t)(1 + (i % (FR_MAP_W - 2)));
        uint8_t y = (uint8_t)(1 + (i / (FR_MAP_W - 2)));
        fr_set_terrain(&game, x, y, (i & 1u) != 0 ? FR_TERR_PUDDLE : FR_TERR_GRASS_TRAMPLED);
    }

    FrFloorState* floor = &game.floors[0];
    assert(floor->tile_delta_count == FR_MAX_TILE_DELTAS);
    for(uint8_t i = 0; i < FR_MAX_TILE_DELTAS; i++) {
        uint8_t x = (uint8_t)(floor->tile_delta_pos[i] % FR_MAP_W);
        uint8_t y = (uint8_t)(floor->tile_delta_pos[i] / FR_MAP_W);
        assert(fr_get_terrain(&game, x, y) == floor->tile_delta_tile[i]);
    }
}

static void test_v12_path_queue_overflow_fails_safely(void) {
    FrGame game;
    make_empty_test_room(&game);
    for(uint8_t y = 0; y < FR_MAP_H; y++) {
        for(uint8_t x = 0; x < FR_MAP_W; x++) fr_set_terrain(&game, x, y, FR_TERR_FLOOR);
    }
    assert(!fr_path_exists(&game, 1, 1, (uint8_t)(FR_MAP_W - 2), (uint8_t)(FR_MAP_H - 2)));
}

static void test_v12_coarse_explored_does_not_restore_extra_walls(void) {
    FrGame game;
    make_empty_test_room(&game);
    game.floor = 1;
    fr_set_terrain(&game, 7, 4, FR_TERR_WALL);
    for(uint8_t y = 0; y < FR_MAP_H; y++) {
        for(uint8_t x = 0; x < FR_MAP_W; x++) {
            game.tiles[y][x] &= (uint8_t)~(FR_TILE_VISIBLE | FR_TILE_EXPLORED);
        }
    }
    game.tiles[4][6] |= FR_TILE_EXPLORED;
    fr_init_floor_state(&game, 1, 5, 5, 10, 5);

    game.tiles[4][6] &= (uint8_t)~FR_TILE_EXPLORED;
    game.tiles[4][7] &= (uint8_t)~FR_TILE_EXPLORED;
    fr_apply_saved_floor_state(&game, 1);

    assert((game.tiles[4][6] & FR_TILE_EXPLORED) != 0);
    assert((game.tiles[4][7] & FR_TILE_EXPLORED) == 0);
}

static void test_v072_compact_floor_state_restores_full_runtime_floor(void) {
    FrGame game;
    fr_game_init_class(&game, 606u, FR_CLASS_RANGER);
    game.player.dex = 6;

    uint8_t delta_x = 0;
    uint8_t delta_y = 0;
    assert(find_plain_floor(&game, &delta_x, &delta_y));
    fr_set_terrain(&game, delta_x, delta_y, FR_TERR_PUDDLE);
    game.tiles[delta_y][delta_x] |= FR_TILE_EXPLORED;

    uint8_t stand_x = 0;
    uint8_t stand_y = 0;
    uint8_t hidden_x = 0;
    uint8_t hidden_y = 0;
    assert(find_floor_next_to_wall(&game, &stand_x, &stand_y, &hidden_x, &hidden_y));
    game.player.x = stand_x;
    game.player.y = stand_y;
    game.tiles[hidden_y][hidden_x] |= FR_TILE_HIDDEN_DOOR;
    assert(fr_search_nearby(&game));
    assert(fr_get_terrain(&game, hidden_x, hidden_y) == FR_TERR_DOOR_CLOSED);

    uint8_t actor_y = delta_y > 2 ? (uint8_t)(delta_y - 1) : (uint8_t)(delta_y + 1);
    uint8_t trap_y = delta_y > 3 ? (uint8_t)(delta_y - 2) : (uint8_t)(delta_y + 2);
    FrActor* rat = fr_spawn_actor(&game, FR_MON_RAT, delta_x, actor_y);
    assert(rat != NULL);
    rat->hp = 1;
    rat->memory = 9;
    rat->target_x = delta_x;
    rat->target_y = delta_y;

    bool moved_item = false;
    for(uint8_t i = 0; i < FR_MAX_ITEMS; i++) {
        if(!game.items[i].active) continue;
        game.items[i].x = delta_x;
        game.items[i].y = delta_y;
        game.items[i].amount = 7;
        game.items[i].flags = 3;
        moved_item = true;
        break;
    }
    assert(moved_item);

    assert(fr_place_trap(&game, delta_x, trap_y, FR_TRAP_POISON));
    FrTrap* trap = fr_trap_at(&game, delta_x, trap_y);
    assert(trap != NULL);
    trap->hidden = 0;

    uint8_t down_x = 0;
    uint8_t down_y = 0;
    assert(fr_find_first_tile(&game, FR_TERR_STAIRS_DOWN, &down_x, &down_y));
    game.player.x = down_x;
    game.player.y = down_y;
    fr_update_fov(&game);

    FrGame expected = game;

    assert(fr_descend(&game).kind == FR_ACTION_DESCEND);
    assert(game.floor == 2);

    uint8_t up_x = 0;
    uint8_t up_y = 0;
    assert(fr_find_first_tile(&game, FR_TERR_STAIRS_UP, &up_x, &up_y));
    game.player.x = up_x;
    game.player.y = up_y;
    assert(fr_ascend(&game).kind == FR_ACTION_DESCEND);
    assert(game.floor == 1);

    assert_runtime_floor_equal(&expected, &game);
    assert((game.tiles[delta_y][delta_x] & FR_TILE_EXPLORED) != 0);
    assert((game.tiles[delta_y & (uint8_t)~1u][delta_x & (uint8_t)~1u] & FR_TILE_EXPLORED) != 0);
    for(uint8_t i = 0; i < FR_MAX_ITEMS; i++) {
        if(!expected.items[i].active) continue;
        assert(game.items[i].active);
        assert(game.items[i].type == expected.items[i].type);
        assert(game.items[i].subtype == expected.items[i].subtype);
        assert(game.items[i].x == expected.items[i].x);
        assert(game.items[i].y == expected.items[i].y);
        assert(game.items[i].amount == expected.items[i].amount);
        assert(game.items[i].flags == expected.items[i].flags);
    }
}

static void test_v07_dew_from_grass_heals_and_records_event(void) {
    FrGame game;
    make_empty_test_room(&game);
    game.seed = 7133u;
    game.player.hp = 10;
    fr_set_terrain(&game, 6, 5, FR_TERR_GRASS);

    assert(fr_move_player(&game, 1, 0).kind == FR_ACTION_MOVE);
    assert(fr_get_terrain(&game, 6, 5) == FR_TERR_GRASS_TRAMPLED);
    assert(game.player.hp == 11);
    assert(game.last_event == FR_EVENT_DEW);
    assert(game.event_x == 6 && game.event_y == 5);
    assert(strstr(game.log, "Dew") != NULL);
}

static void test_v07_trap_detection_scales_with_dex(void) {
    FrGame warrior;
    make_empty_test_room(&warrior);
    warrior.player.dex = 3;
    assert(fr_place_trap(&warrior, 6, 5, FR_TRAP_FIRE));
    assert(fr_hidden_trap_detection_chance(&warrior) == 35);
    assert(!fr_player_detects_trap(&warrior, 6, 5));
    assert(fr_search_nearby(&warrior));
    assert(fr_player_detects_trap(&warrior, 6, 5));
    assert(warrior.last_event == FR_EVENT_TRAP_SPOTTED);
    assert(warrior.event_x == 6 && warrior.event_y == 5);

    FrGame ranger;
    make_empty_test_room(&ranger);
    ranger.player.class_id = FR_CLASS_RANGER;
    ranger.player.dex = 6;
    assert(fr_place_trap(&ranger, 6, 5, FR_TRAP_FIRE));
    assert(fr_hidden_trap_detection_chance(&ranger) == 90);
    assert(fr_player_detects_trap(&ranger, 6, 5));
}

static bool trap_source_points_to_trigger(const FrGame* game, const FrTrap* trap) {
    assert(trap->source_x < FR_MAP_W && trap->source_y < FR_MAP_H);
    assert(fr_get_terrain(game, trap->source_x, trap->source_y) == FR_TERR_WALL);
    assert((trap->dir_x == 0) != (trap->dir_y == 0));
    int16_t x = trap->source_x;
    int16_t y = trap->source_y;
    for(uint8_t step = 0; step < 24; step++) {
        x = (int16_t)(x + trap->dir_x);
        y = (int16_t)(y + trap->dir_y);
        if(x < 0 || y < 0 || x >= FR_MAP_W || y >= FR_MAP_H) return false;
        if((uint8_t)x == trap->x && (uint8_t)y == trap->y) return true;
        uint8_t terrain = fr_get_terrain(game, (uint8_t)x, (uint8_t)y);
        if(terrain == FR_TERR_WALL || terrain == FR_TERR_DOOR_CLOSED) return false;
    }
    return false;
}

static void test_v11_directional_traps_have_wall_sources(void) {
    FrGame game;
    make_empty_test_room(&game);
    assert(fr_place_trap(&game, 6, 5, FR_TRAP_ARROW));
    FrTrap* arrow = fr_trap_at(&game, 6, 5);
    assert(arrow != NULL);
    assert(trap_source_points_to_trigger(&game, arrow));

    bool saw_arrow = false;
    bool saw_fire = false;
    for(uint32_t seed = 1u; seed <= 120u; seed++) {
        fr_game_init_class(&game, seed, FR_CLASS_WARRIOR);
        fr_generate_floor(&game, 8);
        for(uint8_t i = 0; i < FR_MAX_TRAPS; i++) {
            FrTrap* trap = &game.traps[i];
            if(!trap->active) continue;
            if(trap->type != FR_TRAP_ARROW && trap->type != FR_TRAP_FIRE) continue;
            if(trap->type == FR_TRAP_ARROW) saw_arrow = true;
            if(trap->type == FR_TRAP_FIRE) saw_fire = true;
            assert(trap_source_points_to_trigger(&game, trap));
        }
    }
    assert(saw_arrow);
    assert(saw_fire);

    FrRoom tiny = {5, 5, 3, 3};
    uint8_t tx = 0;
    uint8_t ty = 0;
    uint8_t wx = 0;
    uint8_t wy = 0;
    assert(!fr_find_wall_opposed_floor_in_room(&game, &tiny, &tx, &ty, &wx, &wy));
}

static void test_v11_arrow_trap_damage_projectile_and_reveal(void) {
    FrGame game;
    make_empty_test_room(&game);
    game.player.hp = game.player.max_hp;
    assert(fr_place_trap(&game, 6, 5, FR_TRAP_ARROW));
    FrTrap* arrow = fr_trap_at(&game, 6, 5);
    assert(arrow != NULL);
    uint8_t source_x = arrow->source_x;
    uint8_t source_y = arrow->source_y;

    assert(fr_move_player(&game, 1, 0).kind == FR_ACTION_MOVE);
    assert(game.player.hp < game.player.max_hp);
    assert(strstr(game.log, "Arrow trap") != NULL);
    assert(game.last_event == FR_EVENT_MON_PROJECTILE);
    assert(game.event_x == source_x && game.event_y == source_y);
    assert(game.event_tx == game.player.x && game.event_ty == game.player.y);
    assert(game.event_glyph == '-');

    make_empty_test_room(&game);
    game.player.hp = game.player.max_hp;
    game.player.class_id = FR_CLASS_RANGER;
    game.player.dex = 6;
    assert(fr_place_trap(&game, 6, 5, FR_TRAP_ARROW));
    assert(fr_player_detects_trap(&game, 6, 5));
    assert(fr_move_player(&game, 1, 0).kind == FR_ACTION_MOVE);
    assert(game.player.hp == game.player.max_hp);
    assert(fr_trap_at(&game, 6, 5) != NULL);
}

static void test_v11_fire_launcher_trap_projectile_and_fire_field(void) {
    FrGame game;
    make_empty_test_room(&game);
    game.player.hp = game.player.max_hp;
    fr_set_terrain(&game, 7, 5, FR_TERR_GRASS);
    fr_set_terrain(&game, 6, 4, FR_TERR_PUDDLE);
    fr_set_terrain(&game, 6, 6, FR_TERR_WATER);
    assert(fr_place_trap(&game, 6, 5, FR_TRAP_FIRE));
    FrTrap* trap = fr_trap_at(&game, 6, 5);
    assert(trap != NULL);
    uint8_t source_x = trap->source_x;
    uint8_t source_y = trap->source_y;

    assert(fr_move_player(&game, 1, 0).kind == FR_ACTION_MOVE);
    assert(game.last_event == FR_EVENT_MON_PROJECTILE);
    assert(game.event_x == source_x && game.event_y == source_y);
    assert(game.event_tx == game.player.x && game.event_ty == game.player.y);
    assert(game.event_glyph == '*');
    assert(strstr(game.log, "Oil jar") != NULL);
    assert(fr_terrain_fire_at(&game, game.floor, 6, 5));
    assert(fr_get_terrain(&game, 7, 5) == FR_TERR_GRASS_TRAMPLED);
    assert(!fr_terrain_fire_at(&game, game.floor, 6, 4));
    assert(!fr_terrain_fire_at(&game, game.floor, 6, 6));
}

static void test_v11_snare_trap_paralyzes_for_three_ticks(void) {
    FrGame game;
    make_empty_test_room(&game);
    assert(fr_place_trap(&game, 6, 5, FR_TRAP_SNARE));
    assert(fr_move_player(&game, 1, 0).kind == FR_ACTION_MOVE);
    assert((game.player.effects & FR_FX_STUNNED) != 0);
    assert(game.player.fx_timer[FR_FX_STUNNED_INDEX] == 3);
    assert(game.last_event == FR_EVENT_SNARE);
    assert(strstr(game.log, "Snare locks") != NULL);

    assert(fr_move_player(&game, 1, 0).kind == FR_ACTION_REST);
    assert(game.player.x == 6 && game.player.y == 5);
    assert(game.player.fx_timer[FR_FX_STUNNED_INDEX] == 2);
    fr_finish_world_turns(&game, 2);
    assert((game.player.effects & FR_FX_STUNNED) == 0);
}

static void test_v11_terrain_fire_capacity_ticks_and_spread(void) {
    FrGame game;
    make_empty_test_room(&game);
    game.seed = 2u;
    fr_set_terrain(&game, 7, 5, FR_TERR_GRASS);
    fr_set_terrain(&game, 5, 5, FR_TERR_PUDDLE);
    fr_set_terrain(&game, 6, 4, FR_TERR_SAND);
    fr_set_terrain(&game, 6, 6, FR_TERR_WATER);
    game.player.x = 6;
    game.player.y = 5;
    FrActor* rat = fr_spawn_actor(&game, FR_MON_RAT, 7, 5);
    assert(rat != NULL);

    fr_ignite_fire_field(&game, game.floor, 6, 5, 0);
    assert(fr_terrain_fire_at(&game, game.floor, 6, 5));
    uint8_t hp_before = game.player.hp;
    fr_tick_terrain_effects(&game);
    assert(game.player.hp < hp_before);
    assert(hp_before - game.player.hp <= 3);
    assert((game.player.effects & FR_FX_BURNING) != 0);
    assert(fr_terrain_fire_at(&game, game.floor, 7, 5));
    assert(fr_get_terrain(&game, 7, 5) == FR_TERR_GRASS_TRAMPLED);
    fr_tick_terrain_effects(&game);
    assert(!rat->active || rat->hp < rat->max_hp || (rat->effects & FR_FX_BURNING) != 0);
    assert(!fr_terrain_fire_at(&game, game.floor, 5, 5));
    assert(!fr_terrain_fire_at(&game, game.floor, 6, 4));
    assert(!fr_terrain_fire_at(&game, game.floor, 6, 6));

    FrGame capacity;
    make_empty_test_room(&capacity);
    fr_ignite_fire_field(&capacity, 1, 5, 5, 0);
    fr_ignite_fire_field(&capacity, 2, 5, 5, 0);
    fr_ignite_fire_field(&capacity, 3, 5, 5, 0);
    fr_ignite_fire_field(&capacity, 4, 5, 5, 0);
    assert(fr_active_terrain_field_count(&capacity) == FR_TERRAIN_FIELD_CAP);
    assert(!fr_terrain_fire_at(&capacity, 1, 5, 5));
    assert(fr_terrain_fire_at(&capacity, 4, 5, 5));
    capacity.floor = 1;
    for(uint8_t i = 0; i < 6; i++) fr_tick_terrain_effects(&capacity);
    assert(fr_active_terrain_field_count(&capacity) == 0);
}

static void test_v12_glass_charm_peeks_one_tile_past_walls(void) {
    FrGame game;
    make_empty_test_room(&game);
    fr_set_terrain(&game, 6, 5, FR_TERR_WALL);
    fr_set_terrain(&game, 7, 5, FR_TERR_FLOOR);
    fr_set_terrain(&game, 8, 5, FR_TERR_FLOOR);
    fr_update_fov(&game);
    assert((game.tiles[5][7] & FR_TILE_VISIBLE) == 0);

    assert(fr_add_inventory(&game, FR_ITEM_TRINKET, FR_TRINKET_GLASS, 1));
    game.player.inv[0].flags |= FR_INV_EQUIPPED;
    fr_update_fov(&game);
    assert((game.tiles[5][7] & FR_TILE_VISIBLE) != 0);
    assert((game.tiles[5][8] & FR_TILE_VISIBLE) == 0);
}

static void test_v12_fire_extinguish_and_refresh_merge(void) {
    FrGame game;
    make_empty_test_room(&game);
    fr_ignite_fire_field(&game, game.floor, 5, 5, 0);
    fr_ignite_fire_field(&game, game.floor, 8, 5, 0);
    assert(fr_fire_area_has_fire(&game, game.floor, 5, 5, 1));
    assert(fr_extinguish_fire_area(&game, game.floor, 5, 5, 1));
    assert(!fr_terrain_fire_at(&game, game.floor, 5, 5));
    assert(fr_terrain_fire_at(&game, game.floor, 8, 5));

    FrGame merge;
    make_empty_test_room(&merge);
    fr_refresh_or_expand_fire_field(&merge, merge.floor, 5, 5, 1);
    assert(fr_active_terrain_field_count(&merge) == 1);
    fr_refresh_or_expand_fire_field(&merge, merge.floor, 6, 5, 1);
    assert(fr_active_terrain_field_count(&merge) == 1);
    assert(fr_terrain_fire_at(&merge, merge.floor, 8, 5));

    for(uint8_t i = 0; i < 6; i++) fr_refresh_or_expand_fire_field(&merge, merge.floor, 6, 5, 1);
    assert(fr_active_terrain_field_count(&merge) <= FR_TERRAIN_FIELD_CAP);
}

static void test_v12_ice_terrain_freeze_spread_and_slide(void) {
    FrGame game;
    make_empty_test_room(&game);
    assert(fr_is_walkable(FR_TERR_ICE));
    assert(!fr_blocks_sight(FR_TERR_ICE));
    assert(strcmp(fr_terrain_name(FR_TERR_ICE), "Ice") == 0);

    fr_set_terrain(&game, 6, 5, FR_TERR_WATER);
    fr_set_terrain(&game, 7, 5, FR_TERR_WATER);
    fr_set_terrain(&game, 8, 5, FR_TERR_FLOOR);
    FrActor* eel = fr_spawn_actor(&game, FR_MON_EEL, 6, 5);
    assert(eel != NULL);
    fr_freeze_water_area(&game, game.floor, 6, 5, 0);
    assert(fr_get_terrain(&game, 6, 5) == FR_TERR_ICE);
    assert(!eel->active);
    fr_tick_terrain_effects(&game);
    assert(fr_get_terrain(&game, 7, 5) == FR_TERR_ICE);
    fr_set_terrain(&game, 5, 5, FR_TERR_WATER);
    FrActor* eel_again = fr_spawn_actor(&game, FR_MON_EEL, 5, 5);
    assert(eel_again != NULL);
    assert(!fr_try_move_actor_current(&game, eel_again, 1, 0));

    FrGame slide;
    make_empty_test_room(&slide);
    slide.seed = 4u;
    fr_set_terrain(&slide, 6, 5, FR_TERR_ICE);
    fr_set_terrain(&slide, 7, 5, FR_TERR_ICE);
    fr_set_terrain(&slide, 8, 5, FR_TERR_ICE);
    assert(fr_move_player(&slide, 1, 0).kind == FR_ACTION_MOVE);
    assert(slide.player.x == 8 && slide.player.y == 5);

    FrGame actor_slide;
    make_empty_test_room(&actor_slide);
    actor_slide.player.x = 4;
    actor_slide.player.y = 5;
    fr_set_terrain(&actor_slide, 6, 5, FR_TERR_ICE);
    fr_set_terrain(&actor_slide, 7, 5, FR_TERR_ICE);
    fr_set_terrain(&actor_slide, 8, 5, FR_TERR_ICE);
    FrActor* rat = fr_spawn_actor(&actor_slide, FR_MON_RAT, 5, 5);
    assert(rat != NULL);
    actor_slide.seed = 4u;
    assert(fr_try_move_actor_current(&actor_slide, rat, 1, 0));
    assert(rat->x == 8 && rat->y == 5);
}

static void test_v07_hidden_door_search_and_connectivity(void) {
    FrGame game;
    make_empty_test_room(&game);
    game.player.dex = 6;
    fr_set_terrain(&game, 6, 4, FR_TERR_WALL);
    game.tiles[4][6] |= FR_TILE_HIDDEN_DOOR;

    assert(fr_search_nearby(&game));
    assert(fr_get_terrain(&game, 6, 4) == FR_TERR_DOOR_CLOSED);
    assert((game.tiles[4][6] & FR_TILE_HIDDEN_DOOR) == 0);

    static const uint32_t seeds[] = {7u, 101u, 777u, 2027u};
    for(uint8_t i = 0; i < sizeof(seeds) / sizeof(seeds[0]); i++) {
        fr_game_init_class(&game, seeds[i], FR_CLASS_RANGER);
        for(uint8_t floor = 1; floor < FR_MAX_FLOORS; floor++) {
            fr_generate_floor(&game, floor);
            uint8_t up_x = 0;
            uint8_t up_y = 0;
            uint8_t down_x = 0;
            uint8_t down_y = 0;
            assert(fr_find_first_tile(&game, FR_TERR_STAIRS_UP, &up_x, &up_y));
            assert(fr_find_first_tile(&game, FR_TERR_STAIRS_DOWN, &down_x, &down_y));
            assert(fr_path_exists(&game, up_x, up_y, down_x, down_y));
        }
    }
}

static void test_v102_secret_search_radius_bump_reveal_and_generation(void) {
    FrGame game;
    make_empty_test_room(&game);
    game.player.dex = 6;
    fr_set_terrain(&game, 7, 5, FR_TERR_WALL);
    game.tiles[5][7] |= FR_TILE_HIDDEN_DOOR;
    assert(fr_search_nearby(&game));
    assert(fr_get_terrain(&game, 7, 5) == FR_TERR_DOOR_CLOSED);
    assert((game.tiles[5][7] & FR_TILE_HIDDEN_DOOR) == 0);

    make_empty_test_room(&game);
    game.player.dex = 0;
    fr_set_terrain(&game, 6, 5, FR_TERR_WALL);
    game.tiles[5][6] |= FR_TILE_HIDDEN_DOOR;
    assert(fr_move_player(&game, 1, 0).kind == FR_ACTION_BLOCKED);
    assert(fr_get_terrain(&game, 6, 5) == FR_TERR_DOOR_CLOSED);
    assert((game.tiles[5][6] & FR_TILE_HIDDEN_DOOR) == 0);
    assert(game.last_event == FR_EVENT_SECRET);

    bool saw_secret = false;
    for(uint32_t seed = 1u; seed < 40u; seed++) {
        fr_game_init_class(&game, seed, FR_CLASS_WARRIOR);
        for(uint8_t y = 1; y < FR_MAP_H - 1; y++) {
            for(uint8_t x = 1; x < FR_MAP_W - 1; x++) {
                if((game.tiles[y][x] & FR_TILE_HIDDEN_DOOR) == 0) continue;
                saw_secret = true;
                uint8_t walkable_sides = 0;
                if(terrain_is_walkable_for_test(fr_get_terrain(&game, (uint8_t)(x - 1), y))) walkable_sides++;
                if(terrain_is_walkable_for_test(fr_get_terrain(&game, (uint8_t)(x + 1), y))) walkable_sides++;
                if(terrain_is_walkable_for_test(fr_get_terrain(&game, x, (uint8_t)(y - 1)))) walkable_sides++;
                if(terrain_is_walkable_for_test(fr_get_terrain(&game, x, (uint8_t)(y + 1)))) walkable_sides++;
                assert(walkable_sides >= 2);
            }
        }
    }
    assert(saw_secret);
}

static void test_v11_water_gated_secret_rooms_are_reachable(void) {
    bool saw_water_gate = false;
    for(uint32_t seed = 1u; seed <= 220u && !saw_water_gate; seed++) {
        FrGame game;
        fr_game_init_class(&game, seed, FR_CLASS_WARRIOR);
        fr_generate_floor(&game, 5);

        for(uint8_t y = 1; y < FR_MAP_H - 1 && !saw_water_gate; y++) {
            for(uint8_t x = 1; x < FR_MAP_W - 1 && !saw_water_gate; x++) {
                if((game.tiles[y][x] & FR_TILE_HIDDEN_DOOR) == 0) continue;
                bool touches_water = false;
                const int8_t dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
                for(uint8_t i = 0; i < 4; i++) {
                    uint8_t nx = (uint8_t)(x + dirs[i][0]);
                    uint8_t ny = (uint8_t)(y + dirs[i][1]);
                    if(fr_get_terrain(&game, nx, ny) == FR_TERR_WATER) touches_water = true;
                }
                if(!touches_water) continue;

                game.tiles[y][x] &= (uint8_t)~FR_TILE_HIDDEN_DOOR;
                fr_set_terrain(&game, x, y, FR_TERR_DOOR_OPEN);
                bool reachable_loot = false;
                bool nearby_guard = false;
                uint8_t pocket_x = x;
                uint8_t pocket_y = y;
                for(uint8_t i = 0; i < 4; i++) {
                    uint8_t nx = (uint8_t)(x + dirs[i][0]);
                    uint8_t ny = (uint8_t)(y + dirs[i][1]);
                    if(fr_get_terrain(&game, nx, ny) != FR_TERR_WATER && terrain_is_walkable_for_test(fr_get_terrain(&game, nx, ny))) {
                        pocket_x = nx;
                        pocket_y = ny;
                    }
                }
                for(uint8_t i = 0; i < FR_MAX_ITEMS; i++) {
                    FrItem* item = &game.items[i];
                    if(!item->active) continue;
                    uint8_t dist = (uint8_t)(abs((int)item->x - (int)x) + abs((int)item->y - (int)y));
                    if(dist <= 10 && fr_path_exists(&game, x, y, item->x, item->y)) reachable_loot = true;
                }
                for(uint8_t i = 0; i < FR_MAX_ACTORS; i++) {
                    FrActor* actor = &game.actors[i];
                    if(!actor->active) continue;
                    uint8_t dist = (uint8_t)(abs((int)actor->x - (int)pocket_x) + abs((int)actor->y - (int)pocket_y));
                    if(dist <= 4 && fr_path_exists(&game, pocket_x, pocket_y, actor->x, actor->y)) nearby_guard = true;
                }
                assert(reachable_loot);
                assert(!nearby_guard);
                saw_water_gate = true;
            }
        }
    }
    assert(saw_water_gate);
}

static void test_v10_secret_reveal_event_debounces(void) {
    FrGame game;
    make_empty_test_room(&game);
    game.player.dex = 6;
    game.tiles[4][6] |= FR_TILE_HIDDEN_DOOR;
    game.tiles[6][6] |= FR_TILE_HIDDEN_DOOR;

    assert(fr_search_nearby(&game));
    assert((game.tiles[4][6] & FR_TILE_HIDDEN_DOOR) == 0);
    assert((game.tiles[6][6] & FR_TILE_HIDDEN_DOOR) == 0);
    assert(game.last_event == FR_EVENT_SECRET);
}

static void test_closed_door_opens_and_closes(void) {
    FrGame game;
    make_empty_test_room(&game);
    fr_set_terrain(&game, 6, 5, FR_TERR_DOOR_CLOSED);
    fr_set_terrain(&game, 7, 5, FR_TERR_FLOOR);

    FrActionResult first = fr_move_player(&game, 1, 0);
    assert(first.kind == FR_ACTION_MOVE);
    assert(game.player.x == 6);
    assert(game.player.y == 5);
    assert(fr_get_terrain(&game, 6, 5) == FR_TERR_DOOR_OPEN);

    FrActionResult second = fr_move_player(&game, 1, 0);
    assert(second.kind == FR_ACTION_MOVE);
    assert(game.player.x == 7);
    assert(game.player.y == 5);

    FrActionResult third = fr_move_player(&game, 1, 0);
    assert(third.kind == FR_ACTION_MOVE);
    assert(game.player.x == 8);
    assert(game.player.y == 5);

    fr_auto_close_doors(&game);
    assert(fr_get_terrain(&game, 6, 5) == FR_TERR_DOOR_CLOSED);
}

static void test_v09_fire_burst_changes_terrain_and_respects_puddles(void) {
    FrGame game;
    make_empty_test_room(&game);
    fr_set_terrain(&game, 6, 5, FR_TERR_GRASS);
    fr_set_terrain(&game, 7, 5, FR_TERR_PUDDLE);
    FrActor* rat = fr_spawn_actor(&game, FR_MON_RAT, 6, 5);
    assert(rat != NULL);
    assert(fr_add_inventory(&game, FR_ITEM_POTION, FR_POTION_FLAME, 1));
    assert(fr_use_inventory(&game, 0, FR_USE_THROW, 6, 5).kind == FR_ACTION_USE);
    assert(fr_get_terrain(&game, 6, 5) == FR_TERR_GRASS_TRAMPLED);
    assert(fr_get_terrain(&game, 7, 5) == FR_TERR_PUDDLE);
    assert(!rat->active || (rat->effects & FR_FX_BURNING) != 0);
}

static void test_v102_puddles_can_spread_during_generation(void) {
    bool saw_single_puddle = false;
    bool saw_spread_puddle = false;
    for(uint32_t seed = 1u; seed < 120u; seed++) {
        FrGame game;
        fr_game_init_class(&game, seed, FR_CLASS_WARRIOR);
        uint8_t puddles = count_terrain(&game, FR_TERR_PUDDLE);
        if(puddles == 1) saw_single_puddle = true;
        if(puddles > 1) saw_spread_puddle = true;
        assert(puddles >= 1);
        assert(puddles <= 6);
    }
    assert(saw_single_puddle);
    assert(saw_spread_puddle);
}

static void test_v12_sand_generates_as_patch_and_blocks_fire(void) {
    bool saw_patch = false;
    for(uint32_t seed = 1u; seed < 40u; seed++) {
        FrGame game;
        fr_game_init_class(&game, seed, FR_CLASS_WARRIOR);
        uint8_t sand = count_terrain(&game, FR_TERR_SAND);
        assert(sand == 0 || sand >= 3);
        if(sand >= 3) saw_patch = true;
    }
    assert(saw_patch);

    FrGame fire;
    make_empty_test_room(&fire);
    fr_set_terrain(&fire, 6, 5, FR_TERR_SAND);
    fr_ignite_fire_field(&fire, fire.floor, 5, 5, 0);
    fr_tick_terrain_effects(&fire);
    assert(!fr_terrain_fire_at(&fire, fire.floor, 6, 5));
}

static void test_v11_flooded_rooms_have_deep_water_and_shallow_edges(void) {
    bool saw_deep_water = false;
    for(uint32_t seed = 1u; seed <= 120u; seed++) {
        FrGame game;
        fr_game_init_class(&game, seed, FR_CLASS_WARRIOR);
        fr_generate_floor(&game, 5);
        for(uint8_t y = 1; y < FR_MAP_H - 1; y++) {
            for(uint8_t x = 1; x < FR_MAP_W - 1; x++) {
                if(fr_get_terrain(&game, x, y) != FR_TERR_WATER) continue;
                saw_deep_water = true;
                assert(fr_get_terrain(&game, x, y) != FR_TERR_STAIRS_UP);
                assert(fr_get_terrain(&game, x, y) != FR_TERR_STAIRS_DOWN);
                const int8_t dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
                for(uint8_t i = 0; i < 4; i++) {
                    uint8_t nx = (uint8_t)(x + dirs[i][0]);
                    uint8_t ny = (uint8_t)(y + dirs[i][1]);
                    uint8_t neighbor = fr_get_terrain(&game, nx, ny);
                    if(neighbor == FR_TERR_WATER || neighbor == FR_TERR_WALL ||
                       neighbor == FR_TERR_DOOR_CLOSED || neighbor == FR_TERR_DOOR_OPEN) {
                        continue;
                    }
                    assert(neighbor == FR_TERR_PUDDLE);
                }
            }
        }
    }
    assert(saw_deep_water);

    FrGame movement;
    make_empty_test_room(&movement);
    movement.player.x = 5;
    movement.player.y = 5;
    fr_set_terrain(&movement, 6, 5, FR_TERR_WATER);
    assert(fr_move_player(&movement, 1, 0).kind == FR_ACTION_MOVE);
    assert(movement.player.x == 6 && movement.player.y == 5);

    make_empty_test_room(&movement);
    movement.player.x = 4;
    movement.player.y = 5;
    FrActor* rat = fr_spawn_actor(&movement, FR_MON_RAT, 5, 5);
    assert(rat != NULL);
    fr_set_terrain(&movement, 6, 5, FR_TERR_WATER);
    assert(!fr_try_move_actor_current(&movement, rat, 1, 0));
    assert(rat->x == 5 && rat->y == 5);
}

static void test_v11_water_movement_cost_and_extinguish(void) {
    FrGame deep;
    make_empty_test_room(&deep);
    deep.turn = 10;
    deep.player.effects = FR_FX_BURNING;
    deep.player.fx_timer[FR_FX_BURNING_INDEX] = 5;
    fr_set_terrain(&deep, 6, 5, FR_TERR_WATER);

    assert(fr_move_player(&deep, 1, 0).kind == FR_ACTION_MOVE);
    assert(deep.turn == 12);
    assert((deep.player.effects & FR_FX_BURNING) == 0);
    assert(deep.player.fx_timer[FR_FX_BURNING_INDEX] == 0);
    assert(strstr(deep.log, "Doused") != NULL);

    FrGame puddle;
    make_empty_test_room(&puddle);
    puddle.turn = 10;
    puddle.player.effects = FR_FX_BURNING;
    puddle.player.fx_timer[FR_FX_BURNING_INDEX] = 5;
    fr_set_terrain(&puddle, 6, 5, FR_TERR_PUDDLE);

    assert(fr_move_player(&puddle, 1, 0).kind == FR_ACTION_MOVE);
    assert(puddle.turn == 11);
    assert((puddle.player.effects & FR_FX_BURNING) == 0);
    assert(strstr(puddle.log, "Doused") != NULL);

    FrGame floor;
    make_empty_test_room(&floor);
    floor.turn = 10;
    assert(fr_move_player(&floor, 1, 0).kind == FR_ACTION_MOVE);
    assert(floor.turn == 11);
}
