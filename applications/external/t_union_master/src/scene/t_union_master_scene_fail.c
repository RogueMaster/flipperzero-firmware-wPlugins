#include "../t_union_master_i.h"

void tum_scene_fail_widget_callback(GuiButtonType result, InputType type, void* ctx) {
    TUM_App* app = ctx;
    if(type == InputTypeShort) {
        view_dispatcher_send_custom_event(app->view_dispatcher, result);
    }
}

void tum_scene_fail_on_enter(void* ctx) {
    TUM_App* app = ctx;
    Widget* widget = app->widget;

    notification_message(app->notifications, &sequence_error);

    widget_add_icon_element(widget, 83, 22, &I_WarningDolphinFlip_45x42);
    widget_add_string_element_utf8(widget, 5, 4, AlignLeft, AlignTop, "读取出错，请重试！");

    char* error_reason;
    switch(app->poller_error) {
    case TUnionPollerErrorNotPresent:
        error_reason = "Err:无权限\nNotPresent";
        break;
    case TUnionPollerErrorTimeout:
        error_reason = "Err:通讯超时\nTimeout";
        break;
    case TUnionPollerErrorProtocol:
        error_reason = "Err:协议错误\nProtocol";
        break;
    case TUnionPollerErrorInvaildAID:
        error_reason = "Err:应用不支持\nInvaildAID";
        break;
    case TUnionPollerErrorNone:
    default:
        error_reason = "";
    }
    widget_add_string_multiline_element_utf8(widget, 5, 22, AlignLeft, AlignTop, error_reason);

    widget_add_button_element_utf8(
        widget, GuiButtonTypeLeft, "重试", tum_scene_fail_widget_callback, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, TUM_ViewWidget);
}

bool tum_scene_fail_on_event(void* ctx, SceneManagerEvent event) {
    TUM_App* app = ctx;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == GuiButtonTypeLeft) {
            consumed = scene_manager_search_and_switch_to_previous_scene(
                app->scene_manager, TUM_SceneRead);
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        consumed =
            scene_manager_search_and_switch_to_previous_scene(app->scene_manager, TUM_SceneRead);
    }

    return consumed;
}

void tum_scene_fail_on_exit(void* ctx) {
    TUM_App* app = ctx;

    widget_reset(app->widget);
}
