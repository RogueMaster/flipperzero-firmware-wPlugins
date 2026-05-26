#include "ui_draw.h"
#include "score_store.h"
#include "tile_sprites.h"
#include "ui_camera.h"
#include "ui_screens.h"

FrItem* item_at(FrGame* game, uint8_t x, uint8_t y) {
    for(uint8_t i = 0; i < FR_MAX_ITEMS; i++) {
        FrItem* item = &game->items[i];
        if(item->active && item->x == x && item->y == y) return item;
    }
    return NULL;
}

void draw_text_center(Canvas* canvas, int y, const char* text) {
    canvas_draw_str_aligned(canvas, SCREEN_W / 2, y, AlignCenter, AlignTop, text);
}

static const char* death_status(const FrGame* game) {
    switch(game->death_cause) {
    case FR_DEATH_STARVED:
        return " STRV";
    case FR_DEATH_BURNED:
        return " BURN";
    case FR_DEATH_POISONED:
        return " POIS";
    case FR_DEATH_KILLED:
    default:
        return " KILLED";
    }
}

void draw_status(Canvas* canvas, const FrGame* game) {
    char line[40];
    const char* state = "";
    const char* orb = "";
    if(game->mode == FR_MODE_GAME_OVER) {
        state = death_status(game);
    } else if(game->mode == FR_MODE_VICTORY) {
        state = " WIN";
    } else if(game->mode == FR_MODE_PLAYING && game->player.has_orb) {
        orb = "*";
    }
    snprintf(
        line,
        sizeof(line),
        "%c%u HP%u/%u F%u%s%s",
        fr_player_class_name(game->player.class_id)[0],
        game->player.level,
        game->player.hp,
        game->player.max_hp,
        game->floor,
        orb,
        state);
    canvas_draw_str(canvas, 0, 8, line);
    uint8_t hunger = fr_hunger_state(game);
    if(game->mode == FR_MODE_PLAYING && hunger != FR_HUNGER_OK) {
        canvas_draw_str(canvas, 78, 8, hunger == FR_HUNGER_STARVING ? "STRV" : "HNGR");
    }
    if(game->mode == FR_MODE_PLAYING && game->player.class_id == FR_CLASS_RANGER) {
        snprintf(line, sizeof(line), "A%u", game->player.arrows);
        canvas_draw_str(canvas, 110, 8, line);
    } else if(game->mode == FR_MODE_PLAYING && game->player.class_id == FR_CLASS_MAGE) {
        snprintf(line, sizeof(line), "C%u", game->player.charges);
        canvas_draw_str(canvas, 110, 8, line);
    }
}

void draw_glyph(Canvas* canvas, uint8_t sx, uint8_t sy, char glyph) {
    if(glyph == ' ') return;
    if(glyph == '.') {
        canvas_draw_dot(canvas, (uint8_t)(sx + TILE_PX / 2), (uint8_t)(sy + TILE_PX / 2));
        return;
    }
    char str[2] = {glyph, '\0'};
    canvas_draw_str_aligned(canvas, sx + TILE_PX / 2, sy, AlignCenter, AlignTop, str);
}

void draw_map(Canvas* canvas, AppContext* app) {
    FrGame* game = app->game;
    camera_update(app);
    uint8_t cam_x = app->camera_x;
    uint8_t cam_y = app->camera_y;

    for(uint8_t vy = 0; vy < APP_VIEW_H; vy++) {
        for(uint8_t vx = 0; vx < APP_VIEW_W; vx++) {
            uint8_t mx = cam_x + vx;
            uint8_t my = cam_y + vy;
            uint8_t sx = vx * TILE_PX;
            uint8_t sy = MAP_Y + vy * TILE_PX;
            uint8_t tile = game->tiles[my][mx];

            if((tile & FR_TILE_EXPLORED) == 0) continue;

            bool drew_terrain = fr_draw_terrain_sprite(canvas, game, mx, my, sx, sy, app->terrain_anim_tick);
            char glyph = drew_terrain ? '\0' : fr_tile_glyph(game, mx, my);
            fr_draw_fire_overlay(canvas, game, mx, my, sx, sy, app->terrain_anim_tick);
            if((tile & FR_TILE_VISIBLE) != 0) {
                FrItem* item = item_at(game, mx, my);
                FrActor* actor = fr_actor_at(game, mx, my);
                if(item) {
                    if(fr_draw_item_sprite(canvas, item, sx, sy)) {
                        glyph = '\0';
                    } else {
                        glyph = fr_item_glyph(item->type);
                    }
                    /* fallback: glyph = fr_item_glyph(item->type); */
                }
                if(actor && fr_actor_visible_to_player(game, actor)) glyph = fr_actor_glyph(actor->type);
            }
            if(game->player.x == mx && game->player.y == my) glyph = '@';
            draw_glyph(canvas, sx, sy, glyph);
        }
    }

    if(app->dew_phase > 0 && app->dew_x >= cam_x && app->dew_x < cam_x + APP_VIEW_W &&
       app->dew_y >= cam_y && app->dew_y < cam_y + APP_VIEW_H) {
        uint8_t sx = (uint8_t)((app->dew_x - cam_x) * TILE_PX);
        uint8_t sy = (uint8_t)(MAP_Y + (app->dew_y - cam_y) * TILE_PX);
        if(app->dew_phase == 1) draw_glyph(canvas, sx, sy, '"');
        canvas_draw_circle(canvas, (uint8_t)(sx + TILE_PX / 2), (uint8_t)(sy + TILE_PX / 2), 3);
    }

    if(app->screen == UI_LOOK || app->screen == UI_TARGET) {
        if(app->cursor_x >= cam_x && app->cursor_x < cam_x + APP_VIEW_W && app->cursor_y >= cam_y &&
           app->cursor_y < cam_y + APP_VIEW_H) {
            uint8_t sx = (app->cursor_x - cam_x) * TILE_PX;
            uint8_t sy = MAP_Y + (app->cursor_y - cam_y) * TILE_PX;
            canvas_draw_frame(canvas, sx, sy, TILE_PX, TILE_PX);
        }
    }
    if(app->fx_active && app->fx_x >= cam_x && app->fx_x < cam_x + APP_VIEW_W && app->fx_y >= cam_y &&
       app->fx_y < cam_y + APP_VIEW_H) {
        draw_glyph(canvas, (uint8_t)((app->fx_x - cam_x) * TILE_PX), (uint8_t)(MAP_Y + (app->fx_y - cam_y) * TILE_PX), app->fx_glyph);
    }
    if(app->flash_ticks > 0 && app->flash_x >= cam_x && app->flash_x < cam_x + APP_VIEW_W && app->flash_y >= cam_y &&
       app->flash_y < cam_y + APP_VIEW_H) {
        canvas_draw_frame(
            canvas,
            (uint8_t)((app->flash_x - cam_x) * TILE_PX),
            (uint8_t)(MAP_Y + (app->flash_y - cam_y) * TILE_PX),
            TILE_PX,
            TILE_PX);
    }
}

void draw_log(Canvas* canvas, const FrGame* game) {
    const char* msg = game->log[0] ? game->log : "D-pad move. OK rest. Back menu.";
    canvas_draw_str_aligned(canvas, 0, LOG_Y, AlignLeft, AlignTop, msg);
}

void draw_death_log(Canvas* canvas, const FrGame* game) {
    char lines[2][32];
    uint8_t count = fr_ui_death_log_lines(fr_death_log(game), lines, 2, 27);
    for(uint8_t row = 0; row < count; row++) draw_text_center(canvas, (int)(39 + row * 8), lines[row]);
}

void draw_menu(Canvas* canvas, AppContext* app) {
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_box(canvas, 0, 0, SCREEN_W, SCREEN_H);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_frame(canvas, 2, 2, 124, 60);
    canvas_draw_str(canvas, 14, 14, "Game Menu");
    char stats[18];
    snprintf(
        stats,
        sizeof(stats),
        "S%u I%u D%u",
        app->game->player.str,
        app->game->player.wil,
        app->game->player.dex);
    canvas_draw_str_aligned(canvas, 122, 6, AlignRight, AlignTop, stats);
    canvas_draw_str(canvas, 14, 23, fr_player_effects_label(app->game));

    for(uint8_t i = 0; i < MENU_COUNT; i++) {
        char label[24];
        if(i == 0) {
            snprintf(label, sizeof(label), "Inventory");
        } else if(i == 1) {
            snprintf(label, sizeof(label), "Eat food (%u left)", app->game->player.food);
        } else if(i == 2) {
            snprintf(label, sizeof(label), "Look");
        } else if(i == 3) {
            snprintf(label, sizeof(label), "New run");
        } else {
            snprintf(label, sizeof(label), "Quit");
        }
        uint8_t y = (uint8_t)(31 + i * 7);
        if(i == app->selection) canvas_draw_str(canvas, 14, y, ">");
        canvas_draw_str(canvas, 24, y, label);
    }
}

void draw_callback(Canvas* canvas, void* ctx) {
    AppContext* app = ctx;
    furi_mutex_acquire(app->mutex, FuriWaitForever);

    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontSecondary);

    if(app->screen == UI_TITLE) {
        draw_title(canvas, app);
        furi_mutex_release(app->mutex);
        return;
    }
    if(app->screen == UI_SCORES) {
        draw_scores(canvas, app);
        furi_mutex_release(app->mutex);
        return;
    }
    if(app->screen == UI_HELP_MENU) {
        draw_help_menu(canvas, app);
        furi_mutex_release(app->mutex);
        return;
    }
    if(app->screen == UI_HELP_PAGE) {
        draw_help_page(canvas, app);
        furi_mutex_release(app->mutex);
        return;
    }
    if(app->screen == UI_CLASS_SELECT) {
        draw_class_select(canvas, app);
        furi_mutex_release(app->mutex);
        return;
    }

    if(app->game->mode == FR_MODE_GAME_OVER) {
        canvas_set_font(canvas, FontSecondary);
        draw_status(canvas, app->game);
        canvas_set_font(canvas, FontPrimary);
        draw_text_center(canvas, 14, "Game Over");
        canvas_set_font(canvas, FontSecondary);
        draw_text_center(canvas, 29, fr_run_summary(app->game));
        draw_death_log(canvas, app->game);
        draw_text_center(canvas, 56, "OK new  Back quit");
        furi_mutex_release(app->mutex);
        return;
    }

    if(app->game->mode == FR_MODE_VICTORY) {
        canvas_set_font(canvas, FontPrimary);
        draw_text_center(canvas, 8, "Victory");
        canvas_set_font(canvas, FontSecondary);
        draw_status(canvas, app->game);
        draw_text_center(canvas, 34, fr_run_summary(app->game));
        draw_text_center(canvas, 44, "The dungeon blinks.");
        draw_text_center(canvas, 52, "OK new  Back quit");
        furi_mutex_release(app->mutex);
        return;
    }

    draw_status(canvas, app->game);
    draw_map(canvas, app);
    if(app->screen == UI_LOOK) {
        canvas_draw_str_aligned(canvas, 0, LOG_Y, AlignLeft, AlignTop, look_hint(app));
    } else if(app->screen == UI_TARGET) {
        canvas_draw_str_aligned(
            canvas,
            0,
            LOG_Y,
            AlignLeft,
            AlignTop,
            app->target_action == FR_USE_ZAP ? "Choose where to ZAP." :
                                                (app->target_action == FR_USE_THROW ? "Choose target." :
                                                                                      "Choose target."));
    } else {
        draw_log(canvas, app->game);
    }

    if(app->screen == UI_MENU) {
        draw_menu(canvas, app);
    } else if(app->screen == UI_INVENTORY) {
        draw_inventory(canvas, app);
    } else if(app->screen == UI_ITEM_CHOICE) {
        draw_item_choice(canvas, app);
    } else if(app->screen == UI_CHEST_CHOICE) {
        draw_chest_choice(canvas, app);
    } else if(app->screen == UI_IDENTIFY_PICK) {
        draw_identify_pick(canvas, app);
    } else if(app->screen == UI_MONSTER_CARD) {
        draw_monster_card(canvas, app);
    } else if(app->screen == UI_PERK_CHOICE) {
        draw_perk_choice(canvas, app);
    } else if(app->screen == UI_PERK_DETAIL) {
        draw_perk_detail(canvas, app);
    }

    furi_mutex_release(app->mutex);
}
