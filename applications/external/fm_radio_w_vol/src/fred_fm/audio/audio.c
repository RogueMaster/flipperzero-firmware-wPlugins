/**
 * @file audio.c
 * @brief PT2257/PT2259 and PAM8406 audio output control.
 */
#include "src/fred_fm/include/config.h"
#include "src/fred_fm/include/types.h"
#include "src/fred_fm/core/core.h"
#include "src/fred_fm/audio/audio.h"

#include <furi.h>
#include "src/drivers/pt/PT22xx.h"
#include "src/drivers/pam/PAM8406.h"

/** Apply selected PT chip type and I2C address to the driver. */
void fred_fm_pt_apply_config(void) {
    pt22xx_set_chip(pt_chip);
    pt22xx_set_i2c_addr(pt_i2c_addr8);
}

/** Probe/init PT2257/PT2259; updates @c pt_ready_cached. */
bool fred_fm_pt_refresh_state(bool force_init) {
    bool local_initialized;

    fred_fm_state_lock();
    local_initialized = pt_initialized_cached;
    fred_fm_state_unlock();

    fred_fm_pt_apply_config();
    bool ready = pt22xx_is_device_ready();

    if(!ready) {
        local_initialized = false;
    } else if(force_init || !local_initialized) {
        local_initialized = pt22xx_init();
        if(!local_initialized) ready = false;
    }

    fred_fm_state_lock();
    pt_ready_cached = ready;
    pt_initialized_cached = local_initialized;
    fred_fm_state_unlock();

    return ready;
}

/** Push mute/attenuation to PT chip (no-op if not ready). */
void fred_fm_apply_pt_state(void) {
    fred_fm_state_lock();
    bool local_ready = pt_ready_cached;
    bool local_muted = current_volume;
    uint8_t local_atten_db = pt_atten_db;
    fred_fm_state_unlock();

    if(!local_ready) return;

    PT22xxState state = {
        .attenuation_db = local_atten_db,
        .muted = local_muted,
    };

    (void)pt22xx_apply_state(&state);
}

/** Sync PT volume and PAM8406 power/mode from app state. */
void fred_fm_apply_audio_output_state(void) {
    if(fred_fm_app_exiting) return;
    fred_fm_state_lock();
    bool local_muted = current_volume;
    bool local_amp_power = amp_power_enabled;
    bool local_amp_mode_class_d = amp_mode_class_d;
    fred_fm_state_unlock();

    fred_fm_apply_pt_state();

    PAM8406State amp_state = {
        .powered = local_amp_power,
        .muted = local_muted,
        .class_d_mode = local_amp_mode_class_d,
    };
    pam8406_apply_state(&amp_state);
}

/** Mute PT and shut down PAM8406 (app exit or alloc failure). */
void fred_fm_audio_shutdown(void) {
    bool local_ready = false;
    uint8_t local_atten_db = pt_atten_db;

    if(state_mutex) {
        fred_fm_state_lock();
        local_ready = pt_ready_cached;
        local_atten_db = pt_atten_db;
        fred_fm_state_unlock();
    }

    if(local_ready) {
        PT22xxState state = {
            .attenuation_db = local_atten_db,
            .muted = true,
        };
        (void)pt22xx_apply_state(&state);
    }

    pam8406_shutdown();
}
