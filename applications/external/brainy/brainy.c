#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <input/input.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>
#include <storage/storage.h>
#include <stdlib.h>
#include <string.h>
#include "particles.h"
#include "music.h"
#include "brainy_sprite.h"

// ---- Persistence ----------------------------------------------------------

#define SAVE_DIR  EXT_PATH("apps_data/brainy")
#define SAVE_FILE EXT_PATH("apps_data/brainy/best.dat")

static uint32_t load_high_score(void) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, SAVE_DIR);
    File* file = storage_file_alloc(storage);
    uint32_t score = 0;
    if(storage_file_open(file, SAVE_FILE, FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_read(file, &score, sizeof(score));
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return score;
}

static void save_high_score(uint32_t score) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, SAVE_DIR);
    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, SAVE_FILE, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_write(file, &score, sizeof(score));
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

// ---- Constants ------------------------------------------------------------

#define MAX_SEQUENCE      100
#define SHOW_DURATION_MS  400
#define SHOW_PAUSE_MS     150
#define INPUT_FEEDBACK_MS 300

// ---- Types ----------------------------------------------------------------

typedef enum {
    SimonStateTitle,
    SimonStateShowSequence,
    SimonStateWaitInput,
    SimonStateGameOver,
    SimonStateWin,
} SimonState;

typedef enum {
    SimonBtnUp    = 0,
    SimonBtnRight = 1,
    SimonBtnDown  = 2,
    SimonBtnLeft  = 3,
    SimonBtnNone  = 0xFF,
} SimonBtn;

typedef struct {
    InputEvent input;
} SimonEvent;

// Vibrate + cycle R→G→B LEDs, then a final double-buzz
static const NotificationSequence seq_lose = {
    &message_vibro_on,
    &message_red_255,
    &message_delay_100,
    &message_red_0,
    &message_green_255,
    &message_delay_100,
    &message_green_0,
    &message_blue_255,
    &message_delay_100,
    &message_blue_0,
    &message_vibro_off,
    &message_delay_50,
    &message_vibro_on,
    &message_delay_50,
    &message_vibro_off,
    NULL,
};

typedef struct {
    SimonState     state;
    uint8_t        sequence[MAX_SEQUENCE];
    uint8_t        seq_len;
    uint8_t        show_step;
    uint8_t        input_step;
    SimonBtn       lit_btn;
    uint32_t       high_score;
    uint32_t       tick_phase;
    ParticleSystem    particles;
    BgMusic           music;
    NotificationApp*  notifications;
    FuriMutex*        mutex;
    FuriMessageQueue* queue;
    bool           running;
} SimonApp;

// ---- Audio ----------------------------------------------------------------

static const float simon_freq[4] = {
    659.25f,  // Up    — E5
    783.99f,  // Right — G5
    523.25f,  // Down  — C5
    440.00f,  // Left  — A4
};

static void play_tone(SimonBtn btn, uint32_t duration_ms) {
    if(btn >= 4) return;
    if(!furi_hal_speaker_acquire(500)) return;
    furi_hal_speaker_start(simon_freq[btn], 1.0f);
    furi_delay_ms(duration_ms);
    furi_hal_speaker_stop();
    furi_hal_speaker_release();
}

static void play_fail_sound(void) {
    if(!furi_hal_speaker_acquire(500)) return;
    furi_hal_speaker_start(130.81f, 1.0f);
    furi_delay_ms(600);
    furi_hal_speaker_stop();
    furi_hal_speaker_release();
}

// ---- Rendering ------------------------------------------------------------
//
// Screen layout (128 x 64):
//
//   y=0..7   top bar: "Lvl X"  |  "Hi000"
//   y=8..55  d-pad cross — 3×3 grid, 4 cells used
//   y=56..63 bottom bar: "Watch..." left  |  "X/Y" right
//
//   col:  L    C    R
//   row0: .   [▲]   .
//   row1:[◄]   .   [►]
//   row2: .   [▼]   .
//
// Cell 14×14, gap 3px → 48×48px span, centred on 128px → GRID_X=40

#define BTN_SIZE 14
#define BTN_PAD   3
#define GRID_X   40
#define GRID_Y    8
#define STEP     (BTN_SIZE + BTN_PAD)

static const uint8_t btn_x[4] = {
    GRID_X + STEP,      // Up    — centre column
    GRID_X + 2 * STEP,  // Right — right column
    GRID_X + STEP,      // Down  — centre column
    GRID_X,             // Left  — left column
};
static const uint8_t btn_y[4] = {
    GRID_Y,             // Up    — top row
    GRID_Y + STEP,      // Right — middle row
    GRID_Y + 2 * STEP,  // Down  — bottom row
    GRID_Y + STEP,      // Left  — middle row
};

static void draw_arrow(Canvas* canvas, SimonBtn btn, uint8_t x, uint8_t y) {
    uint8_t cx = x + BTN_SIZE / 2;
    uint8_t cy = y + BTN_SIZE / 2;
    switch(btn) {
    case SimonBtnUp:
        canvas_draw_line(canvas, cx,     cy - 3, cx - 4, cy + 3);
        canvas_draw_line(canvas, cx,     cy - 3, cx + 4, cy + 3);
        canvas_draw_line(canvas, cx - 4, cy + 3, cx + 4, cy + 3);
        break;
    case SimonBtnRight:
        canvas_draw_line(canvas, cx + 3, cy,     cx - 3, cy - 4);
        canvas_draw_line(canvas, cx + 3, cy,     cx - 3, cy + 4);
        canvas_draw_line(canvas, cx - 3, cy - 4, cx - 3, cy + 4);
        break;
    case SimonBtnDown:
        canvas_draw_line(canvas, cx,     cy + 3, cx - 4, cy - 3);
        canvas_draw_line(canvas, cx,     cy + 3, cx + 4, cy - 3);
        canvas_draw_line(canvas, cx - 4, cy - 3, cx + 4, cy - 3);
        break;
    case SimonBtnLeft:
        canvas_draw_line(canvas, cx - 3, cy,     cx + 3, cy - 4);
        canvas_draw_line(canvas, cx - 3, cy,     cx + 3, cy + 4);
        canvas_draw_line(canvas, cx + 3, cy - 4, cx + 3, cy + 4);
        break;
    default:
        break;
    }
}

static void draw_button(Canvas* canvas, SimonBtn btn, bool filled) {
    uint8_t x = btn_x[btn];
    uint8_t y = btn_y[btn];
    if(filled) {
        canvas_draw_rbox(canvas, x, y, BTN_SIZE, BTN_SIZE, 2);
        canvas_set_color(canvas, ColorWhite);
    } else {
        canvas_draw_rframe(canvas, x, y, BTN_SIZE, BTN_SIZE, 2);
    }
    draw_arrow(canvas, btn, x, y);
    canvas_set_color(canvas, ColorBlack);
}

static void draw_callback(Canvas* canvas, void* ctx) {
    SimonApp* app = ctx;
    furi_mutex_acquire(app->mutex, FuriWaitForever);

    particle_system_update(&app->particles);

    canvas_clear(canvas);
    particle_system_draw(&app->particles, canvas);

    if(app->state == SimonStateTitle) {
        // Clear sprite area so particles don't bleed through
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_box(canvas, 1, 16, 34, 32);
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_xbm(canvas, 2, 16, BRAINY_W, BRAINY_H, brainy_xbm);

        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 84, 12, AlignCenter, AlignCenter, "BRAINY");
        canvas_set_font(canvas, FontSecondary);
        char hs[20];
        snprintf(hs, sizeof(hs), "Best: %03lu", (unsigned long)app->high_score);
        canvas_draw_str_aligned(canvas, 84, 32, AlignCenter, AlignCenter, hs);
        canvas_draw_str_aligned(canvas, 84, 48, AlignCenter, AlignCenter, "Press OK to play");

    } else if(app->state == SimonStateGameOver) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 14, AlignCenter, AlignCenter, "GAME OVER");
        canvas_set_font(canvas, FontSecondary);
        char sc[32];
        snprintf(sc, sizeof(sc), "Score: %u", (unsigned)app->seq_len - 1);
        canvas_draw_str_aligned(canvas, 64, 30, AlignCenter, AlignCenter, sc);
        char hs[32];
        snprintf(hs, sizeof(hs), "Best: %03lu", (unsigned long)app->high_score);
        canvas_draw_str_aligned(canvas, 64, 42, AlignCenter, AlignCenter, hs);
        canvas_draw_str_aligned(canvas, 64, 56, AlignCenter, AlignCenter, "OK=retry  BACK=menu");

    } else if(app->state == SimonStateWin) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 14, AlignCenter, AlignCenter, "YOU WIN!");
        canvas_set_font(canvas, FontSecondary);
        char sc[32];
        snprintf(sc, sizeof(sc), "Score: %lu", (unsigned long)MAX_SEQUENCE);
        canvas_draw_str_aligned(canvas, 64, 30, AlignCenter, AlignCenter, sc);
        canvas_draw_str_aligned(canvas, 64, 46, AlignCenter, AlignCenter, "OK=retry  BACK=menu");

    } else {
        // ShowSequence or WaitInput
        for(SimonBtn b = SimonBtnUp; b <= SimonBtnLeft; b++) {
            draw_button(canvas, b, app->lit_btn == b);
        }
        canvas_set_font(canvas, FontSecondary);
        char lvl[12];
        snprintf(lvl, sizeof(lvl), "Lvl %u", (unsigned)app->seq_len);
        canvas_draw_str(canvas, 1, 7, lvl);

        char hs[12];
        snprintf(hs, sizeof(hs), "Hi%03lu", (unsigned long)app->high_score);
        canvas_draw_str_aligned(canvas, 127, 7, AlignRight, AlignTop, hs);

        if(app->state == SimonStateShowSequence) {
            canvas_draw_str_aligned(canvas, 1, 63, AlignLeft, AlignBottom, "Watch...");
        } else {
            char hint[12];
            snprintf(hint, sizeof(hint), "%u/%u", (unsigned)app->input_step + 1, (unsigned)app->seq_len);
            canvas_draw_str_aligned(canvas, 127, 63, AlignRight, AlignBottom, hint);
        }
    }

    furi_mutex_release(app->mutex);
}

// ---- Input callback -------------------------------------------------------

static void input_callback(InputEvent* event, void* ctx) {
    SimonApp* app = ctx;
    SimonEvent e = {.input = *event};
    furi_message_queue_put(app->queue, &e, 0);
}

// ---- Game logic -----------------------------------------------------------

// Speed shrinks 5% per level, floored at 0.25 (= 4× faster than base).
static float get_speed_factor(uint8_t seq_len) {
    float f = 1.0f - (float)(seq_len - 1) * 0.05f;
    return f < 0.25f ? 0.25f : f;
}

static void add_to_sequence(SimonApp* app) {
    if(app->seq_len < MAX_SEQUENCE) {
        app->sequence[app->seq_len] = (uint8_t)(rand() % 4);
        app->seq_len++;
    }
}

static void game_start(SimonApp* app) {
    srand((unsigned)furi_get_tick());
    app->seq_len    = 1;
    app->sequence[0] = (uint8_t)(rand() % 4);
    app->show_step  = 0;
    app->input_step = 0;
    app->lit_btn    = SimonBtnNone;
    app->state      = SimonStateShowSequence;
    app->tick_phase = 0;
    app->music.idx    = 0;
    app->music.active = true;
}

// ---- Main app -------------------------------------------------------------

int32_t brainy(void* p) {
    UNUSED(p);

    SimonApp* app = malloc(sizeof(SimonApp));
    memset(app, 0, sizeof(SimonApp));
    app->mutex    = furi_mutex_alloc(FuriMutexTypeNormal);
    app->queue    = furi_message_queue_alloc(8, sizeof(SimonEvent));
    app->state      = SimonStateTitle;
    app->lit_btn    = SimonBtnNone;
    app->running    = true;
    app->high_score = load_high_score();

    particle_system_init(&app->particles);
    bg_music_start(&app->music);

    ViewPort* vp = view_port_alloc();
    view_port_draw_callback_set(vp, draw_callback, app);
    view_port_input_callback_set(vp, input_callback, app);

    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, vp, GuiLayerFullscreen);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    SimonEvent event;
    uint32_t show_next_at = 0;

    while(app->running) {
        uint32_t timeout = (app->state == SimonStateShowSequence) ? 10 : PARTICLE_MS;
        FuriStatus status = furi_message_queue_get(app->queue, &event, timeout);

        furi_mutex_acquire(app->mutex, FuriWaitForever);

        if(status == FuriStatusOk) {
            InputEvent* ie = &event.input;

            // Back — always exits or returns to title
            if(ie->key == InputKeyBack && ie->type == InputTypeShort) {
                if(app->state == SimonStateTitle) {
                    app->running = false;
                } else {
                    app->lit_btn = SimonBtnNone;
                    app->music.active = false;
                    app->state = SimonStateTitle;
                }
                goto next;
            }

            // Title screen
            if(app->state == SimonStateTitle) {
                if(ie->key == InputKeyOk && ie->type == InputTypeShort) {
                    game_start(app);
                    show_next_at = furi_get_tick() + SHOW_PAUSE_MS;
                }
                goto next;
            }

            // Game over / Win screen
            if(app->state == SimonStateGameOver || app->state == SimonStateWin) {
                if(ie->key == InputKeyOk && ie->type == InputTypeShort) {
                    game_start(app);
                    show_next_at = furi_get_tick() + SHOW_PAUSE_MS;
                }
                goto next;
            }

            // Player input
            if(app->state == SimonStateWaitInput && ie->type == InputTypeShort) {
                SimonBtn pressed = SimonBtnNone;
                switch(ie->key) {
                case InputKeyUp:    pressed = SimonBtnUp;    break;
                case InputKeyRight: pressed = SimonBtnRight; break;
                case InputKeyDown:  pressed = SimonBtnDown;  break;
                case InputKeyLeft:  pressed = SimonBtnLeft;  break;
                default: break;
                }

                if(pressed != SimonBtnNone) {
                    app->lit_btn = pressed;
                    view_port_update(vp);

                    furi_mutex_release(app->mutex);
                    play_tone(pressed, INPUT_FEEDBACK_MS);
                    furi_mutex_acquire(app->mutex, FuriWaitForever);

                    app->lit_btn = SimonBtnNone;

                    if(pressed == (SimonBtn)app->sequence[app->input_step]) {
                        app->input_step++;
                        if(app->input_step >= app->seq_len) {
                            if(app->seq_len >= MAX_SEQUENCE) {
                                if(app->seq_len > app->high_score) {
                                    app->high_score = app->seq_len;
                                    save_high_score(app->high_score);
                                }
                                app->music.active = false;
                                app->state = SimonStateWin;
                            } else {
                                add_to_sequence(app);
                                if(app->seq_len > app->high_score) {
                                    app->high_score = app->seq_len - 1;
                                    save_high_score(app->high_score);
                                }
                                app->show_step  = 0;
                                app->input_step = 0;
                                app->state      = SimonStateShowSequence;
                                app->tick_phase = 0;
                                show_next_at = furi_get_tick() +
                                    (uint32_t)(SHOW_PAUSE_MS * 2 * get_speed_factor(app->seq_len));
                            }
                        }
                    } else {
                        uint32_t score = app->seq_len > 0 ? app->seq_len - 1 : 0;
                        bool new_best = score > app->high_score;
                        if(new_best) app->high_score = score;
                        furi_mutex_release(app->mutex);
                        if(new_best) save_high_score(score);
                        play_fail_sound();
                        notification_message(app->notifications, &seq_lose);
                        furi_mutex_acquire(app->mutex, FuriWaitForever);
                        app->lit_btn = SimonBtnNone;
                        app->music.active = false;
                        app->state = SimonStateGameOver;
                    }
                }
            }
        }

        // ShowSequence state machine
        if(app->state == SimonStateShowSequence) {
            uint32_t now = furi_get_tick();
            if(now >= show_next_at) {
                float sf       = get_speed_factor(app->seq_len);
                uint32_t note_ms  = (uint32_t)(SHOW_DURATION_MS * sf);
                uint32_t pause_ms = (uint32_t)(SHOW_PAUSE_MS    * sf);
                if(app->tick_phase == 0) {
                    SimonBtn b = (SimonBtn)app->sequence[app->show_step];
                    app->lit_btn = b;
                    view_port_update(vp);
                    furi_mutex_release(app->mutex);
                    play_tone(b, note_ms);
                    furi_mutex_acquire(app->mutex, FuriWaitForever);
                    app->lit_btn = SimonBtnNone;
                    show_next_at = furi_get_tick() + pause_ms;
                    app->tick_phase = 1;
                } else {
                    app->show_step++;
                    if(app->show_step >= app->seq_len) {
                        app->state = SimonStateWaitInput;
                        app->input_step = 0;
                    } else {
                        app->tick_phase = 0;
                        show_next_at = furi_get_tick() + pause_ms;
                    }
                }
            }
        }

    next:
        view_port_update(vp);
        furi_mutex_release(app->mutex);
    }

    bg_music_stop(&app->music);
    furi_record_close(RECORD_NOTIFICATION);
    gui_remove_view_port(gui, vp);
    view_port_free(vp);
    furi_record_close(RECORD_GUI);
    furi_message_queue_free(app->queue);
    furi_mutex_free(app->mutex);
    free(app);

    return 0;
}
