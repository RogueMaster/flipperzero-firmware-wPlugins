/**
 * FlipNote v3 - Virtual Buffer Edition
 * Copy/Paste + dynamic file loading for large files
 */
#include <furi.h>
#include <gui/gui.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/text_input.h>
#include <gui/modules/number_input.h>
#include <input/input.h>
#include <storage/storage.h>
#include <dialogs/dialogs.h>
#include <lib/toolbox/path.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ---- constants ---- */
#define TOTAL_MAX_LINES 2000
#define BUFFER_LINES    80
#define MAX_LINE        128
#define HEADER_H        9
#define SEP_Y           9
#define CONTENT_Y       10
#define SCALE_COUNT     8
#define DEFAULT_SCALE   3 /* 1.00x */
#define MENU_TAB_COUNT  3
#define MENU_FILE_COUNT 4
#define MENU_EDIT_COUNT 7
#define MENU_VIEW_COUNT 4
#define TMP_FILE        "/ext/.flipnote.tmp"

/* ---- enums ---- */
typedef enum {
    AppViewEditor = 0,
    AppViewText = 1,
    AppViewNum = 2
} AppViewId;
typedef enum {
    EvOpenFile = 0,
    EvPickFolder = 1
} CustomEv;
typedef enum {
    PNone = 0,
    PEditLine,
    PSavePath,
    PFindQ,
    PFindRepQ,
    PFindRepR,
    PGotoRow
} Pending;
typedef enum {
    ModeNormal = 0,
    ModeMenu = 1,
    ModeFind = 2
} EdMode;

/* ---- static data ---- */
static const float SCALES[SCALE_COUNT] = {0.25f, 0.50f, 0.75f, 1.00f, 1.25f, 1.50f, 1.75f, 2.00f};
static const char* TAB_NAMES[3] = {"F", "E", "V"};
static const char* FILE_ITEMS[4] = {"New", "Open", "Save", "Save As"};
static const char* EDIT_ITEMS[7] =
    {"Find", "Find+Replace", "Copy Line", "Paste Line", "Delete Row", "Clear All", "Goto Row"};
static const char* VIEW_ITEMS[4] = {"Scale", "Row Numbers", "First Row", "Last Row"};

/* ---- model (heap-allocated via view) ---- */
typedef struct {
    /* virtual file */
    char filename[256];
    bool is_new;
    bool dirty;
    uint32_t offsets[TOTAL_MAX_LINES]; /* byte offset of each line in file */
    int total; /* total lines in file             */
    int orig_end; /* first line after original buffer */
    int orig_buf_cnt; /* buf_count at load time           */
    /* active buffer */
    char buf[BUFFER_LINES][MAX_LINE];
    int buf_start;
    int buf_count;
    /* cursor */
    int cursor;
    int scroll;
    /* ui */
    EdMode mode;
    int mtab, mitem;
    bool scale_ed;
    bool row_nums;
    int scale;
    int hscroll; /* horizontal scroll in pixels */
    /* find */
    char fq[MAX_LINE]; /* find query */
    int fc; /* find current absolute line */
    bool fmode;
    int fidx;
    int ftotal;
} Model;

/* ---- app context ---- */
typedef struct {
    ViewDispatcher* vd;
    View* ev;
    TextInput* ti;
    NumberInput* ni;
    Pending pending;
    char ebuf[MAX_LINE]; /* edit buffer for text input */
    char ptxt[MAX_LINE]; /* pending text (find query temp) */
    char savename[256]; /* filename part for save-as folder flow */
    char clipboard[MAX_LINE];
    bool clip_ok;
    int edit_idx;
    int goto_cnt, goto_cur;
} App;

/* ================================================================
   HELPERS
   ================================================================ */
static int lh(int si) {
    int h = (int)(13.0f * SCALES[si]);
    if(h < 7) h = 7;
    if(h > 40) h = 40;
    return h;
}
static Font lfont(int si) {
    return SCALES[si] >= 1.25f ? FontPrimary : FontSecondary;
}
static int vis(int si) {
    int v = (64 - CONTENT_Y) / lh(si);
    return v < 1 ? 1 : v;
}
static int mitems(int t) {
    if(t == 0) return MENU_FILE_COUNT;
    if(t == 1) return MENU_EDIT_COUNT;
    return MENU_VIEW_COUNT;
}
static const char* mname(int t, int i) {
    if(t == 0) return FILE_ITEMS[i];
    if(t == 1) return EDIT_ITEMS[i];
    return VIEW_ITEMS[i];
}
static void fix_scroll(Model* m) {
    int v = vis(m->scale);
    if(m->cursor < m->scroll) m->scroll = m->cursor;
    if(m->cursor >= m->scroll + v) m->scroll = m->cursor - v + 1;
    if(m->scroll < 0) m->scroll = 0;
}
static const char* get_line(Model* m, int idx) {
    int rel = idx - m->buf_start;
    if(rel >= 0 && rel < m->buf_count) return m->buf[rel];
    return "";
}

/* ================================================================
   FILE INDEXING
   ================================================================ */
static void index_file(Model* m, const char* path) {
    Storage* s = furi_record_open(RECORD_STORAGE);
    File* f = storage_file_alloc(s);
    m->total = 0;
    m->offsets[0] = 0;
    if(storage_file_open(f, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        uint32_t off = 0;
        uint8_t ch;
        while(storage_file_read(f, &ch, 1) == 1) {
            if(ch == '\r') {
                off++;
                continue;
            }
            off++;
            if(ch == '\n' && m->total < TOTAL_MAX_LINES - 1) {
                m->total++;
                m->offsets[m->total] = off;
            }
        }
        /* last line (no trailing newline): total already set correctly as
           total = number of newlines = number of complete lines seen */
        /* If file ended without newline and total==0, we still have 1 line */
        if(m->total == 0) m->total = 1;
        storage_file_close(f);
    } else {
        m->total = 1;
    }
    storage_file_free(f);
    furi_record_close(RECORD_STORAGE);
}

/* ================================================================
   BUFFER LOADING
   ================================================================ */
static void load_buf(Model* m, const char* path, int start) {
    if(start < 0) start = 0;
    if(start >= m->total) start = m->total - 1;
    m->buf_start = start;
    m->buf_count = 0;

    Storage* s = furi_record_open(RECORD_STORAGE);
    File* f = storage_file_alloc(s);
    if(storage_file_open(f, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_seek(f, m->offsets[start], true);
        int limit = BUFFER_LINES;
        if(start + limit > m->total) limit = m->total - start;
        for(int i = 0; i < limit; i++) {
            char* b = m->buf[i];
            int pos = 0;
            uint8_t ch;
            while(pos < MAX_LINE - 1 && storage_file_read(f, &ch, 1) == 1) {
                if(ch == '\r') continue;
                if(ch == '\n') break;
                b[pos++] = (char)ch;
            }
            b[pos] = '\0';
            m->buf_count++;
        }
        storage_file_close(f);
    }
    if(m->buf_count == 0) {
        m->buf[0][0] = '\0';
        m->buf_count = 1;
    }
    m->orig_end = start + m->buf_count;
    m->orig_buf_cnt = m->buf_count;
    storage_file_free(f);
    furi_record_close(RECORD_STORAGE);
}

/* ================================================================
   SAVE (virtual buffer aware)
   ================================================================ */
static void save_virtual(Model* m, const char* dst) {
    bool same = !m->is_new && (strcmp(dst, m->filename) == 0);
    const char* wpath = same ? TMP_FILE : dst;

    Storage* stor = furi_record_open(RECORD_STORAGE);
    File* rf = storage_file_alloc(stor);
    File* wf = storage_file_alloc(stor);

    bool has_src = !m->is_new && storage_file_open(rf, m->filename, FSAM_READ, FSOM_OPEN_EXISTING);

    if(storage_file_open(wf, wpath, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        uint8_t chunk[128];
        /* 1. Lines before buffer: copy raw from source */
        if(has_src && m->buf_start > 0) {
            storage_file_seek(rf, 0, true);
            uint32_t remain = m->offsets[m->buf_start];
            while(remain > 0) {
                uint16_t rd = remain > 128 ? 128 : (uint16_t)remain;
                uint16_t got = storage_file_read(rf, chunk, rd);
                if(!got) break;
                storage_file_write(wf, chunk, got);
                remain -= got;
            }
        }
        /* 2. Buffer lines */
        for(int i = 0; i < m->buf_count; i++) {
            uint16_t len = (uint16_t)strlen(m->buf[i]);
            if(len) storage_file_write(wf, m->buf[i], len);
            storage_file_write(wf, "\n", 1);
        }
        /* 3. Lines after original buffer: copy raw from source */
        if(has_src && m->orig_end < m->total) {
            storage_file_seek(rf, m->offsets[m->orig_end], true);
            uint16_t got;
            while((got = storage_file_read(rf, chunk, sizeof(chunk))) > 0)
                storage_file_write(wf, chunk, got);
        }
        storage_file_close(wf);
    }
    if(has_src) storage_file_close(rf);
    storage_file_free(rf);
    storage_file_free(wf);

    if(same) {
        storage_simply_remove(stor, m->filename);
        storage_common_rename(stor, TMP_FILE, m->filename);
    }
    furi_record_close(RECORD_STORAGE);

    strncpy(m->filename, dst, sizeof(m->filename) - 1);
    m->is_new = false;
    m->dirty = false;
    index_file(m, m->filename);
    int ns = m->cursor - BUFFER_LINES / 2;
    if(ns < 0) ns = 0;
    load_buf(m, m->filename, ns);
}

/* Flush dirty buffer before reloading at new position */
static void flush_and_reload(Model* m, int new_center) {
    if(m->dirty) save_virtual(m, m->filename);
    int ns = new_center - BUFFER_LINES / 2;
    if(ns < 0) ns = 0;
    load_buf(m, m->filename, ns);
}

/* ================================================================
   INSERT / DELETE LINES (within buffer)
   ================================================================ */
static void insert_line_below(Model* m) {
    if(m->buf_count >= BUFFER_LINES) return;
    int rel = m->cursor - m->buf_start;
    if(rel < 0 || rel >= m->buf_count) return;
    /* shift buffer down */
    for(int i = m->buf_count; i > rel + 1; i--)
        memcpy(m->buf[i], m->buf[i - 1], MAX_LINE);
    m->buf[rel + 1][0] = '\0';
    m->buf_count++;
    m->total++;
    /* update offsets for lines after orig_end */
    /* (orig_end lines are unchanged in source, offsets still valid) */
    m->cursor++;
    fix_scroll(m);
    m->dirty = true;
}

static void delete_line(Model* m) {
    int rel = m->cursor - m->buf_start;
    if(rel < 0 || rel >= m->buf_count) return;
    if(m->total <= 1) {
        m->buf[rel][0] = '\0';
        m->dirty = true;
        return;
    }
    for(int i = rel; i < m->buf_count - 1; i++)
        memcpy(m->buf[i], m->buf[i + 1], MAX_LINE);
    m->buf_count--;
    m->total--;
    if(m->cursor >= m->total) m->cursor = m->total - 1;
    fix_scroll(m);
    m->dirty = true;
}

/* ================================================================
   FIND (scan through entire file + buffer)
   ================================================================ */
static int find_next(Model* m, const char* path, int from, bool forward) {
    if(!m->fq[0]) return -1;
    int step = forward ? 1 : -1;
    int start = (from + step + m->total) % m->total;
    int cur = start;
    Storage* stor = furi_record_open(RECORD_STORAGE);
    File* f = storage_file_alloc(stor);
    bool opened = !m->is_new && storage_file_open(f, path, FSAM_READ, FSOM_OPEN_EXISTING);
    int found = -1;
    for(int tries = 0; tries < m->total; tries++) {
        int rel = cur - m->buf_start;
        const char* line;
        char tmp[MAX_LINE];
        if(rel >= 0 && rel < m->buf_count) {
            line = m->buf[rel];
        } else if(opened && cur < m->total) {
            storage_file_seek(f, m->offsets[cur], true);
            int pos = 0;
            uint8_t ch;
            while(pos < MAX_LINE - 1 && storage_file_read(f, &ch, 1) == 1) {
                if(ch == '\r') {
                    continue;
                }
                if(ch == '\n') {
                    break;
                }
                tmp[pos++] = (char)ch;
            }
            tmp[pos] = '\0';
            line = tmp;
        } else {
            line = "";
        }
        if(strstr(line, m->fq)) {
            found = cur;
            break;
        }
        cur = (cur + step + m->total) % m->total;
    }
    if(opened) storage_file_close(f);
    storage_file_free(f);
    furi_record_close(RECORD_STORAGE);
    return found;
}

static void do_find_replace_virtual(Model* m, const char* path, const char* q, const char* r) {
    if(!q[0]) return;
    size_t qlen = strlen(q), rlen = strlen(r);
    const char* wpath = TMP_FILE;
    Storage* stor = furi_record_open(RECORD_STORAGE);
    File* rf = storage_file_alloc(stor);
    File* wf = storage_file_alloc(stor);
    bool opened = !m->is_new && storage_file_open(rf, path, FSAM_READ, FSOM_OPEN_EXISTING);
    if(storage_file_open(wf, wpath, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        for(int ln = 0; ln < m->total; ln++) {
            char line[MAX_LINE];
            int rel = ln - m->buf_start;
            if(rel >= 0 && rel < m->buf_count) {
                strncpy(line, m->buf[rel], MAX_LINE - 1);
            } else if(opened) {
                storage_file_seek(rf, m->offsets[ln], true);
                int pos = 0;
                uint8_t ch;
                while(pos < MAX_LINE - 1 && storage_file_read(rf, &ch, 1) == 1) {
                    if(ch == '\r') {
                        continue;
                    }
                    if(ch == '\n') {
                        break;
                    }
                    line[pos++] = (char)ch;
                }
                line[pos] = '\0';
            } else {
                line[0] = '\0';
            }
            /* do replacement in line */
            char out[MAX_LINE];
            size_t oi = 0;
            char* p = line;
            while(*p && oi < MAX_LINE - 1) {
                if(strncmp(p, q, qlen) == 0) {
                    size_t rc = rlen < (MAX_LINE - 1 - oi) ? rlen : (MAX_LINE - 1 - oi);
                    memcpy(out + oi, r, rc);
                    oi += rc;
                    p += qlen;
                } else {
                    out[oi++] = *p++;
                }
            }
            out[oi] = '\0';
            uint16_t len = (uint16_t)strlen(out);
            if(len) storage_file_write(wf, out, len);
            storage_file_write(wf, "\n", 1);
        }
        storage_file_close(wf);
    }
    if(opened) storage_file_close(rf);
    storage_file_free(rf);
    storage_file_free(wf);
    bool is_self = !m->is_new && (strcmp(path, m->filename) == 0);
    if(is_self) {
        storage_simply_remove(stor, m->filename);
        storage_common_rename(stor, TMP_FILE, m->filename);
    }
    furi_record_close(RECORD_STORAGE);
    if(is_self) {
        index_file(m, m->filename);
        int ns = m->cursor - BUFFER_LINES / 2;
        if(ns < 0) ns = 0;
        load_buf(m, m->filename, ns);
    }
    m->dirty = false;
}

/* ================================================================
   DRAW
   ================================================================ */
static void draw_cb(Canvas* canvas, void* ctx) {
    Model* m = (Model*)ctx;
    canvas_clear(canvas);
    canvas_set_font(canvas, FontSecondary);

    /* header */
    if(m->fmode) {
        char hdr[MAX_LINE + 4];
        snprintf(hdr, sizeof(hdr), "/%s", m->fq);
        canvas_draw_str(canvas, 1, 7, hdr);
        char cnt[24];
        if(m->ftotal > 0)
            snprintf(cnt, sizeof(cnt), "%d/%d", m->fidx + 1, m->ftotal);
        else
            snprintf(cnt, sizeof(cnt), "no match");
        uint16_t w = canvas_string_width(canvas, cnt);
        canvas_draw_str(canvas, (int32_t)(127 - w), 7, cnt);
    } else {
        const int tx[3] = {1, 14, 27};
        const int tw = 11;
        for(int t = 0; t < MENU_TAB_COUNT; t++) {
            bool act = (m->mode == ModeMenu && m->mtab == t);
            if(act) {
                canvas_draw_box(canvas, tx[t], 0, tw, HEADER_H);
                canvas_set_color(canvas, ColorWhite);
            }
            canvas_draw_str(canvas, tx[t] + 2, 7, TAB_NAMES[t]);
            if(act) canvas_set_color(canvas, ColorBlack);
        }
        canvas_draw_line(canvas, 40, 0, 40, HEADER_H - 1);
        char disp[48];
        if(m->is_new)
            snprintf(disp, sizeof(disp), "%s(new)", m->dirty ? "*" : "");
        else {
            const char* fn = m->filename;
            const char* sl = strrchr(fn, '/');
            if(sl) fn = sl + 1;
            snprintf(disp, sizeof(disp), "%s%.26s", m->dirty ? "*" : "", fn);
        }
        canvas_draw_str(canvas, 42, 7, disp);
        char info[24];
        snprintf(info, sizeof(info), "%dL", m->total);
        uint16_t w = canvas_string_width(canvas, info);
        canvas_draw_str(canvas, (int32_t)(127 - w), 7, info);
    }
    canvas_draw_line(canvas, 0, SEP_Y, 127, SEP_Y);
    /* hscroll indicators */
    if(m->hscroll > 0) {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 0, SEP_Y - 1, "<");
    }

    /* content */
    int lhv = lh(m->scale);
    int vv = vis(m->scale);
    int rw = m->row_nums ? 15 : 0;
    int cx = 1 + rw + 6;
    for(int i = 0; i < vv; i++) {
        int idx = m->scroll + i;
        if(idx >= m->total) break;
        int ty = CONTENT_Y + i * lhv, by = ty + lhv - 2;
        bool isc = (idx == m->cursor);
        bool ism = m->fmode && (idx == m->fc);
        if(isc)
            canvas_draw_box(canvas, cx - 1, ty, 128 - cx + 1, lhv);
        else if(ism)
            for(int dx = cx; dx < 127; dx += 2)
                canvas_draw_dot(canvas, dx, ty + lhv - 1);
        /* 1. draw text first (may overflow into left margin) */
        canvas_set_font(canvas, lfont(m->scale));
        if(isc) canvas_set_color(canvas, ColorWhite);
        canvas_draw_str(canvas, cx - m->hscroll, by, get_line(m, idx));
        if(isc) canvas_set_color(canvas, ColorBlack);
        /* 2. mask left margin to hide overflow */
        if(m->hscroll > 0 && cx > 1) {
            canvas_set_color(canvas, isc ? ColorBlack : ColorWhite);
            canvas_draw_box(canvas, 0, ty, (size_t)(cx - 1), (size_t)lhv);
            canvas_set_color(canvas, ColorBlack);
        }
        /* 3. redraw row numbers and cursor on top of mask */
        if(m->row_nums) {
            char rn[12];
            snprintf(rn, sizeof(rn), "%d", idx + 1);
            canvas_set_font(canvas, FontSecondary);
            if(isc) canvas_set_color(canvas, ColorWhite);
            canvas_draw_str(canvas, 1, by, rn);
            if(isc) canvas_set_color(canvas, ColorBlack);
        }
        if(isc) {
            canvas_set_color(canvas, ColorWhite);
            canvas_draw_str(canvas, 1 + rw, by, ">");
            canvas_set_color(canvas, ColorBlack);
        }
    }

    /* menu dropdown */
    if(m->mode == ModeMenu) {
        int tab = m->mtab, ic = mitems(tab);
        int ih = 8, mw = 78;
        int max_vis = (63 - HEADER_H - 1 - 3) / ih;
        int scroll_off = (m->mitem >= max_vis) ? (m->mitem - max_vis + 1) : 0;
        int drawn = ic - scroll_off;
        if(drawn > max_vis) drawn = max_vis;
        int mh = drawn * ih + 3;
        const int tabx[3] = {1, 14, 27};
        int mx = tabx[tab];
        if(mx + mw > 127) mx = 127 - mw;
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_rbox(canvas, mx, HEADER_H + 1, mw, mh, 2);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_rframe(canvas, mx, HEADER_H + 1, mw, mh, 2);
        canvas_set_font(canvas, FontSecondary);
        for(int i = 0; i < drawn; i++) {
            int item = i + scroll_off;
            int iy = HEADER_H + 2 + i * ih;
            bool sel = (item == m->mitem);
            if(sel) {
                canvas_set_color(canvas, ColorWhite);
                canvas_draw_box(canvas, mx + 1, iy, mw - 2, ih);
                canvas_set_color(canvas, ColorBlack);
            } else
                canvas_set_color(canvas, ColorWhite);
            char lbl[40];
            if(tab == 2 && item == 0) {
                if(m->scale_ed)
                    snprintf(lbl, sizeof(lbl), "<%.2f>", (double)SCALES[m->scale]);
                else
                    snprintf(lbl, sizeof(lbl), "Scale %.2f", (double)SCALES[m->scale]);
            } else if(tab == 2 && item == 1) {
                snprintf(lbl, sizeof(lbl), "Row# [%s]", m->row_nums ? "ON" : "OFF");
            } else {
                snprintf(lbl, sizeof(lbl), "%s", mname(tab, item));
            }
            canvas_draw_str(canvas, mx + 4, iy + ih - 2, lbl);
        }
        /* scroll arrows */
        if(scroll_off > 0) canvas_draw_str(canvas, mx + mw - 8, HEADER_H + 3, "^");
        if(scroll_off + drawn < ic) canvas_draw_str(canvas, mx + mw - 8, HEADER_H + mh - 2, "v");
        canvas_set_color(canvas, ColorBlack);
    }
}

/* ================================================================
   FORWARD DECLS
   ================================================================ */
static void text_done(void* ctx);
static void num_done(void* ctx, int32_t n);

static void show_text(App* app, const char* hdr, const char* pre) {
    strncpy(app->ebuf, pre ? pre : "", MAX_LINE - 1);
    app->ebuf[MAX_LINE - 1] = '\0';
    text_input_reset(app->ti);
    text_input_set_header_text(app->ti, hdr);
#ifdef MOMENTUM_FW
    text_input_show_illegal_symbols(app->ti, true);
#endif
    text_input_set_result_callback(app->ti, text_done, app, app->ebuf, MAX_LINE, false);
    view_dispatcher_switch_to_view(app->vd, AppViewText);
}

/* ================================================================
   CUSTOM EVENTS (blocking dialogs on main thread)
   ================================================================ */
static bool custom_ev(void* ctx, uint32_t ev) {
    App* app = (App*)ctx;
    if(ev == EvOpenFile || ev == EvPickFolder) {
        DialogsApp* d = furi_record_open(RECORD_DIALOGS);
        FuriString* p = furi_string_alloc_set_str("/ext");
        DialogsFileBrowserOptions opts;
        dialog_file_browser_set_basic_options(&opts, "*", NULL);
        opts.hide_dot_files = true;
        if(dialog_file_browser_show(d, p, p, &opts)) {
            Model* m = (Model*)view_get_model(app->ev);
            if(ev == EvOpenFile) {
                strncpy(m->filename, furi_string_get_cstr(p), sizeof(m->filename) - 1);
                m->is_new = false;
                m->dirty = false;
                index_file(m, m->filename);
                load_buf(m, m->filename, 0);
                m->cursor = 0;
                m->scroll = 0;
                m->hscroll = 0;
            } else {
                FuriString* dir = furi_string_alloc();
                path_extract_dirname(furi_string_get_cstr(p), dir);
                char full[256];
                const char* ds = furi_string_get_cstr(dir);
                size_t dl = strlen(ds), fl = strlen(app->savename);
                if(dl + 1 + fl < sizeof(full) - 1) {
                    memcpy(full, ds, dl);
                    full[dl] = '/';
                    memcpy(full + dl + 1, app->savename, fl + 1);
                } else {
                    strncpy(full, ds, sizeof(full) - 1);
                    full[sizeof(full) - 1] = '\0';
                }
                save_virtual(m, full);
                furi_string_free(dir);
            }
            view_commit_model(app->ev, true);
        }
        furi_string_free(p);
        furi_record_close(RECORD_DIALOGS);
        return true;
    }
    return false;
}

/* ================================================================
   TEXT INPUT DONE
   ================================================================ */
static void text_done(void* ctx) {
    App* app = ctx;
    Pending act = app->pending;
    app->pending = PNone;
    Model* m = (Model*)view_get_model(app->ev);
    switch(act) {
    case PEditLine:
        strncpy(m->buf[m->cursor - m->buf_start], app->ebuf, MAX_LINE - 1);
        m->buf[m->cursor - m->buf_start][MAX_LINE - 1] = '\0';
        m->dirty = true;
        view_commit_model(app->ev, true);
        view_dispatcher_switch_to_view(app->vd, AppViewEditor);
        break;
    case PSavePath:
        view_commit_model(app->ev, false);
        if(app->ebuf[0] == '/') {
            m = (Model*)view_get_model(app->ev);
            save_virtual(m, app->ebuf);
            view_commit_model(app->ev, true);
            view_dispatcher_switch_to_view(app->vd, AppViewEditor);
        } else {
            strncpy(app->savename, app->ebuf, sizeof(app->savename) - 1);
            view_dispatcher_switch_to_view(app->vd, AppViewEditor);
            view_dispatcher_send_custom_event(app->vd, EvPickFolder);
        }
        break;
    case PFindQ: {
        strncpy(m->fq, app->ebuf, MAX_LINE - 1);
        int found = find_next(m, m->filename, m->cursor - 1, true);
        m->fmode = true;
        m->ftotal = 0;
        m->fidx = 0;
        if(found >= 0) {
            m->fc = found;
            m->fidx = 0;
            m->ftotal = 1;
            m->cursor = found;
            fix_scroll(m);
        } else {
            m->fc = -1;
        }
        view_commit_model(app->ev, true);
        view_dispatcher_switch_to_view(app->vd, AppViewEditor);
        break;
    }
    case PFindRepQ:
        view_commit_model(app->ev, false);
        strncpy(app->ptxt, app->ebuf, MAX_LINE - 1);
        app->pending = PFindRepR;
        show_text(app, "Replace with", "");
        break;
    case PFindRepR:
        do_find_replace_virtual(m, m->filename, app->ptxt, app->ebuf);
        view_commit_model(app->ev, true);
        view_dispatcher_switch_to_view(app->vd, AppViewEditor);
        break;
    default:
        view_commit_model(app->ev, false);
        view_dispatcher_switch_to_view(app->vd, AppViewEditor);
        break;
    }
}

/* ================================================================
   NUMBER INPUT DONE (goto row)
   ================================================================ */
static void num_done(void* ctx, int32_t n) {
    App* app = ctx;
    Model* m = (Model*)view_get_model(app->ev);
    int t = (int)n - 1;
    if(t < 0) {
        t = 0;
    }
    if(t >= m->total) {
        t = m->total - 1;
    }
    /* need to buffer-load for new cursor position */
    if(t < m->buf_start || t >= m->buf_start + m->buf_count) flush_and_reload(m, t);
    m->cursor = t;
    fix_scroll(m);
    view_commit_model(app->ev, true);
    view_dispatcher_switch_to_view(app->vd, AppViewEditor);
}

/* ================================================================
   NAV CALLBACK
   ================================================================ */
static bool nav_cb(void* ctx) {
    App* app = ctx;
    app->pending = PNone;
    view_dispatcher_switch_to_view(app->vd, AppViewEditor);
    return true;
}

/* ================================================================
   INPUT CALLBACK
   ================================================================ */
static bool input_cb(InputEvent* ev, void* ctx) {
    App* app = ctx;
    if(ev->type != InputTypeShort && ev->type != InputTypeLong && ev->type != InputTypeRepeat)
        return false;

    Model* m = (Model*)view_get_model(app->ev);
    bool stop = false;
    Pending post = PNone;
    bool need_reload = false;

    if(m->fmode) {
        /* FIND NAV MODE */
        if(ev->key == InputKeyBack) {
            m->fmode = false;
            m->fc = -1;
        } else if(ev->key == InputKeyDown || ev->key == InputKeyUp) {
            bool fwd = (ev->key == InputKeyDown);
            int found = find_next(m, m->filename, m->fc, fwd);
            if(found >= 0) {
                m->fc = found;
                m->cursor = found;
                if(found < m->buf_start || found >= m->buf_start + m->buf_count)
                    need_reload = true;
                else
                    fix_scroll(m);
            }
        }
    } else if(m->mode == ModeNormal) {
        /* EDITOR MODE */
        if(ev->key == InputKeyBack) {
            if(ev->type == InputTypeLong)
                stop = true;
            else {
                m->mode = ModeMenu;
                m->mtab = 0;
                m->mitem = 0;
            }
        } else if(ev->key == InputKeyUp) {
            if(m->cursor > 0) {
                m->cursor--;
                if(m->cursor < m->buf_start + 5 && m->buf_start > 0)
                    need_reload = true;
                else
                    fix_scroll(m);
            }
        } else if(ev->key == InputKeyDown) {
            if(m->cursor < m->total - 1) {
                m->cursor++;
                int bend = m->buf_start + m->buf_count;
                if(m->cursor >= bend - 5 && bend < m->total)
                    need_reload = true;
                else
                    fix_scroll(m);
            }
        } else if(ev->key == InputKeyLeft) {
            m->hscroll -= 6;
            if(m->hscroll < 0) m->hscroll = 0;
        } else if(ev->key == InputKeyRight) {
            m->hscroll += 6;
        } else if(ev->key == InputKeyOk) {
            if(ev->type == InputTypeLong) {
                int rel = m->cursor - m->buf_start;
                if(rel >= 0 && rel < m->buf_count) {
                    app->edit_idx = m->cursor;
                    strncpy(app->ebuf, m->buf[rel], MAX_LINE - 1);
                    app->ebuf[MAX_LINE - 1] = '\0';
                    post = PEditLine;
                }
            } else {
                insert_line_below(m);
            }
        }
    } else {
        /* MENU MODE */
        if(m->scale_ed) {
            if(ev->key == InputKeyLeft && m->scale > 0)
                m->scale--;
            else if(ev->key == InputKeyRight && m->scale < SCALE_COUNT - 1)
                m->scale++;
            else if(ev->key == InputKeyOk || ev->key == InputKeyBack) {
                m->scale_ed = false;
                m->mode = ModeNormal;
            }
        } else {
            if(ev->key == InputKeyBack) {
                m->mode = ModeNormal;
            } else if(ev->key == InputKeyLeft) {
                if(m->mtab > 0) {
                    m->mtab--;
                    m->mitem = 0;
                }
            } else if(ev->key == InputKeyRight) {
                if(m->mtab < MENU_TAB_COUNT - 1) {
                    m->mtab++;
                    m->mitem = 0;
                }
            } else if(ev->key == InputKeyUp) {
                if(m->mitem > 0) m->mitem--;
            } else if(ev->key == InputKeyDown) {
                int mx = mitems(m->mtab) - 1;
                if(m->mitem < mx) m->mitem++;
            } else if(ev->key == InputKeyOk) {
                int tab = m->mtab, item = m->mitem;
                m->mode = ModeNormal;
                if(tab == 0) {
                    if(item == 0) { /* New */
                        m->buf[0][0] = '\0';
                        m->buf_count = 1;
                        m->total = 1;
                        m->buf_start = 0;
                        m->cursor = 0;
                        m->scroll = 0;
                        m->dirty = false;
                        m->is_new = true;
                        m->hscroll = 0;
                        strncpy(m->filename, "(new)", sizeof(m->filename) - 1);
                    } else if(item == 1) { /* Open */
                        view_dispatcher_send_custom_event(app->vd, EvOpenFile);
                    } else if(item == 2) { /* Save */
                        if(m->is_new)
                            post = PSavePath;
                        else
                            save_virtual(m, m->filename);
                    } else if(item == 3) { /* Save As */
                        post = PSavePath;
                    }
                } else if(tab == 1) {
                    if(item == 0) {
                        post = PFindQ;
                    } else if(item == 1) {
                        post = PFindRepQ;
                    } else if(item == 2) { /* Copy Line */
                        int rel = m->cursor - m->buf_start;
                        if(rel >= 0 && rel < m->buf_count) {
                            strncpy(app->clipboard, m->buf[rel], MAX_LINE - 1);
                            app->clipboard[MAX_LINE - 1] = '\0';
                            app->clip_ok = true;
                        }
                    } else if(item == 3) { /* Paste Line */
                        if(app->clip_ok) {
                            if(m->buf_count < BUFFER_LINES) {
                                int rel = m->cursor - m->buf_start;
                                if(rel >= 0 && rel < m->buf_count) {
                                    for(int i = m->buf_count; i > rel + 1; i--)
                                        memcpy(m->buf[i], m->buf[i - 1], MAX_LINE);
                                    strncpy(m->buf[rel + 1], app->clipboard, MAX_LINE - 1);
                                    m->buf_count++;
                                    m->total++;
                                    m->cursor++;
                                    fix_scroll(m);
                                    m->dirty = true;
                                }
                            }
                        }
                    } else if(item == 4) { /* Delete Row */
                        delete_line(m);
                    } else if(item == 5) { /* Clear All */
                        for(int i = 0; i < m->buf_count; i++)
                            m->buf[i][0] = '\0';
                        m->dirty = true;
                    } else if(item == 6) { /* Goto Row */
                        app->goto_cnt = m->total;
                        app->goto_cur = m->cursor + 1;
                        post = PGotoRow;
                    }
                } else if(tab == 2) {
                    if(item == 0) {
                        m->scale_ed = true;
                        m->mode = ModeMenu;
                    } else if(item == 1) {
                        m->row_nums = !m->row_nums;
                    } else if(item == 2) { /* First Row */
                        if(m->cursor != 0 || m->buf_start != 0) flush_and_reload(m, 0);
                        m->cursor = 0;
                        m->scroll = 0;
                        m->hscroll = 0;
                    } else if(item == 3) { /* Last Row */
                        int last = m->total - 1;
                        if(last < m->buf_start || last >= m->buf_start + m->buf_count)
                            flush_and_reload(m, last);
                        m->cursor = last;
                        fix_scroll(m);
                        m->hscroll = 0;
                    }
                }
            }
        }
    }

    if(need_reload) {
        flush_and_reload(m, m->cursor);
        fix_scroll(m);
    }

    view_commit_model(app->ev, true);

    if(stop) {
        view_dispatcher_stop(app->vd);
        return true;
    }

    if(post == PEditLine) {
        app->pending = PEditLine;
        show_text(app, "Edit Line", app->ebuf);
    } else if(post == PSavePath) {
        app->pending = PSavePath;
        show_text(app, "Save As", "/ext/");
    } else if(post == PFindQ) {
        app->pending = PFindQ;
        show_text(app, "Find", "");
    } else if(post == PFindRepQ) {
        app->pending = PFindRepQ;
        show_text(app, "Find", "");
    } else if(post == PGotoRow) {
        number_input_set_header_text(app->ni, "Goto Row");
        number_input_set_result_callback(app->ni, num_done, app, app->goto_cur, 1, app->goto_cnt);
        view_dispatcher_switch_to_view(app->vd, AppViewNum);
    }
    return true;
}

/* ================================================================
   ENTRY POINT
   ================================================================ */
int32_t flipnote_app(void* p) {
    UNUSED(p);
    App* app = malloc(sizeof(App));
    furi_assert(app);
    memset(app, 0, sizeof(App));

    app->vd = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->vd, app);
    view_dispatcher_set_navigation_event_callback(app->vd, nav_cb);
    view_dispatcher_set_custom_event_callback(app->vd, custom_ev);

    app->ev = view_alloc();
    view_set_context(app->ev, app);
    view_set_draw_callback(app->ev, draw_cb);
    view_set_input_callback(app->ev, input_cb);
    view_allocate_model(app->ev, ViewModelTypeLockFree, sizeof(Model));

    Model* m = (Model*)view_get_model(app->ev);
    memset(m, 0, sizeof(Model));
    m->scale = DEFAULT_SCALE;
    m->row_nums = true;
    m->buf[0][0] = '\0';
    m->buf_count = 1;
    m->total = 1;
    m->is_new = true;
    m->fc = -1;
    strncpy(m->filename, "(new)", sizeof(m->filename) - 1);
    view_commit_model(app->ev, false);

    view_dispatcher_add_view(app->vd, AppViewEditor, app->ev);

    app->ti = text_input_alloc();
    view_dispatcher_add_view(app->vd, AppViewText, text_input_get_view(app->ti));

    app->ni = number_input_alloc();
    view_dispatcher_add_view(app->vd, AppViewNum, number_input_get_view(app->ni));

    Gui* gui = furi_record_open(RECORD_GUI);
    view_dispatcher_attach_to_gui(app->vd, gui, ViewDispatcherTypeFullscreen);
    view_dispatcher_switch_to_view(app->vd, AppViewEditor);
    view_dispatcher_run(app->vd);

    view_dispatcher_remove_view(app->vd, AppViewEditor);
    view_dispatcher_remove_view(app->vd, AppViewText);
    view_dispatcher_remove_view(app->vd, AppViewNum);
    view_free(app->ev);
    text_input_free(app->ti);
    number_input_free(app->ni);
    view_dispatcher_free(app->vd);
    furi_record_close(RECORD_GUI);
    free(app);
    return 0;
}
