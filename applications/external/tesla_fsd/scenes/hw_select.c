#include "../tesla_fsd_app.h"
#include "../scenes_config/app_scene_functions.h"

static void hw_select_callback(void* context, uint32_t index) {
    TeslaFSDApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void tesla_fsd_scene_hw_select_on_enter(void* context) {
    TeslaFSDApp* app = context;

    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Select HW Version");
    submenu_add_item(app->submenu, "HW3", TeslaFSDEventSelectHW3, hw_select_callback, app);
    submenu_add_item(app->submenu, "HW4 (FSD V14)", TeslaFSDEventSelectHW4, hw_select_callback, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, TeslaFSDViewSubmenu);
}

bool tesla_fsd_scene_hw_select_on_event(void* context, SceneManagerEvent event) {
    TeslaFSDApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        TeslaHWVersion hw = TeslaHW_Unknown;
        if(event.event == TeslaFSDEventSelectHW3) hw = TeslaHW_HW3;
        if(event.event == TeslaFSDEventSelectHW4) hw = TeslaHW_HW4;

        if(hw != TeslaHW_Unknown) {
            app->hw_version = hw;
            fsd_state_init(&app->fsd_state, hw);
            scene_manager_next_scene(app->scene_manager, tesla_fsd_scene_fsd_running);
            consumed = true;
        }
    }
    return consumed;
}

void tesla_fsd_scene_hw_select_on_exit(void* context) {
    TeslaFSDApp* app = context;
    submenu_reset(app->submenu);
}
