#include "../tpms_app_i.h"
#include <stdio.h>
#include <string.h>

typedef enum { TpmsLabPhaseRx = 0, TpmsLabPhaseQuiet } TpmsLabPhase;
static TpmsLabPhase lab_phase = TpmsLabPhaseRx;
static uint32_t lab_ui_tick = 0U;
static uint32_t lab_led_tick = 0U;
static bool lab_led_on = false;

static void tpms_lab_run_widget_callback(GuiButtonType result, InputType type, void* context) {
    TPMSApp* app = context;
    if(app && type == InputTypeShort)
        view_dispatcher_send_custom_event(app->view_dispatcher, result);
}

static const char* tpms_lab_preset_short(uint8_t preset) {
    if(preset == TpmsRadioPresetGFSK) return "GFSK";
    if(preset == TpmsRadioPresetOOK) return "OOK";
    if(preset == TpmsRadioPresetFSK) return "FSK";
    return "AUTO";
}

static const char* tpms_lab_mod_short(const TPMSApp* app) {
    if(!app || !app->txrx || !app->txrx->preset || !app->txrx->preset->name) return "---";
    const char* name = furi_string_get_cstr(app->txrx->preset->name);
    if(strstr(name, "OOK")) return "OOK";
    if(strstr(name, "GFSK")) return "GFSK";
    return "FSK";
}

static void tpms_lab_draw(TPMSApp* app) {
    widget_reset(app->widget);
    char text[256];
    const uint32_t f = app->txrx && app->txrx->preset ? app->txrx->preset->frequency : 0U;
    char rf[32];
    snprintf(
        rf,
        sizeof(rf),
        "%lu.%02lu %s",
        (unsigned long)(f / 1000000U),
        (unsigned long)((f % 1000000U) / 10000U),
        tpms_lab_mod_short(app));
    char last_tx[48];
    if(app->lab_last_tx_frequency != 0U && app->lab_last_tx_kind != TpmsEncoderNone) {
        snprintf(
            last_tx,
            sizeof(last_tx),
            "%lu.%02lu %s",
            (unsigned long)(app->lab_last_tx_frequency / 1000000U),
            (unsigned long)((app->lab_last_tx_frequency % 1000000U) / 10000U),
            tpms_lab_preset_short(app->lab_last_tx_preset));
    } else {
        snprintf(last_tx, sizeof(last_tx), "---");
    }
    snprintf(
        text,
        sizeof(text),
        "\e#AUTO LAB\n\nSensors: %u/10\nTarget: %.2f bar\nRX: %s\nTX: %s\nTX RF: %s\n\n%s\nRound: %lu\nTX OK: %u/%u\n\nBACK = finish",
        app->lab_sensor_count,
        (double)(app->lab_target_pressure_kpa / 100.0f),
        rf,
        app->lab_last_tx_kind != TpmsEncoderNone ?
            tpms_encoder_display_name((TpmsEncoderKind)app->lab_last_tx_kind) : "---",
        last_tx,
        lab_phase == TpmsLabPhaseRx ? "LISTEN 5 s" : "QUIET 1 s",
        (unsigned long)app->lab_round,
        app->lab_last_tx_ok,
        app->lab_sensor_count);
    widget_add_text_scroll_element(app->widget, 0, 0, 128, 64, text);
    widget_add_button_element(
        app->widget, GuiButtonTypeCenter, "ID", tpms_lab_run_widget_callback, app);
}

static void tpms_lab_rx_led(TPMSApp* app, uint32_t now) {
    if(!app || lab_phase != TpmsLabPhaseRx || (int32_t)(now - lab_led_tick) < 0) return;
    notification_message(
        app->notifications, lab_led_on ? &sequence_reset_rgb : &sequence_set_only_green_255);
    lab_led_on = !lab_led_on;
    lab_led_tick = now + furi_ms_to_ticks(500U);
}

void tpms_scene_lab_run_on_enter(void* context) {
    TPMSApp* app = context;
    if(!app || !app->lab_unlocked || !tpms_lab_prepare(app) || !tpms_lab_start_rx(app)) {
        if(app) {
            tpms_lab_release(app, true);
            notification_message(app->notifications, &sequence_reset_rgb);
            scene_manager_previous_scene(app->scene_manager);
        }
        return;
    }
    lab_phase = TpmsLabPhaseRx;
    lab_ui_tick = 0U;
    lab_led_tick = 0U;
    lab_led_on = false;
    app->lab_phase_started_tick = furi_get_tick();
    tpms_lab_draw(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, TPMSViewWidget);
}

bool tpms_scene_lab_run_on_event(void* context, SceneManagerEvent event) {
    TPMSApp* app = context;
    if(!app) return false;
    if(event.type == SceneManagerEventTypeCustom && event.event == GuiButtonTypeCenter) {
        tpms_lab_stop_rx(app);
        notification_message(app->notifications, &sequence_reset_rgb);
        app->lab_sensor_list_source = TPMSLabListSession;
        scene_manager_next_scene(app->scene_manager, TPMSSceneLabSensors);
        return true;
    }
    if(event.type == SceneManagerEventTypeTick) {
        const uint32_t now = furi_get_tick();
        if(lab_phase == TpmsLabPhaseRx && app->txrx->txrx_state == TPMSTxRxStateRx) {
            tpms_lab_rx_led(app, now);
            SubGhzProtocolDecoderBase* result = proto6_bridge_scan(app->txrx->proto6);
            if(result) (void)tpms_lab_capture_decoder(app, result);
        }
        if(lab_phase == TpmsLabPhaseRx) {
            if((now - app->lab_phase_started_tick) >= furi_ms_to_ticks(5000U)) {
                notification_message(app->notifications, &sequence_reset_rgb);
                lab_led_on = false;
                if(app->lab_sensor_count > 0U) {
                    (void)tpms_lab_send_burst(app);
                    lab_phase = TpmsLabPhaseQuiet;
                    app->lab_phase_started_tick = furi_get_tick();
                } else {
                    /* No sensor yet: rotate AUTO profile safely between RX windows. */
                    if(tpms_lab_start_rx(app)) app->lab_phase_started_tick = furi_get_tick();
                    else app->lab_phase_started_tick = now;
                }
            }
        } else if((now - app->lab_phase_started_tick) >= furi_ms_to_ticks(1000U)) {
            if(tpms_lab_start_rx(app)) {
                lab_phase = TpmsLabPhaseRx;
                app->lab_phase_started_tick = furi_get_tick();
                lab_led_tick = 0U;
                lab_led_on = false;
            }
        }
        if((now - lab_ui_tick) >= furi_ms_to_ticks(500U)) {
            lab_ui_tick = now;
            tpms_lab_draw(app);
        }
        return true;
    }
    return false;
}

void tpms_scene_lab_run_on_exit(void* context) {
    TPMSApp* app = context;
    if(!app) return;
    notification_message(app->notifications, &sequence_reset_rgb);
    /* Keep the just-finished session available for review/delete/save until
       the next LAB START or app restart. tpms_lab_prepare() clears it then. */
    tpms_lab_release(app, false);
    widget_reset(app->widget);
}
