#pragma once
#include <furi.h>
bool furi_hal_speaker_acquire(uint32_t timeout);
void furi_hal_speaker_release(void);
bool furi_hal_speaker_is_mine(void);
void furi_hal_speaker_start(float frequency, float volume);
void furi_hal_speaker_stop(void);
