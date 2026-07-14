#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <stdio.h>

#define NUM_CHORDS 28

typedef struct {
    const char* name;
    int8_t frets[6];
} CuratedChord;

static const CuratedChord chords[NUM_CHORDS] = {
    {"C Major", {-1, 3, 2, 0, 1, 0}},  {"C7", {-1, 3, 2, 3, 1, 0}},
    {"Cmaj7", {-1, 3, 2, 0, 0, 0}},    {"Cadd9", {-1, 3, 2, 0, 3, 0}},
    {"D Major", {-1, -1, 0, 2, 3, 2}}, {"Dm", {-1, -1, 0, 2, 3, 1}},
    {"D7", {-1, -1, 0, 2, 1, 2}},      {"Dmaj7", {-1, -1, 0, 2, 2, 2}},
    {"Dsus4", {-1, -1, 0, 2, 3, 3}},   {"Dadd9", {-1, -1, 0, 2, 3, 0}},
    {"E Major", {0, 2, 2, 1, 0, 0}},   {"Em", {0, 2, 2, 0, 0, 0}},
    {"E7", {0, 2, 0, 1, 0, 0}},        {"Em7", {0, 2, 2, 0, 3, 0}},
    {"F Major", {1, 3, 3, 2, 1, 1}},   {"G Major", {3, 2, 0, 0, 0, 3}},
    {"G7", {3, 2, 0, 0, 0, 1}},        {"Gadd9", {3, 2, 0, 2, 0, 3}},
    {"Gmaj7", {3, 2, 0, 0, 0, 2}},     {"A Major", {-1, 0, 2, 2, 2, 0}},
    {"Am", {-1, 0, 2, 2, 1, 0}},       {"Am7", {-1, 0, 2, 0, 1, 0}},
    {"A7", {-1, 0, 2, 0, 2, 0}},       {"Asus4", {-1, 0, 2, 2, 3, 0}},
    {"Asus2", {-1, 0, 2, 2, 0, 0}},    {"B Major", {-1, 2, 4, 4, 4, 2}},
    {"Bm", {-1, 2, 4, 4, 3, 2}},       {"B7", {-1, 2, 1, 2, 0, 2}},
};

typedef struct {
    uint8_t current_index;
    FuriMessageQueue* event_queue;
    ViewPort* view_port;
    bool running;
} PocketChordsApp;

static void draw_chord_diagram(Canvas* canvas, int x, int y, const CuratedChord* chord) {
    const int string_spacing = 8;
    const int fret_height = 9;
    const int num_frets = 5;
    const int diagram_width = 5 * string_spacing;

    canvas_draw_line(canvas, x, y, x + diagram_width, y);
    canvas_draw_line(canvas, x, y + 1, x + diagram_width, y + 1);

    for(int f = 1; f <= num_frets; f++) {
        canvas_draw_line(canvas, x, y + f * fret_height, x + diagram_width, y + f * fret_height);
    }

    for(int s = 0; s < 6; s++) {
        canvas_draw_line(
            canvas, x + s * string_spacing, y, x + s * string_spacing, y + num_frets * fret_height);
    }

    canvas_set_font(canvas, FontSecondary);
    for(int s = 0; s < 6; s++) {
        int sx = x + s * string_spacing - 2;
        int8_t val = chord->frets[s];
        if(val == -1)
            canvas_draw_str(canvas, sx, y - 3, "X");
        else if(val == 0)
            canvas_draw_str(canvas, sx, y - 3, "O");
    }

    for(int s = 0; s < 6; s++) {
        int8_t fret = chord->frets[s];
        if(fret <= 0) continue;
        int dot_y = y + (fret * fret_height) - (fret_height / 2) + 1;
        canvas_draw_disc(canvas, x + s * string_spacing, dot_y, 2);
    }
}

static void draw_callback(Canvas* canvas, void* ctx) {
    PocketChordsApp* app = (PocketChordsApp*)ctx;
    canvas_clear(canvas);

    const CuratedChord* chord = &chords[app->current_index];

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 32, 6, AlignCenter, AlignTop, chord->name);

    draw_chord_diagram(canvas, 12, 40, chord);
}

static void input_callback(InputEvent* input_event, void* ctx) {
    PocketChordsApp* app = (PocketChordsApp*)ctx;
    furi_message_queue_put(app->event_queue, input_event, FuriWaitForever);
}

int32_t pocket_chords_app(void* p) {
    UNUSED(p);
    PocketChordsApp* app = malloc(sizeof(PocketChordsApp));
    if(!app) return -1;

    app->current_index = 0;
    app->running = true;

    app->event_queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    if(!app->event_queue) {
        free(app);
        return -1;
    }

    app->view_port = view_port_alloc();
    view_port_set_orientation(app->view_port, ViewPortOrientationVertical);

    view_port_draw_callback_set(app->view_port, draw_callback, app);
    view_port_input_callback_set(app->view_port, input_callback, app);

    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, app->view_port, GuiLayerFullscreen);
    view_port_update(app->view_port);

    InputEvent event;
    while(app->running) {
        if(furi_message_queue_get(app->event_queue, &event, FuriWaitForever) != FuriStatusOk)
            continue;

        if(event.key == InputKeyBack) {
            app->running = false;
            continue;
        }

        if(event.type == InputTypeShort || event.type == InputTypeRepeat) {
            bool changed = false;
            uint8_t new_index = app->current_index;

            if(event.key == InputKeyLeft || event.key == InputKeyRight) {
                // L/R → ALWAYS go to the Major chord of the next/previous root
                char current_root = chords[app->current_index].name[0];
                int direction = (event.key == InputKeyRight) ? 1 : -1;

                // Jump until root changes
                do {
                    new_index = (new_index + direction + NUM_CHORDS) % NUM_CHORDS;
                } while(chords[new_index].name[0] == current_root);

                // Walk back to the first chord of this new root (which is always the Major)
                while(new_index > 0 &&
                      chords[new_index - 1].name[0] == chords[new_index].name[0]) {
                    new_index--;
                }

                changed = true;
            } else if(event.key == InputKeyUp || event.key == InputKeyDown) {
                // Up/Down → cycle only inside current root
                char current_root = chords[app->current_index].name[0];

                int family_start = app->current_index;
                while(family_start > 0 && chords[family_start - 1].name[0] == current_root)
                    family_start--;

                int family_end = app->current_index;
                while(family_end < NUM_CHORDS - 1 &&
                      chords[family_end + 1].name[0] == current_root)
                    family_end++;

                int family_size = family_end - family_start + 1;
                int pos = app->current_index - family_start;

                int direction = (event.key == InputKeyDown) ? 1 : -1;
                int new_pos = (pos + direction + family_size) % family_size;

                new_index = family_start + new_pos;
                changed = true;
            }

            if(changed) {
                app->current_index = new_index;
                view_port_update(app->view_port);
            }
        }
    }

    gui_remove_view_port(gui, app->view_port);
    view_port_free(app->view_port);
    furi_message_queue_free(app->event_queue);
    furi_record_close(RECORD_GUI);
    free(app);
    return 0;
}
