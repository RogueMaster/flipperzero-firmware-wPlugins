#include <furi.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>

#include "network_subnet.h"
#include "scenes/scenes.h"
#include "views/result_view.h"
#include "views/ip_input_view.h"
#include "views/mask_input_view.h"

static bool view_dispatcher_custom_event_callback(void* context, uint32_t event) {
    furi_assert(context);
    NetworkSubnetApp* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool view_dispatcher_nav_callback(void* context) {
    furi_assert(context);
    NetworkSubnetApp* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

static NetworkSubnetApp* allocate_and_init() {
    // allocate
    FURI_LOG_D(NETWORK_SUBNET_TAG, "App started");
    FURI_LOG_D(NETWORK_SUBNET_TAG, "Allocating now");
    NetworkSubnetApp* app = malloc(sizeof(NetworkSubnetApp));

    app->cidr = 24;
    app->ip[0] = 192;
    app->ip[1] = 168;
    app->ip[2] = 1;
    app->ip[3] = 1;
    app->subnet_mask[0] = 255;
    app->subnet_mask[1] = 255;
    app->subnet_mask[2] = 255;
    app->subnet_mask[3] = 0;

    FURI_LOG_D(NETWORK_SUBNET_TAG, "App allocated");
    app->scene_manager = scene_manager_alloc(&network_subnet_scene_handlers, app);
    FURI_LOG_D(NETWORK_SUBNET_TAG, "Scene Manager Allocated");
    app->view_dispatcher = view_dispatcher_alloc();
    FURI_LOG_D(NETWORK_SUBNET_TAG, "View Dispatcher allocated");
    app->submenu = submenu_alloc();
    FURI_LOG_D(NETWORK_SUBNET_TAG, "Submenu Allocated");
    app->result_view = result_view_alloc();
    FURI_LOG_T(NETWORK_SUBNET_TAG, "Result view allocated");
    app->ip_input_view = ip_input_view_alloc(app);
    FURI_LOG_T(NETWORK_SUBNET_TAG, "IP input view");
    app->mask_input_view = mask_input_view_alloc(app);
    FURI_LOG_T(NETWORK_SUBNET_TAG, "IP input view");

    // prepare
    Gui* gui = furi_record_open(RECORD_GUI);
    FURI_LOG_D(NETWORK_SUBNET_TAG, "GUI Record Opened");
    view_dispatcher_attach_to_gui(app->view_dispatcher, gui, ViewDispatcherTypeFullscreen);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    FURI_LOG_D(NETWORK_SUBNET_TAG, "Attached VD to GUI");
    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, view_dispatcher_custom_event_callback);
    FURI_LOG_D(NETWORK_SUBNET_TAG, "Registered custom event allback");
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, view_dispatcher_nav_callback);
    FURI_LOG_D(NETWORK_SUBNET_TAG, "Registered nav callback");
    view_dispatcher_add_view(app->view_dispatcher, ViewIdMenu, submenu_get_view(app->submenu));
    FURI_LOG_D(NETWORK_SUBNET_TAG, "Adding view");
    view_dispatcher_add_view(app->view_dispatcher, ViewIdIpInput, app->ip_input_view);
    FURI_LOG_D(NETWORK_SUBNET_TAG, "Adding view ip input view");
    view_dispatcher_add_view(app->view_dispatcher, ViewIdResult, app->result_view);
    FURI_LOG_D(NETWORK_SUBNET_TAG, "Adding view result view");
    view_dispatcher_add_view(app->view_dispatcher, ViewIdMaskInput, app->mask_input_view);
    FURI_LOG_D(NETWORK_SUBNET_TAG, "Adding mask input view");
    return app;
}

static void clean_and_free(NetworkSubnetApp* app) {
    // free

    FURI_LOG_D(NETWORK_SUBNET_TAG, "Starting Freeing the memory");
    view_dispatcher_remove_view(app->view_dispatcher, ViewIdMaskInput);
    FURI_LOG_D(NETWORK_SUBNET_TAG, "mask input view removed");
    view_dispatcher_remove_view(app->view_dispatcher, ViewIdIpInput);
    FURI_LOG_D(NETWORK_SUBNET_TAG, "ip input view removed");
    view_dispatcher_remove_view(app->view_dispatcher, ViewIdMenu);
    FURI_LOG_D(NETWORK_SUBNET_TAG, "menu view removed");
    view_dispatcher_remove_view(app->view_dispatcher, ViewIdResult);
    FURI_LOG_D(NETWORK_SUBNET_TAG, "result view removed");
    submenu_free(app->submenu);
    FURI_LOG_D(NETWORK_SUBNET_TAG, "Submenu freed");
    result_view_free(app->result_view);
    FURI_LOG_D(NETWORK_SUBNET_TAG, "Result view freed");
    ip_input_view_free(app->ip_input_view);
    FURI_LOG_D(NETWORK_SUBNET_TAG, "Ip input view freed");
    mask_input_view_free(app->mask_input_view);
    FURI_LOG_D(NETWORK_SUBNET_TAG, "mask view freed");
    view_dispatcher_free(app->view_dispatcher);
    FURI_LOG_D(NETWORK_SUBNET_TAG, "vd freed");
    scene_manager_free(app->scene_manager);
    FURI_LOG_D(NETWORK_SUBNET_TAG, "sm freed");
    furi_record_close(RECORD_GUI);
    FURI_LOG_D(NETWORK_SUBNET_TAG, "gui closed");
    free(app);
    FURI_LOG_D(NETWORK_SUBNET_TAG, "remaining app freed");
}

int32_t network_subnet_app(void* p) {
    UNUSED(p);
    NetworkSubnetApp* app = allocate_and_init();

    // start
    FURI_LOG_D(NETWORK_SUBNET_TAG, "Setting next scene");
    scene_manager_next_scene(app->scene_manager, NetworkSubnetSceneSubMenu);
    FURI_LOG_D(NETWORK_SUBNET_TAG, "Starting VD");
    view_dispatcher_run(app->view_dispatcher);
    FURI_LOG_D(NETWORK_SUBNET_TAG, "VD ended");

    clean_and_free(app);
    return 0;
}
