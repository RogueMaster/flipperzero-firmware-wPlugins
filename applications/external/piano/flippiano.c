// made by bergrfpv/bergr22   ---FlipPiano---

#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <input/input.h>
#include <furi_hal_speaker.h>

// 4. and 5. octave freq list
static const float note_freqs[14] = {
    261.63f,
    293.66f,
    329.63f,
    349.23f,
    392.00f,
    440.00f,
    493.88f, // 4. octave
    523.25f,
    587.33f,
    659.25f,
    698.46f,
    783.99f,
    880.00f,
    987.77f // 5. octave
};

static const char* note_names_tr[14] =
    {"DO", "RE", "MI", "FA", "SOL", "LA", "SI", "DO", "RE", "MI", "FA", "SOL", "LA", "SI"};

static const char* note_names_en[14] =
    {"C4", "D4", "E4", "F4", "G4", "A4", "B4", "C5", "D5", "E5", "F5", "G5", "A5", "B5"};

typedef enum {
    ModePiano,
    ModeVolume
} AppMode;

typedef struct {
    int selected_key; // 0 - 13  (keys)
    bool is_playing;
    float volume; // 0.0f - 1.0f  (sound)
    AppMode mode;
    FuriMutex* mutex;
} PianoState;

static void draw_callback(Canvas* canvas, void* ctx) {
    PianoState* state = (PianoState*)ctx;
    furi_mutex_acquire(state->mutex, FuriWaitForever);

    canvas_clear(canvas);

    // menu place (0 - 15 px) ---
    if(state->mode == ModeVolume) {
        // vol set menu
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 2, 11, "VOL:");

        // vol sign
        canvas_draw_box(canvas, 22, 5, 4, 6);
        canvas_draw_line(canvas, 26, 5, 29, 2);
        canvas_draw_line(canvas, 26, 10, 29, 13);
        canvas_draw_line(canvas, 29, 2, 29, 13);

        // Bar
        int bar_width = 50;
        int fill_width = (int)(state->volume * bar_width);
        canvas_draw_frame(canvas, 33, 5, bar_width, 6);
        canvas_draw_box(canvas, 33, 5, fill_width, 6);

        // %
        char vol_str[16];
        snprintf(vol_str, sizeof(vol_str), "%%%d", (int)(state->volume * 100));
        canvas_draw_str(canvas, 87, 11, vol_str);

        canvas_draw_str(canvas, 114, 10, " ");
    } else {
        // show note and freq
        canvas_set_font(canvas, FontPrimary);
        char header_str[32];
        snprintf(
            header_str,
            sizeof(header_str),
            "%s - %s (%.1f Hz)",
            note_names_tr[state->selected_key],
            note_names_en[state->selected_key],
            (double)note_freqs[state->selected_key]);
        canvas_draw_str(canvas, 4, 12, header_str);

        // arrow for volume menu
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 114, 10, "[^]");
    }

    // piano buttons place (16 - 63 px)

    int key_w = 9;
    int key_h = 48;
    int start_y = 16;

    for(int i = 0; i < 14; i++) {
        int x = 1 + (i * key_w);

        if(i == state->selected_key) {
            canvas_draw_box(canvas, x, start_y, key_w, key_h);
            canvas_set_color(canvas, ColorWhite);
        } else {
            canvas_draw_frame(canvas, x, start_y, key_w, key_h);
            canvas_set_color(canvas, ColorBlack);
        }

        canvas_set_font(canvas, FontSecondary);
        char letter[2] = {note_names_en[i][0], '\0'};
        canvas_draw_str(canvas, x + 2, start_y + key_h - 4, letter);

        canvas_set_color(canvas, ColorBlack);
    }

    // piano signs
    int black_keys[] = {0, 1, 3, 4, 5, 7, 8, 10, 11, 12};
    for(int j = 0; j < 10; j++) {
        int idx = black_keys[j];
        int bx = 1 + (idx * key_w) + 6;
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_box(canvas, bx, start_y, 5, 26);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_frame(canvas, bx, start_y, 5, 26);
        canvas_set_color(canvas, ColorBlack);
    }

    furi_mutex_release(state->mutex);
}

// button settings
static void input_callback(InputEvent* input_event, void* ctx) {
    furi_assert(ctx);
    FuriMessageQueue* event_queue = ctx;
    furi_message_queue_put(event_queue, input_event, FuriWaitForever);
}

int32_t piano_app_main(void* p) {
    UNUSED(p);
    FuriMessageQueue* event_queue = furi_message_queue_alloc(8, sizeof(InputEvent));

    PianoState* state = malloc(sizeof(PianoState));
    state->selected_key = 0;
    state->is_playing = false;
    state->volume = 1.0f;
    state->mode = ModePiano;
    state->mutex = furi_mutex_alloc(FuriMutexTypeNormal);

    ViewPort* view_port = view_port_alloc();
    view_port_draw_callback_set(view_port, draw_callback, state);
    view_port_input_callback_set(view_port, input_callback, event_queue);

    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    InputEvent event;
    bool running = true;

    while(running) {
        if(furi_message_queue_get(event_queue, &event, 100) == FuriStatusOk) {
            furi_mutex_acquire(state->mutex, FuriWaitForever);

            if(event.key == InputKeyBack && event.type == InputTypeShort) {
                if(state->mode == ModeVolume) {
                    state->mode = ModePiano;
                } else {
                    running = false; // exit app
                }
            } else if(state->mode == ModePiano) {
                // piano controllers
                if(event.type == InputTypeShort || event.type == InputTypeRepeat) {
                    if(event.key == InputKeyLeft) {
                        state->selected_key = (state->selected_key + 13) % 14;
                    } else if(event.key == InputKeyRight) {
                        state->selected_key = (state->selected_key + 1) % 14;
                    } else if(event.key == InputKeyUp) {
                        state->mode = ModeVolume;
                        if(state->is_playing && furi_hal_speaker_is_mine()) {
                            furi_hal_speaker_stop();
                            furi_hal_speaker_release();
                            state->is_playing = false;
                        }
                    }
                }

                // OK button
                if(event.key == InputKeyOk) {
                    if(event.type == InputTypePress) {
                        if(furi_hal_speaker_acquire(1000)) {
                            furi_hal_speaker_start(note_freqs[state->selected_key], state->volume);
                            state->is_playing = true;
                        }
                    } else if(event.type == InputTypeRelease) {
                        if(furi_hal_speaker_is_mine()) {
                            furi_hal_speaker_stop();
                            furi_hal_speaker_release();
                        }
                        state->is_playing = false;
                    }
                }
            } else if(state->mode == ModeVolume) {
                // volume settings
                if(event.type == InputTypeShort || event.type == InputTypeRepeat) {
                    if(event.key == InputKeyLeft) {
                        state->volume -= 0.05f;
                        if(state->volume < 0.0f) state->volume = 0.0f;
                    } else if(event.key == InputKeyRight) {
                        state->volume += 0.05f;
                        if(state->volume > 1.0f) state->volume = 1.0f;
                    } else if(event.key == InputKeyDown) {
                        state->mode = ModePiano;
                    }
                }
            }

            furi_mutex_release(state->mutex);
            view_port_update(view_port);
        }
    }

    if(furi_hal_speaker_is_mine()) {
        furi_hal_speaker_stop();
        furi_hal_speaker_release();
    }

    gui_remove_view_port(gui, view_port);
    view_port_free(view_port);
    furi_message_queue_free(event_queue);
    furi_record_close(RECORD_GUI);
    furi_mutex_free(state->mutex);
    free(state);

    return 0;
}
