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
#include <gui/modules/text_input.h>

#include "subghz_raw_edit_icons.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SUBGHZ_DIR "/ext/subghz"
#define MAX_SAMPLES 24000
#define DUR_CLAMP 32000
#define LOAD_HEAP_RESERVE 12288

#define KEELOQ_TE_US 400
#define KEELOQ_GUARD_US 1500
#define KEELOQ_PREAMBLE_PULSES 12
#define KEELOQ_HEADER_GAP_TE 10
#define KEELOQ_MAX_KEY_BYTES 16

#define MERGE_GAP_US 15000
#define MERGE_MAX_FILES 16
#define MERGE_PATH_LEN 128

#define APP_VERSION "1.4"
#define APP_REPO "github.com/Lechnio/SubGHz-RAW-Edit"

#define SCREEN_W_PX 128
#define SCREEN_H_PX 64
#define FONT_SIZE_PX 7
#define GAP_PX 1

#define TOP_TEXT_BASE FONT_SIZE_PX
#define OV_Y (TOP_TEXT_BASE + GAP_PX)
#define OV_H 5
#define WAVE_H 32
#define WAVE_TOP (OV_Y + OV_H + 1)
#define WAVE_BOT (WAVE_TOP + WAVE_H)
#define SEP_Y (WAVE_BOT + 1)
#define LINE1_BASE (SEP_Y + 1 + GAP_PX + FONT_SIZE_PX)
#define LINE2_BASE (LINE1_BASE + 1 + FONT_SIZE_PX)

static_assert(LINE2_BASE <= SCREEN_H_PX);

#define MENU_SEL_SAVE 0
#define MENU_SEL_CUT 1
#define MENU_SEL_UNDO 2

typedef struct
{
    int16_t *data;
    size_t count;
    size_t cap;
    int32_t total_us;
    uint32_t frequency;
    char preset[48];
    bool truncated;
    bool out_of_memory;
} SubData;

typedef enum
{
    EditModeEdit = 0,
    EditModeMenu = 1,
} EditMode;

typedef struct
{
    SubData sd;
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

    bool loading;
    bool saving;

    int mode;
    int menu_sel;

    int16_t *undo_data;
    size_t undo_count;
    int32_t undo_total_us;
    int32_t undo_marker_a;
    int32_t undo_marker_b;
    int undo_active;
    int32_t undo_view_start;
    int32_t undo_view_end;
    bool has_undo;

    FuriMutex *mutex;
} App;

static inline int32_t iabs32(int32_t v)
{
    return v < 0 ? -v : v;
}

static void fmt_time(int32_t us, char *out, size_t n)
{
    if (us < 0)
        us = 0;

    if (us < 1000)
    {
        snprintf(out, n, "%luus", (unsigned long)us);
    }
    else if (us < 1000000)
    {
        long ms = us / 1000;
        long t = (us % 1000) / 100;
        snprintf(out, n, "%ld.%ldms", ms, t);
    }
    else
    {
        long s = us / 1000000;
        long h = (us % 1000000) / 10000;
        snprintf(out, n, "%ld.%02lds", s, h);
    }
}

static int time_to_x(App *a, int32_t t)
{
    int32_t span = a->view_end - a->view_start;
    if (span <= 0)
        span = 1;

    return (int)(((int64_t)(t - a->view_start) * SCREEN_W_PX) / span);
}

static void clamp_marker(App *a, int idx)
{
    int32_t *m = idx ? &a->marker_b : &a->marker_a;
    if (*m < 0)
        *m = 0;
    if (*m > a->sd.total_us)
        *m = a->sd.total_us;
}

static void clamp_view(App *a)
{
    int32_t total = a->sd.total_us;
    if (total < 1)
        total = 1;

    int32_t margin = total / 3 + 50000;
    int32_t lo = -margin;
    int32_t hi = total + margin;
    int32_t fullspan = hi - lo;

    int32_t span = a->view_end - a->view_start;
    if (span < 200)
        span = 200;

    if (span > fullspan)
        span = fullspan;

    if (a->view_start < lo)
        a->view_start = lo;

    a->view_end = a->view_start + span;

    if (a->view_end > hi)
    {
        a->view_end = hi;
        a->view_start = hi - span;
        if (a->view_start < lo)
            a->view_start = lo;
    }
}

static void ensure_visible(App *a, int32_t m)
{
    int32_t span = a->view_end - a->view_start;
    if (m < a->view_start)
    {
        a->view_start = m - span / 5;
        a->view_end = a->view_start + span;
    }
    else if (m > a->view_end)
    {
        a->view_end = m + span / 5;
        a->view_start = a->view_end - span;
    }

    clamp_view(a);
}

static void recompute_activity(App *a)
{
    memset(a->activity, 0, sizeof(a->activity));
    int32_t span = a->view_end - a->view_start;
    if (span <= 0)
        span = 1;

    a->wave_mode = (span <= 30000);

    int32_t run = 0;
    uint16_t mx = 0;
    for (size_t i = 0; i < a->sd.count; i++)
    {
        int32_t ad = iabs32(a->sd.data[i]);
        int32_t s0 = run;
        run += ad;
        int32_t s1 = run;

        if (s1 < a->view_start || s0 > a->view_end)
            continue;

        int x = (int)(((int64_t)(s0 - a->view_start) * SCREEN_W_PX) / span);

        if (x < 0)
            x = 0;

        if (x > SCREEN_W_PX - 1)
            x = SCREEN_W_PX - 1;

        if (a->activity[x] < 65535)
            a->activity[x]++;

        if (a->activity[x] > mx)
            mx = a->activity[x];
    }
    a->act_max = mx ? mx : 1;
}

static void recompute_overview(App *a)
{
    memset(a->overview, 0, sizeof(a->overview));
    int32_t total = a->sd.total_us;
    if (total < 1)
        total = 1;

    int32_t run = 0;
    uint16_t mx = 0;
    for (size_t i = 0; i < a->sd.count; i++)
    {
        int32_t ad = iabs32(a->sd.data[i]);
        int32_t s0 = run;
        run += ad;
        int x = (int)(((int64_t)s0 * SCREEN_W_PX) / total);

        if (x < 0)
            x = 0;

        if (x > SCREEN_W_PX - 1)
            x = SCREEN_W_PX - 1;

        if (a->overview[x] < 65535)
            a->overview[x]++;

        if (a->overview[x] > mx)
            mx = a->overview[x];
    }
    a->ov_max = mx ? mx : 1;
}

static void auto_detect(App *a)
{
    int32_t total = a->sd.total_us;
    if (total < 1 || a->sd.count < 2)
    {
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
    int32_t best_gap = 0;

    for (size_t i = 0; i < a->sd.count; i++)
    {
        int32_t ad = iabs32(a->sd.data[i]);
        int32_t s0 = run;
        run += ad;
        int32_t s1 = run;

        if (ad >= GAP_US)
        {
            if (seg_start >= 0)
            {
                int32_t seglen = last_edge - seg_start;
                if (seglen >= MIN_FRAME_US && seglen > best_len)
                {
                    best_len = seglen;
                    best_a = seg_start;
                    best_b = last_edge;
                    best_gap = ad;
                }

                seg_start = -1;
            }
        }
        else
        {
            if (seg_start < 0)
                seg_start = s0;

            last_edge = s1;
        }
    }

    if (seg_start >= 0)
    {
        int32_t seglen = last_edge - seg_start;
        if (seglen >= MIN_FRAME_US && seglen > best_len)
        {
            best_a = seg_start;
            best_b = last_edge;
            best_gap = 0;
        }
    }

    if (best_a < 0)
    {
        a->marker_a = 0;
        a->marker_b = total;
        a->view_start = 0;
        a->view_end = total;
        clamp_view(a);

        return;
    }

    /* Extend B into the trailing inter-frame gap so the auto-selected frame
     * keeps the end-of-frame silence that decoders (KeeLoq, ...) require to
     * finalize a frame. Without it the selection ends on the last bit and the
     * saved capture won't decode (it had to be widened by hand). Any positive
     * extension is enough: write_selection emits the whole gap sample once the
     * marker lands inside it. Limit it to KEELOQ_GUARD_US so the marker/view
     * stay tidy and never reach into the next frame. */
    int32_t gap_keep = best_gap;
    if (gap_keep > KEELOQ_GUARD_US)
        gap_keep = KEELOQ_GUARD_US;

    int32_t pad = (best_b - best_a) / 6 + 500;
    a->marker_a = best_a;
    a->marker_b = best_b + gap_keep;
    a->view_start = best_a - pad * 3;
    a->view_end = a->marker_b + pad * 3;
    clamp_view(a);
}

typedef struct
{
    File *file;
    uint8_t buf[128];
    size_t len;
    size_t pos;
    bool eof;
} LineReader;

static bool lr_read_line(LineReader *lr, FuriString *out)
{
    furi_string_reset(out);
    bool any = false;
    while (true)
    {
        if (lr->pos >= lr->len)
        {
            if (lr->eof)
                break;

            lr->len = storage_file_read(lr->file, lr->buf, sizeof(lr->buf));
            lr->pos = 0;
            if (lr->len == 0)
            {
                lr->eof = true;
                break;
            }
        }

        char c = (char)lr->buf[lr->pos++];
        if (c == '\n')
        {
            any = true;
            break;
        }

        if (c == '\r')
            continue;

        furi_string_push_back(out, c);
        any = true;
    }

    return any;
}

static bool append_sample(SubData *sd, int32_t v)
{
    if (sd->count >= sd->cap)
    {
        sd->truncated = true;
        return false;
    }

    if (v > DUR_CLAMP)
        v = DUR_CLAMP;

    if (v < -DUR_CLAMP)
        v = -DUR_CLAMP;

    sd->data[sd->count++] = (int16_t)v;

    return true;
}

typedef enum
{
    SubFormatRaw,
    SubFormatKeeloq,
} SubFormat;

static bool is_hex_digit(char c)
{
    return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
}

static size_t parse_hex_bytes(const char *s, uint8_t *out, size_t max_bytes)
{
    size_t n = 0;
    while (*s && n < max_bytes)
    {
        while (*s == ' ' || *s == '\t')
            s++;

        if (!s[0] || !s[1])
            break;

        if (!is_hex_digit(s[0]) || !is_hex_digit(s[1]))
            break;

        char hex[3] = {s[0], s[1], '\0'};
        out[n++] = (uint8_t)strtoul(hex, NULL, 16);
        s += 2;
    }

    return n;
}

static void *safe_malloc(size_t size)
{
    if (size == 0 || memmgr_get_free_heap() < size + LOAD_HEAP_RESERVE)
        return NULL;

    return malloc(size);
}

static void *safe_realloc(void *ptr, size_t size)
{
    if (size == 0 || memmgr_get_free_heap() < size + LOAD_HEAP_RESERVE)
        return NULL;

    return realloc(ptr, size);
}

static bool synthesize_keeloq(SubData *sd, const uint8_t *key, size_t key_len, uint16_t bit_count)
{
    if (key_len == 0 || bit_count == 0 || bit_count > key_len * 8)
        return false;

    size_t needed = (KEELOQ_PREAMBLE_PULSES * 2) + ((size_t)bit_count * 2) + 2;
    if (needed > MAX_SAMPLES)
        return false;

    if (sd->cap < needed)
    {
        int16_t *grown = safe_realloc(sd->data, needed * sizeof(int16_t));
        if (!grown)
        {
            sd->out_of_memory = true;
            return false;
        }

        sd->data = grown;
        sd->cap = needed;
    }

    sd->count = 0;
    sd->truncated = false;

    for (int i = 0; i < KEELOQ_PREAMBLE_PULSES; i++)
    {
        if (!append_sample(sd, KEELOQ_TE_US))
            return false;

        if (!append_sample(sd, -KEELOQ_TE_US))
            return false;
    }

    sd->data[sd->count - 1] = -(int16_t)(KEELOQ_HEADER_GAP_TE * KEELOQ_TE_US);

    /* Data bits, MSB-first across the byte stream (byte 0 = MSB of the Key).
     *
     * PWM polarity (verified by diffing a Flipper-native Elmes RAW against this
     * output): Flipper reads a SHORT high pulse as bit "1" and a LONG high pulse
     * as bit "0". Emitting the reverse made every synthesized frame decode to
     * the bit-complement of the key, so Flipper saw a valid 64-bit KeeLoq frame
     * but no manufacturer ever matched ("Unknown").
     *   bit "1" = Te ON, 2*Te OFF   (short high)
     *   bit "0" = 2*Te ON, Te OFF   (long high) */
    for (uint16_t b = 0; b < bit_count; b++)
    {
        size_t byte_idx = b / 8;
        size_t bit_in_byte = 7 - (b % 8);
        bool is_one = (key[byte_idx] >> bit_in_byte) & 1;
        if (is_one)
        {
            if (!append_sample(sd, KEELOQ_TE_US))
                return false;

            if (!append_sample(sd, -2 * KEELOQ_TE_US))
                return false;
        }
        else
        {
            if (!append_sample(sd, 2 * KEELOQ_TE_US))
                return false;

            if (!append_sample(sd, -KEELOQ_TE_US))
                return false;
        }
    }

    /* End-of-frame guard. Flipper's KeeLoq decoder only declares success when
     * it sees a timing mismatch *after* accumulating 64 bits. A trailing short
     * ON followed by a long OFF (the classic 39*Te HCS300 guard) lands the
     * decoder in CheckDuration, fails to match either bit pattern, and
     * triggers the count-check that emits the "found" callback. */
    if (!append_sample(sd, KEELOQ_TE_US))
        return false;

    if (!append_sample(sd, -(int32_t)KEELOQ_GUARD_US))
        return false;

    return true;
}

static bool load_sub(Storage *storage, const char *path, SubData *sd)
{
    memset(sd, 0, sizeof(*sd));
    sd->frequency = 433920000;
    strncpy(sd->preset, "FuriHalSubGhzPresetOok650Async", sizeof(sd->preset) - 1);

    File *f = storage_file_alloc(storage);
    if (!storage_file_open(f, path, FSAM_READ, FSOM_OPEN_EXISTING))
    {
        storage_file_free(f);
        return false;
    }

    uint64_t fsize = storage_file_size(f);

    /* Predict sample count from file weight. Measured RAW .sub captures run
     * ~4.4-5.4 bytes per sample (number text + sign + space), so fsize/4 is a
     * tight upper bound: well under the old fsize/2 (~2x) over-allocation, yet
     * still above the real count so normal captures aren't truncated. Decoded
     * files (KeeLoq etc.) are tiny; synthesize_keeloq grows the buffer if it
     * needs more. */
    size_t est = (size_t)(fsize / 4) + 64;
    if (est > MAX_SAMPLES)
        est = MAX_SAMPLES;

    sd->data = safe_malloc(est * sizeof(int16_t));
    if (!sd->data)
    {
        sd->out_of_memory = true;
        storage_file_close(f);
        storage_file_free(f);
        return false;
    }

    sd->cap = est;

    char protocol[32] = {0};
    uint16_t bit_count = 0;
    uint8_t key_bytes[KEELOQ_MAX_KEY_BYTES];
    size_t key_len = 0;

    LineReader lr = {.file = f, .len = 0, .pos = 0, .eof = false};
    FuriString *line = furi_string_alloc();
    bool stop = false;

    while (!stop && lr_read_line(&lr, line))
    {
        const char *s = furi_string_get_cstr(line);
        if (strncmp(s, "RAW_Data:", 9) == 0)
        {
            const char *p = s + 9;
            char *end;
            while (*p)
            {
                long v = strtol(p, &end, 10);
                if (end == p)
                {
                    if (*p == '\0')
                        break;

                    p++;
                    continue;
                }

                p = end;
                if (!append_sample(sd, (int32_t)v))
                {
                    stop = true;
                    break;
                }
            }
        }
        else if (strncmp(s, "Frequency:", 10) == 0)
        {
            sd->frequency = (uint32_t)strtoul(s + 10, NULL, 10);
        }
        else if (strncmp(s, "Preset:", 7) == 0)
        {
            const char *p = s + 7;
            while (*p == ' ')
                p++;

            strncpy(sd->preset, p, sizeof(sd->preset) - 1);
            sd->preset[sizeof(sd->preset) - 1] = '\0';
        }
        else if (strncmp(s, "Protocol:", 9) == 0)
        {
            const char *p = s + 9;
            while (*p == ' ')
                p++;

            strncpy(protocol, p, sizeof(protocol) - 1);
            protocol[sizeof(protocol) - 1] = '\0';
        }
        else if (strncmp(s, "Bit:", 4) == 0)
        {
            bit_count = (uint16_t)strtoul(s + 4, NULL, 10);
        }
        else if (strncmp(s, "Key:", 4) == 0)
        {
            const char *p = s + 4;
            while (*p == ' ')
                p++;

            key_len = parse_hex_bytes(p, key_bytes, KEELOQ_MAX_KEY_BYTES);
        }
    }

    furi_string_free(line);
    storage_file_close(f);
    storage_file_free(f);

    if (sd->count == 0 && key_len > 0 && bit_count > 0)
    {
        if (strcmp(protocol, "KeeLoq") == 0)
        {
            synthesize_keeloq(sd, key_bytes, key_len, bit_count);
        }
    }

    if (sd->data && sd->count > 0 && sd->count < sd->cap)
    {
        int16_t *shrunk = safe_realloc(sd->data, sd->count * sizeof(int16_t));
        if (shrunk)
        {
            sd->data = shrunk;
            sd->cap = sd->count;
        }
    }

    int64_t run = 0;
    for (size_t i = 0; i < sd->count; i++)
        run += iabs32(sd->data[i]);

    if (run > 0x7FFFFFFF)
        run = 0x7FFFFFFF;

    sd->total_us = (int32_t)run;
    return sd->count >= 2;
}

static void recompute_total_us(SubData *sd)
{
    int64_t run = 0;
    for (size_t i = 0; i < sd->count; i++)
        run += iabs32(sd->data[i]);

    if (run > 0x7FFFFFFF)
        run = 0x7FFFFFFF;

    sd->total_us = (int32_t)run;
}

static void merge_push(SubData *dst, int32_t v)
{
    if (dst->count > 0 && ((dst->data[dst->count - 1] < 0) == (v < 0)))
    {
        int32_t merged = (int32_t)dst->data[dst->count - 1] + v;

        if (merged > DUR_CLAMP)
            merged = DUR_CLAMP;

        if (merged < -DUR_CLAMP)
            merged = -DUR_CLAMP;

        dst->data[dst->count - 1] = (int16_t)merged;
    }
    else
    {
        append_sample(dst, v);
    }
}

static size_t count_sub_samples(
    Storage *storage,
    const char *path,
    uint32_t *freq,
    char *preset,
    size_t preset_sz,
    bool *is_raw)
{
    *is_raw = true;

    File *f = storage_file_alloc(storage);
    if (!storage_file_open(f, path, FSAM_READ, FSOM_OPEN_EXISTING))
    {
        storage_file_free(f);
        return 0;
    }

    LineReader lr = {.file = f, .len = 0, .pos = 0, .eof = false};
    FuriString *line = furi_string_alloc();
    size_t raw_count = 0;
    char protocol[32] = {0};
    uint16_t bit_count = 0;
    uint8_t key_bytes[KEELOQ_MAX_KEY_BYTES];
    size_t key_len = 0;

    while (lr_read_line(&lr, line))
    {
        const char *s = furi_string_get_cstr(line);
        if (strncmp(s, "RAW_Data:", 9) == 0)
        {
            const char *p = s + 9;
            char *end;
            while (*p)
            {
                strtol(p, &end, 10);
                if (end == p)
                {
                    if (*p == '\0')
                        break;

                    p++;
                    continue;
                }

                p = end;
                raw_count++;
            }
        }
        else if (strncmp(s, "Frequency:", 10) == 0)
        {
            *freq = (uint32_t)strtoul(s + 10, NULL, 10);
        }
        else if (strncmp(s, "Preset:", 7) == 0)
        {
            const char *q = s + 7;
            while (*q == ' ')
                q++;

            strncpy(preset, q, preset_sz - 1);
            preset[preset_sz - 1] = '\0';
        }
        else if (strncmp(s, "Protocol:", 9) == 0)
        {
            const char *q = s + 9;
            while (*q == ' ')
                q++;

            strncpy(protocol, q, sizeof(protocol) - 1);
            protocol[sizeof(protocol) - 1] = '\0';
        }
        else if (strncmp(s, "Bit:", 4) == 0)
        {
            bit_count = (uint16_t)strtoul(s + 4, NULL, 10);
        }
        else if (strncmp(s, "Key:", 4) == 0)
        {
            const char *q = s + 4;
            while (*q == ' ')
                q++;

            key_len = parse_hex_bytes(q, key_bytes, KEELOQ_MAX_KEY_BYTES);
        }
    }

    furi_string_free(line);
    storage_file_close(f);
    storage_file_free(f);

    if (raw_count > 0)
    {
        *is_raw = true;
        return raw_count;
    }

    /* Decoded KeeLoq: predict the synthesized frame length. Must stay in sync
     * with synthesize_keeloq (preamble pairs + data-bit pairs + 2-sample guard)
     * and its validity guard (bit_count <= key_len * 8). */
    if (strcmp(protocol, "KeeLoq") == 0 && key_len > 0 && bit_count > 0 &&
        bit_count <= key_len * 8)
    {
        *is_raw = false;
        return (size_t)KEELOQ_PREAMBLE_PULSES * 2 + (size_t)bit_count * 2 + 2;
    }

    return 0;
}

static void fill_raw_into(Storage *storage, const char *path, SubData *dst, bool add_separator)
{
    File *f = storage_file_alloc(storage);
    if (!storage_file_open(f, path, FSAM_READ, FSOM_OPEN_EXISTING))
    {
        storage_file_free(f);
        return;
    }

    LineReader lr = {.file = f, .len = 0, .pos = 0, .eof = false};
    FuriString *line = furi_string_alloc();

    if (add_separator)
        merge_push(dst, -MERGE_GAP_US);

    while (lr_read_line(&lr, line))
    {
        const char *s = furi_string_get_cstr(line);
        if (strncmp(s, "RAW_Data:", 9) != 0)
            continue;

        const char *p = s + 9;
        char *end;
        while (*p)
        {
            long v = strtol(p, &end, 10);
            if (end == p)
            {
                if (*p == '\0')
                    break;

                p++;
                continue;
            }
            p = end;
            merge_push(dst, (int32_t)v);
        }
    }

    furi_string_free(line);
    storage_file_close(f);
    storage_file_free(f);
}

static void fill_sub_into(
    Storage *storage, const char *path, SubData *dst, bool is_raw, bool add_separator)
{
    if (is_raw)
    {
        fill_raw_into(storage, path, dst, add_separator);
        return;
    }

    SubData tmp;
    bool ok = load_sub(storage, path, &tmp);
    if (ok && tmp.data)
    {
        if (add_separator)
            merge_push(dst, -MERGE_GAP_US);
        for (size_t i = 0; i < tmp.count; i++)
            merge_push(dst, tmp.data[i]);
    }
    if (tmp.data)
        free(tmp.data);
}

static void propose_edit_name(Storage *st, App *a, char *out, size_t outlen)
{
    FuriString *path = furi_string_alloc();
    int suffix = 1;
    while (true)
    {
        snprintf(out, outlen, "%s_edit%d", a->basename, suffix);
        furi_string_printf(path, "%s/%s.sub", SUBGHZ_DIR, out);
        if (!storage_file_exists(st, furi_string_get_cstr(path)))
            break;

        suffix++;
        if (suffix > 999)
        {
            snprintf(out, outlen, "%s_edit", a->basename);
            break;
        }
    }
    furi_string_free(path);
}

static bool write_selection(Storage *st, App *a, const char *savename)
{
    int32_t lo = a->marker_a, hi = a->marker_b;
    if (lo > hi)
    {
        int32_t t = lo;
        lo = hi;
        hi = t;
    }

    int32_t run = 0;
    size_t i0 = 0, i1 = 0;
    bool started = false;
    for (size_t i = 0; i < a->sd.count; i++)
    {
        int32_t ad = iabs32(a->sd.data[i]);
        int32_t s0 = run;
        run += ad;
        int32_t s1 = run;
        if (s1 > lo && s0 < hi)
        {
            if (!started)
            {
                i0 = i;
                started = true;
            }
            i1 = i;
        }
    }

    if (!started)
    {
        snprintf(a->status, sizeof(a->status), "Nothing in A..B");
        return false;
    }

    while (i0 < i1 && a->sd.data[i0] < 0)
        i0++;

    while (i1 > i0 && a->sd.data[i1] > 0)
        i1--;

    if (i0 >= i1)
    {
        snprintf(a->status, sizeof(a->status), "Range too small");
        return false;
    }

    FuriString *path = furi_string_alloc();
    furi_string_printf(path, "%s/%s.sub", SUBGHZ_DIR, savename);

    if (storage_file_exists(st, furi_string_get_cstr(path)))
    {
        furi_string_free(path);
        snprintf(a->status, sizeof(a->status), "Name exists");
        return false;
    }

    File *f = storage_file_alloc(st);
    if (!storage_file_open(f, furi_string_get_cstr(path), FSAM_WRITE, FSOM_CREATE_NEW))
    {
        storage_file_free(f);
        furi_string_free(path);
        snprintf(a->status, sizeof(a->status), "Write failed");
        return false;
    }

    char hdr[192];
    int n = snprintf(hdr, sizeof(hdr),
                     "Filetype: Flipper SubGhz RAW File\nVersion: 1\nFrequency: %lu\nPreset: %s\nProtocol: RAW\n",
                     (unsigned long)a->sd.frequency, a->sd.preset);

    storage_file_write(f, hdr, n);

    FuriString *lbuf = furi_string_alloc();
    furi_string_set(lbuf, "RAW_Data:");

    int cnt = 0;
    for (size_t i = i0; i <= i1; i++)
    {
        char vb[16];
        snprintf(vb, sizeof(vb), " %ld", (long)a->sd.data[i]);
        furi_string_cat_str(lbuf, vb);
        if (++cnt >= 512)
        {
            furi_string_push_back(lbuf, '\n');
            storage_file_write(f, furi_string_get_cstr(lbuf), furi_string_size(lbuf));
            furi_string_set(lbuf, "RAW_Data:");
            cnt = 0;
        }
    }

    if (cnt > 0)
    {
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

static bool snapshot_take(App *a)
{
    if (a->sd.count == 0)
        return false;

    size_t need = a->sd.count * sizeof(int16_t);
    int16_t *copy = safe_malloc(need);
    if (!copy)
        return false;

    memcpy(copy, a->sd.data, need);

    if (a->undo_data)
        free(a->undo_data);

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

static void snapshot_restore(App *a)
{
    if (!a->undo_data)
        return;

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

static bool cut_compact(App *a)
{
    int32_t lo = a->marker_a, hi = a->marker_b;
    if (lo > hi)
    {
        int32_t t = lo;
        lo = hi;
        hi = t;
    }

    int16_t *d = a->sd.data;
    size_t n = a->sd.count;
    size_t w = 0;
    int32_t run = 0;
    for (size_t i = 0; i < n; i++)
    {
        int32_t v = d[i];
        int32_t ad = iabs32(v);
        int32_t s0 = run;
        run += ad;
        int32_t s1 = run;
        int sign = (v >= 0) ? 1 : -1;
        int32_t keep = 0;

        if (s0 < lo)
            keep += (s1 < lo ? s1 : lo) - s0;

        if (s1 > hi)
            keep += s1 - (s0 > hi ? s0 : hi);

        if (keep <= 0)
            continue;

        int32_t val = sign * keep;
        if (w > 0 && ((d[w - 1] >= 0) == (val >= 0)))
        {
            int32_t merged = (int32_t)d[w - 1] + val;

            if (merged > DUR_CLAMP)
                merged = DUR_CLAMP;

            if (merged < -DUR_CLAMP)
                merged = -DUR_CLAMP;

            d[w - 1] = (int16_t)merged;
        }
        else
        {
            if (val > DUR_CLAMP)
                val = DUR_CLAMP;

            if (val < -DUR_CLAMP)
                val = -DUR_CLAMP;

            d[w++] = (int16_t)val;
        }
    }

    a->sd.count = w;
    if (w < 2)
        return true;

    int64_t tot = 0;
    for (size_t i = 0; i < w; i++)
        tot += iabs32(d[i]);

    if (tot > 0x7FFFFFFF)
        tot = 0x7FFFFFFF;

    a->sd.total_us = (int32_t)tot;

    a->marker_a = lo;
    a->marker_b = lo;
    clamp_marker(a, 0);
    clamp_marker(a, 1);
    a->active = 0;

    int32_t span = a->view_end - a->view_start;
    if (span < 200)
        span = 200;

    a->view_start = a->marker_a - span / 2;
    a->view_end = a->view_start + span;

    clamp_view(a);

    recompute_overview(a);
    recompute_activity(a);
    return false;
}

static void draw_marker(Canvas *c, int x, bool active)
{
    if (x < 0 || x > SCREEN_W_PX - 1)
        return;

    if (active)
    {
        for (int y = WAVE_TOP; y <= WAVE_BOT; y++)
            canvas_draw_dot(c, x, y);

        canvas_draw_box(c, x - 1, WAVE_TOP - 4, 3, 3);
    }
    else
    {
        for (int y = WAVE_TOP; y <= WAVE_BOT; y += 2)
            canvas_draw_dot(c, x, y);
    }
}

static void draw_cb(Canvas *c, void *ctx)
{
    App *a = ctx;
    furi_mutex_acquire(a->mutex, FuriWaitForever);

    canvas_clear(c);
    canvas_set_color(c, ColorBlack);
    canvas_set_font(c, FontSecondary);

    if (a->loading)
    {
        canvas_draw_str_aligned(c, SCREEN_H_PX, 28, AlignCenter, AlignCenter, "Loading...");
        char ln[20];
        strncpy(ln, a->basename, sizeof(ln) - 1);
        ln[sizeof(ln) - 1] = '\0';
        canvas_draw_str_aligned(c, 64, 40, AlignCenter, AlignCenter, ln);
        furi_mutex_release(a->mutex);
        return;
    }

    if (a->saving)
    {
        canvas_draw_str_aligned(c, 64, 34, AlignCenter, AlignCenter, "Saving...");
        furi_mutex_release(a->mutex);
        return;
    }

    char nm[15];
    strncpy(nm, a->basename, sizeof(nm) - 1);
    nm[sizeof(nm) - 1] = '\0';
    canvas_draw_str(c, 0, FONT_SIZE_PX, nm);

    char total_time_str[14];
    fmt_time(a->sd.total_us, total_time_str, sizeof(total_time_str));
    canvas_draw_str_aligned(c, SCREEN_W_PX - 1, FONT_SIZE_PX, AlignRight, AlignBottom, total_time_str);

    for (int x = 0; x < SCREEN_W_PX; x++)
    {
        if (a->overview[x] > a->ov_max / 6)
            canvas_draw_dot(c, x, OV_Y + OV_H - 1);
    }

    int32_t total = a->sd.total_us > 0 ? a->sd.total_us : 1;
    int vx0 = (int)(((int64_t)a->view_start * SCREEN_W_PX) / total);
    int vx1 = (int)(((int64_t)a->view_end * SCREEN_W_PX) / total);

    if (vx0 < 0)
        vx0 = 0;

    if (vx0 > SCREEN_W_PX - 1)
        vx0 = SCREEN_W_PX - 1;

    if (vx1 <= vx0)
        vx1 = vx0 + 1;

    if (vx1 > SCREEN_W_PX - 1)
        vx1 = SCREEN_W_PX - 1;

    canvas_draw_frame(c, vx0, OV_Y, vx1 - vx0 + 1, OV_H);
    canvas_draw_line(c, 0, SEP_Y, SCREEN_W_PX - 1, SEP_Y);

    if (a->wave_mode)
    {
        int yhi = WAVE_TOP + 2;
        int ylo = WAVE_BOT - 2;

        int32_t run = 0;
        int prev_x = -1;
        int prev_y = ylo;

        for (size_t i = 0; i < a->sd.count; i++)
        {
            int32_t ad = iabs32(a->sd.data[i]);
            int32_t t0 = run;
            run += ad;
            int32_t t1 = run;

            if (t1 < a->view_start || t0 > a->view_end)
                continue;

            int x0 = time_to_x(a, t0);
            int x1 = time_to_x(a, t1);

            if (x0 < 0)
                x0 = 0;

            if (x1 > SCREEN_W_PX - 1)
                x1 = SCREEN_W_PX - 1;

            int y = (a->sd.data[i] > 0) ? yhi : ylo;

            if (prev_x >= 0 && y != prev_y && x0 >= 0 && x0 <= SCREEN_W_PX - 1)
                canvas_draw_line(c, x0, yhi, x0, ylo);

            if (x1 >= x0)
                canvas_draw_line(c, x0, y, x1, y);

            prev_x = x1;
            prev_y = y;
        }
    }
    else
    {
        for (int x = 0; x < SCREEN_W_PX; x++)
        {
            if (a->activity[x] == 0)
                continue;

            int h = (int)(((int32_t)a->activity[x] * (WAVE_BOT - WAVE_TOP - 1)) / a->act_max);
            if (h < 1)
                h = 1;

            canvas_draw_line(c, x, WAVE_BOT, x, WAVE_BOT - h);
        }
    }

    int xa = time_to_x(a, a->marker_a);
    int xb = time_to_x(a, a->marker_b);
    int xlo = xa < xb ? xa : xb;
    int xhi = xa < xb ? xb : xa;

    if (xlo < 0)
        xlo = 0;

    if (xhi > SCREEN_W_PX - 1)
        xhi = SCREEN_W_PX - 1;

    for (int x = xlo; x <= xhi; x += 2)
        canvas_draw_dot(c, x, WAVE_TOP - 1);

    draw_marker(c, time_to_x(a, a->marker_a), a->active == 0);
    draw_marker(c, time_to_x(a, a->marker_b), a->active == 1);

    if (a->view_start > 0)
        canvas_draw_str_aligned(c, 0, (WAVE_TOP + WAVE_BOT) / 2, AlignLeft, AlignCenter, "<");

    if (a->view_end < a->sd.total_us || a->sd.truncated)
        canvas_draw_str_aligned(
            c, SCREEN_W_PX - 1, (WAVE_TOP + WAVE_BOT) / 2, AlignRight, AlignCenter, ">");

    char sa[14], sb[14], zb[14], sl[14];
    fmt_time(a->marker_a, sa, sizeof(sa));
    fmt_time(a->marker_b, sb, sizeof(sb));

    int32_t len = a->marker_b - a->marker_a;
    if (len < 0)
        len = -len;

    int32_t vis_lo = a->view_start < 0 ? 0 : a->view_start;
    int32_t vis_hi = a->view_end > a->sd.total_us ? a->sd.total_us : a->view_end;
    int32_t vis = vis_hi - vis_lo;
    if (vis < 0)
        vis = 0;

    fmt_time(vis, zb, sizeof(zb));
    fmt_time(len, sl, sizeof(sl));

    char l1[SCREEN_W_PX / 4];
    char l2[SCREEN_W_PX / 4];

    snprintf(l1, sizeof(l1), "%sA: %s", a->active == 0 ? ">" : "  ", sa);
    snprintf(l2, sizeof(l2), "%sB: %s", a->active == 1 ? ">" : "  ", sb);
    canvas_draw_str(c, 0, LINE1_BASE, l1);
    canvas_draw_str(c, 0, LINE2_BASE, l2);

    snprintf(l1, sizeof(l1), "View: %s", zb);
    snprintf(l2, sizeof(l2), "Selected: %s", sl);
    canvas_draw_str_aligned(c, SCREEN_W_PX - 1, 56, AlignRight, AlignBottom, l1);
    canvas_draw_str_aligned(c, SCREEN_W_PX - 1, 56 + FONT_SIZE_PX + 1, AlignRight, AlignBottom, l2);

    if (a->mode != EditModeMenu && a->status[0] && furi_get_tick() < a->status_until)
    {
        int w = 124;
        canvas_set_color(c, ColorWhite);
        canvas_draw_box(c, 2, 22, w, 22);
        canvas_set_color(c, ColorBlack);
        canvas_draw_frame(c, 2, 22, w, 22);

        const char *msg = a->status;
        if (strncmp(msg, "Saved ", 6) == 0)
        {
            canvas_draw_str_aligned(c, 64, 29, AlignCenter, AlignCenter, "Saved");
            canvas_draw_str_aligned(c, 64, 38, AlignCenter, AlignCenter, msg + 6);
        }
        else
        {
            canvas_draw_str_aligned(c, 64, 33, AlignCenter, AlignCenter, msg);
        }
    }

    if (a->mode == EditModeMenu)
    {
        const int bx = 36, by = 13, bw = 56, bh = 46;
        canvas_set_color(c, ColorWhite);
        canvas_draw_box(c, bx, by, bw, bh);
        canvas_set_color(c, ColorBlack);
        canvas_draw_frame(c, bx, by, bw, bh);

        const char *labels[3] = {"Save", "Cut", "Undo"};
        for (int i = 0; i < 3; i++)
        {
            int ry = by + 2 + i * 14;
            int rh = 13;
            bool disabled = (i == 2 && !a->has_undo);

            if (a->menu_sel == i)
            {
                canvas_set_color(c, ColorBlack);
                canvas_draw_box(c, bx + 2, ry, bw - 4, rh);
                canvas_set_color(c, ColorWhite);
            }
            else
            {
                canvas_set_color(c, ColorBlack);
            }

            int cx = bx + bw / 2;
            canvas_draw_str_aligned(c, cx, ry + rh / 2 + 1, AlignCenter, AlignCenter, labels[i]);

            if (disabled)
                canvas_draw_line(c, cx - 13, ry + rh / 2, cx + 13, ry + rh / 2);
        }

        canvas_set_color(c, ColorBlack);
    }

    furi_mutex_release(a->mutex);
}

static void input_cb(InputEvent *e, void *ctx)
{
    FuriMessageQueue *q = ctx;
    furi_message_queue_put(q, e, FuriWaitForever);
}

static void zoom(App *a, bool in)
{
    int32_t m = a->active ? a->marker_b : a->marker_a;
    int32_t span = a->view_end - a->view_start;
    int32_t nspan = in ? (span * 2 / 3) : (span * 3 / 2);
    if (nspan < 200)
        nspan = 200;

    int32_t fullspan = a->sd.total_us + 2 * (a->sd.total_us / 3 + 50000);
    if (nspan > fullspan)
        nspan = fullspan;

    if (nspan < 1)
        nspan = 1;

    int32_t off = (int32_t)(((int64_t)(m - a->view_start) * nspan) / (span > 0 ? span : 1));
    a->view_start = m - off;
    a->view_end = a->view_start + nspan;
    clamp_view(a);
}

typedef struct
{
    ViewDispatcher *vd;
    bool confirmed;
} PromptCtx;

static void prompt_result_cb(void *ctx)
{
    PromptCtx *p = ctx;
    p->confirmed = true;
    view_dispatcher_stop(p->vd);
}

static uint32_t prompt_back_cb(void *ctx)
{
    UNUSED(ctx);
    return VIEW_NONE;
}

static bool prompt_filename(Gui *gui, char *namebuf, size_t buflen)
{
    PromptCtx pc = {.vd = NULL, .confirmed = false};
    ViewDispatcher *vd = view_dispatcher_alloc();
    pc.vd = vd;

    TextInput *ti = text_input_alloc();
    text_input_set_header_text(ti, "Name the file");
    text_input_set_result_callback(ti, prompt_result_cb, &pc, namebuf, buflen, false);
    view_set_previous_callback(text_input_get_view(ti), prompt_back_cb);

    view_dispatcher_attach_to_gui(vd, gui, ViewDispatcherTypeFullscreen);
    view_dispatcher_add_view(vd, 0, text_input_get_view(ti));
    view_dispatcher_switch_to_view(vd, 0);
    view_dispatcher_run(vd);

    view_dispatcher_remove_view(vd, 0);
    text_input_free(ti);
    view_dispatcher_free(vd);

    return pc.confirmed;
}

static void prompt_and_save(Gui *gui, ViewPort *vp, FuriMessageQueue *queue, Storage *st, App *a)
{
    char namebuf[64];
    propose_edit_name(st, a, namebuf, sizeof(namebuf));

    gui_remove_view_port(gui, vp);

    bool ok = prompt_filename(gui, namebuf, sizeof(namebuf));

    if (ok && namebuf[0] != '\0')
    {
        a->saving = true;
        gui_add_view_port(gui, vp, GuiLayerFullscreen);
        view_port_update(vp);

        write_selection(st, a, namebuf);

        a->saving = false;
    }
    else
    {
        snprintf(a->status, sizeof(a->status), "Cancelled");
        gui_add_view_port(gui, vp, GuiLayerFullscreen);
    }

    a->status_until = furi_get_tick() + 2000;
    furi_message_queue_reset(queue);
    view_port_update(vp);
}

static void do_cut_action(Gui *gui, ViewPort *vp, FuriMessageQueue *queue, DialogsApp *dialogs, App *a)
{
    furi_mutex_acquire(a->mutex, FuriWaitForever);

    int32_t lo = a->marker_a, hi = a->marker_b;
    if (lo > hi)
    {
        int32_t t = lo;
        lo = hi;
        hi = t;
    }

    if (hi - lo < 1)
    {
        snprintf(a->status, sizeof(a->status), "Range too small");
        a->status_until = furi_get_tick() + 1500;
        furi_mutex_release(a->mutex);
        return;
    }

    if (!snapshot_take(a))
    {
        snprintf(a->status, sizeof(a->status), "Low memory, no cut");
        a->status_until = furi_get_tick() + 1500;
        furi_mutex_release(a->mutex);
        return;
    }

    bool emptied = cut_compact(a);
    if (!emptied)
    {
        snprintf(a->status, sizeof(a->status), "Cut");
        a->status_until = furi_get_tick() + 1200;
        furi_mutex_release(a->mutex);
        return;
    }

    furi_mutex_release(a->mutex);

    gui_remove_view_port(gui, vp);

    DialogMessage *m = dialog_message_alloc();
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
    Gui *gui,
    ViewPort *vp,
    FuriMessageQueue *queue,
    Storage *storage,
    DialogsApp *dialogs,
    App *app,
    bool auto_select)
{
    app->loading = false;
    if (auto_select)
    {
        auto_detect(app);
    }
    else
    {
        app->marker_a = 0;
        app->marker_b = app->sd.total_us;
        app->view_start = 0;
        app->view_end = app->sd.total_us > 0 ? app->sd.total_us : 1;
        clamp_view(app);
    }
    recompute_overview(app);
    recompute_activity(app);
    furi_message_queue_reset(queue);
    view_port_update(vp);

    bool running = true;
    InputEvent e;
    while (running)
    {
        if (furi_message_queue_get(queue, &e, 100) == FuriStatusOk)
        {
            furi_mutex_acquire(app->mutex, FuriWaitForever);
            bool changed = false;
            bool want_save = false;
            bool want_cut = false;

            if (app->mode == EditModeMenu)
            {
                if (e.type == InputTypeShort)
                {
                    if (e.key == InputKeyDown)
                    {
                        app->menu_sel = (app->menu_sel + 1) % 3;
                    }
                    else if (e.key == InputKeyUp)
                    {
                        app->menu_sel = (app->menu_sel + 2) % 3;
                    }
                    else if (e.key == InputKeyOk)
                    {
                        app->mode = EditModeEdit;
                        if (app->menu_sel == MENU_SEL_SAVE)
                        {
                            want_save = true;
                        }
                        else if (app->menu_sel == MENU_SEL_CUT)
                        {
                            want_cut = true;
                        }
                        else if (app->menu_sel == MENU_SEL_UNDO)
                        {
                            if (app->has_undo)
                            {
                                snapshot_restore(app);
                                snprintf(app->status, sizeof(app->status), "Undone");
                            }
                            else
                            {
                                snprintf(app->status, sizeof(app->status), "Nothing to undo");
                            }
                            app->status_until = furi_get_tick() + 1500;
                        }
                    }
                    else if (e.key == InputKeyBack)
                    {
                        app->mode = EditModeEdit;
                    }
                }
            }
            else if (e.type == InputTypePress || e.type == InputTypeRepeat)
            {
                int32_t span = app->view_end - app->view_start;
                int32_t step = span / SCREEN_W_PX;
                if (step < 1)
                    step = 1;

                if (e.type == InputTypeRepeat)
                {
                    int32_t fast = span / 24;
                    if (fast > step)
                        step = fast;
                }

                int32_t *m = app->active ? &app->marker_b : &app->marker_a;
                switch (e.key)
                {
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
            }
            else if (e.type == InputTypeShort)
            {
                if (e.key == InputKeyOk)
                {
                    app->active ^= 1;
                    ensure_visible(app, app->active ? app->marker_b : app->marker_a);
                    changed = true;
                }
                else if (e.key == InputKeyBack)
                {
                    running = false;
                }
            }
            else if (e.type == InputTypeLong)
            {
                if (e.key == InputKeyOk)
                {
                    app->mode = EditModeMenu;
                    app->menu_sel = 0;
                }
                else if (e.key == InputKeyBack)
                {
                    running = false;
                }
            }

            if (changed)
                recompute_activity(app);

            furi_mutex_release(app->mutex);

            if (want_save)
            {
                prompt_and_save(gui, vp, queue, storage, app);
            }
            else if (want_cut)
            {
                do_cut_action(gui, vp, queue, dialogs, app);
            }

            view_port_update(vp);
        }
        else
        {
            view_port_update(vp);
        }
    }
}

static void show_oom_dialog(DialogsApp *dialogs, const char *text)
{
    DialogMessage *m = dialog_message_alloc();
    dialog_message_set_header(m, "Out of memory", 64, 2, AlignCenter, AlignTop);
    dialog_message_set_text(m, text, 64, 32, AlignCenter, AlignCenter);
    dialog_message_set_buttons(m, NULL, NULL, "OK");
    dialog_message_show(dialogs, m);
    dialog_message_free(m);
}

static void run_editor(Storage *storage, DialogsApp *dialogs)
{
    App *app = safe_malloc(sizeof(App));
    if (!app)
    {
        show_oom_dialog(dialogs, "Not enough RAM to\nopen the editor.\nReboot Flipper.");
        return;
    }

    memset(app, 0, sizeof(App));
    app->mutex = furi_mutex_alloc(FuriMutexTypeNormal);

    FuriString *path = furi_string_alloc_set(SUBGHZ_DIR);
    DialogsFileBrowserOptions br;
    dialog_file_browser_set_basic_options(&br, ".sub", &I_sub1_10px);
    br.base_path = SUBGHZ_DIR;
    bool picked = dialog_file_browser_show(dialogs, path, path, &br);

    if (!picked)
        goto cleanup;

    {
        const char *full = furi_string_get_cstr(path);
        const char *slash = strrchr(full, '/');
        const char *nm = slash ? slash + 1 : full;
        strncpy(app->basename, nm, sizeof(app->basename) - 1);
        char *dot = strrchr(app->basename, '.');
        if (dot)
            *dot = '\0';
    }

    Gui *gui = furi_record_open(RECORD_GUI);
    ViewPort *vp = view_port_alloc();
    FuriMessageQueue *queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    view_port_draw_callback_set(vp, draw_cb, app);
    view_port_input_callback_set(vp, input_cb, queue);
    app->loading = true;
    gui_add_view_port(gui, vp, GuiLayerFullscreen);
    view_port_update(vp);

    bool loaded = load_sub(storage, furi_string_get_cstr(path), &app->sd);

    if (app->sd.out_of_memory || !loaded)
    {
        gui_remove_view_port(gui, vp);
        view_port_free(vp);
        furi_message_queue_free(queue);
        furi_record_close(RECORD_GUI);

        DialogMessage *m = dialog_message_alloc();
        if (app->sd.out_of_memory)
        {
            dialog_message_set_header(m, "Out of memory", 64, 2, AlignCenter, AlignTop);
            dialog_message_set_text(
                m,
                "Not enough free RAM to\nload this capture.\nReboot Flipper, then\nopen the app first.",
                64,
                34,
                AlignCenter,
                AlignCenter);
        }
        else
        {
            dialog_message_set_header(m, "Sub-GHz RAW Edit", 64, 4, AlignCenter, AlignTop);
            dialog_message_set_text(
                m, "Unsupported protocol or file is empty", 64, 32, AlignCenter, AlignCenter);
        }

        dialog_message_set_buttons(m, NULL, NULL, "OK");
        dialog_message_show(dialogs, m);
        dialog_message_free(m);
        goto cleanup;
    }

    run_editor_session(gui, vp, queue, storage, dialogs, app, true);

    gui_remove_view_port(gui, vp);
    view_port_free(vp);
    furi_message_queue_free(queue);
    furi_record_close(RECORD_GUI);

cleanup:
    furi_string_free(path);

    if (app->sd.data)
        free(app->sd.data);

    if (app->undo_data)
        free(app->undo_data);

    furi_mutex_free(app->mutex);
    free(app);
}

static void merge_loading_draw_cb(Canvas *c, void *ctx)
{
    UNUSED(ctx);
    canvas_clear(c);
    canvas_set_font(c, FontPrimary);
    canvas_draw_str_aligned(c, 64, 32, AlignCenter, AlignCenter, "Loading...");
}

static DialogMessageButton merge_ask_more(DialogsApp *dialogs, int n, size_t total)
{
    char msg[64];
    snprintf(msg, sizeof(msg), "Added %d file(s).\n%u samples.", n, (unsigned)total);
    DialogMessage *m = dialog_message_alloc();
    dialog_message_set_header(m, "Merge .sub files", 64, 2, AlignCenter, AlignTop);
    dialog_message_set_text(m, msg, 64, 32, AlignCenter, AlignCenter);
    dialog_message_set_buttons(m, "Done", NULL, "Add more");
    DialogMessageButton btn = dialog_message_show(dialogs, m);
    dialog_message_free(m);
    return btn;
}

static void run_merge(Storage *storage, DialogsApp *dialogs)
{
    App *app = safe_malloc(sizeof(App));
    if (!app)
    {
        show_oom_dialog(dialogs, "Not enough RAM to\nstart merging.\nReboot Flipper.");
        return;
    }

    memset(app, 0, sizeof(App));
    app->mutex = furi_mutex_alloc(FuriMutexTypeNormal);

    char (*paths)[MERGE_PATH_LEN] = safe_malloc(MERGE_MAX_FILES * MERGE_PATH_LEN);
    bool is_raw_arr[MERGE_MAX_FILES];
    if (!paths)
    {
        show_oom_dialog(dialogs, "Not enough RAM to\nstart merging.\nReboot Flipper.");
        furi_mutex_free(app->mutex);
        free(app);
        return;
    }

    Gui *gui = furi_record_open(RECORD_GUI);
    ViewPort *load_vp = view_port_alloc();
    view_port_draw_callback_set(load_vp, merge_loading_draw_cb, NULL);

    int n = 0;
    size_t total = 0;
    bool aborted = false;
    uint32_t freq = 433920000;
    char preset[48];
    strncpy(preset, "FuriHalSubGhzPresetOok650Async", sizeof(preset) - 1);
    preset[sizeof(preset) - 1] = '\0';

    FuriString *path = furi_string_alloc_set(SUBGHZ_DIR);
    DialogsFileBrowserOptions br;
    dialog_file_browser_set_basic_options(&br, ".sub", &I_sub1_10px);
    br.base_path = SUBGHZ_DIR;

    while (n < MERGE_MAX_FILES)
    {
        if (!dialog_file_browser_show(dialogs, path, path, &br))
        {
            if (n == 0)
                break;

            DialogMessageButton b = merge_ask_more(dialogs, n, total);
            if (b == DialogMessageButtonBack)
            {
                aborted = true;
                break;
            }

            if (b != DialogMessageButtonRight)
                break;

            continue;
        }

        gui_add_view_port(gui, load_vp, GuiLayerFullscreen);
        view_port_update(load_vp);
        uint32_t f = freq;
        char pr[48];
        strncpy(pr, preset, sizeof(pr) - 1);
        pr[sizeof(pr) - 1] = '\0';
        bool is_raw = true;
        size_t cnt =
            count_sub_samples(storage, furi_string_get_cstr(path), &f, pr, sizeof(pr), &is_raw);
        gui_remove_view_port(gui, load_vp);

        if (cnt == 0)
        {
            DialogMessage *m = dialog_message_alloc();
            dialog_message_set_header(m, "Unsupported file", 64, 2, AlignCenter, AlignTop);
            dialog_message_set_text(
                m, "Need a RAW or decoded\nKeeLoq .sub.", 64, 34, AlignCenter, AlignCenter);
            dialog_message_set_buttons(m, NULL, NULL, "OK");
            dialog_message_show(dialogs, m);
            dialog_message_free(m);
            continue;
        }

        if (n > 0 && f != freq)
        {
            char msg[128];
            snprintf(
                msg,
                sizeof(msg),
                "Merge: %lu.%02lu MHz\nFile: %lu.%02lu MHz",
                (unsigned long)(freq / 1000000),
                (unsigned long)((freq % 1000000) / 10000),
                (unsigned long)(f / 1000000),
                (unsigned long)((f % 1000000) / 10000));
            DialogMessage *m = dialog_message_alloc();
            dialog_message_set_header(m, "Frequency mismatch", 64, 2, AlignCenter, AlignTop);
            dialog_message_set_text(m, msg, 64, 32, AlignCenter, AlignCenter);
            dialog_message_set_buttons(m, NULL, NULL, "OK");
            dialog_message_show(dialogs, m);
            dialog_message_free(m);
            continue;
        }

        size_t extra = cnt + (n > 0 ? 1 : 0);
        size_t newtotal = total + extra;
        bool over_cap = newtotal > MAX_SAMPLES;
        bool over_ram = newtotal * sizeof(int16_t) + LOAD_HEAP_RESERVE > memmgr_get_free_heap();
        if (over_cap || over_ram)
        {
            char msg[128];
            snprintf(msg, sizeof(msg), "Have %u, +%u.\n%s %d.", (unsigned)total, (unsigned)cnt,
                     over_cap ? "Limit" : "RAM fits ~",
                     over_cap ? MAX_SAMPLES : (int)(memmgr_get_free_heap() / sizeof(int16_t)));

            DialogMessage *m = dialog_message_alloc();
            dialog_message_set_header(
                m, over_cap ? "Won't fit" : "Out of memory", 64, 2, AlignCenter, AlignTop);
            dialog_message_set_text(m, msg, 64, 34, AlignCenter, AlignCenter);
            dialog_message_set_buttons(m, NULL, NULL, "OK");
            dialog_message_show(dialogs, m);
            dialog_message_free(m);
            continue;
        }

        strncpy(paths[n], furi_string_get_cstr(path), MERGE_PATH_LEN - 1);
        paths[n][MERGE_PATH_LEN - 1] = '\0';
        is_raw_arr[n] = is_raw;

        if (n == 0)
        {
            freq = f;
            strncpy(preset, pr, sizeof(preset) - 1);
            preset[sizeof(preset) - 1] = '\0';
        }

        total = newtotal;
        n++;

        DialogMessageButton b = merge_ask_more(dialogs, n, total);
        if (b == DialogMessageButtonBack)
        {
            aborted = true;
            break;
        }

        if (b != DialogMessageButtonRight)
            break;
    }

    furi_string_free(path);

    if (aborted || n < 1 || total < 2)
    {
        free(paths);
        goto cleanup;
    }

    app->sd.data = safe_malloc(total * sizeof(int16_t));
    if (!app->sd.data)
    {
        free(paths);
        show_oom_dialog(dialogs, "Not enough RAM to\nbuild the merge.\nReboot Flipper.");
        goto cleanup;
    }

    app->sd.cap = total;
    app->sd.count = 0;
    app->sd.frequency = freq;
    strncpy(app->sd.preset, preset, sizeof(app->sd.preset) - 1);
    app->sd.preset[sizeof(app->sd.preset) - 1] = '\0';

    gui_add_view_port(gui, load_vp, GuiLayerFullscreen);
    view_port_update(load_vp);

    for (int i = 0; i < n; i++)
        fill_sub_into(storage, paths[i], &app->sd, is_raw_arr[i], i > 0);

    gui_remove_view_port(gui, load_vp);
    free(paths);

    recompute_total_us(&app->sd);
    strncpy(app->basename, "merged", sizeof(app->basename) - 1);

    ViewPort *vp = view_port_alloc();
    FuriMessageQueue *queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    view_port_draw_callback_set(vp, draw_cb, app);
    view_port_input_callback_set(vp, input_cb, queue);
    gui_add_view_port(gui, vp, GuiLayerFullscreen);

    run_editor_session(gui, vp, queue, storage, dialogs, app, false);

    gui_remove_view_port(gui, vp);
    view_port_free(vp);
    furi_message_queue_free(queue);

cleanup:
    view_port_free(load_vp);
    furi_record_close(RECORD_GUI);

    if (app->sd.data)
        free(app->sd.data);

    if (app->undo_data)
        free(app->undo_data);

    furi_mutex_free(app->mutex);
    free(app);
}

typedef enum
{
    MenuViewSubmenu,
    MenuViewAbout,
} MenuViewId;

typedef enum
{
    MenuItemSelectFile,
    MenuItemMergeFiles,
    MenuItemAbout,
} MenuItemId;

typedef enum
{
    MenuActionNone = 0,
    MenuActionEdit,
    MenuActionMerge,
} MenuAction;

typedef struct
{
    ViewDispatcher *view_dispatcher;
    Submenu *submenu;
    Widget *widget;
    Storage *storage;
    DialogsApp *dialogs;
    MenuAction action;
} Menu;

static void menu_submenu_cb(void *context, uint32_t index)
{
    Menu *menu = context;
    if (index == MenuItemSelectFile)
    {
        menu->action = MenuActionEdit;
        view_dispatcher_stop(menu->view_dispatcher);
    }
    else if (index == MenuItemMergeFiles)
    {
        menu->action = MenuActionMerge;
        view_dispatcher_stop(menu->view_dispatcher);
    }
    else if (index == MenuItemAbout)
    {
        view_dispatcher_switch_to_view(menu->view_dispatcher, MenuViewAbout);
    }
}

static uint32_t about_back_cb(void *context)
{
    UNUSED(context);
    return MenuViewSubmenu;
}

static uint32_t submenu_back_cb(void *context)
{
    UNUSED(context);
    return VIEW_NONE;
}

static void menu_build_about(Menu *menu)
{
    widget_add_text_scroll_element(
        menu->widget,
        0,
        0,
        SCREEN_W_PX,
        64,
        "\e#Sub-GHz RAW Edit\e#\n"
        "Version " APP_VERSION "\n"
        "\n"
        "Trim a RAW .sub to one\n"
        "clean frame (auto-find,\n"
        "A/B, cut, save).\n"
        "\n"
        "Merge: join several\n"
        ".sub into one.\n"
        "\n"
        "Opens a decoded KeeLoq\n"
        ".sub as RAW too.\n"
        "\n"
        "RX/analysis only - never\n"
        "transmits.\n"
        "\n"
        "by Lechnio\n" APP_REPO "\n");
}

int32_t subghz_raw_edit_app(void *p)
{
    UNUSED(p);

    Menu *menu = safe_malloc(sizeof(Menu));
    if (!menu)
        return 0;

    memset(menu, 0, sizeof(Menu));

    menu->storage = furi_record_open(RECORD_STORAGE);
    menu->dialogs = furi_record_open(RECORD_DIALOGS);
    Gui *gui = furi_record_open(RECORD_GUI);

    menu->view_dispatcher = view_dispatcher_alloc();
    menu->submenu = submenu_alloc();
    menu->widget = widget_alloc();

    submenu_set_header(menu->submenu, "Sub-GHz RAW Edit");
    submenu_add_item(menu->submenu, "Select .sub file", MenuItemSelectFile, menu_submenu_cb, menu);
    submenu_add_item(menu->submenu, "Merge .sub files", MenuItemMergeFiles, menu_submenu_cb, menu);
    submenu_add_item(menu->submenu, "About", MenuItemAbout, menu_submenu_cb, menu);

    menu_build_about(menu);

    view_set_previous_callback(submenu_get_view(menu->submenu), submenu_back_cb);
    view_set_previous_callback(widget_get_view(menu->widget), about_back_cb);

    view_dispatcher_attach_to_gui(menu->view_dispatcher, gui, ViewDispatcherTypeFullscreen);
    view_dispatcher_add_view(
        menu->view_dispatcher, MenuViewSubmenu, submenu_get_view(menu->submenu));
    view_dispatcher_add_view(menu->view_dispatcher, MenuViewAbout, widget_get_view(menu->widget));

    bool running = true;
    while (running)
    {
        menu->action = MenuActionNone;
        view_dispatcher_switch_to_view(menu->view_dispatcher, MenuViewSubmenu);
        view_dispatcher_run(menu->view_dispatcher);

        if (menu->action == MenuActionEdit)
        {
            run_editor(menu->storage, menu->dialogs);
        }
        else if (menu->action == MenuActionMerge)
        {
            run_merge(menu->storage, menu->dialogs);
        }
        else
        {
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
