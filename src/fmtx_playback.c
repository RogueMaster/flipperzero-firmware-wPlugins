#include "fmtx_playback.h"

#include "dsp.h"
#include "fmtx_rf.h"

#include <stdlib.h>
#include <string.h>
#include <furi.h>
#include <storage/storage.h>

#define MINIMP3_IMPLEMENTATION
#define MINIMP3_ONLY_MP3
#define MINIMP3_NO_SIMD
#include "../third_party/minimp3/minimp3.h"

#define INSZ 8192U
#define STACKSZ (24U * 1024U)
#define SKIP1 65536U
#define SKIPN 8192U

typedef struct
{
    uint32_t source_rate;
    uint32_t filled;
    int64_t sum;
} Rs;

typedef enum
{
    FmtxPlaybackOk = 0,
    FmtxPlaybackMemory,
    FmtxPlaybackOpen,
    FmtxPlaybackRead,
    FmtxPlaybackInvalid,
    FmtxPlaybackRadio,
    FmtxPlaybackThread,
} PlayErr;

struct Play
{
    FuriThread *th;
    Rf *rf;
    PlayReq req;
    volatile bool stop;
    volatile bool on;
    volatile bool paused;
    volatile uint32_t err;
    uint8_t gain;
    Dsp dsp;
};

static void seterr(Play *playback, PlayErr err)
{
    if(playback->err == FmtxPlaybackOk) playback->err = err;
}

static bool skipid3(File *file)
{
    uint8_t h[10];
    size_t n = storage_file_read(file, h, sizeof(h));
    uint32_t size;
    if(n < sizeof(h))
    {
        if(n >= 3 && !memcmp(h, "ID3", 3)) return false;
        return storage_file_seek(file, 0, true);
    }
    if(memcmp(h, "ID3", 3)) return storage_file_seek(file, 0, true);
    for(unsigned i = 6; i < 10; i++)
        if(h[i] & 0x80U) return false;
    size = ((uint32_t)h[6] << 21) | ((uint32_t)h[7] << 14) | ((uint32_t)h[8] << 7) | h[9];
    if(size > UINT32_MAX - 10U) return false;
    size += 10U;
    if(h[5] & 0x10U)
    {
        if(size > UINT32_MAX - 10U) return false;
        size += 10U;
    }
    if(size > storage_file_size(file)) return false;
    return storage_file_seek(file, size, true);
}

static bool putsample(Play *p, int16_t s)
{
    while(!p->stop)
    {
        if(p->paused)
        {
            if(p->rf->on) rfpause(p->rf);
            furi_delay_tick(1U);
            continue;
        }
        if(!p->rf->on && !rfresume(p->rf)) return false;
        if(rfput(p->rf, s)) return true;
    }


    return false;
}

static bool outframe(Play *playback, const mp3d_sample_t *pcm, const mp3dec_frame_info_t *info, int samples, Rs *rs)
{
    uint32_t rate;
    if(info->hz <= 0 || (info->channels != 1 && info->channels != 2)) return false;
    rate = info->hz;
    if(rs->source_rate != rate)
    {
        rs->source_rate = rate;
        rs->filled = 0;
        rs->sum = 0;
    }
    for(int i = 0; i < samples && !playback->stop; i++)
    {
        int32_t mono = pcm[i * info->channels];
        uint32_t remaining = dsp_hz;
        if(info->channels == 2) mono = ((int32_t)pcm[i * 2] + (int32_t)pcm[i * 2 + 1]) / 2;
        while(remaining && !playback->stop)
        {
            uint32_t room = rate - rs->filled;
            uint32_t weight = remaining < room ? remaining : room;
            rs->sum += (int64_t)mono * weight;
            rs->filled += weight;
            remaining -= weight;
            if(rs->filled == rate)
            {
                int16_t s = dspsample(&playback->dsp, rs->sum / (int32_t)rate);
                if(!putsample(playback, s)) return false;
                rs->filled = 0;
                rs->sum = 0;
            }
        }
    }
    return !playback->stop;
}

static bool cleantail(const uint8_t *data, size_t size)
{
    if(size == 0) return true;
    bool zero = true;
    for(size_t i = 0; i < size; i++)
        if(data[i] != 0) zero = false;
    if(zero) return true;
    if(size < 128 || memcmp(data, "TAG", 3)) return false;
    for(size_t i = 128; i < size; i++)
        if(data[i] != 0) return false;
    return true;
}

static int32_t playthread(void *ctx)
{
    Play *playback = ctx;
    Storage *storage = furi_record_open(RECORD_STORAGE);
    File *file = storage ? storage_file_alloc(storage) : NULL;
    uint8_t *input = malloc(INSZ);
    mp3d_sample_t *pcm = malloc(MINIMP3_MAX_SAMPLES_PER_FRAME * sizeof(mp3d_sample_t));
    mp3dec_t *decoder = malloc(sizeof(mp3dec_t));
    size_t offset = 0;
    size_t buffered = 0;
    bool eof = false;
    bool read_failed = false;
    uint32_t frames = 0;
    uint32_t skipped_first = 0;
    uint32_t skipped_after = 0;
    Rs rs =
    {
        0
    };

    if(!file || !input || !pcm || !decoder)
    {
        seterr(playback, FmtxPlaybackMemory);
        goto done;
    }
    if(!storage_file_open(file, playback->req.path, FSAM_READ, FSOM_OPEN_EXISTING))
    {
        seterr(playback, FmtxPlaybackOpen);
        goto done;
    }
    if(!skipid3(file))
    {
        seterr(playback, FmtxPlaybackInvalid);
        goto done;
    }
    dsprst(&playback->dsp);
    rfrst(playback->rf);
    rfhold(playback->rf, 6U);
    playback->rf->hz = playback->req.hz;
    if(!rfstart(playback->rf))
    {
        seterr(playback, FmtxPlaybackRadio);
        goto done;
    }
    if(playback->paused) rfpause(playback->rf);

    mp3dec_init(decoder);
    while(!playback->stop)
    {
        if(!eof && buffered <= INSZ / 2U)
        {
            if(offset && buffered) memmove(input, input + offset, buffered);
            offset = 0;
            size_t n = storage_file_read(file, input + buffered, INSZ - buffered);
            buffered += n;
            if(n == 0)
            {
                if(!storage_file_eof(file) && storage_file_get_error(file) != FSE_OK)
                {
                    seterr(playback, FmtxPlaybackRead);
                    read_failed = true;
                    break;
                }
                eof = true;
            }
        }
        if(buffered == 0) break;

        mp3dec_frame_info_t info;
        int samples = mp3dec_decode_frame(decoder, input + offset, buffered, pcm, &info);
        if(samples > 0)
        {
            if(info.frame_bytes <= 0 || (size_t)info.frame_bytes > buffered || info.frame_offset < 0 || info.frame_offset > info.frame_bytes)
            {
                seterr(playback, FmtxPlaybackInvalid);
                break;
            }
            size_t skip = info.frame_offset;
            if(frames == 0)
            {
                if(skip > SKIP1 - skipped_first)
                {
                    seterr(playback, FmtxPlaybackInvalid);
                    break;
                }
                skipped_first += skip;
            }
            else if(skip > SKIPN - skipped_after)
            {
                seterr(playback, FmtxPlaybackInvalid);
                break;
            }
            if(!outframe(playback, pcm, &info, samples, &rs))
            {
                if(!playback->stop) seterr(playback, FmtxPlaybackInvalid);
                break;
            }
            offset += info.frame_bytes;
            buffered -= info.frame_bytes;
            frames++;
            skipped_after = 0;
        }
        else if(eof)
        {
            break;
        }
        else
        {
            size_t skip = info.frame_bytes > 0 ? (size_t)info.frame_bytes : 1U;
            if(skip > buffered)
            {
                seterr(playback, FmtxPlaybackInvalid);
                break;
            }
            if(frames == 0)
            {
                if(skip > SKIP1 - skipped_first)
                {
                    seterr(playback, FmtxPlaybackInvalid);
                    break;
                }
                skipped_first += skip;
            }
            else
            {
                if(skip > SKIPN - skipped_after)
                {
                    seterr(playback, FmtxPlaybackInvalid);
                    break;
                }
                skipped_after += skip;
            }
            offset += skip;
            buffered -= skip;
        }
        if(buffered == 0) offset = 0;
    }

    if(!playback->stop && !read_failed && playback->err == FmtxPlaybackOk)
    {
        if(frames == 0 || !cleantail(input + offset, buffered))
        {
            seterr(playback, FmtxPlaybackInvalid);
        }
        else
        {
            if(rs.filled)
            {
                int16_t s = dspsample(&playback->dsp, rs.sum / (int32_t)rs.filled);
                if(!putsample(playback, s)) goto done;
            }
            (void)rfdrain(playback->rf, 1000U);
        }
    }

done:
    rfstop(playback->rf);
    playback->paused = false;
    if(file)
    {
        storage_file_close(file);
        storage_file_free(file);
    }
    if(storage) furi_record_close(RECORD_STORAGE);
    free(decoder);
    free(pcm);
    free(input);
    __DMB();
    playback->on = false;
    return 0;
}

void playreq(PlayReq *request, const char *path, uint32_t hz)
{
    strlcpy(request->path, path, sizeof(request->path));
    request->hz = hz;
}

Play *playnew(void)
{
    Play *playback = calloc(1, sizeof(Play));
    if(!playback) return NULL;
    playback->rf = malloc(sizeof(Rf));
    if(!playback->rf)
    {
        free(playback);
        return NULL;
    }
    rfinit(playback->rf, 433160000U);
    playback->gain = 2;
    dspinit(&playback->dsp);
    return playback;
}

void playfree(Play *playback)
{
    if(!playback) return;
    playstop(playback);
    free(playback->rf);
    free(playback);
}

static bool start0(Play *playback, const PlayReq *request, bool paused)
{
    if(!playback || !request || playback->on) return false;
    if(playback->th)
    {
        furi_thread_join(playback->th);
        furi_thread_free(playback->th);
        playback->th = NULL;
    }
    memcpy(&playback->req, request, sizeof(*request));
    playback->stop = false;
    playback->paused = paused;
    playback->err = FmtxPlaybackOk;
    playback->on = true;
    playback->th = furi_thread_alloc_ex("FmtxDecode", STACKSZ, playthread, playback);
    if(!playback->th)
    {
        playback->on = false;
        seterr(playback, FmtxPlaybackThread);
        return false;
    }
    furi_thread_set_priority(playback->th, FuriThreadPriorityHigh);
    furi_thread_start(playback->th);
    return true;
}

bool playstart(Play *playback, const PlayReq *request)
{
    return start0(playback, request, false);
}

bool playpaused(Play *playback, const PlayReq *request)
{
    return start0(playback, request, true);
}

void playstop(Play *playback)
{
    if(!playback) return;
    playback->stop = true;
    __DMB();
    if(playback->th)
    {
        furi_thread_join(playback->th);
        furi_thread_free(playback->th);
        playback->th = NULL;
    }
    playback->on = false;
    playback->paused = false;
}

bool playon(const Play *playback)
{
    if(!playback) return false;
    __DMB();
    return playback->on;
}

bool ispaused(const Play *playback)
{
    if(!playback) return false;
    __DMB();


    return playback->paused;
}

bool playenter(Play *playback)
{
    PlayReq req;
    if(!playback || !playback->req.path[0]) return false;
    if(playback->on)
    {
        playback->paused = !playback->paused;
        __DMB();
        if(playback->paused && playback->rf->on) rfpause(playback->rf);
        return true;
    }
    memcpy(&req, &playback->req, sizeof(req));


    return playstart(playback, &req);
}

bool playseek(Play *playback, int32_t frames)
{
    PlayReq req;
    bool paused;
    if(!playback || !playback->req.path[0]) return false;
    if(frames >= 0) return true;
    paused = playback->paused;
    memcpy(&req, &playback->req, sizeof(req));
    playstop(playback);


    return start0(playback, &req, paused);
}

uint32_t playms(const Play *playback)
{
    if(!playback) return 0;
    return (rfplayed(playback->rf) * 1000U) / dsp_hz;
}

uint8_t playgain(const Play *playback)
{
    return playback ? playback->gain : 2;
}

uint8_t gainup(Play *playback)
{
    if(!playback) return 2;
    playback->gain = playback->gain == 2 ? 3 : playback->gain == 3 ? 4 : playback->gain == 4 ? 6 : playback->gain == 6 ? 8 : 2;
    rfgain(playback->rf, playback->gain);

    return playback->gain;
}

bool playfilter(const Play *playback)
{
    return playback && dspon(&playback->dsp);
}

bool filtertoggle(Play *playback)
{
    return playback && dsptoggle(&playback->dsp);
}
