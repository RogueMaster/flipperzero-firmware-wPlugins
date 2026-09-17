#include "../marauder_gui_app_i.h"

enum {
    BtTrackerActionIndexFindMy,
    BtTrackerActionIndexSpoof,
};

static const MarauderMenuItem marauder_bt_tracker_action_items[] = {
    {"Ses Cal",
     "Play Sound",
     "Secilen AirTag/tracker'in sesini uzaktan caldirir, fiziksel olarak bulmani kolaylastirir.",
     "Remotely plays the selected AirTag/tracker's sound, making it easier to find physically."},
    {"Spoof Et",
     "Spoof",
     "Secilen tracker'in kimligini taklit ederek surekli yayin yapar.",
     "Continuously broadcasts, impersonating the selected tracker's identity."},
};

void marauder_gui_scene_bt_tracker_action_on_enter(void* context) {
    MarauderGuiApp* app = context;

    const char* target = "Tracker";
    if(app->selected_tracker_index >= 0 && (size_t)app->selected_tracker_index < app->ap_count) {
        target = app->ap_list[app->selected_tracker_index];
    }

    marauder_gui_menu_set_items(
        app,
        marauder_bt_tracker_action_items,
        sizeof(marauder_bt_tracker_action_items) / sizeof(marauder_bt_tracker_action_items[0]),
        target);
}

bool marauder_gui_scene_bt_tracker_action_on_event(void* context, SceneManagerEvent event) {
    MarauderGuiApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == BtTrackerActionIndexFindMy) {
            app->bt_tracker_action = 0;
            scene_manager_next_scene(app->scene_manager, MarauderGuiSceneBtTrackerStatus);
            consumed = true;
        } else if(event.event == BtTrackerActionIndexSpoof) {
            app->bt_tracker_action = 1;
            scene_manager_next_scene(app->scene_manager, MarauderGuiSceneBtTrackerStatus);
            consumed = true;
        }
    }

    return consumed;
}

void marauder_gui_scene_bt_tracker_action_on_exit(void* context) {
    MarauderGuiApp* app = context;
    app->tick_handler = NULL;
}
