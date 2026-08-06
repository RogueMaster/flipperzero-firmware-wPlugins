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
#include "../lib/minimp3/minimp3.h"

#define INSZ    8192U
#define STACKSZ (24U * 1024U)
#define SKIP1   65536U
#define SKIPN   8192U
#define SEEKBUF 8192U

typedef struct {
    uint32_t source_rate;
    uint32_t filled;
    int64_t sum;
} Rs;

typedef enum {
    FmtxPlaybackOk = 0,
    FmtxPlaybackMemory,
    FmtxPlaybackOpen,
    FmtxPlaybackRead,
    FmtxPlaybackInvalid,
    FmtxPlaybackRadio,
    FmtxPlaybackThread,
} PlayErr;

typedef enum {
    PlaybackStopped,
    PlaybackPlaying,
    PlaybackPaused,
    PlaybackSeeking,
    PlaybackFinished,
} PlaybackState;

struct Play {
    FuriThread* th;
    Rf* rf;
    PlayReq req;
    volatile bool stop;
    volatile PlaybackState state;
    PlaybackState after_seek;
    volatile uint32_t err;
    uint32_t seekat;
    volatile uint32_t want;
    volatile uint32_t seekfrm;
    uint32_t ndec;
    uint32_t nsent;
    volatile uint32_t fhz;
    volatile uint16_t fsamp;
    volatile uint32_t total;
    volatile uint32_t ended;
    volatile bool radio;
    uint8_t gain;
    Dsp dsp;
    int16_t cache[SEEKBUF];
};

static void seterr(Play* playback, PlayErr err) {
    if(playback->err == FmtxPlaybackOk) playback->err = err;
}

static bool isrunning(const Play* playback) {
    return playback->state == PlaybackPlaying || playback->state == PlaybackPaused ||
           playback->state == PlaybackSeeking;
}

static bool skipid3(File* file) {
    uint8_t h[10];
    size_t n = storage_file_read(file, h, sizeof(h));
    uint32_t size;
    if(n < sizeof(h)) {
        if(n >= 3 && !memcmp(h, "ID3", 3)) return false;
        return storage_file_seek(file, 0, true);
    }
    if(memcmp(h, "ID3", 3)) return storage_file_seek(file, 0, true);
    for(unsigned i = 6; i < 10; i++)
        if(h[i] & 0x80U) return false;
    // ID3v2 stores its size as four syncsafe bytes.
    size = ((uint32_t)h[6] << 21) | ((uint32_t)h[7] << 14) | ((uint32_t)h[8] << 7) | h[9];
    if(size > UINT32_MAX - 10U) return false;
    size += 10U;
    if(h[5] & 0x10U) {
        if(size > UINT32_MAX - 10U) return false;
        size += 10U;
    }
    if(size > storage_file_size(file)) return false;
    return storage_file_seek(file, size, true);
}

static uint32_t cachefirst(Play* p) {
    return p->ndec > SEEKBUF ? p->ndec - SEEKBUF : 0;
}

static bool pumpradio(Play* p) {
    while(!p->stop) {
        if(p->state == PlaybackSeeking) return true;
        if(p->state == PlaybackPaused) {
            if(p->rf->on) rfpause(p->rf);
            furi_delay_tick(1U);
            continue;
        }
        if(p->nsent >= p->ndec) return true;
        // Falling behind the cache window cannot be recovered.
        if(p->nsent < cachefirst(p)) {
            seterr(p, FmtxPlaybackInvalid);
            return false;
        }
        if(!p->radio) {
            if(!rfstart(p->rf)) {
                seterr(p, FmtxPlaybackRadio);
                return false;
            }
            p->radio = true;
        } else if(!p->rf->on && !rfresume(p->rf)) {
            seterr(p, FmtxPlaybackRadio);
            return false;
        }
        if(!rfput(p->rf, p->cache[p->nsent & (SEEKBUF - 1U)])) continue;
        p->nsent++;
    }

    return false;
}

static bool putsample(Play* p, int16_t s) {
    p->cache[p->ndec & (SEEKBUF - 1U)] = s;
    p->ndec++;

    return pumpradio(p);
}

static bool outframe(
    Play* playback,
    const mp3d_sample_t* pcm,
    const mp3dec_frame_info_t* info,
    int samples,
    Rs* rs) {
    uint32_t rate;
    if(info->hz <= 0 || (info->channels != 1 && info->channels != 2)) return false;
    rate = info->hz;
    if(rs->source_rate != rate) {
        rs->source_rate = rate;
        rs->filled = 0;
        rs->sum = 0;
    }
    // Weighted buckets resample arbitrary MP3 rates to 8 kHz.
    for(int i = 0; i < samples && !playback->stop && playback->state != PlaybackSeeking; i++) {
        int32_t mono = pcm[i * info->channels];
        uint32_t remaining = dsp_hz;
        if(info->channels == 2) mono = ((int32_t)pcm[i * 2] + (int32_t)pcm[i * 2 + 1]) / 2;
        while(remaining && !playback->stop) {
            uint32_t room = rate - rs->filled;
            uint32_t weight = remaining < room ? remaining : room;
            rs->sum += (int64_t)mono * weight;
            rs->filled += weight;
            remaining -= weight;
            if(rs->filled == rate) {
                int16_t s = dspsample(&playback->dsp, rs->sum / (int32_t)rate);
                rs->filled = 0;
                rs->sum = 0;
                if(!putsample(playback, s)) return false;
            }
        }
    }
    return !playback->stop;
}

// Accept only zero padding or one final ID3v1 tag after the audio.
static bool cleantail(const uint8_t* data, size_t size) {
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

static int32_t playthread(void* ctx) {
    Play* playback = ctx;
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage ? storage_file_alloc(storage) : NULL;
    uint8_t* input = malloc(INSZ);
    mp3d_sample_t* pcm = malloc(MINIMP3_MAX_SAMPLES_PER_FRAME * sizeof(mp3d_sample_t));
    mp3dec_t* decoder = malloc(sizeof(mp3dec_t));
    size_t offset = 0;
    size_t buffered = 0;
    bool eof = false;
    bool read_failed = false;
    uint32_t frames = 0;
    uint32_t seek_frames = 0;
    uint32_t skipped_first = 0;
    uint32_t skipped_after = 0;
    bool finished = false;
    Rs rs = {0};

    if(!file || !input || !pcm || !decoder) {
        seterr(playback, FmtxPlaybackMemory);
        goto done;
    }
    if(!storage_file_open(file, playback->req.path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        seterr(playback, FmtxPlaybackOpen);
        goto done;
    }
    if(!skipid3(file)) {
        seterr(playback, FmtxPlaybackInvalid);
        goto done;
    }
    dsprst(&playback->dsp);
    rfrst(playback->rf);
    rfhold(playback->rf, 6U);
    playback->rf->hz = playback->req.hz;
    playback->ndec = 0;
    playback->nsent = playback->seekat;
    playback->radio = false;

    mp3dec_init(decoder);
    while(!playback->stop) {
        // Forward seeks keep decoder history and discard frames to the target.
        if(playback->state == PlaybackSeeking) {
            __DMB();
            seek_frames = playback->seekfrm;
            rfstop(playback->rf);
            rfrst(playback->rf);
            playback->seekat = playback->want;
            playback->ndec = playback->want;
            playback->nsent = playback->want;
            playback->radio = false;
            dsprst(&playback->dsp);
            rs = (Rs){0};
            __DMB();
            playback->state = playback->after_seek;
        }
        if(!eof && buffered <= INSZ / 2U) {
            if(offset && buffered) memmove(input, input + offset, buffered);
            offset = 0;
            size_t n = storage_file_read(file, input + buffered, INSZ - buffered);
            buffered += n;
            if(n == 0) {
                if(!storage_file_eof(file) && storage_file_get_error(file) != FSE_OK) {
                    seterr(playback, FmtxPlaybackRead);
                    read_failed = true;
                    break;
                }
                eof = true;
            }
        }
        if(buffered == 0) break;

        mp3dec_frame_info_t info = {0};
        int samples = mp3dec_decode_frame(decoder, input + offset, buffered, pcm, &info);
        if(samples > 0) {
            if(info.frame_bytes <= 0 || (size_t)info.frame_bytes > buffered ||
               info.frame_offset < 0 || info.frame_offset > info.frame_bytes) {
                seterr(playback, FmtxPlaybackInvalid);
                break;
            }
            size_t skip = info.frame_offset;
            if(frames == 0) {
                if(skip > SKIP1 - skipped_first) {
                    seterr(playback, FmtxPlaybackInvalid);
                    break;
                }
                skipped_first += skip;
            } else if(skip > SKIPN - skipped_after) {
                seterr(playback, FmtxPlaybackInvalid);
                break;
            }
            playback->fhz = info.hz;
            playback->fsamp = samples;
            __DMB();
            if(frames >= seek_frames && !outframe(playback, pcm, &info, samples, &rs)) {
                if(!playback->stop) seterr(playback, FmtxPlaybackInvalid);
                break;
            }
            offset += info.frame_bytes;
            buffered -= info.frame_bytes;
            frames++;
            skipped_after = 0;
        } else if(eof) {
            break;
        } else {
            size_t skip = info.frame_bytes > 0 ? (size_t)info.frame_bytes : 1U;
            if(skip > buffered) {
                seterr(playback, FmtxPlaybackInvalid);
                break;
            }
            if(frames == 0) {
                if(skip > SKIP1 - skipped_first) {
                    seterr(playback, FmtxPlaybackInvalid);
                    break;
                }
                skipped_first += skip;
            } else {
                if(skip > SKIPN - skipped_after) {
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

    if(!playback->stop && !read_failed && playback->err == FmtxPlaybackOk) {
        if(frames == 0 || !cleantail(input + offset, buffered)) {
            seterr(playback, FmtxPlaybackInvalid);
        } else {
            if(rs.filled) {
                int16_t s = dspsample(&playback->dsp, rs.sum / (int32_t)rs.filled);
                if(!putsample(playback, s)) goto done;
            }
            if(playback->radio) {
                if(!pumpradio(playback)) goto done;
                (void)rfdrain(playback->rf, 1000U);
            }
            playback->total = playback->ndec;
            __DMB();
            finished = true;
        }
    }

done:
    rfstop(playback->rf);
    if(finished) {
        playback->ended = playback->ndec;
    } else {
        uint64_t at = playback->seekat + (uint64_t)rfplayed(playback->rf);
        if(playback->total && at > playback->total) at = playback->total;
        playback->ended = at;
    }
    playback->radio = false;
    if(file) {
        storage_file_close(file);
        storage_file_free(file);
    }
    if(storage) furi_record_close(RECORD_STORAGE);
    free(decoder);
    free(pcm);
    free(input);
    __DMB();
    playback->state = finished ? PlaybackFinished : PlaybackStopped;
    return 0;
}

void fmtx_playback_request_init(PlayReq* request, const char* path, uint32_t hz) {
    strlcpy(request->path, path, sizeof(request->path));
    request->hz = hz;
}

Play* fmtx_playback_alloc(void) {
    Play* playback = calloc(1, sizeof(Play));
    if(!playback) return NULL;
    playback->rf = malloc(sizeof(Rf));
    if(!playback->rf) {
        free(playback);
        return NULL;
    }
    rfinit(playback->rf, 433160000U);
    playback->gain = 3;
    rfgain(playback->rf, playback->gain);
    dspinit(&playback->dsp);
    return playback;
}

void fmtx_playback_free(Play* playback) {
    if(!playback) return;
    fmtx_playback_stop(playback);
    free(playback->rf);
    free(playback);
}

static uint32_t frameat(const Play* p, uint32_t frame) {
    uint32_t hz = p->fhz ? p->fhz : 44100U;
    uint32_t n = p->fsamp ? p->fsamp : 1152U;
    uint64_t at = (uint64_t)frame * n * dsp_hz / hz;

    return at > UINT32_MAX ? UINT32_MAX : at;
}

static uint32_t atframe(const Play* p, uint32_t at) {
    uint32_t hz = p->fhz ? p->fhz : 44100U;
    uint32_t n = p->fsamp ? p->fsamp : 1152U;
    uint64_t one = (uint64_t)n * dsp_hz;
    uint64_t frame = ((uint64_t)at * hz + one / 2U) / one;

    return frame > UINT32_MAX ? UINT32_MAX : frame;
}

static bool startat(Play* playback, const PlayReq* request, uint32_t at, bool paused) {
    bool same;
    if(!playback || !request || isrunning(playback)) return false;
    if(playback->th) {
        furi_thread_join(playback->th);
        furi_thread_free(playback->th);
        playback->th = NULL;
    }
    // Reusing the same file keeps its discovered duration and frame geometry.
    same = !strcmp(playback->req.path, request->path);
    if(!same) {
        playback->total = 0;
        playback->fhz = 0;
        playback->fsamp = 0;
    }
    if(playback->total && at > playback->total) at = playback->total;
    memcpy(&playback->req, request, sizeof(*request));
    playback->stop = false;
    playback->state = paused ? PlaybackPaused : PlaybackPlaying;
    playback->after_seek = playback->state;
    playback->err = FmtxPlaybackOk;
    playback->seekat = at;
    playback->want = at;
    playback->seekfrm = 0;
    playback->ended = at;
    playback->ndec = 0;
    playback->nsent = at;
    playback->radio = false;
    if(playback->total && at == playback->total) {
        playback->state = PlaybackFinished;
        return true;
    }
    playback->th = furi_thread_alloc_ex("FmtxDecode", STACKSZ, playthread, playback);
    if(!playback->th) {
        playback->state = PlaybackStopped;
        seterr(playback, FmtxPlaybackThread);
        return false;
    }
    furi_thread_set_priority(playback->th, FuriThreadPriorityHigh);
    furi_thread_start(playback->th);
    return true;
}

bool fmtx_playback_start(Play* playback, const PlayReq* request) {
    return startat(playback, request, 0, false);
}

bool fmtx_playback_start_paused(Play* playback, const PlayReq* request) {
    return startat(playback, request, 0, true);
}

void fmtx_playback_stop(Play* playback) {
    if(!playback) return;
    playback->stop = true;
    __DMB();
    if(playback->th) {
        furi_thread_join(playback->th);
        furi_thread_free(playback->th);
        playback->th = NULL;
    }
    playback->state = PlaybackStopped;
}

bool fmtx_playback_is_running(const Play* playback) {
    if(!playback) return false;
    __DMB();
    return isrunning(playback);
}

bool fmtx_playback_is_transmitting(const Play* playback) {
    if(!playback) return false;
    __DMB();
    return playback->state == PlaybackPlaying && playback->radio && playback->rf->on;
}

bool fmtx_playback_is_paused(const Play* playback) {
    if(!playback) return false;
    __DMB();
    return playback->state == PlaybackPaused ||
           (playback->state == PlaybackSeeking && playback->after_seek == PlaybackPaused);
}

bool fmtx_playback_toggle_pause(Play* playback) {
    PlayReq req;
    if(!playback || !playback->req.path[0]) return false;
    if(isrunning(playback)) {
        if(playback->state == PlaybackSeeking)
            playback->after_seek = playback->after_seek == PlaybackPaused ? PlaybackPlaying :
                                                                            PlaybackPaused;
        else
            playback->state = playback->state == PlaybackPaused ? PlaybackPlaying : PlaybackPaused;
        __DMB();
        return true;
    }
    memcpy(&req, &playback->req, sizeof(req));

    return startat(playback, &req, 0, false);
}

static uint32_t playbackat(const Play* playback) {
    uint64_t at;
    if(!isrunning(playback)) return playback->ended;
    if(playback->state == PlaybackSeeking) return playback->want;
    at = playback->seekat + (uint64_t)rfplayed(playback->rf);
    if(playback->total && at > playback->total) at = playback->total;

    return at;
}

bool fmtx_playback_seek_frames(Play* playback, int32_t frames) {
    PlayReq req;
    uint64_t frame;
    bool paused;
    uint32_t now;
    uint32_t oldframe;
    if(!playback || !playback->req.path[0]) return false;
    if(frames < 0) {
        paused = fmtx_playback_is_paused(playback);
        memcpy(&req, &playback->req, sizeof(req));
        fmtx_playback_stop(playback);

        return startat(playback, &req, 0, paused);
    }
    if(frames == 0) return true;
    now = playbackat(playback);
    oldframe = atframe(playback, now);
    frame = (uint64_t)oldframe + frames;
    if(playback->total) {
        uint32_t x1 = atframe(playback, playback->total);
        if(frame > x1) frame = x1;
    }
    if((uint32_t)frame == oldframe || !isrunning(playback)) return true;
    playback->seekfrm = frame;
    playback->want = frameat(playback, frame);
    if(playback->total && playback->want > playback->total) playback->want = playback->total;
    __DMB();
    playback->after_seek = playback->state == PlaybackPaused ? PlaybackPaused : PlaybackPlaying;
    playback->state = PlaybackSeeking;
    __DMB();

    return true;
}

uint32_t fmtx_playback_position_ms(const Play* playback) {
    if(!playback) return 0;
    return ((uint64_t)playbackat(playback) * 1000U) / dsp_hz;
}

uint32_t fmtx_playback_duration_ms(const Play* playback) {
    if(!playback) return 0;
    __DMB();
    return ((uint64_t)playback->total * 1000U) / dsp_hz;
}

uint8_t fmtx_playback_gain(const Play* playback) {
    return playback ? playback->gain : 2;
}

uint8_t fmtx_playback_cycle_gain(Play* playback) {
    if(!playback) return 2;
    playback->gain = playback->gain == 2 ? 3 :
                     playback->gain == 3 ? 4 :
                     playback->gain == 4 ? 6 :
                     playback->gain == 6 ? 8 :
                                           2;
    rfgain(playback->rf, playback->gain);

    return playback->gain;
}

bool fmtx_playback_filter_enabled(const Play* playback) {
    return playback && dspon(&playback->dsp);
}

bool fmtx_playback_toggle_filter(Play* playback) {
    return playback && dsptoggle(&playback->dsp);
}
