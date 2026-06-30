#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <input/input.h>
#include <stdlib.h>
#include <dolphin/dolphin.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>
#include <storage/storage.h>

typedef struct {
    //    +-----x
    //    |
    //    |
    //    y
    uint8_t x;
    uint8_t y;
} Point;

typedef enum {
    GameStateLife,
    GameStatePause,
    GameStateLastChance,
    GameStateGameOver,
} GameState;

typedef enum {
    DirectionUp,
    DirectionRight,
    DirectionDown,
    DirectionLeft,
} Direction;

#define MAX_SNAKE_LEN 15 * 31 //128 * 64 / 4 - 1px border line

#define x_back_symbol 50
#define y_back_symbol 9

#define x_arrow_left 81
#define y_arrow_left 20

#define x_arrow_right 104
#define y_arrow_right 20

#define SAVING_FILENAME APP_DATA_PATH("snake2.save")

typedef struct {
    FuriMutex* mutex;
    Point points[MAX_SNAKE_LEN];
    uint16_t len;
    Direction currentMovement;
    Direction nextMovement; // if backward of currentMovement, ignore
    Point fruit;
    GameState state;
    bool Endlessmode;
    uint32_t timer_start_timestamp;
    uint32_t timer_stopped_seconds;
} SnakeState;

typedef enum {
    EventTypeTick,
    EventTypeKey,
} EventType;

typedef struct {
    EventType type;
    InputEvent input;
} SnakeEvent;

const NotificationSequence sequence_fail = {
    &message_vibro_on,

    &message_note_ds4,
    &message_delay_10,
    &message_sound_off,
    &message_delay_10,

    &message_note_ds4,
    &message_delay_10,
    &message_sound_off,
    &message_delay_10,

    &message_note_ds4,
    &message_delay_10,
    &message_sound_off,
    &message_delay_10,

    &message_vibro_off,
    NULL,
};

const NotificationSequence sequence_eat = {

    &message_vibro_on,
    &message_note_c7,
    &message_delay_50,
    &message_sound_off,
    &message_vibro_off,
    NULL,
};

static void snake_game_render_callback(Canvas* const canvas, void* ctx) {
    furi_assert(ctx);
    const SnakeState* snake_state = ctx;
    furi_mutex_acquire(snake_state->mutex, FuriWaitForever);

    // Before the function is called, the state is set with the canvas_reset(canvas)

    // Frame
    canvas_draw_frame(canvas, 0, 0, 128, 64);

    // Fruit
    Point f = snake_state->fruit;
    f.x = f.x * 4 + 1;
    f.y = f.y * 4 + 1;
    canvas_draw_rframe(canvas, f.x, f.y, 6, 6, 2);
    canvas_draw_dot(canvas, f.x + 3, f.y - 1);
    canvas_draw_dot(canvas, f.x + 4, f.y - 2);

    //Dot in the middle of an apple (just to know if the Endless mode is turn off)
    if(!snake_state->Endlessmode) {
        canvas_draw_dot(canvas, f.x + 2, f.y + 2);
        canvas_draw_dot(canvas, f.x + 2, f.y + 3);
        canvas_draw_dot(canvas, f.x + 3, f.y + 2);
        canvas_draw_dot(canvas, f.x + 3, f.y + 3);
    }

    // Snake
    for(uint16_t i = 0; i < snake_state->len; i++) {
        Point p = snake_state->points[i];
        p.x = p.x * 4 + 2;
        p.y = p.y * 4 + 2;
        canvas_draw_box(canvas, p.x, p.y, 4, 4);
        if(i == 0) {
            canvas_set_color(canvas, ColorWhite);
            canvas_draw_box(canvas, p.x + 1, p.y + 1, 2, 2);
            canvas_set_color(canvas, ColorBlack);
        }
    }

    // Pause and GameOver banner
    if(snake_state->state == GameStatePause || snake_state->state == GameStateGameOver) {
        // Screen is 128x64 px
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_box(canvas, 33, 23, 64, 26);

        canvas_set_color(canvas, ColorBlack);
        canvas_draw_frame(canvas, 34, 24, 62, 24);

        canvas_set_font(canvas, FontPrimary);
        if(snake_state->state == GameStateGameOver) {
            if(snake_state->len >= MAX_SNAKE_LEN - 1) {
                canvas_draw_str_aligned(canvas, 65, 35, AlignCenter, AlignBottom, "You WON!");
            } else {
                canvas_draw_str_aligned(canvas, 65, 35, AlignCenter, AlignBottom, "Game Over");
            }
        }
        if(snake_state->state == GameStatePause) {
            canvas_draw_str_aligned(canvas, 65, 35, AlignCenter, AlignBottom, "Pause");
        }

        canvas_set_font(canvas, FontSecondary);
        char buffer[40];
        snprintf(buffer, sizeof(buffer), "Score: %u", snake_state->len - 7U);
        canvas_draw_str_aligned(canvas, 65, 45, AlignCenter, AlignBottom, buffer);

        // Painting "back"-symbol, Help message for Exit App, ProgressBar (Complete %)
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_box(canvas, 22, 1, 87, 22);
        canvas_draw_box(canvas, 24, 54, 83, 9);
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_str_aligned(
            canvas, 65, 10, AlignCenter, AlignBottom, "Hold        to Exit App");
        //Endless mode ON/OFF
        if(snake_state->Endlessmode == false) {
            canvas_draw_str_aligned(canvas, 24, 21, AlignLeft, AlignBottom, "Endless mode   OFF");
        } else {
            canvas_draw_str_aligned(canvas, 24, 21, AlignLeft, AlignBottom, "Endless mode");
            canvas_draw_str_aligned(canvas, 89, 21, AlignLeft, AlignBottom, "ON");
        }

        snprintf(
            buffer,
            sizeof(buffer),
            "%-5.1f%% (%.2ld:%.2ld:%.2ld)",
            (double)((snake_state->len - 7U) / 4.57),
            snake_state->timer_stopped_seconds / 60 / 60,
            snake_state->timer_stopped_seconds / 60 % 60,
            snake_state->timer_stopped_seconds % 60);

        //Back symbol
        {
            canvas_draw_dot(canvas, x_back_symbol + 0, y_back_symbol);
            canvas_draw_dot(canvas, x_back_symbol + 1, y_back_symbol);
            canvas_draw_dot(canvas, x_back_symbol + 2, y_back_symbol);
            canvas_draw_dot(canvas, x_back_symbol + 3, y_back_symbol);
            canvas_draw_dot(canvas, x_back_symbol + 4, y_back_symbol);
            canvas_draw_dot(canvas, x_back_symbol + 5, y_back_symbol - 1);
            canvas_draw_dot(canvas, x_back_symbol + 6, y_back_symbol - 2);
            canvas_draw_dot(canvas, x_back_symbol + 6, y_back_symbol - 3);
            canvas_draw_dot(canvas, x_back_symbol + 5, y_back_symbol - 4);
            canvas_draw_dot(canvas, x_back_symbol + 4, y_back_symbol - 5);
            canvas_draw_dot(canvas, x_back_symbol + 3, y_back_symbol - 5);
            canvas_draw_dot(canvas, x_back_symbol + 2, y_back_symbol - 5);
            canvas_draw_dot(canvas, x_back_symbol + 1, y_back_symbol - 5);
            canvas_draw_dot(canvas, x_back_symbol + 0, y_back_symbol - 5);
            canvas_draw_dot(canvas, x_back_symbol - 1, y_back_symbol - 5);
            canvas_draw_dot(canvas, x_back_symbol - 2, y_back_symbol - 5);
            canvas_draw_dot(canvas, x_back_symbol - 3, y_back_symbol - 5);
            canvas_draw_dot(canvas, x_back_symbol - 2, y_back_symbol - 6);
            canvas_draw_dot(canvas, x_back_symbol - 2, y_back_symbol - 4);
            canvas_draw_dot(canvas, x_back_symbol - 1, y_back_symbol - 6);
            canvas_draw_dot(canvas, x_back_symbol - 1, y_back_symbol - 4);
            canvas_draw_dot(canvas, x_back_symbol - 1, y_back_symbol - 7);
            canvas_draw_dot(canvas, x_back_symbol - 1, y_back_symbol - 3);
        }
        //Left Arrow
        canvas_draw_str_aligned(canvas, 65, 62, AlignCenter, AlignBottom, buffer);
        {
            canvas_draw_dot(canvas, x_arrow_left + 0, y_arrow_left - 3);
            canvas_draw_dot(canvas, x_arrow_left + 1, y_arrow_left - 2);
            canvas_draw_dot(canvas, x_arrow_left + 1, y_arrow_left - 3);
            canvas_draw_dot(canvas, x_arrow_left + 1, y_arrow_left - 4);
            canvas_draw_dot(canvas, x_arrow_left + 2, y_arrow_left - 1);
            canvas_draw_dot(canvas, x_arrow_left + 2, y_arrow_left - 2);
            canvas_draw_dot(canvas, x_arrow_left + 2, y_arrow_left - 3);
            canvas_draw_dot(canvas, x_arrow_left + 2, y_arrow_left - 4);
            canvas_draw_dot(canvas, x_arrow_left + 2, y_arrow_left - 5);
            canvas_draw_dot(canvas, x_arrow_left + 3, y_arrow_left - 0);
            canvas_draw_dot(canvas, x_arrow_left + 3, y_arrow_left - 1);
            canvas_draw_dot(canvas, x_arrow_left + 3, y_arrow_left - 2);
            canvas_draw_dot(canvas, x_arrow_left + 3, y_arrow_left - 3);
            canvas_draw_dot(canvas, x_arrow_left + 3, y_arrow_left - 4);
            canvas_draw_dot(canvas, x_arrow_left + 3, y_arrow_left - 5);
            canvas_draw_dot(canvas, x_arrow_left + 3, y_arrow_left - 6);
        }

        //Right Arrow
        canvas_draw_str_aligned(canvas, 65, 62, AlignCenter, AlignBottom, buffer);
        {
            canvas_draw_dot(canvas, x_arrow_right + 0, y_arrow_right - 0);
            canvas_draw_dot(canvas, x_arrow_right + 0, y_arrow_right - 1);
            canvas_draw_dot(canvas, x_arrow_right + 0, y_arrow_right - 2);
            canvas_draw_dot(canvas, x_arrow_right + 0, y_arrow_right - 3);
            canvas_draw_dot(canvas, x_arrow_right + 0, y_arrow_right - 4);
            canvas_draw_dot(canvas, x_arrow_right + 0, y_arrow_right - 5);
            canvas_draw_dot(canvas, x_arrow_right + 0, y_arrow_right - 6);
            canvas_draw_dot(canvas, x_arrow_right + 1, y_arrow_right - 1);
            canvas_draw_dot(canvas, x_arrow_right + 1, y_arrow_right - 2);
            canvas_draw_dot(canvas, x_arrow_right + 1, y_arrow_right - 3);
            canvas_draw_dot(canvas, x_arrow_right + 1, y_arrow_right - 4);
            canvas_draw_dot(canvas, x_arrow_right + 1, y_arrow_right - 5);
            canvas_draw_dot(canvas, x_arrow_right + 2, y_arrow_right - 2);
            canvas_draw_dot(canvas, x_arrow_right + 2, y_arrow_right - 3);
            canvas_draw_dot(canvas, x_arrow_right + 2, y_arrow_right - 4);
            canvas_draw_dot(canvas, x_arrow_right + 3, y_arrow_right - 3);
        }
    }

    furi_mutex_release(snake_state->mutex);
}

bool load_game(SnakeState* snake_state) {
    Storage* storage = furi_record_open(RECORD_STORAGE);

    File* file = storage_file_alloc(storage);
    uint16_t bytes_readed = 0;
    if(storage_file_open(file, SAVING_FILENAME, FSAM_READ, FSOM_OPEN_EXISTING)) {
        bytes_readed = storage_file_read(file, snake_state, sizeof(SnakeState));
    }
    storage_file_close(file);
    storage_file_free(file);

    furi_record_close(RECORD_STORAGE);

    return bytes_readed == sizeof(SnakeState);
}

void save_game(const SnakeState* snake_state) {
    Storage* storage = furi_record_open(RECORD_STORAGE);

    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, SAVING_FILENAME, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_write(file, snake_state, sizeof(SnakeState));
    }
    storage_file_close(file);
    storage_file_free(file);

    furi_record_close(RECORD_STORAGE);
}

static void snake_game_input_callback(InputEvent* input_event, void* ctx) {
    furi_assert(ctx);
    FuriMessageQueue* event_queue = ctx;

    SnakeEvent event = {.type = EventTypeKey, .input = *input_event};
    furi_message_queue_put(event_queue, &event, FuriWaitForever);
}

static void snake_game_update_timer_callback(void* ctx) {
    furi_assert(ctx);
    FuriMessageQueue* event_queue = ctx;

    SnakeEvent event = {.type = EventTypeTick};
    furi_message_queue_put(event_queue, &event, 0);
}

static void snake_game_init_game(SnakeState* const snake_state) {
    Point p[] = {{8, 6}, {7, 6}, {6, 6}, {5, 6}, {4, 6}, {3, 6}, {2, 6}};
    memcpy(snake_state->points, p, sizeof(p)); //-V1086

    snake_state->len = 7;

    snake_state->currentMovement = DirectionRight;

    snake_state->nextMovement = DirectionRight;

    Point f = {18, 6};
    snake_state->fruit = f;

    DateTime curr_dt;
    furi_hal_rtc_get_datetime(&curr_dt);
    uint32_t curr_ts = datetime_datetime_to_timestamp(&curr_dt);

    snake_state->timer_stopped_seconds = 0;
    snake_state->timer_start_timestamp = curr_ts;

    snake_state->state = GameStateLife;

    save_game(snake_state);
}

static Point snake_game_get_new_fruit(SnakeState const* const snake_state) {
    // Max number of fruits on x axis = (16 * 2) - 1 = 31 (0<=x=>30)
    // Max number of fruits on y axis = (8 * 2)  - 1 = 15 (0<=y=>14)
    // Total fields for fruits and snake body = 31 * 15 = 465
    // Empty fields for next random fruit = 465 - len(snake)

    bool* all_fields;
    int* empty_fields;
    all_fields = (bool*)malloc(MAX_SNAKE_LEN * sizeof(bool));
    empty_fields = (int*)malloc(MAX_SNAKE_LEN * sizeof(int));

    for(uint16_t j = 0; j < MAX_SNAKE_LEN; j++) {
        all_fields[j] = false;
    }

    for(uint16_t j = 0; j < snake_state->len; j++) {
        Point p = snake_state->points[j];
        all_fields[p.x + 31 * p.y] = true;
    }

    int empty_counter = 0;
    for(uint16_t j = 0; j < MAX_SNAKE_LEN; j++) {
        if(!all_fields[j]) {
            empty_fields[empty_counter] = j;
            empty_counter++;
        }
    }

    int newFruit = rand() % empty_counter;

    Point p = {
        .x = empty_fields[newFruit] % 31,
        .y = empty_fields[newFruit] / 31,
    };

    free(all_fields);
    free(empty_fields);

    return p;
}

static bool snake_game_collision_with_frame(Point const next_step) {
    // if x == 0 && currentMovement == left then x - 1 == 255 ,
    // so check only x > right border
    return next_step.x > 30 || next_step.y > 14;
}

static bool
    snake_game_collision_with_tail(SnakeState const* const snake_state, Point const next_step) {
    for(uint16_t i = 0; i < snake_state->len; i++) {
        Point p = snake_state->points[i];
        if(p.x == next_step.x && p.y == next_step.y) {
            return true;
        }
    }

    return false;
}

static Direction snake_game_get_turn_snake(SnakeState const* const snake_state) {
    // Sum of two `Direction` lies between 0 and 6, odd values indicate orthogonality.
    bool is_orthogonal = (snake_state->currentMovement + snake_state->nextMovement) % 2 == 1;
    return is_orthogonal ? snake_state->nextMovement : snake_state->currentMovement;
}

static Point snake_game_get_next_step(SnakeState const* const snake_state) {
    Point next_step = snake_state->points[0];
    switch(snake_state->currentMovement) {
    // +-----x
    // |
    // |
    // y
    case DirectionUp:
        next_step.y--;
        break;
    case DirectionRight:
        next_step.x++;
        break;
    case DirectionDown:
        next_step.y++;
        break;
    case DirectionLeft:
        next_step.x--;
        break;
    }
    return next_step;
}

static void snake_game_move_snake(SnakeState* const snake_state, Point const next_step) {
    memmove(snake_state->points + 1, snake_state->points, snake_state->len * sizeof(Point));
    snake_state->points[0] = next_step;
}

static void
    snake_game_process_game_step(SnakeState* const snake_state, NotificationApp* notification) {
    if(snake_state->state == GameStateGameOver) {
        return;
    }

    snake_state->currentMovement = snake_game_get_turn_snake(snake_state);

    Point next_step = snake_game_get_next_step(snake_state);

    bool crush = snake_game_collision_with_frame(next_step);
    if(crush) {
        if(snake_state->state == GameStateLife) {
            snake_state->state = GameStateLastChance;
            return;
        } else if(snake_state->state == GameStateLastChance) {
            if(snake_state->Endlessmode) {
                snake_state->state = GameStateLastChance;
            } else {
                DateTime curr_dt;
                furi_hal_rtc_get_datetime(&curr_dt);
                uint32_t curr_ts = datetime_datetime_to_timestamp(&curr_dt);

                snake_state->timer_stopped_seconds = curr_ts - snake_state->timer_start_timestamp;

                snake_state->state = GameStateGameOver;
            }
            notification_message_block(notification, &sequence_fail);
            return;
        }
    } else {
        if(snake_state->state == GameStateLastChance) {
            snake_state->state = GameStateLife;
        }
    }

    crush = snake_game_collision_with_tail(snake_state, next_step);
    if(crush) {
        if(snake_state->Endlessmode) {
            snake_state->state = GameStateLastChance;
        } else {
            DateTime curr_dt;
            furi_hal_rtc_get_datetime(&curr_dt);
            uint32_t curr_ts = datetime_datetime_to_timestamp(&curr_dt);

            snake_state->timer_stopped_seconds = curr_ts - snake_state->timer_start_timestamp;

            snake_state->state = GameStateGameOver;
        }
        notification_message_block(notification, &sequence_fail);
        return;
    }

    bool eatFruit = (next_step.x == snake_state->fruit.x) && (next_step.y == snake_state->fruit.y);
    if(eatFruit) {
        snake_state->len++;

        if(snake_state->len >= MAX_SNAKE_LEN - 1) {
            //You win!!!
            //It's impossible to collect ALL fruits, because
            //the number of rows is odd (15),
            //the number of columnss is odd too (31).
            //You just can't locate the snake's body
            //on the odd number of cells.
            //Because of it you win when you collect
            //all but one fruits.

            DateTime curr_dt;
            furi_hal_rtc_get_datetime(&curr_dt);
            uint32_t curr_ts = datetime_datetime_to_timestamp(&curr_dt);

            snake_state->timer_stopped_seconds = curr_ts - snake_state->timer_start_timestamp;

            snake_state->state = GameStateGameOver;
            notification_message_block(notification, &sequence_fail);
            return;
        }
    }

    snake_game_move_snake(snake_state, next_step);

    if(eatFruit) {
        snake_state->fruit = snake_game_get_new_fruit(snake_state);
        notification_message(notification, &sequence_eat);
        notification_message(notification, &sequence_blink_red_100);
    }
}

int32_t snake_20_app(void* p) {
    UNUSED(p);

    FuriMessageQueue* event_queue = furi_message_queue_alloc(8, sizeof(SnakeEvent));

    SnakeState* snake_state = malloc(sizeof(SnakeState));
    if(!load_game(snake_state)) {
        snake_game_init_game(snake_state);
    } else {
        DateTime curr_dt;
        furi_hal_rtc_get_datetime(&curr_dt);
        uint32_t curr_ts = datetime_datetime_to_timestamp(&curr_dt);

        snake_state->timer_start_timestamp = curr_ts - snake_state->timer_stopped_seconds;
        snake_state->state = GameStateLife;
    }

    snake_state->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    if(!snake_state->mutex) {
        FURI_LOG_E("SnakeGame", "cannot create mutex\r\n");
        furi_message_queue_free(event_queue);
        free(snake_state);
        return 255;
    }

    ViewPort* view_port = view_port_alloc();
    view_port_draw_callback_set(view_port, snake_game_render_callback, snake_state);
    view_port_input_callback_set(view_port, snake_game_input_callback, event_queue);

    FuriTimer* timer =
        furi_timer_alloc(snake_game_update_timer_callback, FuriTimerTypePeriodic, event_queue);
    furi_timer_start(timer, furi_kernel_get_tick_frequency() / 4);

    // Open GUI and register view_port
    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);
    NotificationApp* notification = furi_record_open(RECORD_NOTIFICATION);

    notification_message_block(notification, &sequence_display_backlight_enforce_on);

    dolphin_deed(DolphinDeedPluginGameStart);

    SnakeEvent event;
    for(bool processing = true; processing;) {
        FuriStatus event_status = furi_message_queue_get(event_queue, &event, 100);

        furi_mutex_acquire(snake_state->mutex, FuriWaitForever);

        if(event_status == FuriStatusOk) {
            if(event.type == EventTypeKey) {
                // press events
                if(event.input.type == InputTypePress) {
                    switch(event.input.key) {
                    case InputKeyUp:
                        if(snake_state->state != GameStatePause) {
                            snake_state->nextMovement = DirectionUp;
                        }
                        break;
                    case InputKeyDown:
                        if(snake_state->state != GameStatePause) {
                            snake_state->nextMovement = DirectionDown;
                        }
                        break;
                    case InputKeyRight:
                        if(snake_state->state == GameStatePause ||
                           snake_state->state == GameStateGameOver) {
                            snake_state->Endlessmode = !snake_state->Endlessmode;
                        } else {
                            snake_state->nextMovement = DirectionRight;
                        }
                        break;
                    case InputKeyLeft:
                        if(snake_state->state == GameStatePause ||
                           snake_state->state == GameStateGameOver) {
                            snake_state->Endlessmode = !snake_state->Endlessmode;
                        } else {
                            snake_state->nextMovement = DirectionLeft;
                        }
                        break;
                    case InputKeyOk:
                        if(snake_state->state == GameStateGameOver) {
                            snake_game_init_game(snake_state);
                        }
                        if(snake_state->state == GameStatePause) {
                            DateTime curr_dt;
                            furi_hal_rtc_get_datetime(&curr_dt);
                            uint32_t curr_ts = datetime_datetime_to_timestamp(&curr_dt);

                            snake_state->timer_start_timestamp =
                                curr_ts - snake_state->timer_stopped_seconds;

                            furi_timer_start(timer, furi_kernel_get_tick_frequency() / 4);
                            snake_state->state = GameStateLife;
                        }
                        break;
                    case InputKeyBack:
                        if(snake_state->state == GameStateLife) {
                            furi_timer_stop(timer);
                            snake_state->state = GameStatePause;

                            DateTime curr_dt;
                            furi_hal_rtc_get_datetime(&curr_dt);
                            uint32_t curr_ts = datetime_datetime_to_timestamp(&curr_dt);

                            snake_state->timer_stopped_seconds =
                                curr_ts - snake_state->timer_start_timestamp;

                            break;
                        }
                    default:
                        break;
                    }
                }
                //LongPress Events
                if(event.input.type == InputTypeLong) {
                    switch(event.input.key) {
                    case InputKeyUp:
                        if(snake_state->state != GameStatePause) {
                            snake_state->nextMovement = DirectionUp;
                            //Speed Up
                            if(snake_state->currentMovement == DirectionUp) {
                                furi_timer_start(timer, furi_kernel_get_tick_frequency() / 8);
                            }
                            //Breaking
                            if(snake_state->currentMovement == DirectionDown) {
                                furi_timer_start(timer, furi_kernel_get_tick_frequency() / 2);
                            }
                        }
                        break;
                    case InputKeyDown:
                        if(snake_state->state != GameStatePause) {
                            snake_state->nextMovement = DirectionDown;
                            //Speed Up
                            if(snake_state->currentMovement == DirectionDown) {
                                furi_timer_start(timer, furi_kernel_get_tick_frequency() / 8);
                            }
                            //Breaking
                            if(snake_state->currentMovement == DirectionUp) {
                                furi_timer_start(timer, furi_kernel_get_tick_frequency() / 2);
                            }
                        }
                        break;
                    case InputKeyRight:
                        if(snake_state->state != GameStatePause) {
                            snake_state->nextMovement = DirectionRight;
                            //Speed Up
                            if(snake_state->currentMovement == DirectionRight) {
                                furi_timer_start(timer, furi_kernel_get_tick_frequency() / 8);
                            }
                            //Breaking
                            if(snake_state->currentMovement == DirectionLeft) {
                                furi_timer_start(timer, furi_kernel_get_tick_frequency() / 2);
                            }
                        }
                        break;
                    case InputKeyLeft:
                        if(snake_state->state != GameStatePause) {
                            snake_state->nextMovement = DirectionLeft;
                            //Speed Up
                            if(snake_state->currentMovement == DirectionLeft) {
                                furi_timer_start(timer, furi_kernel_get_tick_frequency() / 8);
                            }
                            //Breaking
                            if(snake_state->currentMovement == DirectionRight) {
                                furi_timer_start(timer, furi_kernel_get_tick_frequency() / 2);
                            }
                        }
                        break;
                    case InputKeyBack:
                        if(snake_state->state == GameStatePause ||
                           snake_state->state == GameStateGameOver) {
                            save_game(snake_state);
                            processing = false;
                        } else {
                            snake_state->state = GameStateGameOver;
                        }
                        break;
                    default:
                        break;
                    }
                }
                //ReleaseKey Event
                if(event.input.type == InputTypeRelease) {
                    if(snake_state->state != GameStatePause) {
                        furi_timer_start(timer, furi_kernel_get_tick_frequency() / 4);
                    }
                }
            } else if(event.type == EventTypeTick) {
                snake_game_process_game_step(snake_state, notification);
            }
        } else {
            // event timeout
        }

        furi_mutex_release(snake_state->mutex);
        view_port_update(view_port);
    }

    // Wait for all notifications to be played and return backlight to normal state
    notification_message_block(notification, &sequence_display_backlight_enforce_auto);

    furi_timer_free(timer);
    view_port_enabled_set(view_port, false);
    gui_remove_view_port(gui, view_port);
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);
    view_port_free(view_port);
    furi_message_queue_free(event_queue);
    furi_mutex_free(snake_state->mutex);
    free(snake_state);

    return 0;
}
