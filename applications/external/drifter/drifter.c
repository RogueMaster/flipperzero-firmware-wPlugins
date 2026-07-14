/*
 * Drifter
 * Copyright (C) 2025 Jed Lejosne
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>
#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <lib/flipper_format/flipper_format_i.h>

// Screen resolution
#define XMAX 127
#define YMAX 63

// Game settings
#define TIMER_DIV       18 // The game will tick this much faster than the kernel tick frequency
#define GAP_START       40.0L // Starting gap size.
#define GAP_DEC         0.004L // Gap reduction each tick
#define GAP_MIN         20.0L // Minimum gap size
#define SCORE_INC_START 0.07L // Initial score increment
#define SCORE_INC_MULT  1.04L // Score increment multiplier each tick without keypress
#define SPEED_DIV \
    200.0L // Horizontal speed will increase by length-of-button-push in ms divided by this
#define CHANGE_DIR 21 // How often to change direction, less is more
//#define SHOW_MULTIPLIER     // Un-comment to show the score multiplier in the top-right corner

static const char* HIGHSCORE_PATH = APP_DATA_PATH("highscore");

typedef struct {
    FuriMutex* mutex;
    FuriMessageQueue* event_queue;
    ViewPort* view_port;
    Gui* gui;
    FuriTimer* timer;

    uint8_t map[YMAX + 1];
    uint8_t head;
    int8_t dir;
    double gap;
    uint8_t game_mode; // 0=the boat moves - 1=the walls move

    double boat;
    double speed;

    InputKey key;
    uint32_t when;

    double score;
    double increment;
    uint32_t highscore;

    uint8_t dead;
    uint8_t verydead;
} DrifterState;

typedef enum {
    EventTypeTick,
    EventTypeKey,
} EventType;

typedef struct {
    EventType type;
    InputEvent input;
} DrifterEvent;

static uint32_t load_highscore() {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    const char* path = HIGHSCORE_PATH;
    uint32_t ret = 0;

    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        char line[9];
        storage_file_read(file, line, 9);
        ret = atoi(line);
    }

    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);

    return ret;
}

void save_highscore(uint32_t score) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    const char* path = HIGHSCORE_PATH;

    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        char line[9];
        snprintf(line, 9, "%ld", score);
        storage_file_write(file, line, strlen(line));
        storage_file_write(file, "\n", 1);
    } else {
        FURI_LOG_E("Drifter", "Failed to open file to save highscore: %s", path);
    }

    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

static void drifter_game_render_callback(Canvas* const canvas, void* ctx) {
    UNUSED(canvas);
    DrifterState* drifter_state = ctx;
    furi_check(furi_mutex_acquire(drifter_state->mutex, FuriWaitForever) == FuriStatusOk);

    // Before the function is called, the state is set with the canvas_reset(canvas)

    uint8_t pos = drifter_state->head;
    uint8_t boat = drifter_state->boat;
    for(int i = 0; i <= YMAX; ++i) {
        uint8_t val = drifter_state->map[pos];
        uint8_t gap = drifter_state->gap;
        int8_t gap_start = val;
        if(drifter_state->game_mode == 1) {
            gap_start -= boat - 62;
        }
        if(gap_start >= 0) {
            canvas_draw_line(canvas, 0, i, gap_start, i);
        }
        if(gap_start + gap <= XMAX) {
            canvas_draw_line(canvas, gap_start + gap, i, XMAX, i);
        }
        // Handle collisions here instead of adding a loop to game step function
        if(((i >= 58 && i <= 59) && (val >= boat + 1 || val + gap <= boat + 2)) ||
           ((i >= 60 && i <= 63) && (val >= boat + 0 || val + gap <= boat + 3))) {
            drifter_state->dead = 1;
            canvas_invert_color(canvas);
            canvas_draw_box(canvas, 0, 10, 97, 11);
            canvas_invert_color(canvas);
            if(drifter_state->score > drifter_state->highscore) {
                canvas_draw_str(canvas, 0, 18, "New High Score!");
            } else {
                char msg[21];
                snprintf(msg, 21, "High Score: %ld", drifter_state->highscore);
                canvas_draw_str(canvas, 0, 18, msg);
            }
        }
        // Ring buffer
        if(pos == YMAX) {
            pos = 0;
        } else {
            pos++;
        }
    }

    if(drifter_state->game_mode == 1) {
        boat = 62;
    }
    canvas_draw_line(canvas, boat, 60, boat, 63);
    canvas_draw_box(canvas, boat + 1, 58, 2, 5);
    canvas_draw_line(canvas, boat + 3, 60, boat + 3, 63);
    canvas_invert_color(canvas);
    canvas_draw_box(canvas, 0, 0, 48, 8);
#ifdef SHOW_MULTIPLIER
    canvas_draw_box(canvas, XMAX - 41, 0, 42, 8);
#endif
    canvas_invert_color(canvas);
    uint32_t score = drifter_state->score;
    char s[9] = {0};
    snprintf(s, 9, "%08ld", score);
    canvas_draw_str(canvas, 0, 7, s);
#ifdef SHOW_MULTIPLIER
    double multiplier = drifter_state->increment;
    snprintf(s, 8, "X%.1f", multiplier);
    canvas_draw_str(canvas, XMAX - 40, 7, s);
#endif

    furi_mutex_release(drifter_state->mutex);
}

static void drifter_game_input_callback(InputEvent* input_event, void* ctx) {
    furi_assert(ctx);
    FuriMessageQueue* event_queue = ctx;

    DrifterEvent event = {.type = EventTypeKey, .input = *input_event};
    furi_message_queue_put(event_queue, &event, FuriWaitForever);
}

static void drifter_game_update_timer_callback(void* ctx) {
    furi_assert(ctx);
    FuriMessageQueue* event_queue = ctx;

    DrifterEvent event = {.type = EventTypeTick};
    furi_message_queue_put(event_queue, &event, 0);
}

static void drifter_game_init_state(DrifterState* const drifter_state) {
    drifter_state->head = 0;
    drifter_state->dir = 1;
    drifter_state->gap = GAP_START;
    memset(drifter_state->map, (XMAX + 1 - GAP_START) / 2, YMAX + 1);

    drifter_state->boat = 62;
    drifter_state->speed = 0;

    drifter_state->key = InputKeyMAX;

    drifter_state->score = 0;
    drifter_state->increment = SCORE_INC_START;

    drifter_state->dead = 0;
    drifter_state->verydead = 0;
}

static void drifter_game_init_game(DrifterState* const drifter_state) {
    drifter_state->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    drifter_state->event_queue = furi_message_queue_alloc(8, sizeof(DrifterEvent));
    drifter_state->timer = furi_timer_alloc(
        drifter_game_update_timer_callback, FuriTimerTypePeriodic, drifter_state->event_queue);
    drifter_state->view_port = view_port_alloc();
    drifter_state->gui = furi_record_open("gui");

    drifter_game_init_state(drifter_state);
    drifter_state->highscore = load_highscore();

    furi_timer_start(drifter_state->timer, furi_kernel_get_tick_frequency() / TIMER_DIV);
    view_port_draw_callback_set(
        drifter_state->view_port, drifter_game_render_callback, drifter_state);
    view_port_input_callback_set(
        drifter_state->view_port, drifter_game_input_callback, drifter_state->event_queue);
    gui_add_view_port(drifter_state->gui, drifter_state->view_port, GuiLayerFullscreen);
}

static void drifter_game_process_game_step(DrifterState* const drifter_state) {
    UNUSED(drifter_state);

    // Just died
    if(drifter_state->dead && !drifter_state->verydead) {
        uint32_t score = drifter_state->score;
        if(score > drifter_state->highscore) {
            drifter_state->highscore = score;
            save_highscore(score);
        }
        drifter_state->verydead = 1;
    }

    if(drifter_state->dead) {
        return;
    }

    uint8_t head = drifter_state->head;
    uint8_t new = drifter_state->map[head];
    if(head == 0) {
        head = 63;
    } else {
        head--;
    }
    int8_t dir = drifter_state->dir;
    if(new == 0) {
        dir = 1;
    } else if(new > XMAX - drifter_state->gap) {
        dir = -1;
    } else if(rand() % CHANGE_DIR == 0) {
        dir = -dir;
    }
    new += dir;
    drifter_state->head = head;
    drifter_state->map[head] = new;
    drifter_state->dir = dir;

    drifter_state->boat += drifter_state->speed;
    if(drifter_state->boat < 0) {
        drifter_state->boat = 0;
        drifter_state->speed = 0;
    }
    if(drifter_state->boat > 123) {
        drifter_state->boat = 123;
        drifter_state->speed = 0;
    }
    drifter_state->score += drifter_state->increment;
    if(drifter_state->score > 99999999) {
        drifter_state->score = 99999999;
    }
    // Increase the speed increment by mulitplying by SCORE_INC_MULT + some gap-related amount
    drifter_state->increment *= SCORE_INC_MULT * ((GAP_START - drifter_state->gap) / 100 + 1);
    if(drifter_state->gap > GAP_MIN) {
        drifter_state->gap -= GAP_DEC;
    }
}

void drifter_game_free(DrifterState* drifter_state) {
    furi_timer_free(drifter_state->timer);
    view_port_enabled_set(drifter_state->view_port, false);
    gui_remove_view_port(drifter_state->gui, drifter_state->view_port);
    furi_record_close(RECORD_GUI);
    view_port_free(drifter_state->view_port);
    furi_message_queue_free(drifter_state->event_queue);
    furi_mutex_free(drifter_state->mutex);
    free(drifter_state);
}

int32_t drifter_app(void* p) {
    UNUSED(p);

    DrifterState* drifter_state = malloc(sizeof(DrifterState));
    drifter_state->game_mode = 0;
    drifter_game_init_game(drifter_state);

    DrifterEvent event;
    for(bool processing = true; processing;) {
        if(furi_message_queue_get(drifter_state->event_queue, &event, 100) == FuriStatusOk) {
            furi_check(furi_mutex_acquire(drifter_state->mutex, FuriWaitForever) == FuriStatusOk);
            // press events
            if(event.type == EventTypeKey) {
                uint32_t tick = furi_get_tick();
                double time = tick - drifter_state->when;
                if(time < 0) {
                    // Tick overflow
                    time = UINT32_MAX - drifter_state->when + tick;
                }
                double factor = -1;
                if(event.input.type == InputTypePress) {
                    switch(event.input.key) {
                    case InputKeyRight:
                        factor = 1;
                        __attribute__((fallthrough));
                    case InputKeyLeft:
                        if(drifter_state->key == InputKeyMAX) {
                            drifter_state->key = event.input.key;
                        } else if(drifter_state->key == event.input.key) {
                            drifter_state->speed += factor * time / SPEED_DIV;
                        }
                        drifter_state->when = furi_get_tick();
                        drifter_state->increment = SCORE_INC_START;
                        break;
                    case InputKeyBack:
                        processing = false;
                        break;
                    default:
                        break;
                    }
                } else if(event.input.type == InputTypeRelease) {
                    switch(event.input.key) {
                    case InputKeyDown:
                        drifter_state->game_mode = !drifter_state->game_mode;
                        break;
                    case InputKeyRight:
                        factor = 1;
                        __attribute__((fallthrough));
                    case InputKeyLeft:
                        if(event.input.key == drifter_state->key) {
                            drifter_state->speed += factor * time / SPEED_DIV;
                        }
                        break;
                    case InputKeyOk:
                        if(drifter_state->dead) {
                            drifter_game_init_state(drifter_state);
                        }
                        break;
                    case InputKeyBack:
                        processing = false;
                        break;
                    default:
                        break;
                    }
                    drifter_state->key = InputKeyMAX;
                }
            } else if(event.type == EventTypeTick) {
                drifter_game_process_game_step(drifter_state);
                view_port_update(drifter_state->view_port);
            }
        }
        furi_mutex_release(drifter_state->mutex);
    }
    drifter_game_free(drifter_state);

    return 0;
}
