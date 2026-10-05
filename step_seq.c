// Step Seq: a step sequencer for the Flipper's piezo speaker.
// One melody note and one drum hit per step. The speaker has a single voice,
// so a drum takes the first few milliseconds of its step and the note gets the rest.
// Chords are faked the chiptune way, by cycling quickly through their notes.

#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <input/input.h>
#include <storage/storage.h>
#include <stdio.h>
#include <string.h>

#define PAGE_STEPS   16
#define MAX_STEPS    32
#define SLOTS        8
#define PITCH_ROWS   15
#define VISIBLE_ROWS 8
#define NOTE_NONE    (-1)

// Layout
#define GRID_X       16
#define CELL_W       7
#define CELL_H       6
#define GRID_Y       9
#define DRUM_Y       58
#define MENU_VISIBLE 6

#define SONG_SLOTS 16
#define SONG_COLS  8

#define AUDIO_POLL_MS  5
#define PREVIEW_MS     120
// How long each note of a chord sounds before the next takes over
#define ARP_MS         30
// An accented note starts with a blip an octave up, which cuts through at any volume
#define ACCENT_BLIP_MS 12
#define TOAST_MS       1500

#define BPM_MIN                 60
#define BPM_MAX                 240
#define BPM_STEP                5
#define OCTAVE_MIN              2
#define OCTAVE_MAX              5
#define VOLUME_LEVELS           5
// Each swing level makes the first 16th of every pair 4% longer: 50% (straight) to 70%
#define SWING_LEVELS            5
#define SWING_PERCENT_PER_LEVEL 4

#define SAVE_DIR      EXT_PATH("apps_data/step_seq")
#define SAVE_PATH     SAVE_DIR "/pattern.bin"
#define SAVE_MAGIC    0x53455133 // "SEQ3"
#define SAVE_MAGIC_V2 0x53455132 // "SEQ2": no per-step flags, no song
#define SAVE_MAGIC_V1 0x53455131 // "SEQ1": a single 16-step pattern
#define EXPORT_DIR    EXT_PATH("music_player")

typedef enum {
    DrumNone,
    DrumKick,
    DrumSnare,
    DrumHat,
    DrumOpenHat,
    DrumClap,
    DrumTom,
    DrumCount,
} Drum;

static const uint8_t drum_length_ms[DrumCount] = {
    [DrumNone] = 0,
    [DrumKick] = 50,
    [DrumSnare] = 45,
    [DrumHat] = 15,
    [DrumOpenHat] = 60,
    [DrumClap] = 45,
    [DrumTom] = 55,
};

static const char* const drum_names[DrumCount] = {
    [DrumNone] = "drum",
    [DrumKick] = "kick",
    [DrumSnare] = "snare",
    [DrumHat] = "hat",
    [DrumOpenHat] = "open",
    [DrumClap] = "clap",
    [DrumTom] = "tom",
};

// Per-step flags
#define FLAG_LEN_MASK    0x07 // a NoteLen
#define FLAG_ACCENT      0x08
#define FLAG_DRUM_ACCENT 0x10
#define FLAG_CHORD_SHIFT 5
#define FLAG_CHORD_MASK  0x60 // a Chord
#define FLAG_NOTE_MASK   (FLAG_LEN_MASK | FLAG_ACCENT | FLAG_CHORD_MASK)

// Zero is the plain one-step note, so old patterns need no conversion
typedef enum {
    LenNormal,
    LenShort,
    Len2,
    Len3,
    Len4,
    Len6,
    Len8,
    LenCount,
} NoteLen;

static const uint8_t len_steps[LenCount] = {1, 1, 2, 3, 4, 6, 8};
#define MAX_NOTE_STEPS 8

typedef enum {
    ChordNone,
    ChordOctave,
    ChordFifth,
    ChordTriad,
    ChordCount,
} Chord;

// What OK does on the grid
typedef enum {
    ToolNote,
    ToolAccent,
    ToolLength,
    ToolChord,
    ToolCount,
} Tool;

static const char* const tool_names[ToolCount] = {"Note", "Accent", "Length", "Chord"};

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
    MenuTool,
    MenuPattern,
    MenuPlay,
    MenuSong,
    MenuTempo,
    MenuSwing,
    MenuLength,
    MenuScale,
    MenuRoot,
    MenuOctave,
    MenuVolume,
    MenuTranspose,
    MenuShift,
    MenuRandom,
    MenuUndo,
    MenuCopy,
    MenuClear,
    MenuExport,
    MenuCount,
} MenuItem;

typedef struct {
    int8_t notes[MAX_STEPS];
    uint8_t drums[MAX_STEPS];
    uint8_t flags[MAX_STEPS];
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
    uint8_t play_song;
    uint8_t drum_sel;
    // Pattern numbers 1..SLOTS in playing order; 0 is an empty place and is skipped
    uint8_t song[SONG_SLOTS];
    Pattern patterns[SLOTS];
} Save;

// The second version's save file
typedef struct {
    int8_t notes[MAX_STEPS];
    uint8_t drums[MAX_STEPS];
    uint8_t length;
    uint8_t bpm;
    uint8_t swing;
    uint8_t scale;
    uint8_t root;
    uint8_t octave;
} PatternV2;

typedef struct {
    uint32_t magic;
    uint8_t slot;
    uint8_t volume;
    PatternV2 patterns[SLOTS];
} SaveV2;

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
    Tool tool;

    bool song_open;
    int song_cursor;

    // One level of undo: the pattern as it was before the last edit
    Pattern undo;
    int undo_slot;
    bool has_undo;

    char toast[28];
    uint32_t toast_start;

    bool playing;
    bool song_playing;
    int song_pos;
    int step;
    uint32_t step_start;

    bool previewing;
    uint32_t preview_start;
    Drum preview_drum;
    float preview_freq;

    bool speaker_held;
    float last_freq;
    float last_volume;
    uint32_t noise;
} App;

static const int8_t demo_notes[PAGE_STEPS] = {
    0,
    NOTE_NONE,
    5,
    4,
    NOTE_NONE,
    3,
    5,
    NOTE_NONE,
    0,
    NOTE_NONE,
    5,
    6,
    NOTE_NONE,
    4,
    3,
    NOTE_NONE,
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
    memset(p->flags, 0, sizeof(p->flags));
}

static void toast(App* app, const char* text) {
    strlcpy(app->toast, text, sizeof(app->toast));
    app->toast_start = furi_get_tick();
}

static void undo_mark(App* app, int slot) {
    app->undo = app->save.patterns[slot];
    app->undo_slot = slot;
    app->has_undo = true;
}

static NoteLen step_len(const Pattern* p, int step) {
    return (NoteLen)(p->flags[step] & FLAG_LEN_MASK);
}

static Chord step_chord(const Pattern* p, int step) {
    return (Chord)((p->flags[step] & FLAG_CHORD_MASK) >> FLAG_CHORD_SHIFT);
}

// How many steps the note at `step` sounds for: its length, cut short by the
// next note or the end of the pattern
static int note_span(const Pattern* p, int step) {
    int span = len_steps[step_len(p, step)];
    for(int n = 1; n < span; n++) {
        if(step + n >= p->length || p->notes[step + n] != NOTE_NONE) return n;
    }
    return span;
}

// Number of filled places in the song
static int song_count(const Save* save) {
    int count = 0;
    for(int i = 0; i < SONG_SLOTS; i++) {
        if(save->song[i]) count++;
    }
    return count;
}

// The next filled place after `pos`, wrapping round; -1 if the song is empty
static int song_next(const Save* save, int pos) {
    for(int n = 1; n <= SONG_SLOTS; n++) {
        int i = (pos + n) % SONG_SLOTS;
        if(save->song[i]) return i;
    }
    return -1;
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
    save->play_song = 0;
    save->drum_sel = DrumKick;
    memset(save->song, 0, sizeof(save->song));
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
    if(save->play_song > 1 || save->drum_sel == DrumNone || save->drum_sel >= DrumCount) {
        return false;
    }
    for(int i = 0; i < SONG_SLOTS; i++) {
        if(save->song[i] > SLOTS) return false;
    }
    for(int i = 0; i < SLOTS; i++) {
        const Pattern* p = &save->patterns[i];
        if(p->length != PAGE_STEPS && p->length != MAX_STEPS) return false;
        if(p->swing > SWING_LEVELS) return false;
        if(!cells_valid(p->notes, p->drums, MAX_STEPS)) return false;
        if(!tuning_valid(p->bpm, p->scale, p->root, p->octave, save->volume)) return false;
        for(int n = 0; n < MAX_STEPS; n++) {
            if((p->flags[n] & FLAG_LEN_MASK) >= LenCount) return false;
        }
    }
    return true;
}

// Brings a second-version save in: the same patterns, with nothing flagged
static bool save_import_v2(Save* save, const SaveV2* old) {
    if(old->magic != SAVE_MAGIC_V2 || old->slot >= SLOTS) return false;
    Save* fresh = malloc(sizeof(Save));
    save_init(fresh);
    fresh->slot = old->slot;
    fresh->volume = old->volume;
    for(int i = 0; i < SLOTS; i++) {
        const PatternV2* o = &old->patterns[i];
        Pattern* p = &fresh->patterns[i];
        pattern_clear(p);
        memcpy(p->notes, o->notes, sizeof(o->notes));
        memcpy(p->drums, o->drums, sizeof(o->drums));
        p->length = o->length;
        p->bpm = o->bpm;
        p->swing = o->swing;
        p->scale = o->scale;
        p->root = o->root;
        p->octave = o->octave;
    }
    bool ok = save_valid(fresh);
    if(ok) *save = *fresh;
    free(fresh);
    return ok;
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
        } else if(size == sizeof(SaveV2)) {
            save_import_v2(save, (const SaveV2*)loaded);
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

// Square-wave stand-ins for drums: falling sweeps for the kick and tom, random pitches for noise
static float drum_freq(App* app, Drum drum, uint32_t t) {
    switch(drum) {
    case DrumKick:
        return 200.0f - 140.0f * (float)t / (float)drum_length_ms[DrumKick];
    case DrumTom:
        return 420.0f - 200.0f * (float)t / (float)drum_length_ms[DrumTom];
    case DrumSnare:
        return 900.0f + (float)(noise_next(app) % 3000);
    case DrumClap:
        // Three quick bursts, like several hands landing not quite together
        if((t >= 8 && t < 13) || (t >= 21 && t < 26)) return 0;
        return 1400.0f + (float)(noise_next(app) % 2200);
    case DrumHat:
    case DrumOpenHat:
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

static void speaker_set(App* app, float freq, float volume) {
    if(freq > 0) {
        if(!app->speaker_held) {
            if(!furi_hal_speaker_acquire(20)) return;
            app->speaker_held = true;
            app->last_freq = 0;
        }
        if(freq != app->last_freq || volume != app->last_volume) {
            furi_hal_speaker_start(freq, volume);
            app->last_freq = freq;
            app->last_volume = volume;
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

// Accents play one volume level up, when there is one
static float volume_for(const App* app, bool accent) {
    int level = app->save.volume - 1;
    if(accent && level < VOLUME_LEVELS - 1) level++;
    return volume_levels[level];
}

// The pitch a note is making `elapsed` ms after it started
static float note_freq(const Pattern* p, int step, uint32_t elapsed) {
    int row = p->notes[step];
    float freq = row_freq(p, row);
    uint32_t turn = elapsed / ARP_MS;
    switch(step_chord(p, step)) {
    case ChordOctave:
        if(turn % 2) freq *= 2.0f;
        break;
    case ChordFifth:
        if(turn % 2) freq *= 1.4983f;
        break;
    case ChordTriad:
        // Stacked thirds from the scale, so the chord stays in key
        freq = row_freq(p, row + 2 * (int)(turn % 3));
        break;
    default:
        break;
    }
    if((p->flags[step] & FLAG_ACCENT) && elapsed < ACCENT_BLIP_MS) freq *= 2.0f;
    return freq;
}

// Moves the song on to its next pattern
static void song_advance(App* app) {
    int next = song_next(&app->save, app->song_pos);
    if(next < 0) {
        app->song_playing = false;
        return;
    }
    app->song_pos = next;
    app->save.slot = app->save.song[next] - 1;
    app->cursor_x %= current(app)->length;
}

// Returns true when the playhead moved and the screen needs a redraw
static bool audio_update(App* app) {
    const Pattern* p = current(app);
    uint32_t now = furi_get_tick();
    bool moved = false;
    float freq = 0;
    float volume = volume_for(app, false);

    if(app->playing) {
        // The pattern may have been switched or shortened since the last step
        app->step %= p->length;
        while(now - app->step_start >= step_ms(p, app->step)) {
            app->step_start += step_ms(p, app->step);
            app->step++;
            if(app->step >= p->length) {
                app->step = 0;
                if(app->song_playing) {
                    song_advance(app);
                    p = current(app);
                }
            }
            moved = true;
        }
        uint32_t t = now - app->step_start;
        uint32_t this_step = step_ms(p, app->step);

        Drum drum = p->drums[app->step];
        bool drum_accent = p->flags[app->step] & FLAG_DRUM_ACCENT;
        // An accented drum rings longer, but never crowds out the whole step
        uint32_t drum_ms = drum_length_ms[drum];
        if(drum_accent) drum_ms += drum_ms / 2;
        drum_ms = MIN(drum_ms, this_step * 3 / 4);

        if(drum != DrumNone && t < drum_ms) {
            freq = drum_freq(app, drum, t);
            volume = volume_for(app, drum_accent);
        } else {
            // The note sounding now may have started a few steps back
            for(int back = 0; back < MAX_NOTE_STEPS && back <= app->step; back++) {
                int src = app->step - back;
                if(p->notes[src] == NOTE_NONE) continue;
                int span = note_span(p, src);
                if(back >= span) break;
                uint32_t elapsed = t;
                for(int k = src; k < app->step; k++)
                    elapsed += step_ms(p, k);
                // The gap at the end re-triggers repeated notes; a short note is mostly gap
                uint32_t sounding = step_len(p, src) == LenShort ? this_step / 3 :
                                                                   this_step * 7 / 8;
                if(back < span - 1 || t < sounding) {
                    freq = note_freq(p, src, elapsed);
                    volume = volume_for(app, p->flags[src] & FLAG_ACCENT);
                }
                break;
            }
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

    speaker_set(app, freq, volume);
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
    app->song_playing = false;
    if(app->playing) {
        app->step = 0;
        app->step_start = furi_get_tick();
        // A song starts from its first filled place
        int first = app->save.play_song ? song_next(&app->save, SONG_SLOTS - 1) : -1;
        if(first >= 0) {
            app->song_playing = true;
            app->song_pos = first;
            app->save.slot = app->save.song[first] - 1;
            app->cursor_x %= current(app)->length;
        }
    }
}

// --- Export

// Durations the Flipper music format can write, longest first, in 16th-note steps
typedef struct {
    uint8_t steps;
    const char* duration;
    const char* dots;
} FmfLength;

static const FmfLength fmf_lengths[] = {
    {8, "2", ""},
    {6, "4", "."},
    {4, "4", ""},
    {3, "8", "."},
    {2, "8", ""},
    {1, "16", ""},
};

static void fmf_pause(FuriString* out, int steps) {
    for(size_t i = 0; i < COUNT_OF(fmf_lengths) && steps > 0; i++) {
        const FmfLength* l = &fmf_lengths[i];
        if(l->dots[0]) continue; // keep rests plain
        while(steps >= l->steps) {
            furi_string_cat_printf(out, ", %sP", l->duration);
            steps -= l->steps;
        }
    }
}

// Adds one pattern's melody. The format has one voice and no ties, so drums, chords,
// accents and swing are left out, and a length it cannot write is rounded down with a rest.
static void fmf_pattern(FuriString* out, const Pattern* p) {
    int step = 0;
    while(step < p->length) {
        if(p->notes[step] == NOTE_NONE) {
            fmf_pause(out, 1);
            step++;
            continue;
        }
        int semitone = row_semitone(p, p->notes[step]);
        const char* name = note_names[semitone % 12];
        int octave = p->octave + semitone / 12;
        int span = note_span(p, step);
        if(step_len(p, step) == LenShort) {
            furi_string_cat_printf(out, ", 32%s%d, 32P", name, octave);
        } else {
            for(size_t i = 0; i < COUNT_OF(fmf_lengths); i++) {
                const FmfLength* l = &fmf_lengths[i];
                if(l->steps > span) continue;
                furi_string_cat_printf(out, ", %s%s%d%s", l->duration, name, octave, l->dots);
                fmf_pause(out, span - l->steps);
                break;
            }
        }
        step += span;
    }
}

// Writes the song (in song mode) or the current pattern for the Music Player app
static void export_fmf(App* app) {
    const Save* save = &app->save;
    bool song = save->play_song && song_count(save) > 0;
    const Pattern* first =
        song ? &save->patterns[save->song[song_next(save, SONG_SLOTS - 1)] - 1] : current(app);

    FuriString* notes = furi_string_alloc();
    if(song) {
        for(int i = 0; i < SONG_SLOTS; i++) {
            if(save->song[i]) fmf_pattern(notes, &save->patterns[save->song[i] - 1]);
        }
    } else {
        fmf_pattern(notes, first);
    }

    FuriString* text = furi_string_alloc_printf(
        "Filetype: Flipper Music Format\nVersion: 0\nBPM: %d\nDuration: 16\nOctave: 4\nNotes: ",
        first->bpm);
    // Drop the separator in front of the first note
    furi_string_cat_str(text, furi_string_get_cstr(notes) + 2);
    furi_string_cat_str(text, "\n");

    char name[24];
    if(song) {
        snprintf(name, sizeof(name), "StepSeq_Song.fmf");
    } else {
        snprintf(name, sizeof(name), "StepSeq_P%d.fmf", save->slot + 1);
    }
    FuriString* path = furi_string_alloc_printf("%s/%s", EXPORT_DIR, name);

    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_simply_mkdir(storage, EXPORT_DIR);
    File* file = storage_file_alloc(storage);
    size_t size = furi_string_size(text);
    bool ok =
        storage_file_open(file, furi_string_get_cstr(path), FSAM_WRITE, FSOM_CREATE_ALWAYS) &&
        storage_file_write(file, furi_string_get_cstr(text), size) == size;
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);

    toast(app, ok ? name : "Export failed");

    furi_string_free(path);
    furi_string_free(text);
    furi_string_free(notes);
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
            memcpy(&p->flags[PAGE_STEPS], p->flags, PAGE_STEPS * sizeof(p->flags[0]));
        }
    }
    p->length = length;
}

// Moves every note up or down a row, if they all fit
static void transpose(App* app, int dir) {
    Pattern* p = current(app);
    for(int i = 0; i < MAX_STEPS; i++) {
        if(p->notes[i] == NOTE_NONE) continue;
        int row = p->notes[i] + dir;
        if(row < 0 || row >= PITCH_ROWS) {
            toast(app, "No room to transpose");
            return;
        }
    }
    undo_mark(app, app->save.slot);
    for(int i = 0; i < MAX_STEPS; i++) {
        if(p->notes[i] != NOTE_NONE) p->notes[i] += dir;
    }
}

// Rotates the pattern a step left or right
static void shift(App* app, int dir) {
    Pattern* p = current(app);
    undo_mark(app, app->save.slot);
    Pattern old = *p;
    for(int i = 0; i < p->length; i++) {
        int from = (i - dir + p->length) % p->length;
        p->notes[i] = old.notes[from];
        p->drums[i] = old.drums[from];
        p->flags[i] = old.flags[from];
    }
}

// A new melody that wanders around the scale. The drums are left alone.
static void randomize(App* app) {
    Pattern* p = current(app);
    undo_mark(app, app->save.slot);
    int row = scales[p->scale].length;
    for(int i = 0; i < p->length; i++) {
        p->flags[i] &= FLAG_DRUM_ACCENT;
        if(furi_hal_random_get() % 100 >= 65) {
            p->notes[i] = NOTE_NONE;
            continue;
        }
        row += (int)(furi_hal_random_get() % 5) - 2;
        row = CLAMP(row, PITCH_ROWS - 1, 0);
        p->notes[i] = row;
        if(i % 4 == 0 && furi_hal_random_get() % 3 == 0) p->flags[i] |= FLAG_ACCENT;
    }
}

static void undo(App* app) {
    if(!app->has_undo) {
        toast(app, "Nothing to undo");
        return;
    }
    // Swapping means a second undo brings the change back
    Pattern now = app->save.patterns[app->undo_slot];
    app->save.patterns[app->undo_slot] = app->undo;
    app->undo = now;
    app->save.slot = app->undo_slot;
    toast(app, "Undone");
}

static void menu_adjust(App* app, int dir) {
    Pattern* p = current(app);
    switch(app->menu_item) {
    case MenuTool:
        app->tool = (app->tool + ToolCount + dir) % ToolCount;
        break;
    case MenuPlay:
        app->save.play_song = !app->save.play_song;
        if(app->save.play_song && song_count(&app->save) == 0) toast(app, "The song is empty");
        break;
    case MenuSong:
        if(dir > 0) {
            app->menu_open = false;
            app->song_open = true;
        }
        break;
    case MenuTranspose:
        transpose(app, dir);
        break;
    case MenuShift:
        shift(app, dir);
        break;
    case MenuRandom:
        if(dir > 0) randomize(app);
        break;
    case MenuUndo:
        if(dir > 0) undo(app);
        break;
    case MenuExport:
        if(dir > 0) export_fmf(app);
        break;
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
        if((dir > 0 ? MAX_STEPS : PAGE_STEPS) != p->length) undo_mark(app, app->save.slot);
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
            undo_mark(app, next);
            app->save.patterns[next] = *p;
            app->save.slot = next;
        }
        break;
    case MenuClear:
        if(dir > 0) {
            undo_mark(app, app->save.slot);
            pattern_clear(p);
        }
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

// Left/Right pick a place, Up/Down choose the pattern that plays there
static void handle_song_key(App* app, InputKey key) {
    uint8_t* place = &app->save.song[app->song_cursor];
    switch(key) {
    case InputKeyLeft:
        app->song_cursor = (app->song_cursor + SONG_SLOTS - 1) % SONG_SLOTS;
        break;
    case InputKeyRight:
        app->song_cursor = (app->song_cursor + 1) % SONG_SLOTS;
        break;
    case InputKeyUp:
        *place = (*place + 1) % (SLOTS + 1);
        break;
    case InputKeyDown:
        *place = (*place + SLOTS) % (SLOTS + 1);
        break;
    case InputKeyOk:
    case InputKeyBack:
        app->song_open = false;
        break;
    default:
        break;
    }
}

static void move_cursor_y(App* app, int dir) {
    if(app->cursor_y == 0 && dir < 0) {
        // Down on the drum row picks the next drum sound
        app->save.drum_sel = app->save.drum_sel % (DrumCount - 1) + 1;
        preview_drum(app, app->save.drum_sel);
        return;
    }
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
    uint8_t* flags = &p->flags[x];

    if(app->cursor_y == 0) {
        if(app->tool == ToolAccent) {
            if(p->drums[x] == DrumNone) return;
            undo_mark(app, app->save.slot);
            *flags ^= FLAG_DRUM_ACCENT;
        } else {
            // Places the chosen drum, or lifts it if it is already there
            undo_mark(app, app->save.slot);
            if(p->drums[x] == app->save.drum_sel) {
                p->drums[x] = DrumNone;
                *flags &= ~FLAG_DRUM_ACCENT;
            } else {
                p->drums[x] = app->save.drum_sel;
            }
        }
        preview_drum(app, p->drums[x]);
        return;
    }

    int row = app->cursor_y - 1;
    if(app->tool != ToolNote) {
        // The other tools change the note already on this step, whichever row it is on
        if(p->notes[x] == NOTE_NONE) return;
        undo_mark(app, app->save.slot);
        if(app->tool == ToolAccent) {
            *flags ^= FLAG_ACCENT;
        } else if(app->tool == ToolLength) {
            *flags = (*flags & ~FLAG_LEN_MASK) | ((step_len(p, x) + 1) % LenCount);
        } else {
            int chord = (step_chord(p, x) + 1) % ChordCount;
            *flags = (*flags & ~FLAG_CHORD_MASK) | (chord << FLAG_CHORD_SHIFT);
        }
        preview_row(app, p->notes[x]);
        return;
    }

    undo_mark(app, app->save.slot);
    if(p->notes[x] == row) {
        p->notes[x] = NOTE_NONE;
        *flags &= ~FLAG_NOTE_MASK;
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

    // The note or drum under the cursor, or the tool OK is set to
    if(app->tool != ToolNote && (app->cursor_y > 0 || app->tool == ToolAccent)) {
        canvas_draw_str(canvas, 38, 7, tool_names[app->tool]);
    } else if(app->cursor_y == 0) {
        canvas_draw_str(canvas, 38, 7, drum_names[app->save.drum_sel]);
    } else {
        row_name(p, app->cursor_y - 1, buf, sizeof(buf));
        canvas_draw_str(canvas, 38, 7, buf);
    }

    if(app->song_playing) {
        int place = 0;
        for(int i = 0; i <= app->song_pos; i++) {
            if(app->save.song[i]) place++;
        }
        snprintf(buf, sizeof(buf), "song %d/%d", place, song_count(&app->save));
    } else {
        snprintf(buf, sizeof(buf), "%s %s", note_names[p->root], scales[p->scale].name);
    }
    canvas_draw_str_aligned(canvas, 127, 7, AlignRight, AlignBottom, buf);
}

// A note is a block: narrow if short, with a tail if long, filling the whole cell if
// accented, and with one to three holes for an octave, fifth or triad chord.
// `room` is how many cells are left on the page, counting this one.
static void draw_note(Canvas* canvas, const Pattern* p, int step, int cx, int y, int room) {
    uint8_t flags = p->flags[step];
    int span = MIN(note_span(p, step), room);

    if(flags & FLAG_ACCENT) {
        canvas_draw_box(canvas, cx, y, CELL_W, CELL_H);
    } else if(step_len(p, step) == LenShort) {
        canvas_draw_box(canvas, cx + 1, y + 1, 3, CELL_H - 2);
    } else {
        canvas_draw_box(canvas, cx + 1, y + 1, CELL_W - 2, CELL_H - 2);
    }
    if(span > 1) {
        canvas_draw_box(canvas, cx + CELL_W - 1, y + 2, (span - 1) * CELL_W - 1, 2);
    }

    Chord chord = step_chord(p, step);
    if(chord != ChordNone) {
        canvas_set_color(canvas, ColorWhite);
        if(chord == ChordOctave) {
            canvas_draw_dot(canvas, cx + 3, y + 2);
        } else if(chord == ChordFifth) {
            canvas_draw_dot(canvas, cx + 2, y + 2);
            canvas_draw_dot(canvas, cx + 4, y + 2);
        } else {
            canvas_draw_dot(canvas, cx + 1, y + 2);
            canvas_draw_dot(canvas, cx + 3, y + 2);
            canvas_draw_dot(canvas, cx + 5, y + 2);
        }
        canvas_set_color(canvas, ColorBlack);
    }
}

static void draw_drum(Canvas* canvas, Drum drum, bool accent, int cx) {
    const int y = DRUM_Y;
    switch(drum) {
    case DrumKick:
        canvas_draw_box(canvas, cx + 1, y + 1, CELL_W - 2, CELL_H - 2);
        break;
    case DrumSnare:
        canvas_draw_frame(canvas, cx + 1, y + 1, CELL_W - 2, CELL_H - 2);
        break;
    case DrumHat:
        canvas_draw_line(canvas, cx + 1, y + 3, cx + CELL_W - 2, y + 3);
        break;
    case DrumOpenHat:
        canvas_draw_line(canvas, cx + 1, y + 2, cx + CELL_W - 2, y + 2);
        canvas_draw_line(canvas, cx + 1, y + 4, cx + CELL_W - 2, y + 4);
        break;
    case DrumClap:
        canvas_draw_line(canvas, cx + 1, y + 1, cx + 4, y + 4);
        canvas_draw_line(canvas, cx + 1, y + 4, cx + 4, y + 1);
        break;
    case DrumTom:
        canvas_draw_disc(canvas, cx + 3, y + 3, 2);
        break;
    default:
        canvas_draw_dot(canvas, cx + 3, y + 3);
        break;
    }
    // An accent is a bar across the top of the cell
    if(accent) canvas_draw_line(canvas, cx + 1, y - 1, cx + CELL_W - 2, y - 1);
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
                draw_note(canvas, p, first + x, cx, y, PAGE_STEPS - x);
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
        draw_drum(
            canvas,
            p->drums[first + x],
            p->flags[first + x] & FLAG_DRUM_ACCENT,
            GRID_X + x * CELL_W);
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
        [MenuTool] = "OK sets",
        [MenuPlay] = "Play",
        [MenuSong] = "Edit song",
        [MenuTranspose] = "Transpose",
        [MenuShift] = "Shift steps",
        [MenuRandom] = "Random melody",
        [MenuUndo] = "Undo",
        [MenuExport] = "Export .fmf",
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
        case MenuTool:
            snprintf(value, sizeof(value), "%s", tool_names[app->tool]);
            break;
        case MenuPlay:
            snprintf(value, sizeof(value), "%s", app->save.play_song ? "Song" : "Pattern");
            break;
        case MenuTranspose:
            snprintf(value, sizeof(value), "< down  up >");
            break;
        case MenuShift:
            snprintf(value, sizeof(value), "<  >");
            break;
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

static void draw_song(Canvas* canvas, App* app) {
    const int x = 8, y = 6, w = 112, h = 52, cell = 12;

    canvas_set_color(canvas, ColorWhite);
    canvas_draw_box(canvas, x, y, w, h);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_frame(canvas, x, y, w, h);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, x + 4, y + 10, "Song: patterns in order");

    for(int i = 0; i < SONG_SLOTS; i++) {
        int cx = x + 8 + (i % SONG_COLS) * cell;
        int cy = y + 14 + (i / SONG_COLS) * cell;
        char text[2] = {app->save.song[i] ? '0' + app->save.song[i] : '-', 0};
        bool selected = i == app->song_cursor;
        bool sounding = app->song_playing && i == app->song_pos;
        if(selected) {
            canvas_draw_box(canvas, cx, cy, cell - 1, cell - 1);
            canvas_set_color(canvas, ColorWhite);
        } else if(sounding) {
            canvas_draw_frame(canvas, cx, cy, cell - 1, cell - 1);
        }
        canvas_draw_str_aligned(canvas, cx + 5, cy + 2, AlignCenter, AlignTop, text);
        canvas_set_color(canvas, ColorBlack);
    }
    canvas_draw_str(canvas, x + 4, y + h - 3, "Up/Dn: pattern  OK: done");
}

static void draw_toast(Canvas* canvas, App* app) {
    canvas_set_font(canvas, FontSecondary);
    int w = canvas_string_width(canvas, app->toast) + 8;
    int x = (128 - w) / 2;
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_box(canvas, x, 25, w, 13);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_frame(canvas, x, 25, w, 13);
    canvas_draw_str(canvas, x + 4, 35, app->toast);
}

static void draw_callback(Canvas* canvas, void* ctx) {
    App* app = ctx;
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    canvas_clear(canvas);
    draw_top_bar(canvas, app);
    draw_grid(canvas, app);
    if(app->menu_open) draw_menu(canvas, app);
    if(app->song_open) draw_song(canvas, app);
    if(app->toast[0]) draw_toast(canvas, app);
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
        uint32_t timeout = audio_active  ? furi_ms_to_ticks(AUDIO_POLL_MS) :
                           app->toast[0] ? furi_ms_to_ticks(100) :
                                           FuriWaitForever;

        if(furi_message_queue_get(queue, &event, timeout) == FuriStatusOk) {
            bool is_press = event.type == InputTypeShort || event.type == InputTypeRepeat;
            bool is_long = event.type == InputTypeLong;

            furi_mutex_acquire(app->mutex, FuriWaitForever);
            bool quit = false;
            if(app->song_open) {
                if(event.type == InputTypeShort ||
                   (event.type == InputTypeRepeat && event.key != InputKeyOk &&
                    event.key != InputKeyBack)) {
                    handle_song_key(app, event.key);
                }
            } else if(app->menu_open) {
                // Closing needs a fresh press, or the hold that opened the menu would shut it
                bool closes = event.key == InputKeyOk || event.key == InputKeyBack;
                if(closes ? event.type == InputTypeShort : is_press) {
                    handle_menu_key(app, event.key);
                }
            } else if(is_long && event.key == InputKeyBack) {
                quit = true;
            } else if(is_long && event.key == InputKeyOk) {
                app->menu_open = true;
            } else if(
                event.type == InputTypeShort ||
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
        if(app->toast[0] && furi_get_tick() - app->toast_start > furi_ms_to_ticks(TOAST_MS)) {
            app->toast[0] = '\0';
            moved = true;
        }
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
