#include "ui_input.h"
#include "chest_actions.h"
#include "game_core.h"
#include "score_store.h"
#include "shrine_actions.h"
#include "ui_camera.h"
#include "ui_feedback.h"
#include "ui_screens.h"
#include "turns.h"

static uint32_t make_run_seed(AppContext* app, uint8_t class_id) {
    app->run_counter++;
    uint32_t seed = furi_hal_random_get();
    seed ^= furi_get_tick() * 1103515245u;
    seed ^= app->run_counter * 2654435761u;
    seed ^= ((uint32_t)class_id + 1u) * 0x9E3779B9u;
    seed ^= (uint32_t)(uintptr_t)app;
    return seed == 0 ? 0xF11F0001u : seed;
}

static void start_new_game(AppContext* app, uint8_t class_id) {
    fr_game_init_class(app->game, make_run_seed(app, class_id), class_id);
    app->screen = UI_PLAY;
    app->selection = 0;
    app->inv_tab = 0;
    app->scroll = 0;
    app->score_recorded = false;
    app->fx_active = false;
    app->flash_ticks = 0;
    app->hold_blocked = false;
    app->next_hold_tick = 0;
    app->dew_phase = 0;
    app->dew_ticks = 0;
    app->terrain_anim_tick = 0;
    app->paralysis_next_tick = 0;
    app->cursor_x = app->game->player.x;
    app->cursor_y = app->game->player.y;
    camera_center_on_player(app);
}

static void toggle_sound_setting(AppContext* app) {
    app->sound_enabled = !app->sound_enabled;
    save_sound_setting(app);
}

static bool ui_blocks_sight(uint8_t terrain) {
    return terrain == FR_TERR_VOID || terrain == FR_TERR_WALL || terrain == FR_TERR_DOOR_CLOSED ||
           terrain == FR_TERR_GRASS;
}

static bool line_actor_in_view(AppContext* app, int8_t dx, int8_t dy, uint8_t* out_x, uint8_t* out_y) {
    FrGame* game = app->game;
    camera_update(app);
    uint8_t x = game->player.x;
    uint8_t y = game->player.y;
    for(uint8_t step = 0; step < 12; step++) {
        int16_t nx = (int16_t)x + dx;
        int16_t ny = (int16_t)y + dy;
        if(nx < 0 || ny < 0 || nx >= FR_MAP_W || ny >= FR_MAP_H) return false;
        x = (uint8_t)nx;
        y = (uint8_t)ny;
        if(x < app->camera_x || x >= app->camera_x + APP_VIEW_W || y < app->camera_y ||
           y >= app->camera_y + APP_VIEW_H) {
            return false;
        }
        if(ui_blocks_sight(fr_get_terrain(game, x, y))) return false;
        FrActor* actor = fr_actor_at(game, x, y);
        if(actor && fr_actor_visible_to_player(game, actor)) {
            if(out_x) *out_x = x;
            if(out_y) *out_y = y;
            return true;
        }
    }
    return false;
}

static void maybe_open_picked_charm(AppContext* app, uint8_t old_inv_count, FrActionResult result) {
    if(result.kind != FR_ACTION_MOVE || app->screen != UI_PLAY || app->game->mode != FR_MODE_PLAYING) return;
    for(uint8_t i = old_inv_count; i < app->game->player.inv_count; i++) {
        if(app->game->player.inv[i].type != FR_ITEM_TRINKET) continue;
        app->item_index = i;
        app->selection = 0;
        app->item_choice_from_pickup = true;
        app->screen = UI_ITEM_CHOICE;
        return;
    }
}

static FrActionResult play_direction(AppContext* app, int8_t dx, int8_t dy) {
    FrGame* game = app->game;
    FeedbackBefore before = feedback_capture(app);
    uint8_t old_inv_count = game->player.inv_count;
    int16_t ax = (int16_t)game->player.x + dx;
    int16_t ay = (int16_t)game->player.y + dy;
    if(ax >= 0 && ay >= 0 && ax < FR_MAP_W && ay < FR_MAP_H) {
        FrItem* chest = fr_adjacent_chest_for_bump(game, (uint8_t)ax, (uint8_t)ay);
        if(chest) {
            if(fr_chest_choice_count(game, chest) == 0) {
                FrActionResult result = fr_bump_chest(game, chest);
                finish_action(app, before, result);
                return result;
            }
            app->item_index = (uint8_t)(chest - game->items);
            app->selection = 0;
            app->screen = UI_CHEST_CHOICE;
            return (FrActionResult){FR_ACTION_BLOCKED};
        }
    }
    bool adjacent_actor = ax >= 0 && ay >= 0 && ax < FR_MAP_W && ay < FR_MAP_H &&
                          fr_actor_at(game, (uint8_t)ax, (uint8_t)ay) != NULL;
    uint8_t tx = 0;
    uint8_t ty = 0;
    bool ranged_actor = fr_player_can_ranged(game) && line_actor_in_view(app, dx, dy, &tx, &ty);
    if(adjacent_actor) {
        app->flash_x = (uint8_t)ax;
        app->flash_y = (uint8_t)ay;
        app->flash_ticks = 3;
    }
    if(adjacent_actor || game->player.class_id == FR_CLASS_WARRIOR || ranged_actor) {
        if(ranged_actor) {
            app->fx_active = true;
            app->fx_x = (uint8_t)(game->player.x + dx);
            app->fx_y = (uint8_t)(game->player.y + dy);
            app->fx_tx = tx;
            app->fx_ty = ty;
            app->fx_dx = dx;
            app->fx_dy = dy;
            app->fx_glyph = game->player.class_id == FR_CLASS_RANGER ? '-' : '*';
            app->flash_x = tx;
            app->flash_y = ty;
            app->flash_ticks = 4;
        }
        FrActionResult result = fr_try_direction(game, dx, dy);
        finish_action(app, before, result);
        maybe_open_picked_charm(app, old_inv_count, result);
        return result;
    } else {
        FrActionResult result = fr_move_player(game, dx, dy);
        finish_action(app, before, result);
        maybe_open_picked_charm(app, old_inv_count, result);
        return result;
    }
}

bool tick_visual_fx(AppContext* app) {
    bool dirty = false;
    if(app->screen == UI_PLAY) {
        app->terrain_anim_tick++;
        dirty = true;
        if(app->game->mode == FR_MODE_PLAYING && (app->game->player.effects & FR_FX_STUNNED) != 0) {
            uint32_t now = furi_get_tick();
            if(app->paralysis_next_tick == 0) app->paralysis_next_tick = now + PARALYSIS_TICK_MS;
            if(now >= app->paralysis_next_tick) {
                FeedbackBefore before = feedback_capture(app);
                fr_log(app->game, "Dazed.");
                fr_finish_world_turns(app->game, 1);
                finish_action(app, before, (FrActionResult){FR_ACTION_REST});
                app->paralysis_next_tick = now + PARALYSIS_TICK_MS;
            }
        } else {
            app->paralysis_next_tick = 0;
        }
    }
    if(app->fx_active) {
        if(app->fx_x == app->fx_tx && app->fx_y == app->fx_ty) {
            app->fx_active = false;
        } else {
            app->fx_x = (uint8_t)((int8_t)app->fx_x + app->fx_dx);
            app->fx_y = (uint8_t)((int8_t)app->fx_y + app->fx_dy);
        }
        dirty = true;
    }
    if(app->flash_ticks > 0) {
        app->flash_ticks--;
        dirty = true;
    }
    if(app->dew_phase > 0) {
        if(app->dew_ticks > 0) {
            app->dew_ticks--;
        } else if(app->dew_phase == 1) {
            app->dew_phase = 2;
            app->dew_ticks = DEW_PHASE_TICKS;
        } else {
            app->dew_phase = 0;
        }
        dirty = true;
    }
    return dirty;
}

static bool is_direction_key(InputKey key) {
    return key == InputKeyUp || key == InputKeyDown || key == InputKeyLeft || key == InputKeyRight;
}

static void key_to_direction(InputKey key, int8_t* dx, int8_t* dy) {
    *dx = 0;
    *dy = 0;
    if(key == InputKeyUp) *dy = -1;
    else if(key == InputKeyDown) *dy = 1;
    else if(key == InputKeyLeft) *dx = -1;
    else if(key == InputKeyRight) *dx = 1;
}

static void handle_play(AppContext* app, InputKey key, InputType type) {
    if(type == InputTypeLong && key == InputKeyOk) {
        app->screen = UI_LOOK;
        app->cursor_x = app->game->player.x;
        app->cursor_y = app->game->player.y;
        return;
    }
    if(type == InputTypeLong && key == InputKeyBack) {
        app->screen = UI_TITLE;
        app->selection = 0;
        return;
    }
    if(type == InputTypeRelease && is_direction_key(key)) {
        app->hold_blocked = false;
        return;
    }
    if(type == InputTypeRepeat && is_direction_key(key)) {
        if(app->hold_blocked) return;
        uint32_t now = furi_get_tick();
        if(now < app->next_hold_tick) return;
        int8_t dx = 0;
        int8_t dy = 0;
        key_to_direction(key, &dx, &dy);
        FrActionResult result = play_direction(app, dx, dy);
        app->next_hold_tick = now + HOLD_REPEAT_MS;
        if(result.kind == FR_ACTION_ATTACK || result.kind == FR_ACTION_RANGED) app->hold_blocked = true;
        return;
    }
    if(type != InputTypeShort) return;
    app->hold_blocked = false;
    app->next_hold_tick = furi_get_tick() + HOLD_REPEAT_MS;

    switch(key) {
    case InputKeyUp:
        play_direction(app, 0, -1);
        break;
    case InputKeyDown:
        play_direction(app, 0, 1);
        break;
    case InputKeyLeft:
        play_direction(app, -1, 0);
        break;
    case InputKeyRight:
        play_direction(app, 1, 0);
        break;
    case InputKeyOk:
    {
        FeedbackBefore before = feedback_capture(app);
        FrActionResult result = {FR_ACTION_NONE};
        FrItem* chest = fr_chest_at(app->game, app->game->player.x, app->game->player.y);
        if(!chest) {
            static const int8_t dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
            for(uint8_t i = 0; i < 4 && !chest; i++) {
                int16_t cx = (int16_t)app->game->player.x + dirs[i][0];
                int16_t cy = (int16_t)app->game->player.y + dirs[i][1];
                if(cx < 0 || cy < 0 || cx >= FR_MAP_W || cy >= FR_MAP_H) continue;
                chest = fr_adjacent_chest_for_bump(app->game, (uint8_t)cx, (uint8_t)cy);
            }
        }
        if(chest) {
            if(fr_chest_choice_count(app->game, chest) == 0) {
                result = fr_bump_chest(app->game, chest);
                finish_action(app, before, result);
                break;
            }
            app->item_index = (uint8_t)(chest - app->game->items);
            app->selection = 0;
            app->screen = UI_CHEST_CHOICE;
            break;
        }
        if(fr_get_terrain(app->game, app->game->player.x, app->game->player.y) == FR_TERR_SHRINE) {
            result = fr_use_shrine(app->game);
        } else if(fr_get_terrain(app->game, app->game->player.x, app->game->player.y) == FR_TERR_STAIRS_DOWN) {
            uint8_t old_floor = app->game->floor;
            result = fr_descend(app->game);
            if(app->game->floor != old_floor || app->game->mode == FR_MODE_VICTORY) {
                app->cursor_x = app->game->player.x;
                app->cursor_y = app->game->player.y;
                camera_center_on_player(app);
            }
        } else if(fr_get_terrain(app->game, app->game->player.x, app->game->player.y) == FR_TERR_STAIRS_UP) {
            uint8_t old_floor = app->game->floor;
            result = fr_ascend(app->game);
            if(app->game->floor != old_floor || app->game->mode == FR_MODE_VICTORY) {
                app->cursor_x = app->game->player.x;
                app->cursor_y = app->game->player.y;
                camera_center_on_player(app);
            }
        } else {
            result = fr_rest(app->game);
        }
        finish_action(app, before, result);
        break;
    }
    case InputKeyBack:
        app->screen = UI_MENU;
        app->selection = 0;
        break;
    default:
        break;
    }
}

static void handle_chest_choice(AppContext* app, InputKey key) {
    FrItem* chest = app->item_index < FR_MAX_ITEMS ? &app->game->items[app->item_index] : NULL;
    uint8_t count = fr_chest_choice_count(app->game, chest);
    if(key == InputKeyBack) {
        fr_cancel_chest(app->game, chest);
        app->screen = UI_PLAY;
        return;
    }
    if(count == 0) {
        app->screen = UI_PLAY;
        return;
    }
    if(key == InputKeyUp) {
        app->selection = app->selection == 0 ? (uint8_t)(count - 1) : (uint8_t)(app->selection - 1);
        return;
    }
    if(key == InputKeyDown) {
        app->selection = (uint8_t)((app->selection + 1) % count);
        return;
    }
    if(key != InputKeyOk) return;

    FeedbackBefore before = feedback_capture(app);
    FrActionResult result = fr_open_chest_choice(app->game, chest, app->selection);
    app->screen = UI_PLAY;
    finish_action(app, before, result);
}

static void handle_menu(AppContext* app, InputKey key) {
    if(key == InputKeyBack) {
        app->screen = UI_PLAY;
        return;
    }
    if(key == InputKeyUp) {
        app->selection = app->selection == 0 ? (uint8_t)(MENU_COUNT - 1) : (uint8_t)(app->selection - 1);
        return;
    }
    if(key == InputKeyDown) {
        app->selection = (uint8_t)((app->selection + 1) % MENU_COUNT);
        return;
    }
    if(key != InputKeyOk) return;

    switch(app->selection) {
    case 0:
        app->screen = UI_INVENTORY;
        app->selection = 0;
        app->inv_tab = 0;
        break;
    case 1:
    {
        FeedbackBefore before = feedback_capture(app);
        FrActionResult result = fr_eat_food(app->game);
        app->screen = UI_PLAY;
        finish_action(app, before, result);
        break;
    }
    case 2:
        app->screen = UI_LOOK;
        app->cursor_x = app->game->player.x;
        app->cursor_y = app->game->player.y;
        break;
    case 3:
        start_new_game(app, app->game->player.class_id);
        break;
    case 4:
        app->screen = UI_TITLE;
        app->selection = 0;
        break;
    default:
        break;
    }
}

static void handle_inventory(AppContext* app, InputKey key) {
    FrGame* game = app->game;
    if(key == InputKeyBack) {
        app->screen = UI_PLAY;
        return;
    }
    if(key == InputKeyLeft) {
        app->inv_tab = app->inv_tab == 0 ? (uint8_t)(INV_TAB_COUNT - 1) : (uint8_t)(app->inv_tab - 1);
        app->selection = 0;
        inventory_clamp_selection(app);
        return;
    }
    if(key == InputKeyRight) {
        app->inv_tab = (uint8_t)((app->inv_tab + 1) % INV_TAB_COUNT);
        app->selection = 0;
        inventory_clamp_selection(app);
        return;
    }

    uint8_t visible_count = inventory_visible_count(game, app->inv_tab);
    if(visible_count == 0) return;
    if(key == InputKeyUp) {
        app->selection = app->selection == 0 ? (uint8_t)(visible_count - 1) : (uint8_t)(app->selection - 1);
        return;
    }
    if(key == InputKeyDown) {
        app->selection = (uint8_t)((app->selection + 1) % visible_count);
        return;
    }
    if(key != InputKeyOk) return;

    uint8_t inv_index = inventory_slot_index_at(game, app->inv_tab, app->selection);
    if(inv_index == 0xFF) return;

    app->item_index = inv_index;
    app->selection = 0;
    app->item_choice_from_pickup = false;
    app->screen = UI_ITEM_CHOICE;
}

static bool scroll_needs_target(uint8_t scroll) {
    return scroll == FR_SCROLL_FIRE || scroll == FR_SCROLL_BLINK || scroll == FR_SCROLL_FEAR;
}

static void inventory_action_done(AppContext* app, FeedbackBefore before, FrActionResult result) {
    app->screen = UI_PLAY;
    app->selection = 0;
    app->item_choice_from_pickup = false;
    finish_action(app, before, result);
}

static void handle_item_choice(AppContext* app, InputKey key) {
    FrGame* game = app->game;
    if(key == InputKeyBack) {
        app->screen = app->item_choice_from_pickup ? UI_PLAY : UI_INVENTORY;
        app->item_choice_from_pickup = false;
        app->selection = 0;
        return;
    }
    if(app->item_index >= game->player.inv_count) {
        app->screen = UI_PLAY;
        app->item_choice_from_pickup = false;
        return;
    }
    FrInvSlot* slot = &game->player.inv[app->item_index];
    uint8_t count = item_choice_count(slot);
    if(key == InputKeyUp) {
        app->selection = app->selection == 0 ? (uint8_t)(count - 1) : (uint8_t)(app->selection - 1);
        return;
    }
    if(key == InputKeyDown) {
        app->selection = (uint8_t)((app->selection + 1) % count);
        return;
    }
    if(key != InputKeyOk) return;

    if(slot->type == FR_ITEM_POTION) {
        if(app->selection == 0) {
            FeedbackBefore before = feedback_capture(app);
            FrActionResult result =
                fr_use_inventory(game, app->item_index, FR_USE_QUAFF, game->player.x, game->player.y);
            inventory_action_done(app, before, result);
        } else if(app->selection == 1) {
            app->target_index = app->item_index;
            app->target_action = FR_USE_THROW;
            app->cursor_x = game->player.x;
            app->cursor_y = game->player.y;
            app->screen = UI_TARGET;
        } else {
            FeedbackBefore before = feedback_capture(app);
            FrActionResult result =
                fr_use_inventory(game, app->item_index, FR_USE_DROP, game->player.x, game->player.y);
            inventory_action_done(app, before, result);
        }
        return;
    }

    if(app->selection == 1) {
        FeedbackBefore before = feedback_capture(app);
        FrActionResult result =
            fr_use_inventory(game, app->item_index, FR_USE_DROP, game->player.x, game->player.y);
        inventory_action_done(app, before, result);
        return;
    }

    if(slot->type == FR_ITEM_WAND) {
        app->target_index = app->item_index;
        app->target_action = FR_USE_ZAP;
        app->cursor_x = game->player.x;
        app->cursor_y = game->player.y;
        app->screen = UI_TARGET;
    } else if(slot->type == FR_ITEM_THROWABLE) {
        app->target_index = app->item_index;
        app->target_action = FR_USE_THROW;
        app->cursor_x = game->player.x;
        app->cursor_y = game->player.y;
        app->screen = UI_TARGET;
    } else if(slot->type == FR_ITEM_TRINKET) {
        FeedbackBefore before = feedback_capture(app);
        FrActionResult result = fr_use_inventory(game, app->item_index, FR_USE_EQUIP, game->player.x, game->player.y);
        inventory_action_done(app, before, result);
    } else if(slot->type == FR_ITEM_SCROLL && slot->subtype == FR_SCROLL_IDENTIFY &&
              identify_visible_count(game, app->item_index) > 0) {
        app->target_index = app->item_index;
        app->selection = 0;
        app->screen = UI_IDENTIFY_PICK;
    } else if(slot->type == FR_ITEM_SCROLL && scroll_needs_target(slot->subtype)) {
        app->target_index = app->item_index;
        app->target_action = FR_USE_READ;
        app->cursor_x = game->player.x;
        app->cursor_y = game->player.y;
        app->screen = UI_TARGET;
    } else {
        FeedbackBefore before = feedback_capture(app);
        uint8_t action = slot->type == FR_ITEM_SCROLL ? FR_USE_READ : FR_USE_READ;
        FrActionResult result = fr_use_inventory(game, app->item_index, action, game->player.x, game->player.y);
        inventory_action_done(app, before, result);
    }
}

static void handle_identify_pick(AppContext* app, InputKey key) {
    FrGame* game = app->game;
    if(key == InputKeyBack) {
        app->screen = UI_ITEM_CHOICE;
        app->selection = 0;
        return;
    }
    uint8_t count = identify_visible_count(game, app->target_index);
    if(count == 0) {
        app->screen = UI_ITEM_CHOICE;
        app->selection = 0;
        return;
    }
    if(key == InputKeyUp) {
        app->selection = app->selection == 0 ? (uint8_t)(count - 1) : (uint8_t)(app->selection - 1);
        return;
    }
    if(key == InputKeyDown) {
        app->selection = (uint8_t)((app->selection + 1) % count);
        return;
    }
    if(key != InputKeyOk) return;

    uint8_t identify_index = identify_slot_index_at(game, app->target_index, app->selection);
    if(identify_index == 0xFF) return;
    FeedbackBefore before = feedback_capture(app);
    FrActionResult result = fr_use_inventory(game, app->target_index, FR_USE_READ, identify_index, 0xFF);
    inventory_action_done(app, before, result);
}

static void handle_cursor_screen(AppContext* app, InputKey key) {
    switch(key) {
    case InputKeyUp:
        cursor_move(app, 0, -1);
        break;
    case InputKeyDown:
        cursor_move(app, 0, 1);
        break;
    case InputKeyLeft:
        cursor_move(app, -1, 0);
        break;
    case InputKeyRight:
        cursor_move(app, 1, 0);
        break;
    case InputKeyOk:
        if(app->screen == UI_TARGET) {
            FeedbackBefore before = feedback_capture(app);
            int8_t dx = (int8_t)((int16_t)app->cursor_x - (int16_t)app->game->player.x);
            int8_t dy = (int8_t)((int16_t)app->cursor_y - (int16_t)app->game->player.y);
            dx = dx > 0 ? 1 : (dx < 0 ? -1 : 0);
            dy = dy > 0 ? 1 : (dy < 0 ? -1 : 0);
            FrInvSlot* target_slot = app->target_index < app->game->player.inv_count ?
                                         &app->game->player.inv[app->target_index] :
                                         NULL;
            bool can_animate = target_slot != NULL &&
                               !(target_slot->type == FR_ITEM_WAND && target_slot->amount == 0) &&
                               !(app->game->player.cube_hp > 0 && app->target_action == FR_USE_THROW);
            if(can_animate && (dx != 0 || dy != 0) &&
               (app->target_action == FR_USE_ZAP || app->target_action == FR_USE_THROW)) {
                app->fx_active = true;
                app->fx_x = (uint8_t)(app->game->player.x + dx);
                app->fx_y = (uint8_t)(app->game->player.y + dy);
                app->fx_tx = app->cursor_x;
                app->fx_ty = app->cursor_y;
                app->fx_dx = dx;
                app->fx_dy = dy;
                app->fx_glyph = app->target_action == FR_USE_THROW ? '-' : '*';
            }
            FrActionResult result =
                fr_use_inventory(app->game, app->target_index, app->target_action, app->cursor_x, app->cursor_y);
            app->screen = UI_PLAY;
            finish_action(app, before, result);
        } else if(fr_actor_at(app->game, app->cursor_x, app->cursor_y) &&
                  fr_actor_visible_to_player(app->game, fr_actor_at(app->game, app->cursor_x, app->cursor_y))) {
            app->screen = UI_MONSTER_CARD;
        } else {
            app->screen = UI_PLAY;
        }
        break;
    case InputKeyBack:
        app->screen = UI_PLAY;
        break;
    default:
        break;
    }
}

static void handle_perk_choice(AppContext* app, InputKey key) {
    if(key == InputKeyBack) {
        app->selection = 0;
        return;
    }
    if(key == InputKeyUp) {
        app->selection = app->selection == 0 ? 4 : (uint8_t)(app->selection - 1);
        return;
    }
    if(key == InputKeyDown) {
        app->selection = (uint8_t)((app->selection + 1) % 5);
        return;
    }
    if(key != InputKeyOk) return;

    app->perk_choice = app->selection;
    app->screen = UI_PERK_DETAIL;
}

static void handle_perk_detail(AppContext* app, InputKey key) {
    if(key == InputKeyBack) {
        app->screen = UI_PERK_CHOICE;
        app->selection = app->perk_choice;
        return;
    }
    if(key != InputKeyOk) return;
    if(fr_apply_perk(app->game, app->perk_choice)) {
        if(app->game->player.pending_perks == 0) {
            app->screen = UI_PLAY;
        } else {
            app->screen = UI_PERK_CHOICE;
        }
        app->selection = 0;
    }
}

static void handle_scores(AppContext* app, InputKey key) {
    if(key == InputKeyBack || key == InputKeyOk) {
        app->screen = UI_TITLE;
        app->selection = 0;
        app->scroll = 0;
        return;
    }
    if(app->score_count <= SCORE_VISIBLE_ROWS) return;
    if(key == InputKeyUp && app->scroll > 0) {
        app->scroll = fr_ui_prev_page_scroll(app->scroll, SCORE_VISIBLE_ROWS);
    } else if(key == InputKeyDown) {
        app->scroll = fr_ui_next_page_scroll(app->scroll, app->score_count, SCORE_VISIBLE_ROWS);
    }
}

static void handle_help_menu(AppContext* app, InputKey key) {
    if(key == InputKeyBack) {
        app->screen = UI_TITLE;
        app->selection = 0;
        app->scroll = 0;
        return;
    }
    if(key == InputKeyUp) {
        app->selection = app->selection == 0 ? (uint8_t)(HELP_TOPIC_COUNT - 1) : (uint8_t)(app->selection - 1);
    } else if(key == InputKeyDown) {
        app->selection = (uint8_t)((app->selection + 1) % HELP_TOPIC_COUNT);
    } else if(key == InputKeyOk) {
        app->help_topic = app->selection;
        app->screen = UI_HELP_PAGE;
        app->scroll = 0;
    }
}

static void handle_help_page(AppContext* app, InputKey key) {
    uint8_t count = help_line_count(app->help_topic);
    if(key == InputKeyBack || key == InputKeyOk) {
        app->screen = UI_HELP_MENU;
        app->selection = app->help_topic;
        app->scroll = 0;
        return;
    }
    if(count <= HELP_VISIBLE_ROWS) return;
    if(key == InputKeyUp && app->scroll > 0) {
        app->scroll = fr_ui_prev_page_scroll(app->scroll, HELP_VISIBLE_ROWS);
    } else if(key == InputKeyDown) {
        app->scroll = fr_ui_next_page_scroll(app->scroll, count, HELP_VISIBLE_ROWS);
    }
}

void handle_input(AppContext* app, InputEvent* event) {
    if(event->type != InputTypeShort && event->type != InputTypeLong && event->type != InputTypeRepeat &&
       event->type != InputTypeRelease) {
        return;
    }

    if(app->screen == UI_TITLE) {
        if(event->type != InputTypeShort) return;
        if(event->key == InputKeyUp) {
            app->selection =
                app->selection == 0 ? (uint8_t)(TITLE_MENU_COUNT - 1) : (uint8_t)(app->selection - 1);
        } else if(event->key == InputKeyDown) {
            app->selection = (uint8_t)((app->selection + 1) % TITLE_MENU_COUNT);
        } else if((event->key == InputKeyLeft || event->key == InputKeyRight) && app->selection == 3) {
            toggle_sound_setting(app);
        } else if(event->key == InputKeyOk) {
            if(app->selection == 0) {
                app->screen = UI_CLASS_SELECT;
                app->selection = 0;
            } else if(app->selection == 1) {
                load_score_store(app);
                app->screen = UI_SCORES;
                app->scroll = 0;
            } else if(app->selection == 2) {
                app->screen = UI_HELP_MENU;
                app->selection = 0;
                app->scroll = 0;
            } else {
                toggle_sound_setting(app);
            }
        } else if(event->key == InputKeyBack) {
            app->game->mode = FR_MODE_QUIT;
        }
        return;
    }

    if(app->screen == UI_CLASS_SELECT) {
        if(event->type != InputTypeShort) return;
        if(event->key == InputKeyUp) {
            app->selection = app->selection == 0 ? 2 : (uint8_t)(app->selection - 1);
        } else if(event->key == InputKeyDown) {
            app->selection = (uint8_t)((app->selection + 1) % 3);
        } else if(event->key == InputKeyOk) {
            start_new_game(app, app->selection);
        } else if(event->key == InputKeyBack) {
            app->screen = UI_TITLE;
            app->selection = 0;
        }
        return;
    }

    if((app->game->mode == FR_MODE_GAME_OVER || app->game->mode == FR_MODE_VICTORY) &&
       app->screen != UI_SCORES && app->screen != UI_HELP_MENU && app->screen != UI_HELP_PAGE) {
        if(event->type == InputTypeShort && event->key == InputKeyOk) {
            start_new_game(app, app->game->player.class_id);
        } else if(event->type == InputTypeShort && event->key == InputKeyBack) {
            app->screen = UI_TITLE;
            app->selection = 0;
        }
        return;
    }

    switch(app->screen) {
    case UI_SCORES:
        if(event->type == InputTypeShort) handle_scores(app, event->key);
        break;
    case UI_HELP_MENU:
        if(event->type == InputTypeShort) handle_help_menu(app, event->key);
        break;
    case UI_HELP_PAGE:
        if(event->type == InputTypeShort) handle_help_page(app, event->key);
        break;
    case UI_PLAY:
        handle_play(app, event->key, event->type);
        break;
    case UI_MENU:
        if(event->type == InputTypeShort) handle_menu(app, event->key);
        break;
    case UI_INVENTORY:
        if(event->type == InputTypeShort) handle_inventory(app, event->key);
        break;
    case UI_ITEM_CHOICE:
        if(event->type == InputTypeShort) handle_item_choice(app, event->key);
        break;
    case UI_CHEST_CHOICE:
        if(event->type == InputTypeShort) handle_chest_choice(app, event->key);
        break;
    case UI_IDENTIFY_PICK:
        if(event->type == InputTypeShort) handle_identify_pick(app, event->key);
        break;
    case UI_PERK_CHOICE:
        if(event->type == InputTypeShort) handle_perk_choice(app, event->key);
        break;
    case UI_PERK_DETAIL:
        if(event->type == InputTypeShort) handle_perk_detail(app, event->key);
        break;
    case UI_LOOK:
    case UI_TARGET:
    case UI_MONSTER_CARD:
        if(app->screen == UI_MONSTER_CARD) {
            if(event->type == InputTypeShort && (event->key == InputKeyOk || event->key == InputKeyBack)) {
                app->screen = UI_LOOK;
            }
            break;
        }
        if(event->type == InputTypeShort) handle_cursor_screen(app, event->key);
        break;
    case UI_TITLE:
    case UI_CLASS_SELECT:
    default:
        break;
    }
}
