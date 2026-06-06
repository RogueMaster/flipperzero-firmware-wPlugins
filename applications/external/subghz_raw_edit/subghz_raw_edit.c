/*
 * Sub-GHz RAW Edit — a tiny waveform editor for Flipper Zero RAW .sub captures.
 * https://github.com/Lechnio/SubGHz-RAW-Edit
 */

#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <storage/storage.h>
#include <dialogs/dialogs.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <gui/modules/widget.h>

#include "subghz_raw_edit_icons.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SUBGHZ_DIR  "/ext/subghz"
#define MAX_SAMPLES 24000
#define DUR_CLAMP   32000

#define APP_VERSION "1.3"
#define APP_REPO    "github.com/Lechnio/SubGHz-RAW-Edit"

#define WAVE_TOP 17
#define WAVE_BOT 50
#define OV_Y     11
#define OV_H     5

typedef struct {
    int16_t* data;
    size_t count;
    size_t cap;
    int32_t total_us;
    uint32_t frequency;
    char preset[48];
    bool truncated;
    bool out_of_memory;
} SubData;

typedef struct {
    SubData sd;
    char basename[48];

    int32_t view_start;
    int32_t view_end;
    int32_t marker_a;
    int32_t marker_b;
    int active;

    uint16_t activity[128];
    uint16_t act_max;
    uint16_t overview[128];
    uint16_t ov_max;
    bool wave_mode;

    char status[80];
    uint32_t status_until;

    FuriMutex* mutex;
} App;

static inline int32_t iabs32(int32_t v) {
    return v < 0 ? -v : v;
}

static void fmt_time(int32_t us, char* out, size_t n) {
    if(us < 0) us = 0;
    if(us < 1000) {
        snprintf(out, n, "%luus", (unsigned long)us);
    } else if(us < 1000000) {
        long ms = us / 1000;
        long t = (us % 1000) / 100;
        snprintf(out, n, "%ld.%ldms", ms, t);
    } else {
        long s = us / 1000000;
        long h = (us % 1000000) / 10000;
        snprintf(out, n, "%ld.%02lds", s, h);
    }
}

static int time_to_x(App* a, int32_t t) {
    int32_t span = a->view_end - a->view_start;
    if(span <= 0) span = 1;
    return (int)(((int64_t)(t - a->view_start) * 128) / span);
}

static void clamp_marker(App* a, int idx) {
    int32_t* m = idx ? &a->marker_b : &a->marker_a;
    if(*m < 0) *m = 0;
    if(*m > a->sd.total_us) *m = a->sd.total_us;
}

static void clamp_view(App* a) {
    int32_t total = a->sd.total_us;
    if(total < 1) total = 1;
    int32_t margin = total / 3 + 50000;
    int32_t lo = -margin;
    int32_t hi = total + margin;
    int32_t fullspan = hi - lo;

    int32_t span = a->view_end - a->view_start;
    if(span < 200) span = 200;
    if(span > fullspan) span = fullspan;
    if(a->view_start < lo) a->view_start = lo;
    a->view_end = a->view_start + span;
    if(a->view_end > hi) {
        a->view_end = hi;
        a->view_start = hi - span;
        if(a->view_start < lo) a->view_start = lo;
    }
}

static void ensure_visible(App* a, int32_t m) {
    int32_t span = a->view_end - a->view_start;
    if(m < a->view_start) {
        a->view_start = m - span / 5;
        a->view_end = a->view_start + span;
    } else if(m > a->view_end) {
        a->view_end = m + span / 5;
        a->view_start = a->view_end - span;
    }
    clamp_view(a);
}

static void recompute_activity(App* a) {
    memset(a->activity, 0, sizeof(a->activity));
    int32_t span = a->view_end - a->view_start;
    if(span <= 0) span = 1;
    a->wave_mode = (span <= 30000);

    int32_t run = 0;
    uint16_t mx = 0;
    for(size_t i = 0; i < a->sd.count; i++) {
        int32_t ad = iabs32(a->sd.data[i]);
        int32_t s0 = run;
        run += ad;
        int32_t s1 = run;
        if(s1 < a->view_start || s0 > a->view_end) continue;
        int x = (int)(((int64_t)(s0 - a->view_start) * 128) / span);
        if(x < 0) x = 0;
        if(x > 127) x = 127;
        if(a->activity[x] < 65535) a->activity[x]++;
        if(a->activity[x] > mx) mx = a->activity[x];
    }
    a->act_max = mx ? mx : 1;
}

static void recompute_overview(App* a) {
    memset(a->overview, 0, sizeof(a->overview));
    int32_t total = a->sd.total_us;
    if(total < 1) total = 1;
    int32_t run = 0;
    uint16_t mx = 0;
    for(size_t i = 0; i < a->sd.count; i++) {
        int32_t ad = iabs32(a->sd.data[i]);
        int32_t s0 = run;
        run += ad;
        int x = (int)(((int64_t)s0 * 128) / total);
        if(x < 0) x = 0;
        if(x > 127) x = 127;
        if(a->overview[x] < 65535) a->overview[x]++;
        if(a->overview[x] > mx) mx = a->overview[x];
    }
    a->ov_max = mx ? mx : 1;
}

static void auto_detect(App* a) {
    int32_t total = a->sd.total_us;
    if(total < 1 || a->sd.count < 2) {
        a->view_start = 0;
        a->view_end = total > 0 ? total : 1;
        a->marker_a = total / 4;
        a->marker_b = (total * 3) / 4;
        return;
    }

    const int32_t GAP_US = 15000;
    const int32_t MIN_FRAME_US = 30000;

    int32_t run = 0;
    int32_t seg_start = -1;
    int32_t last_edge = 0;
    int32_t best_a = -1, best_b = -1;
    int32_t best_len = 0;

    for(size_t i = 0; i < a->sd.count; i++) {
        int32_t ad = iabs32(a->sd.data[i]);
        int32_t s0 = run;
        run += ad;
        int32_t s1 = run;

        if(ad >= GAP_US) {
            if(seg_start >= 0) {
                int32_t seglen = last_edge - seg_start;
                if(seglen >= MIN_FRAME_US && seglen > best_len) {
                    best_len = seglen;
                    best_a = seg_start;
                    best_b = last_edge;
                }
                seg_start = -1;
            }
        } else {
            if(seg_start < 0) seg_start = s0;
            last_edge = s1;
        }
    }

    if(seg_start >= 0) {
        int32_t seglen = last_edge - seg_start;
        if(seglen >= MIN_FRAME_US && seglen > best_len) {
            best_a = seg_start;
            best_b = last_edge;
        }
    }

    if(best_a < 0) {
        a->marker_a = 0;
        a->marker_b = total;
        a->view_start = 0;
        a->view_end = total;
        clamp_view(a);
        return;
    }

    int32_t pad = (best_b - best_a) / 6 + 500;
    a->marker_a = best_a;
    a->marker_b = best_b;
    a->view_start = best_a - pad * 3;
    a->view_end = best_b + pad * 3;
    clamp_view(a);
}

typedef struct {
    File* file;
    uint8_t buf[512];
    size_t len;
    size_t pos;
    bool eof;
} LineReader;

static bool lr_read_line(LineReader* lr, FuriString* out) {
    furi_string_reset(out);
    bool any = false;
    while(true) {
        if(lr->pos >= lr->len) {
            if(lr->eof) break;
            lr->len = storage_file_read(lr->file, lr->buf, sizeof(lr->buf));
            lr->pos = 0;
            if(lr->len == 0) {
                lr->eof = true;
                break;
            }
        }
        char c = (char)lr->buf[lr->pos++];
        if(c == '\n') {
            any = true;
            break;
        }
        if(c == '\r') continue;
        furi_string_push_back(out, c);
        any = true;
    }
    return any;
}

static bool append_sample(SubData* sd, int32_t v) {
    if(sd->count >= MAX_SAMPLES) {
        sd->truncated = true;
        return false;
    }

    if(sd->count >= sd->cap) {
        size_t ncap = sd->cap + 2000;
        if(ncap > MAX_SAMPLES) ncap = MAX_SAMPLES;
        size_t need = ncap * sizeof(int16_t);

        if(memmgr_get_free_heap() < need + 8192) {
            sd->out_of_memory = true;
            return false;
        }

        int16_t* nd = realloc(sd->data, need);
        if(!nd) {
            sd->out_of_memory = true;
            return false;
        }

        sd->data = nd;
        sd->cap = ncap;
    }

    if(v > DUR_CLAMP) v = DUR_CLAMP;
    if(v < -DUR_CLAMP) v = -DUR_CLAMP;

    sd->data[sd->count++] = (int16_t)v;

    return true;
}

static bool load_sub(Storage* storage, const char* path, SubData* sd) {
    memset(sd, 0, sizeof(*sd));
    sd->frequency = 433920000;
    strncpy(sd->preset, "FuriHalSubGhzPresetOok650Async", sizeof(sd->preset) - 1);

    File* f = storage_file_alloc(storage);
    if(!storage_file_open(f, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_free(f);
        return false;
    }

    LineReader lr = {.file = f, .len = 0, .pos = 0, .eof = false};
    FuriString* line = furi_string_alloc();
    bool stop = false;

    while(!stop && lr_read_line(&lr, line)) {
        const char* s = furi_string_get_cstr(line);
        if(strncmp(s, "RAW_Data:", 9) == 0) {
            const char* p = s + 9;
            char* end;
            while(*p) {
                long v = strtol(p, &end, 10);
                if(end == p) {
                    if(*p == '\0') break;
                    p++;
                    continue;
                }
                p = end;
                if(!append_sample(sd, (int32_t)v)) {
                    stop = true;
                    break;
                }
            }
        } else if(strncmp(s, "Frequency:", 10) == 0) {
            sd->frequency = (uint32_t)strtoul(s + 10, NULL, 10);
        } else if(strncmp(s, "Preset:", 7) == 0) {
            const char* p = s + 7;
            while(*p == ' ')
                p++;
            strncpy(sd->preset, p, sizeof(sd->preset) - 1);
            sd->preset[sizeof(sd->preset) - 1] = '\0';
        }
    }

    furi_string_free(line);
    storage_file_close(f);
    storage_file_free(f);

    if(sd->data && sd->count > 0 && sd->count < sd->cap) {
        int16_t* shrunk = realloc(sd->data, sd->count * sizeof(int16_t));
        if(shrunk) {
            sd->data = shrunk;
            sd->cap = sd->count;
        }
    }

    int64_t run = 0;
    for(size_t i = 0; i < sd->count; i++)
        run += iabs32(sd->data[i]);
    if(run > 0x7FFFFFFF) run = 0x7FFFFFFF;
    sd->total_us = (int32_t)run;
    return sd->count >= 2;
}

static bool do_save(Storage* st, App* a) {
    int32_t lo = a->marker_a, hi = a->marker_b;
    if(lo > hi) {
        int32_t t = lo;
        lo = hi;
        hi = t;
    }

    int32_t run = 0;
    size_t i0 = 0, i1 = 0;
    bool started = false;
    for(size_t i = 0; i < a->sd.count; i++) {
        int32_t ad = iabs32(a->sd.data[i]);
        int32_t s0 = run;
        run += ad;
        int32_t s1 = run;
        if(s1 > lo && s0 < hi) {
            if(!started) {
                i0 = i;
                started = true;
            }
            i1 = i;
        }
    }
    if(!started) {
        snprintf(a->status, sizeof(a->status), "Nothing in A..B");
        return false;
    }

    while(i0 < i1 && a->sd.data[i0] < 0)
        i0++;
    while(i1 > i0 && a->sd.data[i1] > 0)
        i1--;
    if(i0 >= i1) {
        snprintf(a->status, sizeof(a->status), "Range too small");
        return false;
    }

    FuriString* path = furi_string_alloc();
    char savename[64];
    int suffix = 0;
    while(true) {
        if(suffix == 0)
            snprintf(savename, sizeof(savename), "%s_trim", a->basename);
        else
            snprintf(savename, sizeof(savename), "%s_trim%d", a->basename, suffix);
        furi_string_printf(path, "%s/%s.sub", SUBGHZ_DIR, savename);
        if(!storage_file_exists(st, furi_string_get_cstr(path))) break;
        suffix++;
        if(suffix > 999) {
            furi_string_free(path);
            snprintf(a->status, sizeof(a->status), "Too many trims");
            return false;
        }
    }

    File* f = storage_file_alloc(st);
    if(!storage_file_open(f, furi_string_get_cstr(path), FSAM_WRITE, FSOM_CREATE_NEW)) {
        storage_file_free(f);
        furi_string_free(path);
        snprintf(a->status, sizeof(a->status), "Write failed");
        return false;
    }

    char hdr[192];
    int n = snprintf(
        hdr,
        sizeof(hdr),
        "Filetype: Flipper SubGhz RAW File\nVersion: 1\nFrequency: %lu\nPreset: %s\nProtocol: RAW\n",
        (unsigned long)a->sd.frequency,
        a->sd.preset);
    storage_file_write(f, hdr, n);

    FuriString* lbuf = furi_string_alloc();
    furi_string_set(lbuf, "RAW_Data:");
    int cnt = 0;
    for(size_t i = i0; i <= i1; i++) {
        char vb[16];
        snprintf(vb, sizeof(vb), " %ld", (long)a->sd.data[i]);
        furi_string_cat_str(lbuf, vb);
        if(++cnt >= 512) {
            furi_string_push_back(lbuf, '\n');
            storage_file_write(f, furi_string_get_cstr(lbuf), furi_string_size(lbuf));
            furi_string_set(lbuf, "RAW_Data:");
            cnt = 0;
        }
    }
    if(cnt > 0) {
        furi_string_push_back(lbuf, '\n');
        storage_file_write(f, furi_string_get_cstr(lbuf), furi_string_size(lbuf));
    }

    furi_string_free(lbuf);
    storage_file_close(f);
    storage_file_free(f);

    snprintf(a->status, sizeof(a->status), "Saved %s.sub", savename);
    furi_string_free(path);
    return true;
}

static void draw_marker(Canvas* c, int x, bool active) {
    if(x < 0) {
        canvas_draw_str(c, 0, WAVE_TOP - 3, "<");
        return;
    }
    if(x > 127) {
        canvas_draw_str_aligned(c, 127, WAVE_TOP - 3, AlignRight, AlignTop, ">");
        return;
    }
    if(active) {
        for(int y = WAVE_TOP; y <= WAVE_BOT; y++)
            canvas_draw_dot(c, x, y);
        canvas_draw_box(c, x - 1, WAVE_TOP - 4, 3, 3);
    } else {
        for(int y = WAVE_TOP; y <= WAVE_BOT; y += 2)
            canvas_draw_dot(c, x, y);
    }
}

static void draw_cb(Canvas* c, void* ctx) {
    App* a = ctx;
    furi_mutex_acquire(a->mutex, FuriWaitForever);

    canvas_clear(c);
    canvas_set_color(c, ColorBlack);
    canvas_set_font(c, FontSecondary);

    char nm[14];
    strncpy(nm, a->basename, sizeof(nm) - 1);
    nm[sizeof(nm) - 1] = '\0';
    canvas_draw_str(c, 0, 8, nm);

    char zb[14];
    fmt_time(a->view_end - a->view_start, zb, sizeof(zb));
    canvas_draw_str_aligned(c, 127, 8, AlignRight, AlignBottom, zb);

    for(int x = 0; x < 128; x++) {
        if(a->overview[x] > a->ov_max / 6) canvas_draw_dot(c, x, OV_Y + OV_H - 1);
    }

    int32_t total = a->sd.total_us > 0 ? a->sd.total_us : 1;
    int vx0 = (int)(((int64_t)a->view_start * 128) / total);
    int vx1 = (int)(((int64_t)a->view_end * 128) / total);
    if(vx0 < 0) vx0 = 0;
    if(vx0 > 127) vx0 = 127;
    if(vx1 <= vx0) vx1 = vx0 + 1;
    if(vx1 > 127) vx1 = 127;
    canvas_draw_frame(c, vx0, OV_Y, vx1 - vx0 + 1, OV_H);

    canvas_draw_line(c, 0, WAVE_BOT + 1, 127, WAVE_BOT + 1);

    if(a->wave_mode) {
        int yhi = WAVE_TOP + 2;
        int ylo = WAVE_BOT - 2;

        int32_t run = 0;
        int prev_x = -1;
        int prev_y = ylo;
        for(size_t i = 0; i < a->sd.count; i++) {
            int32_t ad = iabs32(a->sd.data[i]);
            int32_t t0 = run;
            run += ad;
            int32_t t1 = run;

            if(t1 < a->view_start || t0 > a->view_end) continue;

            int x0 = time_to_x(a, t0);
            int x1 = time_to_x(a, t1);
            if(x0 < 0) x0 = 0;
            if(x1 > 127) x1 = 127;

            int y = (a->sd.data[i] > 0) ? yhi : ylo;

            if(prev_x >= 0 && y != prev_y && x0 >= 0 && x0 <= 127)
                canvas_draw_line(c, x0, yhi, x0, ylo);

            if(x1 >= x0) canvas_draw_line(c, x0, y, x1, y);

            prev_x = x1;
            prev_y = y;
        }
    } else {
        for(int x = 0; x < 128; x++) {
            if(a->activity[x] == 0) continue;
            int h = (int)(((int32_t)a->activity[x] * (WAVE_BOT - WAVE_TOP - 1)) / a->act_max);
            if(h < 1) h = 1;
            canvas_draw_line(c, x, WAVE_BOT, x, WAVE_BOT - h);
        }
    }

    int xa = time_to_x(a, a->marker_a);
    int xb = time_to_x(a, a->marker_b);
    int xlo = xa < xb ? xa : xb;
    int xhi = xa < xb ? xb : xa;
    if(xlo < 0) xlo = 0;
    if(xhi > 127) xhi = 127;
    for(int x = xlo; x <= xhi; x += 2)
        canvas_draw_dot(c, x, WAVE_TOP - 1);

    draw_marker(c, time_to_x(a, a->marker_a), a->active == 0);
    draw_marker(c, time_to_x(a, a->marker_b), a->active == 1);

    char sa[14], sb[14], sl[14];
    fmt_time(a->marker_a, sa, sizeof(sa));
    fmt_time(a->marker_b, sb, sizeof(sb));
    int32_t len = a->marker_b - a->marker_a;
    if(len < 0) len = -len;
    fmt_time(len, sl, sizeof(sl));

    char l1[40];
    snprintf(
        l1,
        sizeof(l1),
        "%cA %s  %cB %s",
        a->active == 0 ? '>' : ' ',
        sa,
        a->active == 1 ? '>' : ' ',
        sb);
    canvas_draw_str(c, 0, 58, l1);

    char l2[40];
    snprintf(l2, sizeof(l2), "len %s  OK:A/B  holdOK:save", sl);
    canvas_draw_str(c, 0, 64, l2);

    if(a->status[0] && furi_get_tick() < a->status_until) {
        int w = 124;
        canvas_set_color(c, ColorWhite);
        canvas_draw_box(c, 2, 22, w, 22);
        canvas_set_color(c, ColorBlack);
        canvas_draw_frame(c, 2, 22, w, 22);

        const char* msg = a->status;
        if(strncmp(msg, "Saved ", 6) == 0) {
            canvas_draw_str_aligned(c, 64, 29, AlignCenter, AlignCenter, "Saved");
            canvas_draw_str_aligned(c, 64, 38, AlignCenter, AlignCenter, msg + 6);
        } else {
            canvas_draw_str_aligned(c, 64, 33, AlignCenter, AlignCenter, msg);
        }
    }

    furi_mutex_release(a->mutex);
}

static void input_cb(InputEvent* e, void* ctx) {
    FuriMessageQueue* q = ctx;
    furi_message_queue_put(q, e, FuriWaitForever);
}

static void zoom(App* a, bool in) {
    int32_t m = a->active ? a->marker_b : a->marker_a;
    int32_t span = a->view_end - a->view_start;
    int32_t nspan = in ? (span * 2 / 3) : (span * 3 / 2);
    if(nspan < 200) nspan = 200;
    int32_t fullspan = a->sd.total_us + 2 * (a->sd.total_us / 3 + 50000);
    if(nspan > fullspan) nspan = fullspan;
    if(nspan < 1) nspan = 1;
    int32_t off = (int32_t)(((int64_t)(m - a->view_start) * nspan) / (span > 0 ? span : 1));
    a->view_start = m - off;
    a->view_end = a->view_start + nspan;
    clamp_view(a);
}

static void run_editor(Storage* storage, DialogsApp* dialogs) {
    App* app = malloc(sizeof(App));
    memset(app, 0, sizeof(App));
    app->mutex = furi_mutex_alloc(FuriMutexTypeNormal);

    FuriString* path = furi_string_alloc_set(SUBGHZ_DIR);
    DialogsFileBrowserOptions br;
    dialog_file_browser_set_basic_options(&br, ".sub", &I_sub1_10px);
    br.base_path = SUBGHZ_DIR;
    bool picked = dialog_file_browser_show(dialogs, path, path, &br);

    bool loaded = false;
    if(picked) {
        const char* full = furi_string_get_cstr(path);
        const char* slash = strrchr(full, '/');
        const char* nm = slash ? slash + 1 : full;
        strncpy(app->basename, nm, sizeof(app->basename) - 1);
        char* dot = strrchr(app->basename, '.');
        if(dot) *dot = '\0';

        loaded = load_sub(storage, full, &app->sd);
    }

    if(app->sd.out_of_memory) {
        DialogMessage* m = dialog_message_alloc();
        dialog_message_set_header(m, "Out of memory", 64, 2, AlignCenter, AlignTop);
        dialog_message_set_text(
            m,
            "Not enough free RAM to\nload this capture.\nReboot Flipper, then\nopen the app first.",
            64,
            34,
            AlignCenter,
            AlignCenter);
        dialog_message_set_buttons(m, NULL, NULL, "OK");
        dialog_message_show(dialogs, m);
        dialog_message_free(m);
        goto cleanup;
    }

    if(!loaded) {
        if(picked) {
            DialogMessage* m = dialog_message_alloc();
            dialog_message_set_header(m, "Sub-GHz RAW Edit", 64, 4, AlignCenter, AlignTop);
            dialog_message_set_text(
                m, "Not a RAW capture\nor file is empty", 64, 32, AlignCenter, AlignCenter);
            dialog_message_set_buttons(m, NULL, NULL, "OK");
            dialog_message_show(dialogs, m);
            dialog_message_free(m);
        }
        goto cleanup;
    }

    auto_detect(app);
    recompute_overview(app);
    recompute_activity(app);

    Gui* gui = furi_record_open(RECORD_GUI);
    ViewPort* vp = view_port_alloc();
    FuriMessageQueue* queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    view_port_draw_callback_set(vp, draw_cb, app);
    view_port_input_callback_set(vp, input_cb, queue);
    gui_add_view_port(gui, vp, GuiLayerFullscreen);

    bool running = true;
    InputEvent e;
    while(running) {
        if(furi_message_queue_get(queue, &e, 100) == FuriStatusOk) {
            furi_mutex_acquire(app->mutex, FuriWaitForever);
            bool changed = false;

            if(e.type == InputTypePress || e.type == InputTypeRepeat) {
                int32_t span = app->view_end - app->view_start;
                int32_t step = span / 128;
                if(step < 1) step = 1;
                if(e.type == InputTypeRepeat) {
                    int32_t fast = span / 24;
                    if(fast > step) step = fast;
                }
                int32_t* m = app->active ? &app->marker_b : &app->marker_a;
                switch(e.key) {
                case InputKeyLeft:
                    *m -= step;
                    clamp_marker(app, app->active);
                    ensure_visible(app, *m);
                    changed = true;
                    break;
                case InputKeyRight:
                    *m += step;
                    clamp_marker(app, app->active);
                    ensure_visible(app, *m);
                    changed = true;
                    break;
                case InputKeyUp:
                    zoom(app, true);
                    changed = true;
                    break;
                case InputKeyDown:
                    zoom(app, false);
                    changed = true;
                    break;
                default:
                    break;
                }
            } else if(e.type == InputTypeShort) {
                if(e.key == InputKeyOk) {
                    app->active ^= 1;
                    changed = true;
                } else if(e.key == InputKeyBack) {
                    running = false;
                }
            } else if(e.type == InputTypeLong) {
                if(e.key == InputKeyOk) {
                    do_save(storage, app);
                    app->status_until = furi_get_tick() + 2000;
                    changed = true;
                } else if(e.key == InputKeyBack) {
                    running = false;
                }
            }

            if(changed) recompute_activity(app);
            furi_mutex_release(app->mutex);
            view_port_update(vp);
        } else {
            view_port_update(vp);
        }
    }

    gui_remove_view_port(gui, vp);
    view_port_free(vp);
    furi_message_queue_free(queue);
    furi_record_close(RECORD_GUI);

cleanup:
    furi_string_free(path);

    if(app->sd.data) free(app->sd.data);

    furi_mutex_free(app->mutex);
    free(app);
}

typedef enum {
    MenuViewSubmenu,
    MenuViewAbout,
} MenuViewId;

typedef enum {
    MenuItemSelectFile,
    MenuItemAbout,
} MenuItemId;

typedef struct {
    ViewDispatcher* view_dispatcher;
    Submenu* submenu;
    Widget* widget;
    Storage* storage;
    DialogsApp* dialogs;
    bool launch_editor;
} Menu;

static void menu_submenu_cb(void* context, uint32_t index) {
    Menu* menu = context;
    if(index == MenuItemSelectFile) {
        menu->launch_editor = true;
        view_dispatcher_stop(menu->view_dispatcher);
    } else if(index == MenuItemAbout) {
        view_dispatcher_switch_to_view(menu->view_dispatcher, MenuViewAbout);
    }
}

static uint32_t about_back_cb(void* context) {
    UNUSED(context);
    return MenuViewSubmenu;
}

static uint32_t submenu_back_cb(void* context) {
    UNUSED(context);
    return VIEW_NONE;
}

static void menu_build_about(Menu* menu) {
    widget_add_text_scroll_element(
        menu->widget,
        0,
        0,
        128,
        64,
        "\e#Sub-GHz RAW Edit\e#\n"
        "Version " APP_VERSION "\n"
        "\n"
        "Trim recorded RAW .sub\n"
        "captures down to a single\n"
        "clean frame, right on the\n"
        "Flipper. Auto-finds the\n"
        "signal, zoom in/out, set\n"
        "A/B and save.\n"
        "\n"
        "RX/analysis only - never\n"
        "transmits.\n"
        "\n"
        "by Lechnio\n" APP_REPO "\n");
}

int32_t subghz_raw_edit_app(void* p) {
    UNUSED(p);

    Menu* menu = malloc(sizeof(Menu));
    memset(menu, 0, sizeof(Menu));

    menu->storage = furi_record_open(RECORD_STORAGE);
    menu->dialogs = furi_record_open(RECORD_DIALOGS);
    Gui* gui = furi_record_open(RECORD_GUI);

    menu->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_enable_queue(menu->view_dispatcher);
    menu->submenu = submenu_alloc();
    menu->widget = widget_alloc();

    submenu_set_header(menu->submenu, "Sub-GHz RAW Edit");
    submenu_add_item(menu->submenu, "Select .sub file", MenuItemSelectFile, menu_submenu_cb, menu);
    submenu_add_item(menu->submenu, "About", MenuItemAbout, menu_submenu_cb, menu);

    menu_build_about(menu);

    view_set_previous_callback(submenu_get_view(menu->submenu), submenu_back_cb);
    view_set_previous_callback(widget_get_view(menu->widget), about_back_cb);

    view_dispatcher_attach_to_gui(menu->view_dispatcher, gui, ViewDispatcherTypeFullscreen);
    view_dispatcher_add_view(
        menu->view_dispatcher, MenuViewSubmenu, submenu_get_view(menu->submenu));
    view_dispatcher_add_view(menu->view_dispatcher, MenuViewAbout, widget_get_view(menu->widget));

    bool running = true;
    while(running) {
        menu->launch_editor = false;
        view_dispatcher_switch_to_view(menu->view_dispatcher, MenuViewSubmenu);
        view_dispatcher_run(menu->view_dispatcher);

        if(menu->launch_editor) {
            run_editor(menu->storage, menu->dialogs);
        } else {
            running = false;
        }
    }

    view_dispatcher_remove_view(menu->view_dispatcher, MenuViewSubmenu);
    view_dispatcher_remove_view(menu->view_dispatcher, MenuViewAbout);
    submenu_free(menu->submenu);
    widget_free(menu->widget);
    view_dispatcher_free(menu->view_dispatcher);

    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_DIALOGS);
    furi_record_close(RECORD_STORAGE);

    free(menu);
    return 0;
}
