/**
 * anim_previewer.c — Flipper Zero Animation Previewer
 *
 * Navigate the SD card with the built-in file browser.
 * Browse into any animation folder and tap meta.txt to play it.
 * Default starting location: /ext/dolphin
 *
 * Player controls
 *   OK           play / pause
 *   Up / Down    faster / slower
 *   Left / Right step one frame (paused only)
 *   Back (short) return to browser at current location
 *   Back (long)  exit app
 *
 * Browser
 *   Back         exit app
 */

#include <furi.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/file_browser.h>
#include <gui/view.h>
#include <storage/storage.h>
#include <toolbox/compress.h>
#include <toolbox/path.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* ------------------------------------------------------------------ */
/* Config                                                              */
/* ------------------------------------------------------------------ */

#define TAG              "AnimPreviewer"
#define START_PATH       "/ext/dolphin"
#define BASE_PATH        "/ext"
#define META_FILENAME    "meta.txt"
#define FILE_EXT         "txt"

#define MAX_NAME_LEN     64
#define MAX_FRAMES       128
#define MAX_BM_SIZE      8192
#define FRAME_MEM_BUDGET 120000u
#define DEFAULT_MS       100
#define MIN_MS           40

/* ------------------------------------------------------------------ */
/* View IDs                                                            */
/* ------------------------------------------------------------------ */

typedef enum {
    AppViewBrowser = 0,
    AppViewPlayer,
} AppViewId;

/* ------------------------------------------------------------------ */
/* Player model                                                        */
/* ------------------------------------------------------------------ */

typedef struct {
    uint8_t* frames[MAX_FRAMES];
    uint32_t frame_sizes[MAX_FRAMES];
    uint8_t  frame_count;
    uint8_t  current_frame;
    uint8_t  frame_w;
    uint8_t  frame_h;
    bool     loaded;
    bool     paused;
    uint32_t frame_ms;
    char     name[MAX_NAME_LEN];
} PlayerModel;

/* ------------------------------------------------------------------ */
/* App state                                                           */
/* ------------------------------------------------------------------ */

typedef struct {
    Storage*        storage;
    Gui*            gui;
    ViewDispatcher* view_dispatcher;
    FileBrowser*    file_browser;
    FuriString*     browser_path;
    View*           player_view;
    FuriTimer*      anim_timer;
} App;

/* ------------------------------------------------------------------ */
/* Meta parser                                                         */
/* ------------------------------------------------------------------ */

static bool parse_meta(
    App* app, const char* path,
    uint8_t* out_w, uint8_t* out_h,
    uint8_t* out_frames, uint32_t* out_frame_ms)
{
    *out_w = 0; *out_h = 0; *out_frames = 0; *out_frame_ms = DEFAULT_MS;

    File* f = storage_file_alloc(app->storage);
    if(!storage_file_open(f, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_free(f);
        return false;
    }
    char buf[512];
    size_t n = storage_file_read(f, buf, sizeof(buf) - 1);
    storage_file_close(f);
    storage_file_free(f);
    if(!n) return false;
    buf[n] = '\0';

    uint32_t w = 0, h = 0, frames = 0, duration = 0;
    const char* p;
    if((p = strstr(buf, "Width:")))    sscanf(p, "Width: %lu",    &w);
    if((p = strstr(buf, "Height:")))   sscanf(p, "Height: %lu",   &h);
    if((p = strstr(buf, "Frames:")))   sscanf(p, "Frames: %lu",   &frames);
    if((p = strstr(buf, "Duration:"))) sscanf(p, "Duration: %lu", &duration);

    *out_w      = (w > 0 && w <= 128) ? (uint8_t)w : 128;
    *out_h      = (h > 0 && h <= 64)  ? (uint8_t)h : 64;
    *out_frames = (frames > 0 && frames <= MAX_FRAMES) ? (uint8_t)frames :
                  (frames > MAX_FRAMES) ? MAX_FRAMES : 0;

    if(duration > 0 && frames > 0) {
        uint32_t ms = (duration * 33u) / frames;
        *out_frame_ms = ms < MIN_MS ? MIN_MS : ms;
    }
    return (*out_w > 0 && *out_h > 0);
}

/* ------------------------------------------------------------------ */
/* Frame decoder                                                       */
/* ------------------------------------------------------------------ */

static uint8_t* decode_frame(
    App* app, const char* path,
    uint8_t w, uint8_t h, uint32_t* out_size)
{
    *out_size = 0;
    FileInfo fi;
    if(storage_common_stat(app->storage, path, &fi) != FSE_OK) return NULL;
    if(fi.size > MAX_BM_SIZE || fi.size < 2) return NULL;

    File* f = storage_file_alloc(app->storage);
    if(!storage_file_open(f, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_free(f); return NULL;
    }
    uint8_t* raw = malloc(fi.size);
    if(!raw) { storage_file_close(f); storage_file_free(f); return NULL; }
    size_t got = storage_file_read(f, raw, fi.size);
    storage_file_close(f);
    storage_file_free(f);
    if(got != fi.size) { free(raw); return NULL; }

    uint32_t decoded_size = ((uint32_t)((w + 7) / 8)) * h;
    CompressIcon* ci = compress_icon_alloc(decoded_size);
    uint8_t* decoded = NULL;
    compress_icon_decode(ci, raw, &decoded);

    uint8_t* result = NULL;
    if(decoded) {
        /* Compressed path — copy out of CompressIcon's internal buffer */
        result = malloc(decoded_size);
        if(result) { memcpy(result, decoded, decoded_size); *out_size = decoded_size; }
        compress_icon_free(ci);
        free(raw);
    } else {
        /* Decompression failed — frame is likely stored as raw XBM bytes.
         * Use the file bytes directly if they are the right size. */
        compress_icon_free(ci);
        if(fi.size >= decoded_size) {
            /* Hand ownership of raw to the caller */
            result    = raw;
            *out_size = decoded_size;
        } else {
            free(raw);
        }
    }
    return result;
}

/* ------------------------------------------------------------------ */
/* Frame management                                                    */
/* ------------------------------------------------------------------ */

static void app_free_frames(App* app) {
    with_view_model(app->player_view, PlayerModel* m, {
        for(uint8_t i = 0; i < MAX_FRAMES; i++) {
            if(m->frames[i]) { free(m->frames[i]); m->frames[i] = NULL; }
            m->frame_sizes[i] = 0;
        }
        m->frame_count = 0; m->current_frame = 0; m->loaded = false;
    }, false);
}

/* anim_path is the directory containing meta.txt and frame_N.bm */
static void app_load_anim(App* app, const char* anim_path) {
    furi_timer_stop(app->anim_timer);
    app_free_frames(app);

    FuriString* path = furi_string_alloc();

    furi_string_printf(path, "%s/%s", anim_path, META_FILENAME);
    uint8_t w = 0, h = 0, frame_count_meta = 0;
    uint32_t frame_ms = DEFAULT_MS;

    if(!parse_meta(app, furi_string_get_cstr(path), &w, &h, &frame_count_meta, &frame_ms)) {
        FURI_LOG_W(TAG, "Meta parse failed: %s", anim_path);
        furi_string_free(path);
        return;
    }
    uint8_t scan_limit = frame_count_meta ? frame_count_meta : MAX_FRAMES;

    uint8_t* bufs[MAX_FRAMES];
    uint32_t szs[MAX_FRAMES];
    memset(bufs, 0, sizeof(bufs));
    memset(szs,  0, sizeof(szs));
    uint8_t  loaded    = 0;
    uint32_t total_mem = 0;

    for(uint8_t i = 0; i < scan_limit; i++) {
        furi_string_printf(path, "%s/frame_%u.bm", anim_path, (unsigned)i);
        uint32_t sz = 0;
        uint8_t* data = decode_frame(app, furi_string_get_cstr(path), w, h, &sz);
        if(!data) break;
        if(total_mem + sz > FRAME_MEM_BUDGET) { free(data); break; }
        bufs[loaded] = data; szs[loaded] = sz;
        total_mem += sz; loaded++;
    }
    furi_string_free(path);

    if(!loaded) { FURI_LOG_W(TAG, "No frames: %s", anim_path); return; }

    const char* last_slash = strrchr(anim_path, '/');
    const char* display_name = last_slash ? last_slash + 1 : anim_path;

    with_view_model(app->player_view, PlayerModel* m, {
        strncpy(m->name, display_name, MAX_NAME_LEN - 1);
        m->name[MAX_NAME_LEN - 1] = '\0';
        for(uint8_t i = 0; i < loaded; i++) {
            m->frames[i] = bufs[i]; m->frame_sizes[i] = szs[i];
        }
        m->frame_count   = loaded;
        m->current_frame = 0;
        m->frame_w       = w;
        m->frame_h       = h;
        m->loaded        = true;
        m->paused        = false;
        m->frame_ms      = frame_ms;
    }, false);

    FURI_LOG_I(TAG, "Loaded %s: %u frames %ux%u %lums",
               display_name, loaded, w, h, frame_ms);

    if(loaded > 1)
        furi_timer_start(app->anim_timer, furi_ms_to_ticks(frame_ms));
}

/* ------------------------------------------------------------------ */
/* Animation timer                                                     */
/* ------------------------------------------------------------------ */

static void anim_timer_cb(void* context) {
    App* app = context;
    with_view_model(app->player_view, PlayerModel* m, {
        if(m->frame_count > 1)
            m->current_frame = (m->current_frame + 1) % m->frame_count;
    }, true);
}

/* ------------------------------------------------------------------ */
/* Speed step                                                          */
/* ------------------------------------------------------------------ */

static uint32_t speed_step(uint32_t ms) {
    if(ms <= 100) return 10;
    if(ms <= 300) return 25;
    if(ms <= 600) return 50;
    return 100;
}

/* ------------------------------------------------------------------ */
/* Player — draw                                                       */
/* ------------------------------------------------------------------ */

static void player_draw(Canvas* canvas, void* _m) {
    PlayerModel* m = _m;
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    if(!m->loaded || !m->frame_count) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 28, AlignCenter, AlignCenter, "No frames");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 45, AlignCenter, AlignCenter, m->name);
        return;
    }

    uint8_t  cf        = m->current_frame;
    uint8_t* data      = m->frames[cf];
    uint32_t sz        = m->frame_sizes[cf];
    uint8_t  w         = m->frame_w;
    uint8_t  h         = m->frame_h;
    uint8_t  row_bytes = (w + 7) / 8;
    int8_t   ox        = (int8_t)((128 - (int16_t)w) / 2);
    int8_t   oy        = (int8_t)((64  - (int16_t)h) / 2);

    if(data && sz) {
        for(uint8_t py = 0; py < h; py++) {
            for(uint8_t px = 0; px < w; px++) {
                uint32_t bi = (uint32_t)py * row_bytes + px / 8;
                if(bi < sz && (data[bi] & (1u << (px % 8))))
                    canvas_draw_dot(canvas, (int32_t)ox + px, (int32_t)oy + py);
            }
        }
    }

    canvas_set_font(canvas, FontSecondary);

    char frame_str[10];
    snprintf(frame_str, sizeof(frame_str), "%u/%u",
             (unsigned)(cf + 1), (unsigned)m->frame_count);
    canvas_draw_str_aligned(canvas, 2,   63, AlignLeft,   AlignBottom, frame_str);
    canvas_draw_str_aligned(canvas, 64,  63, AlignCenter, AlignBottom,
                            m->paused ? (m->frame_count > 1 ? "|| < >" : "||") : "> OK");
    char spd[14];
    snprintf(spd, sizeof(spd), "%lums ^v", m->frame_ms);
    canvas_draw_str_aligned(canvas, 126, 63, AlignRight,  AlignBottom, spd);
}

/* ------------------------------------------------------------------ */
/* Player — input                                                      */
/* ------------------------------------------------------------------ */

static bool player_input(InputEvent* event, void* context) {
    App* app = context;

    if(event->type != InputTypeShort && event->type != InputTypeLong) return false;

    if(event->key == InputKeyBack) {
        if(event->type == InputTypeShort) {
            furi_timer_stop(app->anim_timer);
            app_free_frames(app);
            view_dispatcher_switch_to_view(app->view_dispatcher, AppViewBrowser);
            return true;
        }
        if(event->type == InputTypeLong) {
            furi_timer_stop(app->anim_timer);
            app_free_frames(app);
            view_dispatcher_stop(app->view_dispatcher);
            return true;
        }
    }

    if(event->type != InputTypeShort) return false;

    if(event->key == InputKeyOk) {
        bool now_paused = false; uint32_t ms = 0; uint8_t fc = 0;
        with_view_model(app->player_view, PlayerModel* m, {
            m->paused = !m->paused;
            now_paused = m->paused; ms = m->frame_ms; fc = m->frame_count;
        }, true);
        if(now_paused || fc <= 1) furi_timer_stop(app->anim_timer);
        else furi_timer_start(app->anim_timer, furi_ms_to_ticks(ms));
        return true;
    }

    if(event->key == InputKeyUp) {
        bool playing = false; uint32_t new_ms = 0;
        with_view_model(app->player_view, PlayerModel* m, {
            uint32_t step = speed_step(m->frame_ms);
            m->frame_ms = (m->frame_ms > MIN_MS + step) ? m->frame_ms - step : MIN_MS;
            new_ms = m->frame_ms; playing = !m->paused && m->frame_count > 1;
        }, true);
        if(playing) {
            furi_timer_stop(app->anim_timer);
            furi_timer_start(app->anim_timer, furi_ms_to_ticks(new_ms));
        }
        return true;
    }

    if(event->key == InputKeyDown) {
        bool playing = false; uint32_t new_ms = 0;
        with_view_model(app->player_view, PlayerModel* m, {
            uint32_t step = speed_step(m->frame_ms);
            m->frame_ms = (m->frame_ms + step <= 2000u) ? m->frame_ms + step : 2000u;
            new_ms = m->frame_ms; playing = !m->paused && m->frame_count > 1;
        }, true);
        if(playing) {
            furi_timer_stop(app->anim_timer);
            furi_timer_start(app->anim_timer, furi_ms_to_ticks(new_ms));
        }
        return true;
    }

    if(event->key == InputKeyLeft) {
        with_view_model(app->player_view, PlayerModel* m, {
            if(m->paused && m->frame_count > 1)
                m->current_frame = m->current_frame ? m->current_frame - 1 : m->frame_count - 1;
        }, true);
        return true;
    }

    if(event->key == InputKeyRight) {
        with_view_model(app->player_view, PlayerModel* m, {
            if(m->paused && m->frame_count > 1)
                m->current_frame = (m->current_frame + 1) % m->frame_count;
        }, true);
        return true;
    }

    return false;
}

/* ------------------------------------------------------------------ */
/* Navigation callbacks                                                */
/* ------------------------------------------------------------------ */

static uint32_t nav_exit(void* context)   { UNUSED(context); return VIEW_NONE; }
static uint32_t nav_browse(void* context) { UNUSED(context); return AppViewBrowser; }

/* ------------------------------------------------------------------ */
/* Browser callback                                                    */
/* ------------------------------------------------------------------ */

static void browser_callback(void* context) {
    App* app = context;

    /* browser_path holds the selected meta.txt path.
     * Strip the filename to get the animation directory. */
    FuriString* anim_dir = furi_string_alloc();
    path_extract_dirname(furi_string_get_cstr(app->browser_path), anim_dir);

    app_load_anim(app, furi_string_get_cstr(anim_dir));
    furi_string_free(anim_dir);

    view_dispatcher_switch_to_view(app->view_dispatcher, AppViewPlayer);
}

/* ------------------------------------------------------------------ */
/* Entry point                                                         */
/* ------------------------------------------------------------------ */

int32_t anim_previewer_app(void* p) {
    UNUSED(p);

    App* app = malloc(sizeof(App));
    memset(app, 0, sizeof(App));

    app->storage = furi_record_open(RECORD_STORAGE);
    app->gui     = furi_record_open(RECORD_GUI);

    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_attach_to_gui(
        app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    /* ---- File browser ---- */
    app->browser_path = furi_string_alloc_set_str(START_PATH);
    app->file_browser = file_browser_alloc(app->browser_path);
    file_browser_configure(
        app->file_browser,
        FILE_EXT,   /* show only .txt files (meta.txt)        */
        BASE_PATH,  /* user cannot navigate above /ext         */
        false,      /* skip_assets: off — show all folders     */
        true,       /* hide dot files                          */
        NULL,       /* no custom icon                          */
        false);     /* show the .txt extension in the listing  */
    file_browser_set_callback(app->file_browser, browser_callback, app);
    view_set_previous_callback(
        file_browser_get_view(app->file_browser), nav_exit);
    view_dispatcher_add_view(
        app->view_dispatcher, AppViewBrowser,
        file_browser_get_view(app->file_browser));

    /* ---- Player view ---- */
    app->player_view = view_alloc();
    view_allocate_model(app->player_view, ViewModelTypeLocking, sizeof(PlayerModel));
    view_set_draw_callback(app->player_view, player_draw);
    view_set_input_callback(app->player_view, player_input);
    view_set_context(app->player_view, app);
    view_set_previous_callback(app->player_view, nav_browse);
    view_dispatcher_add_view(
        app->view_dispatcher, AppViewPlayer, app->player_view);

    with_view_model(app->player_view, PlayerModel* m,
                    { memset(m, 0, sizeof(PlayerModel)); }, false);

    /* ---- Timer ---- */
    app->anim_timer = furi_timer_alloc(anim_timer_cb, FuriTimerTypePeriodic, app);

    /* ---- Run ---- */
    file_browser_start(app->file_browser, app->browser_path);
    view_dispatcher_switch_to_view(app->view_dispatcher, AppViewBrowser);
    view_dispatcher_run(app->view_dispatcher);

    /* ---- Cleanup ---- */
    furi_timer_stop(app->anim_timer);
    furi_timer_free(app->anim_timer);

    with_view_model(app->player_view, PlayerModel* m, {
        for(uint8_t i = 0; i < MAX_FRAMES; i++)
            if(m->frames[i]) { free(m->frames[i]); m->frames[i] = NULL; }
    }, false);

    file_browser_stop(app->file_browser);

    view_dispatcher_remove_view(app->view_dispatcher, AppViewPlayer);
    view_dispatcher_remove_view(app->view_dispatcher, AppViewBrowser);

    view_free(app->player_view);
    file_browser_free(app->file_browser);
    furi_string_free(app->browser_path);
    view_dispatcher_free(app->view_dispatcher);

    furi_record_close(RECORD_STORAGE);
    furi_record_close(RECORD_GUI);

    free(app);
    return 0;
}
