#include "view_fujitsu_remote_page1.h"
#include "ac_fujitsu_general_indoor_ir_remote_icons.h"
#include "fujitsu_ac_utils.h"

typedef struct {
    FujiAirMode mode;
    uint16_t temp;
    uint16_t fan;
    bool alarm;
    bool swing;
    bool power;

} FujitsuRemotePage1Model;

void view_fujitsu_remote_page1_alloc(FujitsuRemoteApp* app) {
    app->view_page1 = view_alloc();
    View* view = app->view_page1;
    FujitsuAirState* ac = app->air_conditioner_state;
    view_set_context(view, app);
    view_set_orientation(view, ViewOrientationVertical);
    view_allocate_model(view, ViewModelTypeLocking, sizeof(FujitsuRemotePage1Model));
    view_set_draw_callback(view, view_fujitsu_remote_page1_draw_callback);
    view_set_input_callback(view, view_fujitsu_remote_page1_input_callback);
    view_set_enter_callback(view, view_fujitsu_remote_page1_enter_callback);
    view_set_previous_callback(view, view_fujitsu_remote_page1_exit_callback);

    /* Init model data*/
    with_view_model(
        view,
        FujitsuRemotePage1Model * model,
        {
            model->mode = ac->mode;
            model->temp = ac->temperature;
            model->alarm = ac->has_alarm;
            model->fan = ac->fan_speed;
            model->swing = ac->swing;
            model->power = ac->is_powered;
        },
        false);
}

void view_fujitsu_remote_page1_add_view(FujitsuRemoteApp* app) {
    view_dispatcher_add_view(
        app->view_dispatcher, FujitsuViewId_Page1_MainScreen, app->view_page1);
}

void view_fujitsu_remote_page1_remove_view(FujitsuRemoteApp* app) {
    view_dispatcher_remove_view(app->view_dispatcher, FujitsuViewId_Page1_MainScreen);
}

void view_fujitsu_remote_page1_free(FujitsuRemoteApp* app) {
    view_free(app->view_page1);
}

void view_fujitsu_remote_page1_draw_callback(Canvas* canvas, void* model) {
    FujitsuRemotePage1Model* m = model;
    UNUSED(m);
    canvas_clear(canvas);

    /* Top to bottom */

    { /* ROW 1 */
        const char* power_str = "Power: off";
        if(m->power) power_str = "Power: on";

        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 9, 3, AlignLeft, AlignTop, power_str);
    }

    { /* ROW 2 */
        char str_temp[5] = {0};
        bool half = false;
        uint8_t t_celsius = fuji_AC_temp_to_celsius(m->temp, &half);
        str_temp[1] = '0' + (t_celsius % 10);
        t_celsius /= 10;
        str_temp[0] = '0' + (t_celsius % 10);
        str_temp[2] = '.';
        str_temp[3] = (half ? '5' : '0');

        canvas_set_font(canvas, FontBigNumbers);
        canvas_draw_str_aligned(canvas, 7, 25, AlignLeft, AlignTop, str_temp);

        uint16_t temp_width = canvas_string_width(canvas, str_temp);

        canvas_draw_icon(canvas, 7 + temp_width + 2, 25, &I_asset_fuji_degree_icon_6x6);
    }

    { /* ROW 3 */
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 32, 45, AlignCenter, AlignTop, fuji_AC_mode_str(m->mode));
    }

    { /* ROW 4 */
        if(m->alarm) {
            canvas_draw_icon(canvas, 9, 60, &I_asset_fuji_clock_icon_15x15);
            canvas_draw_str_aligned(canvas, 34, 63, AlignLeft, AlignTop, "ON");
        }
    }

    { /* ROW 5 */
        const char* swing_str = "OFF";
        if(m->swing) swing_str = "ON";
        canvas_draw_icon(canvas, 9, 80, &I_asset_fuji_auto_swing_icon_15x15);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 34, 83, AlignLeft, AlignTop, swing_str);
    }

    { /* ROW 6 */
        const Icon* power_icon = fuji_AC_fan_to_icon(m->fan);
        canvas_draw_icon(canvas, 9, 100, &I_asset_fuji_fan_Icon_15x15);
        canvas_draw_icon(canvas, 34, 100, power_icon);
    }

    /* ROW 7 */
    canvas_draw_icon(canvas, 18, 120, &I_asset_fuji_page_num_25x7_1);

    // canvas_draw_frame(canvas, 0, 0, 64, 128);
}

bool view_fujitsu_remote_page1_input_callback(InputEvent* event, void* ctx) {
    static bool skip_temp_change = false;
    FujitsuRemoteApp* app = ctx;
    bool consumed = false;
    bool update = false;

    if(event->type == InputTypeRelease &&
       (event->key == InputKeyUp || event->key == InputKeyDown)) {
        skip_temp_change = false;
        view_dispatcher_send_custom_event(
            app->view_dispatcher, FujitsuRemoteAppEvent_TempChangeEnd);
        return true;
    }

    with_view_model(
        app->view_page1,
        FujitsuRemotePage1Model * model,
        {
            consumed = true;

            switch(event->key) {
            case InputKeyLeft:
                switch(event->type) {
                case InputTypeShort:
                case InputTypeLong:
                    view_dispatcher_send_custom_event(
                        app->view_dispatcher, FujitsuRemoteAppEvent_OpenPage3);
                    break;
                default:
                    consumed = false;
                    break;
                }
                break;
            case InputKeyRight:
                switch(event->type) {
                case InputTypeShort:
                case InputTypeLong:
                    view_dispatcher_send_custom_event(
                        app->view_dispatcher, FujitsuRemoteAppEvent_OpenPage2);
                    break;
                default:
                    consumed = false;
                    break;
                }
                break;
            case InputKeyUp:
                switch(event->type) {
                case InputTypeRepeat:
                    skip_temp_change = !skip_temp_change;
                    __attribute__((fallthrough));
                case InputTypeShort:
                    if(!skip_temp_change) {
                        model->temp = fuji_AC_temp_up(model->temp);
                        update = true;
                        view_dispatcher_send_custom_event(
                            app->view_dispatcher, FujitsuRemoteAppEvent_TempUp);
                    }
                    break;
                default:
                    consumed = false;
                    break;
                }
                break;
            case InputKeyDown:
                switch(event->type) {
                case InputTypeRepeat:
                    skip_temp_change = !skip_temp_change;
                    __attribute__((fallthrough));
                case InputTypeShort:
                    if(!skip_temp_change) {
                        model->temp = fuji_AC_temp_down(model->temp);
                        update = true;
                        view_dispatcher_send_custom_event(
                            app->view_dispatcher, FujitsuRemoteAppEvent_TempDown);
                    }
                    break;
                default:
                    consumed = false;
                    break;
                }
                break;
            case InputKeyOk:
                switch(event->type) {
                case InputTypePress:
                    view_dispatcher_send_custom_event(
                        app->view_dispatcher, FujitsuRemoteAppEvent_PowerToggle);
                    model->power = !model->power;
                    update = true;
                    break;
                default:
                    consumed = false;
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

void view_fujitsu_remote_page1_enter_callback(void* ctx) {
    furi_assert(ctx);
    FujitsuRemoteApp* app = ctx;
    FujitsuAirState* ac = app->air_conditioner_state;

    /* Init model data*/
    with_view_model(
        app->view_page1,
        FujitsuRemotePage1Model * model,
        {
            model->mode = ac->mode;
            model->temp = ac->temperature;
            model->alarm = ac->has_alarm;
            model->fan = ac->fan_speed;
            model->swing = ac->swing;
            model->power = ac->is_powered;
        },
        true);
}

uint32_t view_fujitsu_remote_page1_exit_callback(void* ctx) {
    UNUSED(ctx);
    return VIEW_NONE;
}
