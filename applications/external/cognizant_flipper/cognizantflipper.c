#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <stdlib.h>
#include <string.h>
#include "word_bank.h"

#define SCREEN_WIDTH       128
#define SCREEN_HEIGHT      64
#define MAX_LINES          (SCREEN_HEIGHT / 8)
#define MAX_CHARS_PER_LINE (SCREEN_WIDTH / 6)

typedef struct {
    char display[MAX_LINES][MAX_CHARS_PER_LINE + 1];
    uint8_t current_line;
    uint8_t current_position;
    bool show_instructions;
} WordGeneratorState;

static void word_generator_draw_callback(Canvas* canvas, void* ctx) {
    WordGeneratorState* state = ctx;
    canvas_clear(canvas);

    if(state->show_instructions) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignCenter, "Press OK to start");
    } else {
        for(int i = 0; i < MAX_LINES; i++) {
            canvas_draw_str(canvas, 0, (i + 1) * 8, state->display[i]);
        }
    }
}

static void word_generator_input_callback(InputEvent* input_event, void* ctx) {
    furi_assert(ctx);
    FuriMessageQueue* event_queue = ctx;
    furi_message_queue_put(event_queue, input_event, FuriWaitForever);
}

static void clear_screen(WordGeneratorState* state) {
    for(int i = 0; i < MAX_LINES; i++) {
        memset(state->display[i], 0, MAX_CHARS_PER_LINE + 1);
    }
    state->current_line = 0;
    state->current_position = 0;
}

static void add_word_to_display(WordGeneratorState* state) {
    // Generate a random index
    uint32_t random_index = rand() % WORD_COUNT;
    const char* new_word = WORD_BANK[random_index];
    size_t word_length = strlen(new_word);

    if(state->current_position + word_length + 1 > MAX_CHARS_PER_LINE) {
        state->current_line++;
        state->current_position = 0;

        if(state->current_line >= MAX_LINES) {
            clear_screen(state);
        }
    }

    if(state->current_position > 0) {
        state->display[state->current_line][state->current_position] = ' ';
        state->current_position++;
    }

    strncpy(
        &state->display[state->current_line][state->current_position],
        new_word,
        MAX_CHARS_PER_LINE - state->current_position);
    state->current_position += word_length;
}

int32_t word_generator_app(void* p) {
    UNUSED(p);
    WordGeneratorState state = {0};
    state.show_instructions = true;

    srand(furi_get_tick());

    FuriMessageQueue* event_queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    ViewPort* view_port = view_port_alloc();
    view_port_draw_callback_set(view_port, word_generator_draw_callback, &state);
    view_port_input_callback_set(view_port, word_generator_input_callback, event_queue);

    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    InputEvent event;
    bool running = true;
    while(running) {
        if(furi_message_queue_get(event_queue, &event, 100) == FuriStatusOk) {
            if(event.type == InputTypePress) {
                switch(event.key) {
                case InputKeyOk:
                    if(state.show_instructions) {
                        state.show_instructions = false;
                    } else {
                        add_word_to_display(&state);
                    }
                    break;
                case InputKeyBack:
                    running = false;
                    break;
                default:
                    break;
                }
            }
        }
        view_port_update(view_port);
    }

    view_port_enabled_set(view_port, false);
    gui_remove_view_port(gui, view_port);
    furi_record_close(RECORD_GUI);
    view_port_free(view_port);
    furi_message_queue_free(event_queue);

    return 0;
}
