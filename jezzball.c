#include "jezzball_game.h"

#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>

#include <stdio.h>
#include <stdlib.h>

#define JZ_BOARD_X     4
#define JZ_BOARD_Y     11
#define JZ_CELL_PIXELS 2
#define JZ_FRAME_MS    (1000 / JZ_TICKS_PER_SECOND)

typedef enum {
    JzAppEventInput,
    JzAppEventTick,
} JzAppEventType;

typedef struct {
    JzAppEventType type;
    InputEvent input;
} JzAppEvent;

typedef struct {
    JzGame game;
    FuriMessageQueue* event_queue;
    FuriMutex* mutex;
    ViewPort* view_port;
    FuriTimer* timer;
    bool running;
} JzApp;

static int32_t jz_screen_x(uint8_t grid_x) {
    return JZ_BOARD_X + ((int32_t)grid_x * JZ_CELL_PIXELS);
}

static int32_t jz_screen_y(uint8_t grid_y) {
    return JZ_BOARD_Y + ((int32_t)grid_y * JZ_CELL_PIXELS);
}

static void jz_draw_board(Canvas* canvas, const JzGame* game) {
    for(uint8_t y = 0; y < JZ_GRID_HEIGHT; y++) {
        for(uint8_t x = 0; x < JZ_GRID_WIDTH; x++) {
            const JzCell cell = game->cells[y][x];
            const int32_t screen_x = jz_screen_x(x);
            const int32_t screen_y = jz_screen_y(y);

            if(cell == JzCellWall) {
                canvas_draw_box(canvas, screen_x, screen_y, JZ_CELL_PIXELS, JZ_CELL_PIXELS);
            } else if(cell == JzCellFilled) {
                canvas_draw_dot(canvas, screen_x + ((x + y) & 1U), screen_y + ((x ^ y) & 1U));
            } else if(cell == JzCellGrowing) {
                canvas_draw_frame(canvas, screen_x, screen_y, JZ_CELL_PIXELS, JZ_CELL_PIXELS);
            }
        }
    }

    for(uint8_t index = 0; index < game->ball_count; index++) {
        int32_t ball_x = JZ_BOARD_X + ((game->balls[index].x * JZ_CELL_PIXELS) >> 8);
        int32_t ball_y = JZ_BOARD_Y + ((game->balls[index].y * JZ_CELL_PIXELS) >> 8);
        canvas_draw_disc(canvas, ball_x, ball_y, 2);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_dot(canvas, ball_x, ball_y);
        canvas_set_color(canvas, ColorBlack);
    }

    if((game->screen == JzScreenPlaying) && !game->growing_wall.active &&
       (game->cells[game->cursor_y][game->cursor_x] == JzCellEmpty)) {
        int32_t cursor_x = jz_screen_x(game->cursor_x) + 1;
        int32_t cursor_y = jz_screen_y(game->cursor_y) + 1;
        if(game->cursor_horizontal) {
            canvas_draw_line(canvas, cursor_x - 3, cursor_y, cursor_x + 3, cursor_y);
        } else {
            canvas_draw_line(canvas, cursor_x, cursor_y - 3, cursor_x, cursor_y + 3);
        }
    }
}

static void jz_draw_status(Canvas* canvas, const JzGame* game) {
    char status[32];
    snprintf(
        status,
        sizeof(status),
        "L%u %u%% S%lu x%u %lus %c",
        game->level,
        jz_game_captured_percent(game),
        (unsigned long)game->score,
        game->lives,
        (unsigned long)jz_game_seconds_remaining(game),
        game->cursor_horizontal ? 'H' : 'V');
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 1, 8, status);
}

static void jz_draw_overlay(Canvas* canvas, const char* title, const char* detail) {
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_box(canvas, 5, 21, 118, 30);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_frame(canvas, 5, 21, 118, 30);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 34, AlignCenter, AlignBottom, title);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 46, AlignCenter, AlignBottom, detail);
}

static void jz_draw_title(Canvas* canvas) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 14, AlignCenter, AlignBottom, "JEZZBALL");

    canvas_draw_frame(canvas, 9, 19, 110, 21);
    canvas_draw_disc(canvas, 25, 28, 2);
    canvas_draw_disc(canvas, 99, 32, 2);
    canvas_draw_line(canvas, 62, 20, 62, 39);
    for(uint8_t y = 22; y < 39; y += 4)
        canvas_draw_dot(canvas, 76, y);

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 49, AlignCenter, AlignBottom, "D-pad: move + choose H/V");
    canvas_draw_str_aligned(canvas, 64, 57, AlignCenter, AlignBottom, "OK: build   Back: pause");
    canvas_draw_str_aligned(canvas, 64, 64, AlignCenter, AlignBottom, "Press OK to start");
}

static void jz_draw_callback(Canvas* canvas, void* context) {
    JzApp* app = context;
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    const JzGame* game = &app->game;

    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    if(game->screen == JzScreenTitle) {
        jz_draw_title(canvas);
    } else {
        jz_draw_status(canvas, game);
        jz_draw_board(canvas, game);

        if(game->screen == JzScreenPaused) {
            jz_draw_overlay(canvas, "PAUSED", "OK or Back to resume");
        } else if(game->screen == JzScreenLevelClear) {
            jz_draw_overlay(canvas, "AREA SECURED!", "OK: next level");
        } else if(game->screen == JzScreenGameOver) {
            jz_draw_overlay(canvas, "GAME OVER", "OK: retry");
        } else if(game->screen == JzScreenVictory) {
            jz_draw_overlay(canvas, "YOU WIN!", "OK: play again");
        } else if(game->invalid_action_ticks > 0) {
            jz_draw_overlay(canvas, "BLOCKED", "Move to open space");
        }
    }

    furi_mutex_release(app->mutex);
}

static void jz_input_callback(InputEvent* input, void* context) {
    JzApp* app = context;
    JzAppEvent event = {
        .type = JzAppEventInput,
        .input = *input,
    };
    furi_message_queue_put(app->event_queue, &event, 0);
}

static void jz_timer_callback(void* context) {
    JzApp* app = context;
    const JzAppEvent event = {.type = JzAppEventTick};
    furi_message_queue_put(app->event_queue, &event, 0);
}

static void jz_handle_direction(JzGame* game, InputKey key, int8_t distance) {
    if(key == InputKeyLeft) {
        jz_game_move_cursor(game, -distance, 0, true);
    } else if(key == InputKeyRight) {
        jz_game_move_cursor(game, distance, 0, true);
    } else if(key == InputKeyUp) {
        jz_game_move_cursor(game, 0, -distance, false);
    } else if(key == InputKeyDown) {
        jz_game_move_cursor(game, 0, distance, false);
    }
}

static void jz_handle_input(JzApp* app, const InputEvent* input) {
    JzGame* game = &app->game;

    if((input->key == InputKeyBack) && (input->type == InputTypeLong)) {
        app->running = false;
        return;
    }

    if((input->key == InputKeyBack) && (input->type == InputTypeShort)) {
        if((game->screen == JzScreenTitle) || (game->screen == JzScreenGameOver) ||
           (game->screen == JzScreenVictory)) {
            app->running = false;
        } else {
            jz_game_toggle_pause(game);
        }
        return;
    }

    if((input->key == InputKeyOk) && (input->type == InputTypeShort)) {
        if(game->screen == JzScreenTitle || game->screen == JzScreenGameOver ||
           game->screen == JzScreenVictory) {
            jz_game_start(game);
        } else if(game->screen == JzScreenPlaying) {
            jz_game_start_wall(game);
        } else if(game->screen == JzScreenPaused) {
            jz_game_toggle_pause(game);
        } else if(game->screen == JzScreenLevelClear) {
            jz_game_advance_level(game);
        }
        return;
    }

    if((input->type == InputTypePress) || (input->type == InputTypeRepeat)) {
        const int8_t distance = (input->type == InputTypeRepeat) ? 2 : 1;
        jz_handle_direction(game, input->key, distance);
    }
}

int32_t jezzball_app(void* p) {
    UNUSED(p);

    JzApp* app = malloc(sizeof(JzApp));
    if(!app) return -1;

    app->event_queue = furi_message_queue_alloc(16, sizeof(JzAppEvent));
    app->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    app->view_port = view_port_alloc();
    app->timer = furi_timer_alloc(jz_timer_callback, FuriTimerTypePeriodic, app);
    app->running = true;
    jz_game_init(&app->game, furi_get_tick());

    view_port_draw_callback_set(app->view_port, jz_draw_callback, app);
    view_port_input_callback_set(app->view_port, jz_input_callback, app);

    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, app->view_port, GuiLayerFullscreen);
    furi_timer_start(app->timer, furi_ms_to_ticks(JZ_FRAME_MS));

    while(app->running) {
        JzAppEvent event;
        if(furi_message_queue_get(app->event_queue, &event, FuriWaitForever) != FuriStatusOk) {
            continue;
        }

        furi_mutex_acquire(app->mutex, FuriWaitForever);
        if(event.type == JzAppEventTick) {
            jz_game_tick(&app->game);
        } else if(event.type == JzAppEventInput) {
            jz_handle_input(app, &event.input);
        }
        furi_mutex_release(app->mutex);
        view_port_update(app->view_port);
    }

    furi_timer_stop(app->timer);
    gui_remove_view_port(gui, app->view_port);
    furi_record_close(RECORD_GUI);
    furi_timer_free(app->timer);
    view_port_free(app->view_port);
    furi_mutex_free(app->mutex);
    furi_message_queue_free(app->event_queue);
    free(app);
    return 0;
}
