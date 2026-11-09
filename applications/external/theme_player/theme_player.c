#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <input/input.h>
#include <storage/storage.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include "tunes.h"

#define TUNE_DIR     EXT_PATH("apps_data/theme_player")
#define MAX_THEMES   24
#define MAX_TUNES    32
#define NAME_LEN     32
#define TUNE_BUF     4096
#define VISIBLE_ROWS 5

typedef enum {
    ScreenThemes,
    ScreenTunes,
    ScreenPlaying
} Screen;

typedef struct {
    char name[NAME_LEN];
    bool on_sd;
} Theme;

typedef struct {
    char name[NAME_LEN];
    int builtin; /* index into builtin_tunes, or -1 for an SD file */
    char file[NAME_LEN];
} Tune;

typedef struct {
    FuriMutex* mutex;
    FuriMessageQueue* input;
    ViewPort* view_port;
    Gui* gui;
    Storage* storage;
    Screen screen;
    Theme themes[MAX_THEMES];
    size_t theme_count, theme_sel;
    Tune tunes[MAX_TUNES];
    size_t tune_count, tune_sel, scroll;
    char* tune_text;
    FuriThread* player;
    volatile bool stop;
    volatile bool looping;
    volatile float volume;
    char status[24];
} App;

/* ---------- loading ---------- */

static void add_theme(App* app, const char* name, bool on_sd) {
    for(size_t i = 0; i < app->theme_count; i++) {
        if(strcmp(app->themes[i].name, name) == 0) {
            app->themes[i].on_sd |= on_sd;
            return;
        }
    }
    if(app->theme_count >= MAX_THEMES) return;
    strlcpy(app->themes[app->theme_count].name, name, NAME_LEN);
    app->themes[app->theme_count++].on_sd = on_sd;
}

static void load_themes(App* app) {
    app->theme_count = 0;
    for(size_t i = 0; i < BUILTIN_TUNE_COUNT; i++)
        add_theme(app, builtin_tunes[i].theme, false);
    storage_simply_mkdir(app->storage, EXT_PATH("apps_data"));
    storage_simply_mkdir(app->storage, TUNE_DIR);
    File* dir = storage_file_alloc(app->storage);
    if(storage_dir_open(dir, TUNE_DIR)) {
        FileInfo info;
        char name[NAME_LEN];
        while(storage_dir_read(dir, &info, name, sizeof(name))) {
            if(file_info_is_dir(&info) && name[0] != '.') add_theme(app, name, true);
        }
    }
    storage_dir_close(dir);
    storage_file_free(dir);
}

static bool has_ext(const char* name, const char* ext) {
    size_t n = strlen(name), e = strlen(ext);
    return n > e && strcasecmp(name + n - e, ext) == 0;
}

static void load_tunes(App* app) {
    const Theme* theme = &app->themes[app->theme_sel];
    app->tune_count = app->tune_sel = app->scroll = 0;
    for(size_t i = 0; i < BUILTIN_TUNE_COUNT && app->tune_count < MAX_TUNES; i++) {
        if(strcmp(builtin_tunes[i].theme, theme->name) != 0) continue;
        Tune* t = &app->tunes[app->tune_count++];
        const char* colon = strchr(builtin_tunes[i].rtttl, ':');
        size_t len = colon ? (size_t)(colon - builtin_tunes[i].rtttl) : NAME_LEN - 1;
        if(len >= NAME_LEN) len = NAME_LEN - 1;
        memcpy(t->name, builtin_tunes[i].rtttl, len);
        t->name[len] = '\0';
        t->builtin = (int)i;
    }
    if(!theme->on_sd) return;
    char path[128];
    snprintf(path, sizeof(path), "%s/%s", TUNE_DIR, theme->name);
    File* dir = storage_file_alloc(app->storage);
    if(storage_dir_open(dir, path)) {
        FileInfo info;
        char name[NAME_LEN];
        while(app->tune_count < MAX_TUNES && storage_dir_read(dir, &info, name, sizeof(name))) {
            if(file_info_is_dir(&info)) continue;
            if(!has_ext(name, ".rtttl") && !has_ext(name, ".txt") && !has_ext(name, ".fmf"))
                continue;
            Tune* t = &app->tunes[app->tune_count++];
            strlcpy(t->file, name, NAME_LEN);
            strlcpy(t->name, name, NAME_LEN);
            char* dot = strrchr(t->name, '.');
            if(dot) *dot = '\0';
            t->builtin = -1;
        }
    }
    storage_dir_close(dir);
    storage_file_free(dir);
}

/* Converts a Flipper Music Format file into an RTTTL string in place. */
static bool fmf_to_rtttl(char* text) {
    int bpm = 120, dur = 4, oct = 5;
    const char* notes = NULL;
    for(char* line = text; line && *line;) {
        char* next = strchr(line, '\n');
        if(next) *next++ = '\0';
        if(strncmp(line, "BPM:", 4) == 0)
            bpm = atoi(line + 4);
        else if(strncmp(line, "Duration:", 9) == 0)
            dur = atoi(line + 9);
        else if(strncmp(line, "Octave:", 7) == 0)
            oct = atoi(line + 7);
        else if(strncmp(line, "Notes:", 6) == 0)
            notes = line + 6;
        line = next;
    }
    if(!notes) return false;
    char header[48];
    int h = snprintf(header, sizeof(header), "fmf:d=%d,o=%d,b=%d:", dur, oct, bpm);
    size_t n = strlen(notes);
    if(h + n + 1 > TUNE_BUF) return false;
    memmove(text + h, notes, n + 1);
    memcpy(text, header, h);
    return true;
}

static bool load_tune_text(App* app, const Tune* tune) {
    if(tune->builtin >= 0) {
        strlcpy(app->tune_text, builtin_tunes[tune->builtin].rtttl, TUNE_BUF);
        return true;
    }
    char path[160];
    snprintf(
        path, sizeof(path), "%s/%s/%s", TUNE_DIR, app->themes[app->theme_sel].name, tune->file);
    File* file = storage_file_alloc(app->storage);
    bool ok = false;
    if(storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        size_t read = storage_file_read(file, app->tune_text, TUNE_BUF - 1);
        app->tune_text[read] = '\0';
        ok = read > 0;
    }
    storage_file_close(file);
    storage_file_free(file);
    if(ok && has_ext(tune->file, ".fmf")) ok = fmf_to_rtttl(app->tune_text);
    return ok;
}

/* ---------- playback ---------- */

static bool wait_ms(App* app, uint32_t ms) {
    while(ms > 0) {
        if(app->stop) return false;
        uint32_t step = ms > 10 ? 10 : ms;
        furi_delay_ms(step);
        ms -= step;
    }
    return !app->stop;
}

static int read_int(const char** p) {
    int value = 0;
    while(isdigit((unsigned char)**p))
        value = value * 10 + (*(*p)++ - '0');
    return value;
}

static bool play_rtttl(App* app, const char* text) {
    const char* p = strchr(text, ':');
    if(!p) return false;
    p++;
    int def_dur = 4, def_oct = 6, bpm = 63;
    while(*p && *p != ':') {
        while(*p == ' ' || *p == ',')
            p++;
        char key = (char)tolower((unsigned char)*p);
        if(key && p[1] == '=') {
            p += 2;
            int v = read_int(&p);
            if(key == 'd' && v)
                def_dur = v;
            else if(key == 'o' && v)
                def_oct = v;
            else if(key == 'b' && v)
                bpm = v;
        } else if(*p && *p != ':') {
            p++;
        }
    }
    if(*p != ':') return false;
    p++;
    const uint32_t whole = 240000U / (uint32_t)bpm;
    static const int semis[7] = {9, 11, 0, 2, 4, 5, 7}; /* a..g */
    while(*p) {
        while(*p == ' ' || *p == ',' || *p == '\r' || *p == '\n')
            p++;
        if(!*p) break;
        int dur = read_int(&p);
        if(!dur) dur = def_dur;
        char c = (char)tolower((unsigned char)*p);
        if(!c) break;
        p++;
        int semi = -1;
        if(c >= 'a' && c <= 'g') semi = semis[c - 'a'];
        if(*p == '#') {
            semi++;
            p++;
        }
        bool dotted = false;
        if(*p == '.') {
            dotted = true;
            p++;
        }
        int oct = isdigit((unsigned char)*p) ? read_int(&p) : def_oct;
        if(*p == '.') {
            dotted = true;
            p++;
        }
        while(*p && *p != ',')
            p++; /* skip anything unknown */
        uint32_t ms = whole / (uint32_t)dur;
        if(dotted) ms += ms / 2;
        if(semi >= 0) {
            float freq = 440.0f * powf(2.0f, (float)(oct * 12 + semi - 57) / 12.0f);
            if(furi_hal_speaker_is_mine()) furi_hal_speaker_start(freq, app->volume);
            if(!wait_ms(app, ms * 9 / 10)) return false;
            if(furi_hal_speaker_is_mine()) furi_hal_speaker_stop();
            if(!wait_ms(app, ms / 10)) return false;
        } else if(!wait_ms(app, ms)) {
            return false;
        }
    }
    return true;
}

static int32_t player_thread(void* context) {
    App* app = context;
    if(!furi_hal_speaker_acquire(1000)) {
        strlcpy(app->status, "Speaker busy", sizeof(app->status));
        return 0;
    }
    do {
        if(!play_rtttl(app, app->tune_text)) break;
    } while(app->looping && wait_ms(app, 400));
    furi_hal_speaker_stop();
    furi_hal_speaker_release();
    if(!app->stop) strlcpy(app->status, "Finished", sizeof(app->status));
    view_port_update(app->view_port);
    return 0;
}

static void stop_playback(App* app) {
    if(!app->player) return;
    app->stop = true;
    furi_thread_join(app->player);
    furi_thread_free(app->player);
    app->player = NULL;
}

static void start_playback(App* app) {
    stop_playback(app);
    if(!load_tune_text(app, &app->tunes[app->tune_sel])) {
        strlcpy(app->status, "Can't read tune", sizeof(app->status));
        return;
    }
    strlcpy(app->status, "Playing", sizeof(app->status));
    app->stop = false;
    app->player = furi_thread_alloc_ex("ThemePlayer", 2048, player_thread, app);
    furi_thread_start(app->player);
}

/* ---------- UI ---------- */

static void draw_list(
    Canvas* canvas,
    const char* title,
    size_t count,
    size_t sel,
    size_t scroll,
    const char* (*label)(void*, size_t),
    void* ctx) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, title);
    canvas_draw_line(canvas, 0, 12, 127, 12);
    canvas_set_font(canvas, FontSecondary);
    if(count == 0) {
        canvas_draw_str(canvas, 4, 30, "No tunes here yet");
        return;
    }
    for(size_t row = 0; row < VISIBLE_ROWS && scroll + row < count; row++) {
        size_t i = scroll + row;
        int y = 23 + (int)row * 10;
        if(i == sel) {
            canvas_draw_box(canvas, 0, y - 8, 128, 10);
            canvas_set_color(canvas, ColorWhite);
        }
        canvas_draw_str(canvas, 4, y, label(ctx, i));
        canvas_set_color(canvas, ColorBlack);
    }
}

static const char* theme_label(void* ctx, size_t i) {
    return ((App*)ctx)->themes[i].name;
}
static const char* tune_label(void* ctx, size_t i) {
    return ((App*)ctx)->tunes[i].name;
}

static void draw_callback(Canvas* canvas, void* context) {
    App* app = context;
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    canvas_clear(canvas);
    if(app->screen == ScreenThemes) {
        draw_list(
            canvas,
            "Theme Player",
            app->theme_count,
            app->theme_sel,
            app->scroll,
            theme_label,
            app);
    } else if(app->screen == ScreenTunes) {
        draw_list(
            canvas,
            app->themes[app->theme_sel].name,
            app->tune_count,
            app->tune_sel,
            app->scroll,
            tune_label,
            app);
    } else {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 2, 10, app->themes[app->theme_sel].name);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(
            canvas, 64, 28, AlignCenter, AlignCenter, app->tunes[app->tune_sel].name);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 40, AlignCenter, AlignCenter, app->status);
        char line[32];
        snprintf(
            line,
            sizeof(line),
            "Loop:%s  Vol:%d%%",
            app->looping ? "On" : "Off",
            (int)(app->volume * 100.0f + 0.5f));
        canvas_draw_str_aligned(canvas, 64, 51, AlignCenter, AlignCenter, line);
        canvas_draw_str(canvas, 0, 63, "<> tune  ^v vol  OK loop");
    }
    furi_mutex_release(app->mutex);
}

static void input_callback(InputEvent* event, void* context) {
    furi_message_queue_put(((App*)context)->input, event, FuriWaitForever);
}

static void move(size_t* sel, size_t* scroll, size_t count, int delta) {
    if(count == 0) return;
    *sel = (*sel + count + (size_t)(delta + (int)count)) % count;
    if(*sel < *scroll) *scroll = *sel;
    if(*sel >= *scroll + VISIBLE_ROWS) *scroll = *sel - VISIBLE_ROWS + 1;
}

int32_t theme_player_app(void* p) {
    UNUSED(p);
    App* app = malloc(sizeof(App));
    memset(app, 0, sizeof(App));
    app->tune_text = malloc(TUNE_BUF);
    app->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    app->input = furi_message_queue_alloc(8, sizeof(InputEvent));
    app->storage = furi_record_open(RECORD_STORAGE);
    app->looping = true;
    app->volume = 0.6f;
    load_themes(app);

    app->view_port = view_port_alloc();
    view_port_draw_callback_set(app->view_port, draw_callback, app);
    view_port_input_callback_set(app->view_port, input_callback, app);
    app->gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);

    bool running = true;
    InputEvent event;
    while(running) {
        if(furi_message_queue_get(app->input, &event, 200) != FuriStatusOk) {
            view_port_update(app->view_port);
            continue;
        }
        if(event.type != InputTypeShort && event.type != InputTypeRepeat) continue;
        furi_mutex_acquire(app->mutex, FuriWaitForever);
        bool play = false, stop = false;
        if(app->screen == ScreenThemes) {
            if(event.key == InputKeyUp)
                move(&app->theme_sel, &app->scroll, app->theme_count, -1);
            else if(event.key == InputKeyDown)
                move(&app->theme_sel, &app->scroll, app->theme_count, 1);
            else if(event.key == InputKeyOk && app->theme_count) {
                load_tunes(app);
                app->screen = ScreenTunes;
            } else if(event.key == InputKeyBack)
                running = false;
        } else if(app->screen == ScreenTunes) {
            if(event.key == InputKeyUp)
                move(&app->tune_sel, &app->scroll, app->tune_count, -1);
            else if(event.key == InputKeyDown)
                move(&app->tune_sel, &app->scroll, app->tune_count, 1);
            else if(event.key == InputKeyOk && app->tune_count) {
                app->screen = ScreenPlaying;
                play = true;
            } else if(event.key == InputKeyBack) {
                app->screen = ScreenThemes;
                app->scroll = app->theme_sel >= VISIBLE_ROWS ? app->theme_sel - VISIBLE_ROWS + 1 :
                                                               0;
            }
        } else {
            if(event.key == InputKeyBack) {
                stop = true;
                app->screen = ScreenTunes;
            } else if(event.key == InputKeyOk) {
                app->looping = !app->looping;
            } else if(event.key == InputKeyUp && app->volume < 0.95f) {
                app->volume += 0.1f;
            } else if(event.key == InputKeyDown && app->volume > 0.15f) {
                app->volume -= 0.1f;
            } else if(event.key == InputKeyLeft || event.key == InputKeyRight) {
                move(
                    &app->tune_sel,
                    &app->scroll,
                    app->tune_count,
                    event.key == InputKeyLeft ? -1 : 1);
                play = true;
            }
        }
        furi_mutex_release(app->mutex);
        if(stop) stop_playback(app);
        if(play) start_playback(app);
        view_port_update(app->view_port);
    }

    stop_playback(app);
    gui_remove_view_port(app->gui, app->view_port);
    view_port_free(app->view_port);
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_STORAGE);
    furi_message_queue_free(app->input);
    furi_mutex_free(app->mutex);
    free(app->tune_text);
    free(app);
    return 0;
}
