#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct RadioAudio RadioAudio;

RadioAudio* radio_audio_alloc(void);
void radio_audio_free(RadioAudio* audio);
bool radio_audio_start(RadioAudio* audio, uint8_t volume);
void radio_audio_stop(RadioAudio* audio);
void radio_audio_request_stop(RadioAudio* audio);
bool radio_audio_write(RadioAudio* audio, int16_t sample);
uint32_t radio_audio_sample_rate(void);
uint32_t radio_audio_underflows(const RadioAudio* audio);
