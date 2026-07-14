#include "../vk_thermo.h"
#include "../helpers/vk_thermo_custom_event.h"
#include "../views/vk_thermo_graph_view.h"

static void vk_thermo_scene_graph_callback(VkThermoCustomEvent event, void* context) {
    furi_assert(context);
    VkThermo* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, event);
}

void vk_thermo_scene_graph_on_enter(void* context) {
    furi_assert(context);
    VkThermo* app = context;

    // Set up graph view
    vk_thermo_graph_view_set_callback(app->graph_view, vk_thermo_scene_graph_callback, app);
    vk_thermo_graph_view_set_log(app->graph_view, &app->log);
    vk_thermo_graph_view_set_temp_unit(app->graph_view, app->temp_unit);

    view_dispatcher_switch_to_view(app->view_dispatcher, VkThermoViewIdGraph);
}

bool vk_thermo_scene_graph_on_event(void* context, SceneManagerEvent event) {
    VkThermo* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case VkThermoCustomEventGraphBack:
            scene_manager_previous_scene(app->scene_manager);
            consumed = true;
            break;

        default:
            break;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        scene_manager_previous_scene(app->scene_manager);
        consumed = true;
    }

    return consumed;
}

void vk_thermo_scene_graph_on_exit(void* context) {
    VkThermo* app = context;
    UNUSED(app);
}
