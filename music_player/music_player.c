#include "music_worker.h"

#include <furi.h>
#include <furi_hal.h>

#include <music_player_icons.h>
#include <gui/gui.h>
#include <dialogs/dialogs.h>
#include <storage/storage.h>

#define TAG "MusicPlayer"

#define MUSIC_PLAYER_APP_EXTENSION "*"
#define MUSIC_PLAYER_EXAMPLE_FILE "Marble_Machine.fmf"

#define MUSIC_PLAYER_SEMITONE_HISTORY_SIZE 3

// Speed control constants
#define MUSIC_PLAYER_TEMPO_STEP 10u   // BPM change per button press
#define MUSIC_PLAYER_TEMPO_MIN  10u   // absolute minimum BPM
#define MUSIC_PLAYER_TEMPO_MAX  500u  // absolute maximum BPM

// Temporary file used when reloading with a new BPM
#define MUSIC_PLAYER_TEMP_FILE APP_DATA_PATH("_tempo_tmp.fmf")

typedef struct {
    uint8_t semitone_history[MUSIC_PLAYER_SEMITONE_HISTORY_SIZE];
    uint8_t duration_history[MUSIC_PLAYER_SEMITONE_HISTORY_SIZE];

    uint8_t volume;
    uint8_t semitone;
    uint8_t dots;
    uint8_t duration;
    float   position;

    uint32_t tempo;  // live BPM (0 = not yet loaded)
    bool     paused; // true when playback is paused via OK button
} MusicPlayerModel;

typedef struct {
    MusicPlayerModel* model;
    FuriMutex* model_mutex;

    FuriMessageQueue* input_queue;

    ViewPort* view_port;
    Gui* gui;

    MusicWorker* worker;

    // Stored so we can patch and reload when BPM changes
    FuriString* file_content; // raw text of the .fmf that was loaded
} MusicPlayer;

static const float MUSIC_PLAYER_VOLUMES[] = {0, .25, .5, .75, 1};

static const char* semitone_to_note(int8_t semitone) {
    switch(semitone) {
    case 0:
        return "C";
    case 1:
        return "C#";
    case 2:
        return "D";
    case 3:
        return "D#";
    case 4:
        return "E";
    case 5:
        return "F";
    case 6:
        return "F#";
    case 7:
        return "G";
    case 8:
        return "G#";
    case 9:
        return "A";
    case 10:
        return "A#";
    case 11:
        return "B";
    default:
        return "--";
    }
}

static bool is_white_note(uint8_t semitone, uint8_t id) {
    switch(semitone) {
    case 0:
        if(id == 0) return true;
        break;
    case 2:
        if(id == 1) return true;
        break;
    case 4:
        if(id == 2) return true;
        break;
    case 5:
        if(id == 3) return true;
        break;
    case 7:
        if(id == 4) return true;
        break;
    case 9:
        if(id == 5) return true;
        break;
    case 11:
        if(id == 6) return true;
        break;
    default:
        break;
    }
    return false;
}

static bool is_black_note(uint8_t semitone, uint8_t id) {
    switch(semitone) {
    case 1:
        if(id == 0) return true;
        break;
    case 3:
        if(id == 1) return true;
        break;
    case 6:
        if(id == 3) return true;
        break;
    case 8:
        if(id == 4) return true;
        break;
    case 10:
        if(id == 5) return true;
        break;
    default:
        break;
    }
    return false;
}

static void render_callback(Canvas* canvas, void* ctx) {
    MusicPlayer* music_player = ctx;
    furi_check(furi_mutex_acquire(music_player->model_mutex, FuriWaitForever) == FuriStatusOk);

    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    // Measure each header item in its own font
    canvas_set_font(canvas, FontPrimary);
    uint16_t title_w = canvas_string_width(canvas, "MusicPlayer");

    const char* playpause = music_player->model->paused ? "[ll]" : "[>]";
    canvas_set_font(canvas, FontSecondary);
    uint16_t pp_w = canvas_string_width(canvas, playpause);

    char bpm_str[16];
    bpm_str[0] = '\0';
    uint16_t bpm_w = 0;
    if(music_player->model->tempo > 0) {
        snprintf(bpm_str, sizeof(bpm_str), "<%luBPM>", (unsigned long)music_player->model->tempo);
        // Measure in FontSecondary
        bpm_w = canvas_string_width(canvas, bpm_str);
    }

    // Distribute the three items with equal gaps across 128px
    // gap = (128 - total_content) / 2
    uint16_t total_content = title_w + pp_w + bpm_w;
    uint16_t gap = (128 - total_content) / 2;

    uint16_t x = 0;
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, x, 12, "MusicPlayer");

    x += title_w + gap;
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, x, 12, playpause);

    if(bpm_w > 0) {
        // Draw BPM in FontSecondary, right-anchored flush to screen edge
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, (int)(128 - bpm_w), 12, bpm_str);
    }
    canvas_set_font(canvas, FontPrimary);

    uint8_t x_pos = 0;
    uint8_t y_pos = 24;
    const uint8_t white_w = 10;
    const uint8_t white_h = 40;

    const int8_t  black_x = 6;
    const int8_t  black_y = -5;
    const uint8_t black_w = 8;
    const uint8_t black_h = 32;

    // white keys
    for(size_t i = 0; i < 7; i++) {
        if(is_white_note(music_player->model->semitone, i)) {
            canvas_draw_box(canvas, x_pos + white_w * i, y_pos, white_w + 1, white_h);
        } else {
            canvas_draw_frame(canvas, x_pos + white_w * i, y_pos, white_w + 1, white_h);
        }
    }

    // black keys
    for(size_t i = 0; i < 7; i++) {
        if(i != 2 && i != 6) {
            canvas_set_color(canvas, ColorWhite);
            canvas_draw_box(
                canvas, x_pos + white_w * i + black_x, y_pos + black_y, black_w + 1, black_h);
            canvas_set_color(canvas, ColorBlack);
            if(is_black_note(music_player->model->semitone, i)) {
                canvas_draw_box(
                    canvas, x_pos + white_w * i + black_x, y_pos + black_y, black_w + 1, black_h);
            } else {
                canvas_draw_frame(
                    canvas, x_pos + white_w * i + black_x, y_pos + black_y, black_w + 1, black_h);
            }
        }
    }

    // volume bar — same y and height as the note panel (y=16, h=48)
    x_pos = 124;
    const uint8_t vol_bar_y = 16;
    const uint8_t vol_bar_h = 48;
    const uint8_t volume_h =
        (vol_bar_h / (COUNT_OF(MUSIC_PLAYER_VOLUMES) - 1)) * music_player->model->volume;
    canvas_draw_frame(canvas, x_pos, vol_bar_y, 4, vol_bar_h);
    canvas_draw_box(canvas, x_pos, vol_bar_y + (vol_bar_h - volume_h), 4, volume_h);

    // note-history panel — 3 rows × 16px = 48px, y=16..63
    x_pos = 73;
    y_pos = 16;
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_frame(canvas, x_pos, y_pos, 49, 48);
    canvas_draw_line(canvas, x_pos + 28, y_pos, x_pos + 28, 64);

    char duration_text[16];
    for(uint8_t i = 0; i < MUSIC_PLAYER_SEMITONE_HISTORY_SIZE; i++) {
        if(music_player->model->duration_history[i] == 0xFF) {
            snprintf(duration_text, 15, "--");
        } else {
            snprintf(duration_text, 15, "%d", music_player->model->duration_history[i]);
        }

        if(i == 0) {
            canvas_draw_box(canvas, x_pos, 64 - 16, 49, 16);
            canvas_set_color(canvas, ColorWhite);
        } else {
            canvas_set_color(canvas, ColorBlack);
        }
        canvas_draw_str(
            canvas, x_pos + 4, 64 - 16 * i - 3,
            semitone_to_note(music_player->model->semitone_history[i]));
        canvas_draw_str(canvas, x_pos + 31, 64 - 16 * i - 3, duration_text);
        if(i < MUSIC_PLAYER_SEMITONE_HISTORY_SIZE - 1)
            canvas_draw_line(canvas, x_pos, 64 - 16 * (i + 1), x_pos + 48, 64 - 16 * (i + 1));
    }

    furi_mutex_release(music_player->model_mutex);
}

static void input_callback(InputEvent* input_event, void* ctx) {
    MusicPlayer* music_player = ctx;
    // Accept short presses and held repeats so the user can ramp speed by holding
    if(input_event->type == InputTypeShort || input_event->type == InputTypeRepeat) {
        furi_message_queue_put(music_player->input_queue, input_event, 0);
    }
}

static void music_worker_callback(
    uint8_t semitone,
    uint8_t dots,
    uint8_t duration,
    float position,
    void* context) {
    MusicPlayer* music_player = context;
    furi_check(furi_mutex_acquire(music_player->model_mutex, FuriWaitForever) == FuriStatusOk);

    for(size_t i = 0; i < MUSIC_PLAYER_SEMITONE_HISTORY_SIZE - 1; i++) {
        size_t r = MUSIC_PLAYER_SEMITONE_HISTORY_SIZE - 1 - i;
        music_player->model->duration_history[r] = music_player->model->duration_history[r - 1];
        music_player->model->semitone_history[r] = music_player->model->semitone_history[r - 1];
    }

    semitone = (semitone == 0xFF) ? 0xFF : semitone % 12;

    music_player->model->semitone = semitone;
    music_player->model->dots = dots;
    music_player->model->duration = duration;
    music_player->model->position = position;

    music_player->model->semitone_history[0] = semitone;
    music_player->model->duration_history[0] = duration;

    furi_mutex_release(music_player->model_mutex);
    view_port_update(music_player->view_port);
}

void music_player_clear(MusicPlayer* instance) {
    memset(instance->model->duration_history, 0xff, MUSIC_PLAYER_SEMITONE_HISTORY_SIZE);
    memset(instance->model->semitone_history, 0xff, MUSIC_PLAYER_SEMITONE_HISTORY_SIZE);
    music_worker_clear(instance->worker);
}

MusicPlayer* music_player_alloc() {
    MusicPlayer* instance = malloc(sizeof(MusicPlayer));

    instance->model = malloc(sizeof(MusicPlayerModel));
    instance->model->volume = 3;
    instance->model->tempo  = 0;
    instance->model->paused = false;

    instance->model_mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    instance->input_queue = furi_message_queue_alloc(8, sizeof(InputEvent));

    instance->file_content = furi_string_alloc();

    instance->worker = music_worker_alloc();
    music_worker_set_volume(instance->worker, MUSIC_PLAYER_VOLUMES[instance->model->volume]);
    music_worker_set_callback(instance->worker, music_worker_callback, instance);

    music_player_clear(instance);

    instance->view_port = view_port_alloc();
    view_port_draw_callback_set(instance->view_port, render_callback, instance);
    view_port_input_callback_set(instance->view_port, input_callback, instance);

    instance->gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(instance->gui, instance->view_port, GuiLayerFullscreen);

    return instance;
}

void music_player_free(MusicPlayer* instance) {
    gui_remove_view_port(instance->gui, instance->view_port);
    furi_record_close(RECORD_GUI);
    view_port_free(instance->view_port);

    music_worker_free(instance->worker);

    furi_message_queue_free(instance->input_queue);
    furi_mutex_free(instance->model_mutex);

    furi_string_free(instance->file_content);

    free(instance->model);
    free(instance);
}

/**
 * Read the entire file at `path` into `out` (plain byte-by-byte read).
 */
static bool music_player_read_file(const char* path, FuriString* out) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File*    file    = storage_file_alloc(storage);
    bool     ok      = false;

    furi_string_reset(out);

    if(storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        char     buf[64];
        uint16_t n;
        while((n = storage_file_read(file, buf, sizeof(buf))) > 0) {
            for(uint16_t i = 0; i < n; i++)
                furi_string_push_back(out, buf[i]);
        }
        ok = !storage_file_get_error(file);
        storage_file_close(file);
    }

    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return ok;
}

/**
 * Parse "BPM: <n>" from an FMF file's raw content.
 */
static uint32_t music_player_parse_bpm(const FuriString* content) {
    const char* s   = furi_string_get_cstr(content);
    const char* tag = "BPM: ";
    const char* p   = strstr(s, tag);
    if(!p) return 0;
    return (uint32_t)atoi(p + strlen(tag));
}

/**
 * Write a modified copy of `file_content` with BPM replaced by `new_bpm`
 * to MUSIC_PLAYER_TEMP_FILE, then reload the worker from that file.
 * Caller must have stopped the worker before calling.
 */
static bool music_player_reload_with_tempo(MusicPlayer* mp, uint32_t new_bpm) {
    const char* src = furi_string_get_cstr(mp->file_content);
    const char* tag = "BPM: ";
    const char* p   = strstr(src, tag);
    if(!p) return false;

    // Position of the digit string that follows "BPM: "
    size_t num_start = (size_t)(p - src) + strlen(tag);
    size_t num_end   = num_start;
    while(src[num_end] >= '0' && src[num_end] <= '9') num_end++;

    // Compose the patched text
    FuriString* patched = furi_string_alloc();
    for(size_t i = 0; i < num_start; i++)
        furi_string_push_back(patched, src[i]);

    char bpm_str[12];
    snprintf(bpm_str, sizeof(bpm_str), "%lu", (unsigned long)new_bpm);
    furi_string_cat_str(patched, bpm_str);

    for(size_t i = num_end; src[i] != '\0'; i++)
        furi_string_push_back(patched, src[i]);

    // Write to temp file
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File*    file    = storage_file_alloc(storage);
    bool     ok      = false;

    if(storage_file_open(file, MUSIC_PLAYER_TEMP_FILE, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        const char* out = furi_string_get_cstr(patched);
        uint16_t    len = (uint16_t)strlen(out);
        ok = (storage_file_write(file, out, len) == len);
        storage_file_close(file);
    }

    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    furi_string_free(patched);

    if(!ok) return false;

    // Reload worker from the temp file
    music_worker_clear(mp->worker);
    if(!music_worker_load(mp->worker, MUSIC_PLAYER_TEMP_FILE)) {
        FURI_LOG_E(TAG, "Failed to reload worker with new tempo");
        return false;
    }

    return true;
}

int32_t music_player_app(void* p) {
    MusicPlayer* music_player = music_player_alloc();

    FuriString* file_path = furi_string_alloc();

    do {
        if(p && strlen(p)) {
            furi_string_set(file_path, (const char*)p);
        } else {
            Storage* storage = furi_record_open(RECORD_STORAGE);
            storage_common_migrate(
                storage, EXT_PATH("music_player"), STORAGE_APP_DATA_PATH_PREFIX);

            if(!storage_common_exists(storage, APP_DATA_PATH(MUSIC_PLAYER_EXAMPLE_FILE))) {
                storage_common_copy(
                    storage,
                    APP_ASSETS_PATH(MUSIC_PLAYER_EXAMPLE_FILE),
                    APP_DATA_PATH(MUSIC_PLAYER_EXAMPLE_FILE));
            }
            furi_record_close(RECORD_STORAGE);

            furi_string_set(file_path, STORAGE_APP_DATA_PATH_PREFIX);

            DialogsFileBrowserOptions browser_options;
            dialog_file_browser_set_basic_options(
                &browser_options, MUSIC_PLAYER_APP_EXTENSION, &I_music_player_icon_10px);
            browser_options.hide_ext = false;
            browser_options.base_path = STORAGE_APP_DATA_PATH_PREFIX;

            DialogsApp* dialogs = furi_record_open(RECORD_DIALOGS);
            bool res = dialog_file_browser_show(dialogs, file_path, file_path, &browser_options);
            furi_record_close(RECORD_DIALOGS);

            if(!res) {
                FURI_LOG_E(TAG, "No file selected");
                break;
            }
        }

        // Cache raw file content so we can patch BPM without touching the original
        if(!music_player_read_file(
               furi_string_get_cstr(file_path), music_player->file_content)) {
            FURI_LOG_E(TAG, "Unable to read file");
            break;
        }

        // Store the original BPM for display
        music_player->model->tempo = music_player_parse_bpm(music_player->file_content);

        if(!music_worker_load(music_player->worker, furi_string_get_cstr(file_path))) {
            FURI_LOG_E(TAG, "Unable to load file");
            break;
        }

        music_worker_start(music_player->worker);

        InputEvent input;
        while(furi_message_queue_get(music_player->input_queue, &input, FuriWaitForever) ==
              FuriStatusOk) {

            if(input.key == InputKeyBack) {
                break;
            }

            furi_check(
                furi_mutex_acquire(music_player->model_mutex, FuriWaitForever) == FuriStatusOk);

            if(input.key == InputKeyUp) {
                if(music_player->model->volume < COUNT_OF(MUSIC_PLAYER_VOLUMES) - 1)
                    music_player->model->volume++;
                music_worker_set_volume(
                    music_player->worker, MUSIC_PLAYER_VOLUMES[music_player->model->volume]);
                furi_mutex_release(music_player->model_mutex);

            } else if(input.key == InputKeyDown) {
                if(music_player->model->volume > 0) music_player->model->volume--;
                music_worker_set_volume(
                    music_player->worker, MUSIC_PLAYER_VOLUMES[music_player->model->volume]);
                furi_mutex_release(music_player->model_mutex);

            } else if(input.key == InputKeyOk) {
                // Toggle play/pause
                music_player->model->paused = !music_player->model->paused;
                bool paused = music_player->model->paused;
                furi_mutex_release(music_player->model_mutex);

                if(paused) {
                    music_worker_pause(music_player->worker);
                } else {
                    music_worker_resume(music_player->worker);
                }

            } else if(input.key == InputKeyRight) {
                // Speed up
                uint32_t new_tempo = music_player->model->tempo + MUSIC_PLAYER_TEMPO_STEP;
                if(new_tempo > MUSIC_PLAYER_TEMPO_MAX) new_tempo = MUSIC_PLAYER_TEMPO_MAX;

                if(new_tempo != music_player->model->tempo) {
                    music_player->model->tempo = new_tempo;
                    bool paused = music_player->model->paused;
                    furi_mutex_release(music_player->model_mutex);

                    music_worker_stop(music_player->worker);
                    if(music_player_reload_with_tempo(music_player, new_tempo) && !paused)
                        music_worker_start(music_player->worker);
                } else {
                    furi_mutex_release(music_player->model_mutex);
                }

            } else if(input.key == InputKeyLeft) {
                // Slow down
                uint32_t cur = music_player->model->tempo;
                uint32_t new_tempo = (cur > MUSIC_PLAYER_TEMPO_MIN + MUSIC_PLAYER_TEMPO_STEP)
                    ? cur - MUSIC_PLAYER_TEMPO_STEP
                    : MUSIC_PLAYER_TEMPO_MIN;

                if(new_tempo != cur) {
                    music_player->model->tempo = new_tempo;
                    bool paused = music_player->model->paused;
                    furi_mutex_release(music_player->model_mutex);

                    music_worker_stop(music_player->worker);
                    if(music_player_reload_with_tempo(music_player, new_tempo) && !paused)
                        music_worker_start(music_player->worker);
                } else {
                    furi_mutex_release(music_player->model_mutex);
                }

            } else {
                furi_mutex_release(music_player->model_mutex);
            }

            view_port_update(music_player->view_port);
        }

        music_worker_stop(music_player->worker);

        // Remove the temp file if it was created
        Storage* storage = furi_record_open(RECORD_STORAGE);
        if(storage_common_exists(storage, MUSIC_PLAYER_TEMP_FILE)) {
            storage_common_remove(storage, MUSIC_PLAYER_TEMP_FILE);
        }
        furi_record_close(RECORD_STORAGE);

        if(p && strlen(p)) break;
        music_player_clear(music_player);
        music_player->model->tempo  = 0;
        music_player->model->paused = false;
        furi_string_reset(music_player->file_content);
    } while(1);

    furi_string_free(file_path);
    music_player_free(music_player);

    return 0;
}
