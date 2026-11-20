#include "zero_buster_scene.h"
#include "zero_buster.h"

bool zero_buster_scene_about_on_event(ZeroBusterApp* app, InputEvent* event) {
    if(event->type == InputTypeShort) {
        if(event->key == InputKeyOk || event->key == InputKeyBack) {
            app->state = State_Idle;
        }
    }
    return true;
}
