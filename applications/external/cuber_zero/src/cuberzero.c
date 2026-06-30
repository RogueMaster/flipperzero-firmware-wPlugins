#include "cuberzero.h"
#include <furi_hal_gpio.h>

static bool callbackEmptyEvent(void* const context, const SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return 0;
}

static void callbackEmptyExit(void* const context) {
    UNUSED(context);
}

static bool callbackCustomEvent(void* const context, const uint32_t event) {
    furi_check(context);
    return scene_manager_handle_custom_event(((PCUBERZERO)context)->manager, event);
}

static bool callbackNavigationEvent(void* const context) {
    furi_check(context);
    return scene_manager_handle_back_event(((PCUBERZERO)context)->manager);
}

int32_t cuberzeroMain(const void* const unused) {
    UNUSED(unused);
    CUBERZERO_INFO("Initializing");
    const PCUBERZERO instance = malloc(sizeof(CUBERZERO));
    instance->view.submenu = submenu_alloc();
    instance->interface = furi_record_open(RECORD_GUI);
    instance->dispatcher = view_dispatcher_alloc();
    static const AppSceneOnEnterCallback handlerEnter[] = {
        SceneHomeEnter, SceneSessionEnter, SceneTimerEnter};
    static const AppSceneOnEventCallback handlerEvent[] = {
        SceneHomeEvent, callbackEmptyEvent, callbackEmptyEvent};
    static const AppSceneOnExitCallback handlerExit[] = {
        callbackEmptyExit, callbackEmptyExit, callbackEmptyExit};
    static const SceneManagerHandlers handlers = {
        handlerEnter, handlerEvent, handlerExit, COUNT_SCENE};
    instance->manager = scene_manager_alloc(&handlers, instance);
    SessionInitialize(&instance->session);
    view_dispatcher_set_event_callback_context(instance->dispatcher, instance);
    view_dispatcher_set_custom_event_callback(instance->dispatcher, callbackCustomEvent);
    view_dispatcher_set_navigation_event_callback(instance->dispatcher, callbackNavigationEvent);
    view_dispatcher_add_view(
        instance->dispatcher, VIEW_SUBMENU, submenu_get_view(instance->view.submenu));
    view_dispatcher_attach_to_gui(
        instance->dispatcher, instance->interface, ViewDispatcherTypeFullscreen);
    scene_manager_next_scene(instance->manager, SCENE_HOME);
    view_dispatcher_run(instance->dispatcher);
    view_dispatcher_remove_view(instance->dispatcher, VIEW_SUBMENU);
    SessionFree(&instance->session);
    scene_manager_free(instance->manager);
    view_dispatcher_free(instance->dispatcher);
    furi_record_close(RECORD_GUI);
    submenu_free(instance->view.submenu);
    free(instance);
    CUBERZERO_INFO("Exiting");
    return 0;
}
