#include "views/minesweeper_game_screen.h"
#include "engine/mine_sweeper_engine.h"
#include "gui/canvas.h"
#include "gui/view.h"
#include "minesweeper_redux_icons.h"
#include <furi.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

static const Icon* tile_icons[12] = {
    &I_tile_0_8x8,
    &I_tile_1_8x8,
    &I_tile_2_8x8,
    &I_tile_3_8x8,
    &I_tile_4_8x8,
    &I_tile_5_8x8,
    &I_tile_6_8x8,
    &I_tile_7_8x8,
    &I_tile_8_8x8,
    &I_tile_mine_8x8,
    &I_tile_flag_8x8,
    &I_tile_uncleared_8x8,
};

struct MineSweeperGameScreen {
    View* view;
    void* callback_context;
    GameScreenInputCallback callback;
};

typedef struct {
    MineSweeperState* game_state;

    // Used to track the projection of the board
    uint8_t right_boundary;
    uint8_t bottom_boundary;

    FuriString* info_str;
    uint32_t elapsed_ms;
    uint32_t elapsed_seconds;
    uint32_t last_tick;
    bool timer_running;
} MineSweeperGameScreenModel;

static uint32_t
    mine_sweeper_game_screen_get_elapsed_seconds(const MineSweeperGameScreenModel* model) {
    furi_assert(model);

    uint32_t elapsed_ms = model->elapsed_ms;
    if(model->timer_running) {
        elapsed_ms += furi_get_tick() - model->last_tick;
    }

    return elapsed_ms / 1000u;
}

static void mine_sweeper_game_screen_format_elapsed(FuriString* out, uint32_t elapsed_seconds) {
    furi_assert(out);

    const uint32_t max_display_seconds = (99u * 60u) + 59u;
    if(elapsed_seconds > max_display_seconds) {
        furi_string_set_str(out, "99:59+");
        return;
    }

    const uint32_t minutes = elapsed_seconds / 60u;
    const uint32_t seconds = elapsed_seconds % 60u;
    furi_string_printf(out, "%02lu:%02lu", (unsigned long)minutes, (unsigned long)seconds);
}

static void
    move_projection_boundary(MineSweeperState* game_state, MineSweeperGameScreenModel* model) {
    int16_t top_boundary = (int16_t)model->bottom_boundary - MINESWEEPER_SCREEN_TILE_HEIGHT;
    int16_t left_boundary = (int16_t)model->right_boundary - MINESWEEPER_SCREEN_TILE_WIDTH;

    bool is_outside_top_boundary = (int16_t)game_state->rt.cursor_row < top_boundary;
    bool is_outside_bottom_boundary = game_state->rt.cursor_row >= model->bottom_boundary;
    bool is_outside_left_boundary = (int16_t)game_state->rt.cursor_col < left_boundary;
    bool is_outside_right_boundary = game_state->rt.cursor_col >= model->right_boundary;

    if(is_outside_top_boundary) {
        model->bottom_boundary = game_state->rt.cursor_row + MINESWEEPER_SCREEN_TILE_HEIGHT;
    } else if(is_outside_bottom_boundary) {
        model->bottom_boundary = game_state->rt.cursor_row + 1;
    }

    if(is_outside_right_boundary) {
        model->right_boundary = game_state->rt.cursor_col + 1;
    } else if(is_outside_left_boundary) {
        model->right_boundary = game_state->rt.cursor_col + MINESWEEPER_SCREEN_TILE_WIDTH;
    }
}

static void mine_sweeper_game_screen_view_enter(void* context) {
    furi_assert(context);
    MineSweeperGameScreen* instance = context;
    const uint32_t now = furi_get_tick();

    with_view_model(
        instance->view,
        MineSweeperGameScreenModel * model,
        {
            if(model->game_state && model->game_state->rt.phase == MineSweeperPhasePlaying) {
                model->timer_running = true;
                model->last_tick = now;
            } else {
                model->timer_running = false;
            }
        },
        true);
}

static void mine_sweeper_game_screen_view_exit(void* context) {
    furi_assert(context);
    MineSweeperGameScreen* instance = context;
    const uint32_t now = furi_get_tick();

    with_view_model(
        instance->view,
        MineSweeperGameScreenModel * model,
        {
            if(model->timer_running) {
                model->elapsed_ms += now - model->last_tick;
                model->elapsed_seconds = model->elapsed_ms / 1000u;
                model->timer_running = false;
            }
        },
        false);
}

static bool mine_sweeper_game_screen_input_callback(InputEvent* event, void* context) {
    furi_assert(event);
    furi_assert(context);

    MineSweeperGameScreen* instance = context;
    MineSweeperEvent mapped_event;
    bool mapped = false;

    switch(event->key) {
    case InputKeyUp:
        if(event->type == InputTypePress || event->type == InputTypeRepeat) {
            mapped_event = MineSweeperEventMoveUp;
            mapped = true;
        }
        break;
    case InputKeyDown:
        if(event->type == InputTypePress || event->type == InputTypeRepeat) {
            mapped_event = MineSweeperEventMoveDown;
            mapped = true;
        }
        break;
    case InputKeyLeft:
        if(event->type == InputTypePress || event->type == InputTypeRepeat) {
            mapped_event = MineSweeperEventMoveLeft;
            mapped = true;
        }
        break;
    case InputKeyRight:
        if(event->type == InputTypePress || event->type == InputTypeRepeat) {
            mapped_event = MineSweeperEventMoveRight;
            mapped = true;
        }
        break;
    case InputKeyOk:
        if(event->type == InputTypeShort) {
            mapped_event = MineSweeperEventShortOkPress;
            mapped = true;
        } else if(event->type == InputTypeLong) {
            mapped_event = MineSweeperEventLongOkPress;
            mapped = true;
        }
        break;
    case InputKeyBack:
        if(event->type == InputTypeLong) {
            mapped_event = MineSweeperEventBackLong;
            mapped = true;
        }
        break;
    default:
        break;
    }

    if(!mapped || !instance->callback) {
        return false;
    }

    instance->callback(mapped_event, instance->callback_context);
    return true;
}

void mine_sweeper_game_screen_set_input_callback(
    MineSweeperGameScreen* instance,
    GameScreenInputCallback callback,
    void* context) {
    furi_assert(instance);
    instance->callback = callback;
    instance->callback_context = context;
}

static void mine_sweeper_game_screen_draw_callback(Canvas* canvas, void* _model) {
    furi_assert(canvas);
    furi_assert(_model);

    MineSweeperGameScreenModel* model = _model;

    furi_assert(model->game_state);

    MineSweeperState* game_state = model->game_state;

    canvas_clear(canvas);

    move_projection_boundary(game_state, model);

    uint16_t cursor_pos_1d =
        board_index(&game_state->board, game_state->rt.cursor_col, game_state->rt.cursor_row);

    for(uint8_t row_rel = 0; row_rel < MINESWEEPER_SCREEN_TILE_HEIGHT; row_rel++) {
        uint16_t row_abs = (model->bottom_boundary - MINESWEEPER_SCREEN_TILE_HEIGHT) + row_rel;

        for(uint8_t col_rel = 0; col_rel < MINESWEEPER_SCREEN_TILE_WIDTH; col_rel++) {
            uint16_t col_abs = (model->right_boundary - MINESWEEPER_SCREEN_TILE_WIDTH) + col_rel;

            uint16_t curr_rendering_tile_pos_1d =
                board_index(&game_state->board, (uint8_t)col_abs, (uint8_t)row_abs);

            MineSweeperCell cell = game_state->board.cells[curr_rendering_tile_pos_1d];

            if(cursor_pos_1d == curr_rendering_tile_pos_1d) {
                canvas_set_color(canvas, ColorWhite);
            } else {
                canvas_set_color(canvas, ColorBlack);
            }

            uint8_t is_cell_revealed = CELL_IS_REVEALED(cell);
            uint8_t is_cell_flagged = CELL_IS_FLAGGED(cell);
            uint8_t is_cell_mine = CELL_IS_MINE(cell);
            uint8_t neighbor_count = CELL_GET_NEIGHBORS(cell);

            if(is_cell_revealed) {
                if(is_cell_mine) {
                    canvas_draw_icon(canvas, col_rel * 8, row_rel * 8, tile_icons[9]);
                } else {
                    uint8_t icon_idx = neighbor_count <= 8 ? neighbor_count : 11;
                    canvas_draw_icon(canvas, col_rel * 8, row_rel * 8, tile_icons[icon_idx]);
                }
            } else if(is_cell_flagged) {
                canvas_draw_icon(canvas, col_rel * 8, row_rel * 8, tile_icons[10]);
            } else {
                canvas_draw_icon(canvas, col_rel * 8, row_rel * 8, tile_icons[11]);
            }
        }
    }

    canvas_set_color(canvas, ColorBlack);

    // Right border
    if(model->right_boundary == game_state->board.width) {
        canvas_draw_line(canvas, 127, 0, 127, 63 - 8);
    }

    // Left border
    if((model->right_boundary - MINESWEEPER_SCREEN_TILE_WIDTH) == 0) {
        canvas_draw_line(canvas, 0, 0, 0, 63 - 8);
    }

    // Bottom border
    if(model->bottom_boundary == game_state->board.height) {
        canvas_draw_line(canvas, 0, 63 - 8, 127, 63 - 8);
    }

    // Top border
    if((model->bottom_boundary - MINESWEEPER_SCREEN_TILE_HEIGHT) == 0) {
        canvas_draw_line(canvas, 0, 0, 127, 0);
    }

    if(game_state->rt.phase == MineSweeperPhasePlaying) {
        // Draw X Position Text
        furi_string_printf(model->info_str, "X:%03hhd", game_state->rt.cursor_col);

        canvas_draw_str_aligned(
            canvas, 0, 64 - 7, AlignLeft, AlignTop, furi_string_get_cstr(model->info_str));

        // Draw Y Position Text
        furi_string_printf(model->info_str, "Y:%03hhd", game_state->rt.cursor_row);

        canvas_draw_str_aligned(
            canvas, 33, 64 - 7, AlignLeft, AlignTop, furi_string_get_cstr(model->info_str));

        // Draw flag text
        furi_string_printf(model->info_str, "F:%03hd", game_state->rt.flags_left);

        canvas_draw_str_aligned(
            canvas, 66, 64 - 7, AlignLeft, AlignTop, furi_string_get_cstr(model->info_str));

    } else {
        const char* status_str = game_state->rt.phase == MineSweeperPhaseWon ? "Won! Press Ok" :
                                                                               "Lost! Press Ok";

        canvas_draw_str_aligned(canvas, 0, 64 - 7, AlignLeft, AlignTop, status_str);
    }
    const uint32_t elapsed_seconds = mine_sweeper_game_screen_get_elapsed_seconds(model);
    mine_sweeper_game_screen_format_elapsed(model->info_str, elapsed_seconds);
    canvas_draw_str_aligned(
        canvas, 127, 64 - 7, AlignRight, AlignTop, furi_string_get_cstr(model->info_str));
}

MineSweeperGameScreen* mine_sweeper_game_screen_alloc(void) {
    MineSweeperGameScreen* mine_sweeper_game_screen =
        (MineSweeperGameScreen*)malloc(sizeof(MineSweeperGameScreen));
    if(!mine_sweeper_game_screen) return NULL;

    memset(mine_sweeper_game_screen, 0, sizeof(MineSweeperGameScreen));

    mine_sweeper_game_screen->view = view_alloc();
    if(mine_sweeper_game_screen->view == NULL) return NULL;

    view_set_context(mine_sweeper_game_screen->view, mine_sweeper_game_screen);
    view_allocate_model(
        mine_sweeper_game_screen->view, ViewModelTypeLocking, sizeof(MineSweeperGameScreenModel));
    view_set_draw_callback(mine_sweeper_game_screen->view, mine_sweeper_game_screen_draw_callback);
    view_set_input_callback(
        mine_sweeper_game_screen->view, mine_sweeper_game_screen_input_callback);
    view_set_enter_callback(mine_sweeper_game_screen->view, mine_sweeper_game_screen_view_enter);
    view_set_exit_callback(mine_sweeper_game_screen->view, mine_sweeper_game_screen_view_exit);

    with_view_model(
        mine_sweeper_game_screen->view,
        MineSweeperGameScreenModel * model,
        {
            memset(model, 0, sizeof(MineSweeperGameScreenModel));
            model->info_str = furi_string_alloc();
            if(!model->info_str) return NULL;
        },
        false);

    return mine_sweeper_game_screen;
}

void mine_sweeper_game_screen_free(MineSweeperGameScreen* instance) {
    furi_assert(instance);

    with_view_model(
        instance->view,
        MineSweeperGameScreenModel * model,
        {
            if(model->info_str) furi_string_free(model->info_str);
        },
        false);

    if(instance->view) view_free(instance->view);

    if(instance) free(instance);
}

void mine_sweeper_game_screen_reset(MineSweeperGameScreen* instance) {
    UNUSED(instance);
}

void mine_sweeper_game_screen_reset_clock(MineSweeperGameScreen* instance) {
    furi_assert(instance);

    with_view_model(
        instance->view,
        MineSweeperGameScreenModel * model,
        {
            model->elapsed_ms = 0;
            model->elapsed_seconds = 0;
            model->last_tick = furi_get_tick();
            model->timer_running = false;
        },
        true);
}

void mine_sweeper_game_screen_update_clock(MineSweeperGameScreen* instance) {
    furi_assert(instance);
    const uint32_t now = furi_get_tick();
    bool need_redraw = false;

    with_view_model(
        instance->view,
        MineSweeperGameScreenModel * model,
        {
            if(model->timer_running) {
                model->elapsed_ms += now - model->last_tick;
                model->last_tick = now;

                if(model->game_state && model->game_state->rt.phase != MineSweeperPhasePlaying) {
                    model->timer_running = false;
                }

                const uint32_t elapsed_seconds = model->elapsed_ms / 1000u;
                if(elapsed_seconds != model->elapsed_seconds) {
                    model->elapsed_seconds = elapsed_seconds;
                    need_redraw = true;
                }
            }
        },
        need_redraw);
}

View* mine_sweeper_game_screen_get_view(MineSweeperGameScreen* instance) {
    furi_assert(instance);
    return instance->view;
}

void mine_sweeper_game_screen_set_context(
    MineSweeperGameScreen* instance,
    MineSweeperState* context) {
    furi_assert(instance);
    with_view_model(
        instance->view,
        MineSweeperGameScreenModel * model,
        {
            model->game_state = context;

            if(context) {
                model->right_boundary = MINESWEEPER_SCREEN_TILE_WIDTH;
                model->bottom_boundary = MINESWEEPER_SCREEN_TILE_HEIGHT;
                if(model->right_boundary > context->board.width) {
                    model->right_boundary = context->board.width;
                }
                if(model->bottom_boundary > context->board.height) {
                    model->bottom_boundary = context->board.height;
                }
            } else {
                model->right_boundary = MINESWEEPER_SCREEN_TILE_WIDTH;
                model->bottom_boundary = MINESWEEPER_SCREEN_TILE_HEIGHT;
            }
        },
        true);
}
