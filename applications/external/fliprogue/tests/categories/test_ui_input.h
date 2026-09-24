#pragma once

#include "chest_actions.h"
#include "ui_input.h"

static void test_press_key(AppContext* app, InputKey key) {
    InputEvent event = {.key = key, .type = InputTypeShort};
    handle_input(app, &event);
}

static void test_v122_ok_stairs_precede_nearby_objects(void) {
    const int8_t dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    const uint8_t chest_flags[] = {0, FR_ITEM_FLAG_OPENED, FR_ITEM_FLAG_MIMIC};
    for(uint8_t up = 0; up < 2; up++) {
        for(uint8_t dir = 0; dir < 4; dir++) {
            for(size_t state = 0; state < sizeof(chest_flags); state++) {
                FrGame game;
                make_empty_test_room(&game);
                game.run_seed = 122;
                game.floor = 2;
                uint8_t terrain = up ? FR_TERR_STAIRS_UP : FR_TERR_STAIRS_DOWN;
                fr_set_terrain(&game, 5, 5, terrain);
                uint8_t cx = (uint8_t)(5 + dirs[dir][0]);
                uint8_t cy = (uint8_t)(5 + dirs[dir][1]);
                assert(fr_place_chest(&game, cx, cy, false));
                game.items[0].flags = chest_flags[state];
                fr_set_terrain(
                    &game,
                    (uint8_t)(5 - dirs[dir][0]),
                    (uint8_t)(5 - dirs[dir][1]),
                    FR_TERR_SHRINE);
                AppContext app = {.game = &game, .screen = UI_PLAY};

                test_press_key(&app, InputKeyOk);

                assert(game.floor == (up ? 1 : 3));
                assert(app.screen == UI_PLAY);
                assert(app.camera_valid);
                assert(app.cursor_x == game.player.x && app.cursor_y == game.player.y);
                assert(
                    fr_get_terrain(&game, game.player.x, game.player.y) ==
                    (up ? FR_TERR_STAIRS_DOWN : FR_TERR_STAIRS_UP));
                assert(game.floors[1].items[0].type == FR_ITEM_CHEST);
                assert(game.floors[1].items[0].flags == chest_flags[state]);
                assert(game.player.inv_count == 0);
            }
        }
    }
}

static void test_v122_blocked_stairs_do_not_fall_through_to_chest(void) {
    for(uint8_t up = 0; up < 2; up++) {
        FrGame game;
        make_empty_test_room(&game);
        game.floor = up ? 1 : FR_MAX_FLOORS;
        fr_set_terrain(&game, 5, 5, up ? FR_TERR_STAIRS_UP : FR_TERR_STAIRS_DOWN);
        assert(fr_place_chest(&game, 6, 5, false));
        AppContext app = {.game = &game, .screen = UI_PLAY};
        test_press_key(&app, InputKeyOk);
        assert(app.screen == UI_PLAY);
        assert(game.floor == (up ? 1 : FR_MAX_FLOORS));
        assert(game.turn == 0 && game.player.inv_count == 0);
        assert(strcmp(game.log, up ? "The surface waits." : "No deeper stairs.") == 0);
        assert(fr_chest_choice_count(&game, &game.items[0]) == 3);
        if(up) {
            game.player.has_orb = 1;
            test_press_key(&app, InputKeyOk);
            assert(game.mode == FR_MODE_VICTORY);
            assert(app.screen != UI_CHEST_CHOICE);
        }
    }
}

static void test_v122_ok_chest_then_rest_and_directional_access(void) {
    FrGame game;
    make_empty_test_room(&game);
    assert(fr_place_chest(&game, 6, 5, false));
    AppContext app = {.game = &game, .screen = UI_PLAY};
    test_press_key(&app, InputKeyOk);
    assert(app.screen == UI_CHEST_CHOICE && app.item_index == 0);
    assert(game.turn == 0 && game.player.inv_count == 0);
    test_press_key(&app, InputKeyBack);
    assert(app.screen == UI_PLAY && game.items[0].active);

    // A directional bump still reaches the chest while standing on stairs.
    fr_set_terrain(&game, 5, 5, FR_TERR_STAIRS_DOWN);
    test_press_key(&app, InputKeyRight);
    assert(app.screen == UI_CHEST_CHOICE && game.floor == 1);
    test_press_key(&app, InputKeyOk);
    assert(app.screen == UI_PLAY && game.player.inv_count == 1);
    assert((game.items[0].flags & FR_ITEM_FLAG_OPENED) != 0);
    assert(game.floor == 1);

    make_empty_test_room(&game);
    app = (AppContext){.game = &game, .screen = UI_PLAY};
    test_press_key(&app, InputKeyOk);
    assert(app.screen == UI_PLAY && game.turn == 1);
}

static void test_v122_doors_and_shrines_beside_chests(void) {
    FrGame game;
    make_empty_test_room(&game);
    assert(fr_place_chest(&game, 5, 4, false));
    fr_set_terrain(&game, 6, 5, FR_TERR_DOOR_CLOSED);
    AppContext app = {.game = &game, .screen = UI_PLAY};
    test_press_key(&app, InputKeyRight);
    assert(fr_get_terrain(&game, 6, 5) == FR_TERR_DOOR_OPEN);
    assert(game.player.x == 6 && game.player.y == 5);
    assert(app.screen == UI_PLAY && game.player.inv_count == 0);
    assert(game.items[0].flags == 0 && game.turn == 1);

    make_empty_test_room(&game);
    assert(fr_place_chest(&game, 5, 4, false));
    fr_set_terrain(&game, 6, 5, FR_TERR_SHRINE);
    app = (AppContext){.game = &game, .screen = UI_PLAY};
    test_press_key(&app, InputKeyRight);
    assert(app.screen == UI_PLAY && game.player.x == 5);
    assert(strstr(game.log, "An old god") != NULL);
    assert(game.items[0].flags == 0);

    // Retain the current-tile shrine interaction for legacy floor states too.
    fr_set_terrain(&game, 5, 5, FR_TERR_SHRINE);
    test_press_key(&app, InputKeyOk);
    assert(app.screen == UI_PLAY);
    assert(strstr(game.log, "An old god") != NULL);
}
