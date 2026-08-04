/*
 * Purpose: Decide when each scene may use audio and vibration outputs.
 * Owns: audio route eligibility and wait-for-transition policy.
 * Depends on: morse_flipper_app_i.h scene/audio state.
 * Tests: firmware build; behaviour is hardware-only.
 */

#include "morse_flipper_app_i.h"

bool morse_flipper_scene_supports_audio_pwm(uint8_t scene) {
    switch(scene) {
    case MorseFlipperSceneAudioCfg:
    case MorseFlipperSceneHamRun:
    case MorseFlipperSceneRun:
    case MorseFlipperSceneTrace:
    case MorseFlipperSceneSession:
    case MorseFlipperSceneSessionEnd:
    case MorseFlipperSceneStraight:
    case MorseFlipperSceneRf:
    case MorseFlipperSceneRfRx:
        return true;
    default:
        return false;
    }
}

bool morse_flipper_audio_path_is_sampled(uint8_t audio_path) {
    return audio_path == MorseFlipperAudioPathGpioP2Hd ||
           audio_path == MorseFlipperAudioPathSoftBuzz;
}

bool morse_flipper_audio_wait_transition(
    const MorseFlipperApp* app,
    uint8_t old_scene,
    uint8_t new_scene) {
    if(app == NULL || !morse_flipper_audio_path_is_sampled(app->audio_path) ||
       old_scene == new_scene)
        return false;
    return morse_flipper_scene_supports_audio_pwm(old_scene) ||
           morse_flipper_scene_supports_audio_pwm(new_scene);
}

uint8_t morse_flipper_p2_volume_pct(const MorseFlipperApp* app) {
    uint8_t pct = app ? app->p2_volume_pct : 100U;

    if(pct < 10U) pct = 10U;
    if(pct > 100U) pct = 100U;
    return pct;
}

static uint8_t morse_flipper_soft_buzz_volume_pct(const MorseFlipperApp* app) {
    return morse_flipper_p2_volume_pct(app);
}

bool morse_flipper_audio_output_is_pwm(const MorseFlipperApp* app) {
    if(app == NULL) return false;
    if(app->screen == MorseFlipperScreenHamRun) return false;
    return morse_flipper_audio_path_is_sampled(app->audio_path) &&
           morse_flipper_scene_supports_audio_pwm(app->scene);
}

void morse_flipper_sync_audio_output(MorseFlipperApp* app) {
    if(app == NULL) return;

    if(app->audio_pwm.running) morse_flipper_audio_pwm_stop(&app->audio_pwm);

    if(morse_flipper_audio_output_is_pwm(app)) {
        if(app->audio_path == MorseFlipperAudioPathSoftBuzz) {
            if(app->speaker_owned || app->tone_on) morse_flipper_tone_stop(app);
            morse_flipper_audio_pwm_prepare_target(
                &app->audio_pwm,
                MorseFlipperAudioPwmTargetSoftBuzz,
                MORSE_FLIPPER_AUDIO_PWM_SOFT_BUZZ_CARRIER_HZ,
                MORSE_FLIPPER_AUDIO_PWM_SOFT_BUZZ_SAMPLE_RATE_HZ,
                (uint32_t)(morse_flipper_active_tone_hz(app) + 0.5f),
                morse_flipper_soft_buzz_volume_pct(app),
                MORSE_FLIPPER_AUDIO_PWM_FADE_MS,
                MORSE_FLIPPER_AUDIO_PWM_FADE_MS);
        } else {
            morse_flipper_audio_pwm_prepare_target(
                &app->audio_pwm,
                MorseFlipperAudioPwmTargetP2,
                MORSE_FLIPPER_AUDIO_PWM_P2_CARRIER_HZ,
                MORSE_FLIPPER_AUDIO_PWM_P2_SAMPLE_RATE_HZ,
                (uint32_t)(morse_flipper_active_tone_hz(app) + 0.5f),
                morse_flipper_p2_volume_pct(app),
                MORSE_FLIPPER_AUDIO_PWM_FADE_MS,
                MORSE_FLIPPER_AUDIO_PWM_FADE_MS);
        }
        morse_flipper_audio_pwm_start(&app->audio_pwm);
    }

    morse_flipper_update_sidetone(app);
}
