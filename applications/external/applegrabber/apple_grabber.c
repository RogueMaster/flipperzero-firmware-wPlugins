#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <input/input.h>
#include <stdlib.h>

#include <apple_grabber_icons.h>

#define CATCH        261.63f //C middle note from Piano
#define PROJECTILES  4 //Number of apples per round
#define GAME_SPEED   1 //Speed of the apples
#define PLAYER_SPEED 1 //Speed of the apples

// Screensize 128 p x 64 p

void draw_cb(Canvas* canvas, void* ap_pointer);
void input_cb(InputEvent* input, void* ap_pointer);
void timer_cb(void* ap_pointer);

enum FinalScore {
    LOSE,
    WIN,
    EXIT
};

typedef struct {
    int x;
    int y;
} Entity;

typedef struct {
    Entity coordinate;
    bool playing;
} Projectile;

typedef struct {
    int game_speed;
    int player_speed;
    bool playing;
    enum FinalScore score;
    int catch;
    Projectile* apples[PROJECTILES];
    FuriMutex* model_mutex;
    FuriMessageQueue* event_queue;
    ViewPort* view_port;
    Gui* gui;
    FuriTimer* timer;
    Entity* player;
} AppleGame;

void free_apple_game(AppleGame* AP) {
    furi_timer_stop(AP->timer);
    furi_timer_free(AP->timer);

    view_port_enabled_set(AP->view_port, false);
    gui_remove_view_port(AP->gui, AP->view_port);
    furi_record_close(RECORD_GUI);
    view_port_free(AP->view_port);
    furi_message_queue_free(AP->event_queue);

    furi_mutex_free(AP->model_mutex);

    if(furi_hal_speaker_is_mine()) {
        furi_hal_speaker_stop();
        furi_hal_speaker_release();
    }

    for(int i = 0; i < PROJECTILES; i++) {
        free(AP->apples[i]);
    }

    free(AP->player);
    free(AP);
}

AppleGame* apple_allocation() {
    AppleGame* AP = malloc(sizeof(AppleGame));

    AP->model_mutex = furi_mutex_alloc(FuriMutexTypeNormal);

    AP->event_queue = furi_message_queue_alloc(8, sizeof(InputEvent));

    AP->view_port = view_port_alloc();
    view_port_set_orientation(AP->view_port, ViewPortOrientationVertical);
    view_port_draw_callback_set(AP->view_port, draw_cb, AP);
    view_port_input_callback_set(AP->view_port, input_cb, AP);

    AP->timer = furi_timer_alloc(timer_cb, FuriTimerTypePeriodic, AP);
    furi_timer_start(AP->timer, furi_kernel_get_tick_frequency() / 4);

    AP->gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(AP->gui, AP->view_port, GuiLayerFullscreen);

    AP->game_speed = GAME_SPEED;
    AP->player_speed = PLAYER_SPEED;
    AP->score = EXIT;
    AP->catch = 0;
    AP->playing = false;

    AP->player = malloc(sizeof(Entity));

    AP->player->x = 113;
    AP->player->y = 32;

    for(int i = 0; i < PROJECTILES; i++) {
        AP->apples[i] = malloc(sizeof(Projectile));
        AP->apples[i]->coordinate.y = rand() % 59;
        AP->apples[i]->coordinate.x = -rand() % 128;
        AP->apples[i]->playing = true;
    }

    return AP;
}

void draw_cb(Canvas* canvas, void* ap_pointer) {
    AppleGame* AP = ap_pointer;
    furi_check(furi_mutex_acquire(AP->model_mutex, FuriWaitForever) == FuriStatusOk);

    Icon* player = (Icon*)&I_player; //Player Icon definition 10 x 15
    Icon* apple = (Icon*)&I_apple; //Apple Icon definition 5 x 6

    if(AP->playing == false) {
        canvas_draw_frame(canvas, 0, 0, 64, 128);
        canvas_draw_str_aligned(canvas, 32, 10, AlignCenter, AlignCenter, "AppleGame");
        canvas_draw_str_aligned(canvas, 32, 30, AlignCenter, AlignCenter, "Press OK");
        canvas_draw_str_aligned(canvas, 32, 40, AlignCenter, AlignCenter, "to start");
        canvas_draw_str_aligned(canvas, 32, 60, AlignCenter, AlignCenter, "Use arrows");
        canvas_draw_str_aligned(canvas, 32, 70, AlignCenter, AlignCenter, "to move");
    } else if(AP->score != EXIT) {
        canvas_draw_frame(canvas, 0, 0, 64, 128);
        if(AP->score == WIN) {
            canvas_draw_str_aligned(canvas, 32, 10, AlignCenter, AlignCenter, "You Won!!");
        } else {
            canvas_draw_str_aligned(canvas, 32, 10, AlignCenter, AlignCenter, "You Lost :(");
        }
        canvas_draw_str_aligned(canvas, 32, 30, AlignCenter, AlignCenter, "Press OK");
        canvas_draw_str_aligned(canvas, 32, 40, AlignCenter, AlignCenter, "to retry");
        canvas_draw_str_aligned(canvas, 32, 60, AlignCenter, AlignCenter, "Press return");
        canvas_draw_str_aligned(canvas, 32, 70, AlignCenter, AlignCenter, "to exit");
    } else {
        canvas_draw_icon(canvas, AP->player->y, AP->player->x, player);
        for(int i = 0; i < PROJECTILES; i++) {
            if(AP->apples[i]->playing == true) {
                canvas_draw_icon(
                    canvas, AP->apples[i]->coordinate.y, AP->apples[i]->coordinate.x, apple);
            }
        }
    }

    furi_mutex_release(AP->model_mutex);
}

//For input callbacks
void input_cb(InputEvent* input, void* ap_pointer) {
    AppleGame* AP = ap_pointer;
    if(AP && AP->event_queue) {
        furi_message_queue_put(AP->event_queue, input, 0);
    }
}

//For apple ticks
void timer_cb(void* ap_pointer) {
    AppleGame* AP = ap_pointer;
    if(AP && AP->event_queue) {
        InputEvent input;
        input.type = InputTypeMAX;
        input.key = 0;
        furi_message_queue_put(AP->event_queue, &input, 0);
    }
}

int32_t apple_grabber_app(void* p) {
    UNUSED(p);

    InputEvent event;

    AppleGame* AP = apple_allocation();
    bool game_stop = false;

    while(game_stop == false) {
        //Playing phase
        AP->playing = false;
        bool running = true;
        while(running) {
            FuriStatus status = furi_message_queue_get(AP->event_queue, &event, FuriWaitForever);
            furi_check(furi_mutex_acquire(AP->model_mutex, FuriWaitForever) == FuriStatusOk);
            float volume = 1.0f;

            if(status == FuriStatusOk) {
                if(event.type == InputTypePress) {
                    switch(event.key) {
                    case InputKeyUp:
                        break;
                    case InputKeyDown:
                        break;
                    case InputKeyRight:
                        AP->player->y += 8;
                        break;
                    case InputKeyLeft:
                        AP->player->y -= 8;
                        break;
                    case InputKeyOk:
                        if(AP->playing == false) {
                            AP->playing = true;
                        }
                        break;
                    case InputKeyBack:
                        running = false;
                        game_stop = true;
                        break;
                    default:
                        break;
                    }
                } else if(event.type == InputTypeRelease) {
                } else if(event.type == InputTypeMAX && AP->playing) {
                    if(furi_hal_speaker_is_mine()) {
                        furi_hal_speaker_stop();
                        furi_hal_speaker_release();
                    }
                    bool apples_remaining = false;
                    for(int i = 0; i < PROJECTILES; i++) {
                        if(AP->apples[i]->playing) {
                            if(AP->apples[i]->coordinate.x > 113 &&
                               AP->apples[i]->coordinate.y >= AP->player->y &&
                               AP->apples[i]->coordinate.y <= (AP->player->y + 10)) {
                                AP->apples[i]->playing = false;
                                if(furi_hal_speaker_is_mine() || furi_hal_speaker_acquire(1000)) {
                                    furi_hal_speaker_start(CATCH, volume);
                                }
                            }
                            if(AP->apples[i]->coordinate.x > 128) {
                                AP->score = LOSE;
                                running = false;
                            } else {
                                apples_remaining = true;
                                AP->apples[i]->coordinate.x += AP->game_speed;
                            }
                        }
                    }

                    // If apples are caught, place them again
                    if(apples_remaining == false) {
                        for(int i = 0; i < PROJECTILES; i++) {
                            AP->apples[i]->coordinate.y = rand() % 59;
                            AP->apples[i]->coordinate.x = -32 - rand() % 128;
                            AP->apples[i]->playing = true;
                        }
                        AP->game_speed += 2;
                    }
                }
            }
            if(AP->player->y > 54) {
                AP->player->y = 54;
            } else if(AP->player->y < 0) {
                AP->player->y = 0;
            }
            if(AP->game_speed > 7) {
                AP->score = WIN;
                running = false;
            }

            furi_mutex_release(AP->model_mutex);
            view_port_update(AP->view_port);
        }

        //End result phase
        while(AP->score != EXIT) {
            if(furi_hal_speaker_is_mine()) {
                furi_hal_speaker_stop();
                furi_hal_speaker_release();
            }
            FuriStatus status = furi_message_queue_get(AP->event_queue, &event, FuriWaitForever);
            furi_check(furi_mutex_acquire(AP->model_mutex, FuriWaitForever) == FuriStatusOk);
            if(status == FuriStatusOk) {
                if(event.type == InputTypePress) {
                    switch(event.key) {
                    case InputKeyOk:
                        AP->score = EXIT;
                        for(int i = 0; i < PROJECTILES; i++) {
                            AP->apples[i]->coordinate.y = rand() % 59;
                            AP->apples[i]->coordinate.x = -32 - rand() % 128;
                            AP->apples[i]->playing = true;
                        }
                        AP->game_speed = GAME_SPEED;
                        break;
                    case InputKeyBack:
                        AP->score = EXIT;
                        game_stop = true;
                        break;
                    default:
                        break;
                    }
                }
            }
            furi_mutex_release(AP->model_mutex);
            view_port_update(AP->view_port);
        }
    }

    free_apple_game(AP);

    return 0;
}
