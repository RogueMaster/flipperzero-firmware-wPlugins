/*
 * Tone TX v1.1 - Flipper Zero (by Marcopiiit)
 *
 * How it works: the CC1101 runs in asynchronous 2-FSK with ~2.38 kHz
 * deviation (below the 2.5 kHz limit of analog PMR). By toggling the data
 * pin level at an audio frequency, an FM receiver demodulates a square
 * wave, i.e. a tone.
 *   .fmf -> each note becomes a tone at its frequency
 *   .txt -> read as music (FMF format with "Notes:", RTTTL, or a plain
 *           list of notes); if the text contains no notes it is sent
 *           as Morse code (fixed 700 Hz tone)
 *
 * Controls: Left/Right channel (PMR 1-16, LPD 1-69), Down switches band
 * PMR/LPD, Up picks a file, OK transmits,
 * Back during TX aborts it, Back on the transmit screen returns to the
 * start menu (Start / Disclaimer / About), Back in the menu exits.
 */

#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/elements.h>
#include <input/input.h>
#include <dialogs/dialogs.h>
#include <storage/storage.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <math.h>

#define MAX_SEGMENTS 3000
#define MAX_FILE_BYTES 8192
#define PMR_BASE_HZ 446006250UL
#define PMR_STEP_HZ 12500UL
#define PMR_CHANNELS 16
#define LPD_BASE_HZ 433075000UL // LPD433: 69 channels of 25 kHz, 433.075-434.775 MHz
#define LPD_STEP_HZ 25000UL
#define LPD_CHANNELS 69
#define TONE_MIN_HZ 300
#define TONE_MAX_HZ 3000
#define MORSE_TONE_HZ 700
#define MORSE_UNIT_MS 80
#define SILENCE_CHUNK_US 10000
#define LEAD_MS 400 // toneless carrier at the start, to open the squelch
#define TAIL_MS 200
#define NOTE_GAP_MS 20

typedef struct {
    uint16_t freq_hz; // 0 = carrier without tone
    uint16_t dur_ms;
} Segment;

typedef struct {
    Segment* seg;
    size_t count;
} SegList;

typedef enum {
    ScreenMenu,
    ScreenDisclaimer,
    ScreenAbout,
    ScreenMain,
} Screen;

typedef struct {
    Gui* gui;
    ViewPort* vp;
    FuriMessageQueue* queue;
    Storage* storage;
    DialogsApp* dialogs;
    SegList list;
    uint8_t channel;
    bool lpd; // false = PMR446, true = LPD433
    Screen screen;
    uint8_t menu_sel;
    uint8_t scroll;
    char last_path[200]; // last chosen file: the browser restarts from there
    uint32_t total_ms;
    char name[32];
    char status[32];
} App;

typedef struct {
    const Segment* seg;
    size_t count;
    size_t cur;
    uint32_t left_us;
    bool level;
    volatile bool done;
} TxState;

/* ---------- segment building ---------- */

static void seg_add(SegList* l, uint16_t freq, uint32_t dur_ms) {
    if(l->count >= MAX_SEGMENTS || dur_ms == 0) return;
    if(dur_ms > 65535) dur_ms = 65535;
    l->seg[l->count].freq_hz = freq;
    l->seg[l->count].dur_ms = (uint16_t)dur_ms;
    l->count++;
}

static void add_tone(SegList* l, uint16_t freq, uint32_t ms, bool with_gap) {
    if(with_gap && ms > 3 * NOTE_GAP_MS) {
        seg_add(l, freq, ms - NOTE_GAP_MS);
        seg_add(l, 0, NOTE_GAP_MS);
    } else {
        seg_add(l, freq, ms);
    }
}

static int semitone(char n) {
    switch(n) {
    case 'C': return 0;
    case 'D': return 2;
    case 'E': return 4;
    case 'F': return 5;
    case 'G': return 7;
    case 'A': return 9;
    case 'B': return 11;
    default: return -1;
    }
}

static void parse_notes(const char* p, uint32_t bpm, uint32_t def_dur, int def_oct, SegList* l) {
    if(bpm == 0) bpm = 120;
    if(def_dur == 0) def_dur = 4;
    const uint32_t whole_ms = 240000UL / bpm;

    while(*p) {
        while(*p == ' ' || *p == ',' || *p == '\t' || *p == '\n' || *p == '\r') p++;
        if(!*p) break;

        uint32_t dur = 0;
        bool dot = false, sharp = false;
        int oct = -1;

        const char* q = p;
        while(*q >= '0' && *q <= '9') {
            dur = dur * 10 + (uint32_t)(*q - '0');
            q++;
        }
        if(*q == '.') {
            dot = true;
            q++;
        }
        char n = (char)toupper((unsigned char)*q);
        if(*q) q++;
        if(*q == '#') {
            sharp = true;
            q++;
        }
        if(*q >= '0' && *q <= '9') {
            oct = *q - '0';
            q++;
        }
        if(*q == '.') {
            dot = true;
            q++;
        }
        while(*q && *q != ',' && *q != ' ' && *q != '\t' && *q != '\n' && *q != '\r') q++;
        p = q;

        if(dur == 0) dur = def_dur;
        if(oct < 0) oct = def_oct;
        uint32_t ms = whole_ms / dur;
        if(dot) ms = ms * 3 / 2;

        if(n == 'P') {
            seg_add(l, 0, ms);
            continue;
        }
        int st = semitone(n);
        if(st < 0) continue;

        int midi = 12 * (oct + 1) + st + (sharp ? 1 : 0);
        float f = 440.0f * powf(2.0f, (float)(midi - 69) / 12.0f);
        while(f < TONE_MIN_HZ) f *= 2.0f;
        while(f > TONE_MAX_HZ) f /= 2.0f;
        add_tone(l, (uint16_t)f, ms, true);
    }
}

static void parse_fmf(char* text, SegList* l) {
    uint32_t bpm = 120, def_dur = 4;
    int def_oct = 5;

    char* line = text;
    while(line && *line) {
        char* nl = strchr(line, '\n');
        if(nl) *nl = 0;
        size_t len = strlen(line);
        if(len && line[len - 1] == '\r') line[len - 1] = 0;

        if(strncasecmp(line, "BPM:", 4) == 0) {
            bpm = (uint32_t)atoi(line + 4);
        } else if(strncasecmp(line, "Duration:", 9) == 0) {
            def_dur = (uint32_t)atoi(line + 9);
        } else if(strncasecmp(line, "Octave:", 7) == 0) {
            def_oct = atoi(line + 7);
        } else if(strncasecmp(line, "Notes:", 6) == 0) {
            parse_notes(line + 6, bpm, def_dur, def_oct, l);
        }
        line = nl ? nl + 1 : NULL;
    }
}

// true if a line of the text starts with "Notes:" (FMF format, even inside a .txt)
static bool has_notes_line(const char* text) {
    const char* line = text;
    while(*line) {
        if(strncasecmp(line, "Notes:", 6) == 0) return true;
        const char* nl = strchr(line, '\n');
        if(!nl) break;
        line = nl + 1;
    }
    return false;
}

// Valid note token: [duration][.]NOTE[#][octave][.]  (NOTE = A-G or P for pause)
static bool is_note_token(const char* s, size_t n) {
    size_t i = 0;
    while(i < n && s[i] >= '0' && s[i] <= '9') i++;
    if(i < n && s[i] == '.') i++;
    if(i >= n) return false;
    char c = (char)toupper((unsigned char)s[i]);
    if(!((c >= 'A' && c <= 'G') || c == 'P')) return false;
    i++;
    if(i < n && s[i] == '#') i++;
    if(i < n && s[i] >= '0' && s[i] <= '9') i++;
    if(i < n && s[i] == '.') i++;
    return i == n;
}

// true if ALL the text is a list of notes (so normal text doesn't randomly become music)
static bool looks_like_note_list(const char* text) {
    size_t tokens = 0;
    const char* p = text;
    while(*p) {
        while(*p == ' ' || *p == ',' || *p == '\t' || *p == '\n' || *p == '\r') p++;
        if(!*p) break;
        const char* start = p;
        while(*p && *p != ' ' && *p != ',' && *p != '\t' && *p != '\n' && *p != '\r') p++;
        if(!is_note_token(start, (size_t)(p - start))) return false;
        tokens++;
    }
    return tokens > 0;
}

// RTTTL: name:d=4,o=5,b=120:note,note,...
static bool is_rtttl(const char* t) {
    const char* c1 = strchr(t, ':');
    if(!c1) return false;
    const char* c2 = strchr(c1 + 1, ':');
    if(!c2) return false;
    for(const char* p = c1 + 1; p < c2; p++) {
        if(*p == '=') return true;
    }
    return false;
}

static void parse_rtttl(const char* text, SegList* l) {
    const char* c1 = strchr(text, ':');
    const char* c2 = strchr(c1 + 1, ':');
    uint32_t dur = 4, bpm = 63;
    int oct = 6;
    for(const char* p = c1 + 1; p < c2; p++) {
        if(p[1] != '=') continue;
        char k = (char)tolower((unsigned char)*p);
        if(k == 'd') dur = (uint32_t)atoi(p + 2);
        else if(k == 'o') oct = atoi(p + 2);
        else if(k == 'b') bpm = (uint32_t)atoi(p + 2);
    }
    parse_notes(c2 + 1, bpm, dur, oct, l);
}

// Try to read the text as music. Returns false if it isn't (then Morse is used).
static bool parse_music_text(char* text, SegList* l) {
    size_t before = l->count;
    if(has_notes_line(text)) parse_fmf(text, l);
    else if(is_rtttl(text)) parse_rtttl(text, l);
    else if(looks_like_note_list(text)) parse_notes(text, 120, 4, 5, l);
    else return false;
    return l->count > before;
}

static const char* const MORSE_AZ[26] = {".-",   "-...", "-.-.", "-..",  ".",   "..-.", "--.",
                                         "....", "..",   ".---", "-.-",  ".-..", "--",
                                         "-.",   "---",  ".--.", "--.-", ".-.", "...",
                                         "-",    "..-",  "...-", ".--",  "-..-", "-.--",
                                         "--.."};
static const char* const MORSE_09[10] =
    {"-----", ".----", "..---", "...--", "....-", ".....", "-....", "--...", "---..", "----."};

static void build_morse(const char* text, SegList* l) {
    for(; *text; text++) {
        char c = (char)toupper((unsigned char)*text);
        const char* code = NULL;
        if(c >= 'A' && c <= 'Z') code = MORSE_AZ[c - 'A'];
        else if(c >= '0' && c <= '9') code = MORSE_09[c - '0'];
        else if(c == ' ' || c == '\n') {
            seg_add(l, 0, 4 * MORSE_UNIT_MS); // 3 end-of-letter + 4 = word space
            continue;
        } else continue;

        for(const char* e = code; *e; e++) {
            seg_add(l, MORSE_TONE_HZ, (*e == '.' ? 1 : 3) * MORSE_UNIT_MS);
            if(e[1]) seg_add(l, 0, MORSE_UNIT_MS);
        }
        seg_add(l, 0, 3 * MORSE_UNIT_MS);
    }
}

/* ---------- transmission ---------- */

// CC1101 preset: asynchronous 2-FSK, ~2.38 kHz deviation. Register/value pairs
// terminated by 0,0, then the PATABLE (8 bytes). 0xC0 = Flipper's standard
// power (~+10 dBm); to lower it, decrease the first byte (e.g. 0x60 = ~0 dBm).
static const uint8_t pmr_preset[] = {
    0x0B, 0x06, // FSCTRL1
    0x08, 0x32, // PKTCTRL0: async serial, no whitening
    0x07, 0x04, // PKTCTRL1
    0x14, 0x00, // MDMCFG0
    0x13, 0x02, // MDMCFG1
    0x12, 0x04, // MDMCFG2: 2-FSK, no preamble/sync
    0x11, 0x83, // MDMCFG3
    0x10, 0x67, // MDMCFG4
    0x15, 0x04, // DEVIATN: ~2.38 kHz
    0x18, 0x18, // MCSM0
    0x19, 0x16, // FOCCFG
    0x1D, 0x91, // AGCCTRL0
    0x1C, 0x00, // AGCCTRL1
    0x1B, 0x07, // AGCCTRL2
    0x20, 0xFB, // WORCTRL
    0x22, 0x10, // FREND0
    0x21, 0xB6, // FREND1
    0x00, 0x00, // end of registers
    0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // PATABLE
};

// Called by the async TX driver: returns level and duration (us) of the next stretch.
static LevelDuration tx_callback(void* ctx) {
    TxState* s = ctx;
    if(s->done) return level_duration_reset();

    if(s->left_us == 0) {
        s->cur++;
        if(s->cur >= s->count) {
            s->done = true;
            return level_duration_reset();
        }
        s->left_us = (uint32_t)s->seg[s->cur].dur_ms * 1000UL;
    }

    uint16_t f = s->seg[s->cur].freq_hz;
    uint32_t d;
    if(f == 0) {
        d = s->left_us < SILENCE_CHUNK_US ? s->left_us : SILENCE_CHUNK_US;
        s->level = false;
    } else {
        d = 500000UL / f; // half period
        s->level = !s->level;
    }
    if(d >= s->left_us) {
        d = s->left_us;
        s->left_us = 0;
    } else {
        s->left_us -= d;
    }
    if(d < 20) d = 20;
    return level_duration_make(s->level, d);
}

static uint8_t max_channel(const App* a) {
    return a->lpd ? LPD_CHANNELS : PMR_CHANNELS;
}

static uint32_t channel_freq(const App* a) {
    if(a->lpd) return LPD_BASE_HZ + (uint32_t)(a->channel - 1) * LPD_STEP_HZ;
    return PMR_BASE_HZ + (uint32_t)(a->channel - 1) * PMR_STEP_HZ;
}

static void set_status(App* a, const char* msg) {
    snprintf(a->status, sizeof(a->status), "%s", msg);
    view_port_update(a->vp);
}

static void transmit(App* a) {
    if(a->list.count <= 2) {
        set_status(a, "Choose a file first");
        return;
    }
    uint32_t freq = channel_freq(a);
    if(!furi_hal_subghz_is_tx_allowed(freq)) {
        set_status(a, "TX not allowed (region)");
        return;
    }

    TxState st = {
        .seg = a->list.seg,
        .count = a->list.count,
        .cur = 0,
        .left_us = (uint32_t)a->list.seg[0].dur_ms * 1000UL,
        .level = false,
        .done = false,
    };

    furi_hal_subghz_reset();
    furi_hal_subghz_idle();
    furi_hal_subghz_load_custom_preset(pmr_preset);
    furi_hal_subghz_set_frequency_and_path(freq);
    furi_hal_subghz_idle();

    if(!furi_hal_subghz_start_async_tx(tx_callback, &st)) {
        furi_hal_subghz_sleep();
        set_status(a, "TX start failed");
        return;
    }

    set_status(a, "Transmitting...");
    bool aborted = false;
    InputEvent ev;
    while(!furi_hal_subghz_is_async_tx_complete()) {
        if(furi_message_queue_get(a->queue, &ev, 20) == FuriStatusOk) {
            if(ev.key == InputKeyBack && ev.type == InputTypeShort) {
                aborted = true;
                break;
            }
        }
    }
    furi_hal_subghz_stop_async_tx();
    furi_hal_subghz_sleep();
    set_status(a, aborted ? "Aborted" : "Done");
}

/* ---------- files ---------- */

static void load_file(App* a, const char* path) {
    a->list.count = 0;
    a->total_ms = 0;

    File* f = storage_file_alloc(a->storage);
    char* buf = NULL;
    if(storage_file_open(f, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        uint64_t sz = storage_file_size(f);
        if(sz > MAX_FILE_BYTES) sz = MAX_FILE_BYTES;
        buf = malloc((size_t)sz + 1);
        size_t rd = storage_file_read(f, buf, (size_t)sz);
        buf[rd] = 0;
    }
    storage_file_close(f);
    storage_file_free(f);

    if(!buf) {
        set_status(a, "File read error");
        return;
    }

    seg_add(&a->list, 0, LEAD_MS);
    bool music = true;
    const char* ext = strrchr(path, '.');
    if(ext && strcasecmp(ext, ".fmf") == 0) {
        parse_fmf(buf, &a->list);
    } else if(!parse_music_text(buf, &a->list)) {
        build_morse(buf, &a->list);
        music = false;
    }
    seg_add(&a->list, 0, TAIL_MS);
    free(buf);

    for(size_t i = 0; i < a->list.count; i++) a->total_ms += a->list.seg[i].dur_ms;

    const char* slash = strrchr(path, '/');
    snprintf(a->name, sizeof(a->name), "%.*s", (int)sizeof(a->name) - 1, slash ? slash + 1 : path);

    if(a->list.count <= 2) set_status(a, "Empty or invalid file");
    else if(a->list.count >= MAX_SEGMENTS) set_status(a, "Ready (file truncated)");
    else set_status(a, music ? "Ready (music)" : "Ready (morse)");
}

// The last chosen file is saved on the SD card, so the browser reopens there even
// after closing and reopening the app.
#define DATA_DIR "/ext/apps_data/tone_tx"
#define LAST_PATH_FILE DATA_DIR "/last_path.txt"

static void save_last_path(App* a) {
    storage_simply_mkdir(a->storage, "/ext/apps_data");
    storage_simply_mkdir(a->storage, DATA_DIR);
    File* f = storage_file_alloc(a->storage);
    if(storage_file_open(f, LAST_PATH_FILE, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_write(f, a->last_path, strlen(a->last_path));
    }
    storage_file_close(f);
    storage_file_free(f);
}

static void load_last_path(App* a) {
    File* f = storage_file_alloc(a->storage);
    if(storage_file_open(f, LAST_PATH_FILE, FSAM_READ, FSOM_OPEN_EXISTING)) {
        size_t rd = storage_file_read(f, a->last_path, sizeof(a->last_path) - 1);
        a->last_path[rd] = 0;
    }
    storage_file_close(f);
    storage_file_free(f);
}

// Where to start the browser: last chosen file; if it no longer exists, its folder;
// if that doesn't exist either, /ext.
static void browse_start_path(App* a, FuriString* out) {
    furi_string_set_str(out, "/ext");
    if(!a->last_path[0]) return;

    if(storage_common_exists(a->storage, a->last_path)) {
        furi_string_set_str(out, a->last_path);
        return;
    }
    char dir[sizeof(a->last_path)];
    snprintf(dir, sizeof(dir), "%s", a->last_path);
    char* slash = strrchr(dir, '/');
    if(slash && slash != dir) {
        *slash = 0;
        if(storage_common_exists(a->storage, dir)) furi_string_set_str(out, dir);
    }
}

static void pick_file(App* a) {
    FuriString* path = furi_string_alloc();
    browse_start_path(a, path);
    DialogsFileBrowserOptions opts;
    dialog_file_browser_set_basic_options(&opts, "*", NULL);
    if(dialog_file_browser_show(a->dialogs, path, path, &opts)) {
        snprintf(a->last_path, sizeof(a->last_path), "%s", furi_string_get_cstr(path));
        save_last_path(a);
        load_file(a, a->last_path);
    }
    furi_string_free(path);
}

/* ---------- user interface ---------- */

#define MENU_ITEMS 3
#define VISIBLE_LINES 6

static const char* const MENU_LABELS[MENU_ITEMS] = {"Start", "Disclaimer", "About"};

// Text already split into lines of max 23 characters for the Flipper screen.
static const char* const DISCLAIMER_LINES[] = {
    "DISCLAIMER",
    "",
    "This application is",
    "provided strictly for",
    "testing, educational,",
    "and development",
    "purposes only.",
    "",
    "By using this",
    "application, you",
    "acknowledge and agree",
    "that you are solely",
    "responsible for how you",
    "use it and for any",
    "actions performed",
    "through or with the",
    "application.",
    "",
    "The developer assumes",
    "no responsibility or",
    "liability for any",
    "direct, indirect,",
    "incidental,",
    "consequential, or other",
    "damages, losses, or",
    "issues arising from the",
    "use, misuse, or",
    "inability to use this",
    "application.",
    "",
    "The developer does not",
    "encourage, support, or",
    "authorize any illegal,",
    "unauthorized, or",
    "harmful use of this",
    "application.",
    "",
    "Use this application at",
    "your own risk.",
};
#define DISCLAIMER_COUNT (sizeof(DISCLAIMER_LINES) / sizeof(DISCLAIMER_LINES[0]))
#define DISCLAIMER_MAX_SCROLL (DISCLAIMER_COUNT - VISIBLE_LINES)

static void draw_menu(Canvas* canvas, const App* a) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 9, AlignCenter, AlignBottom, "Tone TX v1.1");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 19, AlignCenter, AlignBottom, "by Marcopiiit");

    for(uint8_t i = 0; i < MENU_ITEMS; i++) {
        uint8_t top = 24 + i * 13;
        if(i == a->menu_sel) {
            canvas_draw_rbox(canvas, 16, top, 96, 12, 2);
            canvas_set_color(canvas, ColorWhite);
        }
        canvas_draw_str_aligned(canvas, 64, top + 6, AlignCenter, AlignCenter, MENU_LABELS[i]);
        canvas_set_color(canvas, ColorBlack);
    }
}

static void draw_disclaimer(Canvas* canvas, const App* a) {
    for(uint8_t i = 0; i < VISIBLE_LINES; i++) {
        size_t idx = (size_t)a->scroll + i;
        if(idx >= DISCLAIMER_COUNT) break;
        canvas_set_font(canvas, idx == 0 ? FontPrimary : FontSecondary);
        canvas_draw_str(canvas, 2, 9 + i * 10, DISCLAIMER_LINES[idx]);
    }
    elements_scrollbar(canvas, a->scroll, DISCLAIMER_MAX_SCROLL + 1);
}

static void draw_about(Canvas* canvas) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 24, AlignCenter, AlignBottom, "V1.1");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 40, AlignCenter, AlignBottom, "Created and tested by");
    canvas_draw_str_aligned(canvas, 64, 52, AlignCenter, AlignBottom, "Marcopiiit");
}

static void draw_main(Canvas* canvas, const App* a) {
    char line[48];
    uint32_t f = channel_freq(a);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, "Tone TX");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 20, "by Marcopiiit");

    if(a->lpd) {
        snprintf(line, sizeof(line), "Ch LPD %u  %lu.%03lu", a->channel, f / 1000000UL, (f % 1000000UL) / 1000UL);
    } else {
        snprintf(line, sizeof(line), "Ch PMR %u  %lu.%05lu", a->channel, f / 1000000UL, (f % 1000000UL) / 10UL);
    }
    canvas_draw_str(canvas, 2, 32, line);

    snprintf(line, sizeof(line), "File: %s", a->name[0] ? a->name : "(none)");
    canvas_draw_str(canvas, 2, 43, line);

    if(a->total_ms) snprintf(line, sizeof(line), "%s (%lu s)", a->status, (unsigned long)(a->total_ms / 1000));
    else snprintf(line, sizeof(line), "%s", a->status);
    canvas_draw_str(canvas, 2, 54, line);

    canvas_draw_str(canvas, 2, 63, "</>ch ^file vband OK tx");
}

static void draw_cb(Canvas* canvas, void* ctx) {
    App* a = ctx;
    canvas_clear(canvas);
    switch(a->screen) {
    case ScreenMenu: draw_menu(canvas, a); break;
    case ScreenDisclaimer: draw_disclaimer(canvas, a); break;
    case ScreenAbout: draw_about(canvas); break;
    case ScreenMain: draw_main(canvas, a); break;
    }
}

static void input_cb(InputEvent* ev, void* ctx) {
    App* a = ctx;
    furi_message_queue_put(a->queue, ev, 0);
}

// Key handling on the transmit screen (Back returns to the start menu).
static void handle_main_key(App* a, InputKey key) {
    switch(key) {
    case InputKeyBack:
        a->screen = ScreenMenu;
        break;
    case InputKeyLeft:
        a->channel = (a->channel > 1) ? a->channel - 1 : max_channel(a);
        break;
    case InputKeyRight:
        a->channel = (a->channel < max_channel(a)) ? a->channel + 1 : 1;
        break;
    case InputKeyDown: // toggle PMR / LPD
        a->lpd = !a->lpd;
        if(a->channel > max_channel(a)) a->channel = max_channel(a);
        break;
    case InputKeyUp:
        pick_file(a);
        break;
    case InputKeyOk:
        transmit(a);
        break;
    default:
        break;
    }
}

int32_t tone_tx_app(void* p) {
    UNUSED(p);

    App* a = malloc(sizeof(App));
    memset(a, 0, sizeof(App));
    a->channel = 1;
    a->screen = ScreenMenu;
    a->list.seg = malloc(sizeof(Segment) * MAX_SEGMENTS);
    snprintf(a->status, sizeof(a->status), "Choose a file");

    a->queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    a->storage = furi_record_open(RECORD_STORAGE);
    a->dialogs = furi_record_open(RECORD_DIALOGS);
    load_last_path(a);
    a->vp = view_port_alloc();
    view_port_draw_callback_set(a->vp, draw_cb, a);
    view_port_input_callback_set(a->vp, input_cb, a);
    a->gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(a->gui, a->vp, GuiLayerFullscreen);

    bool running = true;
    InputEvent ev;
    while(running) {
        if(furi_message_queue_get(a->queue, &ev, 100) != FuriStatusOk) continue;
        if(ev.type != InputTypeShort && ev.type != InputTypeRepeat) continue;

        switch(a->screen) {
        case ScreenMenu:
            if(ev.key == InputKeyUp) {
                a->menu_sel = (a->menu_sel + MENU_ITEMS - 1) % MENU_ITEMS;
            } else if(ev.key == InputKeyDown) {
                a->menu_sel = (a->menu_sel + 1) % MENU_ITEMS;
            } else if(ev.key == InputKeyOk) {
                if(a->menu_sel == 0) {
                    a->screen = ScreenMain;
                } else if(a->menu_sel == 1) {
                    a->scroll = 0;
                    a->screen = ScreenDisclaimer;
                } else {
                    a->screen = ScreenAbout;
                }
            } else if(ev.key == InputKeyBack) {
                running = false;
            }
            break;

        case ScreenDisclaimer:
            if(ev.key == InputKeyUp) {
                if(a->scroll > 0) a->scroll--;
            } else if(ev.key == InputKeyDown) {
                if(a->scroll < DISCLAIMER_MAX_SCROLL) a->scroll++;
            } else if(ev.key == InputKeyBack || ev.key == InputKeyLeft) {
                a->screen = ScreenMenu;
            }
            break;

        case ScreenAbout:
            if(ev.key == InputKeyBack || ev.key == InputKeyOk || ev.key == InputKeyLeft) {
                a->screen = ScreenMenu;
            }
            break;

        case ScreenMain:
            handle_main_key(a, ev.key);
            break;
        }
        view_port_update(a->vp);
    }

    gui_remove_view_port(a->gui, a->vp);
    view_port_free(a->vp);
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_DIALOGS);
    furi_record_close(RECORD_STORAGE);
    furi_message_queue_free(a->queue);
    free(a->list.seg);
    free(a);
    return 0;
}
