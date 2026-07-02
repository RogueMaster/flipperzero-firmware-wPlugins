/**
 * @file audio.h
 * @brief Volume IC (PT22xx) and amplifier output state.
 */
#pragma once

#include <stdbool.h>

/** Apply PT chip selection and I2C address from settings. */
void fred_fm_pt_apply_config(void);

/** Probe/init PT chip; refresh cached ready state. */
bool fred_fm_pt_refresh_state(bool force_init);

/** Push attenuation/mute to PT22xx from app state. */
void fred_fm_apply_pt_state(void);

/** Apply PT volume and PAM8406 power/mute/mode together. */
void fred_fm_apply_audio_output_state(void);

/** Mute and shut down amplifier on app exit. */
void fred_fm_audio_shutdown(void);
