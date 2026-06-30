#include "disn3y_toolbox_app.h"

#include <extra_beacon.h>
#include <furi_hal_version.h>
#include <string.h>

static bool disn3y_toolbox_app_custom_event_callback(void* context, uint32_t event) {
    furi_assert(context);
    Disn3yToolboxApp* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool disn3y_toolbox_app_back_event_callback(void* context) {
    furi_assert(context);
    Disn3yToolboxApp* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

static void disn3y_toolbox_app_tick_event_callback(void* context) {
    furi_assert(context);
    Disn3yToolboxApp* app = context;
    scene_manager_handle_tick_event(app->scene_manager);
}

static void disn3y_toolbox_app_init_beacon_config(Disn3yToolboxApp* app) {
    GapExtraBeaconConfig* config = &app->beacon_config;
    const GapExtraBeaconConfig* existing = furi_hal_bt_extra_beacon_get_config();

    if(existing) {
        memcpy(config, existing, sizeof(app->beacon_config));
    } else {
        config->min_adv_interval_ms = 50;
        config->max_adv_interval_ms = 150;
        config->adv_channel_map = GapAdvChannelMapAll;
        config->adv_power_level = GapAdvPowerLevel_0dBm;
        config->address_type = GapAddressTypePublic;
        memcpy(config->address, furi_hal_version_get_ble_mac(), sizeof(config->address));
        config->address[0] ^= 0xFF;
        config->address[3] ^= 0xFF;
        furi_check(furi_hal_bt_extra_beacon_set_config(config));
    }

    app->is_beacon_active = furi_hal_bt_extra_beacon_is_active();
}

static Disn3yToolboxApp* disn3y_toolbox_app_alloc(void) {
    Disn3yToolboxApp* app = malloc(sizeof(Disn3yToolboxApp));

    app->gui = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    app->scene_manager = scene_manager_alloc(&disn3y_toolbox_app_scene_handlers, app);
    app->view_dispatcher = view_dispatcher_alloc();

    // Kyber state init
    app->selection_string = furi_string_alloc();
    app->animation_counter = 0;
    app->animation_counter_direction = false;
    app->selected_s1 = CrystalSeries1_0C00;
    app->selected_s2 = CrystalSeries2_01;
    app->kyber_series = 1;
    app->protocol_dict = protocol_dict_alloc(lfrfid_protocols, LFRFIDProtocolMax);
    app->lfworker = lfrfid_worker_alloc(app->protocol_dict);

    // MagicBand state init
    app->status_string = furi_string_alloc();
    app->selected_code_type = MagicBandCodeTypeE905;
    magicband_code_params_init(&app->code_params);

    // Droid state init
    app->selected_droid_personality = DroidPersonalityNoneRSeries;
    app->droid_paired = false;
    app->selected_droid_location = DroidLocationMarketplace;
    app->droid_loc_interval_idx = 0;
    app->droid_loc_rssi_idx = 0;
    app->droid_loc_field = 0;

    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, disn3y_toolbox_app_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, disn3y_toolbox_app_back_event_callback);
    view_dispatcher_set_tick_event_callback(
        app->view_dispatcher, disn3y_toolbox_app_tick_event_callback, 100);
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    app->submenu = submenu_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, Disn3yToolboxAppViewSubmenu, submenu_get_view(app->submenu));

    app->dialog_ex = dialog_ex_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, Disn3yToolboxAppViewDialog, dialog_ex_get_view(app->dialog_ex));

    app->widget = widget_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, Disn3yToolboxAppViewWidget, widget_get_view(app->widget));

    app->popup = popup_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, Disn3yToolboxAppViewPopup, popup_get_view(app->popup));

    app->var_item_list = variable_item_list_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        Disn3yToolboxAppViewConfig,
        variable_item_list_get_view(app->var_item_list));

    app->droid_view = view_alloc();
    view_allocate_model(app->droid_view, ViewModelTypeLockFree, sizeof(Disn3yToolboxApp*));
    Disn3yToolboxApp** droid_model = view_get_model(app->droid_view);
    *droid_model = app;
    view_commit_model(app->droid_view, false);
    view_dispatcher_add_view(app->view_dispatcher, Disn3yToolboxAppViewDroid, app->droid_view);

    disn3y_toolbox_app_init_beacon_config(app);

    return app;
}

static void disn3y_toolbox_app_free(Disn3yToolboxApp* app) {
    view_dispatcher_remove_view(app->view_dispatcher, Disn3yToolboxAppViewSubmenu);
    view_dispatcher_remove_view(app->view_dispatcher, Disn3yToolboxAppViewDialog);
    view_dispatcher_remove_view(app->view_dispatcher, Disn3yToolboxAppViewWidget);
    view_dispatcher_remove_view(app->view_dispatcher, Disn3yToolboxAppViewPopup);
    view_dispatcher_remove_view(app->view_dispatcher, Disn3yToolboxAppViewConfig);
    view_dispatcher_remove_view(app->view_dispatcher, Disn3yToolboxAppViewDroid);

    free(app->lfworker);
    free(app->protocol_dict);

    free(app->submenu);
    free(app->dialog_ex);
    free(app->widget);
    free(app->popup);
    free(app->var_item_list);
    view_free(app->droid_view);

    free(app->scene_manager);
    free(app->view_dispatcher);

    free(app->selection_string);
    free(app->status_string);

    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);
    app->gui = NULL;

    free(app);
}

int32_t disn3y_toolbox_app(void* args) {
    UNUSED(args);

    Disn3yToolboxApp* app = disn3y_toolbox_app_alloc();

    scene_manager_next_scene(app->scene_manager, Disn3yToolboxAppSceneMenu);

    view_dispatcher_run(app->view_dispatcher);

    disn3y_toolbox_app_free(app);
    return 0;
}

void disn3y_toolbox_app_popup_timeout_callback(void* context) {
    Disn3yToolboxApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, Disn3yToolboxEventPopupClosed);
}
