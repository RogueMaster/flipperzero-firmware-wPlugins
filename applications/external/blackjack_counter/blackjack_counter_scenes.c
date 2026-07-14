#include "blackjack_counter.h"

void blackjack_counter_scene_menu_on_enter(void* context) {
    BlackjackCounterApp* app = context;
    submenu_set_selected_item(app->submenu, 0);
    view_dispatcher_switch_to_view(app->view_dispatcher, BlackjackCounterViewMenu);
}

bool blackjack_counter_scene_menu_on_event(void* context, SceneManagerEvent event) {
    BlackjackCounterApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        uint32_t selected = event.event;

        if(selected == 8) { // Index
            scene_manager_next_scene(app->scene_manager, BlackjackCounterSceneIndex);
        } else if(selected == 9) { // About
            scene_manager_next_scene(app->scene_manager, BlackjackCounterSceneAbout);
        } else {
            BlackjackCounterScene next_scene = BlackjackCounterSceneHiLo + selected;
            BlackjackCounterMethod method = (BlackjackCounterMethod)selected;
            blackjack_counter_view_set_method(app->counter_views[selected], method);
            scene_manager_next_scene(app->scene_manager, next_scene);
        }
        consumed = true;
    }

    return consumed;
}

void blackjack_counter_scene_menu_on_exit(void* context) {
    UNUSED(context);
}

void blackjack_counter_scene_counter_on_enter(void* context) {
    BlackjackCounterApp* app = context;

    uint32_t current_scene = scene_manager_get_current_scene(app->scene_manager);
    uint32_t counter_index = current_scene - BlackjackCounterSceneHiLo;
    uint32_t view_id = BlackjackCounterViewCounter + counter_index;

    view_dispatcher_switch_to_view(app->view_dispatcher, view_id);
}

bool blackjack_counter_scene_counter_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void blackjack_counter_scene_counter_on_exit(void* context) {
    UNUSED(context);
}

// Index scene
void blackjack_counter_scene_index_on_enter(void* context) {
    BlackjackCounterApp* app = context;
    view_dispatcher_switch_to_view(app->view_dispatcher, BlackjackCounterViewIndex);
}

bool blackjack_counter_scene_index_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void blackjack_counter_scene_index_on_exit(void* context) {
    UNUSED(context);
}

// About scene
void blackjack_counter_scene_about_on_enter(void* context) {
    BlackjackCounterApp* app = context;
    view_dispatcher_switch_to_view(app->view_dispatcher, BlackjackCounterViewAbout);
}

bool blackjack_counter_scene_about_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void blackjack_counter_scene_about_on_exit(void* context) {
    UNUSED(context);
}

void (*const blackjack_counter_scene_on_enter_handlers[])(void*) = {
    blackjack_counter_scene_menu_on_enter,
    blackjack_counter_scene_counter_on_enter,
    blackjack_counter_scene_counter_on_enter,
    blackjack_counter_scene_counter_on_enter,
    blackjack_counter_scene_counter_on_enter,
    blackjack_counter_scene_counter_on_enter,
    blackjack_counter_scene_counter_on_enter,
    blackjack_counter_scene_counter_on_enter,
    blackjack_counter_scene_counter_on_enter,
    blackjack_counter_scene_index_on_enter,
    blackjack_counter_scene_about_on_enter,
};

bool (*const blackjack_counter_scene_on_event_handlers[])(void*, SceneManagerEvent) = {
    blackjack_counter_scene_menu_on_event,
    blackjack_counter_scene_counter_on_event,
    blackjack_counter_scene_counter_on_event,
    blackjack_counter_scene_counter_on_event,
    blackjack_counter_scene_counter_on_event,
    blackjack_counter_scene_counter_on_event,
    blackjack_counter_scene_counter_on_event,
    blackjack_counter_scene_counter_on_event,
    blackjack_counter_scene_counter_on_event,
    blackjack_counter_scene_index_on_event,
    blackjack_counter_scene_about_on_event,
};

void (*const blackjack_counter_scene_on_exit_handlers[])(void*) = {
    blackjack_counter_scene_menu_on_exit,
    blackjack_counter_scene_counter_on_exit,
    blackjack_counter_scene_counter_on_exit,
    blackjack_counter_scene_counter_on_exit,
    blackjack_counter_scene_counter_on_exit,
    blackjack_counter_scene_counter_on_exit,
    blackjack_counter_scene_counter_on_exit,
    blackjack_counter_scene_counter_on_exit,
    blackjack_counter_scene_counter_on_exit,
    blackjack_counter_scene_index_on_exit,
    blackjack_counter_scene_about_on_exit,
};

const SceneManagerHandlers blackjack_counter_scene_handlers = {
    .on_enter_handlers = blackjack_counter_scene_on_enter_handlers,
    .on_event_handlers = blackjack_counter_scene_on_event_handlers,
    .on_exit_handlers = blackjack_counter_scene_on_exit_handlers,
    .scene_num = BlackjackCounterSceneNum,
};
