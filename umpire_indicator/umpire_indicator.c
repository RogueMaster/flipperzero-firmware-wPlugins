#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
    StateInstructions,
    StateCounter
} AppState;

typedef struct {
    int balls;
    int strikes;
    int outs;
    bool exit;
    AppState state;
    ViewPort* view_port;
    Gui* gui;
} UmpireIndicatorState;

char* int_to_char(int num) {
    static char buffer[2];
    snprintf(buffer, sizeof(buffer), "%d", num);
    return buffer;
}

void draw_instructions(Canvas* canvas) {
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 12, "Umpire Indicator");

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 24, "Left: Ball   Up: Strike");
    canvas_draw_str(canvas, 2, 34, "Right: Out   Down: Undo");
    canvas_draw_str(canvas, 2, 44, "Back: Clear  Hold: Exit");
    canvas_draw_str(canvas, 2, 54, "Press OK to start");
}

void draw_counter(Canvas* canvas, UmpireIndicatorState* state) {
    canvas_clear(canvas);

    // Define box dimensions and positions
    int box_width = 40;
    int box_height = 30;
    int ball_x = 2;
    int strike_x = 44;
    int out_x = 86;
    int box_y = 2;

    // Draw boxes
    canvas_draw_frame(canvas, ball_x, box_y, box_width, box_height);
    canvas_draw_frame(canvas, strike_x, box_y, box_width, box_height);
    canvas_draw_frame(canvas, out_x, box_y, box_width, box_height);

    // Draw labels
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, ball_x + 2, box_y - 2, "B");
    canvas_draw_str(canvas, strike_x + 2, box_y - 2, "S");
    canvas_draw_str(canvas, out_x + 2, box_y - 2, "O");

    // Draw counts
    canvas_set_font(canvas, FontBigNumbers);
    canvas_draw_str(canvas, ball_x + 16, box_y + 22, int_to_char(state->balls));
    canvas_draw_str(canvas, strike_x + 16, box_y + 22, int_to_char(state->strikes));
    canvas_draw_str(canvas, out_x + 16, box_y + 22, int_to_char(state->outs));

    // Draw indicator circles
    canvas_set_font(canvas, FontSecondary);
    for(int i = 0; i < 4; i++) {
        if(i < state->balls)
            canvas_draw_disc(canvas, ball_x + 2 + (i * 9), box_y + box_height + 4, 3);
        else
            canvas_draw_circle(canvas, ball_x + 2 + (i * 9), box_y + box_height + 4, 3);
    }
    for(int i = 0; i < 3; i++) {
        if(i < state->strikes)
            canvas_draw_disc(canvas, strike_x + 2 + (i * 12), box_y + box_height + 4, 3);
        else
            canvas_draw_circle(canvas, strike_x + 2 + (i * 12), box_y + box_height + 4, 3);
    }
    for(int i = 0; i < 3; i++) {
        if(i < state->outs)
            canvas_draw_disc(canvas, out_x + 2 + (i * 12), box_y + box_height + 4, 3);
        else
            canvas_draw_circle(canvas, out_x + 2 + (i * 12), box_y + box_height + 4, 3);
    }
}

void draw_callback(Canvas* canvas, void* ctx) {
    UmpireIndicatorState* state = ctx;
    if(state->state == StateInstructions) {
        draw_instructions(canvas);
    } else {
        draw_counter(canvas, state);
    }
}

void input_callback(InputEvent* input_event, void* ctx) {
    furi_assert(ctx);
    UmpireIndicatorState* state = ctx;
    if(input_event->type == InputTypeShort) {
        switch(input_event->key) {
        case InputKeyOk:
            if(state->state == StateInstructions) {
                state->state = StateCounter;
            }
            break;
        case InputKeyLeft:
            if(state->state == StateCounter) {
                if(state->balls < 4) state->balls++;
                if(state->balls == 4) {
                    state->balls = 0;
                    state->strikes = 0;
                }
            }
            break;
        case InputKeyUp:
            if(state->state == StateCounter) {
                if(state->strikes < 3) state->strikes++;
                if(state->strikes == 3) {
                    state->strikes = 0;
                    state->balls = 0;
                    if(state->outs < 2)
                        state->outs++;
                    else {
                        state->outs = 0;
                    }
                }
            }
            break;
        case InputKeyRight:
            if(state->state == StateCounter) {
                if(state->outs < 2)
                    state->outs++;
                else {
                    state->outs = 0;
                    state->balls = 0;
                    state->strikes = 0;
                }
            }
            break;
        case InputKeyDown:
            if(state->state == StateCounter) {
                if(state->balls > 0)
                    state->balls--;
                else if(state->strikes > 0)
                    state->strikes--;
                else if(state->outs > 0)
                    state->outs--;
            }
            break;
        case InputKeyBack:
            if(state->state == StateCounter) {
                state->balls = 0;
                state->strikes = 0;
                state->outs = 0;
            }
            break;
        default:
            break;
        }
        view_port_update(state->view_port);
    } else if(input_event->type == InputTypeLong && input_event->key == InputKeyBack) {
        state->exit = true;
    }
}

int32_t umpire_indicator_app(void* p) {
    UNUSED(p);
    UmpireIndicatorState* state = malloc(sizeof(UmpireIndicatorState));

    state->balls = 0;
    state->strikes = 0;
    state->outs = 0;
    state->exit = false;
    state->state = StateInstructions;

    state->view_port = view_port_alloc();
    view_port_draw_callback_set(state->view_port, draw_callback, state);
    view_port_input_callback_set(state->view_port, input_callback, state);

    state->gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(state->gui, state->view_port, GuiLayerFullscreen);

    while(!state->exit) {
        furi_delay_ms(100);
    }

    view_port_enabled_set(state->view_port, false);
    gui_remove_view_port(state->gui, state->view_port);
    view_port_free(state->view_port);
    furi_record_close(RECORD_GUI);

    free(state);

    return 0;
}
