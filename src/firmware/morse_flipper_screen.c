/*
 * Purpose: Apply screen changes and their immediate side effects.
 * Owns: screen/scene transition state, audio waits, and per-screen resets.
 * Depends on: morse_flipper_app_i.h and runtime feature modules.
 * Tests: firmware build; screen flow is hardware-only.
 */

#include "morse_flipper_app_i.h"

void morse_flipper_enter_screen(
    MorseFlipperApp* app,
    uint8_t screen,
    uint8_t scene,
    uint32_t now_ms) {
    uint8_t old_screen;
    uint8_t old_scene;
    bool audio_wait;

    if(app->screen == screen && app->scene == scene) return;
    old_screen = app->screen;
    old_scene = app->scene;
    audio_wait = morse_flipper_audio_wait_transition(app, old_scene, scene);

    if(app->screen == MorseFlipperScreenSession && screen != MorseFlipperScreenSession &&
       screen != MorseFlipperScreenSessionEnd) {
        morse_flipper_reset_session_state(app, now_ms);
    }

    if(app->screen == MorseFlipperScreenSessionEnd && screen != MorseFlipperScreenSessionEnd) {
        morse_flipper_reset_session_state(app, now_ms);
    }

    if(app->screen == MorseFlipperScreenStraight && screen != MorseFlipperScreenStraight) {
        morse_flipper_reset_straight_state(app, now_ms);
    }

    if(app->screen == MorseFlipperScreenIcr && screen != MorseFlipperScreenIcr) {
        morse_flipper_plugin_runtime_unload_current(app);
    }

    if(app->screen == MorseFlipperScreenRxPractice && screen != MorseFlipperScreenRxPractice) {
        morse_flipper_plugin_runtime_unload_current(app);
    }

    if(app->screen == MorseFlipperScreenPassive && screen != MorseFlipperScreenPassive) {
        morse_flipper_plugin_runtime_unload_current(app);
    }

    if((app->screen == MorseFlipperScreenTxGroups ||
        app->screen == MorseFlipperScreenTxGroupsResult ||
        app->screen == MorseFlipperScreenTxGroupsFinal) &&
       screen != MorseFlipperScreenTxGroups && screen != MorseFlipperScreenTxGroupsResult &&
       screen != MorseFlipperScreenTxGroupsFinal) {
        morse_flipper_plugin_runtime_unload_current(app);
    }

    if(app->scene == MorseFlipperSceneRun && scene != MorseFlipperSceneRun) {
        morse_flipper_clear_run_wpm(app, now_ms);
    }

    if(app->screen == MorseFlipperScreenHamRun && screen != MorseFlipperScreenHamRun) {
        morse_flipper_ham_log_flush(app);
        morse_flipper_ham_stop_macro(app);
        morse_flipper_ham_gpio_release(app);
        morse_flipper_clear_run_wpm(app, now_ms);
        app->ham.wpm_hold_key = MORSE_FLIPPER_HAM_WPM_HOLD_NONE;
        app->ham.wpm_hold_next_at = 0U;
    }

    morse_flipper_clear_button_keying(app, now_ms);

    if(screen == MorseFlipperScreenSession && app->screen != MorseFlipperScreenSession &&
       app->screen != MorseFlipperScreenSessionEnd) {
        morse_flipper_reset_session_state(app, now_ms);
    }

    if(screen == MorseFlipperScreenSessionEnd && app->screen != MorseFlipperScreenSessionEnd) {
        morse_flipper_drop_live_keying_for_playback(app, now_ms);
        morse_flipper_update_sidetone(app);
    }

    if(screen == MorseFlipperScreenTxGroups && app->screen != MorseFlipperScreenTxGroups &&
       app->screen != MorseFlipperScreenTxGroupsResult &&
       app->screen != MorseFlipperScreenTxGroupsFinal) {
        morse_flipper_reset_tx_groups_state(app, now_ms);
    }

    if(screen == MorseFlipperScreenStraight && app->screen != MorseFlipperScreenStraight) {
        morse_flipper_reset_straight_state(app, now_ms);
    }

    if(screen == MorseFlipperScreenIcr && app->screen != MorseFlipperScreenIcr) {
        /* ICR has no physical-key answer path; discard queued keyer elements before entry. */
        morse_flipper_drop_live_keying_for_playback(app, now_ms);
        morse_keyer_reset(&app->keyer);
        morse_flipper_drain_keyer_events(app);
        morse_flipper_release_all_notes(app);
    }

    if(scene == MorseFlipperSceneRun && app->scene != MorseFlipperSceneRun) {
        app->preview_ticks = 0U;
        app->run_dit_ms = morse_flipper_current_dit_ms(app);
        morse_flipper_reset_run_state(app);
    }

    if(screen == MorseFlipperScreenHamRun && app->screen != MorseFlipperScreenHamRun) {
        app->preview_ticks = 0U;
        app->run_dit_ms = morse_flipper_current_dit_ms(app);
        app->ham.wpm_hold_key = MORSE_FLIPPER_HAM_WPM_HOLD_NONE;
        app->ham.wpm_hold_next_at = 0U;
        morse_flipper_reset_run_state(app);
        morse_flipper_ham_gpio_apply(app);
    }

    if(screen == MorseFlipperScreenTrace && app->screen != MorseFlipperScreenTrace) {
        app->tx_text[0] = '\0';
        app->gpio_text[0] = '\0';
        morse_flipper_cw_decoder_init(&app->tx_decoder, morse_flipper_current_dit_ms(app));
        morse_flipper_cw_decoder_init(&app->gpio_decoder, morse_flipper_current_dit_ms(app));
        app->tx_edge_at = 0U;
        app->gpio_edge_at = 0U;
        app->tx_gap_flushed = true;
        app->gpio_gap_flushed = true;
    }

    app->screen = screen;
    app->scene = scene;
    if(morse_flipper_gpio_probe_screen(app)) {
        morse_flipper_gpio_probe_prepare(app, now_ms);
    } else if(!morse_flipper_gpio_probe_keep_state(screen)) {
        morse_flipper_gpio_probe_reset(app);
    }
    if(old_screen == MorseFlipperScreenStraight || screen == MorseFlipperScreenStraight ||
       old_screen == MorseFlipperScreenTxGroups || screen == MorseFlipperScreenTxGroups) {
        morse_flipper_refresh_keyer(app, now_ms);
    }
    if(screen == MorseFlipperScreenHome) {
        morse_flipper_poll(app);
    }
    if(audio_wait) {
        app->audio_wait_active = true;
        morse_flipper_view_dirty(app);
        furi_delay_ms(MORSE_FLIPPER_AUDIO_WAIT_DRAW_MS);
    }
    if(old_scene != scene || morse_flipper_scene_supports_audio_pwm(scene) ||
       morse_flipper_scene_supports_audio_pwm(old_scene))
        morse_flipper_sync_audio_output(app);
    if(audio_wait) app->audio_wait_active = false;
    morse_flipper_view_dirty(app);
}
