#include <furi.h>
#include <gui/gui.h>
#include <gui/modules/submenu.h>
#include <gui/modules/variable_item_list.h>
#include <gui/scene_manager.h>
#include <gui/view_dispatcher.h>

#include "lidaremulator_app_i.h"

#include "scenes/lidaremulator_scene.h"

static bool lidaremulator_custom_event_callback(void* context, uint32_t event) {
    furi_assert(context);
    LidarEmulatorApp* lidaremulator = context;
    return scene_manager_handle_custom_event(lidaremulator->scene_manager, event);
}

static bool lidaremulator_back_event_callback(void* context) {
    furi_assert(context);
    LidarEmulatorApp* lidaremulator = context;
    return scene_manager_handle_back_event(lidaremulator->scene_manager);
}

static void lidaremulator_tick_event_callback(void* context) {
    furi_assert(context);
    LidarEmulatorApp* lidaremulator = context;
    scene_manager_handle_tick_event(lidaremulator->scene_manager);
}

static LidarEmulatorApp* LidarEmulatorApp_alloc() {
    LidarEmulatorApp* lidaremulator = malloc(sizeof(LidarEmulatorApp));
    
    lidaremulator->gui = furi_record_open(RECORD_GUI);
    lidaremulator->view_dispatcher = view_dispatcher_alloc();
    lidaremulator->scene_manager = scene_manager_alloc(&lidaremulator_scene_handlers, lidaremulator);

    view_dispatcher_set_event_callback_context(lidaremulator->view_dispatcher, lidaremulator);
    view_dispatcher_set_custom_event_callback(lidaremulator->view_dispatcher, lidaremulator_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(lidaremulator->view_dispatcher, lidaremulator_back_event_callback);
    view_dispatcher_set_tick_event_callback(lidaremulator->view_dispatcher, lidaremulator_tick_event_callback, 100);

    lidaremulator->submenu = submenu_alloc();
    view_dispatcher_add_view(
        lidaremulator->view_dispatcher, LidarEmulatorViewSubmenu, submenu_get_view(lidaremulator->submenu));

    lidaremulator->variable_item_list = variable_item_list_alloc();
    view_dispatcher_add_view(
        lidaremulator->view_dispatcher,
        LidarEmulatorViewVariableList,
        variable_item_list_get_view(lidaremulator->variable_item_list));

    lidaremulator->view_hijacker = alloc_view_hijacker();

    lidaremulator->ir_output = LidarEmulatorIrOutputInternal;
    lidaremulator->ir_ext_5v_enabled = true;
    lidaremulator->transmit_thread = NULL;
    lidaremulator_load_settings(lidaremulator);

    return lidaremulator;
}

static void LidarEmulatorApp_free(LidarEmulatorApp* lidaremulator) {

    view_dispatcher_remove_view(lidaremulator->view_dispatcher, LidarEmulatorViewVariableList);
    variable_item_list_free(lidaremulator->variable_item_list);

    view_dispatcher_remove_view(lidaremulator->view_dispatcher, LidarEmulatorViewSubmenu);
    submenu_free(lidaremulator->submenu);

    scene_manager_free(lidaremulator->scene_manager);
    view_dispatcher_free(lidaremulator->view_dispatcher);   

    furi_record_close(RECORD_GUI);
    lidaremulator->gui = NULL;

//    free_view_hijacker(lidaremulator->view_hijacker);

    free(lidaremulator);
}

int lidar_emulator_app(void* p) {
    UNUSED(p);

    LidarEmulatorApp* lidaremulator = LidarEmulatorApp_alloc();

    view_dispatcher_attach_to_gui(lidaremulator->view_dispatcher, lidaremulator->gui, ViewDispatcherTypeFullscreen);

    scene_manager_next_scene(lidaremulator->scene_manager, LidarEmulatorSceneStart);

    view_dispatcher_run(lidaremulator->view_dispatcher);

    LidarEmulatorApp_free(lidaremulator);

    return 0;
}