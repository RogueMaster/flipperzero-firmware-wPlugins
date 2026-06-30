#include "dcf77_logic.h"
#include "dcf77_app.h"
#include "radio_clock_protocol.h"

#include <string.h>

const char* dcf77_logic_get_pulse_label(RadioClockPulse pulse) {
    return radio_clock_protocol_pulse_label(pulse);
}

static RadioClockMinuteFrame dcf77_protocol_frame_scratch;

static void dcf77_logic_set_current_pulse(AppFSM* app_fsm, uint8_t second) {
    app_fsm->bit_number = second;
    app_fsm->current_pulse = app_fsm->pulse_frame[second];
    app_fsm->bit_value = (uint8_t)app_fsm->current_pulse;
}

static void dcf77_logic_get_protocol_datetime(
    RadioClockSignal signal,
    const DateTime* dt,
    DateTime* protocol_dt) {
    *protocol_dt = *dt;

    if(radio_clock_protocol_uses_following_minute(signal)) {
        const uint32_t protocol_timestamp = datetime_datetime_to_timestamp(protocol_dt) + 60U;
        datetime_timestamp_to_datetime(protocol_timestamp, protocol_dt);
    }
}

static void dcf77_logic_set_tx_fields(AppFSM* app_fsm, const DateTime* dt) {
    app_fsm->tx_hour = dt->hour;
    app_fsm->tx_minute = dt->minute;
    app_fsm->tx_day = dt->day;
    app_fsm->tx_month = dt->month;
    app_fsm->tx_year = dt->year % 100;
    app_fsm->tx_dow = dt->weekday;
    app_fsm->tx_day_of_year = radio_clock_protocol_day_of_year(dt->year, dt->month, dt->day);
}

static void
    dcf77_logic_build_protocol_time(const DateTime* dt, RadioClockProtocolTime* protocol_time) {
    protocol_time->year = dt->year;
    protocol_time->day_of_year = radio_clock_protocol_day_of_year(dt->year, dt->month, dt->day);
    protocol_time->month = dt->month;
    protocol_time->day = dt->day;
    protocol_time->weekday = dt->weekday;
    protocol_time->hour = dt->hour;
    protocol_time->minute = dt->minute;
}

static void dcf77_logic_prepare_protocol_frame(
    AppFSM* app_fsm,
    const DateTime* protocol_dt,
    RadioClockMinuteFrame* frame) {
    RadioClockProtocolTime protocol_time;

    dcf77_logic_build_protocol_time(protocol_dt, &protocol_time);
    radio_clock_protocol_prepare_frame(app_fsm->current_signal, frame, &protocol_time);
}

static void dcf77_logic_copy_frame_buffers(
    const RadioClockMinuteFrame* frame,
    uint8_t* message_out,
    RadioClockPulse* pulses_out,
    RadioClockSecondWaveform* waveforms_out) {
    memcpy(message_out, frame->encoded, sizeof(frame->encoded));
    memcpy(pulses_out, frame->pulses, sizeof(frame->pulses));
    memcpy(waveforms_out, frame->waveforms, sizeof(frame->waveforms));
}

static void dcf77_logic_prepare_frame_buffers(
    AppFSM* app_fsm,
    const DateTime* dt,
    uint8_t* message_out,
    RadioClockPulse* pulses_out,
    RadioClockSecondWaveform* waveforms_out,
    DateTime* protocol_dt_out) {
    DateTime protocol_dt;

    dcf77_logic_get_protocol_datetime(app_fsm->current_signal, dt, &protocol_dt);
    dcf77_logic_prepare_protocol_frame(app_fsm, &protocol_dt, &dcf77_protocol_frame_scratch);
    dcf77_logic_copy_frame_buffers(
        &dcf77_protocol_frame_scratch, message_out, pulses_out, waveforms_out);
    if(protocol_dt_out != NULL) {
        *protocol_dt_out = protocol_dt;
    }
}

void dcf77_logic_prepare_scratch_frame(AppFSM* app_fsm, const DateTime* dt) {
    DateTime protocol_dt;

    dcf77_logic_get_protocol_datetime(app_fsm->current_signal, dt, &protocol_dt);
    dcf77_logic_prepare_protocol_frame(app_fsm, &protocol_dt, &dcf77_protocol_frame_scratch);
}

void dcf77_logic_commit_scratch_as_next(AppFSM* app_fsm, const DateTime* dt) {
    app_fsm->next_minute_dt = *dt;
    dcf77_logic_copy_frame_buffers(
        &dcf77_protocol_frame_scratch,
        app_fsm->next_message,
        app_fsm->next_pulse_frame,
        app_fsm->next_waveform_frame);
}

void dcf77_logic_init(AppFSM* app_fsm) {
    app_fsm->bit_number = 0;
    app_fsm->bit_value = 0;
    app_fsm->current_pulse = RadioClockPulseZero;
    app_fsm->output_state = false;
    app_fsm->output_dirty = false;
    app_fsm->scheduler_second = 0;
    app_fsm->scheduler_segment = 0;
    app_fsm->scheduler_ready = false;
    app_fsm->scheduler_synced = false;
    app_fsm->startup_marker_wrap_pending = false;
    app_fsm->next_minute_prepare_pending = false;
    app_fsm->next_minute_ready = false;
}

void dcf77_logic_prepare_minute(AppFSM* app_fsm, const DateTime* dt, bool as_next_minute) {
    if(as_next_minute) {
        app_fsm->next_minute_dt = *dt;
        dcf77_logic_prepare_frame_buffers(
            app_fsm,
            dt,
            app_fsm->next_message,
            app_fsm->next_pulse_frame,
            app_fsm->next_waveform_frame,
            NULL);
    } else {
        DateTime protocol_dt;
        app_fsm->current_minute_dt = *dt;
        dcf77_logic_prepare_frame_buffers(
            app_fsm,
            dt,
            app_fsm->dcf77_message,
            app_fsm->pulse_frame,
            app_fsm->waveform_frame,
            &protocol_dt);
        dcf77_logic_set_tx_fields(app_fsm, &protocol_dt);
    }
}

void dcf77_logic_activate_next_minute(AppFSM* app_fsm) {
    memcpy(app_fsm->dcf77_message, app_fsm->next_message, sizeof(app_fsm->dcf77_message));
    memcpy(app_fsm->pulse_frame, app_fsm->next_pulse_frame, sizeof(app_fsm->pulse_frame));
    memcpy(app_fsm->waveform_frame, app_fsm->next_waveform_frame, sizeof(app_fsm->waveform_frame));
    app_fsm->current_minute_dt = app_fsm->next_minute_dt;
    DateTime protocol_dt;
    dcf77_logic_get_protocol_datetime(
        app_fsm->current_signal, &app_fsm->current_minute_dt, &protocol_dt);
    dcf77_logic_set_tx_fields(app_fsm, &protocol_dt);
}

void dcf77_logic_sync_start(
    AppFSM* app_fsm,
    uint8_t start_second,
    bool startup_marker_wrap_pending) {
    app_fsm->scheduler_second = start_second;
    app_fsm->scheduler_segment = 0;
    dcf77_logic_set_current_pulse(app_fsm, start_second);
    app_fsm->output_state = false;
    app_fsm->output_dirty = true;
    app_fsm->scheduler_ready = true;
    app_fsm->scheduler_synced = true;
    app_fsm->startup_marker_wrap_pending = startup_marker_wrap_pending;
}

void dcf77_logic_sync_to_second(AppFSM* app_fsm, const DateTime* dt) {
    dcf77_logic_sync_start(app_fsm, dt->second, false);
}
