#include "timer_app.h"
#include "timer_logic.h"
#include "timer_ui.h"
#include "timer_alarm.h"

int32_t timer_app_main(void* p) {
    UNUSED(p);
    PomodoroApp* app = malloc(sizeof(PomodoroApp));
    memset(app, 0, sizeof(PomodoroApp));
    app->record_view_active = false;
    app->daily_record_count = 0;
    app->alarm_sound_active = false;

    // 日毎の記録初期化
    load_daily_records(app);
    time_t now = furi_hal_rtc_get_timestamp();
    int year, mon, day, dummy, dummy2, dummy3, dummy4;
    epoch_to_utc_breakdown(now, &year, &mon, &day, &dummy, &dummy2, &dummy3, &dummy4);
    if(app->current_daily_record.year != year || app->current_daily_record.month != mon ||
       app->current_daily_record.day != day) {
        app->current_daily_record.year = year;
        app->current_daily_record.month = mon;
        app->current_daily_record.day = day;
        app->current_daily_record.start_count = 0;
        app->current_daily_record.rest_count = 0;
        app->current_daily_record.fail_count = 0;
    }

    app->gui = furi_record_open("gui");
    app->view_port = view_port_alloc();
    view_port_draw_callback_set(app->view_port, draw_callback, app);
    view_port_input_callback_set(app->view_port, input_callback, app);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);

    now = furi_hal_rtc_get_timestamp();
    app->running = true;
    app->state.phase = PomodoroPhaseWaitingRest;
    int hour, minute;
    get_hour_minute(now, &hour, &minute);
    int diff_to_50 = 50 - minute;
    if(diff_to_50 < 0) diff_to_50 += 60;
    app->state.next_rest_time = now + diff_to_50 * 60;
    app->state.last_hour_triggered = -1;
    app->state.countdown_seconds = 0; // カウントダウン秒数の初期化
    app->blinking = false;
    app->hourly_alarm_active = false;
    app->start_count = 0;
    app->rest_count = 0;
    app->fail_count = 0;

    while(app->running) {
        update_logic(app);
        view_port_update(app->view_port);
        furi_delay_ms(200);
    }

    save_daily_records(app);
    gui_remove_view_port(app->gui, app->view_port);
    view_port_free(app->view_port);
    furi_record_close("gui");
    free(app);
    return 0;
}
