/*
 * Purpose: Generate optional high-definition PWM sidetone on GPIO P2.
 * Owns: PWM buffers, fade/ramp state, and Flipper FAP timer/DMA setup.
 * Depends on: morse_flipper_audio_pwm.h and Flipper HAL PWM resources.
 * Tests: tests/test_audio_pwm.c plus firmware build for HAL integration.
 */

#include "morse_flipper_audio_pwm.h"

#include <string.h>

#ifdef MORSE_FLIPPER_FAP
#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_bus.h>
#include <furi_hal_interrupt.h>
#include <furi_hal_pwm.h>
#include <furi_hal_resources.h>

#include <stm32wbxx_ll_dma.h>
#include <stm32wbxx_ll_dmamux.h>
#include <stm32wbxx_ll_gpio.h>
#include <stm32wbxx_ll_tim.h>
#endif

static const int16_t morse_flipper_audio_pwm_sine_q15[MORSE_FLIPPER_AUDIO_PWM_SINE_SAMPLES] = {
    0,      3212,   6393,   9512,   12539,  15446,  18204,  20787,  23170,  25329,  27245,
    28898,  30273,  31356,  32137,  32609,  32767,  32609,  32137,  31356,  30273,  28898,
    27245,  25329,  23170,  20787,  18204,  15446,  12539,  9512,   6393,   3212,   0,
    -3212,  -6393,  -9512,  -12539, -15446, -18204, -20787, -23170, -25329, -27245, -28898,
    -30273, -31356, -32137, -32609, -32767, -32609, -32137, -31356, -30273, -28898, -27245,
    -25329, -23170, -20787, -18204, -15446, -12539, -9512,  -6393,  -3212,
};

static uint16_t morse_flipper_audio_pwm_clamp_fade_len(uint32_t samples) {
    if(samples > MORSE_FLIPPER_AUDIO_PWM_SINE_SAMPLES) {
        return MORSE_FLIPPER_AUDIO_PWM_SINE_SAMPLES;
    }

    return (uint16_t)samples;
}

static uint16_t morse_flipper_audio_pwm_progress_q15(uint16_t idx, uint16_t len) {
    if(len <= 1U) return MORSE_FLIPPER_AUDIO_PWM_Q15;
    return (uint16_t)(((uint32_t)idx * MORSE_FLIPPER_AUDIO_PWM_Q15) / (uint32_t)(len - 1U));
}

static uint16_t morse_flipper_audio_pwm_current_env_q15(const MorseFlipperAudioPwm* audio) {
    uint32_t delta;

    if(audio == NULL) return 0U;

    switch(audio->env_state) {
    case MorseFlipperAudioPwmEnvAttack:
        if(audio->attack_len == 0U || audio->env_idx >= audio->attack_len) {
            return MORSE_FLIPPER_AUDIO_PWM_Q15;
        }

        delta = MORSE_FLIPPER_AUDIO_PWM_Q15 - audio->env_anchor_q15;
        return (uint16_t)(audio->env_anchor_q15 + ((delta * audio->attack_q15[audio->env_idx]) /
                                                   MORSE_FLIPPER_AUDIO_PWM_Q15));
    case MorseFlipperAudioPwmEnvSustain:
        return MORSE_FLIPPER_AUDIO_PWM_Q15;
    case MorseFlipperAudioPwmEnvRelease:
        if(audio->release_len == 0U || audio->env_idx >= audio->release_len) {
            return 0U;
        }

        return (uint16_t)(((uint32_t)audio->env_anchor_q15 * audio->release_q15[audio->env_idx]) /
                          MORSE_FLIPPER_AUDIO_PWM_Q15);
    default:
        return 0U;
    }
}

static void morse_flipper_audio_pwm_apply_gate(MorseFlipperAudioPwm* audio) {
    if(audio == NULL || audio->gate_applied == audio->gate_requested) return;

    /* Restart fades from the current envelope value, avoiding clicks when keying fast. */
    if(audio->gate_requested) {
        if(audio->env_state == MorseFlipperAudioPwmEnvIdle) {
            audio->phase_q32 = 0U;
            audio->env_anchor_q15 = 0U;
            audio->env_idx = 0U;
            audio->env_state = audio->attack_len == 0U ? MorseFlipperAudioPwmEnvSustain :
                                                         MorseFlipperAudioPwmEnvAttack;
        } else {
            audio->env_anchor_q15 = morse_flipper_audio_pwm_current_env_q15(audio);
            audio->env_idx = 0U;
            audio->env_state = audio->attack_len == 0U ? MorseFlipperAudioPwmEnvSustain :
                                                         MorseFlipperAudioPwmEnvAttack;
        }
    } else if(
        audio->env_state == MorseFlipperAudioPwmEnvAttack ||
        audio->env_state == MorseFlipperAudioPwmEnvSustain) {
        audio->env_anchor_q15 = morse_flipper_audio_pwm_current_env_q15(audio);
        audio->env_idx = 0U;
        audio->env_state = audio->release_len == 0U ? MorseFlipperAudioPwmEnvIdle :
                                                      MorseFlipperAudioPwmEnvRelease;
    }

    audio->gate_applied = audio->gate_requested;
}

static uint16_t morse_flipper_audio_pwm_next_sample(MorseFlipperAudioPwm* audio) {
    uint16_t env_q15;
    uint8_t sine_idx;
    int32_t mixed_q15;
    int32_t delta;
    uint16_t sample;

    if(audio == NULL || !audio->prepared) return 0U;

    /* Q15 envelope over a Q32 phase accumulator: fixed point, because this is not a DAW. */
    morse_flipper_audio_pwm_apply_gate(audio);
    env_q15 = morse_flipper_audio_pwm_current_env_q15(audio);

    if(audio->env_state == MorseFlipperAudioPwmEnvIdle) {
        return audio->pwm_midpoint;
    }

    sample = audio->pwm_midpoint;
    if(env_q15 != 0U) {
        sine_idx = (uint8_t)(audio->phase_q32 >> (32U - 6U));
        mixed_q15 = (morse_flipper_audio_pwm_sine_q15[sine_idx] * (int32_t)env_q15) >> 15;
        delta = (mixed_q15 * (int32_t)audio->pwm_amplitude) >> 15;
        sample = (uint16_t)((int32_t)audio->pwm_midpoint + delta);
    }

    if(audio->env_state == MorseFlipperAudioPwmEnvAttack) {
        audio->env_idx++;
        if(audio->env_idx >= audio->attack_len) {
            audio->env_state = MorseFlipperAudioPwmEnvSustain;
            audio->env_idx = 0U;
            audio->env_anchor_q15 = MORSE_FLIPPER_AUDIO_PWM_Q15;
        }
    } else if(audio->env_state == MorseFlipperAudioPwmEnvRelease) {
        audio->env_idx++;
        if(audio->env_idx >= audio->release_len) {
            audio->env_state = MorseFlipperAudioPwmEnvIdle;
            audio->env_idx = 0U;
            audio->env_anchor_q15 = 0U;
        }
    }

    audio->phase_q32 += audio->phase_step_q32;
    return sample;
}

void morse_flipper_audio_pwm_reset(MorseFlipperAudioPwm* audio) {
    if(audio == NULL) return;
    memset(audio, 0, sizeof(*audio));
}

void morse_flipper_audio_pwm_prepare(
    MorseFlipperAudioPwm* audio,
    uint32_t carrier_hz,
    uint32_t sample_rate_hz,
    uint32_t tone_hz,
    uint8_t volume_pct,
    uint16_t attack_ms,
    uint16_t release_ms) {
    uint32_t i;
    uint32_t attack_samples;
    uint32_t release_samples;

    if(audio == NULL) return;

    carrier_hz = carrier_hz == 0U ? MORSE_FLIPPER_AUDIO_PWM_CARRIER_HZ : carrier_hz;
    sample_rate_hz = sample_rate_hz == 0U ? MORSE_FLIPPER_AUDIO_PWM_SAMPLE_RATE_HZ :
                                            sample_rate_hz;
    tone_hz = tone_hz == 0U ? MORSE_FLIPPER_AUDIO_PWM_TONE_HZ : tone_hz;
    if(volume_pct < 10U) volume_pct = 10U;
    if(volume_pct > 100U) volume_pct = 100U;

    audio->sample_rate_hz = sample_rate_hz;
    audio->carrier_hz = carrier_hz;
    audio->tone_hz = tone_hz;
    audio->phase_q32 = 0U;
    audio->phase_step_q32 = (uint32_t)(((uint64_t)tone_hz << 32U) / (uint64_t)sample_rate_hz);
    audio->pwm_period = (uint16_t)(64000000UL / carrier_hz);
    audio->pwm_midpoint = audio->pwm_period / 2U;
    audio->pwm_amplitude = audio->pwm_midpoint > 2U ? (uint16_t)(audio->pwm_midpoint - 2U) :
                                                      audio->pwm_midpoint;
    audio->pwm_amplitude = (uint16_t)(((uint32_t)audio->pwm_amplitude * volume_pct) / 100U);

    attack_samples = ((uint32_t)attack_ms * sample_rate_hz) / 1000U;
    release_samples = ((uint32_t)release_ms * sample_rate_hz) / 1000U;
    audio->attack_len = morse_flipper_audio_pwm_clamp_fade_len(attack_samples);
    audio->release_len = morse_flipper_audio_pwm_clamp_fade_len(release_samples);
    audio->env_state = MorseFlipperAudioPwmEnvIdle;
    audio->env_idx = 0U;
    audio->env_anchor_q15 = 0U;
    audio->gate_requested = false;
    audio->gate_applied = false;

    for(i = 0U; i < MORSE_FLIPPER_AUDIO_PWM_SINE_SAMPLES; i++) {
        audio->attack_q15[i] =
            i < audio->attack_len ?
                morse_flipper_audio_pwm_progress_q15((uint16_t)i, audio->attack_len) :
                MORSE_FLIPPER_AUDIO_PWM_Q15;
        audio->release_q15[i] =
            i < audio->release_len ?
                (uint16_t)(MORSE_FLIPPER_AUDIO_PWM_Q15 -
                           morse_flipper_audio_pwm_progress_q15((uint16_t)i, audio->release_len)) :
                0U;
    }

    for(i = 0U; i < MORSE_FLIPPER_AUDIO_PWM_BUFFER_SAMPLES; i++) {
        audio->dma_buffer[i] = audio->pwm_midpoint;
    }

    audio->prepared = true;
}

void morse_flipper_audio_pwm_set_tone_hz(MorseFlipperAudioPwm* audio, uint32_t tone_hz) {
    if(audio == NULL || !audio->prepared || audio->sample_rate_hz == 0U) return;

    tone_hz = tone_hz == 0U ? MORSE_FLIPPER_AUDIO_PWM_TONE_HZ : tone_hz;
    if(audio->tone_hz == tone_hz) return;

    audio->tone_hz = tone_hz;
    audio->phase_step_q32 =
        (uint32_t)(((uint64_t)tone_hz << 32U) / (uint64_t)audio->sample_rate_hz);
}

void morse_flipper_audio_pwm_set_gate(MorseFlipperAudioPwm* audio, bool gate) {
    if(audio == NULL || !audio->prepared) return;
    audio->gate_requested = gate;
}

void morse_flipper_audio_pwm_render(MorseFlipperAudioPwm* audio, uint16_t* dst, size_t count) {
    size_t i;

    if(audio == NULL || dst == NULL || !audio->prepared) return;

    for(i = 0U; i < count; i++) {
        dst[i] = morse_flipper_audio_pwm_next_sample(audio);
    }
}

bool morse_flipper_audio_pwm_sound_active(const MorseFlipperAudioPwm* audio) {
    if(audio == NULL) return false;
    return audio->env_state != MorseFlipperAudioPwmEnvIdle;
}

#ifdef MORSE_FLIPPER_FAP

#define MORSE_FLIPPER_AUDIO_PWM_DMA     DMA1
#define MORSE_FLIPPER_AUDIO_PWM_DMA_CH  LL_DMA_CHANNEL_1
#define MORSE_FLIPPER_AUDIO_PWM_DMA_DEF MORSE_FLIPPER_AUDIO_PWM_DMA, MORSE_FLIPPER_AUDIO_PWM_DMA_CH
#define MORSE_FLIPPER_AUDIO_PWM_DMA_IRQ FuriHalInterruptIdDma1Ch1

static void morse_flipper_audio_pwm_ramp_midpoint(bool up, uint32_t carrier_hz) {
    uint32_t step_delay_ms = MORSE_FLIPPER_AUDIO_PWM_RAMP_MS / MORSE_FLIPPER_AUDIO_PWM_RAMP_STEPS;

    if(step_delay_ms == 0U) step_delay_ms = 1U;

    if(up) {
        for(uint8_t step = 1U; step <= MORSE_FLIPPER_AUDIO_PWM_RAMP_STEPS; step++) {
            uint8_t duty = (uint8_t)((50U * step) / MORSE_FLIPPER_AUDIO_PWM_RAMP_STEPS);

            furi_hal_pwm_set_params(FuriHalPwmOutputIdTim1PA7, carrier_hz, duty);
            furi_delay_ms(step_delay_ms);
        }
    } else {
        for(uint8_t step = MORSE_FLIPPER_AUDIO_PWM_RAMP_STEPS; step > 0U; step--) {
            uint8_t duty = (uint8_t)((50U * (step - 1U)) / MORSE_FLIPPER_AUDIO_PWM_RAMP_STEPS);

            furi_hal_pwm_set_params(FuriHalPwmOutputIdTim1PA7, carrier_hz, duty);
            furi_delay_ms(step_delay_ms);
        }
    }
}

static void morse_flipper_audio_pwm_clear_dma_flags(void) {
#if MORSE_FLIPPER_AUDIO_PWM_DMA_CH == LL_DMA_CHANNEL_1
    LL_DMA_ClearFlag_HT1(MORSE_FLIPPER_AUDIO_PWM_DMA);
    LL_DMA_ClearFlag_TC1(MORSE_FLIPPER_AUDIO_PWM_DMA);
    LL_DMA_ClearFlag_TE1(MORSE_FLIPPER_AUDIO_PWM_DMA);
#else
#error Update DMA flag handling for this channel.
#endif
}

static void morse_flipper_audio_pwm_dma_isr(void* context) {
    MorseFlipperAudioPwm* audio = context;

#if MORSE_FLIPPER_AUDIO_PWM_DMA_CH == LL_DMA_CHANNEL_1
    if(LL_DMA_IsActiveFlag_HT1(MORSE_FLIPPER_AUDIO_PWM_DMA) &&
       LL_DMA_IsEnabledIT_HT(MORSE_FLIPPER_AUDIO_PWM_DMA_DEF)) {
        LL_DMA_ClearFlag_HT1(MORSE_FLIPPER_AUDIO_PWM_DMA);
        morse_flipper_audio_pwm_render(
            audio, audio->dma_buffer, MORSE_FLIPPER_AUDIO_PWM_BUFFER_HALF_SAMPLES);
    }

    if(LL_DMA_IsActiveFlag_TC1(MORSE_FLIPPER_AUDIO_PWM_DMA) &&
       LL_DMA_IsEnabledIT_TC(MORSE_FLIPPER_AUDIO_PWM_DMA_DEF)) {
        LL_DMA_ClearFlag_TC1(MORSE_FLIPPER_AUDIO_PWM_DMA);
        morse_flipper_audio_pwm_render(
            audio,
            audio->dma_buffer + MORSE_FLIPPER_AUDIO_PWM_BUFFER_HALF_SAMPLES,
            MORSE_FLIPPER_AUDIO_PWM_BUFFER_HALF_SAMPLES);
    }

    if(LL_DMA_IsActiveFlag_TE1(MORSE_FLIPPER_AUDIO_PWM_DMA) &&
       LL_DMA_IsEnabledIT_TE(MORSE_FLIPPER_AUDIO_PWM_DMA_DEF)) {
        LL_DMA_ClearFlag_TE1(MORSE_FLIPPER_AUDIO_PWM_DMA);
    }
#endif
}

bool morse_flipper_audio_pwm_start(MorseFlipperAudioPwm* audio) {
    LL_DMA_InitTypeDef dma_cfg = {0};

    if(audio == NULL || !audio->prepared) return false;
    if(audio->running) return true;

    /*
     * TIM1 supplies the PWM carrier; DMA feeds CCR1 at the audio sample rate.
     * Bring buses and midpoint ramp up before the circular DMA starts scribbling.
     */
    audio->own_bus_tim1 = false;
    audio->own_bus_dma1 = false;
    audio->own_bus_dmamux1 = false;

    if(!furi_hal_bus_is_enabled(FuriHalBusDMA1)) {
        furi_hal_bus_enable(FuriHalBusDMA1);
        audio->own_bus_dma1 = true;
    }
    if(!furi_hal_bus_is_enabled(FuriHalBusDMAMUX1)) {
        furi_hal_bus_enable(FuriHalBusDMAMUX1);
        audio->own_bus_dmamux1 = true;
    }

    furi_hal_pwm_start(FuriHalPwmOutputIdTim1PA7, audio->carrier_hz, 0U);
    audio->own_bus_tim1 = true;
    morse_flipper_audio_pwm_ramp_midpoint(true, audio->carrier_hz);

    LL_TIM_DisableCounter(TIM1);
    LL_TIM_DisableAllOutputs(TIM1);
    LL_TIM_DisableDMAReq_UPDATE(TIM1);
    LL_TIM_DisableIT_UPDATE(TIM1);
    LL_TIM_SetCounterMode(TIM1, LL_TIM_COUNTERMODE_UP);
    LL_TIM_SetClockDivision(TIM1, LL_TIM_CLOCKDIVISION_DIV1);
    LL_TIM_SetClockSource(TIM1, LL_TIM_CLOCKSOURCE_INTERNAL);
    LL_TIM_SetPrescaler(TIM1, 0U);
    LL_TIM_SetCounter(TIM1, 0U);
    LL_TIM_SetAutoReload(TIM1, audio->pwm_period - 1U);
    LL_TIM_SetRepetitionCounter(
        TIM1,
        (audio->carrier_hz / audio->sample_rate_hz) > 0U ?
            (uint32_t)((audio->carrier_hz / audio->sample_rate_hz) - 1U) :
            0U);
    LL_TIM_DisableMasterSlaveMode(TIM1);
    LL_TIM_EnableARRPreload(TIM1);
    LL_TIM_OC_EnablePreload(TIM1, LL_TIM_CHANNEL_CH1);
    LL_TIM_OC_SetMode(TIM1, LL_TIM_CHANNEL_CH1, LL_TIM_OCMODE_PWM1);
    LL_TIM_OC_SetPolarity(TIM1, LL_TIM_CHANNEL_CH1N, LL_TIM_OCPOLARITY_HIGH);
    LL_TIM_OC_DisableFast(TIM1, LL_TIM_CHANNEL_CH1);
    LL_TIM_OC_SetCompareCH1(TIM1, audio->pwm_midpoint);
    LL_TIM_CC_EnableChannel(TIM1, LL_TIM_CHANNEL_CH1N);
    LL_TIM_EnableAllOutputs(TIM1);

    morse_flipper_audio_pwm_render(
        audio, audio->dma_buffer, MORSE_FLIPPER_AUDIO_PWM_BUFFER_SAMPLES);

    LL_DMA_DisableChannel(MORSE_FLIPPER_AUDIO_PWM_DMA_DEF);
    dma_cfg.PeriphOrM2MSrcAddress = (uint32_t)&TIM1->CCR1;
    dma_cfg.MemoryOrM2MDstAddress = (uint32_t)audio->dma_buffer;
    dma_cfg.Direction = LL_DMA_DIRECTION_MEMORY_TO_PERIPH;
    dma_cfg.Mode = LL_DMA_MODE_CIRCULAR;
    dma_cfg.PeriphOrM2MSrcIncMode = LL_DMA_PERIPH_NOINCREMENT;
    dma_cfg.MemoryOrM2MDstIncMode = LL_DMA_MEMORY_INCREMENT;
    dma_cfg.PeriphOrM2MSrcDataSize = LL_DMA_PDATAALIGN_HALFWORD;
    dma_cfg.MemoryOrM2MDstDataSize = LL_DMA_MDATAALIGN_HALFWORD;
    dma_cfg.NbData = MORSE_FLIPPER_AUDIO_PWM_BUFFER_SAMPLES;
    dma_cfg.PeriphRequest = LL_DMAMUX_REQ_TIM1_UP;
    dma_cfg.Priority = LL_DMA_PRIORITY_HIGH;
    LL_DMA_Init(MORSE_FLIPPER_AUDIO_PWM_DMA_DEF, &dma_cfg);

    morse_flipper_audio_pwm_clear_dma_flags();
    LL_DMA_EnableIT_HT(MORSE_FLIPPER_AUDIO_PWM_DMA_DEF);
    LL_DMA_EnableIT_TC(MORSE_FLIPPER_AUDIO_PWM_DMA_DEF);
    LL_DMA_EnableIT_TE(MORSE_FLIPPER_AUDIO_PWM_DMA_DEF);
    furi_hal_interrupt_set_isr_ex(
        MORSE_FLIPPER_AUDIO_PWM_DMA_IRQ,
        FuriHalInterruptPriorityKamiSama,
        morse_flipper_audio_pwm_dma_isr,
        audio);

    LL_TIM_ClearFlag_UPDATE(TIM1);
    LL_TIM_EnableDMAReq_UPDATE(TIM1);
    LL_DMA_EnableChannel(MORSE_FLIPPER_AUDIO_PWM_DMA_DEF);
    furi_delay_us(5U);
    LL_TIM_GenerateEvent_UPDATE(TIM1);
    furi_delay_us(5U);
    LL_TIM_EnableCounter(TIM1);

    audio->running = true;
    return true;
}

void morse_flipper_audio_pwm_stop(MorseFlipperAudioPwm* audio) {
    if(audio == NULL) return;
    if(!audio->prepared) return;
    if(!audio->running) {
        audio->gate_requested = false;
        audio->gate_applied = false;
        audio->env_state = MorseFlipperAudioPwmEnvIdle;
        audio->env_idx = 0U;
        audio->env_anchor_q15 = 0U;
        audio->own_bus_tim1 = false;
        audio->own_bus_dma1 = false;
        audio->own_bus_dmamux1 = false;
        return;
    }

    /* Stop interrupts and DMA first, then walk the PWM pin back to silence. */
    furi_hal_interrupt_set_isr(MORSE_FLIPPER_AUDIO_PWM_DMA_IRQ, NULL, NULL);
    LL_DMA_DisableIT_HT(MORSE_FLIPPER_AUDIO_PWM_DMA_DEF);
    LL_DMA_DisableIT_TC(MORSE_FLIPPER_AUDIO_PWM_DMA_DEF);
    LL_DMA_DisableIT_TE(MORSE_FLIPPER_AUDIO_PWM_DMA_DEF);
    LL_DMA_DisableChannel(MORSE_FLIPPER_AUDIO_PWM_DMA_DEF);
    morse_flipper_audio_pwm_clear_dma_flags();

    LL_TIM_DisableDMAReq_UPDATE(TIM1);
    LL_TIM_CC_DisableChannel(TIM1, LL_TIM_CHANNEL_CH1N);
    LL_TIM_DisableAllOutputs(TIM1);
    LL_TIM_DisableCounter(TIM1);
    LL_TIM_OC_SetCompareCH1(TIM1, audio->pwm_midpoint);
    LL_TIM_SetRepetitionCounter(TIM1, 0U);
    LL_TIM_CC_EnableChannel(TIM1, LL_TIM_CHANNEL_CH1N);
    LL_TIM_EnableAllOutputs(TIM1);
    LL_TIM_EnableCounter(TIM1);
    morse_flipper_audio_pwm_ramp_midpoint(false, audio->carrier_hz);
    LL_TIM_DisableCounter(TIM1);
    LL_TIM_DisableAllOutputs(TIM1);
    LL_TIM_CC_DisableChannel(TIM1, LL_TIM_CHANNEL_CH1N);

    if(audio->own_bus_tim1) furi_hal_pwm_stop(FuriHalPwmOutputIdTim1PA7);
    if(audio->own_bus_dma1) furi_hal_bus_disable(FuriHalBusDMA1);
    if(audio->own_bus_dmamux1) furi_hal_bus_disable(FuriHalBusDMAMUX1);

    audio->running = false;
    audio->gate_requested = false;
    audio->gate_applied = false;
    audio->env_state = MorseFlipperAudioPwmEnvIdle;
    audio->env_idx = 0U;
    audio->env_anchor_q15 = 0U;
    audio->own_bus_tim1 = false;
    audio->own_bus_dma1 = false;
    audio->own_bus_dmamux1 = false;
}

#endif
