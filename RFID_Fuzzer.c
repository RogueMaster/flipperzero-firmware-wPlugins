#include <furi.h>
#include <furi/core/thread.h>
#include <gui/gui.h>
#include <input/input.h>
#include <storage/storage.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static void draw(Canvas* c, void* ctx);
static void input(InputEvent* e, void* ctx);
static int32_t generate_worker(void* ctx);

/* ===================== TYPES ===================== */

typedef enum { ProtocolRFID, ProtocolNFC, ProtocolIButton } ProtocolType;

typedef enum {
    GenModeRandom = 0,
    GenModeSequential,
    GenModeFuzz,
    GenModeStructured,
    GenModeCount,
} GenerationMode;

typedef enum {
    FocusProtocol,
    FocusMode,
    FocusCount,
    FocusPrefixes,
    FocusGenerate,
    FocusMax,
} UiFocus;

typedef enum {
    SubmenuNone,
    SubmenuSequential,
    SubmenuFuzz,
    SubmenuStructured,
} SubmenuType;

typedef enum {
    SeqStart,
    SeqStep,
    SeqCounter,
    SeqDirection,
} SeqFocus;

typedef enum {
    FuzzBoundary,
    FuzzBitflip,
    FuzzBits,
    FuzzPreserve,
    FuzzCollision,
} FuzzFocus;

typedef enum {
    StructSeq = 0,
    StructRand,
    StructTypeCount,
} StructType;

typedef struct {
    const char* name;
    uint8_t bytes;
    uint64_t max;
    ProtocolType type;
    const uint8_t* prefixes;
    uint8_t prefix_count;
} Protocol;

/* ===================== PREFIX TABLES ===================== */

static const uint8_t P_EM4100[]  = {0x00,0x01,0x02,0x03};
static const uint8_t P_HID[]     = {0xA0,0xB0,0xC0};
static const uint8_t P_INDALA[]  = {0x20,0x21,0x22};
static const uint8_t P_IOPROX[]  = {0x20,0x21,0x22};
static const uint8_t P_PAC[]     = {0xE0};
static const uint8_t P_PARADOX[] = {0xA0,0xC0};
static const uint8_t P_VIKING[]  = {0x40,0x50};
static const uint8_t P_PYRAMID[] = {0x80};
static const uint8_t P_KERI[]    = {0x60};
static const uint8_t P_H10301[]  = {0xF0,0xF1};
static const uint8_t P_JABLO[]   = {0x02};
static const uint8_t P_ELECTRA[] = {0x01};
static const uint8_t P_IDTECK[]  = {0x0A};
static const uint8_t P_GALL[]    = {0x20,0x30};

static const uint8_t P_MIFARE[]  = {0x04,0x08,0x12};
static const uint8_t P_UL[]      = {0x04};
static const uint8_t P_ICLASS[]  = {0xEC};

static const uint8_t P_DALLAS[]  = {0x01,0x02};

/* ===================== PROTOCOL LIST ===================== */

static const Protocol protocols[] = {
    {"EM4100",5,0xFFFFFFFFFFULL,ProtocolRFID,P_EM4100,4},
    {"HID Prox",6,0xFFFFFFFFFFFFULL,ProtocolRFID,P_HID,3},
    {"Indala",4,0xFFFFFFFFULL,ProtocolRFID,P_INDALA,3},
    {"IoProx",4,0xFFFFFFFFULL,ProtocolRFID,P_IOPROX,3},
    {"PAC/Stanley",4,0xFFFFFFFFULL,ProtocolRFID,P_PAC,1},
    {"Paradox",6,0xFFFFFFFFFFFFULL,ProtocolRFID,P_PARADOX,2},
    {"Viking",4,0xFFFFFFFFULL,ProtocolRFID,P_VIKING,2},
    {"Pyramid",4,0xFFFFFFFFULL,ProtocolRFID,P_PYRAMID,1},
    {"Keri",4,0xFFFFFFFFULL,ProtocolRFID,P_KERI,1},
    {"Nexwatch",8,0xFFFFFFFFFFFFFFFFULL,ProtocolRFID,NULL,0},
    {"H10301",3,0xFFFFFFULL,ProtocolRFID,P_H10301,2},
    {"Jablotron",5,0xFFFFFFFFFFULL,ProtocolRFID,P_JABLO,1},
    {"Electra",8,0xFFFFFFFFFFFFFFFFULL,ProtocolRFID,P_ELECTRA,1},
    {"IDTeck",8,0xFFFFFFFFFFFFFFFFULL,ProtocolRFID,P_IDTECK,1},
    {"Gallagher",8,0xFFFFFFFFFFFFFFFFULL,ProtocolRFID,P_GALL,2},

    {"MIFARE Classic 1K",4,0xFFFFFFFFULL,ProtocolNFC,P_MIFARE,3},
    {"MIFARE Classic 4K",4,0xFFFFFFFFULL,ProtocolNFC,P_MIFARE,3},
    {"MIFARE Ultralight",7,0xFFFFFFFFFFFFFFULL,ProtocolNFC,P_UL,1},
    {"DESFire EV1",16,0,ProtocolNFC,NULL,0},
    {"iCLASS",8,0xFFFFFFFFFFFFFFFFULL,ProtocolNFC,P_ICLASS,1},
    {"FeliCa",8,0xFFFFFFFFFFFFFFFFULL,ProtocolNFC,NULL,0},

    {"Dallas DS1990",8,0xFFFFFFFFFFFFFFFFULL,ProtocolIButton,P_DALLAS,2},
    {"Cyfral",2,0xFFFFULL,ProtocolIButton,NULL,0},
    {"Metacom",4,0xFFFFFFFFULL,ProtocolIButton,NULL,0},
    {"Maxim iButton",8,0xFFFFFFFFFFFFFFFFULL,ProtocolIButton,NULL,0},
    {"Keypad/Access Control",8,0xFFFFFFFFFFFFFFFFULL,ProtocolIButton,NULL,0},
    {"Temperature",8,0xFFFFFFFFFFFFFFFFULL,ProtocolIButton,NULL,0},
    {"Custom iButton",8,0xFFFFFFFFFFFFFFFFULL,ProtocolIButton,NULL,0},
};

#define PROTOCOL_COUNT (sizeof(protocols)/sizeof(Protocol))
#define MAX_PREFIXES 4
#define COUNT_STEP 1000
#define MAX_IDS 300000

static const uint8_t STRUCT_TAILS[] = {
    0x00, 0x01, 0x02, 0x10, 0x20, 0xFF, 0xAA, 0x55
};
#define STRUCT_TAIL_COUNT (sizeof(STRUCT_TAILS))

/* ===================== APP STATE ===================== */

typedef struct {
    uint32_t index;
    uint64_t id;
} RecentEntry;

typedef struct {
    size_t proto;
    uint32_t count;
    bool run;
    GenerationMode mode;
    StructType structured_type;
    UiFocus focus;
    SubmenuType submenu;
    bool selecting_prefix;
    uint8_t prefix_cursor;
    bool prefix_enabled[PROTOCOL_COUNT][MAX_PREFIXES];

    uint64_t seq_start;
    uint64_t seq_step;
    bool per_prefix;
    bool seq_reverse;
    SeqFocus seq_focus;

    bool fuzz_boundary;
    bool fuzz_bitflip;
    uint8_t fuzz_bits;
    bool fuzz_preserve;
    FuzzFocus fuzz_focus;

    bool collision_enabled;
    uint8_t collision_rate;
    uint64_t collision_pool[16];
    uint8_t collision_pool_size;

    bool generating;
    uint8_t progress;
    uint32_t generated_count;
    FuriThread* worker;
    uint8_t hold_ticks;
    RecentEntry recent[3];
    uint8_t recent_count;
    UiFocus marquee_owner;
    const char* marquee_last_value;
    uint16_t marquee_tick;
    uint16_t marquee_char_offset;
} AppState;

static const char* base_path(ProtocolType t) {
    if(t == ProtocolRFID) return "/ext/lfrfid_fuzzer/ListEM";
    if(t == ProtocolNFC) return "/ext/mifare_fuzzer/ListEM";
    return "/ext/ibutton_fuzzer/ListEM";
}

static void ensure_dirs(Storage* st, ProtocolType t) {
    storage_common_mkdir(st, "/ext/lfrfid_fuzzer");
    storage_common_mkdir(st, "/ext/mifare_fuzzer");
    storage_common_mkdir(st, "/ext/ibutton_fuzzer");
    storage_common_mkdir(st, base_path(t));
}

/* ===================== GENERATION ===================== */

static int32_t generate_worker(void* ctx) {
    AppState* s = ctx;
    const Protocol* p = &protocols[s->proto];

    s->collision_pool_size = 0;
    s->recent_count = 0;

    Storage* st = furi_record_open(RECORD_STORAGE);
    ensure_dirs(st, p->type);

    char clean[64];
    strncpy(clean, p->name, sizeof(clean) - 1);
    clean[sizeof(clean) - 1] = '\0';
    for(size_t i = 0; clean[i]; i++)
        if(clean[i] == ' ') clean[i] = '_';

    const char* mode_str =
        (s->mode == GenModeSequential) ? "sequential" :
        (s->mode == GenModeFuzz) ? "fuzz" :
        (s->mode == GenModeStructured) ? "structured" :
        "random";

    char prefix_part[64] = "noprefix";
    bool has_prefix = false;
    if(p->prefix_count > 0) {
        prefix_part[0] = '\0';
        for(uint8_t i = 0; i < p->prefix_count; i++) {
            if(s->prefix_enabled[s->proto][i]) {
                if(has_prefix) strcat(prefix_part, "-");
                char tmp[8];
                snprintf(tmp, sizeof(tmp), "%02X", p->prefixes[i]);
                strcat(prefix_part, tmp);
                has_prefix = true;
            }
        }
        if(!has_prefix) strcpy(prefix_part, "noprefix");
    }

    char path[192];
    snprintf(path, sizeof(path), "%s/%s_%s_%s.txt",
        base_path(p->type), clean, mode_str, prefix_part);

    File* f = storage_file_alloc(st);
    storage_file_open(f, path, FSAM_WRITE, FSOM_CREATE_ALWAYS);
    if(!storage_file_is_open(f)) {
        storage_file_free(f);
        furi_record_close(RECORD_STORAGE);
        s->generating = false;
        return 0;
    }

    uint64_t max = p->max ? p->max : ((1ULL << (p->bytes * 8)) - 1);
    uint32_t step = s->count / 100;
    if(step == 0) step = 1;

    for(uint32_t i = 0; i < s->count; i++) {
        uint64_t id = 0;
        uint8_t prefix_index = 0;

        if(s->per_prefix && p->prefix_count > 0) {
            uint8_t enabled = 0;
            for(uint8_t j = 0; j < p->prefix_count; j++)
                if(s->prefix_enabled[s->proto][j]) enabled++;
            if(enabled > 0) {
                uint32_t block = s->count / enabled;
                if(block == 0) block = 1;
                prefix_index = (i / block) % enabled;
            }
        }

        if(s->mode == GenModeSequential) {
            uint64_t offset = (uint64_t)i * s->seq_step;
            uint64_t start = s->seq_reverse
                ? (p->max ? p->max : ((1ULL << (p->bytes * 8)) - 1))
                : s->seq_start;
            uint64_t seq;
            if(!s->seq_reverse) {
                seq = start + offset;
            } else {
                seq = (start > offset) ? (start - offset) : 0;
            }
            id = seq;
        }
        else if(s->mode == GenModeStructured && p->bytes > 1) {
            uint32_t tail_index = i % STRUCT_TAIL_COUNT;
            uint32_t seq_index;
            if(s->structured_type == StructSeq) {
                seq_index = i / STRUCT_TAIL_COUNT;
            } else {
                uint32_t x = i / STRUCT_TAIL_COUNT;
                x ^= x << 13;
                x ^= x >> 17;
                x ^= x << 5;
                seq_index = x;
            }
            uint64_t offset = (uint64_t)seq_index * s->seq_step;
            uint64_t start = s->seq_reverse
                ? (p->max ? p->max : ((1ULL << (p->bytes * 8)) - 1))
                : s->seq_start;
            uint64_t seq;
            if(!s->seq_reverse) {
                seq = start + offset;
            } else {
                seq = (start > offset) ? (start - offset) : 0;
            }

            uint8_t rem_bytes = p->bytes - 1;
            uint8_t seq_bytes = (rem_bytes > 1) ? (rem_bytes - 1) : rem_bytes;
            uint8_t total_bits = (seq_bytes * 8);
            uint8_t card_bits = total_bits / 2;
            uint8_t facility_bits = total_bits - card_bits;
            uint64_t card_mask = (1ULL << card_bits) - 1;
            uint64_t facility_mask = (1ULL << facility_bits) - 1;
            uint64_t card = seq & card_mask;
            uint64_t facility = (seq >> card_bits) & facility_mask;
            uint64_t base = (facility << card_bits) | card;
            uint8_t tail = STRUCT_TAILS[tail_index];

            id = (rem_bytes > 1) ? ((base << 8) | tail) : base;
        }
        else {
            uint64_t r = ((uint64_t)rand() << 32) | rand();
            id = r & max;
        }

        if(s->mode == GenModeFuzz) {
            if(s->fuzz_boundary) {
                static const uint64_t patterns[] = {
                    0x0ULL, 0xFFFFFFFFFFFFFFFFULL,
                    0xAAAAAAAAAAAAAAAAULL, 0x5555555555555555ULL
                };
                id = patterns[i % 4] & max;
            }
            if(s->fuzz_bitflip) {
                for(uint8_t b = 0; b < s->fuzz_bits; b++) {
                    uint8_t bit = rand() % (p->bytes * 8);
                    id ^= (1ULL << bit);
                }
            }
        }

        if(p->prefix_count > 0) {
            uint8_t usable[MAX_PREFIXES];
            uint8_t n = 0;
            for(uint8_t j = 0; j < p->prefix_count; j++)
                if(s->prefix_enabled[s->proto][j]) usable[n++] = p->prefixes[j];

            if(n > 0) {
                uint8_t prefix;
                if(s->mode == GenModeStructured) {
                    uint32_t block_size = STRUCT_TAIL_COUNT * 32;
                    uint32_t comb_prefix_index = (i / block_size) % n;
                    prefix = usable[comb_prefix_index];
                } else {
                    prefix = usable[s->per_prefix ? (prefix_index % n) : (rand() % n)];
                }
                uint8_t rem = p->bytes - 1;
                if(s->mode != GenModeFuzz || s->fuzz_preserve) {
                    id = ((uint64_t)prefix << (rem * 8)) | (id & ((1ULL << (rem * 8)) - 1));
                }
            }
        }

        if(s->mode == GenModeFuzz && s->collision_enabled &&
           s->collision_pool_size > 0 && (i % s->collision_rate == 0)) {
            id = s->collision_pool[rand() % s->collision_pool_size];
        }
        if(s->mode == GenModeFuzz && s->collision_enabled &&
           s->collision_pool_size < 16) {
            s->collision_pool[s->collision_pool_size++] = id;
        }

        char line[40];
        snprintf(line, sizeof(line), "%0*llX\n", p->bytes * 2, id);
        storage_file_write(f, line, strlen(line));

        s->recent[2] = s->recent[1];
        s->recent[1] = s->recent[0];
        s->recent[0].index = i;
        s->recent[0].id = id;
        if(s->recent_count < 3) s->recent_count++;
        /* ------------------------------------------------------------ */

        if(i % step == 0) {
            s->progress = (i * 100) / s->count;
            s->generated_count = i;
        }
    }

    storage_file_close(f);
    storage_file_free(f);
    furi_record_close(RECORD_STORAGE);
    s->generating = false;
    s->progress = 100;
    s->generated_count = s->count;
    return 0;
}

/* ===================== DRAW HELPERS ===================== */


static void draw_row(Canvas* c, uint8_t y, bool selected, const char* label, const char* value) {
    canvas_set_font(c, selected ? FontPrimary : FontSecondary);
    char lbuf[32];
    snprintf(lbuf, sizeof(lbuf), "%s%s", selected ? ">" : " ", label);
    canvas_draw_str(c, 2, y, lbuf);

    uint8_t label_w = canvas_string_width(c, lbuf);
    int16_t avail = 124 - (2 + label_w + 4);
    if(avail < 10) avail = 10;

    char vbuf[40];
    strncpy(vbuf, value, sizeof(vbuf) - 1);
    vbuf[sizeof(vbuf) - 1] = '\0';

    if(canvas_string_width(c, vbuf) > avail) {
        size_t len = strlen(vbuf);
        while(len > 1 && canvas_string_width(c, vbuf) > avail) {
            len--;
            vbuf[len] = '\0';
        }
        if(len > 2) {
            vbuf[len - 2] = '.';
            vbuf[len - 1] = '.';
            vbuf[len] = '\0';
        }
    }

    canvas_draw_str_aligned(c, 126, y, AlignRight, AlignBottom, vbuf);
    canvas_set_font(c, FontSecondary);
}


static void draw_row_marquee(Canvas* c, uint8_t y, bool selected, const char* label,
                              const char* value, UiFocus which, AppState* s) {
    if(!selected) {
        draw_row(c, y, false, label, value);
        return;
    }

    if(s->marquee_owner != which || s->marquee_last_value != value) {
        s->marquee_owner = which;
        s->marquee_last_value = value;
        s->marquee_tick = 0;
        s->marquee_char_offset = 0;
    }

    canvas_set_font(c, FontPrimary);
    char lbuf[32];
    snprintf(lbuf, sizeof(lbuf), ">%s", label);
    canvas_draw_str(c, 2, y, lbuf);

    uint8_t label_w = canvas_string_width(c, lbuf);
    int16_t avail = 124 - (2 + label_w + 15);
    if(avail < 10) avail = 10;

    uint16_t full_w = canvas_string_width(c, value);

    if(full_w <= avail) {
        s->marquee_tick = 0;
        s->marquee_char_offset = 0;
        canvas_draw_str_aligned(c, 126, y, AlignRight, AlignBottom, value);
        canvas_set_font(c, FontSecondary);
        return;
    }

    char loop[48];
    snprintf(loop, sizeof(loop), "%s   ", value); 
    size_t loop_len = strlen(loop);

    const uint16_t pause_frames = 20;  
    const uint16_t frames_per_char = 4; 

    s->marquee_tick++;
    size_t char_offset = 0;
    if(s->marquee_tick > pause_frames) {
        size_t adv = (s->marquee_tick - pause_frames) / frames_per_char;
        if(adv >= loop_len) {
            s->marquee_tick = 0; 
            adv = 0;
        }
        char_offset = adv;
    }
    s->marquee_char_offset = (uint16_t)char_offset;

    char visible[40];
    size_t vi = 0;
    uint16_t vw = 0;
    for(size_t k = 0; k < loop_len && vi < sizeof(visible) - 1; k++) {
        size_t idx = (char_offset + k) % loop_len;
        char ch[2] = {loop[idx], '\0'};
        uint16_t cw = canvas_string_width(c, ch);
        if(vw + cw > avail) break;
        visible[vi++] = loop[idx];
        vw += cw;
    }
    visible[vi] = '\0';

    canvas_draw_str_aligned(c, 126, y, AlignRight, AlignBottom, visible);
    canvas_set_font(c, FontSecondary);
}

static void draw_block_bar(Canvas* c, uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t pct) {
    canvas_draw_frame(c, x, y, w, h);
    const uint8_t segments = 24;
    uint8_t seg_w = (w - 2) / segments;
    uint8_t filled = (segments * pct) / 100;
    for(uint8_t i = 0; i < filled; i++) {
        canvas_draw_box(c, x + 1 + i * seg_w, y + 1, seg_w - 1, h - 2);
    }
}

/* ===================== DRAW ===================== */

static void draw(Canvas* c, void* ctx) {
    AppState* s = ctx;
    const Protocol* p = &protocols[s->proto];
    canvas_clear(c);
    canvas_set_font(c, FontSecondary);

    /* ---------- GENERATING ---------- */
    if(s->generating) {
        canvas_draw_str(c, 2, 8, "GENERATING");
        char pct[8];
        snprintf(pct, sizeof(pct), "%u%%", s->progress);
        canvas_draw_str_aligned(c, 126, 8, AlignRight, AlignBottom, pct);
        canvas_draw_line(c, 0, 10, 128, 10);

        draw_block_bar(c, 2, 13, 124, 8, s->progress);

        char cnt[40];
        snprintf(cnt, sizeof(cnt), "%s  %lu/%lu", p->name, s->generated_count, s->count);
        canvas_draw_str(c, 2, 30, cnt);
        canvas_draw_line(c, 0, 33, 128, 33);

        for(uint8_t i = 0; i < s->recent_count; i++) {
            char lbuf[40];
            snprintf(lbuf, sizeof(lbuf), "%06lu: %0*llX",
                s->recent[i].index, p->bytes * 2, s->recent[i].id);
            canvas_draw_str(c, 2, 44 + i * 9, lbuf);
        }
        return;
    }

    /* ---------- PREFIX SELECT ---------- */
    if(s->selecting_prefix) {
        canvas_draw_str(c, 2, 8, "SELECT PREFIX");
        canvas_draw_line(c, 0, 10, 128, 10);
        for(uint8_t i = 0; i < p->prefix_count; i++) {
            bool sel = (i == s->prefix_cursor);
            canvas_set_font(c, sel ? FontPrimary : FontSecondary);
            char l[24];
            snprintf(l, sizeof(l), "%s[%c] 0x%02X",
                sel ? ">" : " ",
                s->prefix_enabled[s->proto][i] ? 'X' : ' ',
                p->prefixes[i]);
            canvas_draw_str(c, 2, 22 + i * 10, l);
        }
        canvas_set_font(c, FontSecondary);
        return;
    }

    /* ---------- SEQUENTIAL SUBMENU ---------- */
    if(s->submenu == SubmenuSequential) {
        canvas_draw_str(c, 2, 8, "SEQUENTIAL");
        canvas_draw_line(c, 0, 10, 128, 10);
        char v[20];
        snprintf(v, sizeof(v), "%06llX", s->seq_start);
        draw_row(c, 23, s->seq_focus == SeqStart, "Start (hex)", v);
        snprintf(v, sizeof(v), "%llu", s->seq_step);
        draw_row(c, 33, s->seq_focus == SeqStep, "Step", v);
        draw_row(c, 43, s->seq_focus == SeqCounter, "Per-prefix", s->per_prefix ? "ON" : "OFF");
        draw_row(c, 53, s->seq_focus == SeqDirection, "Direction", s->seq_reverse ? "REVERSE" : "FORWARD");
        return;
    }

    /* ---------- FUZZ SUBMENU ---------- */
    if(s->submenu == SubmenuFuzz) {
        canvas_draw_str(c, 2, 8, "FUZZ OPTIONS");
        canvas_draw_line(c, 0, 10, 128, 10);
        draw_row(c, 23, s->fuzz_focus == FuzzBoundary, "Boundary IDs", s->fuzz_boundary ? "ON" : "OFF");
        draw_row(c, 33, s->fuzz_focus == FuzzBitflip, "Bit Flip", s->fuzz_bitflip ? "ON" : "OFF");
        char v[8];
        snprintf(v, sizeof(v), "%u", s->fuzz_bits);
        draw_row(c, 43, s->fuzz_focus == FuzzBits, "Flip bits", v);
        draw_row(c, 53, s->fuzz_focus == FuzzPreserve, "Preserve Prefix", s->fuzz_preserve ? "ON" : "OFF");
        char cv[16];
        snprintf(cv, sizeof(cv), "%s/%u", s->collision_enabled ? "ON" : "OFF", s->collision_rate);
        draw_row(c, 63, s->fuzz_focus == FuzzCollision, "Collision", cv);
        return;
    }

    /* ---------- STRUCTURED SUBMENU ---------- */
    if(s->submenu == SubmenuStructured) {
        canvas_draw_str(c, 2, 8, "STRUCTURED TYPE");
        canvas_draw_line(c, 0, 10, 128, 10);
        const char* opts[2] = {"Sequential", "Random"};
        for(uint8_t i = 0; i < StructTypeCount; i++) {
            bool sel = (i == s->structured_type);
            canvas_set_font(c, sel ? FontPrimary : FontSecondary);
            char l[24];
            snprintf(l, sizeof(l), "%s%s", sel ? ">" : " ", opts[i]);
            canvas_draw_str(c, 2, 24 + i * 10, l);
        }
        canvas_set_font(c, FontSecondary);
        return;
    }

    /* ---------- MAIN SCREEN ---------- */
    canvas_draw_str(c, 2, 8, "ListEM");
    const char* type_tag =
        p->type == ProtocolRFID ? "RFID" : p->type == ProtocolNFC ? "NFC" : "iBTN";
    canvas_draw_str_aligned(c, 126, 8, AlignRight, AlignBottom, type_tag);
    canvas_draw_line(c, 0, 10, 128, 10);

    char v[32];
    draw_row_marquee(c, 21, s->focus == FocusProtocol, "Protocol", p->name, FocusProtocol, s);

    draw_row_marquee(c, 31, s->focus == FocusMode, "Mode",
        s->mode == GenModeFuzz ? "Fuzz" :
        s->mode == GenModeStructured ? "Structured" :
        s->mode == GenModeSequential ? "Sequential" : "Random",
        FocusMode, s);

    snprintf(v, sizeof(v), "%lu", s->count);
    draw_row(c, 41, s->focus == FocusCount, "IDs", v);

    {
        uint8_t en = 0;
        for(uint8_t i = 0; i < p->prefix_count; i++)
            if(s->prefix_enabled[s->proto][i]) en++;
        if(p->prefix_count) snprintf(v, sizeof(v), "%u/%u set", en, p->prefix_count);
        else strcpy(v, "N/A");
    }
    draw_row(c, 51, s->focus == FocusPrefixes, "Prefixes", v);

    draw_row(c, 61, s->focus == FocusGenerate, "RUN", "[OK]");
}

/* ===================== INPUT (unchanged logic) ===================== */

static void input(InputEvent* e, void* ctx) {
    AppState* s = ctx;
    const Protocol* p = &protocols[s->proto];
    if(s->generating) return;

    bool is_short = (e->type == InputTypeShort);
    bool is_repeat = (e->type == InputTypeRepeat);
    if(!is_short && !is_repeat) return;

    uint32_t accel = 1;
    if(is_repeat) {
        if(s->hold_ticks < 10) s->hold_ticks++;
        if(s->hold_ticks > 5) accel = s->hold_ticks - 5 + 1;
    } else {
        s->hold_ticks = 0;
    }

    if(s->selecting_prefix) {
        if(e->key==InputKeyUp && s->prefix_cursor>0) s->prefix_cursor--;
        else if(e->key==InputKeyDown && s->prefix_cursor<p->prefix_count-1) s->prefix_cursor++;
        else if(e->key==InputKeyOk) s->prefix_enabled[s->proto][s->prefix_cursor]^=1;
        else if(e->key==InputKeyBack) s->selecting_prefix=false;
        return;
    }

    if(s->submenu == SubmenuSequential) {
        if(e->key==InputKeyUp && s->seq_focus > SeqStart) s->seq_focus--;
        else if(e->key==InputKeyDown && s->seq_focus < SeqDirection) s->seq_focus++;
        else if(e->key==InputKeyLeft) {
            if(s->seq_focus==SeqStart && s->seq_start>0) s->seq_start--;
            else if(s->seq_focus==SeqStep && s->seq_step>1) s->seq_step--;
        }
        else if(e->key==InputKeyRight) {
            if(s->seq_focus==SeqStart) s->seq_start++;
            else if(s->seq_focus==SeqStep) s->seq_step++;
        }
        else if(e->key==InputKeyOk) {
            if(s->seq_focus==SeqCounter) s->per_prefix ^= 1;
            else if(s->seq_focus==SeqDirection) s->seq_reverse ^= 1;
        }
        else if(e->key==InputKeyBack) { s->submenu = SubmenuNone; s->focus = FocusMode; }
        return;
    }

    if(s->submenu == SubmenuFuzz) {
        if(e->key==InputKeyUp && s->fuzz_focus > FuzzBoundary) s->fuzz_focus--;
        else if(e->key==InputKeyDown && s->fuzz_focus < FuzzCollision) s->fuzz_focus++;
        else if(e->key==InputKeyLeft) {
            if(s->fuzz_focus==FuzzBits && s->fuzz_bits > 1) s->fuzz_bits--;
            else if(s->fuzz_focus==FuzzCollision && s->collision_rate > 2) s->collision_rate--;
        }
        else if(e->key==InputKeyRight) {
            if(s->fuzz_focus==FuzzBits) s->fuzz_bits++;
            else if(s->fuzz_focus==FuzzCollision && s->collision_rate < 50) s->collision_rate++;
        }
        else if(e->key==InputKeyOk) {
            if(s->fuzz_focus==FuzzBoundary) s->fuzz_boundary ^= 1;
            else if(s->fuzz_focus==FuzzBitflip) s->fuzz_bitflip ^= 1;
            else if(s->fuzz_focus==FuzzPreserve) s->fuzz_preserve ^= 1;
            else if(s->fuzz_focus==FuzzCollision) s->collision_enabled ^= 1;
        }
        else if(e->key==InputKeyBack) { s->submenu = SubmenuNone; s->focus = FocusMode; }
        return;
    }

    if(s->submenu == SubmenuStructured) {
        if(e->key == InputKeyUp && s->structured_type > 0) s->structured_type--;
        else if(e->key == InputKeyDown && s->structured_type < StructTypeCount - 1) s->structured_type++;
        else if(e->key == InputKeyLeft && s->structured_type > 0) s->structured_type--;
        else if(e->key == InputKeyRight && s->structured_type < StructTypeCount - 1) s->structured_type++;
        else if(e->key == InputKeyBack) { s->submenu = SubmenuNone; s->focus = FocusMode; }
        return;
    }

    if(e->key==InputKeyUp && s->focus > 0) s->focus--;
    else if(e->key==InputKeyDown && s->focus < FocusMax-1) s->focus++;
    else if(e->key==InputKeyLeft) {
        if(s->focus == FocusProtocol) s->proto = (s->proto + PROTOCOL_COUNT - accel) % PROTOCOL_COUNT;
        else if(s->focus == FocusMode) s->mode = (s->mode + GenModeCount - 1) % GenModeCount;
        else if(s->focus == FocusCount) {
            uint32_t ea = accel > 5 ? 5 : accel;
            uint32_t dec = COUNT_STEP * ea;
            if(s->count <= COUNT_STEP || s->count <= dec) s->count = MAX_IDS;
            else s->count -= dec;
        }
    }
    else if(e->key==InputKeyRight) {
        if(s->focus == FocusProtocol) s->proto = (s->proto + accel) % PROTOCOL_COUNT;
        else if(s->focus == FocusMode) s->mode = (s->mode + 1) % GenModeCount;
        else if(s->focus == FocusCount) {
            uint32_t ea = accel > 5 ? 5 : accel;
            uint32_t inc = COUNT_STEP * ea;
            if(s->count + inc > MAX_IDS) s->count = COUNT_STEP;
            else s->count += inc;
        }
    }
    else if(e->key==InputKeyOk) {
        if(s->focus==FocusMode) {
            if(s->mode==GenModeSequential) { s->submenu=SubmenuSequential; s->seq_focus=SeqStart; }
            else if(s->mode==GenModeFuzz) { s->submenu=SubmenuFuzz; s->fuzz_focus=FuzzBoundary; }
            else if(s->mode==GenModeStructured) { s->submenu=SubmenuStructured; }
        }
        else if(s->focus==FocusPrefixes && p->prefix_count>0) {
            s->selecting_prefix=true;
            s->prefix_cursor=0;
        }
        else if(s->focus==FocusGenerate && !s->generating) {
            s->generating = true;
            s->progress = 0;
            s->generated_count = 0;
            s->worker = furi_thread_alloc();
            furi_thread_set_name(s->worker, "ListEM_Gen");
            furi_thread_set_stack_size(s->worker, 4096);
            furi_thread_set_callback(s->worker, generate_worker);
            furi_thread_set_context(s->worker, s);
            furi_thread_start(s->worker);
        }
    }
    else if(e->key==InputKeyBack) {
        s->run = false;
    }
}

/* ===================== ENTRY ===================== */

int32_t rfid_fuzzer_app(void* p) {
    UNUSED(p);
    AppState s = {
        .proto=0,.count=30000,.run=true,
        .mode=GenModeSequential,.structured_type = StructSeq,.focus=FocusProtocol,
        .seq_start=0,.seq_step=1,
        .seq_reverse = false,
        .fuzz_boundary=true,.fuzz_bitflip=true,
        .fuzz_bits=1,.fuzz_preserve=true,
        .collision_enabled = false,
        .collision_rate = 10,
        .collision_pool_size = 0,
        .hold_ticks=0,
        .marquee_owner = FocusMax,
        .marquee_last_value = NULL,
        .marquee_tick = 0,
        .marquee_char_offset = 0,
    };
    memset(s.prefix_enabled, 0, sizeof(s.prefix_enabled));
    s.recent_count = 0;
    if(s.count > MAX_IDS) s.count = MAX_IDS;

    ViewPort* v=view_port_alloc();
    view_port_draw_callback_set(v,draw,&s);
    view_port_input_callback_set(v,input,&s);
    Gui* g=furi_record_open(RECORD_GUI);
    gui_add_view_port(g,v,GuiLayerFullscreen);

    while(s.run) { view_port_update(v); furi_delay_ms(50); }

    if(s.worker){ furi_thread_join(s.worker); furi_thread_free(s.worker); }
    gui_remove_view_port(g,v);
    view_port_free(v);
    furi_record_close(RECORD_GUI);
    return 0;
}
