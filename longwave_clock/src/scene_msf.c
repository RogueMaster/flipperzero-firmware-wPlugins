#include "scene_msf.h"
#include "app_state.h"
#include "furi_hal_rtc.h"
#include "longwave_clock_app.h"
#include "module_date.h"
#include "module_lights.h"
#include "module_rollbits.h"
#include "module_time.h"

#define TOP_BAR             0
#define TOP_TIME            30
#define TOP_DATE            53
#define DURATION_DIFF(x, y) (((x) < (y)) ? ((y) - (x)) : ((x) - (y)))

/** main menu events */

static bool lwc_msf_scene_input(InputEvent* input_event, void* context) {
    UNUSED(input_event);
    UNUSED(context);
    return false;
}

static void lwc_msf_scene_draw(Canvas* canvas, void* context) {
    LWCViewModel* model = context;

    canvas_clear(canvas);

    draw_decoded_bits(
        canvas,
        MSF,
        TOP_BAR,
        model->buffer,
        BUFFER,
        model->received_count,
        model->last_received,
        model->received_interrupt < MIN_INTERRUPT,
        model->decoding == DecodingUnknown);

    draw_decoded_time(
        canvas,
        TOP_TIME,
        model->decoding_time,
        model->hours_10s,
        model->hours_1s,
        model->minutes_10s,
        model->minutes_1s,
        model->seconds,
        model->timezone,
        model->seconds > 51);

    draw_decoded_date(
        canvas,
        TOP_DATE,
        model->decoding_date,
        20,
        model->year_10s,
        model->year_1s,
        model->month_10s,
        model->month_1s,
        model->day_of_month_10s,
        model->day_of_month_1s,
        model->day_of_week);
}

static void msf_add_gui_bit(LWCViewModel* model, Bit bit) {
    if(model->last_received == BUFFER - 1) {
        model->last_received = 0;
    } else {
        model->last_received++;
    }
    if(model->received_count < BUFFER) {
        model->received_count++;
    }

    model->buffer[model->last_received] = bit;
}

static void msf_process_time_bit(LWCViewModel* model) {
    DecodingTimePhase old_decoding_time = model->decoding_time;
    DecodingTimePhase new_decoding_time = msf_get_decoding_time_phase(model->minute_data);

    if(model->decoding_time != new_decoding_time) {
        switch(old_decoding_time) {
        case DecodingTimeHours10s:
            model->hours_10s = msf_decode_hours_10s(model->minute_data);
            break;
        case DecodingTimeHours1s:
            model->hours_1s = msf_decode_hours_1s(model->minute_data);
            break;
        case DecodingTimeMinutes10s:
            model->minutes_10s = msf_decode_minutes_10s(model->minute_data);
            break;
        case DecodingTimeMinutes1s:
            model->minutes_1s = msf_decode_minutes_1s(model->minute_data);
            break;
        case DecodingTimeTimezone:
            model->timezone = msf_decode_timezone(model->minute_data);
            break;
        default:
        }
        switch(new_decoding_time) {
        case DecodingTimeTimezone:
            msf_add_gui_bit(model, BitStartTimezone);
            break;
        case DecodingTimeMinutes10s:
            msf_add_gui_bit(model, BitStartMinute);
            break;
        case DecodingTimeMinutes1s:
            msf_add_gui_bit(model, BitStartEmpty);
            break;
        case DecodingTimeHours10s:
            msf_add_gui_bit(model, BitStartHour);
            break;
        case DecodingTimeHours1s:
            msf_add_gui_bit(model, BitStartEmpty);
            break;
        case DecodingTimeConstant:
            msf_add_gui_bit(model, BitConstant);
            break;
        case DecodingTimeChecksum:
            msf_add_gui_bit(model, BitStartEmpty);
            break;
        default:
        }

        model->decoding_time = new_decoding_time;
    } else if(new_decoding_time == DecodingTimeChecksum) {
        if(msf_get_time_checksum(model->minute_data) != 1) {
            model->minutes_1s = -1;
            model->minutes_10s = -1;
            model->hours_1s = -1;
            model->hours_10s = -1;
            msf_add_gui_bit(model, BitChecksumError);
        } else {
            msf_add_gui_bit(model, BitChecksum);
        }
    }
}

static void msf_process_date_bit(LWCViewModel* model, DecodingPhase current_phase) {
    DecodingDatePhase new_decoding_date;
    if(current_phase == DecodingDate) {
        new_decoding_date = msf_get_decoding_date_phase(model->minute_data);
    } else {
        new_decoding_date = DecodingNoDate;
    }

    if(model->decoding_date != new_decoding_date) {
        switch(new_decoding_date) {
        case DecodingDateDayOfMonth10s:
            model->month_1s = msf_decode_month_1s(model->minute_data);
            msf_add_gui_bit(model, BitStartDayOfMonth);
            break;
        case DecodingDateDayOfMonth1s:
            model->day_of_month_10s = msf_decode_day_of_month_10s(model->minute_data);
            msf_add_gui_bit(model, BitStartEmpty);
            break;
        case DecodingDateDayOfWeek:
            model->day_of_month_1s = msf_decode_day_of_month_1s(model->minute_data);
            msf_add_gui_bit(model, BitStartDayOfWeek);
            break;
        case DecodingDateMonth10s:
            model->year_1s = msf_decode_year_1s(model->minute_data);
            msf_add_gui_bit(model, BitStartMonth);
            break;
        case DecodingDateMonth1s:
            model->month_10s = msf_decode_month_10s(model->minute_data);
            msf_add_gui_bit(model, BitStartEmpty);
            break;
        case DecodingDateYear10s:
            msf_add_gui_bit(model, BitStartYear);
            break;
        case DecodingDateYear1s:
            model->year_10s = msf_decode_year_10s(model->minute_data);
            msf_add_gui_bit(model, BitStartEmpty);
            break;
        case DecodingDateYearChecksum:
            msf_add_gui_bit(model, BitStartEmpty);
            break;
        case DecodingDateInYearChecksum:
            msf_add_gui_bit(model, BitStartEmpty);
            break;
        case DecodingDateDayOfWeekChecksum:
            msf_add_gui_bit(model, BitStartEmpty);
            break;
        default:
        }

        model->decoding_date = new_decoding_date;
    } else {
        switch(new_decoding_date) {
        case DecodingDateYearChecksum:
            if(msf_get_year_checksum(model->minute_data) != 1) {
                model->year_10s = -1;
                model->year_1s = -1;
                msf_add_gui_bit(model, BitChecksumError);
            } else {
                msf_add_gui_bit(model, BitChecksum);
            }
            break;
        case DecodingDateInYearChecksum:
            if(msf_get_inyear_checksum(model->minute_data) != 1) {
                model->month_10s = -1;
                model->month_1s = -1;
                model->day_of_month_1s = -1;
                model->day_of_month_10s = -1;
                msf_add_gui_bit(model, BitChecksumError);
            } else {
                msf_add_gui_bit(model, BitChecksum);
            }
            break;
        case DecodingDateDayOfWeekChecksum:
            if(msf_get_dow_checksum(model->minute_data) != 1) {
                model->day_of_week = -1;
                msf_add_gui_bit(model, BitChecksumError);
            } else {
                msf_add_gui_bit(model, BitChecksum);
            }
            break;
        default:
        }
    }
}

static void msf_process_finish_time_phase(LWCViewModel* model, DecodingTimePhase last_time_phase) {
    UNUSED(last_time_phase);
    model->decoding_time = DecodingNoTime;
}

static void msf_process_finish_date_phase(LWCViewModel* model, DecodingDatePhase last_date_phase) {
    if(last_date_phase == DecodingDateDayOfWeek) {
        model->day_of_week = msf_decode_day_of_week(model->minute_data);
    }
    model->decoding_date = DecodingNoDate;
}

static void msf_receive_bit(void* context, bool is_b, int8_t received_bit) {
    View* view = context;

    FURI_LOG_D(TAG, "Received a %d", received_bit);

    with_view_model(
        view,
        LWCViewModel * model,
        {
            model->received_interrupt = MIN_INTERRUPT;
            minute_data_add_bit(model->minute_data, received_bit);

            if(!is_b) {
                int8_t seconds = minute_data_get_length(model->minute_data) / 2;
                if(seconds > 0) {
                    model->seconds = seconds;
                }
            }

            DecodingPhase old_phase = model->decoding;

            model->decoding = msf_get_decoding_phase(model->minute_data);
            bool change_phase = model->decoding != old_phase;

            if(change_phase) {
                switch(old_phase) {
                case DecodingTime:
                    msf_process_finish_time_phase(model, model->decoding_time);
                    break;
                case DecodingDate:
                    msf_process_finish_date_phase(model, model->decoding_date);
                    break;
                case DecodingUnknown:
                    model->decoding_time = DecodingNoTime;
                    model->decoding_date = DecodingNoDate;
                    break;
                case DecodingMeta:
                    model->decoding_time = DecodingNoTime;
                    model->decoding_date = DecodingNoDate;
                    msf_add_gui_bit(model, BitConstant);
                    break;
                default:
                    break;
                }
                switch(model->decoding) {
                case DecodingMeta:
                    msf_add_gui_bit(model, BitConstant);
                    break;
                case DecodingDUT:
                    model->decoding_time = DecodingNoTime;
                    model->decoding_date = DecodingNoDate;
                    msf_add_gui_bit(model, BitStartDUT);
                    break;
                default:
                    break;
                }
            }
            switch(model->decoding) {
            case DecodingTime:
                msf_process_time_bit(model);
                break;
            case DecodingDate:
                msf_process_date_bit(model, model->decoding);
                break;
            default:
                break;
            }
            msf_add_gui_bit(model, (Bit)received_bit % 2);
        },
        true);
}

static void msf_receive_start_of_minute(void* context) {
    View* view = context;

    FURI_LOG_I(TAG, "Found the MSF synchronization, starting a new minute!");

    with_view_model(
        view,
        LWCViewModel * model,
        {
            model->received_interrupt = MIN_INTERRUPT;
            msf_add_gui_bit(model, BitEndMinute);
            minute_data_start_minute(model->minute_data);
            minute_data_add_bit(model->minute_data, 1);
            msf_add_gui_bit(model, BitOne);
            minute_data_add_bit(model->minute_data, 1);
            msf_add_gui_bit(model, BitOne);
            model->seconds = 0;
        },
        true);
}

static void msf_receive_desync(void* context) {
    View* view = context;

    FURI_LOG_I(TAG, "Got a DESYNC error, resetting all!");

    with_view_model(
        view,
        LWCViewModel * model,
        {
            model->decoding = DecodingUnknown;
            model->decoding_time = DecodingNoTime;
            model->decoding_date = DecodingNoDate;
            msf_add_gui_bit(model, BitEndSync);
        },
        true);
}

static void msf_receive_unknown(void* context) {
    View* view = context;

    FURI_LOG_I(TAG, "Not received any bit, assuming a missing receipt!");

    with_view_model(
        view,
        LWCViewModel * model,
        {
            minute_data_add_bit(model->minute_data, -1);
            msf_add_gui_bit(model, BitUnknown);
        },
        true);
}

static void msf_receive_interrupt(void* context) {
    View* view = context;

    with_view_model(view, LWCViewModel * model, { model->received_interrupt++; }, true);
}

static void msf_receive_gpio(GPIOEvent event, void* context) {
    App* app = context;

    if(!event.shift_up) {
        if(event.time_passed_down > 400) { // Decode A after the second marker
            if(DURATION_DIFF(event.time_passed_up, 100) < 50) { // A0
                scene_manager_handle_custom_event(app->scene_manager, LCWMSFEventReceiveA0);
            } else if(DURATION_DIFF(event.time_passed_up, 250) < 100) { //A1B0 or A1B1
                scene_manager_handle_custom_event(app->scene_manager, LCWMSFEventReceiveA1);
            }
        } else {
            scene_manager_handle_custom_event(app->scene_manager, LCWMSFEventReceiveInterrupt);
        }
    } else if(event.time_passed_down > 750) {
        scene_manager_handle_custom_event(app->scene_manager, LCWMSFEventReceiveB0);
    } else if(
        DURATION_DIFF(event.time_passed_down, 500) < 50 &&
        DURATION_DIFF(event.time_passed_up, 500) < 50) {
        scene_manager_handle_custom_event(app->scene_manager, LCWMSFEventReceiveSync);
    } else if(DURATION_DIFF(event.time_passed_down, 600) < 150) {
        scene_manager_handle_custom_event(app->scene_manager, LCWMSFEventReceiveB1);
    } else {
        scene_manager_handle_custom_event(app->scene_manager, LCWMSFEventReceiveInterrupt);
        FURI_LOG_E(
            TAG,
            "DOWN up for %lu, last down for %lu",
            event.time_passed_up,
            event.time_passed_down);
    }
}

bool lwc_msf_scene_on_event(void* context, SceneManagerEvent event) {
    App* app = context;
    bool consumed = false;

    switch(event.type) {
    case SceneManagerEventTypeCustom:
        LWCMSFEvent msf_event = event.event;

        switch(msf_event) {
        case LCWMSFEventReceiveA0:
            lwc_app_led_on_receive_clear(app);
            msf_receive_bit(app->msf_view, false, 0);
            consumed = true;
            break;
        case LCWMSFEventReceiveA1:
            lwc_app_led_on_receive_clear(app);
            msf_receive_bit(app->msf_view, false, 1);
            consumed = true;
            break;
        case LCWMSFEventReceiveB0:
            lwc_app_led_on_receive_clear(app);
            msf_receive_bit(app->msf_view, true, 0);
            consumed = true;
            break;
        case LCWMSFEventReceiveB1:
            lwc_app_led_on_receive_clear(app);
            msf_receive_bit(app->msf_view, true, 1);
            consumed = true;
            break;
        case LCWMSFEventReceiveSync:
            lwc_app_led_on_sync(app);
            msf_receive_start_of_minute(app->msf_view);
            consumed = true;
            break;
        case LCWMSFEventReceiveDesync:
            lwc_app_led_on_desync(app);
            msf_receive_desync(app->msf_view);
            consumed = true;
            break;
        case LCWMSFEventReceiveUnknown:
            msf_receive_unknown(app->msf_view);
            lwc_app_led_on_receive_unknown(app);
            consumed = true;
            break;
        case LCWMSFEventReceiveInterrupt:
            msf_receive_interrupt(app->msf_view);
            consumed = true;
            break;
        default:
        }
        break;
    case SceneManagerEventTypeTick:
        if(app->state->gpio != NULL) {
            for(int i = 10; i > 0 && gpio_callback_with_event(app->state->gpio, msf_receive_gpio);
                i--) {
            }
        }
        consumed = true;
        break;
    default:
        consumed = false;
        break;
    }
    return consumed;
}

static void msf_refresh_simulated_data(MinuteData* simulation_data, DateTime datetime) {
    FURI_LOG_I(TAG, "Setting simulated data for minute %d", datetime.minute);

    msf_set_simulated_minute_data(
        simulation_data,
        datetime.second,
        datetime.minute,
        datetime.hour,
        datetime.weekday,
        datetime.day,
        datetime.month,
        datetime.year % 100);
}

static void msf_500ms_callback(void* context) {
    App* app = context;

    if(lwc_get_protocol_config(app->state)->run_mode == Demo) {
        MinuteData* simulation = app->state->simulation_data;
        DateTime now;
        furi_hal_rtc_get_datetime(&now);

        if(now.second < MINUTE && simulation->index != now.second * 2) {
            simulation->index += 2;
            if(now.second == 0) {
                msf_refresh_simulated_data(app->state->simulation_data, now);
                scene_manager_handle_custom_event(app->scene_manager, LCWMSFEventReceiveSync);
            } else {
                if(minute_data_get_bit(app->state->simulation_data, now.second * 2) == 0) {
                    scene_manager_handle_custom_event(app->scene_manager, LCWMSFEventReceiveA0);
                } else {
                    scene_manager_handle_custom_event(app->scene_manager, LCWMSFEventReceiveA1);
                }
                if(minute_data_get_bit(app->state->simulation_data, now.second * 2 + 1) == 0) {
                    scene_manager_handle_custom_event(app->scene_manager, LCWMSFEventReceiveB0);
                } else {
                    scene_manager_handle_custom_event(app->scene_manager, LCWMSFEventReceiveB1);
                }
            }
        }
    }

    MinuteDataError result;

    with_view_model(
        app->msf_view,
        LWCViewModel * model,
        { result = minute_data_500ms_passed(model->minute_data); },
        false);
    switch(result) {
    case MinuteDataErrorNone:
        break;
    case MinuteDataErrorUnknownBit:
        scene_manager_handle_custom_event(app->scene_manager, LCWMSFEventReceiveUnknown);
        break;
    case MinuteDataErrorDesync:
        scene_manager_handle_custom_event(app->scene_manager, LCWMSFEventReceiveDesync);
        break;
    }
}

void lwc_msf_scene_on_enter(void* context) {
    App* app = context;
    ProtoConfig* config = lwc_get_protocol_config(app->state);

    with_view_model(
        app->msf_view,
        LWCViewModel * model,
        {
            model->decoding = DecodingUnknown;
            model->decoding_time = DecodingNoTime;
            model->decoding_date = DecodingNoDate;
            model->year_10s = -1;
            model->year_1s = -1;
            model->month_10s = -1;
            model->month_1s = -1;
            model->day_of_month_1s = -1;
            model->day_of_month_10s = -1;
            model->day_of_week = -1;
            model->hours_10s = -1;
            model->hours_1s = -1;
            model->minutes_10s = -1;
            model->minutes_1s = -1;
            model->seconds = -1;
            model->timezone = UnknownTimezone;
            model->last_received = BUFFER - 1;
            model->received_interrupt = config->run_mode == Demo ? MIN_INTERRUPT : 0;
            model->received_count = 0;
            minute_data_reset(model->minute_data);
        },
        true);
    switch(config->run_mode) {
    case Demo:
        app->state->simulation_data = minute_data_alloc(120);
        DateTime now;
        furi_hal_rtc_get_datetime(&now);
        msf_refresh_simulated_data(app->state->simulation_data, now);
        break;
    case GPIO:
        app->state->gpio =
            gpio_start_listening(config->data_pin, config->data_mode == Inverted, app);
        break;
    default:
        break;
    }

    app->state->seconds_timer = furi_timer_alloc(msf_500ms_callback, FuriTimerTypePeriodic, app);
    furi_timer_start(app->state->seconds_timer, furi_kernel_get_tick_frequency() / 2);

    view_dispatcher_switch_to_view(app->view_dispatcher, LWCMSFView);
}

void lwc_msf_scene_on_exit(void* context) {
    App* app = context;

    notification_message_block(app->notifications, &sequence_display_backlight_enforce_auto);
    furi_timer_stop(app->state->seconds_timer);
    furi_timer_free(app->state->seconds_timer);

    switch(lwc_get_protocol_config(app->state)->run_mode) {
    case Demo:
        minute_data_free(app->state->simulation_data);
        break;
    case GPIO:
        gpio_stop_listening(app->state->gpio);
        break;
    default:
        break;
    }
}

View* lwc_msf_scene_alloc() {
    View* view = view_alloc();

    view_allocate_model(view, ViewModelTypeLocking, sizeof(LWCViewModel));
    with_view_model(
        view, LWCViewModel * model, { model->minute_data = minute_data_alloc(120); }, true);
    view_set_context(view, view);
    view_set_input_callback(view, lwc_msf_scene_input);
    view_set_draw_callback(view, lwc_msf_scene_draw);

    return view;
}

void lwc_msf_scene_free(View* view) {
    furi_assert(view);
    with_view_model(view, LWCViewModel * model, { minute_data_free(model->minute_data); }, false);

    view_free(view);
}
