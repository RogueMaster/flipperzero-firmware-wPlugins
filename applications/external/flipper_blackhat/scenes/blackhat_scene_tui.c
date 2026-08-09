#include "../blackhat_app_i.h"
#include <gui/canvas.h>

#define GAME_EXIT_HOLD_MS (5000)
#define A_HOLD_DELAY_MS   (100)

// Single-byte UART protocol shared with bhtui/src/gameboy.cpp.
#define BUTTON_UP_PRESSED      (0x80)
#define BUTTON_UP_RELEASED     (0x81)
#define BUTTON_DOWN_PRESSED    (0x82)
#define BUTTON_DOWN_RELEASED   (0x83)
#define BUTTON_LEFT_PRESSED    (0x84)
#define BUTTON_LEFT_RELEASED   (0x85)
#define BUTTON_RIGHT_PRESSED   (0x86)
#define BUTTON_RIGHT_RELEASED  (0x87)
#define BUTTON_A_PRESSED       (0x88)
#define BUTTON_A_RELEASED      (0x89)
#define BUTTON_B_PRESSED       (0x8a)
#define BUTTON_B_RELEASED      (0x8b)
#define BUTTON_SELECT_PRESSED  (0x8c)
#define BUTTON_SELECT_RELEASED (0x8d)
#define BUTTON_START_PRESSED   (0x8e)
#define BUTTON_START_RELEASED  (0x8f)
#define QUIT_GAME              (0x90)
#define GAME_MODE_STARTED      (0x91)
#define GAME_MODE_STOPPED      (0x92)

static void blackhat_scene_tui_send_byte(BlackhatApp* app, uint8_t byte) {
    blackhat_uart_tx(app->uart, (char*)&byte, 1);
}

static void blackhat_scene_tui_send_tap(BlackhatApp* app, uint8_t pressed, uint8_t released) {
    blackhat_scene_tui_send_byte(app, pressed);
    blackhat_scene_tui_send_byte(app, released);
}

static void blackhat_scene_tui_reset_game_input(BlackhatApp* app) {
    app->tui_ok_held = false;
    app->tui_a_pressed = false;
    app->tui_ok_chord_used = false;
    app->tui_left_chord = false;
    app->tui_right_chord = false;
    app->tui_back_held = false;
    app->tui_back_exit_sent = false;
    app->tui_ok_pressed_at = 0;
    app->tui_back_pressed_at = 0;
}

static void blackhat_scene_tui_handle_rx_data(uint8_t* buf, size_t len, void* context) {
    BlackhatApp* app = context;
    furi_assert(app);

    for(size_t i = 0; i < len; ++i) {
        if(buf[i] == GAME_MODE_STARTED) {
            view_dispatcher_send_custom_event(
                app->view_dispatcher, BlackhatEventTuiGameModeStarted);
        } else if(buf[i] == GAME_MODE_STOPPED) {
            view_dispatcher_send_custom_event(
                app->view_dispatcher, BlackhatEventTuiGameModeStopped);
        }
    }
}

static void blackhat_scene_tui_draw_callback(Canvas* canvas, void* model) {
    bool const game_mode = *(bool*)model;

    canvas_clear(canvas);
    canvas_set_font(canvas, FontSecondary);
    if(game_mode) {
        canvas_draw_str(canvas, 2, 12, "Centre: A    Back: B");
        canvas_draw_str(canvas, 2, 25, "Centre + Left: Select");
        canvas_draw_str(canvas, 2, 38, "Centre + Right: Start");
        canvas_draw_str(canvas, 2, 55, "Hold Back 5s to exit");
    } else {
        canvas_draw_str(canvas, 2, 12, "Press Back to exit.");
    }
}

static bool blackhat_scene_tui_game_input(InputEvent* event, BlackhatApp* app) {
    switch(event->key) {
    case InputKeyUp:
        if(event->type == InputTypePress) {
            blackhat_scene_tui_send_byte(app, BUTTON_UP_PRESSED);
        } else if(event->type == InputTypeRelease) {
            blackhat_scene_tui_send_byte(app, BUTTON_UP_RELEASED);
        }
        return true;

    case InputKeyDown:
        if(event->type == InputTypePress) {
            blackhat_scene_tui_send_byte(app, BUTTON_DOWN_PRESSED);
        } else if(event->type == InputTypeRelease) {
            blackhat_scene_tui_send_byte(app, BUTTON_DOWN_RELEASED);
        }
        return true;

    case InputKeyLeft:
        if(event->type == InputTypePress) {
            if(app->tui_ok_held) {
                if(app->tui_a_pressed) {
                    blackhat_scene_tui_send_byte(app, BUTTON_A_RELEASED);
                    app->tui_a_pressed = false;
                }
                app->tui_ok_chord_used = true;
                app->tui_left_chord = true;
                blackhat_scene_tui_send_byte(app, BUTTON_SELECT_PRESSED);
            } else {
                blackhat_scene_tui_send_byte(app, BUTTON_LEFT_PRESSED);
            }
        } else if(event->type == InputTypeRelease) {
            if(app->tui_left_chord) {
                blackhat_scene_tui_send_byte(app, BUTTON_SELECT_RELEASED);
                app->tui_left_chord = false;
            } else {
                blackhat_scene_tui_send_byte(app, BUTTON_LEFT_RELEASED);
            }
        }
        return true;

    case InputKeyRight:
        if(event->type == InputTypePress) {
            if(app->tui_ok_held) {
                if(app->tui_a_pressed) {
                    blackhat_scene_tui_send_byte(app, BUTTON_A_RELEASED);
                    app->tui_a_pressed = false;
                }
                app->tui_ok_chord_used = true;
                app->tui_right_chord = true;
                blackhat_scene_tui_send_byte(app, BUTTON_START_PRESSED);
            } else {
                blackhat_scene_tui_send_byte(app, BUTTON_RIGHT_PRESSED);
            }
        } else if(event->type == InputTypeRelease) {
            if(app->tui_right_chord) {
                blackhat_scene_tui_send_byte(app, BUTTON_START_RELEASED);
                app->tui_right_chord = false;
            } else {
                blackhat_scene_tui_send_byte(app, BUTTON_RIGHT_RELEASED);
            }
        }
        return true;

    case InputKeyOk:
        if(event->type == InputTypePress) {
            app->tui_ok_held = true;
            app->tui_a_pressed = false;
            app->tui_ok_chord_used = false;
            app->tui_ok_pressed_at = furi_get_tick();
        } else if(event->type == InputTypeRelease) {
            if(app->tui_a_pressed) {
                blackhat_scene_tui_send_byte(app, BUTTON_A_RELEASED);
            } else if(!app->tui_ok_chord_used) {
                blackhat_scene_tui_send_tap(app, BUTTON_A_PRESSED, BUTTON_A_RELEASED);
            }
            app->tui_ok_held = false;
            app->tui_a_pressed = false;
            app->tui_ok_chord_used = false;
        }
        return true;

    case InputKeyBack:
        if(event->type == InputTypePress) {
            app->tui_back_held = true;
            app->tui_back_exit_sent = false;
            app->tui_back_pressed_at = furi_get_tick();
            blackhat_scene_tui_send_byte(app, BUTTON_B_PRESSED);
        } else if(event->type == InputTypeRelease) {
            if(app->tui_back_held && !app->tui_back_exit_sent) {
                blackhat_scene_tui_send_byte(app, BUTTON_B_RELEASED);
            }
            app->tui_back_held = false;
        }
        return true;

    default:
        return false;
    }
}

static bool blackhat_scene_tui_input_callback(InputEvent* event, void* context) {
    BlackhatApp* app = context;
    furi_assert(app);

    if(app->tui_game_mode) {
        return blackhat_scene_tui_game_input(event, app);
    }

    if(event->type != InputTypeShort && event->type != InputTypeRepeat) {
        return false;
    }

    switch(event->key) {
    case InputKeyUp: {
        static const char seq[] = "\x1b[A";
        blackhat_uart_tx(app->uart, (char*)seq, sizeof(seq) - 1);
        return true;
    }
    case InputKeyDown: {
        static const char seq[] = "\x1b[B";
        blackhat_uart_tx(app->uart, (char*)seq, sizeof(seq) - 1);
        return true;
    }
    case InputKeyLeft: {
        static const char seq[] = "\x1b[D";
        blackhat_uart_tx(app->uart, (char*)seq, sizeof(seq) - 1);
        return true;
    }
    case InputKeyRight: {
        static const char seq[] = "\x1b[C";
        blackhat_uart_tx(app->uart, (char*)seq, sizeof(seq) - 1);
        return true;
    }
    case InputKeyOk: {
        static const char seq[] = "\r";
        blackhat_uart_tx(app->uart, (char*)seq, sizeof(seq) - 1);
        return true;
    }
    case InputKeyBack: {
        static const char seq[] = "\x03";
        blackhat_uart_tx(app->uart, (char*)seq, sizeof(seq) - 1);
        scene_manager_search_and_switch_to_previous_scene(app->scene_manager, BlackhatSceneStart);
        return true;
    }
    default:
        return false;
    }
}

void blackhat_scene_tui_on_enter(void* context) {
    BlackhatApp* app = context;
    View* view = app->tui_view;

    app->tui_game_mode = false;
    blackhat_scene_tui_reset_game_input(app);
    with_view_model(view, bool* game_mode, { *game_mode = false; }, false);

    view_set_context(view, app);
    view_set_draw_callback(view, blackhat_scene_tui_draw_callback);
    view_set_input_callback(view, blackhat_scene_tui_input_callback);

    blackhat_uart_set_handle_rx_data_cb(app->uart, blackhat_scene_tui_handle_rx_data);

    view_dispatcher_switch_to_view(app->view_dispatcher, BlackhatAppViewTui);

    static const char start_cmd[] = BHTUI_CMD "\n";
    blackhat_uart_tx(app->uart, (char*)start_cmd, sizeof(start_cmd) - 1);
}

bool blackhat_scene_tui_on_event(void* context, SceneManagerEvent event) {
    BlackhatApp* app = context;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == BlackhatEventTuiGameModeStarted) {
            app->tui_game_mode = true;
            blackhat_scene_tui_reset_game_input(app);
            with_view_model(app->tui_view, bool* game_mode, { *game_mode = true; }, true);
            return true;
        }
        if(event.event == BlackhatEventTuiGameModeStopped) {
            app->tui_game_mode = false;
            blackhat_scene_tui_reset_game_input(app);
            with_view_model(app->tui_view, bool* game_mode, { *game_mode = false; }, true);
            return true;
        }
    }

    if(event.type == SceneManagerEventTypeTick && app->tui_game_mode && app->tui_ok_held &&
       !app->tui_ok_chord_used && !app->tui_a_pressed &&
       furi_get_tick() - app->tui_ok_pressed_at >= furi_ms_to_ticks(A_HOLD_DELAY_MS)) {
        blackhat_scene_tui_send_byte(app, BUTTON_A_PRESSED);
        app->tui_a_pressed = true;
        return true;
    }

    if(event.type == SceneManagerEventTypeTick && app->tui_game_mode && app->tui_back_held &&
       !app->tui_back_exit_sent &&
       furi_get_tick() - app->tui_back_pressed_at >= furi_ms_to_ticks(GAME_EXIT_HOLD_MS)) {
        blackhat_scene_tui_send_byte(app, BUTTON_B_RELEASED);
        blackhat_scene_tui_send_byte(app, QUIT_GAME);
        app->tui_back_exit_sent = true;
        return true;
    }

    return false;
}

void blackhat_scene_tui_on_exit(void* context) {
    BlackhatApp* app = context;
    blackhat_uart_set_handle_rx_data_cb(app->uart, NULL);
    app->tui_game_mode = false;
    blackhat_scene_tui_reset_game_input(app);
}
