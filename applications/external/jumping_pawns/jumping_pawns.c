#include <furi.h>
#include <furi_hal.h>
#include <gui/canvas.h>
#include <gui/gui.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <gui/modules/widget.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>
#include <input/input.h>
#include <storage/storage.h>
#include "engine.h"
#include "jumping_pawns_icons.h"

typedef enum {
    JumpingPawnsSubmenu,
    JumpingPawnsDifficultySubmenu,
    JumpingPawnsNotificationsSubmenu,
    JumpingPawnsPlayView,
    JumpingPawnsViewTutorial,
    JumpingPawnsAbout,
} JumpingPawnsViews;

typedef enum {
    JumpingPawnsPlayPvPIndex,
    JumpingPawnsPlayPvEIndex,
    JumpingPawnsTutorialIndex,
    JumpingPawnsAboutIndex,
} JumpingPawnsSubmenuIndex;

typedef enum {
    JumpingPawnsEasyIndex,
    JumpingPawnsMediumIndex,
    JumpingPawnsHardIndex,
} JumpingPawnsDifficultyIndex;

typedef enum {
    JumpingPawnsNoIndex,
    JumpingPawnsLEDIndex,
    JumpingPawnsVibroIndex,
    JumpingPawnsBothIndex,
} JumpingPawnsNotifictionsIndex;

typedef enum {
    JumpingPawnsRedrawEventId = 0
} JumpingPawnsEventId;

typedef struct {
    ViewDispatcher* view_dispatcher;
    NotificationApp* notifications;
    Submenu* submenu;
    Submenu* difficulty_submenu;
    Submenu* notifications_submenu;
    View* view;
    Widget* widget_tutorial;
    Widget* widget_about;
} JumpingPawnsApp;

static void led_notify(void* context) {
    JumpingPawnsApp* app = (JumpingPawnsApp*)context;
    notification_message(app->notifications, &sequence_blink_red_100);
}

static void vibro_notify(void* context) {
    JumpingPawnsApp* app = (JumpingPawnsApp*)context;
    notification_message(app->notifications, &sequence_single_vibro);
}

void end_turn(void* context) {
    JumpingPawnsApp* app = (JumpingPawnsApp*)context;
    JumpingPawnsModel* model = view_get_model(app->view);
    if(strcmp(model->ai_or_player, "player") == 0) {
        if(strcmp(model->whose_turn, "player1") == 0) {
            model->whose_turn = "player2";
        } else if(strcmp(model->whose_turn, "player2") == 0) {
            model->whose_turn = "player1";
        }
    }
}

void find_jumps(int board[11][6], int x, int y) {
    int dx[] = {0, 0, -1, 1}; // directions: up, down, left, right
    int dy[] = {-1, 1, 0, 0};

    for(int dir = 0; dir < 4; dir++) {
        int first_pawn_x = x + dx[dir];
        int first_pawn_y = y + dy[dir];

        // Check if the first square in this direction contains a pawn (not empty and not a move marker)
        if(first_pawn_x < 0 || first_pawn_x >= 6 || first_pawn_y < 0 || first_pawn_y >= 11 ||
           board[first_pawn_y][first_pawn_x] == 0 || board[first_pawn_y][first_pawn_x] == 3) {
            continue;
        }

        // Now look for the first empty space after the contiguous block of pawns
        int next_x = first_pawn_x + dx[dir];
        int next_y = first_pawn_y + dy[dir];

        while(next_x >= 0 && next_x < 6 && next_y >= 0 && next_y < 11 &&
              board[next_y][next_x] != 0 && board[next_y][next_x] != 3) {
            next_x += dx[dir];
            next_y += dy[dir];
        }

        int landing_x = next_x;
        int landing_y = next_y;

        // Must jump at least one pawn, and the landing space must be empty and on the board
        if((abs(landing_x - x) <= 1 && abs(landing_y - y) <= 1) || landing_x < 0 ||
           landing_x >= 6 || landing_y < 0 || landing_y >= 11 ||
           board[landing_y][landing_x] != 0) {
            continue;
        }

        // Mark as a valid jump destination
        board[landing_y][landing_x] = 3;

        // Temporarily remove the pawn(s) jumped over for this direction
        int temp_x = first_pawn_x;
        int temp_y = first_pawn_y;
        // Save the pawns' values to restore later (for multi-pawn jump)
        int jumped_pawns[11][6] = {0};
        while(temp_x >= 0 && temp_x < 6 && temp_y >= 0 && temp_y < 11 &&
              board[temp_y][temp_x] != 0 && board[temp_y][temp_x] != 3) {
            jumped_pawns[temp_y][temp_x] = board[temp_y][temp_x];
            board[temp_y][temp_x] = 0;
            temp_x += dx[dir];
            temp_y += dy[dir];
        }

        // Recurse from the new position
        find_jumps(board, landing_x, landing_y);

        // Restore the jumped pawns
        temp_x = first_pawn_x;
        temp_y = first_pawn_y;
        while(temp_x >= 0 && temp_x < 6 && temp_y >= 0 && temp_y < 11 &&
              jumped_pawns[temp_y][temp_x] != 0) {
            board[temp_y][temp_x] = jumped_pawns[temp_y][temp_x];
            temp_x += dx[dir];
            temp_y += dy[dir];
        }
    }
}

void check_for_game_end(void* context) {
    JumpingPawnsApp* app = (JumpingPawnsApp*)context;
    JumpingPawnsModel* model = view_get_model(app->view);

    bool player_1_win = true;
    bool player_2_win = true;

    // Check top 2 rows (y = 0, 1) for all 1s (player pawns)
    for(int y = 0; y <= 1; y++) {
        for(int x = 0; x < 6; x++) {
            if(model->board_state[y][x] != 1) {
                player_1_win = false;
                break;
            }
        }
    }

    // Check bottom 2 rows (y = 9, 10) for all 2s (ai / player2 pawns)
    for(int y = 9; y <= 10; y++) {
        for(int x = 0; x < 6; x++) {
            if(model->board_state[y][x] != 2) {
                player_2_win = false;
                break;
            }
        }
    }

    if(player_1_win) {
        model->game_over = true;
        model->player_1_win = true;
    } else if(player_2_win) {
        model->game_over = true;
        model->player_2_win = true;
    }
}

void player_moving(void* context) {
    JumpingPawnsApp* app = (JumpingPawnsApp*)context;
    JumpingPawnsModel* model = view_get_model(app->view);
    int flipped[11][6];

    // Convert from 1-based to 0-based coordinates
    int sx = model->selected_x - 1;
    int sy = model->selected_y - 1;

    // Flip board if player 2
    if(strcmp(model->whose_turn, "player2") == 0) {
        sx = 5 - sx;
        sy = 10 - sy;
    }

    // Move piece if destination is a jump marker
    if(model->board_state[sy][sx] == 3) {
        int orig_x = model->last_calculated_piece_x - 1;
        int orig_y = model->last_calculated_piece_y - 1;
        model->board_state[orig_y][orig_x] = 0;

        int piece_value = strcmp(model->whose_turn, "player2") == 0 ? 2 : 1;
        model->board_state[sy][sx] = piece_value;

        // Clear jump markers
        for(int y = 0; y < 11; y++) {
            for(int x = 0; x < 6; x++) {
                if(model->board_state[y][x] == 3) {
                    model->board_state[y][x] = 0;
                }
            }
        }

        check_for_game_end(context);

        if(strcmp(model->ai_or_player, "ai") == 0) {
            model->is_ai_thinking = true;
            with_view_model(app->view, model, {}, true);
            ai_move(model);
            if(model->led_notifications) {
                led_notify(app);
            }
            if(model->vibro_notifications) {
                vibro_notify(app);
            }
        } else {
            end_turn(app);
        }

        return;
    }

    // Clear previous jump markers
    for(int y = 0; y < 11; y++) {
        for(int x = 0; x < 6; x++) {
            if(model->board_state[y][x] == 3) {
                model->board_state[y][x] = 0;
            }
        }
    }

    if(strcmp(model->whose_turn, "player2") == 0) {
        // Build flipped board for Player 2's perspective
        for(int i = 0; i < 11; i++) {
            for(int j = 0; j < 6; j++) {
                int val = model->board_state[10 - i][5 - j];
                flipped[i][j] = (val == 3) ? 0 : val;
            }
        }

        // Convert already flipped coordinates back to flipped board
        int flipped_sx = 5 - sx;
        int flipped_sy = 10 - sy;

        if(flipped[flipped_sy][flipped_sx] != 2) return;

        // Set last calculated origin
        model->last_calculated_piece_x = 7 - model->selected_x;
        model->last_calculated_piece_y = 12 - model->selected_y;

        find_jumps(flipped, flipped_sx, flipped_sy);

        // Map back jump markers
        for(int i = 0; i < 11; i++) {
            for(int j = 0; j < 6; j++) {
                if(flipped[i][j] == 3) {
                    model->board_state[10 - i][5 - j] = 3;
                }
            }
        }

    } else {
        // Player 1
        if(model->board_state[sy][sx] != 1) return;

        model->last_calculated_piece_x = model->selected_x;
        model->last_calculated_piece_y = model->selected_y;

        find_jumps(model->board_state, sx, sy);
    }
}

static uint32_t jumping_pawns_navigation_exit_callback(void* _context) {
    UNUSED(_context);
    return VIEW_NONE;
}

static uint32_t navigation_submenu_callback(void* _context) {
    UNUSED(_context);
    return JumpingPawnsSubmenu;
}

static uint32_t notifications_navigation_submenu_callback(void* _context) {
    UNUSED(_context);
    return JumpingPawnsDifficultySubmenu;
}

static void submenu_callback(void* context, uint32_t index) {
    JumpingPawnsApp* app = (JumpingPawnsApp*)context;
    JumpingPawnsModel* model = view_get_model(app->view);

    int temp_board[11][6] = {
        {2, 2, 2, 2, 2, 2},
        {2, 2, 2, 2, 2, 2},
        {0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0},
        {1, 1, 1, 1, 1, 1},
        {1, 1, 1, 1, 1, 1}};

    // reset board
    memcpy(model->board_state, temp_board, sizeof(temp_board));

    switch(index) {
    case JumpingPawnsPlayPvPIndex:
        model->ai_or_player = "player";
        model->whose_turn = "player1";
        view_dispatcher_switch_to_view(app->view_dispatcher, JumpingPawnsPlayView);
        break;
    case JumpingPawnsPlayPvEIndex:
        model->ai_or_player = "ai";
        model->whose_turn = "player";
        view_dispatcher_switch_to_view(app->view_dispatcher, JumpingPawnsDifficultySubmenu);
        break;
    case JumpingPawnsTutorialIndex:
        view_dispatcher_switch_to_view(app->view_dispatcher, JumpingPawnsViewTutorial);
        break;
    case JumpingPawnsAboutIndex:
        view_dispatcher_switch_to_view(app->view_dispatcher, JumpingPawnsAbout);
        break;
    default:
        break;
    }
}

static void difficulty_submenu_callback(void* context, uint32_t index) {
    JumpingPawnsApp* app = (JumpingPawnsApp*)context;
    JumpingPawnsModel* model = view_get_model(app->view);

    int temp_board[11][6] = {
        {2, 2, 2, 2, 2, 2},
        {2, 2, 2, 2, 2, 2},
        {0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0},
        {1, 1, 1, 1, 1, 1},
        {1, 1, 1, 1, 1, 1}};

    // reset board
    memcpy(model->board_state, temp_board, sizeof(temp_board));

    switch(index) {
    case JumpingPawnsEasyIndex:
        model->difficulty_level = 1;
        view_dispatcher_switch_to_view(app->view_dispatcher, JumpingPawnsNotificationsSubmenu);
        break;
    case JumpingPawnsMediumIndex:
        model->difficulty_level = 2;
        view_dispatcher_switch_to_view(app->view_dispatcher, JumpingPawnsNotificationsSubmenu);
        break;
    case JumpingPawnsHardIndex:
        model->difficulty_level = 3;
        view_dispatcher_switch_to_view(app->view_dispatcher, JumpingPawnsNotificationsSubmenu);
        break;
    default:
        break;
    }
}

static void notifications_submenu_callback(void* context, uint32_t index) {
    JumpingPawnsApp* app = (JumpingPawnsApp*)context;
    JumpingPawnsModel* model = view_get_model(app->view);

    switch(index) {
    case JumpingPawnsNoIndex:
        model->led_notifications = false;
        model->vibro_notifications = false;
        view_dispatcher_switch_to_view(app->view_dispatcher, JumpingPawnsPlayView);
        break;
    case JumpingPawnsLEDIndex:
        model->led_notifications = true;
        model->vibro_notifications = false;
        view_dispatcher_switch_to_view(app->view_dispatcher, JumpingPawnsPlayView);
        break;
    case JumpingPawnsVibroIndex:
        model->led_notifications = false;
        model->vibro_notifications = true;
        view_dispatcher_switch_to_view(app->view_dispatcher, JumpingPawnsPlayView);
        break;
    case JumpingPawnsBothIndex:
        model->led_notifications = true;
        model->vibro_notifications = true;
        view_dispatcher_switch_to_view(app->view_dispatcher, JumpingPawnsPlayView);
        break;
    default:
        break;
    }
}

static void jumping_pawns_draw_callback(Canvas* canvas, void* model) {
    JumpingPawnsModel* my_model = (JumpingPawnsModel*)model;

    canvas_clear(canvas);
    canvas_set_font(canvas, FontSecondary);

    if(my_model->game_over) {
        canvas_clear(canvas);
        if(strcmp(my_model->ai_or_player, "ai") == 0) {
            if(my_model->player_1_win) {
                canvas_draw_str(canvas, 5, 62, "Player 1 wins!");
            } else if(my_model->player_2_win) {
                canvas_draw_str(canvas, 15, 62, "AI wins!");
            }
        } else {
            if(my_model->player_1_win) {
                canvas_draw_str(canvas, 5, 62, "Player 1 wins!");
            } else if(my_model->player_2_win) {
                canvas_draw_str(canvas, 5, 62, "Player 2 wins!");
            }
        }
        return;
    }

    // draw gameboard
    int squares_wide = 6;
    int squares_tall = 11;
    int square_size = 10;
    int x_offset = 2;
    int y_offset = 2;

    // draw vertical lines
    for(int i = 0; i <= squares_wide; i++) {
        int x = x_offset + (square_size * i);
        canvas_draw_line(canvas, x, y_offset, x, y_offset + (square_size * squares_tall));
    }

    // draw horizontal lines
    for(int j = 0; j <= squares_tall; j++) {
        int y = y_offset + (square_size * j);
        canvas_draw_line(canvas, x_offset, y, x_offset + (square_size * squares_wide), y);
    }

    // flip board if player2
    if(strcmp(my_model->whose_turn, "player2") == 0) {
        int flipped[11][6];

        for(int i = 0; i < 11; i++) {
            for(int j = 0; j < 6; j++) {
                flipped[i][j] = my_model->board_state[10 - i][5 - j];
            }
        }

        // draw pieces / dot to move to based on board state var
        for(int y = 0; y < 11; y++) {
            for(int x = 0; x < 6; x++) {
                int draw_x = x_offset + (square_size * x) + 1;
                int draw_y = y_offset + (square_size * y) + 1;

                if(flipped[y][x] == 1) {
                    canvas_draw_icon(canvas, draw_x, draw_y, &I_pawn1);
                } else if(flipped[y][x] == 2) {
                    canvas_draw_icon(canvas, draw_x, draw_y, &I_pawn2);
                } else if(flipped[y][x] == 3) {
                    canvas_draw_icon(canvas, draw_x, draw_y, &I_dot);
                }
            }
        }

        if(flipped[my_model->selected_y - 1][my_model->selected_x - 1] == 3) {
            canvas_draw_icon(
                canvas, (10 * my_model->selected_x) - 7, (10 * my_model->selected_y) - 7, &I_dot);
        }

        // draw border around selected square / pawn / dot
        if(flipped[my_model->selected_y - 1][my_model->selected_x - 1] == 1) {
            canvas_draw_icon(
                canvas,
                (10 * my_model->selected_x) - 7,
                (10 * my_model->selected_y) - 7,
                &I_pawn1selected);
        } else if(flipped[my_model->selected_y - 1][my_model->selected_x - 1] == 2) {
            canvas_draw_icon(
                canvas,
                (10 * my_model->selected_x) - 7,
                (10 * my_model->selected_y) - 7,
                &I_pawn2selected);
        } else if(flipped[my_model->selected_y - 1][my_model->selected_x - 1] == 3) {
            canvas_draw_icon(
                canvas,
                (10 * my_model->selected_x) - 7,
                (10 * my_model->selected_y) - 7,
                &I_selecteddot);
        } else {
            canvas_draw_icon(
                canvas,
                (10 * my_model->selected_x) - 7,
                (10 * my_model->selected_y) - 7,
                &I_frame1);
        }

    } else {
        // draw pieces / dot to move to based on board state var
        for(int y = 0; y < 11; y++) {
            for(int x = 0; x < 6; x++) {
                int draw_x = x_offset + (square_size * x) + 1;
                int draw_y = y_offset + (square_size * y) + 1;

                if(my_model->board_state[y][x] == 1) {
                    canvas_draw_icon(canvas, draw_x, draw_y, &I_pawn1);
                } else if(my_model->board_state[y][x] == 2) {
                    canvas_draw_icon(canvas, draw_x, draw_y, &I_pawn2);
                } else if(my_model->board_state[y][x] == 3) {
                    canvas_draw_icon(canvas, draw_x, draw_y, &I_dot);
                }
            }
        }

        if(my_model->board_state[my_model->selected_y - 1][my_model->selected_x - 1] == 3) {
            canvas_draw_icon(
                canvas, (10 * my_model->selected_x) - 7, (10 * my_model->selected_y) - 7, &I_dot);
        }

        // draw border around selected square / pawn / dot
        if(my_model->board_state[my_model->selected_y - 1][my_model->selected_x - 1] == 1) {
            canvas_draw_icon(
                canvas,
                (10 * my_model->selected_x) - 7,
                (10 * my_model->selected_y) - 7,
                &I_pawn1selected);
        } else if(my_model->board_state[my_model->selected_y - 1][my_model->selected_x - 1] == 2) {
            canvas_draw_icon(
                canvas,
                (10 * my_model->selected_x) - 7,
                (10 * my_model->selected_y) - 7,
                &I_pawn2selected);
        } else if(my_model->board_state[my_model->selected_y - 1][my_model->selected_x - 1] == 3) {
            canvas_draw_icon(
                canvas,
                (10 * my_model->selected_x) - 7,
                (10 * my_model->selected_y) - 7,
                &I_selecteddot);
        } else {
            canvas_draw_icon(
                canvas,
                (10 * my_model->selected_x) - 7,
                (10 * my_model->selected_y) - 7,
                &I_frame1);
        }
    }

    if(strcmp(my_model->ai_or_player, "ai") == 0) {
        if(my_model->is_ai_thinking) {
            canvas_draw_str(canvas, 5, 123, "Thinking...");
        } else {
            canvas_draw_str(canvas, 5, 123, "Your move!");
        }
    }
}

static bool jumping_pawns_input_callback(InputEvent* event, void* context) {
    JumpingPawnsApp* app = (JumpingPawnsApp*)context;
    JumpingPawnsModel* model = view_get_model(app->view);

    bool redraw = true;
    if(event->type == InputTypeShort) {
        switch(event->key) {
        case InputKeyUp:
            with_view_model(
                app->view,
                model,
                {
                    if(model->selected_y == 1) {
                        model->selected_y = 11;
                    } else {
                        model->selected_y -= 1;
                    }
                },
                redraw);
            break;
        case InputKeyDown:
            with_view_model(
                app->view,
                model,
                {
                    if(model->selected_y == 11) {
                        model->selected_y = 1;
                    } else {
                        model->selected_y += 1;
                    }
                },
                redraw);
            break;
        case InputKeyLeft:
            with_view_model(
                app->view,
                model,
                {
                    if(model->selected_x == 1) {
                        model->selected_x = 6;
                    } else {
                        model->selected_x -= 1;
                    }
                },
                redraw);
            break;
        case InputKeyRight:
            with_view_model(
                app->view,
                model,
                {
                    if(model->selected_x == 6) {
                        model->selected_x = 1;
                    } else {
                        model->selected_x += 1;
                    }
                },
                redraw);
            break;
        case InputKeyOk:
            with_view_model(app->view, model, { player_moving(app); }, redraw);
            return true;
        default:
            break;
        }
    } else if(event->type == InputTypeRepeat) {
        switch(event->key) {
        case InputKeyUp:
            with_view_model(
                app->view,
                model,
                {
                    if(model->selected_y == 1) {
                        model->selected_y = 11;
                    } else {
                        model->selected_y -= 1;
                    }
                },
                redraw);
            break;
        case InputKeyDown:
            with_view_model(
                app->view,
                model,
                {
                    if(model->selected_y == 11) {
                        model->selected_y = 1;
                    } else {
                        model->selected_y += 1;
                    }
                },
                redraw);
            break;
        case InputKeyLeft:
            with_view_model(
                app->view,
                model,
                {
                    if(model->selected_x == 1) {
                        model->selected_x = 6;
                    } else {
                        model->selected_x -= 1;
                    }
                },
                redraw);
            break;
        case InputKeyRight:
            with_view_model(
                app->view,
                model,
                {
                    if(model->selected_x == 6) {
                        model->selected_x = 1;
                    } else {
                        model->selected_x += 1;
                    }
                },
                redraw);
            break;
        default:
            break;
        }
    }

    return false;
}

static bool jumping_pawns_custom_event_callback(uint32_t event, void* context) {
    JumpingPawnsApp* app = (JumpingPawnsApp*)context;

    switch(event) {
    case JumpingPawnsRedrawEventId: {
        bool redraw = true;
        with_view_model(app->view, JumpingPawnsModel * _model, { UNUSED(_model); }, redraw);
        return true;
    }
    default:
        return false;
    }
}

static JumpingPawnsApp* jumping_pawns_alloc() {
    JumpingPawnsApp* app = (JumpingPawnsApp*)malloc(sizeof(JumpingPawnsApp));

    Gui* gui = furi_record_open(RECORD_GUI);

    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_attach_to_gui(app->view_dispatcher, gui, ViewDispatcherTypeFullscreen);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);

    app->submenu = submenu_alloc();
    submenu_add_item(app->submenu, "Play PvP", JumpingPawnsPlayPvPIndex, submenu_callback, app);
    submenu_add_item(app->submenu, "Play PvE", JumpingPawnsPlayPvEIndex, submenu_callback, app);
    submenu_add_item(app->submenu, "Tutorial", JumpingPawnsTutorialIndex, submenu_callback, app);
    submenu_add_item(app->submenu, "About", JumpingPawnsAboutIndex, submenu_callback, app);
    view_set_previous_callback(
        submenu_get_view(app->submenu), jumping_pawns_navigation_exit_callback);
    view_dispatcher_add_view(
        app->view_dispatcher, JumpingPawnsSubmenu, submenu_get_view(app->submenu));
    view_dispatcher_switch_to_view(app->view_dispatcher, JumpingPawnsSubmenu);

    app->difficulty_submenu = submenu_alloc();
    submenu_add_item(
        app->difficulty_submenu, "Easy", JumpingPawnsEasyIndex, difficulty_submenu_callback, app);
    submenu_add_item(
        app->difficulty_submenu,
        "Medium",
        JumpingPawnsMediumIndex,
        difficulty_submenu_callback,
        app);
    submenu_add_item(
        app->difficulty_submenu, "Hard", JumpingPawnsHardIndex, difficulty_submenu_callback, app);
    view_set_previous_callback(
        submenu_get_view(app->difficulty_submenu), navigation_submenu_callback);
    view_dispatcher_add_view(
        app->view_dispatcher,
        JumpingPawnsDifficultySubmenu,
        submenu_get_view(app->difficulty_submenu));

    app->notifications_submenu = submenu_alloc();
    submenu_add_item(
        app->notifications_submenu,
        "No Notifications",
        JumpingPawnsNoIndex,
        notifications_submenu_callback,
        app);
    submenu_add_item(
        app->notifications_submenu,
        "LED",
        JumpingPawnsLEDIndex,
        notifications_submenu_callback,
        app);
    submenu_add_item(
        app->notifications_submenu,
        "Vibro",
        JumpingPawnsVibroIndex,
        notifications_submenu_callback,
        app);
    submenu_add_item(
        app->notifications_submenu,
        "Both",
        JumpingPawnsBothIndex,
        notifications_submenu_callback,
        app);
    view_set_previous_callback(
        submenu_get_view(app->notifications_submenu), notifications_navigation_submenu_callback);
    view_dispatcher_add_view(
        app->view_dispatcher,
        JumpingPawnsNotificationsSubmenu,
        submenu_get_view(app->notifications_submenu));

    app->view = view_alloc();
    view_set_draw_callback(app->view, jumping_pawns_draw_callback);
    view_set_input_callback(app->view, jumping_pawns_input_callback);
    view_set_previous_callback(app->view, navigation_submenu_callback);
    view_set_context(app->view, app);
    view_set_custom_callback(app->view, jumping_pawns_custom_event_callback);
    view_allocate_model(app->view, ViewModelTypeLockFree, sizeof(JumpingPawnsModel));
    view_set_orientation(app->view, ViewOrientationVertical);

    JumpingPawnsModel* model = view_get_model(app->view);
    model->ai_or_player = "";
    model->whose_turn = "";
    model->selected_x = 1;
    model->selected_y = 1;
    model->led_notifications = false;
    model->vibro_notifications = false;
    model->last_calculated_piece_x = 0;
    model->last_calculated_piece_y = 0;
    model->is_ai_thinking = false;
    model->game_over = false;
    model->player_1_win = false;
    model->player_2_win = false;
    model->difficulty_level = 1;
    int temp_board[11][6] = {
        {2, 2, 2, 2, 2, 2},
        {2, 2, 2, 2, 2, 2},
        {0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0},
        {1, 1, 1, 1, 1, 1},
        {1, 1, 1, 1, 1, 1}};
    memcpy(model->board_state, temp_board, sizeof(temp_board));

    view_dispatcher_add_view(app->view_dispatcher, JumpingPawnsPlayView, app->view);

    app->widget_tutorial = widget_alloc();
    widget_add_text_scroll_element(
        app->widget_tutorial,
        0,
        0,
        128,
        64,
        "How to play:\n\n"
        "You and your opponent have 12 pawns. These pawns can only move by jumping over other groups of one or more pawns. The pawns can jump horizontally and vertically, but not diagonally. Pawns can also jump over multiple pawn groups. Your goal is to get all of your pawns to the two rows opposite your side before your opponent. A more detailed explanation can be found in the GitHub readme.\n"
        "https://github.com/Tyl3rA/Jumping-Pawns/README.md");
    view_set_previous_callback(widget_get_view(app->widget_tutorial), navigation_submenu_callback);
    view_dispatcher_add_view(
        app->view_dispatcher, JumpingPawnsViewTutorial, widget_get_view(app->widget_tutorial));

    app->widget_about = widget_alloc();
    widget_add_text_scroll_element(
        app->widget_about,
        0,
        0,
        128,
        64,
        "Jumping Pawns\n"
        "v0.1\n\n"
        "Author: @Tyl3rA\n"
        "Repo: https://github.com/Tyl3rA/Jumping-Pawns");
    view_set_previous_callback(widget_get_view(app->widget_about), navigation_submenu_callback);
    view_dispatcher_add_view(
        app->view_dispatcher, JumpingPawnsAbout, widget_get_view(app->widget_about));

    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    return app;
}

static void jumping_pawns_free(JumpingPawnsApp* app) {
    furi_record_close(RECORD_NOTIFICATION);
    view_dispatcher_remove_view(app->view_dispatcher, JumpingPawnsAbout);
    widget_free(app->widget_about);
    view_dispatcher_remove_view(app->view_dispatcher, JumpingPawnsViewTutorial);
    widget_free(app->widget_tutorial);
    view_dispatcher_remove_view(app->view_dispatcher, JumpingPawnsPlayView);
    view_free(app->view);
    view_dispatcher_remove_view(app->view_dispatcher, JumpingPawnsSubmenu);
    submenu_free(app->submenu);
    view_dispatcher_remove_view(app->view_dispatcher, JumpingPawnsDifficultySubmenu);
    submenu_free(app->difficulty_submenu);
    view_dispatcher_remove_view(app->view_dispatcher, JumpingPawnsNotificationsSubmenu);
    submenu_free(app->notifications_submenu);
    view_dispatcher_free(app->view_dispatcher);
    furi_record_close(RECORD_GUI);

    free(app);
}

int32_t jumping_pawns_app(void* _p) {
    UNUSED(_p);
    JumpingPawnsApp* app = jumping_pawns_alloc();
    view_dispatcher_run(app->view_dispatcher);
    jumping_pawns_free(app);
    return 0;
}
