#include "../t_union_master_i.h"

void tum_scene_about_on_enter(void* ctx) {
    TUM_App* app = ctx;
    widget_add_string_element_utf8(app->widget, 64, 2, AlignCenter, AlignTop, "关于");
    widget_add_frame_element(app->widget, 0, 0, 128, 16, 0);

    widget_add_text_scroll_element_utf8(
        app->widget,
        0,
        18,
        128,
        44,
        "本APP用于查询交通联合卡（T-Union）的信息、余额、行程及交易记录，支持显示多个城市的公交和地铁站点数据。\n"
        "原理：基于Flipper硬件支持的NFC-4A（EMV）协议实现与卡片进行通讯。\n"
        "Ver：V0.1\n"
        "Author：社会易姐QwQ <1440239038@qq.com>");
    view_dispatcher_switch_to_view(app->view_dispatcher, TUM_ViewWidget);
}

bool tum_scene_about_on_event(void* ctx, SceneManagerEvent event) {
    TUM_App* app = ctx;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeBack) {
        consumed =
            scene_manager_search_and_switch_to_previous_scene(app->scene_manager, TUM_SceneStart);
    }
    if(event.type == SceneManagerEventTypeTick) {
    }

    return consumed;
}

void tum_scene_about_on_exit(void* ctx) {
    TUM_App* app = ctx;

    widget_reset(app->widget);
}
