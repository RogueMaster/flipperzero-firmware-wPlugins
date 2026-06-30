#include <expansion/expansion.h>
#include <furi.h>
#include <furi_hal.h>

#include "blackhat_app_i.h"

static bool blackhat_app_custom_event_callback(void* context, uint32_t event)
{
    furi_assert(context);
    BlackhatApp* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool blackhat_app_back_event_callback(void* context)
{
    furi_assert(context);
    BlackhatApp* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

static void blackhat_app_tick_event_callback(void* context)
{
    furi_assert(context);
    BlackhatApp* app = context;
    scene_manager_handle_tick_event(app->scene_manager);
}

BlackhatApp* blackhat_app_alloc()
{
    BlackhatApp* app = malloc(sizeof(BlackhatApp));

    app->dialogs = furi_record_open(RECORD_DIALOGS);

    memset(app->cmd, 0x00, sizeof(app->cmd));

    app->gui = furi_record_open(RECORD_GUI);

    app->view_dispatcher = view_dispatcher_alloc();

    app->scene_manager = scene_manager_alloc(&blackhat_scene_handlers, app);

    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);

    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, blackhat_app_custom_event_callback
    );
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, blackhat_app_back_event_callback
    );
    view_dispatcher_set_tick_event_callback(
        app->view_dispatcher, blackhat_app_tick_event_callback, 100
    );

    view_dispatcher_attach_to_gui(
        app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen
    );

    app->var_item_list = variable_item_list_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        BlackhatAppViewVarItemList,
        variable_item_list_get_view(app->var_item_list)
    );

    // Var item list for script
    app->script_item_list = variable_item_list_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        BlackhatAppViewScriptItemList,
        variable_item_list_get_view(app->script_item_list)
    );

    app->text_input = text_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        BlackhatAppViewTextInput,
        text_input_get_view(app->text_input)
    );

    app->tui_view = view_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, BlackhatAppViewTui, app->tui_view
    );

    for (int i = 0; i < NUM_MENU_ITEMS; ++i) {
        app->selected_option_index[i] = 0;
    }

    app->text_box = text_box_alloc();

    view_dispatcher_add_view(
        app->view_dispatcher,
        BlackhatAppViewConsoleOutput,
        text_box_get_view(app->text_box)
    );

    app->script_text = malloc(BLACKHAT_TEXT_BOX_STORE_SIZE);
    app->script_text_ptr = 0;

    app->text_box_store = furi_string_alloc();
    furi_string_reserve(app->text_box_store, BLACKHAT_TEXT_BOX_STORE_SIZE);

    scene_manager_next_scene(app->scene_manager, BlackhatSceneStart);

    return app;
}

void blackhat_app_free(BlackhatApp* app)
{
    furi_assert(app);

    for(int i = 0 ; i < app->num_scripts ; i++) {
        if(app->cmd[i])
            free(app->cmd[i]);
    }

    // Views
    view_dispatcher_remove_view(
        app->view_dispatcher, BlackhatAppViewVarItemList
    );

    view_dispatcher_remove_view(
        app->view_dispatcher, BlackhatAppViewScriptItemList
    );

    variable_item_list_free(app->script_item_list);
    variable_item_list_free(app->var_item_list);

    view_dispatcher_remove_view(app->view_dispatcher, BlackhatAppViewTextInput);
    text_input_free(app->text_input);
    view_dispatcher_remove_view(app->view_dispatcher, BlackhatAppViewTui);
    view_free(app->tui_view);
    view_dispatcher_remove_view(
        app->view_dispatcher, BlackhatAppViewConsoleOutput
    );
    text_box_free(app->text_box);
    furi_string_free(app->text_box_store);

    // View dispatcher
    view_dispatcher_free(app->view_dispatcher);
    scene_manager_free(app->scene_manager);

    blackhat_uart_free(app->uart);

    // Close records
    furi_record_close(RECORD_GUI);

    furi_record_close(RECORD_DIALOGS);

    free(app);
}

int32_t blackhat_app(void* p)
{
    UNUSED(p);

    // Disable expansion protocol to avoid interference with UART Handle
    Expansion* expansion = furi_record_open(RECORD_EXPANSION);
    expansion_disable(expansion);

    BlackhatApp* blackhat_app = blackhat_app_alloc();
    blackhat_app->scanned = false;

    bool otg_was_enabled = furi_hal_power_is_otg_enabled();
    // turn off 5v, so it gets reset on startup
    if (otg_was_enabled) {
        furi_hal_power_disable_otg();
    }
    uint8_t attempts = 0;
    while (!furi_hal_power_is_otg_enabled() && attempts++ < 5) {
        furi_hal_power_enable_otg();
        furi_delay_ms(10);
    }
    furi_delay_ms(200);

    blackhat_app->uart = blackhat_uart_init(blackhat_app);
    view_dispatcher_run(blackhat_app->view_dispatcher);
    blackhat_app_free(blackhat_app);

    if (furi_hal_power_is_otg_enabled() && !otg_was_enabled) {
        furi_hal_power_disable_otg();
    }

    // Return previous state of expansion
    expansion_enable(expansion);
    furi_record_close(RECORD_EXPANSION);

    return 0;
}
