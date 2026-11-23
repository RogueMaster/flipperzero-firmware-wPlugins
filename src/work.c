/* The RAM window over the working file: loading and sliding it, the overview of
 * the whole signal, and Cut and Undo, which rewrite the file. A cut keeps the
 * file it replaced as WORK_UNDO_PATH, so Undo costs no RAM. */

#include "app.h"

int32_t samples_us(const int16_t *d, size_t n)
{
    int64_t t = 0;
    for (size_t i = 0; i < n; i++)
        t += iabs32(d[i]);

    return t > 0x7FFFFFFF ? 0x7FFFFFFF : (int32_t)t;
}

// More of the signal than the window holds.
bool work_windowed(const SubData *sd)
{
    return sd->win_s0 || sd->win_s0 + sd->count < sd->file_count;
}

static File *work_file(Storage *st, const char *path, FS_AccessMode mode, FS_OpenMode open)
{
    File *f = storage_file_alloc(st);
    if (storage_file_open(f, path, mode, open))
        return f;

    storage_file_free(f);
    return NULL;
}

static void work_close(File *f)
{
    storage_file_close(f);
    storage_file_free(f);
}

static bool work_read(File *f, size_t s0, int16_t *dst, size_t n)
{
    size_t bytes = n * sizeof(int16_t);
    return storage_file_seek(f, s0 * sizeof(int16_t), true) && storage_file_read(f, dst, bytes) == bytes;
}

// Read n samples from s0 of the working file into dst.
static bool work_read_at(App *a, size_t s0, int16_t *dst, size_t n)
{
    File *f = work_file(a->st, WORK_PATH, FSAM_READ, FSOM_OPEN_EXISTING);
    if (!f)
        return false;

    bool ok = work_read(f, s0, dst, n);
    work_close(f);
    return ok;
}

// Fill the window from sample s0; the caller sets win_t0.
bool work_load(App *a, size_t s0)
{
    SubData *sd = &a->sd;
    size_t n = sd->file_count - s0;
    if (n > sd->cap)
        n = sd->cap;

    bool ok = work_read_at(a, s0, sd->data, n);
    sd->win_s0 = s0;
    sd->count = ok ? n : 0;
    sd->win_us = samples_us(sd->data, sd->count);
    return ok;
}

/* Move the window a quarter of itself later (dir > 0) or earlier. The kept
 * part shifts in RAM and only the new quarter is read. False at either end of
 * the signal. */
bool work_slide(App *a, int dir)
{
    SubData *sd = &a->sd;
    size_t k = sd->cap / 4 ? sd->cap / 4 : 1;
    size_t end = sd->win_s0 + sd->count;
    bool ok;
    if (dir > 0)
    {
        if (end >= sd->file_count)
            return false;

        if (k > sd->file_count - end)
            k = sd->file_count - end;

        sd->win_t0 += samples_us(sd->data, k);
        memmove(sd->data, sd->data + k, (sd->count - k) * sizeof(int16_t));
        ok = work_read_at(a, end, sd->data + sd->count - k, k);
        sd->win_s0 += k;
    }
    else
    {
        if (!sd->win_s0)
            return false;

        if (k > sd->win_s0)
            k = sd->win_s0;

        memmove(sd->data + k, sd->data, (sd->count - k) * sizeof(int16_t));
        ok = work_read_at(a, sd->win_s0 - k, sd->data, k);
        sd->win_s0 -= k;
        sd->win_t0 -= samples_us(sd->data, k);
    }

    if (!ok)
        work_load(a, sd->win_s0);

    sd->win_us = samples_us(sd->data, sd->count);
    return ok;
}

// The overview bar covers the whole signal, so it is read from the file.
void recompute_overview(App *a)
{
    memset(a->overview, 0, sizeof(a->overview));
    int32_t total = a->sd.total_us > 0 ? a->sd.total_us : 1;
    uint16_t mx = 0;

    File *f = work_file(a->st, WORK_PATH, FSAM_READ, FSOM_OPEN_EXISTING);
    if (f)
    {
        int16_t buf[WORK_CHUNK];
        int64_t t = 0;
        bool prev_low = true; // the first sample is an edge when it is a pulse
        size_t got;
        while ((got = storage_file_read(f, buf, sizeof(buf)) / sizeof(int16_t)) > 0)
        {
            for (size_t i = 0; i < got; i++)
            {
                bool low = buf[i] < 0;
                if (low != prev_low)
                {
                    int x = (int)((t * SCREEN_W_PX) / total);
                    if (x > SCREEN_W_PX - 1)
                        x = SCREEN_W_PX - 1;

                    if (a->overview[x] < UINT16_MAX)
                        a->overview[x]++;

                    if (a->overview[x] > mx)
                        mx = a->overview[x];
                }

                prev_low = low;
                t += iabs32(buf[i]);
            }
        }

        work_close(f);
    }

    a->ov_max = mx ? mx : 1;
}

/* Snap the jitter of the whole file to the TE fitted on the first window,
 * rewriting it in place (same sample count), then reload the window. */
static bool work_jitter(App *a)
{
    SubData *sd = &a->sd;
    float te = jitter_te(sd->data, sd->count);
    if (te <= 0.0f)
        return true;

    File *f = work_file(a->st, WORK_PATH, FSAM_READ_WRITE, FSOM_OPEN_EXISTING);
    if (!f)
        return false;

    int16_t buf[WORK_CHUNK];
    int64_t total = 0;
    bool ok = true;
    for (size_t s = 0; ok && s < sd->file_count; s += WORK_CHUNK)
    {
        size_t n = sd->file_count - s < WORK_CHUNK ? sd->file_count - s : WORK_CHUNK;
        size_t bytes = n * sizeof(int16_t);
        ok = work_read(f, s, buf, n);
        jitter_apply(buf, n, te);
        total += samples_us(buf, n);
        ok = ok && storage_file_seek(f, s * sizeof(int16_t), true) && storage_file_write(f, buf, bytes) == bytes;
    }

    work_close(f);
    sd->total_us = total > 0x7FFFFFFF ? 0x7FFFFFFF : (int32_t)total;
    return ok && work_load(a, 0);
}

/* Take the RAM window once the working file is written: all of the signal when
 * it fits, else as much as RAM allows. */
bool work_open(App *a)
{
    SubData *sd = &a->sd;
    size_t cap = sample_budget();
    if (cap > sd->file_count)
        cap = sd->file_count;

    sd->data = cap >= 2 ? safe_malloc(cap * sizeof(int16_t)) : NULL;
    if (!sd->data)
    {
        sd->out_of_memory = true;
        return false;
    }

    sd->cap = cap;
    sd->win_t0 = 0;
    if (!work_load(a, 0) || (g_normalize_jitter && !work_jitter(a)))
        return false;

    recompute_overview(a);
    return true;
}

static void state_save(const App *a, WorkState *s)
{
    *s = (WorkState){
        .file_count = a->sd.file_count,
        .win_s0 = a->sd.win_s0,
        .win_t0 = a->sd.win_t0,
        .total_us = a->sd.total_us,
        .marker_a = a->marker_a,
        .marker_b = a->marker_b,
        .view_start = a->view_start,
        .view_end = a->view_end,
        .active = a->active,
    };
}

static void state_restore(App *a, const WorkState *s)
{
    a->sd.file_count = s->file_count;
    a->sd.win_t0 = s->win_t0;
    a->sd.total_us = s->total_us;
    a->marker_a = s->marker_a;
    a->marker_b = s->marker_b;
    a->view_start = s->view_start;
    a->view_end = s->view_end;
    a->active = s->active;
    work_load(a, s->win_s0);
}

static bool work_copy(File *src, File *dst, size_t from, size_t n)
{
    int16_t buf[WORK_CHUNK];
    if (!storage_file_seek(src, from * sizeof(int16_t), true))
        return false;

    while (n)
    {
        size_t k = n < WORK_CHUNK ? n : WORK_CHUNK;
        size_t bytes = k * sizeof(int16_t);
        if (storage_file_read(src, buf, bytes) != bytes || storage_file_write(dst, buf, bytes) != bytes)
            return false;

        n -= k;
    }

    return true;
}

/* Cut A..B out of the window, then write the file anew: what precedes the
 * window, the window, what follows it. The old file becomes the Undo copy. On
 * a card error nothing changes. */
bool work_cut(App *a)
{
    SubData *sd = &a->sd;
    WorkState before;
    state_save(a, &before);
    size_t old_count = sd->count;
    int32_t old_us = sd->win_us;

    cut_compact(a);

    bool ok = false;
    File *src = work_file(a->st, WORK_PATH, FSAM_READ, FSOM_OPEN_EXISTING);
    File *dst = work_file(a->st, WORK_NEW_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS);
    if (src && dst)
    {
        size_t bytes = sd->count * sizeof(int16_t);
        size_t after = sd->win_s0 + old_count;
        ok = work_copy(src, dst, 0, sd->win_s0) && storage_file_write(dst, sd->data, bytes) == bytes &&
             work_copy(src, dst, after, sd->file_count - after);
    }

    if (src)
        work_close(src);

    if (dst)
        work_close(dst);

    if (ok)
    {
        ok = storage_common_rename(a->st, WORK_PATH, WORK_UNDO_PATH) == FSE_OK;
        if (ok && storage_common_rename(a->st, WORK_NEW_PATH, WORK_PATH) != FSE_OK)
        {
            storage_common_rename(a->st, WORK_UNDO_PATH, WORK_PATH);
            ok = false;
        }
    }

    if (!ok)
    {
        storage_simply_remove(a->st, WORK_NEW_PATH);
        state_restore(a, &before);
        recompute_activity(a);
        return false;
    }

    a->undo = before;
    a->has_undo = true;
    sd->file_count = before.file_count - old_count + sd->count;
    sd->total_us = before.total_us - (old_us - sd->win_us);
    work_load(a, sd->win_s0); // refill what the cut emptied
    clamp_view(a);
    recompute_overview(a);
    recompute_activity(a);
    return true;
}

void work_undo(App *a)
{
    if (!a->has_undo || storage_common_rename(a->st, WORK_UNDO_PATH, WORK_PATH) != FSE_OK)
        return;

    a->has_undo = false;
    state_restore(a, &a->undo);
    recompute_overview(a);
    recompute_activity(a);
}

void work_cleanup(Storage *st)
{
    storage_simply_remove_recursive(st, WORK_DIR);
}
