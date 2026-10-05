#include "dab_timer.h"
#include "dab_timer_icons.h"

#define TAG "DabTimer"

/* Precomputed endpoints preserve the existing analog clock pixels without libm. */
static const DabTimerClockPoint dab_clock_hours[12] = {
    {92, 10},
    {103, 12},
    {111, 21},
    {114, 32},
    {111, 42},
    {103, 51},
    {92, 53},
    {81, 51},
    {72, 43},
    {70, 32},
    {72, 21},
    {80, 12},
};

static const DabTimerClockPoint dab_clock_minutes[60] = {
    {92, 6},   {94, 6},   {97, 6},   {100, 7},  {102, 8},  {105, 9},  {107, 10}, {109, 12},
    {111, 14}, {113, 16}, {114, 19}, {115, 21}, {116, 23}, {117, 26}, {117, 29}, {118, 32},
    {117, 34}, {117, 37}, {116, 40}, {115, 42}, {114, 44}, {113, 47}, {111, 49}, {109, 51},
    {107, 53}, {105, 54}, {102, 55}, {100, 56}, {97, 57},  {94, 57},  {92, 57},  {89, 57},
    {86, 57},  {83, 56},  {81, 55},  {79, 54},  {76, 53},  {74, 51},  {72, 49},  {70, 47},
    {69, 45},  {68, 42},  {67, 40},  {66, 37},  {66, 34},  {66, 32},  {66, 29},  {66, 26},
    {67, 24},  {68, 21},  {69, 19},  {70, 16},  {72, 14},  {74, 12},  {76, 10},  {78, 9},
    {81, 8},   {83, 7},   {86, 6},   {89, 6},
};

static const DabTimerClockPoint dab_clock_seconds[60] = {
    {92, 4},   {94, 4},   {97, 4},   {100, 5},  {103, 6},  {106, 7},  {108, 9},  {110, 11},
    {112, 13}, {114, 15}, {116, 18}, {117, 20}, {118, 23}, {119, 26}, {119, 29}, {120, 32},
    {119, 34}, {119, 37}, {118, 40}, {117, 43}, {116, 45}, {114, 48}, {112, 50}, {110, 52},
    {108, 54}, {106, 56}, {103, 57}, {100, 58}, {97, 59},  {94, 59},  {92, 59},  {89, 59},
    {86, 59},  {83, 58},  {80, 57},  {78, 56},  {75, 54},  {73, 52},  {71, 50},  {69, 48},
    {67, 46},  {66, 43},  {65, 40},  {64, 37},  {64, 34},  {64, 32},  {64, 29},  {64, 26},
    {65, 23},  {66, 20},  {67, 18},  {69, 15},  {71, 13},  {73, 11},  {75, 9},   {77, 7},
    {80, 6},   {83, 5},   {86, 4},   {89, 4},
};

static void dab_timer_render_binary_face(Canvas* canvas, uint32_t value, int32_t height) {
    /* Retain the six-bit face's existing saturation for values above 63. */
    const uint32_t bits = MIN(value, 63U);
    for(uint8_t bit = 0; bit < 6; bit++) {
        const Icon* icon = bits & (1U << (5 - bit)) ? &I_GameModeIcon_11x8 :
                                                      &I_InvertGameMode_11x8;
        canvas_draw_icon(canvas, 24 + 14 * bit, height, icon);
    }
}

static void dab_timer_render_callback(Canvas* canvas, void* context) {
    DabTimerApp* app = context;
    DabTimerModel snapshot;
    if(furi_mutex_acquire(app->mutex, 0) != FuriStatusOk) {
        furi_thread_flags_set(app->thread_id, DabTimerFlagRedraw);
        return;
    }
    snapshot = app->model;
    furi_mutex_release(app->mutex);
    const DabTimerModel* plugin_state = &snapshot;
    const DateTime curr_dt = snapshot.datetime;
    const char* time_string = snapshot.time_string;
    const char* date_string = snapshot.date_string;
    const char* meridian_string = snapshot.meridian_string;
    const char* timer_string = snapshot.timer_string;
    const char* alertTime = snapshot.alert_string;
    const bool timer_running = snapshot.run_state == DabTimerRunning;
    const bool timer_started = snapshot.run_state != DabTimerReady;
    const uint32_t elapsed_secs = snapshot.elapsed_seconds;
    const SoundAlert sound_alert = snapshot.sound_alert;
    if(plugin_state->face_style == FaceStylePwn ||
       plugin_state->face_style == FaceStylePwnInverted) {
        if(plugin_state->face_style == FaceStylePwnInverted) {
            canvas_draw_icon(canvas, 0, 0, &I_black);
            if(timer_started) {
                elements_button_left(canvas, "Reset");
            } else {
                elements_button_left(canvas, "F6");
            }
            canvas_set_color(canvas, ColorWhite);
        } else {
            canvas_set_color(canvas, ColorWhite);
            if(timer_started) {
                elements_button_left(canvas, "Reset");
            } else {
                elements_button_left(canvas, "F1");
            }
            canvas_set_color(canvas, ColorBlack);
        }
        canvas_set_font(canvas, FontBigNumbers);
        canvas_draw_str_aligned(canvas, 64, 8, AlignCenter, AlignCenter, time_string); // DRAW TIME
        if(plugin_state->easter_egg && timer_started) {
            int32_t elapsed_secs_img = (elapsed_secs % 60) % 5;
            static const Icon* const count_anim[5] = {
                &I_HappyFlipper_128x64, &I_G0ku, &I_g0ku_1, &I_g0ku_2, &I_g0ku_3};
            canvas_draw_icon(canvas, -5, 15, count_anim[elapsed_secs_img]);
            canvas_draw_str_aligned(
                canvas, 96, 31, AlignCenter, AlignTop, timer_string); // DRAW TIMER
        } else if(timer_started) {
            canvas_draw_str_aligned(
                canvas, 96, 32, AlignCenter, AlignTop, timer_string); // DRAW TIMER
            static const Icon* const flip_face[DabTimerFaceCount] = {
                &I_angry_flipagotchi,        &I_awake_flipagotchi,
                &I_bored_flipagotchi,        &I_broken_flipagotchi,
                &I_cool_flipagotchi,         &I_debug_flipagotchi,
                &I_demotivated_flipagotchi,  &I_excited_flipagotchi,
                &I_friend_flipagotchi,       &I_grateful_flipagotchi,
                &I_happy_flipagotchi,        &I_intense_flipagotchi,
                &I_lonely_flipagotchi,       &I_look_l_flipagotchi,
                &I_look_l_happy_flipagotchi, &I_look_r_flipagotchi,
                &I_look_r_happy_flipagotchi, &I_motivated_flipagotchi,
                &I_sad_flipagotchi,          &I_sleep2_flipagotchi,
                &I_sleep_flipagotchi,        &I_smart_flipagotchi,
                &I_upload1_flipagotchi,      &I_upload2_flipagotchi,
                &I_upload_flipagotchi};
            canvas_draw_icon(canvas, 1, 32, flip_face[plugin_state->emotive_face]);
        } else {
            canvas_draw_icon(canvas, 1, 32, &I_cool_flipagotchi);
        }
#ifdef CANVAS_HAS_FONT_BATTERYPERCENT
        canvas_set_font(canvas, FontBatteryPercent);
        canvas_draw_str_aligned(canvas, 117, 11, AlignCenter, AlignCenter, alertTime);
        if(plugin_state->time_format == LocaleTimeFormat12h)
            canvas_draw_str_aligned(canvas, 117, 4, AlignCenter, AlignCenter, meridian_string);
        canvas_draw_str_aligned(canvas, 96, 20, AlignCenter, AlignTop, date_string); // DRAW DATE
#else
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 118, 12, AlignCenter, AlignCenter, alertTime);
        if(plugin_state->time_format == LocaleTimeFormat12h)
            canvas_draw_str_aligned(canvas, 118, 4, AlignCenter, AlignCenter, meridian_string);
        canvas_draw_str_aligned(canvas, 64, 20, AlignCenter, AlignTop, date_string); // DRAW DATE
#endif
    } else if(
        plugin_state->face_style == FaceStyleOriginal ||
        plugin_state->face_style == FaceStyleOriginalInverted) {
        canvas_set_font(canvas, FontSecondary);
        if(plugin_state->face_style == FaceStyleOriginalInverted) {
            canvas_draw_icon(canvas, 0, 0, &I_black);
            if(timer_started) {
                elements_button_left(canvas, "Reset");
            } else {
                elements_button_left(canvas, "F7");
            }
            canvas_set_color(canvas, ColorWhite);
        } else {
            canvas_set_color(canvas, ColorWhite);
            if(timer_started) {
                elements_button_left(canvas, "Reset");
            } else {
                elements_button_left(canvas, "F2");
            }
            canvas_set_color(canvas, ColorBlack);
        }
        canvas_set_font(canvas, FontBigNumbers);
        if(timer_started && !plugin_state->easter_egg) {
            canvas_draw_str_aligned(
                canvas, 64, 8, AlignCenter, AlignCenter, time_string); // DRAW TIME
            canvas_draw_str_aligned(
                canvas, 64, 32, AlignCenter, AlignTop, timer_string); // DRAW TIMER
#ifdef CANVAS_HAS_FONT_BATTERYPERCENT
            canvas_set_font(canvas, FontBatteryPercent);
            if(plugin_state->time_format == LocaleTimeFormat12h)
                canvas_draw_str_aligned(canvas, 117, 4, AlignCenter, AlignCenter, meridian_string);
            canvas_draw_str_aligned(canvas, 117, 11, AlignCenter, AlignCenter, alertTime);
#else
            canvas_set_font(canvas, FontSecondary);
            if(plugin_state->time_format == LocaleTimeFormat12h)
                canvas_draw_str_aligned(canvas, 118, 4, AlignCenter, AlignCenter, meridian_string);
            canvas_draw_str_aligned(canvas, 118, 12, AlignCenter, AlignCenter, alertTime);
#endif
            canvas_draw_str_aligned(
                canvas, 64, 20, AlignCenter, AlignTop, date_string); // DRAW DATE
#ifdef CANVAS_HAS_FONT_BATTERYPERCENT
            canvas_set_font(canvas, FontSecondary);
#endif
        } else {
#ifdef CANVAS_HAS_FONT_BATTERYPERCENT
            if(plugin_state->easter_egg) canvas_set_font(canvas, FontBatteryPercent);
#else
            if(plugin_state->easter_egg) canvas_set_font(canvas, FontSecondary);
#endif
            if(plugin_state->easter_egg && timer_started) {
                uint32_t elapsed_secs_img = (elapsed_secs % 60) % 5;
                uint32_t elapsed_secs_img2 = (elapsed_secs % 60) % 4;
                static const Icon* const count_anim[5] = {
                    &I_HappyFlipper_128x64, &I_G0ku, &I_g0ku_1, &I_g0ku_2, &I_g0ku_3};
                static const Icon* const count_anim2[4] = {
                    &I_EviWaiting1_18x21,
                    &I_EviWaiting2_18x21,
                    &I_EviSmile1_18x21,
                    &I_EviSmile2_18x21};
                static const Icon* const count_anim3[4] = {
                    &I_frame_01, &I_frame_02, &I_frame_03, &I_frame_02};
#ifdef CANVAS_HAS_FONT_BATTERYPERCENT
                canvas_draw_icon(canvas, -5, 15, count_anim[elapsed_secs_img]);
                canvas_draw_icon(canvas, 90, 0, count_anim2[elapsed_secs_img2]);
                canvas_draw_icon(canvas, 110, 5, count_anim3[elapsed_secs_img2]);
                canvas_draw_str_aligned(
                    canvas, 64, 32, AlignCenter, AlignTop, timer_string); // DRAW TIMER
            }
            canvas_draw_str_aligned(
                canvas, 64, 26, AlignCenter, AlignCenter, time_string); // DRAW TIME
            canvas_set_font(canvas, FontBatteryPercent);
            if(plugin_state->time_format == LocaleTimeFormat12h)
                canvas_draw_str_aligned(canvas, 69, 15, AlignCenter, AlignCenter, meridian_string);
#else
                canvas_draw_icon(canvas, -6, 14, count_anim[elapsed_secs_img]);
                canvas_draw_icon(canvas, 55, -2, count_anim2[elapsed_secs_img2]);
                canvas_draw_icon(canvas, 40, 4, count_anim3[elapsed_secs_img2]);
                canvas_draw_str_aligned(
                    canvas, 97, 38, AlignCenter, AlignCenter, timer_string); // DRAW TIMER
                canvas_draw_str_aligned(
                    canvas, 97, 26, AlignCenter, AlignCenter, time_string); // DRAW TIME
                if(plugin_state->time_format == LocaleTimeFormat12h)
                    canvas_draw_str_aligned(
                        canvas, 97, 14, AlignCenter, AlignCenter, meridian_string);
            } else {
                canvas_draw_str_aligned(
                    canvas, 64, 26, AlignCenter, AlignCenter, time_string); // DRAW TIME
                canvas_set_font(canvas, FontSecondary);
                if(plugin_state->time_format == LocaleTimeFormat12h)
                    canvas_draw_str_aligned(
                        canvas, 100, 14, AlignCenter, AlignCenter, meridian_string);
            }
#endif
            if(!plugin_state->easter_egg)
                canvas_draw_str_aligned(
                    canvas, 64, 38, AlignCenter, AlignTop, date_string); // DRAW DATE
#ifdef CANVAS_HAS_FONT_BATTERYPERCENT
            canvas_set_font(canvas, FontSecondary);
#endif
        }
    } else if(
        plugin_state->face_style == FaceStyleOriginalSmall ||
        plugin_state->face_style == FaceStyleOriginalSmallInverted) {
        if(plugin_state->face_style == FaceStyleOriginalSmallInverted) {
            canvas_draw_icon(canvas, 0, 0, &I_black);
            if(timer_started) {
                elements_button_left(canvas, "Reset");
            } else {
                elements_button_left(canvas, "F8");
            }
            canvas_set_color(canvas, ColorWhite);
        } else {
            canvas_set_color(canvas, ColorWhite);
            if(timer_started) {
                elements_button_left(canvas, "Reset");
            } else {
                elements_button_left(canvas, "F3");
            }
            canvas_set_color(canvas, ColorBlack);
        }
#ifdef CANVAS_HAS_FONT_BATTERYPERCENT
        canvas_set_font(canvas, FontBatteryPercent);
#else
        canvas_set_font(canvas, FontSecondary);
#endif
        if(timer_started) {
            canvas_draw_str_aligned(
                canvas, 64, 31, AlignCenter, AlignTop, timer_string); // DRAW TIMER
        }
        canvas_draw_str_aligned(
#ifdef CANVAS_HAS_FONT_BATTERYPERCENT
            canvas, 64, 26, AlignCenter, AlignCenter, time_string); // DRAW TIME
        canvas_set_font(canvas, FontBatteryPercent);
        if(plugin_state->time_format == LocaleTimeFormat12h)
            canvas_draw_str_aligned(canvas, 69, 15, AlignCenter, AlignCenter, meridian_string);
        if(!plugin_state->easter_egg)
            canvas_draw_str_aligned(
                canvas, 64, 38, AlignCenter, AlignTop, date_string); // DRAW DATE
        canvas_set_font(canvas, FontSecondary);
#else
            canvas, 64, 24, AlignCenter, AlignCenter, time_string); // DRAW TIME
        if(plugin_state->time_format == LocaleTimeFormat12h)
            canvas_draw_str_aligned(canvas, 94, 24, AlignCenter, AlignCenter, meridian_string);
        if(!plugin_state->easter_egg)
            canvas_draw_str_aligned(
                canvas, 64, 40, AlignCenter, AlignTop, date_string); // DRAW DATE
#endif
    } else if(
        plugin_state->face_style == FaceStyleCircle ||
        plugin_state->face_style == FaceStyleCircleInverted) {
        if(plugin_state->face_style == FaceStyleCircleInverted) {
            canvas_draw_icon(canvas, 0, 0, &I_black);
            canvas_set_color(canvas, ColorWhite);
        } else {
            canvas_set_color(canvas, ColorWhite);
            canvas_set_color(canvas, ColorBlack);
        }
        canvas_draw_circle(canvas, 92, 32, 30);
        static const DabTimerClockPoint numeral_positions[] = {
            {118, 32}, {92, 57}, {66, 32}, {91, 6}};
        static const char* const numerals[] = {"3", "6", "9", "12"};
        for(size_t i = 0; i < COUNT_OF(numerals); i++) {
            canvas_draw_str_aligned(
                canvas,
                numeral_positions[i].x,
                numeral_positions[i].y,
                AlignCenter,
                AlignCenter,
                numerals[i]);
        }
        const DabTimerClockPoint hour = dab_clock_hours[curr_dt.hour % COUNT_OF(dab_clock_hours)];
        const DabTimerClockPoint minute = dab_clock_minutes[curr_dt.minute];
        const DabTimerClockPoint second = dab_clock_seconds[curr_dt.second];
        canvas_draw_line(canvas, 92, 32, hour.x, hour.y);
        canvas_draw_line(canvas, 92, 32, minute.x, minute.y);
        canvas_draw_line(canvas, 92, 32, second.x, second.y);
        canvas_set_font(canvas, FontSecondary);
        const char* circle_date = plugin_state->circle_date_string;
        const uint16_t date_width = canvas_string_width(canvas, circle_date);
        canvas_draw_frame(canvas, 0, 41, date_width + 6, 13);
        canvas_draw_str(canvas, 3, 51, circle_date);
        if(timer_started) {
            canvas_draw_str_aligned(canvas, 15, 32, AlignCenter, AlignTop, timer_string);
        }
    } else {
        if(plugin_state->face_style == FaceStyleBinaryInverted) {
            canvas_draw_icon(canvas, 0, 0, &I_black);
            canvas_set_color(canvas, ColorWhite);
        } else {
            canvas_set_color(canvas, ColorWhite);
            canvas_set_color(canvas, ColorBlack);
        }
#ifdef CANVAS_HAS_FONT_BATTERYPERCENT
        canvas_set_font(canvas, FontBatteryPercent);
#endif
        if(timer_started) {
            dab_timer_render_binary_face(canvas, elapsed_secs / 60, 5);
            dab_timer_render_binary_face(canvas, elapsed_secs % 60, 5 + (9 * 1));
            dab_timer_render_binary_face(canvas, curr_dt.hour, 5 + (9 * 3));
            dab_timer_render_binary_face(canvas, curr_dt.minute, 5 + (9 * 4));
            dab_timer_render_binary_face(canvas, curr_dt.second, 5 + (9 * 5));
        } else {
            dab_timer_render_binary_face(canvas, curr_dt.hour, 18);
            dab_timer_render_binary_face(canvas, curr_dt.minute, 28);
            dab_timer_render_binary_face(canvas, curr_dt.second, 38);
        }
    }
    if(plugin_state->face_style >= FaceStylePwnInverted) {
        canvas_set_color(canvas, ColorBlack);
    }
    if(plugin_state->face_style < FaceStylePwnInverted) {
        canvas_set_color(canvas, ColorWhite);
    }
    if(plugin_state->face_style != FaceStyleCircle &&
       plugin_state->face_style != FaceStyleBinary &&
       plugin_state->face_style != FaceStyleCircleInverted &&
       plugin_state->face_style != FaceStyleBinaryInverted) {
#if __has_include(<cfw/cfw.h>)
        if(!plugin_state->game_mode && !plugin_state->easter_egg) {
#else
        if(!plugin_state->easter_egg) {
#endif
            if(timer_running) {
                elements_button_center(canvas, "Stop");
            } else {
                elements_button_center(canvas, "Start");
            }
        }
        if(timer_running && !plugin_state->easter_egg) {
            if(sound_alert == SoundAlertOff) {
                elements_button_right(canvas, "S:OFF");
            } else if(sound_alert == SoundAlertGoGoPoRa) {
                elements_button_right(canvas, "S:PoRa");
            } else if(sound_alert == SoundAlertMario) {
                elements_button_right(canvas, "S:Mario");
            } else if(sound_alert == SoundAlertByMin) {
                elements_button_right(canvas, "S:ByMin");
            } else if(sound_alert == SoundAlertCont) {
                elements_button_right(canvas, "S:Cont");
            }
        }
#if __has_include(<cfw/cfw.h>)
        if(plugin_state->easter_egg && plugin_state->game_mode) {
#else
        if(plugin_state->easter_egg) {
#endif
            canvas_draw_icon(canvas, 0, 0, &I_GameModeIcon_11x8);
        }
    }
}

static void dab_timer_rearm_alarm(DabTimerState* state) {
    state->alarm_phase = DabTimerAlarmWaiting;
    state->last_continuous_second = UINT32_MAX;
}

static void dab_timer_reset_elapsed(DabTimerState* state) {
    state->model.elapsed_seconds = 0;
    state->fractional_ticks = 0;
    state->last_minute = 0;
    dab_timer_rearm_alarm(state);
}

static void dab_timer_state_init(DabTimerState* state, uint32_t now, uint32_t tick_frequency) {
    memset(state, 0, sizeof(*state));
    furi_check(tick_frequency);
    state->last_tick = now;
    state->tick_frequency = tick_frequency;
    state->model.alert_time = 80;
    state->model.face_style = FaceStylePwn;
    state->model.sound_alert = SoundAlertMario;
    state->model.run_state = DabTimerReady;
    state->code_state = DabTimerCodeIdle;
#if __has_include(<cfw/cfw.h>)
    state->model.game_mode = cfw_settings.game_mode;
#endif
    dab_timer_rearm_alarm(state);
}

static void dab_timer_advance_time(DabTimerState* state, uint32_t now) {
    const uint32_t delta = now - state->last_tick;
    state->last_tick = now;
    if(state->model.run_state != DabTimerRunning) return;
    const uint32_t frequency = state->tick_frequency;
    const uint32_t previous_face_step = state->model.elapsed_seconds / 3;
    uint32_t seconds = delta / frequency;
    const uint32_t remainder = delta % frequency;
    /* Carry without overflowing even for a large tick-frequency value. */
    if(remainder >= frequency - state->fractional_ticks) {
        seconds++;
        state->fractional_ticks = remainder - (frequency - state->fractional_ticks);
    } else {
        state->fractional_ticks += remainder;
    }
    if(seconds > UINT32_MAX - state->model.elapsed_seconds) {
        state->model.elapsed_seconds = UINT32_MAX;
    } else {
        state->model.elapsed_seconds += seconds;
    }
    const uint32_t face_steps = state->model.elapsed_seconds / 3 - previous_face_step;
    state->model.emotive_face =
        (DabTimerEmotiveFace)((state->model.emotive_face + face_steps) % DabTimerFaceCount);
}

static DabTimerAction dab_timer_collect_alerts(DabTimerState* state) {
    const DabTimerModel* model = &state->model;
    if(model->run_state != DabTimerRunning) return DabTimerActionNone;
    DabTimerAction actions = DabTimerActionNone;
    const uint32_t minute = model->elapsed_seconds / 60;
    if(minute > state->last_minute) {
        const uint32_t crossed_minute = (state->last_minute + 1) * 60;
        if(model->sound_alert != SoundAlertOff &&
           (crossed_minute < model->alert_time || model->sound_alert == SoundAlertByMin)) {
            actions |= DabTimerActionMinute;
        }
        state->last_minute = minute;
    }
    if(model->elapsed_seconds < model->alert_time) return actions;
    if(model->sound_alert == SoundAlertCont) {
        if(state->last_continuous_second != model->elapsed_seconds) {
            state->last_continuous_second = model->elapsed_seconds;
            actions |= DabTimerActionMarioFirst;
        }
        return actions;
    }
    const bool mario = model->sound_alert == SoundAlertMario;
    const bool rangers = model->sound_alert == SoundAlertGoGoPoRa;
    if(state->alarm_phase == DabTimerAlarmWaiting) {
        actions |= mario   ? DabTimerActionMarioFirst :
                   rangers ? DabTimerActionRangersFirst :
                             DabTimerActionSilent;
        state->alarm_phase = mario || rangers ? DabTimerAlarmFirstPlayed : DabTimerAlarmFinished;
        if(!mario && !rangers) actions |= DabTimerActionRainbow;
        if(model->easter_egg &&
           (!state->xp_tick_valid ||
            (uint32_t)(state->last_tick - state->last_xp_tick) / state->tick_frequency >= 10)) {
            actions |= DabTimerActionXp;
            state->last_xp_tick = state->last_tick;
            state->xp_tick_valid = true;
        }
    }
    const uint32_t phase_elapsed = model->elapsed_seconds - model->alert_time;
    if(state->alarm_phase == DabTimerAlarmFirstPlayed && phase_elapsed >= 1) {
        actions |= mario ? DabTimerActionMarioSecond : DabTimerActionRangersSecond;
        state->alarm_phase = DabTimerAlarmSecondPlayed;
    }
    if(state->alarm_phase == DabTimerAlarmSecondPlayed && phase_elapsed >= 2) {
        actions |= mario ? DabTimerActionMarioThird : DabTimerActionRangersThird;
        actions |= DabTimerActionRainbow;
        state->alarm_phase = DabTimerAlarmFinished;
    }
    return actions;
}

static DabTimerInputResult
    dab_timer_handle_input(DabTimerState* state, const InputEvent* input, DabTimerAction* actions) {
    DabTimerModel* model = &state->model;
    if(input->type == InputTypeLong) {
        if(input->key == InputKeyBack) return DabTimerInputExit;
        if(input->key != InputKeyLeft) return DabTimerInputIgnored;
        state->code_state = DabTimerCodeIdle;
        dab_timer_reset_elapsed(state);
        model->run_state = DabTimerReady;
        return DabTimerInputChanged;
    }
    if(input->type != InputTypeShort && input->type != InputTypeRepeat)
        return DabTimerInputIgnored;
    switch(input->key) {
    case InputKeyUp:
        if(state->code_state == DabTimerCodeIdle)
            state->code_state = DabTimerCodeFirstUp;
        else if(state->code_state == DabTimerCodeFirstUp)
            state->code_state = DabTimerCodeSecondUp;
        else
            state->code_state = DabTimerCodeIdle;
        if(model->run_state == DabTimerRunning && model->alert_time <= UINT32_MAX - 5) {
            model->alert_time += 5;
            dab_timer_rearm_alarm(state);
        }
        break;
    case InputKeyDown:
        if(state->code_state == DabTimerCodeSecondUp)
            state->code_state = DabTimerCodeFirstDown;
        else if(state->code_state == DabTimerCodeFirstDown)
            state->code_state = DabTimerCodeSecondDown;
        else
            state->code_state = DabTimerCodeIdle;
        if(model->run_state == DabTimerRunning && model->alert_time >= 5) {
            model->alert_time -= 5;
            dab_timer_rearm_alarm(state);
        }
        break;
    case InputKeyLeft:
        if(state->code_state == DabTimerCodeSecondDown)
            state->code_state = DabTimerCodeFirstLeft;
        else if(state->code_state == DabTimerCodeFirstRight)
            state->code_state = DabTimerCodeSecondLeft;
        else {
            state->code_state = DabTimerCodeIdle;
            if(model->run_state != DabTimerReady)
                dab_timer_reset_elapsed(state);
            else
                model->face_style = (FaceStyle)((model->face_style + 1) % FaceStyleCount);
        }
        break;
    case InputKeyRight:
        if(state->code_state == DabTimerCodeFirstLeft)
            state->code_state = DabTimerCodeFirstRight;
        else if(state->code_state == DabTimerCodeSecondLeft) {
            state->code_state = DabTimerCodeSecondRight;
#if __has_include(<cfw/cfw.h>)
            model->game_mode = !model->game_mode;
            *actions |= DabTimerActionSaveGameMode;
#endif
            model->easter_egg = true;
        } else {
            state->code_state = DabTimerCodeIdle;
            model->sound_alert = (SoundAlert)((model->sound_alert + 1) % SoundAlertCount);
            dab_timer_rearm_alarm(state);
        }
        break;
    case InputKeyOk:
        if(state->code_state == DabTimerCodeExitBack) {
            state->code_state = DabTimerCodeIdle;
#if __has_include(<cfw/cfw.h>)
            model->game_mode = false;
            *actions |= DabTimerActionSaveGameMode;
#endif
            if(model->sound_alert != SoundAlertOff) {
                *actions |= DabTimerActionSuccess | DabTimerActionRainbow;
            }
            *actions |= DabTimerActionXp;
        } else {
            state->code_state = DabTimerCodeIdle;
            if(!model->game_mode) {
                if(model->sound_alert != SoundAlertOff) *actions |= DabTimerActionStartStop;
                model->run_state = model->run_state == DabTimerRunning ? DabTimerPaused :
                                                                         DabTimerRunning;
            }
        }
        break;
    case InputKeyBack:
        if(state->code_state == DabTimerCodeSecondRight)
            state->code_state = DabTimerCodeExitBack;
        else {
            model->easter_egg = false;
            if(state->code_state == DabTimerCodeIdle)
                state->code_state = DabTimerCodeBackPending;
            else
                return DabTimerInputExit;
        }
        break;
    default:
        return DabTimerInputIgnored;
    }
    return DabTimerInputChanged;
}

static void dab_timer_refresh_display(DabTimerState* state) {
    DabTimerModel* model = &state->model;
    DateTime datetime = {0};
    furi_hal_rtc_get_datetime(&datetime);
    const LocaleTimeFormat time_format = locale_get_time_format();
    const LocaleDateFormat date_format = locale_get_date_format();
    if(!model->time_string[0] || time_format != model->time_format ||
       datetime.hour != model->datetime.hour || datetime.minute != model->datetime.minute ||
       datetime.second != model->datetime.second) {
        unsigned hour = datetime.hour;
        if(time_format == LocaleTimeFormat12h) hour = hour % 12 ? hour % 12 : 12;
        snprintf(
            model->time_string,
            sizeof(model->time_string),
            "%02u:%02u:%02u",
            hour,
            (unsigned)datetime.minute,
            (unsigned)datetime.second);
        snprintf(
            model->meridian_string,
            sizeof(model->meridian_string),
            "%s",
            datetime.hour >= 12 ? "PM" : "AM");
    }
    if(!model->date_string[0] || date_format != model->date_format ||
       datetime.year != model->datetime.year || datetime.month != model->datetime.month ||
       datetime.day != model->datetime.day) {
        const bool ymd = date_format == LocaleDateFormatYMD;
        const unsigned first = ymd                                ? datetime.year :
                               date_format == LocaleDateFormatMDY ? datetime.month :
                                                                    datetime.day;
        const unsigned second = date_format == LocaleDateFormatMDY ? datetime.day : datetime.month;
        const unsigned third = ymd ? datetime.day : datetime.year;
        snprintf(
            model->date_string,
            sizeof(model->date_string),
            ymd ? "%04u-%02u-%02u" : "%02u-%02u-%04u",
            first,
            second,
            third);
        snprintf(
            model->circle_date_string,
            sizeof(model->circle_date_string),
            ymd ? "%04u.%02u.%02u" : "%02u.%02u.%04u",
            first,
            second,
            third);
    }
    model->datetime = datetime;
    model->date_format = date_format;
    model->time_format = time_format;
    snprintf(
        model->timer_string,
        sizeof(model->timer_string),
        "%02lu:%02lu",
        (unsigned long)(model->elapsed_seconds / 60),
        (unsigned long)(model->elapsed_seconds % 60));
    snprintf(
        model->alert_string, sizeof(model->alert_string), "%lu", (unsigned long)model->alert_time);
}

static bool dab_timer_apply_actions(
    NotificationApp* notification,
    const DabTimerState* state,
    DabTimerAction actions) {
#if __has_include(<cfw/cfw.h>)
    if(actions & DabTimerActionSaveGameMode) {
        cfw_settings.game_mode = state->model.game_mode;
        cfw_settings_save();
    }
#else
    UNUSED(state);
#endif
    static const struct {
        DabTimerAction action;
        const NotificationSequence* sequence;
    } sequences[] = {
        {DabTimerActionStartStop, &dab_timer_alert_startStop},
        {DabTimerActionMinute, &dab_timer_alert_perMin},
        {DabTimerActionMarioFirst, &dab_timer_alert_mario1},
        {DabTimerActionMarioSecond, &dab_timer_alert_mario2},
        {DabTimerActionMarioThird, &dab_timer_alert_mario3},
        {DabTimerActionRangersFirst, &dab_timer_alert_pr1},
        {DabTimerActionRangersSecond, &dab_timer_alert_pr2},
        {DabTimerActionRangersThird, &dab_timer_alert_pr3},
        {DabTimerActionSilent, &dab_timer_alert_silent},
        {DabTimerActionSuccess, &sequence_success},
    };
    bool queued_notifications = false;
    for(size_t i = 0; i < COUNT_OF(sequences); i++) {
        if(actions & sequences[i].action) {
            notification_message(notification, sequences[i].sequence);
            queued_notifications = true;
        }
    }
    if(actions & DabTimerActionRainbow) {
        notification_message(notification, &sequence_rainbow);
        queued_notifications = true;
        notification_message(notification, &sequence_rainbow);
    }
    if(actions & DabTimerActionXp) {
#if __has_include(<cfw/cfw.h>)
        dolphin_deed(getRandomDeed());
#else
        dolphin_deed(DolphinDeedBadUsbPlayScript);
#endif
    }
    return queued_notifications;
}

static void dab_timer_input_callback(InputEvent* input, void* context) {
    DabTimerApp* app = context;
    if(input->type == InputTypeLong && input->key == InputKeyBack) {
        furi_thread_flags_set(app->thread_id, DabTimerFlagExit);
        return;
    }
    if(input->type != InputTypeShort && input->type != InputTypeRepeat &&
       !(input->type == InputTypeLong && input->key == InputKeyLeft))
        return;
    const uint32_t timeout = input->type == InputTypeRepeat ? 0 : 10;
    if(furi_message_queue_put(app->event_queue, input, timeout) == FuriStatusOk) {
        furi_thread_flags_set(app->thread_id, DabTimerFlagInput);
    }
}

static void dab_timer_tick(void* context) {
    DabTimerApp* app = context;
    /* Coalesced wakeups are safe: elapsed time comes from the kernel tick clock. */
    furi_thread_flags_set(app->thread_id, DabTimerFlagTick);
}

int32_t dab_timer_app(void* context) {
    UNUSED(context);
    DabTimerApp* app = malloc(sizeof(*app));
    if(!app) return 255;
    memset(app, 0, sizeof(*app));
    app->thread_id = furi_thread_get_current_id();
    app->event_queue = furi_message_queue_alloc(16, sizeof(InputEvent));
    app->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    if(!app->event_queue || !app->mutex) {
        if(app->event_queue) furi_message_queue_free(app->event_queue);
        if(app->mutex) furi_mutex_free(app->mutex);
        free(app);
        return 255;
    }
    FuriTimer* timer = furi_timer_alloc(dab_timer_tick, FuriTimerTypePeriodic, app);
    if(!timer) {
        furi_mutex_free(app->mutex);
        furi_message_queue_free(app->event_queue);
        free(app);
        return 255;
    }
    DabTimerState state;
    dab_timer_state_init(&state, furi_get_tick(), furi_kernel_get_tick_frequency());
    dab_timer_refresh_display(&state);
    app->model = state.model;
    ViewPort* view_port = view_port_alloc();
    view_port_draw_callback_set(view_port, dab_timer_render_callback, app);
    view_port_input_callback_set(view_port, dab_timer_input_callback, app);
    NotificationApp* notification = furi_record_open(RECORD_NOTIFICATION);
    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);
    furi_timer_start(timer, state.tick_frequency);
    bool notifications_sent = false;
    for(bool processing = true; processing;) {
        const uint32_t flags = furi_thread_flags_wait(
            DabTimerFlagInput | DabTimerFlagTick | DabTimerFlagRedraw | DabTimerFlagExit,
            FuriFlagWaitAny,
            FuriWaitForever);
        if(flags & FuriFlagError) break;
        if(flags & DabTimerFlagExit) break;
        const DabTimerModel before = state.model;
        dab_timer_advance_time(&state, furi_get_tick());
        const DabTimerAction timer_actions = dab_timer_collect_alerts(&state);
        DabTimerAction actions = DabTimerActionNone;
        InputEvent input;
        if(furi_message_queue_get(app->event_queue, &input, 0) == FuriStatusOk) {
            if(dab_timer_handle_input(&state, &input, &actions) == DabTimerInputExit) {
                processing = false;
            }
        }
        if(furi_message_queue_get_count(app->event_queue)) {
            furi_thread_flags_set(app->thread_id, DabTimerFlagInput);
        }
        if(!processing) break;
        actions |= dab_timer_collect_alerts(&state);
        dab_timer_refresh_display(&state);
        const bool changed = memcmp(&before, &state.model, sizeof(before)) != 0;
        if(changed) {
            furi_check(furi_mutex_acquire(app->mutex, FuriWaitForever) == FuriStatusOk);
            app->model = state.model;
            furi_mutex_release(app->mutex);
        }
        if(changed || (flags & DabTimerFlagRedraw)) view_port_update(view_port);
        notifications_sent |= dab_timer_apply_actions(notification, &state, timer_actions);
        notifications_sent |= dab_timer_apply_actions(notification, &state, actions);
    }
    furi_timer_free(timer);
    /* The notification queue retains pointers into this FAP until playback finishes. */
    if(notifications_sent)
        notification_message_block(notification, &sequence_display_backlight_on);
    view_port_enabled_set(view_port, false);
    gui_remove_view_port(gui, view_port);
    view_port_free(view_port);
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);
    furi_message_queue_free(app->event_queue);
    furi_mutex_free(app->mutex);
    free(app);
    return 0;
}
