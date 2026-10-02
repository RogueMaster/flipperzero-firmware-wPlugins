// Step Seq: a step sequencer for the Flipper's piezo speaker.
// One melody note and one drum hit per step. The speaker has a single voice,
// so a drum takes the first few milliseconds of its step and the note gets the rest.

#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <input/input.h>
#include <storage/storage.h>
#include <stdio.h>
#include <string.h>

#define PAGE_STEPS 16
#define MAX_STEPS 32
#define SLOTS 8
#define PITCH_ROWS 15
#define VISIBLE_ROWS 8
#define NOTE_NONE (-1)

// Layout
#define GRID_X 16
#define CELL_W 7
#define CELL_H 6
#define GRID_Y 9
#define DRUM_Y 58
#define MENU_VISIBLE 6

#define AUDIO_POLL_MS 5
#define PREVIEW_MS 120

#define BPM_MIN 60
#define BPM_MAX 240
#define BPM_STEP 5
#define OCTAVE_MIN 2
#define OCTAVE_MAX 5
#define VOLUME_LEVELS 5
// Each swing level makes the first 16th of every pair 4% longer: 50% (straight) to 70%
#define SWING_LEVELS 5
#define SWING_PERCENT_PER_LEVEL 4

#define SAVE_DIR EXT_PATH("apps_data/step_seq")
#define SAVE_PATH SAVE_DIR "/pattern.bin"
#define SAVE_MAGIC 0x53455132 // "SEQ2"
#define SAVE_MAGIC_V1 0x53455131 // "SEQ1": a single 16-step pattern

typedef enum {
    DrumNone,
    DrumKick,
    DrumSnare,
    DrumHat,
    DrumCount,
} Drum;

static const uint8_t drum_length_ms[DrumCount] = {
    [DrumNone] = 0,
    [DrumKick] = 50,
    [DrumSnare] = 45,
    [DrumHat] = 15,
};

typedef struct {
    const char* name;
    uint8_t length;
    uint8_t semitones[12];
} Scale;

static const Scale scales[] = {
    {"Min pent", 5, {0, 3, 5, 7, 10}},
    {"Maj pent", 5, {0, 2, 4, 7, 9}},
    {"Minor", 7, {0, 2, 3, 5, 7, 8, 10}},
    {"Major", 7, {0, 2, 4, 5, 7, 9, 11}},
    {"Blues", 6, {0, 3, 5, 6, 7, 10}},
    {"Chromatic", 12, {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11}},
};
#define SCALE_COUNT ((int)COUNT_OF(scales))

static const char* const note_names[12] = {
    "C",
    "C#",
    "D",
    "D#",
    "E",
    "F",
    "F#",
    "G",
    "G#",
    "A",
    "A#",
    "B",
};

// Octave 4, C to B
static const float note_freqs[12] = {
    261.63f,
    277.18f,
    293.66f,
    311.13f,
    329.63f,
    349.23f,
    369.99f,
    392.00f,
    415.30f,
    440.00f,
    466.16f,
    493.88f,
};

static const float volume_levels[VOLUME_LEVELS] = {0.03f, 0.08f, 0.2f, 0.5f, 1.0f};

typedef enum {
    MenuPattern,
    MenuTempo,
    MenuSwing,
    MenuLength,
    MenuScale,
    MenuRoot,
    MenuOctave,
    MenuVolume,
    MenuCopy,
    MenuClear,
    MenuCount,
} MenuItem;

typedef struct {
    int8_t notes[MAX_STEPS];
    uint8_t drums[MAX_STEPS];
    uint8_t length;
    uint8_t bpm;
    uint8_t swing;
    uint8_t scale;
    uint8_t root;
    uint8_t octave;
} Pattern;

// Everything that is saved to the SD card
typedef struct {
    uint32_t magic;
    uint8_t slot;
    uint8_t volume;
    Pattern patterns[SLOTS];
} Save;

// The first version's save file
typedef struct {
    uint32_t magic;
    int8_t notes[PAGE_STEPS];
    uint8_t drums[PAGE_STEPS];
    uint8_t bpm;
    uint8_t scale;
    uint8_t root;
    uint8_t octave;
    uint8_t volume;
} SaveV1;

typedef struct {
    FuriMutex* mutex;
    Save save;

    int cursor_x;
    // 0 is the drum row, 1..PITCH_ROWS are pitch rows from low to high
    int cursor_y;
    int view_base;

    bool menu_open;
    int menu_item;
    int menu_top;

    bool playing;
    int step;
    uint32_t step_start;

    bool previewing;
    uint32_t preview_start;
    Drum preview_drum;
    float preview_freq;

    bool speaker_held;
    float last_freq;
    uint32_t noise;
} App;

static const int8_t demo_notes[PAGE_STEPS] = {
    0, NOTE_NONE, 5, 4, NOTE_NONE, 3, 5, NOTE_NONE, 0, NOTE_NONE, 5, 6, NOTE_NONE, 4, 3, NOTE_NONE,
};

static const uint8_t demo_drums[PAGE_STEPS] = {
    DrumKick,
    DrumNone,
    DrumHat,
    DrumNone,
    DrumSnare,
    DrumNone,
    DrumHat,
    DrumNone,
    DrumKick,
    DrumNone,
    DrumHat,
    DrumNone,
    DrumSnare,
    DrumNone,
    DrumHat,
    DrumKick,
};

static Pattern* current(App* app) {
    return &app->save.patterns[app->save.slot];
}

static void pattern_clear(Pattern* p) {
    memset(p->notes, NOTE_NONE, sizeof(p->notes));
    memset(p->drums, DrumNone, sizeof(p->drums));
}

static void pattern_init(Pattern* p) {
    pattern_clear(p);
    p->length = PAGE_STEPS;
    p->bpm = 120;
    p->swing = 0;
    p->scale = 0;
    p->root = 9; // A
    p->octave = 3;
}

static int row_semitone(const Pattern* p, int row) {
    const Scale* scale = &scales[p->scale];
    return p->root + scale->semitones[row % scale->length] + 12 * (row / scale->length);
}

static float row_freq(const Pattern* p, int row) {
    int semitone = row_semitone(p, row);
    float freq = note_freqs[semitone % 12];
    int octave = p->octave + semitone / 12;
    for(; octave > 4; octave--)
        freq *= 2.0f;
    for(; octave < 4; octave++)
        freq /= 2.0f;
    return freq;
}

static void row_name(const Pattern* p, int row, char* out, size_t size) {
    int semitone = row_semitone(p, row);
    snprintf(out, size, "%s%d", note_names[semitone % 12], p->octave + semitone / 12);
}

// --- Saving

static void save_init(Save* save) {
    save->magic = SAVE_MAGIC;
    save->slot = 0;
    save->volume = 3;
    for(int i = 0; i < SLOTS; i++)
        pattern_init(&save->patterns[i]);
    memcpy(save->patterns[0].notes, demo_notes, sizeof(demo_notes));
    memcpy(save->patterns[0].drums, demo_drums, sizeof(demo_drums));
}

static bool cells_valid(const int8_t* notes, const uint8_t* drums, int count) {
    for(int i = 0; i < count; i++) {
        if(notes[i] < NOTE_NONE || notes[i] >= PITCH_ROWS) return false;
        if(drums[i] >= DrumCount) return false;
    }
    return true;
}

static bool tuning_valid(int bpm, int scale, int root, int octave, int volume) {
    return bpm >= BPM_MIN && bpm <= BPM_MAX && scale < SCALE_COUNT && root < 12 &&
           octave >= OCTAVE_MIN && octave <= OCTAVE_MAX && volume >= 1 && volume <= VOLUME_LEVELS;
}

static bool save_valid(const Save* save) {
    if(save->magic != SAVE_MAGIC || save->slot >= SLOTS) return false;
    for(int i = 0; i < SLOTS; i++) {
        const Pattern* p = &save->patterns[i];
        if(p->length != PAGE_STEPS && p->length != MAX_STEPS) return false;
        if(p->swing > SWING_LEVELS) return false;
        if(!cells_valid(p->notes, p->drums, MAX_STEPS)) return false;
        if(!tuning_valid(p->bpm, p->scale, p->root, p->octave, save->volume)) return false;
    }
    return true;
}

// Brings a first-version save in as pattern 1
static bool save_import_v1(Save* save, const SaveV1* old) {
    if(old->magic != SAVE_MAGIC_V1) return false;
    if(!cells_valid(old->notes, old->drums, PAGE_STEPS)) return false;
    if(!tuning_valid(old->bpm, old->scale, old->root, old->octave, old->volume)) return false;

    Pattern* p = &save->patterns[0];
    pattern_init(p);
    memcpy(p->notes, old->notes, sizeof(old->notes));
    memcpy(p->drums, old->drums, sizeof(old->drums));
    p->bpm = old->bpm;
    p->scale = old->scale;
    p->root = old->root;
    p->octave = old->octave;
    save->volume = old->volume;
    return true;
}

static void save_load(Save* save) {
    save_init(save);
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, SAVE_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        Save* loaded = malloc(sizeof(Save));
        memset(loaded, 0, sizeof(Save));
        size_t size = storage_file_read(file, loaded, sizeof(Save));
        if(size == sizeof(Save) && save_valid(loaded)) {
            *save = *loaded;
        } else if(size == sizeof(SaveV1)) {
            save_import_v1(save, (const SaveV1*)loaded);
        }
        free(loaded);
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

static void save_store(const Save* save) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, EXT_PATH("apps_data"));
    storage_simply_mkdir(storage, SAVE_DIR);
    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, SAVE_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_write(file, save, sizeof(*save));
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

// --- Audio

static uint32_t noise_next(App* app) {
    app->noise = app->noise * 1664525u + 1013904223u;
    return app->noise >> 16;
}

// Square-wave stand-ins for drums: a falling sweep for the kick, random pitches for noise
static float drum_freq(App* app, Drum drum, uint32_t t) {
    switch(drum) {
    case DrumKick:
        return 200.0f - 140.0f * (float)t / (float)drum_length_ms[DrumKick];
    case DrumSnare:
        return 900.0f + (float)(noise_next(app) % 3000);
    case DrumHat:
        return 6000.0f + (float)(noise_next(app) % 3000);
    default:
        return 0;
    }
}

// Length of a 16th note. Swing stretches the first of each pair and shortens the second.
static uint32_t step_ms(const Pattern* p, int step) {
    int32_t straight = 15000 / p->bpm;
    int32_t shift = straight * 2 * p->swing * SWING_PERCENT_PER_LEVEL / 100;
    return (uint32_t)(step % 2 == 0 ? straight + shift : straight - shift);
}

static void speaker_set(App* app, float freq) {
    if(freq > 0) {
        if(!app->speaker_held) {
            if(!furi_hal_speaker_acquire(20)) return;
            app->speaker_held = true;
            app->last_freq = 0;
        }
        if(freq != app->last_freq) {
            furi_hal_speaker_start(freq, volume_levels[app->save.volume - 1]);
            app->last_freq = freq;
        }
    } else if(app->speaker_held) {
        if(app->last_freq != 0) {
            furi_hal_speaker_stop();
            app->last_freq = 0;
        }
        // Keep hold of the speaker between notes while the pattern is running
        if(!app->playing && !app->previewing) {
            furi_hal_speaker_release();
            app->speaker_held = false;
        }
    }
}

// Returns true when the playhead moved and the screen needs a redraw
static bool audio_update(App* app) {
    const Pattern* p = current(app);
    uint32_t now = furi_get_tick();
    bool moved = false;
    float freq = 0;

    if(app->playing) {
        // The pattern may have been switched or shortened since the last step
        app->step %= p->length;
        while(now - app->step_start >= step_ms(p, app->step)) {
            app->step_start += step_ms(p, app->step);
            app->step = (app->step + 1) % p->length;
            moved = true;
        }
        uint32_t t = now - app->step_start;
        Drum drum = p->drums[app->step];
        if(drum != DrumNone && t < drum_length_ms[drum]) {
            freq = drum_freq(app, drum, t);
        } else if(p->notes[app->step] != NOTE_NONE && t < step_ms(p, app->step) * 7 / 8) {
            // The gap at the end of the step re-triggers repeated notes
            freq = row_freq(p, p->notes[app->step]);
        }
    } else if(app->previewing) {
        uint32_t t = now - app->preview_start;
        if(app->preview_drum != DrumNone) {
            if(t < drum_length_ms[app->preview_drum]) {
                freq = drum_freq(app, app->preview_drum, t);
            } else {
                app->previewing = false;
            }
        } else if(t < PREVIEW_MS) {
            freq = app->preview_freq;
        } else {
            app->previewing = false;
        }
    }

    speaker_set(app, freq);
    return moved;
}

static void preview_row(App* app, int row) {
    if(app->playing) return;
    app->previewing = true;
    app->preview_start = furi_get_tick();
    app->preview_drum = DrumNone;
    app->preview_freq = row_freq(current(app), row);
}

static void preview_drum(App* app, Drum drum) {
    if(app->playing || drum == DrumNone) return;
    app->previewing = true;
    app->preview_start = furi_get_tick();
    app->preview_drum = drum;
}

static void toggle_play(App* app) {
    app->playing = !app->playing;
    app->previewing = false;
    if(app->playing) {
        app->step = 0;
        app->step_start = furi_get_tick();
    }
}

// --- Input

static void set_length(Pattern* p, int length) {
    if(length == p->length) return;
    if(length == MAX_STEPS) {
        // Growing into an empty second half starts it as a copy of the first, ready to vary
        bool second_half_empty = true;
        for(int i = PAGE_STEPS; i < MAX_STEPS; i++) {
            if(p->notes[i] != NOTE_NONE || p->drums[i] != DrumNone) second_half_empty = false;
        }
        if(second_half_empty) {
            memcpy(&p->notes[PAGE_STEPS], p->notes, PAGE_STEPS * sizeof(p->notes[0]));
            memcpy(&p->drums[PAGE_STEPS], p->drums, PAGE_STEPS * sizeof(p->drums[0]));
        }
    }
    p->length = length;
}

static void menu_adjust(App* app, int dir) {
    Pattern* p = current(app);
    switch(app->menu_item) {
    case MenuPattern:
        app->save.slot = (app->save.slot + SLOTS + dir) % SLOTS;
        break;
    case MenuTempo:
        p->bpm = CLAMP(p->bpm + dir * BPM_STEP, BPM_MAX, BPM_MIN);
        break;
    case MenuSwing:
        p->swing = CLAMP(p->swing + dir, SWING_LEVELS, 0);
        break;
    case MenuLength:
        set_length(p, dir > 0 ? MAX_STEPS : PAGE_STEPS);
        break;
    case MenuScale:
        p->scale = (p->scale + SCALE_COUNT + dir) % SCALE_COUNT;
        break;
    case MenuRoot:
        p->root = (p->root + 12 + dir) % 12;
        break;
    case MenuOctave:
        p->octave = CLAMP(p->octave + dir, OCTAVE_MAX, OCTAVE_MIN);
        break;
    case MenuVolume:
        app->save.volume = CLAMP(app->save.volume + dir, VOLUME_LEVELS, 1);
        // Force the next note to pick up the new volume
        app->last_freq = 0;
        break;
    case MenuCopy:
        if(dir > 0) {
            int next = (app->save.slot + 1) % SLOTS;
            app->save.patterns[next] = *p;
            app->save.slot = next;
        }
        break;
    case MenuClear:
        if(dir > 0) pattern_clear(p);
        break;
    default:
        break;
    }
    // The cursor may now be past the end of a shorter pattern
    app->cursor_x %= current(app)->length;
}

static void handle_menu_key(App* app, InputKey key) {
    switch(key) {
    case InputKeyUp:
        app->menu_item = (app->menu_item + MenuCount - 1) % MenuCount;
        break;
    case InputKeyDown:
        app->menu_item = (app->menu_item + 1) % MenuCount;
        break;
    case InputKeyLeft:
        menu_adjust(app, -1);
        break;
    case InputKeyRight:
        menu_adjust(app, 1);
        break;
    case InputKeyOk:
    case InputKeyBack:
        app->menu_open = false;
        break;
    default:
        break;
    }
    if(app->menu_item < app->menu_top) app->menu_top = app->menu_item;
    if(app->menu_item >= app->menu_top + MENU_VISIBLE) {
        app->menu_top = app->menu_item - MENU_VISIBLE + 1;
    }
}

static void move_cursor_y(App* app, int dir) {
    app->cursor_y = CLAMP(app->cursor_y + dir, PITCH_ROWS, 0);
    if(app->cursor_y == 0) return;
    int row = app->cursor_y - 1;
    if(row < app->view_base) app->view_base = row;
    if(row >= app->view_base + VISIBLE_ROWS) app->view_base = row - VISIBLE_ROWS + 1;
    preview_row(app, row);
}

static void toggle_cell(App* app) {
    Pattern* p = current(app);
    int x = app->cursor_x;
    if(app->cursor_y == 0) {
        p->drums[x] = (p->drums[x] + 1) % DrumCount;
        preview_drum(app, p->drums[x]);
        return;
    }
    int row = app->cursor_y - 1;
    if(p->notes[x] == row) {
        p->notes[x] = NOTE_NONE;
    } else {
        p->notes[x] = row;
        preview_row(app, row);
    }
}

static void handle_grid_key(App* app, InputKey key) {
    int length = current(app)->length;
    switch(key) {
    case InputKeyLeft:
        app->cursor_x = (app->cursor_x + length - 1) % length;
        break;
    case InputKeyRight:
        app->cursor_x = (app->cursor_x + 1) % length;
        break;
    case InputKeyUp:
        move_cursor_y(app, 1);
        break;
    case InputKeyDown:
        move_cursor_y(app, -1);
        break;
    case InputKeyOk:
        toggle_cell(app);
        break;
    case InputKeyBack:
        toggle_play(app);
        break;
    default:
        break;
    }
}

// --- Drawing

static void draw_top_bar(Canvas* canvas, App* app) {
    const Pattern* p = current(app);
    char buf[24];

    if(app->playing) {
        for(int i = 0; i < 4; i++)
            canvas_draw_line(canvas, 1 + i, 1 + i, 1 + i, 7 - i);
    } else {
        canvas_draw_box(canvas, 1, 2, 5, 5);
    }

    canvas_set_font(canvas, FontSecondary);
    snprintf(buf, sizeof(buf), "P%d", app->save.slot + 1);
    canvas_draw_str(canvas, 9, 7, buf);

    // One block per page of a 32-step pattern; the filled one is on screen
    if(p->length > PAGE_STEPS) {
        int page = app->cursor_x / PAGE_STEPS;
        for(int i = 0; i < p->length / PAGE_STEPS; i++) {
            if(i == page) {
                canvas_draw_box(canvas, 23 + i * 6, 2, 5, 5);
            } else {
                canvas_draw_frame(canvas, 23 + i * 6, 2, 5, 5);
            }
        }
    }

    if(app->cursor_y == 0) {
        canvas_draw_str(canvas, 38, 7, "drum");
    } else {
        row_name(p, app->cursor_y - 1, buf, sizeof(buf));
        canvas_draw_str(canvas, 38, 7, buf);
    }

    snprintf(buf, sizeof(buf), "%s %s", note_names[p->root], scales[p->scale].name);
    canvas_draw_str_aligned(canvas, 127, 7, AlignRight, AlignBottom, buf);
}

static void draw_grid(Canvas* canvas, App* app) {
    const Pattern* p = current(app);
    const Scale* scale = &scales[p->scale];
    // The screen shows the 16 steps around the cursor
    int first = app->cursor_x / PAGE_STEPS * PAGE_STEPS;

    for(int v = 0; v < VISIBLE_ROWS; v++) {
        int row = app->view_base + VISIBLE_ROWS - 1 - v;
        int y = GRID_Y + v * CELL_H;

        // Mark each octave's root note in the gutter
        if(row % scale->length == 0) canvas_draw_box(canvas, 9, y + 2, 5, 2);

        for(int x = 0; x < PAGE_STEPS; x++) {
            int cx = GRID_X + x * CELL_W;
            if(p->notes[first + x] == row) {
                canvas_draw_box(canvas, cx + 1, y + 1, CELL_W - 2, CELL_H - 2);
            } else {
                canvas_draw_dot(canvas, cx + 3, y + 3);
                // A taller tick on each beat
                if(x % 4 == 0) canvas_draw_dot(canvas, cx + 3, y + 2);
            }
        }
    }

    // Arrows when there are more rows above or below
    if(app->view_base + VISIBLE_ROWS < PITCH_ROWS) {
        canvas_draw_line(canvas, 2, GRID_Y + 3, 4, GRID_Y + 1);
        canvas_draw_line(canvas, 4, GRID_Y + 1, 6, GRID_Y + 3);
    }
    if(app->view_base > 0) {
        int y = GRID_Y + VISIBLE_ROWS * CELL_H - 4;
        canvas_draw_line(canvas, 2, y, 4, y + 2);
        canvas_draw_line(canvas, 4, y + 2, 6, y);
    }

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 3, 64, "dr");
    for(int x = 0; x < PAGE_STEPS; x++) {
        int cx = GRID_X + x * CELL_W;
        switch(p->drums[first + x]) {
        case DrumKick:
            canvas_draw_box(canvas, cx + 1, DRUM_Y + 1, CELL_W - 2, CELL_H - 2);
            break;
        case DrumSnare:
            canvas_draw_frame(canvas, cx + 1, DRUM_Y + 1, CELL_W - 2, CELL_H - 2);
            break;
        case DrumHat:
            canvas_draw_line(canvas, cx + 1, DRUM_Y + 3, cx + CELL_W - 2, DRUM_Y + 3);
            break;
        default:
            canvas_draw_dot(canvas, cx + 3, DRUM_Y + 3);
            break;
        }
    }

    int cursor_px = GRID_X + (app->cursor_x - first) * CELL_W;
    int cursor_py = app->cursor_y == 0 ?
                        DRUM_Y :
                        GRID_Y + (app->view_base + VISIBLE_ROWS - app->cursor_y) * CELL_H;
    canvas_draw_frame(canvas, cursor_px, cursor_py, CELL_W, CELL_H);

    // The playhead is only drawn while it is on the page being shown
    if(app->playing && app->step >= first && app->step < first + PAGE_STEPS) {
        canvas_set_color(canvas, ColorXOR);
        canvas_draw_box(
            canvas, GRID_X + (app->step - first) * CELL_W, GRID_Y, CELL_W, 64 - GRID_Y);
        canvas_set_color(canvas, ColorBlack);
    }
}

static void draw_menu(Canvas* canvas, App* app) {
    static const char* const labels[MenuCount] = {
        [MenuPattern] = "Pattern",
        [MenuTempo] = "Tempo",
        [MenuSwing] = "Swing",
        [MenuLength] = "Length",
        [MenuScale] = "Scale",
        [MenuRoot] = "Root",
        [MenuOctave] = "Octave",
        [MenuVolume] = "Volume",
        [MenuCopy] = "Copy to next",
        [MenuClear] = "Clear pattern",
    };
    const Pattern* p = current(app);
    const int x = 12, y = 4, w = 104, h = 56, line = 9;

    canvas_set_color(canvas, ColorWhite);
    canvas_draw_box(canvas, x, y, w, h);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_frame(canvas, x, y, w, h);
    canvas_set_font(canvas, FontSecondary);

    for(int v = 0; v < MENU_VISIBLE; v++) {
        int i = app->menu_top + v;
        char value[16];
        switch(i) {
        case MenuPattern:
            snprintf(value, sizeof(value), "%d of %d", app->save.slot + 1, SLOTS);
            break;
        case MenuTempo:
            snprintf(value, sizeof(value), "%d bpm", p->bpm);
            break;
        case MenuSwing:
            if(p->swing == 0) {
                snprintf(value, sizeof(value), "off");
            } else {
                snprintf(value, sizeof(value), "%d%%", 50 + p->swing * SWING_PERCENT_PER_LEVEL);
            }
            break;
        case MenuLength:
            snprintf(value, sizeof(value), "%d steps", p->length);
            break;
        case MenuScale:
            snprintf(value, sizeof(value), "%s", scales[p->scale].name);
            break;
        case MenuRoot:
            snprintf(value, sizeof(value), "%s", note_names[p->root]);
            break;
        case MenuOctave:
            snprintf(value, sizeof(value), "%d", p->octave);
            break;
        case MenuVolume:
            snprintf(value, sizeof(value), "%d", app->save.volume);
            break;
        default:
            snprintf(value, sizeof(value), "press >");
            break;
        }

        int row_y = y + 1 + v * line;
        if(i == app->menu_item) {
            canvas_draw_box(canvas, x + 1, row_y, w - 2, line);
            canvas_set_color(canvas, ColorWhite);
        }
        canvas_draw_str(canvas, x + 4, row_y + 8, labels[i]);
        canvas_draw_str_aligned(canvas, x + w - 4, row_y + 8, AlignRight, AlignBottom, value);
        canvas_set_color(canvas, ColorBlack);
    }

    // Scroll bar
    int track = h - 4;
    int thumb = track * MENU_VISIBLE / MenuCount;
    int offset = (track - thumb) * app->menu_top / (MenuCount - MENU_VISIBLE);
    canvas_draw_line(canvas, x + w + 1, y + 2 + offset, x + w + 1, y + 2 + offset + thumb);
}

static void draw_callback(Canvas* canvas, void* ctx) {
    App* app = ctx;
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    canvas_clear(canvas);
    draw_top_bar(canvas, app);
    draw_grid(canvas, app);
    if(app->menu_open) draw_menu(canvas, app);
    furi_mutex_release(app->mutex);
}

static void input_callback(InputEvent* event, void* ctx) {
    FuriMessageQueue* queue = ctx;
    furi_message_queue_put(queue, event, 0);
}

int32_t step_seq_app(void* p) {
    UNUSED(p);

    App* app = malloc(sizeof(App));
    memset(app, 0, sizeof(App));
    app->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    app->noise = furi_get_tick() | 1;
    app->cursor_y = 1;
    save_load(&app->save);

    FuriMessageQueue* queue = furi_message_queue_alloc(8, sizeof(InputEvent));

    ViewPort* view_port = view_port_alloc();
    view_port_draw_callback_set(view_port, draw_callback, app);
    view_port_input_callback_set(view_port, input_callback, queue);

    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    InputEvent event;
    while(true) {
        // Poll quickly only while there is sound to shape
        bool audio_active = app->playing || app->previewing || app->speaker_held;
        uint32_t timeout = audio_active ? furi_ms_to_ticks(AUDIO_POLL_MS) : FuriWaitForever;

        if(furi_message_queue_get(queue, &event, timeout) == FuriStatusOk) {
            bool is_press = event.type == InputTypeShort || event.type == InputTypeRepeat;
            bool is_long = event.type == InputTypeLong;

            furi_mutex_acquire(app->mutex, FuriWaitForever);
            bool quit = false;
            if(app->menu_open) {
                // Closing needs a fresh press, or the hold that opened the menu would shut it
                bool closes = event.key == InputKeyOk || event.key == InputKeyBack;
                if(closes ? event.type == InputTypeShort : is_press) {
                    handle_menu_key(app, event.key);
                }
            } else if(is_long && event.key == InputKeyBack) {
                quit = true;
            } else if(is_long && event.key == InputKeyOk) {
                app->menu_open = true;
            } else if(event.type == InputTypeShort ||
                      (event.type == InputTypeRepeat && event.key != InputKeyOk &&
                       event.key != InputKeyBack)) {
                handle_grid_key(app, event.key);
            }
            furi_mutex_release(app->mutex);
            if(quit) break;
            view_port_update(view_port);
        }

        furi_mutex_acquire(app->mutex, FuriWaitForever);
        bool moved = audio_update(app);
        furi_mutex_release(app->mutex);
        if(moved) view_port_update(view_port);
    }

    if(app->speaker_held) {
        furi_hal_speaker_stop();
        furi_hal_speaker_release();
    }
    save_store(&app->save);

    gui_remove_view_port(gui, view_port);
    view_port_free(view_port);
    furi_record_close(RECORD_GUI);
    furi_message_queue_free(queue);
    furi_mutex_free(app->mutex);
    free(app);

    return 0;
}
