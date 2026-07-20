#include "mp3_playback.h"

#include <storage/storage.h>

#define MINIMP3_IMPLEMENTATION
#define MINIMP3_ONLY_MP3
#define MINIMP3_NO_SIMD
#include "minimp3.h"

#define MP3_INPUT_BUFFER 8192U
#define PLAYBACK_STACK_SIZE (24U * 1024U)

typedef struct {
  uint32_t source_rate;
  uint32_t filled;
  int64_t sum;
} PlaybackResampler;

typedef enum {
  PlaybackErrorNone,
  PlaybackErrorNoMemory,
  PlaybackErrorOpenFailed,
  PlaybackErrorReadFailed,
  PlaybackErrorOutputBusy,
  PlaybackErrorPowerUnavailable,
  PlaybackErrorInvalidMp3,
  PlaybackErrorThreadFailed,
} PlaybackError;

#define STREAM_INFO_RATE_MASK 0xFFFFU
#define STREAM_INFO_BITRATE_MASK 0x1FFU
#define STREAM_INFO_BITRATE_SHIFT 16U
#define STREAM_INFO_CHANNELS_SHIFT 25U

struct Mp3Playback {
  FuriThread *thread;
  AudioBackend *audio;
  volatile bool stop_requested;
  volatile bool running;
  volatile bool paused;
  AudioOutput output;
  uint8_t volume;
  char path[256];
  uint32_t error;
  uint32_t stream_info;
  uint32_t duration_ms;
  uint32_t start_position_ms;
  uint32_t known_duration_ms;
};

static void playback_set_error(Mp3Playback *playback, PlaybackError error) {
  uint32_t expected = PlaybackErrorNone;
  __atomic_compare_exchange_n(&playback->error, &expected, error, false,
                              __ATOMIC_RELEASE, __ATOMIC_RELAXED);
}

static void playback_publish_info(Mp3Playback *playback,
                                  const mp3dec_frame_info_t *info) {
  const uint32_t packed =
      ((uint32_t)info->hz & STREAM_INFO_RATE_MASK) |
      (((uint32_t)info->bitrate_kbps & STREAM_INFO_BITRATE_MASK)
       << STREAM_INFO_BITRATE_SHIFT) |
      (((uint32_t)info->channels & 0x3U) << STREAM_INFO_CHANNELS_SHIFT);
  __atomic_store_n(&playback->stream_info, packed, __ATOMIC_RELEASE);
}

static bool playback_seek_past_id3(File *file) {
  uint8_t header[10];
  const size_t read = storage_file_read(file, header, sizeof(header));
  if (read < sizeof(header))
    return storage_file_seek(file, 0, true);

  if (memcmp(header, "ID3", 3) != 0)
    return storage_file_seek(file, 0, true);

  for (uint8_t index = 6; index < 10; index++) {
    if (header[index] & 0x80U)
      return storage_file_seek(file, 0, true);
  }

  uint32_t tag_size = ((uint32_t)header[6] << 21U) |
                      ((uint32_t)header[7] << 14U) |
                      ((uint32_t)header[8] << 7U) | header[9];
  tag_size += sizeof(header);
  if (header[5] & 0x10U)
    tag_size += 10U;

  if (tag_size > storage_file_size(file))
    return false;
  return storage_file_seek(file, tag_size, true);
}

static bool playback_output_frame(Mp3Playback *playback,
                                  const mp3d_sample_t *pcm,
                                  const mp3dec_frame_info_t *info, int samples,
                                  PlaybackResampler *resampler,
                                  uint32_t output_rate) {
  if (info->hz <= 0 || info->channels <= 0)
    return true;

  const uint32_t source_rate = (uint32_t)info->hz;
  if (resampler->source_rate != source_rate) {
    resampler->source_rate = source_rate;
    resampler->filled = 0;
    resampler->sum = 0;
  }

  for (int index = 0; index < samples && !playback->stop_requested; index++) {
    while (playback->paused && !playback->stop_requested)
      furi_delay_tick(1);
    if (playback->stop_requested)
      break;

    int32_t mono = pcm[index * info->channels];
    if (info->channels == 2)
      mono = (mono + pcm[index * 2 + 1]) / 2;

    /* Integrate each source sample into exact output-width windows. Unlike
       point sampling, this averages the source energy that belongs to each
       output sample and prevents ultrasonic MP3 content from folding into
       harsh audible aliases. */
    uint32_t remaining = output_rate;
    while (remaining && !playback->stop_requested) {
      const uint32_t room = source_rate - resampler->filled;
      const uint32_t weight = remaining < room ? remaining : room;
      resampler->sum += (int64_t)mono * weight;
      resampler->filled += weight;
      remaining -= weight;

      if (resampler->filled == source_rate) {
        const int16_t output = (int16_t)(resampler->sum / (int32_t)source_rate);
        if (!audio_backend_write(playback->audio, output))
          return false;
        resampler->filled = 0;
        resampler->sum = 0;
      }
    }
  }
  return !playback->stop_requested;
}

static int32_t playback_thread(void *context) {
  Mp3Playback *playback = context;
  Storage *storage = furi_record_open(RECORD_STORAGE);
  File *file = storage_file_alloc(storage);
  uint8_t *input = malloc(MP3_INPUT_BUFFER);
  mp3d_sample_t *pcm =
      malloc(MINIMP3_MAX_SAMPLES_PER_FRAME * sizeof(mp3d_sample_t));
  mp3dec_t *decoder = malloc(sizeof(mp3dec_t));

  if (!input || !pcm || !decoder) {
    playback_set_error(playback, PlaybackErrorNoMemory);
    goto cleanup;
  }
  if (!storage_file_open(file, playback->path, FSAM_READ, FSOM_OPEN_EXISTING)) {
    playback_set_error(playback, PlaybackErrorOpenFailed);
    goto cleanup;
  }
  if (!playback_seek_past_id3(file)) {
    playback_set_error(playback, PlaybackErrorInvalidMp3);
    goto cleanup;
  }
  const uint64_t audio_start = storage_file_tell(file);
  const uint64_t file_size = storage_file_size(file);
  const uint64_t total_audio_bytes =
      file_size > audio_start ? file_size - audio_start : 0;
  if (playback->start_position_ms && playback->known_duration_ms &&
      total_audio_bytes) {
    const uint64_t relative =
        (total_audio_bytes * playback->start_position_ms) /
        playback->known_duration_ms;
    if (!storage_file_seek(file, audio_start + relative, true)) {
      playback_set_error(playback, PlaybackErrorReadFailed);
      goto cleanup;
    }
  }
  if (!audio_backend_start(playback->audio, playback->output,
                           playback->volume)) {
    playback_set_error(playback, audio_backend_get_error(playback->audio) ==
                                         AudioBackendErrorPowerUnavailable
                                     ? PlaybackErrorPowerUnavailable
                                     : PlaybackErrorOutputBusy);
    goto cleanup;
  }

  mp3dec_init(decoder);
  const uint32_t output_rate = audio_backend_get_sample_rate(playback->output);
  size_t offset = 0;
  size_t buffered = 0;
  bool eof = false;
  uint32_t decoded_frames = 0;
  uint64_t decoded_audio_bytes = 0;
  uint64_t decoded_time_us = 0;
  PlaybackResampler resampler = {0};

  while (!playback->stop_requested) {
    /* Refill in large chunks. Compacting a nearly-full 16 KB buffer after
       every MP3 frame created avoidable CPU bursts that could starve audio. */
    if (!eof && buffered <= MP3_INPUT_BUFFER / 2U) {
      if (offset && buffered)
        memmove(input, input + offset, buffered);
      offset = 0;
      const size_t count = storage_file_read(file, input + buffered,
                                             MP3_INPUT_BUFFER - buffered);
      buffered += count;
      if (count == 0) {
        if (!storage_file_eof(file) && storage_file_get_error(file) != FSE_OK)
          playback_set_error(playback, PlaybackErrorReadFailed);
        eof = true;
      }
    }
    if (buffered == 0)
      break;

    mp3dec_frame_info_t info;
    const int samples =
        mp3dec_decode_frame(decoder, input + offset, buffered, pcm, &info);
    if (info.frame_bytes > 0) {
      if ((size_t)info.frame_bytes > buffered) {
        playback_set_error(playback, PlaybackErrorInvalidMp3);
        break;
      }
      if (samples > 0) {
        decoded_frames++;
        playback_publish_info(playback, &info);
        const uint32_t audio_bytes =
            info.frame_bytes > info.frame_offset
                ? (uint32_t)(info.frame_bytes - info.frame_offset)
                : (uint32_t)info.frame_bytes;
        decoded_audio_bytes += audio_bytes;
        decoded_time_us += ((uint64_t)samples * 1000000ULL) / (uint32_t)info.hz;
        if (!playback->known_duration_ms && decoded_audio_bytes &&
            total_audio_bytes) {
          const uint64_t estimate =
              (total_audio_bytes * decoded_time_us) / decoded_audio_bytes;
          __atomic_store_n(&playback->duration_ms,
                           (uint32_t)(estimate / 1000ULL), __ATOMIC_RELEASE);
        }
        if (!playback_output_frame(playback, pcm, &info, samples, &resampler,
                                   output_rate))
          break;
      }
      offset += info.frame_bytes;
      buffered -= info.frame_bytes;
      if (buffered == 0)
        offset = 0;
    } else if (eof) {
      break;
    } else {
      /* No complete frame was found in the current window. Advance one
         byte until a sync word or the next bulk refill is reached. */
      offset++;
      buffered--;
    }
  }

  if (!playback->stop_requested) {
    if (decoded_frames == 0) {
      playback_set_error(playback, PlaybackErrorInvalidMp3);
    } else if (resampler.filled) {
      const int16_t final_sample =
          (int16_t)(resampler.sum / (int32_t)resampler.filled);
      audio_backend_write(playback->audio, final_sample);
    }
    if (decoded_frames)
      audio_backend_drain(playback->audio, 1000);
  }

cleanup:
  audio_backend_stop(playback->audio);
  storage_file_close(file);
  storage_file_free(file);
  furi_record_close(RECORD_STORAGE);
  free(decoder);
  free(pcm);
  free(input);
  playback->running = false;
  return 0;
}

Mp3Playback *mp3_playback_alloc(void) {
  Mp3Playback *playback = calloc(1, sizeof(Mp3Playback));
  playback->audio = audio_backend_alloc();
  return playback;
}

void mp3_playback_free(Mp3Playback *playback) {
  if (!playback)
    return;
  mp3_playback_stop(playback);
  audio_backend_free(playback->audio);
  free(playback);
}

bool mp3_playback_start(Mp3Playback *playback, const char *path,
                        AudioOutput output, uint8_t volume) {
  return mp3_playback_start_at(playback, path, output, volume, 0, 0);
}

bool mp3_playback_start_at(Mp3Playback *playback, const char *path,
                           AudioOutput output, uint8_t volume,
                           uint32_t position_ms, uint32_t duration_ms) {
  if (playback->running)
    return false;
  if (playback->thread) {
    furi_thread_join(playback->thread);
    furi_thread_free(playback->thread);
    playback->thread = NULL;
  }

  __atomic_store_n(&playback->error, PlaybackErrorNone, __ATOMIC_RELEASE);
  __atomic_store_n(&playback->stream_info, 0, __ATOMIC_RELEASE);
  __atomic_store_n(&playback->duration_ms, duration_ms, __ATOMIC_RELEASE);
  playback->stop_requested = false;
  playback->paused = false;
  playback->start_position_ms = position_ms;
  playback->known_duration_ms = duration_ms;
  playback->running = true;
  playback->output = output;
  playback->volume = volume;
  strlcpy(playback->path, path, sizeof(playback->path));
  playback->thread = furi_thread_alloc_ex("Mp3Decode", PLAYBACK_STACK_SIZE,
                                          playback_thread, playback);
  if (!playback->thread) {
    playback->running = false;
    playback_set_error(playback, PlaybackErrorThreadFailed);
    return false;
  }
  furi_thread_set_priority(playback->thread, FuriThreadPriorityHigh);
  furi_thread_start(playback->thread);
  return true;
}

void mp3_playback_stop(Mp3Playback *playback) {
  if (!playback)
    return;
  playback->stop_requested = true;
  playback->paused = false;
  audio_backend_set_paused(playback->audio, false);
  if (playback->thread) {
    furi_thread_join(playback->thread);
    furi_thread_free(playback->thread);
    playback->thread = NULL;
  }
  playback->running = false;
  __atomic_store_n(&playback->stream_info, 0, __ATOMIC_RELEASE);
  __atomic_store_n(&playback->duration_ms, 0, __ATOMIC_RELEASE);
  audio_backend_reset_progress(playback->audio);
  __atomic_store_n(&playback->error, PlaybackErrorNone, __ATOMIC_RELEASE);
}

bool mp3_playback_is_running(const Mp3Playback *playback) {
  return playback && playback->running;
}

bool mp3_playback_is_paused(const Mp3Playback *playback) {
  return playback && playback->running && playback->paused;
}

void mp3_playback_set_paused(Mp3Playback *playback, bool paused) {
  if (!playback || !playback->running)
    return;
  playback->paused = paused;
  audio_backend_set_paused(playback->audio, paused);
}

void mp3_playback_set_volume(Mp3Playback *playback, uint8_t volume) {
  if (!playback)
    return;
  playback->volume = volume;
  audio_backend_set_volume(playback->audio, volume);
}

const char *mp3_playback_get_error(const Mp3Playback *playback) {
  if (!playback)
    return "";

  switch (__atomic_load_n(&playback->error, __ATOMIC_ACQUIRE)) {
  case PlaybackErrorNoMemory:
    return "Not enough memory";
  case PlaybackErrorOpenFailed:
    return "Cannot open MP3";
  case PlaybackErrorReadFailed:
    return "SD card read failed";
  case PlaybackErrorOutputBusy:
    return "Audio output busy";
  case PlaybackErrorPowerUnavailable:
    return "5V power unavailable";
  case PlaybackErrorInvalidMp3:
    return "No valid MP3 audio";
  case PlaybackErrorThreadFailed:
    return "Cannot start decoder";
  default:
    return "";
  }
}

bool mp3_playback_get_info(const Mp3Playback *playback, Mp3PlaybackInfo *info) {
  if (!playback || !info)
    return false;

  const uint32_t packed =
      __atomic_load_n(&playback->stream_info, __ATOMIC_ACQUIRE);
  info->sample_rate_hz = packed & STREAM_INFO_RATE_MASK;
  info->bitrate_kbps =
      (packed >> STREAM_INFO_BITRATE_SHIFT) & STREAM_INFO_BITRATE_MASK;
  info->channels = (packed >> STREAM_INFO_CHANNELS_SHIFT) & 0x3U;
  return info->sample_rate_hz != 0;
}

uint32_t mp3_playback_get_underflows(const Mp3Playback *playback) {
  return playback ? audio_backend_get_underflows(playback->audio) : 0;
}

uint32_t mp3_playback_get_position_ms(const Mp3Playback *playback) {
  if (!playback)
    return 0;
  const uint32_t rate = audio_backend_get_sample_rate(playback->output);
  return playback->start_position_ms +
         (rate ? (uint32_t)(((uint64_t)audio_backend_get_played_samples(
                                 playback->audio) *
                             1000ULL) /
                            rate)
               : 0);
}

uint32_t mp3_playback_get_duration_ms(const Mp3Playback *playback) {
  return playback ? __atomic_load_n(&playback->duration_ms, __ATOMIC_ACQUIRE)
                  : 0;
}
