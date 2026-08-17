#include "mf_passive_rf_audio.h"

#include <limits.h>
#include <string.h>

#define MF_PASSIVE_RF_DSP_Q15            32768L
#define MF_PASSIVE_RF_DSP_HP_400_Q15     28320L
#define MF_PASSIVE_RF_DSP_LP_2400_Q15    15899L
#define MF_PASSIVE_RF_DSP_COMP_THRESHOLD 6000L

#ifdef MORSE_FLIPPER_FAP
#include <furi_hal.h>
#include <lib/drivers/cc1101_regs.h>
#else
#define __DMB()         ((void)0)
#define CC1101_IOCFG0   0x02U
#define CC1101_FSCTRL1  0x0BU
#define CC1101_PKTCTRL0 0x08U
#define CC1101_PKTCTRL1 0x07U
#define CC1101_MDMCFG0  0x14U
#define CC1101_MDMCFG1  0x13U
#define CC1101_MDMCFG2  0x12U
#define CC1101_MDMCFG3  0x11U
#define CC1101_MDMCFG4  0x10U
#define CC1101_DEVIATN  0x15U
#define CC1101_MCSM0    0x18U
#define CC1101_FOCCFG   0x19U
#define CC1101_AGCCTRL0 0x1DU
#define CC1101_AGCCTRL1 0x1CU
#define CC1101_AGCCTRL2 0x1BU
#define CC1101_WORCTRL  0x20U
#define CC1101_FREND0   0x22U
#define CC1101_FREND1   0x21U
#endif

static const uint8_t mf_passive_rf_preset[] = {
    CC1101_IOCFG0,
    0x0D,
    CC1101_FSCTRL1,
    0x06,
    CC1101_PKTCTRL0,
    0x32,
    CC1101_PKTCTRL1,
    0x04,
    CC1101_MDMCFG0,
    0x00,
    CC1101_MDMCFG1,
    0x02,
    CC1101_MDMCFG2,
    0x04,
    CC1101_MDMCFG3,
    0xE4,
    CC1101_MDMCFG4,
    0x6A,
    CC1101_DEVIATN,
    0x04,
    CC1101_MCSM0,
    0x18,
    CC1101_FOCCFG,
    0x16,
    CC1101_AGCCTRL0,
    0x91,
    CC1101_AGCCTRL1,
    0x00,
    CC1101_AGCCTRL2,
    0x07,
    CC1101_WORCTRL,
    0xFB,
    CC1101_FREND0,
    0x10,
    CC1101_FREND1,
    0x56,
    0x00,
    0x00,
    0xC0,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
};

static bool mf_passive_rf_ops_complete(const MfPassiveRfHardwareOps* ops) {
    return ops != NULL && ops->frequency_valid != NULL && ops->frequency_allowed != NULL &&
           ops->radio_idle != NULL && ops->load_preset != NULL &&
           ops->set_frequency_and_path != NULL && ops->data_gpio_input != NULL &&
           ops->async_start != NULL && ops->async_stop != NULL && ops->radio_sleep != NULL &&
           ops->set_led != NULL && ops->insomnia_enter != NULL && ops->insomnia_exit != NULL;
}

void mf_passive_rf_audio_init(
    MfPassiveRfAudio* audio,
    const MfPassiveRfHardwareOps* ops,
    void* hardware_context,
    MfPassivePcmPipe* pipe) {
    if(audio == NULL) return;
    memset(audio, 0, sizeof(*audio));
    audio->ops = ops;
    audio->hardware_context = hardware_context;
    audio->pipe = pipe;
    audio->voice_gain_pct = MF_PASSIVE_RF_GAIN_DEFAULT_PCT;
}

static bool mf_passive_rf_probe(const MfPassiveRfAudio* audio, uint32_t frequency_hz) {
    return audio->ops->frequency_valid(audio->hardware_context, frequency_hz) &&
           audio->ops->frequency_allowed(audio->hardware_context, frequency_hz);
}

static bool mf_passive_rf_tune(MfPassiveRfAudio* audio, uint32_t frequency_hz) {
    uint32_t tuned_frequency_hz =
        audio->ops->set_frequency_and_path(audio->hardware_context, frequency_hz);
    return tuned_frequency_hz != 0U &&
           mf_passive_rf_audio_frequency_usable(audio, tuned_frequency_hz);
}

bool mf_passive_rf_audio_frequency_usable(const MfPassiveRfAudio* audio, uint32_t frequency_hz) {
    if(audio == NULL || !mf_passive_rf_ops_complete(audio->ops) ||
       frequency_hz < MF_PASSIVE_RF_DEVIATION_HZ ||
       frequency_hz > UINT32_MAX - MF_PASSIVE_RF_DEVIATION_HZ)
        return false;
    return mf_passive_rf_probe(audio, frequency_hz) &&
           mf_passive_rf_probe(audio, frequency_hz - MF_PASSIVE_RF_DEVIATION_HZ) &&
           mf_passive_rf_probe(audio, frequency_hz + MF_PASSIVE_RF_DEVIATION_HZ);
}

void mf_passive_rf_audio_set_silence(MfPassiveRfAudio* audio) {
    if(audio == NULL) return;
    audio->sample = 0;
    __DMB();
    audio->source = MfPassiveRfSourceSilence;
}

static void mf_passive_rf_audio_reset_stream(MfPassiveRfAudio* audio) {
    audio->source = MfPassiveRfSourceSilence;
    audio->source_phase = 0U;
    audio->pdm_error = 0;
    audio->tone_phase = 0U;
    audio->tone_slot = 0U;
    audio->tone_slots_total = 0U;
    audio->sample = 0;
    audio->tone_hz = 0U;
    audio->dsp_x1 = 0;
    audio->dsp_x2 = 0;
    audio->dsp_h1 = 0;
    audio->dsp_h2 = 0;
    audio->dsp_l1 = 0;
    audio->dsp_l2 = 0;
    audio->dsp_comp = 0;
    audio->dsp_was_enabled = audio->dsp_enabled;
    audio->duration_slot = 0U;
    audio->level = false;
    audio->tone_positive = true;
}

void mf_passive_rf_audio_pause(MfPassiveRfAudio* audio) {
    if(audio == NULL || !mf_passive_rf_ops_complete(audio->ops)) return;
    mf_passive_rf_audio_set_silence(audio);
    if(audio->running) audio->ops->async_stop(audio->hardware_context);
    audio->running = false;
    audio->ops->set_led(audio->hardware_context, false);
    audio->ops->radio_idle(audio->hardware_context);
    if(audio->insomnia_owned) {
        audio->ops->insomnia_exit(audio->hardware_context);
        audio->insomnia_owned = false;
    }
}

void mf_passive_rf_audio_release(MfPassiveRfAudio* audio) {
    if(audio == NULL || !mf_passive_rf_ops_complete(audio->ops)) return;
    /* stop_async_tx must return before callback-visible pointers are cleared. */
    mf_passive_rf_audio_pause(audio);
    audio->ops->data_gpio_input(audio->hardware_context);
    audio->ops->radio_idle(audio->hardware_context);
    audio->ops->radio_sleep(audio->hardware_context);
    audio->pipe = NULL;
    audio->frequency_hz = 0U;
    audio->prepared = false;
    mf_passive_rf_audio_reset_stream(audio);
}

bool mf_passive_rf_audio_prepare(MfPassiveRfAudio* audio, uint32_t frequency_hz) {
    if(audio == NULL || audio->pipe == NULL || !mf_passive_rf_ops_complete(audio->ops) ||
       !mf_passive_rf_audio_frequency_usable(audio, frequency_hz)) {
        if(audio != NULL) mf_passive_rf_audio_release(audio);
        return false;
    }
    mf_passive_rf_audio_pause(audio);
    audio->ops->radio_idle(audio->hardware_context);
    if(!audio->ops->load_preset(
           audio->hardware_context, mf_passive_rf_preset, sizeof(mf_passive_rf_preset)) ||
       !mf_passive_rf_tune(audio, frequency_hz)) {
        mf_passive_rf_audio_release(audio);
        return false;
    }
    audio->ops->data_gpio_input(audio->hardware_context);
    audio->frequency_hz = frequency_hz;
    audio->prepared = true;
    return true;
}

bool mf_passive_rf_audio_start_burst(MfPassiveRfAudio* audio) {
    if(audio == NULL || !audio->prepared || audio->running ||
       !mf_passive_rf_audio_frequency_usable(audio, audio->frequency_hz)) {
        if(audio != NULL) mf_passive_rf_audio_release(audio);
        return false;
    }
    mf_passive_rf_audio_reset_stream(audio);
    if(!mf_passive_rf_tune(audio, audio->frequency_hz)) {
        mf_passive_rf_audio_release(audio);
        return false;
    }
    audio->ops->insomnia_enter(audio->hardware_context);
    audio->insomnia_owned = true;
    if(!audio->ops->async_start(audio->hardware_context, audio)) {
        mf_passive_rf_audio_release(audio);
        return false;
    }
    audio->running = true;
    audio->ops->set_led(audio->hardware_context, true);
    return true;
}

static int16_t mf_passive_rf_clamp_sample(int32_t value) {
    int32_t gained = value;
    if(gained > INT16_MAX) return INT16_MAX;
    if(gained < INT16_MIN) return INT16_MIN;
    return (int16_t)gained;
}

static int32_t mf_passive_rf_q15_mul(int32_t coefficient, int32_t value) {
    int64_t product = (int64_t)coefficient * value;
    product += product < 0 ? -(MF_PASSIVE_RF_DSP_Q15 / 2) : MF_PASSIVE_RF_DSP_Q15 / 2;
    return (int32_t)(product / MF_PASSIVE_RF_DSP_Q15);
}

static void mf_passive_rf_reset_dsp(MfPassiveRfAudio* audio) {
    audio->dsp_x1 = 0;
    audio->dsp_x2 = 0;
    audio->dsp_h1 = 0;
    audio->dsp_h2 = 0;
    audio->dsp_l1 = 0;
    audio->dsp_l2 = 0;
    audio->dsp_comp = 0;
    audio->dsp_was_enabled = audio->dsp_enabled;
}

uint16_t mf_passive_rf_audio_voice_gain_pct(const MfPassiveRfAudio* audio) {
    return audio != NULL ? audio->voice_gain_pct : MF_PASSIVE_RF_GAIN_DEFAULT_PCT;
}

void mf_passive_rf_audio_set_voice_gain_pct(MfPassiveRfAudio* audio, uint16_t gain_pct) {
    if(audio == NULL) return;
    if(gain_pct < MF_PASSIVE_RF_GAIN_MIN_PCT) gain_pct = MF_PASSIVE_RF_GAIN_MIN_PCT;
    if(gain_pct > MF_PASSIVE_RF_GAIN_MAX_PCT) gain_pct = MF_PASSIVE_RF_GAIN_MAX_PCT;
    audio->voice_gain_pct = gain_pct;
}

bool mf_passive_rf_audio_dsp_enabled(const MfPassiveRfAudio* audio) {
    return audio != NULL && audio->dsp_enabled;
}

void mf_passive_rf_audio_set_dsp_enabled(MfPassiveRfAudio* audio, bool enabled) {
    if(audio == NULL) return;
    audio->dsp_enabled = enabled;
}

int16_t mf_passive_rf_audio_process_voice_sample(MfPassiveRfAudio* audio, int16_t sample) {
    int32_t value = sample;
    int32_t magnitude;
    int32_t wanted;
    if(audio == NULL) return sample;
    if(audio->dsp_enabled != audio->dsp_was_enabled) mf_passive_rf_reset_dsp(audio);
    if(audio->dsp_enabled) {
        audio->dsp_h1 = mf_passive_rf_q15_mul(
            MF_PASSIVE_RF_DSP_HP_400_Q15, audio->dsp_h1 + value - audio->dsp_x1);
        audio->dsp_x1 = value;
        audio->dsp_h2 = mf_passive_rf_q15_mul(
            MF_PASSIVE_RF_DSP_HP_400_Q15, audio->dsp_h2 + audio->dsp_h1 - audio->dsp_x2);
        audio->dsp_x2 = audio->dsp_h1;
        audio->dsp_l1 +=
            mf_passive_rf_q15_mul(MF_PASSIVE_RF_DSP_LP_2400_Q15, audio->dsp_h2 - audio->dsp_l1);
        audio->dsp_l2 +=
            mf_passive_rf_q15_mul(MF_PASSIVE_RF_DSP_LP_2400_Q15, audio->dsp_l1 - audio->dsp_l2);
        magnitude = audio->dsp_l2 < 0 ? -audio->dsp_l2 : audio->dsp_l2;
        if(magnitude > audio->dsp_comp)
            audio->dsp_comp = magnitude;
        else
            audio->dsp_comp += (magnitude - audio->dsp_comp) / 2048;
        value = audio->dsp_l2 * 2;
        if(audio->dsp_comp > MF_PASSIVE_RF_DSP_COMP_THRESHOLD) {
            wanted = MF_PASSIVE_RF_DSP_COMP_THRESHOLD +
                     (audio->dsp_comp - MF_PASSIVE_RF_DSP_COMP_THRESHOLD) / 8;
            value = (int32_t)(((int64_t)value * wanted) / audio->dsp_comp);
        }
    }
    value = (value * audio->voice_gain_pct) / 100;
    return mf_passive_rf_clamp_sample(value);
}

static void mf_passive_rf_consume_voice(MfPassiveRfAudio* audio) {
    MfPassivePcmPipe* pipe = audio->pipe;
    uint16_t read;
    if(pipe == NULL) {
        mf_passive_rf_audio_set_silence(audio);
        return;
    }
    read = pipe->read_pos;
    if(read != pipe->write_pos) {
        __DMB();
        audio->sample = mf_passive_rf_audio_process_voice_sample(audio, pipe->samples[read]);
        pipe->read_pos = (uint16_t)((read + 1U) & (MF_PASSIVE_PCM_RING_SAMPLES - 1U));
    } else if(pipe->eof) {
        audio->sample = 0;
        audio->source = MfPassiveRfSourceSilence;
        __DMB();
        pipe->drained = true;
    } else {
        audio->sample = 0;
        if(pipe->underruns != UINT32_MAX) pipe->underruns++;
    }
}

bool mf_passive_rf_audio_start_voice(MfPassiveRfAudio* audio) {
    MfPassivePcmPipe* pipe;
    if(audio == NULL || !audio->running || (pipe = audio->pipe) == NULL ||
       pipe->read_pos == pipe->write_pos)
        return false;
    mf_passive_rf_audio_set_silence(audio);
    audio->source_phase = 0U;
    mf_passive_rf_consume_voice(audio);
    __DMB();
    audio->source = MfPassiveRfSourceVoice;
    return true;
}

bool mf_passive_rf_audio_start_tone(
    MfPassiveRfAudio* audio,
    uint16_t tone_hz,
    uint32_t duration_ms) {
    if(audio == NULL || !audio->running || tone_hz == 0U || duration_ms == 0U ||
       duration_ms > UINT32_MAX / (MF_PASSIVE_RF_PDM_HZ / 1000U))
        return false;
    mf_passive_rf_audio_set_silence(audio);
    audio->tone_hz = tone_hz;
    audio->tone_phase = 0U;
    audio->tone_slot = 0U;
    audio->tone_slots_total = duration_ms * (MF_PASSIVE_RF_PDM_HZ / 1000U);
    audio->tone_positive = true;
    __DMB();
    audio->source = MfPassiveRfSourceTone;
    return true;
}

bool mf_passive_rf_audio_tone_complete(const MfPassiveRfAudio* audio) {
    return audio == NULL || audio->source != MfPassiveRfSourceTone;
}

static void mf_passive_rf_tone_sample(MfPassiveRfAudio* audio) {
    uint32_t remaining;
    uint32_t envelope_slots;
    if(audio->tone_slot >= audio->tone_slots_total) {
        mf_passive_rf_audio_set_silence(audio);
        return;
    }
    remaining = audio->tone_slots_total - audio->tone_slot;
    envelope_slots = audio->tone_slot + 1U;
    if(envelope_slots > remaining) envelope_slots = remaining;
    if(envelope_slots > MF_PASSIVE_RF_TONE_RAMP_SLOTS)
        envelope_slots = MF_PASSIVE_RF_TONE_RAMP_SLOTS;
    audio->sample = (int16_t)(((int32_t)MF_PASSIVE_RF_TONE_AMPLITUDE * envelope_slots) /
                              MF_PASSIVE_RF_TONE_RAMP_SLOTS);
    if(!audio->tone_positive) audio->sample = (int16_t)-audio->sample;
}

MfPassiveRfPulse mf_passive_rf_audio_next_pulse(MfPassiveRfAudio* audio) {
    MfPassiveRfPulse pulse = {0};
    if(audio == NULL) return pulse;
    if(audio->source == MfPassiveRfSourceTone) {
        mf_passive_rf_tone_sample(audio);
    } else if(audio->source == MfPassiveRfSourceSilence) {
        audio->sample = 0;
    }

    /* First-order PDM at 48 kbit/s; source updates happen after this held sample.
   */
    audio->pdm_error += audio->sample;
    audio->level = audio->pdm_error >= 0;
    audio->pdm_error += audio->level ? -32767 : 32768;
    pulse.level = audio->level;
    audio->duration_slot++;
    if(audio->duration_slot == 6U) {
        audio->duration_slot = 0U;
        pulse.duration_us = 20U;
    } else {
        pulse.duration_us = 21U;
    }

    if(audio->source == MfPassiveRfSourceVoice) {
        audio->source_phase += MF_PASSIVE_RF_VOICE_RATE_HZ;
        if(audio->source_phase >= MF_PASSIVE_RF_PDM_HZ) {
            audio->source_phase -= MF_PASSIVE_RF_PDM_HZ;
            mf_passive_rf_consume_voice(audio);
        }
    } else if(audio->source == MfPassiveRfSourceTone) {
        audio->tone_slot++;
        audio->tone_phase += 2U * audio->tone_hz;
        if(audio->tone_phase >= MF_PASSIVE_RF_PDM_HZ) {
            audio->tone_phase -= MF_PASSIVE_RF_PDM_HZ;
            audio->tone_positive = !audio->tone_positive;
        }
        if(audio->tone_slot >= audio->tone_slots_total) mf_passive_rf_audio_set_silence(audio);
    }
    return pulse;
}

#ifdef MORSE_FLIPPER_FAP
static bool mf_passive_rf_hw_frequency_valid(void* context, uint32_t frequency_hz) {
    (void)context;
    return furi_hal_subghz_is_frequency_valid(frequency_hz);
}

static bool mf_passive_rf_hw_frequency_allowed(void* context, uint32_t frequency_hz) {
    (void)context;
    return furi_hal_region_is_frequency_allowed(frequency_hz);
}

static void mf_passive_rf_hw_idle(void* context) {
    (void)context;
    furi_hal_subghz_idle();
}

static bool mf_passive_rf_hw_load_preset(void* context, const uint8_t* preset, size_t size) {
    (void)context;
    (void)size;
    furi_hal_subghz_load_custom_preset(preset);
    return true;
}

static uint32_t mf_passive_rf_hw_set_frequency(void* context, uint32_t frequency_hz) {
    (void)context;
    return furi_hal_subghz_set_frequency_and_path(frequency_hz);
}

static void mf_passive_rf_hw_data_gpio_input(void* context) {
    (void)context;
    furi_hal_gpio_init(furi_hal_subghz_get_data_gpio(), GpioModeInput, GpioPullNo, GpioSpeedLow);
}

static LevelDuration mf_passive_rf_async_yield(void* context) {
    MfPassiveRfPulse pulse = mf_passive_rf_audio_next_pulse(context);
    return level_duration_make(pulse.level, pulse.duration_us);
}

static bool mf_passive_rf_hw_async_start(void* context, MfPassiveRfAudio* audio) {
    (void)context;
    return furi_hal_subghz_start_async_tx(mf_passive_rf_async_yield, audio);
}

static void mf_passive_rf_hw_async_stop(void* context) {
    (void)context;
    furi_hal_subghz_stop_async_tx();
}

static void mf_passive_rf_hw_sleep(void* context) {
    (void)context;
    furi_hal_subghz_sleep();
}

static void mf_passive_rf_hw_set_led(void* context, bool on) {
    (void)context;
    if(on) {
        furi_hal_light_set(LightBlue, 0U);
        furi_hal_light_set(LightGreen, 96U);
        furi_hal_light_set(LightRed, 255U);
    } else {
        furi_hal_light_set(LightRed | LightGreen | LightBlue, 0U);
    }
}

static void mf_passive_rf_hw_insomnia_enter(void* context) {
    (void)context;
    furi_hal_power_insomnia_enter();
}

static void mf_passive_rf_hw_insomnia_exit(void* context) {
    (void)context;
    furi_hal_power_insomnia_exit();
}

static const MfPassiveRfHardwareOps mf_passive_rf_production_ops = {
    .frequency_valid = mf_passive_rf_hw_frequency_valid,
    .frequency_allowed = mf_passive_rf_hw_frequency_allowed,
    .radio_idle = mf_passive_rf_hw_idle,
    .load_preset = mf_passive_rf_hw_load_preset,
    .set_frequency_and_path = mf_passive_rf_hw_set_frequency,
    .data_gpio_input = mf_passive_rf_hw_data_gpio_input,
    .async_start = mf_passive_rf_hw_async_start,
    .async_stop = mf_passive_rf_hw_async_stop,
    .radio_sleep = mf_passive_rf_hw_sleep,
    .set_led = mf_passive_rf_hw_set_led,
    .insomnia_enter = mf_passive_rf_hw_insomnia_enter,
    .insomnia_exit = mf_passive_rf_hw_insomnia_exit,
};

void mf_passive_rf_audio_init_production(MfPassiveRfAudio* audio, MfPassivePcmPipe* pipe) {
    mf_passive_rf_audio_init(audio, &mf_passive_rf_production_ops, NULL, pipe);
}
#endif
