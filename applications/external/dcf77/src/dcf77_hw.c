#include "dcf77_hw.h"
#include "dcf77_app.h"
#include "dcf77_logic.h"
#include "radio_clock_protocol.h"

#include <stm32wbxx_ll_lptim.h>
#include <stm32wbxx_ll_bus.h>
#include <stm32wbxx_ll_rcc.h>

#define DCF77_SCHEDULER          LPTIM2
#define DCF77_TEST_PULSE_MS      100U
#define DCF77_TEST_PERIOD_SLICES 5U

static const GpioPinRecord* dcf77_gpio_pin_record(uint8_t pin_number) {
    if(pin_number == 0U) {
        return NULL;
    }

    return furi_hal_resources_pin_by_number(pin_number);
}

static const GpioPinRecord* dcf77_output_gpio_record(const AppFSM* app_fsm) {
    return dcf77_gpio_pin_record(app_fsm->gpio_baseband_pin_number);
}

static const GpioPinRecord* dcf77_gpio_rf_record(const AppFSM* app_fsm) {
    return dcf77_gpio_pin_record(app_fsm->gpio_rf_pin_number);
}

static void
    dcf77_hw_led_color_levels(const AppFSM* app_fsm, uint8_t* red, uint8_t* green, uint8_t* blue) {
    *red = 0U;
    *green = 0U;
    *blue = 0U;

    switch(app_fsm->led_color_index) {
    case Dcf77LedColorRed:
        *red = 0xFFU;
        break;
    case Dcf77LedColorOrange:
        *red = 0xFFU;
        *green = 0x40U;
        break;
    case Dcf77LedColorGreen:
        *green = 0xFFU;
        break;
    case Dcf77LedColorBlue:
        *blue = 0xFFU;
        break;
    case Dcf77LedColorYellow:
        *red = 0xFFU;
        *green = 0xFFU;
        break;
    case Dcf77LedColorCyan:
        *green = 0xFFU;
        *blue = 0xFFU;
        break;
    case Dcf77LedColorMagenta:
        *red = 0xFFU;
        *blue = 0xFFU;
        break;
    case Dcf77LedColorDarkViolet:
        *red = 0x60U;
        *blue = 0xC0U;
        break;
    case Dcf77LedColorPink:
        *red = 0xFFU;
        *blue = 0x90U;
        break;
    case Dcf77LedColorWhite:
        *red = 0xFFU;
        *green = 0xFFU;
        *blue = 0xFFU;
        break;
    default:
        break;
    }
}

static bool dcf77_subghz_ook_level(bool output) {
#if SUBGHZ_OOK_INVERT
    return !output;
#else
    return output;
#endif
}

static bool dcf77_subghz_fsk_enter_static(void) {
    furi_hal_gpio_init(&gpio_cc1101_g0, GpioModeOutputPushPull, GpioPullNo, GpioSpeedLow);
    furi_hal_gpio_write(&gpio_cc1101_g0, false);
    return furi_hal_subghz_tx();
}

static void dcf77_subghz_init(AppFSM* app_fsm, const uint8_t* preset) {
    if(app_fsm->subghz_ready) {
        return;
    }

    app_fsm->subghz_async_tx = false;
    app_fsm->subghz_output = false;
    app_fsm->subghz_tone_phase = false;

    furi_hal_subghz_reset();
    furi_hal_subghz_load_custom_preset(preset);
    furi_hal_subghz_set_frequency_and_path(dcf77_app_get_subghz_frequency(app_fsm));
    furi_hal_gpio_init(&gpio_cc1101_g0, GpioModeInput, GpioPullNo, GpioSpeedLow);

    app_fsm->subghz_ready = true;
}

static LevelDuration dcf77_subghz_fsk_yield(void* context) {
    AppFSM* app_fsm = context;

    if(!dcf77_app_subghz_runtime_enabled(app_fsm) ||
       (app_fsm->subghz_signal_mode != SubGhzSignalModeFsk &&
        app_fsm->subghz_signal_mode != SubGhzSignalModeFskFull)) {
        return level_duration_reset();
    }

    app_fsm->subghz_tone_phase = !app_fsm->subghz_tone_phase;
    app_fsm->subghz_output = app_fsm->subghz_tone_phase;
    return level_duration_make(
        app_fsm->subghz_tone_phase,
        dcf77_subghz_note_half_period_us(app_fsm->subghz_fsk_tone_index));
}

static uint32_t dcf77_scheduler_sanitize_ticks(uint32_t ticks) {
    if(ticks < DCF77_MIN_TIMER_TICKS) {
        return DCF77_MIN_TIMER_TICKS;
    }

    return ticks;
}

static void dcf77_scheduler_reset(void) {
    furi_hal_bus_reset(FuriHalBusLPTIM2);
    NVIC_ClearPendingIRQ(LPTIM2_IRQn);
}

static void dcf77_scheduler_prepare(void) {
    LL_LPTIM_Enable(DCF77_SCHEDULER);
    while(!LL_LPTIM_IsEnabled(DCF77_SCHEDULER)) {
    }

    LL_LPTIM_ClearFlag_CMPM(DCF77_SCHEDULER);
    LL_LPTIM_ClearFlag_ARRM(DCF77_SCHEDULER);
    LL_LPTIM_SetAutoReload(DCF77_SCHEDULER, DCF77_FULL_SECOND_TICKS - 1U);
    LL_LPTIM_EnableIT_ARRM(DCF77_SCHEDULER);
}

static void dcf77_scheduler_set_compare_ticks(uint32_t ticks) {
    const uint32_t compare_ticks = dcf77_scheduler_sanitize_ticks(ticks) - 1U;
    LL_LPTIM_ClearFlag_CMPM(DCF77_SCHEDULER);
    LL_LPTIM_SetCompare(DCF77_SCHEDULER, compare_ticks);
    LL_LPTIM_EnableIT_CMPM(DCF77_SCHEDULER);
}

static void dcf77_scheduler_disable_compare(void) {
    LL_LPTIM_DisableIT_CMPM(DCF77_SCHEDULER);
    LL_LPTIM_ClearFlag_CMPM(DCF77_SCHEDULER);
}

static void dcf77_apply_output(AppFSM* app_fsm, bool output) {
    if(app_fsm->tx_active && app_fsm->current_signal != RadioClockSignalTest &&
       !app_fsm->tx_frame_enabled) {
        output = false;
    }

    if(app_fsm->output_state == output) {
        return;
    }

    app_fsm->output_state = output;
    app_fsm->output_dirty = true;

    dcf77_output_gpio_set(app_fsm, output);
    if(app_fsm->lf_ready) {
        if(output) {
            furi_hal_rfid_tim_read_continue();
        } else {
            furi_hal_rfid_tim_read_pause();
        }
    }
    if(app_fsm->subghz_signal_mode == SubGhzSignalModeOok && app_fsm->subghz_ready) {
        const bool level = dcf77_subghz_ook_level(output);
        furi_hal_gpio_write(&gpio_cc1101_g0, level);
        app_fsm->subghz_output = level;
    }
}

static void dcf77_apply_output_force(AppFSM* app_fsm, bool output) {
    app_fsm->output_state = !output;
    dcf77_apply_output(app_fsm, output);
}

void dcf77_debug_sync_frame(AppFSM* app_fsm) {
    for(uint8_t i = 0; i < 5; i++) {
        dcf77_apply_output(app_fsm, true);
        dcf77_hw_indicator_set(app_fsm, true);
        furi_delay_ms(100);

        dcf77_apply_output(app_fsm, false);
        dcf77_hw_indicator_set(app_fsm, false);
        furi_delay_ms(100);
    }

    furi_delay_ms(500);
}

static void dcf77_get_following_minute(const AppFSM* app_fsm, DateTime* next_minute) {
    dcf77_experimental_time_get_frame_datetime(
        &app_fsm->experimental_time_settings,
        &app_fsm->experimental_time_runtime,
        app_fsm->experimental_time_runtime.frame_index + 1U,
        next_minute);
    next_minute->second = 0U;
}

static const RadioClockSecondWaveform* dcf77_scheduler_current_waveform(const AppFSM* app_fsm) {
    return &app_fsm->waveform_frame[app_fsm->scheduler_second];
}

static uint32_t dcf77_scheduler_segment_boundary_ticks(
    const RadioClockSecondWaveform* waveform,
    uint8_t segment_index) {
    uint32_t ticks = 0U;

    for(uint8_t i = 0; i <= segment_index && i < waveform->segment_count; i++) {
        ticks += waveform->segments[i].ticks;
    }

    return ticks;
}

static void dcf77_scheduler_apply_current_segment(AppFSM* app_fsm) {
    const RadioClockSecondWaveform* waveform = dcf77_scheduler_current_waveform(app_fsm);

    if(waveform->segment_count == 0U) {
        dcf77_apply_output(app_fsm, true);
        dcf77_scheduler_disable_compare();
        return;
    }

    dcf77_apply_output(app_fsm, waveform->segments[app_fsm->scheduler_segment].level);

    if((uint8_t)(app_fsm->scheduler_segment + 1U) >= waveform->segment_count) {
        dcf77_scheduler_disable_compare();
        return;
    }

    dcf77_scheduler_set_compare_ticks(
        dcf77_scheduler_segment_boundary_ticks(waveform, app_fsm->scheduler_segment));
}

static void dcf77_scheduler_advance_second(AppFSM* app_fsm) {
    app_fsm->scheduler_second++;
    if(app_fsm->scheduler_second >= DCF77_SECONDS_PER_MINUTE) {
        app_fsm->scheduler_second = 0;
        if(app_fsm->startup_marker_wrap_pending) {
            app_fsm->startup_marker_wrap_pending = false;
        } else if(app_fsm->next_minute_ready) {
            dcf77_logic_activate_next_minute(app_fsm);
            dcf77_experimental_time_advance_runtime(&app_fsm->experimental_time_runtime);
            app_fsm->tx_frame_enabled = app_fsm->next_tx_frame_enabled;
            app_fsm->next_minute_ready = false;
            app_fsm->next_minute_prepare_pending = true;
        } else {
            app_fsm->next_minute_prepare_pending = true;
        }
    }

    app_fsm->scheduler_segment = 0;
    app_fsm->bit_number = app_fsm->scheduler_second;
    app_fsm->current_pulse = app_fsm->pulse_frame[app_fsm->bit_number];
    app_fsm->bit_value = (uint8_t)app_fsm->current_pulse;
}

static void dcf77_scheduler_apply_current_second(AppFSM* app_fsm) {
    app_fsm->scheduler_segment = 0;
    dcf77_scheduler_apply_current_segment(app_fsm);
}

static void dcf77_timing_isr(void* context) {
    AppFSM* app_fsm = context;
    const bool compare_match = LL_LPTIM_IsActiveFlag_CMPM(DCF77_SCHEDULER);
    const bool autoreload_match = LL_LPTIM_IsActiveFlag_ARRM(DCF77_SCHEDULER);

    if(compare_match) {
        LL_LPTIM_ClearFlag_CMPM(DCF77_SCHEDULER);
    }
    if(autoreload_match) {
        LL_LPTIM_ClearFlag_ARRM(DCF77_SCHEDULER);
    }

    if(!app_fsm->scheduler_ready) {
        return;
    }

    if(compare_match) {
        const RadioClockSecondWaveform* waveform = dcf77_scheduler_current_waveform(app_fsm);

        if((uint8_t)(app_fsm->scheduler_segment + 1U) < waveform->segment_count) {
            app_fsm->scheduler_segment++;
            dcf77_scheduler_apply_current_segment(app_fsm);
        } else {
            dcf77_scheduler_disable_compare();
        }
    }

    if(autoreload_match) {
        dcf77_scheduler_advance_second(app_fsm);
        dcf77_scheduler_apply_current_second(app_fsm);
    }
}

static void dcf77_test_timer_callback(void* context) {
    AppFSM* app_fsm = context;

    app_fsm->test_phase++;
    if(app_fsm->test_phase >= DCF77_TEST_PERIOD_SLICES) {
        app_fsm->test_phase = 0;
    }

    dcf77_apply_output(app_fsm, app_fsm->test_phase == 0);
}

static void dcf77_scheduler_init(AppFSM* app_fsm) {
    furi_hal_bus_enable(FuriHalBusLPTIM2);
    LL_RCC_SetLPTIMClockSource(LL_RCC_LPTIM2_CLKSOURCE_LSE);
    LL_APB1_GRP2_EnableClockSleep(LL_APB1_GRP2_PERIPH_LPTIM2);
    furi_hal_interrupt_set_isr_ex(
        FuriHalInterruptIdLpTim2, FuriHalInterruptPriorityKamiSama, dcf77_timing_isr, app_fsm);
    dcf77_scheduler_reset();
}

bool dcf77_wait_for_next_second(DateTime* dt) {
    DateTime now;
    furi_hal_rtc_get_datetime(&now);
    const uint8_t start_second = now.second;

    for(uint16_t i = 0; i < 1500; i++) {
        furi_delay_ms(1);
        furi_hal_rtc_get_datetime(&now);
        if(now.second != start_second) {
            *dt = now;
            return true;
        }
    }

    *dt = now;
    return false;
}

void dcf77_timing_start(AppFSM* app_fsm) {
    if(app_fsm->current_signal == RadioClockSignalTest) {
        app_fsm->test_phase = 0;
        dcf77_apply_output_force(app_fsm, true);
        app_fsm->test_timer =
            furi_timer_alloc(dcf77_test_timer_callback, FuriTimerTypePeriodic, app_fsm);
        furi_timer_start(app_fsm->test_timer, furi_ms_to_ticks(DCF77_TEST_PULSE_MS));
        return;
    }

    dcf77_scheduler_init(app_fsm);
    dcf77_scheduler_prepare();
    dcf77_apply_output_force(app_fsm, true);
    dcf77_scheduler_apply_current_second(app_fsm);
    LL_LPTIM_StartCounter(DCF77_SCHEDULER, LL_LPTIM_OPERATING_MODE_CONTINUOUS);
}

void dcf77_timing_stop(AppFSM* app_fsm) {
    if(app_fsm->test_timer != NULL) {
        furi_timer_stop(app_fsm->test_timer);
        furi_timer_free(app_fsm->test_timer);
        app_fsm->test_timer = NULL;
        return;
    }

    app_fsm->scheduler_ready = false;
    dcf77_scheduler_reset();
    furi_hal_interrupt_set_isr(FuriHalInterruptIdLpTim2, NULL, NULL);
    furi_hal_bus_disable(FuriHalBusLPTIM2);
}

void dcf77_prepare_pending_minute(AppFSM* app_fsm) {
    DateTime next_minute;

    if(!app_fsm->tx_active || !app_fsm->next_minute_prepare_pending ||
       app_fsm->current_signal == RadioClockSignalTest) {
        return;
    }

    dcf77_get_following_minute(app_fsm, &next_minute);
    dcf77_logic_prepare_scratch_frame(app_fsm, &next_minute);

    NVIC_DisableIRQ(LPTIM2_IRQn);
    if(app_fsm->tx_active && app_fsm->next_minute_prepare_pending) {
        dcf77_logic_commit_scratch_as_next(app_fsm, &next_minute);
        app_fsm->next_tx_frame_enabled = dcf77_app_should_send_frame(
            app_fsm, app_fsm->experimental_time_runtime.frame_index + 1U);
        app_fsm->next_minute_ready = true;
        app_fsm->next_minute_prepare_pending = false;
    }
    NVIC_EnableIRQ(LPTIM2_IRQn);
}

void dcf77_hw_indicator_set(const AppFSM* app_fsm, bool status) {
    static bool last;
    static uint8_t last_red;
    static uint8_t last_green;
    static uint8_t last_blue;
    uint8_t red;
    uint8_t green;
    uint8_t blue;

    dcf77_hw_led_color_levels(app_fsm, &red, &green, &blue);

    if(last == status && last_red == red && last_green == green && last_blue == blue) {
        return;
    }

    if(status && (red != 0U || green != 0U || blue != 0U)) {
        furi_hal_light_set(LightRed, red);
        furi_hal_light_set(LightGreen, green);
        furi_hal_light_set(LightBlue, blue);
    } else {
        furi_hal_light_set(LightRed | LightGreen | LightBlue, 0);
    }

    last = status;
    last_red = red;
    last_green = green;
    last_blue = blue;
}

void dcf77_hw_indicator_blink_preview(const AppFSM* app_fsm) {
    uint8_t red;
    uint8_t green;
    uint8_t blue;

    dcf77_hw_led_color_levels(app_fsm, &red, &green, &blue);

    if(red == 0U && green == 0U && blue == 0U) {
        dcf77_hw_indicator_set(app_fsm, false);
        return;
    }

    for(uint8_t i = 0; i < 2U; i++) {
        dcf77_hw_indicator_set(app_fsm, true);
        furi_delay_ms(80);
        dcf77_hw_indicator_set(app_fsm, false);
        furi_delay_ms(80);
    }
}

void dcf77_output_gpio_init(const AppFSM* app_fsm) {
    const GpioPinRecord* record = dcf77_output_gpio_record(app_fsm);

    if(record == NULL) {
        return;
    }

    furi_hal_gpio_write(record->pin, false);
    furi_hal_gpio_init(record->pin, GpioModeOutputPushPull, GpioPullNo, GpioSpeedVeryHigh);
}

void dcf77_output_gpio_reconfigure(const AppFSM* app_fsm, uint8_t old_pin_number) {
    const GpioPinRecord* old_record = dcf77_gpio_pin_record(old_pin_number);

    if(old_record != NULL && old_pin_number != app_fsm->gpio_baseband_pin_number) {
        furi_hal_gpio_write(old_record->pin, false);
        furi_hal_gpio_init(old_record->pin, GpioModeAnalog, GpioPullNo, GpioSpeedVeryHigh);
    }

    dcf77_output_gpio_init(app_fsm);
    dcf77_output_gpio_set(app_fsm, app_fsm->output_state);
}

void dcf77_output_gpio_set(const AppFSM* app_fsm, bool status) {
    const GpioPinRecord* record = dcf77_output_gpio_record(app_fsm);

    if(record == NULL) {
        return;
    }

    furi_hal_gpio_write(record->pin, status);
}

void dcf77_output_gpio_deinit(const AppFSM* app_fsm) {
    const GpioPinRecord* record = dcf77_output_gpio_record(app_fsm);

    if(record == NULL) {
        return;
    }

    furi_hal_gpio_write(record->pin, false);
    furi_hal_gpio_init(record->pin, GpioModeAnalog, GpioPullNo, GpioSpeedVeryHigh);
}

void dcf77_lf_set_frequency(AppFSM* app_fsm, uint32_t freq) {
    if(app_fsm->lf_ready) {
        furi_hal_rfid_tim_read_pause();
        furi_hal_rfid_tim_read_stop();
        furi_hal_rfid_pins_reset();
    }

    app_fsm->lf_freq = freq;
    furi_hal_rfid_tim_read_start(app_fsm->lf_freq, 0.5f);
    app_fsm->lf_ready = true;

    if(app_fsm->output_state) {
        furi_hal_rfid_tim_read_continue();
    } else {
        furi_hal_rfid_tim_read_pause();
    }
}

void dcf77_lf_apply_output(const AppFSM* app_fsm) {
    if(!app_fsm->lf_ready) {
        return;
    }

    if(app_fsm->output_state) {
        furi_hal_rfid_tim_read_continue();
    } else {
        furi_hal_rfid_tim_read_pause();
    }
}

void dcf77_lf_deinit(AppFSM* app_fsm) {
    if(!app_fsm->lf_ready) {
        return;
    }

    furi_hal_rfid_tim_read_pause();
    furi_hal_rfid_tim_read_stop();
    furi_hal_rfid_pins_reset();
    app_fsm->lf_ready = false;
}

void dcf77_gpio_rf_apply_settings(AppFSM* app_fsm) {
    const GpioPinRecord* record = dcf77_gpio_rf_record(app_fsm);

    dcf77_gpio_rf_deinit(app_fsm);

    if(!app_fsm->tx_active || record == NULL || record->pwm_output == FuriHalPwmOutputIdNone) {
        return;
    }

    if(app_fsm->output_state) {
        furi_hal_pwm_start(record->pwm_output, app_fsm->lf_freq, app_fsm->gpio_rf_duty_cycle);
        app_fsm->gpio_rf_running = true;
    }
}

void dcf77_gpio_rf_sync_output(AppFSM* app_fsm) {
    const GpioPinRecord* record = dcf77_gpio_rf_record(app_fsm);

    if(record == NULL || record->pwm_output == FuriHalPwmOutputIdNone) {
        app_fsm->gpio_rf_running = false;
        return;
    }

    if(app_fsm->output_state) {
        if(furi_hal_pwm_is_running(record->pwm_output)) {
            furi_hal_pwm_set_params(
                record->pwm_output, app_fsm->lf_freq, app_fsm->gpio_rf_duty_cycle);
        } else {
            furi_hal_pwm_start(record->pwm_output, app_fsm->lf_freq, app_fsm->gpio_rf_duty_cycle);
        }
        app_fsm->gpio_rf_running = true;
    } else {
        if(furi_hal_pwm_is_running(record->pwm_output)) {
            furi_hal_pwm_stop(record->pwm_output);
        }
        app_fsm->gpio_rf_running = false;
    }
}

void dcf77_gpio_rf_deinit(AppFSM* app_fsm) {
    const GpioPinRecord* record = dcf77_gpio_rf_record(app_fsm);

    if(record != NULL && record->pwm_output != FuriHalPwmOutputIdNone &&
       furi_hal_pwm_is_running(record->pwm_output)) {
        furi_hal_pwm_stop(record->pwm_output);
    }

    app_fsm->gpio_rf_running = false;
}

void dcf77_subghz_apply_settings(AppFSM* app_fsm) {
    if(!app_fsm->tx_active) {
        return;
    }

    if(dcf77_app_gpio_rf_enabled(app_fsm)) {
        dcf77_subghz_deinit(app_fsm);
        return;
    }

    dcf77_subghz_deinit(app_fsm);
    if(!dcf77_app_subghz_runtime_enabled(app_fsm)) {
        return;
    }

    if(app_fsm->subghz_signal_mode == SubGhzSignalModeOok) {
        dcf77_subghz_init(app_fsm, subghz_device_cc1101_preset_ook_270khz_async_regs);
        furi_hal_gpio_init(&gpio_cc1101_g0, GpioModeOutputPushPull, GpioPullNo, GpioSpeedLow);
        if(!furi_hal_subghz_tx()) {
            dcf77_subghz_deinit(app_fsm);
            app_fsm->subghz_signal_mode = SubGhzSignalModeDisabled;
            return;
        }
        const bool level = dcf77_subghz_ook_level(app_fsm->output_state);
        furi_hal_gpio_write(&gpio_cc1101_g0, level);
        app_fsm->subghz_output = level;
        return;
    }

    dcf77_subghz_init(app_fsm, subghz_device_cc1101_preset_2fsk_dev2_38khz_async_regs);
    app_fsm->subghz_tone_phase = false;
    app_fsm->subghz_output = false;
    dcf77_subghz_sync_output(app_fsm);
}

void dcf77_subghz_sync_output(AppFSM* app_fsm) {
    if(!dcf77_app_subghz_runtime_enabled(app_fsm) ||
       (app_fsm->subghz_signal_mode != SubGhzSignalModeFsk &&
        app_fsm->subghz_signal_mode != SubGhzSignalModeFskFull) ||
       dcf77_app_gpio_rf_enabled(app_fsm) || !app_fsm->subghz_ready) {
        return;
    }

    if(app_fsm->subghz_signal_mode == SubGhzSignalModeFskFull) {
        if(app_fsm->output_state) {
            if(!app_fsm->subghz_async_tx) {
                app_fsm->subghz_tone_phase = false;
                app_fsm->subghz_output = false;
                app_fsm->subghz_async_tx =
                    furi_hal_subghz_start_async_tx(dcf77_subghz_fsk_yield, app_fsm);
                if(!app_fsm->subghz_async_tx) {
                    dcf77_subghz_deinit(app_fsm);
                    app_fsm->subghz_signal_mode = SubGhzSignalModeDisabled;
                }
            }
        } else {
            if(app_fsm->subghz_async_tx) {
                furi_hal_subghz_stop_async_tx();
            }
            app_fsm->subghz_async_tx = false;
            app_fsm->subghz_tone_phase = false;
            app_fsm->subghz_output = false;
            if(!dcf77_subghz_fsk_enter_static()) {
                dcf77_subghz_deinit(app_fsm);
                app_fsm->subghz_signal_mode = SubGhzSignalModeDisabled;
            }
        }
        return;
    }

    if(app_fsm->output_state) {
        if(!app_fsm->subghz_async_tx) {
            app_fsm->subghz_tone_phase = false;
            app_fsm->subghz_output = false;
            app_fsm->subghz_async_tx =
                furi_hal_subghz_start_async_tx(dcf77_subghz_fsk_yield, app_fsm);
            if(!app_fsm->subghz_async_tx) {
                dcf77_subghz_deinit(app_fsm);
                app_fsm->subghz_signal_mode = SubGhzSignalModeDisabled;
            }
        }
    } else {
        if(app_fsm->subghz_async_tx) {
            furi_hal_subghz_stop_async_tx();
        }
        app_fsm->subghz_async_tx = false;
        app_fsm->subghz_tone_phase = false;
        app_fsm->subghz_output = false;
        furi_hal_subghz_idle();
    }
}

void dcf77_subghz_deinit(AppFSM* app_fsm) {
    if(app_fsm->subghz_async_tx) {
        furi_hal_subghz_stop_async_tx();
    }
    app_fsm->subghz_async_tx = false;
    app_fsm->subghz_tone_phase = false;
    app_fsm->subghz_output = false;

    if(app_fsm->subghz_ready) {
        furi_hal_gpio_write(&gpio_cc1101_g0, false);
        furi_hal_gpio_init(&gpio_cc1101_g0, GpioModeInput, GpioPullNo, GpioSpeedLow);
        furi_hal_subghz_idle();
        furi_hal_subghz_sleep();
        app_fsm->subghz_ready = false;
    }
}
