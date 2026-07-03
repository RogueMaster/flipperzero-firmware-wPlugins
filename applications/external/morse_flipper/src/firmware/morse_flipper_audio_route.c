/*
 * Purpose: Decide when each scene may use audio and vibration outputs.
 * Owns: audio route eligibility and wait-for-transition policy.
 * Depends on: morse_flipper_app_i.h scene/audio state.
 * Tests: firmware build; behaviour is hardware-only.
 */

#include "morse_flipper_app_i.h"

bool morse_flipper_scene_supports_audio_pwm(uint8_t scene) {
    switch(scene) {
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

bool morse_flipper_audio_wait_transition(
    const MorseFlipperApp* app,
    uint8_t old_scene,
    uint8_t new_scene) {
    if(app == NULL || app->audio_path != MorseFlipperAudioPathGpioP2Hd || old_scene == new_scene)
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

bool morse_flipper_audio_output_is_pwm(const MorseFlipperApp* app) {
    if(app == NULL) return false;
    if(app->screen == MorseFlipperScreenHamRun) return false;
    return app->audio_path == MorseFlipperAudioPathGpioP2Hd &&
           morse_flipper_scene_supports_audio_pwm(app->scene);
}

void morse_flipper_sync_audio_output(MorseFlipperApp* app) {
    if(app == NULL) return;

    if(app->audio_pwm.running) morse_flipper_audio_pwm_stop(&app->audio_pwm);

    if(morse_flipper_audio_output_is_pwm(app)) {
        morse_flipper_audio_pwm_prepare(
            &app->audio_pwm,
            MORSE_FLIPPER_AUDIO_PWM_CARRIER_HZ,
            MORSE_FLIPPER_AUDIO_PWM_SAMPLE_RATE_HZ,
            (uint32_t)(morse_flipper_active_tone_hz(app) + 0.5f),
            morse_flipper_p2_volume_pct(app),
            MORSE_FLIPPER_AUDIO_PWM_FADE_MS,
            MORSE_FLIPPER_AUDIO_PWM_FADE_MS);
        morse_flipper_audio_pwm_start(&app->audio_pwm);
    }

    morse_flipper_update_sidetone(app);
}
