#include <string.h>
#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <input/input.h>
#include <stdlib.h>

// Define screen dimensions
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64

// Player properties
#define PLAYER_WIDTH         11
#define PLAYER_HEIGHT        15
#define PLAYER_HITBOX_WIDTH  (PLAYER_WIDTH - 4)
#define PLAYER_HITBOX_HEIGHT (PLAYER_HEIGHT - 2)

static const uint8_t player_bitmap[] = {0x07, 0x00, 0x09, 0x00, 0x11, 0x00, 0x21, 0x00,
                                        0x41, 0x00, 0x81, 0x00, 0x02, 0x03, 0x04, 0x04,
                                        0x02, 0x03, 0x81, 0x00, 0x41, 0x00, 0x21, 0x00,
                                        0x11, 0x00, 0x09, 0x00, 0x07, 0x00};
int player_x = 16;
int player_y = SCREEN_HEIGHT / 2 - PLAYER_HEIGHT / 2;
int player_lives = 3;
#define MAX_LIVES 3
bool shield_active = false;
bool bonus_life_active = false;
int bonus_life_x = 0;
int bonus_life_y = 0;
int bonus_life_timer = 0;
#define BONUS_LIFE_DURATION_SECONDS 4

// Player speed
#define PLAYER_SPEED 3

// Bullet properties
#define MAX_BULLETS 5
int bullet_count = 0;
int bullet_x[MAX_BULLETS];
int bullet_y[MAX_BULLETS];
#define BULLET_WIDTH  5
#define BULLET_HEIGHT 5
#define BULLET_SPEED  3
bool bullet_active[MAX_BULLETS];

// Score
int score = 0;
char score_str[8];

#define MAX_ENEMIES 6
int enemy_count = MAX_ENEMIES;
int enemy_x[MAX_ENEMIES];
int enemy_y[MAX_ENEMIES];

int enemy_speed[MAX_ENEMIES];

// enemy bitmap
static const uint8_t enemy_bitmap[] = {
    0x7c,
    0x00,
    0xfe,
    0x00,
    0x01,
    0x01,
    0x1d,
    0x01,
    0x71,
    0x01,
    0x01,
    0x01,
    0xfe,
    0x00,
    0x7c,
    0x00}; // Updated to 9x8

// Player life icon and bonus life icon
static const uint8_t life_icon[] = {0x36, 0x49, 0x41, 0x22, 0x14, 0x08};
static const uint8_t bonus_life_icon[] = {0x36, 0x49, 0x41, 0x22, 0x14, 0x08};

bool running = true;

static void app_draw_callback(Canvas* canvas, void* ctx) {
    UNUSED(ctx);
    canvas_clear(canvas);

    // Draw player
    canvas_set_bitmap_mode(canvas, true);
    canvas_draw_xbm(canvas, player_x, player_y, PLAYER_WIDTH, PLAYER_HEIGHT, player_bitmap);

    // Draw enemies
    for(int i = 0; i < enemy_count; ++i) {
        canvas_set_bitmap_mode(canvas, true);
        canvas_draw_xbm(canvas, enemy_x[i], enemy_y[i], 9, 8, enemy_bitmap);
        canvas_set_bitmap_mode(canvas, false);
    }

    // Draw bullets
    for(int i = 0; i < MAX_BULLETS; ++i) {
        if(bullet_active[i]) {
            int bullet_center_x = bullet_x[i] - BULLET_WIDTH / 2;
            int bullet_center_y = bullet_y[i] - BULLET_HEIGHT / 2;
            canvas_set_bitmap_mode(canvas, true);
            canvas_draw_box(canvas, bullet_center_x, bullet_center_y, BULLET_WIDTH, BULLET_HEIGHT);
            canvas_set_bitmap_mode(canvas, false);
        }
    }

    // Draw player lives
    canvas_set_bitmap_mode(canvas, true);
    for(int i = 0; i < player_lives; ++i) {
        canvas_draw_xbm(canvas, SCREEN_WIDTH - (i + 1) * 12, 0, 7, 7, life_icon);
    }

    // Draw bonus life icon
    if(bonus_life_active) {
        canvas_draw_xbm(canvas, bonus_life_x, bonus_life_y, 7, 7, bonus_life_icon);
    }

    // Draw shield if active
    if(shield_active) {
        canvas_draw_str(canvas, 30, 50, "Shield Active");
    }

    // Draw score
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 8, "Score:");
    itoa(score, score_str, 10);
    canvas_draw_str(canvas, 40, 8, score_str);

    // Draw game over message if necessary
    if(player_lives <= 0) {
        canvas_draw_str(canvas, 30, 30, "Game Over");
    }
}

static void app_input_callback(InputEvent* input_event, void* ctx) {
    furi_assert(ctx);

    FuriMessageQueue* event_queue = ctx;
    furi_message_queue_put(event_queue, input_event, FuriWaitForever);

    if(input_event->type == InputTypePress && input_event->key == InputKeyBack) {
        running = false;
    }
}

void shoot() {
    for(int i = 0; i < MAX_BULLETS; ++i) {
        if(!bullet_active[i]) {
            bullet_x[i] = player_x + PLAYER_WIDTH / 2;
            bullet_y[i] = player_y + PLAYER_HEIGHT / 2;
            bullet_active[i] = true;
            break;
        }
    }
}

void update_bonus_life() {
    if(!bonus_life_active) {
        if(rand() % 500 == 0) {
            bonus_life_x = rand() % (SCREEN_WIDTH - 7);
            bonus_life_y = rand() % (SCREEN_HEIGHT - 6);
            bonus_life_active = true;
            bonus_life_timer = 0;
        }
    } else {
        bonus_life_timer++;
        if(bonus_life_timer >= BONUS_LIFE_DURATION_SECONDS * 60) {
            bonus_life_active = false;
        }
    }
}

void check_bonus_life_collision() {
    if(bonus_life_active) {
        if((player_x + PLAYER_WIDTH >= bonus_life_x && player_x <= bonus_life_x + 6) &&
           (player_y + PLAYER_HEIGHT >= bonus_life_y && player_y <= bonus_life_y + 6)) {
            if(player_lives < MAX_LIVES) {
                player_lives++;
            } else {
                score += 10;
            }
            bonus_life_active = false;
        }
    }
}

void check_shield_powerup() {
    if(!shield_active && rand() % 1000 == 0) {
        shield_active = true;
    }
}

int32_t flight_assault(void* p) {
    UNUSED(p);
    FuriMessageQueue* event_queue = furi_message_queue_alloc(8, sizeof(InputEvent));

    ViewPort* view_port = view_port_alloc();
    view_port_draw_callback_set(view_port, app_draw_callback, view_port);
    view_port_input_callback_set(view_port, app_input_callback, event_queue);

    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    InputEvent event;
    srand(furi_get_tick());

    // for enemy
    for(int i = 0; i < enemy_count; ++i) {
        enemy_x[i] = SCREEN_WIDTH + rand() % SCREEN_WIDTH;
        enemy_y[i] = rand() % (SCREEN_HEIGHT - 6);
        enemy_speed[i] = rand() % 1 + 1;
    }

    while(running) {
        FuriStatus status = furi_message_queue_get(event_queue, &event, 50);
        if(status == FuriStatusOk) {
            if(event.type == InputTypePress || event.type == InputTypeRepeat) {
                if(event.key == InputKeyUp && player_y > 0) player_y -= PLAYER_SPEED;
                if(event.key == InputKeyDown && player_y + PLAYER_HEIGHT < SCREEN_HEIGHT)
                    player_y += PLAYER_SPEED;
                if(event.key == InputKeyRight && player_x + PLAYER_WIDTH < SCREEN_WIDTH)
                    player_x += PLAYER_SPEED;
                if(event.key == InputKeyLeft && player_x > 0) player_x -= PLAYER_SPEED;
                if(event.key == InputKeyOk) shoot();
            }
        }

        // bullet positions
        for(int i = 0; i < MAX_BULLETS; ++i) {
            if(bullet_active[i]) {
                bullet_x[i] += BULLET_SPEED;
                if(bullet_x[i] > SCREEN_WIDTH) {
                    bullet_active[i] = false;
                }
            }
        }

        // wenemies
        for(int i = 0; i < enemy_count; ++i) {
            enemy_x[i] -= enemy_speed[i];
            if(enemy_x[i] < 0) {
                enemy_x[i] = SCREEN_WIDTH;
                enemy_y[i] = rand() % (SCREEN_HEIGHT - 6);
                enemy_speed[i] = rand() % 1 + 1;
            }
        }

        // check collisions
        for(int i = 0; i < MAX_BULLETS; ++i) {
            if(bullet_active[i]) {
                for(int j = 0; j < enemy_count; ++j) {
                    if((bullet_x[i] + BULLET_WIDTH >= enemy_x[j] &&
                        bullet_x[i] <= enemy_x[j] + 9) &&
                       (bullet_y[i] + BULLET_HEIGHT >= enemy_y[j] &&
                        bullet_y[i] <= enemy_y[j] + 8)) {
                        bullet_active[i] = false;
                        score++;
                        enemy_x[j] = SCREEN_WIDTH;
                        enemy_y[j] = rand() % (SCREEN_HEIGHT - 6);
                        enemy_speed[j] = rand() % 1 + 1;
                    }
                }
            }
        }

        // check collisions with player
        for(int i = 0; i < enemy_count; ++i) {
            if((player_x + PLAYER_HITBOX_WIDTH >= enemy_x[i] && player_x <= enemy_x[i] + 9) &&
               (player_y + PLAYER_HITBOX_HEIGHT >= enemy_y[i] && player_y <= enemy_y[i] + 8)) {
                if(shield_active) {
                    shield_active = false;
                } else {
                    player_lives--;
                    if(player_lives <= 0) {
                        running = false;
                    }
                }
                enemy_x[i] = SCREEN_WIDTH;
                enemy_y[i] = rand() % (SCREEN_HEIGHT - 6);
            }
        }

        // bonus life
        update_bonus_life();
        check_bonus_life_collision();

        // Check if shield should be activated
        check_shield_powerup();

        // Render screen
        view_port_update(view_port);
    }

    // Clean up
    gui_remove_view_port(gui, view_port);
    view_port_free(view_port);
    furi_record_close(RECORD_GUI);
    furi_message_queue_free(event_queue);

    return 0;
}

int32_t flight_assault_main(void* p) {
    return flight_assault(p);
}
