#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "mf_passive_audio_pipe.h"

#define MF_PASSIVE_RF_PDM_HZ           48000U
#define MF_PASSIVE_RF_DEVIATION_HZ     2381U
#define MF_PASSIVE_RF_VOICE_RATE_HZ    16000U
#define MF_PASSIVE_RF_TONE_AMPLITUDE   (INT16_MAX / 3)
#define MF_PASSIVE_RF_TONE_RAMP_SLOTS  144U
#define MF_PASSIVE_RF_GAIN_MIN_PCT     50U
#define MF_PASSIVE_RF_GAIN_MAX_PCT     300U
#define MF_PASSIVE_RF_GAIN_STEP_PCT    25U
#define MF_PASSIVE_RF_GAIN_DEFAULT_PCT 150U

typedef struct MfPassiveRfAudio MfPassiveRfAudio;

typedef struct {
    bool level;
    uint8_t duration_us;
} MfPassiveRfPulse;

typedef struct {
    bool (*frequency_valid)(void* context, uint32_t frequency_hz);
    bool (*frequency_allowed)(void* context, uint32_t frequency_hz);
    void (*radio_idle)(void* context);
    bool (*load_preset)(void* context, const uint8_t* preset, size_t size);
    uint32_t (*set_frequency_and_path)(void* context, uint32_t frequency_hz);
    void (*data_gpio_input)(void* context);
    bool (*async_start)(void* context, MfPassiveRfAudio* audio);
    void (*async_stop)(void* context);
    void (*radio_sleep)(void* context);
    void (*set_led)(void* context, bool on);
    void (*insomnia_enter)(void* context);
    void (*insomnia_exit)(void* context);
} MfPassiveRfHardwareOps;

typedef enum {
    MfPassiveRfSourceSilence = 0,
    MfPassiveRfSourceTone,
    MfPassiveRfSourceVoice,
} MfPassiveRfSource;

struct MfPassiveRfAudio {
    const MfPassiveRfHardwareOps* ops;
    void* hardware_context;
    MfPassivePcmPipe* pipe;
    uint32_t frequency_hz;
    uint32_t source_phase;
    int32_t pdm_error;
    uint32_t tone_phase;
    uint32_t tone_slot;
    uint32_t tone_slots_total;
    int32_t dsp_x1;
    int32_t dsp_x2;
    int32_t dsp_h1;
    int32_t dsp_h2;
    int32_t dsp_l1;
    int32_t dsp_l2;
    int32_t dsp_comp;
    int16_t sample;
    volatile uint16_t voice_gain_pct;
    uint16_t tone_hz;
    volatile uint8_t source;
    volatile bool dsp_enabled;
    bool dsp_was_enabled;
    uint8_t duration_slot;
    bool level;
    bool tone_positive;
    bool prepared;
    bool running;
    bool insomnia_owned;
};

void mf_passive_rf_audio_init(
    MfPassiveRfAudio* audio,
    const MfPassiveRfHardwareOps* ops,
    void* hardware_context,
    MfPassivePcmPipe* pipe);
#ifdef MORSE_FLIPPER_FAP
void mf_passive_rf_audio_init_production(MfPassiveRfAudio* audio, MfPassivePcmPipe* pipe);
#endif
bool mf_passive_rf_audio_frequency_usable(const MfPassiveRfAudio* audio, uint32_t frequency_hz);
bool mf_passive_rf_audio_prepare(MfPassiveRfAudio* audio, uint32_t frequency_hz);
bool mf_passive_rf_audio_start_burst(MfPassiveRfAudio* audio);
void mf_passive_rf_audio_pause(MfPassiveRfAudio* audio);
void mf_passive_rf_audio_release(MfPassiveRfAudio* audio);
void mf_passive_rf_audio_set_silence(MfPassiveRfAudio* audio);
bool mf_passive_rf_audio_start_tone(
    MfPassiveRfAudio* audio,
    uint16_t tone_hz,
    uint32_t duration_ms);
bool mf_passive_rf_audio_start_voice(MfPassiveRfAudio* audio);
uint16_t mf_passive_rf_audio_voice_gain_pct(const MfPassiveRfAudio* audio);
void mf_passive_rf_audio_set_voice_gain_pct(MfPassiveRfAudio* audio, uint16_t gain_pct);
bool mf_passive_rf_audio_dsp_enabled(const MfPassiveRfAudio* audio);
void mf_passive_rf_audio_set_dsp_enabled(MfPassiveRfAudio* audio, bool enabled);
int16_t mf_passive_rf_audio_process_voice_sample(MfPassiveRfAudio* audio, int16_t sample);
bool mf_passive_rf_audio_tone_complete(const MfPassiveRfAudio* audio);
MfPassiveRfPulse mf_passive_rf_audio_next_pulse(MfPassiveRfAudio* audio);
