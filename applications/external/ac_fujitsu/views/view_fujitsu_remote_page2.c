#include <core/timer.h>
#include "view_fujitsu_remote_page2.h"
#include "ac_fujitsu_general_indoor_ir_remote_icons.h"
#include "fujitsu_general_remote.h"
#include "fujitsu_ac_utils.h"

typedef enum {
    FujiPage2BUtton_Mode = 0,
    FujiPage2BUtton_ECO,
    FujiPage2BUtton_Set,
    FujiPage2BUtton_Swing,
    FujiPage2BUtton_Fan,
    FujiPage2Button_Count
} FujiPage2Button;

static const FujiPage2Button g_DefaultButton = FujiPage2BUtton_Set;

typedef struct {
    FujiAirMode mode;
    uint8_t fan;
    bool eco;
    bool swing;
    bool set_in_progress;
    FujiPage2Button button_selected; /** @brief button selected (from 0 to 4) */

} FujitsuRemotePage2Model;

static FujiPage2Button view_fujitsu_remote_page2_button_up(FujiPage2Button cur_button) {
    if(cur_button == FujiPage2BUtton_Mode) return FujiPage2BUtton_Fan;

    return --cur_button;
}

static FujiPage2Button view_fujitsu_remote_page2_button_down(FujiPage2Button cur_button) {
    if(cur_button == FujiPage2BUtton_Fan) return FujiPage2BUtton_Mode;

    return ++cur_button;
}

static void view_fujitsu_remote_page2_timer1_callback(void* ctx) {
    furi_assert(ctx);
    FujitsuRemoteApp* app = ctx;
    with_view_model(
        app->view_page2,
        FujitsuRemotePage2Model * model,
        { model->set_in_progress = false; },
        true);
}

void view_fujitsu_remote_page2_alloc(FujitsuRemoteApp* app) {
    app->view_page2 = view_alloc();
    View* view = app->view_page2;
    view_set_context(view, app);
    view_set_orientation(view, ViewOrientationVertical);
    view_allocate_model(view, ViewModelTypeLocking, sizeof(FujitsuRemotePage2Model));
    view_set_draw_callback(view, view_fujitsu_remote_page2_draw_callback);
    view_set_input_callback(view, view_fujitsu_remote_page2_input_callback);
    view_set_enter_callback(view, view_fujitsu_remote_page2_enter_callback);
    view_set_previous_callback(view, view_fujitsu_remote_page2_exit_callback);

    FujitsuAirState* ac = app->air_conditioner_state;
    /* Init model data*/
    with_view_model(
        view,
        FujitsuRemotePage2Model * model,
        {
            model->mode = ac->mode;
            model->fan = ac->fan_speed;
            model->eco = ac->economy;
            model->swing = ac->swing;
            model->set_in_progress = false;
            model->button_selected = g_DefaultButton;
        },
        false);

    app->timers->view_page2_timer1 =
        furi_timer_alloc(view_fujitsu_remote_page2_timer1_callback, FuriTimerTypeOnce, app);
}

void view_fujitsu_remote_page2_add_view(FujitsuRemoteApp* app) {
    view_dispatcher_add_view(app->view_dispatcher, FujitsuViewId_Page2, app->view_page2);
}

void view_fujitsu_remote_page2_remove_view(FujitsuRemoteApp* app) {
    view_dispatcher_remove_view(app->view_dispatcher, FujitsuViewId_Page2);
}

void view_fujitsu_remote_page2_free(FujitsuRemoteApp* app) {
    furi_timer_stop(app->timers->view_page2_timer1);
    furi_timer_free(app->timers->view_page2_timer1);
    view_free(app->view_page2);
}

void view_fujitsu_remote_page2_draw_callback(Canvas* canvas, void* model) {
    FujitsuRemotePage2Model* m = model;

    canvas_clear(canvas);

    { /* ROW 1 Mode */
        if(m->button_selected == FujiPage2BUtton_Mode) canvas_invert_color(canvas);
        canvas_draw_icon(canvas, 6, 9, &I_asset_fuji_buttons_page2_20x18_09_mod);
        if(m->button_selected == FujiPage2BUtton_Mode) canvas_invert_color(canvas);

        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 30, 14, AlignLeft, AlignTop, fuji_AC_mode_str(m->mode));
    }

    { /* ROW 2 Eco */
        if(m->button_selected == FujiPage2BUtton_ECO) canvas_invert_color(canvas);
        canvas_draw_icon(canvas, 6, 31, &I_asset_fuji_buttons_page2_20x18_07_eco);
        if(m->button_selected == FujiPage2BUtton_ECO) canvas_invert_color(canvas);

        const char* eco_txt = (m->eco ? "ON" : "OFF");
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 30, 35, AlignLeft, AlignTop, eco_txt);
    }

    { /* ROW 3 Set */
        if(m->button_selected == FujiPage2BUtton_Set) canvas_invert_color(canvas);
        canvas_draw_icon(canvas, 6, 53, &I_asset_fuji_buttons_page2_20x18_05_set);
        if(m->button_selected == FujiPage2BUtton_Set) canvas_invert_color(canvas);

        if(m->set_in_progress) {
            canvas_set_font(canvas, FontPrimary);
            canvas_draw_str_aligned(canvas, 30, 62, AlignLeft, AlignCenter, "...");
        }
    }

    { /* ROW 4 Swing */
        if(m->button_selected == FujiPage2BUtton_Swing) canvas_invert_color(canvas);
        canvas_draw_icon(canvas, 6, 75, &I_asset_fuji_buttons_page2_20x18_03_swing);
        if(m->button_selected == FujiPage2BUtton_Swing) canvas_invert_color(canvas);

        const char* swing_txt = (m->swing ? "ON" : "OFF");
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 30, 80, AlignLeft, AlignTop, swing_txt);
    }

    { /* ROW 5 Fan */
        if(m->button_selected == FujiPage2BUtton_Fan) canvas_invert_color(canvas);
        canvas_draw_icon(canvas, 6, 97, &I_asset_fuji_buttons_page2_20x18_01_fan);
        if(m->button_selected == FujiPage2BUtton_Fan) canvas_invert_color(canvas);

        const Icon* power_bars_icon = fuji_AC_fan_to_icon(m->fan);
        canvas_draw_icon(canvas, 30, 98, power_bars_icon);
    }
    /* ROW 6 */
    canvas_draw_icon(canvas, 15, 120, &I_asset_fuji_page_num_25x7_2);

    // canvas_draw_frame(canvas, 0, 0, 64, 128);
}

bool view_fujitsu_remote_page2_input_callback(InputEvent* event, void* ctx) {
    UNUSED(event);

    FujitsuRemoteApp* app = ctx;
    bool consumed = true;
    bool update = false;

    if(event->type != InputTypePress) {
        return false;
    }

    with_view_model(
        app->view_page2,
        FujitsuRemotePage2Model * model,
        {
            switch(event->key) {
            case InputKeyLeft:
                view_dispatcher_send_custom_event(
                    app->view_dispatcher, FujitsuRemoteAppEvent_OpenPage1);
                break;
            case InputKeyRight:
                view_dispatcher_send_custom_event(
                    app->view_dispatcher, FujitsuRemoteAppEvent_OpenPage3);
                break;
            case InputKeyUp:
                model->button_selected =
                    view_fujitsu_remote_page2_button_up(model->button_selected);
                update = true;
                break;
            case InputKeyDown:
                model->button_selected =
                    view_fujitsu_remote_page2_button_down(model->button_selected);
                update = true;
                break;
            case InputKeyOk:
                switch(model->button_selected) {
                case FujiPage2BUtton_Mode:
                    model->mode = fuji_AC_mode_next(model->mode);
                    view_dispatcher_send_custom_event(
                        app->view_dispatcher, FujitsuRemoteAppEvent_ModeNext);
                    update = true;
                    break;
                case FujiPage2BUtton_ECO:
                    model->eco = !model->eco;
                    view_dispatcher_send_custom_event(
                        app->view_dispatcher, FujitsuRemoteAppEvent_EconomyModeToggle);
                    update = true;
                    break;
                case FujiPage2BUtton_Set:
                    model->swing = false;
                    model->set_in_progress = true;
                    view_dispatcher_send_custom_event(
                        app->view_dispatcher, FujitsuRemoteAppEvent_SetSwingChanged);
                    furi_timer_start(app->timers->view_page2_timer1, app->timers->gui_event_delay);
                    update = true;
                    break;
                case FujiPage2BUtton_Swing:
                    model->swing = !model->swing;
                    view_dispatcher_send_custom_event(
                        app->view_dispatcher, FujitsuRemoteAppEvent_AutoSwingToggle);
                    update = true;
                    break;
                case FujiPage2BUtton_Fan:
                    model->fan = fuji_AC_fan_next(model->fan);
                    view_dispatcher_send_custom_event(
                        app->view_dispatcher, FujitsuRemoteAppEvent_FanSpeedNext);
                    update = true;
                    break;
                default:
                    furi_assert(false && "Page2: Unknown button");
                }
                break;
            default:
                consumed = false;
                break;
            }
        },
        update);
    return consumed;
}

void view_fujitsu_remote_page2_enter_callback(void* ctx) {
    furi_assert(ctx);
    FujitsuRemoteApp* app = ctx;
    with_view_model(
        app->view_page2,
        FujitsuRemotePage2Model * model,
        { model->button_selected = g_DefaultButton; },
        true);
}

uint32_t view_fujitsu_remote_page2_exit_callback(void* ctx) {
    UNUSED(ctx);
    return VIEW_NONE;
}
