// Reading .sub files: RAW samples, decoded protocols turned into samples,
// custom buttons, jitter clean-up and the merge building blocks.

#include "app.h"

bool lr_read_line(LineReader *lr, FuriString *out)
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

    sd->data[sd->count++] = (int16_t)clamp_dur(v);

    return true;
}

void *safe_malloc(size_t size)
{
    if (size == 0 || memmgr_get_free_heap() < size + LOAD_HEAP_RESERVE)
        return NULL;

    return malloc(size);
}

void *safe_realloc(void *ptr, size_t size)
{
    if (size == 0 || memmgr_get_free_heap() < size + LOAD_HEAP_RESERVE)
        return NULL;

    return realloc(ptr, size);
}

// Most samples a signal may take: everything above LOAD_HEAP_RESERVE, which Decode and Save run
// in. The reserve has to stay contiguous too, so it is also kept back from the largest block the
// samples come out of; the rest of the heap may be only crumbs. Cut works in place and only its
// Undo copy needs as much again, so a signal too big for that copy is cut without Undo after
// asking.
size_t sample_budget(void)
{
    size_t free_heap = memmgr_get_free_heap();
    size_t block = memmgr_heap_get_max_free_block();
    if (free_heap <= LOAD_HEAP_RESERVE || block <= LOAD_HEAP_RESERVE)
        return 0;

    size_t usable = free_heap - LOAD_HEAP_RESERVE;
    if (usable > block - LOAD_HEAP_RESERVE)
        usable = block - LOAD_HEAP_RESERVE;

    return usable / sizeof(int16_t);
}

/* Decoded protocols are synthesized by the firmware's own SubGhz encoder: the
 * key file is deserialized into a transmitter and we collect the level/duration
 * upload it would send on air. Every protocol the firmware supports works with
 * no per-protocol code here. Rolling codes (KeeLoq, Nice Flor-S, ...) bump and
 * re-encrypt their counter inside the encoder; custom firmware lets us zero that
 * step so the frame keeps its recorded counter, official firmware always yields
 * the NEXT one. The manufacturer keystores (system + user) are loaded so that
 * encryption works. */

SubGhzEnvironment *subghz_env_setup(void)
{
    SubGhzEnvironment *env = subghz_environment_alloc();
    if (!env)
        return NULL;

    subghz_environment_set_protocol_registry(env, (void *)&subghz_protocol_registry);

    subghz_environment_set_came_atomo_rainbow_table_file_name(
        env, "/ext/subghz/assets/came_atomo");
    subghz_environment_set_alutech_at_4n_rainbow_table_file_name(
        env, "/ext/subghz/assets/alutech_at_4n");
    subghz_environment_set_nice_flor_s_rainbow_table_file_name(
        env, "/ext/subghz/assets/nice_flor_s");

    return env;
}

static bool synth_grow(SubData *sd, size_t need)
{
    if (sd->cap >= need)
        return true;

    size_t ncap = sd->cap ? sd->cap : 256;
    while (ncap < need)
        ncap *= 2;

    if (ncap > sd->limit)
        ncap = sd->limit;

    if (ncap < need)
        return false;

    int16_t *grown = safe_realloc(sd->data, ncap * sizeof(int16_t));
    if (!grown)
    {
        // What is already loaded stays usable, just truncated
        if (sd->count == 0)
            sd->out_of_memory = true;
        return false;
    }

    sd->data = grown;
    sd->cap = ncap;
    return true;
}

bool push_duration(SubData *sd, int32_t v)
{
    int32_t sign = (v < 0) ? -1 : 1;
    int32_t mag = iabs32(v);

    do
    {
        int32_t chunk = (mag > DUR_CLAMP) ? DUR_CLAMP : mag;
        if (!synth_grow(sd, sd->count + 1))
        {
            sd->truncated = true;
            return false;
        }
        sd->data[sd->count++] = (int16_t)(sign * chunk);
        mag -= chunk;
    } while (mag > 0);

    return true;
}

// How many int16 samples a duration of magnitude |mag| µs occupies once split to
// fit the per-sample clamp (the inverse count of what push_duration emits).
size_t duration_samples(int32_t mag)
{
    return (mag > DUR_CLAMP) ? (size_t)((mag + DUR_CLAMP - 1) / DUR_CLAMP) : 1;
}

static FlipperFormat *fff_load_copy(Storage *storage, const char *path)
{
    FlipperFormat *fff = flipper_format_string_alloc();
    if (!fff)
        return NULL;

    Stream *src = file_stream_alloc(storage);
    bool ok = false;

    if (file_stream_open(src, path, FSAM_READ, FSOM_OPEN_EXISTING))
    {
        Stream *dst = flipper_format_get_raw_stream(fff);
        ok = stream_copy_full(src, dst) > 0;
        stream_rewind(dst);
    }

    stream_free(src);

    if (!ok)
    {
        flipper_format_free(fff);
        return NULL;
    }

    return fff;
}

/* ---- Custom buttons ---------------------------------------------------- */

uint8_t g_custom_btn_id = SUBGHZ_CUSTOM_BTN_OK;

#ifndef FW_ORIGIN_Official

static void btn_extract_label(const char *info, char *out, size_t outlen)
{
    out[0] = '\0';

    const char *p = strstr(info, "Btn");
    if (!p)
        return;

    const char *eol = p;
    while (*eol && *eol != '\r' && *eol != '\n')
        eol++;

    const char *dash = strstr(p, " - ");
    const char *start;

    if (dash && dash < eol)
    {
        start = dash + 3;
    }
    else
    {
        start = strchr(p, ':');
        if (!start || start > eol)
            return;

        start++;
        while (*start == ' ')
            start++;
    }

    size_t n = 0;

    if (dash && dash < eol)
    {
        while (start[n] && start[n] != '\r' && start[n] != '\n' && n < outlen - 1)
            n++;

        memcpy(out, start, n);
        out[n] = '\0';
        return;
    }

    while (start[n] && start[n] != ' ' && start[n] != '\t' && start[n] != '\r' &&
           start[n] != '\n' && n < outlen - 1)
        n++;

    if (n == 0)
        return;

    if (n >= 2 && start[0] == '0' && (start[1] == 'x' || start[1] == 'X'))
        snprintf(out, outlen, "%.*s", (int)n, start);
    else
        snprintf(out, outlen, "0x%.*s", (int)n, start);
}

void btn_probe(Storage *storage, const char *path, const char *protocol, BtnPick *out)
{
    memset(out, 0, sizeof(*out));
    strncpy(out->protocol, protocol, sizeof(out->protocol) - 1);

    SubGhzEnvironment *env = subghz_env_setup();
    if (!env)
        return;

    if (strstr(protocol, "KeeLoq"))
    {
        subghz_environment_load_keystore(env, "/ext/subghz/assets/keeloq_mfcodes");
        subghz_environment_load_keystore(env, "/ext/subghz/assets/keeloq_mfcodes_user");
    }

    SubGhzReceiver *rx = subghz_receiver_alloc_init(env);
    SubGhzProtocolDecoderBase *dec =
        rx ? subghz_receiver_search_decoder_base_by_name(rx, protocol) : NULL;

    subghz_custom_btns_reset();

    FuriString *info = furi_string_alloc();
    int named = 0;

    for (uint8_t slot = 0; slot < BTN_SLOTS; slot++)
    {
        if (slot > 0 && !subghz_custom_btn_set(slot))
            continue;

        FlipperFormat *fff = fff_load_copy(storage, path);
        if (!fff)
            break;

        SubGhzTransmitter *tx = subghz_transmitter_alloc_init(env, protocol);
        bool encoded = tx && subghz_transmitter_deserialize(tx, fff) == SubGhzProtocolStatusOk;

        if (tx)
            subghz_transmitter_free(tx);

        if (slot == 0 && (!encoded || !subghz_custom_btn_is_allowed()))
        {
            flipper_format_free(fff);
            break;
        }

        if (encoded && dec && flipper_format_rewind(fff) &&
            subghz_protocol_decoder_base_deserialize(dec, fff) == SubGhzProtocolStatusOk)
        {
            furi_string_reset(info);
            subghz_protocol_decoder_base_get_string(dec, info);
            btn_extract_label(furi_string_get_cstr(info), out->label[slot], BTN_LABEL_LEN);
        }

        flipper_format_free(fff);

        if (out->label[slot][0])
            named++;
    }

    furi_string_free(info);

    if (rx)
        subghz_receiver_free(rx);

    subghz_environment_free(env);

    out->available = named > 1;
    subghz_custom_btn_set(SUBGHZ_CUSTOM_BTN_OK);
}

#endif // FW_ORIGIN_Official

static bool synthesize_via_transmitter(
    Storage *storage, const char *path, const char *protocol, SubData *sd)
{
    if (!protocol[0])
        return false;

    SubGhzEnvironment *env = subghz_env_setup();
    if (!env)
        return false;

    if (strstr(protocol, "KeeLoq"))
    {
        subghz_environment_load_keystore(env, "/ext/subghz/assets/keeloq_mfcodes");
        subghz_environment_load_keystore(env, "/ext/subghz/assets/keeloq_mfcodes_user");
    }

#ifndef FW_ORIGIN_Official
    /* Rolling-code encoders add this multiplier to the counter before
     * re-encrypting. Zero loads the frame with the counter as recorded, not
     * the next one; the Sub-GHz app's setting is restored right after. */
    int32_t cnt_mult = furi_hal_subghz_get_rolling_counter_mult();
    furi_hal_subghz_set_rolling_counter_mult(0);

    if (g_custom_btn_id != SUBGHZ_CUSTOM_BTN_OK)
    {
        subghz_custom_btns_reset();

        FlipperFormat *prime = fff_load_copy(storage, path);
        if (prime)
        {
            SubGhzTransmitter *ptx = subghz_transmitter_alloc_init(env, protocol);
            if (ptx)
            {
                subghz_transmitter_deserialize(ptx, prime);
                subghz_transmitter_free(ptx);
            }

            flipper_format_free(prime);
        }

        subghz_custom_btn_set(g_custom_btn_id);
    }
#endif

    FlipperFormat *fff = fff_load_copy(storage, path);
    SubGhzTransmitter *tx = NULL;
    bool ok = false;

    do
    {
        if (!fff)
            break;

        tx = subghz_transmitter_alloc_init(env, protocol);
        if (!tx)
            break;

        if (subghz_transmitter_deserialize(tx, fff) != SubGhzProtocolStatusOk)
            break;

        sd->count = 0;
        for (;;)
        {
            LevelDuration ld = subghz_transmitter_yield(tx);
            if (level_duration_is_reset(ld))
                break;

            int32_t dur = (int32_t)level_duration_get_duration(ld);
            int32_t v = level_duration_get_level(ld) ? dur : -dur;

            if (!push_duration(sd, v))
                break; // out of room; keep what we have

            if (sd->count >= SYNTH_YIELD_CAP)
                break;
        }

        /* The encoder repeats the same upload `repeat` times. Find the true
         * period P (smallest P for which the whole buffer is P-periodic - this
         * rejects the short sub-period of a repetitive preamble like KeeLoq's,
         * unlike a header-only match) and keep ONE period plus the next frame's
         * header as its trailing sync. Without this, N duplicates bloat the
         * buffer and OOM on large frames (e.g. Nice Flor-S). */
        size_t n = sd->count;
        for (size_t p = 4; p * 2 <= n; p++)
        {
            bool periodic = true;
            for (size_t i = p; i < n; i++)
            {
                if (sd->data[i] != sd->data[i - p])
                {
                    periodic = false;
                    break;
                }
            }

            if (periodic)
            {
                /* Keep one period. Append the next frame's first sample as a
                 * trailing delimiter only when it is a gap (gap-first protocols
                 * like Dooya/CAME, whose period otherwise ends on a pulse and
                 * leaves the last bit unframed). Pulse-first protocols (KeeLoq)
                 * already end the period on their inter-frame guard gap, so
                 * appending data[p] there would dangle a stray pulse. */
                sd->count = (sd->data[p] < 0) ? p + 1 : p;
                break;
            }
        }

        ok = sd->count > 0;
    } while (0);

    if (tx)
        subghz_transmitter_free(tx);

    if (fff)
        flipper_format_free(fff);

#ifndef FW_ORIGIN_Official
    furi_hal_subghz_set_rolling_counter_mult(cnt_mult);
#endif

    subghz_environment_free(env);
    return ok;
}

/* Snap each pulse to the nearest multiple of a base unit Te, removing timing
 * jitter and cumulative drift. Te is found by an iterative least-squares fit
 * |dur| ~ m*Te; gaps and noise (outside the data window) are left alone. */
bool g_normalize_jitter = false;

/* Manual silence separator inserted between independent signals (different
 * files) during a merge, in milliseconds. Repetitions of the same signal are
 * separated by the signal's own native gap instead. */
int32_t g_merge_gap_ms = MERGE_GAP_DEFAULT_MS;

/* How many times each loaded signal is repeated in the merged output. */
int32_t g_merge_repeat = MERGE_REPEAT_DEFAULT;

#define JITTER_DATA_MIN_US 60
#define JITTER_DATA_MAX_US 3000
#define JITTER_FIT_ITERS 8

void normalize_jitter(SubData *sd)
{
    if (!sd || !sd->data || sd->count < 8)
        return;

    int64_t sum_all = 0;
    size_t n_all = 0;
    for (size_t i = 0; i < sd->count; i++)
    {
        int32_t a = iabs32(sd->data[i]);
        if (a >= JITTER_DATA_MIN_US && a <= JITTER_DATA_MAX_US)
        {
            sum_all += a;
            n_all++;
        }
    }

    if (n_all == 0)
        return;

    int32_t mean_all = (int32_t)(sum_all / (int64_t)n_all);
    int64_t sum_short = 0;
    size_t n_short = 0;
    for (size_t i = 0; i < sd->count; i++)
    {
        int32_t a = iabs32(sd->data[i]);
        if (a >= JITTER_DATA_MIN_US && a <= mean_all)
        {
            sum_short += a;
            n_short++;
        }
    }

    float te = (n_short > 0) ? (float)sum_short / (float)n_short : (float)mean_all;
    if (te < 1.0f)
        return;

    for (int it = 0; it < JITTER_FIT_ITERS; it++)
    {
        float num = 0.0f, den = 0.0f;
        for (size_t i = 0; i < sd->count; i++)
        {
            int32_t a = iabs32(sd->data[i]);
            if (a < JITTER_DATA_MIN_US || a > JITTER_DATA_MAX_US)
                continue;

            int m = (int)((float)a / te + 0.5f);
            if (m < 1)
                m = 1;

            num += (float)m * (float)a;
            den += (float)m * (float)m;
        }
        if (den <= 0.0f)
            return;

        te = num / den;
    }

    for (size_t i = 0; i < sd->count; i++)
    {
        int32_t a = iabs32(sd->data[i]);
        if (a < JITTER_DATA_MIN_US || a > JITTER_DATA_MAX_US)
            continue;

        int m = (int)((float)a / te + 0.5f);
        if (m < 1)
            m = 1;

        int32_t mag = (int32_t)((float)m * te + 0.5f);
        if (mag > DUR_CLAMP)
            mag = DUR_CLAMP;

        sd->data[i] = (int16_t)(sd->data[i] < 0 ? -mag : mag);
    }
}

// Sum of all sample durations, saturated to int32, cached on the SubData.
void recompute_total_us(SubData *sd)
{
    int64_t run = 0;
    for (size_t i = 0; i < sd->count; i++)
        run += iabs32(sd->data[i]);

    if (run > 0x7FFFFFFF)
        run = 0x7FFFFFFF;

    sd->total_us = (int32_t)run;
}

// Parse a .sub metadata line (Frequency/Preset/Protocol) into the given fields.
// Returns true if the line matched one of them; RAW_Data is handled by callers.
bool parse_sub_meta(
    const char *s, uint32_t *freq, char *preset, size_t preset_sz, char *protocol, size_t proto_sz)
{
    if (strncmp(s, "Frequency:", 10) == 0)
    {
        *freq = (uint32_t)strtoul(s + 10, NULL, 10);
        return true;
    }

    if (strncmp(s, "Preset:", 7) == 0)
    {
        const char *p = s + 7;
        while (*p == ' ')
            p++;

        strncpy(preset, p, preset_sz - 1);
        preset[preset_sz - 1] = '\0';
        return true;
    }

    if (strncmp(s, "Protocol:", 9) == 0)
    {
        const char *p = s + 9;
        while (*p == ' ')
            p++;

        strncpy(protocol, p, proto_sz - 1);
        protocol[proto_sz - 1] = '\0';
        return true;
    }

    return false;
}

// Pull the next integer from a RAW_Data value list, advancing *p. Returns false
// at end of line. Skips stray non-numeric characters between values.
static bool next_raw_value(const char **p, long *out)
{
    char *end;
    while (**p)
    {
        long v = strtol(*p, &end, 10);
        if (end == *p)
        {
            if (**p == '\0')
                break;

            (*p)++;
            continue;
        }

        *p = end;
        *out = v;

        return true;
    }

    return false;
}

bool load_sub(Storage *storage, const char *path, SubData *sd)
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

    // Predict sample count from file weight. Measured RAW .sub captures run
    // ~4.4-5.4 bytes per sample (number text + sign + space), so fsize/4 is a
    // good first guess that fits normal captures without over-allocating. It is
    // only a hint: push_duration grows the buffer on demand (e.g. when long gaps
    // split into several samples), and decoded files synthesize into it.
    // The line buffer is sized before the samples take what is left: growing it later, with the
    // heap nearly full, would have nowhere to go.
    FuriString *line = furi_string_alloc();
    furi_string_reserve(line, LINE_RESERVE);

    sd->limit = sample_budget();
    size_t est = (size_t)(fsize / 4) + 64;
    if (est > sd->limit)
        est = sd->limit;

    sd->data = safe_malloc(est * sizeof(int16_t));
    if (!sd->data)
    {
        sd->out_of_memory = true;
        furi_string_free(line);
        storage_file_close(f);
        storage_file_free(f);
        return false;
    }

    sd->cap = est;

    char protocol[32] = {0};

    LineReader lr = {.file = f, .len = 0, .pos = 0, .eof = false};
    bool stop = false;

    while (!stop && lr_read_line(&lr, line))
    {
        const char *s = furi_string_get_cstr(line);
        if (strncmp(s, "RAW_Data:", 9) == 0)
        {
            const char *p = s + 9;
            long v;
            while (next_raw_value(&p, &v))
            {
                if (!push_duration(sd, (int32_t)v))
                {
                    stop = true;
                    break;
                }
            }
        }
        else
        {
            parse_sub_meta(
                s, &sd->frequency, sd->preset, sizeof(sd->preset), protocol, sizeof(protocol));
        }
    }

    furi_string_free(line);
    storage_file_close(f);
    storage_file_free(f);

    if (sd->count == 0 && protocol[0] && strcmp(protocol, "RAW") != 0)
        sd->synthesized = synthesize_via_transmitter(storage, path, protocol, sd);

    if (sd->data && sd->count > 0 && sd->count < sd->cap)
    {
        int16_t *shrunk = safe_realloc(sd->data, sd->count * sizeof(int16_t));
        if (shrunk)
        {
            sd->data = shrunk;
            sd->cap = sd->count;
        }
    }

    if (g_normalize_jitter)
        normalize_jitter(sd);

    recompute_total_us(sd);
    return sd->count >= 2;
}

/* Insert a clean inter-signal separator so the silence at the join equals
 * exactly the configured gap. Signals (especially synthesized KeeLoq) end and
 * start with their own guard silence. Adding the gap on top of it would double
 * (or worse) the visible gap. So we replace the previous
 * signal's trailing silence with the gap here and the next signal's leading
 * silence is dropped by the caller when it is appended. */
static void merge_separator(SubData *dst)
{
    // Absorb the previous signal's trailing silence so the gap isn't added on
    // top of it, then emit the gap via push_duration (which splits it into
    // several samples when it exceeds the per-sample clamp).
    if (g_merge_gap_ms == 0)
        return;

    while (dst->count > 0 && dst->data[dst->count - 1] < 0)
        dst->count--;

    push_duration(dst, -(g_merge_gap_ms * 1000));
}

size_t count_sub_samples(
    Storage *storage, const char *path, uint32_t *freq, char *preset, size_t preset_sz)
{
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

    while (lr_read_line(&lr, line))
    {
        const char *s = furi_string_get_cstr(line);
        if (strncmp(s, "RAW_Data:", 9) == 0)
        {
            const char *p = s + 9;
            long v;
            while (next_raw_value(&p, &v))
            {
                // Must match load_sub: durations beyond the clamp are split into
                // several samples, so count them the same way.
                raw_count += duration_samples(iabs32((int32_t)v));
            }
        }
        else
        {
            parse_sub_meta(s, freq, preset, preset_sz, protocol, sizeof(protocol));
        }
    }

    furi_string_free(line);
    storage_file_close(f);
    storage_file_free(f);

    if (raw_count > 0)
        return raw_count;

    /* Decoded protocol: the frame length isn't predictable from the file alone,
     * so synthesize it once via the firmware encoder and count the samples. */
    if (protocol[0] && strcmp(protocol, "RAW") != 0)
    {
        SubData tmp = {.limit = sample_budget()};
        size_t n = 0;
        if (synthesize_via_transmitter(storage, path, protocol, &tmp))
            n = tmp.count;

        if (tmp.data)
            free(tmp.data);

        if (n > 0)
            return n;
    }

    return 0;
}


// Prepare the boundary before appending a signal and report whether its own
// leading silence must be dropped so gaps don't stack into a doubled one.
//  - Manual join: replace the previous signal's trailing silence with our gap.
//  - Native join: keep the previous signal's native trailing silence as the gap.
// In both cases the appended signal's leading silence is dropped.
static bool merge_prepare_join(SubData *dst, MergeJoin join)
{
    if (join == MergeJoinManual)
    {
        merge_separator(dst);
        return true;
    }

    if (join == MergeJoinNative && dst->count > 0 && dst->data[dst->count - 1] < 0)
        return true;

    return false;
}

bool load_signal(Storage *storage, const char *path, SubData *out)
{
    return load_sub(storage, path, out);
}

// Append a preloaded signal to dst, inserting the appropriate join gap and
// dropping the signal's own leading silence so gaps don't stack.
void append_signal(SubData *dst, const SubData *sig, MergeJoin join)
{
    bool skip_lead = merge_prepare_join(dst, join);
    for (size_t i = 0; i < sig->count; i++)
    {
        int32_t v = sig->data[i];
        if (skip_lead)
        {
            if (v < 0)
                continue;

            skip_lead = false;
        }

        if (!append_sample(dst, v))
            break;
    }
}

bool load_sub_repeated(Storage *storage, const char *path, SubData *out)
{
    SubData sig = {0};
    bool ok = load_sub(storage, path, &sig);
    if (!ok || !sig.data || sig.count == 0)
    {
        *out = sig; // carries out_of_memory / flags for the caller
        return ok;
    }

    // Metadata (frequency, preset, flags) carries over; rebuild the samples.
    *out = sig;
    out->data = NULL;
    out->count = 0;
    out->cap = 0;

    size_t want = sig.count * (size_t)g_merge_repeat;
    out->limit = sample_budget();
    size_t need = want > out->limit ? out->limit : want;
    out->data = safe_malloc(need * sizeof(int16_t));
    if (!out->data)
    {
        out->out_of_memory = true;
        free(sig.data);
        return false;
    }
    out->cap = need;
    if (want > need)
        out->truncated = true;

    for (int r = 0; r < g_merge_repeat; r++)
    {
        MergeJoin join = (r == 0) ? MergeJoinNone : MergeJoinNative;
        append_signal(out, &sig, join);
    }

    recompute_total_us(out);
    free(sig.data);
    return out->count >= 2;
}
