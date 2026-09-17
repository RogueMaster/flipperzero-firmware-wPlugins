#include "fujitsu_general_remote.h"
#include "fujitsu_ac_utils.h"
#include "views/view_fujitsu_remote_page1.h"
#include "views/view_fujitsu_remote_page2.h"
#include "views/view_fujitsu_remote_page3.h"
#include "views/view_fujitsu_remote_page_sleep.h"
#include "views/view_fujitsu_remote_page_timer.h"
#include "fujitsu_general_ir.h"
#include "fuji_general_storage.h"

#include <core/core_defines.h>
#include <core/log.h>
#include <gui/view_dispatcher.h>
#include <furi_hal_speaker.h>

const uint8_t FujiAir_C_MIN_TEMP = 0;
const uint8_t FujiAir_C_MAX_TEMP = 24;
const uint8_t FujiAir_C_MAX_FAN_SPEED = 4;

static void fujitsu_remote_play_beep() {
    if(furi_hal_speaker_acquire(1000)) {
        furi_hal_speaker_start(1000.0f, 0.5f);
        furi_delay_ms(100);
        furi_hal_speaker_stop();
        furi_hal_speaker_release();
    }
}

static void fujitsu_remote_send_AC_state(FujitsuAirState* ac, bool power_on) {
    Fuji128Signal sig;
    sig.temperature = ac->temperature + 20;
    sig.power = 0;
    sig.mode = 0;
    sig.swing = 0;
    sig.fan_speed = 0;
    sig.timer_type = 0;
    sig.timer_off = 0;
    sig.flag_timer_off = 0;
    sig.timer_on = 0;

    if(power_on) sig.power = 1;

    if(ac->has_alarm) {
        // Todo:: Check if timers and timer flags are sent each time
        if(ac->sleep_timer != 0) {
            sig.timer_type = 1;
            sig.flag_timer_off = 1;
            sig.timer_off = ac->sleep_timer;
        } else {
            switch(ac->on_off_timer_mode) {
            case FujiAirTimerModes_OFF:
                sig.timer_type = 2;
                break;
            case FujiAirTimerModes_ON:
                sig.timer_type = 3;
                break;
            case FujiAirTimerModes_ON_OFF:
                sig.timer_type = 4;
                break;
            case FujiAirTimerModes_OFF_ON:
                sig.flag_timer_off = 1;
                sig.timer_type = 4;
                break;
            default:
                sig.timer_type = 0;
                break;
            }

            sig.timer_off = ac->off_timer;
            sig.timer_on = ac->on_timer;
        }
    }

    switch(ac->mode) {
    case FujiAirMode_Cool:
        sig.mode = 1;
        break;
    case FujiAirMode_Dry:
        sig.mode = 2;
        break;
    case FujiAirMode_Fan:
        sig.mode = 3;
        break;
    case FujiAirMode_Heat:
        sig.mode = 4;
        break;
    default:
        sig.mode = 0;
        break;
    }

    sig.swing = (ac->swing ? 1 : 0);

    switch(ac->fan_speed) {
    case 1:
        sig.fan_speed = 4;
        break;
    case 2:
        sig.fan_speed = 3;
        break;
    case 3:
        sig.fan_speed = 2;
        break;
    case 4:
        sig.fan_speed = 1;
        break;
    default:
        sig.fan_speed = 0;
        break;
    }

    send_fuji128_state(&sig);
    fujitsu_remote_play_beep();
}

static void fujitsu_remote_send_AC_cmd(Fuji56ProtocolCommand cmd) {
    send_fuji56_command(cmd);
    fujitsu_remote_play_beep();
}

static void fujitsu_remote_air_conditioner_alloc(FujitsuRemoteApp* app) {
    FujitsuAirState* ac = malloc(sizeof(FujitsuAirState));
    /* TODO:: Initialize from file. */
    ac->mode = FujiAirMode_Auto;
    ac->temperature = FujiAir_C_MIN_TEMP;
    ac->fan_speed = 0;
    ac->sleep_timer = 0;
    ac->on_timer = 0;
    ac->off_timer = 0;
    ac->on_off_timer_mode = FujiAirTimerModes_NOT_SET;
    ac->is_powered = false;
    ac->swing = true;
    ac->has_alarm = false;
    ac->economy = false;

    fujitsu_remote_ac_state_read_from_file(ac);
    app->air_conditioner_state = ac;
}

static void fujitsu_remote_air_conditioner_free(FujitsuRemoteApp* app) {
    fujitsu_remote_ac_state_write_to_file(app->air_conditioner_state);
    free(app->air_conditioner_state);
}

static FujitsuRemoteApp* fujitsu_remote_app_alloc() {
    FujitsuRemoteApp* app = malloc(sizeof(FujitsuRemoteApp));
    app->timers = malloc(sizeof(FujitsuRemoteAppTimers));
    app->timers->gui_event_delay = furi_ms_to_ticks(500);

    fujitsu_remote_air_conditioner_alloc(app);

    /* Allocate each view */
    view_fujitsu_remote_page1_alloc(app);
    view_fujitsu_remote_page2_alloc(app);
    view_fujitsu_remote_page3_alloc(app);
    view_fujitsu_remote_page_sleep_alloc(app);
    view_fujitsu_remote_page_timer_alloc(app);
    /* End Allocate each view */

    app->gui = furi_record_open(RECORD_GUI);

    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);

    /* Add each view to view_dispatcher */
    view_fujitsu_remote_page1_add_view(app);
    view_fujitsu_remote_page2_add_view(app);
    view_fujitsu_remote_page3_add_view(app);
    view_fujitsu_remote_page_sleep_add_view(app);
    view_fujitsu_remote_page_timer_add_view(app);
    /* End Add each view to view_dispatcher */

    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);
    return app;
}

static void fujitsu_remote_app_free(FujitsuRemoteApp* app) {
    /* Remove each view */
    view_fujitsu_remote_page1_remove_view(app);
    view_fujitsu_remote_page2_remove_view(app);
    view_fujitsu_remote_page3_remove_view(app);
    view_fujitsu_remote_page_sleep_remove_view(app);
    view_fujitsu_remote_page_timer_remove_view(app);
    /* End Remove each view */

    view_dispatcher_free(app->view_dispatcher);

    /* Free each view */
    view_fujitsu_remote_page1_free(app);
    view_fujitsu_remote_page2_free(app);
    view_fujitsu_remote_page3_free(app);
    view_fujitsu_remote_page_sleep_free(app);
    view_fujitsu_remote_page_timer_free(app);
    /* End free each view */

    furi_record_close(RECORD_GUI);
    fujitsu_remote_air_conditioner_free(app);
    free(app->timers);
    free(app);
}

static bool fujitsu_remote_app_ac_change_event_callback(void* ctx, uint32_t event) {
    FujitsuRemoteApp* app = ctx;
    FujitsuAirState* ac = app->air_conditioner_state;
    UNUSED(app);
    switch(event) {
    case FujitsuRemoteAppEvent_TempUp:
        ac->temperature = fuji_AC_temp_up(ac->temperature);
        return true;
    case FujitsuRemoteAppEvent_TempDown:
        ac->temperature = fuji_AC_temp_down(ac->temperature);
        return true;
    case FujitsuRemoteAppEvent_TempChangeEnd:
        fujitsu_remote_send_AC_state(ac, false);
        return true;
    case FujitsuRemoteAppEvent_PowerToggle:
        ac->is_powered = !ac->is_powered;
        if(ac->is_powered)
            fujitsu_remote_send_AC_state(ac, true);
        else
            fujitsu_remote_send_AC_cmd(Fuji56ProtocolCommand_PowerOff);
        return true;
    case FujitsuRemoteAppEvent_FanSpeedNext:
        ac->fan_speed = fuji_AC_fan_next(ac->fan_speed);
        fujitsu_remote_send_AC_state(ac, false);
        return true;
    case FujitsuRemoteAppEvent_AutoSwingToggle:
        ac->swing = !ac->swing;
        fujitsu_remote_send_AC_state(ac, false);
        return true;
    case FujitsuRemoteAppEvent_ModeNext:
        ac->mode = fuji_AC_mode_next(ac->mode);
        fujitsu_remote_send_AC_state(ac, false);
        return true;
    case FujitsuRemoteAppEvent_EconomyModeToggle:
        ac->economy = !ac->economy;
        fujitsu_remote_send_AC_cmd(Fuji56ProtocolCommand_Economy);
        return true;
    case FujitsuRemoteAppEvent_PowerModeToggle:
        fujitsu_remote_send_AC_cmd(Fuji56ProtocolCommand_Powerfull);
        return true;
    case FujitsuRemoteAppEvent_SetSwingChanged:
        ac->swing = false;
        fujitsu_remote_send_AC_cmd(Fuji56ProtocolCommand_Set);
        return true;
    case FujitsuRemoteAppEvent_CancelTimer:
        ac->has_alarm = false;
        ac->sleep_timer = 0;
        ac->on_off_timer_mode = FujiAirTimerModes_NOT_SET;
        ac->on_timer = 0;
        ac->off_timer = 0;
        fujitsu_remote_send_AC_state(ac, false);
        return true;
    case FujitsuRemoteAppEvent_TimerChanged:
        ac->has_alarm = true;
        ac->sleep_timer = 0;
        fujitsu_remote_send_AC_state(ac, false);
        return true;
    case FujitsuRemoteAppEvent_SleepChanged:
        ac->has_alarm = true;
        ac->on_off_timer_mode = FujiAirTimerModes_NOT_SET;
        ac->on_timer = 0;
        ac->off_timer = 0;
        fujitsu_remote_send_AC_state(ac, false);
        return true;
        /* TODO:: Process Air conditioner state changes events */
        return true;
    }
    return false;
}

static bool fujitsu_remote_app_custom_event_callback(void* ctx, uint32_t event) {
    FujitsuRemoteApp* app = ctx;
    switch(event) {
    case FujitsuRemoteAppEvent_OpenPage1:
        view_dispatcher_switch_to_view(app->view_dispatcher, FujitsuViewId_Page1_MainScreen);
        return true;
    case FujitsuRemoteAppEvent_OpenPage2:
        view_dispatcher_switch_to_view(app->view_dispatcher, FujitsuViewId_Page2);
        return true;
    case FujitsuRemoteAppEvent_OpenPage3:
        view_dispatcher_switch_to_view(app->view_dispatcher, FujitsuViewId_Page3);
        return true;
    case FujitsuRemoteAppEvent_OpenPageSleep:
        view_dispatcher_switch_to_view(app->view_dispatcher, FujitsuViewId_Page_Sleep);
        return true;
    case FujitsuRemoteAppEvent_OpenPageTimer:
        view_dispatcher_switch_to_view(app->view_dispatcher, FujitsuViewId_Page_Timer);
        return true;
    default:
        return fujitsu_remote_app_ac_change_event_callback(ctx, event);
    }
    return false;
}

int32_t fujitsu_general_remote_app(void* p) {
    UNUSED(p);
    FURI_LOG_T(LTAG, "Starting Fujitsu General Remote App");

    FujitsuRemoteApp* app = fujitsu_remote_app_alloc();

    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, fujitsu_remote_app_custom_event_callback);
    view_dispatcher_switch_to_view(app->view_dispatcher, FujitsuViewId_Page1_MainScreen);
    view_dispatcher_run(app->view_dispatcher);

    fujitsu_remote_app_free(app);
    FURI_LOG_T(LTAG, "Ending Fujitsu General Remote App");
    return 0;
}
