#include <furi.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/view_port.h>
#include <gui/view.h>
#include <gui/elements.h>
#include <gui/modules/text_input.h>
#include <storage/storage.h>
#include <loader/loader.h>
#include <toolbox/stream/file_stream.h>

#define APPS_DIR   "/ext/apps"
#define DATA_DIR   "/ext/apps_data/appsearch"
#define USAGE_PATH DATA_DIR "/usage.txt"
#define NAMES_PATH DATA_DIR "/names.cache" // path+size -> name, skips ELF parse
#define MAX_APPS   512 // headroom; real limiter is ARENA / heap, see below
#define ARENA      40000 // packed name+path strings; offsets are uint16 (<64KB)
#define NAME_LEN   36
#define QUERY_LEN  36
#define NO_MATCH   (-1000000)
#define ROW_H      10 // list row height
#define VIS        4 // visible list rows

typedef enum {
    ViewIdSearch, // text input
    ViewIdList, // results
} ViewId;

// Strings live in App.arena; an entry is just offsets + state (~12 bytes vs the
// ~140 bytes fixed buffers would cost), so 512 apps fit in ~6KB of entries.
typedef struct {
    uint16_t name_off; // display name (manifest name / prettified filename / built-in)
    uint16_t path_off; // fap: full path to launch; built-in: registered app name
    uint16_t uses; // launch count, persisted; drives frequency ranking
    int score; // transient, set per query
} Entry;

typedef struct {
    Entry entries[MAX_APPS];
    uint16_t count;
    uint16_t order[MAX_APPS]; // result indices into entries[], sorted
    uint16_t result_count;

    char arena[ARENA];
    uint16_t arena_used;

    char query[QUERY_LEN];
    int launch_index; // -1 = nothing chosen

    uint16_t sel; // selected result (index into order[])
    uint16_t top; // first visible result (scroll offset)

    ViewDispatcher* vd;
    Gui* gui;
    View* list_view; // custom list with a native button bar
    TextInput* input;
} App;

static App* g_app; // single instance; used by the list view's draw callback

static const char* e_name(const App* app, const Entry* e) {
    return app->arena + e->name_off;
}
static const char* e_path(const App* app, const Entry* e) {
    return app->arena + e->path_off;
}

// ---- fuzzy -----------------------------------------------------------------

static inline char lc(char c) {
    return (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c;
}

// Case-insensitive subsequence score. Returns NO_MATCH if query chars don't all
// appear in order. Bonuses: consecutive run, and matching at a word boundary.
static int fuzzy_score(const char* text, const char* q) {
    if(!q[0]) return 0; // empty query: everything matches, neutral score
    int score = 0, ti = 0, streak = 0;
    for(int qi = 0; q[qi]; qi++) {
        char qc = lc(q[qi]);
        bool found = false;
        for(; text[ti]; ti++) {
            if(lc(text[ti]) == qc) {
                char pc = (ti == 0) ? ' ' : text[ti - 1];
                score += 1 + (streak ? 5 : 0);
                if(pc == ' ' || pc == '-' || pc == '_' || pc == '/') score += 8;
                streak++;
                ti++;
                found = true;
                break;
            }
            streak = 0;
        }
        if(!found) return NO_MATCH;
    }
    score -= (int)strlen(text) / 8; // mild preference for tighter matches
    return score;
}

// order a before b? higher fuzzy score first, then most-used, then name.
// With an empty query all scores are 0, so this becomes a most-used list.
static bool entry_before(App* app, uint16_t ia, uint16_t ib) {
    const Entry* a = &app->entries[ia];
    const Entry* b = &app->entries[ib];
    if(a->score != b->score) return a->score > b->score;
    if(a->uses != b->uses) return a->uses > b->uses;
    return strcasecmp(e_name(app, a), e_name(app, b)) < 0;
}

// ---- index -----------------------------------------------------------------

// Built-in tools live in firmware, not under /ext/apps. They launch by their
// registered name (not a path). Names must match the loader's app records.
static const char* const BUILTINS[] = {
    "Sub-GHz",
    "NFC",
    "Infrared",
    "125 kHz RFID",
    "iButton",
    "GPIO",
    "Bad USB",
    "U2F",
};

// --- read a fap's real display name straight from its ELF .fapmeta section.
// Pure file reads only: the flipper_application_* loader API deadlocks when
// called from inside a running fap, so we parse the ELF by hand instead.
#define FAP_MAGIC 0x52474448u

static uint16_t rd16(const uint8_t* p) {
    return (uint16_t)(p[0] | (p[1] << 8));
}
static uint32_t rd32(const uint8_t* p) {
    return (uint32_t)(p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24));
}

static bool read_full(File* f, void* buf, size_t n) {
    uint8_t* p = buf;
    size_t got = 0;
    while(got < n) {
        size_t r = storage_file_read(f, p + got, n - got);
        if(r == 0) return false; // EOF / error before n
        got += r;
    }
    return true;
}

static bool read_at(File* f, uint32_t off, void* buf, size_t n) {
    return storage_file_seek(f, off, true) && read_full(f, buf, n);
}

#define SHT_MAX 16384 // section-header table cap (shnum*shentsize)

// Each fap costs a handful of reads instead of one seek+read per section header
// (that per-section loop was the ~7s bottleneck): grab the whole section-header
// table in one read, then walk it in RAM.
static bool read_fap_name(Storage* storage, const char* path, char* out, size_t out_sz) {
    bool ok = false;
    File* f = storage_file_alloc(storage);
    if(!storage_file_open(f, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_free(f);
        return false;
    }
    uint8_t* sht = NULL;
    char* strtab = NULL;
    do {
        uint8_t eh[52];
        if(!read_full(f, eh, sizeof(eh))) break;
        if(!(eh[0] == 0x7f && eh[1] == 'E' && eh[2] == 'L' && eh[3] == 'F')) break;
        uint32_t shoff = rd32(eh + 32); // ELF32 e_shoff
        uint16_t shentsize = rd16(eh + 46);
        uint16_t shnum = rd16(eh + 48);
        uint16_t shstrndx = rd16(eh + 50);
        if(!shoff || !shnum || shentsize < 40 || shentsize > 64 || shstrndx >= shnum) break;
        uint32_t sht_size = (uint32_t)shnum * shentsize;
        if(sht_size > SHT_MAX) break;

        sht = malloc(sht_size);
        if(!sht || !read_at(f, shoff, sht, sht_size)) break;

        // section-header string table, via the shstrndx section header
        const uint8_t* sstr = sht + (uint32_t)shstrndx * shentsize;
        uint32_t stroff = rd32(sstr + 16);
        uint32_t strsize = rd32(sstr + 20);
        if(strsize == 0 || strsize > 4096) break;
        strtab = malloc(strsize);
        if(!strtab || !read_at(f, stroff, strtab, strsize)) break;
        strtab[strsize - 1] = '\0'; // a corrupt .fap may omit the final nul; keep strcmp in bounds

        for(uint16_t i = 0; i < shnum; i++) {
            const uint8_t* sh = sht + (uint32_t)i * shentsize;
            uint32_t nameoff = rd32(sh + 0);
            if(nameoff >= strsize || strcmp(strtab + nameoff, ".fapmeta")) continue;
            uint32_t moff = rd32(sh + 16);
            uint32_t msize = rd32(sh + 20);
            if(msize < 21) break; // need magic + at least one name char
            // manifest layout: name[32] char field starts at offset 20
            uint8_t meta[54];
            uint32_t rdn = msize < sizeof(meta) ? msize : sizeof(meta);
            if(read_at(f, moff, meta, rdn) && rd32(meta + 0) == FAP_MAGIC) {
                char nm[33];
                size_t n = 0;
                for(; n < 32 && (20 + n) < rdn && meta[20 + n]; n++)
                    nm[n] = (char)meta[20 + n];
                nm[n] = '\0';
                if(n > 0) {
                    strlcpy(out, nm, out_sz);
                    ok = true;
                }
            }
            break;
        }
    } while(0);
    if(sht) free(sht);
    if(strtab) free(strtab);
    storage_file_close(f);
    storage_file_free(f);
    return ok;
}

// filename -> friendly label: '_'/'-' become spaces, each word capitalised
static void prettify(char* s) {
    bool start = true;
    for(char* p = s; *p; p++) {
        if(*p == '_' || *p == '-') {
            *p = ' ';
            start = true;
        } else if(start && *p >= 'a' && *p <= 'z') {
            *p = (char)(*p - 32);
            start = false;
        } else {
            start = false;
        }
    }
}

// append a nul-terminated string to the arena; false if it won't fit
static bool arena_put(App* app, const char* s, uint16_t* off) {
    size_t len = strlen(s) + 1;
    if((size_t)app->arena_used + len > ARENA) return false;
    *off = app->arena_used;
    memcpy(app->arena + app->arena_used, s, len);
    app->arena_used += (uint16_t)len;
    return true;
}

static void add_entry(App* app, const char* name, const char* target) {
    if(app->count >= MAX_APPS) return;
    // dedupe by display name: some faps ship in two folders (e.g. .../X.fap and
    // .../sub/X.fap), and we'd otherwise list them twice.
    for(uint16_t i = 0; i < app->count; i++)
        if(!strcmp(e_name(app, &app->entries[i]), name)) return;
    uint16_t no, po;
    // keep both strings or neither, so an entry never half-lands in the arena
    uint16_t mark = app->arena_used;
    if(!arena_put(app, name, &no) || !arena_put(app, target, &po)) {
        app->arena_used = mark;
        return;
    }
    Entry* e = &app->entries[app->count++];
    e->name_off = no;
    e->path_off = po;
    e->uses = 0;
    e->score = 0;
}

// ---- name cache ------------------------------------------------------------
// Transient scan state: the loaded cache (path+size -> name) plus the rebuilt
// cache text. Heap-allocated; freed after the scan.
typedef struct {
    const char* self; // our own fap path, skipped
    char* cbuf; // owns old-cache text, tokenised in place
    uint32_t csize[MAX_APPS];
    const char* cpath[MAX_APPS];
    const char* cname[MAX_APPS];
    int cn; // cached entries
    int hits; // cache hits this scan
    int misses; // cache misses (ELF parsed)
    File* wf; // new cache written incrementally (no big RAM buffer); may be NULL
} ScanCtx;

static void load_cache(ScanCtx* ctx, Storage* storage) {
    File* f = storage_file_alloc(storage);
    if(storage_file_open(f, NAMES_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        uint64_t sz = storage_file_size(f);
        if(sz > 0 && sz < 200000) {
            ctx->cbuf = malloc((size_t)sz + 1);
            if(ctx->cbuf && read_full(f, ctx->cbuf, (size_t)sz)) {
                ctx->cbuf[sz] = '\0';
                char* p = ctx->cbuf;
                while(*p && ctx->cn < MAX_APPS) {
                    char* line = p;
                    char* nl = strchr(p, '\n');
                    if(nl) {
                        *nl = '\0';
                        p = nl + 1;
                    } else {
                        p = line + strlen(line);
                    }
                    char* t1 = strchr(line, '\t');
                    if(!t1) continue;
                    *t1 = '\0';
                    char* t2 = strchr(t1 + 1, '\t');
                    if(!t2) continue;
                    *t2 = '\0';
                    uint32_t v = 0;
                    for(const char* c = line; *c >= '0' && *c <= '9'; c++)
                        v = v * 10 + (uint32_t)(*c - '0');
                    ctx->csize[ctx->cn] = v;
                    ctx->cpath[ctx->cn] = t1 + 1;
                    ctx->cname[ctx->cn] = t2 + 1;
                    ctx->cn++;
                }
            } else if(ctx->cbuf) {
                free(ctx->cbuf);
                ctx->cbuf = NULL;
            }
        }
    }
    storage_file_close(f);
    storage_file_free(f);
}

static const char* cache_lookup(const ScanCtx* ctx, const char* path, uint32_t size) {
    for(int i = 0; i < ctx->cn; i++)
        if(ctx->csize[i] == size && !strcmp(ctx->cpath[i], path)) return ctx->cname[i];
    return NULL;
}

#define DIR_LEN  160
#define MAX_DIRS 64

// Walk /ext/apps/** iteratively (NOT recursively: deep recursion + vsnprintf
// blew the fap stack -> MPU fault). A heap work-list of directory paths keeps
// stack depth constant.
static void scan_all(App* app, Storage* storage, ScanCtx* ctx) {
    char(*stack)[DIR_LEN] = malloc(sizeof(char[DIR_LEN]) * MAX_DIRS);
    if(!stack) return;
    int sp = 0;
    strlcpy(stack[sp++], APPS_DIR, DIR_LEN);

    File* dir = storage_file_alloc(storage);
    char name[96];
    char cur[DIR_LEN];
    while(sp > 0 && app->count < MAX_APPS) {
        strlcpy(cur, stack[--sp], sizeof(cur));
        if(!storage_dir_open(dir, cur)) {
            storage_dir_close(dir);
            continue;
        }
        FileInfo info;
        while(app->count < MAX_APPS && storage_dir_read(dir, &info, name, sizeof(name))) {
            if(name[0] == '\0') continue;
            char full[DIR_LEN + 100];
            snprintf(full, sizeof(full), "%s/%s", cur, name);
            if(info.flags & FSF_DIRECTORY) {
                if(sp < MAX_DIRS) strlcpy(stack[sp++], full, DIR_LEN);
                continue;
            }
            size_t fl = strlen(name);
            if(fl < 5 || strcasecmp(name + fl - 4, ".fap")) continue;
            if(ctx->self[0] && !strcmp(full, ctx->self)) continue;

            uint32_t size = (uint32_t)info.size;
            char disp[NAME_LEN];
            const char* cached = cache_lookup(ctx, full, size);
            if(cached) {
                strlcpy(disp, cached, sizeof(disp));
                ctx->hits++;
            } else {
                ctx->misses++;
                // real manifest name from .fapmeta; prettified filename on failure
                if(!read_fap_name(storage, full, disp, sizeof(disp))) {
                    strlcpy(disp, name, sizeof(disp));
                    char* dot = strrchr(disp, '.');
                    if(dot) *dot = '\0';
                    prettify(disp);
                }
            }
            add_entry(app, disp, full);
            if(ctx->wf) {
                char line[DIR_LEN + 140];
                int n =
                    snprintf(line, sizeof(line), "%lu\t%s\t%s\n", (unsigned long)size, full, disp);
                if(n > (int)sizeof(line) - 1)
                    n = (int)sizeof(line) - 1; // snprintf returns intended len
                if(n > 0) storage_file_write(ctx->wf, line, (size_t)n);
            }
        }
        storage_dir_close(dir);
    }
    storage_file_free(dir);
    free(stack);
}

static void build_index(App* app) {
    app->count = 0;
    for(size_t i = 0; i < COUNT_OF(BUILTINS); i++)
        add_entry(app, BUILTINS[i], BUILTINS[i]);

    Storage* storage = furi_record_open(RECORD_STORAGE);

    ScanCtx* ctx = malloc(sizeof(ScanCtx));
    if(!ctx) {
        furi_record_close(RECORD_STORAGE);
        return;
    }
    memset(ctx, 0, sizeof(ScanCtx));

    // skip our own fap so it doesn't show up / relaunch itself
    char self[128] = {0};
    Loader* loader = furi_record_open(RECORD_LOADER);
    FuriString* sp = furi_string_alloc();
    if(loader_get_application_launch_path(loader, sp))
        strlcpy(self, furi_string_get_cstr(sp), sizeof(self));
    furi_string_free(sp);
    furi_record_close(RECORD_LOADER);
    ctx->self = self;

    load_cache(ctx, storage); // reads old cache fully into RAM, then closes it

    // Rewrite the cache in place as we scan (old data is already in ctx->cbuf,
    // so truncating the file now is safe). Written incrementally to avoid a
    // second ~22KB RAM buffer, which was overflowing the heap.
    storage_common_mkdir(storage, DATA_DIR);
    File* wf = storage_file_alloc(storage);
    if(storage_file_open(wf, NAMES_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) ctx->wf = wf;

    scan_all(app, storage, ctx);

    if(ctx->wf) storage_file_close(wf);
    storage_file_free(wf);
    if(ctx->cbuf) free(ctx->cbuf);
    free(ctx);
    furi_record_close(RECORD_STORAGE);
}

// ---- usage (frequency ranking) ---------------------------------------------

// usage.txt lines: "<count>\t<target>". target may contain spaces (built-ins),
// so split on the tab only.
static void load_usage(App* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    Stream* s = file_stream_alloc(storage);
    FuriString* line = furi_string_alloc();
    if(file_stream_open(s, USAGE_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        while(stream_read_line(s, line)) {
            furi_string_trim(line); // drop trailing newline / CR
            const char* c = furi_string_get_cstr(line);
            uint32_t v = 0;
            while(*c >= '0' && *c <= '9')
                v = v * 10 + (uint32_t)(*c++ - '0');
            if(*c != '\t') continue;
            const char* target = c + 1;
            for(uint16_t i = 0; i < app->count; i++) {
                if(!strcmp(e_path(app, &app->entries[i]), target)) {
                    app->entries[i].uses = (v > 0xFFFF) ? 0xFFFF : (uint16_t)v;
                    break;
                }
            }
        }
    }
    furi_string_free(line);
    file_stream_close(s);
    stream_free(s);
    furi_record_close(RECORD_STORAGE);
}

static void save_usage(App* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    storage_common_mkdir(storage, DATA_DIR); // no-op if it already exists
    FuriString* out = furi_string_alloc();
    for(uint16_t i = 0; i < app->count; i++) {
        if(app->entries[i].uses)
            furi_string_cat_printf(
                out, "%u\t%s\n", app->entries[i].uses, e_path(app, &app->entries[i]));
    }
    File* f = storage_file_alloc(storage);
    if(storage_file_open(f, USAGE_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS))
        storage_file_write(f, furi_string_get_cstr(out), furi_string_size(out));
    storage_file_close(f);
    storage_file_free(f);
    furi_string_free(out);
    furi_record_close(RECORD_STORAGE);
}

// ---- results ---------------------------------------------------------------

static void rebuild_results(App* app) {
    for(uint16_t i = 0; i < app->count; i++)
        app->entries[i].score = fuzzy_score(e_name(app, &app->entries[i]), app->query);

    app->result_count = 0;
    for(uint16_t i = 0; i < app->count; i++)
        if(app->entries[i].score > NO_MATCH) app->order[app->result_count++] = i;

    // insertion sort (result_count <= MAX_APPS; n^2 is trivial here)
    for(uint16_t r = 1; r < app->result_count; r++) {
        uint16_t key = app->order[r];
        int j = r - 1;
        while(j >= 0 && entry_before(app, key, app->order[j])) {
            app->order[j + 1] = app->order[j];
            j--;
        }
        app->order[j + 1] = key;
    }
    app->sel = 0;
    app->top = 0;
}

// ---- list view -------------------------------------------------------------

static void list_scroll_fix(App* app) {
    if(app->result_count <= VIS) {
        app->top = 0;
        return;
    }
    if(app->sel < app->top)
        app->top = app->sel;
    else if(app->sel >= app->top + VIS)
        app->top = app->sel - VIS + 1;
    if(app->top > app->result_count - VIS) app->top = app->result_count - VIS;
}

static void list_redraw(App* app) {
    // commit the (dummy) model with update=true to trigger a redraw
    with_view_model(app->list_view, void** m, { UNUSED(m); }, true);
}

static void list_draw(Canvas* canvas, void* model) {
    UNUSED(model);
    App* app = g_app;
    canvas_clear(canvas);
    canvas_set_font(canvas, FontSecondary);

    // title line: the hint on landing, the query while filtering
    const char* title = app->query[0] ? app->query : "Most used first";
    canvas_draw_str(canvas, 2, 8, title);
    canvas_draw_line(canvas, 0, 10, 127, 10);

    if(app->result_count == 0) {
        canvas_draw_str_aligned(
            canvas, 64, 30, AlignCenter, AlignCenter, app->query[0] ? "No matches" : "No apps");
    } else {
        for(uint16_t r = 0; r < VIS; r++) {
            uint16_t ri = app->top + r;
            if(ri >= app->result_count) break;
            const Entry* e = &app->entries[app->order[ri]];
            int y = 12 + r * ROW_H;
            bool seld = (ri == app->sel);
            if(seld) canvas_draw_box(canvas, 0, y, 120, ROW_H);
            canvas_set_color(canvas, seld ? ColorWhite : ColorBlack);
            canvas_draw_str(canvas, 2, y + 8, e_name(app, e));
            canvas_set_color(canvas, ColorBlack);
        }
        elements_scrollbar(canvas, app->sel, app->result_count);
    }

    // native button hints: Left opens search, OK launches the selection
    elements_button_left(canvas, "Search");
    if(app->result_count) elements_button_right(canvas, "Open");
}

// cppcheck-suppress constParameterCallback ; ViewInputCallback signature is non-const
static bool list_input(InputEvent* event, void* context) {
    App* app = context;
    if(event->type != InputTypeShort && event->type != InputTypeRepeat) return false;

    switch(event->key) {
    case InputKeyUp:
        if(app->result_count) {
            app->sel = (app->sel == 0) ? app->result_count - 1 : app->sel - 1;
            list_scroll_fix(app);
            list_redraw(app);
        }
        return true;
    case InputKeyDown:
        if(app->result_count) {
            app->sel = (app->sel + 1) % app->result_count;
            list_scroll_fix(app);
            list_redraw(app);
        }
        return true;
    case InputKeyLeft:
        text_input_set_header_text(app->input, "Search apps");
        view_dispatcher_switch_to_view(app->vd, ViewIdSearch);
        return true;
    case InputKeyOk:
        if(app->result_count) {
            app->launch_index = app->order[app->sel];
            view_dispatcher_stop(app->vd);
        }
        return true;
    default:
        return false; // Back falls through to the navigation callback (exit)
    }
}

// ---- callbacks -------------------------------------------------------------

static void search_done_cb(void* ctx) {
    App* app = ctx;
    rebuild_results(app);
    view_dispatcher_switch_to_view(app->vd, ViewIdList);
}

static uint32_t to_list(void* ctx) {
    UNUSED(ctx);
    return ViewIdList; // Back from the keyboard returns to the list
}

static uint32_t exit_app(void* ctx) {
    UNUSED(ctx);
    return VIEW_NONE; // Back from the list exits the app
}

static void loading_draw(Canvas* canvas, void* ctx) {
    UNUSED(ctx);
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 28, AlignCenter, AlignCenter, "App Search");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 44, AlignCenter, AlignCenter, "Scanning apps...");
}

// ---- entry -----------------------------------------------------------------

int32_t app_search_app(void* p) {
    UNUSED(p);
    App* app = malloc(sizeof(App));
    if(!app) return -1;
    memset(app, 0, sizeof(App));
    app->launch_index = -1;
    g_app = app;

    app->gui = furi_record_open(RECORD_GUI);

    // show a scan screen on the compositor thread while we index (this thread
    // blocks on the scan; GUI compositing runs separately so the screen draws)
    ViewPort* loading = view_port_alloc();
    view_port_draw_callback_set(loading, loading_draw, NULL);
    gui_add_view_port(app->gui, loading, GuiLayerFullscreen);
    build_index(app);
    load_usage(app); // apply persisted launch counts for frequency ranking
    gui_remove_view_port(app->gui, loading);
    view_port_free(loading);

    app->vd = view_dispatcher_alloc();
    view_dispatcher_attach_to_gui(app->vd, app->gui, ViewDispatcherTypeFullscreen);

    app->list_view = view_alloc();
    view_set_context(app->list_view, app);
    view_allocate_model(app->list_view, ViewModelTypeLockFree, sizeof(void*));
    view_set_draw_callback(app->list_view, list_draw);
    view_set_input_callback(app->list_view, list_input);
    view_set_previous_callback(app->list_view, exit_app);
    view_dispatcher_add_view(app->vd, ViewIdList, app->list_view);

    app->input = text_input_alloc();
    text_input_set_header_text(app->input, "Search apps");
    text_input_set_result_callback(
        app->input, search_done_cb, app, app->query, sizeof(app->query), false);
    view_set_previous_callback(text_input_get_view(app->input), to_list);
    view_dispatcher_add_view(app->vd, ViewIdSearch, text_input_get_view(app->input));

    // land on the full list (most-used first); keyboard is opened with Left
    rebuild_results(app);
    view_dispatcher_switch_to_view(app->vd, ViewIdList);
    view_dispatcher_run(app->vd);

    view_dispatcher_remove_view(app->vd, ViewIdSearch);
    view_dispatcher_remove_view(app->vd, ViewIdList);
    view_free(app->list_view);
    text_input_free(app->input);
    view_dispatcher_free(app->vd);
    furi_record_close(RECORD_GUI);

    if(app->launch_index >= 0) {
        Entry* e = &app->entries[app->launch_index];
        if(e->uses < 0xFFFF) e->uses++; // bump frequency and persist
        save_usage(app);
        Loader* loader = furi_record_open(RECORD_LOADER);
        // fap: path as the name arg launches the file; built-in: registered name
        loader_enqueue_launch(loader, e_path(app, e), NULL, LoaderDeferredLaunchFlagNone);
        // re-enqueue ourselves so App Search reopens when that app exits
        FuriString* me = furi_string_alloc();
        if(loader_get_application_launch_path(loader, me))
            loader_enqueue_launch(
                loader, furi_string_get_cstr(me), NULL, LoaderDeferredLaunchFlagNone);
        furi_string_free(me);
        furi_record_close(RECORD_LOADER);
    }

    free(app);
    return 0;
}
