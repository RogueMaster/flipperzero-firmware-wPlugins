#include "tesla_fsd_app.h"
#include "scenes_config/app_scene_functions.h"

static bool tesla_fsd_custom_event_callback(void* context, uint32_t event) {
    TeslaFSDApp* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool tesla_fsd_back_event_callback(void* context) {
    TeslaFSDApp* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

TeslaFSDApp* tesla_fsd_app_alloc(void) {
    TeslaFSDApp* app = malloc(sizeof(TeslaFSDApp));
    memset(app, 0, sizeof(TeslaFSDApp));

    // Use mcp_alloc() instead of raw malloc — it calls spi_alloc() to
    // properly initialize the SPI bus handle. Without this, mcp_can->spi
    // is NULL and furi_hal_spi_bus_handle_init() triggers furi_check fail.
    app->mcp_can = mcp_alloc(MCP_NORMAL, MCP_16MHZ, MCP_500KBPS);

    app->mutex = furi_mutex_alloc(FuriMutexTypeNormal);

    app->gui = furi_record_open(RECORD_GUI);
    app->storage = furi_record_open(RECORD_STORAGE);
    app->dialogs = furi_record_open(RECORD_DIALOGS);

    app->scene_manager = scene_manager_alloc(&tesla_fsd_scene_handlers, app);

    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, tesla_fsd_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, tesla_fsd_back_event_callback);
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    app->submenu = submenu_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, TeslaFSDViewSubmenu, submenu_get_view(app->submenu));

    app->widget = widget_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, TeslaFSDViewWidget, widget_get_view(app->widget));

    app->var_item_list = variable_item_list_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        TeslaFSDViewVarItemList,
        variable_item_list_get_view(app->var_item_list));

    app->hw_version = TeslaHW_Unknown;
    fsd_state_init(&app->fsd_state, TeslaHW_Unknown);

    app->force_fsd = false;
    app->suppress_speed_chime = false;
    app->emergency_vehicle_detect = false;
    app->nag_killer = false;
    app->precondition = false;
    // 14.x firmware warning default ON (pessimistic) — most affected users don't
    // know their firmware version, so the warning needs to reach them. Users who
    // are sure they're on pre-14.x can disable it in Settings.
    app->firmware_14x_warning = true;
    // First-boot default: Listen-Only. Forces the user to make an explicit
    // decision in Settings before any TX happens. Better for new users who
    // haven't read the README, and matches the safer default that the ESP32
    // port (PR #6) uses.
    app->op_mode = OpMode_ListenOnly;

    return app;
}

void tesla_fsd_app_free(TeslaFSDApp* app) {
    view_dispatcher_remove_view(app->view_dispatcher, TeslaFSDViewSubmenu);
    view_dispatcher_remove_view(app->view_dispatcher, TeslaFSDViewWidget);
    view_dispatcher_remove_view(app->view_dispatcher, TeslaFSDViewVarItemList);

    submenu_free(app->submenu);
    widget_free(app->widget);
    variable_item_list_free(app->var_item_list);

    scene_manager_free(app->scene_manager);
    view_dispatcher_free(app->view_dispatcher);

    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_STORAGE);
    furi_record_close(RECORD_DIALOGS);

    furi_mutex_free(app->mutex);

    free_mcp2515(app->mcp_can);
    free(app);
}

int32_t tesla_fsd_main(void* p) {
    UNUSED(p);
    TeslaFSDApp* app = tesla_fsd_app_alloc();

    scene_manager_next_scene(app->scene_manager, tesla_fsd_scene_main_menu);
    view_dispatcher_run(app->view_dispatcher);

    tesla_fsd_app_free(app);
    return 0;
}
