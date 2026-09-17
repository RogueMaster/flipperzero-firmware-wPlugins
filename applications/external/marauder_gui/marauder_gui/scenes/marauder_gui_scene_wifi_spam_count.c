#include "../marauder_gui_app_i.h"

#define WIFI_SPAM_COUNT_MIN 1
#define WIFI_SPAM_COUNT_MAX 50
#define WIFI_SPAM_COUNT_DEFAULT 10

static void marauder_gui_scene_wifi_spam_count_input_callback(void* context, int32_t number) {
    MarauderGuiApp* app = context;
    app->wifi_spam_count = (int)number;
    view_dispatcher_send_custom_event(app->view_dispatcher, 0);
}

void marauder_gui_scene_wifi_spam_count_on_enter(void* context) {
    MarauderGuiApp* app = context;

    if(app->wifi_spam_count <= 0) {
        app->wifi_spam_count = WIFI_SPAM_COUNT_DEFAULT;
    }

    number_input_set_header_text(app->number_input, marauder_gui_text(app, "Kac sahte AG?", "How many fake APs?"));
    number_input_set_result_callback(
        app->number_input,
        marauder_gui_scene_wifi_spam_count_input_callback,
        app,
        app->wifi_spam_count,
        WIFI_SPAM_COUNT_MIN,
        WIFI_SPAM_COUNT_MAX);

    view_dispatcher_switch_to_view(app->view_dispatcher, MarauderGuiViewNumberInput);
}

bool marauder_gui_scene_wifi_spam_count_on_event(void* context, SceneManagerEvent event) {
    MarauderGuiApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        app->wifi_spam_use_count = true;
        scene_manager_next_scene(app->scene_manager, MarauderGuiSceneWifiSpam);
        consumed = true;
    }

    return consumed;
}

void marauder_gui_scene_wifi_spam_count_on_exit(void* context) {
    UNUSED(context);
}
