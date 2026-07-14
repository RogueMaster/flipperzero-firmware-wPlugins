#include <stdlib.h>
#include <flappy_bird_icons.h>
#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <dolphin/dolphin.h>
#include <storage/storage.h>
#include <furi_hal.h>
#define TAG "Flappy"

#define DEBUG false

#define FLAPPY_BIRD_HEIGHT 15
#define FLAPPY_BIRD_WIDTH  10

#define FLAPPY_PILAR_MAX  6
#define FLAPPY_PILAR_DIST 35

#define FLAPPY_GAB_HEIGHT   24
#define YAPPER_GAB_HEIGHT   32
#define GHOSTESP_GAB_HEIGHT 28
#define FLAPPY_GAB_WIDTH    10

#define YAPPER_HEIGHT 22
#define YAPPER_WIDTH  16

#define FLAPPY_GRAVITY_JUMP -1.1
#define FLAPPY_GRAVITY_TICK 0.15

#define FLIPPER_LCD_WIDTH  128
#define FLIPPER_LCD_HEIGHT 64

static const char* FLAPPY_SAVE_PATH = APP_DATA_PATH("flappy_high.save");

typedef enum {
    BirdState0 = 0,
    BirdState1,
    BirdState2,
    BirdStateMAX
} BirdState;

typedef enum {
    BirdTypeDefault = 0,
    BirdType420,
    BirdTypeGhost,
    BirdTypeGhostESP,
    BirdTypeKirby,
    BirdTypeYapper,
    BirdTypeMAX
} BirdType;

typedef struct {
    int width;
    int height;
    int hitbox_x_offset;
    int hitbox_y_offset;
    int hitbox_width;
    int hitbox_height;
} CharacterDimensions;

static const CharacterDimensions character_dimensions[] = {
    {FLAPPY_BIRD_WIDTH,
     FLAPPY_BIRD_HEIGHT,
     2,
     2,
     FLAPPY_BIRD_WIDTH - 4,
     FLAPPY_BIRD_HEIGHT - 4}, // Default bird
    {YAPPER_WIDTH - 1, YAPPER_HEIGHT - 3, 5, 5, YAPPER_WIDTH - 9, YAPPER_HEIGHT - 11}, // 420
    {YAPPER_WIDTH, YAPPER_HEIGHT, 5, 5, YAPPER_WIDTH - 8, YAPPER_HEIGHT - 8}, // Ghost
    {YAPPER_WIDTH,
     YAPPER_HEIGHT,
     5,
     5,
     YAPPER_WIDTH - 10,
     YAPPER_HEIGHT - 10}, // Ghost with smaller hitbox
    {FLAPPY_BIRD_WIDTH,
     FLAPPY_BIRD_HEIGHT - 5,
     2,
     2,
     FLAPPY_BIRD_WIDTH - 4,
     FLAPPY_BIRD_HEIGHT - 9}, // Kirby
    {YAPPER_WIDTH,
     YAPPER_HEIGHT,
     4,
     4,
     YAPPER_WIDTH - 8,
     YAPPER_HEIGHT - 8} // Yapper with larger offset
};

const Icon* bird_sets[BirdTypeMAX][BirdStateMAX] = {
    {&I_bird_01, &I_bird_02, &I_bird_03},
    {&I_leaf_01, &I_leaf_02, &I_leaf_03},
    {&I_ghost_01, &I_ghost_02, &I_ghost_03},
    {&I_ghostesp_01, &I_ghostesp_02, &I_ghostesp_03},
    {&I_kirby_01, &I_kirby_02, &I_kirby_03},
    {&I_yapper_01, &I_yapper_02, &I_yapper_03}};

typedef enum {
    EventTypeTick,
    EventTypeKey,
} EventType;

typedef struct {
    int x;
    int y;
} POINT;

typedef struct {
    float gravity;
    POINT point;
} BIRD;

typedef struct {
    POINT point;
    int height;
    int visible;
    bool passed;
} PILAR;

typedef enum {
    GameStateStart,
    GameStateLife,
    GameStateGameOver,
} State;

typedef struct {
    BIRD bird;
    int points;
    int high_score;
    int pilars_count;
    PILAR pilars[FLAPPY_PILAR_MAX];
    bool debug;
    State state;
    FuriMutex* mutex;
    uint8_t collision_frame;
    BirdType selected_bird;
    bool in_bird_select;
} GameState;

typedef struct {
    EventType type;
    InputEvent input;
} GameEvent;

typedef enum {
    DirectionUp,
    DirectionRight,
    DirectionDown,
    DirectionLeft,
} Direction;

static void flappy_game_save_score(int score) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);

    if(storage_file_open(file, FLAPPY_SAVE_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_write(file, &score, sizeof(int));
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

static int flappy_game_load_score() {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    int score = 0;

    if(storage_file_open(file, FLAPPY_SAVE_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_read(file, &score, sizeof(int));
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return score;
}

static inline int get_gap_height(BirdType bird_type) {
    switch(bird_type) {
    case BirdType420:
        return GHOSTESP_GAB_HEIGHT + 1;
    case BirdTypeGhost:
        return YAPPER_GAB_HEIGHT;
    case BirdTypeGhostESP:
        return GHOSTESP_GAB_HEIGHT;
    case BirdTypeKirby:
        return FLAPPY_GAB_HEIGHT - 3;
    case BirdTypeYapper:
        return YAPPER_GAB_HEIGHT;
    default:
        return FLAPPY_GAB_HEIGHT;
    }
}

static void flappy_game_random_pilar(GameState* const game_state) {
    PILAR pilar;
    int gap_height = get_gap_height(game_state->selected_bird);

    pilar.passed = false;
    pilar.visible = 1;
    pilar.height = random() % (FLIPPER_LCD_HEIGHT - gap_height) + 1;
    pilar.point.y = 0;
    pilar.point.x = FLIPPER_LCD_WIDTH + FLAPPY_GAB_WIDTH + 1;

    game_state->pilars_count++;
    game_state->pilars[game_state->pilars_count % FLAPPY_PILAR_MAX] = pilar;
}

static void flappy_game_state_init(GameState* const game_state) {
    BIRD bird;
    bird.gravity = 0.0f;
    bird.point.x = 15;
    bird.point.y = 32;

    game_state->debug = DEBUG;
    game_state->bird = bird;
    game_state->pilars_count = 0;
    game_state->points = 0;
    game_state->state = GameStateStart;
    game_state->collision_frame = 0;
    game_state->in_bird_select = false;
    // Keep the selected bird between games
    if(game_state->selected_bird >= BirdTypeMAX) {
        game_state->selected_bird = BirdTypeDefault;
    }
    // Only load high score if it's not already loaded
    if(game_state->high_score == 0) {
        game_state->high_score = flappy_game_load_score();
    }
    memset(game_state->pilars, 0, sizeof(game_state->pilars));

    flappy_game_random_pilar(game_state);
}

static void flappy_game_state_free(GameState* const game_state) {
    free(game_state);
}

static bool check_collision(
    const GameState* game_state,
    const PILAR* pilar,
    CharacterDimensions dims,
    int gap_height) {
    // Calculate character's hitbox with offsets
    int char_left = game_state->bird.point.x + dims.hitbox_x_offset;
    int char_right = char_left + dims.hitbox_width;
    int char_top = game_state->bird.point.y + dims.hitbox_y_offset;
    int char_bottom = char_top + dims.hitbox_height;

    // Calculate pipe hitboxes
    int pipe_left = pilar->point.x + 1; // Small offset for visual alignment
    int pipe_right = pilar->point.x + FLAPPY_GAB_WIDTH - 1;
    int top_pipe_bottom = pilar->height;
    int bottom_pipe_top = pilar->height + gap_height;

    // No collision if we're not within the horizontal bounds of the pipes
    if(char_right < pipe_left || char_left > pipe_right) {
        return false;
    }

    // Extra forgiving collision for Yapper/Ghost
    if(game_state->selected_bird == BirdType420 || game_state->selected_bird == BirdTypeGhost ||
       game_state->selected_bird == BirdTypeGhostESP ||
       game_state->selected_bird == BirdTypeYapper) {
        // Add small grace area for top and bottom pipes
        top_pipe_bottom += 2;
        bottom_pipe_top -= 2;
    }

    // Collision with top pipe
    if(char_top < top_pipe_bottom) {
        return true;
    }

    // Collision with bottom pipe
    if(char_bottom > bottom_pipe_top) {
        return true;
    }

    return false;
}

static void flappy_game_tick(GameState* const game_state) {
    if(game_state->collision_frame > 0) {
        game_state->collision_frame--;
    }

    if(game_state->state == GameStateLife) {
        if(!game_state->debug) {
            game_state->bird.gravity += FLAPPY_GRAVITY_TICK;
            game_state->bird.point.y += game_state->bird.gravity;
        }

        CharacterDimensions dims = character_dimensions[game_state->selected_bird];
        int gap_height = get_gap_height(game_state->selected_bird);

        if(game_state->bird.point.y + game_state->bird.gravity <= 0) {
            // Hit the ceiling - game over
            game_state->state = GameStateGameOver;
            game_state->collision_frame = 4;
            if(game_state->points > game_state->high_score) {
                game_state->high_score = game_state->points;
                flappy_game_save_score(game_state->high_score);
            }
            return;
        }

        float max_y = FLIPPER_LCD_HEIGHT - dims.height;
        if(game_state->bird.point.y > max_y) {
            game_state->state = GameStateGameOver;
            game_state->collision_frame = 4;
            if(game_state->points > game_state->high_score) {
                game_state->high_score = game_state->points;
                flappy_game_save_score(game_state->high_score);
            }
            return;
        }

        // Check pilar spawning
        PILAR* last_pilar = &game_state->pilars[game_state->pilars_count % FLAPPY_PILAR_MAX];
        if(last_pilar->point.x == (FLIPPER_LCD_WIDTH - FLAPPY_PILAR_DIST)) {
            flappy_game_random_pilar(game_state);
        }

        // Process all pilars
        for(int i = 0; i < FLAPPY_PILAR_MAX; i++) {
            PILAR* pilar = &game_state->pilars[i];
            if(pilar->visible) {
                pilar->point.x--;

                if(!pilar->passed &&
                   game_state->bird.point.x >= pilar->point.x + FLAPPY_GAB_WIDTH) {
                    pilar->passed = true;
                    game_state->points++;
                }

                if(pilar->point.x < -FLAPPY_GAB_WIDTH) {
                    pilar->visible = 0;
                }

                if(check_collision(game_state, pilar, dims, gap_height)) {
                    game_state->state = GameStateGameOver;
                    game_state->collision_frame = 4;
                    if(game_state->points > game_state->high_score) {
                        game_state->high_score = game_state->points;
                        flappy_game_save_score(game_state->high_score);
                    }
                    break;
                }
            }
        }
    }
}
static void flappy_game_flap(GameState* const game_state) {
    game_state->bird.gravity = FLAPPY_GRAVITY_JUMP;
}

static void flappy_game_render_callback(Canvas* const canvas, void* ctx) {
    furi_assert(ctx);
    const GameState* game_state = ctx;
    furi_mutex_acquire(game_state->mutex, FuriWaitForever);

    canvas_draw_frame(canvas, 0, 0, 128, 64);

    if(game_state->state == GameStateStart) {
        if(!game_state->in_bird_select) {
            canvas_set_color(canvas, ColorWhite);
            canvas_draw_box(canvas, 14, 8, 100, 48);

            canvas_set_color(canvas, ColorBlack);
            canvas_draw_frame(canvas, 14, 8, 100, 48);
            canvas_set_font(canvas, FontPrimary);
            // Change title based on selected character
            if(game_state->selected_bird == BirdType420) {
                canvas_draw_str_aligned(canvas, 64, 20, AlignCenter, AlignBottom, "420");
            } else if(game_state->selected_bird == BirdTypeGhost) {
                canvas_draw_str_aligned(canvas, 64, 20, AlignCenter, AlignBottom, "Ghost");
            } else if(game_state->selected_bird == BirdTypeGhostESP) {
                canvas_draw_str_aligned(canvas, 64, 20, AlignCenter, AlignBottom, "Flappy Ghost");
            } else if(game_state->selected_bird == BirdTypeKirby) {
                canvas_draw_str_aligned(canvas, 64, 20, AlignCenter, AlignBottom, "Kirby");
            } else if(game_state->selected_bird == BirdTypeYapper) {
                canvas_draw_str_aligned(canvas, 64, 20, AlignCenter, AlignBottom, "Yappy Bird");
            } else {
                canvas_draw_str_aligned(canvas, 64, 20, AlignCenter, AlignBottom, "Flappy Bird");
            }
            canvas_set_font(canvas, FontSecondary);
            canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignBottom, "Press OK to start");
            canvas_draw_str_aligned(canvas, 64, 42, AlignCenter, AlignBottom, "^ to select char");

            if(game_state->high_score > 0) {
                char hi_buffer[24];
                snprintf(hi_buffer, sizeof(hi_buffer), "Best: %d", game_state->high_score);
                canvas_draw_str_aligned(canvas, 64, 52, AlignCenter, AlignBottom, hi_buffer);
            }
        } else {
            // Character selection menu with larger box
            canvas_set_color(canvas, ColorWhite);
            canvas_draw_box(canvas, 16, 4, 96, 56); // Much bigger box

            canvas_set_color(canvas, ColorBlack);
            canvas_draw_frame(canvas, 16, 4, 96, 56);

            // Title more space from top
            canvas_set_font(canvas, FontPrimary);
            canvas_draw_str_aligned(canvas, 64, 14, AlignCenter, AlignBottom, "Select Character");

            // Get current character dimensions
            CharacterDimensions dims = character_dimensions[game_state->selected_bird];

            // Centered position for preview with more vertical space
            int preview_x = 64 - (dims.width / 2);
            int preview_y = 32 - (dims.height / 2); // Center vertically

            // Draw character preview
            canvas_draw_icon(
                canvas, preview_x, preview_y, bird_sets[game_state->selected_bird][BirdState1]);

            // Draw selection arrows with more spacing
            canvas_draw_str_aligned(canvas, 26, 34, AlignCenter, AlignBottom, "<");
            canvas_draw_str_aligned(canvas, 102, 34, AlignCenter, AlignBottom, ">");

            canvas_set_font(canvas, FontSecondary);
            // Instructions pushed lower with more spacing
            canvas_draw_str_aligned(canvas, 64, 48, AlignCenter, AlignBottom, "</> to choose");
            canvas_draw_str_aligned(canvas, 64, 56, AlignCenter, AlignBottom, "OK to confirm");
        }
    }

    if(game_state->state == GameStateLife) {
        // Get current gap height for rendering
        int gap_height = get_gap_height(game_state->selected_bird);

        // Pilars
        for(int i = 0; i < FLAPPY_PILAR_MAX; i++) {
            const PILAR* pilar = &game_state->pilars[i];
            if(pilar != NULL && pilar->visible == 1) {
                canvas_draw_frame(
                    canvas, pilar->point.x, pilar->point.y, FLAPPY_GAB_WIDTH, pilar->height);

                canvas_draw_frame(
                    canvas, pilar->point.x + 1, pilar->point.y, FLAPPY_GAB_WIDTH, pilar->height);

                canvas_draw_frame(
                    canvas,
                    pilar->point.x + 2,
                    pilar->point.y,
                    FLAPPY_GAB_WIDTH - 1,
                    pilar->height);

                canvas_draw_frame(
                    canvas,
                    pilar->point.x,
                    pilar->point.y + pilar->height + gap_height,
                    FLAPPY_GAB_WIDTH,
                    FLIPPER_LCD_HEIGHT - pilar->height - gap_height);

                canvas_draw_frame(
                    canvas,
                    pilar->point.x + 1,
                    pilar->point.y + pilar->height + gap_height,
                    FLAPPY_GAB_WIDTH - 1,
                    FLIPPER_LCD_HEIGHT - pilar->height - gap_height);

                canvas_draw_frame(
                    canvas,
                    pilar->point.x + 2,
                    pilar->point.y + pilar->height + gap_height,
                    FLAPPY_GAB_WIDTH - 1,
                    FLIPPER_LCD_HEIGHT - pilar->height - gap_height);
            }
        }

        // Switch animation - Using character directly from bird_sets
        int bird_state = BirdState1;
        if(game_state->bird.gravity < -0.5)
            bird_state = BirdState0;
        else if(game_state->bird.gravity > 0.5)
            bird_state = BirdState2;

        // Get current character dimensions
        CharacterDimensions dims = character_dimensions[game_state->selected_bird];

        // Adjust Y position to keep character in bounds
        int adjusted_y = game_state->bird.point.y;
        if(adjusted_y < 0) adjusted_y = 0;
        if(adjusted_y > FLIPPER_LCD_HEIGHT - dims.height) {
            adjusted_y = FLIPPER_LCD_HEIGHT - dims.height;
        }

        // Draw the character with adjusted position
        canvas_draw_icon(
            canvas,
            game_state->bird.point.x,
            adjusted_y,
            bird_sets[game_state->selected_bird][bird_state]);

        // Score display
        canvas_set_font(canvas, FontSecondary);
        char buffer[12];
        snprintf(buffer, sizeof(buffer), "%u", game_state->points);
        canvas_draw_str_aligned(canvas, 64, 12, AlignCenter, AlignBottom, buffer);

        if(game_state->debug) {
            char coordinates[20];
            snprintf(coordinates, sizeof(coordinates), "Y: %u", adjusted_y);
            canvas_draw_str_aligned(canvas, 1, 12, AlignCenter, AlignBottom, coordinates);
        }
    }

    if(game_state->state == GameStateGameOver) {
        // Adjusted box height for exactly 3 lines of text
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_box(canvas, 24, 12, 82, 36); // Reduced height from 42 to 36

        canvas_set_color(canvas, ColorBlack);
        canvas_draw_frame(canvas, 24, 12, 82, 36);

        // Game Over text
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 22, AlignCenter, AlignBottom, "Game Over");

        // Current score
        canvas_set_font(canvas, FontSecondary);
        char buffer[12];
        snprintf(buffer, sizeof(buffer), "Score: %u", game_state->points);
        canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignBottom, buffer);

        // High score
        char hi_buffer[16];
        snprintf(hi_buffer, sizeof(hi_buffer), "Best: %u", game_state->high_score);
        canvas_draw_str_aligned(canvas, 64, 42, AlignCenter, AlignBottom, hi_buffer);

        // New Best! message (shown outside the box)
        if(game_state->points > game_state->high_score) {
            canvas_draw_str_aligned(canvas, 64, 52, AlignCenter, AlignBottom, "New Best!");
        }

        // Collision effect
        if(game_state->collision_frame > 0) {
            canvas_invert_color(canvas);
        }
    }
    furi_mutex_release(game_state->mutex);
}
static void flappy_game_input_callback(InputEvent* input_event, void* ctx) {
    furi_assert(ctx);
    FuriMessageQueue* event_queue = ctx;

    GameEvent event = {.type = EventTypeKey, .input = *input_event};
    furi_message_queue_put(event_queue, &event, FuriWaitForever);
}

static void flappy_game_update_timer_callback(void* ctx) {
    furi_assert(ctx);
    FuriMessageQueue* event_queue = ctx;

    GameEvent event = {.type = EventTypeTick};
    furi_message_queue_put(event_queue, &event, 0);
}
int32_t flappy_game_app(void* p) {
    UNUSED(p);
    int32_t return_code = 0;

    FuriMessageQueue* event_queue = furi_message_queue_alloc(8, sizeof(GameEvent));

    GameState* game_state = malloc(sizeof(GameState));
    flappy_game_state_init(game_state);

    game_state->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    if(!game_state->mutex) {
        FURI_LOG_E(TAG, "cannot create mutex\r\n");
        return_code = 255;
        goto free_and_exit;
    }

    // Set system callbacks
    ViewPort* view_port = view_port_alloc();
    view_port_draw_callback_set(view_port, flappy_game_render_callback, game_state);
    view_port_input_callback_set(view_port, flappy_game_input_callback, event_queue);

    FuriTimer* timer =
        furi_timer_alloc(flappy_game_update_timer_callback, FuriTimerTypePeriodic, event_queue);
    furi_timer_start(timer, furi_kernel_get_tick_frequency() / 25);

    // Open GUI and register view_port
    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    // Call dolphin deed on game start
    dolphin_deed(DolphinDeedPluginGameStart);

    GameEvent event;
    for(bool processing = true; processing;) {
        FuriStatus event_status = furi_message_queue_get(event_queue, &event, 100);
        furi_mutex_acquire(game_state->mutex, FuriWaitForever);

        if(event_status == FuriStatusOk) {
            // press events
            if(event.type == EventTypeKey) {
                if(event.input.type == InputTypePress) {
                    switch(event.input.key) {
                    case InputKeyUp:
                        if(game_state->state == GameStateStart && !game_state->in_bird_select) {
                            game_state->in_bird_select = true;
                        } else if(game_state->state == GameStateLife) {
                            flappy_game_flap(game_state);
                        }
                        break;

                    case InputKeyDown:
                        if(game_state->state == GameStateStart && game_state->in_bird_select) {
                            game_state->in_bird_select = false;
                        }
                        break;

                    case InputKeyLeft:
                        if(game_state->state == GameStateStart && game_state->in_bird_select) {
                            if(game_state->selected_bird == 0) {
                                game_state->selected_bird = BirdTypeMAX - 1;
                            } else {
                                game_state->selected_bird--;
                            }
                        }
                        break;

                    case InputKeyRight:
                        if(game_state->state == GameStateStart && game_state->in_bird_select) {
                            game_state->selected_bird =
                                (game_state->selected_bird + 1) % BirdTypeMAX;
                        }
                        break;

                    case InputKeyOk:
                        if(game_state->state == GameStateStart) {
                            if(game_state->in_bird_select) {
                                game_state->in_bird_select = false;
                            } else {
                                game_state->state = GameStateLife;
                            }
                        } else if(game_state->state == GameStateGameOver) {
                            flappy_game_state_init(game_state);
                        } else if(game_state->state == GameStateLife) {
                            flappy_game_flap(game_state);
                        }
                        break;

                    case InputKeyBack:
                        if(game_state->state == GameStateStart && game_state->in_bird_select) {
                            game_state->in_bird_select = false;
                        } else {
                            processing = false;
                        }
                        break;

                    default:
                        break;
                    }
                }
            } else if(event.type == EventTypeTick) {
                flappy_game_tick(game_state);
            }
        }

        furi_mutex_release(game_state->mutex);
        view_port_update(view_port);
    }

    furi_timer_free(timer);
    view_port_enabled_set(view_port, false);
    gui_remove_view_port(gui, view_port);
    furi_record_close(RECORD_GUI);
    view_port_free(view_port);
    furi_mutex_free(game_state->mutex);

free_and_exit:
    flappy_game_state_free(game_state);
    furi_message_queue_free(event_queue);

    return return_code;
}
