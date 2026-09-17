#include "radio_player.h"
#include "radio_audio.h"

#include <furi.h>
#include <stdlib.h>
#include <string.h>

#define MINIMP3_IMPLEMENTATION
#define MINIMP3_ONLY_MP3
#define MINIMP3_NO_SIMD
#include "third_party/minimp3/minimp3.h"

#define RADIO_COMPRESSED_RING 12288U
#define RADIO_DECODE_WINDOW   4096U
#define RADIO_PREBUFFER_BYTES 11264U
#define RADIO_THREAD_STACK    (24U * 1024U)

typedef struct {
    uint32_t source_rate;
    uint32_t filled;
    int64_t sum;
} RadioResampler;

struct RadioPlayer {
    FuriThread* thread;
    FuriMutex* mutex;
    FuriSemaphore* data_ready;
    RadioAudio* audio;
    uint8_t* ring;
    size_t head;
    size_t tail;
    size_t used;
    volatile bool running;
    volatile bool stop_requested;
    volatile uint32_t decoded_frames;
};

static size_t radio_player_take(RadioPlayer* player, uint8_t* output, size_t maximum) {
    furi_mutex_acquire(player->mutex, FuriWaitForever);
    const size_t count = player->used < maximum ? player->used : maximum;
    const size_t first = count < RADIO_COMPRESSED_RING - player->tail ?
                             count :
                             RADIO_COMPRESSED_RING - player->tail;
    memcpy(output, player->ring + player->tail, first);
    memcpy(output + first, player->ring, count - first);
    player->tail = (player->tail + count) % RADIO_COMPRESSED_RING;
    player->used -= count;
    furi_mutex_release(player->mutex);
    return count;
}

static size_t radio_player_buffered(RadioPlayer* player) {
    furi_mutex_acquire(player->mutex, FuriWaitForever);
    const size_t used = player->used;
    furi_mutex_release(player->mutex);
    return used;
}

static bool radio_output_frame(
    RadioPlayer* player,
    const mp3d_sample_t* pcm,
    const mp3dec_frame_info_t* info,
    int samples,
    RadioResampler* resampler) {
    if(info->hz <= 0 || info->channels <= 0) return true;
    const uint32_t source_rate = (uint32_t)info->hz;
    const uint32_t output_rate = radio_audio_sample_rate();
    if(resampler->source_rate != source_rate) {
        resampler->source_rate = source_rate;
        resampler->filled = 0U;
        resampler->sum = 0;
    }
    for(int index = 0; index < samples && !player->stop_requested; ++index) {
        int32_t mono = pcm[index * info->channels];
        if(info->channels == 2) mono = (mono + pcm[index * 2 + 1]) / 2;
        uint32_t remaining = output_rate;
        while(remaining && !player->stop_requested) {
            const uint32_t room = source_rate - resampler->filled;
            const uint32_t weight = remaining < room ? remaining : room;
            resampler->sum += (int64_t)mono * weight;
            resampler->filled += weight;
            remaining -= weight;
            if(resampler->filled == source_rate) {
                const int16_t sample = (int16_t)(resampler->sum / (int32_t)source_rate);
                if(!radio_audio_write(player->audio, sample)) return false;
                resampler->filled = 0U;
                resampler->sum = 0;
            }
        }
    }
    return !player->stop_requested;
}

static int32_t radio_player_thread(void* context) {
    RadioPlayer* player = context;
    uint8_t* input = malloc(RADIO_DECODE_WINDOW);
    mp3d_sample_t* pcm = malloc(MINIMP3_MAX_SAMPLES_PER_FRAME * sizeof(mp3d_sample_t));
    mp3dec_t* decoder = malloc(sizeof(mp3dec_t));
    if(!input || !pcm || !decoder || !radio_audio_start(player->audio, 100U)) goto done;
    mp3dec_init(decoder);
    /* Internet radio arrives in bursts. Build a compressed reservoir before
     * starting the real-time decoder so short network/USB stalls stay silent. */
    while(!player->stop_requested && radio_player_buffered(player) < RADIO_PREBUFFER_BYTES) {
        furi_semaphore_acquire(player->data_ready, furi_ms_to_ticks(100U));
    }
    size_t buffered = 0U;
    RadioResampler resampler = {0};
    while(!player->stop_requested) {
        if(buffered < RADIO_DECODE_WINDOW) {
            buffered +=
                radio_player_take(player, input + buffered, RADIO_DECODE_WINDOW - buffered);
        }
        if(buffered < 1024U) {
            furi_semaphore_acquire(player->data_ready, furi_ms_to_ticks(100U));
            continue;
        }
        mp3dec_frame_info_t info;
        const int samples = mp3dec_decode_frame(decoder, input, buffered, pcm, &info);
        if(info.frame_bytes > 0 && (size_t)info.frame_bytes <= buffered) {
            if(samples > 0) {
                player->decoded_frames++;
                if(!radio_output_frame(player, pcm, &info, samples, &resampler)) break;
            }
            memmove(input, input + info.frame_bytes, buffered - (size_t)info.frame_bytes);
            buffered -= (size_t)info.frame_bytes;
        } else if(buffered == RADIO_DECODE_WINDOW) {
            memmove(input, input + 1U, --buffered);
        } else {
            furi_semaphore_acquire(player->data_ready, furi_ms_to_ticks(50U));
        }
    }
done:
    radio_audio_stop(player->audio);
    free(decoder);
    free(pcm);
    free(input);
    player->running = false;
    return 0;
}

RadioPlayer* radio_player_alloc(void) {
    RadioPlayer* player = calloc(1, sizeof(RadioPlayer));
    if(!player) return NULL;
    player->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    player->data_ready = furi_semaphore_alloc(1U, 0U);
    player->audio = radio_audio_alloc();
    player->ring = malloc(RADIO_COMPRESSED_RING);
    if(!player->mutex || !player->data_ready || !player->audio || !player->ring) {
        radio_player_free(player);
        return NULL;
    }
    return player;
}

static bool radio_player_reap_stopped(RadioPlayer* player) {
    if(!player->thread) return true;
    if(furi_thread_get_state(player->thread) != FuriThreadStateStopped) return false;
    furi_thread_join(player->thread);
    furi_thread_free(player->thread);
    player->thread = NULL;
    return true;
}

bool radio_player_start(RadioPlayer* player) {
    if(!player || player->running) return false;
    /* A GUI Stop is intentionally non-blocking. Normally the decoder exits in
     * a few ticks; give a subsequent Play a short bounded chance to reap it. */
    for(uint8_t attempt = 0U; player->thread && attempt < 10U; ++attempt) {
        if(radio_player_reap_stopped(player)) break;
        furi_delay_ms(10U);
    }
    if(!radio_player_reap_stopped(player)) return false;
    player->head = player->tail = player->used = 0U;
    player->decoded_frames = 0U;
    player->stop_requested = false;
    player->running = true;
    player->thread =
        furi_thread_alloc_ex("FibRadioDecoder", RADIO_THREAD_STACK, radio_player_thread, player);
    if(!player->thread) {
        player->running = false;
        return false;
    }
    /* Audio must refill the small PCM reservoir before UI/transport work.
     * The thread normally blocks while that reservoir is full, so High does
     * not starve USB; it only prevents a short GUI burst from causing a gap. */
    furi_thread_set_priority(player->thread, FuriThreadPriorityHigh);
    furi_thread_start(player->thread);
    return true;
}

void radio_player_request_stop(RadioPlayer* player) {
    if(!player) return;
    const bool first_request = !player->stop_requested;
    player->stop_requested = true;
    if(player->audio) radio_audio_request_stop(player->audio);
    /* A binary semaphore may already be full after the first Stop/Back.
     * Releasing it twice triggers a Furi check, so only the first active stop
     * request is allowed to wake the decoder. */
    if(first_request && player->running && player->data_ready) {
        furi_semaphore_release(player->data_ready);
    }
}

void radio_player_stop(RadioPlayer* player) {
    if(!player) return;
    /* Unblock PCM writes, then let the decoder thread release the speaker
     * and timer resources that it acquired. */
    radio_player_request_stop(player);
    if(player->thread) {
        furi_thread_join(player->thread);
        furi_thread_free(player->thread);
        player->thread = NULL;
    }
    player->running = false;
}

void radio_player_free(RadioPlayer* player) {
    if(!player) return;
    radio_player_stop(player);
    free(player->ring);
    radio_audio_free(player->audio);
    if(player->data_ready) furi_semaphore_free(player->data_ready);
    if(player->mutex) furi_mutex_free(player->mutex);
    free(player);
}

bool radio_player_push(RadioPlayer* player, const uint8_t* data, size_t length) {
    if(!player || !data || !player->running) return false;
    size_t offset = 0U;
    while(offset < length && !player->stop_requested) {
        furi_mutex_acquire(player->mutex, FuriWaitForever);
        const size_t free_bytes = RADIO_COMPRESSED_RING - player->used;
        const size_t count = (length - offset < free_bytes) ? length - offset : free_bytes;
        const size_t first = count < RADIO_COMPRESSED_RING - player->head ?
                                 count :
                                 RADIO_COMPRESSED_RING - player->head;
        memcpy(player->ring + player->head, data + offset, first);
        memcpy(player->ring, data + offset + first, count - first);
        player->head = (player->head + count) % RADIO_COMPRESSED_RING;
        player->used += count;
        furi_mutex_release(player->mutex);
        offset += count;
        furi_semaphore_release(player->data_ready);
        if(count == 0U) furi_delay_tick(1U);
    }
    return offset == length;
}

bool radio_player_is_running(const RadioPlayer* player) {
    return player && player->running;
}

uint32_t radio_player_decoded_frames(const RadioPlayer* player) {
    return player ? player->decoded_frames : 0U;
}

size_t radio_player_buffered_bytes(RadioPlayer* player) {
    return player ? radio_player_buffered(player) : 0U;
}

uint32_t radio_player_underflows(const RadioPlayer* player) {
    return player ? radio_audio_underflows(player->audio) : 0U;
}
