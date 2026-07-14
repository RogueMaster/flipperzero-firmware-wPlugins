#include "scene_dcf77.h"
#include "app_state.h"
#include "furi_hal_rtc.h"
#include "longwave_clock_app.h"
#include "module_date.h"
#include "module_lights.h"
#include "module_rollbits.h"
#include "module_time.h"
#include "scenes.h"

#define TOP_BAR             0
#define TOP_TIME            30
#define TOP_DATE            53
#define DURATION_DIFF(x, y) (((x) < (y)) ? ((y) - (x)) : ((x) - (y)))

/** main menu events */

static bool lwc_dcf77_scene_input(InputEvent* input_event, void* context) {
    UNUSED(input_event);
    UNUSED(context);
    return false;
}

static void lwc_dcf77_scene_draw(Canvas* canvas, void* context) {
    LWCViewModel* model = context;

    canvas_clear(canvas);

    draw_decoded_bits(
        canvas,
        DCF77,
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
        model->seconds > 23);

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

static void dcf77_add_gui_bit(LWCViewModel* model, Bit bit) {
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

static void dcf77_process_time_bit(LWCViewModel* model, DecodingPhase current_phase) {
    DecodingTimePhase new_decoding_time;
    if(current_phase == DecodingTime) {
        new_decoding_time = dcf77_get_decoding_time_phase(model->minute_data);
    } else {
        new_decoding_time = DecodingNoTime;
    }

    if(model->decoding_time != new_decoding_time) {
        switch(new_decoding_time) {
        case DecodingTimeTimezone:
            dcf77_add_gui_bit(model, BitStartTimezone);
            break;
        case DecodingTimeMinutes1s:
            dcf77_add_gui_bit(model, BitStartMinute);
            break;
        case DecodingTimeMinutes10s:
            dcf77_add_gui_bit(model, BitStartEmpty);
            break;
        case DecodingTimeMinutes:
            if(dcf77_get_minutes_checksum(model->minute_data) != 0) {
                model->minutes_1s = -1;
                model->minutes_10s = -1;
                dcf77_add_gui_bit(model, BitChecksumError);
            } else {
                model->minutes_10s = dcf77_decode_minutes_10s(model->minute_data);
                dcf77_add_gui_bit(model, BitChecksum);
            }
            break;
        case DecodingTimeHours1s:
            dcf77_add_gui_bit(model, BitStartHour);
            break;
        case DecodingTimeHours10s:
            dcf77_add_gui_bit(model, BitStartEmpty);
            break;
        case DecodingTimeHours:
            if(dcf77_get_hours_checksum(model->minute_data) != 0) {
                model->hours_1s = -1;
                model->hours_10s = -1;
                dcf77_add_gui_bit(model, BitChecksumError);
            } else {
                model->hours_10s = dcf77_decode_hours_10s(model->minute_data);
                dcf77_add_gui_bit(model, BitChecksum);
            }
            break;
        case DecodingTimeConstant:
            dcf77_add_gui_bit(model, BitConstant);
            break;
        case DecodingTimeSeconds:
            dcf77_add_gui_bit(model, BitConstant);
            break;
        default:
        }
        switch(model->decoding_time) {
        case DecodingTimeTimezone:
            model->timezone = dcf77_decode_timezone(model->minute_data);
            break;
        case DecodingTimeMinutes1s:
            model->minutes_1s = dcf77_decode_minutes_1s(model->minute_data);
            break;
        case DecodingTimeHours1s:
            model->hours_1s = dcf77_decode_hours_1s(model->minute_data);
            break;
        default:
        }
        model->decoding_time = new_decoding_time;
    }
}

static void dcf77_process_date_bit(LWCViewModel* model, DecodingPhase current_phase) {
    DecodingDatePhase new_decoding_date;
    if(current_phase == DecodingDate) {
        new_decoding_date = dcf77_get_decoding_date_phase(model->minute_data);
    } else {
        new_decoding_date = DecodingNoDate;
    }

    if(model->decoding_date != new_decoding_date) {
        switch(new_decoding_date) {
        case DecodingDateDayOfMonth1s:
            dcf77_add_gui_bit(model, BitStartDayOfMonth);
            break;
        case DecodingDateDayOfMonth10s:
            dcf77_add_gui_bit(model, BitStartEmpty);
            break;
        case DecodingDateDayOfWeek:
            dcf77_add_gui_bit(model, BitStartDayOfWeek);
            break;
        case DecodingDateMonth1s:
            dcf77_add_gui_bit(model, BitStartMonth);
            break;
        case DecodingDateMonth10s:
            dcf77_add_gui_bit(model, BitStartEmpty);
            break;
        case DecodingDateYear1s:
            dcf77_add_gui_bit(model, BitStartYear);
            break;
        case DecodingDateYear10s:
            dcf77_add_gui_bit(model, BitStartEmpty);
            break;
        case DecodingDateDate:
            if(dcf77_get_date_checksum(model->minute_data) != 0) {
                model->year_10s = -1;
                model->year_1s = -1;
                model->month_10s = -1;
                model->month_1s = -1;
                model->day_of_month_1s = -1;
                model->day_of_month_10s = -1;
                model->day_of_week = -1;
                dcf77_add_gui_bit(model, BitChecksumError);
            } else {
                model->year_10s = dcf77_decode_year_10s(model->minute_data);
                dcf77_add_gui_bit(model, BitChecksum);
            }
            break;
        default:
        }
        switch(model->decoding_date) {
        case DecodingDateDayOfMonth1s:
            model->day_of_month_1s = dcf77_decode_day_of_month_1s(model->minute_data);
            break;
        case DecodingDateDayOfMonth10s:
            model->day_of_month_10s = dcf77_decode_day_of_month_10s(model->minute_data);
            break;
        case DecodingDateDayOfWeek:
            model->day_of_week = dcf77_decode_day_of_week(model->minute_data);
            break;
        case DecodingDateMonth1s:
            model->month_1s = dcf77_decode_month_1s(model->minute_data);
            break;
        case DecodingDateMonth10s:
            model->month_10s = dcf77_decode_month_10s(model->minute_data);
            break;
        case DecodingDateYear1s:
            model->year_1s = dcf77_decode_year_1s(model->minute_data);
            break;
        default:
        }

        model->decoding_date = new_decoding_date;
    }
}

static void dcf77_receive_bit(void* context, int8_t received_bit) {
    View* view = context;

    FURI_LOG_D(TAG, "Received a %d", received_bit);

    with_view_model(
        view,
        LWCViewModel * model,
        {
            model->received_interrupt = MIN_INTERRUPT;
            minute_data_add_bit(model->minute_data, received_bit);
            int8_t seconds = minute_data_get_length(model->minute_data);
            if(seconds > 0) {
                model->seconds = seconds - 1;
            }

            DecodingPhase old_phase = model->decoding;

            model->decoding = dcf77_get_decoding_phase(model->minute_data);
            bool change_phase = model->decoding != old_phase;
            switch(model->decoding) {
            case DecodingUnknown:
                if(change_phase) {
                    model->decoding_time = DecodingNoTime;
                    model->decoding_date = DecodingNoDate;
                }
                break;
            case DecodingMeta:
                if(change_phase) {
                    model->decoding_time = DecodingNoTime;
                    model->decoding_date = DecodingNoDate;
                }
                dcf77_add_gui_bit(model, BitConstant);
                break;
            case DecodingWeather:
                if(change_phase) {
                    model->decoding_time = DecodingNoTime;
                    model->decoding_date = DecodingNoDate;
                    dcf77_add_gui_bit(model, BitStartWeather);
                }
                break;
            case DecodingTime:
                dcf77_process_time_bit(model, model->decoding);
                break;
            case DecodingDate:
                if(change_phase) {
                    model->decoding_time = DecodingNoTime;
                }

                dcf77_process_date_bit(model, model->decoding);
                break;
            default:
                break;
            }
            dcf77_add_gui_bit(model, (Bit)received_bit);
        },
        true);
}

static void dcf77_receive_end_of_minute(void* context) {
    View* view = context;

    FURI_LOG_I(TAG, "Found the DCF77 synchronization, starting a new minute!");

    with_view_model(
        view,
        LWCViewModel * model,
        {
            dcf77_add_gui_bit(model, BitEndMinute);
            minute_data_start_minute(model->minute_data);
            model->decoding = DecodingTime;
            model->decoding_time = DecodingTimeSeconds;
        },
        true);
}

static void dcf77_receive_desync(void* context) {
    View* view = context;

    FURI_LOG_I(TAG, "Got a DESYNC error, resetting all!");

    with_view_model(
        view,
        LWCViewModel * model,
        {
            model->decoding = DecodingUnknown;
            model->decoding_time = DecodingNoTime;
            model->decoding_date = DecodingNoDate;
            dcf77_add_gui_bit(model, BitEndSync);
        },
        true);
}

static void dcf77_receive_unknown(void* context) {
    View* view = context;

    FURI_LOG_I(TAG, "Not received any bit, assuming a missing receipt!");

    with_view_model(
        view,
        LWCViewModel * model,
        {
            minute_data_add_bit(model->minute_data, -1);
            dcf77_add_gui_bit(model, BitUnknown);
        },
        true);
}

static void dcf77_receive_interrupt(void* context) {
    View* view = context;

    with_view_model(view, LWCViewModel * model, { model->received_interrupt++; }, true);
}

static void dcf77_receive_gpio(GPIOEvent event, void* context) {
    App* app = context;

    if(event.shift_up) {
        if(DURATION_DIFF(event.time_passed_down, 1850) < 100) {
            scene_manager_handle_custom_event(app->scene_manager, LCWDCF77EventReceiveSync);
        } else {
            scene_manager_handle_custom_event(app->scene_manager, LCWDCF77EventReceiveInterrupt);
        }
    } else if(event.time_passed_down > 400) {
        if(DURATION_DIFF(event.time_passed_up, 200) < 50) {
            scene_manager_handle_custom_event(app->scene_manager, LCWDCF77EventReceive1);
        } else if(DURATION_DIFF(event.time_passed_up, 100) < 50) {
            scene_manager_handle_custom_event(app->scene_manager, LCWDCF77EventReceive0);
        }
    } else {
        scene_manager_handle_custom_event(app->scene_manager, LCWDCF77EventReceiveInterrupt);
    }
}

bool lwc_dcf77_scene_on_event(void* context, SceneManagerEvent event) {
    App* app = context;
    bool consumed = false;

    switch(event.type) {
    case SceneManagerEventTypeCustom:
        LWCDCF77Event dcf77_event = event.event;

        switch(dcf77_event) {
        case LCWDCF77EventReceive0:
            lwc_app_led_on_receive_clear(app);
            dcf77_receive_bit(app->dcf77_view, 0);
            consumed = true;
            break;
        case LCWDCF77EventReceive1:
            lwc_app_led_on_receive_clear(app);
            dcf77_receive_bit(app->dcf77_view, 1);
            consumed = true;
            break;
        case LCWDCF77EventReceiveSync:
            lwc_app_led_on_sync(app);
            dcf77_receive_end_of_minute(app->dcf77_view);
            consumed = true;
            break;
        case LCWDCF77EventReceiveDesync:
            lwc_app_led_on_desync(app);
            dcf77_receive_desync(app->dcf77_view);
            consumed = true;
            break;
        case LCWDCF77EventReceiveUnknown:
            dcf77_receive_unknown(app->dcf77_view);
            lwc_app_led_on_receive_unknown(app);
            consumed = true;
            break;
        case LCWDCF77EventReceiveInterrupt:
            dcf77_receive_interrupt(app->dcf77_view);
            consumed = true;
            break;
        default:
        }
        break;
    case SceneManagerEventTypeTick:
        if(app->state->gpio != NULL) {
            for(int i = 10;
                i > 0 && gpio_callback_with_event(app->state->gpio, dcf77_receive_gpio);
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

static void dcf77_refresh_simulated_data(MinuteData* simulation_data, DateTime datetime) {
    FURI_LOG_I(TAG, "Setting simulated data for minute %d", datetime.minute);

    dcf77_set_simulated_minute_data(
        simulation_data,
        datetime.second,
        datetime.minute,
        datetime.hour,
        datetime.weekday,
        datetime.day,
        datetime.month,
        datetime.year % 100);
}

static void dcf77_500ms_callback(void* context) {
    App* app = context;

    if(lwc_get_protocol_config(app->state)->run_mode == Demo) {
        MinuteData* simulation = app->state->simulation_data;
        DateTime now;
        furi_hal_rtc_get_datetime(&now);

        if(now.second < MINUTE - 1 && simulation->index != now.second) {
            simulation->index++;
            if(now.second == 0) {
                dcf77_refresh_simulated_data(app->state->simulation_data, now);
                scene_manager_handle_custom_event(app->scene_manager, LCWDCF77EventReceiveSync);
            }

            if(minute_data_get_bit(app->state->simulation_data, now.second) == 0) {
                scene_manager_handle_custom_event(app->scene_manager, LCWDCF77EventReceive0);
            } else {
                scene_manager_handle_custom_event(app->scene_manager, LCWDCF77EventReceive1);
            }
        }
    }

    MinuteDataError result;

    with_view_model(
        app->dcf77_view,
        LWCViewModel * model,
        { result = minute_data_500ms_passed(model->minute_data); },
        false);
    switch(result) {
    case MinuteDataErrorNone:
        break;
    case MinuteDataErrorUnknownBit:
        scene_manager_handle_custom_event(app->scene_manager, LCWDCF77EventReceiveUnknown);
        break;
    case MinuteDataErrorDesync:
        scene_manager_handle_custom_event(app->scene_manager, LCWDCF77EventReceiveDesync);
        break;
    }
}

void lwc_dcf77_scene_on_enter(void* context) {
    App* app = context;
    ProtoConfig* config = lwc_get_protocol_config(app->state);

    with_view_model(
        app->dcf77_view,
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
        app->state->simulation_data = minute_data_alloc(60);
        DateTime now;
        furi_hal_rtc_get_datetime(&now);
        dcf77_refresh_simulated_data(app->state->simulation_data, now);
        break;
    case GPIO:
        app->state->gpio =
            gpio_start_listening(config->data_pin, config->data_mode == Inverted, app);
        break;
    default:
        break;
    }

    app->state->seconds_timer = furi_timer_alloc(dcf77_500ms_callback, FuriTimerTypePeriodic, app);
    furi_timer_start(app->state->seconds_timer, furi_kernel_get_tick_frequency() / 2);

    view_dispatcher_switch_to_view(app->view_dispatcher, LWCDCF77View);
}

void lwc_dcf77_scene_on_exit(void* context) {
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

View* lwc_dcf77_scene_alloc() {
    View* view = view_alloc();

    view_allocate_model(view, ViewModelTypeLocking, sizeof(LWCViewModel));
    with_view_model(
        view, LWCViewModel * model, { model->minute_data = minute_data_alloc(60); }, true);
    view_set_context(view, view);
    view_set_input_callback(view, lwc_dcf77_scene_input);
    view_set_draw_callback(view, lwc_dcf77_scene_draw);

    return view;
}

void lwc_dcf77_scene_free(View* view) {
    furi_assert(view);
    with_view_model(view, LWCViewModel * model, { minute_data_free(model->minute_data); }, false);

    view_free(view);
}
