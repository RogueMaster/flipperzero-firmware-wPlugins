#include "morse_flipper_app_i.h"

#define MORSE_FLIPPER_PASSIVE_PLUGIN_PATH \
    APP_ASSETS_PATH("plugins/morse_flipper_passive_listening.fal")
#define MORSE_FLIPPER_PASSIVE_SETTINGS_PLUGIN_PATH \
    APP_ASSETS_PATH("plugins/morse_flipper_passive_settings.fal")

static bool mf_passive_command(
    void* context,
    MfPassiveHostCommand command,
    uint32_t value,
    MfPassivePcmPipe* pipe) {
    MorseFlipperApp* app = context;
    MorseFlipperAudioPwmTarget pwm_target;
    uint32_t carrier_hz;
    uint32_t sample_rate_hz;
    if(app == NULL) return false;
    if(command == MfPassiveHostCommandSilence) {
        morse_flipper_audio_pwm_set_silence(&app->audio_pwm);
        return true;
    }
    if(command == MfPassiveHostCommandTone) {
        if(!app->audio_pwm.running) return false;
        morse_flipper_audio_pwm_set_tone_hz(&app->audio_pwm, value);
        morse_flipper_audio_pwm_set_gate(&app->audio_pwm, true);
        return true;
    }
    if(command == MfPassiveHostCommandVoice) {
        if(!app->audio_pwm.running) return false;
        morse_flipper_audio_pwm_set_voice(&app->audio_pwm, app->audio_pwm.voice_pipe, value);
        return app->audio_pwm.voice_primed;
    }
    if(command == MfPassiveHostCommandVibration) {
        furi_hal_vibro_on(value != 0U);
        return true;
    }
    if(command == MfPassiveHostCommandRelease) {
        morse_flipper_audio_pwm_stop(&app->audio_pwm);
        return true;
    }
    if(command != MfPassiveHostCommandClaim || pipe == NULL) return false;
    if(app->audio_pwm.running) morse_flipper_audio_pwm_stop(&app->audio_pwm);
    if(app->speaker_owned || app->tone_on) morse_flipper_tone_stop(app);
    furi_hal_vibro_on(false);
    if((uint8_t)(value >> 8U) == MfPassiveOutputP2) {
        pwm_target = MorseFlipperAudioPwmTargetP2;
        carrier_hz = MORSE_FLIPPER_AUDIO_PWM_P2_CARRIER_HZ;
        sample_rate_hz = MORSE_FLIPPER_AUDIO_PWM_P2_SAMPLE_RATE_HZ;
    } else {
        pwm_target = MorseFlipperAudioPwmTargetSoftBuzz;
        carrier_hz = MORSE_FLIPPER_AUDIO_PWM_SOFT_BUZZ_CARRIER_HZ;
        sample_rate_hz = MORSE_FLIPPER_AUDIO_PWM_SOFT_BUZZ_SAMPLE_RATE_HZ;
    }
    morse_flipper_audio_pwm_prepare_target(
        &app->audio_pwm,
        pwm_target,
        carrier_hz,
        sample_rate_hz,
        value >> 16U,
        (uint8_t)value,
        MORSE_FLIPPER_AUDIO_PWM_FADE_MS,
        MORSE_FLIPPER_AUDIO_PWM_FADE_MS);
    if(!morse_flipper_audio_pwm_start(&app->audio_pwm)) return false;
    app->audio_pwm.voice_pipe = pipe;
    return true;
}

static MfPassiveHostServices mf_passive_services = {
    .struct_size = sizeof(MfPassiveHostServices),
    .command = mf_passive_command,
};

bool morse_flipper_passive_host_enter(MorseFlipperApp* app, uint32_t now_ms, uint8_t entry_kind) {
    MorseFlipperMappedFalResult initial = {0};
    MfPassiveEnterArgs args;
    MfPassiveOutputTarget target;
    bool entered;
    const char* plugin_path;
    uint32_t api_magic;

    if(app == NULL || app->plugin_slot.mutex == NULL) return false;
    if(entry_kind == MfPassiveEntrySettings) {
        if(app->settings_list == NULL) return false;
        args = (MfPassiveEnterArgs){
            .struct_size = sizeof(args),
            .entry_kind = MfPassiveEntrySettings,
            .entry.settings =
                {
                    .list = app->settings_list,
                },
        };
        plugin_path = MORSE_FLIPPER_PASSIVE_SETTINGS_PLUGIN_PATH;
        api_magic = MF_PASSIVE_SETTINGS_API_MAGIC;
    } else if(entry_kind == MfPassiveEntryPlayback) {
        morse_flipper_drop_live_keying_for_playback(app, now_ms);
        morse_flipper_release_all_notes(app);
        morse_flipper_reset_answer_decoder(app);
        morse_flipper_sync_ptt(app, now_ms);
        target = app->audio_path == MorseFlipperAudioPathGpioP2Hd ? MfPassiveOutputP2 :
                                                                    MfPassiveOutputInternal;
        args = (MfPassiveEnterArgs){
            .struct_size = sizeof(args),
            .entry_kind = MfPassiveEntryPlayback,
            .entry.playback =
                {
                    .now_ms = now_ms,
                    .rng_seed = furi_hal_random_get(),
                    .frequency_hz = app->rf_frequency_hz,
                    .tone_hz = (uint16_t)(morse_flipper_active_tone_hz(app) + 0.5f),
                    .output_target = target,
                    .volume_pct = morse_flipper_p2_volume_pct(app),
                    .services = &mf_passive_services,
                },
        };
        plugin_path = MORSE_FLIPPER_PASSIVE_PLUGIN_PATH;
        api_magic = MF_PASSIVE_API_MAGIC;
    } else {
        return false;
    }
    mf_passive_services.context = app;
    furi_mutex_acquire(app->plugin_slot.mutex, FuriWaitForever);
    entered = morse_flipper_plugin_runtime_open_mapped_locked(
        app,
        MorseFlipperPluginOwnerPassive,
        entry_kind == MfPassiveEntrySettings ? 1U : 0U,
        plugin_path,
        MF_PASSIVE_API_VERSION,
        api_magic,
        sizeof(MfPassiveApi),
        &args,
        &initial);
    if(entered) app->plugin_slot.phase = initial.phase;
    furi_mutex_release(app->plugin_slot.mutex);
    if(entered && entry_kind == MfPassiveEntryPlayback) morse_flipper_view_dirty(app);
    return entered;
}
