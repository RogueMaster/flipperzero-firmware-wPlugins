#include "tpms_editor_tx.h"
#include "tpms_app_i.h"
#include "protocols/proto6/custom_presets.h"

#include <lib/toolbox/level_duration.h>

#define TAG "TPMSTX14"
#define TPMS_REPEAT_GAP_US 10000U

static TpmsTxWave tx_wave;

typedef struct {
    const TpmsTxWave* wave;
    size_t index;
    uint8_t repeat;
    uint8_t repeat_target;
} TpmsTxContext;

static TpmsTxContext tx_context;

static LevelDuration tpms_tx_yield(void* context) {
    TpmsTxContext* tx = context;
    if(!tx || !tx->wave || tx->wave->pulse_count == 0U) return level_duration_reset();

    if(tx->index >= tx->wave->pulse_count) {
        tx->repeat++;
        if(tx->repeat >= tx->repeat_target) return level_duration_reset();
        tx->index = 0U;
        return level_duration_make(false, TPMS_REPEAT_GAP_US);
    }

    const TpmsPulse* pulse = &tx->wave->pulses[tx->index++];
    return level_duration_make(pulse->level, pulse->duration_us);
}

static const uint8_t* tpms_tx_preset_data(const TpmsEditValues* values, modulation_t modulation) {
    /* AUTO LAB can receive the same protocol using a different physical CC1101
     * preset (most importantly Toyota on the GFSK preset). Preserve that exact
     * RX preset for TX instead of guessing it again from the protocol family. */
    if(values) {
        if(values->radio_preset == TpmsRadioPresetGFSK) {
            return (const uint8_t*)protoview_subghz_tpms3_gfsk_async_regs;
        }
        if(values->radio_preset == TpmsRadioPresetOOK) {
            return (const uint8_t*)protoview_subghz_tpms2_ook_async_regs;
        }
        if(values->radio_preset == TpmsRadioPresetFSK) {
            return (const uint8_t*)protoview_subghz_tpms1_fsk_async_regs;
        }
    }
    if(modulation == OOK_PULSE_PCM || modulation == OOK_PULSE_MANCHESTER_ZEROBIT) {
        return (const uint8_t*)protoview_subghz_tpms2_ook_async_regs;
    }
    return (const uint8_t*)protoview_subghz_tpms1_fsk_async_regs;
}

static void tpms_restore_rx(TPMSApp* app) {
    proto6_bridge_reset(app->txrx->proto6);
    tpms_begin(app, app->txrx->preset->data);
    tpms_rx(app, app->txrx->preset->frequency);
}

bool tpms_editor_send(TPMSApp* app, const TpmsEditValues* values) {
    furi_assert(app);
    if(!values || values->kind == TpmsEncoderNone) return false;
    if(!tpms_encoder_build(values, &tx_wave) || tx_wave.pulse_count == 0U ||
       tx_wave.overflowed) {
        FURI_LOG_E(TAG, "Frame build failed");
        return false;
    }

    uint8_t repeat = values->repeat_count;
    if(repeat == 0U) repeat = 1U;
    if(repeat > 10U) repeat = 10U;

    const bool rx_was_running = app->txrx->txrx_state == TPMSTxRxStateRx;
    if(rx_was_running) tpms_rx_end(app);

    bool ok = false;
    bool async_started = false;
    do {
        if(!tpms_radio_ensure_selected(app) || !app->txrx->radio_device) break;
        if(!subghz_devices_is_frequency_valid(app->txrx->radio_device, tx_wave.frequency_hz)) break;

        subghz_devices_reset(app->txrx->radio_device);
        subghz_devices_idle(app->txrx->radio_device);
        subghz_devices_load_preset(
            app->txrx->radio_device,
            FuriHalSubGhzPresetCustom,
            (uint8_t*)tpms_tx_preset_data(values, tx_wave.modulation));
        subghz_devices_set_frequency(app->txrx->radio_device, tx_wave.frequency_hz);
        if(!subghz_devices_set_tx(app->txrx->radio_device)) {
            FURI_LOG_E(TAG, "Selected radio rejected TX");
            break;
        }

        tx_context.wave = &tx_wave;
        tx_context.index = 0U;
        tx_context.repeat = 0U;
        tx_context.repeat_target = repeat;
        app->txrx->txrx_state = TPMSTxRxStateTx;

        async_started = subghz_devices_start_async_tx(
            app->txrx->radio_device, tpms_tx_yield, &tx_context);
        if(!async_started) {
            FURI_LOG_E(TAG, "Async TX start failed");
            break;
        }

        const uint32_t started = furi_get_tick();
        const uint32_t timeout = furi_ms_to_ticks(10000U);
        while(!subghz_devices_is_async_complete_tx(app->txrx->radio_device)) {
            if((furi_get_tick() - started) > timeout) break;
            furi_delay_ms(2U);
        }
        ok = subghz_devices_is_async_complete_tx(app->txrx->radio_device);
        subghz_devices_stop_async_tx(app->txrx->radio_device);
        async_started = false;
        subghz_devices_idle(app->txrx->radio_device);
    } while(false);

    if(async_started && app->txrx->radio_device) {
        subghz_devices_stop_async_tx(app->txrx->radio_device);
    }
    if(app->txrx->radio_device) subghz_devices_idle(app->txrx->radio_device);

    app->txrx->txrx_state = TPMSTxRxStateIDLE;
    tx_context.wave = NULL;
    if(rx_was_running) tpms_restore_rx(app);
    return ok;
}
