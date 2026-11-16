/*
 * IR RAW Edit - a tiny waveform editor for Flipper Zero infrared .ir signals.
 * https://github.com/Lechnio/IR-RAW-Edit
 */

#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <storage/storage.h>
#include <dialogs/dialogs.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <gui/modules/widget.h>
#include <gui/modules/text_input.h>
#include <gui/modules/number_input.h>
#include <gui/modules/variable_item_list.h>

#include <infrared.h>
#include <infrared_worker.h>

#include "ir_raw_edit_icons.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define IR_DIR            "/ext/infrared"
#define IR_EXT            ".ir"
#define MAX_SAMPLES       24000
#define SYNTH_STEPS_MAX   4096
#define DUR_CLAMP         32000
#define LOAD_HEAP_RESERVE 12288
#define PICK_MAX_SIGNALS  256

#define DEFAULT_FREQUENCY 38000
#define DEFAULT_DUTY      "0.330000"

#define WAVE_SPAN_US        40000
#define DETECT_GAP_US       15000
#define DETECT_MIN_FRAME_US 5000

#define MERGE_GAP_DEFAULT_MS 100
#define MERGE_GAP_MIN_MS     1
#define MERGE_GAP_MAX_MS     1000
#define MERGE_REPEAT_DEFAULT 1
#define MERGE_REPEAT_MIN     1
#define MERGE_REPEAT_MAX     64
#define MERGE_PATH_LEN       128
#define MERGE_BUF_CHUNK      512

#define APP_VERSION "1.0"
#define APP_REPO    "github.com/Lechnio/IR-RAW-Edit"
#define APP_NAME    "IR RAW Edit"

#define SCREEN_W_PX  128
#define SCREEN_H_PX  64
#define FONT_SIZE_PX 7
#define GAP_PX       1

#define TOP_TEXT_BASE FONT_SIZE_PX
#define OV_Y          (TOP_TEXT_BASE + GAP_PX)
#define OV_H          5
#define WAVE_H        32
#define WAVE_TOP      (OV_Y + OV_H + 1)
#define WAVE_BOT      (WAVE_TOP + WAVE_H)
#define SEP_Y         (WAVE_BOT + 1)
#define LINE1_BASE    (SEP_Y + 1 + GAP_PX + FONT_SIZE_PX)
#define LINE2_BASE    (LINE1_BASE + 1 + FONT_SIZE_PX)

static_assert(LINE2_BASE <= SCREEN_H_PX);

#define MENU_SEL_SAVE   0
#define MENU_SEL_DECODE 1
#define MENU_SEL_CUT    2
#define MENU_SEL_UNDO   3
#define MENU_ITEMS      4

#define DECODE_TAIL_US 150000
#define DECODE_MAX     32

// Samples are signed durations in us: positive = carrier on (mark), negative = off (space).
typedef struct {
    int16_t* data;
    size_t count;
    size_t cap;
    int32_t total_us;
    uint32_t frequency;
    char duty[12];
    char name[32];
    bool truncated;
    bool out_of_memory;
    bool synthesized;
} IrData;

typedef enum {
    EditModeEdit = 0,
    EditModeMenu = 1,
} EditMode;

typedef struct {
    IrData sd;
    char basename[48];

    int32_t view_start;
    int32_t view_end;
    int32_t marker_a;
    int32_t marker_b;
    int active;

    uint16_t activity[SCREEN_W_PX];
    uint16_t act_max;
    uint16_t overview[SCREEN_W_PX];
    uint16_t ov_max;
    bool wave_mode;

    char status[80];
    uint32_t status_until;

    bool saving;

    int mode;
    int menu_sel;

    int16_t* undo_data;
    size_t undo_count;
    int32_t undo_total_us;
    int32_t undo_marker_a;
    int32_t undo_marker_b;
    int undo_active;
    int32_t undo_view_start;
    int32_t undo_view_end;
    bool has_undo;

    FuriMutex* mutex;
} App;

static bool g_normalize_jitter = false;
static int32_t g_merge_gap_ms = MERGE_GAP_DEFAULT_MS;
static int32_t g_merge_repeat = MERGE_REPEAT_DEFAULT;

static inline int32_t iabs32(int32_t v) {
    return v < 0 ? -v : v;
}

static inline int32_t clamp_dur(int32_t v) {
    if(v > DUR_CLAMP) return DUR_CLAMP;

    if(v < -DUR_CLAMP) return -DUR_CLAMP;

    return v;
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

    return (int)(((int64_t)(t - a->view_start) * SCREEN_W_PX) / span);
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

// A long space split into several samples is one level, not repeated edges.
static bool sample_is_edge(const IrData* sd, size_t i) {
    return (i == 0) ? (sd->data[i] > 0) : ((sd->data[i] < 0) != (sd->data[i - 1] < 0));
}

static void recompute_activity(App* a) {
    memset(a->activity, 0, sizeof(a->activity));
    int32_t span = a->view_end - a->view_start;
    if(span <= 0) span = 1;

    a->wave_mode = (span <= WAVE_SPAN_US);

    int32_t run = 0;
    uint16_t mx = 0;
    for(size_t i = 0; i < a->sd.count; i++) {
        int32_t ad = iabs32(a->sd.data[i]);
        int32_t s0 = run;
        run += ad;
        int32_t s1 = run;

        if(s1 < a->view_start || s0 > a->view_end) continue;

        if(!sample_is_edge(&a->sd, i)) continue;

        int x = (int)(((int64_t)(s0 - a->view_start) * SCREEN_W_PX) / span);

        if(x < 0) x = 0;

        if(x > SCREEN_W_PX - 1) x = SCREEN_W_PX - 1;

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

        if(!sample_is_edge(&a->sd, i)) continue;

        int x = (int)(((int64_t)s0 * SCREEN_W_PX) / total);

        if(x < 0) x = 0;

        if(x > SCREEN_W_PX - 1) x = SCREEN_W_PX - 1;

        if(a->overview[x] < 65535) a->overview[x]++;

        if(a->overview[x] > mx) mx = a->overview[x];
    }

    a->ov_max = mx ? mx : 1;
}

static void select_all(App* a) {
    int32_t total = a->sd.total_us;
    a->marker_a = 0;
    a->marker_b = total;
    a->view_start = 0;
    a->view_end = total > 0 ? total : 1;
    clamp_view(a);
}

// Select the longest burst between long spaces (the most complete frame copy).
static void auto_detect(App* a) {
    if(a->sd.synthesized || a->sd.count < 2) {
        select_all(a);
        return;
    }

    int32_t run = 0;
    int32_t seg_start = -1;
    int32_t last_edge = 0;
    int32_t best_a = -1, best_b = -1;
    int32_t best_len = 0;

    for(size_t i = 0; i <= a->sd.count; i++) {
        bool end = (i == a->sd.count);
        int32_t ad = end ? 0 : iabs32(a->sd.data[i]);
        int32_t s0 = run;
        run += ad;

        if(end || (a->sd.data[i] < 0 && ad >= DETECT_GAP_US)) {
            int32_t seglen = last_edge - seg_start;
            if(seg_start >= 0 && seglen >= DETECT_MIN_FRAME_US && seglen > best_len) {
                best_len = seglen;
                best_a = seg_start;
                best_b = last_edge;
            }

            seg_start = -1;
        } else if(a->sd.data[i] > 0) {
            if(seg_start < 0) seg_start = s0;

            last_edge = run;
        }
    }

    if(best_a < 0) {
        select_all(a);
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
    uint8_t buf[128];
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

// Value of a "key: value" line, or NULL when the line has another key.
static const char* line_value(const char* s, const char* key) {
    size_t n = strlen(key);
    if(strncmp(s, key, n) != 0 || s[n] != ':') return NULL;

    s += n + 1;
    while(*s == ' ')
        s++;

    return s;
}

static void copy_str(char* dst, const char* src, size_t n) {
    strncpy(dst, src, n - 1);
    dst[n - 1] = '\0';
}

static bool append_sample(IrData* sd, int32_t v) {
    if(sd->count >= sd->cap) {
        sd->truncated = true;
        return false;
    }

    sd->data[sd->count++] = (int16_t)clamp_dur(v);

    return true;
}

static void* safe_malloc(size_t size) {
    if(size == 0 || memmgr_get_free_heap() < size + LOAD_HEAP_RESERVE) return NULL;

    return malloc(size);
}

static void* safe_realloc(void* ptr, size_t size) {
    if(size == 0 || memmgr_get_free_heap() < size + LOAD_HEAP_RESERVE) return NULL;

    return realloc(ptr, size);
}

static bool data_grow(IrData* sd, size_t need) {
    if(sd->cap >= need) return true;

    size_t ncap = sd->cap ? sd->cap : 256;
    while(ncap < need)
        ncap *= 2;

    if(ncap > MAX_SAMPLES) ncap = MAX_SAMPLES;

    if(ncap < need) return false;

    int16_t* grown = safe_realloc(sd->data, ncap * sizeof(int16_t));
    if(!grown) {
        sd->out_of_memory = true;
        return false;
    }

    sd->data = grown;
    sd->cap = ncap;
    return true;
}

// Durations longer than DUR_CLAMP are split into several same-sign samples.
static bool push_duration(IrData* sd, int32_t v) {
    int32_t sign = (v < 0) ? -1 : 1;
    int32_t mag = iabs32(v);

    do {
        int32_t chunk = (mag > DUR_CLAMP) ? DUR_CLAMP : mag;
        if(!data_grow(sd, sd->count + 1)) {
            sd->truncated = true;
            return false;
        }
        sd->data[sd->count++] = (int16_t)(sign * chunk);
        mag -= chunk;
    } while(mag > 0);

    return true;
}

static size_t duration_samples(int32_t mag) {
    return (mag > DUR_CLAMP) ? (size_t)((mag + DUR_CLAMP - 1) / DUR_CLAMP) : 1;
}

// "07 00 00 00" -> 0x00000007 (bytes are stored little-endian in .ir files).
static uint32_t parse_hex_le(const char* s) {
    uint32_t v = 0;
    char* end;
    for(int i = 0; i < 4; i++) {
        unsigned long b = strtoul(s, &end, 16);
        if(end == s) break;

        v |= (uint32_t)(b & 0xFF) << (8 * i);
        s = end;
    }

    return v;
}

static bool message_is_valid(const InfraredMessage* msg) {
    if(!infrared_is_protocol_valid(msg->protocol)) return false;

    uint8_t al = infrared_get_protocol_address_length(msg->protocol);
    uint8_t cl = infrared_get_protocol_command_length(msg->protocol);
    return (al >= 32 || (msg->address >> al) == 0) && (cl >= 32 || (msg->command >> cl) == 0);
}

static void fmt_duty(float duty, char* out, size_t n) {
    uint32_t u = (uint32_t)(duty * 1000000.0f + 0.5f);
    snprintf(out, n, "%lu.%06lu", (unsigned long)(u / 1000000), (unsigned long)(u % 1000000));
}

// Parsed signals are rendered by the firmware's own IR encoder, so every
// protocol it knows works. Frames are repeated as many times as a real
// transmission would send them.
static bool synthesize_message(const InfraredMessage* msg, IrData* sd) {
    InfraredEncoderHandler* enc = infrared_alloc_encoder();
    infrared_reset_encoder(enc, msg);

    size_t frames = infrared_get_protocol_min_repeat_count(msg->protocol);
    if(frames < 1) frames = 1;

    bool ok = true;
    for(size_t step = 0; frames > 0 && step < SYNTH_STEPS_MAX; step++) {
        uint32_t dur;
        bool level;
        InfraredStatus st = infrared_encode(enc, &dur, &level);
        if(st == InfraredStatusError) {
            ok = false;
            break;
        }

        if((level || sd->count > 0) && !push_duration(sd, level ? (int32_t)dur : -(int32_t)dur))
            break;

        if(st == InfraredStatusDone) frames--;
    }

    infrared_free_encoder(enc);

    sd->frequency = infrared_get_protocol_frequency(msg->protocol);
    fmt_duty(infrared_get_protocol_duty_cycle(msg->protocol), sd->duty, sizeof(sd->duty));
    return ok && sd->count > 0;
}

static void normalize_jitter(IrData* sd);
static void recompute_total_us(IrData* sd);

// Load the signal number `index` of an .ir file. Raw timings start with a mark
// and alternate mark/space; parsed signals are synthesized.
static bool load_ir(Storage* storage, const char* path, uint32_t index, IrData* sd) {
    memset(sd, 0, sizeof(*sd));
    sd->frequency = DEFAULT_FREQUENCY;
    copy_str(sd->duty, DEFAULT_DUTY, sizeof(sd->duty));

    File* f = storage_file_alloc(storage);
    if(!storage_file_open(f, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_free(f);
        return false;
    }

    LineReader lr = {.file = f, .len = 0, .pos = 0, .eof = false};
    FuriString* line = furi_string_alloc();
    InfraredMessage msg = {.protocol = InfraredProtocolUnknown};
    bool parsed = false;
    int32_t cur = -1;

    while(lr_read_line(&lr, line)) {
        const char* s = furi_string_get_cstr(line);
        const char* v;

        if((v = line_value(s, "name"))) {
            if(++cur > (int32_t)index) break;

            if(cur == (int32_t)index) copy_str(sd->name, v, sizeof(sd->name));

            continue;
        }

        if(cur != (int32_t)index) continue;

        if((v = line_value(s, "type"))) {
            parsed = (strcmp(v, "parsed") == 0);
        } else if((v = line_value(s, "frequency"))) {
            sd->frequency = (uint32_t)strtoul(v, NULL, 10);
        } else if((v = line_value(s, "duty_cycle"))) {
            copy_str(sd->duty, v, sizeof(sd->duty));
        } else if((v = line_value(s, "protocol"))) {
            msg.protocol = infrared_get_protocol_by_name(v);
        } else if((v = line_value(s, "address"))) {
            msg.address = parse_hex_le(v);
        } else if((v = line_value(s, "command"))) {
            msg.command = parse_hex_le(v);
        } else if((v = line_value(s, "data"))) {
            bool mark = true;
            char* end;
            while(*v) {
                unsigned long d = strtoul(v, &end, 10);
                if(end == v) break;

                v = end;
                if(!push_duration(sd, mark ? (int32_t)d : -(int32_t)d)) break;

                mark = !mark;
            }
        }
    }

    furi_string_free(line);
    storage_file_close(f);
    storage_file_free(f);

    if(parsed && message_is_valid(&msg)) sd->synthesized = synthesize_message(&msg, sd);

    if(sd->data && sd->count > 0 && sd->count < sd->cap) {
        int16_t* shrunk = safe_realloc(sd->data, sd->count * sizeof(int16_t));
        if(shrunk) {
            sd->data = shrunk;
            sd->cap = sd->count;
        }
    }

    if(g_normalize_jitter) normalize_jitter(sd);

    recompute_total_us(sd);
    return sd->count >= 2;
}

#define JITTER_DATA_MIN_US 60
#define JITTER_DATA_MAX_US 3000
#define JITTER_FIT_ITERS   8

// Snap each pulse to the nearest multiple of a base unit Te found by an
// iterative least-squares fit |dur| ~ m*Te. Long marks/spaces are left alone.
static void normalize_jitter(IrData* sd) {
    if(!sd || !sd->data || sd->count < 8) return;

    int64_t sum_all = 0;
    size_t n_all = 0;
    for(size_t i = 0; i < sd->count; i++) {
        int32_t a = iabs32(sd->data[i]);
        if(a >= JITTER_DATA_MIN_US && a <= JITTER_DATA_MAX_US) {
            sum_all += a;
            n_all++;
        }
    }

    if(n_all == 0) return;

    int32_t mean_all = (int32_t)(sum_all / (int64_t)n_all);
    int64_t sum_short = 0;
    size_t n_short = 0;
    for(size_t i = 0; i < sd->count; i++) {
        int32_t a = iabs32(sd->data[i]);
        if(a >= JITTER_DATA_MIN_US && a <= mean_all) {
            sum_short += a;
            n_short++;
        }
    }

    float te = (n_short > 0) ? (float)sum_short / (float)n_short : (float)mean_all;
    if(te < 1.0f) return;

    for(int it = 0; it < JITTER_FIT_ITERS; it++) {
        float num = 0.0f, den = 0.0f;
        for(size_t i = 0; i < sd->count; i++) {
            int32_t a = iabs32(sd->data[i]);
            if(a < JITTER_DATA_MIN_US || a > JITTER_DATA_MAX_US) continue;

            int m = (int)((float)a / te + 0.5f);
            if(m < 1) m = 1;

            num += (float)m * (float)a;
            den += (float)m * (float)m;
        }
        if(den <= 0.0f) return;

        te = num / den;
    }

    for(size_t i = 0; i < sd->count; i++) {
        int32_t a = iabs32(sd->data[i]);
        if(a < JITTER_DATA_MIN_US || a > JITTER_DATA_MAX_US) continue;

        int m = (int)((float)a / te + 0.5f);
        if(m < 1) m = 1;

        int32_t mag = (int32_t)((float)m * te + 0.5f);
        if(mag > DUR_CLAMP) mag = DUR_CLAMP;

        sd->data[i] = (int16_t)(sd->data[i] < 0 ? -mag : mag);
    }
}

static void recompute_total_us(IrData* sd) {
    int64_t run = 0;
    for(size_t i = 0; i < sd->count; i++)
        run += iabs32(sd->data[i]);

    if(run > 0x7FFFFFFF) run = 0x7FFFFFFF;

    sd->total_us = (int32_t)run;
}

// Replace the previous signal's trailing space with exactly the configured gap.
static void merge_separator(IrData* dst) {
    while(dst->count > 0 && dst->data[dst->count - 1] < 0)
        dst->count--;

    push_duration(dst, -(g_merge_gap_ms * 1000));
}

typedef enum {
    MergeJoinNone, // first signal; nothing precedes it
    MergeJoinManual, // another signal: separate with the configured gap
    MergeJoinNative, // a repetition: keep the signal's own trailing space
} MergeJoin;

// Learned signals end on a mark, so a repetition without its own trailing
// space falls back to the configured gap. Returns true when the appended
// signal's leading space must be dropped.
static bool merge_prepare_join(IrData* dst, MergeJoin join) {
    if(join == MergeJoinNone || dst->count == 0) return false;

    if(join == MergeJoinManual || dst->data[dst->count - 1] > 0) merge_separator(dst);

    return true;
}

static void append_signal(IrData* dst, const IrData* sig, MergeJoin join) {
    bool skip_lead = merge_prepare_join(dst, join);
    for(size_t i = 0; i < sig->count; i++) {
        int32_t v = sig->data[i];
        if(skip_lead) {
            if(v < 0) continue;

            skip_lead = false;
        }

        if(!append_sample(dst, v)) break;
    }
}

static size_t repeated_samples(size_t count, size_t copies) {
    return (count + duration_samples(g_merge_gap_ms * 1000)) * copies;
}

static bool load_ir_repeated(Storage* storage, const char* path, uint32_t index, IrData* out) {
    IrData sig;
    bool ok = load_ir(storage, path, index, &sig);
    if(!ok || !sig.data || sig.count == 0) {
        *out = sig;
        return ok;
    }

    *out = sig;
    out->data = NULL;
    out->count = 0;
    out->cap = 0;

    size_t want = repeated_samples(sig.count, (size_t)g_merge_repeat);
    size_t need = want > MAX_SAMPLES ? MAX_SAMPLES : want;
    out->data = safe_malloc(need * sizeof(int16_t));
    if(!out->data) {
        out->out_of_memory = true;
        free(sig.data);
        return false;
    }
    out->cap = need;

    for(int r = 0; r < g_merge_repeat; r++)
        append_signal(out, &sig, r == 0 ? MergeJoinNone : MergeJoinNative);

    recompute_total_us(out);
    free(sig.data);
    return out->count >= 2;
}

static void propose_edit_name(Storage* st, App* a, char* out, size_t outlen) {
    FuriString* path = furi_string_alloc();
    int suffix = 1;
    while(true) {
        snprintf(out, outlen, "%s_edit%d", a->basename, suffix);
        furi_string_printf(path, "%s/%s" IR_EXT, IR_DIR, out);
        if(!storage_file_exists(st, furi_string_get_cstr(path))) break;

        suffix++;
        if(suffix > 999) {
            snprintf(out, outlen, "%s_edit", a->basename);
            break;
        }
    }

    furi_string_free(path);
}

// Samples whose middle lies in A..B, trimmed to start and end on a mark.
static bool selection_range(App* a, size_t* i0, size_t* i1) {
    int32_t lo = MIN(a->marker_a, a->marker_b);
    int32_t hi = MAX(a->marker_a, a->marker_b);

    int32_t run = 0;
    bool started = false;
    for(size_t i = 0; i < a->sd.count; i++) {
        int32_t ad = iabs32(a->sd.data[i]);
        int32_t mid = run + ad / 2;
        run += ad;
        if(mid >= lo && mid <= hi) {
            if(!started) *i0 = i;

            started = true;
            *i1 = i;
        }
    }

    if(!started) {
        snprintf(a->status, sizeof(a->status), "Nothing in A..B");
        return false;
    }

    while(*i0 < *i1 && a->sd.data[*i0] < 0)
        (*i0)++;

    while(*i1 > *i0 && a->sd.data[*i1] < 0)
        (*i1)--;

    if(*i0 >= *i1) {
        snprintf(a->status, sizeof(a->status), "Range too small");
        return false;
    }

    return true;
}

// Last sample of the level run starting at i; split samples are summed into *dur.
static size_t run_end(const IrData* sd, size_t i, size_t i1, uint32_t* dur) {
    *dur = 0;
    for(;; i++) {
        *dur += iabs32(sd->data[i]);
        if(i == i1 || (sd->data[i] < 0) != (sd->data[i + 1] < 0)) return i;
    }
}

static void cat_hex_le(FuriString* buf, const char* key, uint32_t v) {
    furi_string_cat_printf(
        buf,
        "%s: %02lX %02lX %02lX %02lX\n",
        key,
        (unsigned long)(v & 0xFF),
        (unsigned long)((v >> 8) & 0xFF),
        (unsigned long)((v >> 16) & 0xFF),
        (unsigned long)(v >> 24));
}

// Write A..B as a raw signal, or `msg` as a parsed one. Raw timings strictly
// alternate, starting and ending on a mark.
static bool write_ir(Storage* st, App* a, const char* savename, const InfraredMessage* msg) {
    size_t i0 = 0, i1 = 0;
    uint32_t dur;
    if(!msg) {
        if(!selection_range(a, &i0, &i1)) return false;

        size_t timings = 0;
        for(size_t i = i0; i <= i1; i++, timings++)
            i = run_end(&a->sd, i, i1, &dur);

        if(timings > MAX_TIMINGS_AMOUNT) {
            snprintf(
                a->status,
                sizeof(a->status),
                "Too long: %u/%u",
                (unsigned)timings,
                (unsigned)MAX_TIMINGS_AMOUNT);
            return false;
        }
    }

    FuriString* buf = furi_string_alloc_printf("%s/%s" IR_EXT, IR_DIR, savename);
    if(storage_file_exists(st, furi_string_get_cstr(buf))) {
        furi_string_free(buf);
        snprintf(a->status, sizeof(a->status), "Name exists");
        return false;
    }

    File* f = storage_file_alloc(st);
    if(!storage_file_open(f, furi_string_get_cstr(buf), FSAM_WRITE, FSOM_CREATE_NEW)) {
        storage_file_free(f);
        furi_string_free(buf);
        snprintf(a->status, sizeof(a->status), "Write failed");
        return false;
    }

    furi_string_printf(
        buf,
        "Filetype: IR signals file\nVersion: 1\n#\nname: %s\n",
        a->sd.name[0] ? a->sd.name : "Edited");

    if(msg) {
        furi_string_cat_printf(
            buf, "type: parsed\nprotocol: %s\n", infrared_get_protocol_name(msg->protocol));
        cat_hex_le(buf, "address", msg->address);
        cat_hex_le(buf, "command", msg->command);
    } else {
        furi_string_cat_printf(
            buf,
            "type: raw\nfrequency: %lu\nduty_cycle: %s\ndata:",
            (unsigned long)a->sd.frequency,
            a->sd.duty);

        for(size_t i = i0; i <= i1; i++) {
            i = run_end(&a->sd, i, i1, &dur);
            furi_string_cat_printf(buf, " %lu", (unsigned long)dur);

            if(furi_string_size(buf) >= 256) {
                storage_file_write(f, furi_string_get_cstr(buf), furi_string_size(buf));
                furi_string_reset(buf);
            }
        }

        furi_string_push_back(buf, '\n');
    }

    storage_file_write(f, furi_string_get_cstr(buf), furi_string_size(buf));

    furi_string_free(buf);
    storage_file_close(f);
    storage_file_free(f);

    snprintf(a->status, sizeof(a->status), "Saved %s" IR_EXT, savename);
    return true;
}

typedef struct {
    InfraredMessage msg;
    uint16_t frames;
    uint16_t repeats;
} Decoded;

// Consecutive identical frames and their repeat codes collapse into one entry.
static void decoded_add(Decoded* list, size_t* n, const InfraredMessage* m) {
    Decoded* last = *n ? &list[*n - 1] : NULL;
    if(last && last->msg.protocol == m->protocol && last->msg.address == m->address &&
       last->msg.command == m->command) {
        if(m->repeat)
            last->repeats++;
        else
            last->frames++;

        return;
    }

    if(*n >= DECODE_MAX) return;

    list[*n] = (Decoded){.msg = *m, .frames = !m->repeat, .repeats = m->repeat};
    list[*n].msg.repeat = false;
    (*n)++;
}

// Run A..B through the firmware IR decoders.
static size_t decode_selection(App* a, Decoded* list) {
    size_t i0 = 0, i1 = 0;
    if(!selection_range(a, &i0, &i1)) return 0;

    InfraredDecoderHandler* dec = infrared_alloc_decoder();
    infrared_reset_decoder(dec);

    size_t n = 0;
    uint32_t dur;
    for(size_t i = i0; i <= i1 + 2; i++) {
        const InfraredMessage* m;
        if(i <= i1) {
            i = run_end(&a->sd, i, i1, &dur);
            m = infrared_decode(dec, a->sd.data[i] > 0, dur);
        } else {
            m = (i == i1 + 1) ? infrared_decode(dec, false, DECODE_TAIL_US) :
                                infrared_check_decoder_ready(dec);
        }

        if(m) decoded_add(list, &n, m);
    }

    infrared_free_decoder(dec);

    if(!n) snprintf(a->status, sizeof(a->status), "No known protocol");

    return n;
}

static bool snapshot_take(App* a) {
    if(a->sd.count == 0) return false;

    size_t need = a->sd.count * sizeof(int16_t);
    int16_t* copy = safe_malloc(need);
    if(!copy) return false;

    memcpy(copy, a->sd.data, need);

    if(a->undo_data) free(a->undo_data);

    a->undo_data = copy;
    a->undo_count = a->sd.count;
    a->undo_total_us = a->sd.total_us;
    a->undo_marker_a = a->marker_a;
    a->undo_marker_b = a->marker_b;
    a->undo_active = a->active;
    a->undo_view_start = a->view_start;
    a->undo_view_end = a->view_end;
    a->has_undo = true;
    return true;
}

static void snapshot_restore(App* a) {
    if(!a->undo_data) return;

    free(a->sd.data);
    a->sd.data = a->undo_data;
    a->sd.count = a->undo_count;
    a->sd.cap = a->undo_count;
    a->sd.total_us = a->undo_total_us;
    a->marker_a = a->undo_marker_a;
    a->marker_b = a->undo_marker_b;
    a->active = a->undo_active;
    a->view_start = a->undo_view_start;
    a->view_end = a->undo_view_end;

    a->undo_data = NULL;
    a->has_undo = false;

    recompute_overview(a);
    recompute_activity(a);
}

static bool cut_compact(App* a) {
    int32_t lo = a->marker_a, hi = a->marker_b;
    if(lo > hi) {
        int32_t t = lo;
        lo = hi;
        hi = t;
    }

    int16_t* d = a->sd.data;
    size_t n = a->sd.count;
    size_t w = 0;
    int32_t run = 0;
    bool seam = false;
    for(size_t i = 0; i < n; i++) {
        int32_t v = d[i];
        int32_t ad = iabs32(v);
        int32_t s0 = run;
        run += ad;
        int32_t s1 = run;
        int sign = (v >= 0) ? 1 : -1;
        int32_t keep = 0;

        if(s0 < lo) keep += (s1 < lo ? s1 : lo) - s0;

        if(s1 > hi) keep += s1 - (s0 > hi ? s0 : hi);

        if(keep < ad) seam = true;

        if(keep <= 0) continue;

        int32_t val = sign * keep;
        // Join same-sign samples only across the cut seam; elsewhere they are
        // long spaces split on purpose.
        if(seam && w > 0 && ((d[w - 1] >= 0) == (val >= 0))) {
            int32_t sum = (int32_t)d[w - 1] + val;
            if(iabs32(sum) <= DUR_CLAMP) {
                d[w - 1] = (int16_t)sum;
            } else {
                d[w - 1] = (int16_t)(sign * DUR_CLAMP);
                d[w++] = (int16_t)(sum - sign * DUR_CLAMP);
            }
        } else {
            d[w++] = (int16_t)val;
        }

        if(keep == ad && s0 >= hi) seam = false;
    }

    a->sd.count = w;
    if(w < 2) return true;

    recompute_total_us(&a->sd);

    a->marker_a = lo;
    a->marker_b = lo;
    clamp_marker(a, 0);
    clamp_marker(a, 1);
    a->active = 0;

    int32_t span = a->view_end - a->view_start;
    if(span < 200) span = 200;

    a->view_start = a->marker_a - span / 2;
    a->view_end = a->view_start + span;

    clamp_view(a);

    recompute_overview(a);
    recompute_activity(a);
    return false;
}

static void draw_marker(Canvas* c, int x, bool active) {
    if(x < 0 || x > SCREEN_W_PX - 1) return;

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

    if(a->saving) {
        canvas_draw_str_aligned(c, 64, 34, AlignCenter, AlignCenter, "Saving...");
        furi_mutex_release(a->mutex);
        return;
    }

    char nm[15];
    copy_str(nm, a->sd.name[0] ? a->sd.name : a->basename, sizeof(nm));
    canvas_draw_str(c, 0, FONT_SIZE_PX, nm);

    char total_time_str[14];
    fmt_time(a->sd.total_us, total_time_str, sizeof(total_time_str));
    canvas_draw_str_aligned(
        c, SCREEN_W_PX - 1, FONT_SIZE_PX, AlignRight, AlignBottom, total_time_str);

    for(int x = 0; x < SCREEN_W_PX; x++) {
        if(a->overview[x] > a->ov_max / 6) canvas_draw_dot(c, x, OV_Y + OV_H - 1);
    }

    int32_t total = a->sd.total_us > 0 ? a->sd.total_us : 1;
    int vx0 = (int)(((int64_t)a->view_start * SCREEN_W_PX) / total);
    int vx1 = (int)(((int64_t)a->view_end * SCREEN_W_PX) / total);

    if(vx0 < 0) vx0 = 0;

    if(vx0 > SCREEN_W_PX - 1) vx0 = SCREEN_W_PX - 1;

    if(vx1 <= vx0) vx1 = vx0 + 1;

    if(vx1 > SCREEN_W_PX - 1) vx1 = SCREEN_W_PX - 1;

    canvas_draw_frame(c, vx0, OV_Y, vx1 - vx0 + 1, OV_H);
    canvas_draw_line(c, 0, SEP_Y, SCREEN_W_PX - 1, SEP_Y);

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

            if(x1 > SCREEN_W_PX - 1) x1 = SCREEN_W_PX - 1;

            int y = (a->sd.data[i] > 0) ? yhi : ylo;

            if(prev_x >= 0 && y != prev_y && x0 >= 0 && x0 <= SCREEN_W_PX - 1)
                canvas_draw_line(c, x0, yhi, x0, ylo);

            if(x1 >= x0) canvas_draw_line(c, x0, y, x1, y);

            prev_x = x1;
            prev_y = y;
        }
    } else {
        for(int x = 0; x < SCREEN_W_PX; x++) {
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

    if(xhi > SCREEN_W_PX - 1) xhi = SCREEN_W_PX - 1;

    for(int x = xlo; x <= xhi; x += 2)
        canvas_draw_dot(c, x, WAVE_TOP - 1);

    draw_marker(c, xa, a->active == 0);
    draw_marker(c, xb, a->active == 1);

    if(a->view_start > 0)
        canvas_draw_str_aligned(c, 0, (WAVE_TOP + WAVE_BOT) / 2, AlignLeft, AlignCenter, "<");

    if(a->view_end < a->sd.total_us || a->sd.truncated) {
        canvas_draw_str_aligned(
            c, SCREEN_W_PX - 1, (WAVE_TOP + WAVE_BOT) / 2, AlignRight, AlignCenter, ">");
    }

    char sa[14], sb[14], zb[14], sl[14];
    fmt_time(a->marker_a, sa, sizeof(sa));
    fmt_time(a->marker_b, sb, sizeof(sb));

    int32_t len = a->marker_b - a->marker_a;
    if(len < 0) len = -len;

    int32_t vis_lo = a->view_start < 0 ? 0 : a->view_start;
    int32_t vis_hi = a->view_end > a->sd.total_us ? a->sd.total_us : a->view_end;
    int32_t vis = vis_hi - vis_lo;
    if(vis < 0) vis = 0;

    fmt_time(vis, zb, sizeof(zb));
    fmt_time(len, sl, sizeof(sl));

    char l1[SCREEN_W_PX / 4];
    char l2[SCREEN_W_PX / 4];

    snprintf(l1, sizeof(l1), "%sA: %s", a->active == 0 ? ">" : "  ", sa);
    snprintf(l2, sizeof(l2), "%sB: %s", a->active == 1 ? ">" : "  ", sb);
    canvas_draw_str(c, 0, LINE1_BASE, l1);
    canvas_draw_str(c, 0, LINE2_BASE, l2);

    snprintf(l1, sizeof(l1), "View: %s", zb);
    snprintf(l2, sizeof(l2), "Marked: %s", sl);
    canvas_draw_str_aligned(c, SCREEN_W_PX - 1, 56, AlignRight, AlignBottom, l1);
    canvas_draw_str_aligned(
        c, SCREEN_W_PX - 1, 56 + FONT_SIZE_PX + 1, AlignRight, AlignBottom, l2);

    if(a->mode != EditModeMenu && a->status[0] && furi_get_tick() < a->status_until) {
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

    if(a->mode == EditModeMenu) {
        const int bx = 36, bw = 56, bh = MENU_ITEMS * 14 + 4, by = (SCREEN_H_PX - bh) / 2;
        canvas_set_color(c, ColorWhite);
        canvas_draw_box(c, bx, by, bw, bh);
        canvas_set_color(c, ColorBlack);
        canvas_draw_frame(c, bx, by, bw, bh);

        const char* labels[MENU_ITEMS] = {"Save", "Decode", "Cut", "Undo"};
        for(int i = 0; i < MENU_ITEMS; i++) {
            int ry = by + 2 + i * 14;
            int rh = 13;
            bool disabled = (i == MENU_SEL_UNDO && !a->has_undo);

            if(a->menu_sel == i) {
                canvas_set_color(c, ColorBlack);
                canvas_draw_box(c, bx + 2, ry, bw - 4, rh);
                canvas_set_color(c, ColorWhite);
            } else {
                canvas_set_color(c, ColorBlack);
            }

            int cx = bx + bw / 2;
            canvas_draw_str_aligned(c, cx, ry + rh / 2 + 1, AlignCenter, AlignCenter, labels[i]);

            if(disabled) canvas_draw_line(c, cx - 13, ry + rh / 2, cx + 13, ry + rh / 2);
        }

        canvas_set_color(c, ColorBlack);
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

typedef struct {
    ViewDispatcher* vd;
    bool confirmed;
    uint32_t index;
} PromptCtx;

static void prompt_result_cb(void* ctx) {
    PromptCtx* p = ctx;
    p->confirmed = true;
    view_dispatcher_stop(p->vd);
}

static void pick_index_cb(void* ctx, uint32_t index) {
    PromptCtx* p = ctx;
    p->index = index;
    prompt_result_cb(p);
}

static uint32_t prompt_back_cb(void* ctx) {
    UNUSED(ctx);
    return VIEW_NONE;
}

static bool prompt_run(Gui* gui, PromptCtx* pc, View* view) {
    pc->vd = view_dispatcher_alloc();
    view_set_previous_callback(view, prompt_back_cb);
    view_dispatcher_attach_to_gui(pc->vd, gui, ViewDispatcherTypeFullscreen);
    view_dispatcher_add_view(pc->vd, 0, view);
    view_dispatcher_switch_to_view(pc->vd, 0);
    view_dispatcher_run(pc->vd);
    view_dispatcher_remove_view(pc->vd, 0);
    view_dispatcher_free(pc->vd);
    return pc->confirmed;
}

static bool prompt_filename(Gui* gui, char* namebuf, size_t buflen) {
    PromptCtx pc = {0};
    TextInput* ti = text_input_alloc();
    text_input_set_header_text(ti, "Name the file");
    text_input_set_result_callback(ti, prompt_result_cb, &pc, namebuf, buflen, false);
    bool ok = prompt_run(gui, &pc, text_input_get_view(ti));
    text_input_free(ti);
    return ok;
}

// Let the user pick one of the file's signals. A file with a single signal
// is picked right away. Returns false when cancelled.
static bool
    pick_signal(Gui* gui, Storage* storage, const char* path, const char* title, uint32_t* index) {
    PromptCtx pc = {0};
    Submenu* sm = submenu_alloc();
    submenu_set_header(sm, title);

    File* f = storage_file_alloc(storage);
    uint32_t count = 0;
    if(storage_file_open(f, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        LineReader lr = {.file = f, .len = 0, .pos = 0, .eof = false};
        FuriString* line = furi_string_alloc();
        while(count < PICK_MAX_SIGNALS && lr_read_line(&lr, line)) {
            const char* v = line_value(furi_string_get_cstr(line), "name");
            if(v) submenu_add_item(sm, v, count++, pick_index_cb, &pc);
        }

        furi_string_free(line);
        storage_file_close(f);
    }
    storage_file_free(f);

    bool ok = true;
    *index = 0;
    if(count > 1) {
        ok = prompt_run(gui, &pc, submenu_get_view(sm));
        *index = pc.index;
    }

    submenu_free(sm);
    return ok;
}

static void prompt_and_save(
    Gui* gui,
    ViewPort* vp,
    FuriMessageQueue* queue,
    Storage* st,
    App* a,
    const InfraredMessage* msg) {
    char namebuf[64];
    propose_edit_name(st, a, namebuf, sizeof(namebuf));

    gui_remove_view_port(gui, vp);

    bool ok = prompt_filename(gui, namebuf, sizeof(namebuf));

    if(ok && namebuf[0] != '\0') {
        a->saving = true;
        gui_add_view_port(gui, vp, GuiLayerFullscreen);
        view_port_update(vp);

        write_ir(st, a, namebuf, msg);

        a->saving = false;
    } else {
        snprintf(a->status, sizeof(a->status), "Cancelled");
        gui_add_view_port(gui, vp, GuiLayerFullscreen);
    }

    a->status_until = furi_get_tick() + 2000;
    furi_message_queue_reset(queue);
    view_port_update(vp);
}

static int decoded_hex_digits(uint8_t bits) {
    return (bits + 3) / 4;
}

static void decoded_label(const Decoded* d, char* out, size_t n) {
    int len = snprintf(
        out,
        n,
        "%s %0*lX %0*lX",
        infrared_get_protocol_name(d->msg.protocol),
        decoded_hex_digits(infrared_get_protocol_address_length(d->msg.protocol)),
        (unsigned long)d->msg.address,
        decoded_hex_digits(infrared_get_protocol_command_length(d->msg.protocol)),
        (unsigned long)d->msg.command);

    if(d->frames > 1 && len > 0 && (size_t)len < n)
        snprintf(out + len, n - len, " x%u", (unsigned)d->frames);
}

// Details of one decoded entry. Returns true when Save was pressed.
static bool decoded_show(DialogsApp* dialogs, const Decoded* d) {
    char text[72];
    snprintf(
        text,
        sizeof(text),
        "Address: 0x%0*lX\nCommand: 0x%0*lX\nFrames: %u  Repeats: %u",
        decoded_hex_digits(infrared_get_protocol_address_length(d->msg.protocol)),
        (unsigned long)d->msg.address,
        decoded_hex_digits(infrared_get_protocol_command_length(d->msg.protocol)),
        (unsigned long)d->msg.command,
        (unsigned)d->frames,
        (unsigned)d->repeats);

    DialogMessage* m = dialog_message_alloc();
    dialog_message_set_header(
        m, infrared_get_protocol_name(d->msg.protocol), 64, 2, AlignCenter, AlignTop);
    dialog_message_set_text(m, text, 64, 30, AlignCenter, AlignCenter);
    dialog_message_set_buttons(m, "Back", NULL, "Save");
    DialogMessageButton b = dialog_message_show(dialogs, m);
    dialog_message_free(m);
    return b == DialogMessageButtonRight;
}

// List of decoded entries (skipped for a single one). Returns the entry to save or -1.
static int32_t decoded_browse(Gui* gui, DialogsApp* dialogs, const Decoded* list, size_t n) {
    uint32_t sel = 0;
    while(true) {
        if(n > 1) {
            PromptCtx pc = {0};
            Submenu* sm = submenu_alloc();
            char header[24];
            snprintf(header, sizeof(header), "Decoded: %u", (unsigned)n);
            submenu_set_header(sm, header);

            for(size_t i = 0; i < n; i++) {
                char label[48];
                decoded_label(&list[i], label, sizeof(label));
                submenu_add_item(sm, label, i, pick_index_cb, &pc);
            }

            submenu_set_selected_item(sm, sel);
            bool ok = prompt_run(gui, &pc, submenu_get_view(sm));
            submenu_free(sm);

            if(!ok) return -1;

            sel = pc.index;
        }

        if(decoded_show(dialogs, &list[sel])) return (int32_t)sel;

        if(n == 1) return -1;
    }
}

static void do_decode_action(
    Gui* gui,
    ViewPort* vp,
    FuriMessageQueue* queue,
    DialogsApp* dialogs,
    Storage* st,
    App* a) {
    Decoded* list = safe_malloc(DECODE_MAX * sizeof(Decoded));
    if(!list) {
        snprintf(a->status, sizeof(a->status), "Low memory");
        a->status_until = furi_get_tick() + 1500;
        return;
    }

    furi_mutex_acquire(a->mutex, FuriWaitForever);
    size_t n = decode_selection(a, list);
    if(!n) a->status_until = furi_get_tick() + 1500;
    furi_mutex_release(a->mutex);

    int32_t save = -1;
    if(n) {
        gui_remove_view_port(gui, vp);
        save = decoded_browse(gui, dialogs, list, n);
        gui_add_view_port(gui, vp, GuiLayerFullscreen);
        furi_message_queue_reset(queue);
    }

    if(save >= 0) prompt_and_save(gui, vp, queue, st, a, &list[save].msg);

    free(list);
}

static void
    do_cut_action(Gui* gui, ViewPort* vp, FuriMessageQueue* queue, DialogsApp* dialogs, App* a) {
    furi_mutex_acquire(a->mutex, FuriWaitForever);

    if(iabs32(a->marker_b - a->marker_a) < 1) {
        snprintf(a->status, sizeof(a->status), "Range too small");
        a->status_until = furi_get_tick() + 1500;
        furi_mutex_release(a->mutex);
        return;
    }

    if(!snapshot_take(a)) {
        snprintf(a->status, sizeof(a->status), "Low memory, no cut");
        a->status_until = furi_get_tick() + 1500;
        furi_mutex_release(a->mutex);
        return;
    }

    if(!cut_compact(a)) {
        snprintf(a->status, sizeof(a->status), "Cut");
        a->status_until = furi_get_tick() + 1200;
        furi_mutex_release(a->mutex);
        return;
    }

    furi_mutex_release(a->mutex);

    gui_remove_view_port(gui, vp);

    DialogMessage* m = dialog_message_alloc();
    dialog_message_set_header(m, "Cannot cut", 64, 2, AlignCenter, AlignTop);
    dialog_message_set_text(
        m, "That would leave the\nsignal empty.", 64, 30, AlignCenter, AlignCenter);
    dialog_message_set_buttons(m, NULL, NULL, "OK");
    dialog_message_show(dialogs, m);
    dialog_message_free(m);

    furi_mutex_acquire(a->mutex, FuriWaitForever);
    snapshot_restore(a);
    snprintf(a->status, sizeof(a->status), "Cut cancelled");
    a->status_until = furi_get_tick() + 1500;
    furi_mutex_release(a->mutex);

    gui_add_view_port(gui, vp, GuiLayerFullscreen);
    furi_message_queue_reset(queue);
}

static void run_editor_session(
    Gui* gui,
    ViewPort* vp,
    FuriMessageQueue* queue,
    Storage* storage,
    DialogsApp* dialogs,
    App* app,
    bool auto_select) {
    if(auto_select)
        auto_detect(app);
    else
        select_all(app);

    recompute_overview(app);
    recompute_activity(app);
    furi_message_queue_reset(queue);
    view_port_update(vp);

    bool running = true;
    InputEvent e;
    while(running) {
        if(furi_message_queue_get(queue, &e, 100) != FuriStatusOk) {
            view_port_update(vp);
            continue;
        }

        furi_mutex_acquire(app->mutex, FuriWaitForever);
        bool changed = false;
        bool want_save = false;
        bool want_cut = false;
        bool want_decode = false;

        if(app->mode == EditModeMenu) {
            if(e.type == InputTypeShort) {
                if(e.key == InputKeyDown) {
                    app->menu_sel = (app->menu_sel + 1) % MENU_ITEMS;
                } else if(e.key == InputKeyUp) {
                    app->menu_sel = (app->menu_sel + MENU_ITEMS - 1) % MENU_ITEMS;
                } else if(e.key == InputKeyOk) {
                    app->mode = EditModeEdit;
                    if(app->menu_sel == MENU_SEL_SAVE) {
                        want_save = true;
                    } else if(app->menu_sel == MENU_SEL_DECODE) {
                        want_decode = true;
                    } else if(app->menu_sel == MENU_SEL_CUT) {
                        want_cut = true;
                    } else if(app->menu_sel == MENU_SEL_UNDO) {
                        if(app->has_undo) {
                            snapshot_restore(app);
                            snprintf(app->status, sizeof(app->status), "Undone");
                        } else {
                            snprintf(app->status, sizeof(app->status), "Nothing to undo");
                        }

                        app->status_until = furi_get_tick() + 1500;
                    }
                } else if(e.key == InputKeyBack) {
                    app->mode = EditModeEdit;
                }
            }
        } else if(e.type == InputTypePress || e.type == InputTypeRepeat) {
            int32_t span = app->view_end - app->view_start;
            int32_t step = span / SCREEN_W_PX;
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
                ensure_visible(app, app->active ? app->marker_b : app->marker_a);
                changed = true;
            } else if(e.key == InputKeyBack) {
                running = false;
            }
        } else if(e.type == InputTypeLong) {
            if(e.key == InputKeyOk) {
                app->mode = EditModeMenu;
                app->menu_sel = 0;
            } else if(e.key == InputKeyBack) {
                running = false;
            }
        }

        if(changed) recompute_activity(app);

        furi_mutex_release(app->mutex);

        if(want_save)
            prompt_and_save(gui, vp, queue, storage, app, NULL);
        else if(want_decode)
            do_decode_action(gui, vp, queue, dialogs, storage, app);
        else if(want_cut)
            do_cut_action(gui, vp, queue, dialogs, app);

        view_port_update(vp);
    }
}

static void show_message(DialogsApp* dialogs, const char* header, const char* text) {
    DialogMessage* m = dialog_message_alloc();
    dialog_message_set_header(m, header, 64, 2, AlignCenter, AlignTop);
    dialog_message_set_text(m, text, 64, 34, AlignCenter, AlignCenter);
    dialog_message_set_buttons(m, NULL, NULL, "OK");
    dialog_message_show(dialogs, m);
    dialog_message_free(m);
}

static void show_oom_dialog(DialogsApp* dialogs, const char* text) {
    show_message(dialogs, "Out of memory", text);
}

static App* app_alloc(DialogsApp* dialogs, const char* what) {
    App* app = safe_malloc(sizeof(App));
    if(!app) {
        show_oom_dialog(dialogs, what);
        return NULL;
    }

    memset(app, 0, sizeof(App));
    app->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    return app;
}

static void app_free(App* app) {
    if(app->sd.data) free(app->sd.data);

    if(app->undo_data) free(app->undo_data);

    furi_mutex_free(app->mutex);
    free(app);
}

static void file_basename(const char* full, char* out, size_t n) {
    const char* slash = strrchr(full, '/');
    copy_str(out, slash ? slash + 1 : full, n);
    char* dot = strrchr(out, '.');
    if(dot) *dot = '\0';
}

static bool browse_ir(DialogsApp* dialogs, FuriString* path) {
    DialogsFileBrowserOptions br;
    dialog_file_browser_set_basic_options(&br, IR_EXT, &I_ir_10px);
    br.base_path = IR_DIR;
    return dialog_file_browser_show(dialogs, path, path, &br);
}

static void
    editor_view(Gui* gui, Storage* storage, DialogsApp* dialogs, App* app, bool auto_select) {
    ViewPort* vp = view_port_alloc();
    FuriMessageQueue* queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    view_port_draw_callback_set(vp, draw_cb, app);
    view_port_input_callback_set(vp, input_cb, queue);
    gui_add_view_port(gui, vp, GuiLayerFullscreen);

    run_editor_session(gui, vp, queue, storage, dialogs, app, auto_select);

    gui_remove_view_port(gui, vp);
    view_port_free(vp);
    furi_message_queue_free(queue);
}

static void loading_draw_cb(Canvas* c, void* ctx) {
    UNUSED(ctx);
    canvas_clear(c);
    canvas_set_font(c, FontPrimary);
    canvas_draw_str_aligned(c, 64, 32, AlignCenter, AlignCenter, "Loading...");
}

static void run_editor(Storage* storage, DialogsApp* dialogs) {
    App* app = app_alloc(dialogs, "Not enough RAM to\nopen the editor.\nReboot Flipper.");
    if(!app) return;

    Gui* gui = furi_record_open(RECORD_GUI);
    FuriString* path = furi_string_alloc_set(IR_DIR);
    uint32_t index;

    if(browse_ir(dialogs, path)) {
        const char* p = furi_string_get_cstr(path);
        file_basename(p, app->basename, sizeof(app->basename));

        if(pick_signal(gui, storage, p, app->basename, &index)) {
            ViewPort* load_vp = view_port_alloc();
            view_port_draw_callback_set(load_vp, loading_draw_cb, NULL);
            gui_add_view_port(gui, load_vp, GuiLayerFullscreen);
            view_port_update(load_vp);

            bool loaded = (g_merge_repeat > 1) ? load_ir_repeated(storage, p, index, &app->sd) :
                                                 load_ir(storage, p, index, &app->sd);

            gui_remove_view_port(gui, load_vp);
            view_port_free(load_vp);

            if(app->sd.out_of_memory)
                show_oom_dialog(
                    dialogs,
                    "Not enough free RAM to\nload this signal.\nReboot Flipper, then\nopen the app first.");
            else if(!loaded)
                show_message(dialogs, APP_NAME, "Unsupported protocol or signal is empty");
            else
                editor_view(gui, storage, dialogs, app, true);
        }
    }

    furi_string_free(path);
    furi_record_close(RECORD_GUI);
    app_free(app);
}

static DialogMessageButton merge_ask_more(DialogsApp* dialogs, int n, size_t total) {
    char msg[64];
    snprintf(msg, sizeof(msg), "Added %d signal(s).\n%u samples.", n, (unsigned)total);
    DialogMessage* m = dialog_message_alloc();
    dialog_message_set_header(m, "Merge .ir signals", 64, 2, AlignCenter, AlignTop);
    dialog_message_set_text(m, msg, 64, 32, AlignCenter, AlignCenter);
    dialog_message_set_buttons(m, "Done", NULL, "Add more");
    DialogMessageButton btn = dialog_message_show(dialogs, m);
    dialog_message_free(m);
    return btn;
}

static void run_merge(Storage* storage, DialogsApp* dialogs) {
    App* app = app_alloc(dialogs, "Not enough RAM to\nstart merging.\nReboot Flipper.");
    if(!app) return;

    // Packed list of picks: <len><path bytes><index lo><index hi>, ended by a 0 length.
    size_t pcap = MERGE_BUF_CHUNK;
    size_t pused = 0;
    uint8_t* pbuf = safe_malloc(pcap);
    if(!pbuf) {
        show_oom_dialog(dialogs, "Not enough RAM to\nstart merging.\nReboot Flipper.");
        app_free(app);
        return;
    }
    pbuf[0] = 0;

    Gui* gui = furi_record_open(RECORD_GUI);
    ViewPort* load_vp = view_port_alloc();
    view_port_draw_callback_set(load_vp, loading_draw_cb, NULL);

    int n = 0;
    size_t total = 0;
    bool aborted = false;
    uint32_t freq = DEFAULT_FREQUENCY;
    char duty[sizeof(app->sd.duty)];
    copy_str(duty, DEFAULT_DUTY, sizeof(duty));

    FuriString* path = furi_string_alloc_set(IR_DIR);
    char title[48];

    while(true) {
        if(!browse_ir(dialogs, path)) {
            if(n == 0) break;

            DialogMessageButton b = merge_ask_more(dialogs, n, total);
            if(b == DialogMessageButtonBack) {
                aborted = true;
                break;
            }

            if(b != DialogMessageButtonRight) break;

            continue;
        }

        const char* cstr = furi_string_get_cstr(path);
        uint32_t index;
        file_basename(cstr, title, sizeof(title));
        if(!pick_signal(gui, storage, cstr, title, &index)) continue;

        gui_add_view_port(gui, load_vp, GuiLayerFullscreen);
        view_port_update(load_vp);
        IrData sig;
        load_ir(storage, cstr, index, &sig);
        gui_remove_view_port(gui, load_vp);

        size_t cnt = sig.count;
        uint32_t f = sig.frequency;
        if(sig.data) free(sig.data);

        if(cnt < 2) {
            show_message(dialogs, "Unsupported signal", "Unsupported protocol or signal is empty");
            continue;
        }

        if(n > 0 && f != freq) {
            char msg[64];
            snprintf(
                msg,
                sizeof(msg),
                "Merge: %lu Hz\nSignal: %lu Hz",
                (unsigned long)freq,
                (unsigned long)f);
            show_message(dialogs, "Carrier mismatch", msg);
            continue;
        }

        size_t newtotal = total + repeated_samples(cnt, (size_t)g_merge_repeat);
        bool over_cap = newtotal > MAX_SAMPLES;
        bool over_ram = newtotal * sizeof(int16_t) + LOAD_HEAP_RESERVE > memmgr_get_free_heap();
        if(over_cap || over_ram) {
            char msg[64];
            snprintf(
                msg,
                sizeof(msg),
                "Have %u, +%u.\n%s %d.",
                (unsigned)total,
                (unsigned)cnt,
                over_cap ? "Limit" : "RAM fits ~",
                over_cap ? MAX_SAMPLES : (int)(memmgr_get_free_heap() / sizeof(int16_t)));
            show_message(dialogs, over_cap ? "Won't fit" : "Out of memory", msg);
            continue;
        }

        size_t plen = strlen(cstr);
        if(plen > MERGE_PATH_LEN - 1) plen = MERGE_PATH_LEN - 1;

        size_t need = pused + 1 + plen + 2 + 1;
        if(need > pcap) {
            size_t newcap = pcap;
            while(newcap < need)
                newcap *= 2;

            uint8_t* np = safe_realloc(pbuf, newcap);
            if(!np) {
                show_message(
                    dialogs,
                    "Merge .ir signals",
                    "Out of RAM for more\nsignals. Merging what\nwas added so far.");
                break;
            }

            pbuf = np;
            pcap = newcap;
        }

        pbuf[pused++] = (uint8_t)plen;
        memcpy(pbuf + pused, cstr, plen);
        pused += plen;
        pbuf[pused++] = (uint8_t)(index & 0xFF);
        pbuf[pused++] = (uint8_t)(index >> 8);
        pbuf[pused] = 0;

        if(n == 0) {
            freq = f;
            copy_str(duty, sig.duty, sizeof(duty));
        }

        total = newtotal;
        n++;

        DialogMessageButton b = merge_ask_more(dialogs, n, total);
        if(b == DialogMessageButtonBack) {
            aborted = true;
            break;
        }

        if(b != DialogMessageButtonRight) break;
    }

    furi_string_free(path);

    if(!aborted && n > 0 && total >= 2) {
        app->sd.data = safe_malloc(total * sizeof(int16_t));
        if(!app->sd.data) {
            show_oom_dialog(dialogs, "Not enough RAM to\nbuild the merge.\nReboot Flipper.");
        } else {
            app->sd.cap = total;
            app->sd.frequency = freq;
            copy_str(app->sd.duty, duty, sizeof(app->sd.duty));
            copy_str(app->sd.name, "Merged", sizeof(app->sd.name));
            copy_str(app->basename, "merged", sizeof(app->basename));

            gui_add_view_port(gui, load_vp, GuiLayerFullscreen);
            view_port_update(load_vp);

            char pathbuf[MERGE_PATH_LEN];
            size_t off = 0;
            for(int i = 0; i < n; i++) {
                size_t plen = pbuf[off++];
                memcpy(pathbuf, pbuf + off, plen);
                pathbuf[plen] = '\0';
                off += plen;
                uint32_t index = pbuf[off] | ((uint32_t)pbuf[off + 1] << 8);
                off += 2;

                IrData sig;
                if(load_ir(storage, pathbuf, index, &sig) && sig.data) {
                    app->sd.synthesized |= sig.synthesized;
                    for(int r = 0; r < g_merge_repeat; r++) {
                        MergeJoin join = (i == 0 && r == 0) ? MergeJoinNone :
                                         (r == 0)           ? MergeJoinManual :
                                                              MergeJoinNative;
                        append_signal(&app->sd, &sig, join);
                    }
                }

                if(sig.data) free(sig.data);
            }

            if(g_normalize_jitter) normalize_jitter(&app->sd);

            gui_remove_view_port(gui, load_vp);
            recompute_total_us(&app->sd);

            editor_view(gui, storage, dialogs, app, false);
        }
    }

    free(pbuf);
    view_port_free(load_vp);
    furi_record_close(RECORD_GUI);
    app_free(app);
}

typedef enum {
    MenuViewSubmenu,
    MenuViewAbout,
    MenuViewConfig,
    MenuViewNumInput,
} MenuViewId;

typedef enum {
    MenuItemSelectFile,
    MenuItemMerge,
    MenuItemConfig,
    MenuItemAbout,
} MenuItemId;

typedef enum {
    MenuActionNone = 0,
    MenuActionEdit,
    MenuActionMerge,
} MenuAction;

typedef struct {
    ViewDispatcher* view_dispatcher;
    Submenu* submenu;
    Widget* widget;
    VariableItemList* config_list;
    NumberInput* num_input;
    VariableItem* gap_item;
    VariableItem* repeat_item;
    int editing;
    Storage* storage;
    DialogsApp* dialogs;
    MenuAction action;
} Menu;

static void menu_submenu_cb(void* context, uint32_t index) {
    Menu* menu = context;
    if(index == MenuItemSelectFile) {
        menu->action = MenuActionEdit;
        view_dispatcher_stop(menu->view_dispatcher);
    } else if(index == MenuItemMerge) {
        menu->action = MenuActionMerge;
        view_dispatcher_stop(menu->view_dispatcher);
    } else if(index == MenuItemConfig) {
        view_dispatcher_switch_to_view(menu->view_dispatcher, MenuViewConfig);
    } else if(index == MenuItemAbout) {
        view_dispatcher_switch_to_view(menu->view_dispatcher, MenuViewAbout);
    }
}

static uint32_t to_submenu_cb(void* context) {
    UNUSED(context);
    return MenuViewSubmenu;
}

static void config_norm_changed_cb(VariableItem* item) {
    uint8_t idx = variable_item_get_current_value_index(item);
    g_normalize_jitter = (idx != 0);
    variable_item_set_current_value_text(item, idx ? "ON" : "OFF");
}

typedef enum {
    ConfigItemNormalize = 0,
    ConfigItemGap,
    ConfigItemRepeat,
} ConfigItem;

// A 3-slot stepper (down / hold / up) recentred on every change, so the gap
// can step by 1 ms across a range wider than a VariableItem's 255 values.
static void config_gap_sync_item(VariableItem* item) {
    variable_item_set_current_value_index(item, 1);

    char buf[8];
    snprintf(buf, sizeof(buf), "%ld ms", (long)g_merge_gap_ms);
    variable_item_set_current_value_text(item, buf);
}

static void config_repeat_sync_item(VariableItem* item) {
    char buf[8];
    snprintf(buf, sizeof(buf), "%ldx", (long)g_merge_repeat);
    variable_item_set_current_value_index(item, g_merge_repeat - MERGE_REPEAT_MIN);
    variable_item_set_current_value_text(item, buf);
}

static void config_gap_changed_cb(VariableItem* item) {
    uint8_t idx = variable_item_get_current_value_index(item);
    if(idx == 0 && g_merge_gap_ms > MERGE_GAP_MIN_MS)
        g_merge_gap_ms--;
    else if(idx == 2 && g_merge_gap_ms < MERGE_GAP_MAX_MS)
        g_merge_gap_ms++;

    config_gap_sync_item(item);
}

static void config_repeat_changed_cb(VariableItem* item) {
    g_merge_repeat = MERGE_REPEAT_MIN + variable_item_get_current_value_index(item);
    config_repeat_sync_item(item);
}

static void config_num_input_cb(void* context, int32_t v) {
    Menu* menu = context;

    if(menu->editing == ConfigItemGap) {
        g_merge_gap_ms = CLAMP(v, MERGE_GAP_MAX_MS, MERGE_GAP_MIN_MS);
        config_gap_sync_item(menu->gap_item);
    } else if(menu->editing == ConfigItemRepeat) {
        g_merge_repeat = CLAMP(v, MERGE_REPEAT_MAX, MERGE_REPEAT_MIN);
        config_repeat_sync_item(menu->repeat_item);
    }

    view_dispatcher_switch_to_view(menu->view_dispatcher, MenuViewConfig);
}

static uint32_t config_num_input_back_cb(void* context) {
    UNUSED(context);
    return MenuViewConfig;
}

static void config_enter_cb(void* context, uint32_t index) {
    Menu* menu = context;

    const char* header;
    int32_t current, min_val, max_val;
    if(index == ConfigItemGap) {
        header = "Merge gap [ms]";
        current = g_merge_gap_ms;
        min_val = MERGE_GAP_MIN_MS;
        max_val = MERGE_GAP_MAX_MS;
    } else if(index == ConfigItemRepeat) {
        header = "Repeat each";
        current = g_merge_repeat;
        min_val = MERGE_REPEAT_MIN;
        max_val = MERGE_REPEAT_MAX;
    } else {
        return;
    }

    menu->editing = index;
    number_input_set_header_text(menu->num_input, header);
    number_input_set_result_callback(
        menu->num_input, config_num_input_cb, menu, current, min_val, max_val);
    view_dispatcher_switch_to_view(menu->view_dispatcher, MenuViewNumInput);
}

static void menu_build_config(Menu* menu) {
    VariableItem* it = variable_item_list_add(
        menu->config_list, "Normalize jitter", 2, config_norm_changed_cb, menu);
    variable_item_set_current_value_index(it, g_normalize_jitter ? 1 : 0);
    variable_item_set_current_value_text(it, g_normalize_jitter ? "ON" : "OFF");

    menu->gap_item =
        variable_item_list_add(menu->config_list, "Merge gap", 3, config_gap_changed_cb, menu);
    config_gap_sync_item(menu->gap_item);

    menu->repeat_item = variable_item_list_add(
        menu->config_list,
        "Repeat",
        MERGE_REPEAT_MAX - MERGE_REPEAT_MIN + 1,
        config_repeat_changed_cb,
        menu);
    config_repeat_sync_item(menu->repeat_item);

    variable_item_list_set_enter_callback(menu->config_list, config_enter_cb, menu);
}

static uint32_t submenu_back_cb(void* context) {
    UNUSED(context);
    return VIEW_NONE;
}

static void menu_build_about(Menu* menu) {
    int title_h = FONT_SIZE_PX * 2;
    widget_add_text_box_element(
        menu->widget,
        0,
        0,
        SCREEN_W_PX,
        title_h,
        AlignCenter,
        AlignBottom,
        "\e#\e!     " APP_NAME "     \e!\n",
        false);

    int scroll_element_offset = title_h + GAP_PX;
    widget_add_text_scroll_element(
        menu->widget,
        0,
        scroll_element_offset,
        SCREEN_W_PX,
        SCREEN_H_PX - scroll_element_offset,
        "\e#Version " APP_VERSION "\n"
        "Enjoyed? Leave a star:\n"
        "" APP_REPO "\n"
        "\n"
        "A tiny on-device waveform\n"
        "editor for Flipper Zero that\n"
        "trims infrared .ir signals\n"
        "down to just the part\n"
        "you care about.\n"
        "\n"
        "Full documentation available on the GitHub.");
}

int32_t ir_raw_edit_app(void* p) {
    UNUSED(p);

    Menu* menu = safe_malloc(sizeof(Menu));
    if(!menu) return 0;

    memset(menu, 0, sizeof(Menu));

    menu->storage = furi_record_open(RECORD_STORAGE);
    menu->dialogs = furi_record_open(RECORD_DIALOGS);
    Gui* gui = furi_record_open(RECORD_GUI);

    menu->view_dispatcher = view_dispatcher_alloc();
    menu->submenu = submenu_alloc();
    menu->widget = widget_alloc();
    menu->config_list = variable_item_list_alloc();
    menu->num_input = number_input_alloc();

    submenu_set_header(menu->submenu, APP_NAME);
    submenu_add_item(menu->submenu, "Select .ir file", MenuItemSelectFile, menu_submenu_cb, menu);
    submenu_add_item(menu->submenu, "Merge .ir signals", MenuItemMerge, menu_submenu_cb, menu);
    submenu_add_item(menu->submenu, "Config", MenuItemConfig, menu_submenu_cb, menu);
    submenu_add_item(menu->submenu, "About", MenuItemAbout, menu_submenu_cb, menu);

    menu_build_about(menu);
    menu_build_config(menu);

    view_set_previous_callback(submenu_get_view(menu->submenu), submenu_back_cb);
    view_set_previous_callback(widget_get_view(menu->widget), to_submenu_cb);
    view_set_previous_callback(variable_item_list_get_view(menu->config_list), to_submenu_cb);
    view_set_previous_callback(number_input_get_view(menu->num_input), config_num_input_back_cb);

    view_dispatcher_attach_to_gui(menu->view_dispatcher, gui, ViewDispatcherTypeFullscreen);
    view_dispatcher_add_view(
        menu->view_dispatcher, MenuViewSubmenu, submenu_get_view(menu->submenu));
    view_dispatcher_add_view(menu->view_dispatcher, MenuViewAbout, widget_get_view(menu->widget));
    view_dispatcher_add_view(
        menu->view_dispatcher, MenuViewConfig, variable_item_list_get_view(menu->config_list));
    view_dispatcher_add_view(
        menu->view_dispatcher, MenuViewNumInput, number_input_get_view(menu->num_input));

    bool running = true;
    while(running) {
        menu->action = MenuActionNone;
        view_dispatcher_switch_to_view(menu->view_dispatcher, MenuViewSubmenu);
        view_dispatcher_run(menu->view_dispatcher);

        if(menu->action == MenuActionEdit)
            run_editor(menu->storage, menu->dialogs);
        else if(menu->action == MenuActionMerge)
            run_merge(menu->storage, menu->dialogs);
        else
            running = false;
    }

    view_dispatcher_remove_view(menu->view_dispatcher, MenuViewSubmenu);
    view_dispatcher_remove_view(menu->view_dispatcher, MenuViewAbout);
    view_dispatcher_remove_view(menu->view_dispatcher, MenuViewConfig);
    view_dispatcher_remove_view(menu->view_dispatcher, MenuViewNumInput);
    submenu_free(menu->submenu);
    widget_free(menu->widget);
    variable_item_list_free(menu->config_list);
    number_input_free(menu->num_input);
    view_dispatcher_free(menu->view_dispatcher);

    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_DIALOGS);
    furi_record_close(RECORD_STORAGE);

    free(menu);
    return 0;
}
