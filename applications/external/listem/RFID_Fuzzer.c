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

typedef enum {
    ProtocolRFID,
    ProtocolNFC,
    ProtocolIButton
} ProtocolType;

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

/* RFID */
static const uint8_t P_EM4100[] = {0x00, 0x01, 0x02, 0x03};
static const uint8_t P_HID[] = {0xA0, 0xB0, 0xC0};
static const uint8_t P_INDALA[] = {0x20, 0x21, 0x22};
static const uint8_t P_IOPROX[] = {0x20, 0x21, 0x22};
static const uint8_t P_PAC[] = {0xE0};
static const uint8_t P_PARADOX[] = {0xA0, 0xC0};
static const uint8_t P_VIKING[] = {0x40, 0x50};
static const uint8_t P_PYRAMID[] = {0x80};
static const uint8_t P_KERI[] = {0x60};
static const uint8_t P_H10301[] = {0xF0, 0xF1};
static const uint8_t P_JABLO[] = {0x02};
static const uint8_t P_ELECTRA[] = {0x01};
static const uint8_t P_IDTECK[] = {0x0A};
static const uint8_t P_GALL[] = {0x20, 0x30};

/* NFC */
static const uint8_t P_MIFARE[] = {0x04, 0x08, 0x12};
static const uint8_t P_UL[] = {0x04};
static const uint8_t P_ICLASS[] = {0xEC};

/* iButton */
static const uint8_t P_DALLAS[] = {0x01, 0x02};

/* ===================== PROTOCOL LIST ===================== */

static const Protocol protocols[] = {
    /* RFID */
    {"EM4100", 5, 0xFFFFFFFFFF, ProtocolRFID, P_EM4100, 4},
    {"HID Prox", 6, 0xFFFFFFFFFFFF, ProtocolRFID, P_HID, 3},
    {"Indala", 4, 0xFFFFFFFF, ProtocolRFID, P_INDALA, 3},
    {"IoProx", 4, 0xFFFFFFFF, ProtocolRFID, P_IOPROX, 3},
    {"PAC/Stanley", 4, 0xFFFFFFFF, ProtocolRFID, P_PAC, 1},
    {"Paradox", 6, 0xFFFFFFFFFFFF, ProtocolRFID, P_PARADOX, 2},
    {"Viking", 4, 0xFFFFFFFF, ProtocolRFID, P_VIKING, 2},
    {"Pyramid", 4, 0xFFFFFFFF, ProtocolRFID, P_PYRAMID, 1},
    {"Keri", 4, 0xFFFFFFFF, ProtocolRFID, P_KERI, 1},
    {"Nexwatch", 8, 0xFFFFFFFFFFFFFFFF, ProtocolRFID, NULL, 0},
    {"H10301", 3, 0xFFFFFF, ProtocolRFID, P_H10301, 2},
    {"Jablotron", 5, 0xFFFFFFFFFF, ProtocolRFID, P_JABLO, 1},
    {"Electra", 8, 0xFFFFFFFFFFFFFFFF, ProtocolRFID, P_ELECTRA, 1},
    {"IDTeck", 8, 0xFFFFFFFFFFFFFFFF, ProtocolRFID, P_IDTECK, 1},
    {"Gallagher", 8, 0xFFFFFFFFFFFFFFFF, ProtocolRFID, P_GALL, 2},

    /* NFC */
    {"MIFARE Classic 1K", 4, 0xFFFFFFFF, ProtocolNFC, P_MIFARE, 3},
    {"MIFARE Classic 4K", 4, 0xFFFFFFFF, ProtocolNFC, P_MIFARE, 3},
    {"MIFARE Ultralight", 7, 0xFFFFFFFFFFFFFF, ProtocolNFC, P_UL, 1},
    {"DESFire EV1", 16, 0, ProtocolNFC, NULL, 0},
    {"iCLASS", 8, 0xFFFFFFFFFFFFFFFF, ProtocolNFC, P_ICLASS, 1},
    {"FeliCa", 8, 0xFFFFFFFFFFFFFFFF, ProtocolNFC, NULL, 0},

    /* iButton */
    {"Dallas DS1990", 8, 0xFFFFFFFFFFFFFFFF, ProtocolIButton, P_DALLAS, 2},
    {"Cyfral", 2, 0xFFFF, ProtocolIButton, NULL, 0},
    {"Metacom", 4, 0xFFFFFFFF, ProtocolIButton, NULL, 0},
    {"Maxim iButton", 8, 0xFFFFFFFFFFFFFFFF, ProtocolIButton, NULL, 0},
    {"Keypad/Access Control", 8, 0xFFFFFFFFFFFFFFFF, ProtocolIButton, NULL, 0},
    {"Temperature", 8, 0xFFFFFFFFFFFFFFFF, ProtocolIButton, NULL, 0},
    {"Custom iButton", 8, 0xFFFFFFFFFFFFFFFF, ProtocolIButton, NULL, 0},
};

#define PROTOCOL_COUNT (sizeof(protocols) / sizeof(Protocol))
#define MAX_PREFIXES   4
#define COUNT_STEP     1000
#define MAX_IDS        300000

static const uint8_t STRUCT_TAILS[] = {0x00, 0x01, 0x02, 0x10, 0x20, 0xFF, 0xAA, 0x55};

#define STRUCT_TAIL_COUNT (sizeof(STRUCT_TAILS))

/* ===================== APP STATE ===================== */

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
    FuriThread* worker;

    uint8_t hold_ticks;
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

    /* reset collision pool */
    s->collision_pool_size = 0;

    Storage* st = furi_record_open(RECORD_STORAGE);
    ensure_dirs(st, p->type);

    /* clean protocol name */
    char clean[64];
    strncpy(clean, p->name, sizeof(clean) - 1);
    clean[sizeof(clean) - 1] = '\0';
    for(size_t i = 0; clean[i]; i++)
        if(clean[i] == ' ') clean[i] = '_';

    const char* mode_str = (s->mode == GenModeSequential) ? "sequential" :
                           (s->mode == GenModeFuzz)       ? "fuzz" :
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

        if(!has_prefix) {
            strcpy(prefix_part, "noprefix");
        }
    }

    /* ===================== OUTPUT PATH ===================== */

    char path[192];
    snprintf(
        path, sizeof(path), "%s/%s_%s_%s.txt", base_path(p->type), clean, mode_str, prefix_part);

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

            uint64_t start = s->seq_reverse ? (p->max ? p->max : ((1ULL << (p->bytes * 8)) - 1)) :
                                              s->seq_start;

            uint64_t seq;

            if(!s->seq_reverse) {
                seq = start + offset;
            } else {
                if(start > offset)
                    seq = start - offset;
                else
                    seq = 0;
            }

            id = seq;
        }

        /* ================= STRUCTURED ================= */

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

            uint64_t start = s->seq_reverse ? (p->max ? p->max : ((1ULL << (p->bytes * 8)) - 1)) :
                                              s->seq_start;

            uint64_t seq;

            if(!s->seq_reverse) {
                seq = start + offset;
            } else {
                if(start > offset)
                    seq = start - offset;
                else
                    seq = 0;
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

            if(rem_bytes > 1) {
                id = (base << 8) | tail;
            } else {
                id = base;
            }
        }

        else {
            uint64_t r = ((uint64_t)rand() << 32) | rand();
            id = r & max;
        }

        /* ================= FUZZ MUTATIONS ================= */

        if(s->mode == GenModeFuzz) {
            if(s->fuzz_boundary) {
                static const uint64_t patterns[] = {
                    0x0ULL, 0xFFFFFFFFFFFFFFFFULL, 0xAAAAAAAAAAAAAAAAULL, 0x5555555555555555ULL};
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
                    /*======================= COMBINATORIAL PREFIX ===============*/

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

        if(s->mode == GenModeFuzz && s->collision_enabled && s->collision_pool_size > 0 &&
           (i % s->collision_rate == 0)) {
            id = s->collision_pool[rand() % s->collision_pool_size];
        }

        if(s->mode == GenModeFuzz && s->collision_enabled && s->collision_pool_size < 16) {
            s->collision_pool[s->collision_pool_size++] = id;
        }

        char line[40];
        snprintf(line, sizeof(line), "%0*llX\n", p->bytes * 2, id);
        storage_file_write(f, line, strlen(line));

        if(i % step == 0) s->progress = (i * 100) / s->count;
    }

    storage_file_close(f);
    storage_file_free(f);
    furi_record_close(RECORD_STORAGE);

    s->generating = false;
    return 0;
}

/* ===================== DRAW ===================== */

static void draw(Canvas* c, void* ctx) {
    AppState* s = ctx;
    const Protocol* p = &protocols[s->proto];
    canvas_clear(c);

    if(s->generating) {
        canvas_draw_str(c, 18, 12, "Generating...");

        char pct[16];
        snprintf(pct, sizeof(pct), "%u%%", s->progress);
        canvas_draw_str(c, 52, 26, pct);

        const uint8_t bar_x = 10;
        const uint8_t bar_y = 40;
        const uint8_t bar_w = 108;
        const uint8_t bar_h = 8;

        canvas_draw_frame(c, bar_x, bar_y, bar_w, bar_h);

        uint8_t fill = (bar_w - 2) * s->progress / 100;
        canvas_draw_box(c, bar_x + 1, bar_y + 1, fill, bar_h - 2);

        return;
    }

    if(s->selecting_prefix) {
        canvas_draw_str(c, 2, 12, "Select Prefix");
        for(uint8_t i = 0; i < p->prefix_count; i++) {
            char l[32];
            snprintf(
                l,
                sizeof(l),
                "%c [%c] 0x%02X",
                (i == s->prefix_cursor) ? '>' : ' ',
                s->prefix_enabled[s->proto][i] ? 'X' : ' ',
                p->prefixes[i]);
            canvas_draw_str(c, 2, 24 + i * 10, l);
        }
        return;
    }

    if(s->submenu == SubmenuSequential) {
        char l[32];

        snprintf(l, sizeof(l), "Start: %llu", s->seq_start);
        canvas_draw_str(c, 2, 14, (s->seq_focus == SeqStart) ? ">" : " ");
        canvas_draw_str(c, 10, 14, l);

        snprintf(l, sizeof(l), "Step: %llu", s->seq_step);
        canvas_draw_str(c, 2, 26, (s->seq_focus == SeqStep) ? ">" : " ");
        canvas_draw_str(c, 10, 26, l);

        canvas_draw_str(c, 2, 38, (s->seq_focus == SeqCounter) ? ">" : " ");
        canvas_draw_str(c, 10, 38, s->per_prefix ? "Per-prefix: ON" : "Per-prefix: OFF");

        snprintf(l, sizeof(l), "Direction: %s", s->seq_reverse ? "Reverse" : "Forward");
        canvas_draw_str(c, 2, 50, (s->seq_focus == SeqDirection) ? ">" : " ");
        canvas_draw_str(c, 10, 50, l);

        return;
    }

    if(s->submenu == SubmenuFuzz) {
        canvas_draw_str(c, 2, 14, (s->fuzz_focus == FuzzBoundary) ? ">" : " ");
        canvas_draw_str(c, 10, 14, s->fuzz_boundary ? "Boundary IDs: ON" : "Boundary IDs: OFF");

        canvas_draw_str(c, 2, 26, (s->fuzz_focus == FuzzBitflip) ? ">" : " ");
        canvas_draw_str(c, 10, 26, s->fuzz_bitflip ? "Bit Flip: ON" : "Bit Flip: OFF");

        char l[32];
        snprintf(l, sizeof(l), "Flip bits: %u", s->fuzz_bits);
        canvas_draw_str(c, 2, 38, (s->fuzz_focus == FuzzBits) ? ">" : " ");
        canvas_draw_str(c, 10, 38, l);

        canvas_draw_str(c, 2, 50, (s->fuzz_focus == FuzzPreserve) ? ">" : " ");
        canvas_draw_str(
            c, 10, 50, s->fuzz_preserve ? "Preserve Prefix: ON" : "Preserve Prefix: OFF");

        char cbuf[32];
        snprintf(
            cbuf,
            sizeof(cbuf),
            "Collision: %s (%u)",
            s->collision_enabled ? "ON" : "OFF",
            s->collision_rate);

        canvas_draw_str(c, 2, 62, (s->fuzz_focus == FuzzCollision) ? ">" : " ");
        canvas_draw_str(c, 10, 62, cbuf);

        return;
    }

    if(s->submenu == SubmenuStructured) {
        canvas_draw_str(c, 2, 12, "Structured Type");

        for(uint8_t i = 0; i < StructTypeCount; i++) {
            char line[32];

            const char* name = (i == StructSeq) ? "Sequential" : "Random";

            snprintf(
                line,
                sizeof(line),
                "%c [%c] %s",
                (i == s->structured_type) ? '>' : ' ',
                (i == s->structured_type) ? 'X' : ' ',
                name);

            canvas_draw_str(c, 2, 24 + i * 12, line);
        }

        return;
    }

    int y = 12;
#define ROW(id, text)                                       \
    canvas_draw_str(c, 2, y, (s->focus == id) ? ">" : " "); \
    canvas_draw_str(c, 10, y, text);                        \
    y += 12;

    char line[64];
    snprintf(line, sizeof(line), "Protocol: %s", p->name);
    ROW(FocusProtocol, line);
    snprintf(
        line,
        sizeof(line),
        "Mode: %s",
        s->mode == GenModeFuzz       ? "Fuzz" :
        s->mode == GenModeStructured ? "Structured" :
        s->mode == GenModeSequential ? "Sequential" :
                                       "Random");
    ROW(FocusMode, line);
    snprintf(line, sizeof(line), "IDs: %lu", s->count);
    ROW(FocusCount, line);
    ROW(FocusPrefixes, p->prefix_count ? "Prefixes: Edit" : "Prefixes: N/A");
    ROW(FocusGenerate, "Generate!");
#undef ROW
}

/* ===================== INPUT ===================== */

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
        if(e->key == InputKeyUp && s->prefix_cursor > 0)
            s->prefix_cursor--;
        else if(e->key == InputKeyDown && s->prefix_cursor < p->prefix_count - 1)
            s->prefix_cursor++;
        else if(e->key == InputKeyOk)
            s->prefix_enabled[s->proto][s->prefix_cursor] ^= 1;
        else if(e->key == InputKeyBack)
            s->selecting_prefix = false;
        return;
    }

    if(s->submenu == SubmenuSequential) {
        if(e->key == InputKeyUp && s->seq_focus > SeqStart)
            s->seq_focus--;

        else if(e->key == InputKeyDown && s->seq_focus < SeqDirection)
            s->seq_focus++;

        else if(e->key == InputKeyLeft) {
            if(s->seq_focus == SeqStart && s->seq_start > 0)
                s->seq_start--;
            else if(s->seq_focus == SeqStep && s->seq_step > 1)
                s->seq_step--;
        }

        else if(e->key == InputKeyRight) {
            if(s->seq_focus == SeqStart)
                s->seq_start++;
            else if(s->seq_focus == SeqStep)
                s->seq_step++;
        }

        else if(e->key == InputKeyOk) {
            if(s->seq_focus == SeqCounter)
                s->per_prefix ^= 1;
            else if(s->seq_focus == SeqDirection)
                s->seq_reverse ^= 1;
        }

        else if(e->key == InputKeyBack) {
            s->submenu = SubmenuNone;
            s->focus = FocusMode;
        }

        return;
    }

    if(s->submenu == SubmenuFuzz) {
        if(e->key == InputKeyUp && s->fuzz_focus > FuzzBoundary)
            s->fuzz_focus--;

        else if(e->key == InputKeyDown && s->fuzz_focus < FuzzCollision)
            s->fuzz_focus++;

        else if(e->key == InputKeyLeft) {
            if(s->fuzz_focus == FuzzBits && s->fuzz_bits > 1)
                s->fuzz_bits--;
            else if(s->fuzz_focus == FuzzCollision && s->collision_rate > 2)
                s->collision_rate--;
        }

        else if(e->key == InputKeyRight) {
            if(s->fuzz_focus == FuzzBits)
                s->fuzz_bits++;
            else if(s->fuzz_focus == FuzzCollision && s->collision_rate < 50)
                s->collision_rate++;
        }

        else if(e->key == InputKeyOk) {
            if(s->fuzz_focus == FuzzBoundary)
                s->fuzz_boundary ^= 1;
            else if(s->fuzz_focus == FuzzBitflip)
                s->fuzz_bitflip ^= 1;
            else if(s->fuzz_focus == FuzzPreserve)
                s->fuzz_preserve ^= 1;
            else if(s->fuzz_focus == FuzzCollision)
                s->collision_enabled ^= 1;
        }

        else if(e->key == InputKeyBack) {
            s->submenu = SubmenuNone;
            s->focus = FocusMode;
        }

        return;
    }

    if(s->submenu == SubmenuStructured) {
        if(e->key == InputKeyUp && s->structured_type > 0) {
            s->structured_type--;
        }

        else if(e->key == InputKeyDown && s->structured_type < StructTypeCount - 1) {
            s->structured_type++;
        }

        else if(e->key == InputKeyBack) {
            s->submenu = SubmenuNone;
            s->focus = FocusMode;
        }

        return;
    }

    if(e->key == InputKeyUp && s->focus > 0) {
        s->focus--;
    } else if(e->key == InputKeyDown && s->focus < FocusMax - 1) {
        s->focus++;
    } else if(e->key == InputKeyLeft) {
        if(s->focus == FocusProtocol)
            s->proto = (s->proto + PROTOCOL_COUNT - accel) % PROTOCOL_COUNT;
        else if(s->focus == FocusMode)
            s->mode = (s->mode + GenModeCount - 1) % GenModeCount;
        else if(s->focus == FocusCount) {
            uint32_t effective_accel = accel;
            if(effective_accel > 5) effective_accel = 5;

            uint32_t dec = COUNT_STEP * effective_accel;

            if(s->count <= COUNT_STEP || s->count <= dec) {
                s->count = MAX_IDS;
            } else {
                s->count -= dec;
            }
        }
    } else if(e->key == InputKeyRight) {
        if(s->focus == FocusProtocol)
            s->proto = (s->proto + accel) % PROTOCOL_COUNT;
        else if(s->focus == FocusMode)
            s->mode = (s->mode + 1) % GenModeCount;
        else if(s->focus == FocusCount) {
            uint32_t effective_accel = accel;
            if(effective_accel > 5) effective_accel = 5;

            uint32_t inc = COUNT_STEP * effective_accel;

            if(s->count + inc > MAX_IDS) {
                s->count = COUNT_STEP;
            } else {
                s->count += inc;
            }
        }
    } else if(e->key == InputKeyOk) {
        if(s->focus == FocusMode) {
            if(s->mode == GenModeSequential) {
                s->submenu = SubmenuSequential;
                s->seq_focus = SeqStart;
            } else if(s->mode == GenModeFuzz) {
                s->submenu = SubmenuFuzz;
                s->fuzz_focus = FuzzBoundary;
            } else if(s->mode == GenModeStructured) {
                s->submenu = SubmenuStructured;
            }
        } else if(s->focus == FocusPrefixes && p->prefix_count > 0) {
            s->selecting_prefix = true;
            s->prefix_cursor = 0;
        } else if(s->focus == FocusGenerate && !s->generating) {
            s->generating = true;
            s->progress = 0;

            s->worker = furi_thread_alloc();
            furi_thread_set_name(s->worker, "ListEM_Gen");
            furi_thread_set_stack_size(s->worker, 4096);
            furi_thread_set_callback(s->worker, generate_worker);
            furi_thread_set_context(s->worker, s);
            furi_thread_start(s->worker);
        }
    } else if(e->key == InputKeyBack) {
        s->run = false;
    }
}

/* ===================== ENTRY ===================== */

int32_t rfid_fuzzer_app(void* p) {
    UNUSED(p);

    AppState s = {
        .proto = 0,
        .count = 30000,
        .run = true,
        .mode = GenModeSequential,
        .structured_type = StructSeq,
        .focus = FocusProtocol,
        .seq_start = 0,
        .seq_step = 1,
        .seq_reverse = false,
        .fuzz_boundary = true,
        .fuzz_bitflip = true,
        .fuzz_bits = 1,
        .fuzz_preserve = true,
        .collision_enabled = false,
        .collision_rate = 10,
        .collision_pool_size = 0,
        .hold_ticks = 0,
    };

    memset(s.prefix_enabled, 0, sizeof(s.prefix_enabled));

    if(s.count > MAX_IDS) s.count = MAX_IDS;

    ViewPort* v = view_port_alloc();
    view_port_draw_callback_set(v, draw, &s);
    view_port_input_callback_set(v, input, &s);

    Gui* g = furi_record_open(RECORD_GUI);
    gui_add_view_port(g, v, GuiLayerFullscreen);

    while(s.run) {
        view_port_update(v);
        furi_delay_ms(50);
    }

    if(s.worker) {
        furi_thread_join(s.worker);
        furi_thread_free(s.worker);
    }
    gui_remove_view_port(g, v);
    view_port_free(v);
    furi_record_close(RECORD_GUI);
    return 0;
}
