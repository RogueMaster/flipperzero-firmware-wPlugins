#include "view_fujitsu_remote_page3.h"
#include "ac_fujitsu_general_indoor_ir_remote_icons.h"

typedef enum {
    FujiPage3Button_Timer = 0,
    FujiPage3Button_Cancel,
    FujiPage3Button_Sleep,
    FujiPage3Button_Power,
    FujiPage3Button_Count
} FujiPage3Button;

static const FujiPage3Button g_FujiPage3DefaultButton = FujiPage3Button_Cancel;

typedef struct {
    FujiPage3Button button_selected;
    bool cancel_in_progress;
    bool power_in_progress;
} FujitsuRemotePage3Model;

static FujiPage3Button view_fujitsu_remote_page3_button_up(FujiPage3Button cur_button) {
    if(cur_button == FujiPage3Button_Timer) return FujiPage3Button_Power;

    return --cur_button;
}

static FujiPage3Button view_fujitsu_remote_page3_button_down(FujiPage3Button cur_button) {
    if(cur_button == FujiPage3Button_Power) return FujiPage3Button_Timer;

    return ++cur_button;
}

static void view_fujitsu_remote_page3_timer1_callback(void* ctx) {
    furi_assert(ctx);
    FujitsuRemoteApp* app = ctx;
    with_view_model(
        app->view_page3,
        FujitsuRemotePage3Model * model,
        { model->cancel_in_progress = false; },
        true);
}

static void view_fujitsu_remote_page3_timer2_callback(void* ctx) {
    furi_assert(ctx);
    FujitsuRemoteApp* app = ctx;
    with_view_model(
        app->view_page3,
        FujitsuRemotePage3Model * model,
        { model->power_in_progress = false; },
        true);
}

void view_fujitsu_remote_page3_alloc(FujitsuRemoteApp* app) {
    app->view_page3 = view_alloc();
    View* view = app->view_page3;
    view_set_context(view, app);
    view_set_orientation(view, ViewOrientationVertical);
    view_allocate_model(view, ViewModelTypeLocking, sizeof(FujitsuRemotePage3Model));
    view_set_draw_callback(view, view_fujitsu_remote_page3_draw_callback);
    view_set_input_callback(view, view_fujitsu_remote_page3_input_callback);
    view_set_previous_callback(view, view_fujitsu_remote_page3_exit_callback);

    /* Init model data*/
    with_view_model(
        view,
        FujitsuRemotePage3Model * model,
        {
            model->button_selected = g_FujiPage3DefaultButton;
            model->cancel_in_progress = false;
            model->power_in_progress = false;
        },
        false);

    app->timers->view_page3_timer1 =
        furi_timer_alloc(view_fujitsu_remote_page3_timer1_callback, FuriTimerTypeOnce, app);
    app->timers->view_page3_timer2 =
        furi_timer_alloc(view_fujitsu_remote_page3_timer2_callback, FuriTimerTypeOnce, app);
}

void view_fujitsu_remote_page3_add_view(FujitsuRemoteApp* app) {
    view_dispatcher_add_view(app->view_dispatcher, FujitsuViewId_Page3, app->view_page3);
}

void view_fujitsu_remote_page3_remove_view(FujitsuRemoteApp* app) {
    view_dispatcher_remove_view(app->view_dispatcher, FujitsuViewId_Page3);
}

void view_fujitsu_remote_page3_free(FujitsuRemoteApp* app) {
    furi_timer_stop(app->timers->view_page3_timer2);
    furi_timer_free(app->timers->view_page3_timer2);
    furi_timer_stop(app->timers->view_page3_timer1);
    furi_timer_free(app->timers->view_page3_timer1);
    view_free(app->view_page3);
}

void view_fujitsu_remote_page3_draw_callback(Canvas* canvas, void* model) {
    FujitsuRemotePage3Model* m = model;
    canvas_clear(canvas);
    canvas_set_font(canvas, FontSecondary);

    { /* ROW 1 Timer */
        if(m->button_selected == FujiPage3Button_Timer) canvas_invert_color(canvas);

        canvas_draw_icon(canvas, 14, 5, &I_asset_fuji_page3_blank_button_34x19);
        canvas_draw_str_aligned(canvas, 30, 14, AlignCenter, AlignCenter, "Timer");

        if(m->button_selected == FujiPage3Button_Timer) canvas_invert_color(canvas);
    }

    { /* ROW 2 Cancel */
        if(m->button_selected == FujiPage3Button_Cancel) canvas_invert_color(canvas);

        canvas_draw_icon(canvas, 14, 35, &I_asset_fuji_page3_blank_button_34x19);
        if(m->cancel_in_progress)
            canvas_draw_str_aligned(canvas, 30, 43, AlignCenter, AlignCenter, "...");
        else
            canvas_draw_str_aligned(canvas, 30, 44, AlignCenter, AlignCenter, "Cancel");

        if(m->button_selected == FujiPage3Button_Cancel) canvas_invert_color(canvas);
    }

    { /* ROW 3 Sleep */
        if(m->button_selected == FujiPage3Button_Sleep) canvas_invert_color(canvas);

        canvas_draw_icon(canvas, 14, 65, &I_asset_fuji_page3_blank_button_34x19);
        canvas_draw_str_aligned(canvas, 30, 74, AlignCenter, AlignCenter, "Sleep");

        if(m->button_selected == FujiPage3Button_Sleep) canvas_invert_color(canvas);
    }

    { /* ROW 4 Power */
        if(m->button_selected == FujiPage3Button_Power) canvas_invert_color(canvas);

        canvas_draw_icon(canvas, 14, 95, &I_asset_fuji_page3_blank_button_34x19);
        if(m->power_in_progress)
            canvas_draw_str_aligned(canvas, 30, 104, AlignCenter, AlignCenter, "...");
        else
            canvas_draw_str_aligned(canvas, 30, 104, AlignCenter, AlignCenter, "Power");

        if(m->button_selected == FujiPage3Button_Power) canvas_invert_color(canvas);
    }

    /* ROW 5 */
    canvas_draw_icon(canvas, 15, 120, &I_asset_fuji_page_num_25x7_3);
}

bool view_fujitsu_remote_page3_input_callback(InputEvent* event, void* ctx) {
    UNUSED(event);

    FujitsuRemoteApp* app = ctx;
    bool consumed = true;
    bool update = false;

    if(event->type != InputTypeShort && event->type != InputTypeRepeat) return false;

    with_view_model(
        app->view_page3,
        FujitsuRemotePage3Model * model,
        {
            switch(event->key) {
            case InputKeyLeft:
                view_dispatcher_send_custom_event(
                    app->view_dispatcher, FujitsuRemoteAppEvent_OpenPage2);
                break;
            case InputKeyRight:
                view_dispatcher_send_custom_event(
                    app->view_dispatcher, FujitsuRemoteAppEvent_OpenPage1);
                break;
            case InputKeyUp:
                model->button_selected =
                    view_fujitsu_remote_page3_button_up(model->button_selected);
                update = true;
                break;
            case InputKeyDown:
                model->button_selected =
                    view_fujitsu_remote_page3_button_down(model->button_selected);
                update = true;
                break;
            case InputKeyOk:
                switch(model->button_selected) {
                case FujiPage3Button_Timer:
                    view_dispatcher_send_custom_event(
                        app->view_dispatcher, FujitsuRemoteAppEvent_OpenPageTimer);
                    break;
                case FujiPage3Button_Cancel:
                    model->cancel_in_progress = true;
                    furi_timer_start(app->timers->view_page3_timer1, app->timers->gui_event_delay);
                    view_dispatcher_send_custom_event(
                        app->view_dispatcher, FujitsuRemoteAppEvent_CancelTimer);
                    update = true;
                    break;
                case FujiPage3Button_Sleep:
                    view_dispatcher_send_custom_event(
                        app->view_dispatcher, FujitsuRemoteAppEvent_OpenPageSleep);
                    break;
                case FujiPage3Button_Power:
                    model->power_in_progress = true;
                    furi_timer_start(app->timers->view_page3_timer2, app->timers->gui_event_delay);
                    view_dispatcher_send_custom_event(
                        app->view_dispatcher, FujitsuRemoteAppEvent_PowerModeToggle);
                    update = true;
                    break;
                default:
                    furi_assert(false && "Page 3: Unknown button");
                }
                break;
            default:
                consumed = false;
                break;
            }
        },
        update) return consumed;
}

void view_fujitsu_remote_page3_enter_callback(void* ctx) {
    furi_assert(ctx);
    FujitsuRemoteApp* app = ctx;
    with_view_model(
        app->view_page3,
        FujitsuRemotePage3Model * model,
        {
            model->button_selected = g_FujiPage3DefaultButton;
            model->cancel_in_progress = false;
            model->power_in_progress = false;
        },
        true);
}

uint32_t view_fujitsu_remote_page3_exit_callback(void* ctx) {
    UNUSED(ctx);
    return VIEW_NONE;
}
