#include "dolphin/dolphin.h"
#include <furi.h>
#include <furi_hal_rtc.h>
#include <gui/gui.h>
#include <stdlib.h>

int SCORE = 0;
int SCORE_BONUS = 1;
int HIGH_SCORE = 10;
char score_str[16];

int player_x = 6;
int player_y = 28;
int kelp_x = 124;
int kelp_y = 56;
int jellyfish_x = 64;
int jellyfish_y = 0;

bool is_jumping = false;
bool is_random_kelp = true;
bool is_random_jellyfish = true;
int kelp_x_rand;
int kelp_y_rand;
int jellyfish_x_rand;
int jellyfish_y_rand;

// specials
int max_gravity = 1;
int max_jump = 2;

// player coordinates for drawing
int players[][2] = {
    {7, 4},
    {8, 4},
    {3, 5},
    {4, 5},
    {6, 5},
    {9, 5},
    {3, 6},
    {5, 6},
    {10, 6},
    {3, 7},
    {5, 7},
    {10, 7},
    {3, 8},
    {4, 8},
    {6, 8},
    {9, 8},
    {7, 9},
    {8, 9}};
int kelp[][2] =
    {{2, 2}, {4, 2}, {3, 3}, {2, 4}, {4, 4}, {3, 5}, {2, 6}, {4, 6}, {3, 7}, {2, 8}, {4, 8}, {3, 9}};
int jellyfish[][2] = {{3, 2}, {4, 2}, {5, 2}, {6, 2}, {7, 2}, {8, 2}, {2, 3}, {9, 3}, {2, 4},
                      {9, 4}, {3, 5}, {4, 5}, {5, 5}, {6, 5}, {7, 5}, {8, 5}, {3, 6}, {6, 6},
                      {8, 6}, {4, 7}, {6, 7}, {9, 7}, {2, 8}, {4, 8}, {7, 8}, {3, 9}};

void collide_rect() {
    int player_left = player_x;
    int player_top = player_y - 1;
    int player_right = player_x + 8;
    int player_bottom = player_y + 7;

    int kelp_left = kelp_x - kelp_x_rand * 8 + 4;
    int kelp_top = kelp_y - kelp_y_rand * 8 - 10;
    int kelp_right = kelp_x;
    int kelp_bottom = kelp_y + 10;

    int jellyfish_left = jellyfish_x - jellyfish_x_rand * 8;
    int jellyfish_top = jellyfish_y + 10;
    int jellyfish_right = jellyfish_x;
    int jellyfish_bottom = jellyfish_y + jellyfish_y_rand * 8 + 10;

    bool kelp_collision = player_left <= kelp_right && player_right >= kelp_left &&
                          player_top <= kelp_bottom && player_bottom >= kelp_top;

    bool jellyfish_collision = player_left <= jellyfish_right && player_right >= jellyfish_left &&
                               player_top <= jellyfish_bottom && player_bottom >= jellyfish_top;

    if(kelp_collision || jellyfish_collision || player_bottom >= 64 || player_top <= 0) {
        kelp_x = 124;
        is_random_kelp = true;

        jellyfish_x = 64;
        is_random_jellyfish = true;

        player_x = 6;
        player_y = 28;

        SCORE_BONUS = 1;
        HIGH_SCORE = 10;

        SCORE = 0;
    }
}

void draw_player(Canvas* canvas) {
    if(is_jumping) {
        player_y -= max_jump;
    }

    else {
        player_y += max_gravity;
    }

    int array_size = sizeof(players) / sizeof(players[0]);
    for(int i = 0; i < array_size; i++) {
        int x = players[i][0];
        int y = players[i][1];
        if(x != 0 && y != 0) {
            canvas_draw_dot(canvas, x + player_x, y + player_y);
        }
    }
}

void draw_kelp(Canvas* canvas) {
    if(is_random_kelp) {
        kelp_x_rand = rand() % 2 + 1;
        kelp_y_rand = rand() % 2 + 1;
        is_random_kelp = false;
    }

    int array_size = sizeof(kelp) / sizeof(kelp[0]);
    for(int a = 1; a < kelp_y_rand + 2; a++) {
        for(int b = 1; b < kelp_x_rand + 2; b++) {
            for(int i = 0; i < array_size; i++) {
                int x = kelp[i][0];
                int y = kelp[i][1];
                if(x != 0 && y != 0) {
                    canvas_draw_dot(canvas, (x + kelp_x) - (b * 4), y + kelp_y - a * 8);
                }
            }
        }
    }

    kelp_x -= SCORE_BONUS + 1;

    if(kelp_x <= -8) {
        kelp_x = 124;
        is_random_kelp = true;
        SCORE += 1;
    }
}

void draw_jellyfish(Canvas* canvas) {
    if(is_random_jellyfish) {
        jellyfish_x_rand = rand() % 2 + 1;
        jellyfish_y_rand = rand() % 2 + 1;
        is_random_jellyfish = false;
    }

    int array_size = sizeof(jellyfish) / sizeof(jellyfish[0]);
    for(int a = 1; a < jellyfish_y_rand + 2; a++) {
        for(int b = 1; b < jellyfish_x_rand + 2; b++) {
            for(int i = 0; i < array_size; i++) {
                int x = jellyfish[i][0];
                int y = jellyfish[i][1];
                if(x != 0 && y != 0) {
                    canvas_draw_dot(canvas, (x + jellyfish_x) - (b * 8), y + jellyfish_y + a * 8);
                }
            }
        }
    }

    jellyfish_x -= SCORE_BONUS + 1;

    if(jellyfish_x <= -8) {
        jellyfish_x = 124;
        is_random_jellyfish = true;
        SCORE += 1;
    }
}

static void input_callback(InputEvent* event, void* context) {
    FuriMessageQueue* queue = (FuriMessageQueue*)context;
    if(event->type == InputTypeShort || event->type == InputTypeRepeat ||
       event->type == InputTypePress) {
        if(event->key == InputKeyOk) {
            is_jumping = true;
        }
    }

    if(event->type == InputTypeRelease) {
        if(event->key == InputKeyOk) {
            is_jumping = false;
        }
    }

    furi_message_queue_put(queue, event, FuriWaitForever);
}

static void draw_callback(Canvas* canvas, void* context) {
    UNUSED(context);
    canvas_clear(canvas);
    collide_rect();
    draw_player(canvas);
    draw_kelp(canvas);
    draw_jellyfish(canvas);

    snprintf(score_str, sizeof(score_str), "%d", SCORE);
    canvas_draw_str(canvas, 2, 8, score_str);

    snprintf(score_str, sizeof(score_str), "%dx", SCORE_BONUS);
    canvas_draw_str(canvas, 2, 64, score_str);

    if(SCORE >= HIGH_SCORE) {
        HIGH_SCORE += 10;
        SCORE_BONUS += 1;
    }

    canvas_commit(canvas);
}

int main() {
    DateTime dt;
    furi_hal_rtc_get_datetime(&dt);
    unsigned int seed = dt.hour * 3600 + dt.minute * 60 + dt.second;
    srand(seed);

    kelp_x_rand = rand() % 2 + 1;
    kelp_y_rand = rand() % 2 + 1;
    jellyfish_x_rand = rand() % 2 + 1;
    jellyfish_y_rand = rand() % 2 + 1;

    FuriMessageQueue* queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    ViewPort* view_port = view_port_alloc();
    view_port_draw_callback_set(view_port, draw_callback, NULL);
    view_port_input_callback_set(view_port, input_callback, queue);
    Gui* gui = (Gui*)furi_record_open("gui");
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);
    dolphin_deed(DolphinDeedPluginGameStart);
    InputEvent event;
    bool running = true;
    while(running) {
        if(furi_message_queue_get(queue, &event, FuriWaitForever) == FuriStatusOk) {
            if(event.type == InputTypeShort && event.key == InputKeyBack) {
                running = false;
            }
        }
        view_port_update(view_port);
    }

    view_port_enabled_set(view_port, false);
    furi_message_queue_free(queue);
    gui_remove_view_port(gui, view_port);
    view_port_free(view_port);
    furi_record_close(RECORD_GUI);
    return 0;
}
