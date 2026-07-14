#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <gui/view.h>

#define LOG "InfiniteTicTacToe"

//////////////////////////////////
// Start Of Board Definition
//////////////////////////////////

#define SQUARE_INITIAL_LIFETIME 7
#define BOARD_DIM_SIZE          3
#define PLAYER_O_MASK           1

typedef struct {
    uint8_t lifetime : 3;
    uint8_t player   : 1;
} BoardSquare;

typedef struct {
    BoardSquare board[BOARD_DIM_SIZE][BOARD_DIM_SIZE];
    uint8_t pos_r      : 2;
    uint8_t pos_c      : 2;
    uint8_t inprogress : 1;
    uint8_t turn       : 1;
} GameState;

static GameState* game_state_alloc() {
    FURI_LOG_T(LOG, __func__);
    GameState* game_state = malloc(sizeof(GameState));

    game_state->turn = 0;
    game_state->inprogress = 1;
    game_state->pos_r = BOARD_DIM_SIZE / 2; // Center Square
    game_state->pos_c = BOARD_DIM_SIZE / 2;

    for(int r = 0; r < BOARD_DIM_SIZE; r++) {
        for(int c = 0; c < BOARD_DIM_SIZE; c++) {
            game_state->board[r][c].lifetime = 0;
        }
    }

    return game_state;
}

static void game_state_free(GameState* game_state) {
    FURI_LOG_T(LOG, __func__);
    free(game_state);
}

//////////////////////////////////
// Start Of Board Drawing
//////////////////////////////////

static void draw_cross(Canvas* const canvas, uint8_t x, uint8_t y, uint8_t lifetime) {
    FURI_LOG_T(LOG, __func__);

    canvas_draw_line(canvas, x, y, x + 9, y + 9); // top left - bottom right slash
    canvas_draw_line(canvas, x + 9, y, x, y + 9); // down left - top right slash

    // Give newer crosses thicker borders
    if(lifetime > 2) {
        canvas_draw_line(canvas, x, y + 1, x + 9, y + 10); // top left - bottom right slash
        canvas_draw_line(canvas, x + 9, y + 1, x, y + 10); // down left - top right slash

        canvas_draw_line(canvas, x, y - 1, x + 9, y + 8); // top left - bottom right slash
        canvas_draw_line(canvas, x + 9, y - 1, x, y + 8); // down left - top right slash
    }
}

static void draw_circle(Canvas* const canvas, uint8_t x, uint8_t y, uint8_t lifetime) {
    FURI_LOG_T(LOG, __func__);

    canvas_draw_circle(canvas, x + 4, y + 5, 5);

    // Give newer circles thicker borders
    if(lifetime > 2) {
        canvas_draw_circle(canvas, x + 4, y + 5, 6);
        canvas_draw_circle(canvas, x + 4, y + 5, 4);
    }
}

static void draw_callback(Canvas* const canvas, void* ctx) {
    FURI_LOG_T(LOG, __func__);
    furi_assert(canvas);
    furi_assert(ctx);

    GameState* game_state = ctx;

    // Draws the game field
    canvas_draw_frame(canvas, 0, 0, 64, 64); // frame
    canvas_draw_line(canvas, 0, 21, 63, 21); // horizontal line
    canvas_draw_line(canvas, 0, 42, 63, 42); // horizontal line
    canvas_draw_line(canvas, 21, 0, 21, 63); // vertical line
    canvas_draw_line(canvas, 42, 0, 42, 63); // vertical line

    // Draws the selection box
    uint8_t x_coord = 21 * game_state->pos_c + 1;
    uint8_t y_coord = 21 * game_state->pos_r + 1;

    canvas_set_color(canvas, ColorBlack);
    canvas_draw_frame(canvas, x_coord, y_coord, 20, 20);
    canvas_draw_frame(canvas, x_coord + 1, y_coord + 1, 18, 18);

    // Draws the sidebar
    canvas_set_font(canvas, FontSecondary);

    char sidebar_buffer[10];
    if(game_state->inprogress) {
        snprintf(
            sidebar_buffer,
            sizeof(sidebar_buffer),
            "%c\'s Turn",
            game_state->turn & PLAYER_O_MASK ? 'O' : 'X');
    } else {
        snprintf(
            sidebar_buffer,
            sizeof(sidebar_buffer),
            "%c Wins",
            game_state->turn & PLAYER_O_MASK ? 'O' : 'X');
    }
    canvas_draw_str(canvas, 80, 24, sidebar_buffer);

    for(int r = 0; r < BOARD_DIM_SIZE; r++) {
        for(int c = 0; c < BOARD_DIM_SIZE; c++) {
            if(game_state->board[r][c].lifetime == 0) continue;

            x_coord = 21 * c + 6;
            y_coord = 21 * r + 6;

            if(game_state->board[r][c].player & PLAYER_O_MASK) {
                draw_circle(canvas, x_coord, y_coord, game_state->board[r][c].lifetime);
            } else {
                draw_cross(canvas, x_coord, y_coord, game_state->board[r][c].lifetime);
            }
        }
    }
}

//////////////////////////////////
// Start Of Input Handling
//////////////////////////////////

typedef struct {
    InputEvent input;
} GameEvent;

static void input_callback(InputEvent* input_event, void* context) {
    FURI_LOG_T(LOG, __func__);
    furi_assert(context);
    FuriMessageQueue* event_queue = (FuriMessageQueue*)context;

    GameEvent event = {.input = *input_event};
    furi_message_queue_put(event_queue, &event, FuriWaitForever);
}

//////////////////////////////////
// Main
//////////////////////////////////

int32_t infinite_tic_tac_toe_app(void* p) {
    UNUSED(p);
    FURI_LOG_T(LOG, __func__);

    FuriMessageQueue* event_queue = furi_message_queue_alloc(8, sizeof(GameEvent));

    GameState* game_state = game_state_alloc();

    // Set system callbacks
    ViewPort* view_port = view_port_alloc();
    view_port_draw_callback_set(view_port, draw_callback, game_state);
    view_port_input_callback_set(view_port, input_callback, event_queue);

    // Open GUI and register view_port
    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    // Main Loop
    GameEvent event;
    bool processing = true;

    while(processing) {
        FuriStatus event_status = furi_message_queue_get(event_queue, &event, 100);

        if(event_status == FuriStatusOk && event.input.type == InputTypePress) {
            switch(event.input.key) {
            case InputKeyBack:
                processing = false;
                break;
            case InputKeyRight:
                if(game_state->pos_c < BOARD_DIM_SIZE - 1) {
                    game_state->pos_c++;
                }
                break;
            case InputKeyLeft:
                if(game_state->pos_c > 0) {
                    game_state->pos_c--;
                }
                break;
            case InputKeyUp:
                if(game_state->pos_r > 0) {
                    game_state->pos_r--;
                }
                break;
            case InputKeyDown:
                if(game_state->pos_r < BOARD_DIM_SIZE - 1) {
                    game_state->pos_r++;
                }
                break;
            case InputKeyOk:
                if(game_state->inprogress) {
                    // Play Sequence
                    // 1. Check that square's lifetime is 0
                    // 2. Place Piece
                    // 3. Decrement Pieces
                    // 4. Determine Winner
                    // 5. Switch Turns (if no winner)

                    if(game_state->board[game_state->pos_r][game_state->pos_c].lifetime != 0) {
                        break;
                    }

                    game_state->board[game_state->pos_r][game_state->pos_c].lifetime =
                        SQUARE_INITIAL_LIFETIME;
                    game_state->board[game_state->pos_r][game_state->pos_c].player =
                        game_state->turn;

                    for(int r = 0; r < BOARD_DIM_SIZE; r++) {
                        for(int c = 0; c < BOARD_DIM_SIZE; c++) {
                            if(game_state->board[r][c].lifetime > 0) {
                                game_state->board[r][c].lifetime--;
                            }
                        }
                    }

                    for(int i = 0; i < BOARD_DIM_SIZE; i++) {
                        game_state->inprogress &= !(
                            // Check all squares in the row have the same piece and the lifetime is not 0
                            game_state->board[i][0].lifetime != 0 &&
                            game_state->board[i][1].lifetime != 0 &&
                            game_state->board[i][2].lifetime != 0 &&
                            game_state->board[i][0].player == game_state->board[i][1].player &&
                            game_state->board[i][1].player == game_state->board[i][2].player);

                        game_state->inprogress &= !(
                            // Check all squares in the column have the same piece and the lifetime is not 0
                            game_state->board[0][i].lifetime != 0 &&
                            game_state->board[1][i].lifetime != 0 &&
                            game_state->board[2][i].lifetime != 0 &&
                            game_state->board[0][i].player == game_state->board[1][i].player &&
                            game_state->board[1][i].player == game_state->board[2][i].player);
                    }

                    game_state->inprogress &= !(
                        // Check all the squares on the major diagonal have the same piece and the lifetime is not 0
                        game_state->board[0][0].lifetime != 0 &&
                        game_state->board[1][1].lifetime != 0 &&
                        game_state->board[2][2].lifetime != 0 &&
                        game_state->board[0][0].player == game_state->board[1][1].player &&
                        game_state->board[1][1].player == game_state->board[2][2].player);

                    game_state->inprogress &= !(
                        // Check all the squares on the minor diagonal have the same piece and the lifetime is not 0
                        game_state->board[0][2].lifetime != 0 &&
                        game_state->board[1][1].lifetime != 0 &&
                        game_state->board[2][0].lifetime != 0 &&
                        game_state->board[0][2].player == game_state->board[1][1].player &&
                        game_state->board[1][1].player == game_state->board[2][0].player);

                    if(game_state->inprogress) {
                        game_state->turn = ~game_state->turn;
                    }
                }
                break;
            default:
                break;
            }
        }

        view_port_update(view_port);
    }

    view_port_enabled_set(view_port, false);
    gui_remove_view_port(gui, view_port);
    furi_record_close(RECORD_GUI);
    view_port_free(view_port);
    furi_message_queue_free(event_queue);
    game_state_free(game_state);

    return 0;
}
