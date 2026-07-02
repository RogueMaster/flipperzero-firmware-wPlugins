/*
 * Purpose: Publish the GPIO P2 PWM audio backend interface.
 * Owns: audio PWM constants, backend state, and lifecycle API.
 * Depends on: host-safe integer types; FAP-only code lives in the .c file.
 * Tests: tests/test_audio_pwm.c.
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MORSE_FLIPPER_AUDIO_PWM_SAMPLE_RATE_HZ      32000U
#define MORSE_FLIPPER_AUDIO_PWM_CARRIER_HZ          256000U
#define MORSE_FLIPPER_AUDIO_PWM_TONE_HZ             700U
#define MORSE_FLIPPER_AUDIO_PWM_FADE_MS             2U
#define MORSE_FLIPPER_AUDIO_PWM_RAMP_MS             250U
#define MORSE_FLIPPER_AUDIO_PWM_RAMP_STEPS          10U
#define MORSE_FLIPPER_AUDIO_PWM_SINE_SAMPLES        64U
#define MORSE_FLIPPER_AUDIO_PWM_BUFFER_SAMPLES      32U
#define MORSE_FLIPPER_AUDIO_PWM_BUFFER_HALF_SAMPLES (MORSE_FLIPPER_AUDIO_PWM_BUFFER_SAMPLES / 2U)
#define MORSE_FLIPPER_AUDIO_PWM_Q15                 32767U

typedef enum {
    MorseFlipperAudioPwmEnvIdle = 0,
    MorseFlipperAudioPwmEnvAttack = 1,
    MorseFlipperAudioPwmEnvSustain = 2,
    MorseFlipperAudioPwmEnvRelease = 3,
} MorseFlipperAudioPwmEnvState;

typedef struct {
    bool prepared;
    bool running;
    volatile bool gate_requested;
    bool gate_applied;
    uint32_t sample_rate_hz;
    uint32_t carrier_hz;
    uint32_t tone_hz;
    uint32_t phase_q32;
    uint32_t phase_step_q32;
    uint16_t pwm_period;
    uint16_t pwm_midpoint;
    uint16_t pwm_amplitude;
    uint16_t attack_len;
    uint16_t release_len;
    uint16_t env_idx;
    uint16_t env_anchor_q15;
    MorseFlipperAudioPwmEnvState env_state;
    uint16_t attack_q15[MORSE_FLIPPER_AUDIO_PWM_SINE_SAMPLES];
    uint16_t release_q15[MORSE_FLIPPER_AUDIO_PWM_SINE_SAMPLES];
    uint16_t dma_buffer[MORSE_FLIPPER_AUDIO_PWM_BUFFER_SAMPLES];
#ifdef MORSE_FLIPPER_FAP
    bool own_bus_tim1;
    bool own_bus_dma1;
    bool own_bus_dmamux1;
#endif
} MorseFlipperAudioPwm;

void morse_flipper_audio_pwm_reset(MorseFlipperAudioPwm* audio);
void morse_flipper_audio_pwm_prepare(
    MorseFlipperAudioPwm* audio,
    uint32_t carrier_hz,
    uint32_t sample_rate_hz,
    uint32_t tone_hz,
    uint8_t volume_pct,
    uint16_t attack_ms,
    uint16_t release_ms);
void morse_flipper_audio_pwm_set_tone_hz(MorseFlipperAudioPwm* audio, uint32_t tone_hz);
void morse_flipper_audio_pwm_set_gate(MorseFlipperAudioPwm* audio, bool gate);
void morse_flipper_audio_pwm_render(MorseFlipperAudioPwm* audio, uint16_t* dst, size_t count);
bool morse_flipper_audio_pwm_sound_active(const MorseFlipperAudioPwm* audio);

#ifdef MORSE_FLIPPER_FAP
bool morse_flipper_audio_pwm_start(MorseFlipperAudioPwm* audio);
void morse_flipper_audio_pwm_stop(MorseFlipperAudioPwm* audio);
#endif
