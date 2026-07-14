/**
 * @file p1x_adventure.c
 * @brief Simple adventure game with hero and enemies.
 */
#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <input/input.h>
#include "p1x_adventure_icons.h"
#include <stdlib.h> // For random number generation

#define MAX_ENEMIES             4
#define SCREEN_WIDTH            128
#define SCREEN_HEIGHT           64
#define INITIAL_WEAPON_LENGTH   8
#define INITIAL_WEAPON_DURATION 4
#define CASTLE_WIDTH            15
#define CASTLE_HEIGHT           30
#define PLAYER_SPEED            4

typedef enum {
    GameStateTitle,
    GameStateGameplay,
    GameStateGameOver,
    GameStateWin,
    GameStateUpgrade,
    GameStateBossFight,
} GameStateEnum;

#define DIR_UP    0
#define DIR_RIGHT 1
#define DIR_DOWN  2
#define DIR_LEFT  3

typedef struct {
    int32_t x, y;
    int32_t health;
    uint8_t direction;
    bool weapon_active;
    uint8_t weapon_timer;
} Player;

typedef struct {
    int32_t x, y;
    bool active;
    int32_t hitpoints;
} Enemy;

typedef struct {
    GameStateEnum state;
    Player player;
    Enemy enemies[MAX_ENEMIES];
    int32_t score;
    uint8_t selected_upgrade;
    bool wave_completed;
    int32_t weapon_length;
    int32_t weapon_duration;
    Enemy boss;
} GameState;

static GameState game_state = {
    .state = GameStateTitle,
    .player =
        {.x = 10,
         .y = 30,
         .health = 100,
         .direction = DIR_RIGHT,
         .weapon_active = false,
         .weapon_timer = 0},
    .score = 0,
    .selected_upgrade = 0,
    .wave_completed = false,
    .weapon_length = INITIAL_WEAPON_LENGTH,
    .weapon_duration = INITIAL_WEAPON_DURATION};

static void get_weapon_end_point(int* end_x, int* end_y) {
    *end_x = game_state.player.x + 4;
    *end_y = game_state.player.y + 4;
    switch(game_state.player.direction) {
    case DIR_UP:
        *end_y -= game_state.weapon_length;
        break;
    case DIR_RIGHT:
        *end_x += game_state.weapon_length;
        break;
    case DIR_DOWN:
        *end_y += game_state.weapon_length;
        break;
    case DIR_LEFT:
        *end_x -= game_state.weapon_length;
        break;
    }
}

static bool check_castle_reached() {
    int castle_x = SCREEN_WIDTH - CASTLE_WIDTH;
    int castle_y = (SCREEN_HEIGHT - CASTLE_HEIGHT) / 2;
    int castle_door_y = castle_y + (CASTLE_HEIGHT / 2) - 8;
    return (
        game_state.player.x >= castle_x - 2 && game_state.player.y >= castle_door_y - 4 &&
        game_state.player.y <= castle_door_y + 20);
}

static void draw_title_screen(Canvas* canvas) {
    canvas_clear(canvas);
    canvas_draw_icon(canvas, 68, 0, &I_big_castle);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 48, 12, AlignCenter, AlignCenter, "P1X Adventure");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 48, 26, AlignCenter, AlignCenter, "Rescue the princess");
    canvas_draw_str_aligned(canvas, 48, 36, AlignCenter, AlignCenter, "from the castle!");
    canvas_draw_str_aligned(canvas, 48, 49, AlignCenter, AlignCenter, "Press OK to start");
}

static void draw_game_over_screen(Canvas* canvas) {
    canvas_clear(canvas);
    canvas_draw_icon(canvas, 110, 10, &I_sword); // Draw defeat background
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 24, AlignCenter, AlignCenter, "GAME OVER");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 50, AlignCenter, AlignCenter, "Press Back to restart");
}

static void draw_win_screen(Canvas* canvas) {
    canvas_clear(canvas);
    canvas_draw_icon(canvas, 0, 0, &I_tree_left);
    canvas_draw_icon(canvas, 97, 0, &I_tree_right);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 10, AlignCenter, AlignCenter, "YOU WIN!");
    char score_str[20];
    snprintf(score_str, sizeof(score_str), "Score: %ld", game_state.score);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 30, AlignCenter, AlignCenter, score_str);
    canvas_draw_str_aligned(canvas, 64, 56, AlignCenter, AlignCenter, "Press Back to replay");
}

static void draw_upgrade_screen(Canvas* canvas) {
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 12, AlignCenter, AlignCenter, "LEVEL UP!");
    canvas_set_font(canvas, FontSecondary);
    if(game_state.selected_upgrade == 0) {
        canvas_draw_str_aligned(canvas, 64, 30, AlignCenter, AlignCenter, "> Weapon Upgrade <");
    } else {
        canvas_draw_str_aligned(canvas, 64, 30, AlignCenter, AlignCenter, "  Weapon Upgrade   ");
    }
    if(game_state.selected_upgrade == 1) {
        canvas_draw_str_aligned(canvas, 64, 40, AlignCenter, AlignCenter, "> Health Restore <");
    } else {
        canvas_draw_str_aligned(canvas, 64, 40, AlignCenter, AlignCenter, "  Health Restore  ");
    }
    canvas_draw_str_aligned(canvas, 64, 55, AlignCenter, AlignCenter, "Select your upgrade!");
}

static void draw_gameplay_screen(Canvas* canvas) {
    canvas_clear(canvas);
    canvas_draw_icon(canvas, 128 - 22, 24, &I_castle); // Draw level 1 background

    canvas_draw_frame(canvas, game_state.player.x, game_state.player.y, 8, 8);
    if(game_state.player.weapon_active) {
        int weapon_end_x, weapon_end_y;
        get_weapon_end_point(&weapon_end_x, &weapon_end_y);
        if(weapon_end_x < 0) weapon_end_x = 0;
        if(weapon_end_x >= SCREEN_WIDTH) weapon_end_x = SCREEN_WIDTH - 1;
        if(weapon_end_y < 0) weapon_end_y = 0;
        if(weapon_end_y >= SCREEN_HEIGHT) weapon_end_y = SCREEN_HEIGHT - 1;
        canvas_draw_line(
            canvas, game_state.player.x + 4, game_state.player.y + 4, weapon_end_x, weapon_end_y);
    }
    for(int i = 0; i < MAX_ENEMIES; i++) {
        if(game_state.enemies[i].active) {
            canvas_draw_circle(
                canvas, game_state.enemies[i].x + 4, game_state.enemies[i].y + 4, 4);
        }
    }
    canvas_set_font(canvas, FontSecondary);
    char health_str[12];
    snprintf(health_str, sizeof(health_str), "HP: %ld", game_state.player.health);
    canvas_draw_str(canvas, 5, 10, health_str);
    char score_str[12];
    snprintf(score_str, sizeof(score_str), "Score: %ld", game_state.score);
    canvas_draw_str(canvas, 70, 10, score_str);
}

static void draw_boss_fight_screen(Canvas* canvas) {
    canvas_clear(canvas);
    canvas_draw_frame(canvas, game_state.player.x, game_state.player.y, 8, 8);
    if(game_state.player.weapon_active) {
        int weapon_end_x, weapon_end_y;
        get_weapon_end_point(&weapon_end_x, &weapon_end_y);
        if(weapon_end_x < 0) weapon_end_x = 0;
        if(weapon_end_x >= SCREEN_WIDTH) weapon_end_x = SCREEN_WIDTH - 1;
        if(weapon_end_y < 0) weapon_end_y = 0;
        if(weapon_end_y >= SCREEN_HEIGHT) weapon_end_y = SCREEN_HEIGHT - 1;
        canvas_draw_line(
            canvas, game_state.player.x + 4, game_state.player.y + 4, weapon_end_x, weapon_end_y);
    }
    if(game_state.boss.active) {
        canvas_draw_circle(canvas, game_state.boss.x + 8, game_state.boss.y + 8, 6);
    }
    canvas_set_font(canvas, FontSecondary);
    char health_str[12];
    snprintf(health_str, sizeof(health_str), "HP: %ld", game_state.player.health);
    canvas_draw_str(canvas, 5, 10, health_str);
    char score_str[12];
    snprintf(score_str, sizeof(score_str), "Score: %ld", game_state.score);
    canvas_draw_str(canvas, 70, 10, score_str);
}

static void app_draw_callback(Canvas* canvas, void* ctx) {
    UNUSED(ctx);
    canvas_clear(canvas);
    switch(game_state.state) {
    case GameStateTitle:
        draw_title_screen(canvas);
        break;
    case GameStateGameplay:
        draw_gameplay_screen(canvas);
        break;
    case GameStateGameOver:
        draw_game_over_screen(canvas);
        break;
    case GameStateWin:
        draw_win_screen(canvas);
        break;
    case GameStateUpgrade:
        draw_upgrade_screen(canvas);
        break;
    case GameStateBossFight:
        draw_boss_fight_screen(canvas);
        break;
    }
}

static void app_input_callback(InputEvent* input_event, void* ctx) {
    furi_assert(ctx);
    FuriMessageQueue* event_queue = ctx;
    furi_message_queue_put(event_queue, input_event, FuriWaitForever);
}

static void reset_game() {
    game_state.player.x = 10;
    game_state.player.y = 30;
    game_state.player.health = 100;
    game_state.player.direction = DIR_RIGHT;
    game_state.player.weapon_active = false;
    game_state.player.weapon_timer = 0;
    game_state.score = 0;
    game_state.selected_upgrade = 0;
    game_state.wave_completed = false;
    game_state.weapon_length = INITIAL_WEAPON_LENGTH;
    game_state.weapon_duration = INITIAL_WEAPON_DURATION;
}

static void init_enemies() {
    for(int i = 0; i < MAX_ENEMIES; i++) {
        game_state.enemies[i].x = 30 + (i * 20) % 100;
        game_state.enemies[i].y = 20 + (i * 15) % 30;
        game_state.enemies[i].active = true;
        game_state.enemies[i].hitpoints = 2;
    }
}

static void init_boss_fight() {
    game_state.player.x = 20;
    game_state.player.y = 30;
    game_state.boss.x = 90;
    game_state.boss.y = 30;
    game_state.boss.active = true;
    game_state.boss.hitpoints = 50;
    game_state.state = GameStateBossFight;
}

static bool point_in_circle(int px, int py, int cx, int cy, int radius) {
    int dx = px - cx;
    int dy = py - cy;
    return (dx * dx + dy * dy) <= (radius * radius);
}

static bool line_circle_collision(int x1, int y1, int x2, int y2, int cx, int cy, int radius) {
    int dx = x2 - x1;
    int dy = y2 - y1;
    int length_squared = dx * dx + dy * dy;
    if(length_squared == 0) {
        return point_in_circle(x1, y1, cx, cy, radius);
    }
    float t = ((cx - x1) * dx + (cy - y1) * dy) / (float)length_squared;
    if(t < 0) t = 0;
    if(t > 1) t = 1;
    int nearest_x = x1 + t * dx;
    int nearest_y = y1 + t * dy;
    return point_in_circle(nearest_x, nearest_y, cx, cy, radius);
}

static void check_weapon_collisions() {
    if(!game_state.player.weapon_active) return;
    int weapon_end_x, weapon_end_y;
    get_weapon_end_point(&weapon_end_x, &weapon_end_y);
    for(int i = 0; i < MAX_ENEMIES; i++) {
        if(game_state.enemies[i].active) {
            if(line_circle_collision(
                   game_state.player.x + 4,
                   game_state.player.y + 4,
                   weapon_end_x,
                   weapon_end_y,
                   game_state.enemies[i].x + 4,
                   game_state.enemies[i].y + 4,
                   4)) {
                game_state.enemies[i].active = false;
                game_state.score += 10;
                furi_hal_vibro_on(true);
                furi_delay_ms(20);
                furi_hal_vibro_on(false);
            }
        }
    }
}

static void check_boss_weapon_collision() {
    if(!game_state.player.weapon_active || !game_state.boss.active) return;
    int weapon_end_x, weapon_end_y;
    get_weapon_end_point(&weapon_end_x, &weapon_end_y);
    if(line_circle_collision(
           game_state.player.x + 4,
           game_state.player.y + 4,
           weapon_end_x,
           weapon_end_y,
           game_state.boss.x + 4,
           game_state.boss.y + 4,
           8)) {
        game_state.boss.hitpoints--;
        if(game_state.boss.hitpoints <= 0) {
            game_state.boss.active = false;
            game_state.score += 50;
            game_state.state = GameStateWin;
        }
    }
}

static void check_boss_player_collision() {
    const int collision_threshold = 8;
    int dx = abs(game_state.player.x - game_state.boss.x);
    int dy = abs(game_state.player.y - game_state.boss.y);
    if(dx < collision_threshold && dy < collision_threshold) {
        game_state.player.health -= 10;
        furi_hal_vibro_on(true);
        furi_delay_ms(100);
        furi_hal_vibro_on(false);
        int bounce_dist = 5;
        if(game_state.player.x < game_state.boss.x)
            game_state.player.x =
                (game_state.player.x - bounce_dist < 0) ? 0 : game_state.player.x - bounce_dist;
        else
            game_state.player.x = (game_state.player.x + bounce_dist > SCREEN_WIDTH - 8) ?
                                      SCREEN_WIDTH - 8 :
                                      game_state.player.x + bounce_dist;
        if(game_state.player.y < game_state.boss.y)
            game_state.player.y =
                (game_state.player.y - bounce_dist < 0) ? 0 : game_state.player.y - bounce_dist;
        else
            game_state.player.y = (game_state.player.y + bounce_dist > SCREEN_HEIGHT - 8) ?
                                      SCREEN_HEIGHT - 8 :
                                      game_state.player.y + bounce_dist;
        if(game_state.player.health <= 0) {
            game_state.state = GameStateGameOver;
        }
    }
}

static void check_collisions() {
    const int collision_threshold = 8;
    for(int i = 0; i < MAX_ENEMIES; i++) {
        if(game_state.enemies[i].active) {
            int dx = abs(game_state.player.x - game_state.enemies[i].x);
            int dy = abs(game_state.player.y - game_state.enemies[i].y);
            if(dx < collision_threshold && dy < collision_threshold) {
                game_state.player.health -= 10;
                furi_hal_vibro_on(true);
                furi_delay_ms(100);
                furi_hal_vibro_on(false);
                int bounce_dist = 5;
                if(game_state.player.x < game_state.enemies[i].x)
                    game_state.player.x = (game_state.player.x - bounce_dist < 0) ?
                                              0 :
                                              game_state.player.x - bounce_dist;
                else
                    game_state.player.x = (game_state.player.x + bounce_dist > SCREEN_WIDTH - 8) ?
                                              SCREEN_WIDTH - 8 :
                                              game_state.player.x + bounce_dist;
                if(game_state.player.y < game_state.enemies[i].y)
                    game_state.player.y = (game_state.player.y - bounce_dist < 0) ?
                                              0 :
                                              game_state.player.y - bounce_dist;
                else
                    game_state.player.y = (game_state.player.y + bounce_dist > SCREEN_HEIGHT - 8) ?
                                              SCREEN_HEIGHT - 8 :
                                              game_state.player.y + bounce_dist;
                game_state.enemies[i].hitpoints--;
                if(game_state.enemies[i].hitpoints <= 0) {
                    game_state.enemies[i].active = false;
                }
            }
        }
    }
    if(game_state.player.health <= 0) {
        game_state.state = GameStateGameOver;
    }
}

static void move_enemies() {
    for(int i = 0; i < MAX_ENEMIES; i++) {
        if(game_state.enemies[i].active) {
            if(game_state.enemies[i].x < game_state.player.x) {
                game_state.enemies[i].x++;
            } else if(game_state.enemies[i].x > game_state.player.x) {
                game_state.enemies[i].x--;
            }
            if(game_state.enemies[i].y < game_state.player.y) {
                game_state.enemies[i].y++;
            } else if(game_state.enemies[i].y > game_state.player.y) {
                game_state.enemies[i].y--;
            }
        }
    }
}

static void move_boss() {
    static uint8_t slow_counter = 0;
    slow_counter++;
    if(slow_counter % 5 != 0) return; // Increase boss movement speed by reducing delay

    if(!game_state.boss.active) return;

    // Generate random movement direction
    int direction = rand() % 4;
    switch(direction) {
    case 0: // Move up
        if(game_state.boss.y > 0) game_state.boss.y--;
        break;
    case 1: // Move down
        if(game_state.boss.y < SCREEN_HEIGHT - 16) game_state.boss.y++;
        break;
    case 2: // Move left
        if(game_state.boss.x > 0) game_state.boss.x--;
        break;
    case 3: // Move right
        if(game_state.boss.x < SCREEN_WIDTH - 16) game_state.boss.x++;
        break;
    }
}

static bool all_enemies_defeated() {
    for(int i = 0; i < MAX_ENEMIES; i++) {
        if(game_state.enemies[i].active) return false;
    }
    return true;
}

static void handle_title_screen(InputEvent* event) {
    if(event->type == InputTypePress && event->key == InputKeyOk) {
        reset_game();
        init_enemies();
        game_state.state = GameStateGameplay;
    }
}

static void handle_game_over_screen(InputEvent* event) {
    if(event->type == InputTypePress && event->key == InputKeyBack) {
        game_state.state = GameStateTitle;
    }
}

static void handle_win_screen(InputEvent* event) {
    if(event->type == InputTypePress && event->key == InputKeyBack) {
        game_state.state = GameStateTitle;
    }
}

static void handle_upgrade_screen(InputEvent* event) {
    if(event->type == InputTypePress) {
        switch(event->key) {
        case InputKeyUp:
        case InputKeyDown:
            game_state.selected_upgrade = 1 - game_state.selected_upgrade;
            break;
        case InputKeyOk:
            if(game_state.selected_upgrade == 0) {
                game_state.weapon_duration += 1;
                game_state.weapon_length += 2;
            } else {
                game_state.player.health += 25;
                if(game_state.player.health > 100) {
                    game_state.player.health = 100;
                }
            }
            init_enemies();
            game_state.state = GameStateGameplay;
            game_state.wave_completed = false;
            furi_hal_vibro_on(true);
            furi_delay_ms(50);
            furi_hal_vibro_on(false);
            break;
        default:
            break;
        }
    }
}

int32_t p1x_adventure_main(void* p) {
    UNUSED(p);
    FuriMessageQueue* event_queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    ViewPort* view_port = view_port_alloc();
    view_port_draw_callback_set(view_port, app_draw_callback, NULL);
    view_port_input_callback_set(view_port, app_input_callback, event_queue);
    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);
    InputEvent event;
    uint32_t last_tick = furi_get_tick();
    bool running = true;

    while(running) {
        if(furi_message_queue_get(event_queue, &event, 10) == FuriStatusOk) {
            if(event.type == InputTypePress) {
                if(event.key == InputKeyBack) {
                    if(game_state.state == GameStateTitle) {
                        running = false;
                    } else {
                        game_state.state = GameStateTitle;
                    }
                } else {
                    switch(game_state.state) {
                    case GameStateTitle:
                        handle_title_screen(&event);
                        break;
                    case GameStateGameOver:
                        handle_game_over_screen(&event);
                        break;
                    case GameStateWin:
                        handle_win_screen(&event);
                        break;
                    case GameStateUpgrade:
                        handle_upgrade_screen(&event);
                        break;
                    case GameStateGameplay:
                    case GameStateBossFight: // Ensure weapon timer works on both screens
                        if((event.type == InputTypePress) || (event.type == InputTypeRepeat)) {
                            switch(event.key) {
                            case InputKeyLeft:
                                game_state.player.x -= PLAYER_SPEED;
                                game_state.player.direction = DIR_LEFT;
                                if(game_state.player.x < 0) game_state.player.x = 0;
                                break;
                            case InputKeyRight:
                                game_state.player.x += PLAYER_SPEED;
                                game_state.player.direction = DIR_RIGHT;
                                if(game_state.player.x > SCREEN_WIDTH - 8)
                                    game_state.player.x = SCREEN_WIDTH - 8;
                                break;
                            case InputKeyUp:
                                game_state.player.y -= PLAYER_SPEED;
                                game_state.player.direction = DIR_UP;
                                if(game_state.player.y < 15) game_state.player.y = 15;
                                break;
                            case InputKeyDown:
                                game_state.player.y += PLAYER_SPEED;
                                game_state.player.direction = DIR_DOWN;
                                if(game_state.player.y > SCREEN_HEIGHT - 8)
                                    game_state.player.y = SCREEN_HEIGHT - 8;
                                break;
                            case InputKeyOk:
                                game_state.player.weapon_active = true;
                                game_state.player.weapon_timer = game_state.weapon_duration;
                                break;
                            default:
                                break;
                            }
                        }
                        break;
                    }
                }
            }
        }

        uint32_t current_tick = furi_get_tick();
        if((game_state.state == GameStateGameplay || game_state.state == GameStateBossFight) &&
           current_tick - last_tick > 100) {
            if(game_state.player.weapon_active) {
                if(--game_state.player.weapon_timer <= 0) {
                    game_state.player.weapon_active = false;
                }
            }
            if(game_state.state == GameStateGameplay) {
                move_enemies();
                check_collisions();
                check_weapon_collisions();
                if(check_castle_reached()) {
                    init_boss_fight();
                }
                if(all_enemies_defeated() && !game_state.wave_completed) {
                    game_state.wave_completed = true;
                    game_state.score += 20;
                    game_state.state = GameStateUpgrade;
                }
            } else if(game_state.state == GameStateBossFight) {
                move_boss();
                check_boss_weapon_collision();
                check_boss_player_collision();
            }
            last_tick = current_tick;
        }

        view_port_update(view_port);
    }

    view_port_enabled_set(view_port, false);
    gui_remove_view_port(gui, view_port);
    view_port_free(view_port);
    furi_message_queue_free(event_queue);
    furi_record_close(RECORD_GUI);
    return 0;
}
