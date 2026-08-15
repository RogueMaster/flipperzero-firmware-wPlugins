#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <input/input.h>
#include <storage/storage.h>
#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <furi_hal_light.h>

#define SAVE_FOLDER "/ext/apps_data/stack"
#define SAVE_PATH "/ext/apps_data/stack/stack.save"

#define FPS 30
#define BLOCK_H 4
#define INITIAL_SIZE 22.0f
#define MIN_SIZE 2.5f
#define MAX_TOWER 16
#define PERFECT_TOLERANCE 1.2f
#define COMBO_TARGET 6
#define GROWTH_AMT 3.5f

#define SCREEN_CENTER_X 64
#define SCREEN_CENTER_Y 34

// Musical Scale Notes: DO, RE, MI, FA, SOL, LA (Hz)
static const float SCALE_FREQS[] = {
    523.25f, // DO (C5)
    587.33f, // RE (D5)
    659.25f, // MI (E5)
    698.46f, // FA (F5)
    783.99f, // SOL (G5)
    880.00f  // LA (A5)
};

typedef enum {
    StateMenu,
    StateSettings,
    StateCredits,
    StatePlaying,
    StateGameOver
} GameState;

typedef enum {
    MenuItemStart = 0,
    MenuItemSettings,
    MenuItemCredits,
    MenuItemCount
} MenuItem;

typedef enum {
    SettingsItemSound = 0,
    SettingsItemVibro,
    SettingsItemCount
} SettingsItem;

typedef enum {
    AxisX,
    AxisY
} Axis;

typedef struct {
    float x, y;
    float w, l;
    float z;
} IsoBlock;

typedef enum {
    AppEventTypeTick,
    AppEventTypeInput
} AppEventType;

typedef struct {
    AppEventType type;
    InputEvent input;
} AppEvent;

typedef struct {
    int high_score;
    bool sound_enabled;
    bool vibro_enabled;
} SaveData;

typedef struct {
    GameState state;
    MenuItem menu_selected;
    SettingsItem settings_selected;

    IsoBlock tower[MAX_TOWER];
    int tower_count;
    int total_height;
    
    Axis active_axis;
    float active_pos;
    float active_w, active_l;
    float speed;
    float dir;
    
    int score;
    int high_score;
    int combo;
    
    bool sound_enabled;
    bool vibro_enabled;

    float camera_z;
    bool invert_screen;

    // Hardware Feedback Control
    int effect_ticks;
    bool speaker_active;
    bool vibro_active;
    
    FuriMutex* mutex;
} StackGame;

// --- Feedback & Hardware Control ---

static void clear_effects(StackGame* game) {
    if(game->speaker_active) {
        furi_hal_speaker_stop();
        furi_hal_speaker_release();
        game->speaker_active = false;
    }
    if(game->vibro_active) {
        furi_hal_vibro_on(false);
        game->vibro_active = false;
    }
    furi_hal_light_set(LightRed, 0);
    furi_hal_light_set(LightGreen, 0);
    furi_hal_light_set(LightBlue, 0);
}

static void trigger_feedback_perfect(StackGame* game, int combo_count) {
    clear_effects(game);

    int note_idx = combo_count;
    if(note_idx > 5) note_idx = 5; // Lock on LA
    float freq = SCALE_FREQS[note_idx];

    if(game->sound_enabled && furi_hal_speaker_acquire(10)) {
        furi_hal_speaker_start(freq, 0.8f);
        game->speaker_active = true;
    }
    if(game->vibro_enabled) {
        furi_hal_vibro_on(true);
        game->vibro_active = true;
    }

    // Flash White LED
    furi_hal_light_set(LightRed, 255);
    furi_hal_light_set(LightGreen, 255);
    furi_hal_light_set(LightBlue, 255);

    game->effect_ticks = 2; // ~66ms
}

static void trigger_feedback_normal(StackGame* game) {
    clear_effects(game);

    if(game->sound_enabled && furi_hal_speaker_acquire(10)) {
        furi_hal_speaker_start(400.0f, 0.4f);
        game->speaker_active = true;
    }
    game->effect_ticks = 2;
}

static void trigger_feedback_gameover(StackGame* game) {
    clear_effects(game);

    if(game->sound_enabled && furi_hal_speaker_acquire(10)) {
        furi_hal_speaker_start(160.0f, 0.9f); // Deep low tone
        game->speaker_active = true;
    }

    // Red LED Only (No Vibro)
    furi_hal_light_set(LightRed, 255);
    furi_hal_light_set(LightGreen, 0);
    furi_hal_light_set(LightBlue, 0);

    game->effect_ticks = 15; // 0,5 Seconds (30 Ticks = 30 FPS)
}

// --- Storage Functions ---

static void load_settings(StackGame* game) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    SaveData save;

    if(storage_file_open(file, SAVE_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        if(storage_file_read(file, &save, sizeof(SaveData)) == sizeof(SaveData)) {
            game->high_score = save.high_score;
            game->sound_enabled = save.sound_enabled;
            game->vibro_enabled = save.vibro_enabled;
        } else {
            game->high_score = 0;
            game->sound_enabled = true;
            game->vibro_enabled = true;
        }
    } else {
        game->high_score = 0;
        game->sound_enabled = true;
        game->vibro_enabled = true;
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

static void save_settings(StackGame* game) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, SAVE_FOLDER);
    File* file = storage_file_alloc(storage);

    if(storage_file_open(file, SAVE_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        SaveData save = {
            .high_score = game->high_score,
            .sound_enabled = game->sound_enabled,
            .vibro_enabled = game->vibro_enabled
        };
        storage_file_write(file, &save, sizeof(SaveData));
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

// --- 3D Isometric Projection ---

static void project_iso(float x, float y, float z, float cam_z, int* out_x, int* out_y) {
    *out_x = SCREEN_CENTER_X + (int)(x - y);
    *out_y = SCREEN_CENTER_Y + (int)((x + y) * 0.5f) - (int)((z - cam_z) * BLOCK_H);
}

static void draw_filled_triangle(Canvas* canvas, int x0, int y0, int x1, int y1, int x2, int y2) {
    if(y0 > y1) { int tx=x0; x0=x1; x1=tx; int ty=y0; y0=y1; y1=ty; }
    if(y1 > y2) { int tx=x1; x1=x2; x2=tx; int ty=y1; y1=y2; y2=ty; }
    if(y0 > y1) { int tx=x0; x0=x1; x1=tx; int ty=y0; y0=y1; y1=ty; }

    if(y0 == y2) return;

    for(int y = y0; y <= y2; y++) {
        bool second_half = (y > y1) || (y1 == y0);
        int segment_height = second_half ? (y2 - y1) : (y1 - y0);
        if(segment_height == 0) continue;

        float alpha = (float)(y - y0) / (float)(y2 - y0);
        float beta  = (float)(y - (second_half ? y1 : y0)) / (float)segment_height;

        int ax = x0 + (int)((x2 - x0) * alpha);
        int bx = second_half ? (x1 + (int)((x2 - x1) * beta)) : (x0 + (int)((x1 - x0) * beta));

        if(ax > bx) { int t = ax; ax = bx; bx = t; }
        canvas_draw_line(canvas, ax, y, bx, y);
    }
}

static void draw_iso_block(Canvas* canvas, float x, float y, float z, float w, float l, float cam_z) {
    int x0, y0, x1, y1, x2, y2, x3, y3;
    
    project_iso(x, y, z, cam_z, &x0, &y0);         // Top corner
    project_iso(x + w, y, z, cam_z, &x1, &y1);     // Right corner
    project_iso(x + w, y + l, z, cam_z, &x2, &y2); // Bottom corner
    project_iso(x, y + l, z, cam_z, &x3, &y3);     // Left corner

    int h = BLOCK_H;

    // Right Wall
    canvas_set_color(canvas, ColorBlack);
    draw_filled_triangle(canvas, x1, y1, x2, y2, x2, y2 + h);
    draw_filled_triangle(canvas, x1, y1, x2, y2 + h, x1, y1 + h);

    // Left Wall
    draw_filled_triangle(canvas, x3, y3, x2, y2, x2, y2 + h);
    draw_filled_triangle(canvas, x3, y3, x2, y2 + h, x3, y3 + h);

    // Top Face (White)
    canvas_set_color(canvas, ColorWhite);
    draw_filled_triangle(canvas, x0, y0, x1, y1, x2, y2);
    draw_filled_triangle(canvas, x0, y0, x2, y2, x3, y3);

    // Outlines (Black)
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_line(canvas, x0, y0, x1, y1);
    canvas_draw_line(canvas, x1, y1, x2, y2);
    canvas_draw_line(canvas, x2, y2, x3, y3);
    canvas_draw_line(canvas, x3, y3, x0, y0);

    canvas_draw_line(canvas, x3, y3, x3, y3 + h);
    canvas_draw_line(canvas, x2, y2, x2, y2 + h);
    canvas_draw_line(canvas, x1, y1, x1, y1 + h);
}

// --- Game Engine Logic ---

static void spawn_block(StackGame* game) {
    game->active_axis = (game->score % 2 == 1) ? AxisX : AxisY;
    game->dir = 1.0f;
    game->active_pos = -26.0f;
    game->speed = 1.2f + (game->score * 0.05f);
    if(game->speed > 3.5f) game->speed = 3.5f;
}

static void reset_game(StackGame* game) {
    game->state = StatePlaying;
    game->score = 0;
    game->combo = 0;
    game->tower_count = 1;
    game->total_height = 0;
    game->camera_z = 0.0f;
    game->invert_screen = false;

    game->tower[0].x = -INITIAL_SIZE / 2.0f;
    game->tower[0].y = -INITIAL_SIZE / 2.0f;
    game->tower[0].w = INITIAL_SIZE;
    game->tower[0].l = INITIAL_SIZE;
    game->tower[0].z = 0;

    game->active_w = INITIAL_SIZE;
    game->active_l = INITIAL_SIZE;

    spawn_block(game);
}

static void place_block(StackGame* game) {
    IsoBlock* top = &game->tower[0];
    float cur_x = (game->active_axis == AxisX) ? game->active_pos : top->x;
    float cur_y = (game->active_axis == AxisY) ? game->active_pos : top->y;

    if(game->active_axis == AxisX) {
        float diff = cur_x - top->x;
        float abs_diff = fabs(diff);

        if(abs_diff <= PERFECT_TOLERANCE) {
            cur_x = top->x;
            trigger_feedback_perfect(game, game->combo);
            game->combo++;
            game->invert_screen = true;

            if(game->combo >= COMBO_TARGET) {
                game->active_w += GROWTH_AMT;
                if(game->active_w > INITIAL_SIZE) game->active_w = INITIAL_SIZE;
                cur_x -= (GROWTH_AMT / 2.0f);
            }
        } else {
            game->combo = 0;
            if(abs_diff >= game->active_w) {
                game->state = StateGameOver;
                trigger_feedback_gameover(game);
                if(game->score > game->high_score) {
                    game->high_score = game->score;
                    save_settings(game);
                }
                return;
            }
            game->active_w -= abs_diff;
            if(game->active_w < MIN_SIZE) {
                game->state = StateGameOver;
                trigger_feedback_gameover(game);
                if(game->score > game->high_score) {
                    game->high_score = game->score;
                    save_settings(game);
                }
                return;
            }
            if(diff < 0) cur_x = top->x;
            trigger_feedback_normal(game);
        }
    } else { // AxisY
        float diff = cur_y - top->y;
        float abs_diff = fabs(diff);

        if(abs_diff <= PERFECT_TOLERANCE) {
            cur_y = top->y;
            trigger_feedback_perfect(game, game->combo);
            game->combo++;
            game->invert_screen = true;

            if(game->combo >= COMBO_TARGET) {
                game->active_l += GROWTH_AMT;
                if(game->active_l > INITIAL_SIZE) game->active_l = INITIAL_SIZE;
                cur_y -= (GROWTH_AMT / 2.0f);
            }
        } else {
            game->combo = 0;
            if(abs_diff >= game->active_l) {
                game->state = StateGameOver;
                trigger_feedback_gameover(game);
                if(game->score > game->high_score) {
                    game->high_score = game->score;
                    save_settings(game);
                }
                return;
            }
            game->active_l -= abs_diff;
            if(game->active_l < MIN_SIZE) {
                game->state = StateGameOver;
                trigger_feedback_gameover(game);
                if(game->score > game->high_score) {
                    game->high_score = game->score;
                    save_settings(game);
                }
                return;
            }
            if(diff < 0) cur_y = top->y;
            trigger_feedback_normal(game);
        }
    }

    game->score++;
    game->total_height++;

    for(int i = MAX_TOWER - 1; i > 0; i--) {
        game->tower[i] = game->tower[i - 1];
    }

    game->tower[0].x = cur_x;
    game->tower[0].y = cur_y;
    game->tower[0].w = game->active_w;
    game->tower[0].l = game->active_l;
    game->tower[0].z = game->total_height;

    if(game->tower_count < MAX_TOWER) game->tower_count++;

    spawn_block(game);
}

static void game_tick(StackGame* game) {
    // Effect timer handling
    if(game->effect_ticks > 0) {
        game->effect_ticks--;
        if(game->effect_ticks == 0) {
            clear_effects(game);
        }
    }

    if(game->state == StatePlaying) {
        game->active_pos += game->speed * game->dir;

        if(game->active_pos <= -35.0f) {
            game->active_pos = -35.0f;
            game->dir = 1.0f;
        } else if(game->active_pos >= 22.0f) {
            game->active_pos = 22.0f;
            game->dir = -1.0f;
        }

        game->camera_z += (game->total_height - game->camera_z) * 0.15f;
    }
}

// --- Drawing Callbacks ---

static void draw_callback(Canvas* canvas, void* ctx) {
    StackGame* game = ctx;
    furi_mutex_acquire(game->mutex, FuriWaitForever);

    if(game->invert_screen) {
        canvas_draw_box(canvas, 0, 0, 128, 64);
        canvas_set_color(canvas, ColorWhite);
        game->invert_screen = false;
    } else {
        canvas_clear(canvas);
        canvas_set_color(canvas, ColorBlack);
    }

    if(game->state == StateMenu) {
        // Sleek Menu Box Header
        canvas_draw_frame(canvas, 18, 2, 92, 18);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 11, AlignCenter, AlignCenter, "3D STACK");

        // High Score Subtitle
        canvas_set_font(canvas, FontSecondary);
        char best_str[24];
        snprintf(best_str, sizeof(best_str), "BEST: %d", game->high_score);
        canvas_draw_str_aligned(canvas, 64, 26, AlignCenter, AlignCenter, best_str);

        // Menu Options
        const char* menu_items[] = {"START", "SETTINGS", "CREDITS"};
        for(int i = 0; i < MenuItemCount; i++) {
            int y = 38 + (i * 9);
            if(i == (int)game->menu_selected) {
                canvas_draw_box(canvas, 34, y - 6, 60, 9);
                canvas_set_color(canvas, ColorWhite);
                canvas_draw_str_aligned(canvas, 64, y - 1, AlignCenter, AlignCenter, menu_items[i]);
                canvas_set_color(canvas, ColorBlack);
            } else {
                canvas_draw_str_aligned(canvas, 64, y - 1, AlignCenter, AlignCenter, menu_items[i]);
            }
        }
    } 
    else if(game->state == StateSettings) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 10, AlignCenter, AlignCenter, "SETTINGS");
        canvas_draw_line(canvas, 10, 18, 118, 18);

        canvas_set_font(canvas, FontSecondary);
        char snd_str[20], vib_str[20];
        snprintf(snd_str, sizeof(snd_str), "Sound: <%s>", game->sound_enabled ? "ON" : "OFF");
        snprintf(vib_str, sizeof(vib_str), "Vibro: <%s>", game->vibro_enabled ? "ON" : "OFF");

        if(game->settings_selected == SettingsItemSound) {
            canvas_draw_box(canvas, 20, 24, 88, 12);
            canvas_set_color(canvas, ColorWhite);
            canvas_draw_str_aligned(canvas, 64, 30, AlignCenter, AlignCenter, snd_str);
            canvas_set_color(canvas, ColorBlack);
            canvas_draw_str_aligned(canvas, 64, 46, AlignCenter, AlignCenter, vib_str);
        } else {
            canvas_draw_str_aligned(canvas, 64, 30, AlignCenter, AlignCenter, snd_str);
            canvas_draw_box(canvas, 20, 40, 88, 12);
            canvas_set_color(canvas, ColorWhite);
            canvas_draw_str_aligned(canvas, 64, 46, AlignCenter, AlignCenter, vib_str);
            canvas_set_color(canvas, ColorBlack);
        }

        canvas_draw_str_aligned(canvas, 64, 58, AlignCenter, AlignCenter, "[BACK] Return");
    }
    else if(game->state == StateCredits) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 12, AlignCenter, AlignCenter, "CREDITS");
        canvas_draw_line(canvas, 10, 20, 118, 20);

        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 31, AlignCenter, AlignCenter, "Developer: bergr22");
        canvas_draw_str_aligned(canvas, 64, 43, AlignCenter, AlignCenter, "github.com/bergr22");
        canvas_draw_str_aligned(canvas, 64, 57, AlignCenter, AlignCenter, "[BACK] Return");
    }
    else if(game->state == StatePlaying || game->state == StateGameOver) {
        // Tower rendering
        for(int i = game->tower_count - 1; i >= 0; i--) {
            IsoBlock* b = &game->tower[i];
            draw_iso_block(canvas, b->x, b->y, b->z, b->w, b->l, game->camera_z);
        }

        if(game->state == StatePlaying) {
            IsoBlock* top = &game->tower[0];
            float cur_x = (game->active_axis == AxisX) ? game->active_pos : top->x;
            float cur_y = (game->active_axis == AxisY) ? game->active_pos : top->y;
            float cur_z = game->total_height + 1;

            draw_iso_block(canvas, cur_x, cur_y, cur_z, game->active_w, game->active_l, game->camera_z);
        }

        // Score display
        canvas_set_color(canvas, ColorBlack);
        canvas_set_font(canvas, FontBigNumbers);
        char score_str[16];
        snprintf(score_str, sizeof(score_str), "%d", game->score);
        canvas_draw_str_aligned(canvas, 64, 12, AlignCenter, AlignCenter, score_str);

        if(game->combo > 1) {
            canvas_set_font(canvas, FontSecondary);
            canvas_draw_str_aligned(canvas, 118, 10, AlignRight, AlignCenter, "PERFECT!");
        }

        if(game->state == StateGameOver) {
            canvas_set_color(canvas, ColorWhite);
            canvas_draw_box(canvas, 14, 14, 100, 38);
            canvas_set_color(canvas, ColorBlack);
            canvas_draw_frame(canvas, 14, 14, 100, 38);

            canvas_set_font(canvas, FontPrimary);
            canvas_draw_str_aligned(canvas, 64, 23, AlignCenter, AlignCenter, "GAME OVER");
            
            canvas_set_font(canvas, FontSecondary);
            char best_info[32];
            snprintf(best_info, sizeof(best_info), "Score: %d  Best: %d", game->score, game->high_score);
            canvas_draw_str_aligned(canvas, 64, 35, AlignCenter, AlignCenter, best_info);
            canvas_draw_str_aligned(canvas, 64, 45, AlignCenter, AlignCenter, "Press OK to Retry");
        }
    }

    furi_mutex_release(game->mutex);
}

static void input_callback(InputEvent* input_event, void* ctx) {
    FuriMessageQueue* event_queue = ctx;
    AppEvent event = {.type = AppEventTypeInput, .input = *input_event};
    furi_message_queue_put(event_queue, &event, FuriWaitForever);
}

static void timer_callback(void* ctx) {
    FuriMessageQueue* event_queue = ctx;
    AppEvent event = {.type = AppEventTypeTick};
    furi_message_queue_put(event_queue, &event, 0);
}

// --- Entry Point ---

int32_t stack_app(void* p) {
    UNUSED(p);

    StackGame* game = malloc(sizeof(StackGame));
    game->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    game->state = StateMenu;
    game->menu_selected = MenuItemStart;
    game->settings_selected = SettingsItemSound;
    game->score = 0;
    game->effect_ticks = 0;
    game->speaker_active = false;
    game->vibro_active = false;

    load_settings(game);

    FuriMessageQueue* event_queue = furi_message_queue_alloc(8, sizeof(AppEvent));

    ViewPort* view_port = view_port_alloc();
    view_port_draw_callback_set(view_port, draw_callback, game);
    view_port_input_callback_set(view_port, input_callback, event_queue);

    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    FuriTimer* timer = furi_timer_alloc(timer_callback, FuriTimerTypePeriodic, event_queue);
    furi_timer_start(timer, furi_kernel_get_tick_frequency() / FPS);

    bool running = true;
    while(running) {
        AppEvent event;
        if(furi_message_queue_get(event_queue, &event, 100) == FuriStatusOk) {
            furi_mutex_acquire(game->mutex, FuriWaitForever);

            if(event.type == AppEventTypeTick) {
                game_tick(game);
            } else if(event.type == AppEventTypeInput) {
                if(event.input.type == InputTypeShort || event.input.type == InputTypeRepeat) {
                    
                    // --- MENU INPUTS ---
                    if(game->state == StateMenu) {
                        if(event.input.key == InputKeyUp) {
                            if(game->menu_selected > 0) game->menu_selected--;
                        } else if(event.input.key == InputKeyDown) {
                            if(game->menu_selected < MenuItemCount - 1) game->menu_selected++;
                        } else if(event.input.key == InputKeyOk) {
                            if(game->menu_selected == MenuItemStart) {
                                reset_game(game);
                            } else if(game->menu_selected == MenuItemSettings) {
                                game->state = StateSettings;
                            } else if(game->menu_selected == MenuItemCredits) {
                                game->state = StateCredits;
                            }
                        } else if(event.input.key == InputKeyBack) {
                            running = false; // Exit Application
                        }
                    }
                    // --- SETTINGS INPUTS ---
                    else if(game->state == StateSettings) {
                        if(event.input.key == InputKeyUp || event.input.key == InputKeyDown) {
                            game->settings_selected = (game->settings_selected == SettingsItemSound) ? 
                                                      SettingsItemVibro : SettingsItemSound;
                        } else if(event.input.key == InputKeyLeft || event.input.key == InputKeyRight || event.input.key == InputKeyOk) {
                            if(game->settings_selected == SettingsItemSound) {
                                game->sound_enabled = !game->sound_enabled;
                            } else {
                                game->vibro_enabled = !game->vibro_enabled;
                            }
                            save_settings(game);
                        } else if(event.input.key == InputKeyBack) {
                            game->state = StateMenu;
                        }
                    }
                    // --- CREDITS INPUTS ---
                    else if(game->state == StateCredits) {
                        if(event.input.key == InputKeyBack || event.input.key == InputKeyOk) {
                            game->state = StateMenu;
                        }
                    }
                    // --- PLAYING INPUTS ---
                    else if(game->state == StatePlaying) {
                        if(event.input.key == InputKeyOk) {
                            place_block(game);
                        } else if(event.input.key == InputKeyBack) {
                            game->state = StateMenu;
                        }
                    }
                    // --- GAME OVER INPUTS ---
                    else if(game->state == StateGameOver) {
                        if(event.input.key == InputKeyOk) {
                            reset_game(game);
                        } else if(event.input.key == InputKeyBack) {
                            game->state = StateMenu;
                        }
                    }
                }
            }

            furi_mutex_release(game->mutex);
            view_port_update(view_port);
        }
    }

    // Clean Freeing
    clear_effects(game);
    furi_timer_stop(timer);
    furi_timer_free(timer);
    gui_remove_view_port(gui, view_port);
    furi_record_close(RECORD_GUI);
    view_port_free(view_port);
    furi_message_queue_free(event_queue);
    furi_mutex_free(game->mutex);
    free(game);

    return 0;
}