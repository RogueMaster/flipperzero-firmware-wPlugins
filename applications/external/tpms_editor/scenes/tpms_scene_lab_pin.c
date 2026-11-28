#include "../tpms_app_i.h"
#define TPMS_LAB_PIN 7777
static void tpms_lab_pin_done(void* context, int32_t value) {
    TPMSApp* app = context;
    if(!app) return;
    if(value == TPMS_LAB_PIN) {
        app->lab_unlocked = true;
        scene_manager_next_scene(app->scene_manager, TPMSSceneLabPressure);
    } else {
        app->lab_unlocked = false;
        scene_manager_previous_scene(app->scene_manager);
    }
}
void tpms_scene_lab_pin_on_enter(void* context) {
    TPMSApp* app = context;
    if(!app || !tpms_ensure_number_input(app)) return;
    number_input_set_header_text(app->number_input, "Code");
    number_input_set_result_callback(app->number_input, tpms_lab_pin_done, app, 0, 0, 9999);
    view_dispatcher_switch_to_view(app->view_dispatcher, TPMSViewNumberInput);
}
bool tpms_scene_lab_pin_on_event(void* context, SceneManagerEvent event) { UNUSED(context); UNUSED(event); return false; }
void tpms_scene_lab_pin_on_exit(void* context) { UNUSED(context); }
