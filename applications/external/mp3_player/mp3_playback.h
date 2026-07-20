#pragma once

#include "audio_backend.h"

typedef struct Mp3Playback Mp3Playback;

typedef struct {
  uint32_t sample_rate_hz;
  uint16_t bitrate_kbps;
  uint8_t channels;
} Mp3PlaybackInfo;

Mp3Playback *mp3_playback_alloc(void);
void mp3_playback_free(Mp3Playback *playback);
bool mp3_playback_start(Mp3Playback *playback, const char *path,
                        AudioOutput output, uint8_t volume);
bool mp3_playback_start_at(Mp3Playback *playback, const char *path,
                           AudioOutput output, uint8_t volume,
                           uint32_t position_ms, uint32_t duration_ms);
void mp3_playback_stop(Mp3Playback *playback);
bool mp3_playback_is_running(const Mp3Playback *playback);
bool mp3_playback_is_paused(const Mp3Playback *playback);
void mp3_playback_set_paused(Mp3Playback *playback, bool paused);
void mp3_playback_set_volume(Mp3Playback *playback, uint8_t volume);
const char *mp3_playback_get_error(const Mp3Playback *playback);
bool mp3_playback_get_info(const Mp3Playback *playback, Mp3PlaybackInfo *info);
uint32_t mp3_playback_get_underflows(const Mp3Playback *playback);
uint32_t mp3_playback_get_position_ms(const Mp3Playback *playback);
uint32_t mp3_playback_get_duration_ms(const Mp3Playback *playback);
