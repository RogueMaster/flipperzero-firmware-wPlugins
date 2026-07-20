#pragma once

#include <furi.h>

typedef enum {
  AudioOutputInternal,
  AudioOutputMax98357a,
} AudioOutput;

typedef enum {
  AudioBackendErrorNone,
  AudioBackendErrorResourceBusy,
  AudioBackendErrorPowerUnavailable,
} AudioBackendError;

typedef struct AudioBackend AudioBackend;

AudioBackend *audio_backend_alloc(void);
void audio_backend_free(AudioBackend *backend);

bool audio_backend_start(AudioBackend *backend, AudioOutput output,
                         uint8_t volume);
void audio_backend_stop(AudioBackend *backend);
bool audio_backend_write(AudioBackend *backend, int16_t sample);
void audio_backend_set_volume(AudioBackend *backend, uint8_t volume);
void audio_backend_set_paused(AudioBackend *backend, bool paused);
void audio_backend_drain(AudioBackend *backend, uint32_t timeout_ms);
uint32_t audio_backend_get_underflows(const AudioBackend *backend);
uint32_t audio_backend_get_played_samples(const AudioBackend *backend);
void audio_backend_reset_progress(AudioBackend *backend);
AudioBackendError audio_backend_get_error(const AudioBackend *backend);
uint32_t audio_backend_get_sample_rate(AudioOutput output);
